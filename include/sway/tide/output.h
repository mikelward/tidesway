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

#include <json-c/json.h>
#include <stdbool.h>

struct cmd_results;
struct sway_output;

/* cmd_output, at runtime: whether this is a power command, which it then
 * runs, with its result in *result */
bool tide_output_power(int argc, char **argv, struct cmd_results **result);

/* GET_OUTPUTS' layers and window_area, as tidewm's (its docs/ipc.md), for
 * tide's doctor: each mapped layer surface, from the bottom layer up, with
 * its namespace, layer, exclusive_zone and rect, and the area left for
 * windows, both in the layout's coordinates */
void tide_output_describe_layers(struct sway_output *output, json_object *object);

#endif
