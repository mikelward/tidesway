#ifndef _SWAY_TIDE_FADE_H
#define _SWAY_TIDE_FADE_H

/*
 * Fades, as tidewm has them (its effects.h) and tide's Hyprland config: with
 * fade_duration above 0, a window fades in as it opens, and the dim
 * (dim.h) fades as focus moves, each over that many milliseconds, easing
 * out. A window that opens dimmed has its dim fade in with it. Without the
 * line, both are instant. A fade steps as each frame is drawn, by the
 * time, and asks for the next frame while it goes on.
 */

#include <stdbool.h>
#include <stdint.h>

struct sway_container;
struct sway_view;
struct wlr_scene_buffer;

// A window's fades, held in its struct sway_view
struct tide_fade_view {
	int64_t opened; // when it mapped, in milliseconds
	bool fading_in; // from then until fade_duration later
	bool dim_known; // whether it has been dimmed or not before
	bool dim_target; // whether it's dimmed, once the dim's fade ends
	float dim_from; // how dimmed it was, of 1, as the fade began
	int64_t dim_start; // when that was
};

// view has just mapped: it fades in from here, unless fades are instant
void tide_fade_opened(struct sway_view *view);

/* How far con's window has faded in by now, from 0 to 1: 1 for a
 * container that isn't a window, or one that isn't fading */
float tide_fade_window(struct sway_container *con);

/* view is to be dimmed, or not, from now: the dim fades there from where
 * it is. Returns whether its dim layer shows now, dimmed or still fading
 * out. A window dimmed for the first time is dimmed at once, as its own
 * fade in shows it. */
bool tide_fade_dim_set(struct sway_view *view, bool dimmed);

/* While on, a dim changes at once, with no fade: for a reload, which
 * reads dim_inactive again from none */
void tide_fade_settling(bool on);

// How dimmed view is by now, from 0, not at all, to 1, by dim_inactive
float tide_fade_dim(struct sway_view *view);

// The same, for the window whose dim layer this is
float tide_fade_dim_of(struct wlr_scene_buffer *layer);

/* Steps con's fades before a frame, from output_configure_scene: ends the
 * ones that are over, and asks for another frame while one goes on */
void tide_fade_frame(struct sway_container *con);

#endif
