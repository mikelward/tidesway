/*
 * Input lines checked as tidewm checks them (its settings.h), failing in
 * its words. include/sway/tide/inputcheck.h says what's checked.
 */
#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "stringop.h"
#include "sway/commands.h"
#include "sway/config.h"
#include "sway/input/input-manager.h"
#include "sway/server.h"
#include "sway/tide/inputcheck.h"
#include "sway/tide/inputname.h"

bool tide_input_bad_name(const char *name, char *err, size_t size) {
	// sway's types, of which tidewm has keyboard, pointer and touchpad
	static const char *const types[] = {
		"keyboard", "pointer", "touchpad", "touch", "tablet_tool", "tablet_pad", "switch",
	};
	// A name with a space came of a quote left open
	if (!name || !*name || strpbrk(name, " \t")) {
		snprintf(err, size, "input needs a device's identifier, a type:, or *, then a setting");
		return true;
	}
	if (!has_prefix(name, "type:")) {
		return false;
	}
	for (size_t i = 0; i < sizeof(types) / sizeof(types[0]); i++) {
		if (strcmp(name + 5, types[i]) == 0) {
			return false;
		}
	}
	snprintf(err, size, "input %s: no device type is called %s", name, name + 5);
	return true;
}

bool tide_input_bad_block(const char *prefix, char *err, size_t size) {
	/* As sway reads the lines in it: its variables replaced, though sway
	 * replaces them later, then split as split_args splits, so a name in
	 * quotes may hold a space */
	char *copy = strdup(prefix);
	char *expanded = copy ? do_var_replacement(copy) : NULL;
	int argc = 0;
	char **argv = expanded ? split_args(expanded, &argc) : NULL;
	free(expanded);
	if (!argv) {
		snprintf(err, size, "input: no memory to read its name");
		return true;
	}
	char name[256];
	snprintf(name, sizeof(name), "%s", argc > 0 ? argv[0] : "");
	free_argv(argc, argv);
	strip_quotes(name);
	return tide_input_bad_name(name, err, size);
}

/* Whether the config line being read has a " it doesn't close, read as
 * sway's split_args reads it: a " in single quotes, or escaped, is a
 * character */
static bool quote_open(void) {
	const char *line = config->reading ? config->current_config_line : NULL;
	bool in_string = false, in_char = false, escaped = false;
	for (const char *c = line; c && *c; c++) {
		if (*c == '"' && !in_char && !escaped) {
			in_string = !in_string;
		} else if (*c == '\'' && !in_string && !escaped) {
			in_char = !in_char;
		}
		escaped = *c == '\\' && !escaped;
	}
	return in_string;
}

static bool one_of(const char *value, const char *const *words) {
	for (; *words; words++) {
		if (strcasecmp(value, *words) == 0) {
			return true;
		}
	}
	return false;
}

struct cmd_results *tide_input_check(int argc, char **argv,
		bool (*is_setting)(const char *setting)) {
	// What sway's parse_boolean reads, and nothing else
	static const char *const booleans[] = {
		"1", "yes", "on", "true", "enable", "enabled", "active", "toggle",
		"0", "no", "off", "false", "disable", "disabled", "inactive", NULL,
	};
	// tidewm's settings that take one value, and those of them that are on or off
	static const char *const single[] = {
		"xkb_rules", "xkb_model", "xkb_layout", "xkb_variant", "xkb_options",
		"pointer_accel", "scroll_factor", "repeat_rate", "repeat_delay",
		"tap", "tap_button_map", "drag", "drag_lock", "natural_scroll", "dwt",
		"left_handed", "middle_emulation", "scroll_method", "click_method",
		"accel_profile", "events", NULL,
	};
	static const char *const onoff[] = {
		"tap", "drag", "drag_lock", "natural_scroll", "dwt", "left_handed",
		"middle_emulation", NULL,
	};
	char err[256];
	if (tide_input_bad_name(argc > 0 ? argv[0] : NULL, err, sizeof(err))) {
		return cmd_results_new(CMD_INVALID, "%s", err);
	}
	const char *name = argv[0];
	if (argc < 2) {
		return cmd_results_new(CMD_INVALID, "input %s needs a setting", name);
	}
	const char *word = argv[1];
	if (quote_open()) {
		return cmd_results_new(CMD_INVALID, "input %s: %s's \" has no closing \"", name, word);
	}
	if (!is_setting(word)) {
		return cmd_results_new(CMD_INVALID, "input %s: no setting is called %s", name, word);
	}
	if (!one_of(word, single)) {
		return NULL;
	}
	// sway's events toggle takes the modes to toggle between
	if (strcasecmp(word, "events") == 0 && argc > 3 && strcasecmp(argv[2], "toggle") == 0) {
		return NULL;
	}
	if (argc != 3) {
		return cmd_results_new(CMD_INVALID, "input %s: %s needs one value", name, word);
	}
	const char *arg = argv[2];
	char *end;
	errno = 0;
	if (strcasecmp(word, "pointer_accel") == 0) {
		double d = strtod(arg, &end);
		if (end == arg || *end || errno || !isfinite(d) || d < -1 || d > 1) {
			return cmd_results_new(CMD_INVALID,
				"input %s: pointer_accel %s isn't a number from -1 to 1", name, arg);
		}
	} else if (strcasecmp(word, "scroll_factor") == 0) {
		double d = strtod(arg, &end);
		if (end == arg || *end || errno || !isfinite(d) || d < 0) {
			return cmd_results_new(CMD_INVALID,
				"input %s: scroll_factor %s isn't a number, 0 or more", name, arg);
		}
	} else if (strcasecmp(word, "repeat_rate") == 0 || strcasecmp(word, "repeat_delay") == 0) {
		long n = strtol(arg, &end, 10);
		if (end == arg || *end || errno || n < 0 || n > 100000) {
			return cmd_results_new(CMD_INVALID,
				"input %s: %s %s isn't a whole number from 0 to 100000", name, word, arg);
		}
	} else if (one_of(word, onoff) && !one_of(arg, booleans)
			// sway's, where libinput has it
			&& !(strcasecmp(word, "drag_lock") == 0 && strcmp(arg, "enabled_sticky") == 0)) {
		return cmd_results_new(CMD_INVALID, "input %s: %s can't be %s", name, word, arg);
	}
	return NULL;
}

struct cmd_results *tide_input_unmatched(const char *name) {
	bool wildcard = strcmp(name, "*") == 0;
	bool type = has_prefix(name, "type:");
	struct sway_input_device *device;
	wl_list_for_each(device, &server.input->devices, link) {
		if (wildcard || (type && strcmp(input_device_get_type(device), name + 5) == 0)
				|| tide_input_named(device, name)) {
			return NULL;
		}
	}
	return cmd_results_new(CMD_FAILURE, "No input device matches %s", name);
}
