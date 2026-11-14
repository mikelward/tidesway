#ifndef _SWAY_TIDE_ANNOUNCE_H
#define _SWAY_TIDE_ANNOUNCE_H
#include <stdbool.h>

struct sway_view;

/*
 * When a window's new event goes.
 *
 * sway tells a new window no size at first, so an app draws at a size of
 * its own, maps, and is only then told the size its place gives it. A
 * window event at map would name a window still about to change size: a
 * capture of it begun then is refused its first frame, as the frame's
 * buffer is for the old size, and a shell that drew it would draw it
 * twice. tidewm's new event names a window at its size, so tidesway's
 * waits for the transaction that gives the window its size: until the app
 * has drawn at it, or sway has given up waiting for that. The window's
 * other events (its focus, say) wait behind its new one, in order; a focus
 * event that waited isn't sent if focus has moved on by then, as the
 * window that took it has already said so.
 */

// The window is mapped and placed: its new event goes once it has its size
void tide_announce_mapped(struct sway_view *view);

/* Sends the window's new event and those that waited behind it, if they
 * haven't gone: as its first transaction applies, or as it unmaps */
void tide_announce(struct sway_view *view);

/* Whether a window event of this kind waits behind the window's new one.
 * If so it's kept, to go after it */
bool tide_announce_holds(struct sway_view *view, const char *change);

#endif
