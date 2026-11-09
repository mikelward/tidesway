"""Dimming on screen, where tidewm's suite only asks the IPC: grim's pixels
of an unfocused window are darkened at dim_inactive's strength, to its
edges, as tide's layouts, sway's own and floating lay it out, and the
focused window's and a dimmed window's menu aren't."""

import select
import subprocess
import unittest

import tidesway  # noqa: F401  (sets up harness)
from harness import EVENT_WINDOW, GET_OUTPUTS, TIMEOUT, TidewmTest

STRENGTH = 0.5
# The test client's color, and it under the dim
BLUE = (0x33, 0x66, 0x99)
DIMMED = tuple(c * (1 - STRENGTH) for c in BLUE)


class DimOnScreenHelpers(TidewmTest):
    """Helpers, with no tests of their own."""
    CONFIG = f"dim_inactive {STRENGTH}\n"

    def setUp(self):
        super().setUp()
        self.ipc = self.connect()
        self.ipc.subscribe("window")
        self.clients = {}

    def open(self, app_id):
        """Opens a window driven by its stdin, and returns its id."""
        client = self.wm.open_window(app_id, app_id, stdin=True, stdout=True)
        event = self.ipc.next_event(EVENT_WINDOW, lambda e: e["change"] == "new"
                                    and e["container"]["app_id"] == app_id)
        self.clients[event["container"]["id"]] = client
        return event["container"]["id"]

    def say(self, con_id, line, answer):
        """Writes line to the window's client and waits for its answer."""
        client = self.clients[con_id]
        client.stdin.write(line.encode() + b"\n")
        client.stdin.flush()
        while True:
            ready, _, _ = select.select([client.stdout], [], [], TIMEOUT)
            self.assertTrue(ready, f"no {answer!r} from {con_id}")
            got = client.stdout.readline()
            self.assertTrue(got, f"{con_id}'s client closed before {answer!r}")
            if got == answer.encode() + b"\n":
                return

    def settle(self):
        """Waits until sway has every window's answer to its last configure,
        so the layout on screen is the one GET_TREE says: a client acks a
        configure as it has it, which is before its first answer to commit,
        and sway has the ack by its second."""
        for con_id in self.clients:
            self.say(con_id, "commit", "committed")
            self.say(con_id, "commit", "committed")

    def pixel(self, x, y):
        shot = subprocess.run(["grim", "-t", "ppm", "-g", f"{x},{y} 1x1", "-"],
                              env=self.wm.env, check=True, timeout=TIMEOUT,
                              capture_output=True).stdout
        # P6, width, height, maximum, then the pixel
        fields = shot.split(maxsplit=4)
        self.assertEqual(fields[:4], [b"P6", b"1", b"1", b"255"], shot[:32])
        return tuple(fields[4][:3])

    def looks(self, color, expected):
        return all(abs(c - e) <= 3 for c, e in zip(color, expected))

    def corners(self, con_id):
        """Points just inside each corner of the window."""
        r = self.windows()[con_id]["rect"]
        x0, y0, x1, y1 = r["x"] + 3, r["y"] + 3, r["x"] + r["width"] - 4, r["y"] + r["height"] - 4
        return [(x0, y0), (x1, y0), (x0, y1), (x1, y1)]

    def assertPixels(self, points, expected):
        """Each point is the color expected, read with nothing but grim."""
        self.settle()
        wrong = [f"{x},{y}: {self.pixel(x, y)}" for x, y in points
                 if not self.looks(self.pixel(x, y), expected)]
        self.assertEqual(wrong, [], f"not {expected}")

    def assertShown(self, dimmed):
        """Each window, by id, is darkened corner to corner, or not at all."""
        self.settle()
        wrong = []
        for con_id, dim in dimmed.items():
            expected = DIMMED if dim else BLUE
            for x, y in self.corners(con_id):
                color = self.pixel(x, y)
                if not self.looks(color, expected):
                    wrong.append(f"window {con_id} at {x},{y}: {color}, not {expected}")
        self.assertEqual(wrong, [])


class TideLayoutDimTest(DimOnScreenHelpers):
    def test_unfocused_windows_are_darkened_and_the_focused_one_isnt(self):
        first = self.open("test.first")
        second = self.open("test.second")
        self.assertShown({first: True, second: False})
        self.command(self.connect(), f"[con_id={first}] focus")
        self.assertShown({first: False, second: True})

    def test_the_dim_follows_a_window_as_its_tile_changes(self):
        first = self.open("test.first")
        second = self.open("test.second")
        third = self.open("test.third")
        self.assertShown({first: True, second: True, third: False})
        self.command(self.connect(), "layout monocle")
        self.command(self.connect(), "layout tile")
        self.assertShown({first: True, second: True, third: False})

    def test_a_dimmed_windows_menu_isnt_dimmed(self):
        first = self.open("test.first")
        second = self.open("test.second")
        windows = self.windows()
        # The one in the output's top left corner, where its menu stays whole
        corner, other = sorted([first, second], key=lambda i: (windows[i]["rect"]["x"],
                                                               windows[i]["rect"]["y"]))
        self.command(self.connect(), f"[con_id={other}] focus")
        self.say(corner, "menu", "menu")
        self.settle()
        rect = self.windows()[corner]["rect"]
        self.assertTrue(self.looks(self.pixel(rect["x"] + 10, rect["y"] + 10), BLUE))
        self.assertTrue(self.looks(self.pixel(rect["x"] + rect["width"] // 2,
                                              rect["y"] + rect["height"] // 2), DIMMED))


class TitleDimTest(DimOnScreenHelpers):
    def test_a_window_retitled_picture_in_picture_stops_dimming_and_back(self):
        first = self.open("test.first")
        self.open("test.second")
        corners = self.corners(first)
        # Nothing listens for window events, whose description would
        # bring the dim up to date, nor asks for the tree from here on. sway
        # has seen the listener go by the time it answers a later roundtrip.
        self.ipc.close()
        self.assertPixels(corners, DIMMED)
        self.say(first, "title Picture-in-Picture\ncommit", "committed")
        self.assertPixels(corners, BLUE)
        self.say(first, "title Something else\ncommit", "committed")
        self.assertPixels(corners, DIMMED)


class SwayLayoutDimTest(DimOnScreenHelpers):
    def test_windows_in_sways_layouts_dim(self):
        self.command(self.connect(), "layout splitv")
        first = self.open("test.first")
        second = self.open("test.second")
        self.assertShown({first: True, second: False})


class FloatingDimTest(DimOnScreenHelpers):
    CONFIG = (f"dim_inactive {STRENGTH}\n"
              'for_window [app_id="^test.float$"] floating enable\n')

    def test_a_floating_window_dims(self):
        floating = self.open("test.float")
        tiled = self.open("test.tiled")
        self.assertShown({floating: True})
        self.command(self.connect(), f"[con_id={floating}] focus")
        self.assertShown({floating: False, tiled: True})


class LinearFilterDimTest(DimOnScreenHelpers):
    CONFIG = (f"dim_inactive {STRENGTH}\n"
              "output * scale_filter linear\n")

    def test_the_dim_is_even_to_the_edges_with_a_linear_filter(self):
        # One pixel stretched by a linear filter would fade toward the edges
        output, = self.connect().request(GET_OUTPUTS)
        self.assertEqual(output["scale_filter"], "linear")
        first = self.open("test.first")
        second = self.open("test.second")
        self.assertShown({first: True, second: False})


if __name__ == "__main__":
    unittest.main()
