"""Named bindings with no key where sway's mouse flags meet them: a rebind
gives a button to a name whose line had none, as it would to one whose
line gave a button with the same flags."""

import unittest

import tidesway  # noqa: F401  (sets up harness)
from harness import GET_NAMED_BINDINGS, TidewmTest


class KeylessButtonTest(TidewmTest):
    CONFIG = ("bindsym --name grow --whole-window none nop grow\n"
              "bindsym --name shrink --whole-window --exclude-titlebar none nop shrink\n")

    def rebind(self, line):
        self.wm.write_config(self.CONFIG + line)
        self.command(self.connect(), "reload")

    def named(self, name):
        return next(b for b in self.connect().request(GET_NAMED_BINDINGS)
                    if b["name"] == name)

    def test_a_rebind_gives_a_name_with_no_keys_a_button(self):
        self.rebind("rebind grow button1\n")
        binding = self.named("grow")
        self.assertEqual((binding["type"], binding["keys"]), ("mouse", ["button1"]))

    def test_a_rebind_gives_a_button_past_the_title_bar(self):
        self.rebind("rebind shrink button3\n")
        binding = self.named("shrink")
        self.assertEqual((binding["type"], binding["keys"]), ("mouse", ["button3"]))


if __name__ == "__main__":
    unittest.main()
