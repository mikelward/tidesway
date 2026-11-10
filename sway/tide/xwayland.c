/*
 * X11 windows that map whatever order wlroots hears of them in.
 * include/sway/tide/xwayland.h says why.
 */
#include <wlr/types/wlr_compositor.h>
#include <wlr/xwayland.h>
#include <xcb/xcb.h>
#include "log.h"
#include "sway/server.h"
#include "sway/tide/xwayland.h"

// Makes the X server answer, so wlroots' window manager reads its queue
static void wake(struct wlr_xwayland_surface *xsurface) {
	xcb_connection_t *conn = wlr_xwayland_get_xwm_connection(server.xwayland.wlr_xwayland);
	if (!conn || xcb_connection_has_error(conn)) {
		sway_log(SWAY_DEBUG, "tide: no X11 window manager to wake for window %u",
			xsurface->window_id);
		return;
	}
	// The answer is what wakes it, so nothing waits for it
	xcb_get_input_focus_cookie_t cookie = xcb_get_input_focus(conn);
	xcb_discard_reply(conn, cookie.sequence);
	xcb_flush(conn);
}

void tide_xwayland_paired(struct wlr_xwayland_surface *xsurface) {
	wake(xsurface);
	struct wlr_surface *surface = xsurface->surface;
	if (!surface->mapped && wlr_surface_has_buffer(surface)) {
		sway_log(SWAY_DEBUG, "tide: mapping X11 window %u, drawn before it was paired",
			xsurface->window_id);
		wlr_surface_map(surface);
	}
}
