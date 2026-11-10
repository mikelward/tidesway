/*
 * Floating windows' places per app, as tidewm keeps them.
 * include/sway/tide/floatplace.h says how they behave.
 */
#include <stdlib.h>
#include <string.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/util/box.h>
#include "list.h"
#include "log.h"
#include "sway/tide/dialog.h"
#include "sway/tide/floatplace.h"
#include "sway/tree/arrange.h"
#include "sway/tree/container.h"
#include "sway/tree/root.h"
#include "sway/tree/view.h"
#include "sway/tree/workspace.h"

struct place {
	char *app_id;
	struct wlr_box box; // relative to its output's window area
};

static list_t *places; // struct place

// Whether view's floating place is its app's: it doesn't place itself
static bool apps(struct sway_view *view) {
	const char *app_id = view_get_app_id(view);
	if (!app_id) {
		app_id = view_get_class(view);
	}
	return app_id && *app_id && !(view->impl->wants_floating
			&& view->impl->wants_floating(view))
		&& !tide_dialog_pip(view) && !tide_dialog_modal(view);
}

static const char *app_of(struct sway_view *view) {
	const char *app_id = view_get_app_id(view);
	return app_id ? app_id : view_get_class(view);
}

static struct place *find(struct sway_view *view) {
	const char *app_id = app_of(view);
	for (int i = 0; places && i < places->length; i++) {
		struct place *p = places->items[i];
		if (strcmp(p->app_id, app_id) == 0) {
			return p;
		}
	}
	return NULL;
}

static void box_of(struct sway_container *con, struct wlr_box *box) {
	*box = (struct wlr_box){ (int)con->pending.x, (int)con->pending.y,
		(int)con->pending.width, (int)con->pending.height };
}

static void keep(struct sway_container *con) {
	struct sway_workspace *ws = con->pending.workspace;
	struct place *p = find(con->view);
	if (!p) {
		if (!places && !(places = create_list())) {
			sway_log(SWAY_ERROR, "tide: no memory to keep a floating place");
			return;
		}
		if (!(p = calloc(1, sizeof(*p))) || !(p->app_id = strdup(app_of(con->view)))) {
			sway_log(SWAY_ERROR, "tide: no memory to keep a floating place");
			free(p);
			return;
		}
		list_add(places, p);
	}
	struct wlr_box area;
	workspace_get_box(ws, &area);
	box_of(con, &p->box);
	p->box.x -= area.x;
	p->box.y -= area.y;
}

// Whether con is one of its app's floating windows, in its own place
static bool keeps(struct sway_container *con) {
	return con->view && con->view->surface && con->view->surface->mapped
		&& con->pending.workspace && container_is_floating(con)
		&& con->pending.fullscreen_mode == FULLSCREEN_NONE
		&& !con->view->tide.maximized && !con->view->tide.minimized
		&& !con->view->tide.mapping && apps(con->view);
}

void tide_floatplace_by_hand(struct sway_container *con, struct wlr_box *tiled) {
	struct sway_workspace *ws = con->pending.workspace;
	if (!con->view || !ws || con->view->tide.mapping || !apps(con->view)
			|| !container_is_floating(con)) {
		return;
	}
	struct place *p = find(con->view);
	struct wlr_box box = p ? p->box : *tiled, area;
	workspace_get_box(ws, &area);
	if (p) {
		box.x += area.x;
		box.y += area.y;
	}
	// At least its minimum, then kept in the area
	if (con->view->type == SWAY_VIEW_XDG_SHELL) {
		struct wlr_xdg_toplevel_state *s = &con->view->wlr_xdg_toplevel->current;
		if (box.width < s->min_width) {
			box.width = s->min_width;
		}
		if (box.height < s->min_height) {
			box.height = s->min_height;
		}
	}
	if (box.width > area.width) {
		box.width = area.width;
	}
	if (box.height > area.height) {
		box.height = area.height;
	}
	if (box.x + box.width > area.x + area.width) {
		box.x = area.x + area.width - box.width;
	}
	if (box.y + box.height > area.y + area.height) {
		box.y = area.y + area.height - box.height;
	}
	box.x = box.x < area.x ? area.x : box.x;
	box.y = box.y < area.y ? area.y : box.y;
	con->pending.x = box.x;
	con->pending.y = box.y;
	con->pending.width = box.width;
	con->pending.height = box.height;
	arrange_workspace(ws);
	box_of(con, &con->view->tide.place_seen);
	keep(con);
}

static void survey(struct sway_container *con, void *data) {
	if (!keeps(con)) {
		return;
	}
	struct wlr_box box;
	box_of(con, &box);
	if (wlr_box_equal(&box, &con->view->tide.place_seen)) {
		return;
	}
	con->view->tide.place_seen = box;
	keep(con);
}

void tide_floatplace_remember(void) {
	root_for_each_container(survey, NULL);
}
