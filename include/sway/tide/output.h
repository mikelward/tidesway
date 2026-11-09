#ifndef _SWAY_TIDE_OUTPUT_H
#define _SWAY_TIDE_OUTPUT_H

/*
 * The output command's power, as tidewm takes it (its docs/ipc.md): output
 * NAME|* power on|off|toggle, alone, or dpms, its old name. It turns the
 * output off as an idle daemon does, keeping its windows and its place, or
 * on again; * means every enabled output, each toggled by its own power. A
 * disabled output stays off, and the command fails for it alone, as for a
 * name no output has. The config file's power lines are sway's.
 */

#include <stdbool.h>

struct cmd_results;

/* cmd_output, at runtime: whether this is a power command, which it then
 * runs, with its result in *result */
bool tide_output_power(int argc, char **argv, struct cmd_results **result);

#endif
