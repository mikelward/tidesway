#ifndef _SWAY_TIDE_X11SCALE_H
#define _SWAY_TIDE_X11SCALE_H
#include <stdbool.h>
#include <stdint.h>
#include <wlr/config.h>

/*
 * X11 windows sharp at a fractional scale, as tidewm's x11.h has them
 * (tide's SPEC §17). With xwayland_scaling disable, as Hyprland's
 * xwayland:force_zero_scaling, X11's pixels are the outputs' own: an X11
 * window is configured in its output's pixels, and the scene shows its
 * buffer at that size over the output's scale, one pixel for one, so
 * it's sharp. An app that scales itself, by GDK_SCALE, QT_SCALE_FACTOR or
 * Xft.dpi, is then the size it would be scaled; one that doesn't is
 * small. By default X11 windows are scaled, as in sway, and blurry at a
 * fractional scale.
 *
 * X11 has one space for every output, so each output's box in it is its
 * place in the layout spread by the largest scale, so that none overlap,
 * and its size in its own pixels. Xwayland hears of those boxes over a
 * zxdg_output_manager_v1 of tidesway's own, which only Xwayland sees, in
 * place of wlroots' one, so its screen and RandR match.
 */

struct sway_output;
struct sway_view;
struct sway_container;
struct wl_client;
struct wl_global;
struct wlr_surface;

// Starts Xwayland's xdg-output, once outputs and Xwayland are made
void tide_x11_init(void);

/* Whether client may see global: 1 or 0 for the globals this decides,
 * Xwayland's xdg-output and wlroots', or -1 for the rest */
int tide_x11_filter(const struct wl_client *client, const struct wl_global *global);

/* Outputs were configured, enabled or not: Xwayland hears their boxes,
 * and X11 windows are configured again */
void tide_x11_outputs_changed(void);

// Before the server and its output layout go
void tide_x11_fini(void);

// The config's xwayland_scaling, now in force: after a reload
void tide_x11_apply(void);

#if WLR_HAS_XWAYLAND
struct wlr_xwayland_surface;
struct wlr_scene_surface;
struct wlr_box;

/* The size and place to configure view at, from lx, ly, width by height
 * in the layout: in its output's pixels, with xwayland_scaling disable.
 * Returns a serial its commit acknowledges by (tide_x11_acked), or 0 for
 * sway's way, by geometry. */
uint32_t tide_x11_configure(struct sway_view *view, double *lx, double *ly,
	int *width, int *height);

/* Whether configuring view at lx, ly, width by height in the layout
 * would send what its last configure did, in X11's pixels, which a
 * client answers with nothing: below scale 1, layout sizes share them */
bool tide_x11_configures_nothing(struct sway_view *view, double lx, double ly,
	int width, int height);

/* Whether view's commit, at x, y, width by height in X11's space, is
 * its last configure's, and so acknowledges its transaction: a size or
 * place in X11's pixels may not convert back to the layout's exactly */
bool tide_x11_acked(struct sway_view *view, int x, int y, int width, int height);

// A size of view's in X11's pixels, in the layout
int tide_x11_to_layout_size(struct sway_view *view, int size);

/* A size an X11 window asks for, in X11's pixels, in the layout on the
 * output it opens on or is on */
int tide_x11_asked_size(struct sway_view *view, int size);

// x, y in X11's space, in the layout
void tide_x11_to_layout(int x, int y, double *lx, double *ly);

/* x, y in X11's space, where view was configured, in the layout: on the
 * output it was configured on, as its transactions compare it */
void tide_x11_view_to_layout(struct sway_view *view, int x, int y, double *lx, double *ly);

/* An X11 window opening on output: its natural size, in its pixels, into
 * the layout there */
void tide_x11_opening(struct sway_view *view, struct sway_output *output);

/* Shows view's buffer at its size in the layout, after wlroots' scene
 * sizes it, as the window maps, commits or is clipped */
void tide_x11_scale_view(struct sway_view *view);

// clip, in the layout, in view's own pixels, as the scene clips its surface
void tide_x11_clip(struct sway_view *view, struct wlr_box *clip);

// Shows an unmanaged X11 surface, as a menu, at its place and size in the layout
void tide_x11_place_unmanaged(struct wlr_xwayland_surface *xsurface,
	struct wlr_scene_surface *scene_surface);

// How far surface's pixels are from the layout's: X11's scale for it, or 1
double tide_x11_surface_scale(struct wlr_surface *surface);

/* A floating window going onto output to, however it gets there: an X11
 * one keeps its pixels, so its size in the layout follows the scale */
void tide_x11_moved(struct sway_container *con, struct sway_output *to);
#endif

#endif
