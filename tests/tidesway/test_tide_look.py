"""Rounded corners and shadows on screen, by grim's pixels: corner_radius
cuts each window's corners, showing what's behind them, and leaves the
rest of its top and bottom rows its own, to the pointer too; shadows puts
one around a floating window and none around a tile."""

import os
import select
import subprocess
import unittest

import tidesway  # noqa: F401  (sets up harness)
import test_xwayland as x11  # noqa: E402  (tidewm's, from the checkout)
from harness import EVENT_WINDOW, GET_OUTPUTS, ROOT, RUN_COMMAND, TIMEOUT
from test_tide_dimming import BLUE, DIMMED, STRENGTH, DimOnScreenHelpers

POINTER = os.path.join(ROOT, "tests", "pointer")

RADIUS = 12
BLACK = (0, 0, 0)  # nothing: no window and no wallpaper
FLOATING = ('for_window [app_id="^test.float$"] floating enable, '
            "resize set 300 200, move position 100 100\n")


class LookHelpers(DimOnScreenHelpers):
    """Helpers, with no tests of their own."""
    CONFIG = f"corner_radius {RADIUS}\n"

    def rect(self, con_id):
        r = self.windows()[con_id]["rect"]
        return r["x"], r["y"], r["x"] + r["width"] - 1, r["y"] + r["height"] - 1

    def cut(self, con_id):
        """A point in each corner of the window's box that a corner of
        RADIUS leaves out."""
        x0, y0, x1, y1 = self.rect(con_id)
        return [(x0 + 1, y0 + 1), (x1 - 1, y0 + 1), (x0 + 1, y1 - 1), (x1 - 1, y1 - 1)]

    def kept(self, con_id):
        """Points of the window's top and bottom rows that a corner of
        RADIUS leaves in: by each corner, and in the middle of each side."""
        x0, y0, x1, y1 = self.rect(con_id)
        middle = (x0 + x1) // 2
        return [(x0 + 5, y0 + 5), (x1 - 5, y0 + 5), (x0 + 5, y1 - 5), (x1 - 5, y1 - 5),
                (x0 + RADIUS, y0), (x1 - RADIUS, y0), (x0 + RADIUS, y1), (x1 - RADIUS, y1),
                (middle, y0), (middle, y1), (x0, y0 + RADIUS), (x1, y1 - RADIUS)]

    def click_at(self, x, y):
        output, = self.connect().request(GET_OUTPUTS)
        subprocess.run([POINTER, "move", str(x), str(y), str(output["rect"]["width"]),
                        str(output["rect"]["height"]), "click"],
                       env=self.wm.env, check=True, timeout=TIMEOUT)

    def reload(self, config):
        self.wm.write_config(config)
        return self.connect().request(RUN_COMMAND, "reload")


class RoundedTest(LookHelpers):
    def test_a_windows_corners_are_cut_and_the_rest_of_it_isnt(self):
        window = self.open("test.app")
        self.assertPixels(self.cut(window), BLACK)
        self.assertPixels(self.kept(window), BLUE)

    def test_each_tiles_corners_are_cut_as_the_tiles_change(self):
        first = self.open("test.first")
        second = self.open("test.second")
        for window in first, second:
            self.assertPixels(self.cut(window), BLACK)
            self.assertPixels(self.kept(window), BLUE)
        third = self.open("test.third")
        for window in first, second, third:
            self.assertPixels(self.cut(window), BLACK)
            self.assertPixels(self.kept(window), BLUE)

    def test_a_fullscreen_windows_corners_are_square(self):
        window = self.open("test.app")
        self.command(self.connect(), f"[con_id={window}] fullscreen enable")
        self.assertPixels(self.cut(window) + self.kept(window), BLUE)
        self.command(self.connect(), f"[con_id={window}] fullscreen disable")
        self.assertPixels(self.cut(window), BLACK)
        self.assertPixels(self.kept(window), BLUE)

    def test_a_bordered_windows_corners_are_square(self):
        window = self.open("test.app")
        self.command(self.connect(), f"[con_id={window}] border pixel 1")
        x0, y0, x1, y1 = self.rect(window)
        self.assertPixels([(x0 + 1, y0 + 1), (x1 - 1, y1 - 1), (x0 + 5, y0 + 5)], BLUE)

    def test_a_reload_takes_the_new_radius(self):
        window = self.open("test.app")
        self.assertPixels(self.cut(window), BLACK)
        self.assertEqual(self.reload(""), [{"success": True}])
        self.assertPixels(self.cut(window) + self.kept(window), BLUE)
        self.assertEqual(self.reload(f"corner_radius {RADIUS}\n"), [{"success": True}])
        self.assertPixels(self.cut(window), BLACK)
        self.assertPixels(self.kept(window), BLUE)

    def test_a_click_in_a_corners_rows_goes_to_the_window(self):
        client = self.wm.open_window("test.app", "test.app", stdin=True, stdout=True,
                                     scrolls=True)
        window = self.ipc.next_event(EVENT_WINDOW, lambda e: e["change"] == "new")["container"]["id"]
        self.clients[window] = client
        self.settle()
        x0, y0, x1, y1 = self.rect(window)
        # The first click makes the seat's pointer, which the client then takes
        self.click_at((x0 + x1) // 2, (y0 + y1) // 2)
        self.assertEqual(client.stdout.readline(), b"pointing\n")
        for x, y in ((x0 + x1) // 2, y0 + 2), ((x0 + x1) // 2, y1 - 2):
            self.click_at(x, y)
            line = None
            while line != b"button\n":  # after "pointing", once it has a pointer
                ready, _, _ = select.select([client.stdout], [], [], TIMEOUT)
                self.assertTrue(ready, f"no click at {x},{y} reached the window")
                line = client.stdout.readline()
                self.assertTrue(line, "the client closed")


SUBSURFACE = os.path.join(os.path.dirname(os.path.dirname(os.environ.get(
    "TIDESWAY", os.path.join(tidesway.ROOT, "build", "sway", "sway")))), "tests", "tidesway", "subsurface")
GREEN, RED = (0, 255, 0), (255, 0, 0)


class SubsurfaceTest(LookHelpers):
    """tests/tidesway/subsurface.c's window: blue, with a green subsurface
    over its top four rows, all within a rounded corner's rows."""

    def setUp(self):
        super().setUp()
        self.need(os.path.exists(SUBSURFACE), f"no test client at {SUBSURFACE}; build tidesway")
        client = subprocess.Popen([SUBSURFACE, "test.sub"], env=self.wm.env,
                                  stdin=subprocess.PIPE, stdout=subprocess.PIPE)
        self.wm.clients.append(client)
        self.addCleanup(client.stdout.close)
        self.addCleanup(client.stdin.close)
        self.window = self.ipc.next_event(
            EVENT_WINDOW, lambda e: e["change"] == "new")["container"]["id"]
        self.clients[self.window] = client

    def test_a_subsurface_in_a_corners_rows_is_shown_and_its_corners_cut(self):
        x0, y0, x1, y1 = self.rect(self.window)
        middle = (x0 + x1) // 2
        self.assertPixels([(middle, y0), (middle, y0 + 3), (x0 + RADIUS, y0)], GREEN)
        self.assertPixels([(middle, y0 + 4), (middle, y1)], BLUE)
        self.assertPixels(self.cut(self.window), BLACK)

    def test_a_subsurfaces_own_commit_in_a_corners_rows_is_shown_and_told_its_frame(self):
        self.settle()
        x0, y0, x1, y1 = self.rect(self.window)
        # Nothing else asks for a frame: the subsurface commits alone
        self.say(self.window, "recolor", "frame")
        self.assertEqual(self.pixel((x0 + x1) // 2, y0 + 1), RED)


    def test_a_subsurface_that_goes_is_gone_from_a_corners_rows(self):
        self.settle()
        x0, y0, x1, y1 = self.rect(self.window)
        self.say(self.window, "hide", "hidden")
        self.assertEqual(self.pixel((x0 + x1) // 2, y0 + 1), BLUE)

    def test_a_subsurface_its_window_moves_is_shown_where_it_went(self):
        self.settle()
        x0, y0, x1, y1 = self.rect(self.window)
        middle = (x0 + x1) // 2
        self.say(self.window, "move", "moved")
        self.assertEqual(self.pixel(middle, y0 + 1), BLUE)
        self.assertEqual(self.pixel(middle, y1 - 1), GREEN)

    def test_a_windows_opacity_changing_alone_reaches_its_corners_rows(self):
        self.settle()
        x0, y0, x1, y1 = self.rect(self.window)
        middle = (x0 + x1) // 2
        self.say(self.window, "fade", "faded")
        # Half as blue over black, in the rows between the corners and in theirs
        half = tuple(c // 2 for c in BLUE)
        between = self.pixel(middle, (y0 + y1) // 2)
        self.assertTrue(self.looks(between, half), between)
        for y in y0 + 6, y1 - 1:
            self.assertTrue(self.looks(self.pixel(middle, y), half), (y, self.pixel(middle, y)))


class InputRegionTest(LookHelpers):
    CONFIG = (f"corner_radius {RADIUS}\n"
              'for_window [app_id="^test.sub$"] floating enable, '
              "resize set 300 200, move position 100 100\n")

    def test_a_click_where_a_window_takes_no_input_goes_to_the_window_under_it(self):
        tiled = self.open("test.tiled")
        client = subprocess.Popen([SUBSURFACE, "test.sub"], env=self.wm.env,
                                  stdin=subprocess.PIPE, stdout=subprocess.PIPE)
        self.wm.clients.append(client)
        self.addCleanup(client.stdout.close)
        self.addCleanup(client.stdin.close)
        floating = self.ipc.next_event(EVENT_WINDOW, lambda e: e["change"] == "new"
                                       and e["container"]["app_id"] == "test.sub")["container"]["id"]
        self.clients[floating] = client
        self.settle()
        self.assertTrue(self.windows()[floating]["focused"])
        self.say(floating, "hole", "holed")
        x0, y0, x1, y1 = self.rect(floating)
        # In a corner's rows, below the subsurface, as between them
        self.click_at((x0 + x1) // 2, y0 + 8)
        self.assertTrue(self.windows()[tiled]["focused"])


class FloatingRoundedTest(LookHelpers):
    CONFIG = f"corner_radius {RADIUS}\n" + FLOATING

    def test_a_floating_windows_corners_follow_it_as_it_moves_and_resizes(self):
        floating = self.open("test.float")
        self.assertPixels(self.cut(floating), BLACK)
        self.assertPixels(self.kept(floating), BLUE)
        was = self.rect(floating)
        self.command(self.connect(), f"[con_id={floating}] resize set 420 260")
        self.command(self.connect(), f"[con_id={floating}] move position 30 40")
        self.assertNotEqual(self.rect(floating), was)
        self.assertPixels(self.cut(floating), BLACK)
        self.assertPixels(self.kept(floating), BLUE)

    def test_a_window_too_small_for_its_corners_is_square(self):
        floating = self.open("test.float")
        self.assertPixels(self.cut(floating), BLACK)
        self.command(self.connect(), f"[con_id={floating}] resize set 200 {2 * RADIUS}")
        self.settle()
        self.assertEqual(self.windows()[floating]["rect"]["height"], 2 * RADIUS)
        self.assertPixels(self.cut(floating), BLUE)


class RoundedDimTest(LookHelpers):
    CONFIG = (f"corner_radius {RADIUS}\ndim_inactive {STRENGTH}\n" + FLOATING)

    def test_a_floating_windows_corners_show_the_window_behind(self):
        tiled = self.open("test.tiled")
        floating = self.open("test.float")
        # The floating window is focused, so the tile behind it is dimmed
        self.assertPixels(self.cut(floating), DIMMED)
        self.assertPixels(self.kept(floating), BLUE)
        self.assertPixels(self.cut(tiled), BLACK)

    def test_a_dimmed_window_is_dimmed_to_its_rounded_corners_and_no_further(self):
        tiled = self.open("test.tiled")
        floating = self.open("test.float")
        self.command(self.connect(), f"[con_id={tiled}] focus")
        # Dimmed in its corners' rows too, and the tile in its corners isn't
        self.assertPixels(self.kept(floating), DIMMED)
        self.assertPixels(self.cut(floating), BLUE)


    def test_a_dimmed_window_made_see_through_is_as_dark_in_its_corners_rows(self):
        first = self.open("test.first")
        self.open("test.second")  # focused, so the first is dimmed
        self.command(self.connect(), f"[con_id={first}] opacity 0.5")
        self.settle()
        x0, y0, x1, y1 = self.rect(first)
        middle = (x0 + x1) // 2
        # Over black: half there, then dimmed by half of the strength
        expected = tuple(c * 0.5 * (1 - STRENGTH * 0.5) for c in BLUE)
        between = self.pixel(middle, (y0 + y1) // 2)
        self.assertTrue(self.looks(between, expected), between)
        for y in y0 + 2, y1 - 2:
            self.assertTrue(self.looks(self.pixel(middle, y), expected), (y, self.pixel(middle, y)))


class ConfigTest(LookHelpers):
    def test_a_bad_line_fails_the_reload_naming_it(self):
        for line, error in (
                ("corner_radius -1", "corner_radius takes a number of pixels from 0 to 100"),
                ("corner_radius 6.5", "corner_radius takes a number of pixels from 0 to 100"),
                ("shadows maybe", "Expected shadows enable|disable"),
                ("shadow_blur_radius far", "shadow_blur_radius takes a number of pixels from 0 to 100"),
                ("shadow_color black", "shadow_color takes a color as #RRGGBB or #RRGGBBAA")):
            with self.subTest(line=line):
                result, = self.reload(line + "\n")
                self.assertFalse(result["success"])
                self.assertIn(error, result["error"])


class X11Helpers(x11.X11Helpers, LookHelpers):
    """Helpers, with no tests of their own."""

    def setUp(self):
        x11.X11Helpers.setUp(self)

    def assertTopLeft(self, cut):
        """An X11 window's top left corner is cut, or square, once a click
        in its top rows has reached it: it's white."""
        client = self.wm.open_x11_window("test.x11", "X11", report=True)
        self.opened(client)
        self.assertEqual(self.clicked(client, 40, 2)[:7], b"button ")
        white = (255, 255, 255)
        self.assertEqual(self.pixel(40, 2), white)
        self.assertEqual(self.pixel(5, 5), white)
        self.assertEqual(self.pixel(1, 1), BLACK if cut else white)


class X11RoundedTest(X11Helpers):
    CONFIG = f"corner_radius {RADIUS}\noutput HEADLESS-1 mode 800x600@50Hz scale 1\n"

    def test_an_x11_windows_corners_are_cut(self):
        self.assertTopLeft(cut=True)


class X11UnscaledTest(X11Helpers):
    CONFIG = f"corner_radius {RADIUS}\nxwayland_scaling disable\n" + x11.OUTPUT

    def test_an_x11_window_in_the_outputs_pixels_is_square(self):
        self.assertTopLeft(cut=False)


WHITE = "#ffffffff"
BLUR = 8


class ShadowHelpers(LookHelpers):
    """Helpers, with no tests of their own."""

    def lightness(self, x, y):
        return sum(self.pixel(x, y))

    def assertShadowed(self, floating):
        """A white shadow lightens the tile around the floating window,
        less the further out, and not at all past BLUR."""
        self.settle()
        x0, y0, x1, y1 = self.rect(floating)
        middle_x, middle_y = (x0 + x1) // 2, (y0 + y1) // 2
        blue = sum(BLUE)
        for near, far, past in (
                ((x0 - 1, middle_y), (x0 - 5, middle_y), (x0 - BLUR - 2, middle_y)),
                ((x1 + 1, middle_y), (x1 + 5, middle_y), (x1 + BLUR + 2, middle_y)),
                ((middle_x, y0 - 1), (middle_x, y0 - 5), (middle_x, y0 - BLUR - 2)),
                ((middle_x, y1 + 1), (middle_x, y1 + 5), (middle_x, y1 + BLUR + 2)),
                ((x0 - 1, y0 - 1), (x0 - 4, y0 - 4), (x0 - BLUR, y0 - BLUR)),
                ((x1 + 1, y1 + 1), (x1 + 4, y1 + 4), (x1 + BLUR, y1 + BLUR))):
            with self.subTest(near=near):
                self.assertGreater(self.lightness(*near), self.lightness(*far))
                self.assertGreater(self.lightness(*far), blue + 9)
                self.assertTrue(self.looks(self.pixel(*past), BLUE), self.pixel(*past))


class ShadowTest(ShadowHelpers):
    CONFIG = f"shadows enable\nshadow_color {WHITE}\nshadow_blur_radius {BLUR}\n" + FLOATING

    def test_a_floating_window_has_a_shadow_and_a_tile_has_none(self):
        left = self.open("test.left")
        right = self.open("test.right")
        floating = self.open("test.float")
        self.assertShadowed(floating)
        # Where the tiles meet, each is its own color to its edge
        x0, y0, x1, y1 = self.rect(left)
        self.assertPixels([(x1 - 1, y1 - 50), (x1 + 2, y1 - 50)], BLUE)
        self.assertPixels(self.kept(floating) + self.cut(floating), BLUE)
        self.assertTrue(self.windows()[right]["rect"]["x"] > x1)

    def test_a_click_on_a_shadow_goes_to_the_window_under_it(self):
        tiled = self.open("test.tiled")
        floating = self.open("test.float")
        self.assertTrue(self.windows()[floating]["focused"])
        x0, y0, x1, y1 = self.rect(floating)
        self.click_at(x0 - 3, (y0 + y1) // 2)
        self.assertTrue(self.windows()[tiled]["focused"])

    def test_a_window_no_longer_floating_loses_its_shadow(self):
        self.open("test.tiled")
        floating = self.open("test.float")
        self.assertShadowed(floating)
        x0, y0, x1, y1 = self.rect(floating)
        self.command(self.connect(), f"[con_id={floating}] floating disable")
        self.assertPixels([(x0 - 2, (y0 + y1) // 2), (x1 + 2, (y0 + y1) // 2)], BLUE)

    def test_a_reload_takes_the_shadows_off(self):
        self.open("test.tiled")
        floating = self.open("test.float")
        self.assertShadowed(floating)
        x0, y0, x1, y1 = self.rect(floating)
        self.assertEqual(self.reload(FLOATING), [{"success": True}])
        self.assertPixels([(x0 - 2, (y0 + y1) // 2), (x1 + 2, (y0 + y1) // 2)], BLUE)


class MaximizedShadowTest(ShadowHelpers):
    OUTPUTS = 2
    CONFIG = ShadowTest.CONFIG

    def test_a_maximized_windows_shadow_doesnt_reach_the_next_output(self):
        floating = self.open("test.float")
        first = min(self.connect().request(GET_OUTPUTS), key=lambda o: o["rect"]["x"])["rect"]
        beside = (first["x"] + first["width"] + 1, 200)  # level with the window
        self.command(self.connect(), f"[con_id={floating}] move position {first['width'] - 300} 100")
        self.settle()
        self.assertGreater(self.lightness(*beside), 9)  # its shadow, on the next output
        self.command(self.connect(), f"[con_id={floating}] maximize enable")
        self.assertPixels([beside], BLACK)


class RoundedShadowTest(ShadowHelpers):
    CONFIG = (f"corner_radius {RADIUS}\nshadows enable\nshadow_color {WHITE}\n"
              f"shadow_blur_radius {BLUR}\n" + FLOATING)

    def test_a_rounded_windows_shadow_follows_its_corners(self):
        self.open("test.tiled")
        floating = self.open("test.float")
        self.settle()
        x0, y0, x1, y1 = self.rect(floating)
        middle = (y0 + y1) // 2
        blue = sum(BLUE)
        # In the corner the window leaves, and fading out from it
        self.assertGreater(self.lightness(x0 + 1, y0 + 1), blue + 9)
        self.assertGreater(self.lightness(x0 + 1, y0 + 1), self.lightness(x0 - 1, y0 - 1))
        # Beside the corner it's further from the window than beside its side
        self.assertGreater(self.lightness(x0 - 3, middle), self.lightness(x0 - 3, y0 + 1))
        self.assertPixels(self.kept(floating), BLUE)


class ScaledLookTest(LookHelpers):
    CONFIG = f"corner_radius {RADIUS}\noutput * scale 2\n"

    def pixel(self, x, y):
        """The last of the output's four pixels in the layout's one."""
        shot = subprocess.run(["grim", "-t", "ppm", "-g", f"{x},{y} 1x1", "-"],
                              env=self.wm.env, check=True, timeout=TIMEOUT,
                              capture_output=True).stdout
        fields = shot.split(maxsplit=4)
        self.assertEqual(fields[:4], [b"P6", b"2", b"2", b"255"], shot[:32])
        return tuple(fields[4][9:12])

    def test_corners_are_cut_at_a_scale_of_two(self):
        window = self.open("test.app")
        self.assertPixels(self.cut(window), BLACK)
        self.assertPixels(self.kept(window), BLUE)
        # The corner's edge is smoothed in the output's pixels, not the
        # layout's: the edge crosses this pixel of the layout, and the
        # output's last pixel in it is wholly inside
        x0, y0, x1, y1 = self.rect(window)
        self.assertPixels([(x0 + 2, y0 + 5)], BLUE)


if __name__ == "__main__":
    unittest.main()
