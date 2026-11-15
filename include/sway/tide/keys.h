#ifndef _SWAY_TIDE_KEYS_H
#define _SWAY_TIDE_KEYS_H
#include <stdbool.h>

struct sway_config;
struct sway_mode;

// A mode of tidewm's built-in keys, config.h's, for the config to keep
struct sway_mode *tide_keys_builtin(void);

// Whether config binds a key at its top level, so the built-in keys are out
bool tide_keys_bound(struct sway_config *config);

/* The mode whose keys a key press looks in: the current one, or the
 * built-in keys at the top level of a config that binds none there */
struct sway_mode *tide_keys_mode(void);

#endif
