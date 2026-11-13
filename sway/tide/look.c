/*
 * Rounded corners and shadows (include/sway/tide/look.h says how): tide's
 * look on Hyprland, with wlroots' own scene graph and whichever renderer
 * it has.
 */
#include <drm_fourcc.h>
#include <errno.h>
#include <math.h>
#include <pixman.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <wlr/interfaces/wlr_buffer.h>
#include <wlr/render/allocator.h>
#include <wlr/render/color.h>
#include <wlr/render/pass.h>
#include <wlr/render/swapchain.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/render/wlr_texture.h>
#include <wlr/types/wlr_alpha_modifier_v1.h>
#include <wlr/types/wlr_buffer.h>
#include <wlr/types/wlr_color_management_v1.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_linux_drm_syncobj_v1.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_subcompositor.h>
#include <wlr/util/box.h>
#include <wlr/util/transform.h>
#include "config.h"
#include "log.h"
#include "sway/commands.h"
#include "sway/config.h"
#include "sway/output.h"
#include "sway/scene_descriptor.h"
#include "sway/server.h"
#include "sway/tide/dim.h"
#include "sway/tide/look.h"
#include "sway/tide/maximize.h"
#include "sway/tree/container.h"
#include "sway/tree/root.h"
#include "sway/tree/view.h"
#include "util.h"
#if WLR_HAS_XWAYLAND
#include "sway/tide/x11scale.h"
#endif

// How finely a corner's edge is smoothed: SUB by SUB samples to a pixel
#define SUB 4
#define LEVELS (SUB * SUB)

void tide_look_config_init(struct tide_look_config *look) {
	*look = (struct tide_look_config){
		.shadow_blur_radius = 8,
		.shadow_color = {0, 0, 0, 0x44 / 255.0f},
	};
}

/* The output with the largest scale, which the strips and shadows are
 * drawn at, or NULL with no output */
static struct sway_output *sharpest_output(void) {
	struct sway_output *sharpest = NULL;
	for (int i = 0; i < root->outputs->length; i++) {
		struct sway_output *output = root->outputs->items[i];
		if (output->wlr_output && (!sharpest
				|| output->wlr_output->scale > sharpest->wlr_output->scale)) {
			sharpest = output;
		}
	}
	return sharpest;
}

static float max_scale(void) {
	struct sway_output *output = sharpest_output();
	return output ? output->wlr_output->scale : 1;
}

static void schedule_frames(void) {
	for (int i = 0; i < root->outputs->length; i++) {
		struct sway_output *output = root->outputs->items[i];
		if (output->enabled && output->wlr_output) {
			wlr_output_schedule_frame(output->wlr_output);
		}
	}
}

/*
 * Rounded corners
 */

// The format the strips are drawn in: an output's, with alpha
static bool strip_format(struct wlr_drm_format *format) {
	for (int i = 0; i < root->outputs->length; i++) {
		struct sway_output *output = root->outputs->items[i];
		if (output->wlr_output && output->wlr_output->swapchain) {
			// Its modifiers are borrowed: the swapchain made copies them
			*format = output->wlr_output->swapchain->format;
			format->format = DRM_FORMAT_ARGB8888;
			return true;
		}
	}
	return false;
}

static void has_color(struct wlr_surface *surface, int sx, int sy, void *data) {
	if (wlr_surface_get_image_description_v1_data(surface)) {
		*(bool *)data = true;
	}
}

/* Whether view draws its own corners and shadow: a floating window with its
 * own decorations whose surface reaches past its geometry, as GTK's does
 * for its shadow. sway doesn't clip such a window to its geometry. */
static bool draws_its_own(struct sway_view *view) {
	return container_is_floating(view->container) && view->using_csd
		&& view->surface && (view->geometry.x != 0 || view->geometry.y != 0
			|| view->geometry.width != view->surface->current.width
			|| view->geometry.height != view->surface->current.height);
}

int tide_look_radius(struct sway_view *view) {
	struct sway_container *con = view->container;
	int r = config->tide_look.corner_radius;
	if (r <= 0 || !con || !view->surface || view->tide_look.failed) {
		return 0;
	}
	if (con->current.fullscreen_mode != FULLSCREEN_NONE
			|| con->pending.fullscreen_mode != FULLSCREEN_NONE) {
		return 0;
	}
	// A border or title bar of sway's around it would stay square
	if (con->current.content_x != con->current.x
			|| con->current.content_y != con->current.y
			|| con->current.content_width != con->current.width
			|| con->current.content_height != con->current.height) {
		return 0;
	}
	if (draws_its_own(view)) {
		return 0;
	}
#if WLR_HAS_XWAYLAND
	if (tide_x11_surface_scale(view->surface) != 1) {
		return 0;
	}
#endif
	// The strips are composited as sRGB, which would flatten HDR
	bool color = false;
	wlr_surface_for_each_surface(view->surface, has_color, &color);
	if (color) {
		return 0;
	}
	// A row at least is left between the strips
	if (con->current.content_width < 2 * r || con->current.content_height <= 2 * r) {
		return 0;
	}
	// Until an output has drawn, there's no format to draw the strips in
	struct wlr_drm_format format;
	if (!strip_format(&format)) {
		return 0;
	}
	return r;
}

/* The surface of a strip's window that takes input at a point of the
 * strip, and the point in it, or NULL: the strip is over rows the window's
 * surfaces are clipped out of */
static struct wlr_surface *strip_surface_at(struct wlr_scene_buffer *buffer,
		double sx, double sy, double *sub_x, double *sub_y) {
	// The strips' tree is in the view's
	struct wlr_scene_tree *view_tree =
		buffer->node.parent ? buffer->node.parent->node.parent : NULL;
	struct sway_view *view = view_tree
		? scene_descriptor_try_get(&view_tree->node, SWAY_SCENE_DESC_VIEW) : NULL;
	if (!view || !view->surface) {
		return NULL;
	}
	// From the strip to the window's content, then to its surface
	double x = sx + buffer->node.x + view->geometry.x;
	double y = sy + buffer->node.y + view->geometry.y;
	return wlr_surface_surface_at(view->surface, x, y, sub_x, sub_y);
}

/* A strip takes the pointer where a surface of its window would, so a hole
 * in the window's input region lets a click through, and node_at_coords
 * finds that surface (tide_look_surface_at) */
static bool strip_accepts_input(struct wlr_scene_buffer *buffer, double *sx, double *sy) {
	double sub_x, sub_y;
	return strip_surface_at(buffer, *sx, *sy, &sub_x, &sub_y) != NULL;
}

// The strips' tree and nodes, made the first time the window is rounded
static bool make_strips(struct sway_view *view) {
	struct tide_look_view *look = &view->tide_look;
	if (look->tree) {
		return true;
	}
	look->tree = wlr_scene_tree_create(view->scene_tree);
	if (!look->tree) {
		sway_log(SWAY_ERROR, "tide: can't make the tree of a window's rounded corners");
		return false;
	}
	// Over the content, and under the dim layer, which comes later
	wlr_scene_node_place_above(&look->tree->node, &view->content_tree->node);
	for (int i = 0; i < 2; i++) {
		look->strip[i] = wlr_scene_buffer_create(look->tree, NULL);
		if (!look->strip[i]) {
			sway_log(SWAY_ERROR, "tide: can't make a window's rounded corners");
			wlr_scene_node_destroy(&look->tree->node);
			look->tree = NULL;
			return false;
		}
		look->strip[i]->point_accepts_input = strip_accepts_input;
	}
	return true;
}

/* Puts view's strips where its corners' rows are, for the radius r, and
 * shows them, drawn or not: the pointer finds them from here on. False if
 * they couldn't be made. */
static bool place_strips(struct sway_view *view, int r) {
	struct sway_container *con = view->container;
	struct tide_look_view *look = &view->tide_look;
	if (!make_strips(view)) {
		return false;
	}
	wlr_scene_node_set_position(&look->tree->node,
		view->content_tree->node.x, view->content_tree->node.y);
	wlr_scene_node_set_position(&look->strip[0]->node, 0, 0);
	wlr_scene_node_set_position(&look->strip[1]->node, 0,
		con->current.content_height - r);
	for (int i = 0; i < 2; i++) {
		wlr_scene_buffer_set_dest_size(look->strip[i], con->current.content_width, r);
	}
	wlr_scene_node_set_enabled(&look->tree->node, true);
	return true;
}

void tide_look_clip(struct sway_view *view, struct wlr_box *clip) {
	struct tide_look_view *look = &view->tide_look;
	int r = tide_look_radius(view);
	/* The strips take the rows' place as the rows go, so there's no moment
	 * the pointer finds neither. While a resize shows the window's saved
	 * buffers, they stay as they were, with those. */
	if (!view->saved_surface_tree) {
		if (r > 0 && !place_strips(view, r)) {
			look->failed = true; // square from here on
			r = 0;
		}
		if (r == 0 && look->tree) {
			wlr_scene_node_set_enabled(&look->tree->node, false);
			for (int i = 0; i < 2; i++) {
				wlr_scene_buffer_set_buffer(look->strip[i], NULL);
			}
			tide_look_view_destroy(view); // the buffers, until it's rounded again
			look->drawn = 0;
		}
	}
	if (r != look->radius) {
		look->radius = r;
		tide_dim_update(view); // its layer is over the rows between the strips
	}
	if (r == 0) {
		return;
	}
	if (wlr_box_empty(clip)) {
		// Not clipped until now: a floating window with its own decorations
		*clip = (struct wlr_box){
			.x = view->geometry.x,
			.y = view->geometry.y,
			.width = view->container->current.content_width,
			.height = view->container->current.content_height,
		};
	}
	clip->y += r;
	clip->height -= 2 * r;
}

void tide_look_surface_at(struct wlr_scene_buffer *buffer,
		struct wlr_surface **surface, double *sx, double *sy) {
	if (buffer->point_accepts_input != strip_accepts_input) {
		return;
	}
	double sub_x, sub_y;
	struct wlr_surface *found = strip_surface_at(buffer, *sx, *sy, &sub_x, &sub_y);
	if (found) {
		*surface = found;
		*sx = sub_x;
		*sy = sub_y;
	}
}

struct stamp {
	uint64_t hash;
};

static void stamp_add(struct stamp *stamp, uint64_t value) {
	// FNV-1a, a value at a time
	stamp->hash = (stamp->hash ^ value) * 0x100000001b3;
}

static void stamp_surface(struct wlr_surface *surface, int sx, int sy, void *data) {
	struct stamp *stamp = data;
	stamp_add(stamp, (uintptr_t)surface);
	stamp_add(stamp, (uint32_t)sx);
	stamp_add(stamp, (uint32_t)sy);
	stamp_add(stamp, (uint32_t)surface->current.width);
	stamp_add(stamp, (uint32_t)surface->current.height);
	// Its opacity is drawn into the strips, and can change with no damage
	const struct wlr_alpha_modifier_surface_v1_state *alpha =
		wlr_alpha_modifier_v1_get_surface_state(surface);
	stamp_add(stamp, alpha ? (uint64_t)(alpha->multiplier * UINT32_MAX) : UINT32_MAX);
}

static void stamp_subsurface(struct wlr_surface *surface, int sx, int sy, void *data) {
	struct stamp *stamp = data;
	// The window's own surface says which of its commits count
	// (tide_look_committed); a subsurface's all do
	if (wlr_subsurface_try_from_wlr_surface(surface)) {
		stamp_add(stamp, surface->current.seq);
	}
}

static bool dim_shown(struct sway_view *view) {
	return view->tide_dim.layer && view->tide_dim.layer->node.enabled;
}

// Everything a strip's pixels come from
static uint64_t view_stamp(struct sway_view *view, int r, float scale) {
	struct sway_container *con = view->container;
	struct stamp stamp = {0xcbf29ce484222325};
	stamp_add(&stamp, (uintptr_t)con); // a window mapped again has a new one
	stamp_add(&stamp, (uint32_t)lroundf(con->alpha * 65535)); // drawn into them
	stamp_add(&stamp, (uint32_t)r);
	stamp_add(&stamp, (uint32_t)lroundf(scale * 1000));
	// The filter the surfaces are scaled into the strips with (draw_surface)
	struct sway_output *output = sharpest_output();
	stamp_add(&stamp, output ? (uint32_t)output->scale_filter : 0);
	stamp_add(&stamp, (uint32_t)con->current.content_width);
	stamp_add(&stamp, (uint32_t)con->current.content_height);
	stamp_add(&stamp, (uint32_t)view->geometry.x);
	stamp_add(&stamp, (uint32_t)view->geometry.y);
	stamp_add(&stamp, view->tide_look.commits);
	stamp_add(&stamp, dim_shown(view)
		? (uint32_t)lround(config->tide_dim_inactive * 255) + 1 : 0);
	wlr_surface_for_each_surface(view->surface, stamp_surface, &stamp);
	wlr_surface_for_each_surface(view->surface, stamp_subsurface, &stamp);
	return stamp.hash;
}

// What a strip is drawn from, as each of the window's surfaces is added
struct strip_draw {
	struct sway_view *view;
	struct wlr_render_pass *pass;
	int top; // the strip's first row, in the window's content
	float scale;
};

static void draw_surface(struct wlr_surface *surface, int sx, int sy, void *data) {
	struct strip_draw *draw = data;
	struct wlr_texture *texture = wlr_surface_get_texture(surface);
	if (!texture) {
		return;
	}
	// Where the surface is in the strip, in the strip's pixels
	double x = sx - draw->view->geometry.x;
	double y = sy - draw->view->geometry.y - draw->top;
	struct wlr_box dst = {
		.x = (int)lround(x * draw->scale),
		.y = (int)lround(y * draw->scale),
	};
	dst.width = (int)lround((x + surface->current.width) * draw->scale) - dst.x;
	dst.height = (int)lround((y + surface->current.height) * draw->scale) - dst.y;
	struct wlr_fbox src;
	wlr_surface_get_buffer_source_box(surface, &src);

	// As the scene graph draws a surface (wlroots' types/scene/surface.c)
	float alpha = 1;
	const struct wlr_alpha_modifier_surface_v1_state *alpha_state =
		wlr_alpha_modifier_v1_get_surface_state(surface);
	if (alpha_state) {
		alpha = (float)alpha_state->multiplier;
	}
	// And the window's opacity, which the scene graph gives each buffer
	alpha *= draw->view->container->alpha;
	/* The filter output_configure_scene gives the surface's own buffer, on
	 * the output the strip is drawn for, so its rows look as the rest do */
	struct sway_output *output = sharpest_output();
	bool smaller = dst.width < src.width || dst.height < src.height;
	enum wlr_scale_filter_mode filter =
		!smaller && output && output->scale_filter == SCALE_FILTER_NEAREST
		? WLR_SCALE_FILTER_NEAREST : WLR_SCALE_FILTER_BILINEAR;
	struct wlr_render_texture_options options = {
		.texture = texture,
		.src_box = src,
		.dst_box = dst,
		.alpha = &alpha,
		.filter_mode = filter,
		.transform = wlr_output_transform_invert(surface->current.transform),
		.blend_mode = WLR_RENDER_BLEND_MODE_PREMULTIPLIED,
	};
	struct wlr_linux_drm_syncobj_surface_v1_state *sync =
		wlr_linux_drm_syncobj_v1_get_surface_state(surface);
	if (sync && server.renderer->features.timeline) {
		options.wait_timeline = sync->acquire_timeline;
		options.wait_point = sync->acquire_point;
	}
	wlr_render_pass_add_texture(draw->pass, &options);
}

static void clear(struct wlr_render_pass *pass, int width, int height) {
	wlr_render_pass_add_rect(pass, &(struct wlr_render_rect_options){
		.box = {0, 0, width, height},
		.color = {0, 0, 0, 0},
		.blend_mode = WLR_RENDER_BLEND_MODE_NONE,
	});
}

/* How much of the pixel at x, y is inside the quarter circle of radius r
 * about cx, cy, in LEVELS: the corner's edge, smoothed */
static int coverage(int x, int y, double cx, double cy, double r) {
	int inside = 0;
	for (int j = 0; j < SUB; j++) {
		for (int i = 0; i < SUB; i++) {
			double px = x + (i + 0.5) / SUB - cx;
			double py = y + (j + 0.5) / SUB - cy;
			if (px * px + py * py <= r * r) {
				inside++;
			}
		}
	}
	return inside;
}

/* The strip's pixels by how much of each is inside the window's rounded
 * outline: level[LEVELS] the whole ones, level[n] those n LEVELS-ths in.
 * width and height are the strip's, in pixels, r the radius in pixels,
 * and bottom whether it's the window's bottom rows. */
static void mask(pixman_region32_t level[static LEVELS + 1], int width, int height,
		double r, bool bottom) {
	for (int n = 0; n <= LEVELS; n++) {
		pixman_region32_init(&level[n]);
	}
	int corner = (int)ceil(r);
	if (corner > width / 2) {
		corner = width / 2;
	}
	// Between the corners every pixel is in
	pixman_region32_union_rect(&level[LEVELS], &level[LEVELS],
		corner, 0, width - 2 * corner, height);
	// The circles' centers: r in from each side, and from the top or bottom
	double cy = bottom ? height - r : r;
	for (int y = 0; y < height; y++) {
		for (int x = 0; x < corner; x++) {
			int n = bottom ? (y + 1 <= cy ? LEVELS : coverage(x, y, r, cy, r))
				: (y >= cy ? LEVELS : coverage(x, y, r, cy, r));
			if (n > 0) {
				pixman_region32_union_rect(&level[n], &level[n], x, y, 1, 1);
				// And its mirror image in the other corner
				pixman_region32_union_rect(&level[n], &level[n],
					width - 1 - x, y, 1, 1);
			}
		}
	}
}

/* Draws one strip of view, its top rows or its bottom ones, at scale, and
 * shows it. False if it couldn't be drawn. */
static bool draw_strip(struct sway_view *view, int which, int r, float scale) {
	struct sway_container *con = view->container;
	struct tide_look_view *look = &view->tide_look;
	int width = (int)ceil(con->current.content_width * scale);
	int height = (int)ceil(r * scale);

	struct wlr_swapchain *chain = look->chain[which];
	// A renderer made again after a GPU reset took the allocator with it
	if (!chain || !chain->allocator || chain->width != width || chain->height != height) {
		struct wlr_drm_format format;
		if (!strip_format(&format)) {
			sway_log(SWAY_ERROR, "tide: no format for a window's rounded corners");
			return false;
		}
		wlr_swapchain_destroy(chain);
		chain = look->chain[which] =
			wlr_swapchain_create(server.allocator, width, height, &format);
		if (!chain) {
			sway_log(SWAY_ERROR, "tide: can't make the buffers of a window's rounded corners");
			return false;
		}
	}

	// The window's surfaces, and its dimming, as they'd be with square corners
	struct wlr_buffer *whole = wlr_swapchain_acquire(chain);
	struct wlr_buffer *cut = wlr_swapchain_acquire(chain);
	struct wlr_texture *texture = NULL;
	bool ok = false;
	if (!whole || !cut) {
		sway_log(SWAY_ERROR, "tide: no buffer free for a window's rounded corners");
		goto out;
	}
	struct wlr_render_pass *pass =
		wlr_renderer_begin_buffer_pass(server.renderer, whole, NULL);
	if (!pass) {
		sway_log(SWAY_ERROR, "tide: can't draw a window's rounded corners");
		goto out;
	}
	clear(pass, width, height);
	struct strip_draw draw = {
		.view = view,
		.pass = pass,
		.top = which == 0 ? 0 : con->current.content_height - r,
		.scale = scale,
	};
	wlr_surface_for_each_surface(view->surface, draw_surface, &draw);
	if (dim_shown(view)) {
		wlr_render_pass_add_rect(pass, &(struct wlr_render_rect_options){
			.box = {0, 0, width, height},
			// At the window's opacity, as its dim layer between the strips is
			.color = {0, 0, 0, (float)config->tide_dim_inactive * con->alpha},
			.blend_mode = WLR_RENDER_BLEND_MODE_PREMULTIPLIED,
		});
	}
	if (!wlr_render_pass_submit(pass)) {
		sway_log(SWAY_ERROR, "tide: drawing a window's rounded corners failed");
		goto out;
	}

	// Then each pixel scaled by how much of it is inside the corners
	texture = wlr_texture_from_buffer(server.renderer, whole);
	pass = texture ? wlr_renderer_begin_buffer_pass(server.renderer, cut, NULL) : NULL;
	if (!pass) {
		sway_log(SWAY_ERROR, "tide: can't cut a window's rounded corners");
		goto out;
	}
	clear(pass, width, height);
	pixman_region32_t level[LEVELS + 1];
	mask(level, width, height, r * scale, which == 1);
	for (int n = 1; n <= LEVELS; n++) {
		if (pixman_region32_not_empty(&level[n])) {
			float alpha = (float)n / LEVELS;
			wlr_render_pass_add_texture(pass, &(struct wlr_render_texture_options){
				.texture = texture,
				.dst_box = {0, 0, width, height},
				.alpha = &alpha,
				.clip = &level[n],
				.blend_mode = WLR_RENDER_BLEND_MODE_NONE,
			});
		}
	}
	for (int n = 0; n <= LEVELS; n++) {
		pixman_region32_fini(&level[n]);
	}
	if (!wlr_render_pass_submit(pass)) {
		sway_log(SWAY_ERROR, "tide: cutting a window's rounded corners failed");
		goto out;
	}

	struct wlr_scene_buffer *strip = look->strip[which];
	wlr_scene_buffer_set_buffer(strip, cut);
	// A scale that isn't whole leaves part of the last pixel past the window
	wlr_scene_buffer_set_source_box(strip, &(struct wlr_fbox){0, 0,
		con->current.content_width * (double)scale, r * (double)scale});
	wlr_scene_buffer_set_dest_size(strip, con->current.content_width, r);
	ok = true;
out:
	if (texture) {
		wlr_texture_destroy(texture);
	}
	if (whole) {
		wlr_buffer_unlock(whole);
	}
	if (cut) {
		wlr_buffer_unlock(cut);
	}
	return ok;
}

// Square from here on: the strips couldn't be drawn
static void fail(struct sway_view *view) {
	view->tide_look.failed = true;
	view_center_and_clip_surface(view); // which hides the strips
}

void tide_look_frame(struct sway_container *con) {
	struct sway_view *view = con->view;
	if (!view || !view->surface) {
		return;
	}
	struct tide_look_view *look = &view->tide_look;
	// While a resize shows its saved buffers, the strips stay as they were
	if (view->saved_surface_tree) {
		return;
	}
	if (tide_look_radius(view) != look->radius) {
		// Clips its surfaces, and sets look->radius
		view_center_and_clip_surface(view);
	}
	int r = look->radius;
	if (r == 0 || !look->tree || !look->tree->node.enabled) {
		return;
	}
	float scale = max_scale();
	uint64_t stamp = view_stamp(view, r, scale);
	if (stamp != look->drawn) {
		if (!draw_strip(view, 0, r, scale) || !draw_strip(view, 1, r, scale)) {
			fail(view);
			return;
		}
		look->drawn = stamp;
	}
}

void tide_look_committed(struct sway_view *view) {
	struct sway_container *con = view->container;
	int r = view->tide_look.radius;
	if (r == 0 || !con || !view->surface) {
		return;
	}
	// The commit's damage, in the surface's coordinates, as the rows are
	pixman_region32_t damage;
	pixman_region32_init(&damage);
	wlr_surface_get_effective_damage(view->surface, &damage);
	pixman_box32_t top = {
		.x1 = view->geometry.x,
		.y1 = view->geometry.y,
		.x2 = view->geometry.x + con->current.content_width,
		.y2 = view->geometry.y + r,
	};
	pixman_box32_t bottom = top;
	bottom.y1 = view->geometry.y + con->current.content_height - r;
	bottom.y2 = view->geometry.y + con->current.content_height;
	bool damaged =
		pixman_region32_contains_rectangle(&damage, &top) != PIXMAN_REGION_OUT
		|| pixman_region32_contains_rectangle(&damage, &bottom) != PIXMAN_REGION_OUT;
	pixman_region32_fini(&damage);
	if (damaged) {
		view->tide_look.commits++;
	}
	/* The rows are clipped out of the scene graph's surfaces, so damage to
	 * them alone asks it for no frame. Nor does a subsurface wholly in
	 * them that this commit moved or restacked, which only the window's
	 * commit applies: with any subsurface, ask, and the frame draws the
	 * strips again if where the subsurfaces are has changed (view_stamp). */
	if (damaged || !wl_list_empty(&view->surface->current.subsurfaces_below)
			|| !wl_list_empty(&view->surface->current.subsurfaces_above)) {
		schedule_frames();
	}
}

/* Whether a surface whose rows, in view's surface's coordinates, are from
 * top to bottom has any in the strips' rows, and whether it has none
 * outside them */
static void strip_rows(struct sway_view *view, int top, int bottom,
		bool *some, bool *all) {
	int r = view->tide_look.radius;
	int first = view->geometry.y; // the window's first row
	int last = first + view->container->current.content_height; // past its last
	*some = top < first + r || bottom > last - r;
	*all = bottom <= first + r || top >= last - r;
}

/* A surface's commits, and its going, heard from when it's made until it's
 * destroyed */
struct surface_watch {
	struct wlr_surface *surface;
	struct wl_listener commit;
	struct wl_listener unmap;
	struct wl_listener destroy;
};

/* Asks for a frame if surface is a subsurface, at any depth, of a rounded
 * window: the frame draws the strips again if what they're drawn from has
 * changed, which every subsurface's commits, place and order are in
 * (view_stamp). Whatever its own rows, as its commit also applies where
 * its own subsurfaces are, and theirs may be in the strips' rows. */
static void subsurface_changed(struct wlr_surface *surface) {
	// A window's own surface is tide_look_committed's
	if (config->tide_look.corner_radius <= 0
			|| !wlr_subsurface_try_from_wlr_surface(surface)) {
		return;
	}
	struct sway_view *view =
		view_from_wlr_surface(wlr_surface_get_root_surface(surface));
	if (view && view->container && view->tide_look.radius > 0) {
		schedule_frames();
	}
}

static void handle_surface_commit(struct wl_listener *listener, void *data) {
	struct surface_watch *watch = wl_container_of(listener, watch, commit);
	subsurface_changed(watch->surface);
}

// Unmapped, as it is before it's destroyed: the strips show it no more
static void handle_surface_unmap(struct wl_listener *listener, void *data) {
	struct surface_watch *watch = wl_container_of(listener, watch, unmap);
	subsurface_changed(watch->surface);
}

static void handle_surface_destroy(struct wl_listener *listener, void *data) {
	struct surface_watch *watch = wl_container_of(listener, watch, destroy);
	wl_list_remove(&watch->commit.link);
	wl_list_remove(&watch->unmap.link);
	wl_list_remove(&watch->destroy.link);
	free(watch);
}

static void handle_new_surface(struct wl_listener *listener, void *data) {
	struct wlr_surface *surface = data;
	struct surface_watch *watch = calloc(1, sizeof(*watch));
	if (!watch) {
		sway_log(SWAY_ERROR, "tide: can't watch a surface's commits; "
			"if it's a subsurface in a window's rounded corners' rows, they may show late");
		return;
	}
	watch->surface = surface;
	watch->commit.notify = handle_surface_commit;
	wl_signal_add(&surface->events.commit, &watch->commit);
	watch->unmap.notify = handle_surface_unmap;
	wl_signal_add(&surface->events.unmap, &watch->unmap);
	watch->destroy.notify = handle_surface_destroy;
	wl_signal_add(&surface->events.destroy, &watch->destroy);
}

static struct wl_listener new_surface = { .notify = handle_new_surface };

void tide_look_init(void) {
	wl_signal_add(&server.compositor->events.new_surface, &new_surface);
}

void tide_look_fini(void) {
	// wlroots asserts that nothing still listens as the compositor goes
	wl_list_remove(&new_surface.link);
}

struct frame_done {
	struct sway_output *output;
	const struct timespec *when;
	struct sway_view *view; // the window whose surfaces are being told
};

static void frame_done_surface(struct wlr_surface *surface, int sx, int sy, void *data) {
	struct frame_done *done = data;
	bool some, all;
	strip_rows(done->view, sy, sy + surface->current.height, &some, &all);
	if (all) {
		wlr_surface_send_frame_done(surface, done->when);
	}
}

static void frame_done_container(struct sway_container *con, void *data) {
	struct frame_done *done = data;
	struct sway_view *view = con->view;
	if (!view || !view->surface || view->tide_look.radius == 0) {
		return;
	}
	/* Shown mostly on this output, as the scene graph picks the output
	 * whose frames a surface is told of */
	struct tide_look_view *look = &view->tide_look;
	if (!look->tree || !look->tree->node.enabled
			|| (look->strip[0]->primary_output != done->output->scene_output
				&& look->strip[1]->primary_output != done->output->scene_output)) {
		return;
	}
	done->view = view;
	wlr_surface_for_each_surface(view->surface, frame_done_surface, done);
}

void tide_look_frame_done(struct sway_output *output, const struct timespec *when) {
	if (config->tide_look.corner_radius <= 0) {
		return;
	}
	struct frame_done done = { .output = output, .when = when };
	root_for_each_container(frame_done_container, &done);
}

void tide_look_view_destroy(struct sway_view *view) {
	// The strips' nodes go with the view's tree
	for (int i = 0; i < 2; i++) {
		wlr_swapchain_destroy(view->tide_look.chain[i]);
		view->tide_look.chain[i] = NULL;
	}
}

/*
 * Shadows
 */

/* A shadow's pixels, shared by every window with the same radius: its top
 * left corner, the edge above a window and the edge left of one. The other
 * corners and edges are these mirrored. */
struct shadow_pixels {
	struct wlr_buffer base;
	uint32_t *data;
};

struct shadow_tiles {
	int radius, blur;
	float scale;
	float color[4];
	struct shadow_pixels *corner, *above, *left;
};

// One set for rounded windows and one for square ones
static struct shadow_tiles tiles[2];

static void shadow_pixels_destroy(struct wlr_buffer *buffer) {
	struct shadow_pixels *p = wl_container_of(buffer, p, base);
	wlr_buffer_finish(buffer);
	free(p->data);
	free(p);
}

static bool shadow_pixels_begin(struct wlr_buffer *buffer, uint32_t flags,
		void **data, uint32_t *format, size_t *stride) {
	struct shadow_pixels *p = wl_container_of(buffer, p, base);
	if (flags & WLR_BUFFER_DATA_PTR_ACCESS_WRITE) {
		return false;
	}
	*data = p->data;
	*format = DRM_FORMAT_ARGB8888;
	*stride = sizeof(uint32_t) * buffer->width;
	return true;
}

static void shadow_pixels_end(struct wlr_buffer *buffer) {
	// Nothing to release: the pixels are the buffer's own
}

static const struct wlr_buffer_impl shadow_pixels_impl = {
	.destroy = shadow_pixels_destroy,
	.begin_data_ptr_access = shadow_pixels_begin,
	.end_data_ptr_access = shadow_pixels_end,
};

bool tide_look_is_strip(struct wlr_scene_buffer *buffer) {
	return buffer->point_accepts_input == strip_accepts_input;
}

bool tide_look_is_shadow(struct wlr_scene_buffer *buffer) {
	return buffer->buffer && buffer->buffer->impl == &shadow_pixels_impl;
}

static struct shadow_pixels *shadow_pixels_create(int width, int height) {
	struct shadow_pixels *p = calloc(1, sizeof(*p));
	uint32_t *data = calloc((size_t)width * height, sizeof(*data));
	if (!p || !data) {
		sway_log(SWAY_ERROR, "tide: can't allocate a shadow's pixels");
		free(p);
		free(data);
		return NULL;
	}
	wlr_buffer_init(&p->base, &shadow_pixels_impl, width, height);
	p->data = data;
	return p;
}

/* How dark the shadow is at distance d outside the window, of 1: Hyprland's
 * falloff with its render_power 2, gone at the blur radius */
static double falloff(double d, int blur) {
	if (d < 0 || d >= blur) {
		return 0; // under the window, where one that's see-through shows none
	}
	double t = 1 - d / blur;
	return t * t;
}

// A premultiplied ARGB pixel of the shadow's color at strength, of 1
static uint32_t shadow_pixel(const float color[static 4], double strength) {
	double a = color[3] * strength;
	return (uint32_t)lround(a * 255) << 24
		| (uint32_t)lround(color[0] * a * 255) << 16
		| (uint32_t)lround(color[1] * a * 255) << 8
		| (uint32_t)lround(color[2] * a * 255);
}

static void tiles_drop(struct shadow_tiles *t) {
	struct shadow_pixels *all[] = {t->corner, t->above, t->left};
	for (size_t i = 0; i < sizeof(all) / sizeof(all[0]); i++) {
		if (all[i]) {
			// Freed once the last shadow showing it takes the new one
			wlr_buffer_drop(&all[i]->base);
		}
	}
	*t = (struct shadow_tiles){0};
}

// The tiles for windows rounded by radius, made if anything about them is new
static struct shadow_tiles *shadow_tiles(int radius) {
	const struct tide_look_config *look = &config->tide_look;
	struct shadow_tiles *t = &tiles[radius > 0];
	int blur = look->shadow_blur_radius;
	float scale = max_scale();
	if (t->corner && t->radius == radius && t->blur == blur && t->scale == scale
			&& memcmp(t->color, look->shadow_color, sizeof(t->color)) == 0) {
		return t;
	}
	tiles_drop(t);
	int reach = (int)ceil(blur * scale); // the shadow's width, in pixels
	int side = (int)ceil((blur + radius) * scale); // the corner's
	t->corner = shadow_pixels_create(side, side);
	t->above = shadow_pixels_create(1, reach);
	t->left = shadow_pixels_create(reach, 1);
	if (!t->corner || !t->above || !t->left) {
		tiles_drop(t);
		return NULL;
	}
	// The corner: by the distance to the window's rounded outline
	for (int y = 0; y < side; y++) {
		for (int x = 0; x < side; x++) {
			double sum = 0;
			for (int j = 0; j < SUB; j++) {
				for (int i = 0; i < SUB; i++) {
					// From the corner's circle's center, in layout pixels
					double px = radius + blur - (x + (i + 0.5) / SUB) / scale;
					double py = radius + blur - (y + (j + 0.5) / SUB) / scale;
					sum += falloff(hypot(px, py) - radius, blur);
				}
			}
			t->corner->data[y * side + x] =
				shadow_pixel(look->shadow_color, sum / LEVELS);
		}
	}
	// The edges: by the distance to the window's side
	for (int i = 0; i < reach; i++) {
		double sum = 0;
		for (int j = 0; j < SUB; j++) {
			sum += falloff(blur - (i + (j + 0.5) / SUB) / scale, blur);
		}
		t->above->data[i] = t->left->data[i] =
			shadow_pixel(look->shadow_color, sum / SUB);
	}
	t->radius = radius;
	t->blur = blur;
	t->scale = scale;
	memcpy(t->color, look->shadow_color, sizeof(t->color));
	return t;
}

// Clicks and the pointer go through a shadow to what's under it
static bool shadow_no_input(struct wlr_scene_buffer *buffer, double *sx, double *sy) {
	return false;
}

/* Whether con has a shadow: a floating window or container that isn't
 * fullscreen or maximized, as in tidewm, and doesn't draw its own */
static bool shadowed(struct sway_container *con) {
	return config->tide_look.shadows && config->tide_look.shadow_blur_radius > 0
		&& config->tide_look.shadow_color[3] > 0
		&& container_is_floating(con)
		&& con->current.fullscreen_mode == FULLSCREEN_NONE
		&& !tide_maximized(con)
		&& !(con->view && draws_its_own(con->view))
		&& con->current.width > 0 && con->current.height > 0;
}

static bool make_shadow(struct sway_container *con) {
	struct tide_look_shadow *shadow = &con->tide_shadow;
	if (shadow->tree) {
		return true;
	}
	shadow->tree = wlr_scene_tree_create(con->scene_tree);
	if (!shadow->tree) {
		sway_log(SWAY_ERROR, "tide: can't make a window's shadow");
		return false;
	}
	// Under the window's borders and content
	wlr_scene_node_lower_to_bottom(&shadow->tree->node);
	static const enum wl_output_transform mirror[8] = {
		// The corners: top left, top right, bottom left, bottom right
		WL_OUTPUT_TRANSFORM_NORMAL, WL_OUTPUT_TRANSFORM_FLIPPED,
		WL_OUTPUT_TRANSFORM_FLIPPED_180, WL_OUTPUT_TRANSFORM_180,
		// The edges: above, below, left, right
		WL_OUTPUT_TRANSFORM_NORMAL, WL_OUTPUT_TRANSFORM_FLIPPED_180,
		WL_OUTPUT_TRANSFORM_NORMAL, WL_OUTPUT_TRANSFORM_FLIPPED,
	};
	for (int i = 0; i < 8; i++) {
		struct wlr_scene_buffer *part = wlr_scene_buffer_create(shadow->tree, NULL);
		if (!part) {
			sway_log(SWAY_ERROR, "tide: can't make part of a window's shadow");
			wlr_scene_node_destroy(&shadow->tree->node);
			*shadow = (struct tide_look_shadow){0};
			return false;
		}
		part->point_accepts_input = shadow_no_input;
		wlr_scene_buffer_set_transform(part, mirror[i]);
		// An edge is one pixel stretched along the window's side
		wlr_scene_buffer_set_filter_mode(part, WLR_SCALE_FILTER_NEAREST);
		shadow->part[i] = part;
	}
	return true;
}

static void place(struct wlr_scene_buffer *part, struct shadow_pixels *pixels,
		int x, int y, int width, int height) {
	bool shown = width > 0 && height > 0;
	wlr_scene_node_set_enabled(&part->node, shown);
	if (!shown) {
		return;
	}
	if (part->buffer != &pixels->base) {
		wlr_scene_buffer_set_buffer(part, &pixels->base);
	}
	wlr_scene_node_set_position(&part->node, x, y);
	wlr_scene_buffer_set_dest_size(part, width, height);
}

static void update_shadow(struct sway_container *con) {
	struct tide_look_shadow *shadow = &con->tide_shadow;
	if (!shadowed(con)) {
		if (shadow->tree) {
			wlr_scene_node_set_enabled(&shadow->tree->node, false);
		}
		return;
	}
	int width = con->current.width, height = con->current.height;
	int blur = config->tide_look.shadow_blur_radius;
	int r = con->view ? tide_look_radius(con->view) : 0;
	struct shadow_tiles *t = shadow_tiles(r);
	if (!t || !make_shadow(con)) {
		return;
	}
	int side = blur + r; // a corner reaches under the window's rounded one
	struct wlr_scene_buffer **part = shadow->part;
	place(part[0], t->corner, -blur, -blur, side, side);
	place(part[1], t->corner, width - r, -blur, side, side);
	place(part[2], t->corner, -blur, height - r, side, side);
	place(part[3], t->corner, width - r, height - r, side, side);
	place(part[4], t->above, r, -blur, width - 2 * r, blur);
	place(part[5], t->above, r, height, width - 2 * r, blur);
	place(part[6], t->left, -blur, r, blur, height - 2 * r);
	place(part[7], t->left, width, r, blur, height - 2 * r);
	wlr_scene_node_set_enabled(&shadow->tree->node, true);
}

static void update_container(struct sway_container *con, void *data) {
	struct sway_view *view = con->view;
	if (view && view->surface && !view->saved_surface_tree
			&& tide_look_radius(view) != view->tide_look.radius) {
		// The clip its surfaces have is for another radius
		view_center_and_clip_surface(view);
	}
	update_shadow(con);
}

void tide_look_update_all(void) {
	if (!root || !config) {
		return;
	}
	root_for_each_container(update_container, NULL);
}

/*
 * The config's lines
 */

// A whole number of pixels from 0 to max, or -1
static int pixels(const char *arg, int max) {
	char *end = NULL;
	errno = 0;
	long v = strtol(arg, &end, 10);
	if (end == arg || *end || errno || v < 0 || v > max) {
		return -1;
	}
	return (int)v;
}

// corner_radius PIXELS
struct cmd_results *cmd_corner_radius(int argc, char **argv) {
	int v = argc == 1 ? pixels(argv[0], 100) : -1;
	if (v < 0) {
		return cmd_results_new(CMD_INVALID,
			"corner_radius takes a number of pixels from 0 to 100");
	}
	config->tide_look.corner_radius = v;
	return cmd_results_new(CMD_SUCCESS, NULL);
}

// shadows enable|disable
struct cmd_results *cmd_shadows(int argc, char **argv) {
	if (argc == 1 && strcasecmp(argv[0], "enable") == 0) {
		config->tide_look.shadows = true;
	} else if (argc == 1 && strcasecmp(argv[0], "disable") == 0) {
		config->tide_look.shadows = false;
	} else {
		return cmd_results_new(CMD_INVALID, "Expected shadows enable|disable");
	}
	return cmd_results_new(CMD_SUCCESS, NULL);
}

// shadow_blur_radius PIXELS
struct cmd_results *cmd_shadow_blur_radius(int argc, char **argv) {
	int v = argc == 1 ? pixels(argv[0], 100) : -1;
	if (v < 0) {
		return cmd_results_new(CMD_INVALID,
			"shadow_blur_radius takes a number of pixels from 0 to 100");
	}
	config->tide_look.shadow_blur_radius = v;
	return cmd_results_new(CMD_SUCCESS, NULL);
}

// shadow_color #RRGGBB|#RRGGBBAA
struct cmd_results *cmd_shadow_color(int argc, char **argv) {
	uint32_t color;
	if (argc != 1 || !parse_color(argv[0], &color)) {
		return cmd_results_new(CMD_INVALID,
			"shadow_color takes a color as #RRGGBB or #RRGGBBAA");
	}
	color_to_rgba(config->tide_look.shadow_color, color);
	return cmd_results_new(CMD_SUCCESS, NULL);
}
