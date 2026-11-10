#ifndef _SWAY_TIDE_FOCUS_H
#define _SWAY_TIDE_FOCUS_H

/*
 * The focus guard, as tidewm has it (its focus.h, tide's SPEC §14):
 * nothing takes the keyboard unless you asked for it. With the guard on
 * (focus_guard enable), a new window takes focus when a grant names its
 * app or a process it descends from, when it's from the app you're in,
 * when it's a portal's dialog while a window has focus, or when it's a
 * polkit agent's prompt right after a key press. Any other opens where it
 * would, held from focus and marked urgent, until you go to it; the lone
 * window on an empty workspace too. A window with no app ID and no title
 * is a status popup: it floats, held but unmarked, and focus next, prev
 * and the arrow keys pass it by.
 *
 * A grant is one-shot. Anything you do after it (a key press, a click,
 * focus moving to another window by any means) cancels its focus, but a
 * canceled grant still opens its window on the workspace it was made on,
 * if you've left it, held and marked, as Hyprland's
 * initial_workspace_tracking does. An activation marks a window urgent
 * unless a grant names its app, guard or no guard.
 */

#include <json-c/json.h>
#include <stdbool.h>
#include <stdint.h>
#include <wlr/util/box.h>

struct cmd_results;
struct sway_container;
struct sway_node;
struct sway_seat;
struct sway_view;
struct sway_workspace;
struct wlr_surface;

enum tide_held {
	TIDE_FREE, // nothing holds it
	TIDE_WAITING, // held from focus and marked, until you go to it
	TIDE_POPUP, // a status popup: held, unmarked, passed by
};

// A window's focus state, held in its struct sway_view
struct tide_view {
	enum tide_held held;
	uint64_t focus_seq; // when it last had focus, by count; 0 for never
	int fullscreen; // enum sway_fullscreen_mode it asked for while held
	bool minimized; // tide's minimize: hidden, out of the layout
	int minimized_fullscreen; // enum sway_fullscreen_mode it had as it was minimized
	bool maximized; // tide's maximize: covering its output's window area
	double max_x, max_y, max_width, max_height; // a floating one's place before
	char *keep_open_ws; // tide's keep_open: the workspace it's kept open on
	uint64_t urgent_order; // higher for a more recent mark, 0 if unmarked
	bool urgency_hint; // an X11 window's urgency hint, as last heard
	struct sway_workspace *follow_ws; // its workspace, as its dialogs last knew it
	bool mapping; // mapping, its rules running: its events wait for its new one
	struct wlr_box place_seen; // its floating place, as its app's last saw it
};

// The config file's focus_guard lines
struct tide_focus_config {
	bool guard;
	double grant_seconds; // how long a grant holds
	double prompt_seconds; // how soon after a key press a polkit prompt may focus
};

void tide_focus_config_defaults(struct tide_focus_config *fc);

// After a reload: a notification's grant lapses by the new grant_seconds
void tide_focus_reloaded(void);

/* view_map's: before view is placed, whether it's held and where a grant
 * sends it; then, once it's placed and focused or not, its urgency and
 * its grant's workspace and focus */
void tide_focus_open(struct sway_view *view);
void tide_focus_opened(struct sway_view *view);

/* select_workspace's: the workspace a grant sends view to, made if it's
 * gone, or NULL for none */
struct sway_workspace *tide_focus_placed(struct sway_view *view);

// should_focus's: whether the guard holds view from focus
bool tide_focus_held(struct sway_view *view);

// A window with neither an app ID nor a title, which floats
bool tide_focus_nameless(struct sway_view *view);

/* container_set_fullscreen's: a window the guard holds goes fullscreen
 * once it has focus, so its request, mode, waits, or FULLSCREEN_NONE takes
 * it back. Returns whether it waits. */
bool tide_focus_defer_fullscreen(struct sway_container *con, int mode);

// Whether con is held with a fullscreen request waiting, for the fullscreen command
bool tide_focus_wants_fullscreen(struct sway_container *con);

// Whether focus next|prev and the arrow keys pass con by: a status popup
bool tide_focus_passed_by(struct sway_container *con);

/* The seat's focus went to node, a window or a workspace: a held window
 * isn't held now, and focus moving on cancels the grants */
void tide_focus_focused(struct sway_node *node);

/* Focus moving for no act of yours, as an output goes: nest these around
 * it, so it cancels nothing */
void tide_focus_quiet_begin(void);
void tide_focus_quiet_end(void);

/* The node focus goes to as the focused window closes, in place of next:
 * never a window the guard holds, which waits for you */
struct sway_node *tide_focus_after_close(struct sway_seat *seat,
	struct sway_node *next);

/* The window focus next|prev goes to on ws with no window focused: its
 * most recent one, or one the guard holds if that's all it has; NULL for
 * none */
struct sway_container *tide_focus_go(struct sway_workspace *ws);

/* A key press, or a click: you moving on. Then the surface it went to,
 * for GET_SEATS' last_input. */
void tide_focus_key(void);
void tide_focus_click(void);
void tide_focus_delivered(struct wlr_surface *surface);

/* The window at lx, ly that hasn't yet drawn itself at the place it was
 * given, for the pointer: the part of that place it hasn't drawn yet is
 * its, as GET_TREE places it, where sway's scene still has a hole, and
 * its surface and the point on it go in *surface, *sx and *sy. What it
 * has drawn keeps its input region. NULL for none. */
struct sway_container *tide_focus_unready_at(double lx, double ly,
	struct wlr_surface **surface, double *sx, double *sy);

/* An activation of mapped view: a grant naming its app focuses it, and
 * with the guard on, any other marks it urgent, returning true; with the
 * guard off, one no grant names is left to focus_on_window_activation */
bool tide_focus_activate(struct sway_view *view);

// ws was renamed: a grant made on it keeps the new name
void tide_focus_renamed(struct sway_workspace *ws);

// GET_SEATS' last_input and prompt_may_focus, added to a seat's object
void tide_focus_describe_seat(json_object *object);

// The commands: grant, and focus recent (argv after "recent")
struct cmd_results *tide_cmd_focus_recent(int argc, char **argv);

#endif
