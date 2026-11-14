/*
 * A test window with a subsurface, which tidewm's test client has none of:
 * one xdg-shell toplevel filled blue, its app ID and title argv[1], with a
 * desynchronized subsurface over its top four rows, green. With a second
 * argument, `slow`, it's an app that takes a while to draw: it waits 80 ms
 * before each drawing but its first, as a real one's layout would take.
 * Lines on stdin drive it:
 *
 * commit   draws the window again, as it is, and prints "committed" once
 *          the compositor has had the commit, as tidewm's client does.
 * recolor  draws the subsurface red, committing it alone, and prints
 *          "frame" when the compositor says that frame was shown.
 * hide     unmaps the subsurface, and prints "hidden" once the compositor
 *          has had the commit.
 * move     moves the subsurface to the window's bottom four rows, with a
 *          commit of the window alone, and prints "moved" likewise.
 * hole     takes all of the window's input region and the subsurface's
 *          away, and prints "holed" likewise.
 * fade     halves the window's opacity, over alpha-modifier-v1, with no
 *          new buffer or damage, and prints "faded" likewise.
 */
#define _GNU_SOURCE
#include <errno.h>
#include <poll.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>
#include <wayland-client.h>
#include "alpha-modifier-v1-client-protocol.h"
#include "xdg-shell-client-protocol.h"

#define ROWS 4
#define BLUE 0xff336699
#define GREEN 0xff00ff00
#define RED 0xffff0000

static struct wl_display *display;
static struct wl_compositor *compositor;
static struct wl_subcompositor *subcompositor;
static struct wl_shm *shm;
static struct xdg_wm_base *wm_base;
static struct wp_alpha_modifier_v1 *alpha_modifier;
static struct wl_surface *surface, *sub_surface;
static struct wl_subsurface *sub;
static int width = 100, height = 100;
static uint32_t sub_color = GREEN;
static bool closed;
static bool slow, drawn;

static void die(const char *why) {
	fprintf(stderr, "subsurface: %s\n", why);
	exit(1);
}

// A buffer of width by height pixels of color, which the compositor frees
static struct wl_buffer *filled(int w, int h, uint32_t color) {
	size_t size = (size_t)w * h * 4;
	int fd = memfd_create("subsurface", MFD_CLOEXEC);
	if (fd < 0 || ftruncate(fd, size) < 0) {
		die("can't make a buffer's memory");
	}
	uint32_t *pixels = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (pixels == MAP_FAILED) {
		die("can't map a buffer's memory");
	}
	for (size_t i = 0; i < (size_t)w * h; i++) {
		pixels[i] = color;
	}
	munmap(pixels, size);
	struct wl_shm_pool *pool = wl_shm_create_pool(shm, fd, size);
	struct wl_buffer *buffer =
		wl_shm_pool_create_buffer(pool, 0, w, h, w * 4, WL_SHM_FORMAT_XRGB8888);
	wl_shm_pool_destroy(pool);
	close(fd);
	return buffer;
}

static void draw(struct wl_surface *s, int w, int h, uint32_t color) {
	struct wl_buffer *buffer = filled(w, h, color);
	wl_surface_attach(s, buffer, 0, 0);
	wl_surface_damage_buffer(s, 0, 0, w, h);
	wl_surface_commit(s);
	// The compositor keeps what it needs of it
	wl_buffer_destroy(buffer);
}

// The subsurface first, so the window's commit shows both at their size
static void draw_all(void) {
	draw(sub_surface, width, ROWS, sub_color);
	draw(surface, width, height, BLUE);
}

static void said_done(void *data, struct wl_callback *callback, uint32_t time) {
	wl_callback_destroy(callback);
	printf("%s\n", (const char *)data);
	fflush(stdout);
}

static const struct wl_callback_listener said_listener = { .done = said_done };

static void toplevel_configure(void *data, struct xdg_toplevel *toplevel,
		int32_t w, int32_t h, struct wl_array *states) {
	if (w > 0 && h > 0) {
		width = w;
		height = h;
	}
}

static void toplevel_close(void *data, struct xdg_toplevel *toplevel) {
	closed = true;
}

static void toplevel_bounds(void *data, struct xdg_toplevel *toplevel, int32_t w, int32_t h) {
}

static void toplevel_capabilities(void *data, struct xdg_toplevel *toplevel,
		struct wl_array *capabilities) {
}

static const struct xdg_toplevel_listener toplevel_listener = {
	.configure = toplevel_configure,
	.close = toplevel_close,
	.configure_bounds = toplevel_bounds,
	.wm_capabilities = toplevel_capabilities,
};

static void xdg_surface_configure(void *data, struct xdg_surface *xdg, uint32_t serial) {
	xdg_surface_ack_configure(xdg, serial);
	if (slow && drawn) {
		// Not a wait for anything: it's how long this app takes to draw
		struct timespec work = { .tv_nsec = 80 * 1000 * 1000 };
		nanosleep(&work, NULL);
	}
	drawn = true;
	draw_all();
}

static const struct xdg_surface_listener xdg_surface_listener = {
	.configure = xdg_surface_configure,
};

static void wm_base_ping(void *data, struct xdg_wm_base *base, uint32_t serial) {
	xdg_wm_base_pong(base, serial);
}

static const struct xdg_wm_base_listener wm_base_listener = { .ping = wm_base_ping };

static void global(void *data, struct wl_registry *registry, uint32_t name,
		const char *interface, uint32_t version) {
	if (strcmp(interface, wl_compositor_interface.name) == 0) {
		compositor = wl_registry_bind(registry, name, &wl_compositor_interface, 4);
	} else if (strcmp(interface, wl_subcompositor_interface.name) == 0) {
		subcompositor = wl_registry_bind(registry, name, &wl_subcompositor_interface, 1);
	} else if (strcmp(interface, wl_shm_interface.name) == 0) {
		shm = wl_registry_bind(registry, name, &wl_shm_interface, 1);
	} else if (strcmp(interface, xdg_wm_base_interface.name) == 0) {
		wm_base = wl_registry_bind(registry, name, &xdg_wm_base_interface, 4);
		xdg_wm_base_add_listener(wm_base, &wm_base_listener, NULL);
	} else if (strcmp(interface, wp_alpha_modifier_v1_interface.name) == 0) {
		alpha_modifier = wl_registry_bind(registry, name, &wp_alpha_modifier_v1_interface, 1);
	}
}

static void global_remove(void *data, struct wl_registry *registry, uint32_t name) {
}

static const struct wl_registry_listener registry_listener = {
	.global = global,
	.global_remove = global_remove,
};

static void command(const char *line) {
	if (strcmp(line, "commit") == 0) {
		draw_all();
		wl_callback_add_listener(wl_display_sync(display), &said_listener, "committed");
	} else if (strcmp(line, "recolor") == 0) {
		sub_color = RED;
		wl_callback_add_listener(wl_surface_frame(sub_surface), &said_listener, "frame");
		draw(sub_surface, width, ROWS, sub_color);
	} else if (strcmp(line, "hide") == 0) {
		wl_surface_attach(sub_surface, NULL, 0, 0);
		wl_surface_commit(sub_surface);
		wl_callback_add_listener(wl_display_sync(display), &said_listener, "hidden");
	} else if (strcmp(line, "move") == 0) {
		// Applied by the window's commit, with none of the subsurface's
		wl_subsurface_set_position(sub, 0, height - ROWS);
		wl_surface_commit(surface);
		wl_callback_add_listener(wl_display_sync(display), &said_listener, "moved");
	} else if (strcmp(line, "hole") == 0) {
		struct wl_region *none = wl_compositor_create_region(compositor);
		wl_surface_set_input_region(sub_surface, none);
		wl_surface_commit(sub_surface);
		wl_surface_set_input_region(surface, none);
		wl_surface_commit(surface);
		wl_region_destroy(none);
		wl_callback_add_listener(wl_display_sync(display), &said_listener, "holed");
	} else if (strcmp(line, "fade") == 0) {
		if (!alpha_modifier) {
			die("the compositor lacks wp_alpha_modifier_v1");
		}
		struct wp_alpha_modifier_surface_v1 *alpha =
			wp_alpha_modifier_v1_get_surface(alpha_modifier, surface);
		wp_alpha_modifier_surface_v1_set_multiplier(alpha, UINT32_MAX / 2);
		wl_surface_commit(surface);
		wl_callback_add_listener(wl_display_sync(display), &said_listener, "faded");
	} else {
		die("unknown command");
	}
}

int main(int argc, char *argv[]) {
	if (argc == 3 && strcmp(argv[2], "slow") == 0) {
		slow = true;
	} else if (argc != 2) {
		die("usage: subsurface APP_ID [slow]");
	}
	display = wl_display_connect(NULL);
	if (!display) {
		die("can't connect to the compositor");
	}
	struct wl_registry *registry = wl_display_get_registry(display);
	wl_registry_add_listener(registry, &registry_listener, NULL);
	wl_display_roundtrip(display);
	if (!compositor || !subcompositor || !shm || !wm_base) {
		die("the compositor lacks wl_compositor, wl_subcompositor, wl_shm or xdg_wm_base");
	}

	surface = wl_compositor_create_surface(compositor);
	sub_surface = wl_compositor_create_surface(compositor);
	sub = wl_subcompositor_get_subsurface(subcompositor, sub_surface, surface);
	wl_subsurface_set_position(sub, 0, 0);
	wl_subsurface_set_desync(sub);
	struct xdg_surface *xdg = xdg_wm_base_get_xdg_surface(wm_base, surface);
	xdg_surface_add_listener(xdg, &xdg_surface_listener, NULL);
	struct xdg_toplevel *toplevel = xdg_surface_get_toplevel(xdg);
	xdg_toplevel_add_listener(toplevel, &toplevel_listener, NULL);
	xdg_toplevel_set_app_id(toplevel, argv[1]);
	xdg_toplevel_set_title(toplevel, argv[1]);
	wl_surface_commit(surface);

	char line[64];
	size_t have = 0;
	struct pollfd fds[2] = {
		{ .fd = wl_display_get_fd(display), .events = POLLIN },
		{ .fd = STDIN_FILENO, .events = POLLIN },
	};
	while (!closed) {
		if (wl_display_flush(display) < 0 && errno != EAGAIN) {
			break;
		}
		if (poll(fds, 2, -1) < 0) {
			if (errno == EINTR) {
				continue;
			}
			die("poll failed");
		}
		if (fds[0].revents && wl_display_dispatch(display) < 0) {
			break;
		}
		if (fds[1].revents & (POLLIN | POLLHUP)) {
			ssize_t n = read(STDIN_FILENO, line + have, sizeof(line) - 1 - have);
			if (n <= 0) {
				fds[1].fd = -1; // stdin closed: run until the window is
				continue;
			}
			have += (size_t)n;
			line[have] = '\0';
			char *end;
			while ((end = strchr(line, '\n'))) {
				*end = '\0';
				command(line);
				have -= (size_t)(end + 1 - line);
				memmove(line, end + 1, have + 1);
			}
			if (have == sizeof(line) - 1) {
				die("a line too long");
			}
		}
	}
	return 0;
}
