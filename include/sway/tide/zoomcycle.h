#ifndef _SWAY_TIDE_ZOOMCYCLE_H
#define _SWAY_TIDE_ZOOMCYCLE_H
#include <stdbool.h>
#include <stdint.h>
#include <xkbcommon/xkbcommon.h>
#include "sway/input/seat.h"

/* tidewm's zoom next and zoom prev, tide's Alt+Tab (sway/tide/zoomcycle.c) */

struct wlr_keyboard;

// The keyboard and modifiers held as a binding runs, for zoom next to wait
// on; NULL and 0 after
void tide_zoomcycle_press(struct wlr_keyboard *keyboard, uint32_t mods);

// Whether a binding's key with modifiers runs the command now: without,
// zoom next and prev are zoom
bool tide_zoomcycle_pressed(void);

// zoom next (dir 1) or zoom prev (dir -1), on the seat's focus
void tide_zoomcycle_zoom(struct sway_seat *seat, int dir);

// keyboard's modifiers are now mods: those it waits on up, on the keyboard
// holding them, end it
void tide_zoomcycle_modifiers(struct sway_seat *seat,
		const struct wlr_keyboard *keyboard, uint32_t mods);

// A key pressed while it's under way: Escape ends it and is taken
bool tide_zoomcycle_key(struct sway_seat *seat, xkb_keysym_t sym);

// keyboard is going: a cycle held on it ends, with no master made
void tide_zoomcycle_keyboard_gone(const struct wlr_keyboard *keyboard);

#endif
