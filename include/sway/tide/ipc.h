#ifndef _SWAY_TIDE_IPC_H
#define _SWAY_TIDE_IPC_H

/*
 * tidesway's IPC socket, as tidewm's (its docs/ipc.md): named
 * $XDG_RUNTIME_DIR/tidewm-ipc.UID.PID.sock, so what looks for tidewm's
 * socket finds it, and named in SWAYSOCK and I3SOCK whatever they said
 * before. One a tidesway or tidewm of this user's left by dying is removed
 * as another starts, so tidewm-ipc.*.sock matches only running ones.
 */

// The start of the socket's name, before "UID.PID.sock"
#define TIDE_IPC_PREFIX "tidewm-ipc."

/* Removes, from the directory path is in, each socket named as this
 * user's would be for a process that's gone, saying so; path is this
 * process's own, which stays */
void tide_ipc_remove_stale(const char *path);

#endif
