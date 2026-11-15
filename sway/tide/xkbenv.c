/*
 * The environment's XKB_DEFAULT_ names dropped once XKB can't make a keymap
 * of them, as tidewm's settings.h does; include/sway/tide/xkbenv.h says how.
 */
#include <stdlib.h>
#include "log.h"
#include "stringop.h"
#include "sway/tide/xkbenv.h"

static bool dropped;

enum xkb_context_flags tide_xkb_flags(void) {
	return dropped ? XKB_CONTEXT_NO_ENVIRONMENT_NAMES : XKB_CONTEXT_NO_FLAGS;
}

bool tide_xkb_drop_environment(void) {
	static const char *const vars[] = { "XKB_DEFAULT_RULES", "XKB_DEFAULT_MODEL",
		"XKB_DEFAULT_LAYOUT", "XKB_DEFAULT_VARIANT", "XKB_DEFAULT_OPTIONS" };
	if (dropped) {
		return false;
	}
	dropped = true;
	char *names = NULL;
	for (size_t i = 0; i < sizeof(vars) / sizeof(vars[0]); i++) {
		const char *value = getenv(vars[i]);
		if (value) {
			char *more = format_str("%s %s=%s", names ? names : "", vars[i], value);
			free(names);
			names = more;
		}
	}
	// "tidewm:", as tidewm says it, and its tests look for
	sway_log(SWAY_ERROR, "tidewm: XKB can't make a keymap of the environment's%s; "
		"keyboards have XKB's own layout, us, where the config file names none",
		names ? names : "");
	free(names);
	return true;
}
