/*
 * The focus guard, as tidewm's focus.h has it (tide's SPEC §14): grants,
 * held windows, a canceled grant's placement, a notification's grant
 * that brings up its app's window, and the grant, grant extend and focus
 * recent commands. include/sway/tide/focus.h says what it does; tidewm's
 * docs/config.md and docs/ipc.md are the reference.
 */
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <wayland-server-core.h>
#include <wlr/config.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/util/box.h>
#if WLR_HAS_XWAYLAND
#include <wlr/xwayland.h>
#endif
#include "log.h"
#include "sway/commands.h"
#include "sway/config.h"
#include "sway/desktop/transaction.h"
#include "sway/input/input-manager.h"
#include "sway/input/seat.h"
#include "sway/output.h"
#include "sway/server.h"
#include "sway/tide/focus.h"
#include "sway/tree/arrange.h"
#include "sway/tree/container.h"
#include "sway/tree/node.h"
#include "sway/tree/root.h"
#include "sway/tree/view.h"
#include "sway/tree/workspace.h"

// tidewm's config.h: polkit agents, whose prompt takes focus right after a key press
static const char *const prompt_classes[] = {
	"hyprpolkitagent",
	"polkit-gnome-authentication-agent-1",
	"org.kde.polkit-kde-authentication-agent-1",
};

// Portal backends, whose dialogs, such as the file chooser, are for the app you're in
static const char *const portal_classes[] = {
	"xdg-desktop-portal-gtk",
	"xdg-desktop-portal-kde",
	"xdg-desktop-portal-gnome",
};

#define LENGTH(a) (sizeof(a) / sizeof((a)[0]))
#define MAX_SECONDS 86400.0

// A one-shot grant
struct grant {
	struct wl_list link;
	char **apps; // normalized ids; "*" alone stands for any app
	size_t napps;
	pid_t pid; // the process that asked, whose descendants may use it; 0 for none
	unsigned long key; // for grant extend; 0 for none
	char *workspace; // where its window goes, focus following; NULL for where it opens
	bool or_recent; // lapsing unused, it brings up the app's most recent window
	uint64_t at; // when, in monotonic microseconds
	size_t origin; // the node id of the workspace focused as it was made
	char *origin_name; // that workspace's name then, to make it again; NULL for none
	bool canceled; // by you moving on: it gives no focus, only a place
};

static struct wl_list grants = { &grants, &grants };
static size_t active; // the node id of the window focused last; 0 for none
static size_t active_ws; // the focused workspace's node id then
static uint64_t last_key; // when you last pressed a key; 0 for never
// Your last key press or click: when, and the window it went to, 0 for none
static struct { uint64_t at; bool button; size_t window; } last_input;
static uint64_t focus_count; // focuses so far, for focus_seq
static struct wl_event_source *timer; // for or-recent grants' lapses
static int quiet; // focus moving that isn't you moving on
static struct sway_view *mapping; // the window mapping now
static struct sway_view *granted; // a new window that used a grant
static char *granted_ws; // its grant's workspace, focus following
// Where a new window goes: a canceled grant's workspace, or a grant's --workspace
static struct { struct sway_view *view; size_t id; char *name; } place;

static uint64_t now_us(void) {
	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);
	return (uint64_t)now.tv_sec * 1000000 + (uint64_t)now.tv_nsec / 1000;
}

static bool guard_on(void) {
	return config && config->tide_focus.guard;
}

static uint64_t grant_us(void) {
	return (uint64_t)(1e6 * (config ? config->tide_focus.grant_seconds : 10));
}

static uint64_t prompt_us(void) {
	return (uint64_t)(1e6 * (config ? config->tide_focus.prompt_seconds : 2));
}

void tide_focus_config_defaults(struct tide_focus_config *fc) {
	fc->guard = false;
	fc->grant_seconds = 10;
	fc->prompt_seconds = 2;
}

/* An app id as a grant or a window might spell it, normalized: lowercase
 * and without a .desktop suffix. NULL for an empty one. */
static char *normalize(const char *id) {
	if (!id || !*id) {
		return NULL;
	}
	char *s = strdup(id);
	if (!s) {
		sway_log(SWAY_ERROR, "Out of memory normalizing an app id");
		return NULL;
	}
	for (char *p = s; *p; p++) {
		*p = (char)tolower((unsigned char)*p);
	}
	size_t n = strlen(s);
	if (n >= 8 && strcmp(s + n - 8, ".desktop") == 0) {
		s[n - 8] = '\0';
	}
	if (!*s) {
		free(s);
		return NULL;
	}
	return s;
}

/* Whether two normalized ids name the same app. Qualified ids must match
 * in full; a bare name matches the last part of a qualified one, so
 * nautilus matches org.gnome.nautilus. */
static bool same_id(const char *a, const char *b) {
	if (!a || !b) {
		return false;
	}
	if (strcmp(a, b) == 0) {
		return true;
	}
	if (!strchr(a, '.') == !strchr(b, '.')) {
		return false;
	}
	const char *bare = strchr(a, '.') ? b : a;
	const char *qualified = bare == a ? b : a;
	return strcmp(strrchr(qualified, '.') + 1, bare) == 0;
}

// A window's app ID, or X11 class, as it gave it: NULL for none
static const char *raw_app(struct sway_view *view) {
	const char *app = view_get_app_id(view);
	return app && *app ? app : view_get_class(view);
}

bool tide_focus_nameless(struct sway_view *view) {
	const char *app = raw_app(view), *title = view_get_title(view);
	return (!app || !*app) && (!title || !*title);
}

// Whether the view is a dialog, which goes with its parent
static bool has_parent(struct sway_view *view) {
	switch (view->type) {
	case SWAY_VIEW_XDG_SHELL:
		return view->wlr_xdg_toplevel && view->wlr_xdg_toplevel->parent;
#if WLR_HAS_XWAYLAND
	case SWAY_VIEW_XWAYLAND:
		return view->wlr_xwayland_surface && view->wlr_xwayland_surface->parent;
#endif
	default:
		break;
	}
	return false;
}

static void grant_free(struct grant *g) {
	wl_list_remove(&g->link);
	for (size_t i = 0; i < g->napps; i++) {
		free(g->apps[i]);
	}
	free(g->apps);
	free(g->workspace);
	free(g->origin_name);
	free(g);
}

static bool lapsed(const struct grant *g, uint64_t now) {
	return now - g->at > grant_us();
}

// Drops the grants that have lapsed, but the or-recent ones, which lapse() handles
static void expire(void) {
	struct grant *g, *tmp;
	uint64_t now = now_us();
	wl_list_for_each_safe(g, tmp, &grants, link) {
		if (!g->or_recent && lapsed(g, now)) {
			grant_free(g);
		}
	}
}

static bool names(const struct grant *g, const char *app) {
	for (size_t i = 0; i < g->napps; i++) {
		if (same_id(g->apps[i], app)) {
			return true;
		}
	}
	return false;
}

static bool wildcard(const struct grant *g) {
	return g->napps == 1 && strcmp(g->apps[0], "*") == 0;
}

/* Anything you do after a grant cancels every grant. One that can still
 * place its window stays, canceled, until it's used or lapses: not a
 * notification's, nor a wildcard that can't tell its window from any
 * other. */
static void cancel(void) {
	struct grant *g, *tmp;
	wl_list_for_each_safe(g, tmp, &grants, link) {
		if (g->origin_name && (!wildcard(g) || g->pid)) {
			g->canceled = true;
		} else {
			grant_free(g);
		}
	}
}

// Takes g off the list, for the caller to free
static struct grant *unlink_grant(struct grant *g) {
	wl_list_remove(&g->link);
	wl_list_init(&g->link);
	return g;
}

/* The grant for view's app, canceled or not as asked: one naming it
 * before a wildcard, which any app's window uses if it isn't canceled */
static struct grant *take(struct sway_view *view, bool canceled) {
	char *app = normalize(raw_app(view));
	uint64_t now = now_us();
	struct grant *g;
	expire();
	for (int wild = 0; wild < 2 - canceled; wild++) {
		wl_list_for_each(g, &grants, link) {
			if (!lapsed(g, now) && g->canceled == canceled && wildcard(g) == wild
					&& (wild || names(g, app))) {
				free(app);
				return unlink_grant(g);
			}
		}
	}
	free(app);
	return NULL;
}

/* A process's parent, from /proc/PID/stat, or -1 if it can't be read. The
 * command name may hold spaces and parentheses, so the parent comes after
 * the last ')'. Linux only, as sway is. */
static pid_t parent_of(pid_t pid) {
	char path[64], line[1024], *close;
	snprintf(path, sizeof(path), "/proc/%d/stat", (int)pid);
	FILE *f = fopen(path, "r");
	if (!f) {
		// A process that has exited has no parent to walk
		sway_log(SWAY_DEBUG, "Can't read %s: %s", path, strerror(errno));
		return -1;
	}
	close = fgets(line, sizeof(line), f) ? strrchr(line, ')') : NULL;
	fclose(f);
	int ppid;
	if (!close || sscanf(close + 1, " %*s %d", &ppid) != 1) {
		sway_log(SWAY_DEBUG, "Can't parse %s", path);
		return -1;
	}
	return ppid;
}

// Whether pid is ancestor or descends from it, walking at most 64 parents
static bool descends(pid_t pid, pid_t ancestor) {
	for (int i = 0; i < 64 && pid > 1; i++) {
		if (pid == ancestor) {
			return true;
		}
		pid = parent_of(pid);
	}
	return false;
}

/* A grant naming the process that asked (the shell a terminal command ran
 * in), for a window whose process descends from it */
static struct grant *take_by_ancestry(struct sway_view *view, bool canceled) {
	struct grant *g;
	expire();
	if (view->pid <= 0) {
		return NULL;
	}
	wl_list_for_each(g, &grants, link) {
		if (g->pid && g->canceled == canceled && descends(view->pid, g->pid)) {
			return unlink_grant(g);
		}
	}
	return NULL;
}

static bool class_in(struct sway_view *view, const char *const *classes, size_t n) {
	char *app = normalize(raw_app(view));
	bool in = false;
	for (size_t i = 0; i < n && !in; i++) {
		char *class = normalize(classes[i]);
		in = same_id(class, app);
		free(class);
	}
	free(app);
	return in;
}

// The focused window, or NULL
static struct sway_container *focused_window(void) {
	struct sway_container *con =
		seat_get_focused_container(input_manager_current_seat());
	return con && con->view ? con : NULL;
}

static bool same_app_as_focused(struct sway_view *view) {
	struct sway_container *con = focused_window();
	if (!con) {
		return false;
	}
	if (view->pid > 0 && view->pid == con->view->pid) {
		return true;
	}
	char *app = normalize(raw_app(view)), *other = normalize(raw_app(con->view));
	bool same = same_id(app, other);
	free(app);
	free(other);
	return same;
}

// Whether a polkit prompt opening now follows a key press
static bool prompt_may_focus(void) {
	return last_key && now_us() - last_key <= prompt_us();
}

// Focuses con as the guard, which isn't you moving on, showing its workspace
static void focus_window(struct sway_container *con) {
	struct sway_seat *seat = input_manager_current_seat();
	quiet++;
	if (container_is_scratchpad_hidden_or_child(con)) {
		root_scratchpad_show(con);
	}
	struct sway_container *obstructing = container_obstructing_fullscreen_container(con);
	if (obstructing) {
		container_fullscreen_disable(obstructing);
		arrange_root();
	}
	seat_set_focus_container(seat, con);
	container_raise_floating(con);
	quiet--;
}

struct recent {
	char **apps;
	size_t napps;
	struct sway_container *best;
};

static void find_recent(struct sway_container *con, void *data) {
	struct recent *r = data;
	if (!con->view) {
		return;
	}
	char *app = normalize(raw_app(con->view));
	bool mine = false;
	for (size_t i = 0; i < r->napps && !mine; i++) {
		mine = same_id(r->apps[i], app);
	}
	free(app);
	if (mine && (!r->best || con->view->tide.focus_seq > r->best->view->tide.focus_seq)) {
		r->best = con;
	}
}

/* The window of any of apps focused most recently, or, with none ever
 * focused, the first of them; NULL for none */
static struct sway_container *most_recent(char **apps, size_t napps) {
	struct recent r = { apps, napps, NULL };
	root_for_each_container(find_recent, &r);
	return r.best;
}

// Brings up the most recent window of apps, as the guard, or says there's none
static bool bring_up(char **apps, size_t napps, const char *why) {
	struct sway_container *con = most_recent(apps, napps);
	if (con) {
		focus_window(con);
		return true;
	}
	char list[256] = "";
	for (size_t i = 0; i < napps; i++) {
		size_t n = strlen(list);
		snprintf(list + n, sizeof(list) - n, " %s", apps[i]);
	}
	sway_log(SWAY_ERROR, "%s: no window of%s to bring up", why, list);
	return false;
}

static int lapse(void *data);

// Sets the timer for the next or-recent grant to lapse, or stops it
static void arm(void) {
	struct grant *g;
	uint64_t now = now_us(), next = 0;
	wl_list_for_each(g, &grants, link) {
		uint64_t due = g->at + grant_us();
		if (g->or_recent && (!next || due < next)) {
			next = due;
		}
	}
	if (!timer && next) {
		timer = wl_event_loop_add_timer(server.wl_event_loop, lapse, NULL);
		if (!timer) {
			// Without its timer, a notification's grant just lapses
			sway_log(SWAY_ERROR, "Can't make the focus guard's timer");
			return;
		}
	}
	if (timer) {
		wl_event_source_timer_update(timer, !next ? 0
			: next <= now ? 1 : (int)fmin((double)(next - now + 999) / 1000, INT_MAX));
	}
}

/* A notification's grant ran out unused: its app's most recent window
 * comes up instead. Each that's due, in the order they were made. */
static int lapse(void *data) {
	uint64_t now = now_us();
	for (;;) {
		struct grant *g, *due = NULL;
		wl_list_for_each(g, &grants, link) {
			if (g->or_recent && lapsed(g, now)) {
				due = g;
				break;
			}
		}
		if (!due) {
			break;
		}
		unlink_grant(due);
		bring_up(due->apps, due->napps, "a notification's grant lapsed");
		grant_free(due);
	}
	arm();
	transaction_commit_dirty();
	return 0;
}

void tide_focus_reloaded(void) {
	arm();
}

// A workspace by its node id, or, with none that has it, by its name
static struct sway_workspace *find_workspace(size_t id, const char *name) {
	for (int i = 0; i < root->outputs->length; i++) {
		struct sway_output *output = root->outputs->items[i];
		for (int j = 0; j < output->workspaces->length; j++) {
			struct sway_workspace *ws = output->workspaces->items[j];
			if (id && ws->node.id == id) {
				return ws;
			}
		}
	}
	for (int i = 0; name && i < root->outputs->length; i++) {
		struct sway_output *output = root->outputs->items[i];
		for (int j = 0; j < output->workspaces->length; j++) {
			struct sway_workspace *ws = output->workspaces->items[j];
			if (strcmp(ws->name, name) == 0) {
				return ws;
			}
		}
	}
	return NULL;
}

static void unaim(void) {
	free(place.name);
	place.name = NULL;
	place.view = NULL;
	place.id = 0;
}

static void aim_at(struct sway_view *view, size_t id, const char *name) {
	unaim();
	place.name = strdup(name);
	if (!place.name) {
		sway_log(SWAY_ERROR, "Out of memory placing a window");
		return;
	}
	place.view = view;
	place.id = id;
}

/* Aims view where canceled grant g says, if you've left the workspace g
 * was made on: on its --workspace, or back on that one, made again if
 * it's gone. Returns whether it did. */
static bool aim(struct sway_view *view, const struct grant *g) {
	struct sway_workspace *cur =
		seat_get_focused_workspace(input_manager_current_seat());
	if (!cur || find_workspace(g->origin, g->origin_name) == cur) {
		return false;
	}
	size_t id = g->workspace ? 0 : g->origin;
	const char *name = g->workspace ? g->workspace : g->origin_name;
	struct sway_workspace *target = find_workspace(id, name);
	if (target == cur) {
		return false;
	}
	aim_at(view, target ? target->node.id : 0, name);
	return place.view == view;
}

struct sway_workspace *tide_focus_placed(struct sway_view *view) {
	if (view != place.view || !place.name) {
		return NULL;
	}
	struct sway_workspace *ws = find_workspace(place.id, place.name);
	return ws ? ws : workspace_create(NULL, place.name);
}

void tide_focus_open(struct sway_view *view) {
	struct grant *g;
	// A window mapping again is new again: nothing holds it yet
	view->tide.held = TIDE_FREE;
	view->tide.fullscreen = FULLSCREEN_NONE;
	mapping = view;
	granted = NULL;
	free(granted_ws);
	granted_ws = NULL;
	unaim();
	// A status popup opens as it is: unfocused, unmarked, taking no grant
	if (tide_focus_nameless(view)) {
		if (guard_on()) {
			view->tide.held = TIDE_POPUP;
		}
		return;
	}
	// Grants first, so one is used up even by a window of the app you're in
	if ((g = take(view, false)) || (g = take_by_ancestry(view, false))) {
		granted = view;
		granted_ws = g->workspace;
		g->workspace = NULL;
		grant_free(g);
		if (granted_ws) {
			aim_at(view, 0, granted_ws);
		}
		return;
	}
	/* Then a canceled one, for a window that isn't a dialog: it opens
	 * where it was launched from, if you've gone elsewhere, held and
	 * marked, with the guard or without it */
	if (!has_parent(view) && ((g = take(view, true)) || (g = take_by_ancestry(view, true)))) {
		bool placed = aim(view, g);
		grant_free(g);
		if (placed) {
			view->tide.held = TIDE_WAITING;
			return;
		}
	}
	if (!guard_on()) {
		return;
	}
	// Let in, it may take focus
	if (same_app_as_focused(view)
			|| (focused_window() && class_in(view, portal_classes, LENGTH(portal_classes)))
			|| (prompt_may_focus() && class_in(view, prompt_classes, LENGTH(prompt_classes)))) {
		return;
	}
	view->tide.held = TIDE_WAITING;
}

bool tide_focus_held(struct sway_view *view) {
	return view->tide.held != TIDE_FREE;
}

bool tide_focus_defer_fullscreen(struct sway_container *con, int mode) {
	if (!con->view || con->view->tide.held == TIDE_FREE) {
		return false;
	}
	con->view->tide.fullscreen = mode;
	return true;
}

bool tide_focus_wants_fullscreen(struct sway_container *con) {
	return con->view && con->view->tide.held != TIDE_FREE
		&& con->view->tide.fullscreen != FULLSCREEN_NONE;
}

bool tide_focus_passed_by(struct sway_container *con) {
	return con->view && con->view->tide.held == TIDE_POPUP;
}

/* A new window took focus with no grant, one the guard let in or any with
 * the guard off: focus moving on. One that opened elsewhere, unfocused,
 * moves nothing. */
void tide_focus_opened(struct sway_view *view) {
	bool was_granted = granted == view;
	char *ws = granted_ws;
	granted = NULL;
	granted_ws = NULL;
	mapping = NULL;
	unaim();
	struct sway_workspace *here = view->container ? view->container->pending.workspace : NULL;
	if (was_granted && ws && view->container) {
		focus_window(view->container);
	} else if (was_granted && here && focused_window() != view->container
			&& here == seat_get_focused_workspace(input_manager_current_seat())
			&& (here->fullscreen || root->fullscreen_global)) {
		/* Granted, it takes focus even behind a fullscreen window, which
		 * ends that fullscreen, as in tidewm; one a rule sent elsewhere
		 * stays there unfocused */
		focus_window(view->container);
	} else if (!was_granted && view->container && focused_window() == view->container) {
		cancel();
	}
	free(ws);
	if (view->tide.held == TIDE_WAITING && view->container) {
		view_set_urgent(view, true);
	}
}

void tide_focus_focused(struct sway_node *node) {
	size_t before = active, before_ws = active_ws;
	struct sway_container *con = node->type == N_CONTAINER
		&& node->sway_container->view ? node->sway_container : NULL;
	struct sway_workspace *ws = node->type == N_WORKSPACE ? node->sway_workspace
		: node->type == N_CONTAINER ? node->sway_container->pending.workspace : NULL;
	active = con ? con->node.id : 0;
	active_ws = ws ? ws->node.id : 0;
	if (con) {
		// However you got there, a held window isn't held now, nor marked
		bool was_held = con->view->tide.held != TIDE_FREE;
		con->view->tide.held = TIDE_FREE;
		con->view->tide.focus_seq = ++focus_count;
		if (was_held && view_is_urgent(con->view)) {
			view_set_urgent(con->view, false);
		}
		// Held no more, it goes fullscreen if it asked to while held
		enum sway_fullscreen_mode wanted = con->view->tide.fullscreen;
		con->view->tide.fullscreen = FULLSCREEN_NONE;
		if (was_held && wanted != FULLSCREEN_NONE) {
			container_set_fullscreen(con, wanted);
			arrange_root();
		}
	}
	bool returned = (active && active == before)
		|| (!active && !before && active_ws == before_ws);
	if (quiet || returned || mapping) {
		return;
	}
	cancel();
}

void tide_focus_quiet_begin(void) {
	quiet++;
}

void tide_focus_quiet_end(void) {
	quiet--;
}

struct sway_node *tide_focus_after_close(struct sway_seat *seat,
		struct sway_node *next) {
	if (!next || next->type != N_CONTAINER || !next->sway_container->view
			|| next->sway_container->view->tide.held == TIDE_FREE) {
		return next;
	}
	struct sway_workspace *ws = next->sway_container->pending.workspace;
	if (!ws) {
		return next;
	}
	struct sway_seat_node *current;
	wl_list_for_each(current, &seat->focus_stack, link) {
		struct sway_node *node = current->node;
		if (node->type == N_CONTAINER && node->sway_container->view
				&& node->sway_container->view->tide.held == TIDE_FREE
				&& node_has_ancestor(node, &ws->node)) {
			return node;
		}
	}
	return &ws->node;
}

struct sway_container *tide_focus_go(struct sway_workspace *ws) {
	struct sway_seat *seat = input_manager_current_seat();
	struct sway_container *waiting = NULL;
	struct sway_seat_node *current;
	wl_list_for_each(current, &seat->focus_stack, link) {
		struct sway_node *node = current->node;
		if (node->type != N_CONTAINER || !node->sway_container->view
				|| !node_has_ancestor(node, &ws->node)) {
			continue;
		}
		enum tide_held held = node->sway_container->view->tide.held;
		if (held == TIDE_FREE) {
			return node->sway_container;
		}
		if (held == TIDE_WAITING && !waiting) {
			waiting = node->sway_container;
		}
	}
	return waiting;
}

void tide_focus_key(void) {
	cancel();
	last_key = now_us();
	last_input.at = last_key;
	last_input.button = false;
	last_input.window = 0;
}

void tide_focus_click(void) {
	cancel();
	last_input.at = now_us();
	last_input.button = true;
	last_input.window = 0;
}

void tide_focus_delivered(struct wlr_surface *surface) {
	struct sway_view *view = surface
		? view_from_wlr_surface(wlr_surface_get_root_surface(surface)) : NULL;
	last_input.window = view && view->container ? view->container->node.id : 0;
}

// Whether con is a window shown at lx, ly that hasn't caught up with its place
/* The point lx, ly on view's surface, where the scene puts the surface:
 * at the content's corner, or centered in it, as sway does a floating or
 * fullscreen window's smaller buffer */
static void surface_point(struct sway_view *view, double lx, double ly,
		double *sx, double *sy) {
	int x, y;
	wlr_scene_node_coords(&view->content_tree->node, &x, &y);
	*sx = lx - x + view->geometry.x;
	*sy = ly - y + view->geometry.y;
}

static bool unready_at(struct sway_container *con, double lx, double ly) {
	struct sway_view *view = con->view;
	if (!view || !view->surface || view->type != SWAY_VIEW_XDG_SHELL
			|| !view_is_visible(view)
			|| wl_list_empty(&view->wlr_xdg_toplevel->base->configure_list)) {
		return false;
	}
	struct wlr_box box = { con->current.content_x, con->current.content_y,
		con->current.content_width, con->current.content_height };
	if (!wlr_box_contains_point(&box, lx, ly)) {
		return false;
	}
	/* Only where it hasn't drawn yet: what it has drawn keeps its input
	 * region, which the scene already asked */
	struct wlr_box drawn;
	wlr_surface_get_extents(view->surface, &drawn);
	double sx, sy;
	surface_point(view, lx, ly, &sx, &sy);
	return !wlr_box_contains_point(&drawn, sx, sy);
}

/* The topmost window in cons, or in their children, as the screen shows
 * them, that hasn't caught up with its place at lx, ly: the last in a list
 * is on top, as floating windows are */
static struct sway_container *unready_in(list_t *cons, double lx, double ly) {
	for (int i = cons->length - 1; i >= 0; i--) {
		struct sway_container *con = cons->items[i];
		if (unready_at(con, lx, ly)) {
			return con;
		}
		struct sway_container *child = con->current.children
			? unready_in(con->current.children, lx, ly) : NULL;
		if (child) {
			return child;
		}
	}
	return NULL;
}

struct sway_container *tide_focus_unready_at(double lx, double ly,
		struct wlr_surface **surface, double *sx, double *sy) {
	if (server.session_lock.lock || root->fullscreen_global) {
		return NULL;
	}
	struct wlr_output *wlr_output =
		wlr_output_layout_output_at(root->output_layout, lx, ly);
	struct sway_output *output = wlr_output ? wlr_output->data : NULL;
	/* The workspace the output shows, as its last transaction left it: a
	 * workspace made since, such as one a grant's window opens on, has no
	 * state on screen yet (its current lists are NULL) until its own
	 * transaction applies, and a layer surface mapping in between rebases
	 * the cursor through here */
	struct sway_workspace *ws = output && output->enabled
		? output->current.active_workspace : NULL;
	if (!ws) {
		return NULL;
	}
	struct sway_container *con = NULL;
	if (ws->current.fullscreen) {
		con = unready_at(ws->current.fullscreen, lx, ly)
			? ws->current.fullscreen : NULL;
	} else {
		// The floating windows are over the tiles
		con = unready_in(ws->current.floating, lx, ly);
		if (!con) {
			con = unready_in(ws->current.tiling, lx, ly);
		}
	}
	if (con) {
		*surface = con->view->surface;
		surface_point(con->view, lx, ly, sx, sy);
	}
	return con;
}

void tide_focus_describe_seat(json_object *object) {
	json_object *last = NULL;
	if (last_input.at) {
		last = json_object_new_object();
		json_object_object_add(last, "type",
			json_object_new_string(last_input.button ? "button" : "key"));
		json_object_object_add(last, "window", last_input.window
			? json_object_new_int64((int64_t)last_input.window) : NULL);
		json_object_object_add(last, "msec_ago",
			json_object_new_int64((int64_t)((now_us() - last_input.at) / 1000)));
	}
	json_object_object_add(object, "last_input", last);
	json_object_object_add(object, "prompt_may_focus",
		json_object_new_boolean(prompt_may_focus()));
}

bool tide_focus_activate(struct sway_view *view) {
	if (!view->surface || !view->container) {
		return false;
	}
	struct grant *g = take(view, false);
	if (!g) {
		/* A canceled one is used too, giving nothing: its app answered
		 * with the window it had, so no new one is coming */
		if ((g = take(view, true))) {
			grant_free(g);
		}
		if (!guard_on()) {
			return false;
		}
		// With the guard, whatever focus_on_window_activation says, it's marked
		view_set_urgent(view, true);
		transaction_commit_dirty();
		return true;
	}
	grant_free(g);
	focus_window(view->container);
	transaction_commit_dirty();
	return true;
}

void tide_focus_renamed(struct sway_workspace *ws) {
	struct grant *g;
	wl_list_for_each(g, &grants, link) {
		if (!g->origin_name || g->origin != ws->node.id) {
			continue;
		}
		char *copy = strdup(ws->name);
		if (!copy) {
			sway_log(SWAY_ERROR, "Out of memory renaming a grant's workspace");
			continue;
		}
		free(g->origin_name);
		g->origin_name = copy;
	}
}

/* Reads an option's value, --name=VALUE or --name VALUE, if argv[*i] is
 * --name. Returns 1 for one, 0 for no such option, or -1 for a missing
 * or empty value. */
static int option(int argc, char **argv, int *i, const char *name, const char **value) {
	size_t n = strlen(name);
	const char *arg = argv[*i];
	if (strncmp(arg, name, n) != 0 || (arg[n] && arg[n] != '=')) {
		return 0;
	}
	if (arg[n] == '=') {
		*value = arg + n + 1;
	} else if (*i + 1 < argc) {
		*value = argv[++*i];
	} else {
		return -1;
	}
	return **value ? 1 : -1;
}

// A process id or a key, a whole number from min up
static bool number(const char *s, unsigned long min, unsigned long *n) {
	char *end;
	errno = 0;
	*n = strtoul(s, &end, 10);
	return !errno && end != s && !*end && isdigit((unsigned char)*s) && *n >= min
		&& *n <= INT_MAX;
}

static bool workspace_name_ok(const char *name) {
	return *name && strncmp(name, "__", 2) != 0;
}

static void free_apps(char **apps, size_t napps) {
	for (size_t i = 0; i < napps; i++) {
		free(apps[i]);
	}
	free(apps);
}

/* The words from argv[i] on as normalized app ids, or NULL with *bad set
 * for one that's empty or holds a quote, which ends nowhere */
static char **parse_apps(int argc, char **argv, int i, size_t *napps, bool *bad) {
	char **apps = NULL;
	*napps = 0;
	*bad = false;
	for (; i < argc; i++) {
		char *word = strchr(argv[i], '"') ? NULL : normalize(argv[i]);
		char **more = word ? realloc(apps, (*napps + 1) * sizeof(*apps)) : NULL;
		if (!more) {
			free(word);
			free_apps(apps, *napps);
			*napps = 0;
			*bad = true;
			return NULL;
		}
		apps = more;
		apps[(*napps)++] = word;
	}
	return apps;
}

static struct cmd_results *grant_extend(unsigned long key, unsigned long pid,
		char **apps, size_t napps) {
	/* Adds ids to the grant just made for apps[0], while it holds: one
	 * canceled, used or lapsed in between stays gone */
	struct grant *g;
	expire();
	wl_list_for_each(g, &grants, link) {
		if (g->canceled || g->key != key || g->pid != (pid_t)pid || !g->napps
				|| strcmp(g->apps[0], apps[0]) != 0) {
			continue;
		}
		for (size_t i = 1; i < napps; i++) {
			size_t j;
			for (j = 0; j < g->napps && strcmp(g->apps[j], apps[i]) != 0; j++) {
			}
			if (j < g->napps) {
				continue;
			}
			char **more = realloc(g->apps, (g->napps + 1) * sizeof(*g->apps));
			if (!more) {
				sway_log(SWAY_ERROR, "Out of memory extending a grant");
				break;
			}
			g->apps = more;
			g->apps[g->napps++] = apps[i];
			apps[i] = NULL;
		}
		break;
	}
	free_apps(apps, napps);
	return cmd_results_new(CMD_SUCCESS, NULL);
}

/*
 *   grant [--pid PID] [--key KEY] [--workspace NAME] [APP ...]
 *   grant --or-recent APP ...
 *   grant extend --key KEY [--pid PID] APP ID ...
 */
struct cmd_results *cmd_grant(int argc, char **argv) {
	unsigned long pid = 0, key = 0;
	const char *value, *workspace = NULL;
	bool extend = false, or_recent = false;
	int i = 0, r;
	if (argc > 0 && strcmp(argv[0], "extend") == 0) {
		extend = true;
		i++;
	}
	for (; i < argc && strncmp(argv[i], "--", 2) == 0; i++) {
		if (!extend && strcmp(argv[i], "--or-recent") == 0) {
			or_recent = true;
		} else if ((r = option(argc, argv, &i, "--pid", &value))) {
			if (r < 0 || !number(value, 2, &pid)) {
				return cmd_results_new(CMD_FAILURE, "grant: --pid takes a process id");
			}
		} else if ((r = option(argc, argv, &i, "--key", &value))) {
			if (r < 0 || !number(value, 1, &key)) {
				return cmd_results_new(CMD_FAILURE,
					"grant: --key takes a positive whole number");
			}
		} else if (!extend && (r = option(argc, argv, &i, "--workspace", &value))) {
			if (r < 0 || !workspace_name_ok(value)) {
				return cmd_results_new(CMD_FAILURE,
					"grant: --workspace takes a workspace's name");
			}
			workspace = value;
		} else {
			return cmd_results_new(CMD_FAILURE, "grant: %.64s is not an option (--pid, --key%s are)",
				argv[i], extend ? "" : ", --workspace, --or-recent");
		}
	}
	size_t napps;
	bool bad;
	char **apps = parse_apps(argc, argv, i, &napps, &bad);
	if (bad) {
		return cmd_results_new(CMD_FAILURE, "grant: expected app ids");
	}
	for (size_t j = 0; j < napps; j++) {
		if (strcmp(apps[j], "*") == 0 && (napps > 1 || extend)) {
			free_apps(apps, napps);
			return cmd_results_new(CMD_FAILURE,
				"grant: * stands alone, for any app's window");
		}
	}
	if (extend) {
		if (!key || napps < 2) {
			free_apps(apps, napps);
			return cmd_results_new(CMD_FAILURE,
				"grant extend needs --key, the grant's app, then ids to add");
		}
		return grant_extend(key, pid, apps, napps);
	}
	if (!napps && !pid) {
		free_apps(apps, napps);
		return cmd_results_new(CMD_FAILURE, "grant needs an app id, *, or --pid");
	}
	if (or_recent && (!napps || strcmp(apps[0], "*") == 0 || pid || key || workspace)) {
		free_apps(apps, napps);
		return cmd_results_new(CMD_FAILURE,
			"grant --or-recent takes app ids alone, whose window it brings up");
	}
	expire();
	struct grant *g = calloc(1, sizeof(*g));
	char *ws_copy = workspace ? strdup(workspace) : NULL;
	struct sway_workspace *cur = or_recent ? NULL
		: seat_get_focused_workspace(input_manager_current_seat());
	char *origin_name = cur ? strdup(cur->name) : NULL;
	if (!g || (workspace && !ws_copy) || (cur && !origin_name)) {
		free(g);
		free(ws_copy);
		free(origin_name);
		free_apps(apps, napps);
		return cmd_results_new(CMD_FAILURE, "grant: out of memory");
	}
	g->apps = apps;
	g->napps = napps;
	g->pid = (pid_t)pid;
	g->key = key;
	g->workspace = ws_copy;
	g->or_recent = or_recent;
	g->at = now_us();
	g->origin = cur ? cur->node.id : 0;
	g->origin_name = origin_name;
	wl_list_insert(grants.prev, &g->link);
	if (or_recent) {
		arm();
	}
	return cmd_results_new(CMD_SUCCESS, NULL);
}

struct cmd_results *tide_cmd_focus_recent(int argc, char **argv) {
	size_t napps;
	bool bad;
	char **apps = parse_apps(argc, argv, 0, &napps, &bad);
	for (size_t i = 0; !bad && i < napps; i++) {
		bad = strcmp(apps[i], "*") == 0;
	}
	if (bad) {
		free_apps(apps, napps);
		return cmd_results_new(CMD_INVALID, "focus recent: expected app ids");
	}
	if (!napps) {
		return cmd_results_new(CMD_INVALID, "focus recent needs an app id");
	}
	if (!most_recent(apps, napps)) {
		struct cmd_results *error = cmd_results_new(CMD_FAILURE,
			"No window of %.64s to bring up", apps[0]);
		free_apps(apps, napps);
		return error;
	}
	// You moving on from anything else
	cancel();
	bring_up(apps, napps, "focus recent");
	free_apps(apps, napps);
	return cmd_results_new(CMD_SUCCESS, NULL);
}

static bool parse_seconds(const char *arg, double *value) {
	char *end;
	errno = 0;
	*value = strtod(arg, &end);
	return end != arg && !*end && !errno && isfinite(*value)
		&& *value > 0 && *value <= MAX_SECONDS;
}

// focus_guard enable|disable|grant_seconds S|prompt_seconds S
struct cmd_results *cmd_focus_guard(int argc, char **argv) {
	struct tide_focus_config *fc = &config->tide_focus;
	if (argc == 1 && strcasecmp(argv[0], "enable") == 0) {
		fc->guard = true;
	} else if (argc == 1 && strcasecmp(argv[0], "disable") == 0) {
		fc->guard = false;
	} else if (argc >= 1 && (strcasecmp(argv[0], "grant_seconds") == 0
			|| strcasecmp(argv[0], "prompt_seconds") == 0)) {
		bool grant = strcasecmp(argv[0], "grant_seconds") == 0;
		double s;
		if (argc != 2 || !parse_seconds(argv[1], &s)) {
			return cmd_results_new(CMD_INVALID,
				"focus_guard %s takes a number of seconds, more than 0 and at most a day",
				grant ? "grant_seconds" : "prompt_seconds");
		}
		*(grant ? &fc->grant_seconds : &fc->prompt_seconds) = s;
	} else {
		return cmd_results_new(CMD_INVALID,
			"focus_guard takes enable, disable, grant_seconds S or prompt_seconds S");
	}
	return cmd_results_new(CMD_SUCCESS, NULL);
}
