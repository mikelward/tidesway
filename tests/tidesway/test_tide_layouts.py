"""tide's layouts where sway's own split tree meets them: a workspace in a
tide layout keeps its windows direct children of the workspace, whatever
sway's commands and workspace_layout made before."""

import math
import unittest

import tidesway  # noqa: F401  (sets up harness)
from harness import GET_OUTPUTS, GET_TREE, RUN_COMMAND, TidewmTest


def edge(start, length, frac):
    return math.floor(start + length * frac + 0.5)


class FlatTreeTest(TidewmTest):
    def setUp(self):
        super().setUp()
        self.ipc = self.connect()
        self.ipc.subscribe("window")
        r = self.connect().request(GET_OUTPUTS)[0]["rect"]
        self.x, self.y, self.w, self.h = r["x"], r["y"], r["width"], r["height"]

    def run_commands(self, *commands):
        """Runs each command, whether or not it works, as a user might."""
        for command in commands:
            self.connect().request(RUN_COMMAND, command)

    def workspace(self, name="1"):
        tree = self.connect().request(GET_TREE)
        for output in tree["nodes"]:
            for ws in output["nodes"]:
                if ws["name"] == name:
                    return ws
        self.fail(f"no workspace {name}")

    def assert_tiled(self, cons):
        """cons, in order, are tile's master and its stack, each a direct
        child of the workspace, with no title bar taking its room."""
        ws = self.workspace()
        self.assertEqual([n["id"] for n in ws["nodes"]], [c["id"] for c in cons])
        mw = edge(0, self.w, 0.55)
        windows = self.windows()
        for i, con in enumerate(cons):
            rect = windows[con["id"]]["rect"]
            want_x = self.x if i == 0 else self.x + mw
            self.assertEqual(rect["x"], want_x, con["app_id"])
            self.assertEqual(windows[con["id"]]["window_rect"]["y"], 0, con["app_id"])
            self.assertEqual(windows[con["id"]]["window_rect"]["height"], rect["height"],
                             con["app_id"])

    def test_a_fullscreen_split_from_before_a_tide_layout(self):
        # Without a flat tree, a fullscreen split container on a tide
        # workspace arranged itself through the workspace, for ever
        self.command(self.connect(), "layout splith")
        cons = [self.open_window(self.ipc, f"test.w{i}")[1] for i in range(2)]
        self.command(self.connect(), "splitv")
        cons.append(self.open_window(self.ipc, "test.w2")[1])
        self.run_commands("layout tile", "focus parent", "fullscreen enable",
                          "fullscreen disable")
        self.assert_tiled(cons)

    def test_a_fullscreen_split_a_move_makes(self):
        # move up on a row nests in sway; fullscreen on that split still
        # has to arrange
        cons = [self.open_window(self.ipc, f"test.w{i}")[1] for i in range(2)]
        self.run_commands("move up", "focus parent", "fullscreen enable")
        self.assertTrue(self.connect().request(GET_TREE)["nodes"])
        # zoom flattens it again, and twice puts the master back
        zoom = f"[con_id={cons[0]['id']}] zoom"
        self.run_commands("fullscreen disable", zoom, zoom)
        self.assertEqual(sorted(n["id"] for n in self.workspace()["nodes"]),
                         sorted(c["id"] for c in cons))

    def test_switching_from_tabbed_leaves_no_title_bars(self):
        cons = [self.open_window(self.ipc, f"test.w{i}")[1] for i in range(2)]
        self.command(self.connect(), "layout tabbed")
        self.command(self.connect(), "layout tile")
        self.assert_tiled(cons)

    def test_a_tabbed_workspace_switched_to_tile_is_a_row(self):
        self.command(self.connect(), "layout stacking")
        self.command(self.connect(), "layout tile")
        cons = [self.open_window(self.ipc, f"test.w{i}")[1] for i in range(2)]
        self.assert_tiled(cons)


class DirectionTest(FlatTreeTest):
    """focus and move by the tiles on screen, not sway's flat row"""

    def focused(self):
        focused, = [i for i, w in self.windows().items() if w["focused"]]
        return focused

    def test_focus_and_move_go_by_the_tiles(self):
        master, top, bottom = [self.open_window(self.ipc, f"test.w{i}")[1]["id"]
                               for i in range(3)]
        self.command(self.connect(), "focus up")
        self.assertEqual(self.focused(), top)
        self.command(self.connect(), "focus left")
        self.assertEqual(self.focused(), master)
        self.command(self.connect(), f"[con_id={top}] focus")
        self.command(self.connect(), "move down")
        self.assertEqual([n["id"] for n in self.workspace()["nodes"]], [master, bottom, top])
        self.assertEqual(self.focused(), top)

    def test_focus_next_goes_round_monocle(self):
        self.command(self.connect(), "layout monocle")
        ids = [self.open_window(self.ipc, f"test.w{i}")[1]["id"] for i in range(3)]
        seen = []
        for _ in range(3):
            self.command(self.connect(), "focus next")
            seen.append(self.focused())
        self.assertEqual(seen, [ids[0], ids[1], ids[2]])
        self.command(self.connect(), "focus prev")
        self.assertEqual(self.focused(), ids[1])

    test_a_fullscreen_split_from_before_a_tide_layout = None
    test_a_fullscreen_split_a_move_makes = None
    test_switching_from_tabbed_leaves_no_title_bars = None
    test_a_tabbed_workspace_switched_to_tile_is_a_row = None


class LeavingTest(FlatTreeTest):
    def test_a_split_layout_lays_out_by_sways_rules(self):
        # The workspace is a row already, so sway's layout doesn't change
        cons = [self.open_window(self.ipc, f"test.w{i}")[1] for i in range(2)]
        self.command(self.connect(), "layout splith")
        windows = self.windows()
        widths = [windows[c["id"]]["rect"]["width"] for c in cons]
        self.assertEqual(sum(widths), self.w)
        self.assertLessEqual(abs(widths[0] - widths[1]), 1)

    def test_a_layout_that_fails_leaves_tides(self):
        cons = [self.open_window(self.ipc, f"test.w{i}")[1] for i in range(2)]
        before = {i: w["rect"] for i, w in self.windows().items()}
        result, = self.connect().request(RUN_COMMAND, "layout invalid")
        self.assertFalse(result["success"])
        self.assertEqual({i: w["rect"] for i, w in self.windows().items()}, before)
        self.assert_tiled(cons)
        self.assertEqual(self.workspace()["layout"], "tile")

    def test_a_global_fullscreen_stays_through_a_tide_layout(self):
        self.command(self.connect(), "layout splith")
        cons = [self.open_window(self.ipc, f"test.w{i}")[1] for i in range(2)]
        self.command(self.connect(), "splitv")
        cons.append(self.open_window(self.ipc, "test.w2")[1])
        self.command(self.connect(), "fullscreen enable global")
        self.command(self.connect(), "layout tile")
        # A global fullscreen window follows the outputs' new size
        self.command(self.connect(), "output HEADLESS-1 mode 1024x768")
        tree = self.connect().request(GET_TREE)
        window = self.windows()[cons[2]["id"]]
        self.assertEqual(window["fullscreen_mode"], 2)
        self.assertEqual((window["rect"]["width"], window["rect"]["height"]),
                         (tree["rect"]["width"], tree["rect"]["height"]))

    test_a_fullscreen_split_from_before_a_tide_layout = None
    test_a_fullscreen_split_a_move_makes = None
    test_switching_from_tabbed_leaves_no_title_bars = None
    test_a_tabbed_workspace_switched_to_tile_is_a_row = None


class SplitMovedInTest(FlatTreeTest):
    def test_a_tabbed_split_moved_to_an_empty_workspace_has_no_title_bars(self):
        cons = [self.open_window(self.ipc, f"test.w{i}")[1] for i in range(2)]
        self.command(self.connect(), "layout tabbed")
        self.command(self.connect(), "focus parent")
        self.command(self.connect(), "move to workspace empty")
        self.command(self.connect(), "workspace empty")
        self.assertEqual(self.workspace("empty")["layout"], "tile")
        ws = self.workspace("empty")
        self.assertEqual([n["id"] for n in ws["nodes"]], [c["id"] for c in cons])
        windows = self.windows()
        for con in cons:
            self.assertEqual(windows[con["id"]]["window_rect"]["y"], 0)

    test_a_fullscreen_split_from_before_a_tide_layout = None
    test_a_fullscreen_split_a_move_makes = None
    test_switching_from_tabbed_leaves_no_title_bars = None
    test_a_tabbed_workspace_switched_to_tile_is_a_row = None


class TwoOutputsTest(TidewmTest):
    OUTPUTS = 2

    def test_focus_into_a_fullscreen_split_lands_on_its_window(self):
        ipc = self.connect()
        ipc.subscribe("window")
        left, right = sorted(self.connect().request(GET_OUTPUTS), key=lambda o: o["rect"]["x"])
        self.command(self.connect(), f"focus output {right['name']}")
        self.command(self.connect(), "layout splith")
        there = [self.open_window(ipc, "test.r0")[1]["id"]]
        self.command(self.connect(), "splitv")
        there.append(self.open_window(ipc, "test.r1")[1]["id"])
        self.command(self.connect(), "focus parent")
        self.command(self.connect(), "fullscreen enable")
        self.command(self.connect(), f"focus output {left['name']}")
        self.open_window(ipc, "test.here")
        self.command(self.connect(), "focus right")
        focused = [i for i, w in self.windows().items() if w["focused"]]
        self.assertEqual(len(focused), 1)
        self.assertIn(focused[0], there)


class WorkspaceLayoutTest(FlatTreeTest):
    # workspace_layout wraps each new tile in a tabbed container, in sway
    CONFIG = "workspace_layout tabbed\nnew_window top\n"

    def test_new_windows_go_in_unwrapped(self):
        cons = [self.open_window(self.ipc, f"test.w{i}")[1] for i in range(3)]
        # top: after the master, so w0, w2, w1
        self.assert_tiled([cons[0], cons[2], cons[1]])

    # Not FlatTreeTest's, run again with this config
    test_a_fullscreen_split_from_before_a_tide_layout = None
    test_a_fullscreen_split_a_move_makes = None
    test_switching_from_tabbed_leaves_no_title_bars = None
    test_a_tabbed_workspace_switched_to_tile_is_a_row = None


class MovedWindowTest(FlatTreeTest):
    CONFIG = "new_window master\n"

    def test_a_window_moved_in_goes_where_new_window_says(self):
        self.command(self.connect(), "workspace far")
        cons = [self.open_window(self.ipc, f"test.w{i}")[1] for i in range(2)]
        self.command(self.connect(), "workspace 1")
        away = self.open_window(self.ipc, "test.away")[1]
        self.command(self.connect(), f"[con_id={away['id']}] move to workspace far")
        self.command(self.connect(), "workspace far")
        # master: new windows go first, so w1, w0 were already that way round
        self.assertEqual([n["id"] for n in self.workspace("far")["nodes"]],
                         [away["id"], cons[1]["id"], cons[0]["id"]])

    test_a_fullscreen_split_from_before_a_tide_layout = None
    test_a_fullscreen_split_a_move_makes = None
    test_switching_from_tabbed_leaves_no_title_bars = None
    test_a_tabbed_workspace_switched_to_tile_is_a_row = None


class BorderTest(FlatTreeTest):
    CONFIG = "default_border pixel 5\n"

    def test_size_hints_are_the_contents_not_the_borders(self):
        self.open_window(self.ipc, "test.master")
        tall = self.open_window(self.ipc, "test.tall", min_size=f"10x{self.h * 3 // 4}")[1]
        # Its row in a stack of two is half the height, less than it allows
        self.open_window(self.ipc, "test.below")
        window = self.windows()[tall["id"]]
        self.assertEqual(window["window_rect"]["height"], self.h * 3 // 4)
        self.assertEqual(window["rect"]["height"], self.h * 3 // 4 + 10)

    test_a_fullscreen_split_from_before_a_tide_layout = None
    test_a_fullscreen_split_a_move_makes = None
    test_switching_from_tabbed_leaves_no_title_bars = None
    test_a_tabbed_workspace_switched_to_tile_is_a_row = None


if __name__ == "__main__":
    unittest.main()
