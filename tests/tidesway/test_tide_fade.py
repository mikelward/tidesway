"""Fades on screen, by grim's pixels: with fade_duration, a window fades
in as it opens and the dim fades as focus moves, and without it both are
at once. A fade_duration of an hour keeps a fade near its start for as
long as a test looks, and one of a millisecond is over by then."""

import unittest

import tidesway  # noqa: F401  (sets up harness)
from harness import RUN_COMMAND
from test_tide_dimming import BLUE, DIMMED, STRENGTH, DimOnScreenHelpers

BLACK = (0, 0, 0)  # nothing: no window and no wallpaper
HOUR = "fade_duration 3600000\n"
DIM = f"dim_inactive {STRENGTH}\n"


def nearer(color, a, b):
    """Whether color is nearer a than b."""
    def distance(x, y):
        return sum((i - j) ** 2 for i, j in zip(x, y))
    return distance(color, a) < distance(color, b)


class FadeHelpers(DimOnScreenHelpers):
    """Helpers, with no tests of their own."""

    def middle(self, con_id):
        r = self.windows()[con_id]["rect"]
        return r["x"] + r["width"] // 2, r["y"] + r["height"] // 2

    def reload(self, config):
        self.wm.write_config(config)
        result, = self.connect().request(RUN_COMMAND, "reload")
        return result


class FadeInTest(FadeHelpers):
    CONFIG = HOUR + DIM

    def test_a_window_fades_in_as_it_opens(self):
        window = self.open("test.app")
        self.settle()
        color = self.pixel(*self.middle(window))
        self.assertTrue(nearer(color, BLACK, BLUE), f"{color} already shows")

    def test_a_window_that_was_open_before_fades_stays_shown(self):
        self.assertTrue(self.reload(DIM)["success"])
        window = self.open("test.app")
        self.settle()
        self.assertTrue(self.reload(self.CONFIG)["success"])
        self.assertTrue(self.looks(self.pixel(*self.middle(window)), BLUE))

    def test_the_dim_fades_as_focus_moves(self):
        # Both shown in full first, as their own fades in would hide the dim's
        self.assertTrue(self.reload(DIM)["success"])
        first = self.open("test.first")
        second = self.open("test.second")
        self.command(self.connect(), f"[con_id={first}] focus")
        self.settle()
        self.assertTrue(self.looks(self.pixel(*self.middle(second)), DIMMED))
        self.assertTrue(self.reload(self.CONFIG)["success"])
        # A reload leaves the dim as it was: it doesn't fade in again
        self.assertTrue(self.looks(self.pixel(*self.middle(second)), DIMMED))
        self.command(self.connect(), f"[con_id={second}] focus")
        # The first is fading to dimmed, and the second from it
        self.assertTrue(self.windows()[first]["dimmed"])
        self.assertFalse(self.windows()[second]["dimmed"])
        color = self.pixel(*self.middle(first))
        self.assertTrue(nearer(color, BLUE, DIMMED), f"{color} is dimmed already")
        color = self.pixel(*self.middle(second))
        self.assertTrue(nearer(color, DIMMED, BLUE), f"{color} isn't dimmed any more")


class RoundedFadeTest(FadeHelpers):
    CONFIG = HOUR + "corner_radius 12\n"

    def test_a_rounded_windows_corners_rows_fade_in_with_it(self):
        window = self.open("test.app")
        self.settle()
        r = self.windows()[window]["rect"]
        x = r["x"] + r["width"] // 2
        for y in r["y"] + 2, r["y"] + r["height"] // 2, r["y"] + r["height"] - 3:
            color = self.pixel(x, y)
            self.assertTrue(nearer(color, BLACK, BLUE), f"{color} at {x},{y} already shows")


class ShortFadeTest(FadeHelpers):
    CONFIG = "fade_duration 1\n" + DIM

    def test_a_fade_ends_with_the_window_shown_and_the_dim_whole(self):
        first = self.open("test.first")
        second = self.open("test.second")
        self.assertShown({first: True, second: False})
        self.command(self.connect(), f"[con_id={first}] focus")
        self.assertShown({first: False, second: True})


class NoFadeTest(FadeHelpers):
    CONFIG = DIM

    def test_without_fade_duration_a_window_and_its_dim_show_at_once(self):
        first = self.open("test.first")
        second = self.open("test.second")
        self.assertShown({first: True, second: False})


class FadeConfigTest(FadeHelpers):
    CONFIG = ""

    def test_a_bad_fade_duration_fails_the_reload_naming_it(self):
        for line in "fade_duration -1", "fade_duration 3600001", "fade_duration soon", "fade_duration":
            with self.subTest(line=line):
                result = self.reload(line + "\n")
                self.assertFalse(result["success"])
                self.assertIn("fade_duration takes milliseconds from 0 to 3600000", result["error"])


if __name__ == "__main__":
    unittest.main()
