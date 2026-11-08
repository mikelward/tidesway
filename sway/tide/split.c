/*
 * Dragging the master/stack edge, as tidewm's split.h has it (tide's SPEC
 * §6.6): a plain left press within resize_grab pixels either side of the
 * edge between the masters and the stack drags it, setting mfact, until a
 * button comes up. threecol's centered master grows or shrinks both ways
 * from either side, and a lone window's sides drag its width, centered.
 * Anything over the edge takes the press as usual: a floating window, a
 * layer surface or a window's popup; so does a press on an output that
 * isn't focused, under a fullscreen window, or while a popup grabs the
 * pointer.
 */
#include <math.h>
#include <stdlib.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_seat.h>
#include "log.h"
#include "sway/config.h"
#include "sway/desktop/transaction.h"
#include "sway/input/cursor.h"
#include "sway/input/seat.h"
#include "sway/output.h"
#include "sway/server.h"
#include "sway/tide/layout.h"
#include "sway/tide/split.h"
#include "sway/tree/arrange.h"
#include "sway/tree/container.h"
#include "sway/tree/root.h"
#include "sway/tree/view.h"
#include "sway/tree/workspace.h"

// tidewm's config.h: 4 pixels either side
#define RESIZE_GRAB 4

struct tide_split_event {
	struct sway_workspace *ws;
	enum tide_layout lt;
	struct tide_edge edge;
	double offset; // the pointer's from the edge, which it keeps
	double start; // the pointer's x at the press
	bool moved; // until the pointer moves, nothing's set
};

static int grab(void) {
	return config->tide.resize_grab >= 0 ? config->tide.resize_grab : RESIZE_GRAB;
}

// Whether ws is still a workspace, not one gone since the press
static bool still_there(struct sway_workspace *ws) {
	for (int i = 0; i < root->outputs->length; i++) {
		struct sway_output *output = root->outputs->items[i];
		for (int j = 0; j < output->workspaces->length; j++) {
			if (output->workspaces->items[j] == ws) {
				return !ws->node.destroying;
			}
		}
	}
	return false;
}

static void handle_button(struct sway_seat *seat, uint32_t time_msec,
		struct wlr_input_device *device, uint32_t button,
		enum wl_pointer_button_state state) {
	if (state == WL_POINTER_BUTTON_STATE_RELEASED) {
		seatop_begin_default(seat);
	}
}

static void handle_pointer_motion(struct sway_seat *seat, uint32_t time_msec) {
	struct tide_split_event *e = seat->seatop_data;
	double x = seat->cursor->cursor->x;
	if (x != e->start) {
		e->moved = true;
	}
	// While the workspace or the layout isn't the one dragged, nothing changes
	if (!e->moved || !still_there(e->ws) || !workspace_is_visible(e->ws)
			|| tide_workspace_layout(e->ws) != e->lt) {
		return;
	}
	if (tide_follow(e->ws, &e->edge, x - e->offset)) {
		arrange_workspace(e->ws);
		transaction_commit_dirty();
	}
}

static const struct sway_seatop_impl seatop_impl = {
	.button = handle_button,
	.pointer_motion = handle_pointer_motion,
};

static void begin(struct sway_seat *seat, struct sway_workspace *ws,
		const struct tide_edge *edge) {
	struct tide_split_event *e = calloc(1, sizeof(*e));
	if (!e) {
		sway_log(SWAY_ERROR, "tide: can't allocate a drag");
		return;
	}
	double x = seat->cursor->cursor->x;
	e->ws = ws;
	e->lt = tide_workspace_layout(ws);
	e->edge = *edge;
	e->offset = x - edge->x;
	e->start = x;
	seatop_end(seat);
	seat->seatop_impl = &seatop_impl;
	seat->seatop_data = e;
	// The windows don't see the drag
	cursor_set_image(seat->cursor, "ew-resize", NULL);
	wlr_seat_pointer_notify_clear_focus(seat->wlr_seat);
}

/* The focused workspace, if the cursor's on its output's window area and
 * nothing covers its tiles: no fullscreen window, lock or pointer grab */
static struct sway_workspace *pressed_workspace(struct sway_seat *seat) {
	struct wlr_cursor *cursor = seat->cursor->cursor;
	struct sway_workspace *ws = seat_get_focused_workspace(seat);
	if (!ws || !ws->output || ws->fullscreen || root->fullscreen_global
			|| server.session_lock.lock
			|| seat->wlr_seat->pointer_state.grab
				!= seat->wlr_seat->pointer_state.default_grab
			|| tide_workspace_layout(ws) == TIDE_NONE) {
		return NULL;
	}
	struct wlr_output *at = wlr_output_layout_output_at(root->output_layout,
		cursor->x, cursor->y);
	struct wlr_box area;
	workspace_get_box(ws, &area);
	if (at != ws->output->wlr_output || cursor->y < area.y
			|| cursor->y >= area.y + area.height) {
		return NULL;
	}
	return ws;
}

bool tide_split_press(struct sway_seat *seat, struct sway_node *node,
		struct wlr_surface *surface) {
	int g = grab();
	struct sway_workspace *ws = g > 0 ? pressed_workspace(seat) : NULL;
	if (!ws) {
		return false;
	}
	/* What's over the tiles takes the press: a layer surface or its popup,
	 * an unmanaged window, a floating window, or a window's popup */
	if (!node) {
		return false;
	}
	if (node->type == N_CONTAINER) {
		struct sway_container *con = node->sway_container;
		if (container_is_floating_or_child(con) || (surface && con->view
				&& wlr_surface_get_root_surface(surface) != con->view->surface)) {
			return false;
		}
	} else if (node->type != N_WORKSPACE) {
		return false;
	}
	double x = seat->cursor->cursor->x;
	struct tide_edge edges[2];
	int n = tide_edges(ws, edges);
	for (int i = 0; i < n; i++) {
		if (x >= edges[i].x - g && x < edges[i].x + g) {
			begin(seat, ws, &edges[i]);
			return true;
		}
	}
	return false;
}

bool tide_split_press_tile(struct sway_seat *seat, struct sway_container *con) {
	struct sway_workspace *ws = con->pending.workspace;
	if (!ws || !con->view || container_is_floating_or_child(con)
			|| tide_workspace_layout(ws) == TIDE_NONE || ws->fullscreen) {
		return false;
	}
	struct tide_edge edges[2];
	int n = tide_edges(ws, edges), best = 0;
	double x = seat->cursor->cursor->x;
	if (!n) {
		// A tile with no edge floats, and resizes as a floating window does
		container_set_floating(con, true);
		arrange_workspace(ws);
		seat_set_focus_container(seat, con);
		seatop_begin_resize_floating(seat, con, WLR_EDGE_BOTTOM | WLR_EDGE_RIGHT);
		return true;
	}
	for (int i = 1; i < n; i++) {
		if (fabs(edges[i].x - x) < fabs(edges[best].x - x)) {
			best = i;
		}
	}
	seat_set_focus_container(seat, con);
	begin(seat, ws, &edges[best]);
	return true;
}
