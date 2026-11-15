"""tidewm's config lines where tidewm's own tests can't reach them on
tidesway yet."""

import os
import select
import shlex
import unittest

import tidesway  # noqa: F401  (sets up harness)
from harness import TIMEOUT, TidewmTest


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


if __name__ == "__main__":
    unittest.main()
