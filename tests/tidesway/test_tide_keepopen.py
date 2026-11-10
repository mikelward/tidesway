"""keep_open's --kept-open bindings: a window kept open takes its close
keys, so the flag is for keys, and a button's binding with it is an
error, rather than one that runs on every window. In a window kept open,
its --kept-open binding comes before one for every window, as in tidewm."""

import os
import select
import subprocess
import unittest

import tidesway  # noqa: F401  (sets up harness)
from harness import EVENT_BINDING, EVENT_WINDOW, ROOT, RUN_COMMAND, TIMEOUT, TidewmTest

KEYBOARD = os.path.join(ROOT, "tests", "keyboard")


class KeptOpenButtonTest(TidewmTest):
    CONFIG = "bindsym --kept-open Ctrl+w kill\n"

    def test_kept_open_is_for_keys_not_buttons(self):
        self.wm.write_config("bindsym --kept-open --whole-window button8 kill\n")
        result, = self.connect().request(RUN_COMMAND, "reload")
        self.assertFalse(result["success"], result)
        self.assertIn("--kept-open is for keys, not buttons", result["error"])


class KeptOpenFirstTest(TidewmTest):
    CONFIG = "bindsym Ctrl+w nop\nbindsym --kept-open Ctrl+w kill\n"

    def line(self, client):
        self.assertTrue(select.select([client.stdout], [], [], TIMEOUT)[0], "nothing came")
        return client.stdout.readline()

    def test_a_kept_open_binding_comes_before_one_for_every_window(self):
        # The headless backend has no keyboard: the tests' is the seat's
        keyboard = subprocess.Popen([KEYBOARD, "0"], env=dict(self.wm.env, XKB_LAYOUT="us"),
                                    stdin=subprocess.PIPE, stdout=subprocess.PIPE)
        self.wm.clients.append(keyboard)
        self.addCleanup(keyboard.stdout.close)
        self.addCleanup(keyboard.stdin.close)
        self.assertEqual(self.line(keyboard), b"switched\n")
        ipc = self.connect()
        ipc.subscribe("window")
        bindings = self.connect()
        bindings.subscribe("binding")
        client = self.wm.open_window("test.kept", "test.kept", stdout=True, keys=True)
        self.addCleanup(client.stdout.close)
        con = ipc.next_event(EVENT_WINDOW, lambda e: e["change"] == "new")["container"]["id"]
        self.assertEqual(self.line(client), b"typing\n")
        self.press("-M", "ctrl", "-k", "w", "-m", "ctrl")
        self.assertEqual(bindings.next_event(EVENT_BINDING)["binding"]["command"], "nop")
        self.command(self.connect(), f"[con_id={con}] keep_open enable")
        self.press("-M", "ctrl", "-k", "w", "-m", "ctrl")
        self.assertEqual(bindings.next_event(EVENT_BINDING)["binding"]["command"], "kill")


if __name__ == "__main__":
    unittest.main()
