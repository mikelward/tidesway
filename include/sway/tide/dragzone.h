#ifndef _SWAY_TIDE_DRAGZONE_H
#define _SWAY_TIDE_DRAGZONE_H

/*
 * A window's shadow is where a drag resizes it, as on Hyprland (tide's
 * SPEC §6, "Resizing by an edge"). A plain left press just outside a
 * floating window's edge or corner resizes it by that edge or corner. The
 * zone reaches as far as a shadow does, shadow_blur_radius pixels, and 8
 * pixels where the config gives no window one. A floating window that
 * casts none now, as an unfocused one with shadows active, has the same
 * zone. A press inside the window is the app's, as ever.
 *
 * The zone takes the press from what's under it, a tile or another
 * floating window. Beside the end of an edge, it's the corner's. What's over the window keeps the press: a floating window above
 * it, a fullscreen window, as on the next output, a top or overlay layer
 * surface, a popup or the lock. A fullscreen or maximized window has no
 * zone, nor a tile in the floating layout.
 *
 * A tile's shadow across the edge between the masters and the stack drags
 * that edge (split.h). The pointer shows a resize cursor over either zone,
 * and the window under it doesn't have the pointer. Focus doesn't follow
 * the pointer into a zone, as in tidewm, so a shadow stays where it is to
 * be dragged: with shadows active it would go as the pointer reached it.
 * Focus moves once the pointer is past the zone.
 */

#include <stdbool.h>

struct sway_cursor;
struct sway_node;
struct sway_seat;
struct wlr_surface;

/* A plain left press at the cursor, on node and surface as node_at_coords
 * found them: starts resizing the floating window whose zone it's in, or
 * dragging the tiles' edge whose zone it's in, and returns true */
bool tide_dragzone_press(struct sway_seat *seat, struct sway_node *node,
	struct wlr_surface *surface);

/* The pointer moved, onto node and *surface as node_at_coords found
 * them. Returns whether it's in a zone: there it's on no surface, and
 * focus doesn't follow it. */
bool tide_dragzone_hover(struct sway_seat *seat, struct sway_node *node,
	struct wlr_surface **surface);

/* cursor_update_image's: in a zone, shows the resize cursor for it and
 * returns true */
bool tide_dragzone_cursor(struct sway_cursor *cursor);

#endif
