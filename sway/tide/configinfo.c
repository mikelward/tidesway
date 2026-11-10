/*
 * What the IPC says about loading the config, as tidewm's settings.h has
 * it: a load's errors, each "FILE:LINE: ERROR", a failed reload's reply
 * naming the first few, and GET_CONFIG's path, errors and included files.
 */
#include <json.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "list.h"
#include "log.h"
#include "stringop.h"
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
	json_object_object_add(json, "path", path ? json_object_new_string(path) : NULL);
	json_object *list = json_object_new_array();
	for (int i = 0; errors && i < errors->length; i++) {
		json_object_array_add(list, json_object_new_string(errors->items[i]));
	}
	json_object_object_add(json, "errors", list);
}
