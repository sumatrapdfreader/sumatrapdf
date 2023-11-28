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

#ifndef SOURCE_HTML_IMP_H
#define SOURCE_HTML_IMP_H

#include "mupdf/fitz.h"

#include "../fitz/xml-imp.h"

typedef struct fz_html_font_face_s fz_html_font_face;
typedef struct fz_html_font_set_s fz_html_font_set;
typedef struct fz_html_s fz_html;
typedef struct fz_html_box_s fz_html_box;
typedef struct fz_html_flow_s fz_html_flow;
typedef struct fz_css_style_splay_s fz_css_style_splay;

typedef struct fz_css_s fz_css;
typedef struct fz_css_rule_s fz_css_rule;
typedef struct fz_css_match_s fz_css_match;
typedef struct fz_css_style_s fz_css_style;

typedef struct fz_css_selector_s fz_css_selector;
typedef struct fz_css_condition_s fz_css_condition;
typedef struct fz_css_property_s fz_css_property;
typedef struct fz_css_value_s fz_css_value;
typedef struct fz_css_number_s fz_css_number;
typedef struct fz_css_color_s fz_css_color;

struct fz_html_font_face_s
{
	char *family;
	int is_bold;
	int is_italic;
	int is_small_caps;
	fz_font *font;
	char *src;
	fz_html_font_face *next;
};

struct fz_html_font_set_s
{
	fz_font *fonts[12]; /* Times, Helvetica, Courier in R,I,B,BI */
	fz_html_font_face *custom;
};

#define UCS_MAX 0x10ffff

enum
{
	CSS_KEYWORD = UCS_MAX+1,
	CSS_HASH,
	CSS_STRING,
	CSS_NUMBER,
	CSS_LENGTH,
	CSS_PERCENT,
	CSS_URI,
};

struct fz_css_s
{
	fz_pool *pool;
	fz_css_rule *rule;
};

struct fz_css_rule_s
{
	fz_css_selector *selector;
	fz_css_property *declaration;
	fz_css_rule *next;
	int loaded;
};

struct fz_css_selector_s
{
	char *name;
	int combine;
	fz_css_condition *cond;
	fz_css_selector *left;
	fz_css_selector *right;
	fz_css_selector *next;
};

struct fz_css_condition_s
{
	int type;
	char *key;
	char *val;
	fz_css_condition *next;
};

struct fz_css_property_s
{
	int name;
	fz_css_value *value;
	short spec;
	short important;
	fz_css_property *next;
};

struct fz_css_value_s
{
	int type;
	char *data;
	fz_css_value *args; /* function arguments */
	fz_css_value *next;
};

enum
{
	PRO_BACKGROUND_COLOR,
	PRO_BORDER_BOTTOM_COLOR,
	PRO_BORDER_BOTTOM_STYLE,
	PRO_BORDER_BOTTOM_WIDTH,
	PRO_BORDER_LEFT_COLOR,
	PRO_BORDER_LEFT_STYLE,
	PRO_BORDER_LEFT_WIDTH,
	PRO_BORDER_RIGHT_COLOR,
	PRO_BORDER_RIGHT_STYLE,
	PRO_BORDER_RIGHT_WIDTH,
	PRO_BORDER_TOP_COLOR,
	PRO_BORDER_TOP_STYLE,
	PRO_BORDER_TOP_WIDTH,
	PRO_BORDER_SPACING,
	PRO_COLOR,
	PRO_DIRECTION,
	PRO_DISPLAY,
	PRO_FONT_FAMILY,
	PRO_FONT_SIZE,
	PRO_FONT_STYLE,
	PRO_FONT_VARIANT,
	PRO_FONT_WEIGHT,
	PRO_HEIGHT,
	PRO_LEADING,
	PRO_LETTER_SPACING,
	PRO_LINE_HEIGHT,
	PRO_LIST_STYLE_IMAGE,
	PRO_LIST_STYLE_POSITION,
	PRO_LIST_STYLE_TYPE,
	PRO_MARGIN_BOTTOM,
	PRO_MARGIN_LEFT,
	PRO_MARGIN_RIGHT,
	PRO_MARGIN_TOP,
	PRO_ORPHANS,
	PRO_OVERFLOW_WRAP,
	PRO_PADDING_BOTTOM,
	PRO_PADDING_LEFT,
	PRO_PADDING_RIGHT,
	PRO_PADDING_TOP,
	PRO_PAGE_BREAK_AFTER,
	PRO_PAGE_BREAK_BEFORE,
	PRO_QUOTES,
	PRO_SRC,
	PRO_TEXT_ALIGN,
	PRO_TEXT_DECORATION,
	PRO_TEXT_INDENT,
	PRO_TEXT_TRANSFORM,
	PRO_VERTICAL_ALIGN,
	PRO_VISIBILITY,
	PRO_WHITE_SPACE,
	PRO_WIDOWS,
	PRO_WIDTH,
	PRO_WORD_SPACING,

	/* Number of real properties. */
	NUM_PROPERTIES,

	/* Short-hand properties (always expanded when applied, never used as is): */
	PRO_BORDER,
	PRO_BORDER_BOTTOM,
	PRO_BORDER_COLOR,
	PRO_BORDER_LEFT,
	PRO_BORDER_RIGHT,
	PRO_BORDER_STYLE,
	PRO_BORDER_TOP,
	PRO_BORDER_WIDTH,
	PRO_LIST_STYLE,
	PRO_MARGIN,
	PRO_PADDING,
};

struct fz_css_match_s
{
	fz_css_match *up;
	short spec[NUM_PROPERTIES];
	fz_css_value *value[NUM_PROPERTIES];
};

enum { DIS_NONE, DIS_BLOCK, DIS_INLINE, DIS_LIST_ITEM, DIS_INLINE_BLOCK, DIS_TABLE, DIS_TABLE_GROUP, DIS_TABLE_ROW, DIS_TABLE_CELL };
enum { POS_STATIC, POS_RELATIVE, POS_ABSOLUTE, POS_FIXED };
enum { TA_LEFT, TA_RIGHT, TA_CENTER, TA_JUSTIFY };
enum { VA_BASELINE, VA_SUB, VA_SUPER, VA_TOP, VA_BOTTOM, VA_TEXT_TOP, VA_TEXT_BOTTOM };
enum { BS_NONE, BS_SOLID };
enum { V_VISIBLE, V_HIDDEN, V_COLLAPSE };
enum { PB_AUTO, PB_ALWAYS, PB_AVOID, PB_LEFT, PB_RIGHT };
enum { TD_NONE, TD_UNDERLINE, TD_LINE_THROUGH };

enum {
	WS_COLLAPSE = 1,
	WS_ALLOW_BREAK_SPACE = 2,
	WS_FORCE_BREAK_NEWLINE = 4,
	WS_NORMAL = WS_COLLAPSE | WS_ALLOW_BREAK_SPACE,
	WS_PRE = WS_FORCE_BREAK_NEWLINE,
	WS_NOWRAP = WS_COLLAPSE,
	WS_PRE_WRAP = WS_ALLOW_BREAK_SPACE | WS_FORCE_BREAK_NEWLINE,
	WS_PRE_LINE = WS_COLLAPSE | WS_ALLOW_BREAK_SPACE | WS_FORCE_BREAK_NEWLINE
};

enum {
	LST_NONE,
	LST_DISC, LST_CIRCLE, LST_SQUARE,
	LST_DECIMAL, LST_DECIMAL_ZERO,
	LST_LC_ROMAN, LST_UC_ROMAN,
	LST_LC_GREEK, LST_UC_GREEK,
	LST_LC_LATIN, LST_UC_LATIN,
	LST_LC_ALPHA, LST_UC_ALPHA,
	LST_ARMENIAN, LST_GEORGIAN,
};

enum {
	OVERFLOW_WRAP_NORMAL = 0,
	OVERFLOW_WRAP_BREAK_WORD = 1
	/* We do not support 'anywhere'. */
};

enum { N_NUMBER='u', N_LENGTH='p', N_SCALE='m', N_PERCENT='%', N_AUTO='a', N_UNDEFINED='x' };

struct fz_css_number_s
{
	float value;
	int unit;
};

struct fz_css_color_s
{
	unsigned char r, g, b, a;
};

struct fz_css_style_s
{
	fz_css_number font_size;
	fz_css_number width, height;
	fz_css_number margin[4];
	fz_css_number padding[4];
	fz_css_number border_width[4];
	fz_css_number border_spacing;
	fz_css_number text_indent;
	unsigned int visibility : 2;
	unsigned int white_space : 3;
	unsigned int text_align : 2;
	unsigned int vertical_align : 3;
	unsigned int list_style_type : 4;
	unsigned int page_break_before : 3;
	unsigned int page_break_after : 3;
	unsigned int border_style_0 : 1;
	unsigned int border_style_1 : 1;
	unsigned int border_style_2 : 1;
	unsigned int border_style_3 : 1;
	unsigned int small_caps : 1;
	unsigned int text_decoration: 2;
	unsigned int overflow_wrap : 1;
	/* Ensure the extra bits in the bitfield are copied
	 * on structure copies. */
	unsigned int blank : 3;
	fz_css_number line_height;
	fz_css_number leading;
	fz_css_color background_color;
	fz_css_color border_color[4];
	fz_css_color color;
	fz_font *font;
};

struct fz_css_style_splay_s {
	fz_css_style style;
	fz_css_style_splay *lt;
	fz_css_style_splay *gt;
	fz_css_style_splay *up;
};

enum
{
	BOX_BLOCK,		/* block-level: contains block, break, flow, and table boxes */
	BOX_FLOW,		/* block-level: contains only inline boxes */
	BOX_INLINE,		/* inline-level: contains only inline boxes */
	BOX_TABLE,		/* table: contains table-row */
	BOX_TABLE_ROW,		/* table-row: contains table-cell */
	BOX_TABLE_CELL,		/* table-cell: contains block */
};

typedef struct
{
	fz_storable storable;
	fz_pool *pool; /* pool allocator for this html tree */
	fz_html_box *root;
} fz_html_tree;

struct fz_html_s
{
	/* fz_html is derived from fz_html_tree, so must start with that. */
	/* Arguably 'tree' should be called 'super'. */
	fz_html_tree tree;

	float page_w, page_h;
	float layout_w, layout_h, layout_em;
	float page_margin[4];
	char *title;
};

typedef struct {
	/* start will be filled in on entry with the first node to start
	 * operation on. NULL means start 'immediately'. As we traverse
	 * the tree, once we reach the node to start on, we set this to
	 * NULL, hence if 'start != NULL' then we are still skipping to
	 * find the starting node. */
	fz_html_box *start;

	/* If start is a BOX_FLOW, then start_flow will be the flow entry
	 * at which we should start. */
	fz_html_flow *start_flow;


	/* end should be NULL on entry. On exit, if it's NULL, then we
	 * finished. Otherwise, this is where we should restart the
	 * process the next time. */
	fz_html_box *end;

	/* If end is a BOX_FLOW, then end_flow will be the flow entry at which
	 * we should restart next time. */
	fz_html_flow *end_flow;


	/* Workspace used on the traversal of the tree to store a good place
	 * to restart. Typically this will be set to an enclosing box with
	 * a border, so that if we then fail to put any content into the box
	 * we'll elide the entire box/border, not output an empty one. */
	fz_html_box *potential;
} fz_html_restarter;

struct fz_story
{
	/* fz_story is derived from fz_html_tree, so must start with */
	/* that. Argubly 'tree' should be called 'super'. */
	fz_html_tree tree;

	/* The user_css (or NULL) */
	char *user_css;

	/* The HTML story as XML nodes with a DOM */
	fz_xml *dom;

	/* The fontset for the content. */
	fz_html_font_set *font_set;

	/* restart_place holds the start position for the next place.
	 * This is updated by draw. */
	fz_html_restarter restart_place;

	/* restart_draw holds the start position for the next draw.
	 * This is updated by place. */
	fz_html_restarter restart_draw;

	/* complete is set true when all the story has been placed and
	 * drawn. */
	int complete;

	/* The last bbox we laid out for. Used for making a clipping
	 * rectangle. */
	fz_rect bbox;

	/* The default 'em' size. */
	float em;

	/* Collected parsing warnings. */
	fz_buffer *warnings;

	/* Rectangle layout count. */
	int rect_count;

	/* Archive from which to load any resources. */
	fz_archive *zip;
};

struct fz_html_box_s
{
	unsigned int type : 3;
	unsigned int is_first_flow : 1; /* for text-indent */
	unsigned int markup_dir : 2;
	unsigned int heading : 3;
	unsigned int list_item : 21;

	fz_html_box *up, *down, *next;

	const char *tag, *id, *href;
	const fz_css_style *style;

	union {
		/* Only needed during build stage */
		struct {
			fz_html_box *last_child;
			fz_html_flow **flow_tail;
		} build;

		/* Only needed during layout */
		struct {
			float x, y, w, b; /* content */
			float em;
		} layout;
	} s;

	union {
		/* Only BOX_FLOW use the following */
		struct {
			fz_html_flow *head;
		} flow;

		/* Only BOX_{BLOCK,TABLE,TABLE_ROW,TABLE_CELL} use the following */
		struct {
			float margin[4]; // TODO: is margin needed post layout?
			float border[4];
			float padding[4];
		} block;
	} u;
};

static inline int
fz_html_box_has_boxes(fz_html_box *box)
{
	return (box->type == BOX_BLOCK || box->type == BOX_TABLE || box->type == BOX_TABLE_ROW || box->type == BOX_TABLE_CELL);
}

enum
{
	FLOW_WORD = 0,
	FLOW_SPACE = 1,
	FLOW_BREAK = 2,
	FLOW_IMAGE = 3,
	FLOW_SBREAK = 4,
	FLOW_SHYPHEN = 5,
	FLOW_ANCHOR = 6
};

struct fz_html_flow_s
{
	/* What type of node */
	unsigned int type : 3;

	/* Whether this should expand during justification */
	unsigned int expand : 1;

	/* Whether this node is currently taken as a line break */
	unsigned int breaks_line : 1;

	/* Whether this word node can be split or consists of a single glyph cluster */
	unsigned int atomic : 1;

	/* Whether lines may be broken before this word for overflow-wrap: word-break */
	unsigned int overflow_wrap : 1;

	/* Direction setting for text - UAX#9 says 125 is the max */
	unsigned int bidi_level : 7;

	/* The script detected by the bidi code. */
	unsigned int script : 8;

	/* Whether the markup specifies a given language. */
	unsigned short markup_lang;

	float x, y, w, h;
	fz_html_box *box; /* for style and em */
	fz_html_flow *next;
	union {
		char text[1];
		fz_image *image;
	} content;
};


fz_css *fz_new_css(fz_context *ctx);
void fz_parse_css(fz_context *ctx, fz_css *css, const char *source, const char *file);
fz_css_property *fz_parse_css_properties(fz_context *ctx, fz_pool *pool, const char *source);
void fz_drop_css(fz_context *ctx, fz_css *css);
void fz_debug_css(fz_context *ctx, fz_css *css);
const char *fz_css_property_name(int name);

void fz_match_css(fz_context *ctx, fz_css_match *match, fz_css_match *up, fz_css *css, fz_xml *node);
void fz_match_css_at_page(fz_context *ctx, fz_css_match *match, fz_css *css);

int fz_get_css_match_display(fz_css_match *node);
void fz_default_css_style(fz_context *ctx, fz_css_style *style);
void fz_apply_css_style(fz_context *ctx, fz_html_font_set *set, fz_css_style *style, fz_css_match *match);

/*
	Lookup style in the splay tree, returning a pointer
	to the found instance if there is one, creating and
	inserting (and moving to root) one if there is not.
*/
const fz_css_style *fz_css_enlist(fz_context *ctx, const fz_css_style *style, fz_css_style_splay **tree, fz_pool *pool);

float fz_from_css_number(fz_css_number number, float em, float percent_value, float auto_value);
float fz_from_css_number_scale(fz_css_number number, float scale);
int fz_css_number_defined(fz_css_number number);

fz_html_font_set *fz_new_html_font_set(fz_context *ctx);
void fz_add_html_font_face(fz_context *ctx, fz_html_font_set *set,
	const char *family, int is_bold, int is_italic, int is_small_caps, const char *src, fz_font *font);
fz_font *fz_load_html_font(fz_context *ctx, fz_html_font_set *set, const char *family, int is_bold, int is_italic, int is_small_caps);
void fz_drop_html_font_set(fz_context *ctx, fz_html_font_set *htx);

void fz_add_css_font_faces(fz_context *ctx, fz_html_font_set *set, fz_archive *zip, const char *base_uri, fz_css *css);

fz_html *fz_parse_fb2(fz_context *ctx, fz_html_font_set *htx, fz_archive *zip, const char *base_uri, fz_buffer *buf, const char *user_css);
fz_html *fz_parse_html5(fz_context *ctx, fz_html_font_set *htx, fz_archive *zip, const char *base_uri, fz_buffer *buf, const char *user_css);
fz_html *fz_parse_xhtml(fz_context *ctx, fz_html_font_set *htx, fz_archive *zip, const char *base_uri, fz_buffer *buf, const char *user_css);
fz_html *fz_parse_mobi(fz_context *ctx, fz_html_font_set *htx, fz_archive *zip, const char *base_uri, fz_buffer *buf, const char *user_css);
fz_html *fz_parse_txt(fz_context *ctx, fz_html_font_set *htx, fz_archive *zip, const char *base_uri, fz_buffer *buf, const char *user_css);
fz_html *fz_parse_office(fz_context *ctx, fz_html_font_set *set, fz_archive *zip, const char *base_uri, fz_buffer *buf, const char *user_css);

/* Defaults are all 0's. FIXME: Very subject to change. Possibly might be removed entirely. */
typedef struct
{
	int output_page_numbers;
	int output_sheet_names;
	int output_cell_markers;
	int output_cell_row_markers;
	int output_cell_names;
	int output_formatting;
	int output_filenames;
	int output_errors;
}
fz_office_to_html_opts;

/*
 * Returns html representation of office archive in `buf`.
 */
fz_buffer *fz_office_to_html(fz_context *ctx, fz_html_font_set *set, fz_buffer *buf, const char *user_css, fz_office_to_html_opts *opts);

void fz_layout_html(fz_context *ctx, fz_html *html, float w, float h, float em);
void fz_draw_html(fz_context *ctx, fz_device *dev, fz_matrix ctm, fz_html *html, int page);
fz_outline *fz_load_html_outline(fz_context *ctx, fz_html *node);

float fz_find_html_target(fz_context *ctx, fz_html *html, const char *id);
fz_link *fz_load_html_links(fz_context *ctx, fz_html *html, int page, const char *base_uri);
fz_html *fz_keep_html(fz_context *ctx, fz_html *html);
void fz_drop_html(fz_context *ctx, fz_html *html);
fz_bookmark fz_make_html_bookmark(fz_context *ctx, fz_html *html, int page);
int fz_lookup_html_bookmark(fz_context *ctx, fz_html *html, fz_bookmark mark);
void fz_debug_html(fz_context *ctx, fz_html_box *box);

fz_html *fz_store_html(fz_context *ctx, fz_html *html, void *doc, int chapter);
fz_html *fz_find_html(fz_context *ctx, void *doc, int chapter);
void fz_purge_stored_html(fz_context *ctx, void *doc);

void fz_restartable_layout_html(fz_context *ctx, fz_html_tree *tree, float start_x, float start_y, float page_w, float page_h, float em, fz_html_restarter *restart);

fz_html_flow *fz_html_split_flow(fz_context *ctx, fz_pool *pool, fz_html_flow *flow, size_t offset);

fz_archive *fz_extract_html_from_mobi(fz_context *ctx, fz_buffer *mobi);

fz_structure fz_html_tag_to_structure(const char *tag);

#endif
