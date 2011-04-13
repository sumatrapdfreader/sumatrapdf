#ifndef _MUXPS_H_
#define _MUXPS_H_

#ifndef _FITZ_H_
#error "fitz.h must be included before muxps.h"
#endif

typedef unsigned char byte;

/*
 * XPS and ZIP constants.
 */

typedef struct xps_context_s xps_context;

#define REL_START_PART \
	"http://schemas.microsoft.com/xps/2005/06/fixedrepresentation"
#define REL_REQUIRED_RESOURCE \
	"http://schemas.microsoft.com/xps/2005/06/required-resource"
#define REL_REQUIRED_RESOURCE_RECURSIVE \
	"http://schemas.microsoft.com/xps/2005/06/required-resource#recursive"

#define ZIP_LOCAL_FILE_SIG 0x04034b50
#define ZIP_DATA_DESC_SIG 0x08074b50
#define ZIP_CENTRAL_DIRECTORY_SIG 0x02014b50
#define ZIP_END_OF_CENTRAL_DIRECTORY_SIG 0x06054b50

/*
 * Memory, and string functions.
 */

int xps_strcasecmp(char *a, char *b);
void xps_absolute_path(char *output, char *base_uri, char *path, int output_size);

/*
 * XML document model
 */

typedef struct element xml_element;

xml_element *xml_parse_document(byte *buf, int len);
xml_element *xml_next(xml_element *item);
xml_element *xml_down(xml_element *item);
char *xml_tag(xml_element *item);
char *xml_att(xml_element *item, const char *att);
void xml_free_element(xml_element *item);
void xml_print_element(xml_element *item, int level);

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

xps_part *xps_new_part(xps_context *ctx, char *name, int size);
xps_part *xps_read_part(xps_context *ctx, char *partname);
void xps_free_part(xps_context *ctx, xps_part *part);

/*
 * Document structure.
 */

typedef struct xps_document_s xps_document;
typedef struct xps_page_s xps_page;

struct xps_document_s
{
	char *name;
	xps_document *next;
};

struct xps_page_s
{
	char *name;
	int width;
	int height;
	xml_element *root;
	xps_page *next;
};

int xps_read_page_list(xps_context *ctx);
void xps_debug_page_list(xps_context *ctx);
void xps_free_page_list(xps_context *ctx);

int xps_count_pages(xps_context *ctx);
int xps_load_page(xps_page **page, xps_context *ctx, int number);
void xps_free_page(xps_context *ctx, xps_page *page);

/*
 * Images, fonts, and colorspaces.
 */

int xps_decode_jpeg(fz_pixmap **imagep, byte *rbuf, int rlen);
int xps_decode_png(fz_pixmap **imagep, byte *rbuf, int rlen);
int xps_decode_tiff(fz_pixmap **imagep, byte *rbuf, int rlen);

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

void xps_measure_font_glyph(xps_context *ctx, fz_font *font, int gid, xps_glyph_metrics *mtx);

void xps_debug_path(xps_context *ctx);

void xps_parse_color(xps_context *ctx, char *base_uri, char *hexstring, fz_colorspace **csp, float *samples);
void xps_set_color(xps_context *ctx, fz_colorspace *colorspace, float *samples);

/*
 * Resource dictionaries.
 */

typedef struct xps_resource_s xps_resource;

struct xps_resource_s
{
	char *name;
	char *base_uri; /* only used in the head nodes */
	xml_element *base_xml; /* only used in the head nodes, to free the xml document */
	xml_element *data;
	xps_resource *next;
	xps_resource *parent; /* up to the previous dict in the stack */
};

int xps_parse_resource_dictionary(xps_context *ctx, xps_resource **dictp, char *base_uri, xml_element *root);
void xps_free_resource_dictionary(xps_context *ctx, xps_resource *dict);
void xps_resolve_resource_reference(xps_context *ctx, xps_resource *dict, char **attp, xml_element **tagp, char **urip);

void xps_debug_resource_dictionary(xps_resource *dict);

/*
 * Fixed page/graphics parsing.
 */

void xps_parse_fixed_page(xps_context *ctx, fz_matrix ctm, xps_page *page);
void xps_parse_canvas(xps_context *ctx, fz_matrix ctm, fz_rect area, char *base_uri, xps_resource *dict, xml_element *node);
void xps_parse_path(xps_context *ctx, fz_matrix ctm, char *base_uri, xps_resource *dict, xml_element *node);
void xps_parse_glyphs(xps_context *ctx, fz_matrix ctm, char *base_uri, xps_resource *dict, xml_element *node);
void xps_parse_solid_color_brush(xps_context *ctx, fz_matrix ctm, char *base_uri, xps_resource *dict, xml_element *node);
void xps_parse_image_brush(xps_context *ctx, fz_matrix ctm, fz_rect area, char *base_uri, xps_resource *dict, xml_element *node);
void xps_parse_visual_brush(xps_context *ctx, fz_matrix ctm, fz_rect area, char *base_uri, xps_resource *dict, xml_element *node);
void xps_parse_linear_gradient_brush(xps_context *ctx, fz_matrix ctm, fz_rect area, char *base_uri, xps_resource *dict, xml_element *node);
void xps_parse_radial_gradient_brush(xps_context *ctx, fz_matrix ctm, fz_rect area, char *base_uri, xps_resource *dict, xml_element *node);

void xps_parse_tiling_brush(xps_context *ctx, fz_matrix ctm, fz_rect area, char *base_uri, xps_resource *dict, xml_element *root, void(*func)(xps_context*, fz_matrix, fz_rect, char*, xps_resource*, xml_element*, void*), void *user);

void xps_parse_matrix_transform(xps_context *ctx, xml_element *root, fz_matrix *matrix);
void xps_parse_render_transform(xps_context *ctx, char *text, fz_matrix *matrix);
void xps_parse_rectangle(xps_context *ctx, char *text, fz_rect *rect);

void xps_begin_opacity(xps_context *ctx, fz_matrix ctm, fz_rect area, char *base_uri, xps_resource *dict, char *opacity_att, xml_element *opacity_mask_tag);
void xps_end_opacity(xps_context *ctx, char *base_uri, xps_resource *dict, char *opacity_att, xml_element *opacity_mask_tag);

void xps_parse_brush(xps_context *ctx, fz_matrix ctm, fz_rect area, char *base_uri, xps_resource *dict, xml_element *node);
void xps_parse_element(xps_context *ctx, fz_matrix ctm, fz_rect area, char *base_uri, xps_resource *dict, xml_element *node);

void xps_clip(xps_context *ctx, fz_matrix ctm, xps_resource *dict, char *clip_att, xml_element *clip_tag);

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

struct xps_context_s
{
	char *directory;
	fz_stream *file;
	int zip_count;
	xps_entry *zip_table;

	char *start_part; /* fixed document sequence */
	xps_document *first_fixdoc; /* first fixed document */
	xps_document *last_fixdoc; /* last fixed document */
	xps_page *first_page; /* first page of document */
	xps_page *last_page; /* last page of document */

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
};

int xps_open_file(xps_context **ctxp, char *filename);
int xps_open_stream(xps_context **ctxp, fz_stream *file);
void xps_free_context(xps_context *ctx);

#endif
