#ifndef MUPDF_FITZ_STRUCTURED_TEXT_H
#define MUPDF_FITZ_STRUCTURED_TEXT_H

#include "mupdf/fitz/system.h"
#include "mupdf/fitz/context.h"
#include "mupdf/fitz/math.h"
#include "mupdf/fitz/font.h"
#include "mupdf/fitz/colorspace.h"
#include "mupdf/fitz/image.h"
#include "mupdf/fitz/output.h"
#include "mupdf/fitz/device.h"

/*
	Text extraction device: Used for searching, format conversion etc.

	(In development - Subject to change in future versions)
*/

typedef struct fz_text_style_s fz_text_style;
typedef struct fz_text_char_s fz_text_char;
typedef struct fz_text_span_s fz_text_span;
typedef struct fz_text_line_s fz_text_line;
typedef struct fz_text_block_s fz_text_block;
typedef struct fz_image_block_s fz_image_block;
typedef struct fz_page_block_s fz_page_block;

typedef struct fz_text_sheet_s fz_text_sheet;
typedef struct fz_text_page_s fz_text_page;

/*
	fz_text_sheet: A text sheet contains a list of distinct text styles
	used on a page (or a series of pages).
*/
struct fz_text_sheet_s
{
	int maxid;
	fz_text_style *style;
};

/*
	fz_text_style: A text style contains details of a distinct text style
	used on a page.
*/
struct fz_text_style_s
{
	fz_text_style *next;
	int id;
	fz_font *font;
	float size;
	int wmode;
	int script;
	float ascender;
	float descender;
	/* etc... */
};

/*
	fz_text_page: A text page is a list of page blocks, together with
	an overall bounding box.
*/
struct fz_text_page_s
{
	fz_rect mediabox;
	int len, cap;
	fz_page_block *blocks;
	fz_text_page *next;
};

/*
	fz_page_block: A page block is a typed block pointer.
*/
struct fz_page_block_s
{
	int type;
	union
	{
		fz_text_block *text;
		fz_image_block *image;
	} u;
};

enum
{
	FZ_PAGE_BLOCK_TEXT = 0,
	FZ_PAGE_BLOCK_IMAGE = 1
};

/*
	fz_text_block: A text block is a list of lines of text. In typical
	cases this may correspond to a paragraph or a column of text. A
	collection of blocks makes up a page.
*/
struct fz_text_block_s
{
	fz_rect bbox;
	int len, cap;
	fz_text_line *lines;
};

/*
	fz_image_block: An image block is an image, together with the  list of lines of text. In typical
	cases this may correspond to a paragraph or a column of text. A
	collection of blocks makes up a page.
*/
struct fz_image_block_s
{
	fz_rect bbox;
	fz_matrix mat;
	fz_image *image;
	fz_colorspace *cspace;
	float colors[FZ_MAX_COLORS];
};

/*
	fz_text_line: A text line is a list of text spans, with the same
	baseline. In typical cases this should correspond (as expected) to
	complete lines of text. A collection of lines makes up a block.
*/
struct fz_text_line_s
{
	fz_text_span *first_span, *last_span;

	/* Cached information */
	float distance; /* Perpendicular distance from previous line */
	fz_rect bbox;
	void *region; /* Opaque value for matching line masks */
};

/*
	fz_text_span: A text span is a list of characters that share a common
	baseline/transformation. In typical cases a single span may be enough
	to represent a complete line. In cases where the text has big gaps in
	it (perhaps as it crosses columns or tables), a line may be represented
	by multiple spans.
*/
struct fz_text_span_s
{
	int len, cap;
	fz_text_char *text;
	fz_point min; /* Device space */
	fz_point max; /* Device space */
	int wmode; /* 0 for horizontal, 1 for vertical */
	fz_matrix transform; /* e and f are always 0 here */
	float ascender_max; /* Document space */
	float descender_min; /* Document space */
	fz_rect bbox; /* Device space */

	/* Cached information */
	float base_offset; /* Perpendicular distance from baseline of line */
	float spacing; /* Distance along baseline from previous span in this line (or 0 if first) */
	int column; /* If non zero, the column that it's in */
	float column_width; /* Percentage */
	int align; /* 0 = left, 1 = centre, 2 = right */
	float indent; /* The indent position for this column. */

	fz_text_span *next;
};

/*
	fz_text_char: A text char is a unicode character, the style in which
	is appears, and the point at which it is positioned. Transform
	(and hence bbox) information is given by the enclosing span.
*/
struct fz_text_char_s
{
	fz_point p; /* Device space */
	int c;
	fz_text_style *style;
};

typedef struct fz_char_and_box_s fz_char_and_box;

struct fz_char_and_box_s
{
	int c;
	fz_rect bbox;
};

fz_char_and_box *fz_text_char_at(fz_char_and_box *cab, fz_text_page *page, int idx);

/*
	fz_text_char_bbox: Return the bbox of a text char. Calculated from
	the supplied enclosing span.

	bbox: A place to store the bbox

	span: The enclosing span

	idx: The index of the char within the span

	Returns bbox (updated)

	Does not throw exceptions
*/
fz_rect *fz_text_char_bbox(fz_rect *bbox, fz_text_span *span, int idx);

/*
	fz_new_text_sheet: Create an empty style sheet.

	The style sheet is filled out by the text device, creating
	one style for each unique font, color, size combination that
	is used.
*/
fz_text_sheet *fz_new_text_sheet(fz_context *ctx);
void fz_free_text_sheet(fz_context *ctx, fz_text_sheet *sheet);

/*
	fz_new_text_page: Create an empty text page.

	The text page is filled out by the text device to contain the blocks,
	lines and spans of text on the page.
*/
fz_text_page *fz_new_text_page(fz_context *ctx);
void fz_free_text_page(fz_context *ctx, fz_text_page *page);

void fz_analyze_text(fz_context *ctx, fz_text_sheet *sheet, fz_text_page *page);

/*
	fz_print_text_sheet: Output a text sheet to a file as CSS.
*/
void fz_print_text_sheet(fz_context *ctx, fz_output *out, fz_text_sheet *sheet);

/*
	fz_print_text_page_html: Output a page to a file in HTML format.
*/
void fz_print_text_page_html(fz_context *ctx, fz_output *out, fz_text_page *page);

/*
	fz_print_text_page_xml: Output a page to a file in XML format.
*/
void fz_print_text_page_xml(fz_context *ctx, fz_output *out, fz_text_page *page);

/*
	fz_print_text_page: Output a page to a file in UTF-8 format.
*/
void fz_print_text_page(fz_context *ctx, fz_output *out, fz_text_page *page);

/*
	fz_search_text_page: Search for occurrence of 'needle' in text page.

	Return the number of hits and store hit bboxes in the passed in array.

	NOTE: This is an experimental interface and subject to change without notice.
*/
int fz_search_text_page(fz_context *ctx, fz_text_page *text, const char *needle, fz_rect *hit_bbox, int hit_max);

/*
	fz_highlight_selection: Return a list of rectangles to highlight given a selection rectangle.

	NOTE: This is an experimental interface and subject to change without notice.
*/
int fz_highlight_selection(fz_context *ctx, fz_text_page *page, fz_rect rect, fz_rect *hit_bbox, int hit_max);

/*
	fz_copy_selection: Return a newly allocated UTF-8 string with the text for a given selection rectangle.

	NOTE: This is an experimental interface and subject to change without notice.
*/
char *fz_copy_selection(fz_context *ctx, fz_text_page *page, fz_rect rect);

/*
	fz_new_text_device: Create a device to extract the text on a page.

	Gather and sort the text on a page into spans of uniform style,
	arranged into lines and blocks by reading order. The reading order
	is determined by various heuristics, so may not be accurate.

	sheet: The text sheet to which styles should be added. This can
	either be a newly created (empty) text sheet, or one containing
	styles from a previous text device. The same sheet cannot be used
	in multiple threads simultaneously.

	page: The text page to which content should be added. This will
	usually be a newly created (empty) text page, but it can be one
	containing data already (for example when merging multiple pages, or
	watermarking).
*/
fz_device *fz_new_text_device(fz_context *ctx, fz_text_sheet *sheet, fz_text_page *page);

#endif
