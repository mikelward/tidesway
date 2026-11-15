"""Commands tidesway adds keep sway's own reachable: the command table is
searched by bsearch, so an entry out of order hides its neighbors, as
maximize once hid max_render_time. A window that opens maximized says so
in its new event. A binding's exec takes the rest of its line, as tide's
next_attention binding needs."""

import os
import select
import unittest

import tidesway  # noqa: F401  (sets up harness)
from harness import EVENT_WINDOW, GET_BINDINGS, GET_OUTPUTS, RUN_COMMAND, TIMEOUT, TidewmTest

# tide's tidewm/tide.conf's next_attention binding, on a key, its jq the
# shell's printf: a ; in braces, and a } ending the line
ATTENTION = ("""exec false || { id=$(printf '%s' '[.. | objects]' | wc -c) && """
             """test -n "$id" && printf '%s;' "[con_id=$id] focus" > "$XDG_RUNTIME_DIR/out"; }""")


class ExecQuotedTest(TidewmTest):
    def exec_output(self, payload):
        fifo = os.path.join(self.wm.dir.name, "out")
        os.mkfifo(fifo)
        fd = os.open(fifo, os.O_RDONLY | os.O_NONBLOCK)
        self.addCleanup(os.close, fd)
        for result in self.connect().request(RUN_COMMAND, payload):
            self.assertTrue(result["success"], f"{payload!r}: {result}")
        self.assertTrue(select.select([fd], [], [], TIMEOUT)[0], "exec's command didn't run")
        output = b""
        while chunk := os.read(fd, 100):
            output += chunk
            select.select([fd], [], [], TIMEOUT)
        return output

    def test_one_quoted_word_loses_its_quotes_space_after_it_or_not(self):
        command = 'printf ok > "$XDG_RUNTIME_DIR/out"'
        self.assertEqual(self.exec_output(f"exec '{command}' \t"), b"ok")


class ExecBindingTest(TidewmTest):
    CONFIG = f"bindsym --name=next_attention Mod4+F9 {ATTENTION}\n"

    def test_a_bindings_exec_takes_the_rest_of_its_line(self):
        binding, = [b for b in self.connect().request(GET_BINDINGS) if b["name"]]
        self.assertEqual(binding["command"], ATTENTION)
        fifo = os.path.join(self.wm.dir.name, "out")
        os.mkfifo(fifo)
        fd = os.open(fifo, os.O_RDONLY | os.O_NONBLOCK)
        self.addCleanup(os.close, fd)
        self.press("-M", "logo", "-k", "F9", "-m", "logo")
        self.assertTrue(select.select([fd], [], [], TIMEOUT)[0], "the binding's exec didn't run")
        output = b""
        while chunk := os.read(fd, 100):
            output += chunk
            select.select([fd], [], [], TIMEOUT)
        self.assertEqual(output, b"[con_id=14] focus;")


class CommandTableTest(TidewmTest):
    def test_sways_commands_beside_tides_are_found(self):
        ipc = self.connect()
        ipc.subscribe("window")
        self.wm.open_window("test.app", "test.app")
        ipc.next_event(EVENT_WINDOW, lambda e: e["change"] == "new")
        for command in ["max_render_time off", "maximize toggle", "maximize toggle"]:
            result, = self.connect().request(RUN_COMMAND, command)
            self.assertTrue(result["success"], f"{command}: {result}")



class OpensMaximizedTest(TidewmTest):
    def test_a_window_that_opens_maximized_says_so_in_its_new_event(self):
        ipc = self.connect()
        ipc.subscribe("window")
        self.wm.open_window("test.max", "test.max", maximized=True)
        new = ipc.next_event(EVENT_WINDOW, lambda e: e["change"] == "new")
        self.assertTrue(new["container"]["maximized"], new["container"])


class RuleMaximizedTest(TidewmTest):
    CONFIG = 'for_window [app_id="^test.ruled$"] maximize enable\n'

    def test_a_rule_that_maximizes_a_window_tells_the_app(self):
        client = self.wm.open_window("test.ruled", "test.ruled", stdout=True)
        self.addCleanup(client.stdout.close)
        while True:
            self.assertTrue(select.select([client.stdout], [], [], TIMEOUT)[0], "it said nothing")
            line = client.stdout.readline()
            self.assertTrue(line, "the client went")
            self.assertNotEqual(line, b"unmaximized\n")
            if line == b"maximized\n":
                break


class FloatedWhileMaximizedTest(TidewmTest):
    def test_a_tile_floated_while_maximized_has_a_size_after(self):
        ipc = self.connect()
        ipc.subscribe("window")
        self.wm.open_window("test.app", "test.app")
        con = ipc.next_event(EVENT_WINDOW, lambda e: e["change"] == "new")["container"]["id"]
        for command in ["maximize enable", "floating enable", "maximize disable"]:
            self.command(self.connect(), f"[con_id={con}] {command}")
        rect = self.windows()[con]["rect"]
        self.assertGreater(rect["width"], 0, rect)
        self.assertGreater(rect["height"], 0, rect)


class MaximizedOnAnotherOutputTest(TidewmTest):
    OUTPUTS = 2

    def test_a_window_unmaximized_on_another_output_comes_back_there(self):
        ipc = self.connect()
        ipc.subscribe("window")
        first, second = sorted(self.connect().request(GET_OUTPUTS), key=lambda o: o["rect"]["x"])
        self.command(self.connect(), f"focus output {first['name']}")
        self.wm.open_window("test.app", "test.app")
        con = ipc.next_event(EVENT_WINDOW, lambda e: e["change"] == "new")["container"]["id"]
        for command in ["floating enable", "resize set 300 200", "move position 100 100",
                        "maximize enable", f"move container to output {second['name']}",
                        "maximize disable"]:
            self.command(self.connect(), f"[con_id={con}] {command}")
        rect, area = self.windows()[con]["rect"], second["rect"]
        self.assertGreaterEqual(rect["x"], area["x"], rect)
        self.assertLessEqual(rect["x"] + rect["width"], area["x"] + area["width"], rect)


if __name__ == "__main__":
    unittest.main()
