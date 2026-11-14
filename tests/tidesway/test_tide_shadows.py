"""Which windows cast a shadow, by grim's pixels: shadows floating puts one
around each floating window, shadows active around the focused window
alone, tiled or floating, and shadows all around every window. Tiles
touch, so a tile's shadow shows where it falls on the tile beside it: the
focused tile's is over its neighbors, and the others' are under them."""

import unittest

import tidesway  # noqa: F401  (sets up harness)
import test_tide_look as look
from harness import GET_OUTPUTS
from test_tide_dimming import BLUE

WHITE = look.WHITE
BLUR = look.BLUR
BLACK = look.BLACK
# Inside the first tile, whichever layout puts it there, clear of the seam
FLOATING = ('for_window [app_id="^test.float$"] floating enable, '
            "resize set 200 150, move position 60 60\n")


def config(mode, more=""):
    return f"shadows {mode}\nshadow_color {WHITE}\nshadow_blur_radius {BLUR}\n" + more + FLOATING


class TileShadowHelpers(look.ShadowHelpers):
    """Helpers, with no tests of their own."""

    def tiles(self):
        """Two tiles side by side, the left one and the right one; the
        right one is focused."""
        first = self.open("test.first")
        second = self.open("test.second")
        left, right = sorted((first, second), key=lambda w: self.rect(w)[0])
        self.assertEqual(self.rect(left)[2] + 1, self.rect(right)[0])  # they touch
        self.focus(right)
        return left, right

    def focus(self, con_id):
        self.command(self.connect(), f"[con_id={con_id}] focus")
        self.assertTrue(self.windows()[con_id]["focused"])

    def beside(self, caster, neighbor):
        """Points of neighbor going away from caster, which is beside it,
        and points of caster going away from neighbor: each from the column
        where they meet, level with the middle of both."""
        cx0, cy0, cx1, cy1 = self.rect(caster)
        nx0, ny0, nx1, ny1 = self.rect(neighbor)
        y = (max(cy0, ny0) + min(cy1, ny1)) // 2
        if cx1 < nx0:
            return [(nx0 + d, y) for d in range(BLUR + 3)], [(cx1 - d, y) for d in range(BLUR + 3)]
        return [(nx1 - d, y) for d in range(BLUR + 3)], [(cx0 + d, y) for d in range(BLUR + 3)]

    def assertFallsOn(self, caster, neighbor):
        """caster's white shadow lightens neighbor beside it, less the
        further in and not at all past BLUR, and leaves caster as it is."""
        self.settle()
        onto, own = self.beside(caster, neighbor)
        blue = sum(BLUE)
        self.assertGreater(self.lightness(*onto[0]), self.lightness(*onto[4]))
        self.assertGreater(self.lightness(*onto[4]), blue + 9)
        self.assertTrue(self.looks(self.pixel(*onto[BLUR + 2]), BLUE), self.pixel(*onto[BLUR + 2]))
        self.assertPixels([own[0], own[4]], BLUE)

    def assertNoneBetween(self, one, other):
        """Where the tiles meet, each is its own color to its edge."""
        onto, own = self.beside(one, other)
        self.assertPixels([onto[0], onto[4], own[0], own[4]], BLUE)

    def assertUnshadowed(self, floating):
        """Nothing lightens the tile around the floating window."""
        x0, y0, x1, y1 = self.rect(floating)
        middle_x, middle_y = (x0 + x1) // 2, (y0 + y1) // 2
        self.assertPixels([(x0 - 1, middle_y), (x1 + 1, middle_y), (middle_x, y0 - 1),
                           (middle_x, y1 + 1), (x0 - 1, y0 - 1), (x1 + 1, y1 + 1)], BLUE)


class FloatingShadowTest(look.ShadowTest):
    """shadows floating is what shadows enable was, and still is."""
    CONFIG = config("floating").replace(FLOATING, look.FLOATING)


class ActiveShadowTest(TileShadowHelpers):
    CONFIG = config("active")

    def test_the_focused_tiles_shadow_falls_on_the_tile_beside_it(self):
        left, right = self.tiles()
        self.assertFallsOn(right, left)

    def test_the_shadow_goes_with_focus(self):
        left, right = self.tiles()
        self.assertFallsOn(right, left)
        self.focus(left)
        self.assertFallsOn(left, right)
        # GET_TREE says so too
        self.assertEqual([w for w, c in self.windows().items() if c["shadow"]], [left])
        self.focus(right)
        self.assertFallsOn(right, left)
        self.assertEqual([w for w, c in self.windows().items() if c["shadow"]], [right])

    def test_a_focused_floating_window_has_the_shadow_and_the_tiles_none(self):
        left, right = self.tiles()
        floating = self.open("test.float")
        self.assertTrue(self.windows()[floating]["focused"])
        self.assertShadowed(floating)
        self.assertNoneBetween(left, right)

    def test_an_unfocused_floating_window_has_none(self):
        left, right = self.tiles()
        floating = self.open("test.float")
        self.focus(right)
        self.assertUnshadowed(floating)
        self.assertFallsOn(right, left)
        self.focus(floating)
        self.assertShadowed(floating)
        self.assertNoneBetween(left, right)

    def test_a_click_on_a_tiles_shadow_goes_to_the_tile_under_it(self):
        left, right = self.tiles()
        self.settle()
        onto, own = self.beside(right, left)
        # Under the shadow, and clear of the tiles' edge, where sway resizes
        self.click_at(*onto[BLUR - 2])
        self.assertTrue(self.windows()[left]["focused"])
        self.assertFallsOn(left, right)

    def test_a_shadow_moves_no_window(self):
        left, right = self.tiles()
        before = {w: self.rect(w) for w in (left, right)}
        self.focus(left)
        self.assertEqual({w: self.rect(w) for w in (left, right)}, before)

    def test_a_fullscreen_window_has_none(self):
        left, right = self.tiles()
        self.command(self.connect(), f"[con_id={right}] fullscreen enable")
        self.settle()
        x0, y0, x1, y1 = self.rect(right)
        self.assertPixels([(x0, 300), (x0 + 4, 300), (x1, 300), (x1 - 4, 300)], BLUE)
        self.command(self.connect(), f"[con_id={right}] fullscreen disable")
        self.assertFallsOn(right, left)

    def test_a_reload_changes_which_windows_have_one(self):
        left, right = self.tiles()
        floating = self.open("test.float")
        self.focus(right)
        self.assertFallsOn(right, left)
        self.assertUnshadowed(floating)
        for mode in "floating", "enable":
            with self.subTest(mode=mode):
                self.assertEqual(self.reload(config(mode)), [{"success": True}])
                self.assertShadowed(floating)
                self.assertNoneBetween(left, right)
        self.assertEqual(self.reload(config("all")), [{"success": True}])
        self.assertShadowed(floating)
        self.assertFallsOn(right, left)
        self.assertEqual(self.reload(config("disable")), [{"success": True}])
        self.assertUnshadowed(floating)
        self.assertNoneBetween(left, right)
        self.assertEqual(self.reload(config("active")), [{"success": True}])
        self.assertUnshadowed(floating)
        self.assertFallsOn(right, left)
        # And with no line, none, as with none before
        self.assertEqual(self.reload(FLOATING), [{"success": True}])
        self.assertUnshadowed(floating)
        self.assertNoneBetween(left, right)


class ActiveTabsTest(TileShadowHelpers):
    """In sway's own layouts: a tile beside two tabs."""
    CONFIG = config("active")

    def test_the_tab_showing_casts_the_shadow_and_a_hidden_one_none(self):
        self.command(self.connect(), "layout splith")
        left = self.open("test.left")
        first = self.open("test.first")
        self.command(self.connect(), f"[con_id={first}] splitv")
        self.command(self.connect(), f"[con_id={first}] layout tabbed")
        second = self.open("test.second")
        self.assertTrue(self.windows()[second]["focused"])
        self.assertEqual(self.rect(first), self.rect(second))  # tabs of one place
        self.assertFallsOn(second, left)
        # Its shadow starts beside its tab, as the tabs are part of it
        y = self.rect(second)[1]
        x = self.rect(left)[2]
        self.assertGreater(self.lightness(x, y + 1), sum(BLUE) + 9)
        self.focus(left)
        self.assertFallsOn(left, second)
        self.focus(first)
        self.assertFallsOn(first, left)


class ActiveOutputsTest(TileShadowHelpers):
    OUTPUTS = 2
    CONFIG = config("active")

    def test_focus_on_another_outputs_empty_workspace_leaves_no_shadow(self):
        first, second = sorted(self.connect().request(GET_OUTPUTS), key=lambda o: o["rect"]["x"])
        self.command(self.connect(), f"focus output {first['name']}")
        left, right = self.tiles()
        self.assertFallsOn(right, left)
        self.command(self.connect(), f"focus output {second['name']}")
        self.assertEqual([w for w, c in self.windows().items() if c["focused"]], [])
        self.assertNoneBetween(left, right)
        self.command(self.connect(), f"focus output {first['name']}")
        self.assertFallsOn(right, left)

    def test_a_tiles_shadow_stays_on_its_output(self):
        first, second = sorted(self.connect().request(GET_OUTPUTS), key=lambda o: o["rect"]["x"])
        self.command(self.connect(), f"focus output {first['name']}")
        left, right = self.tiles()
        self.assertFallsOn(right, left)
        x1 = self.rect(right)[2]
        self.assertEqual(x1 + 1, second["rect"]["x"])  # the next output starts there
        self.assertPixels([(x1 + 1, 300), (x1 + 4, 300)], BLACK)


class AllShadowTest(TileShadowHelpers):
    CONFIG = config("all")

    def test_the_focused_tiles_is_over_its_neighbor_and_its_neighbors_under_it(self):
        left, right = self.tiles()
        self.assertFallsOn(right, left)
        self.focus(left)
        self.assertFallsOn(left, right)

    def test_a_floating_window_has_one_focused_or_not(self):
        left, right = self.tiles()
        floating = self.open("test.float")
        self.assertShadowed(floating)
        # Neither tile is focused, so each one's shadow is under the other
        self.assertNoneBetween(left, right)
        self.focus(right)
        self.assertShadowed(floating)
        self.assertFallsOn(right, left)


class AllGapsTest(TileShadowHelpers):
    """With gaps, the tiles' shadows show in them, over what's behind."""
    GAP = 2 * BLUR + 4
    MODE = "all"

    @classmethod
    def setUpClass(cls):
        cls.CONFIG = config(cls.MODE, f"gaps inner {cls.GAP}\n")
        super().setUpClass()

    def gap(self):
        self.command(self.connect(), "layout splith")  # tide's layouts have no gaps
        first = self.open("test.first")
        second = self.open("test.second")
        left, right = sorted((first, second), key=lambda w: self.rect(w)[0])
        self.focus(right)
        self.settle()
        lx0, ly0, lx1, ly1 = self.rect(left)
        rx0, ry0, rx1, ry1 = self.rect(right)
        self.assertEqual(rx0 - lx1 - 1, self.GAP)
        y = (max(ly0, ry0) + min(ly1, ry1)) // 2
        return left, right, (lx1 + 1, y), (rx0 - 1, y), ((lx1 + rx0) // 2, y)

    def test_each_tiles_shadow_shows_in_the_gap(self):
        left, right, by_left, by_right, between = self.gap()
        self.assertGreater(self.lightness(*by_left), 9)
        self.assertGreater(self.lightness(*by_right), 9)
        self.assertPixels([between], BLACK)
        self.assertNoneBetween(left, right)  # and none is on a tile


class ActiveGapsTest(AllGapsTest):
    MODE = "active"

    def test_each_tiles_shadow_shows_in_the_gap(self):
        left, right, by_left, by_right, between = self.gap()
        self.assertGreater(self.lightness(*by_right), 9)  # the focused one's
        self.assertPixels([by_left, between], BLACK)
        self.focus(left)
        self.settle()
        self.assertGreater(self.lightness(*by_left), 9)
        self.assertPixels([by_right, between], BLACK)


class ConfigTest(look.LookHelpers):
    def test_a_mode_shadows_doesnt_know_fails_the_reload_naming_it(self):
        for line in "shadows tiled", "shadows", "shadows all floating":
            with self.subTest(line=line):
                result, = self.reload(line + "\n")
                self.assertFalse(result["success"])
                self.assertIn("shadows takes disable, floating, active or all", result["error"])


if __name__ == "__main__":
    unittest.main()
