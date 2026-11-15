/*
 * tidewm's built-in keys, its config.h's as config.def.h has them, which
 * work while tidewm's file is missing or binds no key at its top level, so
 * there are always keys to fix it with, as tidewm has it. Only under the
 * tidesway command, whose config names tidewm's file: sway's own config
 * binds the keys it binds. They're off on the lock screen, aren't in
 * GET_BINDINGS, and have only the top level, as tidewm's are. So are its
 * rules, while there's no file: a file's for_window lines replace them.
 */
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "list.h"
#include "log.h"
#include "stringop.h"
#include "sway/commands.h"
#include "sway/config.h"
#include "sway/input/keyboard.h"
#include "sway/input/seat.h"
#include "sway/tide/configinfo.h"
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

/* What's always there to fix a config with and to leave (tide's SPEC §16:
 * quit, a terminal and the launcher always keep a key): config.h's keys
 * for them, the terminal xdg-terminal-exec's where there is one. They work
 * alongside a config that binds keys, under the keys it binds, while it
 * has an error or binds no key that quits, as a config with a line or a
 * file left out may have lost the keys it meant to give */
static const char *const safety[][2] = {
	{ "Mod1+p", "exec wmenu-run" },
	{ "Mod1+Shift+Return", "exec xdg-terminal-exec || exec foot" },
	{ "Mod1+Shift+q", "exit" },
	{ "Ctrl+Mod1+BackSpace", "exit" },
};

struct sway_mode *tide_keys_safety(void) {
	struct sway_mode *mode = calloc(1, sizeof(*mode));
	if (!mode) {
		sway_log(SWAY_ERROR, "tide: can't allocate the keys to fix a config with");
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
	for (size_t i = 0; i < sizeof(safety) / sizeof(safety[0]); i++) {
		bind(safety[i][0], safety[i][1]);
	}
	config->current_mode = current;
	return mode;
}

// Whether a command quits: a command in its list is exit, in any case, with
// no criteria. The list splits at ; and , outside quotes, as sway splits
// it, and exec takes the rest, so an exit in its shell is the shell's
static bool quits(const char *command) {
	const char *p = command;
	bool criteria = false;
	while (*p) {
		p += strspn(p, " \t\n");
		if (*p == '[') {
			const char *close = strchr(p, ']');
			if (!close) {
				return false;
			}
			criteria = true;
			p = close + 1;
			continue;
		}
		size_t n = strcspn(p, " \t\n;,");
		if (n == 4 && strncasecmp(p, "exec", 4) == 0) {
			return false;
		}
		const char *end = p;
		char quote = 0;
		for (; *end && (quote || (*end != ';' && *end != ',')); end++) {
			if (quote && *end == quote) {
				quote = 0;
			} else if (!quote && (*end == '"' || *end == '\'')) {
				quote = *end;
			}
		}
		size_t len = end - p;
		while (len > 0 && (p[len - 1] == ' ' || p[len - 1] == '\t')) {
			len--;
		}
		if (!criteria && len == 4 && strncasecmp(p, "exit", 4) == 0) {
			return true;
		}
		if (*end == ';') {
			criteria = false;
		}
		p = *end ? end + 1 : end;
	}
	return false;
}

// Whether a binding of the config's top level quits, wherever the
// keyboard is and whichever keyboard it is: a --kept-open one quits only in
// a window kept open, an --input-device one only on that keyboard, and one
// of a layout only in it
static bool binds_exit(struct sway_config *config) {
	struct sway_mode *top = config->modes->items[0];
	list_t *lists[] = { top->keysym_bindings, top->keycode_bindings };
	for (size_t l = 0; l < 2; l++) {
		for (int i = 0; i < lists[l]->length; i++) {
			struct sway_binding *binding = lists[l]->items[i];
			if (binding->command && !(binding->flags & BINDING_KEPT_OPEN)
					&& (!binding->input || strcmp(binding->input, "*") == 0)
					&& binding->group == XKB_LAYOUT_INVALID
					&& quits(binding->command)) {
				return true;
			}
		}
	}
	return false;
}

struct sway_mode *tide_keys_safety_mode(void) {
	if (!config->tide_safety_keys || config->current_mode != config->modes->items[0]
			|| !tide_keys_bound(config)) {
		// Without the config's keys, all of config.h's work instead
		return NULL;
	}
	return tide_config_has_errors() || !binds_exit(config) ? config->tide_safety_keys : NULL;
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

void tide_keys_pointer_used(struct sway_seat *seat) {
	struct sway_seat_device *device;
	wl_list_for_each(device, &seat->devices, link) {
		if (device->keyboard) {
			device->keyboard->held_binding = NULL;
		}
	}
	struct sway_keyboard_group *group;
	wl_list_for_each(group, &seat->keyboard_groups, link) {
		if (group->seat_device && group->seat_device->keyboard) {
			group->seat_device->keyboard->held_binding = NULL;
		}
	}
}

// tidewm's config.def.h's rules, as sway's lines: an app ID containing
// either name, as a criteria's pattern matches
static const char *const builtin_rules[] = {
	"for_window [app_id=\"Gimp_EXAMPLE\"] floating enable",
	"assign [app_id=\"firefox_EXAMPLE\"] workspace 9",
};

void tide_keys_builtin_rules(void) {
	for (size_t i = 0; i < sizeof(builtin_rules) / sizeof(builtin_rules[0]); i++) {
		char *line = strdup(builtin_rules[i]);
		if (!line) {
			sway_log(SWAY_ERROR, "tide: can't allocate a built-in rule");
			return;
		}
		char *new_block = NULL;
		struct cmd_results *res = config_command(line, &new_block);
		if (res && res->status != CMD_SUCCESS) {
			sway_log(SWAY_ERROR, "tide: built-in rule %s: %s", line,
				res->error ? res->error : "failed");
		}
		if (res) {
			free_cmd_results(res);
		}
		free(new_block);
		free(line);
	}
}
