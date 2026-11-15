/*
 * The IPC socket's leftovers, as tidewm's ipc.h removes them.
 * include/sway/tide/ipc.h says what's removed.
 */
#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "log.h"
#include "sway/tide/ipc.h"

void tide_ipc_remove_stale(const char *path) {
	char dir[PATH_MAX];
	snprintf(dir, sizeof(dir), "%s", path);
	char *slash = strrchr(dir, '/');
	if (!slash) {
		return;
	}
	*(slash == dir ? slash + 1 : slash) = '\0';
	DIR *d = opendir(dir);
	if (!d) {
		sway_log_errno(SWAY_ERROR, "tide: can't look in %s for old sockets", dir);
		return;
	}
	struct dirent *entry;
	while ((entry = readdir(d))) {
		unsigned int uid;
		int pid;
		if (sscanf(entry->d_name, TIDE_IPC_PREFIX "%u.%d.sock", &uid, &pid) != 2
				|| uid != (unsigned int)getuid() || pid <= 0 || pid == (int)getpid()) {
			continue;
		}
		// The whole name, and no other way of writing the numbers
		char name[64];
		snprintf(name, sizeof(name), TIDE_IPC_PREFIX "%u.%d.sock", uid, pid);
		if (strcmp(name, entry->d_name) != 0) {
			continue;
		}
		// Gone only if there's no such process: another user's is left
		if (kill(pid, 0) == 0 || errno != ESRCH) {
			continue;
		}
		char file[PATH_MAX];
		struct stat st;
		int n = snprintf(file, sizeof(file), "%s/%s", dir, name);
		if (n < 0 || (size_t)n >= sizeof(file) || lstat(file, &st) < 0
				|| !S_ISSOCK(st.st_mode)) {
			continue;
		}
		if (unlink(file) < 0) {
			sway_log_errno(SWAY_ERROR, "tide: can't remove the old socket %s", file);
		} else {
			// An error's level, so it's in the log sway keeps by default
			sway_log(SWAY_ERROR, "tide: removed %s, left by a tidewm or tidesway that's gone",
				file);
		}
	}
	closedir(d);
}
