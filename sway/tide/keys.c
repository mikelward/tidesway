/*
 * tidewm's built-in keys, its config.h's as config.def.h has them, which
 * work while tidewm's file is missing or binds no key at its top level, so
 * there are always keys to fix it with, as tidewm has it. Only under the
 * tidesway command, whose config names tidewm's file: sway's own config
 * binds the keys it binds. They're off on the lock screen, aren't in
 * GET_BINDINGS, and have only the top level, as tidewm's are.
 */
#include <stdlib.h>
#include <string.h>
#include "list.h"
#include "log.h"
#include "stringop.h"
#include "sway/commands.h"
#include "sway/config.h"
#include "sway/tide/keys.h"

// Each key and its command, as tidewm's config.def.h's keys[] does it.
// Ctrl+Alt+F1 to F12 change VT in sway already; tidesway has no floating
// layout, which tidewm's Alt+f sets.
static const char *const builtin[][2] = {
	{ "Mod1+p", "exec wmenu-run" },
	{ "Mod1+Shift+Return", "exec foot" },
	{ "Mod1+j", "focus next" },
	{ "Mod1+k", "focus prev" },
	{ "Mod1+i", "nmaster +1" },
	{ "Mod1+d", "nmaster -1" },
	{ "Mod1+h", "mfact -0.05" },
	{ "Mod1+l", "mfact +0.05" },
	{ "Mod1+Return", "zoom" },
	{ "Mod1+Tab", "workspace back_and_forth" },
	{ "Mod1+Shift+c", "kill" },
	{ "Mod1+t", "layout tile" },
	{ "Mod1+m", "layout monocle" },
	{ "Mod1+space", "layout toggle" },
	{ "Mod1+Shift+space", "floating toggle" },
	{ "Mod1+e", "fullscreen toggle" },
	{ "Mod1+comma", "focus output left" },
	{ "Mod1+period", "focus output right" },
	{ "Mod1+Shift+comma", "move container to output left" },
	{ "Mod1+Shift+period", "move container to output right" },
	{ "Mod1+Shift+q", "exit" },
	{ "Ctrl+Mod1+BackSpace", "exit" },
};

static void bind(const char *keys, const char *command) {
	char *line = format_str("%s %s", keys, command);
	if (!line) {
		sway_log(SWAY_ERROR, "tide: can't allocate built-in key %s", keys);
		return;
	}
	int argc;
	char **argv = split_args(line, &argc);
	free(line);
	struct cmd_results *res = cmd_bindsym(argc, argv);
	if (res->status != CMD_SUCCESS) {
		sway_log(SWAY_ERROR, "tide: built-in key %s: %s", keys,
			res->error ? res->error : "failed");
	}
	free_cmd_results(res);
	free_argv(argc, argv);
}

struct sway_mode *tide_keys_builtin(void) {
	struct sway_mode *mode = calloc(1, sizeof(*mode));
	if (!mode) {
		sway_log(SWAY_ERROR, "tide: can't allocate the built-in keys");
		return NULL;
	}
	mode->name = strdup("default");
	mode->keysym_bindings = create_list();
	mode->keycode_bindings = create_list();
	mode->mouse_bindings = create_list();
	mode->switch_bindings = create_list();
	mode->gesture_bindings = create_list();
	struct sway_mode *current = config->current_mode;
	config->current_mode = mode;
	for (size_t i = 0; i < sizeof(builtin) / sizeof(builtin[0]); i++) {
		bind(builtin[i][0], builtin[i][1]);
	}
	// Alt and a number show that workspace, and with Shift move the window
	// there, as tidewm's WSKEYS
	for (int n = 1; n <= 9; n++) {
		char keys[32], command[64];
		snprintf(keys, sizeof(keys), "Mod1+%d", n);
		snprintf(command, sizeof(command), "workspace %d", n);
		bind(keys, command);
		snprintf(keys, sizeof(keys), "Mod1+Shift+%d", n);
		snprintf(command, sizeof(command), "move container to workspace %d", n);
		bind(keys, command);
	}
	config->current_mode = current;
	return mode;
}

bool tide_keys_bound(struct sway_config *config) {
	struct sway_mode *top = config->modes->items[0];
	return top->keysym_bindings->length || top->keycode_bindings->length;
}

struct sway_mode *tide_keys_mode(void) {
	struct sway_mode *mode = config->current_mode;
	if (config->tide_builtin_keys && mode == config->modes->items[0]
			&& !tide_keys_bound(config)) {
		return config->tide_builtin_keys;
	}
	return mode;
}
