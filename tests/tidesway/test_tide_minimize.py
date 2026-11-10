"""Minimizing the window you're in takes the keyboard from it, even when
it's alone on its workspace, which then has focus: sway took the
workspace as focused already and left the hidden window hearing keys."""

import select
import unittest

import tidesway  # noqa: F401  (sets up harness)
from harness import EVENT_WINDOW, GET_OUTPUTS, TIMEOUT, TidewmTest


class MinimizeKeyboardTest(TidewmTest):
    def line(self, client):
        self.assertTrue(select.select([client.stdout], [], [], TIMEOUT)[0], "it said nothing")
        return client.stdout.readline()

    def test_a_lone_window_minimized_hears_no_keys(self):
        ipc = self.connect()
        ipc.subscribe("window")
        client = self.wm.open_window("test.typist", "test.typist", stdout=True, keys=True)
        self.addCleanup(client.stdout.close)
        ipc.next_event(EVENT_WINDOW, lambda e: e["change"] == "new")
        # The headless seat has a keyboard once a virtual one comes
        self.press("-k", "Escape")
        while self.line(client) != b"typing\n":
            pass
        self.press("-k", "a")
        while self.line(client) != b"key a\n":
            pass
        self.command(self.connect(), "minimize")
        ipc.next_event(EVENT_WINDOW, lambda e: e["change"] == "minimize")
        self.press("-k", "b")
        # Brought back, the next key it hears is the one after
        self.command(self.connect(), "[app_id=test.typist] focus")
        self.press("-k", "c")
        line = self.line(client)
        while line and not line.startswith(b"key "):
            line = self.line(client)
        self.assertEqual(line, b"key c\n", "the minimized window heard a key")



class MinimizedSplitTest(TidewmTest):
    """In sway's own layouts, a split whose windows are all minimized
    takes no room, so the window beside it fills the output."""

    def test_a_split_of_minimized_windows_takes_no_room(self):
        ipc = self.connect()
        ipc.subscribe("window")

        def opened(app_id):
            self.wm.open_window(app_id, app_id)
            return ipc.next_event(EVENT_WINDOW, lambda e: e["change"] == "new"
                                  and e["container"]["app_id"] == app_id)["container"]["id"]

        self.command(self.connect(), "layout splith")
        first = opened("test.first")
        self.command(self.connect(), "splitv")
        second = opened("test.second")
        right = opened("test.right")
        self.command(self.connect(), f"[con_id={right}] move right")
        output, = self.connect().request(GET_OUTPUTS)
        for con in (first, second):
            self.command(self.connect(), f"[con_id={con}] minimize")
            ipc.next_event(EVENT_WINDOW, lambda e: e["change"] == "minimize")
        self.assertEqual(self.windows()[right]["rect"]["width"], output["rect"]["width"])


class MinimizedRemapTest(TidewmTest):
    """A minimized window that unmaps and maps again is a new window,
    shown, not one still minimized, though it doesn't take focus."""
    CONFIG = 'no_focus [app_id="^test.remap$"]\n'

    def line(self, client):
        self.assertTrue(select.select([client.stdout], [], [], TIMEOUT)[0], "it said nothing")
        return client.stdout.readline()

    def test_a_minimized_window_mapped_again_is_shown(self):
        ipc = self.connect()
        ipc.subscribe("window")
        client = self.wm.open_window("test.remap", "test.remap", stdin=True, stdout=True)
        self.addCleanup(client.stdout.close)
        con = ipc.next_event(EVENT_WINDOW, lambda e: e["change"] == "new")["container"]["id"]
        self.wm.open_window("test.other", "test.other")
        ipc.next_event(EVENT_WINDOW, lambda e: e["change"] == "new"
                       and e["container"]["app_id"] == "test.other")
        self.command(self.connect(), f"[con_id={con}] minimize")
        ipc.next_event(EVENT_WINDOW, lambda e: e["change"] == "minimize")
        # It has heard what minimizing told it before it unmaps, as a real
        # app would have; a configure crossing the unmap is its own error
        client.stdin.write(b"ping\n")
        client.stdin.flush()
        while self.line(client) != b"pong\n":
            pass
        client.stdin.write(b"remap\n")
        client.stdin.flush()
        ipc.next_event(EVENT_WINDOW, lambda e: e["change"] == "new"
                       and e["container"]["id"] == con)
        self.assertTrue(self.windows()[con]["visible"])


class RuleMinimizedTest(TidewmTest):
    CONFIG = 'for_window [app_id="^test.hidden$"] minimize enable\n'

    def test_a_rule_that_minimizes_a_window_keeps_it_so(self):
        ipc = self.connect()
        ipc.subscribe("window")
        self.wm.open_window("test.hidden", "test.hidden")
        con = ipc.next_event(EVENT_WINDOW, lambda e: e["change"] == "new")["container"]["id"]
        self.assertFalse(self.windows()[con]["visible"])

if __name__ == "__main__":
    unittest.main()
