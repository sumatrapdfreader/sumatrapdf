// Copyright (C) 2004-2023 Artifex Software, Inc.
//
// This file is part of MuPDF.
//
// MuPDF is free software: you can redistribute it and/or modify it under the
// terms of the GNU Affero General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option)
// any later version.
//
// MuPDF is distributed in the hope that it will be useful, but WITHOUT ANY
// WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
// FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for more
// details.
//
// You should have received a copy of the GNU Affero General Public License
// along with MuPDF. If not, see <https://www.gnu.org/licenses/agpl-3.0.en.html>
//
// Alternative licensing terms are available from the licensor.
// For commercial licensing, see <https://www.artifex.com/> or contact
// Artifex Software, Inc., 39 Mesa Street, Suite 108A, San Francisco,
// CA 94129, USA, for further information.

#include "gl-app.h"

#include <limits.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#ifndef _WIN32
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#endif

#include "mupdf/helpers/pkcs7-openssl.h"

#include "mujs.h"

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#ifndef _WIN32
#include <sys/stat.h> /* for mkdir */
#include <unistd.h> /* for getcwd */
#include <spawn.h> /* for posix_spawn */
extern char **environ; /* see environ (7) */
#else
#include <direct.h> /* for getcwd */
#endif

#ifdef __APPLE__
static void cleanup(void);
void glutLeaveMainLoop(void)
{
	cleanup();
	exit(0);
}
#endif

fz_context *ctx = NULL;
fz_colorspace *profile = NULL;
pdf_document *pdf = NULL;
pdf_page *page = NULL;
fz_stext_page *page_text = NULL;
fz_matrix draw_page_ctm, view_page_ctm, view_page_inv_ctm;
fz_rect page_bounds, draw_page_bounds, view_page_bounds;
fz_irect view_page_area;
char filename[PATH_MAX];

enum
{
	/* Screen furniture: aggregate size of unusable space from title bars, task bars, window borders, etc */
	SCREEN_FURNITURE_W = 20,
	SCREEN_FURNITURE_H = 40,
};

static void open_browser(const char *uri)
{
#ifndef _WIN32
	char *argv[3];
#endif
	char buf[PATH_MAX];

#ifndef _WIN32
	pid_t pid;
	int err;
#endif

	/* Relative file: URI, make it absolute! */
	if (!strncmp(uri, "file:", 5) && uri[5] != '/')
	{
		char buf_base[PATH_MAX];
		char buf_cwd[PATH_MAX];
		fz_dirname(buf_base, filename, sizeof buf_base);
		if (getcwd(buf_cwd, sizeof buf_cwd))
		{
			fz_snprintf(buf, sizeof buf, "file://%s/%s/%s", buf_cwd, buf_base, uri+5);
			fz_cleanname(buf+7);
			uri = buf;
		}
	}

	if (strncmp(uri, "file://", 7) && strncmp(uri, "http://", 7) && strncmp(uri, "https://", 8) && strncmp(uri, "mailto:", 7))
	{
		fz_warn(ctx, "refusing to open unknown link (%s)", uri);
		return;
	}

#ifdef _WIN32
	ShellExecuteA(NULL, "open", uri, 0, 0, SW_SHOWNORMAL);
#else
	const char *browser = getenv("BROWSER");
	if (!browser)
	{
#ifdef __APPLE__
		browser = "open";
#else
		browser = "xdg-open";
#endif
	}

	argv[0] = (char*) browser;
	argv[1] = (char*) uri;
	argv[2] = NULL;
	err = posix_spawnp(&pid, browser, NULL, NULL, argv, environ);
	if (err)
		fz_warn(ctx, "cannot spawn browser '%s': %s", browser, strerror(err));

#endif
}

static const int zoom_list[] = {
	6, 12, 24, 36, 48, 60, 72, 84, 96, 108,
	120, 144, 168, 192, 228, 264,
	300, 350, 400, 450, 500, 550, 600
};

static int zoom_in(int oldres)
{
	int i;
	for (i = 0; i < (int)nelem(zoom_list) - 1; ++i)
		if (zoom_list[i] <= oldres && zoom_list[i+1] > oldres)
			return zoom_list[i+1];
	return zoom_list[i];
}

static int zoom_out(int oldres)
{
	int i;
	for (i = 0; i < (int)nelem(zoom_list) - 1; ++i)
		if (zoom_list[i] < oldres && zoom_list[i+1] >= oldres)
			return zoom_list[i];
	return zoom_list[0];
}

static const char *paper_size_name(int w, int h)
{
	/* ISO A */
	if (w == 2384 && h == 3370) return "A0";
	if (w == 1684 && h == 2384) return "A1";
	if (w == 1191 && h == 1684) return "A2";
	if (w == 842 && h == 1191) return "A3";
	if (w == 595 && h == 842) return "A4";
	if (w == 420 && h == 595) return "A5";
	if (w == 297 && h == 420) return "A6";

	/* US */
	if (w == 612 && h == 792) return "Letter";
	if (w == 612 && h == 1008) return "Legal";
	if (w == 792 && h == 1224) return "Ledger";
	if (w == 1224 && h == 792) return "Tabloid";

	return NULL;
}

#define MINRES (zoom_list[0])
#define MAXRES (zoom_list[nelem(zoom_list)-1])
#define DEFRES 96

static char *password = "";
static char *anchor = NULL;
static float layout_w = FZ_DEFAULT_LAYOUT_W;
static float layout_h = FZ_DEFAULT_LAYOUT_H;
static float layout_em = FZ_DEFAULT_LAYOUT_EM;
static char *layout_css = NULL;
static int layout_use_doc_css = 1;
static int enable_js = 1;
static int tint_white = 0xFFFFF0;
static int tint_black = 0x303030;

static fz_document *doc = NULL;
static fz_page *fzpage = NULL;
static fz_separations *seps = NULL;
static fz_outline *outline = NULL;
static fz_link *links = NULL;

static int number = 0;

static fz_pixmap *page_contents = NULL;
static struct texture page_tex = { 0 };
static int screen_w = 0, screen_h = 0;
static int scroll_x = 0, scroll_y = 0;
static int canvas_x = 0, canvas_w = 100;
static int canvas_y = 0, canvas_h = 100;

static int outline_w = 14; /* to be scaled by lineheight */
static int annotate_w = 12; /* to be scaled by lineheight */
static int console_h = 14; /* to be scaled by lineheight */

static int outline_start_x = 0;
static int console_start_y = 0;

static int oldbox = FZ_CROP_BOX, currentbox = FZ_CROP_BOX;
static int oldtint = 0, currenttint = 0;
static int oldinvert = 0, currentinvert = 0;
static int oldicc = 1, currenticc = 1;
static int oldaa = 8, currentaa = 8;
static int oldseparations = 1, currentseparations = 1;
static fz_location oldpage = {0,0}, currentpage = {0,0};
static float oldzoom = DEFRES, currentzoom = DEFRES;
static float oldrotate = 0, currentrotate = 0;
int page_contents_changed = 0;
int page_annots_changed = 0;

static fz_output *trace_file = NULL;
static char *reflow_options = NULL;
static int isfullscreen = 0;
static int showoutline = 0;
static int showundo = 0;
static int showlayers = 0;
static int showlinks = 0;
static int showsearch = 0;
static int showconsole = 0;
int showannotate = 0;
int showform = 0;

static pdf_js_console gl_js_console;

static const char *tooltip = NULL;

struct mark
{
	fz_location loc;
	fz_point scroll;
};

static int history_count = 0;
static struct mark history[256];
static int future_count = 0;
static struct mark future[256];
static struct mark marks[10];

static char *get_history_filename(void)
{
	static char history_path[PATH_MAX];
	static int once = 0;
	if (!once)
	{
		char *home = getenv("MUPDF_HISTORY");
		if (home)
			return home;
		home = getenv("XDG_CACHE_HOME");
		if (!home)
			home = getenv("HOME");
		if (!home)
			home = getenv("USERPROFILE");
		if (!home)
			home = "/tmp";
		fz_snprintf(history_path, sizeof history_path, "%s/.mupdf.history", home);
		fz_cleanname(history_path);
		once = 1;
	}
	return history_path;
}

static int read_history_file_as_json(js_State *J)
{
	fz_buffer *buf = NULL;
	const char *json = "{}";
	const char *history_file;

	fz_var(buf);

	history_file = get_history_filename();
	if (strlen(history_file) == 0)
		return 0;

	if (fz_file_exists(ctx, history_file))
	{
		fz_try(ctx)
		{
			buf = fz_read_file(ctx, history_file);
			json = fz_string_from_buffer(ctx, buf);
		}
		fz_catch(ctx)
			;
	}

	js_getglobal(J, "JSON");
	js_getproperty(J, -1, "parse");
	js_pushnull(J);
	js_pushstring(J, json);
	if (js_pcall(J, 1))
	{
		fz_warn(ctx, "Can't parse history file: %s", js_trystring(J, -1, "error"));
		js_pop(J, 1);
		js_newobject(J);
	}
	else
	{
		js_rot2pop1(J);
	}

	fz_drop_buffer(ctx, buf);
	return 1;
}

static fz_location try_location(js_State *J)
{
	fz_location loc;
	if (js_isnumber(J, -1))
		loc = fz_make_location(0, js_tryinteger(J, -1, 1) - 1);
	else
	{
		js_getindex(J, -1, 0);
		loc.chapter = js_tryinteger(J, -1, 1) - 1;
		js_pop(J, 1);
		js_getindex(J, -1, 1);
		loc.page = js_tryinteger(J, -1, 1) - 1;
		js_pop(J, 1);
	}
	return loc;
}

static void push_location(js_State *J, fz_location loc)
{
	if (loc.chapter == 0)
		js_pushnumber(J, (double)loc.page+1);
	else
	{
		js_newarray(J);
		js_pushnumber(J, (double)loc.chapter+1);
		js_setindex(J, -2, 0);
		js_pushnumber(J, (double)loc.page+1);
		js_setindex(J, -2, 1);
	}
}

static void load_history(void)
{
	js_State *J;
	char absname[PATH_MAX];
	int i, n;

	if (!fz_realpath(filename, absname))
		return;

	J = js_newstate(NULL, NULL, 0);

	if (!read_history_file_as_json(J))
		return;

	if (js_hasproperty(J, -1, absname))
	{
		if (js_hasproperty(J, -1, "current"))
		{
			currentpage = try_location(J);
			js_pop(J, 1);
		}

		if (js_hasproperty(J, -1, "history"))
		{
			if (js_isarray(J, -1))
			{
				history_count = fz_clampi(js_getlength(J, -1), 0, nelem(history));
				for (i = 0; i < history_count; ++i)
				{
					js_getindex(J, -1, i);
					history[i].loc = try_location(J);
					js_pop(J, 1);
				}
			}
			js_pop(J, 1);
		}

		if (js_hasproperty(J, -1, "future"))
		{
			if (js_isarray(J, -1))
			{
				future_count = fz_clampi(js_getlength(J, -1), 0, nelem(future));
				for (i = 0; i < future_count; ++i)
				{
					js_getindex(J, -1, i);
					future[i].loc = try_location(J);
					js_pop(J, 1);
				}
			}
			js_pop(J, 1);
		}

		if (js_hasproperty(J, -1, "marks"))
		{
			if (js_isarray(J, -1))
			{
				n = fz_clampi(js_getlength(J, -1), 0, nelem(marks));
				for (i = 0; i < n; ++i)
				{
					js_getindex(J, -1, i);
					marks[i].loc = try_location(J);
					js_pop(J, 1);
				}
			}
			js_pop(J, 1);
		}
	}

	js_freestate(J);
}

static void save_history(void)
{
	js_State *J;
	char absname[PATH_MAX];
	fz_output *out = NULL;
	const char *json;
	int i;

	fz_var(out);

	if (!doc)
		return;

	if (!fz_realpath(filename, absname))
		return;

	J = js_newstate(NULL, NULL, 0);

	if (!read_history_file_as_json(J))
		return;

	js_newobject(J);
	{
		push_location(J, currentpage);
		js_setproperty(J, -2, "current");

		js_newarray(J);
		for (i = 0; i < history_count; ++i)
		{
			push_location(J, history[i].loc);
			js_setindex(J, -2, i);
		}
		js_setproperty(J, -2, "history");

		js_newarray(J);
		for (i = 0; i < future_count; ++i)
		{
			push_location(J, future[i].loc);
			js_setindex(J, -2, i);
		}
		js_setproperty(J, -2, "future");

		js_newarray(J);
		for (i = 0; i < (int)nelem(marks); ++i)
		{
			push_location(J, marks[i].loc);
			js_setindex(J, -2, i);
		}
		js_setproperty(J, -2, "marks");
	}
	js_setproperty(J, -2, absname);

	js_getglobal(J, "JSON");
	js_getproperty(J, -1, "stringify");
	js_pushnull(J);
	js_copy(J, -4);
	js_pushnull(J);
	js_pushnumber(J, 0);
	js_call(J, 3);
	js_rot2pop1(J);
	json = js_tostring(J, -1);

	fz_try(ctx)
	{
		const char *history_file = get_history_filename();
		if (strlen(history_file) > 0) {
			out = fz_new_output_with_path(ctx, history_file, 0);
			fz_write_string(ctx, out, json);
			fz_write_byte(ctx, out, '\n');
			fz_close_output(ctx, out);
		}
	}
	fz_always(ctx)
		fz_drop_output(ctx, out);
	fz_catch(ctx)
		fz_warn(ctx, "Can't write history file.");

	js_freestate(J);
}

static int
fz_mkdir(char *path)
{
#ifdef _WIN32
	int ret;
	wchar_t *wpath = fz_wchar_from_utf8(path);

	if (wpath == NULL)
		return -1;

	ret = _wmkdir(wpath);

	free(wpath);

	return ret;
#else
	return mkdir(path, S_IRWXU | S_IRWXG | S_IRWXO);
#endif
}

static int create_accel_path(char outname[], size_t len, int create, const char *absname, ...)
{
	va_list args;
	char *s = outname;
	size_t z, remain = len;
	char *arg;

	va_start(args, absname);

	while ((arg = va_arg(args, char *)) != NULL)
	{
		z = fz_snprintf(s, remain, "%s", arg);
		if (z+1 > remain)
			goto fail; /* won't fit */

		if (create)
			(void) fz_mkdir(outname);
		if (!fz_is_directory(ctx, outname))
			goto fail; /* directory creation failed, or that dir doesn't exist! */
#ifdef _WIN32
		s[z] = '\\';
#else
		s[z] = '/';
#endif
		s[z+1] = 0;
		s += z+1;
		remain -= z+1;
	}

	if (fz_snprintf(s, remain, "%s.accel", absname) >= remain)
		goto fail; /* won't fit */

	va_end(args);

	return 1;

fail:
	va_end(args);

	return 0;
}

static int convert_to_accel_path(char outname[], char *absname, size_t len, int create)
{
	char *tmpdir;
	char *s;

	if (absname[0] == '/' || absname[0] == '\\')
		++absname;

	s = absname;
	while (*s) {
		if (*s == '/' || *s == '\\' || *s == ':')
			*s = '%';
		++s;
	}

#ifdef _WIN32
	tmpdir = getenv("USERPROFILE");
	if (tmpdir && create_accel_path(outname, len, create, absname, tmpdir, ".config", "mupdf", NULL))
		return 1; /* OK! */
	/* TEMP and TMP are user-specific on modern windows. */
	tmpdir = getenv("TEMP");
	if (tmpdir && create_accel_path(outname, len, create, absname, tmpdir, "mupdf", NULL))
		return 1; /* OK! */
	tmpdir = getenv("TMP");
	if (tmpdir && create_accel_path(outname, len, create, absname, tmpdir, "mupdf", NULL))
		return 1; /* OK! */
#else
	tmpdir = getenv("XDG_CACHE_HOME");
	if (tmpdir && create_accel_path(outname, len, create, absname, tmpdir, "mupdf", NULL))
		return 1; /* OK! */
	tmpdir = getenv("HOME");
	if (tmpdir && create_accel_path(outname, len, create, absname, tmpdir, ".cache", "mupdf", NULL))
		return 1; /* OK! */
#endif
	return 0; /* Fail */
}

static int get_accelerator_filename(char outname[], size_t len, int create)
{
	char absname[PATH_MAX];
	if (!fz_realpath(filename, absname))
		return 0;
	if (!convert_to_accel_path(outname, absname, len, create))
		return 0;
	return 1;
}

static void save_accelerator(void)
{
	char absname[PATH_MAX];

	if (!doc)
		return;
	if (!fz_document_supports_accelerator(ctx, doc))
		return;
	if (!get_accelerator_filename(absname, sizeof(absname), 1))
		return;

	fz_save_accelerator(ctx, doc, absname);
}

static struct input search_input = { { 0 }, 0 };
static int search_dir = 1;
static fz_location search_page = {-1, -1};
static fz_location search_hit_page = {-1, -1};
static int search_active = 0;
char *search_needle = 0;
int search_hit_count = 0;
fz_quad search_hit_quads[5000];

static char *help_dialog_text =
	"The middle mouse button (scroll wheel button) pans the document view. "
	"The right mouse button selects a region and copies the marked text to the clipboard."
	"\n"
	"\n"
	"F1 - show this message\n"
	"` F12 - show javascript console\n"
	"i - show document information\n"
	"o - show document outline\n"
	"u - show undo history\n"
	"Y - show layer list\n"
	"a - show annotation editor\n"
	"R - show redaction editor\n"
	"L - highlight links\n"
	"F - highlight form fields\n"
	"r - reload file\n"
	"S - save file (only for PDF)\n"
	"q - quit\n"
	"\n"
	"< - decrease E-book font size\n"
	"> - increase E-book font size\n"
	"B - cycle between MediaBox, CropBox, ArtBox, etc.\n"
	"A - toggle anti-aliasing\n"
	"I - toggle inverted color mode\n"
	"C - toggle tinted color mode\n"
	"E - toggle ICC color management\n"
	"e - toggle spot color emulation\n"
	"\n"
	"f - fullscreen window\n"
	"w - shrink wrap window\n"
	"W - fit to width\n"
	"H - fit to height\n"
	"Z - fit to page\n"
	"z - reset zoom\n"
	"[number] z - set zoom resolution in DPI\n"
	"plus - zoom in\n"
	"minus - zoom out\n"
	"[ - rotate counter-clockwise\n"
	"] - rotate clockwise\n"
	"arrow keys - scroll in small increments\n"
	"h, j, k, l - scroll in small increments\n"
	"\n"
	"b - smart move backward\n"
	"space - smart move forward\n"
	"comma or page up - go backward\n"
	"period or page down - go forward\n"
	"g - go to first page\n"
	"G - go to last page\n"
	"[number] g - go to page number\n"
	"\n"
	"m - save current location in history\n"
	"t - go backward in history\n"
	"T - go forward in history\n"
	"[number] m - save current location in numbered bookmark\n"
	"[number] t - go to numbered bookmark\n"
	"\n"
	"/ - search for text forward\n"
	"? - search for text backward\n"
	"n - repeat search\n"
	"N - repeat search in reverse direction"
	;

static void help_dialog(void)
{
	static int scroll;
	ui_dialog_begin(ui.gridsize*20, ui.gridsize*40);
	ui_layout(T, X, W, ui.padsize, ui.padsize);
	ui_label("MuPDF %s", FZ_VERSION);
	ui_spacer();
	ui_layout(B, NONE, S, ui.padsize, ui.padsize);
	if (ui_button("Okay") || ui.key == KEY_ENTER || ui.key == KEY_ESCAPE)
		ui.dialog = NULL;
	ui_spacer();
	ui_layout(ALL, BOTH, CENTER, ui.padsize, ui.padsize);
	ui_label_with_scrollbar(help_dialog_text, 0, 0, &scroll, NULL);
	ui_dialog_end();
}

static fz_buffer *format_info_text();

static void info_dialog(void)
{
	static int scroll;
	fz_buffer *info_text;

	ui_dialog_begin(ui.gridsize*20, ui.gridsize*20);
	ui_layout(B, NONE, S, ui.padsize, ui.padsize);
	if (ui_button("Okay") || ui.key == KEY_ENTER || ui.key == KEY_ESCAPE)
		ui.dialog = NULL;
	ui_spacer();
	ui_layout(ALL, BOTH, CENTER, ui.padsize, ui.padsize);

	info_text = format_info_text();
	ui_label_with_scrollbar((char*)fz_string_from_buffer(ctx, info_text), 0, 0, &scroll, NULL);
	fz_drop_buffer(ctx, info_text);

	ui_dialog_end();
}

static char error_message[256];
static void error_dialog(void)
{
	ui_dialog_begin(ui.gridsize*20, (ui.gridsize+ui.padsize*2)*4);
	ui_layout(T, NONE, NW, ui.padsize, ui.padsize);
	ui_label("%C %s", 0x1f4a3, error_message); /* BOMB */
	ui_layout(B, NONE, S, ui.padsize, ui.padsize);
	if (ui_button("Quit") || ui.key == KEY_ENTER || ui.key == KEY_ESCAPE || ui.key == 'q')
		glutLeaveMainLoop();
	ui_dialog_end();
}
void ui_show_error_dialog(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	fz_vsnprintf(error_message, sizeof error_message, fmt, ap);
	va_end(ap);
	ui.dialog = error_dialog;
}

static char warning_message[256];
static void warning_dialog(void)
{
	ui_dialog_begin(ui.gridsize*20, (ui.gridsize+ui.padsize*2)*4);
	ui_layout(T, NONE, NW, ui.padsize, ui.padsize);
	ui_label("%C %s", 0x26a0, warning_message); /* WARNING SIGN */
	ui_layout(B, NONE, S, ui.padsize, ui.padsize);
	if (ui_button("Okay") || ui.key == KEY_ENTER || ui.key == KEY_ESCAPE)
		ui.dialog = NULL;
	ui_dialog_end();
}
void ui_show_warning_dialog(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	fz_vsnprintf(warning_message, sizeof warning_message, fmt, ap);
	va_end(ap);
	ui.dialog = warning_dialog;
}

static void quit_dialog(void)
{
	ui_dialog_begin(ui.gridsize*20, (ui.gridsize+ui.padsize*2)*3);
	ui_layout(T, NONE, NW, ui.padsize, ui.padsize);
	ui_label("%C The document has unsaved changes. Are you sure you want to quit?", 0x26a0); /* WARNING SIGN */
	ui_layout(B, X, S, ui.padsize, ui.padsize);
	ui_panel_begin(0, ui.gridsize, 0, 0, 0);
	{
		ui_layout(R, NONE, S, 0, 0);
		if (ui_button("Save"))
			do_save_pdf_file();
		ui_spacer();
		if (ui_button("Discard") || ui.key == 'q')
			glutLeaveMainLoop();
		ui_layout(L, NONE, S, 0, 0);
		if (ui_button("Cancel") || ui.key == KEY_ESCAPE)
			ui.dialog = NULL;
	}
	ui_panel_end();
	ui_dialog_end();
}

static void quit(void)
{
	if (pdf && pdf_has_unsaved_changes(ctx, pdf))
		ui.dialog = quit_dialog;
	else
		glutLeaveMainLoop();
}

static void reload_dialog(void)
{
	ui_dialog_begin(ui.gridsize*20, (ui.gridsize+ui.padsize*2)*3);
	ui_layout(T, NONE, NW, ui.padsize, ui.padsize);
	ui_label("%C The document has unsaved changes. Are you sure you want to reload?", 0x26a0); /* WARNING SIGN */
	ui_layout(B, X, S, ui.padsize, ui.padsize);
	ui_panel_begin(0, ui.gridsize, 0, 0, 0);
	{
		ui_layout(R, NONE, S, 0, 0);
		if (ui_button("Save"))
			do_save_pdf_file();
		ui_spacer();
		if (ui_button("Reload") || ui.key == 'q')
		{
			ui.dialog = NULL;
			reload_document();
		}
		ui_layout(L, NONE, S, 0, 0);
		if (ui_button("Cancel") || ui.key == KEY_ESCAPE)
			ui.dialog = NULL;
	}
	ui_panel_end();
	ui_dialog_end();
}

void reload(void)
{
	if (pdf && pdf_has_unsaved_changes(ctx, pdf))
		ui.dialog = reload_dialog;
	else
		reload_document();
}

void trace_action(const char *fmt, ...)
{
	va_list args;
	if (trace_file)
	{
		va_start(args, fmt);
		fz_write_vprintf(ctx, trace_file, fmt, args);
		fz_flush_output(ctx, trace_file);
		va_end(args);
		va_start(args, fmt);
		fz_write_vprintf(ctx, fz_stdout(ctx), fmt, args);
		fz_flush_output(ctx, fz_stdout(ctx));
		va_end(args);
	}
}

void trace_page_update(void)
{
	trace_action("page.update();\n");
}

void trace_save_snapshot(void)
{
	static int trace_idx = 1;
	trace_action("page.toPixmap(Matrix.identity, ColorSpace.DeviceRGB).saveAsPNG(\"trace-%03d.png\");\n", trace_idx++);
}

static int document_shown_as_dirty = 0;

void update_title(void)
{
	char buf[256];
	const char *title = "MuPDF/GL";
	char *extra = "";
	size_t n;

	int nc = fz_count_chapters(ctx, doc);

	title = fz_basename(filename);

	document_shown_as_dirty = pdf && pdf_has_unsaved_changes(ctx, pdf);
	if (document_shown_as_dirty)
		extra = "*";

	n = strlen(title);
	if (n > 50)
	{
		if (nc == 1)
			sprintf(buf, "...%s%s - %d/%d", title + n - 50, extra, currentpage.page + 1, fz_count_pages(ctx, doc));
		else
			sprintf(buf, "...%s%s - %d/%d - %d/%d", title + n - 50, extra,
				currentpage.chapter + 1, nc,
				currentpage.page + 1, fz_count_chapter_pages(ctx, doc, currentpage.chapter));
	}
	else
	{
		if (nc == 1)
			sprintf(buf, "%s%s - %d/%d", title, extra, currentpage.page + 1, fz_count_pages(ctx, doc));
		else

			sprintf(buf, "%s%s - %d/%d - %d/%d", title, extra,
				currentpage.chapter + 1, nc,
				currentpage.page + 1, fz_count_chapter_pages(ctx, doc, currentpage.chapter));
	}
	glutSetWindowTitle(buf);
	glutSetIconTitle(buf);
}

void transform_page(void)
{
	draw_page_ctm = fz_transform_page(page_bounds, currentzoom, currentrotate);
	draw_page_bounds = fz_transform_rect(page_bounds, draw_page_ctm);
}

static void clear_selected_annot(void)
{
	/* clear all editor selections */
	if (ui.selected_annot && pdf_annot_type(ctx, ui.selected_annot) == PDF_ANNOT_WIDGET)
		pdf_annot_event_blur(ctx, ui.selected_annot);
	ui_select_annot(NULL);
}

void load_page(void)
{
	fz_irect area;

	clear_selected_annot();

	if (trace_file)
		trace_action("page = doc.loadPage(%d);\n", fz_page_number_from_location(ctx, doc, currentpage));

	fz_drop_stext_page(ctx, page_text);
	page_text = NULL;
	fz_drop_separations(ctx, seps);
	seps = NULL;
	fz_drop_link(ctx, links);
	links = NULL;
	fz_drop_page(ctx, fzpage);
	fzpage = NULL;

	fzpage = fz_load_chapter_page(ctx, doc, currentpage.chapter, currentpage.page);
	if (pdf)
		page = (pdf_page*)fzpage;

	if (trace_file)
	{
		pdf_annot *w;
		int i, s;

		for (i = 0, s = 0, w = pdf_first_widget(ctx, page); w != NULL; i++, w = pdf_next_widget(ctx, w))
			if (pdf_widget_type(ctx, w) == PDF_WIDGET_TYPE_SIGNATURE)
			{
				int is_signed;

				s++;
				trace_action("widget = page.getWidgets()[%d];\n", i);
				trace_action("widgetstr = 'Signature %d on page %d';\n",
					s, fz_page_number_from_location(ctx, doc, currentpage));

				is_signed = pdf_widget_is_signed(ctx, w);
				trace_action("tmp = widget.isSigned();\n");
				trace_action("if (tmp != %d)\n", is_signed);
				trace_action("  throw new RegressionError(widgetstr, 'is signed:', tmp|0, 'expected:', %d);\n", is_signed);

				if (is_signed)
				{
					int valid_until, is_readonly;
					char *cert_error, *digest_error;
					pdf_pkcs7_distinguished_name *dn;
					pdf_pkcs7_verifier *verifier;
					char *signatory = NULL;
					char buf[500];

					valid_until = pdf_validate_signature(ctx, w);
					is_readonly = pdf_widget_is_readonly(ctx, w);
					verifier = pkcs7_openssl_new_verifier(ctx);
					cert_error = pdf_signature_error_description(pdf_check_widget_certificate(ctx, verifier, w));
					digest_error = pdf_signature_error_description(pdf_check_widget_digest(ctx, verifier, w));
					dn = pdf_signature_get_widget_signatory(ctx, verifier, w);
					if (dn)
					{
						char *s = pdf_signature_format_distinguished_name(ctx, dn);
						fz_strlcpy(buf, s, sizeof buf);
						fz_free(ctx, s);
						pdf_signature_drop_distinguished_name(ctx, dn);
					}
					else
					{
						fz_strlcpy(buf, "Signature information missing.", sizeof buf);
					}
					signatory = &buf[0];
					pdf_drop_verifier(ctx, verifier);

					trace_action("tmp = widget.validateSignature();\n");
					trace_action("if (tmp != %d)\n", valid_until);
					trace_action("  throw new RegressionError(widgetstr, 'valid until:', tmp, 'expected:', %d);\n", valid_until);
					trace_action("tmp = widget.isReadOnly();\n");
					trace_action("if (tmp != %d)\n", is_readonly);
					trace_action("  throw new RegressionError(widgetstr, 'is read-only:', tmp, 'expected:', %d);\n", is_readonly);
					trace_action("tmp = widget.checkCertificate();\n");
					trace_action("if (tmp != '%s')\n", cert_error);
					trace_action("  throw new RegressionError(widgetstr, 'is read-only:', tmp, 'expected:', %d);\n", cert_error);
					trace_action("tmp = widget.checkDigest();\n");
					trace_action("if (tmp != %q)\n", digest_error);
					trace_action("  throw new RegressionError(widgetstr, 'digest error:', tmp, 'expected:', %q);\n", digest_error);
					trace_action("tmp = widget.getSignatory();\n");
					trace_action("if (tmp != '%s')\n", signatory);
					trace_action("  throw new RegressionError(widgetstr, 'signatory:', '[', tmp, ']', 'expected:', '[', %q, ']');\n", signatory);
				}
			}
	}

	links = fz_load_links(ctx, fzpage);
	page_text = fz_new_stext_page_from_page(ctx, fzpage, NULL);

	if (currenticc)
		fz_enable_icc(ctx);
	else
		fz_disable_icc(ctx);

	if (currentseparations)
	{
		seps = fz_page_separations(ctx, fzpage);
		if (seps)
		{
			int i, n = fz_count_separations(ctx, seps);
			for (i = 0; i < n; i++)
				fz_set_separation_behavior(ctx, seps, i, FZ_SEPARATION_COMPOSITE);
		}
		else if (fz_page_uses_overprint(ctx, fzpage))
			seps = fz_new_separations(ctx, 0);
		else if (fz_document_output_intent(ctx, doc))
			seps = fz_new_separations(ctx, 0);
	}

	/* compute bounds here for initial window size */
	page_bounds = fz_bound_page_box(ctx, fzpage, currentbox);
	transform_page();

	area = fz_irect_from_rect(draw_page_bounds);
	page_tex.w = area.x1 - area.x0;
	page_tex.h = area.y1 - area.y0;

	page_contents_changed = 1;
}

static void render_page(void)
{
	fz_irect bbox;
	fz_pixmap *pix;
	fz_device *dev;

	page_bounds = fz_bound_page_box(ctx, fzpage, currentbox);
	transform_page();

	fz_set_aa_level(ctx, currentaa);

	if (page_contents_changed)
	{
		fz_drop_pixmap(ctx, page_contents);
		page_contents = NULL;

		bbox = fz_round_rect(fz_transform_rect(fz_bound_page_box(ctx, fzpage, currentbox), draw_page_ctm));
		page_contents = fz_new_pixmap_with_bbox(ctx, profile, bbox, seps, 0);
		fz_clear_pixmap(ctx, page_contents);

		dev = fz_new_draw_device(ctx, draw_page_ctm, page_contents);

		fz_try(ctx)
		{
			fz_run_page_contents(ctx, fzpage, dev, fz_identity, NULL);
			fz_close_device(ctx, dev);
		}
		fz_always(ctx)
			fz_drop_device(ctx, dev);
		fz_catch(ctx)
			fz_rethrow(ctx);
	}

	pix = fz_clone_pixmap_area_with_different_seps(ctx, page_contents, NULL, profile, NULL, fz_default_color_params, NULL);
	{
		dev = fz_new_draw_device(ctx, draw_page_ctm, pix);
		fz_try(ctx)
		{
			fz_run_page_annots(ctx, fzpage, dev, fz_identity, NULL);
			fz_run_page_widgets(ctx, fzpage, dev, fz_identity, NULL);
			fz_close_device(ctx, dev);
		}
		fz_always(ctx)
			fz_drop_device(ctx, dev);
		fz_catch(ctx)
			fz_rethrow(ctx);
	}

	if (currentinvert)
	{
		fz_invert_pixmap_luminance(ctx, pix);
		fz_gamma_pixmap(ctx, pix, 1 / 1.4f);
	}
	if (currenttint)
	{
		fz_tint_pixmap(ctx, pix, tint_black, tint_white);
	}

	ui_texture_from_pixmap(&page_tex, pix);

	fz_drop_pixmap(ctx, pix);

	FZ_LOG_DUMP_STORE(ctx, "Store state after page render:\n");
}

void render_page_if_changed(void)
{
	if (pdf)
	{
		if (pdf_update_page(ctx, page))
		{
			trace_page_update();
			page_annots_changed = 1;
		}
	}

	if (oldpage.chapter != currentpage.chapter ||
		oldpage.page != currentpage.page ||
		oldzoom != currentzoom ||
		oldrotate != currentrotate ||
		oldinvert != currentinvert ||
		oldtint != currenttint ||
		oldicc != currenticc ||
		oldseparations != currentseparations ||
		oldaa != currentaa ||
		oldbox != currentbox)
	{
		page_contents_changed = 1;
	}

	if (page_contents_changed || page_annots_changed)
	{
		render_page();
		oldpage = currentpage;
		oldzoom = currentzoom;
		oldrotate = currentrotate;
		oldinvert = currentinvert;
		oldtint = currenttint;
		oldicc = currenticc;
		oldseparations = currentseparations;
		oldaa = currentaa;
		oldbox = currentbox;
		page_contents_changed = 0;
		page_annots_changed = 0;
	}
}

static struct mark save_mark()
{
	struct mark mark;
	mark.loc = currentpage;
	mark.scroll = fz_transform_point_xy(scroll_x, scroll_y, view_page_inv_ctm);
	return mark;
}

static void restore_mark(struct mark mark)
{
	currentpage = mark.loc;
	mark.scroll = fz_transform_point(mark.scroll, draw_page_ctm);
	scroll_x = mark.scroll.x;
	scroll_y = mark.scroll.y;
}

static int eqloc(fz_location a, fz_location b)
{
	return a.chapter == b.chapter && a.page == b.page;
}

int search_has_results(void)
{
	return !search_active && eqloc(search_hit_page, currentpage) && search_hit_count > 0;
}

static int is_first_page(fz_location loc)
{
	return (loc.chapter == 0 && loc.page == 0);
}

static int is_last_page(fz_location loc)
{
	fz_location last = fz_last_page(ctx, doc);
	return (loc.chapter == last.chapter && loc.page == last.page);
}

static void push_history(void)
{
	if (history_count > 0 && eqloc(history[history_count-1].loc, currentpage))
		return;
	if (history_count + 1 >= (int)nelem(history))
	{
		memmove(history, history + 1, sizeof *history * (nelem(history) - 1));
		history[history_count] = save_mark();
	}
	else
	{
		history[history_count++] = save_mark();
	}
}

static void push_future(void)
{
	if (future_count + 1 >= (int)nelem(future))
	{
		memmove(future, future + 1, sizeof *future * (nelem(future) - 1));
		future[future_count] = save_mark();
	}
	else
	{
		future[future_count++] = save_mark();
	}
}

static void clear_future(void)
{
	future_count = 0;
}

static void jump_to_location(fz_location loc)
{
	clear_future();
	push_history();
	currentpage = fz_clamp_location(ctx, doc, loc);
	push_history();
}

static void jump_to_location_xy(fz_location loc, float x, float y)
{
	fz_point p = fz_transform_point_xy(x, y, draw_page_ctm);
	clear_future();
	push_history();
	currentpage = fz_clamp_location(ctx, doc, loc);
	scroll_x = p.x;
	scroll_y = p.y;
	push_history();
}

static void jump_to_page(int newpage)
{
	clear_future();
	push_history();
	currentpage = fz_location_from_page_number(ctx, doc, newpage);
	currentpage = fz_clamp_location(ctx, doc, currentpage);
	push_history();
}

static void jump_to_page_xy(int newpage, float x, float y)
{
	fz_point p = fz_transform_point_xy(x, y, draw_page_ctm);
	clear_future();
	push_history();
	currentpage = fz_location_from_page_number(ctx, doc, newpage);
	currentpage = fz_clamp_location(ctx, doc, currentpage);
	scroll_x = p.x;
	scroll_y = p.y;
	push_history();
}

static void pop_history(void)
{
	fz_location here = currentpage;
	push_future();
	while (history_count > 0 && eqloc(currentpage, here))
		restore_mark(history[--history_count]);
}

static void pop_future(void)
{
	fz_location here = currentpage;
	push_history();
	while (future_count > 0 && eqloc(currentpage, here))
		restore_mark(future[--future_count]);
	push_history();
}

static void relayout(void)
{
	if (layout_em < 6) layout_em = 6;
	if (layout_em > 36) layout_em = 36;
	if (fz_is_document_reflowable(ctx, doc))
	{
		fz_bookmark mark = fz_make_bookmark(ctx, doc, currentpage);
		fz_layout_document(ctx, doc, layout_w, layout_h, layout_em);
		currentpage = fz_lookup_bookmark(ctx, doc, mark);
		history_count = 0;
		future_count = 0;

		load_page();
		update_title();
	}
}

static int count_outline(fz_outline *node, int end)
{
	int is_selected, n, p, np;
	int count = 0;

	if (!node)
		return 0;
	np = fz_page_number_from_location(ctx, doc, node->page);

	do
	{
		p = np;
		count += 1;
		n = end;
		if (node->next && (np = fz_page_number_from_location(ctx, doc, node->next->page)) >= 0)
			n = fz_page_number_from_location(ctx, doc, node->next->page);
		is_selected = 0;
		if (fz_count_chapters(ctx, doc) == 1)
			is_selected = (p>=0) && (currentpage.page == p || (currentpage.page > p && currentpage.page < n));
		if (node->down && (node->is_open || is_selected))
			count += count_outline(node->down, end);
		node = node->next;
	}
	while (node);

	return count;
}

static void do_outline_imp(struct list *list, int end, fz_outline *node, int depth)
{
	int is_selected, was_open, n, np;

	if (!node)
		return;

	np = fz_page_number_from_location(ctx, doc, node->page);

	do
	{
		int p = np;
		n = end;
		if (node->next && (np = fz_page_number_from_location(ctx, doc, node->next->page)) >= 0)
			n = np;

		was_open = node->is_open;
		is_selected = 0;
		if (fz_count_chapters(ctx, doc) == 1)
			is_selected = (p>=0) && (currentpage.page == p || (currentpage.page > p && currentpage.page < n));
		if (ui_tree_item(list, node, node->title, is_selected, depth, !!node->down, &node->is_open))
		{
			if (p < 0)
			{
				currentpage = fz_resolve_link(ctx, doc, node->uri, &node->x, &node->y);
				jump_to_location_xy(currentpage, node->x, node->y);
			}
			else
			{
				jump_to_page_xy(p, node->x, node->y);
			}
		}

		if (node->down && (was_open || is_selected))
			do_outline_imp(list, n, node->down, depth + 1);
		node = node->next;
	}
	while (node);
}

static void do_outline(fz_outline *node)
{
	static struct list list;
	ui_layout(L, BOTH, NW, 0, 0);
	ui_tree_begin(&list, count_outline(node, 65535), outline_w, 0, 1);
	do_outline_imp(&list, 65535, node, 0);
	ui_tree_end(&list);
}

static void do_undo(void)
{
	static struct list list;
	int count = 0;
	int pos;
	int i;
	int desired = -1;

	if (pdf)
		pos = pdf_undoredo_state(ctx, pdf, &count);
	else
		pos = 0;
	ui_layout(L, BOTH, NW, 0, 0);
	ui_panel_begin(outline_w, 0, ui.padsize*2, ui.padsize*2, 1);
	ui_layout(T, X, NW, ui.padsize, ui.padsize);
	ui_label("Undo history:");

	ui_layout(B, X, NW, ui.padsize, ui.padsize);
	if (ui_button_aux("Redo", pos == count))
		desired = pos+1;
	if (ui_button_aux("Undo", pos == 0))
		desired = pos-1;

	ui_layout(ALL, BOTH, NW, ui.padsize, ui.padsize);
	ui_list_begin(&list, count+1, 0, ui.lineheight * 4 + 4);

	for (i = 0; i < count+1; i++)
	{
		const char *op;

		if (i == 0)
			op = "Original Document";
		else
			op = pdf_undoredo_step(ctx, pdf, i-1);
		if (ui_list_item(&list, (void *)(intptr_t)(i+1), op, i <= pos))
		{
			desired = i;
		}
	}

	ui_list_end(&list);

	if (desired != -1 && desired != pos)
	{
		page_contents_changed = 1;
		while (pos > desired)
		{
			trace_action("doc.undo();\n");
			pdf_undo(ctx, pdf);
			pos--;
		}
		while (pos < desired)
		{
			trace_action("doc.redo();\n");
			pdf_redo(ctx, pdf);
			pos++;
		}
		clear_selected_annot();
		load_page();
	}

	ui_panel_end();
}

static void do_layers(void)
{
	const char *name;
	int n, i, on;

	ui_layout(L, BOTH, NW, 0, 0);
	ui_panel_begin(outline_w, 0, ui.padsize*2, ui.padsize*2, 1);
	ui_layout(T, X, NW, ui.padsize, ui.padsize);
	ui_label("Layers:");
	ui_layout(T, X, NW, ui.padsize*2, ui.padsize);

	if (pdf)
	{
		n = pdf_count_layers(ctx, pdf);
		for (i = 0; i < n; ++i)
		{
			name = pdf_layer_name(ctx, pdf, i);
			on = pdf_layer_is_enabled(ctx, pdf, i);
			if (ui_checkbox(name, &on))
			{
				pdf_enable_layer(ctx, pdf, i, on);
				page_contents_changed = 1;
			}
		}
		if (n == 0)
			ui_label("None");
	}
	else
	{
		ui_label("None");
	}

	ui_panel_end();
}

static void do_links(fz_link *link)
{
	fz_rect bounds;
	fz_irect area;
	float link_x, link_y;

	glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_BLEND);

	tooltip = NULL;

	while (link)
	{
		bounds = link->rect;
		bounds = fz_transform_rect(link->rect, view_page_ctm);
		area = fz_irect_from_rect(bounds);

		if (ui_mouse_inside(area))
		{
			if (!tooltip)
				tooltip = link->uri;
			ui.hot = link;
			if (!ui.active && ui.down)
				ui.active = link;
		}

		if (ui.hot == link || showlinks)
		{
			if (ui.active == link && ui.hot == link)
				glColor4f(0, 0, 1, 0.4f);
			else if (ui.hot == link)
				glColor4f(0, 0, 1, 0.2f);
			else
				glColor4f(0, 0, 1, 0.1f);
			glRectf(area.x0, area.y0, area.x1, area.y1);
		}

		if (ui.active == link && !ui.down)
		{
			if (ui.hot == link)
			{
				if (fz_is_external_link(ctx, link->uri))
					open_browser(link->uri);
				else
				{
					fz_location loc = fz_resolve_link(ctx, doc, link->uri, &link_x, &link_y);
					jump_to_location_xy(loc, link_x, link_y);
				}
			}
		}

		link = link->next;
	}

	glDisable(GL_BLEND);
}

static void do_page_selection(void)
{
	static fz_point pt = { 0, 0 };
	static fz_quad hits[1000];
	fz_rect rect;
	int i, n;

	if (ui_mouse_inside(view_page_area))
	{
		ui.hot = &pt;
		if (!ui.active && ui.right)
		{
			ui.active = &pt;
			pt.x = ui.x;
			pt.y = ui.y;
		}
	}

	if (ui.active == &pt)
	{
		fz_point page_a = { pt.x, pt.y };
		fz_point page_b = { ui.x, ui.y };

		page_a = fz_transform_point(page_a, view_page_inv_ctm);
		page_b = fz_transform_point(page_b, view_page_inv_ctm);

		if (ui.mod == GLUT_ACTIVE_CTRL)
			fz_snap_selection(ctx, page_text, &page_a, &page_b, FZ_SELECT_WORDS);
		else if (ui.mod == GLUT_ACTIVE_CTRL + GLUT_ACTIVE_SHIFT)
			fz_snap_selection(ctx, page_text, &page_a, &page_b, FZ_SELECT_LINES);

		if (ui.mod == GLUT_ACTIVE_SHIFT)
		{
			rect = fz_make_rect(
					fz_min(page_a.x, page_b.x),
					fz_min(page_a.y, page_b.y),
					fz_max(page_a.x, page_b.x),
					fz_max(page_a.y, page_b.y));
			n = 1;
			hits[0] = fz_quad_from_rect(rect);
		}
		else
		{
			n = fz_highlight_selection(ctx, page_text, page_a, page_b, hits, nelem(hits));
		}

		glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
		glEnable(GL_BLEND);
		glColor4f(0.0, 0.1, 0.4, 0.3f);

		glBegin(GL_QUADS);
		for (i = 0; i < n; ++i)
		{
			fz_quad thit = fz_transform_quad(hits[i], view_page_ctm);
			glVertex2f(thit.ul.x, thit.ul.y);
			glVertex2f(thit.ur.x, thit.ur.y);
			glVertex2f(thit.lr.x, thit.lr.y);
			glVertex2f(thit.ll.x, thit.ll.y);
		}
		glEnd();

		glDisable(GL_BLEND);

		if (!ui.right)
		{
			char *s;
#ifdef _WIN32
			if (ui.mod == GLUT_ACTIVE_SHIFT)
				s = fz_copy_rectangle(ctx, page_text, rect, 1);
			else
				s = fz_copy_selection(ctx, page_text, page_a, page_b, 1);
#else
			if (ui.mod == GLUT_ACTIVE_SHIFT)
				s = fz_copy_rectangle(ctx, page_text, rect, 0);
			else
				s = fz_copy_selection(ctx, page_text, page_a, page_b, 0);
#endif
			ui_set_clipboard(s);
			fz_free(ctx, s);
		}
	}
}

static void do_search_hits(void)
{
	int i;

	glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_BLEND);

	glColor4f(1, 0, 0, 0.4f);
	glBegin(GL_QUADS);
	for (i = 0; i < search_hit_count; ++i)
	{
		fz_quad thit = fz_transform_quad(search_hit_quads[i], view_page_ctm);
		glVertex2f(thit.ul.x, thit.ul.y);
		glVertex2f(thit.ur.x, thit.ur.y);
		glVertex2f(thit.lr.x, thit.lr.y);
		glVertex2f(thit.ll.x, thit.ll.y);
	}

	glEnd();
	glDisable(GL_BLEND);
}

static void toggle_fullscreen(void)
{
	static int win_x = 0, win_y = 0;
	static int win_w = 100, win_h = 100;
	if (!isfullscreen)
	{
		win_w = glutGet(GLUT_WINDOW_WIDTH);
		win_h = glutGet(GLUT_WINDOW_HEIGHT);
		win_x = glutGet(GLUT_WINDOW_X);
		win_y = glutGet(GLUT_WINDOW_Y);
		glutFullScreen();
		isfullscreen = 1;
	}
	else
	{
		glutPositionWindow(win_x, win_y);
		glutReshapeWindow(win_w, win_h);
		isfullscreen = 0;
	}
}

static void shrinkwrap(void)
{
	int w = page_tex.w;
	int h = page_tex.h;
	if (showoutline || showundo || showlayers)
		w += outline_w + 4;
	if (showannotate)
		w += annotate_w;
	if (showconsole)
		h += console_h;
	if (screen_w > 0 && w > screen_w)
		w = screen_w;
	if (screen_h > 0 && h > screen_h)
		h = screen_h;
	if (isfullscreen)
		toggle_fullscreen();
	glutReshapeWindow(w, h);
}

static struct input input_password;
static void password_dialog(void)
{
	int is;
	ui_dialog_begin(ui.gridsize*16, (ui.gridsize+ui.padsize*2)*3);
	{
		ui_layout(T, X, NW, ui.padsize, ui.padsize);
		ui_label("Password:");
		is = ui_input(&input_password, 200, 1);

		ui_layout(B, X, NW, ui.padsize, ui.padsize);
		ui_panel_begin(0, ui.gridsize, 0, 0, 0);
		{
			ui_layout(R, NONE, S, 0, 0);
			if (ui_button("Cancel") || (!ui.focus && ui.key == KEY_ESCAPE))
				glutLeaveMainLoop();
			ui_spacer();
			if (ui_button("Okay") || is == UI_INPUT_ACCEPT)
			{
				password = input_password.text;
				ui.dialog = NULL;
				reload_document();
				shrinkwrap();
			}
		}
		ui_panel_end();
	}
	ui_dialog_end();
}

/* Parse "chapter:page" from anchor. "chapter:" is also accepted,
 * meaning first page. Return 1 if parsing succeeded, 0 if failed.
 */
static int
parse_location(const char *anc, fz_location *loc)
{
	const char *s, *p;

	if (anc == NULL)
		return 0;

	s = anc;
	while (*s >= '0' && *s <= '9')
		s++;
	loc->chapter = fz_atoi(anc)-1;
	if (*s == 0)
	{
		*loc = fz_location_from_page_number(ctx, doc, loc->chapter);
		return 1;
	}
	if (*s != ':')
		return 0;
	p = ++s;
	while (*s >= '0' && *s <= '9')
		s++;
	if (s == p)
		loc->page = 0;
	else
		loc->page = fz_atoi(p)-1;

	return 1;
}

static void
reload_or_start_journalling(void)
{
	char journal[PATH_MAX];

	fz_strlcpy(journal, filename, sizeof(journal));
	fz_strlcat(journal, ".journal", sizeof(journal));

	fz_try(ctx)
	{
		/* Probe with fz_file_exists to avoid 'can't find' errors. */
		if (fz_file_exists(ctx, journal))
			pdf_load_journal(ctx, pdf, journal);
	}
	fz_catch(ctx)
	{
		/* Ignore any failures here. */
	}
	trace_action("doc.enableJournal();\n");
	pdf_enable_journal(ctx, pdf);
}

static void alert_box(const char *fmt, const char *str)
{
#ifdef _WIN32
	MessageBoxA(NULL, str, "MuPDF Alert", MB_ICONERROR);
#else
	fprintf(stderr, "MuPDF Alert: %s\n", str);
#endif
}


static void event_cb(fz_context *callback_ctx, pdf_document *callback_doc, pdf_doc_event *evt, void *data)
{
	switch (evt->type)
	{
	case PDF_DOCUMENT_EVENT_ALERT:
		{
			pdf_alert_event *alert = pdf_access_alert_event(callback_ctx, evt);
			alert_box("%s", alert->message);
		}
		break;

	default:
		fz_throw(callback_ctx, FZ_ERROR_GENERIC, "event not yet implemented");
		break;
	}
}

static void load_document(void)
{
	char accelpath[PATH_MAX];
	char *accel = NULL;
	time_t atime;
	time_t dtime;
	fz_location location;

	fz_drop_outline(ctx, outline);
	outline = NULL;
	fz_drop_document(ctx, doc);
	doc = NULL;

	if (!strncmp(filename, "file://", 7))
	{
		anchor = strchr(filename + 7, '#');
		if (anchor)
		{
			memmove(anchor + 1, anchor, strlen(anchor) + 1);
			*anchor = 0;
			anchor++;
		}
		memmove(filename, filename + 7, strlen(filename));
	}

	/* If there was an accelerator to load, what would it be called? */
	if (get_accelerator_filename(accelpath, sizeof(accelpath), 0))
	{
		/* Check whether that file exists, and isn't older than
		 * the document. */
		atime = fz_stat_mtime(accelpath);
		dtime = fz_stat_mtime(filename);
		if (atime == 0)
		{
			/* No accelerator */
		}
		else if (atime > dtime)
			accel = accelpath;
		else
		{
			/* Accelerator data is out of date */
#ifdef _WIN32
			fz_remove_utf8(accelpath);
#else
			remove(accelpath);
#endif
			accel = NULL; /* In case we have jumped up from below */
		}
	}

	trace_action("doc = Document.openDocument(%q);\n", filename);

	doc = fz_open_accelerated_document(ctx, filename, accel);
	pdf = pdf_specifics(ctx, doc);

	if (pdf && trace_file)
	{
		int needspass = pdf_needs_password(ctx, pdf);
		trace_action(
				"tmp = doc.needsPassword();\n"
				"if (tmp != %s)\n"
				"  throw new RegressionError('Document password needed:', tmp, 'expected:', %s);\n",
				needspass ? "true" : "false",
				needspass ? "true" : "false");
	}

	if (fz_needs_password(ctx, doc))
	{
		int result = fz_authenticate_password(ctx, doc, password);

		if (pdf && trace_file)
		{
			trace_action(
					"tmp = doc.authenticatePassword(%q);\n"
					"if (tmp != %s)\n"
					"  throw new RegressionError('Open document with password %q result: %s', 'expected:', '%s');\n",
					password,
					result ? "true" : "false",
					password,
					!result ? "pass" : "fail",
					result ? "pass" : "fail");
		}

		if (!result)
		{
			fz_drop_document(ctx, doc);
			doc = NULL;
			ui_input_init(&input_password, "");
			ui.focus = &input_password;
			ui.dialog = password_dialog;
			return;
		}
	}

	fz_layout_document(ctx, doc, layout_w, layout_h, layout_em);

	fz_try(ctx)
		outline = fz_load_outline(ctx, doc);
	fz_catch(ctx)
	{
		fz_report_error(ctx);
		outline = NULL;
	}

	load_history();

	if (pdf)
	{
		if (enable_js)
		{
			trace_action("doc.enableJS();\n");
			pdf_enable_js(ctx, pdf);
			pdf_js_set_console(ctx, pdf, &gl_js_console, NULL);
		}

		reload_or_start_journalling();

		if (trace_file)
		{
			int vsns = pdf_count_versions(ctx, pdf);
			trace_action(
				"tmp = doc.countVersions();\n"
				"if (tmp != %d)\n"
				"  throw new RegressionError('Document versions:', tmp, 'expected:', %d);\n",
				vsns, vsns);
			if (vsns > 1)
			{
				int valid = pdf_validate_change_history(ctx, pdf);
				trace_action("tmp = doc.validateChangeHistory();\n");
				trace_action("if (tmp != %d)\n", valid);
				trace_action("  throw new RegressionError('History validation:', tmp, 'expected:', %d);\n", valid);
			}
		}
	}

	if (anchor)
	{
		if (parse_location(anchor, &location))
			jump_to_location(location);
		else
		{
			location = fz_resolve_link(ctx, doc, anchor, NULL, NULL);
			if (location.page < 0)
				fz_warn(ctx, "cannot find location: %s", anchor);
			else
				jump_to_location(location);
		}
	}
	anchor = NULL;

	oldpage = currentpage = fz_clamp_location(ctx, doc, currentpage);

	if (pdf)
		pdf_set_doc_event_callback(ctx, pdf, event_cb, NULL, NULL);
}

static void reflow_document(void)
{
	char buf[256];
	fz_document *new_doc;
	fz_stext_options opts;

	if (fz_is_document_reflowable(ctx, doc))
		return;

	fz_drop_outline(ctx, outline);
	outline = NULL;

	fz_parse_stext_options(ctx, &opts, reflow_options);

	new_doc = fz_open_reflowed_document(ctx, doc, &opts);
	fz_drop_document(ctx, doc);
	doc = new_doc;
	pdf = NULL;
	page = NULL;

	fz_layout_document(ctx, doc, layout_w, layout_h, layout_em);

	fz_try(ctx)
		outline = fz_load_outline(ctx, doc);
	fz_catch(ctx)
		outline = NULL;

	fz_strlcpy(buf, filename, sizeof buf);
	fz_snprintf(filename, sizeof filename, "%s.xhtml", buf);

	load_history();

	if (anchor)
		jump_to_page(fz_atoi(anchor) - 1);
	anchor = NULL;

	currentpage = fz_clamp_location(ctx, doc, currentpage);
}

void reload_document(void)
{
	save_history();
	save_accelerator();
	load_document();
	if (doc)
	{
		if (reflow_options)
			reflow_document();
		load_page();
		update_title();
	}
}

static void toggle_outline(void)
{
	if (outline)
	{
		showoutline = !showoutline;
		showundo = showlayers = 0;
		if (canvas_w == page_tex.w && canvas_h == page_tex.h)
			shrinkwrap();
	}
}

static void toggle_undo(void)
{
	showundo = !showundo;
	showoutline = showlayers = 0;
	if (canvas_w == page_tex.w && canvas_h == page_tex.h)
		shrinkwrap();
}

static void toggle_layers(void)
{
	showlayers = !showlayers;
	showoutline = showundo = 0;
	if (canvas_w == page_tex.w && canvas_h == page_tex.h)
		shrinkwrap();
}

void toggle_annotate(int mode)
{
	if (pdf)
	{
		if (showannotate != mode)
			showannotate = mode;
		else
			showannotate = ANNOTATE_MODE_NONE;
		if (canvas_w == page_tex.w && canvas_h == page_tex.h)
			shrinkwrap();
	}
}

static void set_zoom(int z, int cx, int cy)
{
	z = fz_clamp(z, MINRES, MAXRES);
	scroll_x = (scroll_x + cx - canvas_x) * z / currentzoom - cx + canvas_x;
	scroll_y = (scroll_y + cy - canvas_y) * z / currentzoom - cy + canvas_y;
	currentzoom = z;
}

static void auto_zoom_w(void)
{
	currentzoom = fz_clamp(currentzoom * canvas_w / page_tex.w, MINRES, MAXRES);
}

static void auto_zoom_h(void)
{
	currentzoom = fz_clamp(currentzoom * canvas_h / page_tex.h, MINRES, MAXRES);
}

static void auto_zoom(void)
{
	float page_a = (float) page_tex.w / page_tex.h;
	float screen_a = (float) canvas_w / canvas_h;
	if (page_a > screen_a)
		auto_zoom_w();
	else
		auto_zoom_h();
}

static void smart_move_backward(void)
{
	int slop_x = page_tex.w / 20;
	int slop_y = page_tex.h / 20;
	if (scroll_y <= slop_y)
	{
		if (scroll_x <= slop_x)
		{
			fz_location prev = fz_previous_page(ctx, doc, currentpage);
			if (!eqloc(currentpage, prev))
			{
				scroll_x = (page_tex.w <= canvas_w) ? 0 : page_tex.w - canvas_w;
				scroll_y = (page_tex.h <= canvas_h) ? 0 : page_tex.h - canvas_h;
				currentpage = prev;
			}
		}
		else
		{
			scroll_y = page_tex.h;
			scroll_x -= canvas_w * 9 / 10;
		}
	}
	else
	{
		scroll_y -= canvas_h * 9 / 10;
	}
}

static void smart_move_forward(void)
{
	int slop_x = page_tex.w / 20;
	int slop_y = page_tex.h / 20;
	if (scroll_y + canvas_h >= page_tex.h - slop_y)
	{
		if (scroll_x + canvas_w >= page_tex.w - slop_x)
		{
			fz_location next = fz_next_page(ctx, doc, currentpage);
			if (!eqloc(currentpage, next))
			{
				scroll_x = 0;
				scroll_y = 0;
				currentpage = next;
			}
		}
		else
		{
			scroll_y = 0;
			scroll_x += canvas_w * 9 / 10;
		}
	}
	else
	{
		scroll_y += canvas_h * 9 / 10;
	}
}

static void clear_search(void)
{
	showsearch = 0;
	search_page = currentpage;
	search_hit_page = fz_make_location(-1, -1);
	search_hit_count = 0;
}

#define MAX_CONSOLE_LINES 500

static fz_buffer *console_buffer;
static int console_scroll = 0;
static int console_sticky = 1;
static int console_lines = 0;
static struct readline console_readline;
static void (*warning_callback)(void *, const char *) = NULL;
static void (*error_callback)(void *, const char *) = NULL;
static void *warning_user = NULL;
static void *error_user = NULL;

static void
remove_oldest_console_line()
{
	unsigned char *s;
	size_t size = fz_buffer_storage(ctx, console_buffer, &s);
	unsigned char *p = s;
	unsigned char *e = s + size;

	while (p < e && *p != '\n')
		p++;

	if (p < e && *p == '\n')
	{
		p++;
		memmove(s, p, e - p);
		fz_resize_buffer(ctx, console_buffer, e - p);
		console_lines--;
	}
}

static void
gl_js_console_write(void *user, const char *message)
{
	const char *p = NULL;

	if (message == NULL)
		return;

	p = message;
	while (*p)
	{
		if (*p == '\n')
			console_lines++;
		if (console_lines >= MAX_CONSOLE_LINES)
			remove_oldest_console_line();
		if (*p)
			fz_append_byte(ctx, console_buffer, *p);
		p++;
	}
}

static void
gl_js_console_show(void *user)
{
	if (showconsole)
		return;

	showconsole = 1;
	if (canvas_w == page_tex.w && canvas_h == page_tex.h)
		shrinkwrap();
	ui.focus = &console_readline;
}

static void
gl_js_console_hide(void *user)
{
	if (!showconsole)
		return;

	showconsole = 0;
	if (canvas_w == page_tex.w && canvas_h == page_tex.h)
		shrinkwrap();
	ui.focus = NULL;
}

static void
gl_js_console_clear(void *user)
{
	fz_resize_buffer(ctx, console_buffer, 0);
	console_lines = 0;
}

static void console_warn(void *user, const char *message)
{
	gl_js_console_write(ctx, "\nwarning: ");
	gl_js_console_write(ctx, message);
	if (warning_callback)
		warning_callback(warning_user, message);
}

static void console_err(void *user, const char *message)
{
	gl_js_console_write(ctx, "\nerror: ");
	gl_js_console_write(ctx, message);
	if (error_callback)
		error_callback(error_user, message);
}

static void console_init(void)
{
	ui_readline_init(&console_readline, NULL);

	console_buffer = fz_new_buffer(ctx, 0);
	fz_append_printf(ctx, console_buffer, "Welcome to MuPDF %s with MuJS %d.%d.%d",
		FZ_VERSION,
		JS_VERSION_MAJOR, JS_VERSION_MINOR, JS_VERSION_PATCH);

	warning_callback = fz_warning_callback(ctx, &warning_user);
	fz_set_warning_callback(ctx, console_warn, NULL);
	error_callback = fz_error_callback(ctx, &error_user);
	fz_set_error_callback(ctx, console_err, NULL);
}

static void console_fin(void)
{
	fz_set_warning_callback(ctx, warning_callback, warning_user);
	fz_set_error_callback(ctx, error_callback, error_user);
	fz_drop_buffer(ctx, console_buffer);
	console_buffer = NULL;
}

static pdf_js_console gl_js_console = {
	NULL,
	gl_js_console_show,
	gl_js_console_hide,
	gl_js_console_clear,
	gl_js_console_write,
};

static void toggle_console(void)
{
	showconsole = !showconsole;
	if (showconsole)
		ui.focus = &console_readline;
	if (canvas_w == page_tex.w && canvas_h == page_tex.h)
		shrinkwrap();
}

void do_console(void)
{
	pdf_js_console *console = pdf_js_get_console(ctx, pdf);
	char *result = NULL;
	const char *accepted = NULL;

	fz_var(result);

	ui_layout(B, BOTH, NW, 0, 0);
	ui_panel_begin(canvas_w, console_h, ui.padsize, ui.padsize, 1);

	ui_layout(B, X, NW, 0, 0);

	accepted = ui_readline(&console_readline, 0);
	if (accepted != NULL)
	{
		ui.focus = &console_readline;
		if (console_readline.input.text[0])
		{
			fz_try(ctx)
			{
				if (console && console->write)
				{
					console->write(ctx, "\n> ");
					console->write(ctx, console_readline.input.text);
				}
				pdf_js_execute(pdf ? pdf->js : NULL, "console", console_readline.input.text, &result);
				if (result && console && console->write)
				{
					console->write(ctx, "\n");
					console->write(ctx, result);
				}
			}
			fz_always(ctx)
				fz_free(ctx, result);
			fz_catch(ctx)
			{
				if (console)
				{
					console->write(ctx, "\nError: ");
					console->write(ctx, fz_caught_message(ctx));
					fz_report_error(ctx);
				}
			}
			fz_flush_warnings(ctx);
			ui_input_init(&console_readline.input, "");
		}
	}

	ui_layout(ALL, BOTH, NW, ui.padsize, ui.padsize);

	// White background!
	glColorHex(0xF5F5F5);
	glRectf(ui.cavity->x0, ui.cavity->y0, ui.cavity->x1, ui.cavity->y1);

	char *console_string = (char *) fz_string_from_buffer(ctx, console_buffer);
	ui_label_with_scrollbar(console_string, 0, 10, &console_scroll, &console_sticky);

	ui_panel_end();
}

static void do_app(void)
{
	if (ui.mod == GLUT_ACTIVE_ALT)
	{
		if (ui.key == KEY_F4)
			quit();

		if (ui.key == KEY_LEFT)
			ui.key = 't', ui.mod = 0, ui.plain = 1;
		if (ui.key == KEY_RIGHT)
			ui.key = 'T', ui.mod = 0, ui.plain = 1;
	}

	if (trace_file && ui.key == KEY_CTL_P)
		trace_save_snapshot();

	if (!ui.focus && ui.key && ui.plain)
	{
		switch (ui.key)
		{
		case KEY_ESCAPE: clear_search(); ui_select_annot(NULL); break;
		case KEY_F1: ui.dialog = help_dialog; break;
		case 'a': toggle_annotate(ANNOTATE_MODE_NORMAL); break;
		case 'R': toggle_annotate(ANNOTATE_MODE_REDACT); break;
		case 'o': toggle_outline(); break;
		case 'u': toggle_undo(); break;
		case 'Y': toggle_layers(); break;
		case 'L': showlinks = !showlinks; break;
		case 'F': showform = !showform; break;
		case 'i': ui.dialog = info_dialog; break;
		case '`': case KEY_F12: toggle_console(); break;
		case 'r': reload(); break;
		case 'q': quit(); break;
		case 'S': do_save_pdf_file(); break;

		case '>': layout_em = number > 0 ? number : layout_em + 1; relayout(); break;
		case '<': layout_em = number > 0 ? number : layout_em - 1; relayout(); break;

		case 'C': currenttint = !currenttint; break;
		case 'I': currentinvert = !currentinvert; break;
		case 'e': currentseparations = !currentseparations; break;
		case 'E': currenticc = !currenticc; break;
		case 'f': toggle_fullscreen(); break;
		case 'w': shrinkwrap(); break;
		case 'W': auto_zoom_w(); break;
		case 'H': auto_zoom_h(); break;
		case 'Z': auto_zoom(); break;
		case 'z': set_zoom(number > 0 ? number : DEFRES, canvas_w/2, canvas_h/2); break;
		case '+': set_zoom(zoom_in(currentzoom), ui.x, ui.y); break;
		case '-': set_zoom(zoom_out(currentzoom), ui.x, ui.y); break;
		case '[': currentrotate -= 90; break;
		case ']': currentrotate += 90; break;
		case 'k': case KEY_UP: scroll_y -= canvas_h/10; break;
		case 'j': case KEY_DOWN: scroll_y += canvas_h/10; break;
		case 'h': case KEY_LEFT: scroll_x -= canvas_w/10; break;
		case 'l': case KEY_RIGHT: scroll_x += canvas_w/10; break;

		case 'b': number = fz_maxi(number, 1); while (number--) smart_move_backward(); break;
		case ' ': number = fz_maxi(number, 1); while (number--) smart_move_forward(); break;
		case 'g': jump_to_page(number - 1); break;
		case 'G': jump_to_location(fz_last_page(ctx, doc)); break;

		case ',': case KEY_PAGE_UP:
			number = fz_maxi(number, 1);
			while (number--)
				currentpage = fz_previous_page(ctx, doc, currentpage);
			break;
		case '.': case KEY_PAGE_DOWN:
			number = fz_maxi(number, 1);
			while (number--)
				currentpage = fz_next_page(ctx, doc, currentpage);
			break;

		case 'A':
			if (number == 0)
				currentaa = (currentaa == 8 ? 0 : 8);
			else
				currentaa = number;
			break;

		case 'B':
			currentbox += 1;
			if (currentbox >= FZ_UNKNOWN_BOX)
				currentbox = FZ_MEDIA_BOX;
			break;

		case 'm':
			if (number == 0)
				push_history();
			else if (number > 0 && number < (int)nelem(marks))
				marks[number] = save_mark();
			break;
		case 't':
			if (number == 0)
			{
				if (history_count > 0)
					pop_history();
			}
			else if (number > 0 && number < (int)nelem(marks))
			{
				struct mark mark = marks[number];
				restore_mark(mark);
				jump_to_location(mark.loc);
			}
			break;
		case 'T':
			if (number == 0)
			{
				if (future_count > 0)
					pop_future();
			}
			break;

		case '/':
			clear_search();
			search_dir = 1;
			showsearch = 1;
			ui.focus = &search_input;
			search_input.p = search_input.text;
			search_input.q = search_input.end;
			break;
		case '?':
			clear_search();
			search_dir = -1;
			showsearch = 1;
			ui.focus = &search_input;
			search_input.p = search_input.text;
			search_input.q = search_input.end;
			break;
		case 'N':
			search_dir = -1;
			search_active = !!search_needle;
			if (eqloc(search_hit_page, currentpage))
			{
				if (is_first_page(search_page))
					search_active = 0;
				else
					search_page = fz_previous_page(ctx, doc, currentpage);
			}
			else
			{
				search_page = currentpage;
			}
			search_hit_page = fz_make_location(-1, -1);
			break;
		case 'n':
			search_dir = 1;
			search_active = !!search_needle;
			if (eqloc(search_hit_page, currentpage))
			{
				if (is_last_page(search_page))
					search_active = 0;
				else
					search_page = fz_next_page(ctx, doc, currentpage);
			}
			else
			{
				search_page = currentpage;
			}
			search_hit_page = fz_make_location(-1, -1);
			break;
		default:
			if (ui.key < '0' || ui.key > '9')
			{
				number = 0;
				return; /* unrecognized key, pass it through */
			}
		}

		if (ui.key >= '0' && ui.key <= '9')
			number = number * 10 + ui.key - '0';
		else
			number = 0;

		currentpage = fz_clamp_location(ctx, doc, currentpage);
		while (currentrotate < 0) currentrotate += 360;
		while (currentrotate >= 360) currentrotate -= 360;

		if (!eqloc(search_hit_page, currentpage))
			search_hit_page = fz_make_location(-1, -1); /* clear highlights when navigating */

		ui.key = 0; /* we ate the key event, so zap it */
	}
}

typedef struct
{
	int max;
	int len;
	pdf_obj **sig;
} sigs_list;

static void
process_sigs(fz_context *ctx_, pdf_obj *field, void *arg, pdf_obj **ft)
{
	sigs_list *sigs = (sigs_list *)arg;

	if (!pdf_name_eq(ctx, pdf_dict_get(ctx, field, PDF_NAME(Type)), PDF_NAME(Annot)) ||
		!pdf_name_eq(ctx, pdf_dict_get(ctx, field, PDF_NAME(Subtype)), PDF_NAME(Widget)) ||
		!pdf_name_eq(ctx, pdf_dict_get(ctx, field, ft[0]), PDF_NAME(Sig)))
		return;

	if (sigs->len == sigs->max)
	{
		int newsize = sigs->max * 2;
		if (newsize == 0)
			newsize = 4;
		sigs->sig = fz_realloc_array(ctx, sigs->sig, newsize, pdf_obj *);
		sigs->max = newsize;
	}

	sigs->sig[sigs->len++] = field;
}

static char *short_signature_error_desc(pdf_signature_error err)
{
	switch (err)
	{
	case PDF_SIGNATURE_ERROR_OKAY:
		return "OK";
	case PDF_SIGNATURE_ERROR_NO_SIGNATURES:
		return "No signatures";
	case PDF_SIGNATURE_ERROR_NO_CERTIFICATE:
		return "No certificate";
	case PDF_SIGNATURE_ERROR_DIGEST_FAILURE:
		return "Invalid";
	case PDF_SIGNATURE_ERROR_SELF_SIGNED:
		return "Self-signed";
	case PDF_SIGNATURE_ERROR_SELF_SIGNED_IN_CHAIN:
		return "Self-signed in chain";
	case PDF_SIGNATURE_ERROR_NOT_TRUSTED:
		return "Untrusted";
	default:
	case PDF_SIGNATURE_ERROR_UNKNOWN:
		return "Unknown error";
	}
}

const char *format_date(int64_t secs)
{
	static char buf[100];
#ifdef _POSIX_SOURCE
	struct tm tmbuf, *tm;
#else
	struct tm *tm;
#endif

	if (secs <= 0)
		return NULL;

#ifdef _POSIX_SOURCE
	tm = gmtime_r(&secs, &tmbuf);
#else
	tm = gmtime(&secs);
#endif
	if (!tm)
		return NULL;

	strftime(buf, sizeof buf, "%Y-%m-%d %H:%M UTC", tm);
	return buf;
}

static fz_buffer *format_info_text()
{
	fz_buffer *out = fz_new_buffer(ctx, 4096);
	pdf_document *pdoc = pdf_specifics(ctx, doc);
	sigs_list list = { 0, 0, NULL };
	char buf[100];

	if (pdoc)
	{
		static pdf_obj *ft_list[2] = { PDF_NAME(FT), NULL };
		pdf_obj *ft;
		pdf_obj *form_fields = pdf_dict_getp(ctx, pdf_trailer(ctx, pdoc), "Root/AcroForm/Fields");
		pdf_walk_tree(ctx, form_fields, PDF_NAME(Kids), process_sigs, NULL, &list, &ft_list[0], &ft);
	}

	fz_append_printf(ctx, out, "File: %s\n\n", filename);

	if (fz_lookup_metadata(ctx, doc, FZ_META_INFO_TITLE, buf, sizeof buf) > 0)
		fz_append_printf(ctx, out, "Title: %s\n", buf);
	if (fz_lookup_metadata(ctx, doc, FZ_META_INFO_AUTHOR, buf, sizeof buf) > 0)
		fz_append_printf(ctx, out, "Author: %s\n", buf);
	if (fz_lookup_metadata(ctx, doc, FZ_META_FORMAT, buf, sizeof buf) > 0)
		fz_append_printf(ctx, out, "Format: %s\n", buf);
	if (fz_lookup_metadata(ctx, doc, FZ_META_ENCRYPTION, buf, sizeof buf) > 0)
		fz_append_printf(ctx, out, "Encryption: %s\n", buf);

	fz_append_string(ctx, out, "\n");

	if (pdoc)
	{
		int updates = pdf_count_versions(ctx, pdoc);

		if (fz_lookup_metadata(ctx, doc, FZ_META_INFO_CREATOR, buf, sizeof buf) > 0)
			fz_append_printf(ctx, out, "PDF Creator: %s\n", buf);
		if (fz_lookup_metadata(ctx, doc, FZ_META_INFO_PRODUCER, buf, sizeof buf) > 0)
			fz_append_printf(ctx, out, "PDF Producer: %s\n", buf);
		if (fz_lookup_metadata(ctx, doc, FZ_META_INFO_SUBJECT, buf, sizeof buf) > 0)
			fz_append_printf(ctx, out, "Subject: %s\n", buf);
		if (fz_lookup_metadata(ctx, doc, FZ_META_INFO_KEYWORDS, buf, sizeof buf) > 0)
			fz_append_printf(ctx, out, "Keywords: %s\n", buf);
		if (fz_lookup_metadata(ctx, doc, FZ_META_INFO_CREATIONDATE, buf, sizeof buf) > 0)
		{
			const char *s = format_date(pdf_parse_date(ctx, buf));
			if (s)
				fz_append_printf(ctx, out, "Creation date: %s\n", s);
		}
		if (fz_lookup_metadata(ctx, doc, FZ_META_INFO_MODIFICATIONDATE, buf, sizeof buf) > 0)
		{
			const char *s = format_date(pdf_parse_date(ctx, buf));
			if (s)
				fz_append_printf(ctx, out, "Modification date: %s\n", s);
		}

		buf[0] = 0;
		if (fz_has_permission(ctx, doc, FZ_PERMISSION_PRINT))
			fz_strlcat(buf, "print, ", sizeof buf);
		if (fz_has_permission(ctx, doc, FZ_PERMISSION_COPY))
			fz_strlcat(buf, "copy, ", sizeof buf);
		if (fz_has_permission(ctx, doc, FZ_PERMISSION_EDIT))
			fz_strlcat(buf, "edit, ", sizeof buf);
		if (fz_has_permission(ctx, doc, FZ_PERMISSION_ANNOTATE))
			fz_strlcat(buf, "annotate, ", sizeof buf);
		if (fz_has_permission(ctx, doc, FZ_PERMISSION_FORM))
			fz_strlcat(buf, "form, ", sizeof buf);
		if (fz_has_permission(ctx, doc, FZ_PERMISSION_ACCESSIBILITY))
			fz_strlcat(buf, "accessibility, ", sizeof buf);
		if (fz_has_permission(ctx, doc, FZ_PERMISSION_ASSEMBLE))
			fz_strlcat(buf, "assemble, ", sizeof buf);
		if (fz_has_permission(ctx, doc, FZ_PERMISSION_PRINT_HQ))
			fz_strlcat(buf, "print-hq, ", sizeof buf);
		if (strlen(buf) > 2)
			buf[strlen(buf)-2] = 0;
		else
			fz_strlcat(buf, "none", sizeof buf);
		fz_append_printf(ctx, out, "Permissions: %s\n", buf);

		fz_append_printf(ctx, out, "PDF %sdocument with %d update%s\n",
			pdf_doc_was_linearized(ctx, pdoc) ? "linearized " : "",
			updates, updates > 1 ? "s" : "");
		if (updates > 0)
		{
			int n = pdf_validate_change_history(ctx, pdoc);
			if (n == 0)
				fz_append_printf(ctx, out, "Change history seems valid.\n");
			else if (n == 1)
				fz_append_printf(ctx, out, "Invalid changes made to the document in the last update.\n");
			else if (n == 2)
				fz_append_printf(ctx, out, "Invalid changes made to the document in the penultimate update.\n");
			else
				fz_append_printf(ctx, out, "Invalid changes made to the document %d updates ago.\n", n);
		}

		if (list.len)
		{
			int i;
			for (i = 0; i < list.len; i++)
			{
				pdf_obj *field = list.sig[i];
				fz_try(ctx)
				{
					if (pdf_signature_is_signed(ctx, pdf, field))
					{
						pdf_pkcs7_verifier *verifier = pkcs7_openssl_new_verifier(ctx);
						pdf_signature_error sig_cert_error = pdf_check_certificate(ctx, verifier, pdf, field);
						pdf_signature_error sig_digest_error = pdf_check_digest(ctx, verifier, pdf, field);
						fz_append_printf(ctx, out, "Signature %d: CERT: %s, DIGEST: %s%s\n", i+1,
							short_signature_error_desc(sig_cert_error),
							short_signature_error_desc(sig_digest_error),
							pdf_signature_incremental_change_since_signing(ctx, pdf, field) ? ", Changed since": "");
						pdf_drop_verifier(ctx, verifier);
					}
					else
						fz_append_printf(ctx, out, "Signature %d: Unsigned\n", i+1);
				}
				fz_catch(ctx)
					fz_append_printf(ctx, out, "Signature %d: Error\n", i+1);
			}
			fz_free(ctx, list.sig);

			if (updates == 0)
				fz_append_printf(ctx, out, "No updates since document creation\n");
			else
			{
				int n = pdf_validate_change_history(ctx, pdf);
				if (n == 0)
					fz_append_printf(ctx, out, "Document changes conform to permissions\n");
				else
					fz_append_printf(ctx, out, "Document permissions violated %d updates ago\n", n);
			}
		}

		fz_append_string(ctx, out, "\n");
	}

	fz_append_printf(ctx, out, "Page: %d / %d\n", fz_page_number_from_location(ctx, doc, currentpage)+1, fz_count_pages(ctx, doc));
	fz_append_printf(ctx, out, "Page Label: %s\n", fz_page_label(ctx, fzpage, buf, sizeof buf));
	{
		int w = (int)(page_bounds.x1 - page_bounds.x0 + 0.5f);
		int h = (int)(page_bounds.y1 - page_bounds.y0 + 0.5f);
		const char *size = paper_size_name(w, h);
		if (!size)
			size = paper_size_name(h, w);
		if (size)
			fz_append_printf(ctx, out, "Size: %d x %d (%s - %s)\n", w, h, fz_string_from_box_type(currentbox), size);
		else
			fz_append_printf(ctx, out, "Size: %d x %d (%s)\n", w, h, fz_string_from_box_type(currentbox));
	}
	fz_append_printf(ctx, out, "ICC rendering: %s.\n", currenticc ? "on" : "off");
	fz_append_printf(ctx, out, "Spot rendering: %s.\n", currentseparations ? "on" : "off");

	return out;
}

static void do_canvas(void)
{
	static int saved_scroll_x = 0;
	static int saved_scroll_y = 0;
	static int saved_ui_x = 0;
	static int saved_ui_y = 0;
	fz_irect area;
	int page_x, page_y;

	tooltip = NULL;

	ui_layout(ALL, BOTH, NW, 0, 0);
	ui_pack_push(area = ui_pack(0, 0));
	glScissor(area.x0, ui.window_h-area.y1, area.x1-area.x0, area.y1-area.y0);
	glEnable(GL_SCISSOR_TEST);

	canvas_x = area.x0;
	canvas_y = area.y0;
	canvas_w = area.x1 - area.x0;
	canvas_h = area.y1 - area.y0;

	if (ui_mouse_inside(area))
	{
		ui.hot = doc;
		if (!ui.active && ui.middle)
		{
			ui.active = doc;
			saved_scroll_x = scroll_x;
			saved_scroll_y = scroll_y;
			saved_ui_x = ui.x;
			saved_ui_y = ui.y;
		}
	}

	if (ui.hot == doc)
	{
		if (ui.mod == 0)
		{
			scroll_x -= ui.scroll_x * ui.lineheight * 3;
			scroll_y -= ui.scroll_y * ui.lineheight * 3;
		}
		else if (ui.mod == GLUT_ACTIVE_CTRL)
		{
			if (ui.scroll_y > 0) set_zoom(zoom_in(currentzoom), ui.x, ui.y);
			if (ui.scroll_y < 0) set_zoom(zoom_out(currentzoom), ui.x, ui.y);
		}
	}

	render_page_if_changed();

	if (ui.active == doc)
	{
		scroll_x = saved_scroll_x + saved_ui_x - ui.x;
		scroll_y = saved_scroll_y + saved_ui_y - ui.y;
	}

	if (page_tex.w <= canvas_w)
	{
		scroll_x = 0;
		page_x = canvas_x + (canvas_w - page_tex.w) / 2;
	}
	else
	{
		scroll_x = fz_clamp(scroll_x, 0, page_tex.w - canvas_w);
		page_x = canvas_x - scroll_x;
	}

	if (page_tex.h <= canvas_h)
	{
		scroll_y = 0;
		page_y = canvas_y + (canvas_h - page_tex.h) / 2;
	}
	else
	{
		scroll_y = fz_clamp(scroll_y, 0, page_tex.h - canvas_h);
		page_y = canvas_y - scroll_y;
	}

	view_page_ctm = draw_page_ctm;
	view_page_ctm.e += page_x;
	view_page_ctm.f += page_y;
	view_page_inv_ctm = fz_invert_matrix(view_page_ctm);
	view_page_bounds = fz_transform_rect(page_bounds, view_page_ctm);
	view_page_area = fz_irect_from_rect(view_page_bounds);

	ui_draw_image(&page_tex, page_x, page_y);

	if (search_active)
	{
		int chapters = fz_count_chapters(ctx, doc);
		ui_layout(T, X, NW, 0, 0);
		ui_panel_begin(0, ui.gridsize + ui.padsize*4, ui.padsize*2, ui.padsize*2, 1);
		ui_layout(L, NONE, W, ui.padsize, 0);
		if (chapters == 1 && search_page.chapter == 0)
			ui_label("Searching page %d...", search_page.page);
		else
			ui_label("Searching chapter %d page %d...", search_page.chapter, search_page.page);
		ui_panel_end();
	}
	else
	{
		if (pdf)
		{
			do_annotate_canvas(area);
			do_widget_canvas(area);
		}
		do_links(links);
		do_page_selection();

		if (eqloc(search_hit_page, currentpage) && search_hit_count > 0)
			do_search_hits();
	}

	if (showsearch)
	{
		ui_layout(T, X, NW, 0, 0);
		ui_panel_begin(0, ui.gridsize + ui.padsize*4, ui.padsize*2, ui.padsize*2, 1);
		ui_layout(L, NONE, W, ui.padsize, 0);
		ui_label("Search:");
		ui_layout(ALL, X, E, ui.padsize, 0);
		if (ui_input(&search_input, 0, 1) == UI_INPUT_ACCEPT)
		{
			showsearch = 0;
			search_page = fz_make_location(-1, -1);
			if (search_needle)
			{
				fz_free(ctx, search_needle);
				search_needle = NULL;
			}
			if (search_input.end > search_input.text)
			{
				search_needle = fz_strdup(ctx, search_input.text);
				search_active = 1;
				search_page = currentpage;
			}
		}
		if (ui.focus != &search_input)
			showsearch = 0;
		ui_panel_end();
	}

	if (tooltip)
	{
		ui_layout(B, X, N, 0, 0);
		ui_panel_begin(0, ui.gridsize, ui.padsize*2, ui.padsize*2, 1);
		ui_layout(L, NONE, W, ui.padsize, 0);
		ui_label("%s", tooltip);
		ui_panel_end();
	}

	ui_pack_pop();
	glDisable(GL_SCISSOR_TEST);
}

void do_main(void)
{
	if (search_active)
	{
		int start_time = glutGet(GLUT_ELAPSED_TIME);

		if (ui.key == KEY_ESCAPE)
			search_active = 0;

		/* ignore events during search */
		ui.key = ui.mod = ui.plain = 0;
		ui.down = ui.middle = ui.right = 0;

		while (search_active && glutGet(GLUT_ELAPSED_TIME) < start_time + 200)
		{
			search_hit_count = fz_search_chapter_page_number(ctx, doc,
				search_page.chapter, search_page.page,
				search_needle,
				NULL, search_hit_quads, nelem(search_hit_quads));
			trace_action("hits = doc.loadPage(%d).search(%q);\n", fz_page_number_from_location(ctx, doc, search_page), search_needle);
			trace_action("print('Search page %d:', repr(%q), hits.length, repr(hits));\n", fz_page_number_from_location(ctx, doc, search_page), search_needle);
			if (search_hit_count)
			{
				float search_hit_x = search_hit_quads[0].ul.x;
				float search_hit_y = search_hit_quads[0].ul.y;
				search_active = 0;
				search_hit_page = search_page;
				jump_to_location_xy(search_hit_page, search_hit_x, search_hit_y);
			}
			else
			{
				if (search_dir > 0)
				{
					if (is_last_page(search_page))
						search_active = 0;
					else
						search_page = fz_next_page(ctx, doc, search_page);
				}
				else
				{
					if (is_first_page(search_page))
						search_active = 0;
					else
						search_page = fz_previous_page(ctx, doc, search_page);
				}
			}
		}

		/* keep searching later */
		if (search_active)
			glutPostRedisplay();
	}

	do_app();

	if (showoutline)
		do_outline(outline);
	else if (showundo)
		do_undo();
	else if (showlayers)
		do_layers();
	if (showoutline || showundo || showlayers)
		ui_splitter(&outline_start_x, &outline_w, 6*ui.gridsize, 20*ui.gridsize, R);

	if (!eqloc(oldpage, currentpage) || oldseparations != currentseparations || oldicc != currenticc)
	{
		load_page();
		update_title();
	}

	if (showannotate)
	{
		ui_layout(R, BOTH, NW, 0, 0);
		ui_panel_begin(annotate_w, 0, ui.padsize*2, ui.padsize*2, 1);
		if (showannotate == ANNOTATE_MODE_NORMAL)
			do_annotate_panel();
		else
			do_redact_panel();
		ui_panel_end();
	}

	if (showconsole)
	{
		do_console();
		ui_splitter(&console_start_y, &console_h, 6*ui.lineheight, 25*ui.lineheight, T);
	}

	do_canvas();

	if (pdf)
	{
		if (document_shown_as_dirty != pdf_has_unsaved_changes(ctx, pdf))
			update_title();
	}
}

void run_main_loop(void)
{
	if (currentinvert)
		glClearColor(0, 0, 0, 1);
	else
		glClearColor(0.3f, 0.3f, 0.3f, 1);
	ui_begin();
	fz_try(ctx)
	{
		if (ui.dialog)
			ui.dialog();
		else
			do_main();
	}
	fz_catch(ctx)
	{
		ui_show_error_dialog("%s", fz_caught_message(ctx));
		fz_report_error(ctx);
	}
	ui_end();
}

static void usage(const char *argv0)
{
	fprintf(stderr, "mupdf-gl version %s\n", FZ_VERSION);
	fprintf(stderr, "usage: %s [options] document [page]\n", argv0);
	fprintf(stderr, "\t-p -\tpassword\n");
	fprintf(stderr, "\t-r -\tresolution\n");
	fprintf(stderr, "\t-c -\tdisplay ICC profile\n");
	fprintf(stderr, "\t-b -\tuse named page box (MediaBox, CropBox, BleedBox, TrimBox, or ArtBox)\n");
	fprintf(stderr, "\t-I\tinvert colors\n");
	fprintf(stderr, "\t-W -\tpage width for EPUB layout\n");
	fprintf(stderr, "\t-H -\tpage height for EPUB layout\n");
	fprintf(stderr, "\t-S -\tfont size for EPUB layout\n");
	fprintf(stderr, "\t-U -\tuser style sheet for EPUB layout\n");
	fprintf(stderr, "\t-X\tdisable document styles for EPUB layout\n");
	fprintf(stderr, "\t-J\tdisable javascript in PDF forms\n");
	fprintf(stderr, "\t-A -\tset anti-aliasing level (0-8,9,10)\n");
	fprintf(stderr, "\t-B -\tset black tint color (default: 303030)\n");
	fprintf(stderr, "\t-C -\tset white tint color (default: FFFFF0)\n");
	fprintf(stderr, "\t-Y -\tset the UI scaling factor\n");
	fprintf(stderr, "\t-R -\tenable reflow and set the text extraction options\n");
	fprintf(stderr, "\t\t\texample: -R dehyphenate,preserve-images\n");
	exit(1);
}

static int document_filter(const char *fname)
{
	return !!fz_recognize_document(ctx, fname);
}

static void do_open_document_dialog(void)
{
	if (ui_open_file(filename, "Select a document to open:"))
	{
		ui.dialog = NULL;
		if (filename[0] == 0)
			glutLeaveMainLoop();
		else
		{
			load_document();
			if (doc)
			{
				if (reflow_options)
					reflow_document();
				load_page();
				shrinkwrap();
				update_title();
			}
		}
	}
}

static void cleanup(void)
{
	save_history();
	fz_try(ctx)
		save_accelerator();
	fz_catch(ctx)
		fz_warn(ctx, "cannot save accelerator file");

	ui_finish();

	fz_drop_pixmap(ctx, page_contents);
	page_contents = NULL;
#ifndef NDEBUG
	if (fz_atoi(getenv("FZ_DEBUG_STORE")))
		fz_debug_store(ctx, fz_stdout(ctx));
#endif

	trace_action("quit(0);\n");

	fz_flush_warnings(ctx);

	console_fin();

	fz_drop_output(ctx, trace_file);
	fz_drop_stext_page(ctx, page_text);
	fz_drop_separations(ctx, seps);
	fz_drop_link(ctx, links);
	fz_drop_page(ctx, fzpage);
	fz_drop_outline(ctx, outline);
	fz_drop_document(ctx, doc);
	fz_drop_context(ctx);
}

int reloadrequested = 0;

#ifndef _WIN32
static void signal_handler(int signal)
{
	if (signal == SIGHUP)
		reloadrequested = 1;
}
#endif

#ifdef _MSC_VER
int main_utf8(int argc, char **argv)
#else
int main(int argc, char **argv)
#endif
{
	const char *trace_file_name = NULL;
	const char *profile_name = NULL;
	float scale = 0;
	int c;

#ifndef _WIN32

	/* Never wait for termination of child processes. */
	struct sigaction arg = {
		.sa_handler=SIG_IGN,
		.sa_flags=SA_NOCLDWAIT
	};
	sigaction(SIGCHLD, &arg, NULL);

	signal(SIGHUP, signal_handler);
#endif

	glutInit(&argc, argv);

	while ((c = fz_getopt(argc, argv, "p:r:IW:H:S:U:XJb:A:B:C:T:Y:R:c:")) != -1)
	{
		switch (c)
		{
		default: usage(argv[0]); break;
		case 'p': password = fz_optarg; break;
		case 'r': currentzoom = fz_atof(fz_optarg); break;
		case 'c': profile_name = fz_optarg; break;
		case 'I': currentinvert = !currentinvert; break;
		case 'b': currentbox = fz_box_type_from_string(fz_optarg); break;
		case 'W': layout_w = fz_atof(fz_optarg); break;
		case 'H': layout_h = fz_atof(fz_optarg); break;
		case 'S': layout_em = fz_atof(fz_optarg); break;
		case 'U': layout_css = fz_optarg; break;
		case 'X': layout_use_doc_css = 0; break;
		case 'J': enable_js = !enable_js; break;
		case 'A': currentaa = fz_atoi(fz_optarg); break;
		case 'C': currenttint = 1; tint_white = strtol(fz_optarg, NULL, 16); break;
		case 'B': currenttint = 1; tint_black = strtol(fz_optarg, NULL, 16); break;
		case 'R': reflow_options = fz_optarg; break;
		case 'T': trace_file_name = fz_optarg; break;
		case 'Y': scale = fz_atof(fz_optarg); break;
		}
	}

	screen_w = glutGet(GLUT_SCREEN_WIDTH) - SCREEN_FURNITURE_W;
	screen_h = glutGet(GLUT_SCREEN_HEIGHT) - SCREEN_FURNITURE_H;

	ui_init_dpi(scale);

	oldzoom = currentzoom = currentzoom * ui.scale;

	ctx = fz_new_context(NULL, NULL, FZ_STORE_DEFAULT);

#ifdef _WIN32
	/* stderr goes nowhere. Get us a debug stream we have a chance
	 * of seeing. */
	fz_set_stddbg(ctx, fz_stdods(ctx));
#endif

	console_init();

	fz_register_document_handlers(ctx);

	if (trace_file_name)
	{
		if (!strcmp(trace_file_name, "-"))
			trace_file = fz_stdout(ctx);
		else
			trace_file = fz_new_output_with_path(ctx, trace_file_name, 0);
		trace_action("var doc, page, annot, widget, widgetstr, hits, tmp;\n");
		trace_action("function RegressionError() {\n");
		trace_action("  var err = new Error(Array.prototype.join.call(arguments, ' '));\n");
		trace_action("	err.name = 'RegressionError';\n");
		trace_action("	return err;\n");
		trace_action("}\n");
	}

	if (profile_name)
	{
		fz_buffer *profile_data = fz_read_file(ctx, profile_name);
		profile = fz_new_icc_colorspace(ctx, FZ_COLORSPACE_RGB, 0, NULL, profile_data);
		fz_drop_buffer(ctx, profile_data);
	}
	else
	{
		profile = fz_device_rgb(ctx);
	}

	if (layout_css)
	{
		fz_buffer *buf = fz_read_file(ctx, layout_css);
		fz_set_user_css(ctx, fz_string_from_buffer(ctx, buf));
		fz_drop_buffer(ctx, buf);
	}
	fz_set_use_document_css(ctx, layout_use_doc_css);

	if (fz_optind < argc)
	{
		fz_strlcpy(filename, argv[fz_optind++], sizeof filename);
		if (fz_optind < argc)
			anchor = argv[fz_optind++];
		if (fz_optind < argc)
			usage(argv[0]);

		fz_try(ctx)
		{
			page_tex.w = 600;
			page_tex.h = 700;
			load_document();
			if (doc)
			{
				if (reflow_options)
					reflow_document();
				load_page();
			}
		}
		fz_always(ctx)
		{
			float sx = 1, sy = 1;
			if (screen_w > 0 && page_tex.w > screen_w)
				sx = (float)screen_w / page_tex.w;
			if (screen_h > 0 && page_tex.h > screen_h)
				sy = (float)screen_h / page_tex.h;
			if (sy < sx)
				sx = sy;
			if (sx < 1)
			{
				fz_irect area;

				currentzoom *= sx;
				oldzoom = currentzoom;

				/* compute bounds here for initial window size */
				page_bounds = fz_bound_page_box(ctx, fzpage, currentbox);
				transform_page();

				area = fz_irect_from_rect(draw_page_bounds);
				page_tex.w = area.x1 - area.x0;
				page_tex.h = area.y1 - area.y0;
			}

			ui_init(page_tex.w, page_tex.h, "MuPDF: Loading...");
			ui_input_init(&search_input, "");
		}
		fz_catch(ctx)
		{
			ui_show_error_dialog("%s", fz_caught_message(ctx));
			fz_report_error(ctx);
		}

		fz_try(ctx)
		{
			if (doc)
				update_title();
		}
		fz_catch(ctx)
		{
			ui_show_error_dialog("%s", fz_caught_message(ctx));
			fz_report_error(ctx);
		}
	}
	else
	{
#ifdef _WIN32
		win_install();
#endif
		ui_init(ui.gridsize * 26, ui.gridsize * 26, "MuPDF: Open document");
		ui_input_init(&search_input, "");
		ui_init_open_file(".", document_filter);
		ui.dialog = do_open_document_dialog;
	}

	annotate_w *= ui.lineheight;
	outline_w *= ui.lineheight;
	console_h *= ui.lineheight;

	glutMainLoop();

	cleanup();

	return 0;
}

#ifdef _MSC_VER
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
	int argc;
	LPWSTR *wargv = CommandLineToArgvW(GetCommandLineW(), &argc);
	char **argv = fz_argv_from_wargv(argc, wargv);
	int ret = main_utf8(argc, argv);
	fz_free_argv(argc, argv);
	return ret;
}
#endif
