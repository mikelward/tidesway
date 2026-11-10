#ifndef _SWAY_TIDE_STACK_H
#define _SWAY_TIDE_STACK_H

/*
 * Dialogs stay above their parent, as in tidewm: raising a floating
 * window raises its dialogs, and theirs, over it, in the order they were
 * stacked. sway raises only the window itself, leaving a floating
 * parent's dialogs under it.
 */

struct sway_container;

/* container_raise_floating, once it has raised floater: raises its dialogs */
void tide_stack_raise_dialogs(struct sway_container *floater);

#endif
