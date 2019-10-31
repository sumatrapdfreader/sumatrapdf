#ifndef SOURCE_XPS_IMP_H
#define SOURCE_XPS_IMP_H

typedef struct xps_document_s xps_document;
typedef struct xps_page_s xps_page;

fz_document *xps_open_document(fz_context *ctx, const char *filename);
fz_document *xps_open_document_with_stream(fz_context *ctx, fz_stream *file);
int xps_count_pages(fz_context *ctx, fz_document *doc, int chapter);
fz_page *xps_load_page(fz_context *ctx, fz_document *doc, int chapter, int number);
fz_outline *xps_load_outline(fz_context *ctx, fz_document *doc);
void xps_run_page(fz_context *ctx, fz_page *page, fz_device *dev, fz_matrix ctm, fz_cookie *cookie);
fz_link *xps_load_links(fz_context *ctx, fz_page *page);
fz_location xps_lookup_link_target(fz_context *ctx, fz_document *doc, const char *target_uri, float *xp, float *yp);

int xps_strcasecmp(char *a, char *b);
void xps_resolve_url(fz_context *ctx, xps_document *doc, char *output, char *base_uri, char *path, int output_size);
char *xps_parse_point(fz_context *ctx, xps_document *doc, char *s_in, float *x, float *y);

typedef struct xps_part_s xps_part;

struct xps_part_s
{
	char *name;
	fz_buffer *data;
};

int xps_has_part(fz_context *ctx, xps_document *doc, char *partname);
xps_part *xps_read_part(fz_context *ctx, xps_document *doc, char *partname);
void xps_drop_part(fz_context *ctx, xps_document *doc, xps_part *part);

typedef struct xps_fixdoc_s xps_fixdoc;
typedef struct xps_fixpage_s xps_fixpage;
typedef struct xps_target_s xps_target;

struct xps_fixdoc_s
{
	char *name;
	char *outline;
	xps_fixdoc *next;
};

struct xps_fixpage_s
{
	char *name;
	int number;
	int width;
	int height;
	xps_fixpage *next;
};

struct xps_page_s
{
	fz_page super;
	xps_document *doc;
	xps_fixpage *fix;
	fz_xml_doc *xml;
};

struct xps_target_s
{
	char *name;
	int page;
	xps_target *next;
};

void xps_read_page_list(fz_context *ctx, xps_document *doc);
void xps_print_page_list(fz_context *ctx, xps_document *doc);
void xps_drop_page_list(fz_context *ctx, xps_document *doc);

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

int xps_count_font_encodings(fz_context *ctx, fz_font *font);
void xps_identify_font_encoding(fz_context *ctx, fz_font *font, int idx, int *pid, int *eid);
void xps_select_font_encoding(fz_context *ctx, fz_font *font, int idx);
int xps_encode_font_char(fz_context *ctx, fz_font *font, int key);

void xps_measure_font_glyph(fz_context *ctx, xps_document *doc, fz_font *font, int gid, xps_glyph_metrics *mtx);

void xps_print_path(fz_context *ctx, xps_document *doc);

void xps_parse_color(fz_context *ctx, xps_document *doc, char *base_uri, char *hexstring, fz_colorspace **csp, float *samples);
void xps_set_color(fz_context *ctx, xps_document *doc, fz_colorspace *colorspace, float *samples);

typedef struct xps_resource_s xps_resource;

struct xps_resource_s
{
	char *name;
	char *base_uri; /* only used in the head nodes */
	fz_xml_doc *base_xml; /* only used in the head nodes, to free the xml document */
	fz_xml *data;
	xps_resource *next;
	xps_resource *parent; /* up to the previous dict in the stack */
};

xps_resource * xps_parse_resource_dictionary(fz_context *ctx, xps_document *doc, char *base_uri, fz_xml *root);
void xps_drop_resource_dictionary(fz_context *ctx, xps_document *doc, xps_resource *dict);
void xps_resolve_resource_reference(fz_context *ctx, xps_document *doc, xps_resource *dict, char **attp, fz_xml **tagp, char **urip);

void xps_print_resource_dictionary(fz_context *ctx, xps_document *doc, xps_resource *dict);

void xps_parse_fixed_page(fz_context *ctx, xps_document *doc, fz_matrix ctm, xps_page *page);
void xps_parse_canvas(fz_context *ctx, xps_document *doc, fz_matrix ctm, fz_rect area, char *base_uri, xps_resource *dict, fz_xml *node);
void xps_parse_path(fz_context *ctx, xps_document *doc, fz_matrix ctm, char *base_uri, xps_resource *dict, fz_xml *node);
void xps_parse_glyphs(fz_context *ctx, xps_document *doc, fz_matrix ctm, char *base_uri, xps_resource *dict, fz_xml *node);
void xps_parse_solid_color_brush(fz_context *ctx, xps_document *doc, fz_matrix ctm, char *base_uri, xps_resource *dict, fz_xml *node);
void xps_parse_image_brush(fz_context *ctx, xps_document *doc, fz_matrix ctm, fz_rect area, char *base_uri, xps_resource *dict, fz_xml *node);
void xps_parse_visual_brush(fz_context *ctx, xps_document *doc, fz_matrix ctm, fz_rect area, char *base_uri, xps_resource *dict, fz_xml *node);
void xps_parse_linear_gradient_brush(fz_context *ctx, xps_document *doc, fz_matrix ctm, fz_rect area, char *base_uri, xps_resource *dict, fz_xml *node);
void xps_parse_radial_gradient_brush(fz_context *ctx, xps_document *doc, fz_matrix ctm, fz_rect area, char *base_uri, xps_resource *dict, fz_xml *node);

void xps_parse_tiling_brush(fz_context *ctx, xps_document *doc, fz_matrix ctm, fz_rect area, char *base_uri, xps_resource *dict, fz_xml *root, void(*func)(fz_context *ctx, xps_document*, fz_matrix, fz_rect, char*, xps_resource*, fz_xml*, void*), void *user);

fz_font *xps_lookup_font(fz_context *ctx, xps_document *doc, char *base_uri, char *font_uri, char *style_att);
fz_text *xps_parse_glyphs_imp(fz_context *ctx, xps_document *doc, fz_matrix ctm,
	fz_font *font, float size, float originx, float originy,
	int is_sideways, int bidi_level,
	char *indices, char *unicode);
fz_path *xps_parse_abbreviated_geometry(fz_context *ctx, xps_document *doc, char *geom, int *fill_rule);
fz_path *xps_parse_path_geometry(fz_context *ctx, xps_document *doc, xps_resource *dict, fz_xml *root, int stroking, int *fill_rule);
fz_matrix xps_parse_transform(fz_context *ctx, xps_document *doc, char *att, fz_xml *tag, fz_matrix ctm);
fz_rect xps_parse_rectangle(fz_context *ctx, xps_document *doc, char *text);

void xps_begin_opacity(fz_context *ctx, xps_document *doc, fz_matrix ctm, fz_rect area, char *base_uri, xps_resource *dict, char *opacity_att, fz_xml *opacity_mask_tag);
void xps_end_opacity(fz_context *ctx, xps_document *doc, char *base_uri, xps_resource *dict, char *opacity_att, fz_xml *opacity_mask_tag);

void xps_parse_brush(fz_context *ctx, xps_document *doc, fz_matrix ctm, fz_rect area, char *base_uri, xps_resource *dict, fz_xml *node);
void xps_parse_element(fz_context *ctx, xps_document *doc, fz_matrix ctm, fz_rect area, char *base_uri, xps_resource *dict, fz_xml *node);

void xps_clip(fz_context *ctx, xps_document *doc, fz_matrix ctm, xps_resource *dict, char *clip_att, fz_xml *clip_tag);

fz_xml *xps_lookup_alternate_content(fz_context *ctx, xps_document *doc, fz_xml *node);

typedef struct xps_entry_s xps_entry;

struct xps_entry_s
{
	char *name;
	int64_t offset;
	int csize;
	int usize;
};

struct xps_document_s
{
	fz_document super;
	fz_archive *zip;

	char *start_part; /* fixed document sequence */
	xps_fixdoc *first_fixdoc; /* first fixed document */
	xps_fixdoc *last_fixdoc; /* last fixed document */
	xps_fixpage *first_page; /* first page of document */
	xps_fixpage *last_page; /* last page of document */
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
	fz_device *dev;
	fz_cookie *cookie;
};

#endif
