"""The focus guard where sway's own layouts meet it: a workspace in
sway's split tree holds a new window from focus as a tide layout does, and
what tidewm's suite says of focus next and a closing window holds there
too."""

import os
import subprocess
import unittest

import tidesway  # noqa: F401  (sets up harness)
from harness import EVENT_WINDOW, GET_OUTPUTS, ROOT, TIMEOUT, TidewmTest

POINTER = os.path.join(ROOT, "tests", "pointer")


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


class CloseInTabsTest(TidewmTest):
    """Closing the window you're in focuses the window under the pointer:
    in sway's tabbed layout, the tab shown there, not a hidden one whose
    place it shares."""
    CONFIG = 'for_window [app_id="^test.float$"] floating enable\n'
    LAYOUT = "layout tabbed"

    def test_closing_a_floating_window_over_tabs_focuses_the_tab_shown(self):
        ipc = self.connect()
        ipc.subscribe("window")
        self.command(self.connect(), self.LAYOUT)

        def opened(app_id):
            self.wm.open_window(app_id, app_id)
            return ipc.next_event(EVENT_WINDOW, lambda e: e["change"] == "new"
                                  and e["container"]["app_id"] == app_id)["container"]["id"]

        opened("test.hidden")
        shown = opened("test.shown")
        floating = opened("test.float")
        self.command(self.connect(), f"[con_id={floating}] focus")
        rect = self.windows()[floating]["rect"]
        output, = self.connect().request(GET_OUTPUTS)
        subprocess.run([POINTER, "move", str(rect["x"] + 20), str(rect["y"] + 20),
                        str(output["rect"]["width"]), str(output["rect"]["height"])],
                       env=self.wm.env, check=True, timeout=TIMEOUT)
        self.command(self.connect(), f"[con_id={floating}] kill")
        ipc.next_event(EVENT_WINDOW, lambda e: e["change"] == "close")
        self.assertTrue(self.windows()[shown]["focused"])



class CloseInMonocleTest(CloseInTabsTest):
    """In tide's monocle layout, the tile shown is the one under the
    pointer, not one hidden under it."""
    LAYOUT = "layout monocle"

if __name__ == "__main__":
    unittest.main()
