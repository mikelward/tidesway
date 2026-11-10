#ifndef _SWAY_TIDE_MINIMIZE_H
#define _SWAY_TIDE_MINIMIZE_H

/*
 * Minimized windows, as tidewm has them (its docs/ipc.md, tide's SPEC
 * §6.7): minimize [enable|disable|toggle] hides a window, which leaves the
 * layout, and focus on it goes to the window you were in before. It stays
 * on its workspace, which stays while it waits, and keeps its states, a
 * fullscreen one's included. Focusing it by any means brings it back
 * first, as minimize disable does: it joins the end of its part's stack,
 * as an arriving window does. An app's own minimize request does nothing.
 */

#include <json-c/json.h>
#include <stdbool.h>
#include "list.h"

struct cmd_results;
struct sway_container;
struct sway_node;

// Whether con is a minimized window
bool tide_minimized(struct sway_container *con);

/* seat_set_focus's: a minimized window focus goes to comes back first */
void tide_minimize_focusing(struct sway_node *node);

/* sway's layouts': children less the minimized ones, which take no room.
 * Returns children itself when none is; free what else it returns. */
list_t *tide_minimize_shown(list_t *children);

// GET_TREE's minimized, and a minimized fullscreen window's fullscreen_mode
void tide_minimize_describe(struct sway_container *con, json_object *object);

struct cmd_results *cmd_minimize(int argc, char **argv);

// view_unmap's: a window that maps again is a new one, not minimized
void tide_minimize_unmapped(struct sway_view *view);

#endif
