#ifndef _SWAY_TIDE_DIRECTION_H
#define _SWAY_TIDE_DIRECTION_H

/*
 * focus and move left|right|up|down for a tile in a tide layout, by
 * tidewm's rule (its direction.h, tide's SPEC §6.6): of the windows whose
 * center is that way, the one sharing the most height (the most width,
 * going up or down), then the nearest, then the first in tiled order.
 * Past an output's edge it goes on to the next output that way, to the
 * window at the edge it comes in by; past the last output, nowhere.
 */

#include <stdbool.h>
#include <wlr/types/wlr_output_layout.h>

struct sway_container;
struct sway_node;

// Whether con, a tile in a tide layout, goes by tide's rule
bool tide_direction_applies(struct sway_container *con);

/* The node focus goes to from con going dir: a window, or the next
 * output's workspace if it shows none, or NULL past the last output */
struct sway_node *tide_focus_in_direction(struct sway_container *con,
	enum wlr_direction dir);

/* focus next (step 1) or prev (-1) from con on a workspace in a tide
 * layout, as tidewm's focusstack: the next window there in tiled order,
 * then its floating ones, going round; con itself if it's alone, or NULL
 * if con is fullscreen. *handled says whether tide's rule applies. */
struct sway_container *tide_focus_next(struct sway_container *con, int step,
	bool *handled);

/* Moves tile con dir: swaps it with the tile that way on its output, or,
 * past the edge, puts it on the next output's workspace in the place of
 * the tile at the edge it comes in by. Returns an error, or NULL. */
const char *tide_move_in_direction(struct sway_container *con,
	enum wlr_direction dir);

#endif
