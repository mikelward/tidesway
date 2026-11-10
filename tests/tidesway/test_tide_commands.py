"""Commands tidesway adds keep sway's own reachable: the command table is
searched by bsearch, so an entry out of order hides its neighbors, as
maximize once hid max_render_time. A window that opens maximized says so
in its new event."""

import select
import unittest

import tidesway  # noqa: F401  (sets up harness)
from harness import EVENT_WINDOW, RUN_COMMAND, TIMEOUT, TidewmTest


class CommandTableTest(TidewmTest):
    def test_sways_commands_beside_tides_are_found(self):
        ipc = self.connect()
        ipc.subscribe("window")
        self.wm.open_window("test.app", "test.app")
        ipc.next_event(EVENT_WINDOW, lambda e: e["change"] == "new")
        for command in ["max_render_time off", "maximize toggle", "maximize toggle"]:
            result, = self.connect().request(RUN_COMMAND, command)
            self.assertTrue(result["success"], f"{command}: {result}")



class OpensMaximizedTest(TidewmTest):
    def test_a_window_that_opens_maximized_says_so_in_its_new_event(self):
        ipc = self.connect()
        ipc.subscribe("window")
        self.wm.open_window("test.max", "test.max", maximized=True)
        new = ipc.next_event(EVENT_WINDOW, lambda e: e["change"] == "new")
        self.assertTrue(new["container"]["maximized"], new["container"])


class RuleMaximizedTest(TidewmTest):
    CONFIG = 'for_window [app_id="^test.ruled$"] maximize enable\n'

    def test_a_rule_that_maximizes_a_window_tells_the_app(self):
        client = self.wm.open_window("test.ruled", "test.ruled", stdout=True)
        self.addCleanup(client.stdout.close)
        while True:
            self.assertTrue(select.select([client.stdout], [], [], TIMEOUT)[0], "it said nothing")
            line = client.stdout.readline()
            self.assertTrue(line, "the client went")
            self.assertNotEqual(line, b"unmaximized\n")
            if line == b"maximized\n":
                break


class FloatedWhileMaximizedTest(TidewmTest):
    def test_a_tile_floated_while_maximized_has_a_size_after(self):
        ipc = self.connect()
        ipc.subscribe("window")
        self.wm.open_window("test.app", "test.app")
        con = ipc.next_event(EVENT_WINDOW, lambda e: e["change"] == "new")["container"]["id"]
        for command in ["maximize enable", "floating enable", "maximize disable"]:
            self.command(self.connect(), f"[con_id={con}] {command}")
        rect = self.windows()[con]["rect"]
        self.assertGreater(rect["width"], 0, rect)
        self.assertGreater(rect["height"], 0, rect)


if __name__ == "__main__":
    unittest.main()
