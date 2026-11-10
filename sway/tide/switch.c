/*
 * tidewm's switch command (its docs/ipc.md): `switch lid|tablet on|off`
 * runs the config's bindswitch lines as if that switch had moved, for a
 * test, which has no lid, or a lid the hardware doesn't report.
 */
#include <string.h>
#include "sway/commands.h"
#include "sway/input/input-manager.h"
#include "sway/input/switch.h"

struct cmd_results *cmd_switch(int argc, char **argv) {
	const char *usage = "switch takes lid or tablet, then on or off";
	if (argc != 2) {
		return cmd_results_new(CMD_INVALID, "%s", usage);
	}
	enum wlr_switch_type type;
	if (strcmp(argv[0], "lid") == 0) {
		type = WLR_SWITCH_TYPE_LID;
	} else if (strcmp(argv[0], "tablet") == 0) {
		type = WLR_SWITCH_TYPE_TABLET_MODE;
	} else {
		return cmd_results_new(CMD_INVALID, "%s", usage);
	}
	enum wlr_switch_state state;
	if (strcmp(argv[1], "on") == 0) {
		state = WLR_SWITCH_STATE_ON;
	} else if (strcmp(argv[1], "off") == 0) {
		state = WLR_SWITCH_STATE_OFF;
	} else {
		return cmd_results_new(CMD_INVALID, "%s", usage);
	}
	tide_switch_moved(input_manager_current_seat(), type, state);
	return cmd_results_new(CMD_SUCCESS, NULL);
}
