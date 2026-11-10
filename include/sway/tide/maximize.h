#ifndef _SWAY_TIDE_MAXIMIZE_H
#define _SWAY_TIDE_MAXIMIZE_H

/*
 * Maximized windows, as tidewm has them (its docs/ipc.md, tide's SPEC
 * §6.3): maximize [enable|disable|toggle] makes the window cover its
 * output's window area, above the other tiles, which stay where they are,
 * with the bar still showing; alone, it toggles. It lasts until it's
 * turned off, the window closes, or another window on its workspace takes
 * focus (not a dialog of it). An app's own request maximizes it too.
 * Fullscreen covers it while it lasts, and a floating window gets its
 * place back after.
 */

#include <json-c/json.h>
#include <stdbool.h>

struct cmd_results;
struct sway_container;
struct sway_node;
struct sway_view;
struct sway_workspace;
struct wlr_box;

bool tide_maximized(struct sway_container *con);

/* arrange_workspace's, once its windows are laid out: each maximized
 * window on ws covers area */
void tide_maximize_arrange(struct sway_workspace *ws, struct wlr_box *area);

// Whether a maximized tile covers ws's other tiles
bool tide_maximize_covers(struct sway_workspace *ws);

/* The seat's focus went to node: a maximized window on its workspace that
 * isn't it, nor its dialog's parent, isn't maximized now */
void tide_maximize_focused(struct sway_node *node);

/* The app asks to be maximized or not, mapped or as it maps (view_map's) */
void tide_maximize_request(struct sway_view *view, bool maximized);

/* view_map's, for an app that asked before it mapped: maximized, and a
 * floating one's place after is the middle of the window area */
void tide_maximize_opening(struct sway_view *view);

// view_unmap's: a window that maps again is a new one, not maximized
void tide_maximize_unmapped(struct sway_view *view);

// GET_TREE's maximized
void tide_maximize_describe(struct sway_container *con, json_object *object);

struct cmd_results *cmd_maximize(int argc, char **argv);

#endif
