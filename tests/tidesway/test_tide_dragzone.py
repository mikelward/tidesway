"""A window's shadow is where a drag resizes it, as on Hyprland (tide's
SPEC §6): a plain left press just outside a floating window's edge or
corner resizes it, as far out as a shadow reaches, or 8 pixels with none;
and a tile's shadow across the edge between the masters and the stack
drags that edge. A press in either zone doesn't reach the window under
it."""

import os
import subprocess
import unittest

import tidesway  # noqa: F401  (sets up harness)
from tidesway import NO_BORDER
from harness import EVENT_WINDOW, ROOT, RUN_COMMAND
from test_split import SplitTest

LAUNCHER = os.path.join(ROOT, "tests", "launcher")

FLOATING = ('for_window [app_id="^test.float"] floating enable, '
            "resize set 300 200, move position 100 100\n")


class ZoneTest(SplitTest):
    """Helpers, with no tests of their own."""

    def floating(self, app_id="test.float"):
        return self.open_window(self.ipc, app_id)[1]["id"]

    def focus(self, con_id):
        self.command(self.connect(), f"[con_id={con_id}] focus")

    def focused(self, con_id):
        return self.windows()[con_id]["focused"]

    def reload(self, config):
        self.wm.write_config(config)
        self.assertEqual(self.connect().request(RUN_COMMAND, "reload"), [{"success": True}])


class FloatingTest(ZoneTest):
    """With no shadows, the zone is the 8 pixels outside the window."""
    CONFIG = NO_BORDER + FLOATING

    def test_a_press_just_outside_an_edge_resizes_by_it(self):
        self.open_window(self.ipc, "test.tiled")
        con = self.floating()
        x, y, w, h = self.box(con)
        self.drag(x + w + 7, y + h // 2, x + w + 7 + 50)  # its right edge
        self.assertEqual(self.box(con), (x, y, w + 50, h))
        self.drag(x - 8, y + h // 2, x - 8 - 30)  # its left
        self.assertEqual(self.box(con), (x - 30, y, w + 80, h))
        self.drag(x + w // 2, y - 1, x + w // 2, y - 1 - 20)  # its top
        self.assertEqual(self.box(con), (x - 30, y - 20, w + 80, h + 20))
        self.drag(x + w // 2, y + h, x + w // 2, y + h + 40)  # its bottom
        self.assertEqual(self.box(con), (x - 30, y - 20, w + 80, h + 60))

    def test_a_press_just_outside_a_corner_resizes_by_it(self):
        self.open_window(self.ipc, "test.tiled")
        con = self.floating()
        x, y, w, h = self.box(con)
        self.drag(x + w + 5, y + h + 5, x + w + 5 + 30, y + h + 5 + 20)
        self.assertEqual(self.box(con), (x, y, w + 30, h + 20))
        self.drag(x - 5, y - 5, x - 5 - 10, y - 5 + 10)
        self.assertEqual(self.box(con), (x - 10, y + 10, w + 40, h + 10))

    def test_a_press_in_the_zone_doesnt_reach_the_window_under_it(self):
        tiled, client = self.listening("test.tiled")
        con = self.floating()
        x, y, w, h = self.box(con)
        self.drag(x + w + 7, y + h // 2, x + w + 7 + 50)
        self.assertEqual(self.box(con)[2], w + 50)
        self.assertNotIn(b"button\n", self.said(client))
        self.assertTrue(self.focused(con))

    def test_a_press_past_the_zone_goes_to_the_window_under_it(self):
        tiled, client = self.listening("test.tiled")
        con = self.floating()
        before = self.box(con)
        x, y, w, h = before
        self.drag(x + w + 8, y + h // 2, x + w + 8 + 50)
        self.assertEqual(self.box(con), before)
        self.assertIn(b"button\n", self.said(client))
        self.assertTrue(self.focused(tiled))

    def test_a_press_inside_the_window_reaches_the_app(self):
        self.open_window(self.ipc, "test.tiled")
        con, client = self.listening("test.float")
        before = self.box(con)
        x, y, w, h = before
        for at in (x, y + h // 2), (x + w - 1, y + h - 1), (x + w // 2, y):
            self.drag(*at, at[0] + 40)
            self.assertEqual(self.box(con), before)
            self.assertIn(b"button\n", self.said(client))

    def test_a_press_in_the_zone_focuses_the_window(self):
        tiled = self.open_window(self.ipc, "test.tiled")[1]["id"]
        con = self.floating()
        self.focus(tiled)
        x, y, w, h = self.box(con)
        self.drag(x + w + 3, y + h // 2, x + w + 3 + 10)
        self.assertEqual(self.box(con)[2], w + 10)
        self.assertTrue(self.focused(con))

    def test_a_floating_window_over_the_zone_keeps_the_press(self):
        self.open_window(self.ipc, "test.tiled")
        under = self.floating()
        over, client = self.listening("test.float2")
        x, y, w, h = self.box(under)
        # Its left side covers under's right edge and the zone past it
        self.command(self.connect(), f"[con_id={over}] move position {x + w - 20} {y + 20}")
        before = self.box(under)
        self.drag(x + w + 4, y + h // 2, x + w + 4 + 50)
        self.assertEqual(self.box(under), before)
        self.assertIn(b"button\n", self.said(client))
        # Clear of it, under's zone is still there
        self.drag(x + w + 4, y + 10, x + w + 4 + 50)
        self.assertEqual(self.box(under)[2], w + 50)

    def test_the_zone_is_over_a_floating_window_under_it(self):
        self.open_window(self.ipc, "test.tiled")
        under, client = self.listening("test.float")
        over = self.floating("test.float2")
        x, y, w, h = self.box(under)
        self.command(self.connect(), f"[con_id={over}] move position {x + w // 2} {y + h // 2}")
        ox, oy, ow, oh = self.box(over)
        # Just left of over, on under
        self.drag(ox - 4, oy + oh // 4, ox - 4 - 30)
        self.assertEqual(self.box(over), (ox - 30, oy, ow + 30, oh))
        self.assertNotIn(b"button\n", self.said(client))

    def test_a_top_layer_over_the_zone_keeps_the_press(self):
        ax, ay, aw, ah = self.area()
        self.open_window(self.ipc, "test.tiled")
        con = self.floating()
        # The notification is 200x100 in the top right corner: the window's
        # right edge is 4 px left of it, and it's twice as tall
        self.command(self.connect(), f"[con_id={con}] move position {ax + aw - 204 - 300} {ay}")
        note = subprocess.Popen([LAUNCHER, "notification"], env=self.wm.env, bufsize=0,
                                stdin=subprocess.PIPE, stdout=subprocess.PIPE)
        self.wm.clients.append(note)
        self.addCleanup(note.stdout.close)
        self.assertEqual(self.line(note), b"open\n")
        before = self.box(con)
        x, y, w, h = before
        self.drag(x + w + 6, y + 50, x + w + 6 - 50)
        self.assertEqual(self.box(con), before)
        self.drag(x + w + 6, y + 150, x + w + 6 - 50)  # below it
        self.assertEqual(self.box(con)[2], w - 50)

    def test_a_fullscreen_window_over_it_leaves_no_zone(self):
        tiled = self.open_window(self.ipc, "test.tiled")[1]["id"]
        con = self.floating()
        before = self.box(con)
        x, y, w, h = before
        self.command(self.connect(), f"[con_id={tiled}] fullscreen enable")
        self.drag(x + w + 4, y + h // 2, x + w + 4 + 50)
        self.command(self.connect(), f"[con_id={tiled}] fullscreen disable")
        self.assertEqual(self.box(con), before)

    def test_a_maximized_floating_window_has_no_zone(self):
        ax, ay, aw, ah = self.area()
        self.open_window(self.ipc, "test.tiled")
        con = self.floating()
        self.command(self.connect(), f"[con_id={con}] maximize enable")
        before = self.box(con)
        self.drag(ax + aw // 2, ay + ah // 2, ax + aw // 2 + 50)
        self.assertEqual(self.box(con), before)


class TwoOutputsTest(ZoneTest):
    """Floating windows share one stack, whichever output each is on, and
    a fullscreen window is over them all."""
    OUTPUTS = 2
    CONFIG = NO_BORDER + FLOATING

    def beside(self):
        """A floating window on HEADLESS-1 whose left edge is 6 px short of
        HEADLESS-2, left of it, so its zone reaches 2 px onto that output:
        its id, and the x where the outputs meet."""
        x, y, w, h = self.area("HEADLESS-1")
        ox, oy, ow, oh = self.area("HEADLESS-2")
        self.assertEqual(ox + ow, x)
        self.open_window(self.ipc, "test.tiled")
        first = self.floating()
        self.command(self.connect(),
                     f"[con_id={first}] move absolute position {x + 6} {y + 100}")
        self.assertEqual(self.box(first), (x + 6, y + 100, 300, 200))
        return first, x

    def test_a_floating_window_on_the_next_output_keeps_the_press_if_its_above(self):
        first, meet = self.beside()
        y = self.box(first)[1]
        self.command(self.connect(), "focus output HEADLESS-2")
        second, client = self.listening("test.float2")
        # Most of it on its own output, it reaches 2 px over first's zone
        self.command(self.connect(),
                     f"[con_id={second}] move absolute position {meet - 298} {y}")
        self.assertEqual(self.box(second), (meet - 298, y, 300, 200))
        at = meet + 1, y + 100
        self.focus(second)  # which raises it
        before = self.box(first)
        self.drag(*at, at[0] + 40)
        self.assertEqual(self.box(first), before)
        self.assertIn(b"button\n", self.said(client))
        self.focus(first)  # now first is above, and its zone with it
        self.drag(*at, at[0] + 40)
        self.assertEqual(self.box(first), (before[0] + 40, y, 260, 200))
        self.assertNotIn(b"button\n", self.said(client))

    def test_a_fullscreen_window_on_the_next_output_keeps_the_press(self):
        first, meet = self.beside()
        y = self.box(first)[1]
        self.command(self.connect(), "focus output HEADLESS-2")
        full, client = self.listening("test.full")
        self.command(self.connect(), f"[con_id={full}] fullscreen enable")
        self.assertEqual(self.box(full), self.area("HEADLESS-2"))
        before = self.box(first)
        self.drag(meet - 1, y + 100, meet - 1 + 40)  # in the zone, on the fullscreen window
        self.assertEqual(self.box(first), before)
        self.assertIn(b"button\n", self.said(client))
        self.drag(meet + 1, y + 100, meet + 1 + 40)  # on first's own output, the zone is there
        self.assertEqual(self.box(first), (before[0] + 40, y, 260, 200))


    def test_a_fullscreen_windows_dialog_on_the_next_output_keeps_the_press(self):
        first, meet = self.beside()
        y = self.box(first)[1]
        self.command(self.connect(), "focus output HEADLESS-2")
        client = self.wm.open_window("test.app", "test.app", stdin=True)
        app = self.ipc.next_event(EVENT_WINDOW, lambda e: e["change"] == "new"
                                  and e["container"]["app_id"] == "test.app")["container"]["id"]
        self.command(self.connect(), f"[con_id={app}] fullscreen enable")
        client.stdin.write(b"dialog\n")
        client.stdin.flush()
        dialog = self.ipc.next_event(
            EVENT_WINDOW, lambda e: e["change"] == "new"
            and e["container"]["app_id"] == "test.app.dialog")["container"]["id"]
        # The dialog, up with its fullscreen window, reaches 2 px onto
        # first's output, over first's zone
        dw, dh = self.box(dialog)[2:]
        self.command(self.connect(),
                     f"[con_id={dialog}] move absolute position {meet + 2 - dw} {y}")
        self.assertEqual(self.box(dialog), (meet + 2 - dw, y, dw, dh))
        before = self.box(first)
        self.drag(meet + 1, y + dh // 2, meet + 1 + 40)
        self.assertEqual(self.box(first), before)
        self.assertTrue(self.focused(dialog))


class ShadowReachTest(ZoneTest):
    """The zone reaches as far as a shadow does."""
    CONFIG = "shadows floating\nshadow_blur_radius 20\n" + FLOATING

    def tried(self, con, out):
        """Whether a press out pixels right of con's right edge resized
        it; it's put back if so."""
        x, y, w, h = self.box(con)
        self.drag(x + w + out, y + h // 2, x + w + out + 30)
        resized = self.box(con)[2] != w
        if resized:
            self.assertEqual(self.box(con)[2], w + 30)
            self.command(self.connect(), f"[con_id={con}] resize set {w} {h}")
        return resized

    def test_it_reaches_the_shadows_size(self):
        self.open_window(self.ipc, "test.tiled")
        con = self.floating()
        self.assertTrue(self.tried(con, 19))
        self.assertFalse(self.tried(con, 20))

    def test_with_no_shadows_it_is_8_pixels(self):
        self.open_window(self.ipc, "test.tiled")
        con = self.floating()
        for config in ("shadows disable\nshadow_blur_radius 20\n",
                       "shadows floating\nshadow_blur_radius 0\n",
                       "shadows all\nshadow_blur_radius 20\nshadow_color #00000000\n"):
            with self.subTest(config=config):
                self.reload(config + FLOATING)
                self.assertFalse(self.tried(con, 8))
                self.assertTrue(self.tried(con, 7))

    def test_a_floating_window_that_casts_none_has_the_same_zone(self):
        self.reload("shadows active\nshadow_blur_radius 20\n" + FLOATING)
        tiled = self.open_window(self.ipc, "test.tiled")[1]["id"]
        con = self.floating()
        self.focus(tiled)
        self.assertFalse(self.windows()[con]["shadow"])
        self.assertTrue(self.tried(con, 19))
        self.focus(tiled)
        self.assertFalse(self.tried(con, 20))


class ActiveTileTest(ZoneTest):
    """With shadows active, the focused tile's shadow across the edge
    drags it."""
    CONFIG = "shadows active\nshadow_blur_radius 16\n"

    def test_the_focused_tiles_shadow_drags_the_edge_from_its_neighbors_side(self):
        x, y, w, h = self.area()
        master, stack = self.windows_in("tile", 2, 0.5)
        edge = x + w // 2
        self.focus(master)  # its shadow falls right of the edge
        self.drag(edge + 15, y + h // 2, edge + 15 + 64)
        self.assertEqual(self.box(master)[2], w // 2 + 64)
        self.assertTrue(self.focused(master))
        self.drag(edge + 64 + 15, y + h // 2, edge + 15)
        self.assertEqual(self.box(master)[2], w // 2)
        self.drag(edge + 16, y + h // 2, edge + 16 + 64)  # past it
        self.assertEqual(self.box(master)[2], w // 2)
        self.focus(master)
        self.drag(edge - 5, y + h // 2, edge - 5 + 64)  # it casts none on its own side
        self.assertEqual(self.box(master)[2], w // 2)
        self.focus(stack)  # its shadow falls left of the edge
        self.drag(edge - 16, y + h // 2, edge - 16 - 64)
        self.assertEqual(self.box(master)[2], w // 2 - 64)
        self.assertTrue(self.focused(stack))
        self.drag(edge - 64 + 4, y + h // 2, edge - 64 + 4 + 64)
        self.assertEqual(self.box(master)[2], w // 2 - 64)

    def test_only_beside_the_focused_tile(self):
        x, y, w, h = self.area()
        master, upper, lower = self.windows_in("tile", 3, 0.5)
        edge = x + w // 2
        self.focus(upper)
        ux, uy, uw, uh = self.box(upper)
        self.drag(edge - 16, uy + uh + 16, edge - 16 - 64)  # past the end of its shadow
        self.assertEqual(self.box(master)[2], w // 2)
        self.focus(upper)
        self.drag(edge - 16, uy + uh + 15, edge - 16 - 64)  # where its shadow ends
        self.assertEqual(self.box(master)[2], w // 2 - 64)
        self.drag(edge - 64 - 4, y + h - 10, edge - 4)  # resize_grab's zone is all down the edge
        self.assertEqual(self.box(master)[2], w // 2)

    def test_the_window_under_the_shadow_never_sees_the_press(self):
        x, y, w, h = self.area()
        self.command(self.connect(), "layout tile")
        self.command(self.connect(), "mfact 0.5")
        master = self.open_window(self.ipc, "test.master")[1]["id"]
        stack, client = self.listening("test.stack")
        self.focus(master)
        edge = x + w // 2
        self.drag(edge + 10, y + h // 2, edge + 10)  # a click, which moves nothing
        self.assertEqual(self.box(master)[2], w // 2)
        self.assertNotIn(b"button\n", self.said(client))
        self.assertTrue(self.focused(master))
        self.drag(edge + 10, y + h // 2, edge + 10 + 50)
        self.assertEqual(self.box(master)[2], w // 2 + 50)
        self.assertNotIn(b"button\n", self.said(client))

    def cross(self, x, y):
        """Moves the pointer there as a mouse does, which focus follows."""
        self.pointer("cross", str(x), str(y), *self.size())

    def test_focus_stays_with_a_tile_while_the_pointer_is_on_its_shadow(self):
        x, y, w, h = self.area()
        master, stack = self.windows_in("tile", 2, 0.5)
        edge = x + w // 2
        self.cross(edge - 50, y + h // 2)
        self.assertTrue(self.focused(master))
        self.cross(edge + 10, y + h // 2)  # on its shadow, over the stack window
        self.assertTrue(self.focused(master))
        self.pointer("press", "left", "move", str(edge + 10 + 64), str(y + h // 2),
                     *self.size(), "release", "left")
        self.assertEqual(self.box(master)[2], w // 2 + 64)
        self.assertTrue(self.focused(master))
        # And the other way, from the stack window onto the master
        self.cross(edge + 64 + 50, y + h // 2)
        self.assertTrue(self.focused(stack))
        self.cross(edge + 64 - 16, y + h // 2)
        self.assertTrue(self.focused(stack))
        self.pointer("press", "left", "move", str(edge - 16), str(y + h // 2),
                     *self.size(), "release", "left")
        self.assertEqual(self.box(master)[2], w // 2)

    def test_focus_follows_the_pointer_past_the_shadow(self):
        x, y, w, h = self.area()
        master, stack = self.windows_in("tile", 2, 0.5)
        edge = x + w // 2
        self.cross(edge - 50, y + h // 2)
        self.assertTrue(self.focused(master))
        self.cross(edge + 15, y + h // 2)
        self.assertTrue(self.focused(master))
        self.cross(edge + 16, y + h // 2)
        self.assertTrue(self.focused(stack))
        # Its shadow is on the master's side now: 16 px left is on it
        self.cross(edge - 16, y + h // 2)
        self.assertTrue(self.focused(stack))
        self.cross(edge - 17, y + h // 2)
        self.assertTrue(self.focused(master))

    def test_focus_follows_the_pointer_beside_the_shadows_end(self):
        x, y, w, h = self.area()
        master, upper, lower = self.windows_in("tile", 3, 0.5)
        edge = x + w // 2
        ux, uy, uw, uh = self.box(upper)
        self.cross(edge + 50, uy + uh // 2)
        self.assertTrue(self.focused(upper))
        self.cross(edge - 10, uy + uh + 16)  # on the master, below upper's shadow
        self.assertTrue(self.focused(master))

    def test_a_focused_floating_window_leaves_the_tiles_none(self):
        self.reload(self.CONFIG + FLOATING)
        x, y, w, h = self.area()
        master, stack = self.windows_in("tile", 2, 0.5)
        con = self.floating()
        self.assertTrue(self.focused(con))
        edge = x + w // 2
        self.drag(edge + 10, y + h - 20, edge + 10 + 64)
        self.assertEqual(self.box(master)[2], w // 2)


class AllTilesTest(ZoneTest):
    """With shadows all, every tile's shadow drags the edge, either side."""
    CONFIG = "shadows all\nshadow_blur_radius 12\n"

    def test_either_side_drags_all_down_the_edge(self):
        x, y, w, h = self.area()
        master, upper, lower = self.windows_in("tile", 3, 0.5)
        edge = x + w // 2
        self.drag(edge - 12, y + 5, edge - 12 + 64)
        self.assertEqual(self.box(master)[2], w // 2 + 64)
        self.drag(edge + 64 + 11, y + h - 5, edge + 11)
        self.assertEqual(self.box(master)[2], w // 2)
        self.drag(edge + 12, y + h // 2, edge + 12 + 64)
        self.assertEqual(self.box(master)[2], w // 2)
        self.drag(edge - 13, y + h // 2, edge - 13 + 64)
        self.assertEqual(self.box(master)[2], w // 2)

    def test_without_tile_shadows_only_resize_grab_drags(self):
        x, y, w, h = self.area()
        master, stack = self.windows_in("tile", 2, 0.5)
        edge = x + w // 2
        for config in ("shadows floating\nshadow_blur_radius 12\n",
                       "shadows disable\nshadow_blur_radius 12\n",
                       "shadows all\nshadow_blur_radius 0\n",
                       "shadows all\nshadow_blur_radius 12\nresize_grab 0\n"):
            with self.subTest(config=config):
                self.reload(config)
                self.drag(edge + 11, y + h // 2, edge + 11 + 64)
                self.assertEqual(self.box(master)[2], w // 2)
        self.reload("shadows disable\n")
        self.drag(edge + 3, y + h // 2, edge + 3 + 64)
        self.assertEqual(self.box(master)[2], w // 2 + 64)


class LoneWindowTest(ZoneTest):
    CONFIG = "shadows active\nshadow_blur_radius 16\n"

    def setUp(self):
        super().setUp()
        self.command(self.connect(), "output HEADLESS-1 mode 2560x1080")

    def test_a_lone_windows_shadow_drags_its_width(self):
        x, y, w, h = self.area()
        con, = self.windows_in("tile", 1)
        self.assertEqual(self.box(con), (x + w // 10, y, w * 8 // 10, h))  # 80%
        right = x + w * 9 // 10
        self.drag(right + 15, y + h // 2, right + 15 + w // 20)
        self.assertEqual(self.box(con), (x + w // 20, y, w * 9 // 10, h))
        left = x + w // 20
        self.drag(left - 16, y + h // 2, left - 16 + w // 20)
        self.assertEqual(self.box(con), (x + w // 10, y, w * 8 // 10, h))
        self.drag(right + 16, y + h // 2, right + 16 + w // 20)  # past it
        self.assertEqual(self.box(con), (x + w // 10, y, w * 8 // 10, h))


if __name__ == "__main__":
    unittest.main()
