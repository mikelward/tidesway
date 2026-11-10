/*
 * Maximized windows, as tidewm has them.
 * include/sway/tide/maximize.h says how they behave.
 */
#include <strings.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/util/box.h>
#include "config.h"
#include "log.h"
#include "sway/commands.h"
#include "sway/config.h"
#include "sway/ipc-server.h"
#include "sway/tide/maximize.h"
#include "sway/tree/arrange.h"
#include "sway/tree/container.h"
#include "sway/tree/node.h"
#include "sway/tree/view.h"
#include "sway/tree/workspace.h"
#if WLR_HAS_XWAYLAND
#include <wlr/xwayland.h>
#endif

bool tide_maximized(struct sway_container *con) {
	return con && con->view && con->view->tide.maximized;
}

// Tells the app whether it's maximized
static void tell(struct sway_view *view, bool maximized) {
	if (view->type == SWAY_VIEW_XDG_SHELL) {
		struct wlr_xdg_toplevel *toplevel = view->wlr_xdg_toplevel;
		/* Before the tiled states, sway says a tile is maximized, so it
		 * draws no decorations outside its geometry; it stays so */
		if (wl_resource_get_version(toplevel->resource)
				>= XDG_TOPLEVEL_STATE_TILED_LEFT_SINCE_VERSION) {
			wlr_xdg_toplevel_set_maximized(toplevel, maximized);
		}
#if WLR_HAS_XWAYLAND
	} else if (view->type == SWAY_VIEW_XWAYLAND) {
		wlr_xwayland_surface_set_maximized(view->wlr_xwayland_surface,
			maximized, maximized);
#endif
	}
}

// The window area con's workspace has, or an empty box
static struct wlr_box area_of(struct sway_container *con) {
	struct wlr_box area = {0};
	if (con->pending.workspace) {
		workspace_get_box(con->pending.workspace, &area);
	}
	return area;
}

static void set(struct sway_container *con, bool maximized, bool event) {
	struct sway_view *view = con->view;
	if (view->tide.maximized == maximized) {
		return;
	}
	view->tide.maximized = maximized;
	bool floating = container_is_floating(con);
	if (maximized && floating) {
		view->tide.max_area = area_of(con);
	}
	if (maximized && floating && con->pending.fullscreen_mode != FULLSCREEN_NONE) {
		// Its floating place is the one it had as it went fullscreen
		view->tide.max_x = con->saved_x;
		view->tide.max_y = con->saved_y;
		view->tide.max_width = con->saved_width;
		view->tide.max_height = con->saved_height;
	} else if (maximized && floating) {
		view->tide.max_x = con->pending.x;
		view->tide.max_y = con->pending.y;
		view->tide.max_width = con->pending.width;
		view->tide.max_height = con->pending.height;
	} else if (floating && con->pending.fullscreen_mode != FULLSCREEN_NONE) {
		// Fullscreen gives back the place it had as it went fullscreen
		con->saved_x = view->tide.max_x;
		con->saved_y = view->tide.max_y;
		con->saved_width = view->tide.max_width;
		con->saved_height = view->tide.max_height;
	} else if (floating && (view->tide.max_width <= 0 || view->tide.max_height <= 0)) {
		/* It floated while maximized, so it had no floating place: it takes
		 * the size and place a window floating anew does */
		container_floating_set_default_size(con);
		container_floating_move_to_center(con);
	} else if (floating) {
		con->pending.x = view->tide.max_x;
		con->pending.y = view->tide.max_y;
		con->pending.width = view->tide.max_width;
		con->pending.height = view->tide.max_height;
		// Moved to another output since, it keeps its place there
		struct wlr_box now = area_of(con);
		if (now.width > 0 && now.height > 0 && view->tide.max_area.width > 0
				&& !wlr_box_equal(&now, &view->tide.max_area)) {
			floating_fix_coordinates(con, &view->tide.max_area, &now);
		}
	}
	tell(view, maximized);
	if (con->pending.workspace) {
		arrange_workspace(con->pending.workspace);
	}
	if (event) {
		ipc_event_window(con, "maximize");
	}
}

static void cover(struct sway_container *con, struct wlr_box *area) {
	if (!tide_maximized(con) || con->pending.fullscreen_mode != FULLSCREEN_NONE
			|| con->view->tide.minimized) {
		return;
	}
	con->pending.x = area->x;
	con->pending.y = area->y;
	con->pending.width = area->width;
	con->pending.height = area->height;
	arrange_container(con);
}

static void cover_tiles(list_t *children, struct wlr_box *area) {
	for (int i = 0; i < children->length; i++) {
		struct sway_container *child = children->items[i];
		if (child->view) {
			cover(child, area);
		} else {
			cover_tiles(child->pending.children, area);
		}
	}
}

static bool any_tile(list_t *children) {
	for (int i = 0; i < children->length; i++) {
		struct sway_container *child = children->items[i];
		if (child->view ? tide_maximized(child) && !child->view->tide.minimized
				&& child->pending.fullscreen_mode == FULLSCREEN_NONE
				: any_tile(child->pending.children)) {
			return true;
		}
	}
	return false;
}

bool tide_maximize_covers(struct sway_workspace *ws) {
	return any_tile(ws->tiling);
}

void tide_maximize_arrange(struct sway_workspace *ws, struct wlr_box *area) {
	cover_tiles(ws->tiling, area);
	for (int i = 0; i < ws->floating->length; i++) {
		cover(ws->floating->items[i], area);
	}
}

struct ended {
	struct sway_container *focus;
	list_t *ending;
};

static void find_ended(struct sway_container *con, void *data) {
	struct ended *e = data;
	// A minimized one waits as it was
	if (tide_maximized(con) && con != e->focus && !con->view->tide.minimized
			&& !view_is_transient_for(e->focus->view, con->view)) {
		list_add(e->ending, con);
	}
}

void tide_maximize_focused(struct sway_node *node) {
	if (!node || !node_is_view(node) || !node->sway_container->pending.workspace) {
		return;
	}
	struct ended e = { node->sway_container, create_list() };
	if (!e.ending) {
		sway_log(SWAY_ERROR, "tide: no memory to end a maximize");
		return;
	}
	workspace_for_each_container(node->sway_container->pending.workspace, find_ended, &e);
	for (int i = 0; i < e.ending->length; i++) {
		set(e.ending->items[i], false, true);
	}
	list_free(e.ending);
}

void tide_maximize_request(struct sway_view *view, bool maximized) {
	if (view->container) {
		set(view->container, maximized, true);
	}
}

void tide_maximize_opening(struct sway_view *view) {
	struct sway_container *con = view->container;
	struct sway_workspace *ws = con ? con->pending.workspace : NULL;
	if (!ws) {
		return;
	}
	set(con, true, false); // its new event says so
	if (container_is_floating(con)) {
		// Its place after is the middle of the window area, at its size
		struct wlr_box area;
		workspace_get_box(ws, &area);
		view->tide.max_area = area;
		view->tide.max_x = area.x + (area.width - view->tide.max_width) / 2;
		view->tide.max_y = area.y + (area.height - view->tide.max_height) / 2;
	}
}

void tide_maximize_unmapped(struct sway_view *view) {
	view->tide.maximized = false;
}

void tide_maximize_mapped(struct sway_view *view) {
	// A rule that maximized it as it mapped told it so already
	if (view->type != SWAY_VIEW_XDG_SHELL || view->tide.maximized) {
		return;
	}
	struct wlr_xdg_toplevel *toplevel = view->wlr_xdg_toplevel;
	if (toplevel->scheduled.maximized || toplevel->current.maximized) {
		tell(view, false);
	}
}

void tide_maximize_describe(struct sway_container *con, json_object *object) {
	if (con->view) {
		json_object_object_add(object, "maximized",
			json_object_new_boolean(con->view->tide.maximized));
	}
}

struct cmd_results *cmd_maximize(int argc, char **argv) {
	struct sway_container *con = config->handler_context.container;
	bool now = tide_maximized(con), want;
	if (argc == 0 || (argc == 1 && strcasecmp(argv[0], "toggle") == 0)) {
		want = !now;
	} else if (argc == 1 && strcasecmp(argv[0], "enable") == 0) {
		want = true;
	} else if (argc == 1 && strcasecmp(argv[0], "disable") == 0) {
		want = false;
	} else {
		return cmd_results_new(CMD_INVALID, "Expected maximize [enable|disable|toggle]");
	}
	if (!con || !con->view) {
		return cmd_results_new(CMD_FAILURE, "No window to maximize");
	}
	set(con, want, true);
	return cmd_results_new(CMD_SUCCESS, NULL);
}
