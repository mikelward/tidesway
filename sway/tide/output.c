/*
 * The output command's power, as tidewm takes it.
 * include/sway/tide/output.h says how it behaves.
 */
#include <stdbool.h>
#include <string.h>
#include <strings.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/util/box.h>
#include "sway/commands.h"
#include "sway/config.h"
#include "sway/layers.h"
#include "sway/output.h"
#include "sway/scene_descriptor.h"
#include "sway/tide/output.h"
#include "sway/tree/node.h"
#include "sway/tree/root.h"
#include "list.h"
#include "log.h"

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

// Where output goes on side, beside the outputs placed already
static void beside(struct sway_output *output, int side, list_t *placed, int *x, int *y) {
	int width, height;
	wlr_output_effective_resolution(output->wlr_output, &width, &height);
	bool any = false;
	int left = 0, right = 0, top = 0, bottom = 0;
	for (int i = 0; i < placed->length; i++) {
		struct sway_output *other = placed->items[i];
		struct wlr_box box;
		wlr_output_layout_get_box(root->output_layout, other->wlr_output, &box);
		if (wlr_box_empty(&box)) {
			continue;
		}
		left = any && left < box.x ? left : box.x;
		top = any && top < box.y ? top : box.y;
		right = any && right > box.x + box.width ? right : box.x + box.width;
		bottom = any && bottom > box.y + box.height ? bottom : box.y + box.height;
		any = true;
	}
	*x = *y = 0;
	if (!any) {
		return;
	}
	switch (side) {
	case TIDE_POSITION_AUTO_LEFT:
		*x = left - width;
		break;
	case TIDE_POSITION_AUTO_UP:
		*y = top - height;
		break;
	case TIDE_POSITION_AUTO_DOWN:
		*y = bottom;
		break;
	default:
		*x = right;
		break;
	}
}

void tide_output_place_beside(void) {
	list_t *placed = create_list();
	if (!placed) {
		sway_log(SWAY_ERROR, "tide: no memory to place outputs beside the others");
		return;
	}
	for (int i = 0; i < root->outputs->length; i++) {
		struct sway_output *output = root->outputs->items[i];
		if (!output->tide_position_side) {
			list_add(placed, output);
		}
	}
	// In the order they connected, each beside those placed before it
	for (int i = 0; i < root->outputs->length; i++) {
		struct sway_output *output = root->outputs->items[i];
		if (!output->tide_position_side) {
			continue;
		}
		int x, y;
		beside(output, output->tide_position_side, placed, &x, &y);
		sway_log(SWAY_DEBUG, "Set %s position beside the others, %d, %d",
			output->wlr_output->name, x, y);
		wlr_output_layout_add(root->output_layout, output->wlr_output, x, y);
		list_add(placed, output);
	}
	list_free(placed);
}

void tide_output_description(char *buf, size_t len, struct sway_output *output) {
	struct wlr_output *wlr = output->wlr_output;
	const char *parts[] = { wlr->make, wlr->model, wlr->serial };
	size_t at = 0;
	if (len) {
		buf[0] = '\0';
	}
	for (size_t i = 0; i < sizeof(parts) / sizeof(parts[0]); i++) {
		if (!parts[i] || !*parts[i]) {
			continue;
		}
		if (at && at + 1 < len) {
			buf[at++] = ' ';
		}
		for (const char *c = parts[i]; *c && at + 1 < len; c++) {
			if (*c != ',') { // Hyprland's has none
				buf[at++] = *c;
			}
		}
		buf[at] = '\0';
	}
}

bool tide_output_named(const char *name) {
	return strcmp(name, "*") == 0 || all_output_by_name_or_id(name) != NULL;
}
