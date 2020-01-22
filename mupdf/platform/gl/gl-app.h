#ifdef _WIN32
#include <windows.h>
void win_install(void);
#endif

#include "mupdf/fitz.h"
#include "mupdf/ucdn.h"
#include "mupdf/pdf.h" /* for pdf specifics and forms */

#ifndef __APPLE__
#include <GL/freeglut.h>
#else
#include <GLUT/glut.h>
#endif

/* UI */

enum
{
	/* regular control characters */
	KEY_ESCAPE = 27,
	KEY_ENTER = '\r',
	KEY_TAB = '\t',
	KEY_BACKSPACE = '\b',
	KEY_DELETE = 127,

	KEY_CTL_A = 'A' - 64,
	KEY_CTL_B, KEY_CTL_C, KEY_CTL_D, KEY_CTL_E, KEY_CTL_F,
	KEY_CTL_G, KEY_CTL_H, KEY_CTL_I, KEY_CTL_J, KEY_CTL_K, KEY_CTL_L,
	KEY_CTL_M, KEY_CTL_N, KEY_CTL_O, KEY_CTL_P, KEY_CTL_Q, KEY_CTL_R,
	KEY_CTL_S, KEY_CTL_T, KEY_CTL_U, KEY_CTL_V, KEY_CTL_W, KEY_CTL_X,
	KEY_CTL_Y, KEY_CTL_Z,

	/* reuse control characters > 127 for special keys */
	KEY_INSERT = 128,
	KEY_PAGE_UP,
	KEY_PAGE_DOWN,
	KEY_HOME,
	KEY_END,
	KEY_LEFT,
	KEY_UP,
	KEY_RIGHT,
	KEY_DOWN,
	KEY_F1,
	KEY_F2,
	KEY_F3,
	KEY_F4,
	KEY_F5,
	KEY_F6,
	KEY_F7,
	KEY_F8,
	KEY_F9,
	KEY_F10,
	KEY_F11,
	KEY_F12,
};

enum side { ALL, T, R, B, L };
enum fill { NONE = 0, X = 1, Y = 2, BOTH = 3 };
enum anchor { CENTER, N, NE, E, SE, S, SW, W, NW };

struct layout
{
	enum side side;
	enum fill fill;
	enum anchor anchor;
	int padx, pady;
};

struct ui
{
	int window_w, window_h;

	int x, y;
	int down, down_x, down_y;
	int middle, middle_x, middle_y;
	int right, right_x, right_y;

	int scroll_x, scroll_y;
	int key, mod, plain;

	int grab_down, grab_middle, grab_right;
	const void *hot, *active, *focus;
	int last_cursor, cursor;

	int fontsize;
	int baseline;
	int lineheight;
	int gridsize;

	struct layout *layout;
	fz_irect *cavity;
	struct layout layout_stack[32];
	fz_irect cavity_stack[32];

	int overlay;
	GLuint overlay_list;

	void (*dialog)(void);
};

extern struct ui ui;

void ui_init(int w, int h, const char *title);
void ui_quit(void);
void ui_invalidate(void);
void ui_finish(void);

void ui_set_clipboard(const char *buf);
const char *ui_get_clipboard(void);

void ui_init_fonts(void);
void ui_finish_fonts(void);

void ui_draw_string(float x, float y, const char *str);
void ui_draw_string_part(float x, float y, const char *s, const char *e);
void ui_draw_character(float x, float y, int c);
float ui_measure_character(int ucs);
float ui_measure_string(const char *str);
float ui_measure_string_part(const char *s, const char *e);

struct line { char *a, *b; };

int ui_break_lines(char *a, struct line *lines, int nlines, int width, int *maxwidth);
void ui_draw_lines(float x, float y, struct line *lines, int n);

struct texture
{
	GLuint id;
	int x, y, w, h;
	float s, t;
};

void ui_texture_from_pixmap(struct texture *tex, fz_pixmap *pix);
void ui_draw_image(struct texture *tex, float x, float y);

enum
{
	UI_INPUT_NONE = 0,
	UI_INPUT_EDIT = 1,
	UI_INPUT_ACCEPT = 2,
};

struct input
{
	char text[16*1024];
	char *end, *p, *q;
	int scroll;
};

struct list
{
	fz_irect area;
	int scroll_y;
	int item_y;
	int is_tree;
};

void ui_begin(void);
void ui_end(void);

int ui_mouse_inside(fz_irect area);

void ui_layout(enum side side, enum fill fill, enum anchor anchor, int padx, int pady);
fz_irect ui_pack_layout(int slave_w, int slave_h, enum side side, enum fill fill, enum anchor anchor, int padx, int pady);
fz_irect ui_pack(int slave_w, int slave_h);
int ui_available_width(void);
int ui_available_height(void);
void ui_pack_push(fz_irect cavity);
void ui_pack_pop(void);

void ui_dialog_begin(int w, int h);
void ui_dialog_end(void);
void ui_panel_begin(int w, int h, int padx, int pady, int opaque);
void ui_panel_end(void);

void ui_spacer(void);
void ui_splitter(int *x, int min, int max, enum side side);
void ui_label(const char *fmt, ...);
void ui_label_with_scrollbar(char *text, int width, int height, int *scroll);
int ui_button(const char *label);
int ui_checkbox(const char *label, int *value);
int ui_slider(int *value, int min, int max, int width);
int ui_select(const void *id, const char *current, const char *options[], int n);

void ui_input_init(struct input *input, const char *text);
int ui_input(struct input *input, int width, int height);
void ui_scrollbar(int x0, int y0, int x1, int y1, int *value, int page_size, int max);

void ui_tree_begin(struct list *list, int count, int req_w, int req_h, int is_tree);
int ui_tree_item(struct list *list, const void *id, const char *label, int selected, int depth, int is_branch, int *is_open);
void ui_tree_end(struct list *list);

void ui_list_begin(struct list *list, int count, int req_w, int req_h);
int ui_list_item(struct list *list, const void *id, const char *label, int selected);
void ui_list_end(struct list *list);

int ui_popup(const void *id, const char *label, int is_button, int count);
int ui_popup_item(const char *title);
void ui_popup_end(void);

void ui_init_open_file(const char *dir, int (*filter)(const char *fn));
int ui_open_file(char filename[], const char *label);
void ui_init_save_file(const char *path, int (*filter)(const char *fn));
int ui_save_file(char filename[], void (*extra_panel)(void), const char *label);

void ui_show_warning_dialog(const char *fmt, ...);
void ui_show_error_dialog(const char *fmt, ...);

/* Theming */

enum
{
	UI_COLOR_PANEL = 0xc0c0c0,
	UI_COLOR_BUTTON = 0xc0c0c0,
	UI_COLOR_SCROLLBAR = 0xdfdfdf,
	UI_COLOR_TEXT_BG = 0xffffff,
	UI_COLOR_TEXT_FG = 0x000000,
	UI_COLOR_TEXT_SEL_BG = 0x000080,
	UI_COLOR_TEXT_SEL_FG = 0xffffff,
	UI_COLOR_BEVEL_1 = 0x000000,
	UI_COLOR_BEVEL_2 = 0x808080,
	UI_COLOR_BEVEL_3 = 0xdfdfdf,
	UI_COLOR_BEVEL_4 = 0xffffff,
};

void glColorHex(unsigned int hex);
void ui_draw_bevel(fz_irect area, int depressed);
void ui_draw_ibevel(fz_irect area, int depressed);
void ui_draw_bevel_rect(fz_irect area, unsigned int fill, int depressed);
void ui_draw_ibevel_rect(fz_irect area, unsigned int fill, int depressed);

/* App */

extern fz_context *ctx;
extern pdf_document *pdf;
extern pdf_page *page;
extern fz_stext_page *page_text;
extern pdf_annot *selected_annot;
extern fz_matrix draw_page_ctm, view_page_ctm, view_page_inv_ctm;
extern fz_rect page_bounds, draw_page_bounds, view_page_bounds;
extern fz_irect view_page_area;
extern char filename[];
extern int showform;
extern int showannotate;
extern int reloadrequested;

void toggle_annotate();
void run_main_loop(void);
void do_annotate_panel(void);
void do_annotate_canvas(fz_irect canvas_area);
void do_widget_panel(void);
void do_widget_canvas(fz_irect canvas_area);
void load_page(void);
void render_page(void);
void update_title(void);
void reload(void);
void do_save_pdf_file(void);
void do_save_signed_pdf_file(void);
int do_sign(void);
