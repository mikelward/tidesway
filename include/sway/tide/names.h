#ifndef _SWAY_TIDE_NAMES_H
#define _SWAY_TIDE_NAMES_H
#include <json.h>
#include <stdbool.h>

struct cmd_results;
struct sway_binding;
struct sway_config;

/* Names binding, which a bindsym or bindcode line with --name NAME made in
 * the mode being read, its keys as written and its flags as flagv has
 * them: an error if the name's other lines differ, else NULL */
struct cmd_results *tide_names_bind(const char *bindtype, const char *name,
	struct sway_binding *binding, const char *keys, bool code,
	int flagc, char **flagv);

// The rebind config line, and its error in a mode block
struct cmd_results *cmd_rebind(int argc, char **argv);
struct cmd_results *tide_rebind_in_mode(int argc, char **argv);
// An include line's error in a mode block, as tidewm's
struct cmd_results *tide_include_in_mode(int argc, char **argv);

void tide_names_free(struct sway_config *config);

// GET_BINDINGS' reply and GET_NAMED_BINDINGS'
json_object *tide_names_describe_bindings(void);
json_object *tide_names_describe(void);

#endif
