/*
 * Minimized windows, as tidewm has them.
 * include/sway/tide/minimize.h says how they behave.
 */
#include <stdlib.h>
#include <strings.h>
#include "list.h"
#include "log.h"
#include "sway/commands.h"
#include "sway/config.h"
#include "sway/desktop/transaction.h"
#include "sway/input/input-manager.h"
#include "sway/input/seat.h"
#include "sway/ipc-server.h"
#include "sway/server.h"
#include "sway/tide/minimize.h"
#include "sway/tree/arrange.h"
#include "sway/tree/container.h"
#include "sway/tree/node.h"
#include "sway/tree/view.h"
#include "sway/tree/workspace.h"

bool tide_minimized(struct sway_container *con) {
	return con && con->view && con->view->tide.minimized;
}

/* The window focus goes to as con leaves: the most recent one left on
 * its workspace, else the workspace */
static void focus_after(struct sway_seat *seat, struct sway_container *con) {
	struct sway_workspace *ws = con->pending.workspace;
	/* It loses the keyboard first: forgotten, it would leave its workspace
	 * at the head of the focus stack, which focusing would take as focused
	 * already, so it would still hear keys */
	seat_set_focus(seat, NULL);
	seat_tide_forget(seat, &con->node);
	if (!ws) {
		return;
	}
	struct sway_container *next = seat_get_focus_inactive_view(seat, &ws->node);
	seat_set_focus(seat, next && !tide_minimized(next) ? &next->node : &ws->node);
}

static void minimize(struct sway_container *con) {
	struct sway_view *view = con->view;
	view->tide.minimized_fullscreen = con->pending.fullscreen_mode;
	if (con->pending.fullscreen_mode != FULLSCREEN_NONE) {
		// It covers its output no more, but is still fullscreen to come back
		container_set_fullscreen(con, FULLSCREEN_NONE);
	}
	view->tide.minimized = true;
	// Every seat that has it focused moves on, and every seat forgets it
	struct sway_seat *seat;
	wl_list_for_each(seat, &server.input->seats, link) {
		if (seat_get_focus(seat) == &con->node) {
			focus_after(seat, con);
		} else {
			seat_tide_forget(seat, &con->node);
		}
	}
	if (con->pending.workspace) {
		arrange_workspace(con->pending.workspace);
	}
	ipc_event_window(con, "minimize");
}

/* Brings con back, at the end of its part's stack, as an arriving window
 * comes */
static void restore(struct sway_container *con) {
	struct sway_view *view = con->view;
	view->tide.minimized = false;
	struct sway_workspace *ws = con->pending.workspace;
	if (ws && !container_is_floating(con)) {
		struct sway_container *parent = con->pending.parent;
		container_detach(con);
		workspace_add_tiling(ws, con);
		if (parent) {
			container_reap_empty(parent); // a split it leaves empty goes
		}
	}
	if (view->tide.minimized_fullscreen != FULLSCREEN_NONE) {
		container_set_fullscreen(con, view->tide.minimized_fullscreen);
		view->tide.minimized_fullscreen = FULLSCREEN_NONE;
	}
	if (ws) {
		arrange_workspace(ws);
	}
	ipc_event_window(con, "minimize");
}

void tide_minimize_unmapped(struct sway_view *view) {
	view->tide.minimized = false;
	view->tide.minimized_fullscreen = FULLSCREEN_NONE;
}

void tide_minimize_focusing(struct sway_node *node) {
	if (node && node->type == N_CONTAINER && tide_minimized(node->sway_container)) {
		restore(node->sway_container);
	}
}

/* Whether con takes no room: a minimized window, or a split with nothing
 * but minimized windows under it, which keeps its tree to come back to */
static bool hidden(struct sway_container *con) {
	if (con->view) {
		return tide_minimized(con);
	}
	list_t *children = con->pending.children;
	if (!children || !children->length) {
		return false;
	}
	for (int i = 0; i < children->length; i++) {
		if (!hidden(children->items[i])) {
			return false;
		}
	}
	return true;
}

list_t *tide_minimize_shown(list_t *children) {
	bool any = false;
	for (int i = 0; i < children->length && !any; i++) {
		any = hidden(children->items[i]);
	}
	if (!any) {
		return children;
	}
	list_t *shown = create_list();
	if (!shown) {
		sway_log(SWAY_ERROR, "tide: no memory to lay out without minimized windows");
		return children;
	}
	for (int i = 0; i < children->length; i++) {
		if (!hidden(children->items[i])) {
			list_add(shown, children->items[i]);
		}
	}
	return shown;
}

void tide_minimize_describe(struct sway_container *con, json_object *object) {
	if (!con->view) {
		return;
	}
	bool minimized = con->view->tide.minimized;
	json_object_object_add(object, "minimized", json_object_new_boolean(minimized));
	if (minimized && con->view->tide.minimized_fullscreen != FULLSCREEN_NONE) {
		json_object_object_add(object, "fullscreen_mode",
			json_object_new_int(con->view->tide.minimized_fullscreen));
	}
}

struct cmd_results *cmd_minimize(int argc, char **argv) {
	struct sway_container *con = config->handler_context.container;
	bool now = tide_minimized(con), want;
	if (argc == 0 || (argc == 1 && strcasecmp(argv[0], "toggle") == 0)) {
		want = !now;
	} else if (argc == 1 && strcasecmp(argv[0], "enable") == 0) {
		want = true;
	} else if (argc == 1 && strcasecmp(argv[0], "disable") == 0) {
		want = false;
	} else {
		return cmd_results_new(CMD_INVALID, "Expected minimize [enable|disable|toggle]");
	}
	if (!con || !con->view) {
		return cmd_results_new(CMD_FAILURE, "No window to minimize");
	}
	if (want != now) {
		if (want) {
			minimize(con);
		} else {
			restore(con);
		}
	}
	return cmd_results_new(CMD_SUCCESS, NULL);
}
