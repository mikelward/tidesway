#ifndef _SWAY_TIDE_RULES_H
#define _SWAY_TIDE_RULES_H

struct cmd_results;
struct criteria;

/* A for_window line's criteria and commands checked as tidewm checks them,
 * in its words, with sway's criteria's names: NULL if they're good. */
struct cmd_results *tide_rule_check(int argc, char **argv);

// PCRE2 couldn't finish matching criteria's key, with code: it's logged
void tide_rule_match_failed(const struct criteria *criteria, const char *key,
	int code);

#endif
