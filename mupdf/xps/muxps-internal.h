#ifndef MUXPS_INTERNAL_H
#define MUXPS_INTERNAL_H

#include "muxps.h"
#include "fitz-internal.h"

typedef unsigned char byte;

/*
 * Memory, and string functions.
 */

int xps_strcasecmp(char *a, char *b);
void xps_resolve_url(char *output, char *base_uri, char *path, int output_size);
int xps_url_is_remote(char *path);
char *xps_parse_point(char *s_in, float *x, float *y);

/*
 * Container parts.
 */

typedef struct xps_part_s xps_part;

struct xps_part_s
{
	char *name;
	int size;
	int cap;
	byte *data;
};

xps_part *xps_new_part(xps_document *doc, char *name, int size);
int xps_has_part(xps_document *doc, char *partname);
xps_part *xps_read_part(xps_document *doc, char *partname);
void xps_free_part(xps_document *doc, xps_part *part);

/*
 * Document structure.
 */

typedef struct xps_fixdoc_s xps_fixdoc;
typedef struct xps_target_s xps_target;

struct xps_fixdoc_s
{
	char *name;
	char *outline;
	xps_fixdoc *next;
};

struct xps_page_s
{
	char *name;
	int number;
	int width;
	int height;
	fz_xml *root;
	int links_resolved;
	fz_link *links;
	xps_page *next;
};

struct xps_target_s
{
	char *name;
	int page;
	xps_target *next;
	fz_rect rect; /* SumatraPDF: extended link support */
};

void xps_read_page_list(xps_document *doc);
void xps_print_page_list(xps_document *doc);
void xps_free_page_list(xps_document *doc);

int xps_count_pages(xps_document *doc);
xps_page *xps_load_page(xps_document *doc, int number);
fz_link *xps_load_links(xps_document *doc, xps_page *page);
fz_rect *xps_bound_page(xps_document *doc, xps_page *page, fz_rect *rect);
void xps_free_page(xps_document *doc, xps_page *page);
/* SumatraPDF: extract page bounds without parsing the entire page content */
fz_rect xps_bound_page_quick_and_dirty(xps_document *doc, int number);

fz_outline *xps_load_outline(xps_document *doc);

int xps_lookup_link_target(xps_document *doc, char *target_uri);
void xps_add_link(xps_document *doc, const fz_rect *area, char *base_uri, char *target_uri);

/* SumatraPDF: extended link support */
xps_target *xps_lookup_link_target_obj(xps_document *doc, char *target_uri);

/*
 * Images, fonts, and colorspaces.
 */

typedef struct xps_font_cache_s xps_font_cache;

struct xps_font_cache_s
{
	char *name;
	fz_font *font;
	xps_font_cache *next;
};

typedef struct xps_glyph_metrics_s xps_glyph_metrics;

struct xps_glyph_metrics_s
{
	float hadv, vadv, vorg;
};

int xps_count_font_encodings(fz_font *font);
void xps_identify_font_encoding(fz_font *font, int idx, int *pid, int *eid);
void xps_select_font_encoding(fz_font *font, int idx);
int xps_encode_font_char(fz_font *font, int key);

void xps_measure_font_glyph(xps_document *doc, fz_font *font, int gid, xps_glyph_metrics *mtx);

void xps_print_path(xps_document *doc);

void xps_parse_color(xps_document *doc, char *base_uri, char *hexstring, fz_colorspace **csp, float *samples);
void xps_set_color(xps_document *doc, fz_colorspace *colorspace, float *samples);

/*
 * Resource dictionaries.
 */

typedef struct xps_resource_s xps_resource;

struct xps_resource_s
{
	char *name;
	char *base_uri; /* only used in the head nodes */
	fz_xml *base_xml; /* only used in the head nodes, to free the xml document */
	fz_xml *data;
	xps_resource *next;
	xps_resource *parent; /* up to the previous dict in the stack */
};

xps_resource * xps_parse_resource_dictionary(xps_document *doc, char *base_uri, fz_xml *root);
void xps_free_resource_dictionary(xps_document *doc, xps_resource *dict);
void xps_resolve_resource_reference(xps_document *doc, xps_resource *dict, char **attp, fz_xml **tagp, char **urip);

void xps_print_resource_dictionary(xps_resource *dict);

/*
 * Fixed page/graphics parsing.
 */

void xps_parse_fixed_page(xps_document *doc, const fz_matrix *ctm, xps_page *page);
void xps_parse_canvas(xps_document *doc, const fz_matrix *ctm, const fz_rect *area, char *base_uri, xps_resource *dict, fz_xml *node);
void xps_parse_path(xps_document *doc, const fz_matrix *ctm, char *base_uri, xps_resource *dict, fz_xml *node);
void xps_parse_glyphs(xps_document *doc, const fz_matrix *ctm, char *base_uri, xps_resource *dict, fz_xml *node);
void xps_parse_solid_color_brush(xps_document *doc, const fz_matrix *ctm, char *base_uri, xps_resource *dict, fz_xml *node);
void xps_parse_image_brush(xps_document *doc, const fz_matrix *ctm, const fz_rect *area, char *base_uri, xps_resource *dict, fz_xml *node);
void xps_parse_visual_brush(xps_document *doc, const fz_matrix *ctm, const fz_rect *area, char *base_uri, xps_resource *dict, fz_xml *node);
void xps_parse_linear_gradient_brush(xps_document *doc, const fz_matrix *ctm, const fz_rect *area, char *base_uri, xps_resource *dict, fz_xml *node);
void xps_parse_radial_gradient_brush(xps_document *doc, const fz_matrix *ctm, const fz_rect *area, char *base_uri, xps_resource *dict, fz_xml *node);

void xps_parse_tiling_brush(xps_document *doc, const fz_matrix *ctm, const fz_rect *area, char *base_uri, xps_resource *dict, fz_xml *root, void(*func)(xps_document*, const fz_matrix *, const fz_rect *, char*, xps_resource*, fz_xml*, void*), void *user);

void xps_parse_matrix_transform(xps_document *doc, fz_xml *root, fz_matrix *matrix);
void xps_parse_render_transform(xps_document *doc, char *text, fz_matrix *matrix);
void xps_parse_rectangle(xps_document *doc, char *text, fz_rect *rect);

void xps_begin_opacity(xps_document *doc, const fz_matrix *ctm, const fz_rect *area, char *base_uri, xps_resource *dict, char *opacity_att, fz_xml *opacity_mask_tag);
void xps_end_opacity(xps_document *doc, char *base_uri, xps_resource *dict, char *opacity_att, fz_xml *opacity_mask_tag);

void xps_parse_brush(xps_document *doc, const fz_matrix *ctm, const fz_rect *area, char *base_uri, xps_resource *dict, fz_xml *node);
void xps_parse_element(xps_document *doc, const fz_matrix *ctm, const fz_rect *area, char *base_uri, xps_resource *dict, fz_xml *node);

void xps_clip(xps_document *doc, const fz_matrix *ctm, xps_resource *dict, char *clip_att, fz_xml *clip_tag);

fz_xml *xps_lookup_alternate_content(fz_xml *node);

/*
 * The interpreter context.
 */

typedef struct xps_entry_s xps_entry;

struct xps_entry_s
{
	char *name;
	int offset;
	int csize;
	int usize;
};

struct xps_document_s
{
	fz_document super;

	fz_context *ctx;
	char *directory;
	fz_stream *file;
	int zip_count;
	xps_entry *zip_table;

	char *start_part; /* fixed document sequence */
	xps_fixdoc *first_fixdoc; /* first fixed document */
	xps_fixdoc *last_fixdoc; /* last fixed document */
	xps_page *first_page; /* first page of document */
	xps_page *last_page; /* last page of document */
	int page_count;

	xps_target *target; /* link targets */

	char *base_uri; /* base uri for parsing XML and resolving relative paths */
	char *part_uri; /* part uri for parsing metadata relations */

	/* We cache font resources */
	xps_font_cache *font_table;

	/* Opacity attribute stack */
	float opacity[64];
	int opacity_top;

	/* Current color */
	fz_colorspace *colorspace;
	float color[8];
	float alpha;

	/* Current device */
	fz_cookie *cookie;
	fz_device *dev;

	/* Current page we are loading */
	xps_page *current_page;

	/* SumatraPDF: better canvas bounds estimates for links/targets */
	struct {
		fz_link *link;
		fz_rect rect;
	} _clinks[10];
	int _clinks_len;
};

/* SumatraPDF: extended link support */
void xps_extract_anchor_info(xps_document *doc, const fz_rect *rect, char *target_uri, char *anchor_name, int step);

/* SumatraPDF: extract document properties (hacky) */
typedef struct xps_doc_props_s
{
	char *title;
	char *author;
	char *subject;
	char *creation_date;
	char *modification_date;
} xps_doc_props;
xps_doc_props *xps_extract_doc_props(xps_document *doc);
void xps_free_doc_props(fz_context *ctx, xps_doc_props *props);

#endif
