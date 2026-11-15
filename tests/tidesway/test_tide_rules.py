"""for_window lines checked as tidewm checks them, in its words, with
sway's criteria kept: tidewm's test_rules.RulesTest.test_rule_errors
without its GET_CONFIG, which the harness's wrapper config changes."""

import unittest

import tidesway  # noqa: F401  (sets up harness)
from harness import EVENT_WINDOW, RUN_COMMAND, TidewmTest


class RuleErrorsTest(TidewmTest):
    CONFIG = 'for_window [app_id="^test\\.float$"] floating enable\n'

    def fails_to_load(self, text):
        self.wm.write_config(text)
        result, = self.connect().request(RUN_COMMAND, "reload")
        self.assertFalse(result["success"], text)
        return result["error"]

    def test_bad_rules_say_so_in_tidewms_words(self):
        for text, error in [
            ("for_window floating enable\n", ":1: for_window needs [criteria], then commands"),
            ("for_window [app_id=x]\n", ":1: for_window needs [criteria], then commands"),
            ("for_window [] nop\n", ":1: for_window needs [criteria], then commands"),
            ("for_window [colour=red] nop\n", ":1: for_window: no criterion is called colour"),
            ("for_window [app_id=a app_id=b] nop\n", ":1: for_window: app_id is there twice"),
            ('for_window [title="open] nop\n', ":1: for_window: title's value has no closing \""),
            ("for_window [title=(] nop\n", ":1: for_window: title=(: "),
            ("for_window [app_id=x\n", ":1: for_window's criteria have no ]"),
            ("for_window [app_id=x nop\n", ":1: for_window's criteria are key=value, in [ ]"),
            ("for_window [app_id] nop\n", ":1: for_window's criteria are key=value, in [ ]"),
        ]:
            with self.subTest(text=text):
                self.assertIn(error, self.fails_to_load(text))

    def test_sways_own_criteria_still_load(self):
        self.wm.write_config('for_window [floating] nop\nfor_window [tiling app_id="a b" '
                             'con_mark=x] nop\n'
                             # sway's spaces around =, which criteria_parse skips
                             'for_window [app_id = "a b" title= c] nop\n'
                             'for_window [floating instance =d] nop\n'
                             'for_window [app_id="^test\\.float$"] floating enable\n')
        result, = self.connect().request(RUN_COMMAND, "reload")
        self.assertTrue(result["success"], result)
        ipc = self.connect()
        ipc.subscribe("window")
        _, con = self.open_window(ipc, "test.float")
        self.assertEqual(con["type"], "floating_con")


if __name__ == "__main__":
    unittest.main()
