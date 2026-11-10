"""An input device's Hyprland name: a second device of a name numbers the
first anew, as its going does again, and the first then takes its new
name's input lines, as it would have had it come with that name."""

import os
import select
import subprocess
import unittest

import tidesway  # noqa: F401  (sets up harness)
from harness import EVENT_INPUT, GET_INPUTS, ROOT, RUN_COMMAND, TIMEOUT, TidewmTest

KEYBOARD = os.path.join(ROOT, "tests", "keyboard")


class KeyboardHelpers(TidewmTest):
    """Helpers, with no tests of their own."""
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


class NamesakeTest(KeyboardHelpers):
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


class SwaysValuesTest(KeyboardHelpers):
    """Input checked as tidewm checks it still takes what only sway has"""

    def test_events_toggle_takes_its_modes(self):
        self.keyboard()
        self.command(self.connect(), "input type:keyboard events toggle enabled disabled")
        # sway reads a setting's name in any case
        self.command(self.connect(), "input type:keyboard EVENTS toggle enabled disabled")

    def test_drag_lock_takes_enabled_sticky(self):
        self.wm.write_config("input * drag_lock enabled_sticky\ninput * DRAG_LOCK enabled_sticky\n")
        result, = self.connect().request(RUN_COMMAND, "reload")
        self.assertTrue(result["success"], result)

    def test_a_nested_block_names_its_device_first(self):
        self.wm.write_config("input * {\n    repeat_rate {\n        30\n    }\n}\n"
                             'input "type:keyboard" {\n    repeat_delay 300\n}\n')
        result, = self.connect().request(RUN_COMMAND, "reload")
        self.assertTrue(result["success"], result)

    def test_a_double_quote_in_single_quotes_is_a_character(self):
        self.wm.write_config("input * map_to_output 'Acme 27\" Display'\n")
        result, = self.connect().request(RUN_COMMAND, "reload")
        self.assertTrue(result["success"], result)

    def test_a_blocks_name_may_be_a_variable(self):
        self.wm.write_config("set $kind keyboard\ninput type:$kind {\n    repeat_rate 30\n}\n")
        result, = self.connect().request(RUN_COMMAND, "reload")
        self.assertTrue(result["success"], result)
        # or more than a name, a setting with it
        self.wm.write_config("set $kbd type:keyboard repeat_rate\ninput $kbd {\n    30\n}\n")
        result, = self.connect().request(RUN_COMMAND, "reload")
        self.assertTrue(result["success"], result)


if __name__ == "__main__":
    unittest.main()
