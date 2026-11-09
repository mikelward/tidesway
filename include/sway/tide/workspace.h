#ifndef _SWAY_TIDE_WORKSPACE_H
#define _SWAY_TIDE_WORKSPACE_H

/*
 * Workspaces that span every output, as tidewm has them (its workspace.h,
 * tide's SPEC §6.5): a workspace has a name, and each output shows its own
 * part of it, the windows on that output. One workspace is current, on
 * every output at once, and switching to another switches every output.
 * A workspace is made the first time it's named and goes once it's empty
 * and not current, unless it's persistent.
 *
 * Here a workspace is a group of sway's workspaces, one per output, all of
 * one name: its parts. Each part keeps sway's tree and its own tide
 * layouts. GET_WORKSPACES lists each workspace once, as its part on the
 * focused output; the workspace events are the workspace's, not a part's;
 * ext-workspace has one group, on every output, holding one handle per
 * workspace. An output that's disabled gives each part's windows to the
 * part of the same workspace on the first output left.
 */

#include <json-c/json.h>
#include <stdbool.h>
#include <stdint.h>

// sway's code that moves a workspace to another output stays out of the way
#define TIDE_WS_SPANNING true

struct cmd_results;
struct sway_output;
struct sway_seat;
struct sway_workspace;
struct wlr_ext_workspace_group_handle_v1;

// A workspace's own state, which each of its parts points to
struct tide_group;

// A part was made, by workspace_create: it joins its workspace, which
// gets a part on every other output
void tide_ws_created(struct sway_workspace *ws);

// A part is going: the last takes its workspace with it
void tide_ws_destroying(struct sway_workspace *ws);

// workspace_consider_destroy: the workspace goes, every part of it, once
// nothing keeps it
void tide_ws_consider_destroy(struct sway_workspace *ws);

// The part of ws's workspace on output, or ws if it has none there
struct sway_workspace *tide_ws_part(struct sway_workspace *ws,
	struct sway_output *output);

// The part of ws's workspace on the focused output, as commands name it
struct sway_workspace *tide_ws_here(struct sway_workspace *ws);

/* Focus is going to ws: every other output shows its part of ws's
 * workspace, keeping the window last focused in each, and its sticky
 * windows come along */
void tide_ws_show(struct sway_seat *seat, struct sway_workspace *ws);

// The workspace step along from ws's, in tidewm's order, going round: its
// part on ws's output
struct sway_workspace *tide_ws_step(struct sway_workspace *ws, int step);

// An output was enabled: it gets a part of every workspace, showing the
// current one's
void tide_ws_output_enabled(struct sway_output *output);

/* An output is being disabled: each part's windows go to the part of the
 * same workspace on the first output left, and the parts go. Returns
 * false, doing nothing, with no output left, for sway to park them. */
bool tide_ws_evacuate(struct sway_output *output);

// ws's part's urgency changed: its workspace's, by every part's
void tide_ws_urgency(struct sway_workspace *ws);

// ws was renamed: so is every other part of its workspace
void tide_ws_renamed(struct sway_workspace *ws);

// Whether a workspace event from sway's code goes out: the workspace's
// own, not a part's, as tidewm sends them
bool tide_ws_event_wanted(struct sway_workspace *old,
	struct sway_workspace *new, const char *change);

// GET_WORKSPACES: f for each workspace, in the order made, as its part on
// the focused output
void tide_ws_for_each(void (*f)(struct sway_workspace *ws, void *data),
	void *data);

// Whether ws is a part of a persistent workspace, which an output going
// leaves, empty or not
bool tide_ws_persistent(struct sway_workspace *ws);

// A workspace's IPC: its workspace's urgency and persistence
void tide_ws_describe(struct sway_workspace *ws, json_object *object);

// ext-workspace's one group, on every output
struct wlr_ext_workspace_group_handle_v1 *tide_ws_ext_group(uint32_t caps);

#endif
