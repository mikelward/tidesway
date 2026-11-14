"""A window's new event goes once the window has the size its place gives
it, not as it maps at a size of its app's own, and its other events wait
behind it (include/sway/tide/announce.h)."""

import os
import subprocess

import tidesway  # noqa: F401  (sets up harness)
from harness import EVENT_WINDOW, GET_TREE, ROOT, TIMEOUT, TidewmTest

CAPTURE = os.path.join(ROOT, "tests", "capture")
SUBSURFACE = os.path.join(os.path.dirname(os.path.dirname(os.environ.get(
    "TIDESWAY", os.path.join(tidesway.ROOT, "build", "sway", "sway")))), "tests", "tidesway", "subsurface")


class AnnounceTest(TidewmTest):
    CONFIG = ""

    def setUp(self):
        super().setUp()
        self.need(os.path.exists(SUBSURFACE), f"no test client at {SUBSURFACE}; build tidesway")
        self.ipc = self.connect()
        self.ipc.subscribe("window")

    def start(self, app_id, *how):
        client = subprocess.Popen([SUBSURFACE, app_id, *how], env=self.wm.env,
                                  stdin=subprocess.PIPE, stdout=subprocess.PIPE)
        self.wm.clients.append(client)
        self.addCleanup(client.stdout.close)
        return client

    def events(self, app_id, count):
        """The next count window events of the window with app_id."""
        return [self.ipc.next_event(EVENT_WINDOW, lambda e: e["container"]["app_id"] == app_id)
                for _ in range(count)]

    def size_in_tree(self, con_id):
        def find(node):
            if node["id"] == con_id:
                return node
            for child in node.get("nodes", []) + node.get("floating_nodes", []):
                found = find(child)
                if found:
                    return found
            return None
        rect = find(self.connect().request(GET_TREE))["rect"]
        return f"{rect['width']}x{rect['height']}"

    def shot(self, app_id):
        """tests/capture's one frame of the window: "WxH RRGGBB"."""
        return subprocess.run([CAPTURE, "shot", app_id], env=self.wm.env, check=True,
                              capture_output=True, timeout=TIMEOUT).stdout.decode().strip()

    def test_a_slow_apps_window_is_new_once_it_has_drawn_at_its_size(self):
        # The app maps at 100x100, and takes a while to draw at the size
        # its place gives it. A capture begun at its new event shows it at
        # that size, not as it mapped.
        self.start("test.slow", "slow")
        new, = self.events("test.slow", 1)
        self.assertEqual(new["change"], "new")
        size = self.size_in_tree(new["container"]["id"])
        self.assertNotEqual(size, "100x100", "the window fills the workspace")
        self.assertEqual(self.shot("test.slow").split()[0], size)

    def test_a_windows_focus_event_follows_its_new_one(self):
        self.start("test.slow", "slow")
        self.assertEqual([e["change"] for e in self.events("test.slow", 2)], ["new", "focus"])

    def test_focus_that_moved_on_before_a_windows_new_event_isnt_said_after(self):
        # A window that's there, then a slow one, which takes focus as it
        # maps. Focus goes back before the slow one has drawn, so the slow
        # one's own focus event, still waiting behind its new one, would
        # be the last, and name a window that hasn't the focus.
        self.start("test.first")
        first = self.events("test.first", 2)[0]["container"]["id"]
        self.start("test.slow", "slow")
        # It's in the tree as soon as it maps, before its new event
        tree = self.connect()
        while not self.in_tree(tree.request(GET_TREE), "test.slow"):
            pass
        self.command(tree, f"[con_id={first}] focus")
        self.ipc.next_event(EVENT_WINDOW, lambda e: e["container"]["app_id"] == "test.slow"
                            and e["change"] == "new")
        # A mark's event ends the wait: the slow one's are all out by then
        self.command(tree, f"[con_id={first}] mark done")
        seen = []
        while seen[-1:] != [("test.first", "mark")]:
            event = self.ipc.next_event(EVENT_WINDOW)
            seen.append((event["container"]["app_id"], event["change"]))
        # Had the slow one drawn before focus went back, its focus event
        # went then, before the first's: either way it isn't the last
        focus = [app for app, change in seen if change == "focus"]
        self.assertNotEqual(focus[-1:], ["test.slow"], seen)

    def in_tree(self, node, app_id):
        return node.get("app_id") == app_id or any(
            self.in_tree(child, app_id) for child in node.get("nodes", []) + node.get("floating_nodes", []))
