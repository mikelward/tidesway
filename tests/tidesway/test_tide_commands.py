"""Commands tidesway adds keep sway's own reachable: the command table is
searched by bsearch, so an entry out of order hides its neighbors, as
maximize once hid max_render_time."""

import unittest

import tidesway  # noqa: F401  (sets up harness)
from harness import EVENT_WINDOW, RUN_COMMAND, TidewmTest


class CommandTableTest(TidewmTest):
    def test_sways_commands_beside_tides_are_found(self):
        ipc = self.connect()
        ipc.subscribe("window")
        self.wm.open_window("test.app", "test.app")
        ipc.next_event(EVENT_WINDOW, lambda e: e["change"] == "new")
        for command in ["max_render_time off", "maximize toggle", "maximize toggle"]:
            result, = self.connect().request(RUN_COMMAND, command)
            self.assertTrue(result["success"], f"{command}: {result}")


if __name__ == "__main__":
    unittest.main()
