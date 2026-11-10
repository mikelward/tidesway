/*
 * Dialogs and floating windows' places, as tidewm has them.
 * include/sway/tide/dialog.h says how they behave.
 */
#include <string.h>
#include <wlr/types/wlr_xdg_dialog_v1.h>
#include <wlr/types/wlr_xdg_shell.h>
#include "config.h"
#include "list.h"
#include "log.h"
#include "sway/criteria.h"
#include "sway/ipc-server.h"
#include "sway/output.h"
#include "sway/server.h"
#include "sway/tide/dialog.h"
#include "sway/tree/arrange.h"
#include "sway/tree/container.h"
#include "sway/tree/root.h"
#include "sway/tree/view.h"
#include "sway/tree/workspace.h"
#if WLR_HAS_XWAYLAND
#include <wlr/xwayland.h>
#endif

#define PIP_MARGIN 16

bool tide_dialog_pip(struct sway_view *view) {
	/* tide's Hyprland rule: ^Picture[- ]in[- ][Pp]icture$ */
	const char *t = view_get_title(view);
	return t && strlen(t) == 18 && strncmp(t, "Picture", 7) == 0
		&& (t[7] == '-' || t[7] == ' ') && strncmp(t + 8, "in", 2) == 0
		&& (t[10] == '-' || t[10] == ' ') && (t[11] == 'P' || t[11] == 'p')
		&& strcmp(t + 12, "icture") == 0;
}

bool tide_dialog_modal(struct sway_view *view) {
	if (view->type != SWAY_VIEW_XDG_SHELL) {
		return false;
	}
	struct wlr_xdg_dialog_v1 *dialog =
		wlr_xdg_dialog_v1_try_from_wlr_xdg_toplevel(view->wlr_xdg_toplevel);
	return dialog && dialog->modal;
}

// The window view is a dialog of, or NULL
static struct sway_view *parent_of(struct sway_view *view) {
	if (view->type == SWAY_VIEW_XDG_SHELL) {
		struct wlr_xdg_toplevel *parent = view->wlr_xdg_toplevel->parent;
		return parent ? view_from_wlr_xdg_surface(parent->base) : NULL;
	}
#if WLR_HAS_XWAYLAND
	if (view->type == SWAY_VIEW_XWAYLAND) {
		struct wlr_xwayland_surface *parent = view->wlr_xwayland_surface->parent;
		return parent ? view_from_wlr_xwayland_surface(parent) : NULL;
	}
#endif
	return NULL;
}

// The size view asks for, at least its minimum, and at most its maximum
static void asked_size(struct sway_view *view, int *width, int *height, bool *fixed) {
	*width = view->natural_width;
	*height = view->natural_height;
	*fixed = false;
	if (view->type == SWAY_VIEW_XDG_SHELL) {
		struct wlr_xdg_toplevel_state *s = &view->wlr_xdg_toplevel->current;
		if (s->min_width > 0 && *width < s->min_width) {
			*width = s->min_width;
		}
		if (s->min_height > 0 && *height < s->min_height) {
			*height = s->min_height;
		}
		if (s->max_width > 0 && *width > s->max_width) {
			*width = s->max_width;
		}
		if (s->max_height > 0 && *height > s->max_height) {
			*height = s->max_height;
		}
		*fixed = s->min_width > 0 && s->min_width == s->max_width
			&& s->min_height > 0 && s->min_height == s->max_height;
	}
}

// Puts con's content at box, its border around it
static void put(struct sway_container *con, double x, double y, double w, double h) {
	con->pending.content_x = x;
	con->pending.content_y = y;
	con->pending.content_width = w;
	con->pending.content_height = h;
	container_set_geometry_from_content(con);
}

/* Whether a rule that ran as view opened gave it a size or place of its
 * own, which placement then leaves */
static bool rules_placed(struct sway_view *view) {
	static const char *const placing[] = {
		"resize", "position", "move center", "move to center", "move window to center",
	};
	for (int i = 0; i < view->executed_criteria->length; i++) {
		struct criteria *criteria = view->executed_criteria->items[i];
		for (size_t j = 0; criteria->cmdlist && j < sizeof(placing) / sizeof(placing[0]); j++) {
			if (strstr(criteria->cmdlist, placing[j])) {
				return true;
			}
		}
	}
	return false;
}

void tide_dialog_place(struct sway_view *view) {
	struct sway_container *con = view->container;
	struct sway_workspace *ws = con->pending.workspace;
	bool pip = tide_dialog_pip(view);
	if (!ws || !ws->output || !container_is_floating(con)
			|| con->pending.fullscreen_mode != FULLSCREEN_NONE || rules_placed(view)) {
		return;
	}
	if (pip) {
		con->is_sticky = true;
	}
	struct sway_output *output = ws->output;
	struct wlr_box area;
	workspace_get_box(ws, &area);
	int width, height;
	bool fixed;
	asked_size(view, &width, &height, &fixed);
	if (pip) {
		width = output->width / 4;
		height = output->height / 4;
	}
	if (!fixed) {
		if (width > output->width * 4 / 5) {
			width = output->width * 4 / 5;
		}
		if (height > output->height * 4 / 5) {
			height = output->height * 4 / 5;
		}
	}
	if (width <= 0 || height <= 0) {
		return;
	}
	struct sway_view *parent = parent_of(view);
	struct sway_container *pc = parent ? parent->container : NULL;
	struct wlr_box around = area;
	if (pc && pc->pending.workspace && pc->pending.workspace->output == output) {
		around = (struct wlr_box){ pc->pending.x, pc->pending.y,
			pc->pending.width, pc->pending.height };
	}
	double x = pip ? area.x + area.width - width - PIP_MARGIN
		: around.x + (around.width - width) / 2.0;
	double y = pip ? area.y + area.height - height - PIP_MARGIN
		: around.y + (around.height - height) / 2.0;
	// Kept in the window area, as far as it fits
	if (x + width > area.x + area.width) {
		x = area.x + area.width - width;
	}
	if (y + height > area.y + area.height) {
		y = area.y + area.height - height;
	}
	if (x < area.x) {
		x = area.x;
	}
	if (y < area.y) {
		y = area.y;
	}
	put(con, x, y, width, height);
	arrange_workspace(ws);
}

// Puts dialog in the middle of parent, in parent's window area
static void center_on(struct sway_container *dialog, struct sway_container *parent) {
	struct sway_workspace *ws = parent->pending.workspace;
	struct wlr_box area;
	workspace_get_box(ws, &area);
	double w = dialog->pending.content_width, h = dialog->pending.content_height;
	double x = parent->pending.x + (parent->pending.width - w) / 2.0;
	double y = parent->pending.y + (parent->pending.height - h) / 2.0;
	if (x + w > area.x + area.width) {
		x = area.x + area.width - w;
	}
	if (y + h > area.y + area.height) {
		y = area.y + area.height - h;
	}
	put(dialog, x < area.x ? area.x : x, y < area.y ? area.y : y, w, h);
}

struct follow {
	list_t *moved; // windows that went to another workspace
	list_t *floating; // every floating window
};

static void survey(struct sway_container *con, void *data) {
	struct follow *f = data;
	if (!con->view) {
		return;
	}
	struct sway_workspace *ws = con->pending.workspace;
	if (con->view->tide.follow_ws && ws && ws != con->view->tide.follow_ws) {
		list_add(f->moved, con);
	}
	con->view->tide.follow_ws = ws;
	if (container_is_floating(con)) {
		list_add(f->floating, con);
	}
}

/* A seat whose focus was dialog, gone along to ws that isn't shown, moves
 * on to what it had last where the dialog was, so the hidden dialog hears
 * no keys */
static void unfocus_gone(struct sway_container *dialog, struct sway_workspace *old,
		struct sway_workspace *ws) {
	if (workspace_is_visible(ws)) {
		return;
	}
	struct sway_seat *seat;
	wl_list_for_each(seat, &server.input->seats, link) {
		if (seat_get_focus(seat) != &dialog->node) {
			continue;
		}
		struct sway_node *next = old && !old->node.destroying
			? seat_get_focus_inactive(seat, &old->node) : NULL;
		if (!next || next == &dialog->node) {
			next = old && !old->node.destroying ? &old->node : NULL;
		}
		seat_set_focus(seat, next);
	}
}

// parent went to another workspace: its dialogs, not sticky, go along
static void take_dialogs(struct follow *f, struct sway_container *parent) {
	struct sway_workspace *ws = parent->pending.workspace;
	for (int i = 0; i < f->floating->length; i++) {
		struct sway_container *dialog = f->floating->items[i];
		if (dialog == parent || parent_of(dialog->view) != parent->view
				|| container_is_sticky(dialog)) {
			continue;
		}
		struct sway_workspace *old = dialog->pending.workspace;
		if (old != ws) {
			container_detach(dialog);
			workspace_add_floating(ws, dialog);
			dialog->view->tide.follow_ws = ws;
			ipc_event_window(dialog, "move");
			unfocus_gone(dialog, old, ws);
			if (old) {
				// One it leaves empty goes, with no urgency to tell of
				workspace_consider_destroy(old);
				if (!old->node.destroying) {
					workspace_detect_urgent(old);
					arrange_workspace(old);
				}
			}
			workspace_detect_urgent(ws);
		}
		center_on(dialog, parent);
		arrange_workspace(ws);
		take_dialogs(f, dialog);
	}
}

void tide_dialog_follow(void) {
	struct follow f = { create_list(), create_list() };
	if (!f.moved || !f.floating) {
		sway_log(SWAY_ERROR, "tide: no memory for dialogs to follow their windows");
		list_free(f.moved);
		list_free(f.floating);
		return;
	}
	root_for_each_container(survey, &f);
	for (int i = 0; i < f.moved->length; i++) {
		take_dialogs(&f, f.moved->items[i]);
	}
	list_free(f.moved);
	list_free(f.floating);
}

bool tide_dialog_init(void) {
	if (!wlr_xdg_wm_dialog_v1_create(server.wl_display, 1)) {
		sway_log(SWAY_ERROR, "tide: can't offer xdg-dialog-v1");
		return false;
	}
	return true;
}
