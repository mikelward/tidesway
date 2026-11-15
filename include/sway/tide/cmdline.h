#ifndef _SWAY_TIDE_CMDLINE_H
#define _SWAY_TIDE_CMDLINE_H

/*
 * A command list as tidewm reads one (its docs/ipc.md, "Commands"):
 * commands end at a `;`, or sway's `,`, outside double quotes. An
 * apostrophe is no quote, so a workspace can be named `Bob's tasks`. An
 * `exec` takes the rest of the list, `;` and all, and its shell gets it as
 * written, so its quotes are the shell's.
 */

#include <stdbool.h>
#include <stddef.h>

/* Whether cmd, its criteria taken off, is exec or exec_always: the length
 * of its name, or 0 for another command */
size_t tide_cmd_exec_name(const char *cmd);

/* The next command in *stringp, cut off at the first `;` or `,` outside
 * double quotes or after a backslash, as sway's argsep cuts, *matched the
 * one it ended at, or '\0' at the end. *stringp goes past it, or to NULL. */
char *tide_cmd_next(char **stringp, char *matched);

/* exec's arguments, cmd being exec or exec_always, name long, and its
 * rest: the command's name, then `--no-startup-id` if it's there, then
 * the rest as written, one argument, its space at the end and all. Into
 * *argc, and NULL-terminated, for free_argv. */
char **tide_exec_args(const char *cmd, size_t name, int *argc);

/* Whether exec's one argument is one quoted word, which sway takes the
 * quotes off, as in `exec "foo --bar"`, rather than the shell reading them */
bool tide_exec_one_quoted(const char *arg);

/* Whether a config file's line closes a block: } as its first word, as in
 * tidewm, so a binding's shell command can end in one */
bool tide_cmd_closes_block(const char *line);

#endif
