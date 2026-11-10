#include "log.h"
#include "sway/input/input-manager.h"
#include "sway/input/seat.h"
#include "sway/ipc-server.h"
#include "sway/tide/keepopen.h"
#include "sway/tree/container.h"
#include "sway/tree/view.h"
#include "sway/tree/workspace.h"
#include "sway/commands.h"

static void close_container_iterator(struct sway_container *con, void *data) {
	if (con->view && !tide_kept_open(con)) { // tide: not one kept open
		view_close(con->view);
	}
}

struct cmd_results *cmd_kill(int argc, char **argv) {
	if (!root->outputs->length) {
		return cmd_results_new(CMD_INVALID,
				"Can't run this command while there's no outputs connected.");
	}
	struct sway_container *con = config->handler_context.container;
	struct sway_workspace *ws = config->handler_context.workspace;

	// tide: a window kept open stays, and says so
	if (con && tide_kept_open(con)) {
		ipc_event_window(con, "close_refused");
		return cmd_results_new(CMD_FAILURE, "The window is kept open");
	}
	if (con) {
		close_container_iterator(con, NULL);
		container_for_each_child(con, close_container_iterator, NULL);
	} else {
		workspace_for_each_container(ws, close_container_iterator, NULL);
	}

	return cmd_results_new(CMD_SUCCESS, NULL);
}
