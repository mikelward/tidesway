/*
 * send_shortcut MODS+KEY, tidewm's own, as Hyprland's sendshortcut (tidewm's
 * docs/ipc.md): sends the window a press and release of KEY, such as
 * ctrl+v, with the modifiers ctrl, shift, alt or super held, whether or
 * not it's focused, as tide's picker pastes. KEY is a key name, ignoring
 * case, sent as the key that gives it on the keyboard's layout now, at its
 * lowest level, so a shifted one, as plus on a US keyboard, wants shift
 * too. Focus stays where it was. It fails while the screen is locked.
 */
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <wlr/types/wlr_keyboard.h>
#include <wlr/types/wlr_seat.h>
#include <xkbcommon/xkbcommon.h>
#include "sway/commands.h"
#include "sway/config.h"
#include "sway/input/input-manager.h"
#include "sway/input/keyboard.h"
#include "sway/input/seat.h"
#include "sway/server.h"
#include "sway/tree/container.h"
#include "sway/tree/view.h"

// The keycode giving sym on layout, at its lowest level
struct key {
	xkb_layout_index_t layout;
	xkb_keysym_t sym;
	xkb_keycode_t code;
	xkb_level_index_t level;
};

static void find_key(struct xkb_keymap *keymap, xkb_keycode_t code, void *data) {
	struct key *key = data;
	const xkb_keysym_t *syms;
	xkb_level_index_t levels = xkb_keymap_num_levels_for_key(keymap, code, key->layout);
	for (xkb_level_index_t level = 0;
			level < levels && (!key->code || level < key->level); level++) {
		int n = xkb_keymap_key_get_syms_by_level(keymap, code, key->layout, level, &syms);
		for (int i = 0; i < n; i++) {
			if (syms[i] == key->sym) {
				key->code = code;
				key->level = level;
				return;
			}
		}
	}
}

struct cmd_results *cmd_send_shortcut(int argc, char **argv) {
	static const struct { const char *name, *xkb; } mods[] = {
		{ "ctrl", XKB_MOD_NAME_CTRL }, { "control", XKB_MOD_NAME_CTRL },
		{ "shift", XKB_MOD_NAME_SHIFT },
		{ "alt", XKB_MOD_NAME_ALT }, { "mod1", XKB_MOD_NAME_ALT },
		{ "super", XKB_MOD_NAME_LOGO }, { "mod4", XKB_MOD_NAME_LOGO },
		{ "logo", XKB_MOD_NAME_LOGO },
	};
	char buf[64];
	if (argc != 1 || snprintf(buf, sizeof(buf), "%s", argv[0]) >= (int)sizeof(buf) || !*buf) {
		return cmd_results_new(CMD_INVALID, "Expected send_shortcut MODS+KEY, such as ctrl+v");
	}
	struct sway_container *con = config->handler_context.container;
	if (!con || !con->view) {
		return cmd_results_new(CMD_FAILURE, "No window to send it to");
	}
	// Nothing reaches a window under the lock, as no key does
	if (server.session_lock.lock) {
		return cmd_results_new(CMD_FAILURE, "The screen is locked");
	}
	struct sway_seat *seat = config->handler_context.seat;
	if (!seat) {
		seat = input_manager_current_seat();
	}
	struct wlr_seat *wlr_seat = seat->wlr_seat;
	struct wlr_keyboard *kb = wlr_seat_get_keyboard(wlr_seat);
	if (!kb || !kb->keymap || !kb->xkb_state) {
		return cmd_results_new(CMD_FAILURE, "No keyboard to send it with");
	}
	struct wlr_surface *surface = con->view->surface;
	if (!surface || !surface->mapped) {
		return cmd_results_new(CMD_FAILURE, "The window has no surface to send it to");
	}
	struct wlr_keyboard_modifiers held = { .group = kb->modifiers.group };
	struct key key = {
		.layout = xkb_state_serialize_layout(kb->xkb_state, XKB_STATE_LAYOUT_EFFECTIVE),
	};
	char *tok = buf, *next;
	for (; (next = strchr(tok, '+')); tok = next + 1) {
		*next = '\0';
		xkb_mod_index_t mod = XKB_MOD_INVALID;
		for (size_t i = 0; i < sizeof(mods) / sizeof(mods[0]); i++) {
			if (strcasecmp(tok, mods[i].name) == 0) {
				mod = xkb_keymap_mod_get_index(kb->keymap, mods[i].xkb);
			}
		}
		if (mod == XKB_MOD_INVALID) {
			return cmd_results_new(CMD_INVALID,
				"send_shortcut's modifiers are ctrl, shift, alt and super");
		}
		held.depressed |= 1u << mod;
	}
	if ((key.sym = xkb_keysym_from_name(tok, XKB_KEYSYM_CASE_INSENSITIVE)) == XKB_KEY_NoSymbol) {
		return cmd_results_new(CMD_INVALID, "send_shortcut's key isn't a key name");
	}
	xkb_keymap_key_for_each(kb->keymap, find_key, &key);
	if (!key.code) {
		return cmd_results_new(CMD_FAILURE,
			"No key on the keyboard's layout gives send_shortcut's key");
	}

	// Straight to the window, past any grab, as from its own keyboard
	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);
	uint32_t msec = (uint32_t)(now.tv_sec * 1000 + now.tv_nsec / 1000000);
	struct wlr_surface *had = wlr_seat->keyboard_state.focused_surface;
	if (had != surface) {
		wlr_seat_keyboard_enter(wlr_seat, surface, NULL, 0, &held);
	} else {
		wlr_seat_keyboard_send_modifiers(wlr_seat, &held);
	}
	// An XKB keycode is evdev's plus 8
	wlr_seat_keyboard_send_key(wlr_seat, msec, key.code - 8, WL_KEYBOARD_KEY_STATE_PRESSED);
	wlr_seat_keyboard_send_key(wlr_seat, msec, key.code - 8, WL_KEYBOARD_KEY_STATE_RELEASED);
	if (had == surface) {
		wlr_seat_keyboard_send_modifiers(wlr_seat, &kb->modifiers);
	} else if (had) {
		// With the keys it had held, as sway's focus gives them
		struct sway_keyboard *keyboard = sway_keyboard_for_wlr_keyboard(seat, kb);
		struct sway_shortcut_state *sent = keyboard ? &keyboard->state_pressed_sent : NULL;
		wlr_seat_keyboard_enter(wlr_seat, had, sent ? sent->pressed_keycodes : NULL,
			sent ? sent->npressed : 0, &kb->modifiers);
	} else {
		wlr_seat_keyboard_clear_focus(wlr_seat);
	}
	return cmd_results_new(CMD_SUCCESS, NULL);
}
