"""The focus guard where sway's own layouts meet it: a workspace in
sway's split tree holds a new window from focus as a tide layout does, and
what tidewm's suite says of focus next and a closing window holds there
too."""

import unittest

import tidesway  # noqa: F401  (sets up harness)
from harness import EVENT_WINDOW, TidewmTest


class SwayLayoutGuardTest(TidewmTest):
    CONFIG = "focus_guard enable\n"

    def setUp(self):
        super().setUp()
        self.ipc = self.connect()
        self.ipc.subscribe("window")
        self.command(self.connect(), "layout splitv")

    def focused(self):
        return [i for i, w in self.windows().items() if w["focused"]]

    def state(self, con_id):
        w = self.windows()[con_id]
        return w["focused"], w["urgent"]

    def test_the_lone_window_on_an_empty_workspace_is_held(self):
        _, held = self.open_window(self.ipc, "test.held")
        self.assertEqual((self.state(held["id"]), self.focused()), ((False, True), []))

    def test_focus_next_reaches_a_held_window(self):
        _, held = self.open_window(self.ipc, "test.held")
        self.command(self.connect(), "focus next")
        self.assertEqual(self.state(held["id"]), (True, False))

    def test_a_held_window_waits_when_the_focused_one_closes(self):
        self.command(self.connect(), "grant test.one")
        one, _ = self.open_window(self.ipc, "test.one")
        _, held = self.open_window(self.ipc, "test.held")
        one.kill()
        self.ipc.next_event(EVENT_WINDOW, lambda e: e["change"] == "close")
        self.assertEqual((self.state(held["id"]), self.focused()), ((False, True), []))

    def test_a_held_window_that_opens_fullscreen_waits_for_you(self):
        # Its fullscreen took focus as it mapped, before should_focus asked
        self.command(self.connect(), "grant test.mine")
        _, mine = self.open_window(self.ipc, "test.mine")
        self.wm.open_window("test.video", "test.video", fullscreen=True)
        video = self.ipc.next_event(EVENT_WINDOW, lambda e: e["change"] == "new"
                                    and e["container"]["app_id"] == "test.video")
        video = video["container"]["id"]
        windows = self.windows()
        self.assertEqual((windows[mine["id"]]["focused"], windows[video]["focused"],
                          windows[video]["fullscreen_mode"]), (True, False, 0))
        self.command(self.connect(), f"[con_id={video}] focus")
        self.assertEqual(self.windows()[video]["fullscreen_mode"], 1)

    def test_a_granted_window_takes_focus_from_a_fullscreen_one(self):
        # should_focus turned it down, behind the fullscreen window
        self.command(self.connect(), "grant test.mine")
        _, mine = self.open_window(self.ipc, "test.mine")
        self.command(self.connect(), "fullscreen enable")
        self.command(self.connect(), "grant test.app")
        _, app = self.open_window(self.ipc, "test.app")
        self.assertEqual((self.focused(), self.windows()[mine["id"]]["fullscreen_mode"]),
                         ([app["id"]], 0))


class FloatingPopupTest(TidewmTest):
    CONFIG = ('focus_guard enable\n'
              'for_window [app_id="^test.float$"] floating enable\n')

    def test_focus_by_direction_from_a_floating_window_passes_a_status_popup_by(self):
        # Both open in the middle, so sway's floating search would take it
        ipc = self.connect()
        ipc.subscribe("window")
        self.command(self.connect(), "grant test.float")
        _, floating = self.open_window(ipc, "test.float")
        self.wm.open_window("", "")
        popup = ipc.next_event(EVENT_WINDOW, lambda e: e["change"] == "new"
                               and not e["container"]["app_id"])["container"]["id"]
        self.assertEqual(self.windows()[popup]["type"], "floating_con")
        for direction in ("left", "right", "up", "down"):
            self.command(self.connect(), f"focus {direction}")
            focused = [i for i, w in self.windows().items() if w["focused"]]
            self.assertEqual(focused, [floating["id"]], direction)


if __name__ == "__main__":
    unittest.main()
