/*
 * What the IPC says about loading the config, as tidewm's settings.h has
 * it: a load's errors, each "FILE:LINE: ERROR", a failed reload's reply
 * naming the first few, and GET_CONFIG's path, errors and included files.
 */
#define _XOPEN_SOURCE 700 // realpath
#include <json.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "list.h"
#include "log.h"
#include "stringop.h"
#include "sway/commands.h"
#include "sway/config.h"
#include "sway/tide/configinfo.h"

// A reload's reply names this many, then says how many more there are
#define SHOWN 3

// The last load's, kept past a failed reload, whose config is freed
static list_t *errors;

void tide_config_errors_reset(void) {
	if (errors) {
		list_free_items_and_destroy(errors);
	}
	errors = create_list();
}

void tide_config_error(const char *path, int line, const char *error) {
	if (error && strcmp(error, TIDE_INCLUDE_FAILED) == 0) {
		return;
	}
	if (!errors) {
		tide_config_errors_reset();
	}
	char *s = format_str("%s:%i: %s", path ? path : "", line,
		error ? error : "error");
	if (!s) {
		sway_log(SWAY_ERROR, "tide: can't allocate a config error");
		return;
	}
	list_add(errors, s);
}

void tide_config_file_error(const char *path, const char *error) {
	if (!errors) {
		tide_config_errors_reset();
	}
	char *s = format_str("%s: %s", path, error);
	if (!s) {
		sway_log(SWAY_ERROR, "tide: can't allocate a config error");
		return;
	}
	list_add(errors, s);
}

bool tide_config_has_errors(void) {
	return errors && errors->length;
}

char *tide_config_errors_summary(void) {
	if (!errors || !errors->length) {
		return NULL;
	}
	size_t size = 1;
	for (int i = 0; i < errors->length && i < SHOWN; i++) {
		size += strlen(errors->items[i]) + 2;
	}
	size += 32;
	char *s = malloc(size);
	if (!s) {
		sway_log(SWAY_ERROR, "tide: can't allocate a reload's errors");
		return NULL;
	}
	size_t len = 0;
	s[0] = '\0';
	for (int i = 0; i < errors->length && i < SHOWN; i++) {
		len += snprintf(s + len, size - len, "%s%s", i ? "; " : "",
			(char *)errors->items[i]);
	}
	if (errors->length > SHOWN) {
		snprintf(s + len, size - len, "; and %d more", errors->length - SHOWN);
	}
	return s;
}

static char *slurp(const char *path) {
	FILE *f = fopen(path, "r");
	if (!f) {
		sway_log_errno(SWAY_ERROR, "tide: can't read %s", path);
		return NULL;
	}
	char *s = NULL;
	size_t len = 0;
	FILE *out = open_memstream(&s, &len);
	if (!out) {
		sway_log_errno(SWAY_ERROR, "tide: can't read %s", path);
		fclose(f);
		return NULL;
	}
	char buf[4096];
	size_t n;
	while ((n = fread(buf, 1, sizeof(buf), f)) > 0) {
		fwrite(buf, 1, n, out);
	}
	bool failed = ferror(f);
	fclose(f);
	fclose(out);
	if (failed) {
		sway_log(SWAY_ERROR, "tide: can't read %s", path);
		free(s);
		return NULL;
	}
	return s;
}

void tide_config_included(struct sway_config *config, const char *path) {
	// Under tidesway's command, only what tidewm's file includes, as tidewm
	// lists them, not the file itself or what the command's config adds
	struct tide_user_config *user = &config->tide_user;
	if (user->path && (user->depth == 0 || (user->real && strcmp(path, user->real) == 0))) {
		return;
	}
	if (!config->tide_included && !(config->tide_included = create_list())) {
		sway_log(SWAY_ERROR, "tide: can't allocate the included files");
		return;
	}
	struct tide_included_config *inc = calloc(1, sizeof(*inc));
	if (!inc) {
		sway_log(SWAY_ERROR, "tide: can't allocate an included file");
		return;
	}
	inc->path = strdup(path);
	inc->raw = slurp(path);
	list_add(config->tide_included, inc);
}

void tide_config_included_free(struct sway_config *config) {
	if (!config->tide_included) {
		return;
	}
	for (int i = 0; i < config->tide_included->length; i++) {
		struct tide_included_config *inc = config->tide_included->items[i];
		free(inc->path);
		free(inc->raw);
		free(inc);
	}
	list_free(config->tide_included);
	config->tide_included = NULL;
}

void tide_config_describe(struct sway_config *config, json_object *json) {
	json_object *included = json_object_new_array();
	for (int i = 0; config->tide_included && i < config->tide_included->length; i++) {
		struct tide_included_config *inc = config->tide_included->items[i];
		json_object *o = json_object_new_object();
		json_object_object_add(o, "path", json_object_new_string(inc->path));
		json_object_object_add(o, "raw_config",
			json_object_new_string(inc->raw ? inc->raw : ""));
		json_object_array_add(included, o);
	}
	json_object_object_add(json, "included_configs", included);
	const char *path = config->config_chain && config->config_chain->length
		? config->config_chain->items[0] : NULL;
	if (config->tide_user.path) {
		// tidewm's file, as tidewm shows it: "" when it isn't there
		path = config->tide_user.path;
		json_object_object_add(json, "config",
			json_object_new_string(config->tide_user.text ? config->tide_user.text : ""));
	}
	json_object_object_add(json, "path", path ? json_object_new_string(path) : NULL);
	json_object *list = json_object_new_array();
	for (int i = 0; errors && i < errors->length; i++) {
		json_object_array_add(list, json_object_new_string(errors->items[i]));
	}
	json_object_object_add(json, "errors", list);
}

struct cmd_results *cmd_tidewm_config(int argc, char **argv) {
	struct tide_user_config *user = &config->tide_user;
	// Only the line tidesway's command writes, once
	if (user->path || user->depth) {
		return cmd_results_new(CMD_INVALID, "no setting is called tidewm_config");
	}
	bool named = argc > 0 && strcmp(argv[0], "--named") == 0;
	if (argc != (named ? 2 : 1)) {
		return cmd_results_new(CMD_INVALID, "tidewm_config needs [--named] FILE");
	}
	const char *path = argv[named ? 1 : 0];
	user->path = strdup(path);
	if (!user->path) {
		return cmd_results_new(CMD_FAILURE, "can't allocate the config's path");
	}
	user->named = named;
	user->real = realpath(path, NULL);
	if (!user->real) {
		// One -c names that isn't there is an error at startup, as in
		// tidewm; one at tidewm's place isn't, nor is either on a reload,
		// which goes back to config.h's keys
		if (named && !config->reloading) {
			tide_config_file_error(path, "there's no such file");
			sway_log(SWAY_ERROR, "tidesway: %s: there's no such file", path);
		}
		return cmd_results_new(CMD_SUCCESS, NULL);
	}
	user->depth = 1;
	bool loaded = load_include_configs(path, config, &config->swaynag_config_errors);
	user->depth = 0;
	// As loaded, errors and all: tidewm leaves out only the bad lines
	user->text = slurp(user->real);
	return loaded ? cmd_results_new(CMD_SUCCESS, NULL)
		: cmd_results_new(CMD_FAILURE, "%s", TIDE_INCLUDE_FAILED);
}

void tide_user_config_free(struct sway_config *config) {
	free(config->tide_user.path);
	free(config->tide_user.real);
	free(config->tide_user.text);
	config->tide_user = (struct tide_user_config){0};
}

const char *tide_config_loaded_name(struct sway_config *config) {
	if (config->tide_user.path) {
		return config->tide_user.text ? config->tide_user.path : "";
	}
	return config->current_config_path ? config->current_config_path : "";
}

void tide_config_started(struct sway_config *config) {
	struct tide_user_config *user = &config->tide_user;
	if (user->path && !user->text && user->named) {
		sway_log(SWAY_ERROR, "tidesway: not using %s: config.h's keys work instead",
			user->path);
	} else if (tide_config_has_errors()) {
		sway_log(SWAY_ERROR, "tidesway: left out the lines above from the config; "
			"the rest is loaded");
	}
}
