/*
 * X11 windows sharp at a fractional scale, as tidewm's x11.h has them.
 * include/sway/tide/x11scale.h says how.
 */
#include <math.h>
#include <stdlib.h>
#include <strings.h>
#include <wayland-server-core.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_xdg_output_v1.h>
#include <wlr/util/box.h>
#include "config.h"
#include "log.h"
#include "sway/commands.h"
#include "sway/config.h"
#include "sway/input/input-manager.h"
#include "sway/input/seat.h"
#include "sway/output.h"
#include "sway/scene_descriptor.h"
#include "sway/server.h"
#include "sway/tide/x11scale.h"
#include "sway/tree/arrange.h"
#include "sway/tree/container.h"
#include "sway/tree/root.h"
#include "sway/tree/view.h"
#include "sway/tree/workspace.h"
#include "xdg-output-unstable-v1-protocol.h"
#if WLR_HAS_XWAYLAND
#include <wlr/xwayland.h>
#include "sway/desktop/transaction.h"
#endif

// xwayland_scaling disable, in force: X11's pixels are the outputs'
static bool unscaled;

struct cmd_results *cmd_xwayland_scaling(int argc, char **argv) {
	if (argc != 1 || (strcasecmp(argv[0], "enable") != 0
			&& strcasecmp(argv[0], "disable") != 0)) {
		return cmd_results_new(CMD_INVALID, "xwayland_scaling takes enable or disable");
	}
	config->tide_xwayland_unscaled = strcasecmp(argv[0], "disable") == 0;
	if (!config->reading) {
		tide_x11_apply();
	}
	return cmd_results_new(CMD_SUCCESS, NULL);
}

#if !WLR_HAS_XWAYLAND
void tide_x11_init(void) {
}

int tide_x11_filter(const struct wl_client *client, const struct wl_global *global) {
	return -1;
}

void tide_x11_apply(void) {
	unscaled = config->tide_xwayland_unscaled;
}

void tide_x11_fini(void) {
}

void tide_x11_outputs_changed(void) {
}
#else

// The scale X11 has on output: its own, unscaled, or 1
static float scale_of(struct sway_output *output) {
	return unscaled && output && output->enabled ? output->wlr_output->scale : 1;
}

/* How far X11's space spreads the layout: by the largest scale, so the
 * outputs' boxes, each its own pixels, don't overlap */
static float spread(void) {
	float spread = 1;
	for (int i = 0; i < root->outputs->length; i++) {
		float s = scale_of(root->outputs->items[i]);
		if (s > spread) {
			spread = s;
		}
	}
	return spread;
}

/* output's box in the layout, from wlroots' layout: current as it
 * changes, where sway's own copy follows after */
static struct wlr_box layout_box(struct sway_output *output) {
	struct wlr_box box = {0};
	wlr_output_layout_get_box(root->output_layout, output->wlr_output, &box);
	return box;
}

// output's box in X11's space
static struct wlr_box box_of(struct sway_output *output) {
	struct wlr_box box = layout_box(output);
	if (!unscaled || !output->enabled) {
		return box;
	}
	float s = spread();
	box.x = (int)lroundf((float)box.x * s);
	box.y = (int)lroundf((float)box.y * s);
	wlr_output_transformed_resolution(output->wlr_output, &box.width, &box.height);
	return box;
}

// The output whose box in X11's space has x, y, or else the focused one
static struct sway_output *output_at(int x, int y) {
	for (int i = 0; i < root->outputs->length; i++) {
		struct sway_output *output = root->outputs->items[i];
		struct wlr_box box = box_of(output);
		if (output->enabled && wlr_box_contains_point(&box, x, y)) {
			return output;
		}
	}
	struct sway_workspace *ws = seat_get_focused_workspace(input_manager_current_seat());
	return ws ? ws->output : NULL;
}

static struct sway_output *output_of(struct sway_view *view) {
	struct sway_workspace *ws = view->container ? view->container->pending.workspace : NULL;
	if (!ws) {
		// Not placed yet: the output it opens on
		ws = seat_get_focused_workspace(input_manager_current_seat());
	}
	return ws ? ws->output : NULL;
}

static double xscale_of(struct sway_view *view) {
	return view->tide.xscale > 0 ? view->tide.xscale : 1;
}

/* Where a configure of view at lx, ly, width by height in the layout puts
 * it in X11's space, on output at scale s: from where sway's truncation
 * puts it, as its transactions compare it, so the place it commits
 * converts back to that */
static struct wlr_box x11_geometry(struct sway_output *output, float s,
		double lx, double ly, int width, int height) {
	struct wlr_box box = box_of(output), layout = layout_box(output);
	return (struct wlr_box){
		.x = box.x + (int)lround(((int)lx - layout.x) * s),
		.y = box.y + (int)lround(((int)ly - layout.y) * s),
		.width = (int)lroundf((float)width * s),
		.height = (int)lroundf((float)height * s),
	};
}

uint32_t tide_x11_configure(struct sway_view *view, double *lx, double *ly,
		int *width, int *height) {
	static uint32_t serials;
	struct sway_output *output = output_of(view);
	float s = scale_of(output);
	view->tide.xscale = s;
	view->tide.x11_serial = 0;
	if (!unscaled || !output) {
		return 0;
	}
	struct wlr_box g = x11_geometry(output, s, *lx, *ly, *width, *height);
	*lx = g.x;
	*ly = g.y;
	*width = g.width;
	*height = g.height;
	view->tide.x11_configured = g;
	if (++serials == 0) {
		serials = 1;
	}
	view->tide.x11_serial = serials;
	return serials;
}

bool tide_x11_configures_nothing(struct sway_view *view, double lx, double ly,
		int width, int height) {
	struct sway_output *output = output_of(view);
	if (!unscaled || !output || !view->tide.x11_serial
			|| view->tide.xscale != scale_of(output)) {
		return false;
	}
	struct wlr_box g = x11_geometry(output, scale_of(output), lx, ly, width, height);
	return wlr_box_equal(&g, &view->tide.x11_configured);
}

bool tide_x11_acked(struct sway_view *view, int x, int y, int width, int height) {
	struct wlr_box *c = &view->tide.x11_configured;
	return view->tide.x11_serial && view->container && c->x == x && c->y == y
		&& c->width == width && c->height == height
		&& transaction_notify_view_ready_by_serial(view, view->tide.x11_serial);
}

int tide_x11_to_layout_size(struct sway_view *view, int size) {
	return (int)lround(size / xscale_of(view));
}

int tide_x11_asked_size(struct sway_view *view, int size) {
	return (int)lroundf((float)size / scale_of(output_of(view)));
}

void tide_x11_to_layout(int x, int y, double *lx, double *ly) {
	struct sway_output *output = output_at(x, y);
	if (!unscaled || !output) {
		*lx = x;
		*ly = y;
		return;
	}
	struct wlr_box box = box_of(output), layout = layout_box(output);
	float s = scale_of(output);
	*lx = layout.x + lroundf((float)(x - box.x) / s);
	*ly = layout.y + lroundf((float)(y - box.y) / s);
}

void tide_x11_view_to_layout(struct sway_view *view, int x, int y, double *lx, double *ly) {
	struct sway_output *output = output_of(view);
	if (!unscaled || !output) {
		*lx = x;
		*ly = y;
		return;
	}
	struct wlr_box box = box_of(output), layout = layout_box(output);
	float s = scale_of(output);
	*lx = layout.x + lroundf((float)(x - box.x) / s);
	*ly = layout.y + lroundf((float)(y - box.y) / s);
}

void tide_x11_opening(struct sway_view *view, struct sway_output *output) {
	if (view->type != SWAY_VIEW_XWAYLAND) {
		return;
	}
	float s = scale_of(output);
	view->natural_width = (int)lroundf((float)view->natural_width / s);
	view->natural_height = (int)lroundf((float)view->natural_height / s);
}

double tide_x11_surface_scale(struct wlr_surface *surface) {
	struct wlr_xwayland_surface *xsurface =
		surface ? wlr_xwayland_surface_try_from_wlr_surface(surface) : NULL;
	if (!unscaled || !xsurface) {
		return 1;
	}
	if (xsurface->override_redirect) {
		return scale_of(output_at(xsurface->x, xsurface->y));
	}
	struct sway_view *view = view_from_wlr_xwayland_surface(xsurface);
	return view ? xscale_of(view) : 1;
}

/* Whether a point on an X11 window's buffer, in the layout's units from
 * its corner, takes input, and where that is in the window's own pixels,
 * as wlroots' own for a surface, past its clip */
static bool accepts_input(struct wlr_scene_buffer *buffer, double *sx, double *sy) {
	struct wlr_scene_surface *scene_surface = wlr_scene_surface_try_from_buffer(buffer);
	if (!scene_surface) {
		return false;
	}
	/* Its clip, from its source box: an X11 surface's buffer is at scale
	 * 1, so in its pixels */
	double s = tide_x11_surface_scale(scene_surface->surface);
	*sx = *sx * s + buffer->src_box.x;
	*sy = *sy * s + buffer->src_box.y;
	return wlr_surface_point_accepts_input(scene_surface->surface, *sx, *sy);
}

/* Shows buffer, of an X11 surface, at its size over its scale: what the
 * scene shows of it, its clip or else the whole of it, in its own pixels */
static void scale_buffer(struct wlr_scene_buffer *buffer, double s) {
	struct wlr_scene_surface *scene_surface = wlr_scene_surface_try_from_buffer(buffer);
	if (!scene_surface) {
		return;
	}
	buffer->point_accepts_input = accepts_input;
	// At 1 too, as a window that was scaled may be no longer
	// Its clip, as its source box, at scale 1 so in its pixels, or all of it
	struct wlr_fbox *clip = &buffer->src_box;
	double width = wlr_fbox_empty(clip) ? scene_surface->surface->current.width : clip->width;
	double height = wlr_fbox_empty(clip) ? scene_surface->surface->current.height : clip->height;
	if (width > 0 && height > 0) {
		wlr_scene_buffer_set_dest_size(buffer, (int)lround(width / s), (int)lround(height / s));
	}
}

static void scale_node(struct wlr_scene_node *node, double s) {
	if (node->type == WLR_SCENE_NODE_BUFFER) {
		scale_buffer(wlr_scene_buffer_from_node(node), s);
	} else if (node->type == WLR_SCENE_NODE_TREE) {
		struct wlr_scene_node *child;
		wl_list_for_each(child, &wlr_scene_tree_from_node(node)->children, link) {
			scale_node(child, s);
		}
	}
}

void tide_x11_scale_view(struct sway_view *view) {
	if (view->type == SWAY_VIEW_XWAYLAND && view->content_tree) {
		scale_node(&view->content_tree->node, xscale_of(view));
	}
}

void tide_x11_clip(struct sway_view *view, struct wlr_box *clip) {
	if (view->type != SWAY_VIEW_XWAYLAND) {
		return;
	}
	double s = xscale_of(view);
	clip->x = (int)lround(clip->x * s);
	clip->y = (int)lround(clip->y * s);
	clip->width = (int)lround(clip->width * s);
	clip->height = (int)lround(clip->height * s);
}

void tide_x11_place_unmanaged(struct wlr_xwayland_surface *xsurface,
		struct wlr_scene_surface *scene_surface) {
	double lx, ly;
	tide_x11_to_layout(xsurface->x, xsurface->y, &lx, &ly);
	wlr_scene_node_set_position(&scene_surface->buffer->node, (int)lx, (int)ly);
	scale_buffer(scene_surface->buffer, tide_x11_surface_scale(xsurface->surface));
}

void tide_x11_moved(struct sway_container *con, struct sway_output *to) {
	/* Not yet configured, as it opens, it has no pixels to keep: view_map
	 * sizes it for its output (tide_x11_opening); fullscreen, its output
	 * sizes it */
	if (!unscaled || !con->view || con->view->type != SWAY_VIEW_XWAYLAND
			|| con->view->tide.xscale <= 0 || !to
			|| con->pending.fullscreen_mode != FULLSCREEN_NONE) {
		return;
	}
	/* From the scale it was configured at, its pixels' on the output it
	 * leaves, which may be going, disabled already as its windows leave */
	double ratio = con->view->tide.xscale / scale_of(to);
	if (ratio == 1) {
		return;
	}
	/* Its pixels are the new output's now: a commit before its configure
	 * there, at the size it had, keeps that size in the new output's scale */
	con->view->tide.xscale = scale_of(to);
	con->pending.content_width = lround(con->pending.content_width * ratio);
	con->pending.content_height = lround(con->pending.content_height * ratio);
	container_set_geometry_from_content(con);
}

/* Xwayland's xdg-output: zxdg_output_v1s for its outputs, which may have
 * gone */
struct x11_output {
	struct wl_resource *resource;
	struct wlr_output *output;
	struct wl_listener output_destroy;
	struct wl_list link; // outputs
};

static struct wl_global *manager;
static struct wl_list outputs;
static struct wl_listener layout_change;

static void output_send(struct x11_output *o) {
	struct sway_output *output = o->output ? o->output->data : NULL;
	if (!output) {
		return;
	}
	struct wlr_box box = box_of(output);
	zxdg_output_v1_send_logical_position(o->resource, box.x, box.y);
	zxdg_output_v1_send_logical_size(o->resource, box.width, box.height);
	if (wl_resource_get_version(o->resource) >= 3) {
		wlr_output_schedule_done(o->output);
	} else {
		zxdg_output_v1_send_done(o->resource);
	}
}

static void output_gone(struct wl_listener *listener, void *data) {
	struct x11_output *o = wl_container_of(listener, o, output_destroy);
	wl_list_remove(&o->output_destroy.link);
	wl_list_init(&o->output_destroy.link);
	o->output = NULL;
}

static void output_resource_destroy(struct wl_resource *resource) {
	struct x11_output *o = wl_resource_get_user_data(resource);
	wl_list_remove(&o->output_destroy.link);
	wl_list_remove(&o->link);
	free(o);
}

static void output_handle_destroy(struct wl_client *client, struct wl_resource *resource) {
	wl_resource_destroy(resource);
}

static const struct zxdg_output_v1_interface output_impl = {
	.destroy = output_handle_destroy,
};

static void manager_get_output(struct wl_client *client, struct wl_resource *resource,
		uint32_t id, struct wl_resource *output_resource) {
	struct x11_output *o = calloc(1, sizeof(*o));
	if (!o || !(o->resource = wl_resource_create(client, &zxdg_output_v1_interface,
			wl_resource_get_version(resource), id))) {
		sway_log(SWAY_ERROR, "tide: no memory for Xwayland's xdg-output");
		free(o);
		wl_client_post_no_memory(client);
		return;
	}
	wl_resource_set_implementation(o->resource, &output_impl, o, output_resource_destroy);
	wl_list_insert(&outputs, &o->link);
	wl_list_init(&o->output_destroy.link);
	o->output_destroy.notify = output_gone;
	// An output already gone leaves it inert
	if (!(o->output = wlr_output_from_resource(output_resource))) {
		return;
	}
	wl_signal_add(&o->output->events.destroy, &o->output_destroy);
	if (wl_resource_get_version(o->resource) >= 2) {
		zxdg_output_v1_send_name(o->resource, o->output->name);
		if (o->output->description) {
			zxdg_output_v1_send_description(o->resource, o->output->description);
		}
	}
	output_send(o);
}

static void manager_destroy(struct wl_client *client, struct wl_resource *resource) {
	wl_resource_destroy(resource);
}

static const struct zxdg_output_manager_v1_interface manager_impl = {
	.destroy = manager_destroy,
	.get_xdg_output = manager_get_output,
};

static void manager_bind(struct wl_client *client, void *data, uint32_t version, uint32_t id) {
	struct wl_resource *resource = wl_resource_create(client,
		&zxdg_output_manager_v1_interface, (int)version, id);
	if (!resource) {
		sway_log(SWAY_ERROR, "tide: no memory to bind Xwayland's xdg-output");
		wl_client_post_no_memory(client);
		return;
	}
	wl_resource_set_implementation(resource, &manager_impl, NULL, NULL);
}

static const struct wl_client *xwayland_client(void) {
	struct wlr_xwayland *xwayland = server.xwayland.wlr_xwayland;
	return xwayland && xwayland->server ? xwayland->server->client : NULL;
}

int tide_x11_filter(const struct wl_client *client, const struct wl_global *global) {
	if (manager && global == manager) {
		return client == xwayland_client();
	}
	if (server.xdg_output_manager_v1 && global == server.xdg_output_manager_v1->global) {
		const struct wl_client *x = xwayland_client();
		return !(x && client == x);
	}
	return -1;
}

static void configure_again(struct sway_container *con, void *data) {
	struct sway_view *view = con->view;
	if (!view || view->type != SWAY_VIEW_XWAYLAND || !view->surface
			|| !view->surface->mapped) {
		return;
	}
	/* Where its last configure put it: a transaction in flight hasn't made
	 * that current yet, and one that changes nothing configures nothing */
	if (con->pending.content_width <= 0 || con->pending.content_height <= 0) {
		return;
	}
	/* A transaction waiting on it waits on its serial: this configure is
	 * of the same place in the layout, so its commit acknowledges that */
	uint32_t serial = view->tide.x11_serial;
	view_configure(view, con->pending.content_x, con->pending.content_y,
		con->pending.content_width, con->pending.content_height);
	if (serial && con->node.instruction) {
		view->tide.x11_serial = serial;
	}
	// Its clip in its pixels at the new scale, and its buffer shown at it
	view_center_and_clip_surface(view);
}

/* The outputs, or how X11 scales, changed: Xwayland hears their boxes
 * again, and X11 windows are configured and shown again */
static void update(void) {
	struct x11_output *o;
	wl_list_for_each(o, &outputs, link) {
		output_send(o);
	}
	// Each mapped X11 window is configured again, at its size in the layout
	root_for_each_container(configure_again, NULL);
	// And its menus, from where they are
	struct wlr_scene_node *node;
	wl_list_for_each(node, &root->layers.unmanaged->children, link) {
		struct sway_xwayland_unmanaged *menu =
			scene_descriptor_try_get(node, SWAY_SCENE_DESC_XWAYLAND_UNMANAGED);
		if (menu && menu->surface_scene) {
			tide_x11_place_unmanaged(menu->wlr_xwayland_surface, menu->surface_scene);
		}
	}
	transaction_commit_dirty();
}

static void handle_layout_change(struct wl_listener *listener, void *data) {
	update();
}

/* An output joins wlroots' layout before sway enables it, so the layout's
 * change came before root->outputs had it */
void tide_x11_outputs_changed(void) {
	if (manager) {
		update();
	}
}

void tide_x11_init(void) {
	wl_list_init(&outputs);
	if (!(manager = wl_global_create(server.wl_display, &zxdg_output_manager_v1_interface,
			3, NULL, manager_bind))) {
		sway_log(SWAY_ERROR, "tide: can't make Xwayland's xdg-output");
		return;
	}
	layout_change.notify = handle_layout_change;
	wl_signal_add(&root->output_layout->events.change, &layout_change);
}

void tide_x11_fini(void) {
	// The output layout, gone after this, takes no listener with it
	if (manager) {
		wl_list_remove(&layout_change.link);
	}
}

void tide_x11_apply(void) {
	if (unscaled == config->tide_xwayland_unscaled) {
		return;
	}
	unscaled = config->tide_xwayland_unscaled;
	// The first config comes before Xwayland and its outputs
	if (manager) {
		update();
	}
}
#endif
