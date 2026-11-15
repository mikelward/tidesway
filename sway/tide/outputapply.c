/*
 * The outputs taking their settings, as tidewm's do.
 * include/sway/tide/outputapply.h says how.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wayland-util.h>
#include <wlr/backend.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_output_management_v1.h>
#include <wlr/types/wlr_output_swapchain_manager.h>
#include <wlr/util/box.h>
#include "list.h"
#include "log.h"
#include "sway/commands.h"
#include "sway/config.h"
#include "sway/output.h"
#include "sway/server.h"
#include "sway/tide/configinfo.h"
#include "sway/tide/outputapply.h"
#include "sway/tree/root.h"

static bool started; // the backend has listed its outputs
static bool file_pending; // the next apply is the file's
static bool gone_pending; // an output has gone since the last apply
static int file_errors; // the file's last apply's errors
static bool watching; // in an IPC command, for its reload's result
static int command_index; // the result the command running now will have
static int reload_index = -1; // the reload's result, or -1

void tide_output_file_loaded(void) {
	file_pending = true;
	file_errors = 0;
}

void tide_output_gone(bool was_enabled) {
	// One that was off leaves those on as they were: none on is a command's doing
	gone_pending |= was_enabled;
}

void tide_output_started(void) {
	started = true;
}

void tide_output_commanded(const char *name) {
	struct sway_output *output;
	wl_list_for_each(output, &root->all_outputs, link) {
		if (output != root->fallback_output && (strcmp(name, "*") == 0
				|| output == all_output_by_name_or_id(name))) {
			output->tide_file_off = false;
			output->tide_kept = false;
		}
	}
}

// The outputs an output management client's configuration turns on or off
static list_t *toggling;

void tide_output_heads_begin(struct wlr_output_configuration_v1 *cfg) {
	if (!toggling && !(toggling = create_list())) {
		sway_log(SWAY_ERROR, "tide: no memory to note the outputs a client turns on or off");
		return;
	}
	toggling->length = 0;
	struct wlr_output_configuration_head_v1 *head;
	wl_list_for_each(head, &cfg->heads, link) {
		struct sway_output *output = head->state.output->data;
		if (output && output->enabled != head->state.enabled) {
			list_add(toggling, output);
		}
	}
}

void tide_output_heads_end(bool took) {
	for (int i = 0; took && toggling && i < toggling->length; i++) {
		struct sway_output *output = toggling->items[i];
		output->tide_file_off = false;
		output->tide_kept = false;
	}
	if (toggling) {
		toggling->length = 0;
	}
}

// An error of the file's, of no line, which a reload's reply names
static void file_error(bool file, const char *fmt, const char *name) {
	char msg[256];
	snprintf(msg, sizeof(msg), fmt, name);
	sway_log(SWAY_ERROR, "tidewm: %s", msg);
	// tidewm's file, which tidesway's command names, else sway's own
	tide_config_file_error(config->tide_user.path ? config->tide_user.path
		: config->current_config_path, msg);
	if (file) {
		file_errors++;
	}
}

// Whether output would take oc, alone, as it is
static bool works(struct sway_output *output, struct output_config *oc) {
	struct wlr_backend_output_state state = { .output = output->wlr_output };
	wlr_output_state_init(&state.base);
	queue_output_config(oc, output, &state.base);
	struct wlr_output_swapchain_manager manager;
	wlr_output_swapchain_manager_init(&manager, server.backend);
	bool ok = wlr_output_swapchain_manager_prepare(&manager, &state, 1);
	wlr_output_swapchain_manager_finish(&manager);
	wlr_output_state_finish(&state.base);
	return ok;
}

// oc's mode, scale and turn made output's now
static void keep_current(struct output_config *oc, struct sway_output *output) {
	struct wlr_output *wlr = output->wlr_output;
	oc->drm_mode.type = -1;
	if (wlr->current_mode) {
		oc->width = wlr->current_mode->width;
		oc->height = wlr->current_mode->height;
		oc->refresh_rate = wlr->current_mode->refresh / 1000.f;
		oc->custom_mode = 0;
	} else {
		oc->width = wlr->width;
		oc->height = wlr->height;
		oc->refresh_rate = wlr->refresh / 1000.f;
		oc->custom_mode = 1;
	}
	oc->scale = wlr->scale;
	oc->transform = wlr->transform;
}

// oc's mode made the output's preferred one
static void preferred_mode(struct output_config *oc) {
	oc->drm_mode.type = -1;
	oc->width = oc->height = -1;
	oc->refresh_rate = -1;
	oc->custom_mode = -1;
}

/* Whether output takes oc enabled, or else in what it has or, off, its
 * preferred mode, as tidewm keeps one on in; oc is left as it takes it,
 * or as it was */
static bool takes_enabled(struct sway_output *output, struct output_config *oc) {
	struct output_config was = *oc;
	oc->enabled = 1;
	if (works(output, oc)) {
		return true;
	}
	if (output->enabled) {
		keep_current(oc, output);
	} else {
		preferred_mode(oc);
	}
	if (works(output, oc)) {
		return true;
	}
	*oc = was;
	return false;
}

/* The output kept on when the file's lines would leave none on: one
 * that's on now, if any is and it's a candidate, else the first connected
 * that takes it, its line's mode or its own; on unplug, only one the
 * file's line has off is */
static int choose_kept(struct sway_output **outputs, struct output_config **ocs,
		size_t len, bool file) {
	for (int pass = 0; pass < 2; pass++) {
		for (size_t i = 0; i < len; i++) {
			if ((!file && !outputs[i]->tide_file_off) || (pass == 0 && !outputs[i]->enabled)) {
				continue;
			}
			if (takes_enabled(outputs[i], ocs[i])) {
				return (int)i;
			}
		}
	}
	return -1;
}

void tide_output_apply_stored(void) {
	bool file = file_pending && started, gone = gone_pending;
	if (started) {
		file_pending = false;
	}
	gone_pending = false;
	size_t len = 0, n = wl_list_length(&root->all_outputs);
	struct output_config **ocs = calloc(n ? n : 1, sizeof(*ocs));
	struct sway_output **outputs = calloc(n ? n : 1, sizeof(*outputs));
	if (!ocs || !outputs) {
		sway_log(SWAY_ERROR, "tide: no memory to check the outputs' settings; applying them as sway does");
		free(ocs);
		free(outputs);
		apply_output_configs((struct output_config **)config->output_configs->items,
			config->output_configs->length, false, true);
		return;
	}
	// The first connected first
	struct sway_output *output;
	wl_list_for_each_reverse(output, &root->all_outputs, link) {
		if (output == root->fallback_output) {
			continue;
		}
		struct output_config *oc = find_output_config(output);
		if (!oc) {
			sway_log(SWAY_ERROR, "tide: no memory for %s's settings", output->wlr_output->name);
			continue;
		}
		outputs[len] = output;
		ocs[len++] = oc;
	}

	bool any_on = false;
	for (size_t i = 0; i < len; i++) {
		output = outputs[i];
		// The file's lines, which a new output takes too, say what's off
		if (file || !output->tide_configured) {
			output->tide_file_off = ocs[i]->enabled == 0;
			output->tide_kept = false;
		} else if (output->tide_kept && ocs[i]->enabled == 0) {
			ocs[i]->enabled = 1;
		}
		any_on |= ocs[i]->enabled != 0;
	}
	// A command may turn every output off; the file's lines, or an unplug, can't
	if (!any_on && started && len && (file || gone)) {
		int kept = choose_kept(outputs, ocs, len, file);
		if (kept >= 0) {
			ocs[kept]->enabled = 1;
			outputs[kept]->tide_kept = true;
			outputs[kept]->tide_file_off = false;
			file_error(file, "output %s is on, though its line disables it: "
				"no other output is on", outputs[kept]->wlr_output->name);
		}
	}

	for (size_t i = 0; i < len; i++) {
		output = outputs[i];
		bool first = !output->tide_configured;
		output->tide_configured = true;
		if (ocs[i]->enabled == 0 || works(output, ocs[i])) {
			continue;
		}
		const char *why;
		if (output->enabled) {
			keep_current(ocs[i], output);
			why = "output %s refused its settings, so it keeps what it had";
		} else {
			preferred_mode(ocs[i]);
			why = "output %s refused its settings, so it has its preferred mode";
		}
		if (file || first) {
			file_error(file, why, output->wlr_output->name);
		} else {
			// A command's: sway's reply was success, as it's applied later
			char msg[256];
			snprintf(msg, sizeof(msg), why, output->wlr_output->name);
			sway_log(SWAY_ERROR, "tidewm: %s", msg);
		}
		if (!works(output, ocs[i])) {
			sway_log(SWAY_ERROR, "tidewm: output %s refuses that too; sway looks for "
				"what it takes", output->wlr_output->name);
		}
	}

	apply_output_configs(ocs, len, false, true);
	for (size_t i = 0; i < len; i++) {
		free_output_config(ocs[i]);
	}
	free(ocs);
	free(outputs);
}

void tide_output_reply_begin(void) {
	watching = true;
	reload_index = -1;
}

void tide_output_command_index(int index) {
	command_index = index;
}

void tide_output_reload_ran(void) {
	if (watching) {
		reload_index = command_index;
	}
}

void tide_output_reply_end(list_t *results) {
	/* By its place in the reply, not the reload's own result, which sway
	 * frees when it ran on windows, giving one result for them all */
	struct cmd_results *reload_result = reload_index >= 0 && results
		&& reload_index < results->length ? results->items[reload_index] : NULL;
	if (reload_result && file_errors && reload_result->status == CMD_SUCCESS) {
		char *errors = tide_config_errors_summary();
		size_t size = (errors ? strlen(errors) : 0) + 64;
		char *error = malloc(size);
		if (error) {
			snprintf(error, size, "%s; the rest of the file is loaded",
				errors ? errors : "an output refused its settings");
			reload_result->status = CMD_FAILURE;
			free(reload_result->error);
			reload_result->error = error;
		} else {
			sway_log(SWAY_ERROR, "tide: no memory to say why the reload failed");
		}
		free(errors);
	}
	watching = false;
	reload_index = -1;
}

bool tide_output_head_unmoved(struct wlr_output_configuration_head_v1 *head) {
	struct wlr_box box;
	wlr_output_layout_get_box(root->output_layout, head->state.output, &box);
	return !wlr_box_empty(&box) && box.x == head->state.x && box.y == head->state.y;
}
