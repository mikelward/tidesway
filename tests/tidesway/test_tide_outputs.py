"""tidesway's outputs where sway's output command meets tidewm's: a mode's
rate without Hz."""

import unittest

import tidesway  # noqa: F401  (sets up harness)
from harness import GET_OUTPUTS, TidewmTest


def mode(output):
    current = output["current_mode"]
    return current["width"], current["height"], current["refresh"]


class OutputsTest(TidewmTest):
    def outputs(self):
        return {o["name"]: o for o in self.connect().request(GET_OUTPUTS)}


class OneOutputTest(OutputsTest):
    CONFIG = "output HEADLESS-1 mode 800x600@50 scale 2\n"

    def test_a_rate_needs_no_hz(self):
        self.assertEqual(mode(self.outputs()["HEADLESS-1"]), (800, 600, 50000))


if __name__ == "__main__":
    unittest.main()
