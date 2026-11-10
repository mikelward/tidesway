/*
 * Focus grabs over hyprland-focus-grab-v1, and on-demand layer surfaces
 * holding the keyboard, as tidewm has them.
 * include/sway/tide/grab.h says how they behave.
 */
#include <stdlib.h>
#include <wayland-server-core.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_layer_shell_v1.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_subcompositor.h>
#include <wlr/types/wlr_xdg_shell.h>
#include "hyprland-focus-grab-v1-protocol.h"
#include "log.h"
#include "sway/desktop/transaction.h"
#include "sway/input/cursor.h"
#include "sway/input/input-manager.h"
#include "sway/input/seat.h"
#include "sway/server.h"
#include "sway/tide/grab.h"
#include "sway/tree/container.h"
#include "sway/tree/node.h"
#include "sway/tree/view.h"

struct grab;

// A surface a grab names: pending until the client commits it
struct grab_surface {
	struct wl_list link; // grab.surfaces
	struct grab *grab;
	struct wlr_surface *surface;
	struct wl_listener destroy, map, unmap;
	bool pending, current;
};

struct grab {
	struct wl_resource *resource;
	struct wl_list surfaces; // grab_surface.link
};

static struct grab *active; // the grab that holds now, if any
static int asked; // focus moving because you asked
static int focusing; // a grab moving the keyboard to its own surface
/* a press inside a grab that has the keyboard: it stays, but for what a
 * binding the press runs asks for, as asked counts it again */
static bool frozen;
static uint32_t ate; // the button of the press that ended a grab: its release goes nowhere too

/* The seat a grab holds: the default one, as tidewm has one seat, so the
 * pointer and keyboard it holds are the same seat's */
static struct sway_seat *seat(void) {
	return input_manager_get_default_seat();
}

bool tide_grab_holds(struct sway_seat *s) {
	return s == seat();
}

static struct wlr_surface *keyboard_surface(void) {
	return seat()->wlr_seat->keyboard_state.focused_surface;
}

/* The window or layer surface surface belongs to, as a menu or a
 * subsurface does: its view in *view or its layer surface in *layer */
static void owner(struct wlr_surface *surface, struct sway_view **view,
		struct wlr_layer_surface_v1 **layer) {
	*view = NULL;
	*layer = NULL;
	for (int depth = 0; surface && depth < 32; depth++) {
		surface = wlr_surface_get_root_surface(surface);
		struct wlr_xdg_popup *popup = wlr_xdg_popup_try_from_wlr_surface(surface);
		if (popup) {
			surface = popup->parent;
			continue;
		}
		if ((*layer = wlr_layer_surface_v1_try_from_wlr_surface(surface))) {
			return;
		}
		*view = view_from_wlr_surface(surface);
		return;
	}
}

static struct wlr_surface *owner_surface(struct wlr_surface *surface) {
	struct sway_view *view;
	struct wlr_layer_surface_v1 *layer;
	owner(surface, &view, &layer);
	return view ? view->surface : layer ? layer->surface : NULL;
}

static bool names(struct grab *g, struct wlr_surface *surface) {
	struct grab_surface *gs;
	wl_list_for_each(gs, &g->surfaces, link) {
		if (gs->current && gs->surface == surface) {
			return true;
		}
	}
	return false;
}

static bool any(struct grab *g) {
	struct grab_surface *gs;
	wl_list_for_each(gs, &g->surfaces, link) {
		if (gs->current) {
			return true;
		}
	}
	return false;
}

/* Whether g has surface: it, its root, or the window or layer surface it
 * belongs to, as a menu does, is one of g's */
static bool covers(struct grab *g, struct wlr_surface *surface) {
	if (!surface) {
		return false;
	}
	if (names(g, surface) || names(g, wlr_surface_get_root_surface(surface))) {
		return true;
	}
	struct wlr_surface *top = owner_surface(surface);
	return top && names(g, top);
}

/* Whether surface goes when gone does: it's gone itself, a subsurface of
 * it, or a menu of the window or layer surface gone is */
static bool goes_with(struct wlr_surface *surface, struct wlr_surface *gone) {
	if (!surface || !gone) {
		return false;
	}
	return surface == gone || wlr_surface_get_root_surface(surface) == gone
		|| owner_surface(surface) == gone;
}

/* Whether one of g's surfaces, and not one going with gone, is mapped: a
 * grab holds input only while one is */
static bool live(struct grab *g, struct wlr_surface *gone) {
	struct grab_surface *gs;
	wl_list_for_each(gs, &g->surfaces, link) {
		if (gs->current && gs->surface->mapped && !goes_with(gs->surface, gone)) {
			return true;
		}
	}
	return false;
}

static bool has_keyboard(void) {
	return active && live(active, NULL) && covers(active, keyboard_surface());
}

static bool exclusive(struct wlr_layer_surface_v1 *layer) {
	return layer && layer->current.layer >= ZWLR_LAYER_SHELL_V1_LAYER_TOP
		&& layer->current.keyboard_interactive
			== ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_EXCLUSIVE;
}

static void surface_free(struct grab_surface *gs) {
	wl_list_remove(&gs->link);
	wl_list_remove(&gs->destroy.link);
	wl_list_remove(&gs->map.link);
	wl_list_remove(&gs->unmap.link);
	free(gs);
}

// The pointer enters what it may now, and leaves what it may no longer
static void rebase(void) {
	cursor_rebase_all();
}

/* The compositor ends the grab that holds: its surfaces are dropped, and
 * its client hears */
/* A grab ends, however it ends. If a menu of the window or layer surface
 * the seat has focused had the keyboard, which the grab gave it, that
 * window or layer surface has it back, as the menu goes with the grab:
 * nothing would give it back once the grab's listeners are gone, and the
 * seat takes its focus as unchanged. Not while focus moves on as you
 * asked, which takes it elsewhere. */
static void owner_back(void) {
	struct sway_seat *s = seat();
	struct wlr_surface *focused = keyboard_surface();
	if (asked || focusing || !focused) {
		return;
	}
	struct sway_view *view;
	struct wlr_layer_surface_v1 *layer;
	owner(focused, &view, &layer);
	if (layer && layer == s->focused_layer && layer->surface->mapped
			&& focused != layer->surface) {
		focusing++;
		s->focused_layer = NULL; // else it takes the layer as focused already
		seat_set_focus_layer(s, layer);
		focusing--;
		transaction_commit_dirty();
	} else if (view && view->container && view->surface && !s->focused_layer
			&& focused != view->surface
			&& seat_get_focus(s) == &view->container->node) {
		focusing++;
		seat_set_focus_surface(s, view->surface, false);
		focusing--;
	}
}

static void clear(void) {
	struct grab *g = active;
	if (!g) {
		return;
	}
	active = NULL;
	struct grab_surface *gs, *tmp;
	wl_list_for_each_safe(gs, tmp, &g->surfaces, link) {
		surface_free(gs);
	}
	hyprland_focus_grab_v1_send_cleared(g->resource);
	owner_back();
	rebase();
}

/* Gives the keyboard to one of the grab's surfaces, unless one has it
 * that isn't going with gone: to its window or layer surface, or, if the
 * grab names only a menu of it, to the menu itself. A surface not yet
 * mapped waits: it takes the keyboard when it maps. An exclusive layer
 * surface with the keyboard, such as the launcher, keeps it, and the grab
 * ends rather than hold beside it. */
static void give_keyboard(struct grab *g, struct wlr_surface *gone) {
	struct sway_seat *s = seat();
	struct wlr_surface *focused = keyboard_surface();
	if (covers(g, focused) && !goes_with(focused, gone)) {
		return;
	}
	struct sway_view *old_view;
	struct wlr_layer_surface_v1 *old_layer;
	owner(focused, &old_view, &old_layer);
	if (old_layer && !goes_with(focused, gone) && exclusive(old_layer)) {
		if (live(g, gone)) {
			clear();
		}
		return;
	}
	struct grab_surface *gs;
	wl_list_for_each(gs, &g->surfaces, link) {
		if (!gs->current || !gs->surface->mapped || goes_with(gs->surface, gone)) {
			continue;
		}
		struct sway_view *view;
		struct wlr_layer_surface_v1 *layer;
		owner(gs->surface, &view, &layer);
		if ((!view || !view->container) && !layer) {
			continue;
		}
		focusing++;
		if (names(g, view ? view->surface : layer->surface)) {
			if (view) {
				seat_set_focus_container(s, view->container);
			} else {
				seat_set_focus_layer(s, layer);
			}
		} else {
			/* Focus leaves another window or layer surface first, but not
			 * the menu's own, which would close the menu */
			if ((view && view != old_view) || (layer && layer != old_layer)) {
				if (s->focused_layer) {
					seat_set_focus_layer(s, NULL);
				}
				seat_set_focus(s, NULL);
			}
			seat_set_focus_surface(s, gs->surface, false);
		}
		focusing--;
		transaction_commit_dirty();
		return;
	}
}

// A surface the grab names maps: it may have the keyboard now
static void handle_surface_map(struct wl_listener *listener, void *data) {
	struct grab_surface *gs = wl_container_of(listener, gs, map);
	if (gs->grab != active || !gs->current) {
		return;
	}
	give_keyboard(active, NULL);
	rebase();
}

/* A surface the grab names unmaps: if the keyboard goes with it, another
 * of the grab's surfaces has it */
static void handle_surface_unmap(struct wl_listener *listener, void *data) {
	struct grab_surface *gs = wl_container_of(listener, gs, unmap);
	if (gs->grab == active && gs->current) {
		give_keyboard(gs->grab, gs->surface);
	}
}

/* A surface the grab names goes. If the keyboard is still on it, as on a
 * menu's, which sway gives back to nothing as it does a window's or a
 * layer surface's, it goes where it would without a grab. */
static void handle_surface_destroy(struct wl_listener *listener, void *data) {
	struct grab_surface *gs = wl_container_of(listener, gs, destroy);
	struct grab *g = gs->grab;
	bool keyboard = keyboard_surface() == gs->surface;
	surface_free(gs);
	if (g != active) {
		return;
	}
	if (!any(g)) {
		active = NULL;
		rebase();
	}
	if (keyboard && (!active || !live(active, NULL))) {
		struct sway_seat *s = seat();
		// It goes back as no grab or on-demand layer surface would keep it
		focusing++;
		struct wlr_layer_surface_v1 *layer = s->focused_layer;
		if (layer && layer->surface->mapped) {
			/* The layer surface whose menu it was has it back, which
			 * seat_set_focus_layer takes as having it already */
			s->focused_layer = NULL;
			seat_set_focus_layer(s, layer);
		} else {
			struct sway_node *next = seat_get_focus_inactive(s, &root->node);
			seat_set_focus(s, NULL);
			if (next) {
				seat_set_focus(s, next);
			}
		}
		focusing--;
		transaction_commit_dirty();
	}
}

static struct grab_surface *find(struct grab *g, struct wlr_surface *surface) {
	struct grab_surface *gs;
	wl_list_for_each(gs, &g->surfaces, link) {
		if (gs->surface == surface) {
			return gs;
		}
	}
	return NULL;
}

static void grab_handle_add_surface(struct wl_client *client,
		struct wl_resource *resource, struct wl_resource *surface_resource) {
	struct grab *g = wl_resource_get_user_data(resource);
	struct wlr_surface *surface = wlr_surface_from_resource(surface_resource);
	struct grab_surface *gs = find(g, surface);
	if (!gs) {
		if (!(gs = calloc(1, sizeof(*gs)))) {
			sway_log(SWAY_ERROR, "tide: no memory for a focus grab's surface");
			wl_client_post_no_memory(client);
			return;
		}
		gs->grab = g;
		gs->surface = surface;
		gs->destroy.notify = handle_surface_destroy;
		wl_signal_add(&surface->events.destroy, &gs->destroy);
		gs->map.notify = handle_surface_map;
		wl_signal_add(&surface->events.map, &gs->map);
		gs->unmap.notify = handle_surface_unmap;
		wl_signal_add(&surface->events.unmap, &gs->unmap);
		wl_list_insert(g->surfaces.prev, &gs->link);
	}
	gs->pending = true;
}

static void grab_handle_remove_surface(struct wl_client *client,
		struct wl_resource *resource, struct wl_resource *surface_resource) {
	struct grab *g = wl_resource_get_user_data(resource);
	struct grab_surface *gs = find(g, wlr_surface_from_resource(surface_resource));
	if (gs) {
		gs->pending = false;
	}
}

static void grab_handle_commit(struct wl_client *client, struct wl_resource *resource) {
	struct grab *g = wl_resource_get_user_data(resource);
	struct grab_surface *gs, *tmp;
	wl_list_for_each_safe(gs, tmp, &g->surfaces, link) {
		if (!(gs->current = gs->pending)) {
			surface_free(gs);
		}
	}
	if (!any(g)) {
		if (active == g) {
			active = NULL;
			owner_back();
			rebase();
		}
		return;
	}
	if (active != g) {
		clear();
	}
	active = g;
	// The lock has the keyboard and pointer: a grab can't start under it
	if (server.session_lock.lock) {
		clear();
		return;
	}
	give_keyboard(g, NULL);
	rebase();
}

static void grab_handle_destroy(struct wl_client *client, struct wl_resource *resource) {
	wl_resource_destroy(resource);
}

static void grab_resource_destroy(struct wl_resource *resource) {
	struct grab *g = wl_resource_get_user_data(resource);
	bool held = active == g;
	if (held) {
		active = NULL;
	}
	struct grab_surface *gs, *tmp;
	wl_list_for_each_safe(gs, tmp, &g->surfaces, link) {
		surface_free(gs);
	}
	free(g);
	if (held) {
		owner_back();
		rebase();
	}
}

static const struct hyprland_focus_grab_v1_interface grab_impl = {
	.add_surface = grab_handle_add_surface,
	.remove_surface = grab_handle_remove_surface,
	.commit = grab_handle_commit,
	.destroy = grab_handle_destroy,
};

static void manager_handle_create_grab(struct wl_client *client,
		struct wl_resource *resource, uint32_t id) {
	struct grab *g = calloc(1, sizeof(*g));
	if (!g) {
		sway_log(SWAY_ERROR, "tide: no memory for a focus grab");
		wl_client_post_no_memory(client);
		return;
	}
	g->resource = wl_resource_create(client, &hyprland_focus_grab_v1_interface,
		wl_resource_get_version(resource), id);
	if (!g->resource) {
		sway_log(SWAY_ERROR, "tide: no memory for a focus grab's resource");
		free(g);
		wl_client_post_no_memory(client);
		return;
	}
	wl_list_init(&g->surfaces);
	wl_resource_set_implementation(g->resource, &grab_impl, g, grab_resource_destroy);
}

static void manager_handle_destroy(struct wl_client *client, struct wl_resource *resource) {
	wl_resource_destroy(resource);
}

static const struct hyprland_focus_grab_manager_v1_interface manager_impl = {
	.create_grab = manager_handle_create_grab,
	.destroy = manager_handle_destroy,
};

static void manager_bind(struct wl_client *client, void *data, uint32_t version, uint32_t id) {
	struct wl_resource *resource = wl_resource_create(client,
		&hyprland_focus_grab_manager_v1_interface, (int)version, id);
	if (!resource) {
		sway_log(SWAY_ERROR, "tide: no memory to bind the focus grab manager");
		wl_client_post_no_memory(client);
		return;
	}
	wl_resource_set_implementation(resource, &manager_impl, NULL, NULL);
}

static struct wl_global *global;

bool tide_grab_is_global(const struct wl_global *g) {
	return g && g == global;
}

bool tide_grab_init(void) {
	if (!(global = wl_global_create(server.wl_display,
			&hyprland_focus_grab_manager_v1_interface, 1, NULL, manager_bind))) {
		sway_log(SWAY_ERROR, "tide: can't offer hyprland_focus_grab_manager_v1");
		return false;
	}
	return true;
}

void tide_grab_ask_begin(void) {
	asked++;
}

void tide_grab_ask_end(void) {
	asked--;
}

bool tide_grab_press_begin(struct sway_seat *s, struct wlr_surface *surface,
		uint32_t button) {
	if (s != seat()) { // another seat's press, which no grab holds
		asked++;
		return false;
	}
	if (!tide_grab_accepts(surface)) {
		clear();
		ate = button;
		return true;
	}
	// Inside a grab with the keyboard, the keyboard stays where it is
	frozen = has_keyboard();
	asked++;
	return false;
}

void tide_grab_press_end(void) {
	asked--;
	frozen = false;
}

bool tide_grab_touch(struct sway_seat *s, struct wlr_surface *surface) {
	if (s != seat() || tide_grab_accepts(surface)) {
		return false;
	}
	clear();
	return true;
}

bool tide_grab_release(struct sway_seat *s, uint32_t button) {
	if (s != seat() || !ate || ate != button) {
		return false;
	}
	ate = 0;
	return true;
}

// Whether the seat's keyboard is on a layer surface that takes it on demand
static bool on_demand_holds(struct sway_seat *s) {
	struct wlr_layer_surface_v1 *layer = s->focused_layer;
	return layer && layer->current.keyboard_interactive
		== ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_ON_DEMAND;
}

bool tide_grab_keeps(struct sway_seat *s, struct sway_node *node) {
	if (s != seat() || server.session_lock.lock || focusing) {
		return false;
	}
	if (frozen && asked == 1) {
		return true;
	}
	if (active) {
		struct wlr_surface *target = node && node_is_view(node)
			? node->sway_container->view->surface : NULL;
		if (!covers(active, target)) {
			if (asked) {
				clear();
			} else if (live(active, NULL)) {
				return true;
			}
		}
	}
	return !asked && on_demand_holds(s);
}

bool tide_grab_keeps_layer(struct sway_seat *s, struct wlr_layer_surface_v1 *layer) {
	if (s != seat() || !layer || server.session_lock.lock || focusing) {
		return false;
	}
	if (frozen && asked == 1) {
		return true;
	}
	if (active && !covers(active, layer->surface)) {
		if (asked || exclusive(layer)) {
			clear();
		} else if (live(active, NULL)) {
			return true;
		}
	}
	return false;
}

bool tide_grab_keeps_surface(struct sway_seat *s, struct wlr_surface *surface) {
	if (s != seat()) {
		return false;
	}
	// An exclusive layer surface, as the launcher, takes it, as it does
	// through seat_set_focus_layer, which has already ended a grab
	struct wlr_layer_surface_v1 *taking =
		surface ? wlr_layer_surface_v1_try_from_wlr_surface(surface) : NULL;
	if (!surface || server.session_lock.lock || focusing || exclusive(taking)) {
		return false;
	}
	if (frozen && asked == 1) {
		return true;
	}
	if (active && !covers(active, surface)) {
		if (asked) {
			clear();
		} else if (live(active, NULL)) {
			return true;
		}
	}
	// A layer surface's own, as it takes the keyboard, isn't held back
	struct wlr_layer_surface_v1 *layer = s->focused_layer;
	return !asked && on_demand_holds(s) && layer
		&& wlr_surface_get_root_surface(surface) != layer->surface;
}

bool tide_grab_accepts(struct wlr_surface *surface) {
	return !active || !live(active, NULL) || covers(active, surface);
}

void tide_grab_locked(void) {
	clear();
}
