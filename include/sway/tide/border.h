#ifndef _SWAY_TIDE_BORDER_H
#define _SWAY_TIDE_BORDER_H

/*
 * Borders as tidewm has them (its docs/config.md): a client.CLASS line's
 * colors are checked as tidewm checks them, failing in its words, and a
 * window that's focused_inactive has unfocused's colors unless that line
 * is given.
 */

struct cmd_results;

/* A client.CLASS line's colors, each checked as it comes, then how many
 * there are: an error in tidewm's words, or NULL */
struct cmd_results *tide_border_check_colors(const char *name, int argc, char **argv);

#endif
