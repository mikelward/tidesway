#ifndef _SWAY_TIDE_KEYS_H
#define _SWAY_TIDE_KEYS_H
#include <stdbool.h>
#include "list.h"

struct sway_config;
struct sway_mode;
struct sway_seat;

// A mode of tidewm's built-in keys, config.h's, for the config to keep
struct sway_mode *tide_keys_builtin(void);

// Adds tidewm's built-in rules, config.h's, to the config being read
void tide_keys_builtin_rules(void);

/* A mode of the keys that are always there to fix a config with and to
 * leave: config.h's quit, terminal and launcher keys */
struct sway_mode *tide_keys_safety(void);

/* Those keys, while they work: at the top level of a config that binds
 * keys, while it has an error or binds none that quits; else NULL. A key
 * press looks in them after the config's own */
struct sway_mode *tide_keys_safety_mode(void);

/* Sets $mod to Mod4, Super, as tidewm does before its config, so a config
 * that lost its set $mod line (an include gone missing) still binds its
 * $mod keys; the config's own set wins. False when out of memory */
bool tide_keys_default_vars(list_t *symbols);

// Whether config binds a key at its top level, so the built-in keys are out
bool tide_keys_bound(struct sway_config *config);

/* The mode whose keys a key press looks in: the current one, or the
 * built-in keys at the top level of a config that binds none there */
struct sway_mode *tide_keys_mode(void);

/* A button, scroll or touchpad gesture came while a key was down, so its
 * --release binding isn't a tap and doesn't run: Super tapped alone, not
 * Super and a click, a drag or a scroll, as tidewm has it */
void tide_keys_pointer_used(struct sway_seat *seat);

#endif
