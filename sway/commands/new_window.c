#include "log.h"
#include "sway/commands.h"
#include "sway/config.h"
#include "sway/tide/commands.h"

struct cmd_results *cmd_new_window(int argc, char **argv) {
	// tide: where a new window goes
	struct cmd_results *tide = tide_cmd_new_window(argc, argv);
	if (tide) {
		return tide;
	}
	sway_log(SWAY_INFO, "Warning: new_window is deprecated. "
		"Use default_border instead.");
	if (config->reading) {
		config_add_swaynag_warning("new_window is deprecated. "
			"Use default_border instead.");
	}
	return cmd_default_border(argc, argv);
}
