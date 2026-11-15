#ifndef _SWAY_TIDE_OUTPUTAPPLY_H
#define _SWAY_TIDE_OUTPUTAPPLY_H

/*
 * The outputs taking their settings, as tidewm's settings.h has them take
 * the file's (its docs/config.md):
 *
 * - The file's lines never leave every output off. When they would, at
 *   startup or on reload, one stays on, one that's on now if any is, and
 *   that's an error of the file's, which fails a reload, though the rest
 *   of the file loads. When the only output that's on is unplugged, one
 *   the file's line had off comes on, the same way. A command may still
 *   turn every output off, as a lid's bindswitch does, and one that turns
 *   an output off or on, or an output management client that does, says
 *   it's no longer the file's line that has it so.
 * - An output that refuses its settings keeps what it had, or, at startup
 *   or plugged in, takes its preferred mode, and says so: an error of the
 *   file's when the file's lines are what it refused.
 * - power or dpms in the file is taken but left: every output starts on,
 *   as the file nwg-displays saves says dpms off for one that was off then.
 *   A command's power goes after its other settings, which an output that
 *   goes off has taken.
 * - An output management client's head that it didn't move, as it last
 *   saw it, isn't put where it is, so one placed beside the others stays
 *   beside the one moved.
 */

#include <stdbool.h>
#include "list.h"

struct cmd_results;
struct output_config;
struct sway_output;
struct wlr_output_configuration_head_v1;
struct wlr_output_configuration_v1;
struct wlr_output_state;

/* sway's stored output configs applied, as above; sway's
 * apply_stored_output_configs */
void tide_output_apply_stored(void);

// The file's lines are read: the next apply is theirs
void tide_output_file_loaded(void);

/* An output has gone, as one unplugged: if it was enabled, and that leaves
 * none on, one the file has off comes on */
void tide_output_gone(bool was_enabled);

/* The backend has listed the outputs there are at startup: until then,
 * an output its line disables may be the first of several */
void tide_output_started(void);

/* A command set whether the outputs name names (*, a name, or its
 * identifier) are enabled: it's no longer the file's lines that do */
void tide_output_commanded(const char *name);

/* Around an output management client's configuration: one that took
 * turns outputs on or off as a command does (tide_output_commanded) */
void tide_output_heads_begin(struct wlr_output_configuration_v1 *cfg);
void tide_output_heads_end(bool took);

/* Around an IPC command: a reload in it whose outputs refused the file
 * fails, once they've taken it, naming them. execute_command says which
 * of its results the command it runs next will have; a reload says it
 * ran; results is the reply's, once the outputs have taken the file. */
void tide_output_reply_begin(void);
void tide_output_command_index(int index);
void tide_output_reload_ran(void);
void tide_output_reply_end(list_t *results);

// Whether a head puts its output where it is now, as the client last saw it
bool tide_output_head_unmoved(struct wlr_output_configuration_head_v1 *head);

// sway's, config/output.c's: oc's settings for output, into pending
void queue_output_config(struct output_config *oc, struct sway_output *output,
	struct wlr_output_state *pending);

#endif
