/*
 * The pointer's size, as tidewm takes it (include/sway/tide/cursor.h).
 */
#include <stdlib.h>
#include "log.h"
#include "sway/tide/cursor.h"

unsigned tide_cursor_size(void) {
	// Read once, before sway sets XCURSOR_SIZE for what it runs
	static long size = 0;
	if (size == 0) {
		const char *env = getenv("XCURSOR_SIZE");
		size = env ? strtol(env, NULL, 10) : 0;
		if (size <= 0 || size > 512) {
			if (env) {
				sway_log(SWAY_INFO, "tide: XCURSOR_SIZE isn't a size from 1 to 512, "
					"so the pointer is 24");
			}
			size = 24;
		}
	}
	return (unsigned)size;
}
