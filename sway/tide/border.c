#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "sway/commands.h"
#include "sway/config.h"
#include "sway/tide/border.h"
#include "sway/tree/container.h"
#include "sway/tree/root.h"
#include "sway/tree/view.h"
#include "util.h"

struct cmd_results *tide_default_border(int argc, char **argv) {
	enum sway_container_border border = B_PIXEL;
	int width = TIDE_BORDER_WIDTH;
	if (argc == 1 && strcmp(argv[0], "none") == 0) {
		border = B_NONE;
	} else if (argc >= 1 && argc <= 2 && strcmp(argv[0], "pixel") == 0) {
		if (argc == 2) {
			char *end;
			errno = 0;
			long v = strtol(argv[1], &end, 10);
			if (!*argv[1] || *end || errno || v < 0 || v > 100) {
				goto fail;
			}
			width = (int)v;
		}
	} else {
		goto fail;
	}
	// Floating windows' too: tidewm has no default_floating_border
	config->border = config->floating_border = border;
	config->border_thickness = config->floating_border_thickness = width;
	return cmd_results_new(CMD_SUCCESS, NULL);
fail:
	return cmd_results_new(CMD_INVALID, "default_border takes none, or pixel "
		"and a whole number of pixels from 0 to 100");
}

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

static void take_border(struct sway_container *con, void *data) {
	if (!con->view) {
		return;
	}
	bool floating = container_is_floating(con);
	con->pending.border = floating ? config->floating_border : config->border;
	con->pending.border_thickness = floating ?
		config->floating_border_thickness : config->border_thickness;
	// A floating window's box stays, its content taking what the border
	// gave up or giving it what it took, as the reload arranges it
	node_set_dirty(&con->node);
}

void tide_border_reloaded(void) {
	root_for_each_container(take_border, NULL);
}
