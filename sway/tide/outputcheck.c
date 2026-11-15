/*
 * Output lines checked as tidewm checks them, failing in its words.
 * include/sway/tide/outputcheck.h says what's checked.
 */
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "sway/commands.h"
#include "sway/config.h"
#include "sway/tide/outputcheck.h"

/* Whether the config line being read has a " it doesn't close, read as
 * sway's split_args reads it: a " in single quotes, or escaped, is a
 * character. As inputcheck.c's. */
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
	for (; value && *words; words++) {
		if (strcasecmp(value, *words) == 0) {
			return true;
		}
	}
	return false;
}

static bool digits(const char *s) {
	if (!s || !*s) {
		return false;
	}
	for (; *s; s++) {
		if (*s < '0' || *s > '9') {
			return false;
		}
	}
	return true;
}

/* WIDTHxHEIGHT, then @HZ, with Hz or not, as tidewm takes a mode: @0, as
 * nwg-displays writes for an output with no rate, is any rate */
static bool good_mode(const char *arg) {
	int width, height, used = 0;
	if (!arg || sscanf(arg, "%dx%d%n", &width, &height, &used) != 2
			|| width <= 0 || height <= 0 || arg[0] < '0' || arg[0] > '9') {
		return false;
	}
	const char *rest = arg + used;
	if (!*rest) {
		return true;
	}
	if (*rest != '@') {
		return false;
	}
	char *end;
	float hz = strtof(rest + 1, &end);
	if (end == rest + 1 || (*end && strcasecmp(end, "hz") != 0)) {
		return false;
	}
	return isfinite(hz) && hz >= 0 && hz <= 1000;
}

static bool good_scale(const char *arg) {
	char *end;
	float scale = arg ? strtof(arg, &end) : 0;
	return arg && end != arg && !*end && isfinite(scale) && scale > 0 && scale <= 10;
}

static bool good_number(const char *arg, int *n) {
	char extra;
	return arg && sscanf(arg, "%d%c", n, &extra) == 1;
}

struct cmd_results *tide_output_check(int argc, char **argv) {
	// What sway's parse_boolean reads as on or off, and toggle
	static const char *const booleans[] = {
		"1", "yes", "on", "true", "enable", "enabled", "active", "toggle",
		"0", "no", "off", "false", "disable", "disabled", "inactive", NULL,
	};
	static const char *const transforms[] = {
		"normal", "90", "180", "270", "flipped", "flipped-90", "flipped-180",
		"flipped-270", NULL,
	};
	static const char *const places[] = {
		"auto", "auto-right", "auto-left", "auto-up", "auto-down", NULL,
	};
	static const char *const filters[] = { "linear", "nearest", "smart", NULL };
	static const char *const alone[] = { "enable", "disable", "toggle", "unplug", NULL };
	// sway's, which tidewm hasn't: sway checks them, and what follows
	static const char *const sways[] = {
		"allow_tearing", "background", "bg", "color_profile", "hdr",
		"max_render_time", "modeline", "render_bit_depth", "subpixel", NULL,
	};
	bool open = quote_open();
	// A name with a space came of a quote left open
	if (argc < 1 || !*argv[0] || (open && strpbrk(argv[0], " \t"))) {
		return cmd_results_new(CMD_INVALID, "output needs a name, or *, then settings");
	}
	const char *name = argv[0];
	if (argc < 2) {
		return cmd_results_new(CMD_INVALID, "output %s needs a setting", name);
	}
	if (open) {
		return cmd_results_new(CMD_INVALID, "output %s: a value's \" has no closing \"", name);
	}
	int i = 1;
	while (i < argc) {
		const char *word = argv[i++];
		const char *arg = i < argc ? argv[i] : NULL;
		int x, y;
		if (one_of(word, (const char *const[]){ "mode", "resolution", "res", NULL })) {
			if (arg && strcmp(arg, "--custom") == 0) {
				arg = ++i < argc ? argv[i] : NULL;
			}
			// sway's WIDTH HEIGHT too
			if (digits(arg) && i + 1 < argc && digits(argv[i + 1])) {
				i += 2;
				continue;
			}
			if (!good_mode(arg)) {
				return cmd_results_new(CMD_INVALID,
					"output %s: mode %s isn't WIDTHxHEIGHT[@HZ]", name, arg ? arg : "");
			}
			i++;
		} else if (strcasecmp(word, "scale") == 0) {
			if (!good_scale(arg)) {
				return cmd_results_new(CMD_INVALID,
					"output %s: scale %s isn't a number from above 0 to 10",
					name, arg ? arg : "");
			}
			i++;
		} else if (one_of(word, (const char *const[]){ "position", "pos", NULL })) {
			char extra;
			if (one_of(arg, places)) {
				i++;
			} else if (arg && strchr(arg, ',')
					&& sscanf(arg, "%d,%d%c", &x, &y, &extra) == 2) {
				i++;
			} else if (arg && !strchr(arg, ',') && good_number(arg, &x)
					&& i + 1 < argc && good_number(argv[i + 1], &y)) {
				i += 2;
			} else {
				return cmd_results_new(CMD_INVALID, "output %s: position needs X Y, or "
					"auto, auto-right, auto-left, auto-up or auto-down", name);
			}
		} else if (strcasecmp(word, "transform") == 0) {
			if (!one_of(arg, transforms)) {
				return cmd_results_new(CMD_INVALID, "output %s: no transform is called %s",
					name, arg ? arg : "");
			}
			i++;
			// sway's turning from where it is, at runtime
			if (i < argc && one_of(argv[i],
					(const char *const[]){ "clockwise", "anticlockwise", NULL })) {
				i++;
			}
		} else if (one_of(word, alone)) {
			continue;
		} else if (strcasecmp(word, "adaptive_sync") == 0) {
			if (!one_of(arg, booleans)) {
				return cmd_results_new(CMD_INVALID,
					"output %s: adaptive_sync takes on or off", name);
			}
			i++;
		} else if (one_of(word, (const char *const[]){ "power", "dpms", NULL })) {
			if (!one_of(arg, booleans)) {
				return cmd_results_new(CMD_INVALID, "output %s: %s takes on or off",
					name, word);
			}
			i++;
		} else if (strcasecmp(word, "scale_filter") == 0) {
			if (!one_of(arg, filters)) {
				return cmd_results_new(CMD_INVALID,
					"output %s: scale_filter takes linear, nearest or smart", name);
			}
			i++;
		} else if (one_of(word, sways)) {
			return NULL;
		} else {
			return cmd_results_new(CMD_INVALID, "output %s: no setting is called %s",
				name, word);
		}
	}
	return NULL;
}
