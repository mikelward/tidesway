/*
 * tidewm's direction.h, for sway's tree: focus and move by direction on
 * a workspace in a tide layout pick by the windows' boxes on screen, not
 * by sway's split tree, which a tide layout keeps as one flat row.
 */
#include <limits.h>
#include <wlr/util/box.h>
#include "list.h"
#include "sway/input/input-manager.h"
#include "sway/input/seat.h"
#include "sway/ipc-server.h"
#include "sway/output.h"
#include "sway/tide/direction.h"
#include "sway/tide/focus.h"
#include "sway/tide/layout.h"
#include "sway/tree/arrange.h"
#include "sway/tree/container.h"
#include "sway/tree/root.h"
#include "sway/tree/workspace.h"

#define DIR_MIN(a, b) ((a) < (b) ? (a) : (b))
#define DIR_MAX(a, b) ((a) > (b) ? (a) : (b))

// A box's center, doubled, so it stays whole
static int cx(struct wlr_box b) {
	return 2 * b.x + b.width;
}

static int cy(struct wlr_box b) {
	return 2 * b.y + b.height;
}

// Whether to's center is on dir's side of from's
static bool beyond(struct wlr_box from, struct wlr_box to, enum wlr_direction dir) {
	switch (dir) {
	case WLR_DIRECTION_LEFT:
		return cx(to) < cx(from);
	case WLR_DIRECTION_RIGHT:
		return cx(to) > cx(from);
	case WLR_DIRECTION_UP:
		return cy(to) < cy(from);
	default:
		return cy(to) > cy(from);
	}
}

// How much a and b share across dir: height going left or right, else width
static int shared(struct wlr_box a, struct wlr_box b, enum wlr_direction dir) {
	int lo, hi;
	if (dir == WLR_DIRECTION_LEFT || dir == WLR_DIRECTION_RIGHT) {
		lo = DIR_MAX(a.y, b.y);
		hi = DIR_MIN(a.y + a.height, b.y + b.height);
	} else {
		lo = DIR_MAX(a.x, b.x);
		hi = DIR_MIN(a.x + a.width, b.x + b.width);
	}
	return DIR_MAX(hi - lo, 0);
}

static long long distance(struct wlr_box a, struct wlr_box b) {
	long long dx = cx(a) - cx(b), dy = cy(a) - cy(b);
	return dx * dx + dy * dy;
}

// Whether to beats best going dir from from: sharing more, then nearer
static bool better(struct wlr_box from, struct wlr_box to, const struct wlr_box *best,
		enum wlr_direction dir) {
	if (!best) {
		return true;
	}
	int s = shared(from, to, dir), best_s = shared(from, *best, dir);
	if (s != best_s) {
		return s > best_s;
	}
	return distance(from, to) < distance(from, *best);
}

// How far b is from the edge a window going dir comes in by
static int from_edge(struct wlr_box b, enum wlr_direction dir) {
	switch (dir) {
	case WLR_DIRECTION_LEFT:
		return -(b.x + b.width);
	case WLR_DIRECTION_RIGHT:
		return b.x;
	case WLR_DIRECTION_UP:
		return -(b.y + b.height);
	default:
		return b.y;
	}
}

// The next enabled output going dir from output, or NULL: no wrapping
static struct sway_output *next_output(struct sway_output *output, enum wlr_direction dir) {
	struct wlr_box from, box, best_box;
	struct sway_output *found = NULL;
	output_get_box(output, &from);
	for (int i = 0; i < root->outputs->length; i++) {
		struct sway_output *o = root->outputs->items[i];
		if (o == output || !o->enabled) {
			continue;
		}
		output_get_box(o, &box);
		if (beyond(from, box, dir) && better(from, box, found ? &best_box : NULL, dir)) {
			found = o;
			best_box = box;
		}
	}
	return found;
}

static void add_leaves(list_t *children, list_t *out) {
	for (int i = 0; i < children->length; i++) {
		struct sway_container *child = children->items[i];
		if (child->view) {
			// tide: never a minimized window, which only focusing it brings back
			if (child->pending.fullscreen_mode == FULLSCREEN_NONE
					&& !tide_focus_passed_by(child)) {
				list_add(out, child);
			}
		} else {
			add_leaves(child->pending.children, out);
		}
	}
}

/* ws's windows going by direction finds: its tiles in tiled order, then,
 * unless tiled, its floating windows. A fullscreen window covers the
 * others: then only it. */
static list_t *candidates(struct sway_workspace *ws, bool tiled) {
	list_t *out = create_list();
	if (ws->fullscreen) {
		// A fullscreen split's window focus last went to, as sway's focus
		struct sway_container *v = ws->fullscreen->view ? ws->fullscreen
			: seat_get_focus_inactive_view(input_manager_current_seat(),
				&ws->fullscreen->node);
		if (!tiled && v) {
			list_add(out, v);
		}
		return out;
	}
	add_leaves(ws->tiling, out);
	for (int i = 0; !tiled && i < ws->floating->length; i++) {
		struct sway_container *f = ws->floating->items[i];
		// tide: never a status popup, which only the pointer focuses
		if (f->view && !tide_focus_passed_by(f)) {
			list_add(out, f);
		}
	}
	return out;
}

/* The window going dir from from finds on ws, other than skip: one
 * beyond from, if want_beyond, or else one at the edge going dir comes in
 * by, as onto the next output */
static struct sway_container *window_in(struct sway_workspace *ws, struct wlr_box from,
		enum wlr_direction dir, struct sway_container *skip, bool want_beyond, bool tiled) {
	list_t *cons = candidates(ws, tiled);
	struct sway_container *found = NULL;
	struct wlr_box box, best;
	int edge = INT_MAX;
	for (int i = 0; !want_beyond && i < cons->length; i++) {
		if (cons->items[i] != skip) {
			container_get_box(cons->items[i], &box);
			edge = DIR_MIN(edge, from_edge(box, dir));
		}
	}
	for (int i = 0; i < cons->length; i++) {
		struct sway_container *c = cons->items[i];
		if (c == skip) {
			continue;
		}
		container_get_box(c, &box);
		if (want_beyond ? !beyond(from, box, dir) : from_edge(box, dir) != edge) {
			continue;
		}
		if (better(from, box, found ? &best : NULL, dir)) {
			found = c;
			best = box;
		}
	}
	list_free(cons);
	return found;
}

bool tide_direction_applies(struct sway_container *con) {
	struct sway_workspace *ws = con ? con->pending.workspace : NULL;
	return ws && con->view && !container_is_floating_or_child(con)
		&& con->pending.fullscreen_mode == FULLSCREEN_NONE
		&& tide_workspace_layout(ws) != TIDE_NONE;
}

struct sway_node *tide_focus_in_direction(struct sway_container *con,
		enum wlr_direction dir) {
	struct sway_workspace *ws = con->pending.workspace;
	struct wlr_box from;
	container_get_box(con, &from);
	struct sway_container *c = window_in(ws, from, dir, con, true, false);
	if (c) {
		return &c->node;
	}
	struct sway_output *output = ws->output ? next_output(ws->output, dir) : NULL;
	struct sway_workspace *next = output ? output_get_active_workspace(output) : NULL;
	if (!next) {
		return NULL;
	}
	c = window_in(next, from, dir, NULL, false, false);
	return c ? &c->node : &next->node;
}

const char *tide_move_in_direction(struct sway_container *con, enum wlr_direction dir) {
	struct sway_workspace *ws = con->pending.workspace;
	if (!workspace_is_visible(ws)) {
		return "The window's workspace isn't shown";
	}
	struct wlr_box from;
	container_get_box(con, &from);
	struct sway_container *t = window_in(ws, from, dir, con, true, true);
	if (t) {
		// Swaps with the tile that way, in a flat tree
		tide_flatten(ws);
		int a = list_find(ws->tiling, con), b = list_find(ws->tiling, t);
		if (a >= 0 && b >= 0) {
			list_swap(ws->tiling, a, b);
			node_set_dirty(&ws->node);
			arrange_workspace(ws);
		}
		return NULL;
	}
	struct sway_output *output = ws->output ? next_output(ws->output, dir) : NULL;
	struct sway_workspace *next = output ? output_get_active_workspace(output) : NULL;
	if (!next) {
		return NULL; // past the last output, nothing moves
	}
	struct sway_seat *seat = input_manager_current_seat();
	bool focused = seat_get_focused_container(seat) == con;
	t = window_in(next, from, dir, NULL, false, true);
	container_detach(con);
	con->pending.width = con->pending.height = 0;
	con->width_fraction = con->height_fraction = 0;
	if (tide_workspace_layout(next) != TIDE_NONE) {
		// In t's place, which goes one along
		tide_flatten(next);
		int at = t ? list_find(next->tiling, t) : -1;
		workspace_insert_tiling_direct(next, con, at >= 0 ? at : next->tiling->length);
	} else if (t) {
		container_add_sibling(t, con, 0);
	} else {
		workspace_add_tiling(next, con);
	}
	container_update_representation(con);
	ipc_event_window(con, "move");
	workspace_detect_urgent(ws);
	workspace_detect_urgent(next);
	arrange_workspace(ws);
	arrange_workspace(next);
	if (focused) {
		seat_set_focus_container(seat, con);
	}
	return NULL;
}

struct sway_container *tide_focus_next(struct sway_workspace *ws,
		struct sway_container *con, int step, bool *handled) {
	if (!con) {
		/* No window focused, in tide's layouts or sway's: its most recent,
		 * or one the focus guard holds */
		*handled = ws != NULL;
		return ws ? tide_focus_go(ws) : NULL;
	}
	ws = con->pending.workspace;
	*handled = ws && con->view && tide_workspace_layout(ws) != TIDE_NONE;
	if (!*handled || con->pending.fullscreen_mode != FULLSCREEN_NONE) {
		return NULL;
	}
	list_t *cons = candidates(ws, false);
	struct sway_container *found = con;
	int at = list_find(cons, con);
	if (at >= 0 && cons->length > 1) {
		found = cons->items[(at + step + cons->length) % cons->length];
	}
	list_free(cons);
	return found;
}
