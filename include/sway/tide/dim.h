#ifndef _SWAY_TIDE_DIM_H
#define _SWAY_TIDE_DIM_H

/*
 * Dimming, as tidewm has it (its dim.h, tide's SPEC §6.2): with
 * dim_inactive above 0, every window but the focused one is darkened by a
 * translucent black layer over its content, at that strength, and clicks
 * go through the layer to the window. Where there's nothing to tell apart,
 * an output showing one window or a fullscreen one, nothing dims; nor does
 * a window that would look broken dimmed: picture-in-picture, video by its
 * content type, and one the dim command or a for_window rule exempts. A
 * window's popups, menus and tooltips, and layer surfaces, are never
 * dimmed.
 */

#include <stdbool.h>

struct sway_view;
struct wlr_scene_buffer;

// A window's dimming, held in its struct sway_view
struct tide_dim_view {
	struct wlr_scene_buffer *layer; // over its content, made when it first dims
	bool exempt; // the dim command's or a rule's: it never dims
	int content_type; // its surface's, as last committed
};

/* Brings view's layer up to date, as focus, the windows shown and the
 * settings now are, and returns whether it's dimmed. IPC asks through
 * this, so what it says is what's shown, once the transaction laying the
 * window out has applied. */
bool tide_dim_update(struct sway_view *view);

/* Brings every window's layer up to date: after focus moves, a
 * transaction lays the windows out, or the config is reloaded */
void tide_dim_update_all(void);

// A commit on view's surface, which can change its content type
void tide_dim_committed(struct sway_view *view);

#endif
