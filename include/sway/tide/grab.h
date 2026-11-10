#ifndef _SWAY_TIDE_GRAB_H
#define _SWAY_TIDE_GRAB_H

/*
 * Focus grabs, over hyprland-focus-grab-v1, as tidewm has them (its
 * grab.h), which Quickshell's HyprlandFocusGrab speaks: tide's
 * notification center closes on a click outside it, but not when the
 * pointer crosses away, as tide's SPEC §9 has it. While a grab holds, the
 * keyboard stays on its surfaces and the pointer enters only them. A click
 * anywhere else ends it and goes no further, as a menu's does. So does
 * focus you move, by a binding or a command, the launcher opening, or the
 * lock; a window opening doesn't take the keyboard meanwhile. A second
 * grab starting ends the first, and one can't start under the lock.
 *
 * A layer surface that takes the keyboard on demand, such as a
 * notification's reply field, holds it the same way once you've clicked
 * it (tide's SPEC §14.1): until you move focus, by a click elsewhere, a
 * binding or a command, or an exclusive layer surface, such as the
 * launcher, or the lock takes it. The pointer crossing it doesn't give it
 * the keyboard.
 */

#include <stdbool.h>
#include <stdint.h>

struct sway_node;
struct sway_seat;
struct wlr_layer_surface_v1;
struct wlr_surface;

// server_init's: offers hyprland_focus_grab_manager_v1
bool tide_grab_init(void);

/* Whether global is the focus grab manager, which only clients outside a
 * sandbox may bind, as a layer shell's */
struct wl_global;
bool tide_grab_is_global(const struct wl_global *global);

/* Focus moving because you asked, by a command or a binding: nest these
 * around it, so it ends a grab rather than being held back by it */
void tide_grab_ask_begin(void);
void tide_grab_ask_end(void);

/* A button press over surface, around seatop's handling of it. Returns
 * whether it ended a grab, which eats it: then it goes no further. A
 * press inside a grab that has the keyboard leaves the keyboard there.
 * These three leave a grab alone for a seat it doesn't hold. */
bool tide_grab_press_begin(struct sway_seat *seat, struct wlr_surface *surface,
	uint32_t button);
void tide_grab_press_end(void);
/* A touch going down over surface: whether it ended a grab, which eats it,
 * as a press outside one does */
bool tide_grab_touch(struct sway_seat *seat, struct wlr_surface *surface);

// Whether a button's release is of the press that ended a grab, and goes nowhere too
bool tide_grab_release(struct sway_seat *seat, uint32_t button);

/* seat_set_focus's: whether focus stays where it is rather than going to
 * node (NULL for none), as a grab or an on-demand layer surface holds it */
bool tide_grab_keeps(struct sway_seat *seat, struct sway_node *node);

/* seat_set_focus_layer's: whether focus stays where it is rather than
 * going to layer; an exclusive layer surface, such as the launcher, ends
 * a grab and takes it */
bool tide_grab_keeps_layer(struct sway_seat *seat, struct wlr_layer_surface_v1 *layer);

/* seat_set_focus_surface's: whether the keyboard stays where it is rather
 * than going to surface, given it directly, as an X11 window's menu is */
bool tide_grab_keeps_surface(struct sway_seat *seat, struct wlr_surface *surface);

// Whether a grab holds seat's input: the default seat's, as tidewm has one
bool tide_grab_holds(struct sway_seat *seat);

// Whether the pointer may enter surface: always, unless a grab holds without it
bool tide_grab_accepts(struct wlr_surface *surface);

// The session locking ends a grab
void tide_grab_locked(void);

#endif
