/*
 * tidewm's gesture command (its docs/ipc.md): `gesture swipe|pinch|hold
 * FINGERS` makes a touchpad gesture with FINGERS fingers, as if from a
 * touchpad, which goes to the window under the pointer as a real one does,
 * or to a gesture binding: a swipe 10 to the right, a pinch 10 to the
 * right that zooms to twice the size and turns 90 degrees, or a hold. For
 * a test, as the headless backend has no touchpad.
 */
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <wlr/types/wlr_pointer.h>
#include "sway/commands.h"
#include "sway/input/input-manager.h"
#include "sway/input/seat.h"

static uint32_t now_msec(void) {
	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);
	return now.tv_sec * 1000 + now.tv_nsec / 1000000;
}

struct cmd_results *cmd_gesture(int argc, char **argv) {
	const char *usage = "gesture takes swipe, pinch or hold, then a number of fingers";
	if (argc != 2) {
		return cmd_results_new(CMD_INVALID, "%s", usage);
	}
	char *end;
	errno = 0;
	long fingers = strtol(argv[1], &end, 10);
	if (errno || *end || fingers < 1 || fingers > 10) {
		return cmd_results_new(CMD_INVALID, "%s", usage);
	}
	struct sway_seat *seat = input_manager_current_seat();
	uint32_t time = now_msec();
	if (strcmp(argv[0], "swipe") == 0) {
		struct wlr_pointer_swipe_begin_event begin = {
			.time_msec = time, .fingers = fingers };
		seatop_swipe_begin(seat, &begin);
		struct wlr_pointer_swipe_update_event update = {
			.time_msec = time, .fingers = fingers, .dx = 10 };
		seatop_swipe_update(seat, &update);
		struct wlr_pointer_swipe_end_event done = { .time_msec = time };
		seatop_swipe_end(seat, &done);
	} else if (strcmp(argv[0], "pinch") == 0) {
		struct wlr_pointer_pinch_begin_event begin = {
			.time_msec = time, .fingers = fingers };
		seatop_pinch_begin(seat, &begin);
		struct wlr_pointer_pinch_update_event update = {
			.time_msec = time, .fingers = fingers, .dx = 10, .scale = 2,
			.rotation = 90 };
		seatop_pinch_update(seat, &update);
		struct wlr_pointer_pinch_end_event done = { .time_msec = time };
		seatop_pinch_end(seat, &done);
	} else if (strcmp(argv[0], "hold") == 0) {
		struct wlr_pointer_hold_begin_event begin = {
			.time_msec = time, .fingers = fingers };
		seatop_hold_begin(seat, &begin);
		struct wlr_pointer_hold_end_event done = { .time_msec = time };
		seatop_hold_end(seat, &done);
	} else {
		return cmd_results_new(CMD_INVALID, "%s", usage);
	}
	return cmd_results_new(CMD_SUCCESS, NULL);
}
