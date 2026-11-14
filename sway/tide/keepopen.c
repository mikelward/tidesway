/*
 * Windows kept open, as tidewm has them.
 * include/sway/tide/keepopen.h says how they behave.
 */
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "sway/commands.h"
#include "sway/config.h"
#include "sway/input/seat.h"
#include "sway/ipc-server.h"
#include "sway/tide/keepopen.h"
#include "sway/tree/container.h"
#include "sway/tree/view.h"
#include "sway/tree/workspace.h"

bool tide_kept_open(struct sway_container *con) {
	if (!con || !con->view || !con->view->tide.keep_open_ws) {
		return false;
	}
	struct sway_workspace *ws = con->pending.workspace;
	return ws && strcmp(ws->name, con->view->tide.keep_open_ws) == 0;
}

bool tide_kept_open_focused(struct sway_seat *seat) {
	return tide_kept_open(seat_get_focused_container(seat));
}

void tide_keep_open_unmapped(struct sway_view *view) {
	free(view->tide.keep_open_ws);
	view->tide.keep_open_ws = NULL;
}

void tide_keep_open_describe(struct sway_container *con, json_object *object) {
	if (con->view) {
		json_object_object_add(object, "keep_open",
			json_object_new_boolean(tide_kept_open(con)));
	}
}

struct cmd_results *cmd_keep_open(int argc, char **argv) {
	struct sway_container *con = config->handler_context.container;
	bool now = tide_kept_open(con), want;
	if (argc == 1 && strcasecmp(argv[0], "toggle") == 0) {
		want = !now;
	} else if (argc == 1 && strcasecmp(argv[0], "enable") == 0) {
		want = true;
	} else if (argc == 1 && strcasecmp(argv[0], "disable") == 0) {
		want = false;
	} else {
		return cmd_results_new(CMD_INVALID, "Expected keep_open enable|disable|toggle");
	}
	if (!con || !con->view) {
		return cmd_results_new(CMD_FAILURE, "No window to keep open");
	}
	struct sway_view *view = con->view;
	free(view->tide.keep_open_ws);
	view->tide.keep_open_ws = NULL;
	if (want && con->pending.workspace) {
		view->tide.keep_open_ws = strdup(con->pending.workspace->name);
		if (!view->tide.keep_open_ws) {
			return cmd_results_new(CMD_FAILURE, "No memory to keep the window open");
		}
	}
	if (tide_kept_open(con) != now) {
		ipc_event_window(con, "keep_open");
	}
	return cmd_results_new(CMD_SUCCESS, NULL);
}
