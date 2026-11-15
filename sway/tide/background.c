#include <wayland-server-core.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/util/box.h>
#include "log.h"
#include "sway/tide/background.h"
#include "sway/tree/root.h"

// tidewm's config.h's rootcolor, premultiplied as a scene rect takes it
static const float rootcolor[4] = { 0x22 / 255.0f, 0x22 / 255.0f, 0x22 / 255.0f, 1.0f };

static struct wlr_scene_rect *rect;
static struct wl_listener layout_change;

static void handle_layout_change(struct wl_listener *listener, void *data) {
	struct wlr_box box;
	wlr_output_layout_get_box(root->output_layout, NULL, &box);
	wlr_scene_node_set_position(&rect->node, box.x, box.y);
	wlr_scene_rect_set_size(rect, box.width, box.height);
}

void tide_background_init(void) {
	// In the scene's own tree, under root->staging and root->layer_tree,
	// whose layers are all node_at_coords looks in
	rect = wlr_scene_rect_create(&root->root_scene->tree, 0, 0, rootcolor);
	if (!rect) {
		sway_log(SWAY_ERROR, "tide: can't make the background, so it's black");
		return;
	}
	wlr_scene_node_lower_to_bottom(&rect->node);
	layout_change.notify = handle_layout_change;
	wl_signal_add(&root->output_layout->events.change, &layout_change);
	sway_log(SWAY_DEBUG, "tide: the background is #222222");
}

void tide_background_fini(void) {
	if (rect) {
		wl_list_remove(&layout_change.link);
		wlr_scene_node_destroy(&rect->node);
		rect = NULL;
	}
}
