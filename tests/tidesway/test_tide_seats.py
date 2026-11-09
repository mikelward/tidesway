"""GET_SEATS' last_input for a click: the window it went to, in tide's
layouts and sway's, dimmed or not, even right as the window opens, before
its client has drawn it at the place it was given. sway shows a window at
its new place only once its client has drawn it there, so a click there
straight away used to find nothing."""

import os
import subprocess
import unittest

import tidesway  # noqa: F401  (sets up harness)
from harness import (EVENT_WINDOW, GET_OUTPUTS, GET_SEATS, GET_WORKSPACES, ROOT, RUN_COMMAND,
                     TIMEOUT, TidewmTest)

POINTER = os.path.join(ROOT, "tests", "pointer")


class ClickHelpers(TidewmTest):
    """Helpers, with no tests of their own."""

    def setUp(self):
        super().setUp()
        self.ipc = self.connect()
        self.ipc.subscribe("window")

    def open(self, app_id):
        self.wm.open_window(app_id, app_id)
        return self.ipc.next_event(EVENT_WINDOW, lambda e: e["change"] == "new"
                                   and e["container"]["app_id"] == app_id)["container"]["id"]

    def click_at(self, x, y):
        output, = self.connect().request(GET_OUTPUTS)
        subprocess.run([POINTER, "move", str(x), str(y), str(output["rect"]["width"]),
                        str(output["rect"]["height"]), "click"],
                       env=self.wm.env, check=True, timeout=TIMEOUT)

    def click(self, con_id):
        """Clicks inside even the 100x100 buffer the test client draws first"""
        rect = self.windows()[con_id]["rect"]
        self.click_at(rect["x"] + 50, rect["y"] + 50)

    def went_to(self):
        seat, = self.connect().request(GET_SEATS)
        last = seat["last_input"]
        return last and (last["type"], last["window"])


class ClickTest(ClickHelpers):
    def test_a_click_went_to_the_window(self):
        window = self.open("test.app")
        self.click(window)
        self.assertEqual(self.went_to(), ("button", window))

    def test_a_click_where_the_pointer_already_was_went_to_the_window(self):
        window = self.open("test.app")  # the only window, under the pointer
        subprocess.run([POINTER, "click"], env=self.wm.env, check=True, timeout=TIMEOUT)
        self.assertEqual(self.went_to(), ("button", window))
        self.assertTrue(self.windows()[window]["focused"])

    def test_a_click_went_to_the_window_it_focused(self):
        left = self.open("test.left")
        right = self.open("test.right")
        self.click(left)
        self.assertEqual(self.went_to(), ("button", left))
        self.click(right)
        self.assertEqual(self.went_to(), ("button", right))

    def test_a_click_went_to_the_window_in_sways_layouts(self):
        self.command(self.connect(), "layout splith")
        left = self.open("test.left")
        self.open("test.right")
        self.click(left)
        self.assertEqual(self.went_to(), ("button", left))


class DimmedClickTest(ClickHelpers):
    CONFIG = "dim_inactive 0.5\n"

    def test_a_click_on_a_dimmed_window_went_to_it(self):
        first = self.open("test.first")
        self.open("test.second")
        self.assertTrue(self.windows()[first]["dimmed"])
        self.click(first)
        self.assertEqual(self.went_to(), ("button", first))


class NewWorkspaceTest(TidewmTest):
    """The pointer moving while a workspace made since the last frame isn't
    on screen yet. In tidewm's suite, a grant's window opening on a new
    workspace, or a switch back to one, made one, and swaybg's background
    mapping moved the pointer before it showed; tidesway crashed now and
    then (GrantTest, LayoutLinesTest). Here one command list does both, so
    the new workspace never shows before the pointer moves."""

    def test_the_pointer_moving_onto_a_new_workspace_finds_it(self):
        reply = self.connect().request(RUN_COMMAND, "workspace fresh; seat - cursor move 1 1")
        self.assertEqual(reply, [{"success": True}, {"success": True}])
        focused = [w["name"] for w in self.connect().request(GET_WORKSPACES) if w["focused"]]
        self.assertEqual(focused, ["fresh"])


if __name__ == "__main__":
    unittest.main()
