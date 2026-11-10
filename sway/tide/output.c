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
#include "sway/layers.h"
#include "sway/output.h"
#include "sway/scene_descriptor.h"
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

static json_object *rect(int x, int y, int width, int height) {
	json_object *r = json_object_new_object();
	json_object_object_add(r, "x", json_object_new_int(x));
	json_object_object_add(r, "y", json_object_new_int(y));
	json_object_object_add(r, "width", json_object_new_int(width));
	json_object_object_add(r, "height", json_object_new_int(height));
	return r;
}

static void describe_tree(struct sway_output *output, struct wlr_scene_tree *tree,
		const char *name, json_object *layers) {
	struct wlr_scene_node *node;
	wl_list_for_each(node, &tree->children, link) {
		struct sway_layer_surface *surface = scene_descriptor_try_get(node,
			SWAY_SCENE_DESC_LAYER_SHELL);
		if (!surface || !surface->layer_surface->surface->mapped) {
			continue;
		}
		struct wlr_layer_surface_v1 *layer = surface->layer_surface;
		json_object *o = json_object_new_object();
		json_object_object_add(o, "namespace", json_object_new_string(layer->namespace));
		json_object_object_add(o, "layer", json_object_new_string(name));
		json_object_object_add(o, "exclusive_zone",
			json_object_new_int(layer->current.exclusive_zone));
		json_object_object_add(o, "rect", rect(output->lx + node->x, output->ly + node->y,
			(int)layer->current.actual_width, (int)layer->current.actual_height));
		json_object_array_add(layers, o);
	}
}

void tide_output_describe_layers(struct sway_output *output, json_object *object) {
	json_object *layers = json_object_new_array();
	describe_tree(output, output->layers.shell_background, "background", layers);
	describe_tree(output, output->layers.shell_bottom, "bottom", layers);
	describe_tree(output, output->layers.shell_top, "top", layers);
	describe_tree(output, output->layers.shell_overlay, "overlay", layers);
	json_object_object_add(object, "layers", layers);
	struct wlr_box *a = &output->usable_area;
	json_object_object_add(object, "window_area",
		rect(output->lx + a->x, output->ly + a->y, a->width, a->height));
}
