/*
 * A command list as tidewm reads one (include/sway/tide/cmdline.h).
 */
#include <ctype.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "log.h"
#include "stringop.h"
#include "sway/tide/cmdline.h"
#include "sway/tide/configinfo.h"

size_t tide_cmd_exec_name(const char *cmd) {
	static const char *const names[] = { "exec_always", "exec" };
	for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); i++) {
		size_t n = strlen(names[i]);
		if (strncasecmp(cmd, names[i], n) == 0
				&& (!cmd[n] || isspace((unsigned char)cmd[n]))) {
			return n;
		}
	}
	return 0;
}

char *tide_cmd_next(char **stringp, char *matched) {
	char *start = *stringp;
	bool quoted = false, escaped = false;
	for (char *p = start; *p; p++) {
		if (escaped) {
			escaped = false;
		} else if (*p == '\\') {
			escaped = true;
		} else if (*p == '"') {
			quoted = !quoted;
		} else if (!quoted && (*p == ';' || *p == ',')) {
			if (matched) {
				*matched = *p;
			}
			*p = '\0';
			*stringp = p + 1;
			return start;
		}
	}
	if (matched) {
		*matched = '\0';
	}
	*stringp = NULL;
	return start;
}

char **tide_exec_args(const char *cmd, size_t name, int *argc) {
	static const char flag[] = "--no-startup-id";
	char **argv = calloc(4, sizeof(char *));
	if (!argv) {
		sway_log(SWAY_ERROR, "tide: can't allocate exec's arguments");
		abort();
	}
	*argc = 0;
	argv[(*argc)++] = strdup(name == strlen("exec") ? "exec" : "exec_always");
	const char *rest = cmd + name;
	rest += strspn(rest, " \t\n");
	size_t n = strlen(flag);
	if (strncmp(rest, flag, n) == 0 && (!rest[n] || isspace((unsigned char)rest[n]))) {
		argv[(*argc)++] = strdup(flag);
		rest += n;
		rest += strspn(rest, " \t\n");
	}
	if (*rest) {
		argv[(*argc)++] = strdup(rest);
	}
	for (int i = 0; i < *argc; i++) {
		if (!argv[i]) {
			sway_log(SWAY_ERROR, "tide: can't allocate exec's arguments");
			abort();
		}
	}
	return argv;
}

bool tide_exec_one_quoted(const char *arg) {
	/* Space after the closing quote, which an IPC exec's rest keeps, counts
	 * for nothing, as it did when sway split the rest into words; an
	 * escaped one stays, as a config line's does */
	char *word = strdup(arg);
	if (!word) {
		sway_log(SWAY_ERROR, "tide: can't allocate exec's command");
		return false;
	}
	tide_config_strip(word);
	size_t len = strlen(word);
	if (len < 2 || (word[0] != '"' && word[0] != '\'') || word[len - 1] != word[0]) {
		free(word);
		return false;
	}
	int argc;
	char **argv = split_args(word, &argc);
	free_argv(argc, argv);
	free(word);
	return argc == 1;
}

bool tide_cmd_closes_block(const char *line) {
	return line[0] == '}' && (!line[1] || isspace((unsigned char)line[1]));
}
