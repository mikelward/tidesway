#ifndef _SWAY_TIDE_CURSOR_H
#define _SWAY_TIDE_CURSOR_H

/*
 * The pointer's size, as tidewm takes it: XCURSOR_SIZE as tidesway was
 * started with it, from 1 to 512, or 24 without one, for the pointer and
 * for the apps tidesway runs, until a seat's xcursor_theme line gives one.
 */

unsigned tide_cursor_size(void);

#endif
