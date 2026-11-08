/*
 * tide's layout commands and config lines, as tidewm takes them (its
 * docs/ipc.md and docs/config.md): layout tile|threecol|twocol|monocle,
 * layout next|prev|toggle, mfact, nmaster, single_width, zoom and
 * movestack; and single_window, ultrawide_aspect, default_layout,
 * layout_defaults, layout_cycle and new_window.
 */
#include <errno.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "sway/commands.h"
#include "sway/config.h"
#include "sway/input/input-manager.h"
#include "sway/input/seat.h"
#include "sway/tide/commands.h"
#include "sway/tide/layout.h"
#include "sway/tree/arrange.h"
#include "sway/tree/container.h"
#include "sway/tree/workspace.h"

static const char *const mode_names[TIDE_MODES] = { "tile", "threecol", "twocol" };

static enum tide_layout parse_layout(const char *name) {
	for (int lt = 0; lt < TIDE_LAYOUTS; lt++) {
		if (strcasecmp(name, tide_layout_name(lt)) == 0) {
			return lt;
		}
	}
	return TIDE_NONE;
}

static enum tide_layout parse_mode(const char *name) {
	for (int lt = 0; lt < TIDE_MODES; lt++) {
		if (strcmp(name, mode_names[lt]) == 0) {
			return lt;
		}
	}
	return TIDE_NONE;
}

/* A finite number with nothing after it. Its sign, if it has one, says
 * it's relative. */
static bool parse_number(const char *arg, float *value, bool *relative) {
	char *end;
	*relative = *arg == '+' || *arg == '-';
	*value = strtof(arg, &end);
	return end != arg && !*end && isfinite(*value);
}

static bool parse_double(const char *arg, double *value) {
	char *end;
	errno = 0;
	*value = strtod(arg, &end);
	return end != arg && !*end && !errno;
}

// The focused workspace's tide layouts, or an error in *error
static struct sway_workspace *tide_workspace(struct cmd_results **error) {
	struct sway_workspace *ws = config->handler_context.workspace;
	if (!ws) {
		*error = cmd_results_new(CMD_FAILURE, "No workspace");
		return NULL;
	}
	if (tide_workspace_layout(ws) == TIDE_NONE) {
		*error = cmd_results_new(CMD_FAILURE,
			"This workspace is in sway's layouts, not tide's");
		return NULL;
	}
	return ws;
}

struct cmd_results *tide_cmd_layout(int argc, char **argv) {
	struct sway_workspace *ws = config->handler_context.workspace;
	if (!ws) {
		return NULL;
	}
	if (argc == 1) {
		enum tide_layout lt = parse_layout(argv[0]);
		if (lt == TIDE_NONE && (strcasecmp(argv[0], "next") == 0
				|| strcasecmp(argv[0], "prev") == 0)) {
			lt = tide_cycle(ws, strcasecmp(argv[0], "next") == 0 ? 1 : -1);
		}
		if (lt != TIDE_NONE) {
			tide_set_layout(ws, lt);
			return cmd_results_new(CMD_SUCCESS, NULL);
		}
		if (strcasecmp(argv[0], "toggle") == 0
				&& tide_workspace_layout(ws) != TIDE_NONE) {
			tide_set_layout(ws, TIDE_NONE);
			return cmd_results_new(CMD_SUCCESS, NULL);
		}
		if (strcasecmp(argv[0], "floating") == 0) {
			return cmd_results_new(CMD_FAILURE, "tidesway has no floating layout");
		}
	}
	// One of sway's layouts: sway's command checks it, then leaves tide's
	return NULL;
}

struct cmd_results *cmd_mfact(int argc, char **argv) {
	struct cmd_results *error = NULL;
	float f;
	bool relative;
	if ((error = checkarg(argc, "mfact", EXPECTED_EQUAL_TO, 1))) {
		return error;
	}
	if (!parse_number(argv[0], &f, &relative)) {
		return cmd_results_new(CMD_INVALID, "Expected mfact F, +F or -F");
	}
	struct sway_workspace *ws = tide_workspace(&error);
	if (!ws) {
		return error;
	}
	if (relative) {
		f += ws->tide.mfact;
	}
	if (f < 0.1f || f > 0.9f) {
		return cmd_results_new(CMD_FAILURE, "mfact must stay between 0.1 and 0.9");
	}
	ws->tide.mfact = f;
	arrange_workspace(ws);
	return cmd_results_new(CMD_SUCCESS, NULL);
}

struct cmd_results *cmd_nmaster(int argc, char **argv) {
	struct cmd_results *error = NULL;
	float n;
	bool relative;
	if ((error = checkarg(argc, "nmaster", EXPECTED_EQUAL_TO, 1))) {
		return error;
	}
	if (!parse_number(argv[0], &n, &relative) || n != roundf(n) || fabsf(n) > 1000) {
		return cmd_results_new(CMD_INVALID, "Expected nmaster N, +N or -N");
	}
	struct sway_workspace *ws = tide_workspace(&error);
	if (!ws) {
		return error;
	}
	int value = (relative ? ws->tide.nmaster : 0) + (int)n;
	ws->tide.nmaster = value > 0 ? value : 0;
	arrange_workspace(ws);
	return cmd_results_new(CMD_SUCCESS, NULL);
}

struct cmd_results *cmd_single_width(int argc, char **argv) {
	struct cmd_results *error = NULL;
	float f = 0;
	bool relative = false;
	if ((error = checkarg(argc, "single_width", EXPECTED_EQUAL_TO, 1))) {
		return error;
	}
	bool reset = strcasecmp(argv[0], "reset") == 0;
	if (!reset && (!parse_number(argv[0], &f, &relative) || relative)) {
		return cmd_results_new(CMD_INVALID, "Expected single_width F or reset");
	}
	struct sway_workspace *ws = tide_workspace(&error);
	if (!ws) {
		return error;
	}
	if (!reset && (f < 0.1f || f > 1)) {
		return cmd_results_new(CMD_FAILURE, "single_width must be between 0.1 and 1");
	}
	ws->tide.single = reset ? 0 : f;
	arrange_workspace(ws);
	return cmd_results_new(CMD_SUCCESS, NULL);
}

struct cmd_results *cmd_zoom(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "zoom", EXPECTED_EQUAL_TO, 0))) {
		return error;
	}
	struct sway_container *con = config->handler_context.container;
	struct sway_container *master = con ? tide_zoom(con) : NULL;
	if (master) {
		seat_set_focus_container(config->handler_context.seat, master);
	}
	return cmd_results_new(CMD_SUCCESS, NULL);
}

struct cmd_results *cmd_movestack(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "movestack", EXPECTED_EQUAL_TO, 1))) {
		return error;
	}
	bool next = strcasecmp(argv[0], "next") == 0;
	if (!next && strcasecmp(argv[0], "prev") != 0) {
		return cmd_results_new(CMD_INVALID, "Expected movestack next|prev");
	}
	struct sway_container *con = config->handler_context.container;
	if (!con) {
		return cmd_results_new(CMD_FAILURE, "No window to move");
	}
	const char *err = tide_movestack(con, next ? 1 : -1);
	if (err) {
		return cmd_results_new(CMD_FAILURE, "%s", err);
	}
	return cmd_results_new(CMD_SUCCESS, NULL);
}

struct cmd_results *cmd_single_window(int argc, char **argv) {
	struct tide_single_rule r;
	if (argc != 2 || !parse_double(argv[0], &r.min_aspect)
			|| !parse_double(argv[1], &r.width)
			|| !(r.min_aspect >= 0 && r.min_aspect <= 100)
			|| !(r.width >= 0.1 && r.width <= 1)) {
		return cmd_results_new(CMD_INVALID, "single_window takes an aspect "
			"from 0 to 100 and a width from 0.1 to 1");
	}
	if (config->tide.nsingles >= TIDE_MAX_SINGLES) {
		return cmd_results_new(CMD_FAILURE, "single_window takes at most %d lines",
			TIDE_MAX_SINGLES);
	}
	config->tide.singles[config->tide.nsingles++] = r;
	return cmd_results_new(CMD_SUCCESS, NULL);
}

struct cmd_results *cmd_ultrawide_aspect(int argc, char **argv) {
	double v;
	if (argc != 1 || !parse_double(argv[0], &v) || !(v >= 0.1 && v <= 100)) {
		return cmd_results_new(CMD_INVALID,
			"ultrawide_aspect takes a number from 0.1 to 100");
	}
	config->tide.ultrawide_aspect = v;
	return cmd_results_new(CMD_SUCCESS, NULL);
}

struct cmd_results *cmd_default_layout(int argc, char **argv) {
	bool wide = argc == 2 && strcmp(argv[0], "ultrawide") == 0;
	enum tide_layout lt = argc == 2 ? parse_layout(argv[1]) : TIDE_NONE;
	if (argc != 2 || (!wide && strcmp(argv[0], "normal") != 0)
			|| lt == TIDE_NONE || strcmp(argv[1], tide_layout_name(lt)) != 0) {
		return cmd_results_new(CMD_INVALID, "default_layout takes normal or "
			"ultrawide, then tile, threecol, twocol or monocle");
	}
	config->tide.first_layout[wide] = lt;
	return cmd_results_new(CMD_SUCCESS, NULL);
}

struct cmd_results *cmd_layout_defaults(int argc, char **argv) {
	enum tide_layout lt = argc ? parse_mode(argv[0]) : TIDE_NONE;
	if (lt == TIDE_NONE || argc < 2) {
		return cmd_results_new(CMD_INVALID, "layout_defaults takes tile, "
			"threecol or twocol, then mfact, nmaster or both");
	}
	const char *name = mode_names[lt];
	int min_masters = lt == TIDE_TILE ? 0 : 1;
	struct tide_layout_defaults d = config->tide.layout_defaults[lt];
	for (int i = 1; i < argc; i += 2) {
		const char *arg = i + 1 < argc ? argv[i + 1] : "";
		char *end;
		if (strcmp(argv[i], "mfact") == 0) {
			double v;
			if (!parse_double(arg, &v) || !(v >= 0.1 && v <= 0.9)) {
				return cmd_results_new(CMD_INVALID, "layout_defaults %s: mfact "
					"takes a number from 0.1 to 0.9", name);
			}
			d.has_mfact = true;
			d.mfact = (float)v;
		} else if (strcmp(argv[i], "nmaster") == 0) {
			errno = 0;
			long n = strtol(arg, &end, 10);
			if (end == arg || *end || errno || n < min_masters || n > 100) {
				return cmd_results_new(CMD_INVALID, "layout_defaults %s: nmaster "
					"takes a whole number from %d to 100", name, min_masters);
			}
			d.has_nmaster = true;
			d.nmaster = (int)n;
		} else {
			return cmd_results_new(CMD_INVALID,
				"layout_defaults %s: no setting is called %s", name, argv[i]);
		}
	}
	config->tide.layout_defaults[lt] = d;
	return cmd_results_new(CMD_SUCCESS, NULL);
}

struct cmd_results *cmd_layout_cycle(int argc, char **argv) {
	enum tide_layout cycle[TIDE_MODES];
	int n = 0;
	if (!argc) {
		return cmd_results_new(CMD_INVALID, "layout_cycle needs a layout");
	}
	for (int i = 0; i < argc; i++) {
		enum tide_layout lt = parse_mode(argv[i]);
		if (lt == TIDE_NONE) {
			return cmd_results_new(CMD_INVALID, "layout_cycle takes tile, "
				"threecol and twocol, not %s", argv[i]);
		}
		for (int j = 0; j < n; j++) {
			if (cycle[j] == lt) {
				return cmd_results_new(CMD_INVALID,
					"layout_cycle has %s twice", argv[i]);
			}
		}
		cycle[n++] = lt;
	}
	memcpy(config->tide.cycle, cycle, sizeof(cycle));
	config->tide.ncycle = n;
	return cmd_results_new(CMD_SUCCESS, NULL);
}

struct cmd_results *tide_cmd_new_window(int argc, char **argv) {
	static const char *const rules[] = {
		[TIDE_NEW_END] = "end",
		[TIDE_NEW_TOP] = "top",
		[TIDE_NEW_NEXT] = "next",
		[TIDE_NEW_MASTER] = "master",
	};
	// sway's deprecated new_window, default_border's old name
	static const char *const borders[] = { "normal", "none", "pixel", "csd" };
	for (size_t i = 0; argc == 1 && i < sizeof(rules) / sizeof(rules[0]); i++) {
		if (strcmp(argv[0], rules[i]) == 0) {
			config->tide.new_window = (int)i;
			return cmd_results_new(CMD_SUCCESS, NULL);
		}
	}
	for (size_t i = 0; argc >= 1 && i < sizeof(borders) / sizeof(borders[0]); i++) {
		if (strcmp(argv[0], borders[i]) == 0) {
			return NULL;
		}
	}
	return cmd_results_new(CMD_INVALID, "new_window takes end, top, next or master");
}

struct cmd_results *cmd_resize_grab(int argc, char **argv) {
	char *end = NULL;
	long n = argc == 1 ? strtol(argv[0], &end, 10) : -1;
	if (argc != 1 || end == argv[0] || *end || n < 0 || n > 32) {
		return cmd_results_new(CMD_INVALID,
			"resize_grab takes a whole number of pixels from 0 to 32");
	}
	config->tide.resize_grab = (int)n;
	return cmd_results_new(CMD_SUCCESS, NULL);
}
