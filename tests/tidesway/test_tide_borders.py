"""Borders as tidewm has them, from a border tidewm's own tests take from
its config.h, 1px wide, where tide's look, which tidesway starts from, has
none (look.conf's default_border none)."""

import unittest

import tidesway  # noqa: F401  (sets up harness)
from test_borders import FLOAT, BorderHelpers  # tidewm's, from the checkout


class ReloadBorderTest(BorderHelpers):
    CONFIG = "default_border pixel 1\n" + FLOAT

    def test_a_reload_with_none_takes_every_border_away(self):
        tiled = self.open("test.tiled")
        floating = self.open("test.float", size="300x200")
        self.assertEqual(self.size(floating), (302, 202))
        self.reload("default_border none\n" + FLOAT)
        self.assertEqual([self.border(tiled), self.border(floating)],
                         [("none", 0), ("none", 0)])
        # Its place and size stay, border and all
        self.assertEqual(self.size(floating), (302, 202))

    def test_a_reload_with_a_wider_one_keeps_a_floating_windows_size(self):
        floating = self.open("test.float", size="300x200")
        self.reload("default_border pixel 5\n" + FLOAT)
        self.assertEqual((self.border(floating), self.size(floating)), (("pixel", 5), (302, 202)))


if __name__ == "__main__":
    unittest.main()
