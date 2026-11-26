#!/usr/bin/env python3
"""
csi_hub.py -- central collector for the wifi-sensing system.

Subscribes to <topic>/+/data, where every receiver publishes one record per
captured TX frame. For each record it:
  * stamps the arrival time on the hub's clock (boards have independent clocks),
  * appends it to <data-dir>/<node_id>.csv, one file per node, fixed schema,
  * tracks per-receiver rate and frame loss (from the TX frame numbers),
  * optionally feeds a live plot.

Record schema (CSV written to disk, one row per frame):
  arrival_epoch,node_id,seq,rx_seq,tx_counter,invalid_total,role,mac,rssi,rate,
  sig_mode,mcs,bandwidth,smoothing,not_sounding,aggregation,stbc,fec_coding,sgi,
  noise_floor,ampdu_cnt,channel,secondary_channel,local_timestamp,ant,sig_len,
  rx_state,real_time_set,real_timestamp,len,csi

  - arrival_epoch  : hub clock (s) when the record arrived
  - seq            : records produced by this receiver
  - rx_seq         : the TX frame's 802.11 sequence number (0-4095, same at every receiver)
  - tx_counter     : the TX's own frame counter (same at every receiver; -1 if not found)
  - invalid_total  : TX frames this receiver dropped for invalid CSI (running count)
  - rssi           : received signal strength (dBm) -- compare against CSI
  - csi            : raw CSI "[a0 b0 a1 b1 ...]", 64 subcarriers x int8 pairs

Usage:
  python3 csi_hub.py --live                         # watch all receivers live
  python3 csi_hub.py --data-dir data/test --duration 60
"""

import argparse
import collections
import csv
import os
import re
import sys
import threading
import time

import numpy as np
import paho.mqtt.client as mqtt

CSV_HEADER = [
    "arrival_epoch", "node_id", "seq", "rx_seq", "tx_counter", "invalid_total",
    "role", "mac", "rssi", "rate", "sig_mode", "mcs", "bandwidth", "smoothing",
    "not_sounding", "aggregation", "stbc", "fec_coding", "sgi", "noise_floor",
    "ampdu_cnt", "channel", "secondary_channel", "local_timestamp", "ant",
    "sig_len", "rx_state", "real_time_set", "real_timestamp", "len", "csi",
]
# Index of a column within a received record (the record lacks arrival_epoch).
FIELD = {name: i - 1 for i, name in enumerate(CSV_HEADER) if i > 0}
NUM_FIELDS = len(CSV_HEADER) - 1

LIVE_SECONDS = 20      # history shown in the live plot
MOTION_WINDOW = 50     # frames per motion-score point (~0.5 s at 100 Hz)
SEQ_MOD = 4096         # 802.11 sequence numbers wrap here


def csi_amplitudes(csi_field):
    """'[a0 b0 a1 b1 ...]' (int8 I/Q pairs) -> per-subcarrier amplitude array."""
    v = np.array(csi_field.strip("[]").split(), dtype=float)
    return np.hypot(v[0:len(v) - 1:2], v[1::2])


def motion_score(amps, window=MOTION_WINDOW):
    """Rolling channel-change score for an (N frames x K subcarriers) array.

    Each frame is divided by its mean amplitude first, which cancels the
    radio's automatic gain jumps. The score is then the std over `window`
    frames, averaged across subcarriers: ~flat when the room is still, jumps
    when something moves through the link. Returns N - window + 1 values.
    """
    a = np.asarray(amps, dtype=float)
    a = a[:, a.mean(axis=0) > 0]                       # drop null/guard subcarriers
    a = a / np.maximum(a.mean(axis=1, keepdims=True), 1e-9)
    z = np.zeros((1, a.shape[1]))
    c1 = np.cumsum(np.vstack([z, a]), axis=0)
    c2 = np.cumsum(np.vstack([z, a * a]), axis=0)
    mean = (c1[window:] - c1[:-window]) / window
    var = (c2[window:] - c2[:-window]) / window - mean * mean
    return np.sqrt(np.clip(var, 0, None)).mean(axis=1)


class NodeStats:
    """Per-receiver counters. Loss is measured from gaps in the TX's 802.11
    sequence numbers, which every receiver sees identically."""

    def __init__(self):
        self.received = 0
        self.expected = 0
        self.last_rx_seq = None
        self.last_arrival = 0.0
        self.invalid_total = 0

    def update(self, rx_seq, invalid_total, now):
        if self.last_rx_seq is not None:
            gap = (rx_seq - self.last_rx_seq) % SEQ_MOD
            if gap == 0:
                return          # duplicate frame
            self.expected += gap
        else:
            self.expected += 1
        self.received += 1
        self.last_rx_seq = rx_seq
        self.last_arrival = now
        self.invalid_total = invalid_total

    @property
    def loss(self):
        return 1 - self.received / self.expected if self.expected else 0.0


class Hub:
    def __init__(self, data_dir, live=False):
        self.data_dir = data_dir
        self.live = live
        os.makedirs(data_dir, exist_ok=True)
        self._writers = {}          # node_id -> (csv.writer, file handle)
        self.stats = {}             # node_id -> NodeStats
        self._histories = {}        # node_id -> live-plot history
        self._lock = threading.Lock()

    # ----- CSV writing -----
    def _writer_for(self, node_id):
        if node_id not in self._writers:
            safe = re.sub(r"[^A-Za-z0-9_.-]", "_", node_id) or "node"
            fh = open(os.path.join(self.data_dir, f"{safe}.csv"), "a", newline="", buffering=1)
            writer = csv.writer(fh)
            if fh.tell() == 0:
                writer.writerow(CSV_HEADER)
            self._writers[node_id] = (writer, fh)
        return self._writers[node_id][0]

    def close(self):
        with self._lock:
            for _, fh in self._writers.values():
                fh.close()
            self._writers.clear()

    # ----- MQTT callback -----
    def on_message(self, client, userdata, msg):
        payload = msg.payload.decode("utf-8", errors="replace").strip()
        fields = payload.split(",")
        if len(fields) != NUM_FIELDS:
            print(f"[hub] skipped record on {msg.topic}: {len(fields)} fields, expected {NUM_FIELDS} "
                  "(is this receiver running the latest firmware?)", file=sys.stderr)
            return
        try:
            node_id = fields[FIELD["node_id"]]
            rx_seq = int(fields[FIELD["rx_seq"]])
            invalid_total = int(fields[FIELD["invalid_total"]])
            now = time.time()
            with self._lock:
                self._writer_for(node_id).writerow([f"{now:.6f}", *fields])
                self.stats.setdefault(node_id, NodeStats()).update(rx_seq, invalid_total, now)
                if self.live:
                    h = self._histories.setdefault(node_id, {
                        "t": collections.deque(maxlen=LIVE_SECONDS * 120),
                        "amp": collections.deque(maxlen=LIVE_SECONDS * 120)})
                    h["t"].append(now)
                    h["amp"].append(csi_amplitudes(fields[FIELD["csi"]]))
        except (ValueError, IndexError) as exc:
            print(f"[hub] bad record on {msg.topic}: {exc}", file=sys.stderr)

    # ----- status -----
    def status_line(self, last_counts, interval=1.0):
        """One line per call: rate since the last call, loss so far, staleness."""
        parts = []
        with self._lock:
            for node_id in sorted(self.stats):
                st = self.stats[node_id]
                rate = round((st.received - last_counts.get(node_id, 0)) / interval)
                last_counts[node_id] = st.received
                state = "ok" if time.time() - st.last_arrival < 5 else "STALE"
                parts.append(f"{node_id} {rate}/s loss {st.loss:.0%} {state}")
        return "  |  ".join(parts)

    def status_loop(self):
        last = {}
        while True:
            time.sleep(1.0)
            line = self.status_line(last)
            if line:
                print("[hub] " + line, file=sys.stderr)

    # ----- live plot -----
    def live_plot(self, heat_node=None):
        """Top: CSI heatmap for one receiver. Bottom: motion score for every
        receiver, so you can see which links react as someone moves."""
        # No matplotlib.use(): the default interactive backend is the right one.
        import matplotlib.pyplot as plt
        from matplotlib.animation import FuncAnimation

        fig, (ax_heat, ax_motion) = plt.subplots(2, 1, figsize=(11, 7), gridspec_kw={"height_ratios": [2, 3]})
        fig.canvas.manager.set_window_title("CSI hub -- live")

        def refresh(_frame):
            with self._lock:
                snap = {n: (list(h["t"]), list(h["amp"])) for n, h in self._histories.items() if h["t"]}
                loss = {n: s.loss for n, s in self.stats.items()}
            if not snap:
                return []
            now = time.time()
            ax_heat.clear()
            ax_motion.clear()
            shown = heat_node if heat_node in snap else sorted(snap)[0]
            for node_id in sorted(snap):
                t, amps = snap[node_id]
                k = min(len(a) for a in amps)
                a = np.array([x[:k] for x in amps])
                t = np.array(t) - now
                if node_id == shown:
                    ax_heat.imshow(a.T, aspect="auto", origin="lower", cmap="viridis",
                                   extent=[t[0], t[-1], 0, k],
                                   vmin=np.percentile(a, 2), vmax=np.percentile(a, 98))
                if len(a) > MOTION_WINDOW:
                    ax_motion.plot(t[MOTION_WINDOW - 1:], motion_score(a), lw=1.2,
                                   label=f"{node_id} (loss {loss.get(node_id, 0):.0%})")
            ax_heat.set_title(f"{shown}: CSI amplitude per subcarrier", fontsize=10)
            ax_heat.set_ylabel("subcarrier")
            ax_heat.set_xlim(-LIVE_SECONDS, 0)
            ax_motion.set_title("motion score per receiver  (flat = still, spikes = movement)", fontsize=10)
            ax_motion.set_xlim(-LIVE_SECONDS, 0)
            ax_motion.set_ylim(bottom=0)
            ax_motion.set_xlabel("seconds ago")
            ax_motion.legend(loc="upper left", fontsize=9, ncol=3)
            fig.tight_layout()
            return []

        ani = FuncAnimation(fig, refresh, interval=500, blit=False, cache_frame_data=False)
        plt.show()
        return ani


def connect(hub_or_dispatch, host, port, topic, qos=0):
    """Connect to the broker and start delivering records to on_message."""
    client = mqtt.Client(client_id=f"csi-hub-{os.getpid()}", callback_api_version=mqtt.CallbackAPIVersion.VERSION2)
    client.on_message = hub_or_dispatch
    client.connect(host, port, keepalive=30)
    client.subscribe(f"{topic}/+/data", qos=qos)
    client.loop_start()
    return client


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--host", default="127.0.0.1", help="MQTT broker host")
    ap.add_argument("--port", type=int, default=1883, help="MQTT broker port")
    ap.add_argument("--topic", default="csi", help="base MQTT topic (subscribes <topic>/+/data)")
    ap.add_argument("--data-dir", default=os.path.join(os.path.dirname(__file__), "data", "scratch"))
    ap.add_argument("--live", action="store_true", help="show the live plot")
    ap.add_argument("--heat-node", help="receiver to show in the heatmap (default: first)")
    ap.add_argument("--duration", type=float, help="stop after this many seconds")
    ap.add_argument("--qos", type=int, default=0, choices=[0, 1, 2])
    args = ap.parse_args()

    hub = Hub(args.data_dir, live=args.live)
    client = connect(hub.on_message, args.host, args.port, args.topic, args.qos)
    print(f"[hub] subscribed to {args.topic}/+/data on {args.host}:{args.port} -> {args.data_dir}", file=sys.stderr)
    threading.Thread(target=hub.status_loop, daemon=True).start()
    if args.duration:
        threading.Timer(args.duration, lambda: (hub.close(), os._exit(0))).start()

    try:
        if args.live:
            hub.live_plot(args.heat_node)
        else:
            while True:
                time.sleep(3600)
    except KeyboardInterrupt:
        pass
    finally:
        client.loop_stop()
        hub.close()


if __name__ == "__main__":
    main()
