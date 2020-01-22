#include "gl-app.h"

#include <limits.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#ifdef _MSC_VER
#define stat _stat
#endif
#ifndef _WIN32
#include <signal.h>
#endif

#include "mupdf/helpers/pkcs7-check.h"
#include "mupdf/helpers/pkcs7-openssl.h"

#include "mujs.h"

#ifndef PATH_MAX
#define PATH_MAX 2048
#endif

#ifndef _WIN32
#include <unistd.h> /* for fork, exec, and getcwd */
#else
#include <direct.h> /* for getcwd */
char *realpath(const char *path, char *resolved_path); /* in gl-file.c */
#endif

#ifdef __APPLE__
static void cleanup(void);
void glutLeaveMainLoop(void)
{
	cleanup();
	exit(0);
}
#endif

time_t
stat_mtime(const char *path)
{
	struct stat info;

	if (stat(path, &info) < 0)
		return 0;

	return info.st_mtime;
}

fz_context *ctx = NULL;
pdf_document *pdf = NULL;
pdf_page *page = NULL;
pdf_annot *selected_annot = NULL;
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
	char buf[PATH_MAX];

	/* Relative file:// URI, make it absolute! */
	if (!strncmp(uri, "file://", 7) && uri[7] != '/')
	{
		char buf_base[PATH_MAX];
		char buf_cwd[PATH_MAX];
		fz_dirname(buf_base, filename, sizeof buf_base);
		getcwd(buf_cwd, sizeof buf_cwd);
		fz_snprintf(buf, sizeof buf, "file://%s/%s/%s", buf_cwd, buf_base, uri+7);
		fz_cleanname(buf+7);
		uri = buf;
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
	if (fork() == 0)
	{
		execlp(browser, browser, uri, (char*)0);
		fprintf(stderr, "cannot exec '%s'\n", browser);
		exit(0);
	}
#endif
}

static const int zoom_list[] = {
	24, 36, 48, 60, 72, 84, 96, 108,
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

static struct texture page_tex = { 0 };
static int screen_w = 0, screen_h = 0;
static int scroll_x = 0, scroll_y = 0;
static int canvas_x = 0, canvas_w = 100;
static int canvas_y = 0, canvas_h = 100;

static int outline_w = 14; /* to be scaled by lineheight */
static int annotate_w = 12; /* to be scaled by lineheight */

static int oldtint = 0, currenttint = 0;
static int oldinvert = 0, currentinvert = 0;
static int oldicc = 1, currenticc = 1;
static int oldaa = 8, currentaa = 8;
static int oldseparations = 0, currentseparations = 0;
static fz_location oldpage = {0,0}, currentpage = {0,0};
static float oldzoom = DEFRES, currentzoom = DEFRES;
static float oldrotate = 0, currentrotate = 0;

static int isfullscreen = 0;
static int showoutline = 0;
static int showlinks = 0;
static int showsearch = 0;
static int showinfo = 0;
int showannotate = 0;
int showform = 0;

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
		char *home = getenv("HOME");
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

static void read_history_file_as_json(js_State *J)
{
	fz_buffer *buf = NULL;
	const char *json = "{}";

	fz_var(buf);

	if (fz_file_exists(ctx, get_history_filename()))
	{
		fz_try(ctx)
		{
			buf = fz_read_file(ctx, get_history_filename());
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

	if (!realpath(filename, absname))
		return;

	J = js_newstate(NULL, NULL, 0);

	read_history_file_as_json(J);

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

	if (!realpath(filename, absname))
		return;

	J = js_newstate(NULL, NULL, 0);

	read_history_file_as_json(J);

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
		out = fz_new_output_with_path(ctx, get_history_filename(), 0);
		fz_write_string(ctx, out, json);
		fz_write_byte(ctx, out, '\n');
		fz_close_output(ctx, out);
	}
	fz_always(ctx)
		fz_drop_output(ctx, out);
	fz_catch(ctx)
		fz_warn(ctx, "Can't write history file.");

	js_freestate(J);
}

static int convert_to_accel_path(char outname[], char *absname, size_t len)
{
	char *tmpdir;
	char *s;

	tmpdir = getenv("TEMP");
	if (!tmpdir)
		tmpdir = getenv("TMP");
	if (!tmpdir)
		tmpdir = "/var/tmp";
	if (!fz_is_directory(ctx, tmpdir))
		tmpdir = "/tmp";

	if (absname[0] == '/' || absname[0] == '\\')
		++absname;

	s = absname;
	while (*s) {
		if (*s == '/' || *s == '\\' || *s == ':')
			*s = '%';
		++s;
	}

	if (fz_snprintf(outname, len, "%s/%s.accel", tmpdir, absname) >= len)
		return 0;
	return 1;
}

static int get_accelerator_filename(char outname[], size_t len)
{
	char absname[PATH_MAX];
	if (!realpath(filename, absname))
		return 0;
	if (!convert_to_accel_path(outname, absname, len))
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
	if (!get_accelerator_filename(absname, sizeof(absname)))
		return;

	fz_save_accelerator(ctx, doc, absname);
}

static int search_active = 0;
static struct input search_input = { { 0 }, 0 };
static char *search_needle = 0;
static int search_dir = 1;
static fz_location search_page = {-1, -1};
static fz_location search_hit_page = {-1, -1};
static int search_hit_count = 0;
static fz_quad search_hit_quads[5000];

static char *help_dialog_text =
	"The middle mouse button (scroll wheel button) pans the document view. "
	"The right mouse button selects a region and copies the marked text to the clipboard."
	"\n"
	"\n"
	"F1 - show this message\n"
	"i - show document information\n"
	"o - show document outline\n"
	"a - show annotation editor\n"
	"L - highlight links\n"
	"F - highlight form fields\n"
	"r - reload file\n"
	"S - save file (only for PDF)\n"
	"q - quit\n"
	"\n"
	"< - decrease E-book font size\n"
	"> - increase E-book font size\n"
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
	ui_dialog_begin(500, 1000);
	ui_layout(T, X, W, 2, 2);
	ui_label("MuPDF %s", FZ_VERSION);
	ui_spacer();
	ui_layout(B, NONE, S, 2, 2);
	if (ui_button("Okay") || ui.key == KEY_ENTER || ui.key == KEY_ESCAPE)
		ui.dialog = NULL;
	ui_spacer();
	ui_layout(ALL, BOTH, CENTER, 2, 2);
	ui_label_with_scrollbar(help_dialog_text, 0, 0, &scroll);
	ui_dialog_end();
}

static char error_message[256];
static void error_dialog(void)
{
	ui_dialog_begin(500, (ui.gridsize+4)*4);
	ui_layout(T, NONE, NW, 2, 2);
	ui_label("%C %s", 0x1f4a3, error_message); /* BOMB */
	ui_layout(B, NONE, S, 2, 2);
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
	ui_dialog_begin(500, (ui.gridsize+4)*4);
	ui_layout(T, NONE, NW, 2, 2);
	ui_label("%C %s", 0x26a0, warning_message); /* WARNING SIGN */
	ui_layout(B, NONE, S, 2, 2);
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

void update_title(void)
{
	char buf[256];
	char *title = "MuPDF/GL";
	char *extra = "";
	size_t n;

	int nc = fz_count_chapters(ctx, doc);

	title = strrchr(filename, '/');
	if (!title)
		title = strrchr(filename, '\\');
	if (title)
		++title;
	else
		title = filename;

	if (pdf && pdf->dirty)
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

void load_page(void)
{
	fz_irect area;

	/* clear all editor selections */
	if (selected_annot && pdf_annot_type(ctx, selected_annot) == PDF_ANNOT_WIDGET)
		pdf_annot_event_blur(ctx, selected_annot);
	selected_annot = NULL;

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

	links = fz_load_links(ctx, fzpage);
	page_text = fz_new_stext_page_from_page(ctx, fzpage, NULL);

	if (currenticc)
		fz_enable_icc(ctx);
	else
		fz_disable_icc(ctx);

	if (currentseparations)
	{
		seps = fz_page_separations(ctx, &page->super);
		if (seps)
		{
			int i, n = fz_count_separations(ctx, seps);
			for (i = 0; i < n; i++)
				fz_set_separation_behavior(ctx, seps, i, FZ_SEPARATION_COMPOSITE);
		}
		else if (fz_page_uses_overprint(ctx, &page->super))
			seps = fz_new_separations(ctx, 0);
		else if (fz_document_output_intent(ctx, doc))
			seps = fz_new_separations(ctx, 0);
	}

	/* compute bounds here for initial window size */
	page_bounds = fz_bound_page(ctx, fzpage);
	transform_page();

	area = fz_irect_from_rect(draw_page_bounds);
	page_tex.w = area.x1 - area.x0;
	page_tex.h = area.y1 - area.y0;
}

void render_page(void)
{
	fz_pixmap *pix;

	transform_page();

	fz_set_aa_level(ctx, currentaa);

	pix = fz_new_pixmap_from_page_with_separations(ctx, fzpage, draw_page_ctm, fz_device_rgb(ctx), seps, 0);
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
}

void render_page_if_changed(void)
{
	if (oldpage.chapter != currentpage.chapter ||
		oldpage.page != currentpage.page ||
		oldzoom != currentzoom ||
		oldrotate != currentrotate ||
		oldinvert != currentinvert ||
		oldtint != currenttint ||
		oldicc != currenticc ||
		oldseparations != currentseparations ||
		oldaa != currentaa)
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
		render_page();
		update_title();
	}
}

static int count_outline(fz_outline *node, int end)
{
	int is_selected, n, p;
	int count = 0;
	while (node)
	{
		p = node->page;
		count += 1;
		n = end;
		if (node->next && node->next->page >= 0)
			n = node->next->page;
		is_selected = 0;
		if (fz_count_chapters(ctx, doc) == 1)
			is_selected = (p>=0) && (currentpage.page == p || (currentpage.page > p && currentpage.page < n));
		if (node->down && (node->is_open || is_selected))
			count += count_outline(node->down, end);
		node = node->next;
	}
	return count;
}

static void do_outline_imp(struct list *list, int end, fz_outline *node, int depth)
{
	int is_selected, was_open, n;

	while (node)
	{
		int p = node->page;
		n = end;
		if (node->next && node->next->page >= 0)
			n = node->next->page;

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
}

static void do_outline(fz_outline *node)
{
	static struct list list;
	ui_layout(L, BOTH, NW, 0, 0);
	ui_tree_begin(&list, count_outline(node, 65535), outline_w, 0, 1);
	do_outline_imp(&list, 65535, node, 0);
	ui_tree_end(&list);
	ui_splitter(&outline_w, 150, 500, R);
}

static void do_links(fz_link *link)
{
	fz_rect bounds;
	fz_irect area;
	float link_x, link_y;

	glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_BLEND);

	while (link)
	{
		bounds = link->rect;
		bounds = fz_transform_rect(link->rect, view_page_ctm);
		area = fz_irect_from_rect(bounds);

		if (ui_mouse_inside(area))
		{
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

		n = fz_highlight_selection(ctx, page_text, page_a, page_b, hits, nelem(hits));

		glBlendFunc(GL_ONE_MINUS_DST_COLOR, GL_ZERO); /* invert destination color */
		glEnable(GL_BLEND);

		glColor4f(1, 1, 1, 1);
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
			s = fz_copy_selection(ctx, page_text, page_a, page_b, 1);
#else
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
	int w = page_tex.w + (showoutline ? outline_w + 4 : 0) + (showannotate ? annotate_w : 0);
	int h = page_tex.h;
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
	ui_dialog_begin(400, (ui.gridsize+4)*3);
	{
		ui_layout(T, X, NW, 2, 2);
		ui_label("Password:");
		is = ui_input(&input_password, 200, 1);

		ui_layout(B, X, NW, 2, 2);
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
				reload();
				shrinkwrap();
			}
		}
		ui_panel_end();
	}
	ui_dialog_end();
}

static void load_document(void)
{
	char accelpath[PATH_MAX];
	char *accel = NULL;
	time_t atime;
	time_t dtime;

	fz_drop_outline(ctx, outline);
	fz_drop_document(ctx, doc);

	/* If there was an accelerator to load, what would it be called? */
	if (get_accelerator_filename(accelpath, sizeof(accelpath)))
	{
		/* Check whether that file exists, and isn't older than
		 * the document. */
		atime = stat_mtime(accelpath);
		dtime = stat_mtime(filename);
		if (atime == 0)
		{
			/* No accelerator */
		}
		else if (atime > dtime)
			accel = accelpath;
		else
		{
			/* Accelerator data is out of date */
			unlink(accelpath);
			accel = NULL; /* In case we have jumped up from below */
		}
	}

	doc = fz_open_accelerated_document(ctx, filename, accel);
	if (fz_needs_password(ctx, doc))
	{
		if (!fz_authenticate_password(ctx, doc, password))
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
		outline = NULL;

	load_history();

	pdf = pdf_specifics(ctx, doc);
	if (pdf)
	{
		if (enable_js)
			pdf_enable_js(ctx, pdf);
		if (anchor)
			jump_to_page(pdf_lookup_anchor(ctx, pdf, anchor, NULL, NULL));
	}
	else
	{
		if (anchor)
			jump_to_page(fz_atoi(anchor) - 1);
	}
	anchor = NULL;

	currentpage = fz_clamp_location(ctx, doc, currentpage);
}

void reload(void)
{
	save_history();
	save_accelerator();
	load_document();
	if (doc)
	{
		load_page();
		render_page();
		update_title();
	}
}

static void toggle_outline(void)
{
	if (outline)
	{
		showoutline = !showoutline;
		if (canvas_w == page_tex.w && canvas_h == page_tex.h)
			shrinkwrap();
	}
}

void toggle_annotate(void)
{
	if (pdf)
	{
		showannotate = !showannotate;
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

static void do_app(void)
{
	if (ui.key == KEY_F4 && ui.mod == GLUT_ACTIVE_ALT)
		glutLeaveMainLoop();

	if (ui.down || ui.middle || ui.right || ui.key)
		showinfo = 0;

	if (!ui.focus && ui.key && ui.plain)
	{
		switch (ui.key)
		{
		case KEY_ESCAPE: clear_search(); selected_annot = NULL; break;
		case KEY_F1: ui.dialog = help_dialog; break;
		case 'a': toggle_annotate(); break;
		case 'o': toggle_outline(); break;
		case 'L': showlinks = !showlinks; break;
		case 'F': showform = !showform; break;
		case 'i': showinfo = !showinfo; break;
		case 'r': reload(); break;
		case 'q': glutLeaveMainLoop(); break;
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
				search_page = fz_previous_page(ctx, doc, currentpage);
				if (is_first_page(search_page))
					search_active = 0;
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
				search_page = fz_next_page(ctx, doc, currentpage);
				if (is_last_page(search_page))
					search_active = 0;
			}
			else
			{
				search_page = currentpage;
			}
			search_hit_page = fz_make_location(-1, -1);
			break;
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
process_sigs(fz_context *ctx, pdf_obj *field, void *arg, pdf_obj **ft)
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

static void do_info(void)
{
	char buf[100];
	pdf_document *pdoc = pdf_specifics(ctx, doc);
	sigs_list list = { 0, 0, NULL };

	if (pdoc)
	{
		static pdf_obj *ft_list[2] = { PDF_NAME(FT), NULL };
		pdf_obj *ft;
		pdf_obj *form_fields = pdf_dict_getp(ctx, pdf_trailer(ctx, pdoc), "Root/AcroForm/Fields");
		pdf_walk_tree(ctx, form_fields, PDF_NAME(Kids), process_sigs, NULL, &list, &ft_list[0], &ft);
	}

	ui_dialog_begin(500, (14+list.len) * ui.lineheight);
	ui_layout(T, X, W, 0, 0);

	if (fz_lookup_metadata(ctx, doc, FZ_META_INFO_TITLE, buf, sizeof buf) > 0)
		ui_label("Title: %s", buf);
	if (fz_lookup_metadata(ctx, doc, FZ_META_INFO_AUTHOR, buf, sizeof buf) > 0)
		ui_label("Author: %s", buf);
	if (fz_lookup_metadata(ctx, doc, FZ_META_FORMAT, buf, sizeof buf) > 0)
		ui_label("Format: %s", buf);
	if (fz_lookup_metadata(ctx, doc, FZ_META_ENCRYPTION, buf, sizeof buf) > 0)
		ui_label("Encryption: %s", buf);
	if (pdoc)
	{
		int updates = pdf_count_incremental_updates(ctx, pdoc);

		if (fz_lookup_metadata(ctx, doc, "info:Creator", buf, sizeof buf) > 0)
			ui_label("PDF Creator: %s", buf);
		if (fz_lookup_metadata(ctx, doc, "info:Producer", buf, sizeof buf) > 0)
			ui_label("PDF Producer: %s", buf);
		buf[0] = 0;
		if (fz_has_permission(ctx, doc, FZ_PERMISSION_PRINT))
			fz_strlcat(buf, "print, ", sizeof buf);
		if (fz_has_permission(ctx, doc, FZ_PERMISSION_COPY))
			fz_strlcat(buf, "copy, ", sizeof buf);
		if (fz_has_permission(ctx, doc, FZ_PERMISSION_EDIT))
			fz_strlcat(buf, "edit, ", sizeof buf);
		if (fz_has_permission(ctx, doc, FZ_PERMISSION_ANNOTATE))
			fz_strlcat(buf, "annotate, ", sizeof buf);
		if (strlen(buf) > 2)
			buf[strlen(buf)-2] = 0;
		else
			fz_strlcat(buf, "none", sizeof buf);
		ui_label("Permissions: %s", buf);
		ui_label("PDF %sdocument with %d updates",
			pdf_doc_was_linearized(ctx, pdoc) ? "linearized " : "",
			updates);
		if (updates > 0)
		{
			if (pdf_validate_change_history(ctx, pdoc) == 0)
				ui_label("Change history seems valid");
			else
				ui_label("Change history invalid");
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
						if (pdf_supports_signatures(ctx))
						{
							pdf_signature_error sig_cert_error = pdf_check_certificate(ctx, pdf, field);
							pdf_signature_error sig_digest_error = pdf_check_digest(ctx, pdf, field);
							ui_label("Signature %d: CERT: %s, DIGEST: %s%s", i+1,
								short_signature_error_desc(sig_cert_error),
								short_signature_error_desc(sig_digest_error),
									pdf_signature_incremental_change_since_signing(ctx, pdf, field) ? ", Changed since": "");
						}
						else
							ui_label("Signature %d: Signed (cannot test validity)", i+1);
					}
					else
						ui_label("Signature %d: Unsigned", i+1);
				}
				fz_catch(ctx)
					ui_label("Signature %d: Error", i+1);
			}
			fz_free(ctx, list.sig);

			if (updates == 0)
				ui_label("No updates since document creation");
			else
			{
				int n = pdf_validate_change_history(ctx, pdf);
				if (n == 0)
					ui_label("Document changes conform to permissions");
				else
					ui_label("Document permissions violated %d updates ago", n);
			}
		}
	}
	ui_label("Page: %d / %d", fz_page_number_from_location(ctx, doc, currentpage)+1, fz_count_pages(ctx, doc));
	{
		int w = (int)(page_bounds.x1 - page_bounds.x0 + 0.5f);
		int h = (int)(page_bounds.y1 - page_bounds.y0 + 0.5f);
		const char *size = paper_size_name(w, h);
		if (!size)
			size = paper_size_name(h, w);
		if (size)
			ui_label("Size: %d x %d (%s)", w, h, size);
		else
			ui_label("Size: %d x %d", w, h);
	}
	ui_label("ICC rendering: %s.", currenticc ? "on" : "off");
	ui_label("Spot rendering: %s.", currentseparations ? "on" : "off");
	ui_dialog_end();
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
		ui_layout(T, X, NW, 0, 0);
		ui_panel_begin(0, ui.gridsize+8, 4, 4, 1);
		ui_layout(L, NONE, W, 2, 0);
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
		ui_panel_begin(0, ui.gridsize+8, 4, 4, 1);
		ui_layout(L, NONE, W, 2, 0);
		ui_label("Search:");
		ui_layout(ALL, X, E, 2, 0);
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
		ui_panel_begin(0, ui.gridsize, 4, 4, 1);
		ui_layout(L, NONE, W, 2, 0);
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
				search_hit_quads, nelem(search_hit_quads));
			if (search_hit_count)
			{
				search_active = 0;
				search_hit_page = search_page;
				jump_to_location(search_hit_page);
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

	if (!eqloc(oldpage, currentpage) || oldseparations != currentseparations || oldicc != currenticc)
	{
		load_page();
		update_title();
	}

	if (showannotate)
	{
		ui_layout(R, BOTH, NW, 0, 0);
		ui_panel_begin(annotate_w, 0, 4, 4, 1);
		do_annotate_panel();
		ui_panel_end();
	}

	do_canvas();

	if (showinfo)
		do_info();
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
		ui_show_error_dialog("%s", fz_caught_message(ctx));
	ui_end();
}

static void usage(const char *argv0)
{
	fprintf(stderr, "mupdf-gl version %s\n", FZ_VERSION);
	fprintf(stderr, "usage: %s [options] document [page]\n", argv0);
	fprintf(stderr, "\t-p -\tpassword\n");
	fprintf(stderr, "\t-r -\tresolution\n");
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
	exit(1);
}

static int document_filter(const char *filename)
{
	return !!fz_recognize_document(ctx, filename);
}

static void do_open_document_dialog(void)
{
	if (ui_open_file(filename, "Select a document to open:"))
	{
		ui.dialog = NULL;
		if (filename[0] == 0)
			glutLeaveMainLoop();
		else
			load_document();
		if (doc)
		{
			load_page();
			render_page();
			shrinkwrap();
			update_title();
		}
	}
}

static void cleanup(void)
{
	save_history();
	save_accelerator();

	ui_finish();

#ifndef NDEBUG
	if (fz_atoi(getenv("FZ_DEBUG_STORE")))
		fz_debug_store(ctx);
#endif

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
	int c;

#ifndef _WIN32
	signal(SIGHUP, signal_handler);
#endif

	glutInit(&argc, argv);

	screen_w = glutGet(GLUT_SCREEN_WIDTH) - SCREEN_FURNITURE_W;
	screen_h = glutGet(GLUT_SCREEN_HEIGHT) - SCREEN_FURNITURE_H;

	while ((c = fz_getopt(argc, argv, "p:r:IW:H:S:U:XJA:B:C:")) != -1)
	{
		switch (c)
		{
		default: usage(argv[0]); break;
		case 'p': password = fz_optarg; break;
		case 'r': currentzoom = fz_atof(fz_optarg); break;
		case 'I': currentinvert = !currentinvert; break;
		case 'W': layout_w = fz_atof(fz_optarg); break;
		case 'H': layout_h = fz_atof(fz_optarg); break;
		case 'S': layout_em = fz_atof(fz_optarg); break;
		case 'U': layout_css = fz_optarg; break;
		case 'X': layout_use_doc_css = 0; break;
		case 'J': enable_js = !enable_js; break;
		case 'A': currentaa = fz_atoi(fz_optarg); break;
		case 'C': currenttint = 1; tint_white = strtol(fz_optarg, NULL, 16); break;
		case 'B': currenttint = 1; tint_black = strtol(fz_optarg, NULL, 16); break;
		}
	}

	ctx = fz_new_context(NULL, NULL, FZ_STORE_DEFAULT);
	fz_register_document_handlers(ctx);
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

		fz_try(ctx)
		{
			page_tex.w = 600;
			page_tex.h = 700;
			load_document();
			if (doc) load_page();
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
				page_bounds = fz_bound_page(ctx, fzpage);
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
		}

		fz_try(ctx)
		{
			if (doc)
			{
				render_page();
				update_title();
			}
		}
		fz_catch(ctx)
		{
			ui_show_error_dialog("%s", fz_caught_message(ctx));
		}
	}
	else
	{
#ifdef _WIN32
		win_install();
#endif
		ui_init(640, 700, "MuPDF: Open document");
		ui_input_init(&search_input, "");
		ui_init_open_file(".", document_filter);
		ui.dialog = do_open_document_dialog;
	}

	annotate_w *= ui.lineheight;
	outline_w *= ui.lineheight;

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
