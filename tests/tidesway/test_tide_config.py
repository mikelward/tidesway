"""tidewm's config lines where tidewm's own tests can't reach them on
tidesway yet."""

import os
import select
import shlex
import subprocess
import shutil
import tempfile
import unittest

import tidesway  # noqa: F401  (sets up harness)
from harness import EVENT_BINDING, GET_BINDINGS, GET_CONFIG, RUN_COMMAND, TIMEOUT, Tidewm, TidewmTest


class EscapedSpaceTest(TidewmTest):
    CONFIG = "bindsym Mod1+z nop\n"

    def test_an_escaped_space_at_the_end_of_a_line_stays(self):
        """As tidewm's test of it, without the ; that its exec line has."""
        fifo = os.path.join(self.wm.dir.name, "out")
        os.mkfifo(fifo)
        fd = os.open(fifo, os.O_RDONLY | os.O_NONBLOCK)
        self.addCleanup(os.close, fd)
        # The line's own unescaped space and tab still go
        self.wm.write_config(f"bindsym Mod1+x exec exec > {shlex.quote(fifo)} "
                             "printf '<%s>' foo\\  \t\n")
        self.command(self.connect(), "reload")
        self.press("-M", "alt", "-k", "x", "-m", "alt")
        self.assertTrue(select.select([fd], [], [], TIMEOUT)[0], "the binding's exec didn't run")
        output = b""
        while chunk := os.read(fd, 100):  # to the end, when the shell exits
            output += chunk
            select.select([fd], [], [], TIMEOUT)
        self.assertEqual(output, b"<foo >")


class SymlinkedConfigTest(TidewmTest):
    """A config that's a symlink into a dotfiles checkout, as conf's is,
    includes the tide.conf beside the link, as tidewm does, not one beside
    the file it leads to."""
    CONFIG = ""

    def test_a_linked_configs_relative_include_is_beside_the_link(self):
        base = self.wm.dir.name
        checkout = os.path.join(base, "checkout")
        home = os.path.join(base, "home")
        os.makedirs(checkout)
        os.makedirs(home)
        with open(os.path.join(checkout, "config"), "w") as f:
            f.write("include tide.conf\nbindsym $mod+x nop\n")
        with open(os.path.join(home, "tide.conf"), "w") as f:
            f.write("set $mod Mod4\nbindsym $mod+y nop\n")
        os.symlink(os.path.join(checkout, "config"), os.path.join(home, "config"))
        # As the tidesway command does: its own config includes the link
        self.wm.write_config(f"include {home}/config\n")
        reply, = self.connect().request(RUN_COMMAND, "reload")
        self.assertTrue(reply["success"], reply)
        self.assertEqual(self.connect().request(GET_CONFIG).get("errors", []), [])
        keys = sorted(b["keys"] for b in self.connect().request(GET_BINDINGS))
        self.assertEqual(keys, ["Mod4+x", "Mod4+y"])


class KeysToFixWithTest(TidewmTest):
    """Quit, a terminal and the launcher always keep a key (tide's SPEC
    §16): config.h's, under a config's own, while it has an error or binds
    no key that quits, as one with a line or file left out may have lost
    them. Which binding a key ran is its binding event; F12 is the config's
    own, to end the wait. Alt+Shift+Return's terminal is a stand-in that
    does nothing."""
    CONFIG = ""

    def setUp(self):
        self.bin = tempfile.mkdtemp()
        self.addCleanup(shutil.rmtree, self.bin)
        terminal = os.path.join(self.bin, "xdg-terminal-exec")
        with open(terminal, "w") as f:
            f.write("#!/bin/sh\n")
        os.chmod(terminal, 0o755)
        self.ENV = {"PATH": f"{self.bin}:{os.environ['PATH']}"}
        super().setUp()

    def ran_after(self, config):
        """The commands Alt+Shift+Return then F12 ran, tidesway started on
        this config: a reload would keep the last one on an error."""
        self.wm.stop()
        self.wm = Tidewm(1, config + "bindsym F12 nop done\n", True, env=self.ENV)
        self.addCleanup(self.wm.stop)
        ipc = self.connect()
        ipc.subscribe("binding")
        self.press("-M", "alt", "-M", "shift", "-k", "Return", "-m", "shift", "-m", "alt")
        self.press("-k", "F12")
        ran = []
        while ran[-1:] != ["nop done"]:
            ran.append(ipc.next_event(EVENT_BINDING)["binding"]["command"])
        return ran[:-1]

    def test_a_config_with_an_error_keeps_them(self):
        self.assertEqual(self.ran_after("bindsym Print nop\nbindsym $mod+t nop\n"
                                        "bindsym Mod4+Shift+e exit\n"),
                         ["exec xdg-terminal-exec || exec foot"])

    def test_a_config_with_no_key_that_quits_keeps_them(self):
        self.assertEqual(self.ran_after("bindsym Print nop\n"),
                         ["exec xdg-terminal-exec || exec foot"])

    def test_a_whole_config_that_quits_has_its_own_keys_alone(self):
        self.assertEqual(self.ran_after("bindsym Print nop\nbindsym Mod4+Shift+e exit\n"), [])

    def test_a_key_that_quits_in_a_list_or_in_capitals_is_one(self):
        self.assertEqual(self.ran_after("bindsym Print nop\nbindsym Mod4+Shift+e nop leaving; EXIT\n"),
                         [])

    def test_an_exit_in_execs_shell_or_in_quotes_doesnt_quit(self):
        self.assertEqual(self.ran_after('bindsym Print nop\nbindsym Mod4+x exec true; exit\n'
                                        'bindsym Mod4+y workspace "a;exit"\n'),
                         ["exec xdg-terminal-exec || exec foot"])

    def test_a_key_that_quits_only_in_a_window_kept_open_isnt_one(self):
        self.assertEqual(self.ran_after("bindsym Print nop\nbindsym --kept-open Mod4+Shift+e exit\n"),
                         ["exec xdg-terminal-exec || exec foot"])

    def test_a_key_that_quits_on_one_keyboard_isnt_one(self):
        self.assertEqual(self.ran_after("bindsym Print nop\n"
                                        "bindsym --input-device=1:1:Gone Mod4+Shift+e exit\n"),
                         ["exec xdg-terminal-exec || exec foot"])

    def test_a_linked_config_that_lost_its_include_keeps_them(self):
        # As tide's session has it: a link into a dotfiles checkout, its
        # include of the file with most keys, beside the link, gone
        base = tempfile.mkdtemp()
        self.addCleanup(shutil.rmtree, base)
        os.makedirs(os.path.join(base, "checkout"))
        os.makedirs(os.path.join(base, "home"))
        with open(os.path.join(base, "checkout", "config"), "w") as f:
            f.write("include tide.conf\nbindsym Print nop\n")
        os.symlink(os.path.join(base, "checkout", "config"), os.path.join(base, "home", "config"))
        self.assertEqual(self.ran_after(f"include {base}/home/config\n"),
                         ["exec xdg-terminal-exec || exec foot"])

    def test_the_configs_release_binding_of_the_keys_wins(self):
        self.assertEqual(self.ran_after("bindsym Print nop\n"
                                        "bindsym --release Mod1+Shift+Return nop released\n"),
                         ["nop released"])

    def test_the_configs_own_binding_of_the_keys_wins(self):
        self.assertEqual(self.ran_after("bindsym Print nop\nbindsym Mod1+Shift+Return nop mine\n"),
                         ["nop mine"])


class RelativeConfigPathTest(TidewmTest):
    """A config named by a relative path still finds its relative include."""
    CONFIG = ""

    def test_a_relative_configs_relative_include_loads(self):
        base = self.wm.dir.name
        os.makedirs(os.path.join(base, "configs"))
        with open(os.path.join(base, "configs", "main"), "w") as f:
            f.write("include keys.conf\n")
        with open(os.path.join(base, "configs", "keys.conf"), "w") as f:
            f.write("bindsym Mod4+k nop\n")
        sway = os.environ.get("TIDESWAY", os.path.join(tidesway.ROOT, "build", "sway", "sway"))
        env = dict(self.wm.env, WAYLAND_DISPLAY="", SWAYSOCK="")
        # sway -C reads it and exits, saying what failed
        result = subprocess.run([sway, "-C", "-c", "configs/main", "-d"], cwd=base, env=env,
                                capture_output=True, text=True, timeout=TIMEOUT)
        self.assertEqual(result.returncode, 0, result.stderr[-2000:])
        self.assertNotIn("keys.conf not found", result.stderr)
        self.assertIn("Mod4+k", result.stderr)

if __name__ == "__main__":
    unittest.main()
