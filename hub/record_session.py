#!/usr/bin/env python3
"""
record_session.py -- guided recording of one labelled session.

Walks through every condition (empty room, still in each zone, walking in each
zone), records a fixed-length take of each into its own folder, and checks that
every receiver delivered data before moving on:

  data/<session>/<condition>_r<rep>/<node>.csv
  data/<session>/session.json            (settings, receivers seen, notes)

  python3 record_session.py --session s1 --receivers 6
  python3 record_session.py --session s1 --only walk_B     # redo one take

Start mosquitto first. Run one session per sitting; aim for 4 or more sessions
on different days.
"""

import argparse
import json
import os
import sys
import threading
import time

from csi_hub import Hub, connect

CONDITIONS = [
    ("empty",   "Everyone out of the room (or well outside the boards). Close the door."),
    ("still_A", "Sit still in zone A. Phone down, no fidgeting."),
    ("walk_A",  "Walk slowly around inside zone A for the whole take."),
    ("still_B", "Sit still in zone B. Phone down, no fidgeting."),
    ("walk_B",  "Walk slowly around inside zone B for the whole take."),
    ("still_C", "Sit still in zone C. Phone down, no fidgeting."),
    ("walk_C",  "Walk slowly around inside zone C for the whole take."),
]


class Dispatch:
    """One MQTT connection for the whole session; records go to the current take's Hub."""

    def __init__(self):
        self.hub = None
        self.seen = set()

    def __call__(self, client, userdata, msg):
        self.seen.add(msg.topic.split("/")[-2])
        if self.hub is not None:
            self.hub.on_message(client, userdata, msg)


def record_take(dispatch, folder, seconds):
    hub = Hub(folder)
    dispatch.hub = hub
    last = {}
    start = time.time()
    while time.time() - start < seconds:
        step = min(10, seconds - (time.time() - start))
        time.sleep(step)
        print(f"    {time.time() - start:4.0f}s  {hub.status_line(last, step)}")
    dispatch.hub = None
    hub.close()
    return hub.stats


def check(stats, expected, seconds, min_rate):
    """Return a list of problems with this take (missing receivers, low rate)."""
    problems = []
    if len(stats) < expected:
        problems.append(f"only {len(stats)} of {expected} receivers sent data")
    for node, st in sorted(stats.items()):
        rate = st.received / seconds
        if rate < min_rate:
            problems.append(f"{node} averaged {rate:.0f} frames/s (loss {st.loss:.0%})")
    return problems


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--session", required=True, help="session name, e.g. s1")
    ap.add_argument("--receivers", type=int, required=True, help="how many receivers should be sending")
    ap.add_argument("--seconds", type=float, default=120, help="length of each take (default 120)")
    ap.add_argument("--reps", type=int, default=2, help="repetitions of every condition (default 2)")
    ap.add_argument("--only", help="record just this condition (e.g. walk_B)")
    ap.add_argument("--min-rate", type=float, default=60, help="flag receivers below this many frames/s")
    ap.add_argument("--host", default="127.0.0.1")
    ap.add_argument("--port", type=int, default=1883)
    ap.add_argument("--topic", default="csi")
    args = ap.parse_args()

    base = os.path.join(os.path.dirname(os.path.abspath(__file__)), "data", args.session)
    os.makedirs(base, exist_ok=True)
    conditions = [c for c in CONDITIONS if args.only in (None, c[0])]
    if not conditions:
        sys.exit(f"Unknown condition {args.only!r}. Choose from: {', '.join(c for c, _ in CONDITIONS)}")

    dispatch = Dispatch()
    client = connect(dispatch, args.host, args.port, args.topic)
    print("Checking which receivers are sending ...")
    time.sleep(5)
    print(f"  heard from: {', '.join(sorted(dispatch.seen)) or 'nobody'}")
    if len(dispatch.seen) < args.receivers and input(
            f"  Expected {args.receivers}. Continue anyway? [y/N] ").strip().lower() != "y":
        sys.exit("Stopped. Check the missing boards are powered and on Wi-Fi.")

    meta_path = os.path.join(base, "session.json")
    meta = json.load(open(meta_path)) if os.path.exists(meta_path) else {
        "session": args.session, "started": time.strftime("%Y-%m-%d %H:%M:%S"),
        "notes": input("Layout notes for this session (who, room, anything unusual; Enter to skip): ").strip(),
        "takes": []}

    for rep in range(1, args.reps + 1):
        for cond, instruction in conditions:
            take = f"{cond}_r{rep}"
            while True:
                print(f"\n[{take}] {instruction}")
                input("  Press Enter when ready ...")
                for i in (3, 2, 1):
                    print(f"  {i} ...")
                    time.sleep(1)
                print(f"  RECORDING {args.seconds:.0f}s")
                folder = os.path.join(base, take)
                if os.path.isdir(folder):
                    for f in os.listdir(folder):
                        os.remove(os.path.join(folder, f))   # a redo replaces the old take
                stats = record_take(dispatch, folder, args.seconds)
                problems = check(stats, args.receivers, args.seconds, args.min_rate)
                if not problems:
                    print("  take OK")
                    break
                print("  problems: " + "; ".join(problems))
                if input("  Redo this take? [Y/n] ").strip().lower() == "n":
                    break
            meta["takes"].append({"take": take, "seconds": args.seconds,
                                  "time": time.strftime("%Y-%m-%d %H:%M:%S"),
                                  "receivers": {n: {"frames": s.received, "loss": round(s.loss, 4),
                                                    "invalid_total": s.invalid_total}
                                                for n, s in sorted(stats.items())}})
            json.dump(meta, open(meta_path, "w"), indent=2)

    client.loop_stop()
    print(f"\nSession saved in {base}")
    print(f"Quick check:  python3 summarize_session.py {args.session}")


if __name__ == "__main__":
    main()
