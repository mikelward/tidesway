"""Borders as tidewm has them, beyond what tidewm's tests say: a floating
window keeps its size, border and all, through a reload to a wider one."""

import unittest

import tidesway  # noqa: F401  (sets up harness)
from test_borders import FLOAT, BorderHelpers  # tidewm's, from the checkout


class ReloadBorderTest(BorderHelpers):
    CONFIG = FLOAT

    def test_a_reload_with_a_wider_one_keeps_a_floating_windows_size(self):
        floating = self.open("test.float", size="300x200")
        self.assertEqual(self.size(floating), (302, 202))
        self.reload("default_border pixel 5\n" + FLOAT)
        self.assertEqual((self.border(floating), self.size(floating)), (("pixel", 5), (302, 202)))


if __name__ == "__main__":
    unittest.main()
