#!/usr/bin/env python3
"""
mqtt_fake_tx.py -- publishes synthetic CSI records onto the same MQTT topics a
real receiver would use. Lets you exercise the whole hub pipeline (subscribe,
stamp, per-node CSV) with no hardware and no sensing link up.

Examples:
  python3 mqtt_fake_tx.py --node rx-1 --rate 50
  python3 mqtt_fake_tx.py --node rx-2 --rate 30 --host 192.168.1.50
"""

import argparse
import math
import random
import threading
import time

import paho.mqtt.client as mqtt

_RECORD_TEMPLATE = ("{node},{seq},RX,{mac},{rssi},{rate},1,{mcs},0,1,0,0,"
                    "0,0,1,{noise_floor},0,{channel},0,{local_ts},{ant},{sig_len},"
                    "{rx_state},0,{real_ts},{len},{csi}")


def build_csi(len_bytes):
    """Synthetic CSI: a steady multipath pattern, plus a simulated person
    walking through the link for 3 s out of every 10 (subcarriers wobble
    independently), so the live motion score has something to show."""
    now = time.time()
    moving = now % 10 < 3
    out = []
    for i in range(len_bytes):
        base = 12 * math.sin(2 * math.pi * (i / 8))
        wobble = 6 * math.sin(now * 7 + i * 1.3) if moving else 0
        out.append(int(base + wobble + random.uniform(-1, 1)))
    return "[" + " ".join(str(v) for v in out) + "]"


def build_record(node, seq, rssi_base):
    data_len = 128  # LLTF only, same as a default receiver
    return _RECORD_TEMPLATE.format(
        node=node,
        seq=seq,
        mac="1A:00:00:00:00:01",  # the real TX's fixed address
        rssi=int(rssi_base + random.gauss(0, 1.5)),
        rate=11,
        mcs=7,
        noise_floor=-92,
        channel=6,
        local_ts=f"{time.time():.6f}",
        ant=0,
        sig_len=data_len,
        rx_state=3,
        real_ts=f"{math.modf(time.time())[0]:.6f}",
        len=data_len,
        csi=build_csi(data_len),
    )


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--host", default="127.0.0.1")
    ap.add_argument("--port", type=int, default=1883)
    ap.add_argument("--topic", default="csi")
    ap.add_argument("--node", default="rx-1")
    ap.add_argument("--rate", type=float, default=50.0)
    ap.add_argument("--seq-start", type=int, default=1)
    ap.add_argument("--qos", type=int, default=0)
    args = ap.parse_args()

    client = mqtt.Client(client_id=f"fake-{args.node}")
    client.connect(args.host, args.port, keepalive=30)
    client.loop_start()
    topic = f"{args.topic}/{args.node}/data"

    rssi_base = -38 * random.random() - 20
    seq = args.seq_start
    period = 1.0 / args.rate
    print(f"[fake] publishing to {topic} at {args.rate}/s")
    try:
        while True:
            payload = build_record(args.node, seq, rssi_base)
            msg = client.publish(topic, payload, qos=args.qos)
            seq += 1
            time.sleep(period)
    except KeyboardInterrupt:
        pass
    client.loop_stop()


if __name__ == "__main__":
    main()