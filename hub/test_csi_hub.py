"""Run: .venv/bin/python test_csi_hub.py"""
import numpy as np

from csi_hub import MOTION_WINDOW, csi_amplitudes, motion_score

# I/Q pairs -> amplitude; odd trailing value ignored
assert np.allclose(csi_amplitudes("[3 4 0 0 5 12 7]"), [5, 0, 13])

rng = np.random.default_rng(0)
still = 20 + rng.normal(0, 0.2, (200, 52))
# A whole-packet gain jump (the radio's AGC) must not register as motion...
agc = still.copy()
agc[100:] *= 1.8
# ...but subcarriers changing relative to each other (a body in the path) must.
moving = still.copy()
moving[100:] += rng.normal(0, 4, (100, 52))

s_still, s_agc, s_moving = (motion_score(x) for x in (still, agc, moving))
assert len(s_still) == 200 - MOTION_WINDOW + 1
assert s_agc.max() < 2 * s_still.max(), "AGC gain step leaked into the score"
assert s_moving[-1] > 5 * s_still[-1], "motion not detected"
# all-zero (guard/null) subcarriers are ignored, not divided by
assert np.isfinite(motion_score(np.hstack([np.zeros((200, 6)), still]))).all()
print("ok")
