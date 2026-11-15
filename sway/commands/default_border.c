#include "sway/commands.h"
#include "sway/tide/border.h"

// tide: tidewm's default_border, every window's, tiled and floating
struct cmd_results *cmd_default_border(int argc, char **argv) {
	return tide_default_border(argc, argv);
}
