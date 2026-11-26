#!/usr/bin/env python3
"""
summarize_session.py -- quick sanity check after recording a session.

Prints the median motion score for every take and receiver. Walking takes
should be clearly higher than empty and still ones; a receiver that barely
changes between "empty" and "walk_X" has a link that misses that zone.

  python3 summarize_session.py s1
"""

import glob
import os
import sys

import numpy as np

from csi_hub import MOTION_WINDOW, motion_score
from plot_session import load


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    base = os.path.join(os.path.dirname(os.path.abspath(__file__)), "data", sys.argv[1])
    takes = sorted(d for d in os.listdir(base) if os.path.isdir(os.path.join(base, d)))
    nodes = sorted({os.path.basename(f)[:-4] for f in glob.glob(os.path.join(base, "*", "*.csv"))})
    if not takes or not nodes:
        sys.exit(f"No recordings found in {base}")

    print("median motion score (higher = more movement on that receiver's link)\n")
    print(f"{'take':<14}" + "".join(f"{n:>9}" for n in nodes))
    for take in takes:
        row = f"{take:<14}"
        for node in nodes:
            path = os.path.join(base, take, f"{node}.csv")
            if not os.path.exists(path):
                row += f"{'-':>9}"
                continue
            _, amps = load(path)
            row += f"{np.median(motion_score(amps)):>9.3f}" if len(amps) > MOTION_WINDOW else f"{'short':>9}"
        print(row)


if __name__ == "__main__":
    main()
