#ifndef _SWAY_TIDE_INPUTCHECK_H
#define _SWAY_TIDE_INPUTCHECK_H
#include <stdbool.h>
#include <stddef.h>

/*
 * An input line checked as tidewm checks it, and failing in its words:
 * its name, then each setting tidewm knows taking one value it can take.
 * Settings only sway has are sway's to check.
 */

/* What's wrong with name, an input line's or block's, in err, or false
 * if nothing is */
bool tide_input_bad_name(const char *name, char *err, size_t size);

/* What's wrong with the name an input block's prefix, what follows its
 * "input ", starts with, in err, or false if nothing is: a nested block
 * adds a setting after it */
bool tide_input_bad_block(const char *prefix, char *err, size_t size);

/* Why the input command argv, whose settings is_setting knows, can't
 * be, or NULL; the caller frees it */
struct cmd_results *tide_input_check(int argc, char **argv,
	bool (*is_setting)(const char *setting));

/* The input command's failure when no device has name, or NULL when one
 * does; the caller frees it */
struct cmd_results *tide_input_unmatched(const char *name);

#endif
