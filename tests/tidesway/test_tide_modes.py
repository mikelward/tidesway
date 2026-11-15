"""tidewm's mode blocks in the config file, where tidesway goes further
than tidewm's tests."""

import unittest

import tidesway  # noqa: F401  (sets up harness)
from harness import GET_BINDINGS, TidewmTest

class NestedRejectedBlockTest(TidewmTest):
    """A block a mode can't hold takes every line to its own }, blocks
    inside it too, so none of its } close the mode around it."""
    CONFIG = ("mode outer {\n"
              "    mode inner {\n"
              "        input type:keyboard {\n"
              "            repeat_rate 30\n"
              "        }\n"
              "    }\n"
              "    bindsym Mod1+x workspace outer\n"
              "}\n"
              "bindsym Mod1+z workspace top\n")

    def test_a_binding_after_it_stays_in_the_mode(self):
        self.assertEqual([(b["keys"], b["mode"]) for b in self.connect().request(GET_BINDINGS)],
                         [("Mod1+z", "default"), ("Mod1+x", "outer")])



if __name__ == "__main__":
    unittest.main()
