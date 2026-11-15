"""tidesway's outputs where sway's output command meets tidewm's: a mode's
rate without Hz, a command's power before its other settings, a command
an output refuses, a command or a display settings tool, not the file,
turning outputs off, an output kept on in a mode it takes, an unplug, and
a reload on a window."""

import unittest

import tidesway  # noqa: F401  (sets up harness)
from harness import GET_CONFIG, GET_OUTPUTS, RUN_COMMAND, TidewmTest, randr


def mode(output):
    current = output["current_mode"]
    return current["width"], current["height"], current["refresh"]


class OutputsTest(TidewmTest):
    def outputs(self):
        return {o["name"]: o for o in self.connect().request(GET_OUTPUTS)}


class OneOutputTest(OutputsTest):
    CONFIG = "output HEADLESS-1 mode 800x600@50 scale 2\n"

    def test_a_rate_needs_no_hz(self):
        self.assertEqual(mode(self.outputs()["HEADLESS-1"]), (800, 600, 50000))

    def test_power_before_the_other_settings_goes_after_them(self):
        self.command(self.connect(), "output HEADLESS-1 power off scale 1 transform 90")
        output = self.outputs()["HEADLESS-1"]
        self.assertEqual((output["power"], output["scale"], output["transform"]),
                         (False, 1, "90"))

    def test_a_mode_a_command_gives_that_the_output_refuses_leaves_it_as_it_was(self):
        self.command(self.connect(), "output HEADLESS-1 mode 600000000x1")
        output = self.outputs()["HEADLESS-1"]
        self.assertTrue(output["active"])
        self.assertEqual((mode(output), output["scale"]), ((800, 600, 50000), 2))


class CommandTurnsTheRestOffTest(OutputsTest):
    """The file's line has one off; a command, as a lid's bindswitch does,
    turns off the other, and that's no reason to turn the first on."""
    OUTPUTS = 2
    CONFIG = "output HEADLESS-2 disable\n"

    def test_every_output_stays_off(self):
        self.command(self.connect(), "output HEADLESS-1 disable")
        self.assertEqual({name: o["active"] for name, o in self.outputs().items()},
                         {"HEADLESS-1": False, "HEADLESS-2": False})


class ToolAfterKeptOnTest(OutputsTest):
    """The file disables every output, so one is kept on; then a display
    settings tool turns the other on and the kept one off, which is no
    longer the file's doing."""
    OUTPUTS = 2
    CONFIG = "output * disable\n"

    def active(self):
        return {name: o["active"] for name, o in self.outputs().items()}

    def test_the_output_the_tool_turned_off_stays_off(self):
        kept, = [name for name, on in self.active().items() if on]
        other, = {"HEADLESS-1", "HEADLESS-2"} - {kept}
        randr(self, "--output", other, "--on", "--output", kept, "--off")
        self.assertEqual(self.active(), {kept: False, other: True})
        self.command(self.connect(), f"output {other} scale 2")
        self.assertEqual(self.active(), {kept: False, other: True})


class KeptInAModeItTakesTest(OutputsTest):
    """The file disables the only output, in a mode it can't show: it's
    kept on, in its own mode, as tidewm keeps it."""
    CONFIG = "output HEADLESS-1 disable mode 600000000x1\n"

    def test_it_starts_on_in_its_preferred_mode(self):
        output = self.outputs()["HEADLESS-1"]
        self.assertEqual((output["active"], mode(output)), (True, (1280, 720, 0)))
        self.assertEqual(self.connect().request(GET_CONFIG)["errors"],
                         [f"{self.wm.config_path}: output HEADLESS-1 is on, though its line "
                          "disables it: no other output is on"])


class UnplugAnOutputThatWasOffTest(OutputsTest):
    """The file disables two outputs and a command the third, as a lid's
    bindswitch does; unplugging one that was off leaves no other off, so
    nothing comes on."""
    OUTPUTS = 3
    CONFIG = "output HEADLESS-2 disable\noutput HEADLESS-3 disable\n"

    def test_every_output_stays_off(self):
        self.command(self.connect(), "output HEADLESS-1 disable")
        self.command(self.connect(), "output HEADLESS-3 unplug")
        self.assertEqual({name: o["active"] for name, o in self.outputs().items()},
                         {"HEADLESS-1": False, "HEADLESS-2": False})


class ReloadOnAWindowTest(OutputsTest):
    """A reload run with criteria, on each window they match, whose result
    sway gives as one for them all."""
    CONFIG = "output HEADLESS-1 mode 800x600@50Hz\n"

    def test_it_fails_naming_an_output_that_refuses_the_file(self):
        ipc = self.connect()
        ipc.subscribe("window")
        self.open_window(ipc, "test.reload")
        self.wm.write_config("output HEADLESS-1 mode 600000000x1\n")
        result, = self.connect().request(RUN_COMMAND, '[app_id="test.reload"] reload')
        self.assertFalse(result["success"])
        self.assertIn("output HEADLESS-1 refused its settings", result["error"])
        self.assertEqual(mode(self.outputs()["HEADLESS-1"]), (800, 600, 50000))


if __name__ == "__main__":
    unittest.main()
