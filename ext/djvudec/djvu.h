/* djvu.h -- plain-C DjVu decoder (port of DjvuNet, jbig2dec-flavored API).
 *
 * Decode-only. The caller supplies the entire DjVu file up-front as an
 * in-memory buffer that must outlive the djvu_doc.
 */
#ifndef DJVU_H
#define DJVU_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ----- allocator + diagnostics (jbig2dec style) ----- */

typedef void *(*djvu_alloc_cb)(void *user, size_t size);
typedef void  (*djvu_free_cb)(void *user, void *ptr);

typedef enum {
    DJVU_SEVERITY_DEBUG,
    DJVU_SEVERITY_INFO,
    DJVU_SEVERITY_WARNING,
    DJVU_SEVERITY_ERROR,
    DJVU_SEVERITY_FATAL
} djvu_severity;

/* msg is a NUL-terminated, already-formatted message. */
typedef void (*djvu_error_cb)(void *user, djvu_severity sev, const char *msg);

typedef struct djvu_ctx djvu_ctx;
typedef struct djvu_doc djvu_doc;

/* Pass NULL for alloc/free to use the default malloc/free.
   Pass NULL for error to silently ignore diagnostics. */
djvu_ctx *djvu_ctx_new(djvu_alloc_cb alloc, djvu_free_cb free_cb,
                       djvu_error_cb error, void *user);
void djvu_ctx_free(djvu_ctx *ctx);

/* ----- documents ----- */

/* Open a document over an in-memory buffer (NOT copied; must remain valid
   until djvu_doc_close). Returns NULL on failure (diagnostics via error cb). */
djvu_doc *djvu_doc_open(djvu_ctx *ctx, const uint8_t *data, size_t len);
void djvu_doc_close(djvu_doc *doc);

/* Number of displayable pages (FORM:DJVU components). */
int djvu_doc_page_count(djvu_doc *doc);

typedef struct {
    int width;     /* full-resolution width in pixels  */
    int height;    /* full-resolution height in pixels */
    int dpi;       /* dots per inch */
    int version;   /* djvu minor version from INFO */
    int rotation;  /* 0, 90, 180, 270 (degrees clockwise) */
} djvu_page_info;

/* page_no is 0-based. Returns 0 on success, -1 on error. */
int djvu_doc_page_info(djvu_doc *doc, int page_no, djvu_page_info *info);

/* ----- rendering ----- */

typedef enum {
    DJVU_FORMAT_GRAY8 = 1,  /* 1 byte/pixel, 0=black .. 255=white */
    DJVU_FORMAT_RGB24 = 3   /* 3 bytes/pixel, R,G,B */
} djvu_format;

typedef struct {
    int width;
    int height;
    djvu_format format;
    int stride;        /* bytes per row */
    uint8_t *data;     /* width*height*components, top-down */
} djvu_image;

/* Render the full composite page image.
   subsample >= 1 reduces resolution by that integer factor (1 = full res).
   Returns NULL on error; free with djvu_image_destroy. */
djvu_image *djvu_page_render(djvu_doc *doc, int page_no, int subsample);
void djvu_image_destroy(djvu_ctx *ctx, djvu_image *img);

/* Page content classification (from the chunks present in the page form).
   Lets a caller pick a render format the way ddjvu_page_get_type does. */
typedef enum {
    DJVU_PAGE_UNKNOWN  = 0,
    DJVU_PAGE_BITONAL  = 1,  /* JB2 mask only (Sjbz, no BG44/FG44/FGbz) */
    DJVU_PAGE_PHOTO    = 2,  /* IW44 photo only (BG44, no mask)         */
    DJVU_PAGE_COMPOUND = 3   /* mask + background/foreground layers      */
} djvu_page_type;

djvu_page_type djvu_page_get_type(djvu_doc *doc, int page_no);

/* ----- page identity / labels (DJVM directory) -----
   These reflect the DIRM component for a page. For a single-page document
   (FORM:DJVU with no directory) they return NULL / -1. */

/* Component id of a page (e.g. "p0001.djvu"), used by INCL/url refs. */
const char *djvu_doc_page_id(djvu_doc *doc, int page_no);
/* Human-readable page title from the directory, or NULL if none. */
const char *djvu_doc_page_title(djvu_doc *doc, int page_no);
/* Resolve a named destination (a component id, with or without a leading '#')
   to a 0-based page number, or -1 if it doesn't name a page. */
int djvu_doc_page_by_name(djvu_doc *doc, const char *name);

/* ----- text ----- */

/* Returns malloc'd (via ctx allocator) NUL-terminated UTF-8 page text,
   or NULL if the page has no text. Free with djvu_text_destroy. */
char *djvu_page_text(djvu_doc *doc, int page_no);
void djvu_text_destroy(djvu_ctx *ctx, char *text);

/* ----- structured text (zone tree with bounding boxes) -----
   The DjVu hidden-text layer is a tree of nested zones (page > column > region
   > paragraph > line > word > char). Bounding boxes are in full-resolution
   page pixels, top-down (y measured from the top edge), matching the
   orientation of djvu_page_render output at subsample=1. */

typedef enum {
    DJVU_ZONE_PAGE      = 1,
    DJVU_ZONE_COLUMN    = 2,
    DJVU_ZONE_REGION    = 3,
    DJVU_ZONE_PARAGRAPH = 4,
    DJVU_ZONE_LINE      = 5,
    DJVU_ZONE_WORD      = 6,
    DJVU_ZONE_CHAR      = 7
} djvu_zone_type;

typedef struct djvu_text_zone djvu_text_zone;
struct djvu_text_zone {
    djvu_zone_type type;
    int x, y, w, h;            /* bounding box (top-down page pixels) */
    char *text;                /* UTF-8 text covered by this zone (NUL-term) */
    djvu_text_zone *children;  /* array of nchildren child zones */
    int nchildren;
};

typedef struct {
    char *text;                /* full page text, UTF-8 (== djvu_page_text) */
    djvu_text_zone *root;      /* page zone (root of the tree), or NULL */
} djvu_page_text_zones;

/* Returns the page text zone tree, or NULL if the page has no hidden text.
   Free with djvu_text_zones_destroy. */
djvu_page_text_zones *djvu_page_text_get_zones(djvu_doc *doc, int page_no);
void djvu_text_zones_destroy(djvu_ctx *ctx, djvu_page_text_zones *z);

/* ----- document outline / bookmarks (NAVM chunk) ----- */

typedef struct djvu_outline_item djvu_outline_item;
struct djvu_outline_item {
    char *title;                  /* UTF-8 bookmark title (NULL for the root) */
    char *url;                    /* target, e.g. "#12" or "http://..."       */
    int page_no;                  /* 0-based page if url names one, else -1    */
    djvu_outline_item *children;  /* array of nchildren                        */
    int nchildren;
};

/* Returns a synthetic root item (title/url NULL) whose children are the
   top-level bookmarks, or NULL if the document has no outline.
   Free with djvu_outline_destroy. */
djvu_outline_item *djvu_doc_outline(djvu_doc *doc);
void djvu_outline_destroy(djvu_ctx *ctx, djvu_outline_item *root);

/* ----- page hyperlinks / annotations (ANTa/ANTz chunks) ----- */

typedef enum {
    DJVU_LINK_RECT = 0,
    DJVU_LINK_OVAL = 1,
    DJVU_LINK_TEXT = 2
} djvu_link_shape;

typedef struct {
    char *url;              /* target, e.g. "#12" or "http://..." (UTF-8)    */
    char *comment;          /* tooltip/comment, or NULL                       */
    djvu_link_shape shape;
    int x, y, w, h;         /* bounding box (top-down page pixels)            */
} djvu_link;

typedef struct {
    djvu_link *links;
    int nlinks;
} djvu_page_links;

/* Returns the page's hyperlinks (from page or shared/INCL'd annotations),
   or NULL if there are none. Free with djvu_page_links_destroy. */
djvu_page_links *djvu_page_get_links(djvu_doc *doc, int page_no);
void djvu_page_links_destroy(djvu_ctx *ctx, djvu_page_links *links);

#ifdef __cplusplus
}
#endif
#endif /* DJVU_H */
