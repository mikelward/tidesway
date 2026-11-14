/*
 * When a window's new event goes (include/sway/tide/announce.h says why).
 */
#include <stdbool.h>
#include <string.h>
#include "log.h"
#include "sway/input/input-manager.h"
#include "sway/input/seat.h"
#include "sway/ipc-server.h"
#include "sway/tide/announce.h"
#include "sway/tree/container.h"
#include "sway/tree/view.h"

static bool has_focus(struct sway_container *con) {
	struct sway_seat *seat = input_manager_current_seat();
	return seat && seat_get_focus(seat) == &con->node;
}

void tide_announce_mapped(struct sway_view *view) {
	view->tide.unannounced = true;
	view->tide.nwaiting = 0;
}

void tide_announce(struct sway_view *view) {
	if (!view->tide.unannounced || !view->container) {
		return;
	}
	view->tide.unannounced = false;
	ipc_event_window(view->container, "new");
	// Each as the window is now: what it says of the window is no older
	// than the new event before it
	int n = view->tide.nwaiting;
	view->tide.nwaiting = 0;
	for (int i = 0; i < n; i++) {
		const char *change = view->tide.waiting[i];
		/* Focus that has since moved on isn't said: the window that has
		 * it said so as it took it, and this one's after that would have
		 * a shell following focus events on the wrong window */
		if (strcmp(change, "focus") == 0 && !has_focus(view->container)) {
			continue;
		}
		ipc_event_window(view->container, change);
	}
}

bool tide_announce_holds(struct sway_view *view, const char *change) {
	if (!view->tide.unannounced) {
		return false;
	}
	// One of a kind is enough: each is sent with the window as it is then
	for (int i = 0; i < view->tide.nwaiting; i++) {
		if (strcmp(view->tide.waiting[i], change) == 0) {
			return true;
		}
	}
	if (view->tide.nwaiting == TIDE_ANNOUNCE_WAITING) {
		sway_log(SWAY_ERROR, "tide: dropping a window's %s event: too many "
			"wait behind its new one", change);
		return true;
	}
	// Every caller's change is a string literal, so it outlasts the wait
	view->tide.waiting[view->tide.nwaiting++] = change;
	return true;
}
