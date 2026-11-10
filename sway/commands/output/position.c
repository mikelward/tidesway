#include <strings.h>
#include "sway/commands.h"
#include "sway/config.h"

struct cmd_results *output_cmd_position(int argc, char **argv) {
	if (!config->handler_context.output_config) {
		return cmd_results_new(CMD_FAILURE, "Missing output config");
	}
	if (!argc) {
		return cmd_results_new(CMD_INVALID, "Missing position argument.");
	}

	// tide: auto, auto-right, auto-left, auto-up or auto-down, beside the
	// others, as tidewm and Hyprland's monitor rules place it
	static const char *const sides[] = {
		[TIDE_POSITION_AUTO_RIGHT - TIDE_POSITION_AUTO] = "auto-right",
		[TIDE_POSITION_AUTO_LEFT - TIDE_POSITION_AUTO] = "auto-left",
		[TIDE_POSITION_AUTO_UP - TIDE_POSITION_AUTO] = "auto-up",
		[TIDE_POSITION_AUTO_DOWN - TIDE_POSITION_AUTO] = "auto-down",
	};
	int side = strcasecmp(*argv, "auto") == 0 ? TIDE_POSITION_AUTO_RIGHT : 0;
	for (size_t i = 1; !side && i < sizeof(sides) / sizeof(sides[0]); i++) {
		if (strcasecmp(*argv, sides[i]) == 0) {
			side = TIDE_POSITION_AUTO + (int)i;
		}
	}
	if (side) {
		config->handler_context.output_config->x = side;
		config->handler_context.output_config->y = TIDE_POSITION_AUTO;
		config->handler_context.leftovers.argc = argc - 1;
		config->handler_context.leftovers.argv = argv + 1;
		return NULL;
	}

	char *end;
	config->handler_context.output_config->x = strtol(*argv, &end, 10);
	if (*end) {
		// Format is 1234,4321
		if (*end != ',') {
			return cmd_results_new(CMD_INVALID, "Invalid position x.");
		}
		++end;
		config->handler_context.output_config->y = strtol(end, &end, 10);
		if (*end) {
			return cmd_results_new(CMD_INVALID, "Invalid position y.");
		}
	} else {
		// Format is 1234 4321 (legacy)
		argc--; argv++;
		if (!argc) {
			return cmd_results_new(CMD_INVALID, "Missing position argument (y).");
		}
		config->handler_context.output_config->y = strtol(*argv, &end, 10);
		if (*end) {
			return cmd_results_new(CMD_INVALID, "Invalid position y.");
		}
	}

	config->handler_context.leftovers.argc = argc - 1;
	config->handler_context.leftovers.argv = argv + 1;
	return NULL;
}

