/*
 * Dimming, as tidewm's dim.h has it (tide's SPEC §6.2): a translucent
 * black layer over each window's content but the focused one's, which
 * clicks go through. include/sway/tide/dim.h says what dims; tidewm's
 * docs/config.md and docs/ipc.md are the reference.
 */
#include <drm_fourcc.h>
#include <errno.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <wlr/interfaces/wlr_buffer.h>
#include <wlr/types/wlr_buffer.h>
#include <wlr/types/wlr_content_type_v1.h>
#include <wlr/types/wlr_scene.h>
#include "log.h"
#include "sway/commands.h"
#include "sway/config.h"
#include "sway/input/input-manager.h"
#include "sway/input/seat.h"
#include "sway/server.h"
#include "sway/tide/dim.h"
#include "sway/tide/look.h"
#include "sway/tree/container.h"
#include "sway/tree/root.h"
#include "sway/tree/view.h"
#include "sway/tree/workspace.h"

/* The layer's one pixel, black at the strength's alpha (premultiplied, so
 * its color stays 0), which each window's scene buffer stretches. The
 * alpha is in the pixel, not the scene buffer's opacity, which sway's
 * output_configure_scene sets to the window's opacity on every frame. A
 * new strength makes a new pixel; the old one goes once no layer holds
 * it. */
struct dim_pixel {
	struct wlr_buffer base;
	uint32_t pixel;
};

static struct dim_pixel *dim_pixel;

static void dim_pixel_destroy(struct wlr_buffer *buffer) {
	struct dim_pixel *p = wl_container_of(buffer, p, base);
	wlr_buffer_finish(buffer);
	free(p);
}

static bool dim_pixel_begin(struct wlr_buffer *buffer, uint32_t flags,
		void **data, uint32_t *format, size_t *stride) {
	struct dim_pixel *p = wl_container_of(buffer, p, base);
	if (flags & WLR_BUFFER_DATA_PTR_ACCESS_WRITE) {
		return false;
	}
	*data = &p->pixel;
	*format = DRM_FORMAT_ARGB8888;
	*stride = sizeof(p->pixel);
	return true;
}

static void dim_pixel_end(struct wlr_buffer *buffer) {
	// Nothing to release: the pixel is the buffer's own
}

static const struct wlr_buffer_impl dim_pixel_impl = {
	.destroy = dim_pixel_destroy,
	.begin_data_ptr_access = dim_pixel_begin,
	.end_data_ptr_access = dim_pixel_end,
};

// The pixel for the strength, from 0 to 1, made if it's new
static struct wlr_buffer *dim_buffer(double strength) {
	uint32_t pixel = (uint32_t)lround(strength * 255) << 24;
	if (dim_pixel && dim_pixel->pixel == pixel) {
		return &dim_pixel->base;
	}
	struct dim_pixel *p = calloc(1, sizeof(*p));
	if (!p) {
		sway_log(SWAY_ERROR, "tide: can't allocate the dim layer's pixel");
		return NULL;
	}
	wlr_buffer_init(&p->base, &dim_pixel_impl, 1, 1);
	p->pixel = pixel;
	if (dim_pixel) {
		// Freed once the last layer showing it takes the new one
		wlr_buffer_drop(&dim_pixel->base);
	}
	dim_pixel = p;
	return &p->base;
}

bool tide_dim_is_layer(struct wlr_scene_buffer *buffer) {
	return buffer->buffer && buffer->buffer->impl == &dim_pixel_impl;
}

// Clicks and the pointer go through the layer to the window under it
static bool dim_no_input(struct wlr_scene_buffer *buffer, double *sx, double *sy) {
	return false;
}

static void count_view(struct sway_container *con, void *data) {
	if (con->view) {
		++*(int *)data;
	}
}

/* Whether ws's output shows more than one window: a fullscreen one, on the
 * workspace or across every output, hides the rest */
static bool shows_several(struct sway_workspace *ws) {
	if (root->fullscreen_global || ws->fullscreen) {
		return false;
	}
	int n = 0;
	workspace_for_each_container(ws, count_view, &n);
	return n > 1;
}

// A browser's picture-in-picture window, by its title: ^Picture[- ]in[- ][Pp]icture$
static bool is_pip(struct sway_view *view) {
	const char *t = view_get_title(view);
	return t && strlen(t) == 18 && strncmp(t, "Picture", 7) == 0
		&& (t[7] == '-' || t[7] == ' ') && strncmp(t + 8, "in", 2) == 0
		&& (t[10] == '-' || t[10] == ' ') && (t[11] == 'P' || t[11] == 'p')
		&& strcmp(t + 12, "icture") == 0;
}

static bool is_video(struct sway_view *view) {
	return view->surface && wlr_surface_get_content_type_v1(
		server.content_type_manager_v1, view->surface) == WP_CONTENT_TYPE_V1_TYPE_VIDEO;
}

static bool wanted(struct sway_view *view) {
	struct sway_container *con = view->container;
	if (config->tide_dim_inactive <= 0 || !con || !view->surface
			|| view->tide_dim.exempt) {
		return false;
	}
	struct sway_workspace *ws = con->pending.workspace;
	if (!ws || !ws->output || !workspace_is_visible(ws)) {
		return false;
	}
	// The window GET_TREE calls focused
	struct sway_seat *seat = input_manager_get_default_seat();
	if (seat && seat_get_focus(seat) == &con->node) {
		return false;
	}
	return !is_pip(view) && !is_video(view) && shows_several(ws);
}

bool tide_dim_update(struct sway_view *view) {
	struct sway_container *con = view->container;
	bool on = wanted(view);
	// Shown once a transaction has laid it out; until then there's nothing
	// to cover, and the transaction's update shows it
	int width = con ? con->current.content_width : 0;
	int height = con ? con->current.content_height : 0;
	bool shown = on && width > 0 && height > 0;
	struct wlr_buffer *buffer = shown ? dim_buffer(config->tide_dim_inactive) : NULL;
	if (shown && !buffer) {
		shown = false;
	}
	if (shown && !view->tide_dim.layer) {
		// Last in its tree, so over its content and over the buffer saved
		// while it resizes, which sway puts below the output handler
		view->tide_dim.layer = wlr_scene_buffer_create(view->scene_tree, buffer);
		if (!view->tide_dim.layer) {
			sway_log(SWAY_ERROR, "tide: can't make a window's dim layer");
			return on;
		}
		view->tide_dim.layer->point_accepts_input = dim_no_input;
		// One pixel stretched: a bilinear filter would fade it toward the edges
		wlr_scene_buffer_set_filter_mode(view->tide_dim.layer, WLR_SCALE_FILTER_NEAREST);
	}
	struct wlr_scene_buffer *layer = view->tide_dim.layer;
	if (layer) {
		wlr_scene_node_set_enabled(&layer->node, shown);
	}
	if (layer && shown) {
		if (layer->buffer != buffer) {
			wlr_scene_buffer_set_buffer(layer, buffer);
		}
		// A rounded window's corners' rows have the dim drawn into them
		int r = view->tide_look.radius;
		wlr_scene_node_set_position(&layer->node, 0, r);
		wlr_scene_buffer_set_dest_size(layer, width, height - 2 * r);
	}
	return on;
}

static void update_container(struct sway_container *con, void *data) {
	if (con->view) {
		tide_dim_update(con->view);
	}
}

void tide_dim_update_all(void) {
	if (!root || !config) {
		return;
	}
	root_for_each_container(update_container, NULL);
}

void tide_dim_committed(struct sway_view *view) {
	int type = view->surface ? (int)wlr_surface_get_content_type_v1(
		server.content_type_manager_v1, view->surface) : 0;
	if (type != view->tide_dim.content_type) {
		view->tide_dim.content_type = type;
		tide_dim_update(view);
	}
}

// dim_inactive STRENGTH, from 0, none, to 1, black
struct cmd_results *cmd_dim_inactive(int argc, char **argv) {
	char *end = NULL;
	double v = 0;
	if (argc == 1) {
		errno = 0;
		v = strtod(argv[0], &end);
	}
	// NaN fails both comparisons
	if (argc != 1 || end == argv[0] || *end || errno || !(v >= 0 && v <= 1)) {
		return cmd_results_new(CMD_INVALID,
			"dim_inactive takes a strength from 0 to 1");
	}
	config->tide_dim_inactive = v;
	return cmd_results_new(CMD_SUCCESS, NULL);
}

// dim enable|disable|toggle: whether the window dims when it isn't focused
struct cmd_results *cmd_dim(int argc, char **argv) {
	struct sway_container *con = config->handler_context.container;
	bool dims = con && con->view ? !con->view->tide_dim.exempt : true;
	if (argc == 1 && strcasecmp(argv[0], "enable") == 0) {
		dims = true;
	} else if (argc == 1 && strcasecmp(argv[0], "disable") == 0) {
		dims = false;
	} else if (argc == 1 && strcasecmp(argv[0], "toggle") == 0) {
		dims = !dims;
	} else {
		return cmd_results_new(CMD_INVALID, "Expected dim enable|disable|toggle");
	}
	if (!con || !con->view) {
		return cmd_results_new(CMD_FAILURE, "No window to dim");
	}
	con->view->tide_dim.exempt = !dims;
	tide_dim_update_all();
	return cmd_results_new(CMD_SUCCESS, NULL);
}
