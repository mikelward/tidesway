/*
 * Capture sessions, for tidewm's capture event.
 * include/sway/tide/capture.h says what it says.
 */
#include <stdlib.h>
#include <wlr/interfaces/wlr_ext_image_capture_source_v1.h>
#include <wlr/types/wlr_ext_image_capture_source_v1.h>
#include "log.h"
#include "sway/ipc-server.h"
#include "sway/tide/capture.h"
#include "sway/tree/container.h"
#include "sway/tree/root.h"
#include "sway/tree/view.h"

struct session {
	struct wl_listener destroy;
	size_t con_id; // the captured window's, or 0 for an output
};

static struct wl_listener new_session;

static void handle_destroy(struct wl_listener *listener, void *data) {
	struct session *s = wl_container_of(listener, s, destroy);
	wl_list_remove(&s->destroy.link);
	ipc_event_capture("stop", s->con_id);
	free(s);
}

struct find {
	struct wlr_ext_image_capture_source_v1 *source;
	size_t con_id;
};

static void find_window(struct sway_container *con, void *data) {
	struct find *f = data;
	if (con->view && con->view->image_capture_source
			&& con->view->image_capture_source == f->source) {
		f->con_id = con->node.id;
	}
}

static void handle_new_session(struct wl_listener *listener, void *data) {
	struct wlr_ext_image_copy_capture_session_v1 *session = data;
	// A window's source is the one its capture request made for it
	struct find f = { session->source, 0 };
	root_for_each_container(find_window, &f);
	/* Otherwise it's an output's, which can give its pointer's cursor, or
	 * that cursor's own, which can't */
	if (!f.con_id && !session->source->impl->get_pointer_cursor) {
		return;
	}
	struct session *s = calloc(1, sizeof(*s));
	if (!s) {
		sway_log(SWAY_ERROR, "tide: no memory to follow a capture session");
		return;
	}
	s->con_id = f.con_id;
	s->destroy.notify = handle_destroy;
	wl_signal_add(&session->events.destroy, &s->destroy);
	ipc_event_capture("start", s->con_id);
}

void tide_capture_init(struct wlr_ext_image_copy_capture_manager_v1 *manager) {
	new_session.notify = handle_new_session;
	wl_signal_add(&manager->events.new_session, &new_session);
}

void tide_capture_fini(void) {
	if (new_session.notify) {
		wl_list_remove(&new_session.link);
		new_session.notify = NULL;
	}
}
