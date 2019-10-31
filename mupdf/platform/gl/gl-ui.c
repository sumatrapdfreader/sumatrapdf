#include "gl-app.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#ifndef FREEGLUT
/* freeglut extension no-ops */
void glutExit(void) {}
void glutMouseWheelFunc(void *fn) {}
void glutInitErrorFunc(void *fn) {}
void glutInitWarningFunc(void *fn) {}
#define glutSetOption(X,Y)
#endif

enum
{
	/* Default UI sizes */
	DEFAULT_UI_FONTSIZE = 15,
	DEFAULT_UI_BASELINE = 14,
	DEFAULT_UI_LINEHEIGHT = 18,
	DEFAULT_UI_GRIDSIZE = DEFAULT_UI_LINEHEIGHT + 6,
};

struct ui ui;

#if defined(FREEGLUT) && (GLUT_API_VERSION >= 6)

void ui_set_clipboard(const char *buf)
{
	glutSetClipboard(GLUT_PRIMARY, buf);
	glutSetClipboard(GLUT_CLIPBOARD, buf);
}

const char *ui_get_clipboard(void)
{
	return glutGetClipboard(GLUT_CLIPBOARD);
}

#else

static char *clipboard_buffer = NULL;

void ui_set_clipboard(const char *buf)
{
	fz_free(ctx, clipboard_buffer);
	clipboard_buffer = fz_strdup(ctx, buf);
}

const char *ui_get_clipboard(void)
{
	return clipboard_buffer;
}

#endif

static const char *ogl_error_string(GLenum code)
{
#define CASE(E) case E: return #E; break
	switch (code)
	{
	/* glGetError */
	CASE(GL_NO_ERROR);
	CASE(GL_INVALID_ENUM);
	CASE(GL_INVALID_VALUE);
	CASE(GL_INVALID_OPERATION);
	CASE(GL_OUT_OF_MEMORY);
	CASE(GL_STACK_UNDERFLOW);
	CASE(GL_STACK_OVERFLOW);
	default: return "(unknown)";
	}
#undef CASE
}

static int has_ARB_texture_non_power_of_two = 1;
static GLint max_texture_size = 8192;

void ui_init_draw(void)
{
}

static unsigned int next_power_of_two(unsigned int n)
{
	--n;
	n |= n >> 1;
	n |= n >> 2;
	n |= n >> 4;
	n |= n >> 8;
	n |= n >> 16;
	return ++n;
}

void ui_texture_from_pixmap(struct texture *tex, fz_pixmap *pix)
{
	if (!tex->id)
		glGenTextures(1, &tex->id);
	glBindTexture(GL_TEXTURE_2D, tex->id);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

	tex->x = pix->x;
	tex->y = pix->y;
	tex->w = pix->w;
	tex->h = pix->h;

	if (has_ARB_texture_non_power_of_two)
	{
		if (tex->w > max_texture_size || tex->h > max_texture_size)
			fz_warn(ctx, "texture size (%d x %d) exceeds implementation limit (%d)", tex->w, tex->h, max_texture_size);
		glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, tex->w, tex->h, 0, pix->n == 4 ? GL_RGBA : GL_RGB, GL_UNSIGNED_BYTE, pix->samples);
		tex->s = 1;
		tex->t = 1;
	}
	else
	{
		int w2 = next_power_of_two(tex->w);
		int h2 = next_power_of_two(tex->h);
		if (w2 > max_texture_size || h2 > max_texture_size)
			fz_warn(ctx, "texture size (%d x %d) exceeds implementation limit (%d)", w2, h2, max_texture_size);
		glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w2, h2, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, tex->w, tex->h, pix->n == 4 ? GL_RGBA : GL_RGB, GL_UNSIGNED_BYTE, pix->samples);
		tex->s = (float) tex->w / w2;
		tex->t = (float) tex->h / h2;
	}
}

void ui_draw_image(struct texture *tex, float x, float y)
{
	glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_BLEND);
	glBindTexture(GL_TEXTURE_2D, tex->id);
	glEnable(GL_TEXTURE_2D);
	glBegin(GL_TRIANGLE_STRIP);
	{
		glColor4f(1, 1, 1, 1);
		glTexCoord2f(0, tex->t);
		glVertex2f(x + tex->x, y + tex->y + tex->h);
		glTexCoord2f(0, 0);
		glVertex2f(x + tex->x, y + tex->y);
		glTexCoord2f(tex->s, tex->t);
		glVertex2f(x + tex->x + tex->w, y + tex->y + tex->h);
		glTexCoord2f(tex->s, 0);
		glVertex2f(x + tex->x + tex->w, y + tex->y);
	}
	glEnd();
	glDisable(GL_TEXTURE_2D);
	glDisable(GL_BLEND);
}

void glColorHex(unsigned int hex)
{
	float r = ((hex>>16)&0xff) / 255.0f;
	float g = ((hex>>8)&0xff) / 255.0f;
	float b = ((hex)&0xff) / 255.0f;
	glColor3f(r, g, b);
}

void ui_draw_bevel_imp(fz_irect area, unsigned ot, unsigned it, unsigned ib, unsigned ob)
{
	glColorHex(ot);
	glRectf(area.x0, area.y0, area.x1-1, area.y0+1);
	glRectf(area.x0, area.y0+1, area.x0+1, area.y1-1);
	glColorHex(ob);
	glRectf(area.x1-1, area.y0, area.x1, area.y1);
	glRectf(area.x0, area.y1-1, area.x1-1, area.y1);
	glColorHex(it);
	glRectf(area.x0+1, area.y0+1, area.x1-2, area.y0+2);
	glRectf(area.x0+1, area.y0+2, area.x0+2, area.y1-2);
	glColorHex(ib);
	glRectf(area.x1-2, area.y0+1, area.x1-1, area.y1-1);
	glRectf(area.x0+1, area.y1-2, area.x1-2, area.y1-1);
}

void ui_draw_bevel(fz_irect area, int depressed)
{
	if (depressed)
		ui_draw_bevel_imp(area, UI_COLOR_BEVEL_2, UI_COLOR_BEVEL_1, UI_COLOR_BEVEL_3, UI_COLOR_BEVEL_4);
	else
		ui_draw_bevel_imp(area, UI_COLOR_BEVEL_4, UI_COLOR_BEVEL_3, UI_COLOR_BEVEL_2, UI_COLOR_BEVEL_1);
}

void ui_draw_ibevel(fz_irect area, int depressed)
{
	if (depressed)
		ui_draw_bevel_imp(area, UI_COLOR_BEVEL_2, UI_COLOR_BEVEL_1, UI_COLOR_BEVEL_3, UI_COLOR_BEVEL_4);
	else
		ui_draw_bevel_imp(area, UI_COLOR_BEVEL_3, UI_COLOR_BEVEL_4, UI_COLOR_BEVEL_2, UI_COLOR_BEVEL_1);
}

void ui_draw_bevel_rect(fz_irect area, unsigned int fill, int depressed)
{
	ui_draw_bevel(area, depressed);
	glColorHex(fill);
	glRectf(area.x0+2, area.y0+2, area.x1-2, area.y1-2);
}

void ui_draw_ibevel_rect(fz_irect area, unsigned int fill, int depressed)
{
	ui_draw_ibevel(area, depressed);
	glColorHex(fill);
	glRectf(area.x0+2, area.y0+2, area.x1-2, area.y1-2);
}

#if defined(FREEGLUT) && (GLUT_API_VERSION >= 6)
static void on_keyboard(int key, int x, int y)
#else
static void on_keyboard(unsigned char key, int x, int y)
#endif
{
#ifdef __APPLE__
	/* Apple's GLUT has swapped DELETE and BACKSPACE */
	if (key == 8)
		key = 127;
	else if (key == 127)
		key = 8;
#endif
	ui.x = x;
	ui.y = y;
	ui.key = key;
	ui.mod = glutGetModifiers();
	ui.plain = !(ui.mod & ~GLUT_ACTIVE_SHIFT);
	run_main_loop();
	ui.key = ui.plain = 0;
	ui_invalidate(); // TODO: leave this to caller
}

static void on_special(int key, int x, int y)
{
	ui.x = x;
	ui.y = y;
	ui.key = 0;

	switch (key)
	{
	case GLUT_KEY_INSERT: ui.key = KEY_INSERT; break;
#ifdef GLUT_KEY_DELETE
	case GLUT_KEY_DELETE: ui.key = KEY_DELETE; break;
#endif
	case GLUT_KEY_RIGHT: ui.key = KEY_RIGHT; break;
	case GLUT_KEY_LEFT: ui.key = KEY_LEFT; break;
	case GLUT_KEY_DOWN: ui.key = KEY_DOWN; break;
	case GLUT_KEY_UP: ui.key = KEY_UP; break;
	case GLUT_KEY_PAGE_UP: ui.key = KEY_PAGE_UP; break;
	case GLUT_KEY_PAGE_DOWN: ui.key = KEY_PAGE_DOWN; break;
	case GLUT_KEY_HOME: ui.key = KEY_HOME; break;
	case GLUT_KEY_END: ui.key = KEY_END; break;
	case GLUT_KEY_F1: ui.key = KEY_F1; break;
	case GLUT_KEY_F2: ui.key = KEY_F2; break;
	case GLUT_KEY_F3: ui.key = KEY_F3; break;
	case GLUT_KEY_F4: ui.key = KEY_F4; break;
	case GLUT_KEY_F5: ui.key = KEY_F5; break;
	case GLUT_KEY_F6: ui.key = KEY_F6; break;
	case GLUT_KEY_F7: ui.key = KEY_F7; break;
	case GLUT_KEY_F8: ui.key = KEY_F8; break;
	case GLUT_KEY_F9: ui.key = KEY_F9; break;
	case GLUT_KEY_F10: ui.key = KEY_F10; break;
	case GLUT_KEY_F11: ui.key = KEY_F11; break;
	case GLUT_KEY_F12: ui.key = KEY_F12; break;
	}

	if (ui.key)
	{
		ui.mod = glutGetModifiers();
		ui.plain = !(ui.mod & ~GLUT_ACTIVE_SHIFT);
		run_main_loop();
		ui.key = ui.plain = 0;
		ui_invalidate(); // TODO: leave this to caller
	}
}

static void on_wheel(int wheel, int direction, int x, int y)
{
	ui.scroll_x = wheel == 1 ? direction : 0;
	ui.scroll_y = wheel == 0 ? direction : 0;
	ui.mod = glutGetModifiers();
	run_main_loop();
	ui_invalidate(); // TODO: leave this to caller
	ui.scroll_x = ui.scroll_y = 0;
}

static void on_mouse(int button, int action, int x, int y)
{
	ui.x = x;
	ui.y = y;
	if (action == GLUT_DOWN)
	{
		switch (button)
		{
		case GLUT_LEFT_BUTTON:
			ui.down_x = x;
			ui.down_y = y;
			ui.down = 1;
			break;
		case GLUT_MIDDLE_BUTTON:
			ui.middle_x = x;
			ui.middle_y = y;
			ui.middle = 1;
			break;
		case GLUT_RIGHT_BUTTON:
			ui.right_x = x;
			ui.right_y = y;
			ui.right = 1;
			break;
		case 3: on_wheel(0, 1, x, y); break;
		case 4: on_wheel(0, -1, x, y); break;
		case 5: on_wheel(1, 1, x, y); break;
		case 6: on_wheel(1, -1, x, y); break;
		}
	}
	else if (action == GLUT_UP)
	{
		switch (button)
		{
		case GLUT_LEFT_BUTTON: ui.down = 0; break;
		case GLUT_MIDDLE_BUTTON: ui.middle = 0; break;
		case GLUT_RIGHT_BUTTON: ui.right = 0; break;
		}
	}
	ui.mod = glutGetModifiers();
	run_main_loop();
	ui_invalidate(); // TODO: leave this to caller
}

static void on_motion(int x, int y)
{
	ui.x = x;
	ui.y = y;
	ui_invalidate();
}

static void on_passive_motion(int x, int y)
{
	ui.x = x;
	ui.y = y;
	ui_invalidate();
}

static void on_reshape(int w, int h)
{
	ui.window_w = w;
	ui.window_h = h;
}

static void on_display(void)
{
	run_main_loop();
}

static void on_error(const char *fmt, va_list ap)
{
#ifdef _WIN32
	char buf[1000];
	fz_vsnprintf(buf, sizeof buf, fmt, ap);
	MessageBoxA(NULL, buf, "MuPDF GLUT Error", MB_ICONERROR);
#else
	fprintf(stderr, "GLUT error: ");
	vfprintf(stderr, fmt, ap);
	fprintf(stderr, "\n");
#endif
}

static void on_warning(const char *fmt, va_list ap)
{
	fprintf(stderr, "GLUT warning: ");
	vfprintf(stderr, fmt, ap);
	fprintf(stderr, "\n");
}

static void on_timer(int timer_id)
{
	if (reloadrequested)
	{
		reload();
		ui_invalidate();
		reloadrequested = 0;
	}
	glutTimerFunc(500, on_timer, 0);
}

void ui_init(int w, int h, const char *title)
{
	float ui_scale;

#ifdef FREEGLUT
	glutSetOption(GLUT_ACTION_ON_WINDOW_CLOSE, GLUT_ACTION_GLUTMAINLOOP_RETURNS);
#endif

	glutInitErrorFunc(on_error);
	glutInitWarningFunc(on_warning);
	glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE);
	glutInitWindowSize(w, h);
	glutCreateWindow(title);

	glutTimerFunc(500, on_timer, 0);
	glutReshapeFunc(on_reshape);
	glutDisplayFunc(on_display);
#if defined(FREEGLUT) && (GLUT_API_VERSION >= 6)
	glutKeyboardExtFunc(on_keyboard);
#else
	glutKeyboardFunc(on_keyboard);
#endif
	glutSpecialFunc(on_special);
	glutMouseFunc(on_mouse);
	glutMotionFunc(on_motion);
	glutPassiveMotionFunc(on_passive_motion);
	glutMouseWheelFunc(on_wheel);

	has_ARB_texture_non_power_of_two = glutExtensionSupported("GL_ARB_texture_non_power_of_two");
	if (!has_ARB_texture_non_power_of_two)
		fz_warn(ctx, "OpenGL implementation does not support non-power of two texture sizes");

	glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max_texture_size);

	ui_scale = 1;
	{
		int wmm = glutGet(GLUT_SCREEN_WIDTH_MM);
		int wpx = glutGet(GLUT_SCREEN_WIDTH);
		int hmm = glutGet(GLUT_SCREEN_HEIGHT_MM);
		int hpx = glutGet(GLUT_SCREEN_HEIGHT);
		if (wmm > 0 && hmm > 0)
		{
			float ppi = ((wpx * 254) / wmm + (hpx * 254) / hmm) / 20;
			if (ppi >= 144) ui_scale = 1.5f;
			if (ppi >= 192) ui_scale = 2.0f;
			if (ppi >= 288) ui_scale = 3.0f;
		}
	}

	ui.fontsize = DEFAULT_UI_FONTSIZE * ui_scale;
	ui.baseline = DEFAULT_UI_BASELINE * ui_scale;
	ui.lineheight = DEFAULT_UI_LINEHEIGHT * ui_scale;
	ui.gridsize = DEFAULT_UI_GRIDSIZE * ui_scale;

	ui_init_fonts();

	ui.overlay_list = glGenLists(1);
}

void ui_finish(void)
{
	glDeleteLists(ui.overlay_list, 1);
	ui_finish_fonts();
	glutExit();
}

void ui_invalidate(void)
{
	glutPostRedisplay();
}

void ui_begin(void)
{
	ui.hot = NULL;

	ui.cavity = ui.cavity_stack;
	ui.cavity->x0 = 0;
	ui.cavity->y0 = 0;
	ui.cavity->x1 = ui.window_w;
	ui.cavity->y1 = ui.window_h;

	ui.layout = ui.layout_stack;
	ui.layout->side = ALL;
	ui.layout->fill = BOTH;
	ui.layout->anchor = NW;
	ui.layout->padx = 0;
	ui.layout->pady = 0;

	ui.cursor = GLUT_CURSOR_INHERIT;

	ui.overlay = 0;

	glViewport(0, 0, ui.window_w, ui.window_h);
	glClear(GL_COLOR_BUFFER_BIT);

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0, ui.window_w, ui.window_h, 0, -1, 1);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
}

void ui_end(void)
{
	int code;

	if (ui.overlay)
		glCallList(ui.overlay_list);

	if (ui.cursor != ui.last_cursor)
	{
		glutSetCursor(ui.cursor);
		ui.last_cursor = ui.cursor;
	}

	code = glGetError();
	if (code != GL_NO_ERROR)
		fz_warn(ctx, "glGetError: %s", ogl_error_string(code));

	if (!ui.active && (ui.down || ui.middle || ui.right))
		ui.active = "dummy";

	if ((ui.grab_down && !ui.down) || (ui.grab_middle && !ui.middle) || (ui.grab_right && !ui.right))
	{
		ui.grab_down = ui.grab_middle = ui.grab_right = 0;
		ui.active = NULL;
	}

	if (ui.active)
	{
		if (ui.active != ui.focus)
			ui.focus = NULL;
		if (!ui.grab_down && !ui.grab_middle && !ui.grab_right)
		{
			ui.grab_down = ui.down;
			ui.grab_middle = ui.middle;
			ui.grab_right = ui.right;
		}
	}

	glutSwapBuffers();
}

/* Widgets */

int ui_mouse_inside(fz_irect area)
{
	if (ui.x >= area.x0 && ui.x < area.x1 && ui.y >= area.y0 && ui.y < area.y1)
		return 1;
	return 0;
}

fz_irect ui_pack_layout(int slave_w, int slave_h, enum side side, enum fill fill, enum anchor anchor, int padx, int pady)
{
	fz_irect parcel, slave;
	int parcel_w, parcel_h;
	int anchor_x, anchor_y;

	switch (side)
	{
	default:
	case ALL:
		parcel.x0 = ui.cavity->x0 + padx;
		parcel.x1 = ui.cavity->x1 - padx;
		parcel.y0 = ui.cavity->y0 + pady;
		parcel.y1 = ui.cavity->y1 - pady;
		ui.cavity->x0 = ui.cavity->x1;
		ui.cavity->y0 = ui.cavity->y1;
		break;
	case T:
		parcel.x0 = ui.cavity->x0 + padx;
		parcel.x1 = ui.cavity->x1 - padx;
		parcel.y0 = ui.cavity->y0 + pady;
		parcel.y1 = ui.cavity->y0 + pady + slave_h;
		ui.cavity->y0 = parcel.y1 + pady;
		break;
	case B:
		parcel.x0 = ui.cavity->x0 + padx;
		parcel.x1 = ui.cavity->x1 - padx;
		parcel.y0 = ui.cavity->y1 - pady - slave_h;
		parcel.y1 = ui.cavity->y1 - pady;
		ui.cavity->y1 = parcel.y0 - pady;
		break;
	case L:
		parcel.x0 = ui.cavity->x0 + padx;
		parcel.x1 = ui.cavity->x0 + padx + slave_w;
		parcel.y0 = ui.cavity->y0 + pady;
		parcel.y1 = ui.cavity->y1 - pady;
		ui.cavity->x0 = parcel.x1 + padx;
		break;
	case R:
		parcel.x0 = ui.cavity->x1 - padx - slave_w;
		parcel.x1 = ui.cavity->x1 - padx;
		parcel.y0 = ui.cavity->y0 + pady;
		parcel.y1 = ui.cavity->y1 - pady;
		ui.cavity->x1 = parcel.x0 - padx;
		break;
	}

	parcel_w = parcel.x1 - parcel.x0;
	parcel_h = parcel.y1 - parcel.y0;

	if (fill & X)
		slave_w = parcel_w;
	if (fill & Y)
		slave_h = parcel_h;

	anchor_x = parcel_w - slave_w;
	anchor_y = parcel_h - slave_h;

	switch (anchor)
	{
	default:
	case CENTER:
		slave.x0 = parcel.x0 + anchor_x / 2;
		slave.y0 = parcel.y0 + anchor_y / 2;
		break;
	case N:
		slave.x0 = parcel.x0 + anchor_x / 2;
		slave.y0 = parcel.y0;
		break;
	case NE:
		slave.x0 = parcel.x0 + anchor_x;
		slave.y0 = parcel.y0;
		break;
	case E:
		slave.x0 = parcel.x0 + anchor_x;
		slave.y0 = parcel.y0 + anchor_y / 2;
		break;
	case SE:
		slave.x0 = parcel.x0 + anchor_x;
		slave.y0 = parcel.y0 + anchor_y;
		break;
	case S:
		slave.x0 = parcel.x0 + anchor_x / 2;
		slave.y0 = parcel.y0 + anchor_y;
		break;
	case SW:
		slave.x0 = parcel.x0;
		slave.y0 = parcel.y0 + anchor_y;
		break;
	case W:
		slave.x0 = parcel.x0;
		slave.y0 = parcel.y0 + anchor_y / 2;
		break;
	case NW:
		slave.x0 = parcel.x0;
		slave.y0 = parcel.y0;
		break;
	}

	slave.x1 = slave.x0 + slave_w;
	slave.y1 = slave.y0 + slave_h;

	return slave;
}

fz_irect ui_pack(int slave_w, int slave_h)
{
	return ui_pack_layout(slave_w, slave_h, ui.layout->side, ui.layout->fill, ui.layout->anchor, ui.layout->padx, ui.layout->pady);
}

int ui_available_width(void)
{
	return ui.cavity->x1 - ui.cavity->x0 - ui.layout->padx * 2;
}

int ui_available_height(void)
{
	return ui.cavity->y1 - ui.cavity->y0 - ui.layout->pady * 2;
}

void ui_pack_push(fz_irect cavity)
{
	*(++ui.cavity) = cavity;
	++ui.layout;
	ui.layout->side = ALL;
	ui.layout->fill = BOTH;
	ui.layout->anchor = NW;
	ui.layout->padx = 0;
	ui.layout->pady = 0;
}

void ui_pack_pop(void)
{
	--ui.cavity;
	--ui.layout;
}

void ui_layout(enum side side, enum fill fill, enum anchor anchor, int padx, int pady)
{
	ui.layout->side = side;
	ui.layout->fill = fill;
	ui.layout->anchor = anchor;
	ui.layout->padx = padx;
	ui.layout->pady = pady;
}

void ui_panel_begin(int w, int h, int padx, int pady, int opaque)
{
	fz_irect area = ui_pack(w, h);
	if (opaque)
	{
		glColorHex(UI_COLOR_PANEL);
		glRectf(area.x0, area.y0, area.x1, area.y1);
	}
	area.x0 += padx; area.y0 += padx;
	area.x1 -= pady; area.y1 -= pady;
	ui_pack_push(area);
}

void ui_panel_end(void)
{
	ui_pack_pop();
}

void ui_dialog_begin(int w, int h)
{
	fz_irect area;
	int x, y;
	w += 24 + 4;
	h += 24 + 4;
	if (w > ui.window_w) w = ui.window_w - 20;
	if (h > ui.window_h) h = ui.window_h - 20;
	x = (ui.window_w-w)/2;
	y = (ui.window_h-h)/3;
	area = fz_make_irect(x, y, x+w, y+h);
	ui_draw_bevel_rect(area, UI_COLOR_PANEL, 0);
	area = fz_expand_irect(area, -14);
	ui_pack_push(area);
}

void ui_dialog_end(void)
{
	ui_pack_pop();
}

void ui_spacer(void)
{
	ui_pack(ui.lineheight / 2, ui.lineheight / 2);
}

void ui_label(const char *fmt, ...)
{
	char buf[512];
	struct line lines[20];
	int avail, used, n;
	fz_irect area;
	va_list ap;

	va_start(ap, fmt);
	fz_vsnprintf(buf, sizeof buf, fmt, ap);
	va_end(ap);

	avail = ui_available_width();
	n = ui_break_lines(buf, lines, nelem(lines), avail, &used);
	area = ui_pack(used, n * ui.lineheight);
	glColorHex(UI_COLOR_TEXT_FG);
	ui_draw_lines(area.x0, area.y0, lines, n);
}

int ui_button(const char *label)
{
	int width = ui_measure_string(label);
	fz_irect area = ui_pack(width + 20, ui.gridsize);
	int text_x = area.x0 + ((area.x1 - area.x0) - width) / 2;
	int pressed;

	if (ui_mouse_inside(area))
	{
		ui.hot = label;
		if (!ui.active && ui.down)
			ui.active = label;
	}

	pressed = (ui.hot == label && ui.active == label && ui.down);
	ui_draw_bevel_rect(area, UI_COLOR_BUTTON, pressed);
	glColorHex(UI_COLOR_TEXT_FG);
	ui_draw_string(text_x + pressed, area.y0+3 + pressed, label);

	return ui.hot == label && ui.active == label && !ui.down;
}

int ui_checkbox(const char *label, int *value)
{
	int width = ui_measure_string(label);
	fz_irect area = ui_pack(13 + 4 + width, ui.lineheight);
	fz_irect mark = { area.x0, area.y0 + ui.baseline-12, area.x0 + 13, area.y0 + ui.baseline+1 };
	int pressed;

	glColorHex(UI_COLOR_TEXT_FG);
	ui_draw_string(mark.x1 + 4, area.y0, label);

	if (ui_mouse_inside(area))
	{
		ui.hot = label;
		if (!ui.active && ui.down)
			ui.active = label;
	}

	if (ui.hot == label && ui.active == label && !ui.down)
		*value = !*value;

	pressed = (ui.hot == label && ui.active == label && ui.down);
	ui_draw_bevel_rect(mark, pressed ? UI_COLOR_PANEL : UI_COLOR_TEXT_BG, 1);
	if (*value)
	{
		float ax = mark.x0+2 + 1, ay = mark.y0+2 + 3;
		float bx = mark.x0+2 + 4, by = mark.y0+2 + 5;
		float cx = mark.x0+2 + 8, cy = mark.y0+2 + 1;
		glColorHex(UI_COLOR_TEXT_FG);
		glBegin(GL_TRIANGLE_STRIP);
		glVertex2f(ax, ay); glVertex2f(ax, ay+3);
		glVertex2f(bx, by); glVertex2f(bx, by+3);
		glVertex2f(cx, cy); glVertex2f(cx, cy+3);
		glEnd();
	}

	return ui.hot == label && ui.active == label && !ui.down;
}

int ui_slider(int *value, int min, int max, int width)
{
	static int start_value = 0;
	fz_irect area = ui_pack(width, ui.lineheight);
	int m = 6;
	int w = area.x1 - area.x0 - m * 2;
	int h = area.y1 - area.y0;
	fz_irect gutter = { area.x0, area.y0+h/2-2, area.x1, area.y0+h/2+2 };
	fz_irect thumb;
	int x;

	if (ui_mouse_inside(area))
	{
		ui.hot = value;
		if (!ui.active && ui.down)
		{
			ui.active = value;
			start_value = *value;
		}
	}

	if (ui.active == value)
	{
		if (ui.y < area.y0 || ui.y > area.y1)
			*value = start_value;
		else
		{
			float v = (float)(ui.x - (area.x0+m)) / w;
			*value = fz_clamp(min + v * (max - min), min, max);
		}
	}

	x = ((*value - min) * w) / (max - min);
	thumb = fz_make_irect(area.x0+m + x-m, area.y0, area.x0+m + x+m, area.y1);

	ui_draw_bevel(gutter, 1);
	ui_draw_bevel_rect(thumb, UI_COLOR_BUTTON, 0);

	return *value != start_value && ui.active == value && !ui.down;
}

void ui_splitter(int *x, int min, int max, enum side side)
{
	static int start_x = 0;
	fz_irect area = ui_pack(4, 0);

	if (ui_mouse_inside(area))
	{
		ui.hot = x;
		if (!ui.active && ui.down)
		{
			ui.active = x;
			start_x = *x;
		}
	}

	if (ui.active == x)
		*x = fz_clampi(start_x + (ui.x - ui.down_x), min, max);

	if (ui.hot == x || ui.active == x)
		ui.cursor = GLUT_CURSOR_LEFT_RIGHT;

	if (side == L)
	{
		glColorHex(UI_COLOR_BEVEL_4);
		glRectf(area.x0+0, area.y0, area.x0+2, area.y1);
		glColorHex(UI_COLOR_BEVEL_3);
		glRectf(area.x0+2, area.y0, area.x0+3, area.y1);
		glColorHex(UI_COLOR_PANEL);
		glRectf(area.x0+3, area.y0, area.x0+4, area.y1);
	}
	if (side == R)
	{
		glColorHex(UI_COLOR_PANEL);
		glRectf(area.x0, area.y0, area.x0+2, area.y1);
		glColorHex(UI_COLOR_BEVEL_2);
		glRectf(area.x0+2, area.y0, area.x0+3, area.y1);
		glColorHex(UI_COLOR_BEVEL_1);
		glRectf(area.x0+3, area.y0, area.x0+4, area.y1);
	}
}

void ui_scrollbar(int x0, int y0, int x1, int y1, int *value, int page_size, int max)
{
	static float start_top = 0; /* we can only drag in one scrollbar at a time, so static is safe */
	float top;

	int total_h = y1 - y0;
	int thumb_h = fz_maxi(x1 - x0, total_h * page_size / max);
	int avail_h = total_h - thumb_h;

	max -= page_size;

	if (max <= 0)
	{
		*value = 0;
		glColorHex(UI_COLOR_SCROLLBAR);
		glRectf(x0, y0, x1, y1);
		return;
	}

	top = (float) *value * avail_h / max;

	if (ui.down && !ui.active)
	{
		if (ui.x >= x0 && ui.x < x1 && ui.y >= y0 && ui.y < y1)
		{
			if (ui.y < y0 + top)
			{
				ui.active = "pgdn";
				*value -= page_size;
			}
			else if (ui.y >= y0 + top + thumb_h)
			{
				ui.active = "pgup";
				*value += page_size;
			}
			else
			{
				ui.hot = value;
				ui.active = value;
				start_top = top;
			}
		}
	}

	if (ui.active == value)
	{
		*value = (start_top + ui.y - ui.down_y) * max / avail_h;
	}

	if (*value < 0)
		*value = 0;
	else if (*value > max)
		*value = max;

	top = (float) *value * avail_h / max;

	glColorHex(UI_COLOR_SCROLLBAR);
	glRectf(x0, y0, x1, y1);
	ui_draw_ibevel_rect(fz_make_irect(x0, y0+top, x1, y0+top+thumb_h), UI_COLOR_BUTTON, 0);
}

void ui_tree_begin(struct list *list, int count, int req_w, int req_h, int is_tree)
{
	static int start_scroll_y = 0; /* we can only drag in one list at a time, so static is safe */

	fz_irect outer_area = ui_pack(req_w, req_h);
	fz_irect area = { outer_area.x0+2, outer_area.y0+2, outer_area.x1-2, outer_area.y1-2 };

	int max_scroll_y = count * ui.lineheight - (area.y1-area.y0);

	if (max_scroll_y > 0)
		area.x1 -= 16;

	if (ui_mouse_inside(area))
	{
		ui.hot = list;
		if (!ui.active && ui.middle)
		{
			ui.active = list;
			start_scroll_y = list->scroll_y;
		}
	}

	/* middle button dragging */
	if (ui.active == list)
		list->scroll_y = start_scroll_y + (ui.middle_y - ui.y) * 5;

	/* scroll wheel events */
	if (ui.hot == list)
		list->scroll_y -= ui.scroll_y * ui.lineheight * 3;

	/* clamp scrolling to client area */
	if (list->scroll_y >= max_scroll_y)
		list->scroll_y = max_scroll_y;
	if (list->scroll_y < 0)
		list->scroll_y = 0;

	ui_draw_bevel_rect(outer_area, UI_COLOR_TEXT_BG, 1);
	if (max_scroll_y > 0)
	{
		ui_scrollbar(area.x1, area.y0, area.x1+16, area.y1,
				&list->scroll_y, area.y1-area.y0, count * ui.lineheight);
	}

	list->is_tree = is_tree;
	list->area = area;
	list->item_y = area.y0 - list->scroll_y;

	glScissor(list->area.x0, ui.window_h-list->area.y1, list->area.x1-list->area.x0, list->area.y1-list->area.y0);
	glEnable(GL_SCISSOR_TEST);
}

int ui_tree_item(struct list *list, const void *id, const char *label, int selected, int depth, int is_branch, int *is_open)
{
	fz_irect area = { list->area.x0, list->item_y, list->area.x1, list->item_y + ui.lineheight };
	int x_handle, x_item;

	x_item = ui.lineheight / 4;
	x_item += depth * ui.lineheight;
	x_handle = x_item;
	if (list->is_tree)
		x_item += ui_measure_character(0x25BC) + ui.lineheight / 4;

	/* only process visible items */
	if (area.y1 >= list->area.y0 && area.y0 <= list->area.y1)
	{
		if (ui_mouse_inside(list->area) && ui_mouse_inside(area))
		{
			if (list->is_tree && ui.x < area.x0 + x_item)
			{
				ui.hot = is_open;
			}
			else
				ui.hot = id;
			if (!ui.active && ui.down)
			{
				if (list->is_tree && ui.hot == is_open)
					*is_open = !*is_open;
				ui.active = ui.hot;
			}
		}

		if (ui.active == id || selected)
		{
			glColorHex(UI_COLOR_TEXT_SEL_BG);
			glRectf(area.x0, area.y0, area.x1, area.y1);
			glColorHex(UI_COLOR_TEXT_SEL_FG);
		}
		else
		{
			glColorHex(UI_COLOR_TEXT_FG);
		}

		ui_draw_string(area.x0 + x_item, area.y0, label);
		if (list->is_tree && is_branch)
			ui_draw_character(area.x0 + x_handle, area.y0,
				*is_open ? 0x25BC : 0x25B6);
	}

	list->item_y += ui.lineheight;

	/* trigger on mouse up */
	return ui.active == id && !ui.down;
}

void ui_list_begin(struct list *list, int count, int req_w, int req_h)
{
	ui_tree_begin(list, count, req_w, req_h, 0);
}

int ui_list_item(struct list *list, const void *id, const char *label, int selected)
{
	return ui_tree_item(list, id, label, selected, 0, 0, NULL);
}

void ui_tree_end(struct list *list)
{
	glDisable(GL_SCISSOR_TEST);
}

void ui_list_end(struct list *list)
{
	ui_tree_end(list);
}

void ui_label_with_scrollbar(char *text, int width, int height, int *scroll)
{
	struct line lines[500];
	fz_irect area;
	int n;

	area = ui_pack(width, height);
	n = ui_break_lines(text, lines, nelem(lines), area.x1-area.x0 - 16, NULL);
	if (n > (area.y1-area.y0) / ui.lineheight)
	{
		if (ui_mouse_inside(area))
			*scroll -= ui.scroll_y * ui.lineheight * 3;
		ui_scrollbar(area.x1-16, area.y0, area.x1, area.y1, scroll, area.y1-area.y0, n * ui.lineheight);
	}
	else
		*scroll = 0;

	glScissor(area.x0, ui.window_h-area.y1, area.x1-area.x0-16, area.y1-area.y0);
	glEnable(GL_SCISSOR_TEST);
	glColorHex(UI_COLOR_TEXT_FG);
	ui_draw_lines(area.x0, area.y0 - *scroll, lines, n);
	glDisable(GL_SCISSOR_TEST);
}

int ui_popup(const void *id, const char *label, int is_button, int count)
{
	int width = ui_measure_string(label);
	fz_irect area = ui_pack(width + 22 + 6, ui.gridsize);
	fz_irect menu_area;
	int pressed;

	if (ui_mouse_inside(area))
	{
		ui.hot = id;
		if (!ui.active && ui.down)
			ui.active = id;
	}

	pressed = (ui.active == id);

	if (is_button)
	{
		ui_draw_bevel_rect(area, UI_COLOR_BUTTON, pressed);
		glColorHex(UI_COLOR_TEXT_FG);
		ui_draw_string(area.x0 + 6+pressed, area.y0+3+pressed, label);
		glBegin(GL_TRIANGLES);
		glVertex2f(area.x1+pressed-8-10, area.y0+pressed+9);
		glVertex2f(area.x1+pressed-8, area.y0+pressed+9);
		glVertex2f(area.x1+pressed-8-4, area.y0+pressed+14);
		glEnd();
	}
	else
	{
		fz_irect arrow = { area.x1-22, area.y0+2, area.x1-2, area.y1-2 };
		ui_draw_bevel_rect(area, UI_COLOR_TEXT_BG, 1);
		glColorHex(UI_COLOR_TEXT_FG);
		ui_draw_string(area.x0 + 6, area.y0+3, label);
		ui_draw_ibevel_rect(arrow, UI_COLOR_BUTTON, pressed);

		glColorHex(UI_COLOR_TEXT_FG);
		glBegin(GL_TRIANGLES);
		glVertex2f(area.x1+pressed-8-10, area.y0+pressed+9);
		glVertex2f(area.x1+pressed-8, area.y0+pressed+9);
		glVertex2f(area.x1+pressed-8-4, area.y0+pressed+14);
		glEnd();
	}

	if (pressed)
	{
		ui.overlay = 1;

		glNewList(ui.overlay_list, GL_COMPILE);

		/* Area inside the border line */
		menu_area.x0 = area.x0+1;
		menu_area.x1 = area.x1-1; // TODO: width of submenu
		if (area.y1+2 + count * ui.lineheight < ui.window_h)
		{
			menu_area.y0 = area.y1+2;
			menu_area.y1 = menu_area.y0 + count * ui.lineheight;
		}
		else
		{
			menu_area.y1 = area.y0-2;
			menu_area.y0 = menu_area.y1 - count * ui.lineheight;
		}

		glColorHex(UI_COLOR_TEXT_FG);
		glRectf(menu_area.x0-1, menu_area.y0-1, menu_area.x1+1, menu_area.y1+1);
		glColorHex(UI_COLOR_TEXT_BG);
		glRectf(menu_area.x0, menu_area.y0, menu_area.x1, menu_area.y1);

		ui_pack_push(menu_area);
		ui_layout(T, X, NW, 0, 0);
	}

	return pressed;
}

int ui_popup_item(const char *title)
{
	fz_irect area = ui_pack(0, ui.lineheight);

	if (ui_mouse_inside(area))
	{
		ui.hot = title;
		glColorHex(UI_COLOR_TEXT_SEL_BG);
		glRectf(area.x0, area.y0, area.x1, area.y1);
		glColorHex(UI_COLOR_TEXT_SEL_FG);
		ui_draw_string(area.x0 + 4, area.y0, title);
	}
	else
	{
		glColorHex(UI_COLOR_TEXT_FG);
		ui_draw_string(area.x0 + 4, area.y0, title);
	}

	return ui.hot == title && !ui.down;
}

void ui_popup_end(void)
{
	glEndList();
	ui_pack_pop();
}

int ui_select(const void *id, const char *current, const char *options[], int n)
{
	int i, choice = -1;
	if (ui_popup(id, current, 0, n))
	{
		for (i = 0; i < n; ++i)
			if (ui_popup_item(options[i]))
				choice = i;
		ui_popup_end();
	}
	return choice;
}
