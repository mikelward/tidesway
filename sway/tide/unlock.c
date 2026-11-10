/*
 * tidewm's unlock command (its docs/ipc.md, tide's SPEC §10): drops a lock
 * whose screen locker has gone, as after it crashed, showing the windows
 * again, so a TTY's swaymsg gets you back in and keeps the session. It
 * fails while a locker holds the lock, and with nothing locked.
 */
#include "sway/commands.h"
#include "sway/server.h"

struct cmd_results *cmd_unlock(int argc, char **argv) {
	if (argc) {
		return cmd_results_new(CMD_INVALID, "unlock takes nothing");
	}
	const char *why = tide_lock_drop_abandoned();
	return why ? cmd_results_new(CMD_FAILURE, "%s", why)
		: cmd_results_new(CMD_SUCCESS, NULL);
}
