/*
 * The output command's power, as tidewm takes it.
 * include/sway/tide/output.h says how it behaves.
 */
#include <stdbool.h>
#include <string.h>
#include <strings.h>
#include <wlr/types/wlr_output.h>
#include "sway/commands.h"
#include "sway/config.h"
#include "sway/output.h"
#include "sway/tide/output.h"
#include "sway/tree/node.h"
#include "sway/tree/root.h"

static bool applying; // running sway's own command for one output

// sway's output NAME power on|off, for one output
static struct cmd_results *apply(struct sway_output *output, bool on) {
	char *argv[] = { output->wlr_output->name, "power", on ? "on" : "off" };
	applying = true;
	struct cmd_results *result = cmd_output(3, argv);
	applying = false;
	return result;
}

bool tide_output_power(int argc, char **argv, struct cmd_results **result) {
	if (applying || config->reading || argc < 2 || (strcasecmp(argv[1], "power") != 0
			&& strcasecmp(argv[1], "dpms") != 0)) {
		return false;
	}
	if (argc != 3) {
		*result = cmd_results_new(CMD_FAILURE,
			"Expected output NAME power on|off|toggle, alone");
		return true;
	}
	int on = strcasecmp(argv[2], "on") == 0 ? 1
		: strcasecmp(argv[2], "off") == 0 ? 0
		: strcasecmp(argv[2], "toggle") == 0 ? -1 : -2;
	if (on == -2) {
		*result = cmd_results_new(CMD_INVALID,
			"Expected output NAME power on|off|toggle");
		return true;
	}
	struct sway_output *only = NULL;
	if (strcmp(argv[0], "-") == 0 || strcmp(argv[0], "--") == 0) {
		// The focused output, as sway's command takes it
		only = config->handler_context.node
			? node_get_output(config->handler_context.node) : NULL;
		if (!only || only == root->fallback_output) {
			*result = cmd_results_new(CMD_FAILURE, "Unknown output");
			return true;
		}
	} else if (strcmp(argv[0], "*") != 0) {
		only = all_output_by_name_or_id(argv[0]);
		if (!only || !only->wlr_output) {
			*result = cmd_results_new(CMD_FAILURE,
				"No output is called %s", argv[0]);
			return true;
		}
	}
	if (only && !only->enabled) {
		*result = cmd_results_new(CMD_FAILURE, "%s is disabled",
			only->wlr_output->name);
		return true;
	}
	*result = NULL;
	for (int i = 0; i < root->outputs->length; i++) {
		struct sway_output *output = root->outputs->items[i];
		if (only && output != only) {
			continue;
		}
		bool power = on >= 0 ? on : !output->wlr_output->enabled;
		struct cmd_results *r = apply(output, power);
		if (r && r->status != CMD_SUCCESS && !*result) {
			*result = r;
		} else if (r) {
			free_cmd_results(r);
		}
	}
	if (!*result) {
		*result = cmd_results_new(CMD_SUCCESS, NULL);
	}
	return true;
}
