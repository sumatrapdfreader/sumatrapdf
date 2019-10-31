#include "gl-app.h"

#include <string.h>
#include <stdio.h>

static char *find_string_location(char *s, char *e, float w, float x)
{
	int c;
	while (s < e)
	{
		int n = fz_chartorune(&c, s);
		float cw = ui_measure_character(c);
		if (w + (cw / 2) >= x)
			return s;
		w += cw;
		s += n;
	}
	return e;
}

static char *find_input_location(struct line *lines, int n, float left, float top, float x, float y)
{
	int i = 0;
	if (y > top) i = (y - top) / ui.lineheight;
	if (i >= n) i = n - 1;
	return find_string_location(lines[i].a, lines[i].b, left, x);
}

static inline int myisalnum(char *s)
{
	int cat, c;
	fz_chartorune(&c, s);
	cat = ucdn_get_general_category(c);
	if (cat >= UCDN_GENERAL_CATEGORY_LL && cat <= UCDN_GENERAL_CATEGORY_LU)
		return 1;
	if (cat >= UCDN_GENERAL_CATEGORY_ND && cat <= UCDN_GENERAL_CATEGORY_NO)
		return 1;
	return 0;
}

static char *home_line(char *p, char *start)
{
	while (p > start)
	{
		if (p[-1] == '\n' || p[-1] == '\r')
			return p;
		--p;
	}
	return p;
}

static char *end_line(char *p, char *end)
{
	while (p < end)
	{
		if (p[0] == '\n' || p[0] == '\r')
			return p;
		++p;
	}
	return p;
}

static char *up_line(char *p, char *start)
{
	while (p > start)
	{
		--p;
		if (*p == '\n' || *p == '\r')
			return p;
	}
	return p;
}

static char *down_line(char *p, char *end)
{
	while (p < end)
	{
		if (*p == '\n' || *p == '\r')
			return p+1;
		++p;
	}
	return p;
}

static char *prev_char(char *p, char *start)
{
	--p;
	while ((*p & 0xC0) == 0x80 && p > start) /* skip middle and final multibytes */
		--p;
	return p;
}

static char *next_char(char *p)
{
	++p;
	while ((*p & 0xC0) == 0x80) /* skip middle and final multibytes */
		++p;
	return p;
}

static char *prev_word(char *p, char *start)
{
	while (p > start && !myisalnum(prev_char(p, start))) p = prev_char(p, start);
	while (p > start && myisalnum(prev_char(p, start))) p = prev_char(p, start);
	return p;
}

static char *next_word(char *p, char *end)
{
	while (p < end && !myisalnum(p)) p = next_char(p);
	while (p < end && myisalnum(p)) p = next_char(p);
	return p;
}

static void ui_input_delete_selection(struct input *input)
{
	char *p = input->p < input->q ? input->p : input->q;
	char *q = input->p > input->q ? input->p : input->q;
	memmove(p, q, input->end - q);
	input->end -= q - p;
	*input->end = 0;
	input->p = input->q = p;
}

static void ui_input_paste(struct input *input, const char *buf, int n)
{
	if (input->p != input->q)
		ui_input_delete_selection(input);
	if (input->end + n + 1 < input->text + sizeof(input->text))
	{
		memmove(input->p + n, input->p, input->end - input->p);
		memmove(input->p, buf, n);
		input->p += n;
		input->end += n;
		*input->end = 0;
	}
	input->q = input->p;
}

static int ui_input_key(struct input *input, int multiline)
{
	switch (ui.key)
	{
	case 0:
		return UI_INPUT_NONE;
	case KEY_LEFT:
		if (ui.mod == GLUT_ACTIVE_CTRL + GLUT_ACTIVE_SHIFT)
		{
			input->q = prev_word(input->q, input->text);
		}
		else if (ui.mod == GLUT_ACTIVE_CTRL)
		{
			if (input->p != input->q)
				input->p = input->q = input->p < input->q ? input->p : input->q;
			else
				input->p = input->q = prev_word(input->q, input->text);
		}
		else if (ui.mod == GLUT_ACTIVE_SHIFT)
		{
			if (input->q > input->text)
				input->q = prev_char(input->q, input->text);
		}
		else if (ui.mod == 0)
		{
			if (input->p != input->q)
				input->p = input->q = input->p < input->q ? input->p : input->q;
			else if (input->q > input->text)
				input->p = input->q = prev_char(input->q, input->text);
		}
		break;
	case KEY_RIGHT:
		if (ui.mod == GLUT_ACTIVE_CTRL + GLUT_ACTIVE_SHIFT)
		{
			input->q = next_word(input->q, input->end);
		}
		else if (ui.mod == GLUT_ACTIVE_CTRL)
		{
			if (input->p != input->q)
				input->p = input->q = input->p > input->q ? input->p : input->q;
			else
				input->p = input->q = next_word(input->q, input->end);
		}
		else if (ui.mod == GLUT_ACTIVE_SHIFT)
		{
			if (input->q < input->end)
				input->q = next_char(input->q);
		}
		else if (ui.mod == 0)
		{
			if (input->p != input->q)
				input->p = input->q = input->p > input->q ? input->p : input->q;
			else if (input->q < input->end)
				input->p = input->q = next_char(input->q);
		}
		break;
	case KEY_UP:
		if (ui.mod & GLUT_ACTIVE_SHIFT)
			input->q = up_line(input->q, input->text);
		else
			input->p = input->q = up_line(input->p, input->text);
		break;
	case KEY_DOWN:
		if (ui.mod & GLUT_ACTIVE_SHIFT)
			input->q = down_line(input->q, input->end);
		else
			input->p = input->q = down_line(input->q, input->end);
		break;
	case KEY_HOME:
		if (ui.mod == GLUT_ACTIVE_CTRL + GLUT_ACTIVE_SHIFT)
			input->q = input->text;
		else if (ui.mod == GLUT_ACTIVE_SHIFT)
			input->q = home_line(input->q, input->text);
		else if (ui.mod == GLUT_ACTIVE_CTRL)
			input->p = input->q = input->text;
		else if (ui.mod == 0)
			input->p = input->q = home_line(input->p, input->text);
		break;
	case KEY_END:
		if (ui.mod == GLUT_ACTIVE_CTRL + GLUT_ACTIVE_SHIFT)
			input->q = input->end;
		else if (ui.mod == GLUT_ACTIVE_SHIFT)
			input->q = end_line(input->q, input->end);
		else if (ui.mod == GLUT_ACTIVE_CTRL)
			input->p = input->q = input->end;
		else if (ui.mod == 0)
			input->p = input->q = end_line(input->p, input->end);
		break;
	case KEY_DELETE:
		if (input->p != input->q)
			ui_input_delete_selection(input);
		else if (input->p < input->end)
		{
			char *np = next_char(input->p);
			memmove(input->p, np, input->end - np);
			input->end -= np - input->p;
			*input->end = 0;
			input->q = input->p;
		}
		break;
	case KEY_ESCAPE:
		ui.focus = NULL;
		return UI_INPUT_NONE;
	case KEY_ENTER:
		if (!multiline)
		{
			ui.focus = NULL;
			return UI_INPUT_ACCEPT;
		}
		ui_input_paste(input, "\n", 1);
		break;
	case KEY_BACKSPACE:
		if (input->p != input->q)
			ui_input_delete_selection(input);
		else if (input->p > input->text)
		{
			char *pp = prev_char(input->p, input->text);
			memmove(pp, input->p, input->end - input->p);
			input->end -= input->p - pp;
			*input->end = 0;
			input->q = input->p = pp;
		}
		break;
	case KEY_CTL_A:
		input->p = input->q = input->text;
		break;
	case KEY_CTL_E:
		input->p = input->q = input->end;
		break;
	case KEY_CTL_W:
		if (input->p != input->q)
			ui_input_delete_selection(input);
		else
		{
			input->p = prev_word(input->p, input->text);
			ui_input_delete_selection(input);
		}
		break;
	case KEY_CTL_U:
		input->p = input->q = input->end = input->text;
		*input->end = 0;
		break;
	case KEY_CTL_C:
	case KEY_CTL_X:
		if (input->p != input->q)
		{
			char buf[sizeof input->text];
			char *p = input->p < input->q ? input->p : input->q;
			char *q = input->p > input->q ? input->p : input->q;
			memmove(buf, p, q - p);
			buf[q-p] = 0;
			ui_set_clipboard(buf);
			if (ui.key == KEY_CTL_X)
				ui_input_delete_selection(input);
		}
		break;
	case KEY_CTL_V:
		{
			const char *buf = ui_get_clipboard();
			if (buf)
				ui_input_paste(input, buf, (int)strlen(buf));
		}
		break;
	default:
		if (ui.key >= 32 && ui.plain)
		{
			int cat = ucdn_get_general_category(ui.key);
			if (ui.key == ' ' || (cat >= UCDN_GENERAL_CATEGORY_LL && cat < UCDN_GENERAL_CATEGORY_ZL))
			{
				char buf[8];
				int n = fz_runetochar(buf, ui.key);
				ui_input_paste(input, buf, n);
			}
		}
		break;
	}
	return UI_INPUT_EDIT;
}

void ui_input_init(struct input *input, const char *text)
{
	fz_strlcpy(input->text, text, sizeof input->text);
	input->end = input->text + strlen(input->text);
	input->p = input->text;
	input->q = input->end;
	input->scroll = 0;
}

int ui_input(struct input *input, int width, int height)
{
	struct line lines[500];
	fz_irect area;
	float ax, bx;
	int ay, sy;
	char *p, *q;
	int state;
	int i, n;

	if (ui.focus == input)
		state = ui_input_key(input, height > 1);
	else
		state = UI_INPUT_NONE;

	area = ui_pack(width, ui.lineheight * height + 6);
	ui_draw_bevel_rect(area, UI_COLOR_TEXT_BG, 1);
	area = fz_expand_irect(area, -2);

	if (height > 1)
		area.x1 -= ui.lineheight;

	n = ui_break_lines(input->text, lines, nelem(lines), area.x1-area.x0-2, NULL);

	if (height > 1)
		ui_scrollbar(area.x1, area.y0, area.x1+ui.lineheight, area.y1, &input->scroll, 1, fz_maxi(0, n-height)+1);
	else
		input->scroll = 0;

	ax = area.x0 + 2;
	bx = area.x1 - 2;
	ay = area.y0 + 1;
	sy = input->scroll * ui.lineheight;

	if (ui_mouse_inside(area))
	{
		ui.hot = input;
		if (!ui.active || ui.active == input)
			ui.cursor = GLUT_CURSOR_TEXT;
		if (!ui.active && ui.down)
		{
			input->p = find_input_location(lines, n, ax, ay-sy, ui.x, ui.y);
			ui.active = input;
		}
	}

	if (ui.active == input)
	{
		input->q = find_input_location(lines, n, ax, ay-sy, ui.x, ui.y);
		ui.focus = input;
	}

	p = input->p < input->q ? input->p : input->q;
	q = input->p > input->q ? input->p : input->q;

	for (i = input->scroll; i < n && i < input->scroll+height; ++i)
	{
		char *a = lines[i].a, *b = lines[i].b;
		if (ui.focus == input)
		{
			if (p >= a && p <= b && q >= a && q <= b)
			{
				float px = ax + ui_measure_string_part(a, p);
				float qx = px + ui_measure_string_part(p, q);
				glColorHex(UI_COLOR_TEXT_SEL_BG);
				glRectf(px, ay, qx+1, ay + ui.lineheight);
				glColorHex(UI_COLOR_TEXT_FG);
				ui_draw_string_part(ax, ay, a, p);
				glColorHex(UI_COLOR_TEXT_SEL_FG);
				ui_draw_string_part(px, ay, p, q);
				glColorHex(UI_COLOR_TEXT_FG);
				ui_draw_string_part(qx, ay, q, b);
			}
			else if (p < a && q >= a && q <= b)
			{
				float qx = ax + ui_measure_string_part(a, q);
				glColorHex(UI_COLOR_TEXT_SEL_BG);
				glRectf(ax, ay, qx+1, ay + ui.lineheight);
				glColorHex(UI_COLOR_TEXT_SEL_FG);
				ui_draw_string_part(ax, ay, a, q);
				glColorHex(UI_COLOR_TEXT_FG);
				ui_draw_string_part(qx, ay, q, b);
			}
			else if (p >= a && p <= b && q > b)
			{
				float px = ax + ui_measure_string_part(a, p);
				glColorHex(UI_COLOR_TEXT_SEL_BG);
				glRectf(px, ay, bx, ay + ui.lineheight);
				glColorHex(UI_COLOR_TEXT_FG);
				ui_draw_string_part(ax, ay, a, p);
				glColorHex(UI_COLOR_TEXT_SEL_FG);
				ui_draw_string_part(px, ay, p, b);
			}
			else if (p < a && q > b)
			{
				glColorHex(UI_COLOR_TEXT_SEL_BG);
				glRectf(ax, ay, bx, ay + ui.lineheight);
				glColorHex(UI_COLOR_TEXT_SEL_FG);
				ui_draw_string_part(ax, ay, a, b);
			}
			else
			{
				glColorHex(UI_COLOR_TEXT_FG);
				ui_draw_string_part(ax, ay, a, b);
			}
		}
		else
		{
			glColorHex(UI_COLOR_TEXT_FG);
			ui_draw_string_part(ax, ay, a, b);
		}
		ay += ui.lineheight;
	}

	return state;
}
