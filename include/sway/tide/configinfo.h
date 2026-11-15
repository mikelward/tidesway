#ifndef _SWAY_TIDE_CONFIGINFO_H
#define _SWAY_TIDE_CONFIGINFO_H
#include <json.h>

#include <stdbool.h>

struct sway_config;
struct cmd_results;

/* tidewm's config file, which tidesway's command names with a
 * tidewm_config line in the config it writes for sway: what GET_CONFIG and
 * GET_VERSION show, as tidewm's do, rather than that config */
struct tide_user_config {
	char *path; // as tidewm's -c or its place names it; NULL without the line
	char *real; // its real path, or NULL if it isn't there
	char *text; // its text, as loaded; NULL if it isn't there
	bool named; // named by -c, so its being missing is an error
	int depth; // while it loads: above 0 in it or a file it includes
};

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

// Notes an error of path's as a whole, as "PATH: ERROR"
void tide_config_file_error(const char *path, const char *error);

// Whether the last load had any errors
bool tide_config_has_errors(void);

/* tidewm_config [--named] FILE: includes tidewm's config file, the one
 * tidesway's command was given, as the user's own */
struct cmd_results *cmd_tidewm_config(int argc, char **argv);
void tide_user_config_free(struct sway_config *config);

/* The loaded file's path, or "" when it wasn't there, for GET_VERSION's
 * loaded_config_file_name, as tidewm's */
const char *tide_config_loaded_name(struct sway_config *config);

/* Whether new_block, a block opening in block (NULL at the top level), can
 * open, as tidewm has it: a mode block's name, and no block in a mode. If
 * not, err says why, in tidewm's words. */
bool tide_config_block_ok(const char *block, const char *new_block, char *err,
	size_t size);

// Takes a comment from the end of a line that closes a block: "} # mode"
void tide_config_block_end(char *line);

// Says at startup what of the config was left out, as tidewm does
void tide_config_started(struct sway_config *config);

/* The first few errors the last load met, joined by "; ", then how many
 * more, or NULL with none; the caller frees it */
char *tide_config_errors_summary(void);

// Keeps an included file's text for GET_CONFIG, once it's loaded
void tide_config_included(struct sway_config *config, const char *path);
void tide_config_included_free(struct sway_config *config);

// Adds GET_CONFIG's included_configs, path and errors to json
void tide_config_describe(struct sway_config *config, json_object *json);

#endif
