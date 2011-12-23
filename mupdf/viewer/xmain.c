#include "aekit.h"

#include <xcb/xcb.h>
#include <xcb/xcb_icccm.h> /* for wm-hints */
#include <xcb/xcb_image.h> /* for pixmap-from-bitmap-data */

#include "ximage.h"

#define GENBASE 1000
#define GENID (GENBASE + __LINE__)

#ifndef XCB_ATOM_STRING
#define XCB_ATOM_STRING 31
#endif

#define DEFW 612 // 595 /* 612 */
#define DEFH 792 // 842 /* 792 */

void ui_main_loop(void);

char *title = "aekit";

void die(char *msg)
{
	fprintf(stderr, "error: %s\n", msg);
	exit(1);
}

static const char *xcb_error_name_list[] = {
	"None", "Request", "Value", "Window", "Pixmap", "Atom",
	"Cursor", "Font", "Match", "Drawable", "Access", "Alloc",
	"Colormap", "GContext", "IDChoice", "Name", "Length",
	"Implementation",
};

const char *xcb_get_error_name(int detail)
{
	if (detail >= 0 && detail < nelem(xcb_error_name_list))
		return xcb_error_name_list[detail];
	return "Unknown";
}

static xcb_screen_t *
find_screen_by_number(const xcb_setup_t *setup, int screen)
{
	xcb_screen_iterator_t i = xcb_setup_roots_iterator(setup);
	for (; i.rem; --screen, xcb_screen_next(&i))
		if (screen == 0)
			return i.data;
	return 0;
}

struct unifont *font;
struct image dstk;
struct image *dst = &dstk;
struct ui ui = { 0 };

struct color btn_brown = { 206, 193, 180 };
struct color btn_brown_hi = { 218, 205, 192 };
struct color btn_brown_lo = { 173, 160, 147 };
struct color border = { 80, 80, 80 };
struct color gray = { 153, 153, 153 };
struct color black = { 0, 0, 0 };

#define icon_width 16
#define icon_height 16
static unsigned char icon_bits[] = {
	0x00, 0x00, 0x00, 0x1e, 0x00, 0x2b, 0x80, 0x55, 0x8c, 0x62, 0x8c, 0x51,
	0x9c, 0x61, 0x1c, 0x35, 0x3c, 0x1f, 0x3c, 0x0f, 0xfc, 0x0f, 0xec, 0x0d,
	0xec, 0x0d, 0xcc, 0x0c, 0xcc, 0x0c, 0x00, 0x00 };

#define icon_mask_width 16
#define icon_mask_height 16
static unsigned char icon_mask_bits[] = {
	0x00, 0x1e, 0x00, 0x3f, 0x80, 0x7f, 0xce, 0xff, 0xde, 0xff, 0xde, 0xff,
	0xfe, 0xff, 0xfe, 0x7f, 0xfe, 0x3f, 0xfe, 0x1f, 0xfe, 0x1f, 0xfe, 0x1f,
	0xfe, 0x1f, 0xfe, 0x1f, 0xfe, 0x1f, 0xce, 0x1c };

struct ximage *surface;
xcb_connection_t *conn;
xcb_screen_t *screen;
xcb_window_t window;
xcb_gcontext_t gc;
int dirty;

void
on_resize(int w, int h)
{
	if (dst->samples && dst->w == w && dst->h == h)
		return;

	free(dst->samples);

	dst->w = w;
	dst->h = h;
	dst->stride = dst->w * 4;
	dst->samples = malloc(dst->h * dst->stride);

	memset(dst->samples, 192, dst->h * dst->stride);

	ui.key = 0;
	ui_main_loop();

	dirty = 1;
}

void
ui_start_frame(void)
{
	ui.hot = 0;
}

void
ui_end_frame(void)
{
	if (!(ui.down & 1))
		ui.active = 0;
	else if (!ui.active)
		ui.active = -1; /* click outside widgets, mark as unavailable */
	dirty = 1;
}

int
ui_region_hit(int x, int y, int w, int h)
{
	return ui.x >= x && ui.y >= y && ui.x < x + w && ui.y < y + h;
}

int
ui_button(int id, int x, int y, char *label)
{
	int w = ui_measure_unifont_string(font, label) + 12;
	int h = 25;
	int b = 2;

	if (ui_region_hit(x, y, w, h)) {
		ui.hot = id;
		if (!ui.active && (ui.down & 1))
			ui.active = id;
	}

	int x0 = x;
	int y0 = y;
	int x1 = x + w - 1;
	int y1 = y + h - 1;

	ui_fill_rect(dst, x0, y0, x1+1, y0+1, border);
	ui_fill_rect(dst, x0, y1, x1+1, y1+1, border);
	ui_fill_rect(dst, x0, y0, x0+1, y1, border);
	ui_fill_rect(dst, x1, y0, x1+1, y1, border);

	x0++; y0++; x1--; y1--;
	ui_fill_rect(dst, x0, y0, x1+1, y0+1, btn_brown_hi);
	ui_fill_rect(dst, x0, y1, x1+1, y1+1, btn_brown_lo);
	ui_fill_rect(dst, x0, y0, x0+1, y1, btn_brown_hi);
	ui_fill_rect(dst, x1, y0, x1+1, y1, btn_brown_lo);

	if (ui.active == id && (ui.down & 1)) {
		if (ui.hot == id)
			ui_fill_rect(dst, x+b-1, y+b-1, x+w-b+1, y+h-b+1, btn_brown_lo);
		else
			ui_fill_rect(dst, x+b, y+b, x+w-b, y+h-b, btn_brown);
	}
	else
		ui_fill_rect(dst, x+b, y+b, x+w-b, y+h-b, btn_brown);

	ui_draw_unifont_string(dst, font, label, x+b+4, y+b+2, black);

	return !(ui.down & 1) && ui.hot == id && ui.active == id;
}

void
ui_main_loop(void)
{
	ui_start_frame();

	if (ui_button(GENID, 30, 30, "☑ Hello, world!")) puts("click 1");
	if (ui_button(GENID, 30, 60, "☐ 你好")) puts("click 2");
	if (ui_button(GENID, 30, 90, "Γεια σου")) puts("click 3");
	if (ui_button(GENID, 30, 120, "My ♚ will eat your ♖!")) puts("click 4");
	if (ui_button(GENID, 30, 150, "おはよう")) puts("click 5");
	if (ui_button(GENID, 30, 180, "안녕하세요")) puts("click 6");
	if (ui_button(GENID, 30, 210, "☏ xin chào")) puts("click 7");
	if (ui_button(GENID, 30, 240, "[ ☢ ☣ ☥ ☭ ☯ ]")) puts("click 7");
	if (ui_button(GENID, 30, 270, "(♃♄♅♆♇♈♉♊♋♌♍ ...)")) puts("click 7");

	ui_end_frame();
}

int
main(int argc, char **argv)
{
	const xcb_setup_t *setup;
	xcb_generic_event_t *event;
	int screen_num;
	xcb_wm_hints_t hints;
	xcb_pixmap_t icon, icon_mask;
	uint32_t mask;
	uint32_t attrs[2];
	int w, h;
	int button;

	font = ui_load_unifont("unifont.dat");
	if (!font)
		die("cannot load 'unifont.dat'");

	/* Connect and find the screen */

	conn = xcb_connect(NULL, &screen_num);
	if (!conn)
		die("cannot connect to display");

	if (xcb_connection_has_error(conn))
		die("cannot connect to display");

	setup = xcb_get_setup(conn);
	if (!setup)
		die("cannot get display setup");

	screen = find_screen_by_number(setup, screen_num);
	if (!screen)
		die("cannot find screen");

	/* Create window */

	mask = XCB_CW_EVENT_MASK;
	attrs[0] = XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_STRUCTURE_NOTIFY |
		XCB_EVENT_MASK_KEY_PRESS |
		XCB_EVENT_MASK_POINTER_MOTION |
		XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_BUTTON_RELEASE;

	window = xcb_generate_id(conn);
	xcb_create_window(conn, 0, window, screen->root,
		20, 20, DEFW, DEFH, 0,
		XCB_WINDOW_CLASS_INPUT_OUTPUT, screen->root_visual,
		mask, attrs);

	/* Set title and icon */

	xcb_set_wm_name(conn, window, XCB_ATOM_STRING, strlen(title), title);
	xcb_set_wm_icon_name(conn, window, XCB_ATOM_STRING, strlen(title), title);

	icon = xcb_create_pixmap_from_bitmap_data(conn, window,
		icon_bits, icon_width, icon_height, 1,
		screen->black_pixel, screen->white_pixel, NULL);
	icon_mask = xcb_create_pixmap_from_bitmap_data(conn, window,
		icon_mask_bits, icon_mask_width, icon_mask_height, 1,
		screen->black_pixel, screen->white_pixel, NULL);

	memset(&hints, 0, sizeof hints);
	xcb_wm_hints_set_icon_pixmap(&hints, icon);
	xcb_wm_hints_set_icon_mask(&hints, icon_mask);
	xcb_set_wm_hints(conn, window, &hints);

	xcb_map_window(conn, window);

	/* Create GC */

	mask = XCB_GC_FOREGROUND;
	attrs[0] = screen->black_pixel;

	gc = xcb_generate_id(conn);
	xcb_create_gc(conn, gc, window, mask, attrs);

	xcb_flush(conn);

	/* Enter the event loop */

	if (xcb_connection_has_error(conn))
		die("display connection problem 1");

	surface = ximage_create(conn, screen, screen->root_visual);

	if (xcb_connection_has_error(conn))
		die("display connection problem 2");

	dirty = 0;
	w = h = 0;

	dst->w = 0;
	dst->h = 0;
	dst->samples = NULL;

	while (1) {
		event = xcb_wait_for_event(conn);
		if (!event)
			break;

		while (event) {
			switch (event->response_type & 0x7f) {
			case 0: {
				printf("xcb error: Bad%s\n", xcb_get_error_name(event->pad0));
				break;
			}
			case XCB_EXPOSE: {
				dirty = 1;
				break;
			}
			case XCB_REPARENT_NOTIFY:
			case XCB_MAP_NOTIFY:
			case XCB_UNMAP_NOTIFY:
				break;
			case XCB_CONFIGURE_NOTIFY: {
				xcb_configure_notify_event_t *config;
				config = (xcb_configure_notify_event_t*)event;
				w = config->width;
				h = config->height;
				if (w != dst->w || h != dst->h)
					dirty = 1;
				break;
			}
			case XCB_BUTTON_PRESS: {
				xcb_button_press_event_t *press;
				press = (xcb_button_press_event_t*)event;
				button = 1 << (press->detail - 1);
printf("press %d\n", button);
				ui.x = press->event_x;
				ui.y = press->event_y;
				ui.down |= button;
				ui.key = 0;
				ui_main_loop();
				break;
			}
			case XCB_BUTTON_RELEASE: {
				xcb_button_release_event_t *release;
				release = (xcb_button_release_event_t*)event;
				button = 1 << (release->detail - 1);
printf("release %d\n", button);
				ui.x = release->event_x;
				ui.y = release->event_y;
				ui.down &= ~button;
				ui.key = 0;
				ui_main_loop();
				break;
			}
			case XCB_MOTION_NOTIFY: {
				xcb_motion_notify_event_t *motion;
				motion = (xcb_motion_notify_event_t*)event;
				ui.x = motion->event_x;
				ui.y = motion->event_y;
				ui.key = 0;
				/* TODO: coalesce motion events */
				if (ui.active > 0) {
//puts("motion while active");
					ui_main_loop();
				}
				break;
			}
			case XCB_KEY_PRESS: {
				xcb_key_press_event_t *key;
				key = (xcb_key_press_event_t*)event;
				ui.x = key->event_x;
				ui.y = key->event_y;
				ui.key = key->detail;
				ui_main_loop();
				break;
			}
			default:
				printf("got unknown event %d\n", event->response_type);
				break;
			}
			event = xcb_poll_for_event(conn);
		}

		if (dirty) {
//printf("dirty, repainting\n");
			if (w != dst->w || h != dst->h)
				on_resize(w, h);
			ximage_draw(conn, window, gc, surface,
				0, 0, dst->w, dst->h, dst->samples);
			dirty = 0;
		}
	}

	printf("disconnecting\n");

	ximage_destroy(surface);

	xcb_disconnect(conn);

	return 0;
}
