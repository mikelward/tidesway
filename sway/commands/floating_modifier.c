#include <stdarg.h>
#include <strings.h>
#include "list.h"
#include "stringop.h"
#include "sway/commands.h"
#include "sway/config.h"
#include "sway/input/keyboard.h"

bool tide_floating_mod_held(uint32_t modifiers) {
	return config->floating_mod && (modifiers & config->floating_mod) == config->floating_mod;
}

struct cmd_results *cmd_floating_modifier(int argc, char **argv) {
	// tide: in tidewm's words, and its modifiers joined by +, all held
	const char *usage = "floating_modifier takes a modifier, as Mod4 or Mod4+Shift, "
		"or none, then normal or inverse";
	if (argc < 1 || argc > 2) {
		return cmd_results_new(CMD_INVALID, "%s", usage);
	}

	if (strcasecmp(argv[0], "none") == 0) {
		config->floating_mod = 0;
		return cmd_results_new(CMD_SUCCESS, NULL);
	}

	uint32_t mod = 0;
	list_t *split = split_string(argv[0], "+");
	for (int i = 0; i < split->length; i++) {
		uint32_t one = get_modifier_mask_by_name(split->items[i]);
		if (!one) {
			struct cmd_results *error = cmd_results_new(CMD_INVALID,
				"floating_modifier: no modifier is called %s", (char *)split->items[i]);
			list_free_items_and_destroy(split);
			return error;
		}
		mod |= one;
	}
	list_free_items_and_destroy(split);
	if (!mod) {
		return cmd_results_new(CMD_INVALID, "%s", usage);
	}

	if (argc == 1 || strcasecmp(argv[1], "normal") == 0) {
		config->floating_mod_inverse = false;
	} else if (strcasecmp(argv[1], "inverse") == 0) {
		config->floating_mod_inverse = true;
	} else {
		return cmd_results_new(CMD_INVALID, "%s", usage);
	}

	config->floating_mod = mod;

	return cmd_results_new(CMD_SUCCESS, NULL);
}
