#ifndef _SWAY_TIDE_CAPTURE_H
#define _SWAY_TIDE_CAPTURE_H

/*
 * Capture sessions, for the IPC's capture event, tidewm's own (its
 * capture.h), for tide's Sharing pill (its SPEC §12). A session over
 * ext-image-copy-capture starts when a client asks for one, such as
 * xdg-desktop-portal-wlr sharing a window or an output, or grim taking a
 * screenshot, and stops when the client ends it, goes, or its window
 * closes: {"change": "start"|"stop", "source": "window"|"output",
 * "con_id": ...}, con_id null for an output, which wlroots doesn't say
 * which of. A stream of just the pointer's cursor, which a client may
 * capture alongside an output, isn't a capture of its own. wlr-screencopy's
 * captures go unseen: wlroots says nothing of them.
 */

#include <wlr/types/wlr_ext_image_copy_capture_v1.h>

void tide_capture_init(struct wlr_ext_image_copy_capture_manager_v1 *manager);

/* Stops following them, before the display goes: the manager asserts no
 * one listens as it's destroyed */
void tide_capture_fini(void);

#endif
