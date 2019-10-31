#ifndef MUPDF_FITZ_STRUCTURED_TEXT_H
#define MUPDF_FITZ_STRUCTURED_TEXT_H

#include "mupdf/fitz/system.h"
#include "mupdf/fitz/context.h"
#include "mupdf/fitz/geometry.h"
#include "mupdf/fitz/font.h"
#include "mupdf/fitz/image.h"
#include "mupdf/fitz/output.h"
#include "mupdf/fitz/device.h"

/*
	Simple text layout (for use with annotation editing primarily).
*/
typedef struct fz_layout_char_s fz_layout_char;
typedef struct fz_layout_line_s fz_layout_line;
typedef struct fz_layout_block_s fz_layout_block;

struct fz_layout_char_s
{
	float x, w;
	const char *p; /* location in source text of character */
	fz_layout_char *next;
};

struct fz_layout_line_s
{
	float x, y, h;
	const char *p; /* location in source text of start of line */
	fz_layout_char *text;
	fz_layout_line *next;
};

struct fz_layout_block_s
{
	fz_pool *pool;
	fz_matrix matrix;
	fz_matrix inv_matrix;
	fz_layout_line *head, **tailp;
	fz_layout_char **text_tailp;
};

fz_layout_block *fz_new_layout(fz_context *ctx);
void fz_drop_layout(fz_context *ctx, fz_layout_block *block);
void fz_add_layout_line(fz_context *ctx, fz_layout_block *block, float x, float y, float h, const char *p);
void fz_add_layout_char(fz_context *ctx, fz_layout_block *block, float x, float w, const char *p);

/*
	Text extraction device: Used for searching, format conversion etc.

	(In development - Subject to change in future versions)
*/

typedef struct fz_stext_char_s fz_stext_char;
typedef struct fz_stext_line_s fz_stext_line;
typedef struct fz_stext_block_s fz_stext_block;
typedef struct fz_stext_page_s fz_stext_page;

/*
	FZ_STEXT_PRESERVE_LIGATURES: If this option is activated ligatures
	are passed through to the application in their original form. If
	this option is deactivated ligatures are expanded into their
	constituent parts, e.g. the ligature ffi is expanded into three
	separate characters f, f and i.

	FZ_STEXT_PRESERVE_WHITESPACE: If this option is activated whitespace
	is passed through to the application in its original form. If this
	option is deactivated any type of horizontal whitespace (including
	horizontal tabs) will be replaced with space characters of variable
	width.

	FZ_STEXT_PRESERVE_IMAGES: If this option is set, then images will
	be stored in the structured text structure. The default is to ignore
	all images.

	FZ_STEXT_INHIBIT_SPACES: If this option is set, we will not try to
	add missing space characters where there are large gaps between
	characters.
*/
enum
{
	FZ_STEXT_PRESERVE_LIGATURES = 1,
	FZ_STEXT_PRESERVE_WHITESPACE = 2,
	FZ_STEXT_PRESERVE_IMAGES = 4,
	FZ_STEXT_INHIBIT_SPACES = 8,
};

/*
	A text page is a list of blocks, together with an overall bounding box.
*/
struct fz_stext_page_s
{
	fz_pool *pool;
	fz_rect mediabox;
	fz_stext_block *first_block, *last_block;
};

enum
{
	FZ_STEXT_BLOCK_TEXT = 0,
	FZ_STEXT_BLOCK_IMAGE = 1
};

/*
	A text block is a list of lines of text (typically a paragraph), or an image.
*/
struct fz_stext_block_s
{
	int type;
	fz_rect bbox;
	union {
		struct { fz_stext_line *first_line, *last_line; } t;
		struct { fz_matrix transform; fz_image *image; } i;
	} u;
	fz_stext_block *prev, *next;
};

/*
	A text line is a list of characters that share a common baseline.
*/
struct fz_stext_line_s
{
	int wmode; /* 0 for horizontal, 1 for vertical */
	fz_point dir; /* normalized direction of baseline */
	fz_rect bbox;
	fz_stext_char *first_char, *last_char;
	fz_stext_line *prev, *next;
};

/*
	A text char is a unicode character, the style in which is appears, and
	the point at which it is positioned.
*/
struct fz_stext_char_s
{
	int c;
	int color; /* sRGB hex color */
	fz_point origin;
	fz_quad quad;
	float size;
	fz_font *font;
	fz_stext_char *next;
};

extern const char *fz_stext_options_usage;

fz_stext_page *fz_new_stext_page(fz_context *ctx, fz_rect mediabox);
void fz_drop_stext_page(fz_context *ctx, fz_stext_page *page);

void fz_print_stext_page_as_html(fz_context *ctx, fz_output *out, fz_stext_page *page, int id);
void fz_print_stext_header_as_html(fz_context *ctx, fz_output *out);
void fz_print_stext_trailer_as_html(fz_context *ctx, fz_output *out);

void fz_print_stext_page_as_xhtml(fz_context *ctx, fz_output *out, fz_stext_page *page, int id);
void fz_print_stext_header_as_xhtml(fz_context *ctx, fz_output *out);
void fz_print_stext_trailer_as_xhtml(fz_context *ctx, fz_output *out);

void fz_print_stext_page_as_xml(fz_context *ctx, fz_output *out, fz_stext_page *page, int id);

void fz_print_stext_page_as_text(fz_context *ctx, fz_output *out, fz_stext_page *page);

int fz_search_stext_page(fz_context *ctx, fz_stext_page *text, const char *needle, fz_quad *quads, int max_quads);

int fz_highlight_selection(fz_context *ctx, fz_stext_page *page, fz_point a, fz_point b, fz_quad *quads, int max_quads);

enum
{
	FZ_SELECT_CHARS,
	FZ_SELECT_WORDS,
	FZ_SELECT_LINES,
};

fz_quad fz_snap_selection(fz_context *ctx, fz_stext_page *page, fz_point *ap, fz_point *bp, int mode);

char *fz_copy_selection(fz_context *ctx, fz_stext_page *page, fz_point a, fz_point b, int crlf);

/*
	Options for creating a pixmap and draw device.
*/
typedef struct fz_stext_options_s fz_stext_options;

struct fz_stext_options_s
{
	int flags;
};

fz_stext_options *fz_parse_stext_options(fz_context *ctx, fz_stext_options *opts, const char *string);

fz_device *fz_new_stext_device(fz_context *ctx, fz_stext_page *page, const fz_stext_options *options);

#endif
