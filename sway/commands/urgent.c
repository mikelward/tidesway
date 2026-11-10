#include "log.h"
#include "sway/commands.h"
#include "sway/config.h"
#include "sway/tree/arrange.h"
#include "sway/tree/container.h"
#include "sway/tree/view.h"
#include "util.h"

struct cmd_results *cmd_urgent(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "urgent", EXPECTED_EQUAL_TO, 1))) {
		return error;
	}
	// tide: tidewm's urgent: enable or disable, for a shell's own marks;
	// every window may be marked
	if (strcmp(argv[0], "allow") == 0 || strcmp(argv[0], "deny") == 0) {
		return cmd_results_new(CMD_FAILURE, "allow and deny aren't supported");
	}
	bool enable = strcmp(argv[0], "enable") == 0;
	if (!enable && strcmp(argv[0], "disable") != 0) {
		return cmd_results_new(CMD_INVALID, "Expected urgent enable|disable");
	}
	struct sway_container *container = config->handler_context.container;
	if (!container || !container->view) {
		return cmd_results_new(CMD_FAILURE, "No window to mark");
	}
	view_set_urgent(container->view, enable);

	return cmd_results_new(CMD_SUCCESS, NULL);
}
