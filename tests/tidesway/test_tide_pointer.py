"""The pointer as tidewm has it as a button comes up: it stays on the
surface pressed until it moves, presses again or scrolls, and then what's
under it has it."""

import select
import subprocess
import unittest

import tidesway  # noqa: F401  (sets up harness)
from harness import GET_OUTPUTS, TIMEOUT
from test_layers import LAUNCHER, LayerHelpers  # tidewm's, from the checkout


class ReleasedTest(LayerHelpers):
    def test_a_click_on_a_bar_after_a_drag_onto_it_doesnt_reach_the_window(self):
        # Its stdin kept open, as it closes at the end of it
        bar = subprocess.Popen([LAUNCHER, "bar"], env=self.wm.env, bufsize=0,
                               stdin=subprocess.PIPE, stdout=subprocess.PIPE)
        self.wm.clients.append(bar)
        self.addCleanup(bar.stdout.close)
        self.addCleanup(bar.stdin.close)
        self.assertEqual(self.line(bar), b"open\n")
        window, client = self.open("test.window", stdout=True, scrolls=True)
        # The headless seat has a pointer once a virtual one comes, which
        # the window then takes
        self.over(window, "move")
        said = b""
        while b"pointing\n" not in said:
            self.assertTrue(select.select([client.stdout], [], [], TIMEOUT)[0], "it never had the pointer")
            said += client.stdout.read1()
        said = said[said.index(b"pointing\n") + len(b"pointing\n"):]
        window = self.windows()[window["id"]]
        output, = self.connect().request(GET_OUTPUTS)
        size = (str(output["rect"]["width"]), str(output["rect"]["height"]))
        rect = window["rect"]
        x = rect["x"] + rect["width"] // 2
        # Pressed in the window, let go over the bar, which takes no
        # keyboard, then a click there, then a scroll back in the window
        self.point("move", x, rect["y"] + 50, "press", "left", "move", str(x), "10", *size,
                   "release", "left", "click", "move", str(x), str(rect["y"] + 50), *size,
                   "scroll")
        # Every button the window heard, up to the scroll: the press alone
        while b"scroll " not in said:
            self.assertTrue(select.select([client.stdout], [], [], TIMEOUT)[0], "no scroll came")
            said += client.stdout.read1()
        said = said[:said.index(b"scroll ")]
        self.assertEqual(said.count(b"button\n"), 1, said)


if __name__ == "__main__":
    unittest.main()
