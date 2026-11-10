#ifndef _SWAY_TIDE_XWAYLAND_H
#define _SWAY_TIDE_XWAYLAND_H

/*
 * X11 windows that never showed up, from two races in wlroots 0.20 where
 * it pairs an X11 window with its Wayland surface.
 *
 * The pairing reads the window's properties synchronously. X events that
 * arrive meanwhile wait in xcb's queue, and when the pairing came from the
 * Wayland side, nothing wakes wlroots' window manager to read them; a map
 * request among them leaves its window unmapped. So after each pairing
 * this asks the X server for something, whose answer wakes the window
 * manager, which then reads every queued event.
 *
 * wlroots maps the surface on a commit with a buffer after the pairing.
 * Xwayland can commit the window's first buffer before the window manager
 * has heard the X side of the pairing, then waits for that frame to be
 * shown before it commits again, so the window would never map. So a
 * surface paired with a buffer already maps here.
 */

struct wlr_xwayland_surface;

/* After an X11 window is paired with its surface */
void tide_xwayland_paired(struct wlr_xwayland_surface *xsurface);

#endif
