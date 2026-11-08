#ifndef _SWAY_TIDE_COMMANDS_H
#define _SWAY_TIDE_COMMANDS_H

struct cmd_results;

/* sway's layout command, for tide's layouts: tile, threecol, twocol,
 * monocle, next, prev, and toggle on a workspace in tide's layouts.
 * Returns NULL for one of sway's layouts, which sway's command checks and
 * sets, turning the workspace's tide layout off once it's valid. */
struct cmd_results *tide_cmd_layout(int argc, char **argv);

/* sway's new_window, for tide's end|top|next|master. Returns NULL for
 * sway's deprecated border styles, which default_border takes. */
struct cmd_results *tide_cmd_new_window(int argc, char **argv);

#endif
