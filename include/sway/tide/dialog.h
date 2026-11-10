#ifndef _SWAY_TIDE_DIALOG_H
#define _SWAY_TIDE_DIALOG_H

/*
 * Dialogs and other floating windows, as tidewm has them (its README,
 * "Dialogs and floating windows", tide's SPEC §6.4). A window floats when
 * it has a parent, is modal (xdg-dialog-v1), asks for a fixed size, or
 * has neither an app ID nor a title, or a rule floats it. One that floats
 * as it opens goes in the middle of its parent, or of its output's window
 * area without one, as big as it asks but no more than 80% of the output,
 * unless its fixed size is bigger, and kept in the window area. A
 * browser's picture-in-picture window, titled "Picture-in-Picture" or so,
 * floats a quarter of the output each way in the window area's bottom
 * right corner, 16 px in, and is sticky. A dialog follows its parent to
 * another workspace or output, to the middle of it there, unless it's
 * sticky itself.
 */

#include <stdbool.h>

struct sway_view;

// Whether view is a browser's picture-in-picture window, by its title
bool tide_dialog_pip(struct sway_view *view);

// Whether view is a modal dialog, over xdg-dialog-v1
bool tide_dialog_modal(struct sway_view *view);

/* view_map's, once the rules have run: a window floating as it opens
 * takes its place, and picture-in-picture is sticky */
void tide_dialog_place(struct sway_view *view);

/* transaction_commit_dirty's, before anything is committed: each window
 * that went to another workspace takes its dialogs along */
void tide_dialog_follow(void);

// server_init's: offers xdg-dialog-v1
bool tide_dialog_init(void);

#endif
