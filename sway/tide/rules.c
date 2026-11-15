/*
 * for_window rules as tidewm reads them (its docs/config.md, "Window
 * rules"): its errors, in its words, and a match PCRE2 can't finish said in
 * the log. sway's criteria still do the matching, with their other keys.
 */
#define PCRE2_CODE_UNIT_WIDTH 8
#include <ctype.h>
#include <pcre2.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include "list.h"
#include "log.h"
#include "stringop.h"
#include "sway/commands.h"
#include "sway/config.h"
#include "sway/criteria.h"
#include "sway/tide/rules.h"

// sway's criteria, each with whether its value is a pattern, and whether it
// may go without a value
static const struct { const char *name; bool pattern, bare; } keys[] = {
	{ "all", false, true },
	{ "app_id", true, false },
	{ "class", true, false },
	{ "con_id", false, false },
	{ "con_mark", true, false },
	{ "floating", false, true },
	{ "id", false, false },
	{ "instance", true, false },
	{ "pid", false, false },
	{ "sandbox_app_id", true, false },
	{ "sandbox_engine", true, false },
	{ "sandbox_instance_id", true, false },
	{ "shell", true, false },
	{ "tag", true, false },
	{ "tiling", false, true },
	{ "title", true, false },
	{ "urgent", false, false },
	{ "window_role", true, false },
	{ "window_type", false, false },
	{ "workspace", true, false },
};
#define NKEYS (sizeof(keys) / sizeof(keys[0]))

static struct cmd_results *check(char *p) {
	const char *needs = "for_window needs [criteria], then commands";
	bool seen[NKEYS] = { false }, any = false;
	if (*p++ != '[') {
		return cmd_results_new(CMD_INVALID, "%s", needs);
	}
	for (;;) {
		while (isspace((unsigned char)*p)) {
			p++;
		}
		if (*p == ']' || !*p) {
			break;
		}
		char *key = p;
		while (isalnum((unsigned char)*p) || *p == '_') {
			p++;
		}
		size_t n = p - key, i;
		for (i = 0; i < NKEYS && (strlen(keys[i].name) != n
				|| strncmp(key, keys[i].name, n) != 0); i++) {}
		// Spaces around the =, which sway's criteria_parse skips, as tidewm's doesn't
		char *eq = p;
		while (isspace((unsigned char)*eq)) {
			eq++;
		}
		bool bare = i < NKEYS && keys[i].bare && *eq != '=';
		if (!n || (*eq != '=' && !bare)) {
			return cmd_results_new(CMD_INVALID,
				"for_window's criteria are key=value, in [ ]");
		}
		if (i == NKEYS) {
			return cmd_results_new(CMD_INVALID,
				"for_window: no criterion is called %.*s", (int)n, key);
		}
		if (seen[i]) {
			return cmd_results_new(CMD_INVALID,
				"for_window: %s is there twice", keys[i].name);
		}
		seen[i] = any = true;
		if (bare) {
			continue;
		}
		for (p = eq + 1; isspace((unsigned char)*p); p++) {}
		// The value, a word or in double quotes, where \" is a quote
		char *value = p, *end;
		if (*p == '"') {
			for (value = ++p; *p && *p != '"'; p++) {
				if (*p == '\\' && p[1] == '"') {
					p++;
				}
			}
			if (*p != '"') {
				return cmd_results_new(CMD_INVALID,
					"for_window: %s's value has no closing \"", keys[i].name);
			}
			end = p++;
		} else {
			while (*p && *p != ']' && !isspace((unsigned char)*p)) {
				p++;
			}
			end = p;
		}
		if (!keys[i].pattern) {
			continue;
		}
		char *text = strndup(value, end - value);
		if (!text) {
			return cmd_results_new(CMD_FAILURE, "Out of memory reading for_window");
		}
		// sway takes \" in a quoted value as a quote, as here
		char *out = text;
		for (char *in = text; *in; in++) {
			if (*in == '\\' && in[1] == '"') {
				in++;
			}
			*out++ = *in;
		}
		*out = '\0';
		int code;
		PCRE2_SIZE at;
		pcre2_code *re = strcmp(text, "__focused__") == 0 ? NULL
			: pcre2_compile((PCRE2_SPTR)text, PCRE2_ZERO_TERMINATED,
				PCRE2_UTF | PCRE2_UCP, &code, &at, NULL);
		if (!re && strcmp(text, "__focused__") != 0) {
			PCRE2_UCHAR msg[160];
			pcre2_get_error_message(code, msg, sizeof(msg));
			struct cmd_results *error = cmd_results_new(CMD_INVALID,
				"for_window: %s=%s: %s at character %zu",
				keys[i].name, text, (char *)msg, (size_t)at);
			free(text);
			return error;
		}
		pcre2_code_free(re);
		free(text);
	}
	if (*p != ']') {
		return cmd_results_new(CMD_INVALID, "for_window's criteria have no ]");
	}
	for (p++; isspace((unsigned char)*p); p++) {}
	if (!any || !*p) {
		return cmd_results_new(CMD_INVALID, "%s", needs);
	}
	return NULL;
}

struct cmd_results *tide_rule_check(int argc, char **argv) {
	if (argc < 1) {
		return cmd_results_new(CMD_INVALID,
			"for_window needs [criteria], then commands");
	}
	char *line = join_args(argv, argc);
	if (!line) {
		return cmd_results_new(CMD_FAILURE, "Out of memory reading for_window");
	}
	struct cmd_results *error = check(line);
	free(line);
	return error;
}

void tide_rule_match_failed(const struct criteria *criteria, const char *key,
		int code) {
	// Rules count as tidewm's do: for_window lines, from 1
	int n = 0;
	bool found = false;
	for (int i = 0; config && !found && i < config->criteria->length; i++) {
		struct criteria *c = config->criteria->items[i];
		if (c->type == CT_COMMAND) {
			n++;
		}
		found = c == criteria;
	}
	PCRE2_UCHAR msg[160];
	pcre2_get_error_message(code, msg, sizeof(msg));
	// "tidewm:", as tidewm's log says it, and its tests look for
	if (found && criteria->type == CT_COMMAND) {
		sway_log(SWAY_ERROR, "tidewm: for_window rule %d: couldn't match its %s: %s",
			n, key, (char *)msg);
	} else {
		sway_log(SWAY_ERROR, "Criteria %s couldn't match its %s: %s",
			criteria->raw ? criteria->raw : "", key, (char *)msg);
	}
}
