#include <stdint.h>
#include "sway/commands.h"
#include "sway/tide/border.h"
#include "util.h"

struct cmd_results *tide_border_check_colors(const char *name, int argc, char **argv) {
	for (int i = 0; i < argc && i < 5; i++) {
		uint32_t color;
		if (!parse_color(argv[i], &color)) {
			return cmd_results_new(CMD_INVALID, "%s: %s isn't a color, "
				"which is #RRGGBB or #RRGGBBAA", name, argv[i]);
		}
	}
	if (argc < 3 || argc > 5) {
		return cmd_results_new(CMD_INVALID, "%s takes 3 to 5 colors: border, "
			"background, text, indicator, child_border", name);
	}
	return NULL;
}
