#include "sway/commands.h"
#include "sway/config.h"
#include "sway/tide/configinfo.h"

struct cmd_results *cmd_include(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "include", EXPECTED_AT_LEAST, 1))) {
		return error;
	}
	char *files = join_args(argv, argc);
	// tide: an included file's errors are the config's, so reload fails
	// on them and keeps the config it had, as tidewm's does
	bool loaded = load_include_configs(files, config, &config->swaynag_config_errors);
	free(files);

	return loaded ? cmd_results_new(CMD_SUCCESS, NULL)
		: cmd_results_new(CMD_FAILURE, "%s", TIDE_INCLUDE_FAILED);
}
