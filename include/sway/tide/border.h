#ifndef _SWAY_TIDE_BORDER_H
#define _SWAY_TIDE_BORDER_H

/*
 * Borders as tidewm has them (its docs/config.md): default_border is every
 * window's, tiled and floating, none or pixel and a width from 0 to 100,
 * 1 without one, as tidewm has no title bars; a reload gives every window
 * the new one at once, a floating window keeping its place and size,
 * border and all; and no window draws its own decorations in place of
 * one. A client.CLASS line's colors are checked as tidewm checks them,
 * failing in its words, and a window that's focused_inactive has
 * unfocused's colors unless that line is given.
 */

#include <stdbool.h>

struct cmd_results;

/* Where sway lets a floating window draw its own decorations in place of
 * a border (its border B_CSD, its xdg-decoration client-side), it's
 * guarded by !TIDE_NO_CSD, so sway's code stays as it was */
#define TIDE_NO_CSD 1

// The width default_border pixel gives without one: tidewm's config.h's borderpx
#define TIDE_BORDER_WIDTH 1

/* default_border, as tidewm takes it, for sway's command: the border of
 * every window, tiled or floating */
struct cmd_results *tide_default_border(int argc, char **argv);

/* A client.CLASS line's colors, each checked as it comes, then how many
 * there are: an error in tidewm's words, or NULL */
struct cmd_results *tide_border_check_colors(const char *name, int argc, char **argv);

// Every window's border is the config's again, after a reload
void tide_border_reloaded(void);

#endif
