"""Alt+Tab's zoom next where tidewm's suite can't reach: sway's own split
tree, which choosing a window leaves be."""

import unittest

import tidesway  # noqa: F401  (sets up harness)
from tidesway import NO_BORDER
from harness import EVENT_WINDOW, GET_TREE, TidewmTest


def shape(node):
    """A node's ID, layout and children, all the way down."""
    return (node["id"], node.get("layout"), [shape(n) for n in node["nodes"]])


class SplitLayoutTest(TidewmTest):
    CONFIG = NO_BORDER + "bindsym Mod1+Tab zoom next\n"

    def workspace(self):
        for output in self.connect().request(GET_TREE)["nodes"]:
            for ws in output["nodes"]:
                if ws["name"] == "1":
                    return ws
        self.fail("no workspace 1")

    def test_alt_tab_held_leaves_a_split_layout_be(self):
        ipc = self.connect()
        ipc.subscribe("window")
        # sway's own layout, with a split inside it
        self.command(self.connect(), "layout splith")
        for i in range(2):
            self.open_window(ipc, f"test.w{i}")
        self.command(self.connect(), "splitv")
        self.open_window(ipc, "test.w2")
        before = shape(self.workspace())
        self.press("-M", "alt", "-k", "Tab", "-k", "Tab", "-k", "Tab", "-m", "alt")
        self.assertEqual(shape(self.workspace()), before)


class CriteriaTest(TidewmTest):
    CONFIG = NO_BORDER

    def test_over_the_ipc_it_zooms_the_window_the_criteria_name(self):
        ipc = self.connect()
        ipc.subscribe("window")
        cons = [self.open_window(ipc, f"test.w{i}")[1] for i in range(3)]
        self.command(self.connect(), f"[con_id={cons[1]['id']}] focus")
        ipc.next_event(EVENT_WINDOW, lambda e: e["change"] == "focus"
                       and e["container"]["id"] == cons[1]["id"])
        self.command(self.connect(), f"[con_id={cons[2]['id']}] zoom next")
        ws = [w for o in self.connect().request(GET_TREE)["nodes"] for w in o["nodes"]
              if w["name"] == "1"][0]
        self.assertEqual(ws["nodes"][0]["id"], cons[2]["id"])


if __name__ == "__main__":
    unittest.main()
