/* See LICENSE file for copyright and license details. */
#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <getopt.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <linux/input-event-codes.h>
#include <stdlib.h>
#include <string.h>
#include <poll.h>
#include <sys/wait.h>
#include <unistd.h>
#include <wayland-client.h>

#include "drwl.h"
#include "poolbuf.h"
#include "river-status-unstable-v1-protocol.h"
#include "river-control-unstable-v1-protocol.h"
#include "wlr-layer-shell-unstable-v1-protocol.h"

#define LENGTH(X)               (sizeof X / sizeof X[0])
#define END(A)                  ((A) + LENGTH(A))
#define TEXTW(bar, text)        (drwl_font_getwidth(bar->drw, text) + bar->lrpad)

enum { ClkTagBar, ClkLayout, ClkMode, ClkTitle, ClkStatus }; /* clicks */

enum { SchemeNorm, SchemeSel };

typedef struct {
	uint32_t wl_name;
	struct wl_output *wl_output;
	struct wl_surface *surface;
	struct zwlr_layer_surface_v1 *layer_surface;
	struct zriver_output_status_v1 *river_output_status;
	Drwl *drw;
	uint32_t width, height, scale;
	int lrpad;

	uint32_t mtags, ctags, urg;
	char *layout, *title;

	bool selected;

	struct wl_list link;
} Bar;

typedef struct {
	uint32_t wl_name;
	struct wl_seat *wl_seat;
	struct wl_pointer *pointer;
	struct zriver_seat_status_v1 *river_seat_status;
	Bar *bar;
	char *mode;
	int pointer_x, pointer_y;
	uint32_t button;
	
	struct wl_list link;
} Seat;

typedef struct {
	unsigned int click;
	unsigned int button;
	void (*func)(uint32_t);
	uint32_t arg;
} Button;

#include "config.h"

static struct wl_display *display;
static struct wl_registry *registry;
static struct wl_shm *shm;
static struct wl_compositor *compositor;
static struct zwlr_layer_shell_v1 *layer_shell;
static struct zriver_status_manager_v1 *river_status_manager;
static struct zriver_control_v1 *river_control;

static struct wl_list seats, bars;
static char stext[256];

static void
noop()
{
	/*
	 * :3c
	 */
}

static void
die(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);

	if (fmt[0] && fmt[strlen(fmt)-1] == ':') {
		fputc(' ', stderr);
		perror(NULL);
	} else {
		fputc('\n', stderr);
	}

	exit(1);
}

static void
bar_load_fonts(Bar *bar)
{
	char fontattrs[12];

	drwl_destroy_font(bar->drw->font);
	snprintf(fontattrs, sizeof(fontattrs), "dpi=%d", 96 * bar->scale);
	if (!(drwl_load_font(bar->drw, LENGTH(fonts), fonts, fontattrs)))
		die("failed to load fonts");
	bar->lrpad = bar->drw->font->height;
	bar->height = bar->drw->font->height + 2;
}

static void
bar_draw(Bar *bar)
{
	int i;
	int tw = 0;
	int x = 0, w;
	int boxs = bar->drw->font->height / 9;
	int boxw = bar->drw->font->height / 6 + 2;
	PoolBuf *buf;
	Seat *seat;

	if (bar->width < 1 || bar->height < 1)
		return;

	if (!(buf = poolbuf_create(shm, bar->width, bar->height)))
		die("poolbuf_create:");
	drwl_prepare_drawing(bar->drw, bar->width, bar->height, buf->data, buf->stride);
	drwl_setscheme(bar->drw, colors[SchemeNorm]);

	/* draw status first so it can be overdrawn by tags later */
	if (bar->selected) { /* status is only drawn on selected monitor */
		drwl_setscheme(bar->drw, colors[SchemeNorm]);
		tw = TEXTW(bar, stext) - bar->lrpad + 2; /* 2px right padding */
		drwl_text(bar->drw, bar->width - tw, 0, tw, bar->height, 0, stext, 0);
	}

	for (i = 0; i < LENGTH(tags); i++) {
		w = TEXTW(bar, tags[i]);
		drwl_setscheme(bar->drw, colors[bar->mtags & 1 << i ? SchemeSel : SchemeNorm]);
		drwl_text(bar->drw, x, 0, w, bar->height, bar->lrpad / 2, tags[i], bar->urg & 1 << i);
		if (bar->ctags & 1 << i)
			drwl_rect(bar->drw, x + boxs, boxs, boxw, boxw, 0,
				bar->urg & 1 << i);
		x += w;
	}

	if (bar->layout) {
		w = TEXTW(bar, bar->layout);
		drwl_setscheme(bar->drw, colors[SchemeNorm]);
		x = drwl_text(bar->drw, x, 0, w, bar->height, bar->lrpad / 2, bar->layout, 0);
	}

	wl_list_for_each(seat, &seats, link) {
		if (seat->bar != bar)
			continue;
		w = TEXTW(bar, seat->mode);
		drwl_setscheme(bar->drw, colors[SchemeNorm]);
		x = drwl_text(bar->drw, x, 0, w, bar->height, bar->lrpad / 2, seat->mode, 1);
	}

	if ((w = bar->width - tw - x) > bar->height) {
		if (*bar->title != '\0') {
			drwl_setscheme(bar->drw, colors[bar->selected ? SchemeSel : SchemeNorm]);
			drwl_text(bar->drw, x, 0, w, bar->height, bar->lrpad / 2, bar->title, 0);
		} else {
			drwl_setscheme(bar->drw, colors[SchemeNorm]);
			drwl_rect(bar->drw, x, 0, w, bar->height, 1, 1);
		}
	}

	drwl_finish_drawing(bar->drw);
	wl_surface_set_buffer_scale(bar->surface, bar->scale);
	wl_surface_attach(bar->surface, buf->wl_buf, 0, 0);
	wl_surface_damage_buffer(bar->surface, 0, 0, bar->width, bar->height);
	poolbuf_destroy(buf);
	wl_surface_commit(bar->surface);
}

static void
bars_draw()
{
	Bar *bar;

	wl_list_for_each(bar, &bars, link)
		bar_draw(bar);
}

static void
bar_destroy(Bar *bar)
{
	wl_list_remove(&bar->link);
	free(bar->layout);
	free(bar->title);
	drwl_destroy(bar->drw);
	zriver_output_status_v1_destroy(bar->river_output_status);
	zwlr_layer_surface_v1_destroy(bar->layer_surface);
	wl_surface_destroy(bar->surface);
	wl_output_destroy(bar->wl_output);
}

static void
layer_surface_configure(void *data, struct zwlr_layer_surface_v1 *surface,
                        uint32_t serial, uint32_t width, uint32_t height)
{
	Bar *bar = data;
	
	bar->width = width * bar->scale;
	bar->height = height * bar->scale;
	zwlr_layer_surface_v1_ack_configure(bar->layer_surface, serial);
	bar_draw(bar);
}

static void
layer_surface_closed(void *data, struct zwlr_layer_surface_v1 *layer_surface)
{
	Bar *bar = data;
	bar_destroy(bar);
}

static const struct zwlr_layer_surface_v1_listener layer_surface_listener = {
    .configure = layer_surface_configure,
    .closed = layer_surface_closed,
};

static void 
surface_handle_preferred_scale(void *data,
	struct wl_surface *wl_surface, int32_t scale)
{
	Bar *bar = data;
	bar->scale = scale;
	bar_load_fonts(bar);
	zwlr_layer_surface_v1_set_size(bar->layer_surface, 0, bar->height / bar->scale);
}

static const struct wl_surface_listener surface_listener = {
	.enter = noop,
	.leave = noop,
	.preferred_buffer_scale = surface_handle_preferred_scale,
	.preferred_buffer_transform = noop,
};

static void
river_output_status_handle_focused_tags(void *data,
		struct zriver_output_status_v1 *output_status, uint32_t tags)
{
	Bar *bar = data;

	bar->mtags = tags;

	bar_draw(bar);
}

static void
river_output_status_handle_urgent_tags(void *data,
		struct zriver_output_status_v1 *output_status, uint32_t tags)
{
	Bar *bar = data;

	bar->urg = tags;

	bar_draw(bar);
}

static void
river_output_status_handle_view_tags(void *data,
		struct zriver_output_status_v1 *output_status, struct wl_array *wl_array)
{
	uint32_t *vt;
	Bar *bar = data;

	bar->ctags = 0;
	wl_array_for_each(vt, wl_array)
		bar->ctags |= *vt;

	bar_draw(bar);
}

static void
river_output_status_handle_layout_name(void *data,
		struct zriver_output_status_v1 *output_status, const char *name)
{
	Bar *bar = data;

	if (bar->layout)
		free(bar->layout);
	bar->layout = strdup(name);

	bar_draw(bar);
}

static void
river_output_status_handle_layout_name_clear(void *data,
		struct zriver_output_status_v1 *output_status)
{
	Bar *bar = data;

	if (bar->layout)
		free(bar->layout);

	bar_draw(bar);
}

static const struct zriver_output_status_v1_listener river_output_status_listener = {
	.focused_tags = river_output_status_handle_focused_tags,
	.urgent_tags = river_output_status_handle_urgent_tags,
	.view_tags = river_output_status_handle_view_tags,
	.layout_name = river_output_status_handle_layout_name,
	.layout_name_clear = river_output_status_handle_layout_name_clear
};


static void
river_seat_status_handle_focused_output(void *data,
		struct zriver_seat_status_v1 *seat_status, struct wl_output *wl_output)
{
	Bar *bar;
	Seat *seat = data;

	wl_list_for_each(bar, &bars, link) {
		if (bar->wl_output != wl_output)
			continue;

		seat->bar = bar;
		seat->bar->selected = true;
		bar_draw(bar);
		break;
	}
}

static void
river_seat_status_handle_unfocused_output(void *data, struct zriver_seat_status_v1 *seat_status,
				   struct wl_output *wl_output)
{
	Seat *seat = data;

	if (!seat->bar)
		return;

	seat->bar->selected = false;
	bar_draw(seat->bar);
	seat->bar = NULL;
}

static void
river_seat_status_handle_focused_view(void *data,
		struct zriver_seat_status_v1 *seat_status, const char *title)
{
	Seat *seat = data;

	if (!seat->bar)
		return;

	if (seat->bar->title)
		free(seat->bar->title);

	seat->bar->title = strdup(title);
	bar_draw(seat->bar);
}

static void
river_seat_status_handle_mode(void *data,
		struct zriver_seat_status_v1 *seat_status, const char *name)
{
	Seat *seat = data;

	if (seat->mode)
		free(seat->mode);

	seat->mode = strdup(name);
	bars_draw();
}

static const struct zriver_seat_status_v1_listener river_seat_status_listener = {
	.focused_output = river_seat_status_handle_focused_output,
	.unfocused_output = river_seat_status_handle_unfocused_output,
	.focused_view = river_seat_status_handle_focused_view,
	.mode = river_seat_status_handle_mode,
};

static void
pointer_motion(void *data, struct wl_pointer *pointer, uint32_t time,
	       wl_fixed_t surface_x, wl_fixed_t surface_y)
{
	Seat *seat = data;

	seat->pointer_x = wl_fixed_to_int(surface_x);
	seat->pointer_y = wl_fixed_to_int(surface_y);
}

static void
pointer_handle_frame(void *data, struct wl_pointer *pointer)
{
	int lw, mw = 0;
	unsigned int i = 0, /* j = 0, */ x = 0;
	unsigned int click;
	/* Seat *iter; */
	Seat *seat = data;
	char tagbuf[4];

	if (!seat->button || !seat->bar)
		return;

	lw = TEXTW(seat->bar, seat->bar->layout);

	do
		x += TEXTW(seat->bar, tags[i]);
	while (seat->pointer_x >= x && ++i < LENGTH(tags));

/*
	wl_list_for_each(iter, &seats, link) {
		mw += TEXTW(seat->bar, iter->mode);
		if (seat->pointer_x < x + mw)
			j++;
	}
*/
	
	if (i < LENGTH(tags))
		click = ClkTagBar;
	else if (seat->pointer_x > x + lw && seat->pointer_x < x + lw + mw)
		click = ClkMode;
	else if (seat->pointer_x < x + lw)
		click = ClkLayout;
	else if (seat->pointer_x > seat->bar->width - (int)TEXTW(seat->bar, stext))
		click = ClkStatus;
	else
		click = ClkTitle;

	switch (click) {
	case ClkTagBar:
		zriver_control_v1_add_argument(river_control, 
			seat->button == BTN_LEFT ? "set-focused-tags" :
			seat->button == BTN_MIDDLE ? "toggle-focused-tags" :
			"set-view-tags");
		snprintf(tagbuf, sizeof(tagbuf), "%d", 1 << i);
		zriver_control_v1_add_argument(river_control, tagbuf);
		zriver_control_v1_run_command(river_control, seat->wl_seat);
		break;
	case ClkLayout:
		/* TODO: cannot disable rivertile; change layout to 'floating' */
		break;
	case ClkMode:
		/* TODO: river has no method of getting a list of modes */
		break;
	case ClkTitle:
		zriver_control_v1_add_argument(river_control, "zoom");
		zriver_control_v1_run_command(river_control, seat->wl_seat);
		break;
	case ClkStatus:
		/* dwm spawns termcmd */
		break;
	}
}

static void
pointer_handle_button(void *data, struct wl_pointer *pointer, uint32_t serial,
		uint32_t time, uint32_t button, uint32_t state)
{
	Seat *seat = data;
	seat->button = state == WL_POINTER_BUTTON_STATE_PRESSED ? button : 0;
}

static const struct wl_pointer_listener pointer_listener = {
	.enter = noop,
	.leave = noop,
	.motion = pointer_motion,
	.axis = noop,
	.frame = pointer_handle_frame,
	.axis_discrete = noop,
	.axis_source = noop,
	.axis_stop = noop,
	.button = pointer_handle_button,
};

static void
seat_handle_capabilities(void *data, struct wl_seat *wl_seat,
		enum wl_seat_capability caps)
{
	Seat *seat = data;

	if (!(caps & WL_SEAT_CAPABILITY_POINTER))
		return;

	seat->pointer = wl_seat_get_pointer(seat->wl_seat);
	wl_pointer_add_listener(seat->pointer, &pointer_listener, seat);
}


static void
seat_destroy(Seat *seat)
{
	wl_list_remove(&seat->link);
	zriver_seat_status_v1_destroy(seat->river_seat_status);
	wl_pointer_destroy(seat->pointer);
	wl_seat_destroy(seat->wl_seat);
}

static const struct wl_seat_listener seat_listener = {
	.capabilities = seat_handle_capabilities,
	.name = noop,
};

static void
registry_handle_global(void *data, struct wl_registry *wl_registry,
		uint32_t name, const char *interface, uint32_t version)
{
	if (!strcmp(interface, wl_compositor_interface.name))
		compositor = wl_registry_bind(registry, name, &wl_compositor_interface, 4);
	else if (!strcmp(interface, wl_shm_interface.name))
		shm = wl_registry_bind(registry, name, &wl_shm_interface, 1);
	else if (!strcmp(interface, zwlr_layer_shell_v1_interface.name))
		layer_shell = wl_registry_bind(wl_registry, name, &zwlr_layer_shell_v1_interface, 2);
	else if (!strcmp(interface, wl_seat_interface.name)) {
		Seat *seat = calloc(1, sizeof(Seat));
		seat->wl_name = name;
		seat->wl_seat = wl_registry_bind(registry, name, &wl_seat_interface, 5);
		wl_seat_add_listener(seat->wl_seat, &seat_listener, seat);
		wl_list_insert(&seats, &seat->link);
	} else if (!strcmp(interface, zriver_control_v1_interface.name))
		river_control = wl_registry_bind(registry, name, &zriver_control_v1_interface, 1);
	else if (!strcmp(interface, zriver_status_manager_v1_interface.name))
		river_status_manager = wl_registry_bind(registry, name, &zriver_status_manager_v1_interface, 4);
	else if (!strcmp(interface, wl_output_interface.name)) {
		Bar *bar = calloc(1, sizeof(Bar));
		bar->scale = 1;
		bar->wl_name = name;
		bar->wl_output = wl_registry_bind(registry, name, &wl_output_interface, 2);
		wl_list_insert(&bars, &bar->link);
	}
}

static void
registry_handle_global_remove(void *data, struct wl_registry *registry, uint32_t name)
{
	Seat *seat;
	Bar *bar;

	wl_list_for_each(seat, &seats, link) {
		if (seat->wl_name == name) {
			seat_destroy(seat);
			return;
		}
	}
	wl_list_for_each(bar, &bars, link) {
		if (bar->wl_name == name) {
			bar_destroy(bar);
			return;
		}
	}
}

static const struct wl_registry_listener registry_listener = {
	.global = registry_handle_global,
	.global_remove = registry_handle_global_remove,
};

static void
readstdin(void)
{
	ssize_t n;

	n = read(STDIN_FILENO, stext, sizeof(stext) - 1);
	if (n < 0)
		die("read:");

	stext[n] = '\0';
	stext[strcspn(stext, "\n")] = '\0';
	bars_draw();
}

static void
setup(void)
{
	Seat *seat;
	Bar *bar;

	if (!(display = wl_display_connect(NULL)))
		die("failed to connect to wayland");

	wl_list_init(&bars);
	wl_list_init(&seats);

	registry = wl_display_get_registry(display);
	wl_registry_add_listener(registry, &registry_listener, NULL);
	wl_display_roundtrip(display);

	if (!compositor || !shm || !layer_shell || !river_status_manager || !river_control)
		die("unsupported compositor");

	drwl_init();

	wl_list_for_each(seat, &seats, link) {
		seat->river_seat_status = zriver_status_manager_v1_get_river_seat_status(
			river_status_manager, seat->wl_seat);
		zriver_seat_status_v1_add_listener(seat->river_seat_status,
			&river_seat_status_listener, seat);
	}
	
	wl_list_for_each(bar, &bars, link) {
		if (!(bar->drw = drwl_create()))
			die("failed to create drwl context");
		bar_load_fonts(bar);

		bar->river_output_status = zriver_status_manager_v1_get_river_output_status(
			river_status_manager, bar->wl_output);
		zriver_output_status_v1_add_listener(bar->river_output_status,
			&river_output_status_listener, bar);

		bar->surface = wl_compositor_create_surface(compositor);
		wl_surface_add_listener(bar->surface, &surface_listener, NULL);
		
		bar->layer_surface = zwlr_layer_shell_v1_get_layer_surface(
			layer_shell, bar->surface, bar->wl_output, ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM, "bar");
		zwlr_layer_surface_v1_add_listener(
			bar->layer_surface, &layer_surface_listener, bar);
		zwlr_layer_surface_v1_set_size(bar->layer_surface, 0, bar->height);
	    zwlr_layer_surface_v1_set_exclusive_zone(bar->layer_surface, bar->height);
		zwlr_layer_surface_v1_set_anchor(bar->layer_surface,
			ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP | ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT |
			ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT);
		wl_surface_commit(bar->surface);
	}
}

static void
run(void)
{
	struct pollfd pfds[] = {
		{ .fd = wl_display_get_fd(display), .events = POLLIN },
		{ .fd = STDIN_FILENO,               .events = POLLIN },
	};

	for (;;) {
		if (wl_display_prepare_read(display) < 0)
			if (wl_display_dispatch_pending(display) < 0)
				die("wl_display_dispatch_pending:");

		if (wl_display_flush(display) < 0)
			die("wl_display_flush:");

		if (poll(pfds, 2, -1) < 0) {
			wl_display_cancel_read(display);
			die("poll:");
		}

		if (pfds[1].revents & POLLHUP) {
			pfds[1].fd = -1;
			memset(stext, 0, LENGTH(stext));
			bars_draw();
		}
		if (pfds[1].revents & POLLIN)
			readstdin();

		if (!(pfds[0].revents & POLLIN)) {
			wl_display_cancel_read(display);
			continue;
		}

		if (wl_display_read_events(display) < 0)
			die("wl_display_read_events:");
		if (wl_display_dispatch_pending(display) < 0)
			die("wl_display_dispatch_pending");
	}
}

static void
cleanup(void)
{
	Seat *seat;
	Bar *bar;

	wl_list_for_each(seat, &seats, link)
		seat_destroy(seat);
	wl_list_for_each(bar, &bars, link)
		bar_destroy(bar);

	drwl_fini();
	zriver_control_v1_destroy(river_control);
	zriver_status_manager_v1_destroy(river_status_manager);
	zwlr_layer_shell_v1_destroy(layer_shell);
	wl_compositor_destroy(compositor);
	wl_shm_destroy(shm);
	wl_registry_destroy(registry);
	wl_display_disconnect(display);
}

int
main(int argc, char *argv[])
{
	setup();
	run();
	cleanup();
	return EXIT_SUCCESS;
}
