#ifndef _SWAY_TIDE_CONFIGINFO_H
#define _SWAY_TIDE_CONFIGINFO_H
#include <json.h>

struct sway_config;

// A file an include line read, as it was then
struct tide_included_config {
	char *path;
	char *raw; // NULL if it couldn't be read again
};

/* An include line's error when its file had errors, which those name, so
 * it isn't one of its own */
#define TIDE_INCLUDE_FAILED "An included file has errors"

// Forgets the last load's errors, as a load starts
void tide_config_errors_reset(void);

// Notes an error at path's line, for a reload's reply and GET_CONFIG
void tide_config_error(const char *path, int line, const char *error);

/* The first few errors the last load met, joined by "; ", then how many
 * more, or NULL with none; the caller frees it */
char *tide_config_errors_summary(void);

// Keeps an included file's text for GET_CONFIG, once it's loaded
void tide_config_included(struct sway_config *config, const char *path);
void tide_config_included_free(struct sway_config *config);

// Adds GET_CONFIG's included_configs, path and errors to json
void tide_config_describe(struct sway_config *config, json_object *json);

#endif
