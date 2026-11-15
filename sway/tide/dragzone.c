/*
 * A window's shadow as its drag zone. include/sway/tide/dragzone.h says
 * how it behaves.
 */
#include <math.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_layer_shell_v1.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_xcursor_manager.h>
#include <wlr/util/edges.h>
#include "sway/config.h"
#include "sway/input/cursor.h"
#include "sway/input/seat.h"
#include "sway/layers.h"
#include "sway/output.h"
#include "sway/scene_descriptor.h"
#include "sway/server.h"
#include "sway/tide/dragzone.h"
#include "sway/tide/look.h"
#include "sway/tide/maximize.h"
#include "sway/tide/minimize.h"
#include "sway/tide/split.h"
#include "sway/tree/container.h"
#include "sway/tree/node.h"
#include "sway/tree/root.h"
#include "sway/tree/view.h"
#include "sway/tree/workspace.h"

// How far the zone reaches where no window has a shadow, as tide's on Hyprland
#define GRAB 8

static int reach(void) {
	int shadow = tide_look_shadow_reach();
	return shadow > 0 ? shadow : GRAB;
}

/* Whether con is drawn in a layer over the floating windows' one, as a
 * fullscreen window is, and its dialog, which is up there with it */
static bool over_floating(struct sway_container *con) {
	struct wlr_scene_node *layer = &con->scene_tree->node;
	while (layer->parent && layer->parent != root->layer_tree) {
		layer = &layer->parent->node;
	}
	if (!layer->parent) {
		return true; // in no layer we know: it keeps the press
	}
	// The layers are in order, the lowest first
	struct wlr_scene_node *node;
	bool past = false;
	wl_list_for_each(node, &root->layer_tree->children, link) {
		if (node == layer) {
			return past;
		}
		past = past || node == &root->layers.floating->node;
	}
	return true;
}

/* Whether what node_at_coords found may be under a floating window's
 * zone: a tile, a floating window, the workspace, or a background or
 * bottom layer surface. Into *above, the floating window found, which
 * only a window over it has a zone across. */
static bool under(struct sway_seat *seat, struct sway_node *node,
		struct wlr_surface *surface, struct sway_container **above) {
	*above = NULL;
	if (server.session_lock.lock || root->fullscreen_global
			|| seat->wlr_seat->pointer_state.grab
				!= seat->wlr_seat->pointer_state.default_grab) {
		return false;
	}
	if (!node) {
		struct wlr_layer_surface_v1 *layer =
			surface ? toplevel_layer_surface_from_surface(surface) : NULL;
		// Not a layer surface's popup, which is over every window
		return layer && layer->current.layer <= ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM
			&& wlr_surface_get_root_surface(surface) == layer->surface;
	}
	if (node->type == N_WORKSPACE) {
		return true;
	}
	if (node->type != N_CONTAINER) {
		return false;
	}
	struct sway_container *con = node->sway_container;
	// A window's popup is over every window
	if (surface && con->view
			&& wlr_surface_get_root_surface(surface) != con->view->surface) {
		return false;
	}
	/* A fullscreen window is over every floating one, as on the next
	 * output, and so is whatever else is in a layer over theirs, as its
	 * dialog */
	if (container_is_fullscreen_or_child(con) || over_floating(con)) {
		return false;
	}
	if (container_is_floating_or_child(con)) {
		*above = container_toplevel_ancestor(con);
	}
	return true;
}

/* The topmost floating window whose zone holds the point, and the edges
 * the point is past into *edges, or NULL: none does, the point is in a
 * floating window, or above comes first, the floating window found there.
 * Floating windows share one stack, whichever output each is on, so it's
 * the scene's order that says which is on top. */
static struct sway_container *floating_at(double x, double y,
		struct sway_container *above, enum wlr_edges *edges) {
	int r = reach();
	struct wlr_scene_node *node;
	wl_list_for_each_reverse(node, &root->layers.floating->children, link) {
		struct sway_container *con =
			scene_descriptor_try_get(node, SWAY_SCENE_DESC_CONTAINER);
		if (!con || !node->enabled) {
			continue; // not a window, or on a workspace not shown
		}
		if (con == above) {
			return NULL;
		}
		struct sway_workspace *ws = con->pending.workspace;
		if (!ws || ws->fullscreen || !workspace_is_visible(ws)
				|| !container_is_floating(con) || con->node.destroying
				|| con->pending.fullscreen_mode != FULLSCREEN_NONE
				|| tide_maximized(con) || tide_minimized(con)) {
			continue;
		}
		double x0 = con->pending.x, y0 = con->pending.y;
		double x1 = x0 + con->pending.width, y1 = y0 + con->pending.height;
		if (x < x0 - r || x >= x1 + r || y < y0 - r || y >= y1 + r) {
			continue;
		}
		enum wlr_edges e = WLR_EDGE_NONE;
		if (x < x0) {
			e |= WLR_EDGE_LEFT;
		} else if (x >= x1) {
			e |= WLR_EDGE_RIGHT;
		}
		if (y < y0) {
			e |= WLR_EDGE_TOP;
		} else if (y >= y1) {
			e |= WLR_EDGE_BOTTOM;
		}
		if (e == WLR_EDGE_NONE) {
			return NULL; // in the window
		}
		// Beside the end of an edge, its corner, as in tidewm
		if (!(e & (WLR_EDGE_LEFT | WLR_EDGE_RIGHT))) {
			double corner = fmin(fmin(r, 2 * GRAB), floor(con->pending.width / 3));
			e |= x < x0 + corner ? WLR_EDGE_LEFT
				: x >= x1 - corner ? WLR_EDGE_RIGHT : WLR_EDGE_NONE;
		} else if (!(e & (WLR_EDGE_TOP | WLR_EDGE_BOTTOM))) {
			double corner = fmin(fmin(r, 2 * GRAB), floor(con->pending.height / 3));
			e |= y < y0 + corner ? WLR_EDGE_TOP
				: y >= y1 - corner ? WLR_EDGE_BOTTOM : WLR_EDGE_NONE;
		}
		*edges = e;
		return con;
	}
	return NULL;
}

static struct sway_container *zone(struct sway_seat *seat, struct sway_node *node,
		struct wlr_surface *surface, enum wlr_edges *edges) {
	struct sway_container *above;
	if (!under(seat, node, surface, &above)) {
		return NULL;
	}
	return floating_at(seat->cursor->cursor->x, seat->cursor->cursor->y, above, edges);
}

bool tide_dragzone_press(struct sway_seat *seat, struct sway_node *node,
		struct wlr_surface *surface) {
	enum wlr_edges edges;
	struct sway_container *con = zone(seat, node, surface, &edges);
	if (!con) {
		return tide_split_press(seat, node, surface);
	}
	// The window in it that had focus last, if it's a container of several
	struct sway_node *focus = seat_get_focus_inactive(seat, &con->node);
	seat_set_focus(seat, focus ? focus : &con->node);
	seatop_begin_resize_floating(seat, con, edges);
	return true;
}

bool tide_dragzone_hover(struct sway_seat *seat, struct sway_node *node,
		struct wlr_surface **surface) {
	if (!config) { // a seat made before the config is read
		return false;
	}
	enum wlr_edges edges;
	if (!zone(seat, node, *surface, &edges) && !tide_split_hover(seat, node, *surface)) {
		return false;
	}
	*surface = NULL;
	return true;
}

bool tide_dragzone_cursor(struct sway_cursor *cursor) {
	if (!config) {
		return false;
	}
	struct sway_seat *seat = cursor->seat;
	struct wlr_surface *surface = NULL;
	double sx, sy;
	struct sway_node *node = node_at_coords(seat, cursor->cursor->x,
		cursor->cursor->y, &surface, &sx, &sy);
	enum wlr_edges edges;
	if (zone(seat, node, surface, &edges)) {
		cursor_set_image(cursor, wlr_xcursor_get_resize_name(edges), NULL);
		return true;
	}
	if (tide_split_hover(seat, node, surface)) {
		cursor_set_image(cursor, "ew-resize", NULL);
		return true;
	}
	return false;
}
