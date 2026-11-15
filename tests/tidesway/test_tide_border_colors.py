"""Border colors as tidewm has them, where tidewm's own tests set them only
in the config file: a client.* command at runtime repaints the windows."""

import unittest

import tidesway  # noqa: F401  (sets up harness)
import test_borders  # tidewm's, from the checkout
from test_borders import FOCUSED, INACTIVE, NAVY, UNFOCUSED, WIDE, YELLOW, ColorHelpers


class RuntimeInactiveColorTest(ColorHelpers):
    """Two monitors, the tiles on the first, focus on the second: the
    focused tile there is focused_inactive."""
    CONFIG = WIDE + FOCUSED + UNFOCUSED
    OUTPUTS = 2
    setUp = test_borders.InactiveColorTest.setUp

    def test_a_focused_inactive_command_colors_the_window_at_once(self):
        self.open("test.there")
        self.assertEqual(self.colors(), (NAVY, NAVY))  # as unfocused, without the line
        self.command(self.connect(), INACTIVE.strip())
        self.assertEqual(self.colors(), (NAVY, YELLOW))


if __name__ == "__main__":
    unittest.main()
