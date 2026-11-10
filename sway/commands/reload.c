#include <string.h>
#include "sway/commands.h"
#include "sway/config.h"
#include "sway/tide/configinfo.h"
#include "sway/ipc-server.h"
#include "sway/server.h"
#include "sway/tree/arrange.h"
#include "sway/tree/view.h"
#include "list.h"
#include "log.h"

static void title_bar_update_iterator(struct sway_container *con, void *data) {
	container_update_title_bar(con);
}

// tide: whether a reload waits on its idle event
static bool reload_scheduled;

bool tide_reload_take_scheduled(void) {
	bool scheduled = reload_scheduled;
	reload_scheduled = false;
	return scheduled;
}

static void do_reload(void *data) {
	// store bar ids to check against new bars for barconfig_update events
	list_t *bar_ids = create_list();
	for (int i = 0; i < config->bars->length; ++i) {
		struct bar_config *bar = config->bars->items[i];
		list_add(bar_ids, strdup(bar->id));
	}

	const char *path = NULL;
	if (config->user_config_path) {
		path = config->current_config_path;
	}

	if (!load_main_config(path, true, false)) {
		sway_log(SWAY_ERROR, "Error(s) reloading config");
		list_free_items_and_destroy(bar_ids);
		return;
	}

	ipc_event_workspace(NULL, NULL, "reload");

	load_swaybars();

	for (int i = 0; i < config->bars->length; ++i) {
		struct bar_config *bar = config->bars->items[i];
		for (int j = 0; j < bar_ids->length; ++j) {
			if (strcmp(bar->id, bar_ids->items[j]) == 0) {
				ipc_event_barconfig_update(bar);
				break;
			}
		}
	}
	list_free_items_and_destroy(bar_ids);

	root_for_each_container(title_bar_update_iterator, NULL);

	arrange_root();
}

struct cmd_results *cmd_reload(int argc, char **argv) {
	struct cmd_results *error = NULL;
	if ((error = checkarg(argc, "reload", EXPECTED_EQUAL_TO, 0))) {
		return error;
	}

	const char *path = NULL;
	if (config->user_config_path) {
		path = config->current_config_path;
	}

	if (!load_main_config(path, true, true)) {
		// tide: the first few errors, naming their files and lines, as tidewm's
		char *errors = tide_config_errors_summary();
		struct cmd_results *res = cmd_results_new(CMD_FAILURE, "%s",
			errors ? errors : "Error(s) reloading config.");
		free(errors);
		return res;
	}

	// The reload command frees a lot of stuff, so to avoid use-after-frees
	// we schedule the reload to happen using an idle event.
	wl_event_loop_add_idle(server.wl_event_loop, do_reload, NULL);
	reload_scheduled = true;

	return cmd_results_new(CMD_SUCCESS, NULL);
}
