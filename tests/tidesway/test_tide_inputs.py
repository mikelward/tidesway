"""An input device's Hyprland name: a second device of a name numbers the
first anew, as its going does again, and the first then takes its new
name's input lines, as it would have had it come with that name."""

import os
import select
import subprocess
import unittest

import tidesway  # noqa: F401  (sets up harness)
from harness import EVENT_INPUT, GET_INPUTS, ROOT, TIMEOUT, TidewmTest

KEYBOARD = os.path.join(ROOT, "tests", "keyboard")


class NamesakeTest(TidewmTest):
    CONFIG = "\n"

    def line(self, client):
        self.assertTrue(select.select([client.stdout], [], [], TIMEOUT)[0], "nothing came")
        return client.stdout.readline()

    def keyboard(self):
        keyboard = subprocess.Popen([KEYBOARD, "0"], env=dict(self.wm.env, XKB_LAYOUT="us"),
                                    stdin=subprocess.PIPE, stdout=subprocess.PIPE)
        self.wm.clients.append(keyboard)
        self.addCleanup(keyboard.stdout.close)
        self.addCleanup(keyboard.stdin.close)
        self.assertEqual(self.line(keyboard), b"switched\n")
        return keyboard

    def rates(self):
        return {d["hyprland_name"]: d["repeat_rate"]
                for d in self.connect().request(GET_INPUTS) if d["type"] == "keyboard"}

    def test_a_device_numbered_anew_takes_its_new_names_lines(self):
        self.keyboard()
        name, = self.rates()
        self.wm.write_config(f'input "{name}" repeat_rate 30\ninput "{name}-1" repeat_rate 40\n')
        self.command(self.connect(), "reload")
        self.assertEqual(self.rates(), {name: 30})
        second = self.keyboard()
        self.assertEqual(self.rates(), {name: 30, f"{name}-1": 40})
        ipc = self.connect()
        ipc.subscribe("input")
        second.stdin.close()
        ipc.next_event(EVENT_INPUT, lambda e: e["change"] == "removed")
        self.assertEqual(self.rates(), {name: 30})


if __name__ == "__main__":
    unittest.main()
