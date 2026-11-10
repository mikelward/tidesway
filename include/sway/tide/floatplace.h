#ifndef _SWAY_TIDE_FLOATPLACE_H
#define _SWAY_TIDE_FLOATPLACE_H

/*
 * Floating windows' places, remembered per app for the session, as tidewm
 * has them (its floating.h, tide's SPEC §6.4): wherever one of an app's
 * floating windows last opened, moved or was resized to, size and
 * position, floating a window of that app by hand puts it, kept in its
 * output's window area. A redraw where a window already is doesn't count.
 * With no place yet, a window floated by hand stays where it was tiled.
 * Dialogs, fixed-size and modal windows and picture-in-picture place
 * themselves, so theirs isn't kept, and they don't take the app's.
 */

#include <stdbool.h>

struct sway_container;
struct wlr_box;

/* The floating command's, once con floats by hand: it goes to its app's
 * place, or stays where it was tiled, at tiled */
void tide_floatplace_by_hand(struct sway_container *con, struct wlr_box *tiled);

/* transaction_commit_dirty's: each of an app's floating windows that moved
 * or changed size since it was last seen is now its app's place */
void tide_floatplace_remember(void);

#endif
