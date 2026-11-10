/*
 * tide's layouts in sway, ported from tidewm's layout.h and dwl's tile()
 * and monocle(), to the pixel. A workspace is one of tidewm's parts: it
 * keeps its own layouts, mfact and nmaster, and each layout its own mfact
 * and nmaster. Its tiled windows, in tree order, are tidewm's clients in
 * stack order: the masters first, then the stack.
 *
 * Geometry is tide's (hypr/tide/geometry.lua): boxes on whole pixels,
 * neighbors sharing an edge, covering the window area with no gap and no
 * overlap. A window's size hints can make it taller or shorter than its
 * box, and then the rows after it in its column start at its bottom.
 */
#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wlr/util/box.h>
#include "list.h"
#include "log.h"
#include "sway/config.h"
#include "sway/ipc-server.h"
#include "sway/output.h"
#include "sway/tide/layout.h"
#include "sway/tree/arrange.h"
#include "sway/tree/container.h"
#include "sway/tree/root.h"
#include "sway/tree/view.h"
#include "sway/tree/workspace.h"

#define TIDE_MIN(a, b) ((a) < (b) ? (a) : (b))
#define TIDE_MAX(a, b) ((a) > (b) ? (a) : (b))

static const char *const names[TIDE_LAYOUTS] = {
	[TIDE_TILE] = "tile",
	[TIDE_THREECOL] = "threecol",
	[TIDE_TWOCOL] = "twocol",
	[TIDE_MONOCLE] = "monocle",
};

// config.def.h's layouts' symbols in tidewm
static const char *const symbols[TIDE_LAYOUTS] = {
	[TIDE_TILE] = "[]=",
	[TIDE_THREECOL] = "|M|",
	[TIDE_TWOCOL] = "||=",
	[TIDE_MONOCLE] = "[M]",
};

// tidewm's config.h: a lone window is 80% wide from 2.1, 60% from 3.2
static const struct tide_single_rule single_window[] = {
	{ 2.1, 0.8 },
	{ 3.2, 0.6 },
};

// tidewm's monitor rule, and the layouts' own defaults (lt_defaults)
#define RULE_MFACT 0.55f
#define RULE_NMASTER 1

void tide_config_defaults(struct tide_config *tc) {
	memset(tc, 0, sizeof(*tc));
	tc->first_layout[0] = tc->first_layout[1] = TIDE_NONE;
	tc->new_window = -1;
	tc->resize_grab = -1;
}

const char *tide_layout_name(enum tide_layout lt) {
	return lt >= 0 && lt < TIDE_LAYOUTS ? names[lt] : NULL;
}

/* lt's mfact and nmaster before they're changed: the config file's
 * layout_defaults, then tide's, then the monitor rule's */
static void mode_defaults(enum tide_layout lt, float *mfact, int *nmaster) {
	*mfact = RULE_MFACT;
	*nmaster = RULE_NMASTER;
	if (lt == TIDE_THREECOL) {
		*mfact = 0.5f;
		*nmaster = 1;
	} else if (lt == TIDE_TWOCOL) {
		*mfact = 0.72f;
		*nmaster = 2;
	}
	if (lt < 0 || lt >= TIDE_MODES || !config) {
		return;
	}
	if (config->tide.layout_defaults[lt].has_mfact) {
		*mfact = config->tide.layout_defaults[lt].mfact;
	}
	if (config->tide.layout_defaults[lt].has_nmaster) {
		*nmaster = config->tide.layout_defaults[lt].nmaster;
	}
}

// Where an ultrawide monitor begins: ultrawide_aspect, else tide's 2.1
static double ultrawide(void) {
	return config && config->tide.ultrawide_aspect > 0 ?
		config->tide.ultrawide_aspect : 2.1;
}

/* The layout a new workspace starts in: default_layout for its output's
 * aspect, else threecol on an ultrawide and tile otherwise */
static enum tide_layout first_layout(struct sway_output *output) {
	int wide = output && output->height > 0 &&
		output->width >= ultrawide() * output->height;
	enum tide_layout lt = wide ? TIDE_THREECOL : TIDE_TILE;
	if (config && config->tide.first_layout[wide] != TIDE_NONE) {
		lt = config->tide.first_layout[wide];
	}
	return lt;
}

static void defaults_for(struct tide_workspace *t, struct sway_output *output) {
	enum tide_layout lt = first_layout(output);
	memset(t->modes, 0, sizeof(t->modes));
	t->lt[0] = lt;
	t->lt[1] = TIDE_NONE;
	t->sellt = 0;
	t->single = 0;
	mode_defaults(lt, &t->mfact, &t->nmaster);
}

void tide_workspace_init(struct sway_workspace *ws) {
	struct tide_workspace *t = &ws->tide;
	memset(t, 0, sizeof(*t));
	defaults_for(t, NULL);
	if (ws->output && ws->output->width > 0 && ws->output->height > 0) {
		defaults_for(t, ws->output);
		t->settled = true;
	}
}

/* ws's layouts, chosen by its output's aspect once it has one, as tidewm
 * waits for a monitor's geometry */
static struct tide_workspace *settled(struct sway_workspace *ws) {
	struct tide_workspace *t = &ws->tide;
	if (!t->settled && ws->output && ws->output->width > 0
			&& ws->output->height > 0) {
		defaults_for(t, ws->output);
		t->settled = true;
	}
	return t;
}

enum tide_layout tide_workspace_layout(struct sway_workspace *ws) {
	if (!ws) {
		return TIDE_NONE;
	}
	struct tide_workspace *t = settled(ws);
	return t->off ? TIDE_NONE : t->lt[t->sellt];
}

// Whether con is a tile tide lays out: not fullscreen, nor minimized
static bool tiles(struct sway_container *con) {
	return con->pending.fullscreen_mode == FULLSCREEN_NONE
		&& !(con->view && con->view->tide.minimized);
}

/* ws's tiled windows in tree order, into out; with all, the fullscreen
 * ones too. Split containers, sway's own, are passed through. */
static void collect(list_t *children, list_t *out, bool all) {
	for (int i = 0; i < children->length; i++) {
		struct sway_container *child = children->items[i];
		if (child->view) {
			if (all || tiles(child)) {
				list_add(out, child);
			}
		} else {
			collect(child->pending.children, out, all);
		}
	}
}

void tide_layout_symbol(struct sway_workspace *ws, char *buf, int size) {
	enum tide_layout lt = tide_workspace_layout(ws);
	if (lt == TIDE_NONE) {
		snprintf(buf, size, "%s", "");
		return;
	}
	if (lt == TIDE_MONOCLE) {
		// monocle counts its windows, as dwl's monocle() does
		list_t *leaves = create_list();
		collect(ws->tiling, leaves, false);
		int n = leaves->length;
		list_free(leaves);
		if (n) {
			snprintf(buf, size, "[%d]", n);
			return;
		}
	}
	snprintf(buf, size, "%s", symbols[lt]);
}

/* Cuts [start, start + len) into n pieces weighted by w, into pos and
 * size. Each edge is rounded, not each size, so the pieces meet and add up
 * to len. */
static void spans(int start, int len, const double *w, int n, int *pos, int *size) {
	double total = 0, acc = 0;
	int prev = start;
	for (int i = 0; i < n; i++) {
		total += w[i];
	}
	for (int i = 0; i < n; i++) {
		acc += w[i];
		int edge = (int)floor(start + len * acc / total + 0.5);
		pos[i] = prev;
		size[i] = edge - prev;
		prev = edge;
	}
}

// n columns, at most 16, across area, weighted by w, into out
static void columns(struct wlr_box area, const double *w, int n, struct wlr_box *out) {
	int pos[16], size[16];
	spans(area.x, area.width, w, n, pos, size);
	for (int i = 0; i < n; i++) {
		out[i] = (struct wlr_box){ pos[i], area.y, size[i], area.height };
	}
}

// n equal rows down area, into out
static void rows(struct wlr_box area, int n, struct wlr_box *out) {
	int prev = area.y;
	for (int i = 0; i < n; i++) {
		int edge = (int)floor(area.y + area.height * (i + 1) / (double)n + 0.5);
		out[i] = (struct wlr_box){ area.x, prev, area.width, edge - prev };
		prev = edge;
	}
}

static bool same_column(const struct wlr_box *a, const struct wlr_box *b) {
	return a->x == b->x && a->width == b->width;
}

/* Gives con box, as dwl's resize() does: at least 1 pixel, and its content
 * within its size hints, its borders and title bar added to them, as
 * dwl's applybounds() adds its border. Returns the height it took. */
static int resize(struct sway_container *con, struct wlr_box box) {
	double min_w, max_w, min_h, max_h;
	view_get_constraints(con->view, &min_w, &max_w, &min_h, &max_h);
	con->pending.x = box.x;
	con->pending.y = box.y;
	con->pending.width = TIDE_MAX(1, box.width);
	con->pending.height = TIDE_MAX(1, box.height);
	// The content's size in the box tells what its decorations take
	arrange_container(con);
	int deco_w = (int)(con->pending.width - con->pending.content_width);
	int deco_h = (int)(con->pending.height - con->pending.content_height);
	int width = (int)con->pending.width - deco_w;
	int height = (int)con->pending.height - deco_h;
	if (min_w > width && min_w < INT_MAX) {
		width = (int)min_w;
	}
	if (min_h > height && min_h < INT_MAX) {
		height = (int)min_h;
	}
	if (max_w > 0 && max_w < width) {
		width = (int)max_w;
	}
	if (max_h > 0 && max_h < height) {
		height = (int)max_h;
	}
	width = TIDE_MAX(1, width + deco_w);
	height = TIDE_MAX(1, height + deco_h);
	if (width != con->pending.width || height != con->pending.height) {
		con->pending.width = width;
		con->pending.height = height;
		arrange_container(con);
	}
	return height;
}

/* Gives the n windows the boxes, in order. As dwl's tile() goes on from
 * each window's real height, the rows after a window in its column start
 * at its bottom, and share what's left of the column evenly. */
static void place(list_t *leaves, struct wlr_box *boxes, int n) {
	for (int i = 0; i < n; i++) {
		int height = resize(leaves->items[i], boxes[i]);
		if (height == boxes[i].height) {
			continue;
		}
		// The later rows in this column, and where it ends
		int end = boxes[i].y + boxes[i].height, bottom = end, count = 0;
		for (int j = i + 1; j < n; j++) {
			if (same_column(&boxes[j], &boxes[i]) && boxes[j].y >= end) {
				count++;
				bottom = boxes[j].y + boxes[j].height;
			}
		}
		int top = boxes[i].y + height;
		int left = TIDE_MAX(bottom - top, 0);
		// Each is checked before it's moved, so the later ones still are
		for (int j = i + 1, k = 0; j < n && k < count; j++) {
			if (!same_column(&boxes[j], &boxes[i]) || boxes[j].y < end) {
				continue;
			}
			boxes[j].y = top + (int)floor(left * k / (double)count + 0.5);
			boxes[j].height = top + (int)floor(left * (k + 1) / (double)count + 0.5)
				- boxes[j].y;
			k++;
		}
	}
}

/* The lone window's width, as a fraction: the workspace's own, else the
 * single_window rule the window area's aspect reaches with the largest
 * min_aspect, else the whole width */
static double single_width(struct tide_workspace *t, struct wlr_box area) {
	const struct tide_single_rule *r = single_window;
	int n = sizeof(single_window) / sizeof(single_window[0]);
	if (config && config->tide.nsingles) {
		r = config->tide.singles;
		n = config->tide.nsingles;
	}
	double aspect = area.height > 0 ? area.width / (double)area.height : 0;
	double best = -1, width = 1;
	if (t->single > 0) {
		return TIDE_MAX(0.1, TIDE_MIN(t->single, 1));
	}
	for (int i = 0; i < n; i++) {
		if (aspect >= r[i].min_aspect && r[i].min_aspect > best) {
			best = r[i].min_aspect;
			width = r[i].width;
		}
	}
	return TIDE_MAX(0.1, TIDE_MIN(width, 1));
}

// The lone window's box: centered at its width
static struct wlr_box single_box(struct tide_workspace *t, struct wlr_box area) {
	double f = single_width(t, area), side = (1 - f) / 2;
	if (f >= 1) {
		return area;
	}
	struct wlr_box cols[3];
	columns(area, (double[]){ side, f, side }, 3, cols);
	return cols[1];
}

// dwl's tile(), with its rounding
static void tile(struct tide_workspace *t, struct wlr_box area, list_t *leaves) {
	int n = leaves->length, mw, my = 0, ty = 0;
	if (n > t->nmaster) {
		mw = t->nmaster ? (int)roundf(area.width * t->mfact) : 0;
	} else {
		mw = area.width;
	}
	for (int i = 0; i < n; i++) {
		struct sway_container *con = leaves->items[i];
		if (i < t->nmaster) {
			my += resize(con, (struct wlr_box){ area.x, area.y + my, mw,
				(area.height - my) / (TIDE_MIN(n, t->nmaster) - i) });
		} else {
			ty += resize(con, (struct wlr_box){ area.x + mw, area.y + ty,
				area.width - mw, (area.height - ty) / (n - i) });
		}
	}
}

/* The master centered, with a stack either side. With one stack window
 * it's the master and one column. Stack windows go right, left, right...,
 * so adding one never moves another to the other side. */
static void threecol(struct tide_workspace *t, struct wlr_box area, list_t *leaves) {
	int n = leaves->length;
	struct wlr_box *boxes = calloc(n, sizeof(*boxes)), cols[3];
	if (!boxes) {
		sway_log(SWAY_ERROR, "tide: can't allocate %d boxes", n);
		return;
	}
	double two[2] = { t->mfact, 1 - t->mfact };
	double three[3] = { (1 - t->mfact) / 2, t->mfact, (1 - t->mfact) / 2 };
	int nm = TIDE_MIN(TIDE_MAX(t->nmaster, 1), n), s = n - nm;
	if (s == 0) {
		rows(area, n, boxes);
	} else if (s == 1) {
		columns(area, two, 2, cols);
		rows(cols[0], nm, boxes);
		boxes[nm] = cols[1];
	} else {
		columns(area, three, 3, cols);
		int nright = (s + 1) / 2, r = 0, l = 0;
		struct wlr_box *right = calloc(nright, sizeof(*right));
		struct wlr_box *left = calloc(s - nright, sizeof(*left));
		if (!right || !left) {
			sway_log(SWAY_ERROR, "tide: can't allocate %d boxes", s);
			free(right);
			free(left);
			free(boxes);
			return;
		}
		rows(cols[2], nright, right);
		rows(cols[0], s - nright, left);
		rows(cols[1], nm, boxes);
		for (int k = 0; k < s; k++) {
			boxes[nm + k] = k % 2 == 0 ? right[r++] : left[l++];
		}
		free(right);
		free(left);
	}
	place(leaves, boxes, n);
	free(boxes);
}

/* The masters as full-height columns side by side, sharing mfact of the
 * width, and the rest stacked on the right. With no stack the masters
 * share the whole width. */
static void twocol(struct tide_workspace *t, struct wlr_box area, list_t *leaves) {
	int n = leaves->length;
	struct wlr_box *boxes = calloc(n, sizeof(*boxes)), cols[16];
	double w[16];
	if (!boxes) {
		sway_log(SWAY_ERROR, "tide: can't allocate %d boxes", n);
		return;
	}
	// At most 15 master columns, which fit columns()' arrays
	int nm = TIDE_MIN(TIDE_MAX(t->nmaster, 1), TIDE_MIN(n, 15));
	for (int i = 0; i < nm; i++) {
		w[i] = nm == n ? 1 : t->mfact / nm;
	}
	w[nm] = 1 - t->mfact;
	columns(area, w, nm == n ? nm : nm + 1, cols);
	for (int i = 0; i < nm; i++) {
		boxes[i] = cols[i];
	}
	if (nm < n) {
		rows(cols[nm], n - nm, boxes + nm);
	}
	place(leaves, boxes, n);
	free(boxes);
}

// Split containers take the whole area, so their windows place by it
static void cover(list_t *children, struct wlr_box area) {
	for (int i = 0; i < children->length; i++) {
		struct sway_container *child = children->items[i];
		if (child->view) {
			continue;
		}
		child->pending.x = area.x;
		child->pending.y = area.y;
		child->pending.width = area.width;
		child->pending.height = area.height;
		node_set_dirty(&child->node);
		cover(child->pending.children, area);
	}
}

bool tide_arrange_workspace(struct sway_workspace *ws) {
	enum tide_layout lt = tide_workspace_layout(ws);
	if (lt == TIDE_NONE) {
		return false;
	}
	struct tide_workspace *t = &ws->tide;
	struct wlr_box area;
	workspace_get_box(ws, &area);
	cover(ws->tiling, area);
	list_t *leaves = create_list();
	collect(ws->tiling, leaves, false);
	if (leaves->length == 1) {
		// A lone window has the single-window width, in any layout
		struct wlr_box box = single_box(t, area);
		place(leaves, &box, 1);
	} else if (leaves->length) {
		switch (lt) {
		case TIDE_TILE:
			tile(t, area, leaves);
			break;
		case TIDE_THREECOL:
			threecol(t, area, leaves);
			break;
		case TIDE_TWOCOL:
			twocol(t, area, leaves);
			break;
		case TIDE_MONOCLE:
			for (int i = 0; i < leaves->length; i++) {
				resize(leaves->items[i], area);
			}
			break;
		case TIDE_NONE:
		case TIDE_LAYOUTS:
			break;
		}
	}
	list_free(leaves);
	return true;
}

bool tide_arrange_container(struct sway_container *con) {
	struct sway_workspace *ws = con->pending.workspace;
	/* A fullscreen container, and what's in it, keeps sway's arrangement:
	 * arrange_workspace arranges it with arrange_container, so sending it
	 * back there would never end */
	if (con->view || !ws || container_is_floating_or_child(con)
			|| container_is_fullscreen_or_child(con)
			|| tide_workspace_layout(ws) == TIDE_NONE) {
		return false;
	}
	arrange_workspace(ws);
	return true;
}

/* Makes ws's tiled windows its direct children, in tree order, so tide's
 * order is the tiling list's, dropping sway's split containers. A tide
 * layout needs no splits: they come from sway's commands, as a move up
 * on a row, which tide lays out by their windows' order. */
void tide_flatten(struct sway_workspace *ws) {
	bool nested = false;
	for (int i = 0; i < ws->tiling->length; i++) {
		struct sway_container *child = ws->tiling->items[i];
		nested |= !child->view;
	}
	if (!nested) {
		return;
	}
	list_t *leaves = create_list(), *splits = create_list();
	collect(ws->tiling, leaves, true);
	// container_detach() lets go of a global fullscreen, which stays
	struct sway_container *global = root->fullscreen_global;
	for (int i = 0; i < leaves->length; i++) {
		struct sway_container *leaf = leaves->items[i];
		struct sway_container *parent = leaf->pending.parent;
		container_detach(leaf);
		// Its split containers, emptied, innermost first
		while (parent && !parent->pending.children->length) {
			struct sway_container *up = parent->pending.parent;
			list_add(splits, parent);
			container_detach(parent);
			parent = up;
		}
	}
	for (int i = 0; i < splits->length; i++) {
		container_begin_destroy(splits->items[i]);
	}
	for (int i = 0; i < leaves->length; i++) {
		workspace_insert_tiling_direct(ws, leaves->items[i], i);
		if (leaves->items[i] == global) {
			root->fullscreen_global = global;
		}
	}
	list_free(leaves);
	list_free(splits);
}

/* How many of ws's tiled windows come first as masters, as its layout
 * arranges them: monocle's first counting as one, threecol's at least
 * one, twocol's one to 15 */
static int masters(struct sway_workspace *ws) {
	struct tide_workspace *t = &ws->tide;
	switch (tide_workspace_layout(ws)) {
	case TIDE_MONOCLE:
		return 1;
	case TIDE_THREECOL:
		return TIDE_MAX(t->nmaster, 1);
	case TIDE_TWOCOL:
		return TIDE_MIN(TIDE_MAX(t->nmaster, 1), 15);
	default:
		return t->nmaster;
	}
}

struct sway_container *tide_add_new(struct sway_workspace *ws,
		struct sway_container *con, struct sway_container *focused) {
	if (tide_workspace_layout(ws) == TIDE_NONE) {
		return NULL;
	}
	int rule = config->tide.new_window >= 0 ? config->tide.new_window : TIDE_NEW_END;
	if (con->pending.workspace) {
		container_detach(con);
	}
	// The window itself goes in, a direct child, so the tree stays flat
	tide_flatten(ws);
	list_t *tiling = ws->tiling;
	int to = tiling->length;
	if (rule == TIDE_NEW_MASTER) {
		to = 0;
	} else if (rule == TIDE_NEW_TOP) {
		// Before the stack's first window, or with no stack, last
		int want = masters(ws), seen = 0;
		for (int i = 0; i < tiling->length; i++) {
			if (!tiles(tiling->items[i])) {
				continue;
			}
			if (seen++ == want) {
				to = i;
				break;
			}
		}
	} else if (rule == TIDE_NEW_NEXT && focused && focused != con) {
		int f = list_find(tiling, focused);
		if (f >= 0) {
			to = f + 1;
		}
	}
	workspace_insert_tiling_direct(ws, con, to);
	// A split container moved in gives its windows that place, in order
	tide_flatten(ws);
	return con;
}

void tide_normalize(struct sway_workspace *ws) {
	/* A tide layout keeps its windows direct children of the workspace,
	 * with no tabs or stacks around them: sway's split containers go, and
	 * a tabbed or stacked workspace is a row, as sway's layouts again */
	tide_flatten(ws);
	if (ws->layout == L_TABBED || ws->layout == L_STACKED) {
		ws->layout = ws->prev_split_layout != L_NONE ? ws->prev_split_layout : L_HORIZ;
	}
}

void tide_leave(struct sway_workspace *ws) {
	if (tide_workspace_layout(ws) == TIDE_NONE) {
		return;
	}
	ws->tide.off = true;
	// sway's layout command arranges only a layout that changes
	arrange_workspace(ws);
	ipc_event_layout(ws);
}

/* Keeps old's mfact and nmaster, and takes those lt had, or its defaults:
 * each layout keeps its own */
static void lt_switch(struct tide_workspace *t, enum tide_layout old, enum tide_layout lt) {
	if (old == lt) {
		return;
	}
	if (old != TIDE_NONE) {
		t->modes[old].set = true;
		t->modes[old].mfact = t->mfact;
		t->modes[old].nmaster = t->nmaster;
	}
	if (t->modes[lt].set) {
		t->mfact = t->modes[lt].mfact;
		t->nmaster = t->modes[lt].nmaster;
		return;
	}
	mode_defaults(lt, &t->mfact, &t->nmaster);
}

void tide_set_layout(struct sway_workspace *ws, enum tide_layout lt) {
	struct tide_workspace *t = settled(ws);
	enum tide_layout old = t->lt[t->sellt];
	t->settled = true;
	t->off = false;
	tide_normalize(ws);
	if (lt == TIDE_NONE || lt != t->lt[t->sellt]) {
		t->sellt ^= 1;
	}
	if (lt != TIDE_NONE) {
		t->lt[t->sellt] = lt;
	}
	if (t->lt[t->sellt] == TIDE_NONE) {
		// Nothing yet to toggle back to: tidewm's floating, which sway hasn't
		t->sellt ^= 1;
	}
	lt_switch(t, old, t->lt[t->sellt]);
	arrange_workspace(ws);
	ipc_event_layout(ws);
}

enum tide_layout tide_cycle(struct sway_workspace *ws, int step) {
	static const enum tide_layout tides[] = { TIDE_TILE, TIDE_THREECOL, TIDE_TWOCOL };
	const enum tide_layout *cycle = tides;
	int n = sizeof(tides) / sizeof(tides[0]);
	if (config->tide.ncycle) {
		cycle = config->tide.cycle;
		n = config->tide.ncycle;
	}
	struct tide_workspace *t = settled(ws);
	enum tide_layout from = t->off ? TIDE_NONE : t->lt[t->sellt];
	if (from == TIDE_MONOCLE) {
		// From monocle, it goes from the layout before, as tide's does
		from = t->lt[t->sellt ^ 1];
	}
	int at = n;
	for (int i = 0; i < n && at == n; i++) {
		if (from == cycle[i]) {
			at = i;
		}
	}
	if (at == n) {
		return cycle[step > 0 ? 0 : n - 1];
	}
	return cycle[(at + n + (step % n + n)) % n];
}

struct sway_container *tide_zoom(struct sway_container *con) {
	struct sway_workspace *ws = con->pending.workspace;
	if (!con->view || !ws || container_is_floating_or_child(con)
			|| tide_workspace_layout(ws) == TIDE_NONE) {
		return NULL;
	}
	tide_flatten(ws);
	// The first tiled window that isn't con, con going NULL if it's passed
	struct sway_container *sel = con, *c = NULL;
	for (int i = 0; i < ws->tiling->length; i++) {
		struct sway_container *t = ws->tiling->items[i];
		if (t != sel) {
			c = t;
			break;
		}
		sel = NULL;
	}
	if (!c) {
		return NULL; // con is the only tiled window
	}
	if (!sel) {
		sel = c;
	}
	list_del(ws->tiling, list_find(ws->tiling, sel));
	list_insert(ws->tiling, 0, sel);
	node_set_dirty(&ws->node);
	arrange_workspace(ws);
	return sel;
}

const char *tide_movestack(struct sway_container *con, int step) {
	struct sway_workspace *ws = con->pending.workspace;
	if (!ws || tide_workspace_layout(ws) == TIDE_NONE) {
		return "sway's layouts have no stack";
	}
	if (!con->view || container_is_floating_or_child(con) || !tiles(con)) {
		return "A floating or fullscreen window isn't in the stack";
	}
	tide_flatten(ws);
	list_t *tiling = ws->tiling;
	int at = list_find(tiling, con), first = -1, last = -1, found = -1;
	bool passed = false;
	for (int i = 0; i < tiling->length; i++) {
		if (!tiles(tiling->items[i])) {
			continue;
		}
		if (i == at) {
			passed = true;
		} else if (step > 0 && passed && found < 0) {
			found = i;
		} else if (step < 0 && !passed) {
			found = i;
		}
		first = first < 0 ? i : first;
		last = i;
	}
	if (at < 0 || first == last) {
		return NULL;
	}
	if (found >= 0) {
		list_swap(tiling, at, found);
	} else if (step > 0) {
		// From the end, round to the top
		list_del(tiling, at);
		list_insert(tiling, first, con);
	} else {
		list_del(tiling, at);
		list_insert(tiling, last, con);
	}
	node_set_dirty(&ws->node);
	arrange_workspace(ws);
	return NULL;
}

static bool same_defaults(struct tide_config *a, struct tide_config *b) {
	for (int i = 0; i < TIDE_MODES; i++) {
		if (a->layout_defaults[i].has_mfact != b->layout_defaults[i].has_mfact
				|| a->layout_defaults[i].has_nmaster != b->layout_defaults[i].has_nmaster
				|| (a->layout_defaults[i].has_mfact
					&& a->layout_defaults[i].mfact != b->layout_defaults[i].mfact)
				|| (a->layout_defaults[i].has_nmaster
					&& a->layout_defaults[i].nmaster != b->layout_defaults[i].nmaster)) {
			return false;
		}
	}
	return true;
}

static void reset_modes(struct sway_workspace *ws, void *data) {
	struct tide_workspace *t = &ws->tide;
	memset(t->modes, 0, sizeof(t->modes));
	mode_defaults(t->lt[t->sellt], &t->mfact, &t->nmaster);
}

void tide_config_reloaded(struct sway_config *old, struct sway_config *new) {
	if (!same_defaults(&old->tide, &new->tide)) {
		root_for_each_workspace(reset_modes, NULL);
	}
}

int tide_edges(struct sway_workspace *ws, struct tide_edge out[2]) {
	enum tide_layout lt = tide_workspace_layout(ws);
	if (lt == TIDE_NONE) {
		return 0;
	}
	struct tide_workspace *t = &ws->tide;
	struct wlr_box area, cols[16];
	workspace_get_box(ws, &area);
	list_t *leaves = create_list();
	collect(ws->tiling, leaves, false);
	int n = leaves->length;
	list_free(leaves);
	if (n == 0) {
		return 0;
	}
	if (n == 1) {
		struct wlr_box box = single_box(t, area);
		out[0] = (struct tide_edge){ box.x, TIDE_EDGE_LONE, false };
		out[1] = (struct tide_edge){ box.x + box.width, TIDE_EDGE_LONE, true };
		return 2;
	}
	if (lt == TIDE_TILE) {
		if (t->nmaster <= 0 || n <= t->nmaster) {
			return 0;
		}
		// As tile() rounds the masters' width
		out[0] = (struct tide_edge){ area.x + (int)roundf((float)area.width * t->mfact),
			TIDE_EDGE_SPLIT, false };
		return 1;
	}
	if (lt == TIDE_THREECOL) {
		int nm = TIDE_MIN(TIDE_MAX(t->nmaster, 1), n);
		if (n - nm == 0) {
			return 0;
		}
		if (n - nm == 1) {
			columns(area, (double[]){ t->mfact, 1 - t->mfact }, 2, cols);
			out[0] = (struct tide_edge){ cols[1].x, TIDE_EDGE_SPLIT, false };
			return 1;
		}
		columns(area, (double[]){ (1 - t->mfact) / 2, t->mfact, (1 - t->mfact) / 2 },
			3, cols);
		out[0] = (struct tide_edge){ cols[1].x, TIDE_EDGE_CENTER, false };
		out[1] = (struct tide_edge){ cols[2].x, TIDE_EDGE_CENTER, true };
		return 2;
	}
	if (lt == TIDE_TWOCOL) {
		int nm = TIDE_MIN(TIDE_MAX(t->nmaster, 1), TIDE_MIN(n, 15));
		double w[16];
		if (nm == n) {
			return 0;
		}
		// twocol's own weights, so the edge rounds as its columns do
		for (int i = 0; i < nm; i++) {
			w[i] = t->mfact / nm;
		}
		w[nm] = 1 - t->mfact;
		columns(area, w, nm + 1, cols);
		out[0] = (struct tide_edge){ cols[nm].x, TIDE_EDGE_SPLIT, false };
		return 1;
	}
	return 0;
}

bool tide_follow(struct sway_workspace *ws, const struct tide_edge *edge, double x) {
	struct tide_workspace *t = &ws->tide;
	struct wlr_box area;
	workspace_get_box(ws, &area);
	if (area.width <= 0) {
		return false;
	}
	double v;
	if (edge->kind == TIDE_EDGE_SPLIT) {
		v = (x - area.x) / area.width;
	} else {
		/* A centered master's side, or a lone window's: the edge's distance
		 * from the center is half its width. Past the center, it stays at
		 * its narrowest. */
		v = 2 * (x - (area.x + area.width / 2.0)) / area.width;
		if (!edge->right) {
			v = -v;
		}
	}
	if (edge->kind == TIDE_EDGE_LONE) {
		v = TIDE_MAX(0.1, TIDE_MIN(v, 1));
		if (v == t->single) {
			return false;
		}
		t->single = v;
		return true;
	}
	float f = (float)TIDE_MAX(0.1, TIDE_MIN(v, 0.9));
	if (f == t->mfact) {
		return false;
	}
	t->mfact = f;
	return true;
}

const char *tide_resize(struct sway_container *con, bool horizontal, double amount,
		bool pixels) {
	struct sway_workspace *ws = con->pending.workspace;
	struct tide_workspace *t = &ws->tide;
	if (!workspace_is_visible(ws)) {
		return "The window's workspace isn't shown";
	}
	if (!horizontal) {
		return "A tile's height is its stack's equal share";
	}
	list_t *leaves = create_list();
	collect(ws->tiling, leaves, false);
	int at = list_find(leaves, con), n = leaves->length;
	list_free(leaves);
	if (at < 0 || n < 2 || tide_workspace_layout(ws) == TIDE_MONOCLE) {
		return "This tile has no master/stack edge to move";
	}
	struct wlr_box area;
	workspace_get_box(ws, &area);
	double delta = pixels ? amount / (area.width > 0 ? area.width : 1) : amount / 100.0;
	// A master's grows the masters' column; a stack window's, the stack's
	double f = t->mfact + (at < masters(ws) ? delta : -delta);
	float clamped = (float)TIDE_MAX(0.1, TIDE_MIN(f, 0.9));
	if (clamped == t->mfact) {
		return "The tile can't go further";
	}
	t->mfact = clamped;
	arrange_workspace(ws);
	return NULL;
}
