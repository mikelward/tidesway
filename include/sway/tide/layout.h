#ifndef _SWAY_TIDE_LAYOUT_H
#define _SWAY_TIDE_LAYOUT_H

/*
 * tide's layouts, as tidewm has them (its layout.h): tile, threecol,
 * twocol and monocle, each workspace with its own layout, mfact and
 * nmaster, and each layout its own mfact and nmaster there. A workspace
 * laid out by tide's rules places its tiled windows, in tree order, as
 * tidewm places a monitor's clients; one with tide's layout off keeps
 * sway's split tree.
 */

#include <stdbool.h>

struct sway_config;
struct sway_container;
struct sway_workspace;

enum tide_layout {
	TIDE_NONE = -1,
	TIDE_TILE,
	TIDE_THREECOL,
	TIDE_TWOCOL,
	TIDE_MONOCLE,
	TIDE_LAYOUTS,
};

// The layouts with an mfact and nmaster of their own: tile, threecol, twocol
#define TIDE_MODES 3

enum tide_new_window {
	TIDE_NEW_END,
	TIDE_NEW_TOP,
	TIDE_NEW_NEXT,
	TIDE_NEW_MASTER,
};

struct tide_single_rule {
	double min_aspect;
	double width;
};

#define TIDE_MAX_SINGLES 16

// A layout_defaults line's settings
struct tide_layout_defaults {
	bool has_mfact, has_nmaster;
	float mfact;
	int nmaster;
};

// The config file's layout lines (tidewm's docs/config.md)
struct tide_config {
	struct tide_single_rule singles[TIDE_MAX_SINGLES]; // single_window
	int nsingles; // 0: tide's own rules
	double ultrawide_aspect; // 0: not given
	enum tide_layout first_layout[2]; // default_layout normal|ultrawide
	struct tide_layout_defaults layout_defaults[TIDE_MODES];
	enum tide_layout cycle[TIDE_MODES]; // layout_cycle
	int ncycle; // 0: tide's own
	int new_window; // enum tide_new_window; -1: not given
	int resize_grab; // -1: not given
};

// A workspace's layouts, held in its struct sway_workspace
struct tide_workspace {
	bool off; // sway's layouts instead
	bool settled; // its defaults are chosen, by its output's aspect
	enum tide_layout lt[2]; // the layout now and the one to toggle to
	int sellt;
	float mfact;
	int nmaster;
	double single; // its lone window's width of its own; 0: the rules'
	struct {
		bool set;
		float mfact;
		int nmaster;
	} modes[TIDE_LAYOUTS]; // each layout's own, kept while another's on
};

void tide_config_defaults(struct tide_config *tc);

/* After a reload: a change to layout_defaults starts every workspace's
 * layouts from them again, each keeping its layout */
void tide_config_reloaded(struct sway_config *old, struct sway_config *new);

void tide_workspace_init(struct sway_workspace *ws);

// ws's layout, or TIDE_NONE for sway's layouts
enum tide_layout tide_workspace_layout(struct sway_workspace *ws);

const char *tide_layout_name(enum tide_layout lt);

// ws's layout symbol, as tidewm's layout event has it, into buf
void tide_layout_symbol(struct sway_workspace *ws, char *buf, int size);

/* Lays out ws's tiled windows by its tide layout, returning false for a
 * workspace in sway's layouts */
bool tide_arrange_workspace(struct sway_workspace *ws);

/* For arrange_container: a split container on a workspace in a tide layout
 * is arranged with the workspace. Returns whether it was. */
bool tide_arrange_container(struct sway_container *con);

/* Puts a new tiled window on ws, where new_window says, after focused, the
 * container that had focus there. Returns it, as workspace_add_tiling
 * does, or NULL for sway's layouts, where sway places it. */
struct sway_container *tide_add_new(struct sway_workspace *ws,
		struct sway_container *con, struct sway_container *focused);

/* Makes ws's tiled windows its direct children, in tree order, dropping
 * sway's split containers */
void tide_flatten(struct sway_workspace *ws);

/* tide_flatten, and a tabbed or stacked workspace made a row, so no title
 * bars take room a tide layout doesn't give them */
void tide_normalize(struct sway_workspace *ws);

// Turns ws's tide layout off, for sway's split commands and layouts
void tide_leave(struct sway_workspace *ws);

// Sets ws's layout, as tidewm's setlayout: TIDE_NONE toggles
void tide_set_layout(struct sway_workspace *ws, enum tide_layout lt);

// The layout `layout next` (step 1) or `prev` (-1) goes to from ws's
enum tide_layout tide_cycle(struct sway_workspace *ws, int step);

/* zoom: con, or the next tile if it's the master, becomes the master.
 * Returns the window that did, or NULL. */
struct sway_container *tide_zoom(struct sway_container *con);

// movestack: swaps con with the next tile, or the previous one. An error, or NULL.
const char *tide_movestack(struct sway_container *con, int step);

/* resize grow|shrink width for tile con on a visible workspace, as
 * tidewm's: amount, in percentage points or pixels of the window area,
 * moves the master/stack edge, a master's growing the masters' column and
 * a stack window's the stack. Heights are the stack's equal shares, so
 * they fail. Returns an error, or NULL. */
const char *tide_resize(struct sway_container *con, bool horizontal, double amount,
	bool pixels);

// An edge a drag moves (sway/tide/split.c), as tidewm's split.h has them
enum tide_edge_kind {
	TIDE_EDGE_SPLIT, // between the masters and the stack: mfact is its share
	TIDE_EDGE_CENTER, // a side of threecol's centered master
	TIDE_EDGE_LONE, // a side of a lone window
};

struct tide_edge {
	int x; // in layout coordinates
	enum tide_edge_kind kind;
	bool right; // TIDE_EDGE_CENTER, TIDE_EDGE_LONE: the right side
};

// ws's edges as its layout places its tiles now, into out. Returns how many.
int tide_edges(struct sway_workspace *ws, struct tide_edge out[2]);

/* The pointer at x, dragging ws's edge: sets the mfact, or the lone
 * window's width, that puts the edge there. Returns whether it changed. */
bool tide_follow(struct sway_workspace *ws, const struct tide_edge *edge, double x);

#endif
