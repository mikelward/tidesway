"""tidesway's IPC where it's sway's and not tidewm's: a message type
neither knows is answered, sway's GET_MARKS still is, GET_VERSION names
tidesway too, and sway's scratchpad shows in the tree once it holds a
window."""

import unittest

import tidesway  # noqa: F401  (sets up harness)
from harness import GET_TREE, GET_VERSION, RUN_COMMAND, TidewmTest

GET_MARKS = 5


class IpcTest(TidewmTest):
    CONFIG = "bindsym Mod4+x nop\n"

    def test_a_message_type_no_one_knows_is_answered(self):
        self.assertEqual(self.connect().request(99),
                         {"success": False, "error": "unsupported message type"})

    def test_get_marks_is_sways(self):
        self.assertEqual(self.connect().request(GET_MARKS), [])

    def test_get_version_names_tidesway_and_tidewms_config(self):
        version = self.connect().request(GET_VERSION)
        self.assertEqual(version["variant"], "tidewm")
        self.assertTrue(version["human_readable"].endswith(" (tidesway)"))
        self.assertTrue(version["tidesway"].startswith(f"{version['major']}.{version['minor']}"))
        self.assertEqual(version["loaded_config_file_name"], self.wm.config_path)

    def test_the_scratchpad_is_in_the_tree_only_while_it_holds_a_window(self):
        ipc = self.connect()
        ipc.subscribe("window")
        self.assertEqual([o["name"] for o in ipc.request(GET_TREE)["nodes"]], ["HEADLESS-1"])
        self.open_window(ipc, "test.scratch")
        self.assertEqual(self.connect().request(RUN_COMMAND, "move scratchpad"),
                         [{"success": True}])
        tree = self.connect().request(GET_TREE)
        self.assertEqual([o["name"] for o in tree["nodes"]], ["__i3", "HEADLESS-1"])


if __name__ == "__main__":
    unittest.main()
