"""GET_SEATS' last_input for a click: the window it went to, in tide's
layouts and sway's, dimmed or not, even right as the window opens, before
its client has drawn it at the place it was given. sway shows a window at
its new place only once its client has drawn it there, so a click there
straight away used to find nothing."""

import os
import select
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


    def open_talking(self, app_id):
        """A window that says when it's maximized, and answers a ping"""
        client = self.wm.open_window(app_id, app_id, stdin=True, stdout=True)
        self.addCleanup(client.stdout.close)
        con = self.ipc.next_event(EVENT_WINDOW, lambda e: e["change"] == "new"
                                  and e["container"]["app_id"] == app_id)["container"]["id"]
        return con, client

    def settled(self, *clients):
        """Waits till each client has drawn whatever it was last told"""
        for each in clients:
            each.stdin.write(b"ping\n")
            each.stdin.flush()
            while self.line(each) != b"pong\n":
                pass

    def maximized_and_drawn(self, client, *others):
        """Waits till client has drawn itself maximized, and the others have
        drawn whatever they were told: a pong comes once the compositor has
        handled what its window sent before, a new size too"""
        while self.line(client) != b"maximized\n":
            pass
        self.settled(client, *others)

    def line(self, client):
        self.assertTrue(select.select([client.stdout], [], [], TIMEOUT)[0], "it said nothing")
        line = client.stdout.readline()
        self.assertTrue(line, "the client went")
        return line

    def test_a_click_reaches_a_window_maximized_in_sways_layouts(self):
        # sway's own layout placed a maximized window after the ones before it
        self.command(self.connect(), "layout splith")
        left, left_client = self.open_talking("test.left")
        right, client = self.open_talking("test.right")
        rect = self.windows()[left]["rect"]
        # Nothing else in flight, so its new state and size come together
        self.settled(left_client, client)
        self.command(self.connect(), f"[con_id={right}] maximize enable")
        self.maximized_and_drawn(client, left_client)
        self.click_at(rect["x"] + 50, rect["y"] + 50)  # where left was
        self.assertEqual(self.went_to(), ("button", right))

    def test_a_click_reaches_a_window_maximized_in_a_nested_split(self):
        # Its split was under the focused window beside it
        self.command(self.connect(), "layout splith")
        _, top_client = self.open_talking("test.top")
        self.command(self.connect(), "splitv")
        nested, client = self.open_talking("test.nested")
        beside, beside_client = self.open_talking("test.beside")
        self.command(self.connect(), f"[con_id={beside}] move right")
        self.command(self.connect(), f"[con_id={beside}] focus")
        rect = self.windows()[beside]["rect"]
        # Nothing else in flight, as the focus moving, so its new state
        # and size come together
        self.settled(top_client, client, beside_client)
        self.command(self.connect(), f"[con_id={nested}] maximize enable")
        self.maximized_and_drawn(client, top_client, beside_client)
        self.click_at(rect["x"] + 50, rect["y"] + 50)
        self.assertEqual(self.went_to(), ("button", nested))

    def test_a_click_reaches_the_tab_left_when_the_shown_ones_minimize(self):
        # The split shown, its windows minimized under a floating window,
        # stayed the active tab, so no tab was shown
        self.command(self.connect(), "layout tabbed")
        tab, tab_client = self.open_talking("test.tab")
        first, first_client = self.open_talking("test.first")
        self.command(self.connect(), "splitv")
        second, second_client = self.open_talking("test.second")
        rect = self.windows()[tab]["rect"]
        floating, floating_client = self.open_talking("test.floating")
        for command in ["floating enable", "resize set 100 100", "move position 600 400"]:
            self.command(self.connect(), f"[con_id={floating}] {command}")
        for each in (first, second):
            self.command(self.connect(), f"[con_id={each}] minimize enable")
        self.settled(tab_client, first_client, second_client, floating_client)
        self.click_at(rect["x"] + 50, rect["y"] + 50)
        self.assertEqual(self.went_to(), ("button", tab))


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
