"""A dialog goes along when its window moves to another workspace: if it
had focus and that workspace isn't shown, focus moves on, so the hidden
dialog hears no keys. A floating window maximized while fullscreen gets
its floating place back, not the output's."""

import unittest

import tidesway  # noqa: F401  (sets up harness)
from harness import EVENT_WINDOW, TidewmTest


class DialogFollowFocusTest(TidewmTest):
    def test_a_focused_dialog_that_goes_away_with_its_window_has_focus_no_more(self):
        ipc = self.connect()
        ipc.subscribe("window")
        client = self.wm.open_window("test.app", "test.app", stdin=True)
        parent = ipc.next_event(EVENT_WINDOW, lambda e: e["change"] == "new")["container"]["id"]
        client.stdin.write(b"dialog\n")
        client.stdin.flush()
        dialog = ipc.next_event(EVENT_WINDOW, lambda e: e["change"] == "new"
                                and e["container"]["app_id"] == "test.app.dialog")["container"]["id"]
        self.command(self.connect(), f"[con_id={dialog}] focus")
        self.command(self.connect(), f"[con_id={parent}] move container to workspace other")
        ipc.next_event(EVENT_WINDOW, lambda e: e["change"] == "move"
                       and e["container"]["id"] == dialog)
        self.assertFalse(self.windows()[dialog]["focused"])


class MaximizedWhileFullscreenTest(TidewmTest):
    def test_its_floating_place_comes_back(self):
        ipc = self.connect()
        ipc.subscribe("window")
        self.wm.open_window("test.app", "test.app")
        con = ipc.next_event(EVENT_WINDOW, lambda e: e["change"] == "new")["container"]["id"]
        self.command(self.connect(), f"[con_id={con}] floating enable")
        before = self.windows()[con]["rect"]
        for command in ["fullscreen enable", "maximize enable", "fullscreen disable",
                        "maximize disable"]:
            self.command(self.connect(), f"[con_id={con}] {command}")
        self.assertEqual(self.windows()[con]["rect"], before)



class RulePlacedTest(TidewmTest):
    CONFIG = ('for_window [app_id="^test.placed$"] floating enable, '
              "resize set 300 200, move position 10 20\n")

    def test_a_rules_size_and_place_stay(self):
        ipc = self.connect()
        ipc.subscribe("window")
        self.wm.open_window("test.placed", "test.placed")
        con = ipc.next_event(EVENT_WINDOW, lambda e: e["change"] == "new")["container"]["id"]
        rect = self.windows()[con]["rect"]
        self.assertEqual((rect["x"], rect["y"], rect["width"], rect["height"]),
                         (10, 20, 300, 200))

if __name__ == "__main__":
    unittest.main()
