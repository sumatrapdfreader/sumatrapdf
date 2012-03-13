#ifndef _MUXPS_H_
#define _MUXPS_H_

#include "fitz-internal.h"

typedef unsigned char byte;

/*
 * XPS and ZIP constants.
 */

typedef struct xps_document_s xps_document;

#define REL_START_PART \
	"http://schemas.microsoft.com/xps/2005/06/fixedrepresentation"
#define REL_DOC_STRUCTURE \
	"http://schemas.microsoft.com/xps/2005/06/documentstructure"
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
void xps_resolve_url(char *output, char *base_uri, char *path, int output_size);
int xps_url_is_remote(char *path);

/*
 * XML document model
 */

typedef struct element xml_element;

xml_element *xml_parse_document(fz_context *doc, byte *buf, int len);
xml_element *xml_next(xml_element *item);
xml_element *xml_down(xml_element *item);
char *xml_tag(xml_element *item);
char *xml_att(xml_element *item, const char *att);
void xml_free_element(fz_context *doc, xml_element *item);
void xml_print_element(xml_element *item, int level);
/* SumatraPDF: allow to keep only part of an XML tree */
void xml_detach(xml_element *node);

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
typedef struct xps_page_s xps_page;
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
	xml_element *root;
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
void xps_debug_page_list(xps_document *doc);
void xps_free_page_list(xps_document *doc);

int xps_count_pages(xps_document *doc);
xps_page *xps_load_page(xps_document *doc, int number);
fz_link *xps_load_links(xps_document *doc, xps_page *page);
fz_rect xps_bound_page(xps_document *doc, xps_page *page);
void xps_free_page(xps_document *doc, xps_page *page);
/* SumatraPDF: extract page bounds without parsing the entire page content */
fz_rect xps_bound_page_quick_and_dirty(xps_document *doc, int number);

fz_outline *xps_load_outline(xps_document *doc);

int xps_find_link_target(xps_document *doc, char *target_uri);
void xps_add_link(xps_document *doc, fz_rect area, char *base_uri, char *target_uri);
/* SumatraPDF: extended link support */
xps_target *xps_find_link_target_obj(xps_document *doc, char *target_uri);

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

void xps_debug_path(xps_document *doc);

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
	xml_element *base_xml; /* only used in the head nodes, to free the xml document */
	xml_element *data;
	xps_resource *next;
	xps_resource *parent; /* up to the previous dict in the stack */
};

xps_resource * xps_parse_resource_dictionary(xps_document *doc, char *base_uri, xml_element *root);
void xps_free_resource_dictionary(xps_document *doc, xps_resource *dict);
void xps_resolve_resource_reference(xps_document *doc, xps_resource *dict, char **attp, xml_element **tagp, char **urip);

void xps_debug_resource_dictionary(xps_resource *dict);

/*
 * Fixed page/graphics parsing.
 */

void xps_run_page(xps_document *doc, xps_page *page, fz_device *dev, fz_matrix ctm, fz_cookie *cookie);

void xps_parse_fixed_page(xps_document *doc, fz_matrix ctm, xps_page *page);
void xps_parse_canvas(xps_document *doc, fz_matrix ctm, fz_rect area, char *base_uri, xps_resource *dict, xml_element *node);
void xps_parse_path(xps_document *doc, fz_matrix ctm, char *base_uri, xps_resource *dict, xml_element *node);
void xps_parse_glyphs(xps_document *doc, fz_matrix ctm, char *base_uri, xps_resource *dict, xml_element *node);
void xps_parse_solid_color_brush(xps_document *doc, fz_matrix ctm, char *base_uri, xps_resource *dict, xml_element *node);
void xps_parse_image_brush(xps_document *doc, fz_matrix ctm, fz_rect area, char *base_uri, xps_resource *dict, xml_element *node);
void xps_parse_visual_brush(xps_document *doc, fz_matrix ctm, fz_rect area, char *base_uri, xps_resource *dict, xml_element *node);
void xps_parse_linear_gradient_brush(xps_document *doc, fz_matrix ctm, fz_rect area, char *base_uri, xps_resource *dict, xml_element *node);
void xps_parse_radial_gradient_brush(xps_document *doc, fz_matrix ctm, fz_rect area, char *base_uri, xps_resource *dict, xml_element *node);

void xps_parse_tiling_brush(xps_document *doc, fz_matrix ctm, fz_rect area, char *base_uri, xps_resource *dict, xml_element *root, void(*func)(xps_document*, fz_matrix, fz_rect, char*, xps_resource*, xml_element*, void*), void *user);

void xps_parse_matrix_transform(xps_document *doc, xml_element *root, fz_matrix *matrix);
void xps_parse_render_transform(xps_document *doc, char *text, fz_matrix *matrix);
void xps_parse_rectangle(xps_document *doc, char *text, fz_rect *rect);

void xps_begin_opacity(xps_document *doc, fz_matrix ctm, fz_rect area, char *base_uri, xps_resource *dict, char *opacity_att, xml_element *opacity_mask_tag);
void xps_end_opacity(xps_document *doc, char *base_uri, xps_resource *dict, char *opacity_att, xml_element *opacity_mask_tag);

void xps_parse_brush(xps_document *doc, fz_matrix ctm, fz_rect area, char *base_uri, xps_resource *dict, xml_element *node);
void xps_parse_element(xps_document *doc, fz_matrix ctm, fz_rect area, char *base_uri, xps_resource *dict, xml_element *node);

/* SumatraPDF: basic support for alternate content */
xml_element *xps_find_alternate_content(xml_element *node);

void xps_clip(xps_document *doc, fz_matrix ctm, xps_resource *dict, char *clip_att, xml_element *clip_tag);

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

/*
	xps_open_document: Open a document.

	Open a document for reading so the library is able to locate
	objects and pages inside the file.

	The returned xps_document should be used when calling most
	other functions. Note that it wraps the context, so those
	functions implicitly get access to the global state in
	context.

	filename: a path to a file as it would be given to open(2).
*/
xps_document *xps_open_document(fz_context *ctx, char *filename);

/*
	xps_open_document_with_stream: Opens a document.

	Same as xps_open_document, but takes a stream instead of a
	filename to locate the document to open. Increments the
	reference count of the stream. See fz_open_file,
	fz_open_file_w or fz_open_fd for opening a stream, and
	fz_close for closing an open stream.
*/
xps_document *xps_open_document_with_stream(fz_stream *file);

/*
	xps_close_document: Closes and frees an opened document.

	The resource store in the context associated with xps_document
	is emptied.

	Does not throw exceptions.
*/
void xps_close_document(xps_document *doc);

/*
 * Parsing helper functions
 */
char *xps_get_real_params(char *s, int num, float *x);
char *xps_get_point(char *s_in, float *x, float *y);

/* SumatraPDF: extended link support */
void xps_extract_anchor_info(xps_document *doc, fz_rect rect, char *target_uri, char *anchor_name, int step);

/* SumatraPDF: extract document properties (hacky) */
typedef struct xps_doc_prop_s xps_doc_prop;
struct xps_doc_prop_s
{
	char *name;
	char *value;
	xps_doc_prop *next;
};
xps_doc_prop *xps_extract_doc_props(xps_document *doc);
void xps_free_doc_prop(fz_context *ctx, xps_doc_prop *prop);

#endif
