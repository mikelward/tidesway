#ifndef _SWAY_TIDE_LOOK_H
#define _SWAY_TIDE_LOOK_H

/*
 * Rounded corners and shadows, tide's look on Hyprland (its hyprland.lua's
 * rounding and shadow): corner_radius rounds each window's corners, and
 * shadows puts a soft shadow around each floating window, around the
 * focused one, or around them all.
 *
 * wlroots' scene graph draws a buffer as a rectangle, so a rounded
 * window's surfaces are clipped to the rows between its corners, and the
 * rows the corners are in, a strip at its top and one at its bottom, are
 * drawn here: its surfaces composited by the renderer, whichever it is,
 * with the corners cut out, into a buffer of the strip's own. A shadow is
 * eight more buffers, its corners and edges, under the window.
 *
 * A floating window's shadow is in its own tree, so it falls on what's
 * under the window. Tiles touch, so a tile's can only fall on the tiles
 * beside it, and sway stacks those in the order it lays them out. A tile's
 * shadow is in one of two trees of its own instead, as in tidewm: over
 * every tile if it's the focused one, and under them all if not, where it
 * shows in the gaps between them, if there are any. Either is cut off at
 * the edge of its output's window area, so it doesn't reach a bar or the
 * next output.
 */

#include <stdbool.h>
#include <stdint.h>
#include <time.h>
#include <wayland-server-core.h>

struct sway_container;
struct sway_output;
struct sway_view;
struct wlr_box;
struct wlr_scene_buffer;
struct wlr_scene_tree;
struct wlr_surface;
struct wlr_swapchain;

// Which windows cast a shadow
enum tide_shadows {
	TIDE_SHADOWS_NONE, // the default
	TIDE_SHADOWS_FLOATING, // each floating one
	TIDE_SHADOWS_ACTIVE, // the focused one, tiled or floating
	TIDE_SHADOWS_ALL,
};

// The look's lines, held in struct sway_config
struct tide_look_config {
	int corner_radius; // in layout pixels; 0, the default, is square
	enum tide_shadows shadows;
	int shadow_blur_radius; // how far a shadow reaches past its window
	float shadow_color[4]; // red, green, blue and alpha, not premultiplied
};

// A window's rounded corners, held in its struct sway_view
struct tide_look_view {
	struct wlr_scene_tree *tree; // the strips, over its content
	struct wlr_scene_buffer *strip[2]; // its top rows and its bottom ones
	struct wlr_swapchain *chain[2]; // each strip's buffers
	int radius; // what its surfaces are clipped for now; 0 is square
	uint64_t drawn; // what the strips were last drawn from
	uint32_t commits; // its surface's commits that damaged the strips' rows
	bool failed; // the strips couldn't be drawn, so it stays square
};

// A window's shadow, held in its struct sway_container
struct tide_look_shadow {
	/* Under a floating window's borders and content, in its own tree; a
	 * tile's is over or under every tile */
	struct wlr_scene_tree *tree;
	struct wlr_scene_buffer *part[8]; // its four corners, then its four edges
	struct wl_listener destroy; // the tree's, whichever tree it's in
};

// sway's defaults: square corners and no shadows
void tide_look_config_init(struct tide_look_config *look);

/* The radius view's corners are rounded by, as it's laid out now, or 0: a
 * fullscreen window isn't, nor one with a border or title bar of sway's,
 * one that draws its own decorations while floating, an X11 window in the
 * outputs' own pixels, or one with a color description */
int tide_look_radius(struct sway_view *view);

/* Takes the rows of view's corners off clip, the box its surfaces are
 * clipped to, in view_center_and_clip_surface */
void tide_look_clip(struct sway_view *view, struct wlr_box *clip);

/* Brings con's strips up to date before a frame, from
 * output_configure_scene: drawn again if its size, its surfaces or its
 * dimming changed */
void tide_look_frame(struct sway_container *con);

// A commit on view's surface, which a frame shows if it's in the strips' rows
void tide_look_committed(struct sway_view *view);

/* Hears every surface's commits from here on, once the compositor global
 * is made: a subsurface's own, with no commit of its window's, is shown by
 * a frame too if it's in the strips' rows */
void tide_look_init(void);

// Stops hearing of new surfaces, before the compositor global goes
void tide_look_fini(void);

/* Tells the surfaces shown only in a strip on output that a frame was
 * shown, after the scene graph's own surfaces are told: one wholly in a
 * window's corners' rows is clipped out of the scene graph, which never
 * tells it */
void tide_look_frame_done(struct sway_output *output, const struct timespec *when);

/* The surface under a point of a strip, and the point in it, for
 * node_at_coords: the strips are over rows the surfaces are clipped out
 * of. Leaves them alone if buffer isn't a strip. */
void tide_look_surface_at(struct wlr_scene_buffer *buffer,
	struct wlr_surface **surface, double *sx, double *sy);

/* Whether buffer is a strip, which output_configure_scene leaves opaque:
 * its window's opacity is drawn into it, surface by surface and into its
 * dimming, as the scene graph applies it to each of the window's buffers */
bool tide_look_is_strip(struct wlr_scene_buffer *buffer);

/* Whether buffer is part of a shadow, whose filter output_configure_scene
 * leaves nearest, as a dim layer's */
bool tide_look_is_shadow(struct wlr_scene_buffer *buffer);

/* Whether con casts a shadow, for GET_TREE's shadow, as tidewm's: as the
 * config and the commands so far have it, whether or not it's laid out
 * yet, and though another window may be over it. One on a workspace not
 * shown casts none. */
bool tide_look_shadow(struct sway_container *con);

/* How see-through a shadow's buffer is, for output_configure_scene, which
 * has opacity for it: a tile's shadow isn't in its window's tree, so it
 * takes its window's own, fading in as it opens */
float tide_look_shadow_opacity(struct wlr_scene_buffer *buffer, float opacity);

/* Brings every window's clip and shadow up to date: after a transaction
 * lays the windows out, or the config is reloaded */
void tide_look_update_all(void);

/* Focus moved, from seat_set_focus: the focused window's shadow goes with
 * it at once */
void tide_look_focused(void);

// Takes con's shadow with it, as it's destroyed
void tide_look_container_destroy(struct sway_container *con);

// Frees what view's strips were drawn into, as it's destroyed
void tide_look_view_destroy(struct sway_view *view);

#endif
