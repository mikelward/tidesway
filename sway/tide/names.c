/*
 * Named bindings, as tidewm's are (its docs/config.md, "Named bindings"):
 * `bindsym --name NAME` names a binding, and `rebind NAME KEYS...` gives
 * it other keys, so a shell's keyboard shortcuts can change a binding's
 * keys in a file the config includes. GET_BINDINGS lists the config's
 * bindings and GET_NAMED_BINDINGS the named ones, with their default keys.
 */
#include <ctype.h>
#include <json.h>
#include <stdlib.h>
#include <string.h>
#include "list.h"
#include "log.h"
#include "stringop.h"
#include "sway/commands.h"
#include "sway/config.h"
#include "sway/tide/names.h"

struct tide_named {
	char *name;
	char *mode;
	char *command;
	uint32_t flags;
	enum binding_input_type type;
	bool code; // bindcode's, not bindsym's
	list_t *flag_args; // its bindsym line's flags, for rebind's
	list_t *default_keys; // as its bindsym lines give them
};

// While rebind binds a name's keys, which aren't its default ones
static bool rebinding;

static void named_free(struct tide_named *n) {
	free(n->name);
	free(n->mode);
	free(n->command);
	list_free_items_and_destroy(n->flag_args);
	list_free_items_and_destroy(n->default_keys);
	free(n);
}

void tide_names_free(struct sway_config *config) {
	if (!config->tide_named) {
		return;
	}
	for (int i = 0; i < config->tide_named->length; i++) {
		named_free(config->tide_named->items[i]);
	}
	list_free(config->tide_named);
	config->tide_named = NULL;
}

static struct tide_named *find(const char *name) {
	for (int i = 0; config->tide_named && i < config->tide_named->length; i++) {
		struct tide_named *n = config->tide_named->items[i];
		if (strcmp(n->name, name) == 0) {
			return n;
		}
	}
	return NULL;
}

static bool valid(const char *name) {
	if (!islower((unsigned char)name[0])) {
		return false;
	}
	for (const char *c = name; *c; c++) {
		if (!islower((unsigned char)*c) && !isdigit((unsigned char)*c) && *c != '_') {
			return false;
		}
	}
	return true;
}

static bool is_mouse(enum binding_input_type type) {
	return type == BINDING_MOUSECODE || type == BINDING_MOUSESYM;
}

// The flags but --name and its name
static list_t *copy_flags(int argc, char **argv) {
	list_t *l = create_list();
	for (int i = 0; l && i < argc; i++) {
		if (strcmp(argv[i], "--name") == 0) {
			i++;
		} else if (strncmp(argv[i], "--name=", strlen("--name=")) != 0) {
			list_add(l, strdup(argv[i]));
		}
	}
	return l;
}

struct cmd_results *tide_names_bind(const char *bindtype, const char *name,
		struct sway_binding *binding, const char *keys, bool code,
		int flagc, char **flagv) {
	if (!valid(name)) {
		return cmd_results_new(CMD_FAILURE, "%s --name %s: a name is a "
			"lower-case letter, then lower-case letters, digits and _",
			bindtype, name);
	}
	const char *mode = config->current_mode->name;
	struct tide_named *n = find(name);
	if (n && strcmp(n->mode, mode) != 0) {
		return cmd_results_new(CMD_FAILURE,
			"%s --name %s: that name is in mode %s already", bindtype, name, n->mode);
	}
	if (n && strcmp(n->command, binding->command) != 0) {
		return cmd_results_new(CMD_FAILURE,
			"%s --name %s: that name runs %s already", bindtype, name, n->command);
	}
	if (n && (n->flags != binding->flags || n->code != code
			|| is_mouse(n->type) != is_mouse(binding->type))) {
		return cmd_results_new(CMD_FAILURE,
			"%s --name %s: that name's other keys have other flags", bindtype, name);
	}
	binding->tide_name = strdup(name);
	if (rebinding) {
		return NULL;
	}
	if (!n) {
		if (!config->tide_named) {
			config->tide_named = create_list();
		}
		n = calloc(1, sizeof(*n));
		if (!n || !config->tide_named) {
			free(n);
			return cmd_results_new(CMD_FAILURE, "Unable to allocate a named binding");
		}
		n->name = strdup(name);
		n->mode = strdup(mode);
		n->command = strdup(binding->command);
		n->flags = binding->flags;
		n->type = binding->type;
		n->code = code;
		n->flag_args = copy_flags(flagc, flagv);
		n->default_keys = create_list();
		list_add(config->tide_named, n);
	}
	list_add(n->default_keys, strdup(keys));
	return NULL;
}

static struct sway_mode *mode_named(const char *name) {
	for (int i = 0; i < config->modes->length; i++) {
		struct sway_mode *mode = config->modes->items[i];
		if (strcmp(mode->name, name) == 0) {
			return mode;
		}
	}
	return NULL;
}

// Takes name's keys away, in its mode's lists
static void unbind(struct sway_mode *mode, const char *name) {
	list_t *lists[] = {
		mode->keysym_bindings, mode->keycode_bindings, mode->mouse_bindings,
	};
	for (size_t l = 0; l < sizeof(lists) / sizeof(lists[0]); l++) {
		for (int i = lists[l]->length - 1; i >= 0; i--) {
			struct sway_binding *b = lists[l]->items[i];
			if (b->tide_name && strcmp(b->tide_name, name) == 0) {
				free_sway_binding(b);
				list_del(lists[l], i);
			}
		}
	}
}

struct cmd_results *cmd_rebind(int argc, char **argv) {
	if (argc < 2) {
		return cmd_results_new(CMD_FAILURE,
			"rebind needs a binding's name, then its keys, or none");
	}
	const char *name = argv[0];
	struct tide_named *n = find(name);
	if (!n) {
		// So a shell's file that outlives a name the config dropped loads
		sway_log(SWAY_ERROR, "rebind %s: no binding is called %s, so it's skipped",
			name, name);
		return cmd_results_new(CMD_SUCCESS, NULL);
	}
	bool none = false;
	for (int i = 1; i < argc; i++) {
		none |= strcmp(argv[i], "none") == 0;
	}
	if (none && argc != 2) {
		return cmd_results_new(CMD_FAILURE, "rebind %s: none is alone", name);
	}
	struct sway_mode *mode = mode_named(n->mode);
	if (!mode) {
		return cmd_results_new(CMD_FAILURE, "rebind %s: its mode %s is gone",
			name, n->mode);
	}
	unbind(mode, name);
	if (none) {
		return cmd_results_new(CMD_SUCCESS, NULL);
	}
	// Each key, as a bindsym line in its mode with its flags would bind it
	int flagc = n->flag_args->length;
	char **args = calloc(flagc + 5, sizeof(char *));
	if (!args) {
		return cmd_results_new(CMD_FAILURE, "Unable to allocate a rebind");
	}
	for (int i = 0; i < flagc; i++) {
		args[i] = n->flag_args->items[i];
	}
	args[flagc] = "--no-warn"; // taking a key from another binding is the point
	args[flagc + 1] = "--name";
	args[flagc + 2] = n->name;
	args[flagc + 4] = n->command;
	struct sway_mode *stored = config->current_mode;
	config->current_mode = mode;
	rebinding = true;
	struct cmd_results *res = NULL;
	for (int i = 1; i < argc; i++) {
		args[flagc + 3] = argv[i];
		res = n->code ? cmd_bindcode(flagc + 5, args) : cmd_bindsym(flagc + 5, args);
		if (res->status != CMD_SUCCESS) {
			// In rebind's words: bindsym's error after its keys
			const char *why = res->error ? strstr(res->error, ": ") : NULL;
			struct cmd_results *own = why
				? cmd_results_new(CMD_FAILURE, "rebind %s %s%s", name, argv[i], why)
				: cmd_results_new(CMD_FAILURE, "rebind %s %s: %s", name, argv[i],
					res->error ? res->error : "error");
			free_cmd_results(res);
			res = own;
			break;
		}
		free_cmd_results(res);
		res = NULL;
	}
	rebinding = false;
	config->current_mode = stored;
	free(args);
	return res ? res : cmd_results_new(CMD_SUCCESS, NULL);
}


static int by_order(const void *a, const void *b) {
	const struct sway_binding *x = *(void * const *)a, *y = *(void * const *)b;
	return x->order - y->order;
}

// A mode's bindings, in the order the file gives them; free the list only
static list_t *mode_bindings(struct sway_mode *mode) {
	list_t *all = create_list();
	list_cat(all, mode->keysym_bindings);
	list_cat(all, mode->keycode_bindings);
	list_cat(all, mode->mouse_bindings);
	list_qsort(all, by_order);
	return all;
}

static void add_flags(json_object *o, uint32_t flags) {
	json_object_object_add(o, "release",
		json_object_new_boolean(flags & BINDING_RELEASE));
	json_object_object_add(o, "locked",
		json_object_new_boolean(flags & BINDING_LOCKED));
	json_object_object_add(o, "kept_open",
		json_object_new_boolean(flags & BINDING_KEPT_OPEN));
}

static const char *switch_name(enum wlr_switch_type type) {
	switch (type) {
	case WLR_SWITCH_TYPE_LID:
		return "lid";
	case WLR_SWITCH_TYPE_TABLET_MODE:
		return "tablet";
	default:
		return "keypad_slide";
	}
}

static const char *trigger_name(enum sway_switch_trigger trigger) {
	switch (trigger) {
	case SWAY_SWITCH_TRIGGER_ON:
		return "on";
	case SWAY_SWITCH_TRIGGER_OFF:
		return "off";
	default:
		return "toggle";
	}
}

json_object *tide_names_describe_bindings(void) {
	json_object *array = json_object_new_array();
	for (int m = 0; m < config->modes->length; m++) {
		struct sway_mode *mode = config->modes->items[m];
		list_t *all = mode_bindings(mode);
		for (int i = 0; all && i < all->length; i++) {
			struct sway_binding *b = all->items[i];
			json_object *o = json_object_new_object();
			json_object_object_add(o, "type",
				json_object_new_string(is_mouse(b->type) ? "mouse" : "key"));
			json_object_object_add(o, "keys",
				json_object_new_string(b->tide_keys ? b->tide_keys : ""));
			json_object_object_add(o, "command", json_object_new_string(b->command));
			json_object_object_add(o, "mode", json_object_new_string(mode->name));
			add_flags(o, b->flags);
			json_object_object_add(o, "name",
				json_object_new_string(b->tide_name ? b->tide_name : ""));
			json_object_array_add(array, o);
		}
		list_free(all);
	}
	// bindswitch lines last, each in the top level, as tidewm has them
	for (int m = 0; m < config->modes->length; m++) {
		struct sway_mode *mode = config->modes->items[m];
		for (int i = 0; i < mode->switch_bindings->length; i++) {
			struct sway_switch_binding *b = mode->switch_bindings->items[i];
			char *keys = format_str("%s:%s", switch_name(b->type),
				trigger_name(b->trigger));
			json_object *o = json_object_new_object();
			json_object_object_add(o, "type", json_object_new_string("switch"));
			json_object_object_add(o, "keys", json_object_new_string(keys ? keys : ""));
			free(keys);
			json_object_object_add(o, "command", json_object_new_string(b->command));
			json_object_object_add(o, "mode", json_object_new_string("default"));
			add_flags(o, b->flags);
			json_object_object_add(o, "name", json_object_new_string(""));
			json_object_array_add(array, o);
		}
	}
	return array;
}

static json_object *strings(list_t *l) {
	json_object *array = json_object_new_array();
	for (int i = 0; l && i < l->length; i++) {
		json_object_array_add(array, json_object_new_string(l->items[i]));
	}
	return array;
}

json_object *tide_names_describe(void) {
	json_object *array = json_object_new_array();
	for (int i = 0; config->tide_named && i < config->tide_named->length; i++) {
		struct tide_named *n = config->tide_named->items[i];
		json_object *keys = json_object_new_array();
		struct sway_mode *mode = mode_named(n->mode);
		list_t *all = mode ? mode_bindings(mode) : NULL;
		for (int j = 0; all && j < all->length; j++) {
			struct sway_binding *b = all->items[j];
			if (b->tide_name && strcmp(b->tide_name, n->name) == 0) {
				json_object_array_add(keys,
					json_object_new_string(b->tide_keys ? b->tide_keys : ""));
			}
		}
		list_free(all);
		json_object *o = json_object_new_object();
		json_object_object_add(o, "name", json_object_new_string(n->name));
		json_object_object_add(o, "type",
			json_object_new_string(is_mouse(n->type) ? "mouse" : "key"));
		json_object_object_add(o, "keys", keys);
		json_object_object_add(o, "default_keys", strings(n->default_keys));
		json_object_object_add(o, "command", json_object_new_string(n->command));
		json_object_object_add(o, "mode", json_object_new_string(n->mode));
		add_flags(o, n->flags);
		json_object_array_add(array, o);
	}
	return array;
}
