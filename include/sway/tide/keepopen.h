#ifndef _SWAY_TIDE_KEEPOPEN_H
#define _SWAY_TIDE_KEEPOPEN_H

/*
 * Windows kept open, as tidewm has them (its docs/ipc.md), for tide's
 * pinned windows (its SPEC §6.5): keep_open enable|disable|toggle. kill,
 * from a key or the IPC, leaves the window open, and says so with a
 * close_refused window event, for a shell to say why. While the keyboard
 * is in it, its bindings made with --kept-open count. It's kept open on
 * the workspace it's on: on any other, however it got there, it isn't,
 * and minimized it closes as any other. A window that maps again is a new
 * one, not kept open. A keep_open window event says when the command
 * changes it. The window's own close button still closes it.
 */

#include <json-c/json.h>
#include <stdbool.h>

struct cmd_results;
struct sway_container;
struct sway_seat;
struct sway_view;

// Whether con is kept open now: on its workspace, and not minimized
bool tide_kept_open(struct sway_container *con);

// Whether the seat's focused window is kept open, for --kept-open bindings
bool tide_kept_open_focused(struct sway_seat *seat);

// view_unmap's: mapped again, it's a new window
void tide_keep_open_unmapped(struct sway_view *view);

// GET_TREE's keep_open
void tide_keep_open_describe(struct sway_container *con, json_object *object);

struct cmd_results *cmd_keep_open(int argc, char **argv);

#endif
