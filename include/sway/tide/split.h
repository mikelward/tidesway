#ifndef _SWAY_TIDE_SPLIT_H
#define _SWAY_TIDE_SPLIT_H

#include <stdbool.h>

struct sway_container;
struct sway_node;
struct sway_seat;
struct wlr_surface;

/* A plain left press at the cursor, on node and surface as node_at_coords
 * found them: if it's in the zone of an edge of the focused workspace's
 * tide layout, starts dragging it, and returns true. The zone is
 * resize_grab pixels either side of the edge, and a shadow across it, as
 * far as it reaches: the focused tile's with shadows active, on its
 * neighbor's side of the edge and only beside that tile, and every tile's
 * with shadows all. resize_grab 0 turns it off, shadows and all. Anything
 * over the edge takes the press as usual. */
bool tide_split_press(struct sway_seat *seat, struct sway_node *node,
	struct wlr_surface *surface);

// Whether the cursor is in such a zone, where a press would drag an edge
bool tide_split_hover(struct sway_seat *seat, struct sway_node *node,
	struct wlr_surface *surface);

/* floating_modifier's resize button on tiled con: drags its workspace's
 * edge nearest the cursor, or floats con if there's none, as dwl's does.
 * Returns false for a workspace in sway's layouts. */
bool tide_split_press_tile(struct sway_seat *seat, struct sway_container *con);

#endif
