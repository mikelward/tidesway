"""A layer surface that took the keyboard on demand, as a notification's
reply field once clicked, keeps it while the layers are arranged again, as
when a bar opens: only focus you move, an exclusive layer surface or the
lock takes it. sway dropped it whenever it arranged the layers."""

import os
import select
import subprocess
import unittest

import tidesway  # noqa: F401  (sets up harness)
from harness import EVENT_WINDOW, GET_OUTPUTS, ROOT, TIMEOUT, TidewmTest

POINTER = os.path.join(ROOT, "tests", "pointer")
LAUNCHER = os.path.join(ROOT, "tests", "launcher")


class OnDemandArrangeTest(TidewmTest):
    def setUp(self):
        super().setUp()
        # The headless seat has a keyboard once a virtual one comes
        self.press("-k", "Escape")

    def launch(self, kind):
        client = subprocess.Popen([LAUNCHER, kind], env=self.wm.env, bufsize=0,
                                  stdin=subprocess.PIPE, stdout=subprocess.PIPE)
        self.wm.clients.append(client)
        self.addCleanup(client.stdout.close)
        self.assertEqual(self.line(client), b"open\n")
        return client

    def line(self, client):
        self.assertTrue(select.select([client.stdout], [], [], TIMEOUT)[0], "it said nothing")
        return client.stdout.readline()

    def test_a_bar_opening_leaves_a_clicked_notification_the_keyboard(self):
        # A window, for the keyboard to go back to if it were dropped
        ipc = self.connect()
        ipc.subscribe("window")
        self.wm.open_window("test.first", "test.first")
        ipc.next_event(EVENT_WINDOW, lambda e: e["change"] == "new")
        note = self.launch("notification")
        output, = self.connect().request(GET_OUTPUTS)
        subprocess.run([POINTER, "move", str(output["rect"]["width"] - 100), "50",
                        str(output["rect"]["width"]), str(output["rect"]["height"]), "click"],
                       env=self.wm.env, check=True, timeout=TIMEOUT)
        self.assertEqual(self.line(note), b"focused\n")
        self.launch("bar")  # its exclusive zone arranges the layers again
        # Not "unfocused" first
        self.press("-k", "a")
        self.assertEqual(self.line(note), b"key\n")



class BindingInGrabTest(TidewmTest):
    """A button's binding that a press in a grabbed window runs moves focus
    as a key's would, ending the grab, though the press itself leaves the
    keyboard where the grab has it."""
    CONFIG = "bindsym --whole-window button3 workspace two\n"

    def test_a_button_binding_in_the_grabbed_window_ends_the_grab(self):
        ipc = self.connect()
        ipc.subscribe("window")
        client = self.wm.open_window("test.grab", "test.grab", stdin=True, stdout=True)
        window = ipc.next_event(EVENT_WINDOW, lambda e: e["change"] == "new")["container"]
        client.stdin.write(b"grab\n")
        client.stdin.flush()
        while self.line(client) != b"grabbing\n":
            pass
        output, = self.connect().request(GET_OUTPUTS)
        rect = window["rect"]
        subprocess.run([POINTER, "move", str(rect["x"] + 50), str(rect["y"] + 50),
                        str(output["rect"]["width"]), str(output["rect"]["height"]),
                        "press", "right", "release", "right"],
                       env=self.wm.env, check=True, timeout=TIMEOUT)
        while self.line(client) != b"cleared\n":
            pass

    def line(self, client):
        self.assertTrue(select.select([client.stdout], [], [], TIMEOUT)[0], "it said nothing")
        line = client.stdout.readline()
        self.assertTrue(line, "the client went")
        return line


if __name__ == "__main__":
    unittest.main()
