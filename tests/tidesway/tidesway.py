"""What tidesway's own tests share: tidewm's tests/harness.py, from a
mikelward/tidewm checkout with its test clients built (TIDEWM_CHECKOUT,
./tidewm by default, as CI checks it out), driving tidesway through
tests/tidewm-harness/run, as tidewm's suite does.

Import this before harness: it sets TIDEWM, which harness reads."""

import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
CHECKOUT = os.path.abspath(os.environ.get("TIDEWM_CHECKOUT", os.path.join(ROOT, "tidewm")))

os.environ.setdefault("TIDEWM", os.path.join(ROOT, "tests", "tidewm-harness", "run"))
if not os.path.exists(os.path.join(CHECKOUT, "tests", "harness.py")):
    sys.exit(f"tests/tidesway: no tidewm checkout at {CHECKOUT}; set TIDEWM_CHECKOUT")
sys.path.insert(0, os.path.join(CHECKOUT, "tests"))
