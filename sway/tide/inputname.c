/*
 * An input device's Hyprland name, as tidewm gives it (its docs/ipc.md):
 * its name in lower case, with a space, newline or comma as "-", or
 * "unknown-device" for none; a second device of that name has "-1" after
 * it, and so on, in GET_INPUTS' order. An input line can name a device by
 * it, as tide's config, written for Hyprland too, does.
 */
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include "list.h"
#include "log.h"
#include "stringop.h"
#include "sway/input/input-manager.h"
#include "sway/server.h"
#include "sway/tide/inputname.h"

static char *base_name(struct sway_input_device *device) {
	const char *name = device->wlr_device->name;
	if (!name || !*name) {
		return strdup("unknown-device");
	}
	char *base = strdup(name);
	if (!base) {
		return NULL;
	}
	for (char *c = base; *c; c++) {
		*c = (*c == ' ' || *c == '\n' || *c == ',') ? '-' : tolower((unsigned char)*c);
	}
	return base;
}

static bool taken(list_t *names, const char *name) {
	for (int i = 0; i < names->length; i++) {
		if (strcmp(names->items[i], name) == 0) {
			return true;
		}
	}
	return false;
}

char *tide_input_hyprland_name(struct sway_input_device *device) {
	list_t *names = create_list();
	char *found = NULL;
	struct sway_input_device *d;
	wl_list_for_each(d, &server.input->devices, link) {
		char *base = base_name(d);
		if (!base) {
			sway_log(SWAY_ERROR, "tide: can't allocate an input's name");
			break;
		}
		char *name = strdup(base);
		for (int n = 1; name && taken(names, name); n++) {
			free(name);
			name = format_str("%s-%d", base, n);
		}
		free(base);
		if (!name) {
			sway_log(SWAY_ERROR, "tide: can't allocate an input's name");
			break;
		}
		list_add(names, name);
		if (d == device) {
			found = strdup(name);
			break;
		}
	}
	list_free_items_and_destroy(names);
	return found;
}

bool tide_input_namesakes(struct sway_input_device *a, struct sway_input_device *b) {
	char *x = base_name(a), *y = base_name(b);
	bool same = x && y && strcmp(x, y) == 0;
	free(x);
	free(y);
	return same;
}

bool tide_input_named(struct sway_input_device *device, const char *name) {
	if (strcmp(device->identifier, name) == 0) {
		return true;
	}
	if (has_prefix(name, "type:") || strcmp(name, "*") == 0) {
		return false;
	}
	char *hyprland = tide_input_hyprland_name(device);
	bool named = hyprland && strcmp(hyprland, name) == 0;
	free(hyprland);
	return named;
}
