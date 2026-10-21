#!/usr/bin/env python3
"""
plot_session.py -- turn recorded CSVs into one presentation figure.

Each CSV gets a CSI heatmap (all subcarriers over time) and its motion score,
labelled with the name of the folder it was recorded into. A summary of the
motion score per recording is printed, so you can quote numbers on a slide.

  python3 plot_session.py data/demo/empty/rx-1.csv data/demo/walking/rx-1.csv
  python3 plot_session.py data/demo/*/rx-1.csv --out demo.png
"""

import argparse
import collections
import csv
import os
import sys

import numpy as np

from csi_hub import MOTION_WINDOW, csi_amplitudes, motion_score

csv.field_size_limit(sys.maxsize)   # the csi column is long


def load(path):
    """-> (seconds since start, amplitudes [N x 64]) for the file's TX frames."""
    rows = list(csv.DictReader(open(path)))
    if not rows:
        sys.exit(f"{path}: no rows")
    tx = collections.Counter(r["mac"] for r in rows).most_common(1)[0][0]
    rows = [r for r in rows if r["mac"] == tx]
    amps = [csi_amplitudes(r["csi"]) for r in rows]
    k = min(len(a) for a in amps)
    t = np.array([float(r["arrival_epoch"]) for r in rows])
    return t - t[0], np.array([a[:k] for a in amps])


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("csv", nargs="+", help="recorded CSV files, one per take")
    ap.add_argument("--out", default="session.png", help="output image (default session.png)")
    args = ap.parse_args()

    import matplotlib
    matplotlib.use("Agg")   # file output only, no window
    import matplotlib.pyplot as plt

    takes = [(os.path.basename(os.path.dirname(os.path.abspath(p))), *load(p)) for p in args.csv]
    scores = [motion_score(a) if len(a) > MOTION_WINDOW else np.zeros(1) for _, _, a in takes]
    ymax = max(s.max() for s in scores) * 1.1

    fig, axes = plt.subplots(2 * len(takes), 1, figsize=(11, 3.2 * len(takes)), squeeze=False,
                             gridspec_kw={"height_ratios": [3, 1] * len(takes)})
    print(f"{'take':<16}{'frames':>8}{'secs':>7}{'rate/s':>8}{'motion median':>15}{'motion p95':>12}")
    for i, ((name, t, a), s) in enumerate(zip(takes, scores)):
        heat, line = axes[2 * i][0], axes[2 * i + 1][0]
        heat.imshow(a.T, aspect="auto", origin="lower", cmap="viridis",
                    extent=[t[0], t[-1], 0, a.shape[1]],
                    vmin=np.percentile(a, 2), vmax=np.percentile(a, 98))
        heat.set_title(f"{name}: CSI amplitude per subcarrier", fontsize=11, loc="left")
        heat.set_ylabel("subcarrier")
        if len(a) > MOTION_WINDOW:
            line.plot(t[MOTION_WINDOW - 1:], s, lw=1.2, color="#B4385A")
        line.set_xlim(t[0], t[-1])
        line.set_ylim(0, ymax)          # same scale on every take, so they compare
        line.set_ylabel("motion")
        line.set_xlabel("seconds")
        print(f"{name:<16}{len(a):>8}{t[-1]:>7.0f}{len(a) / max(t[-1], 1e-9):>8.1f}"
              f"{np.median(s):>15.3f}{np.percentile(s, 95):>12.3f}")

    fig.tight_layout()
    fig.savefig(args.out, dpi=150)
    print(f"saved {args.out}")


if __name__ == "__main__":
    main()
