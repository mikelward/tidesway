"""Tests for run (tide/tidesway.in, the command tidesway installs), suite and
gate, with a stand-in for tidesway: python3 -m unittest discover -s
tests/tidewm-harness"""

import importlib.machinery
import importlib.util
import json
import os
import shutil
import subprocess
import sys
import tempfile
import unittest

sys.dont_write_bytecode = True
HERE = os.path.dirname(os.path.abspath(__file__))
RUN = os.path.join(HERE, "run")


def load(name):
    loader = importlib.machinery.SourceFileLoader(name, os.path.join(HERE, name))
    spec = importlib.util.spec_from_loader(name, loader)
    module = importlib.util.module_from_spec(spec)
    loader.exec_module(module)
    return module


class RunTest(unittest.TestCase):
    def setUp(self):
        self.dir = tempfile.TemporaryDirectory()
        self.addCleanup(self.dir.cleanup)
        # Stands in for tidesway: writes its arguments and its config's text
        self.fake = os.path.join(self.dir.name, "fake-sway")
        self.out = os.path.join(self.dir.name, "out")
        with open(self.fake, "w") as f:
            f.write("#!/bin/sh\n"
                    f'printf "%s\\n" "$@" > "{self.out}"\n'
                    f'test -z "$2" || cat "$2" >> "{self.out}"\n')
        os.chmod(self.fake, 0o755)
        self.env = dict(os.environ, TIDESWAY=self.fake, XDG_RUNTIME_DIR=self.dir.name,
                        XDG_CONFIG_HOME=os.path.join(self.dir.name, "xdg"))

    def run_harness(self, *args):
        proc = subprocess.run([RUN, *args], env=self.env, capture_output=True, text=True,
                              timeout=10)
        return proc

    def written(self):
        with open(self.out) as f:
            return f.read()

    def test_runs_tidesway_on_look_then_the_config_then_startup(self):
        config = os.path.join(self.dir.name, "config")
        self.assertEqual(self.run_harness("-c", config, "-s", "echo up").returncode, 0)
        text = self.written()
        self.assertTrue(text.startswith("-c\n"))
        self.assertIn("default_border none", text)
        look = text.index("default_border none")
        include = text.index(f"include {config}\n")
        startup = text.index("exec sh ")
        self.assertLess(look, include)
        self.assertLess(include, startup)
        script = text[startup:].split()[2]
        with open(script) as f:
            self.assertEqual(f.read(), "echo up\n")

    def test_alt_drags_unless_the_config_says_what_the_mouse_does(self):
        config = os.path.join(self.dir.name, "config")
        for text, alt in [("", True), ("bindsym Mod1+x nop\n", True),
                          ("floating_modifier Mod4\n", False),
                          ("bindsym --whole-window Mod4+button2 nop\n", False)]:
            with self.subTest(text=text):
                with open(config, "w") as f:
                    f.write(text)
                self.assertEqual(self.run_harness("-c", config, "-s", "true").returncode, 0)
                self.assertEqual("floating_modifier Mod1 normal\n" in self.written(), alt)

    def test_without_c_includes_tidewms_own_config_path(self):
        self.assertEqual(self.run_harness("-s", "true").returncode, 0)
        path = os.path.join(self.dir.name, "xdg", "tidewm", "config")
        self.assertIn(f"include {path}\n", self.written())

    def test_refuses_an_unknown_argument(self):
        proc = self.run_harness("-x")
        self.assertNotEqual(proc.returncode, 0)
        self.assertIn("-x", proc.stderr)
        self.assertFalse(os.path.exists(self.out))

    def test_says_when_tidesway_is_missing(self):
        self.env["TIDESWAY"] = os.path.join(self.dir.name, "nowhere")
        proc = self.run_harness("-s", "true")
        self.assertNotEqual(proc.returncode, 0)
        self.assertIn("nowhere", proc.stderr)

    def test_includes_what_only_tidesway_reads_after_the_config(self):
        config = os.path.join(self.dir.name, "config")
        self.assertEqual(self.run_harness("-c", config, "-s", "true").returncode, 0)
        text = self.written()
        extra = f"include {self.dir.name}/tidesway.d/*.conf\n"
        self.assertIn(extra, text)
        self.assertLess(text.index(f"include {config}\n"), text.index(extra))

    def test_a_relative_config_is_included_by_its_whole_path(self):
        proc = subprocess.run([RUN, "-c", "config", "-s", "true"], env=self.env,
                              cwd=self.dir.name, capture_output=True, text=True, timeout=10)
        self.assertEqual(proc.returncode, 0, proc.stderr)
        self.assertIn(f"include {self.dir.name}/config\n", self.written())

    def test_refuses_a_config_path_sways_include_would_read_as_another(self):
        for name in "my config", "con$fig", "con*fig", "it's":
            with self.subTest(name=name):
                proc = self.run_harness("-c", os.path.join(self.dir.name, name), "-s", "true")
                self.assertEqual(proc.returncode, 1)
                self.assertIn("sway's include can't take", proc.stderr)
                self.assertFalse(os.path.exists(self.out))

    def test_v_asks_sway_its_version(self):
        self.assertEqual(self.run_harness("-v").returncode, 0)
        self.assertEqual(self.written().splitlines()[0], "--version")

    def test_two_at_once_each_keep_their_own_config(self):
        # Each writes under its own process ID, which is sway's
        with open(self.fake, "a") as f:
            f.write(f'echo "pid $$" >> "{self.out}"\n')
        self.assertEqual(self.run_harness("-c", "/a/first", "-s", "true").returncode, 0)
        first = self.written()
        pid = first.split("pid ")[1].strip()
        self.assertIn(f"exec sh {self.dir.name}/tidesway/{pid}/startup.sh\n", first)
        self.assertEqual(first.splitlines()[1], f"{self.dir.name}/tidesway/{pid}/sway.conf")

    def test_what_one_that_ended_left_is_removed_and_a_running_ones_kept(self):
        gone = subprocess.Popen(["true"])
        gone.wait()
        for pid in gone.pid, os.getpid():
            os.makedirs(os.path.join(self.dir.name, "tidesway", str(pid)))
        self.assertEqual(self.run_harness("-s", "true").returncode, 0)
        left = os.listdir(os.path.join(self.dir.name, "tidesway"))
        self.assertNotIn(str(gone.pid), left)
        self.assertIn(str(os.getpid()), left)

    def test_installed_it_runs_the_sway_and_look_meson_put_it_with(self):
        # As tide/meson.build fills it in, with a bindir and datadir that
        # aren't beside each other
        bindir = os.path.join(self.dir.name, "prefix", "bin")
        lookdir = os.path.join(self.dir.name, "elsewhere", "data", "tidesway")
        os.makedirs(bindir)
        os.makedirs(lookdir)
        root = os.path.dirname(os.path.dirname(HERE))
        with open(os.path.join(root, "tide", "tidesway.in")) as f:
            text = f.read().replace("@bindir@", bindir).replace("@lookdir@", lookdir)
        installed = os.path.join(bindir, "tidesway")
        with open(installed, "w") as f:
            f.write(text)
        os.chmod(installed, 0o755)
        shutil.copy(self.fake, os.path.join(bindir, "sway"))
        with open(os.path.join(lookdir, "look.conf"), "w") as f:
            f.write("# the installed look\n")
        env = {k: v for k, v in self.env.items() if k != "TIDESWAY"}
        proc = subprocess.run([installed, "-s", "true"], env=env, capture_output=True,
                              text=True, timeout=10)
        self.assertEqual(proc.returncode, 0, proc.stderr)
        self.assertIn("# the installed look\n", self.written())

class SuiteTest(unittest.TestCase):
    def suite_on(self, files, **env):
        """Runs suite on a stand-in tidewm checkout with these test files"""
        d = tempfile.TemporaryDirectory()
        self.addCleanup(d.cleanup)
        tests = os.path.join(d.name, "tidewm", "tests")
        os.makedirs(tests)
        files = dict(files, **{"harness.py": ""})
        for name, text in files.items():
            with open(os.path.join(tests, name), "w") as f:
                f.write(text)
        out = os.path.join(d.name, "out")
        self.dir = d.name
        proc = subprocess.run([os.path.join(HERE, "suite"), os.path.join(d.name, "tidewm"), out],
                              capture_output=True, text=True, timeout=60,
                              env=dict(os.environ, **env))
        return proc, out

    def assert_gone(self, pid):
        """The process has exited (a zombie no one has reaped yet counts)"""
        try:
            with open(f"/proc/{pid}/stat") as f:
                state = f.read().rsplit(")", 1)[1].split()[0]
        except FileNotFoundError:
            return
        self.assertEqual(state, "Z", f"process {pid} is still running")

    def child_pid(self):
        with open(os.path.join(self.dir, "child")) as f:
            return int(f.read())

    # A test file that starts a long-lived child, as a test starts tidesway,
    # writes its pid where the test can find it, then SLEEPS seconds.
    LEAVES_A_CHILD = (
        "import os, subprocess, time, unittest\n"
        "class LeakTest(unittest.TestCase):\n"
        "    def test_leaves_a_child(self):\n"
        "        child = subprocess.Popen(['sleep', '300'])\n"
        "        with open(os.path.join(os.path.dirname(os.getcwd()), '..', 'child'), 'w') as f:\n"
        "            f.write(str(child.pid))\n"
        "        time.sleep(SLEEPS)\n")

    def test_a_file_that_times_out_leaves_nothing_running(self):
        proc, _ = self.suite_on({"test_leak.py": self.LEAVES_A_CHILD.replace("SLEEPS", "300")},
                                FILE_TIMEOUT="1")
        self.assertEqual(proc.returncode, 0, proc.stderr)
        self.assertIn("'timeout': 1", proc.stdout)
        self.assert_gone(self.child_pid())

    def test_a_file_that_finishes_leaves_nothing_running(self):
        proc, _ = self.suite_on({"test_leak.py": self.LEAVES_A_CHILD.replace("SLEEPS", "0")})
        self.assertEqual(proc.returncode, 0, proc.stderr)
        self.assertIn("1 of 1 pass", proc.stdout)
        self.assert_gone(self.child_pid())

    def test_a_failing_test_is_a_result_not_an_error(self):
        proc, out = self.suite_on({"test_x.py": (
            "import unittest\n"
            "class XTest(unittest.TestCase):\n"
            "    def test_passes(self): pass\n"
            "    def test_fails(self): self.fail()\n")})
        self.assertEqual(proc.returncode, 0, proc.stderr)
        self.assertIn("1 of 2 pass", proc.stdout)
        self.assertTrue(os.path.exists(os.path.join(out, "results.json")))

    def test_a_file_that_doesnt_import_fails_the_run(self):
        proc, _ = self.suite_on({
            "test_ok.py": ("import unittest\n"
                           "class OkTest(unittest.TestCase):\n"
                           "    def test_passes(self): pass\n"),
            "test_broken.py": "import no_such_module\n"})
        self.assertNotEqual(proc.returncode, 0)
        self.assertIn("test_broken", proc.stderr)
        self.assertIn("1 of 1 pass", proc.stdout)

    def results(self, out, module):
        with open(os.path.join(out, "results.json")) as f:
            return json.load(f)[module]["tests"]

    def test_a_class_fixtures_error_errors_its_tests(self):
        proc, out = self.suite_on({"test_fix.py": (
            "import unittest\n"
            "class BadTest(unittest.TestCase):\n"
            "    @classmethod\n"
            "    def setUpClass(cls): raise RuntimeError('no compositor')\n"
            "    def test_a(self): pass\n"
            "    def test_b(self): pass\n"
            "class GoodTest(unittest.TestCase):\n"
            "    def test_c(self): pass\n")})
        self.assertEqual(proc.returncode, 0, proc.stderr)
        self.assertEqual(self.results(out, "test_fix"), {
            "test_fix.BadTest.test_a": "error",
            "test_fix.BadTest.test_b": "error",
            "test_fix.GoodTest.test_c": "pass",
        })

    def test_a_module_fixtures_error_errors_every_test(self):
        proc, out = self.suite_on({"test_mod.py": (
            "import unittest\n"
            "def setUpModule(): raise RuntimeError('no compositor')\n"
            "class ATest(unittest.TestCase):\n"
            "    def test_a(self): pass\n")})
        self.assertEqual(proc.returncode, 0, proc.stderr)
        self.assertEqual(self.results(out, "test_mod"), {"test_mod.ATest.test_a": "error"})

    def test_counts_each_outcome_and_subtest(self):
        proc, out = self.suite_on({"test_kinds.py": (
            "import unittest\n"
            "class KindsTest(unittest.TestCase):\n"
            "    def test_fails(self): self.fail()\n"
            "    def test_errs(self): raise RuntimeError\n"
            "    def test_skips(self): self.skipTest('no X11')\n"
            "    @unittest.expectedFailure\n"
            "    def test_expected(self): self.fail()\n"
            "    def test_subs(self):\n"
            "        for i in range(3):\n"
            "            with self.subTest(i=i):\n"
            "                self.assertNotEqual(i, 1)\n")})
        self.assertEqual(proc.returncode, 0, proc.stderr)
        self.assertEqual(self.results(out, "test_kinds"), {
            "test_kinds.KindsTest.test_fails": "fail",
            "test_kinds.KindsTest.test_errs": "error",
            "test_kinds.KindsTest.test_skips": "skip",
            "test_kinds.KindsTest.test_expected": "pass",
            "test_kinds.KindsTest.test_subs (i=0)": "pass",
            "test_kinds.KindsTest.test_subs (i=1)": "fail",
            "test_kinds.KindsTest.test_subs (i=2)": "pass",
        })

    def test_an_unreported_test_is_a_timeout_only_if_its_file_timed_out(self):
        suite = load("suite")
        records = [{"ids": ["m.T.test_a", "m.T.test_b"]},
                   {"test": "m.T.test_a", "outcome": "pass"}]
        self.assertEqual(suite.outcomes(records, True),
                         {"m.T.test_a": "pass", "m.T.test_b": "timeout"})
        self.assertEqual(suite.outcomes(records, False),
                         {"m.T.test_a": "pass", "m.T.test_b": "error"})
        self.assertIsNone(suite.outcomes([{"load_error": "no module"}], False))

    def test_a_skipped_class_fixture_skips_its_tests(self):
        proc, out = self.suite_on({"test_skipfix.py": (
            "import unittest\n"
            "class X11Test(unittest.TestCase):\n"
            "    @classmethod\n"
            "    def setUpClass(cls): raise unittest.SkipTest('no Xwayland')\n"
            "    def test_a(self): pass\n")})
        self.assertEqual(proc.returncode, 0, proc.stderr)
        self.assertEqual(self.results(out, "test_skipfix"), {"test_skipfix.X11Test.test_a": "skip"})

    def test_repeated_subtest_descriptions_each_count(self):
        proc, out = self.suite_on({"test_same.py": (
            "import unittest\n"
            "class SameTest(unittest.TestCase):\n"
            "    def test_twice(self):\n"
            "        with self.subTest():\n"
            "            self.fail()\n"
            "        with self.subTest():\n"
            "            pass\n")})
        self.assertEqual(proc.returncode, 0, proc.stderr)
        self.assertEqual(self.results(out, "test_same"), {
            "test_same.SameTest.test_twice (<subtest>)": "fail",
            "test_same.SameTest.test_twice (<subtest>) #2": "pass",
        })

    def test_a_teardown_that_hangs_is_a_timeout(self):
        proc, out = self.suite_on({"test_stuck.py": (
            "import time, unittest\n"
            "class StuckTest(unittest.TestCase):\n"
            "    @classmethod\n"
            "    def tearDownClass(cls): time.sleep(300)\n"
            "    def test_a(self): pass\n")}, FILE_TIMEOUT="1")
        self.assertEqual(proc.returncode, 0, proc.stderr)
        self.assertEqual(self.results(out, "test_stuck"), {
            "test_stuck.StuckTest.test_a": "pass",
            "(the rest of the file)": "timeout",
        })

    def test_a_teardown_fixtures_error_counts_as_an_error(self):
        proc, out = self.suite_on({"test_down.py": (
            "import unittest\n"
            "class DownTest(unittest.TestCase):\n"
            "    @classmethod\n"
            "    def tearDownClass(cls): raise RuntimeError('compositor left running')\n"
            "    def test_a(self): pass\n")})
        self.assertEqual(proc.returncode, 0, proc.stderr)
        self.assertEqual(self.results(out, "test_down"), {
            "test_down.DownTest.test_a": "pass",
            "tearDownClass (test_down.DownTest)": "error",
        })

    def test_a_test_that_hangs_after_its_subtests_is_a_timeout(self):
        proc, out = self.suite_on({"test_hang.py": (
            "import time, unittest\n"
            "class HangTest(unittest.TestCase):\n"
            "    def test_subs_then_hangs(self):\n"
            "        with self.subTest(i=0):\n"
            "            pass\n"
            "        time.sleep(300)\n")}, FILE_TIMEOUT="1")
        self.assertEqual(proc.returncode, 0, proc.stderr)
        self.assertEqual(self.results(out, "test_hang"), {
            "test_hang.HangTest.test_subs_then_hangs (i=0)": "pass",
            "test_hang.HangTest.test_subs_then_hangs": "timeout",
        })

    def test_summary_totals_every_file(self):
        suite = load("suite")
        counts = lambda p, f: {"pass": p, "fail": f, "error": 0, "skip": 0, "timeout": 0}
        head, table, totals = suite.summary({"test_a": {"counts": counts(2, 1)},
                                             "test_b": {"counts": counts(1, 3)}})
        self.assertEqual(head, "tidewm's suite on tidesway: 3 of 7 pass")
        self.assertEqual(totals["fail"], 4)
        self.assertIn("| test_b | 4 | 1 | 3 |", table)


class GateTest(unittest.TestCase):
    PASSING = "m.T.test_a\nm.T.test_b\nm.T.test_flaky\n"
    FLAKY = "# Races the pointer\nm.T.test_flaky\n"

    def gate(self, outcomes, passing=PASSING, flaky=FLAKY):
        """Runs gate on a results.json holding these outcomes for module m"""
        d = tempfile.TemporaryDirectory()
        self.addCleanup(d.cleanup)
        paths = {}
        for name, text in (("results.json", json.dumps({"m": {"tests": outcomes}})),
                           ("passing.txt", passing), ("flaky.txt", flaky)):
            paths[name] = os.path.join(d.name, name)
            with open(paths[name], "w") as f:
                f.write(text)
        self.report = os.path.join(d.name, "gate.md")
        return subprocess.run([os.path.join(HERE, "gate"), paths["results.json"],
                               "--passing", paths["passing.txt"],
                               f"--flaky={paths['flaky.txt']}", "--report", self.report],
                              capture_output=True, text=True, timeout=10)

    def test_passes_when_every_baseline_test_passes(self):
        proc = self.gate({"m.T.test_a": "pass", "m.T.test_b": "pass",
                          "m.T.test_flaky": "pass", "m.T.test_c": "fail"})
        self.assertEqual(proc.returncode, 0, proc.stderr)
        self.assertIn("No regressions", proc.stdout)

    def test_a_baseline_test_that_stops_passing_fails_naming_it(self):
        proc = self.gate({"m.T.test_a": "pass", "m.T.test_b": "timeout",
                          "m.T.test_flaky": "pass"})
        self.assertEqual(proc.returncode, 1)
        self.assertIn("`m.T.test_b`: timeout", proc.stdout)
        self.assertNotIn("m.T.test_a", proc.stdout)
        with open(self.report) as f:
            self.assertEqual(f.read(), proc.stdout)

    def test_a_baseline_test_missing_from_the_results_fails(self):
        proc = self.gate({"m.T.test_a": "pass", "m.T.test_flaky": "pass"})
        self.assertEqual(proc.returncode, 1)
        self.assertIn("`m.T.test_b`: missing", proc.stdout)

    def test_a_flaky_failure_doesnt_fail(self):
        proc = self.gate({"m.T.test_a": "pass", "m.T.test_b": "pass",
                          "m.T.test_flaky": "error"})
        self.assertEqual(proc.returncode, 0, proc.stderr)
        self.assertIn("Known flaky", proc.stdout)
        self.assertIn("`m.T.test_flaky`: error", proc.stdout)

    def test_a_newly_passing_test_is_listed_not_failed(self):
        proc = self.gate({"m.T.test_a": "pass", "m.T.test_b": "pass",
                          "m.T.test_flaky": "pass", "m.T.test_new (i=1)": "pass"})
        self.assertEqual(proc.returncode, 0, proc.stderr)
        self.assertIn("Newly passing (1)", proc.stdout)
        self.assertIn("`m.T.test_new (i=1)`", proc.stdout)

    def test_an_id_without_its_module_gets_it(self):
        gate = load("gate")
        self.assertEqual(gate.test_ids({"m": {"tests": {"tearDownClass (m.T)": "error"}}}),
                         {"m: tearDownClass (m.T)": "error"})

    def test_a_flaky_test_needs_a_reason(self):
        proc = self.gate({"m.T.test_a": "pass"}, flaky="m.T.test_flaky\n")
        self.assertEqual(proc.returncode, 2)
        self.assertIn("no reason", proc.stderr)
        # A blank line ends the reason above it
        proc = self.gate({"m.T.test_a": "pass"}, flaky="# Races\nm.T.x\n\nm.T.test_flaky\n")
        self.assertEqual(proc.returncode, 2)

    def test_the_committed_lists_are_well_formed(self):
        gate = load("gate")
        passing = gate.read_list(os.path.join(HERE, "passing.txt"))
        flaky = gate.read_list(os.path.join(HERE, "flaky.txt"), need_reasons=True)
        self.assertEqual(len(passing), len(set(passing)), "passing.txt repeats an id")
        self.assertEqual(passing, sorted(passing), "passing.txt isn't sorted")
        self.assertFalse(set(passing) & set(flaky), "an id is in both passing.txt and flaky.txt")


if __name__ == "__main__":
    unittest.main()
