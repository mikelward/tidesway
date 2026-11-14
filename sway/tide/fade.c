/*
 * Fades, as tidewm has them (include/sway/tide/fade.h says how).
 */
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_scene.h>
#include "sway/commands.h"
#include "sway/config.h"
#include "sway/output.h"
#include "sway/scene_descriptor.h"
#include "sway/tide/fade.h"
#include "sway/tree/container.h"
#include "sway/tree/root.h"
#include "sway/tree/view.h"

#define FADE_MAX 3600000 // an hour, tidewm's most

static int64_t now_ms(void) {
	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);
	return (int64_t)now.tv_sec * 1000 + now.tv_nsec / 1000000;
}

/* How far a fade that began at start is by now, from 0 to 1, easing out:
 * quick at first, slowing to its end, as tidewm's */
static double eased(int64_t start, int64_t now) {
	int duration = config->tide_fade_duration;
	double p = duration > 0 ? (double)(now - start) / duration : 1.0;
	if (p >= 1.0) {
		return 1.0;
	}
	if (p <= 0.0) {
		return 0.0;
	}
	return 1.0 - (1.0 - p) * (1.0 - p) * (1.0 - p);
}

static void schedule_frames(void) {
	for (int i = 0; i < root->outputs->length; i++) {
		struct sway_output *output = root->outputs->items[i];
		if (output->enabled && output->wlr_output) {
			wlr_output_schedule_frame(output->wlr_output);
		}
	}
}

// While a reload brings every window's dim up to date
static bool settling;

void tide_fade_settling(bool on) {
	settling = on;
}

void tide_fade_opened(struct sway_view *view) {
	struct tide_fade_view *fade = &view->tide_fade;
	*fade = (struct tide_fade_view){0};
	if (config->tide_fade_duration <= 0) {
		return;
	}
	fade->opened = now_ms();
	fade->fading_in = true;
	schedule_frames();
}

float tide_fade_window(struct sway_container *con) {
	if (!con->view || !con->view->tide_fade.fading_in) {
		return 1;
	}
	return (float)eased(con->view->tide_fade.opened, now_ms());
}

static float dim_at(const struct tide_fade_view *fade, int64_t now) {
	float to = fade->dim_target ? 1 : 0;
	return fade->dim_from + (to - fade->dim_from) * (float)eased(fade->dim_start, now);
}

float tide_fade_dim(struct sway_view *view) {
	return dim_at(&view->tide_fade, now_ms());
}

float tide_fade_dim_of(struct wlr_scene_buffer *layer) {
	// The layer is in its view's tree
	struct sway_view *view = layer->node.parent
		? scene_descriptor_try_get(&layer->node.parent->node, SWAY_SCENE_DESC_VIEW)
		: NULL;
	return view ? tide_fade_dim(view) : 1;
}

bool tide_fade_dim_set(struct sway_view *view, bool dimmed) {
	struct tide_fade_view *fade = &view->tide_fade;
	int64_t now = now_ms();
	if (!fade->dim_known || config->reloading || settling) {
		/* At once: a new window's own fade in shows its dim with it, and
		 * a reload, which reads dim_inactive again from none, would
		 * otherwise fade every dim in again */
		fade->dim_known = true;
		fade->dim_target = dimmed;
		fade->dim_from = dimmed ? 1 : 0;
		fade->dim_start = now;
	} else if (dimmed != fade->dim_target) {
		fade->dim_from = dim_at(fade, now);
		fade->dim_start = now;
		fade->dim_target = dimmed;
		schedule_frames();
	}
	return dimmed || dim_at(fade, now) > 0;
}

void tide_fade_frame(struct sway_container *con) {
	struct sway_view *view = con->view;
	if (!view || config->tide_fade_duration <= 0) {
		if (view) {
			view->tide_fade.fading_in = false;
		}
		return;
	}
	struct tide_fade_view *fade = &view->tide_fade;
	int64_t now = now_ms();
	bool more = false;
	if (fade->fading_in) {
		if (eased(fade->opened, now) >= 1.0) {
			fade->fading_in = false;
		} else {
			more = true;
		}
	}
	if (fade->dim_known && dim_at(fade, now) != (fade->dim_target ? 1 : 0)) {
		more = true;
	} else if (fade->dim_known && !fade->dim_target && view->tide_dim.layer) {
		// Faded out: the layer goes
		wlr_scene_node_set_enabled(&view->tide_dim.layer->node, false);
	}
	if (more) {
		schedule_frames();
	}
}

// fade_duration MILLISECONDS, from 0, instant
struct cmd_results *cmd_fade_duration(int argc, char **argv) {
	char *end = NULL;
	long v = -1;
	if (argc == 1) {
		errno = 0;
		v = strtol(argv[0], &end, 10);
	}
	if (argc != 1 || end == argv[0] || *end || errno || v < 0 || v > FADE_MAX) {
		return cmd_results_new(CMD_INVALID,
			"fade_duration takes milliseconds from 0 to %d", FADE_MAX);
	}
	config->tide_fade_duration = (int)v;
	return cmd_results_new(CMD_SUCCESS, NULL);
}
