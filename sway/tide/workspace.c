/*
 * Workspaces that span every output, as tidewm's workspace.h has them
 * (tide's SPEC §6.5), from sway's per-output workspaces: each workspace is
 * a group of parts, one per output, all of one name.
 * include/sway/tide/workspace.h says how they behave.
 */
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <wayland-util.h>
#include <wlr/types/wlr_ext_workspace_v1.h>
#include "list.h"
#include "log.h"
#include "stringop.h"
#include "sway/commands.h"
#include "sway/input/input-manager.h"
#include "sway/input/seat.h"
#include "sway/ipc-server.h"
#include "sway/output.h"
#include "sway/server.h"
#include "sway/tide/focus.h"
#include "sway/tide/layout.h"
#include "sway/tide/workspace.h"
#include "sway/tree/arrange.h"
#include "sway/tree/container.h"
#include "sway/tree/node.h"
#include "sway/tree/root.h"
#include "sway/tree/workspace.h"

struct tide_group {
	struct wl_list link; // groups, in the order made
	uint64_t made;
	bool persistent;
	bool urgent; // a window on any part is
	int parts;
	struct wlr_ext_workspace_handle_v1 *ext; // every part's
};

static struct wl_list groups = { &groups, &groups };
static uint64_t made_count;
static bool filling; // making a workspace's parts on the other outputs
static bool emitting; // sending a workspace's own event
static struct wlr_ext_workspace_group_handle_v1 *ext_group;

// Each output a part may be on: the enabled ones, then sway's fallback
static void outputs_with_parts(list_t *out) {
	for (int i = 0; i < root->outputs->length; i++) {
		list_add(out, root->outputs->items[i]);
	}
	if (root->fallback_output) {
		list_add(out, root->fallback_output);
	}
}

// g's part on output, or NULL
static struct sway_workspace *part_on(struct tide_group *g,
		struct sway_output *output) {
	for (int i = 0; output && i < output->workspaces->length; i++) {
		struct sway_workspace *ws = output->workspaces->items[i];
		if (ws->tide_group == g && !ws->node.destroying) {
			return ws;
		}
	}
	return NULL;
}

// g's parts, in a new list
static list_t *parts_of(struct tide_group *g) {
	list_t *parts = create_list();
	list_t *outputs = create_list();
	outputs_with_parts(outputs);
	for (int i = 0; i < outputs->length; i++) {
		struct sway_workspace *ws = part_on(g, outputs->items[i]);
		if (ws) {
			list_add(parts, ws);
		}
	}
	list_free(outputs);
	return parts;
}

static struct sway_output *focused_output(void) {
	struct sway_seat *seat = input_manager_current_seat();
	struct sway_workspace *ws = seat ? seat_get_focused_workspace(seat) : NULL;
	if (ws && ws->output && ws->output->enabled) {
		return ws->output;
	}
	return root->outputs->length ? root->outputs->items[0] : NULL;
}

// The part that stands for g: the focused output's, else its first
static struct sway_workspace *representative(struct tide_group *g) {
	struct sway_workspace *ws = part_on(g, focused_output());
	if (!ws) {
		list_t *parts = parts_of(g);
		ws = parts->length ? parts->items[0] : NULL;
		list_free(parts);
	}
	return ws;
}

// The current workspace, the one the focused output shows
static struct tide_group *current_group(void) {
	struct sway_output *output = focused_output();
	struct sway_workspace *ws = output ? output_get_active_workspace(output) : NULL;
	return ws ? ws->tide_group : NULL;
}

static void emit(const char *change, struct sway_workspace *old,
		struct sway_workspace *new) {
	emitting = true;
	ipc_event_workspace(old, new, change);
	emitting = false;
}

static struct sway_workspace *find_named(const char *name) {
	list_t *outputs = create_list();
	outputs_with_parts(outputs);
	struct sway_workspace *found = NULL;
	for (int i = 0; i < outputs->length && !found; i++) {
		struct sway_output *output = outputs->items[i];
		for (int j = 0; j < output->workspaces->length; j++) {
			struct sway_workspace *ws = output->workspaces->items[j];
			// Ignoring case, as sway's workspace_by_name does
			if (strcasecmp(ws->name, name) == 0 && ws->tide_group
					&& !ws->node.destroying) {
				found = ws;
				break;
			}
		}
	}
	list_free(outputs);
	return found;
}

// Makes g's part on output, which hasn't one
static struct sway_workspace *make_part(struct tide_group *g,
		struct sway_output *output) {
	struct sway_workspace *any = representative(g);
	if (!any) {
		return NULL;
	}
	bool was_filling = filling;
	filling = true;
	struct sway_workspace *ws = workspace_create(output, any->name);
	filling = was_filling;
	return ws;
}

void tide_ws_created(struct sway_workspace *ws) {
	// Another part of one name is a part of the same workspace
	struct sway_workspace *other = NULL;
	struct tide_group *g;
	wl_list_for_each(g, &groups, link) {
		list_t *parts = parts_of(g);
		for (int i = 0; i < parts->length && !other; i++) {
			struct sway_workspace *part = parts->items[i];
			if (part != ws && strcmp(part->name, ws->name) == 0) {
				other = part;
			}
		}
		list_free(parts);
		if (other) {
			break;
		}
	}
	if (other) {
		g = other->tide_group;
		// One handle for every part, so ext-workspace lists it once
		wlr_ext_workspace_handle_v1_destroy(ws->ext_workspace);
		ws->ext_workspace = g->ext;
	} else {
		g = calloc(1, sizeof(*g));
		if (!g) {
			sway_log(SWAY_ERROR, "tide: can't allocate workspace %s", ws->name);
			return;
		}
		g->made = ++made_count;
		g->ext = ws->ext_workspace;
		wl_list_insert(groups.prev, &g->link);
	}
	ws->tide_group = g;
	g->parts++;
	if (!other) {
		emit("init", NULL, ws);
	}
	if (!filling) {
		// It spans every output
		for (int i = 0; i < root->outputs->length; i++) {
			struct sway_output *output = root->outputs->items[i];
			if (!part_on(g, output)) {
				make_part(g, output);
			}
		}
	}
	wlr_ext_workspace_handle_v1_set_active(g->ext, g == current_group());
}

void tide_ws_destroying(struct sway_workspace *ws) {
	struct tide_group *g = ws->tide_group;
	if (!g) {
		wlr_ext_workspace_handle_v1_destroy(ws->ext_workspace);
		ws->ext_workspace = NULL;
		return;
	}
	ws->tide_group = NULL;
	ws->ext_workspace = NULL;
	if (--g->parts > 0) {
		// An activation names the handle's part: one that stays
		if (g->ext->data == ws) {
			struct sway_workspace *part = representative(g);
			g->ext->data = part;
		}
		return;
	}
	emit("empty", NULL, ws);
	wlr_ext_workspace_handle_v1_destroy(g->ext);
	wl_list_remove(&g->link);
	free(g);
}

void tide_ws_consider_destroy(struct sway_workspace *ws) {
	struct tide_group *g = ws->tide_group;
	if (!g || g->persistent) {
		return;
	}
	list_t *parts = parts_of(g);
	bool kept = false;
	for (int i = 0; i < parts->length && !kept; i++) {
		struct sway_workspace *part = parts->items[i];
		kept = part->tiling->length || part->floating->length
			|| (part->output && part->output->enabled
				&& output_get_active_workspace(part->output) == part);
		struct sway_seat *seat;
		wl_list_for_each(seat, &server.input->seats, link) {
			if (seat_get_focus_inactive(seat, &root->node) == &part->node) {
				kept = true;
			}
		}
	}
	for (int i = 0; i < parts->length && !kept; i++) {
		workspace_begin_destroy(parts->items[i]);
	}
	list_free(parts);
}

struct sway_workspace *tide_ws_part(struct sway_workspace *ws,
		struct sway_output *output) {
	if (!ws || !ws->tide_group || !output) {
		return ws;
	}
	struct sway_workspace *part = part_on(ws->tide_group, output);
	return part ? part : ws;
}

struct sway_workspace *tide_ws_here(struct sway_workspace *ws) {
	return tide_ws_part(ws, focused_output());
}

/* Makes part the workspace its output shows, without moving focus: it,
 * and the windows in it as they were last focused, go just above
 * everything else on that output in the seat's focus stack, so focus
 * going to the output finds them */
static void raise_part(struct sway_seat *seat, struct sway_workspace *part) {
	struct sway_node *output_node = &part->output->node;
	struct sway_seat_node *sn, *tmp;
	struct wl_list moved;
	wl_list_init(&moved);
	wl_list_for_each_safe(sn, tmp, &seat->focus_stack, link) {
		if (sn->node == &part->node || node_has_ancestor(sn->node, &part->node)) {
			wl_list_remove(&sn->link);
			wl_list_insert(moved.prev, &sn->link);
		}
	}
	if (wl_list_empty(&moved)) {
		return;
	}
	struct sway_seat_node *first = NULL;
	wl_list_for_each(sn, &seat->focus_stack, link) {
		if (node_has_ancestor(sn->node, output_node)) {
			first = sn;
			break;
		}
	}
	struct wl_list *before = first ? first->link.prev : seat->focus_stack.prev;
	wl_list_insert_list(before, &moved);
}

// Shows part on its output, in place of what it showed
static void show_part(struct sway_seat *seat, struct sway_workspace *part) {
	struct sway_output *output = part->output;
	struct sway_workspace *old = output_get_active_workspace(output);
	if (old == part) {
		return;
	}
	raise_part(seat, part);
	// Its sticky windows come along, as to the focused output's
	if (old) {
		for (int i = 0; i < old->floating->length; i++) {
			struct sway_container *floater = old->floating->items[i];
			if (container_is_sticky(floater)) {
				container_detach(floater);
				workspace_add_floating(part, floater);
				i--;
			}
		}
		node_set_dirty(&old->node);
		workspace_detect_urgent(old);
	}
	workspace_detect_urgent(part); // an urgent sticky window brings its urgency
	node_set_dirty(&output->node);
	node_set_dirty(&part->node);
	arrange_workspace(part);
	ipc_event_layout(part);
}

void tide_ws_show(struct sway_seat *seat, struct sway_workspace *ws) {
	if (!ws || !ws->tide_group || !ws->output || !ws->output->enabled) {
		return;
	}
	struct tide_group *g = ws->tide_group;
	for (int i = 0; i < root->outputs->length; i++) {
		struct sway_output *output = root->outputs->items[i];
		if (output == ws->output) {
			continue;
		}
		struct sway_workspace *part = part_on(g, output);
		if (!part) {
			part = make_part(g, output);
		}
		if (part) {
			show_part(seat, part);
		}
	}
}

/* The number a workspace's name is, or -1 for a name that isn't one, as
 * tidewm's ws_number has it */
static int number(const char *name) {
	const char *p = name;
	while (isdigit((unsigned char)*p)) {
		p++;
	}
	return !*p && p != name && p - name < 10 ? atoi(name) : -1;
}

// Whether a comes before b: numbered workspaces by number, then the rest
static bool before(struct tide_group *a, struct tide_group *b) {
	int na = number(representative(a)->name), nb = number(representative(b)->name);
	return na >= 0 && (nb < 0 || na < nb);
}

struct sway_workspace *tide_ws_step(struct sway_workspace *ws, int step) {
	if (!ws || !ws->tide_group) {
		return ws;
	}
	list_t *order = create_list();
	struct tide_group *g;
	wl_list_for_each(g, &groups, link) {
		if (!representative(g)) {
			continue;
		}
		// Sorted as they come, an equal one staying after, so it's stable
		int i = order->length;
		while (i > 0 && before(g, order->items[i - 1])) {
			i--;
		}
		list_insert(order, i, g);
	}
	int at = list_find(order, ws->tide_group);
	struct sway_workspace *to = ws;
	if (at >= 0 && order->length) {
		int n = order->length;
		g = order->items[((at + step) % n + n) % n];
		to = part_on(g, ws->output);
		if (!to) {
			to = ws->output ? make_part(g, ws->output) : representative(g);
		}
	}
	list_free(order);
	return to ? to : ws;
}

void tide_ws_output_enabled(struct sway_output *output) {
	if (wl_list_empty(&groups)) {
		return;
	}
	struct tide_group *current = current_group();
	struct tide_group *g;
	wl_list_for_each(g, &groups, link) {
		if (!part_on(g, output)) {
			make_part(g, output);
		}
	}
	if (!current) {
		current = wl_container_of(groups.next, current, link);
	}
	struct sway_workspace *part = part_on(current, output);
	if (!part) {
		return;
	}
	struct sway_seat *seat;
	wl_list_for_each(seat, &server.input->seats, link) {
		raise_part(seat, part);
		if (!seat->has_focus) {
			seat_set_focus_workspace(seat, part);
		}
	}
	node_set_dirty(&output->node);
}

static void collect_views(struct sway_container *con, void *data) {
	if (con->view) {
		list_add(data, con);
	}
}

/* Moves con, a top-level window or split, to the part to, with a move
 * event for each window in it: taken first, as a tide layout drops the
 * split as it takes it */
static void move_in(struct sway_container *con, struct sway_workspace *to) {
	struct sway_workspace *from = con->pending.workspace;
	list_t *windows = create_list();
	collect_views(con, windows);
	container_for_each_child(con, collect_views, windows);
	if (container_is_floating(con)) {
		struct wlr_box from_box, to_box;
		workspace_get_box(from, &from_box);
		container_detach(con);
		workspace_add_floating(to, con);
		container_handle_fullscreen_reparent(con);
		workspace_get_box(to, &to_box);
		if (!con->pending.fullscreen_mode) {
			floating_fix_coordinates(con, &from_box, &to_box);
		}
	} else {
		container_detach(con);
		con->pending.width = con->pending.height = 0;
		con->width_fraction = con->height_fraction = 0;
		if (!tide_add_new(to, con, NULL)) {
			workspace_add_tiling(to, con);
		}
		container_handle_fullscreen_reparent(con);
	}
	if (!con->node.destroying) {
		container_update_representation(con);
	}
	for (int i = 0; i < windows->length; i++) {
		ipc_event_window(windows->items[i], "move");
	}
	list_free(windows);
}

bool tide_ws_evacuate(struct sway_output *output) {
	if (!root->outputs->length) {
		return false;
	}
	struct sway_output *dest = root->outputs->items[0];
	while (output->workspaces->length) {
		struct sway_workspace *part = output->workspaces->items[0];
		struct sway_workspace *to = part->tide_group
			? part_on(part->tide_group, dest) : NULL;
		if (!to && part->tide_group) {
			to = make_part(part->tide_group, dest);
		}
		if (!to) {
			to = output_get_active_workspace(dest);
		}
		/* Its fullscreen window, however deep, ends fullscreen for the
		 * move, then is fullscreen again in the part it goes to */
		struct sway_container *fs = part->fullscreen;
		if (!fs && root->fullscreen_global
				&& root->fullscreen_global->pending.workspace == part) {
			fs = root->fullscreen_global;
		}
		enum sway_fullscreen_mode mode = fs ? fs->pending.fullscreen_mode : FULLSCREEN_NONE;
		if (fs && to) {
			container_fullscreen_disable(fs);
		}
		while (to && part->tiling->length) {
			struct sway_container *con = part->tiling->items[0];
			move_in(con, to);
		}
		while (to && part->floating->length) {
			struct sway_container *con = part->floating->items[0];
			move_in(con, to);
		}
		if (fs && to && !fs->node.destroying && mode != FULLSCREEN_NONE) {
			container_set_fullscreen(fs, mode);
		}
		if (to) {
			workspace_detect_urgent(to);
			arrange_workspace(to);
		}
		workspace_begin_destroy(part);
	}
	return true;
}

void tide_ws_urgency(struct sway_workspace *ws) {
	struct tide_group *g = ws->tide_group;
	if (!g) {
		return;
	}
	list_t *parts = parts_of(g);
	bool urgent = false;
	for (int i = 0; i < parts->length; i++) {
		urgent |= ((struct sway_workspace *)parts->items[i])->urgent;
	}
	list_free(parts);
	if (urgent == g->urgent) {
		return;
	}
	g->urgent = urgent;
	wlr_ext_workspace_handle_v1_set_urgent(g->ext, urgent);
	emit("urgent", NULL, representative(g));
}

void tide_ws_renamed(struct sway_workspace *ws) {
	if (!ws->tide_group) {
		return;
	}
	list_t *parts = parts_of(ws->tide_group);
	for (int i = 0; i < parts->length; i++) {
		struct sway_workspace *part = parts->items[i];
		if (part == ws) {
			continue;
		}
		char *name = strdup(ws->name);
		if (!name) {
			sway_log(SWAY_ERROR, "tide: can't rename a part of workspace %s", ws->name);
			continue;
		}
		free(part->name);
		part->name = name;
		output_sort_workspaces(part->output);
		tide_focus_renamed(part); // a grant made on it follows it
	}
	list_free(parts);
}

bool tide_ws_event_wanted(struct sway_workspace *old,
		struct sway_workspace *new, const char *change) {
	if (emitting) {
		return true;
	}
	if (strcmp(change, "focus") == 0) {
		// Focus going to another output's part of it isn't a switch
		return !old || !new || old->tide_group != new->tide_group;
	}
	// A part's coming, going and urgency aren't its workspace's, which
	// this file sends; nor does a part move between outputs
	return strcmp(change, "init") != 0 && strcmp(change, "empty") != 0
		&& strcmp(change, "urgent") != 0 && strcmp(change, "move") != 0;
}

void tide_ws_for_each(void (*f)(struct sway_workspace *ws, void *data),
		void *data) {
	struct tide_group *g;
	wl_list_for_each(g, &groups, link) {
		struct sway_workspace *ws = representative(g);
		if (ws && ws->output && ws->output->enabled) {
			f(ws, data);
		}
	}
}

bool tide_ws_persistent(struct sway_workspace *ws) {
	return ws->tide_group && ws->tide_group->persistent;
}

void tide_ws_describe(struct sway_workspace *ws, json_object *object) {
	struct tide_group *g = ws->tide_group;
	json_object_object_add(object, "urgent",
		json_object_new_boolean(g ? g->urgent : ws->urgent));
	json_object_object_add(object, "persistent",
		json_object_new_boolean(g && g->persistent));
}

struct wlr_ext_workspace_group_handle_v1 *tide_ws_ext_group(uint32_t caps) {
	if (!ext_group) {
		ext_group = wlr_ext_workspace_group_handle_v1_create(
			server.workspace_manager_v1, caps);
		if (!ext_group) {
			sway_log(SWAY_ERROR, "tide: can't make ext-workspace's group");
		}
	}
	return ext_group;
}

// persist|unpersist workspace NAME
static struct cmd_results *persist(int argc, char **argv, bool on) {
	const char *cmd = on ? "persist" : "unpersist";
	struct cmd_results *error = checkarg(argc, cmd, EXPECTED_AT_LEAST, 2);
	if (error) {
		return error;
	}
	if (strcasecmp(argv[0], "workspace") != 0) {
		return cmd_results_new(CMD_INVALID,
			"Expected persist|unpersist workspace NAME");
	}
	if (config->reading || !config->active) {
		return cmd_results_new(CMD_DEFER, NULL);
	}
	if (!root->outputs->length) {
		return cmd_results_new(CMD_FAILURE,
			"Can't run this command while there's no outputs connected.");
	}
	char *name = join_args(argv + 1, argc - 1);
	struct sway_workspace *ws = find_named(name);
	if (!ws && on) {
		ws = workspace_create(NULL, name);
	}
	free(name);
	if (!ws || !ws->tide_group) {
		return cmd_results_new(CMD_FAILURE,
			on ? "Invalid workspace name" : "No such workspace");
	}
	ws->tide_group->persistent = on;
	if (!on) {
		tide_ws_consider_destroy(ws);
	}
	return cmd_results_new(CMD_SUCCESS, NULL);
}

struct cmd_results *cmd_persist(int argc, char **argv) {
	return persist(argc, argv, true);
}

struct cmd_results *cmd_unpersist(int argc, char **argv) {
	return persist(argc, argv, false);
}
