#ifndef _SWAY_TIDE_OUTPUTCHECK_H
#define _SWAY_TIDE_OUTPUTCHECK_H

/*
 * An output line checked as tidewm checks it (its settings.h), failing in
 * its words: a name, then settings, each of tidewm's taking a value it can
 * take. Once a setting only sway has comes, the rest are sway's to check.
 */

struct cmd_results;

/* Why the output command argv can't be, or NULL; the caller frees it */
struct cmd_results *tide_output_check(int argc, char **argv);

#endif
