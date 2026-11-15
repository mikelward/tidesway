#ifndef _SWAY_TIDE_XKBENV_H
#define _SWAY_TIDE_XKBENV_H
#include <stdbool.h>
#include <xkbcommon/xkbcommon.h>

/*
 * The environment's XKB_DEFAULT_ names, as tidewm takes them: once XKB
 * can't make a keymap of them, as a layout it hasn't got, they're said and
 * left out of every keymap from then on, so a keyboard gets XKB's own
 * layout where the config names none, rather than no keymap.
 */

// The flags for an XKB context: without the environment's names once dropped
enum xkb_context_flags tide_xkb_flags(void);

/* XKB couldn't make a keymap with the environment's names in: drops them,
 * saying which they were. Returns false if they were dropped already. */
bool tide_xkb_drop_environment(void);

#endif
