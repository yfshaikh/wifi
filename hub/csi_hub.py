#!/usr/bin/env python3
"""
csi_hub.py -- central collector for the wifi-sensing system.

Subscribes to <topic>/+/data, where every configured receiver publishes one
record per captured CSI packet. For each record it:
  * stamps the arrival time on the hub's clock (the synchronization anchor;
    boards have independent clocks by design),
  * appends it to data/<node_id>.csv, one file per node. The CSV schema is
    fixed, so adding receivers never changes the file format.

Record schema (CSV written to disk, one row per packet):
  arrival_epoch,node_id,seq,role,mac,rssi,rate,sig_mode,mcs,bandwidth,
  smoothing,not_sounding,aggregation,stbc,fec_coding,sgi,noise_floor,
  ampdu_cnt,channel,secondary_channel,local_timestamp,ant,sig_len,rx_state,
  real_time_set,real_timestamp,len,csi

  - arrival_epoch  : hub wall-clock time (seconds since epoch) when the record arrived
  - node_id        : receiver identity (per-board config)
  - seq            : receiver's per-board monotonic sequence number
  - local_timestamp: PHY RX timestamp captured by the ESP32 WiFi hardware
  - real_timestamp : ESP32 steady-clock timestamp in seconds since boot
  - csi            : raw complex CSI as "[I0 Q0 I1 Q1 ...]" (int8 pairs)

Usage:
  python3 csi_hub.py                                  # write to ./data
  python3 csi_hub.py --host 192.168.1.50 --data-dir data
  python3 csi_hub.py --live                           # + live CSI heatmap + motion score
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
    "arrival_epoch", "node_id", "seq", "role", "mac", "rssi", "rate",
    "sig_mode", "mcs", "bandwidth", "smoothing", "not_sounding", "aggregation",
    "stbc", "fec_coding", "sgi", "noise_floor", "ampdu_cnt", "channel",
    "secondary_channel", "local_timestamp", "ant", "sig_len", "rx_state",
    "real_time_set", "real_timestamp", "len", "csi",
]

_NUM_META_FIELDS = 26  # node_id..len (csi is field #27, kept raw)

LIVE_SECONDS = 20      # history shown in the live plot
MOTION_WINDOW = 50     # packets per motion-score point (~0.5 s at 100 Hz)


def csi_amplitudes(csi_field):
    """'[a0 b0 a1 b1 ...]' (int8 I/Q pairs) -> per-subcarrier amplitude array."""
    v = np.array(csi_field.strip("[]").split(), dtype=float)
    return np.hypot(v[0:len(v) - 1:2], v[1::2])


def motion_score(amps, window=MOTION_WINDOW):
    """Rolling channel-change score for an (N packets x K subcarriers) array.

    Each packet is divided by its mean amplitude first, which cancels the
    radio's automatic gain jumps. The score is then the std over `window`
    packets, averaged across subcarriers: ~flat when the room is still, jumps
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


class Hub:
    def __init__(self, args):
        self.args = args
        os.makedirs(args.data_dir, exist_ok=True)
        self._writers = {}          # node_id -> (csv.writer, file_handle)
        self._counts = {}           # node_id -> records received
        self._last_pkt = {}         # node_id -> monotonic time of last record
        self._histories = {}        # node_id -> live-plot history (TX packets only)
        self._lock = threading.Lock()

    # ----- live plot bookkeeping -----
    def _append_history(self, node_id, mac, t, amp):
        """Keep only the node's dominant source MAC -- the TX at 100 pkt/s.
        The RX firmware already drops non-TX frames; this is a backstop for
        fake or mixed data, where a second source would scramble the plot."""
        h = self._histories.setdefault(node_id, {
            "macs": collections.Counter(),
            "t": collections.deque(maxlen=LIVE_SECONDS * 150),
            "amp": collections.deque(maxlen=LIVE_SECONDS * 150),
        })
        h["macs"][mac] += 1
        if mac == h["macs"].most_common(1)[0][0]:
            h["t"].append(t)
            h["amp"].append(amp)

    # ----- CSV writing -----
    def _writer_for(self, node_id):
        if node_id in self._writers:
            return self._writers[node_id]
        safe = re.sub(r"[^A-Za-z0-9_.-]", "_", node_id) or "node"
        path = os.path.join(self.args.data_dir, f"{safe}.csv")
        fh = open(path, "a", newline="", buffering=1)
        writer = csv.writer(fh)
        if fh.tell() == 0:
            writer.writerow(CSV_HEADER)
        self._writers[node_id] = (writer, fh)
        return self._writers[node_id]

    def _write_record(self, node_id, fields, arrival):
        writer, _ = self._writer_for(node_id)
        writer.writerow([f"{arrival:.6f}", *fields])

    # ----- MQTT callback -----
    def on_message(self, client, userdata, msg):
        topic = msg.topic
        payload = msg.payload.decode("utf-8", errors="replace").strip()
        try:
            fields = payload.split(",")
            if len(fields) != _NUM_META_FIELDS + 1:
                print(f"[hub] malformed record on {topic} ({len(fields)} fields), skipping",
                      file=sys.stderr)
                return
            node_id = fields[0]

            now = time.time()
            with self._lock:
                self._write_record(node_id, fields, now)
                self._counts[node_id] = self._counts.get(node_id, 0) + 1
                self._last_pkt[node_id] = now
                if self.args.live:
                    self._append_history(node_id, fields[3], now, csi_amplitudes(fields[-1]))
        except (ValueError, IndexError) as exc:
            print(f"[hub] bad record on {topic}: {exc}", file=sys.stderr)

    # ----- periodic status line -----
    def status_loop(self):
        last = {}
        while True:
            time.sleep(1.0)
            with self._lock:
                lines = []
                for node_id, count in self._counts.items():
                    prev = last.get(node_id, count)
                    rate = count - prev
                    last[node_id] = count
                    since = (time.time() - self._last_pkt.get(node_id, 0))
                    state = "ok" if since < 5 else "STALE"
                    lines.append(f"{node_id}: {rate}/s total={count} {state}")
            if lines:
                print("[hub] " + "  |  ".join(lines), file=sys.stderr)

    def _live_plot(self):
        # ponytail: no matplotlib.use() -- the default interactive backend
        # (macosx / Qt / Tk, whichever is installed) is the right one. Forcing
        # TkAgg crashes on Pythons built without _tkinter.
        import matplotlib.pyplot as plt
        from matplotlib.animation import FuncAnimation

        fig = plt.figure(figsize=(11, 7))
        fig.canvas.manager.set_window_title("CSI hub -- live")
        axes = {}  # node_id -> (heatmap ax, motion ax); rebuilt when nodes appear

        def _refresh(_frame):
            with self._lock:
                snap = {n: (h["macs"].most_common(1)[0][0], list(h["t"]), list(h["amp"]))
                        for n, h in self._histories.items() if h["t"]}
            if not snap:
                return []
            if set(snap) != set(axes):
                fig.clear(); axes.clear()
                grid = fig.add_gridspec(2 * len(snap), 1, height_ratios=[3, 1] * len(snap))
                for i, node_id in enumerate(sorted(snap)):
                    axes[node_id] = (fig.add_subplot(grid[2 * i]), fig.add_subplot(grid[2 * i + 1]))
            now = time.time()
            for node_id, (mac, t, amps) in snap.items():
                ax_heat, ax_motion = axes[node_id]
                k = min(len(a) for a in amps)            # guard against a short record
                a = np.array([x[:k] for x in amps])
                t = np.array(t) - now
                ax_heat.clear()
                ax_heat.imshow(a.T, aspect="auto", origin="lower", cmap="viridis",
                               extent=[t[0], t[-1], 0, k],
                               vmin=np.percentile(a, 2), vmax=np.percentile(a, 98))
                ax_heat.set_title(f"{node_id}: CSI amplitude per subcarrier  (TX {mac})", fontsize=10)
                ax_heat.set_ylabel("subcarrier")
                ax_heat.set_xlim(-LIVE_SECONDS, 0)
                ax_motion.clear()
                if len(a) > MOTION_WINDOW:
                    ax_motion.plot(t[MOTION_WINDOW - 1:], motion_score(a), lw=1.2)
                ax_motion.set_title(f"{node_id}: motion score  (flat = still, spikes = movement)", fontsize=10)
                ax_motion.set_xlim(-LIVE_SECONDS, 0)
                ax_motion.set_ylim(bottom=0)
                ax_motion.set_xlabel("seconds ago")
            fig.tight_layout()
            return []

        ani = FuncAnimation(fig, _refresh, interval=500, blit=False, cache_frame_data=False)
        plt.show()
        return ani

def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--host", default="127.0.0.1", help="MQTT broker host")
    ap.add_argument("--port", type=int, default=1883, help="MQTT broker port")
    ap.add_argument("--topic", default="csi", help="base MQTT topic (subscribe <topic>/+/data)")
    ap.add_argument("--data-dir", default=os.path.join(os.path.dirname(__file__), "data"))
    ap.add_argument("--live", action="store_true", help="show a live CSI heatmap + motion score")
    ap.add_argument("--qos", type=int, default=0, choices=[0, 1, 2])
    args = ap.parse_args()

    hub = Hub(args)
    client = mqtt.Client(client_id="csi-hub", callback_api_version=mqtt.CallbackAPIVersion.VERSION2)
    client.on_message = hub.on_message
    client.connect(args.host, args.port, keepalive=30)
    client.subscribe(f"{args.topic}/+/data", qos=args.qos)
    print(f"[hub] subscribed to {args.topic}/+/data on {args.host}:{args.port} -> {args.data_dir}",
          file=sys.stderr)
    client.loop_start()

    threading.Thread(target=hub.status_loop, daemon=True).start()

    if args.live:
        try:
            hub._live_plot()
        finally:
            client.loop_stop()
    else:
        try:
            while True:
                time.sleep(3600)
        except KeyboardInterrupt:
            pass
        client.loop_stop()


if __name__ == "__main__":
    main()