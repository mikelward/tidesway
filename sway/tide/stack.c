/*
 * Dialogs above their floating parent, as tidewm stacks them.
 * include/sway/tide/stack.h says how it behaves.
 */
#include <wlr/types/wlr_scene.h>
#include "list.h"
#include "log.h"
#include "sway/tide/stack.h"
#include "sway/tree/container.h"
#include "sway/tree/node.h"
#include "sway/tree/view.h"
#include "sway/tree/workspace.h"

void tide_stack_raise_dialogs(struct sway_container *floater) {
	struct sway_workspace *ws = floater->pending.workspace;
	if (!floater->view || !ws) {
		return;
	}
	list_t *dialogs = create_list();
	if (!dialogs) {
		sway_log(SWAY_ERROR, "tide: no memory to raise a window's dialogs");
		return;
	}
	for (int i = 0; i < ws->floating->length; i++) {
		struct sway_container *con = ws->floating->items[i];
		if (con != floater && con->view && view_is_transient_for(con->view, floater->view)) {
			list_add(dialogs, con);
		}
	}
	// In list order, so each keeps its place among the others
	for (int i = 0; i < dialogs->length; i++) {
		struct sway_container *dialog = dialogs->items[i];
		wlr_scene_node_raise_to_top(&dialog->scene_tree->node);
		list_move_to_end(ws->floating, dialog);
	}
	if (dialogs->length) {
		node_set_dirty(&ws->node);
	}
	list_free(dialogs);
}
