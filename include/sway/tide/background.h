#ifndef _SWAY_TIDE_BACKGROUND_H
#define _SWAY_TIDE_BACKGROUND_H

/*
 * The color behind windows, as tidewm's: its config.h's rootcolor,
 * #222222, under everything, where no window, layer surface or wallpaper
 * is. sway draws nothing there, so an output shows the renderer's black
 * unless swaybg runs. A wallpaper, a layer surface in the background
 * layer, covers it, as on tidewm.
 *
 * It's one rectangle the size of the output layout, below every one of
 * sway's layers, out of node_at_coords' way: the pointer over it is over
 * the output, as before.
 */

// Makes the rectangle, once root is, and keeps it the layout's size
void tide_background_init(void);

// Stops following the output layout, before it goes
void tide_background_fini(void);

#endif
