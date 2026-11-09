"""Workspaces that span every output, as sway's per-output workspaces
grouped by name: what tidewm's suite can't ask of tidewm, which has no
parts in its tree, no workspace to move to an output, and no window on a
part of its own."""

import unittest

import tidesway  # noqa: F401  (sets up harness)
from harness import (EVENT_TICK, EVENT_WINDOW, EVENT_WORKSPACE, GET_OUTPUTS, GET_TREE,
                     GET_WORKSPACES, RUN_COMMAND, SEND_TICK, TidewmTest, ext_workspaces)


class TwoOutputHelpers(TidewmTest):
    """Helpers, with no tests of their own."""
    OUTPUTS = 2

    def setUp(self):
        super().setUp()
        outputs = sorted(self.connect().request(GET_OUTPUTS), key=lambda o: o["rect"]["x"])
        self.left, self.right = (o["name"] for o in outputs)
        self.ipc = self.connect()
        self.ipc.subscribe("window")

    def parts(self):
        """Each output's workspaces in the tree, by output name"""
        tree = self.connect().request(GET_TREE)
        return {o["name"]: sorted(w["name"] for w in o["nodes"])
                for o in tree["nodes"] if o["name"] != "__i3"}

    def window_place(self, con_id):
        window = self.windows()[con_id]
        return window["output"], window["workspace"]

    def workspace_events(self, *commands):
        """The workspace events the commands send, as (change, name)"""
        ipc = self.connect()
        ipc.subscribe("workspace", "tick")
        for command in commands:
            self.command(self.connect(), command)
        self.connect().request(SEND_TICK, "done")
        events = []
        while True:
            kind, event = ipc.recv()
            if kind == EVENT_TICK and event["payload"] == "done":
                return events
            if kind == EVENT_WORKSPACE:
                events.append((event["change"], event["current"]["name"]))


class PartsTest(TwoOutputHelpers):
    def test_every_output_has_a_part_of_every_workspace(self):
        self.command(self.connect(), "persist workspace spare")
        self.command(self.connect(), "workspace other")
        self.assertEqual(self.parts(), {self.left: ["other", "spare"],
                                        self.right: ["other", "spare"]})
        names = [(w["name"], w["persistent"]) for w in self.connect().request(GET_WORKSPACES)]
        self.assertEqual(names, [("spare", True), ("other", False)])

    def test_a_switch_sends_one_event_of_each_kind(self):
        self.assertEqual(self.workspace_events("workspace other"),
                         [("init", "other"), ("focus", "other"), ("empty", "1")])

    def test_focus_crossing_to_another_outputs_part_is_no_switch(self):
        self.assertEqual(self.workspace_events("focus output right"), [])

    def test_a_workspace_cant_move_to_an_output(self):
        reply, = self.connect().request(RUN_COMMAND, "move workspace to output right")
        self.assertFalse(reply["success"])
        self.assertEqual(self.parts(), {self.left: ["1"], self.right: ["1"]})

    def test_a_window_moves_to_its_own_outputs_part(self):
        self.command(self.connect(), "focus output right")
        _, con = self.open_window(self.ipc, "test.right")
        self.command(self.connect(), "focus output left")
        self.command(self.connect(), f"[con_id={con['id']}] move to workspace away")
        self.assertEqual(self.window_place(con["id"]), (self.right, "away"))
        self.command(self.connect(), "workspace away")
        self.assertTrue(self.windows()[con["id"]]["visible"])

    def test_a_hidden_window_moved_to_an_output_stays_on_its_workspace(self):
        _, con = self.open_window(self.ipc, "test.hidden")
        self.command(self.connect(), "workspace other")
        self.command(self.connect(), f"[con_id={con['id']}] move container to output right")
        self.assertEqual(self.window_place(con["id"]), (self.right, "1"))
        self.assertFalse(self.windows()[con["id"]]["visible"])

    def test_a_disabled_outputs_windows_join_its_workspaces_parts_left(self):
        self.command(self.connect(), "persist workspace spare")
        self.command(self.connect(), "focus output right")
        _, here = self.open_window(self.ipc, "test.here")
        self.command(self.connect(), f"[con_id={here['id']}] move to workspace spare")
        _, shown = self.open_window(self.ipc, "test.shown")
        self.command(self.connect(), f"output {self.right} disable")
        self.assertEqual(self.window_place(here["id"]), (self.left, "spare"))
        self.assertEqual(self.window_place(shown["id"]), (self.left, "1"))
        self.assertEqual(self.parts(), {self.left: ["1", "spare"]})
        self.command(self.connect(), f"output {self.right} enable")
        self.assertEqual(self.parts(), {self.left: ["1", "spare"], self.right: ["1", "spare"]})
        outputs = {o["name"]: o["current_workspace"] for o in self.connect().request(GET_OUTPUTS)}
        self.assertEqual(outputs, {self.left: "1", self.right: "1"})

    def test_a_window_on_one_part_keeps_its_workspace_while_another_shows(self):
        self.command(self.connect(), "focus output right")
        _, con = self.open_window(self.ipc, "test.kept")
        self.command(self.connect(), "focus output left")
        self.command(self.connect(), "workspace other")
        self.assertFalse(self.windows()[con["id"]]["visible"])
        self.command(self.connect(), "workspace 1")
        self.assertEqual(self.window_place(con["id"]), (self.right, "1"))
        self.assertTrue(self.windows()[con["id"]]["visible"])


class FocusOutputTest(TwoOutputHelpers):
    def test_focusing_an_output_after_a_switch_stays_on_the_switched_to_workspace(self):
        # The right output's window was focused last there, before the switch
        self.command(self.connect(), "focus output right")
        _, con = self.open_window(self.ipc, "test.hidden")
        self.command(self.connect(), "focus output left")
        self.command(self.connect(), "workspace other")
        self.command(self.connect(), "focus output right")
        outputs = {o["name"]: o["current_workspace"] for o in self.connect().request(GET_OUTPUTS)}
        self.assertEqual(outputs, {self.left: "other", self.right: "other"})
        window = self.windows()[con["id"]]
        self.assertEqual((window["focused"], window["visible"]), (False, False))


class StickyTest(TwoOutputHelpers):
    CONFIG = 'for_window [app_id="^test.pin$"] floating enable, sticky enable\n'

    def test_an_urgent_sticky_window_takes_its_urgency_along(self):
        self.command(self.connect(), "focus output right")
        _, con = self.open_window(self.ipc, "test.pin")
        self.command(self.connect(), "focus output left")
        self.command(self.connect(), f"[con_id={con['id']}] urgent enable")
        self.command(self.connect(), "workspace other")
        self.assertEqual(self.window_place(con["id"]), (self.right, "other"))
        urgent = {w["name"]: w["urgent"] for w in self.connect().request(GET_WORKSPACES)}
        self.assertEqual(urgent, {"other": True})


class PowerTest(TwoOutputHelpers):
    def test_the_focused_output_is_minus(self):
        self.command(self.connect(), "focus output right")
        self.command(self.connect(), "output - power off")
        power = {o["name"]: o["power"] for o in self.connect().request(GET_OUTPUTS)}
        self.assertEqual(power, {self.left: True, self.right: False})


class OutputGoneTest(TwoOutputHelpers):
    def test_a_workspace_made_on_a_disabled_output_can_still_be_activated(self):
        # Made on the left output, its ext-workspace handle was that part's
        self.command(self.connect(), "persist workspace two")
        self.command(self.connect(), f"output {self.left} disable")
        ext_workspaces(self.wm, "--activate", "two")
        outputs = {o["name"]: o["current_workspace"]
                   for o in self.connect().request(GET_OUTPUTS) if o["active"]}
        self.assertEqual(outputs, {self.right: "two"})


class EvacuationTest(TwoOutputHelpers):
    def test_a_global_fullscreen_window_stays_one_on_the_output_left(self):
        _, con = self.open_window(self.ipc, "test.big")
        self.command(self.connect(), f"[con_id={con['id']}] fullscreen enable global")
        self.command(self.connect(), f"output {self.left} disable")
        window = self.windows()[con["id"]]
        self.assertEqual(window["fullscreen_mode"], 2)
        self.assertGreater(window["rect"]["width"], 0)
        self.assertTrue(window["visible"])

    def test_a_fullscreen_window_in_a_split_stays_fullscreen_over_the_others(self):
        # Both parts in sway's layouts, so the split moves whole
        self.command(self.connect(), "focus output right")
        self.command(self.connect(), "layout splith")
        self.command(self.connect(), "focus output left")
        self.command(self.connect(), "layout splith")
        _, first = self.open_window(self.ipc, "test.first")
        _, second = self.open_window(self.ipc, "test.second")
        self.command(self.connect(), "splitv")
        _, third = self.open_window(self.ipc, "test.third")
        self.command(self.connect(), f"[con_id={third['id']}] fullscreen enable")
        self.command(self.connect(), f"output {self.left} disable")
        windows = self.windows()
        self.assertEqual((windows[third["id"]]["fullscreen_mode"], windows[third["id"]]["visible"]),
                         (1, True))
        self.assertEqual([windows[c["id"]]["visible"] for c in (first, second)], [False, False])

    def test_each_window_of_a_split_says_it_moved(self):
        self.command(self.connect(), "layout splith")
        _, first = self.open_window(self.ipc, "test.first")
        _, second = self.open_window(self.ipc, "test.second")
        self.command(self.connect(), "splitv")
        _, third = self.open_window(self.ipc, "test.third")
        ipc = self.connect()
        ipc.subscribe("window", "tick")
        self.command(self.connect(), f"output {self.left} disable")
        self.connect().request(SEND_TICK, "done")
        moved = set()
        while True:
            kind, event = ipc.recv()
            if kind == EVENT_TICK and event["payload"] == "done":
                break
            if kind == EVENT_WINDOW and event["change"] == "move":
                moved.add(event["container"]["id"])
        self.assertEqual(moved, {first["id"], second["id"], third["id"]})
        self.assertEqual({self.window_place(c["id"]) for c in (first, second, third)},
                         {(self.right, "1")})


class GrantTest(TwoOutputHelpers):
    CONFIG = "focus_guard enable\n"

    def test_a_grant_made_on_another_outputs_part_follows_a_rename(self):
        self.command(self.connect(), "focus output right")
        self.command(self.connect(), "grant test.late")
        self.command(self.connect(), "focus output left")
        self.command(self.connect(), "rename workspace to renamed")
        self.command(self.connect(), "workspace other")
        _, con = self.open_window(self.ipc, "test.late")
        self.assertEqual(con["workspace"], "renamed")


class PersistTest(TidewmTest):
    def test_persist_names_a_workspace_ignoring_case_as_workspace_does(self):
        self.command(self.connect(), "persist workspace Dev")
        self.command(self.connect(), "persist workspace dev")
        names = [(w["name"], w["persistent"]) for w in self.connect().request(GET_WORKSPACES)]
        self.assertEqual(names, [("1", False), ("Dev", True)])
        self.command(self.connect(), "unpersist workspace dev")
        self.assertEqual(self.workspace_names(), ["1"])


class LastOutputTest(TidewmTest):
    def test_a_persistent_workspace_outlasts_every_output_going(self):
        self.command(self.connect(), "persist workspace spare")
        output, = self.connect().request(GET_OUTPUTS)
        self.command(self.connect(), f"output {output['name']} disable")
        self.command(self.connect(), f"output {output['name']} enable")
        names = [(w["name"], w["persistent"]) for w in self.connect().request(GET_WORKSPACES)]
        self.assertIn(("spare", True), names)


if __name__ == "__main__":
    unittest.main()
