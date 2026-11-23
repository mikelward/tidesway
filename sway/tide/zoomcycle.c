/*
 * tidewm's zoom next and zoom prev, tide's Alt+Tab (its SPEC §6.6), as
 * tidewm's cycle.h has them: bound to a key with modifiers, the first press
 * notes the focused window, each further press while the modifiers are
 * held moves focus to the next, or the last, tiled window of the focused
 * workspace part, in stack order, round from the end, and letting go of the
 * modifiers but Shift makes the focused window the master, as zoom does,
 * unless it's the master already. So a tap is zoom. Escape while they're
 * held puts focus back where it was and makes no master. Run with no
 * modifiers held, as over the IPC, it's zoom at once. Only the keyboard
 * holding the modifiers ends it, and it ends with no master made if that
 * keyboard goes, as a virtual one does.
 */
#include <stdlib.h>
#include <wlr/types/wlr_keyboard.h>
#include <wlr/types/wlr_keyboard_group.h>
#include "list.h"
#include "sway/desktop/transaction.h"
#include "sway/input/seat.h"
#include "sway/tide/grab.h"
#include "sway/tide/layout.h"
#include "sway/tide/zoomcycle.h"
#include "sway/tree/container.h"
#include "sway/tree/root.h"
#include "sway/tree/view.h"
#include "sway/tree/workspace.h"

static struct {
	bool armed;
	struct sway_seat *seat; // the seat it's under way on
	uint32_t mods; // the modifiers whose letting go ends it
	size_t start; // the container focus was in as it began, by ID
	bool stepped; // whether focus moved since
	// where the modifiers are held: a grouped keyboard's group, whose
	// modifier events are the ones that come
	const struct wlr_keyboard *keyboard;
} cycle;

static uint32_t press_mods;

// Whether ws is in one of tide's tiled layouts, where zoom makes a master
static bool tiled(struct sway_workspace *ws) {
	enum tide_layout lt = ws ? tide_workspace_layout(ws) : TIDE_NONE;
	return lt != TIDE_NONE && lt != TIDE_FLOATING;
}
static const struct wlr_keyboard *press_keyboard;

void tide_zoomcycle_press(struct wlr_keyboard *keyboard, uint32_t mods) {
	// A device-specific binding runs on a grouped keyboard itself
	press_keyboard = keyboard && keyboard->group ? &keyboard->group->keyboard : keyboard;
	press_mods = mods;
}

// The window focused, or with the keyboard on a layer surface, such as a
// notification's, the one last focused on the workspace, as a binding's
// command has it
bool tide_zoomcycle_pressed(void) {
	return press_mods != 0;
}

static struct sway_container *focused(struct sway_seat *seat) {
	struct sway_container *con = seat_get_focused_container(seat);
	if (!con) {
		struct sway_workspace *ws = seat_get_focused_workspace(seat);
		con = ws ? seat_get_focus_inactive_view(seat, &ws->node) : NULL;
	}
	return con && con->view ? con : NULL;
}

// Whether con is one it steps to: a tiled window, not fullscreen or minimized
static bool takes(struct sway_container *con) {
	return con->view && !container_is_floating_or_child(con)
		&& con->pending.fullscreen_mode == FULLSCREEN_NONE
		&& !con->view->tide.minimized;
}

// Zoom, as the zoom command: con the master, focus on it
static void zoom(struct sway_seat *seat, struct sway_container *con) {
	struct sway_container *master = con ? tide_zoom(con) : NULL;
	if (master) {
		seat_set_focus_container(seat, master);
	}
}

// The window after con, or before it, in its part's stack, round
static struct sway_container *step(struct sway_container *con, int dir) {
	struct sway_workspace *ws = con->pending.workspace;
	list_t *tiling = ws->tiling;
	int at = list_find(tiling, con);
	for (int i = 1; at >= 0 && i < tiling->length; i++) {
		struct sway_container *c =
			tiling->items[(at + dir * i + tiling->length * i) % tiling->length];
		if (takes(c)) {
			return c;
		}
	}
	return NULL;
}

// The first window it steps to in ws: its master
static struct sway_container *master_of(struct sway_workspace *ws) {
	for (int i = 0; i < ws->tiling->length; i++) {
		struct sway_container *c = ws->tiling->items[i];
		if (takes(c)) {
			return c;
		}
	}
	return NULL;
}

struct by_id {
	size_t id;
	struct sway_container *found;
};

static void find(struct sway_container *con, void *data) {
	struct by_id *by = data;
	if (con->node.id == by->id) {
		by->found = con;
	}
}

void tide_zoomcycle_zoom(struct sway_seat *seat, int dir) {
	struct sway_container *con = focused(seat);
	struct sway_workspace *ws = con ? con->pending.workspace : seat_get_focused_workspace(seat);
	// Over the IPC, say, even while a keyboard's is under way; and where
	// zoom does nothing, in sway's own layouts and the floating one, whose
	// split tree stepping would flatten
	// A seat of its own, while another's is under way, as sway's seats
	// each have their own focus
	if (!press_mods || !tiled(ws) || (cycle.armed && cycle.seat != seat)) {
		zoom(seat, con);
		return;
	}
	if (!cycle.armed) {
		cycle.armed = true;
		// Shift, which turns Alt+Tab back, isn't one it waits on
		cycle.mods = press_mods & ~WLR_MODIFIER_SHIFT;
		if (!cycle.mods) {
			cycle.mods = press_mods;
		}
		cycle.keyboard = press_keyboard;
		cycle.seat = seat;
		cycle.start = con ? con->node.id : 0;
		cycle.stepped = false;
		return;
	}
	struct sway_container *next = NULL;
	if (con && takes(con) && tiled(con->pending.workspace)) {
		tide_flatten(con->pending.workspace);
		next = step(con, dir);
	} else {
		// From a floating window, or none, to the first tiled one
		next = master_of(ws);
	}
	if (next && next != con) {
		seat_set_focus_container(seat, next);
		cycle.stepped = true;
	}
}

void tide_zoomcycle_modifiers(struct sway_seat *seat,
		const struct wlr_keyboard *keyboard, uint32_t mods) {
	if (!cycle.armed || keyboard != cycle.keyboard
			|| (mods & cycle.mods) == cycle.mods) {
		return;
	}
	cycle.armed = false;
	cycle.keyboard = NULL;
	struct sway_container *con = focused(seat);
	if (cycle.stepped && con && con->pending.workspace
			&& master_of(con->pending.workspace) == con) {
		return; // the master already, which a tap's zoom would swap
	}
	// Asked for, as its binding was, so it ends a focus grab and leaves a
	// layer surface, as zoom from a binding does
	tide_grab_ask_begin();
	zoom(seat, con);
	tide_grab_ask_end();
	// Outside a binding, which would: shown now, not at the next event
	transaction_commit_dirty();
}

bool tide_zoomcycle_key(struct sway_seat *seat, xkb_keysym_t sym) {
	if (!cycle.armed || seat != cycle.seat || sym != XKB_KEY_Escape) {
		return false;
	}
	cycle.armed = false;
	cycle.keyboard = NULL;
	struct by_id by = { .id = cycle.start };
	if (by.id) {
		root_for_each_container(find, &by);
	}
	// Only to a window still shown: on a workspace left since, it stays
	if (by.found && by.found->pending.workspace
			&& workspace_is_visible(by.found->pending.workspace)) {
		tide_grab_ask_begin(); // as a binding's is
		seat_set_focus_container(seat, by.found);
		tide_grab_ask_end();
		transaction_commit_dirty(); // as a binding's is
	}
	return true;
}

void tide_zoomcycle_keyboard_gone(const struct wlr_keyboard *keyboard) {
	if (cycle.armed && cycle.keyboard == keyboard) {
		cycle.armed = false;
		cycle.keyboard = NULL;
	}
}
