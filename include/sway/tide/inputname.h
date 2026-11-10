#ifndef _SWAY_TIDE_INPUTNAME_H
#define _SWAY_TIDE_INPUTNAME_H
#include <stdbool.h>

struct sway_input_device;

/* The name Hyprland would give device, as tidewm's GET_INPUTS does, unique
 * in GET_INPUTS' order; the caller frees it */
char *tide_input_hyprland_name(struct sway_input_device *device);

/* Whether a and b have one Hyprland name but for its number, so one
 * coming or going numbers the other anew */
bool tide_input_namesakes(struct sway_input_device *a, struct sway_input_device *b);

// Whether an input line's name names device: its identifier or Hyprland name
bool tide_input_named(struct sway_input_device *device, const char *name);

#endif
