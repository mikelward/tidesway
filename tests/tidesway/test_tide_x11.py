"""X11 windows in their outputs' own pixels, with xwayland_scaling
disable, as tidewm has them, where tidewm's tests can't say so for
tidesway: tide's look has no borders, where theirs assume a 1px one, and
a drag over another window loses its press to Xwayland, as sway moves the
pointer off the window as the button comes up, in the same burst."""

import unittest

import tidesway  # noqa: F401  (sets up harness)
import test_xwayland as x11  # noqa: E402  (tidewm's, from the checkout)


class OneOutputTest(x11.X11Helpers):
    CONFIG = "xwayland_scaling disable\n" + x11.OUTPUT

    def test_a_drag_in_an_x11_window_is_in_its_pixels(self):
        client, _ = self.open()
        self.sized(client, 800, 600)
        self.pointer(("move", "20", "16"), ("press", "left"),
                     ("move", "60", "40"), ("release", "left"))
        self.assertEqual(self.heard(client, b"button "), b"button 25 20\n")
        self.assertEqual(self.heard(client, b"motion "), b"motion 75 50\n")

    def test_all_of_an_x11_window_is_shown(self):
        # Clipped in the layout's units, its bottom right was cut off
        client, _ = self.open()
        self.sized(client, 800, 600)
        self.assertEqual(self.clicked(client, 600, 450), b"button 750 562\n")


class TwoOutputTest(x11.X11Helpers):
    """A monitor at 1 beside one at 2, as tidewm's TwoOutputTest"""
    OUTPUTS = 2
    CONFIG = x11.TwoOutputTest.CONFIG

    def test_a_floating_x11_window_opens_in_the_pixels_of_its_own_monitor(self):
        self.command(self.connect(), "focus output HEADLESS-2")
        client = self.wm.open_x11_window("test.float", "Float", report=True)
        window = self.opened(client)
        self.assertEqual(window["output"], "HEADLESS-2")
        rect = self.windows()[window["id"]]["rect"]
        self.assertEqual((rect["width"], rect["height"]), (50, 50))

    def test_a_fixed_size_x11_window_keeps_its_pixels_on_another_monitor(self):
        client = self.wm.open_x11_window("test.x11", "X11", report=True, fixed_size="400x300")
        window = self.opened(client)
        self.sized(client, 400, 300)
        for output, size in [("HEADLESS-1", (400, 300)), ("HEADLESS-2", (200, 150))]:
            self.command(self.connect(), f"[con_id={window['id']}] move to output {output}")
            rect = self.windows()[window["id"]]["rect"]
            self.assertEqual((rect["width"], rect["height"]), size, output)

    def test_a_floating_x11_window_moved_by_place_keeps_its_pixels(self):
        # As a drag across the monitors moves it
        self.command(self.connect(), "focus output HEADLESS-1")
        client = self.wm.open_x11_window("test.float", "Float", report=True)
        window = self.opened(client)
        self.assertEqual(window["output"], "HEADLESS-1")
        self.command(self.connect(), f"[con_id={window['id']}] move position 900 100")
        rect = self.windows()[window["id"]]["rect"]
        self.assertEqual((rect["width"], rect["height"]), (50, 50))

    def test_a_floating_x11_window_keeps_its_pixels_off_a_disabled_monitor(self):
        self.command(self.connect(), "focus output HEADLESS-2")
        client = self.wm.open_x11_window("test.float", "Float", report=True)
        window = self.opened(client)
        self.assertEqual(window["output"], "HEADLESS-2")
        self.command(self.connect(), "output HEADLESS-2 disable")
        rect = self.windows()[window["id"]]["rect"]
        self.assertEqual((rect["width"], rect["height"]), (100, 100))

    def test_a_floating_x11_window_keeps_its_pixels_from_the_scratchpad(self):
        self.command(self.connect(), "focus output HEADLESS-1")
        client = self.wm.open_x11_window("test.float", "Float", report=True)
        window = self.opened(client)
        self.assertEqual(window["output"], "HEADLESS-1")
        self.command(self.connect(), f"[con_id={window['id']}] move scratchpad")
        self.command(self.connect(), "focus output HEADLESS-2")
        self.command(self.connect(), f"[con_id={window['id']}] scratchpad show")
        shown = self.windows()[window["id"]]
        self.assertEqual(shown["output"], "HEADLESS-2")
        self.assertEqual((shown["rect"]["width"], shown["rect"]["height"]), (50, 50))


class AssignedTest(x11.X11Helpers):
    """A floating X11 window assigned to the monitor at 2, opened with the
    one at 1 focused"""
    OUTPUTS = 2
    CONFIG = x11.TwoOutputTest.CONFIG + 'assign [class="^test.float$"] output HEADLESS-2\n'

    def test_an_x11_window_opens_in_the_pixels_of_the_monitor_its_assigned(self):
        self.command(self.connect(), "focus output HEADLESS-1")
        client = self.wm.open_x11_window("test.float", "Float", report=True)
        window = self.opened(client)
        self.assertEqual(window["output"], "HEADLESS-2")
        rect = self.windows()[window["id"]]["rect"]
        self.assertEqual((rect["width"], rect["height"]), (50, 50))


class MovedOutputTest(x11.X11Helpers):
    """Xwayland hears where an output is as it moves"""
    OUTPUTS = 2
    CONFIG = ("output HEADLESS-1 mode 800x600@50Hz scale 1 position 0 0\n"
              "output HEADLESS-2 mode 800x600@50Hz scale 1 position 800 0\n")

    def test_the_x11_screen_follows_a_moved_output(self):
        client = self.wm.open_x11_window("test.x11", "X11", report=True)
        self.assertEqual(self.line(client), b"screen 1600 600\n")
        self.command(self.connect(), "output HEADLESS-2 position 1600 0")
        client = self.wm.open_x11_window("test.x11", "X11", report=True)
        self.assertEqual(self.line(client), b"screen 2400 600\n")


class EnabledOutputTest(x11.X11Helpers):
    """A monitor at 2 enabled left of one at 1: X11's space spreads by 2,
    so the one at 1 moves from 800 to 1600 in it"""
    OUTPUTS = 2
    CONFIG = ("xwayland_scaling disable\n"
              "output HEADLESS-1 mode 800x600@50Hz scale 2 position 0 0\n"
              "output HEADLESS-2 mode 800x600@50Hz scale 1 position 800 0\n")

    def test_the_x11_screen_spreads_as_a_scaled_output_comes(self):
        self.command(self.connect(), "output HEADLESS-1 disable")
        client = self.wm.open_x11_window("test.x11", "X11", report=True)
        self.assertEqual(self.line(client), b"screen 1600 600\n")
        self.command(self.connect(), "output HEADLESS-1 enable")
        client = self.wm.open_x11_window("test.x11", "X11", report=True)
        self.assertEqual(self.line(client), b"screen 2400 600\n")


if __name__ == "__main__":
    unittest.main()
