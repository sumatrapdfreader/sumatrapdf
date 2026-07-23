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

/* ctx identifies the djvu_ctx the allocation belongs to, so a caller can
   account for allocations per context. It is NULL only for the bootstrap
   allocation/free of the djvu_ctx struct itself (which has no context yet). */
typedef void *(*djvu_alloc_cb)(void *user, void *ctx, size_t size);
typedef void  (*djvu_free_cb)(void *user, void *ctx, void *ptr);

/* Optional mutex hooks for per-page caching (djvu_ctx_set_cache_per_page).
   Required when per-page caching is enabled. */
typedef void (*djvu_lock_cb)(void *user, void *ctx);
typedef void (*djvu_unlock_cb)(void *user, void *ctx);

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

/* Idempotent process-wide setup (bilinear scaler lookup table). Call once from
   the main thread before creating worker threads or concurrent decode. Safe to
   call again; also invoked by djvu_doc_open. */
void djvu_init(void);

/* Pass NULL for alloc/free to use the default malloc/free.
   lock/unlock are optional serialization hooks (required for per-page
   caching). When supplied, shared JB2 dictionaries are decoded lazily on
   first use (serialized via the hooks) instead of eagerly at djvu_doc_open,
   and concurrent renders on the same djvu_doc are safe.
   Pass NULL for error to silently ignore diagnostics. */
djvu_ctx *djvu_ctx_new(djvu_alloc_cb alloc, djvu_free_cb free_cb,
                       djvu_lock_cb lock, djvu_unlock_cb unlock,
                       djvu_error_cb error, void *user);
void djvu_ctx_free(djvu_ctx *ctx);

/* Per-context decode options (defaults off/zero). Set before djvu_doc_open.
   Shared JB2 dicts are pre-decoded at open only when no lock/unlock hooks
   were supplied (see djvu_ctx_new); with hooks they decode lazily. */
/* Retain decoded page-local layers on the document after each render:
   IW44 background/foreground, JB2 mask (Sjbz), and composited background.
   Default off: layers are decoded per use and freed when the render completes.
   When enabled, later renders of the same page reuse the cached layers.
   Requires non-NULL lock and unlock callbacks passed to djvu_ctx_new; the
   decoder serializes first-time decode of a page's layers via those hooks so
   concurrent renders on the same djvu_doc are safe. djvu_doc_open fails if
   caching is enabled but lock/unlock were not supplied. */
void djvu_ctx_set_cache_per_page(djvu_ctx *ctx, int enable);
/* Legacy alias: enable=1 turns on per-page caching. */
void djvu_ctx_set_lazy_iw44(djvu_ctx *ctx, int enable);
void djvu_ctx_set_no_compose(djvu_ctx *ctx, int enable);
void djvu_ctx_set_iw_max_chunks(djvu_ctx *ctx, int max_chunks);
/* When enabled, color render output (DJVU_FORMAT_RGB24) is written in B,G,R
   byte order instead of R,G,B. The image is still tagged DJVU_FORMAT_RGB24
   (3 bytes/pixel); only the channel order changes. Lets a caller whose target
   wants BGR (e.g. a Windows DIB) skip a separate RGB->BGR pass -- the swap is
   folded into the decoder's final output copy at no extra cost. */
void djvu_ctx_set_bgr(djvu_ctx *ctx, int enable);
/* Bump the cooperative render-abort epoch on ctx. ALL in-flight page renders
   on this ctx that already entered via djvu_page_render / djvu_page_render_into
   exit promptly; renders that start afterward proceed normally. Thread-safe.
   To cancel one render without disturbing concurrent renders of other pages
   on the same ctx, use a djvu_abort token with the _abortable variants
   instead. */
void djvu_request_abort(djvu_ctx *ctx);

/* Caller-owned cooperative abort token scoped to individual render calls.
   Initialize with djvu_abort_init, pass to djvu_page_render_abortable /
   djvu_page_render_into_abortable; calling djvu_abort_request (from any
   thread) makes only the render(s) given this token exit promptly --
   unlike djvu_request_abort, which aborts every in-flight render on the
   ctx. The request is sticky: re-init the token to reuse it. The token must
   stay valid until the render call it was passed to has returned. */
typedef struct {
    volatile int requested;
} djvu_abort;
void djvu_abort_init(djvu_abort *ab);
void djvu_abort_request(djvu_abort *ab);

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

/* ----- per-page decoded-layer cache (djvu_ctx_set_cache_per_page) -----
   When caching is enabled, the first render (or layer acquire) of a page
   stores Sjbz / IW44 / composited background on the document; later renders
   of that page reuse them. Shared Djbz dictionaries are doc-wide and are
   not part of this page cache. */

/* Free all page-local cached layers for page_no (Sjbz mask, BG44/FG44,
   composited backgrounds). No-op if caching is off or the page has nothing
   cached. Safe with concurrent renders when lock/unlock are set. */
void djvu_doc_drop_page_cache(djvu_doc *doc, int page_no);

/* Heap bytes currently held in page-local cache for page_no (0 if none /
   invalid page / caching off). Approximate: walks live structures (bitmap
   payloads, shape/blit tables, IW44 coefficient buckets, RGB pixmaps).
   Does not include shared Djbz dicts or the caller's document buffer. */
size_t djvu_doc_page_cache_size(djvu_doc *doc, int page_no);

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

/* Geometry + format a page render will produce, without decoding pixels.
   Lets a caller size a destination buffer (e.g. a DIB) before rendering into
   it with djvu_page_render_into. */
typedef struct {
    int width;
    int height;
    djvu_format format;   /* GRAY8 or RGB24 (RGB24 honors djvu_ctx_set_bgr) */
} djvu_render_info;

/* Fill *info for a page render at the given subsample. Returns 0 on success,
   -1 if the page cannot be rendered. */
int djvu_page_render_info(djvu_doc *doc, int page_no, int subsample,
                          djvu_render_info *info);

/* Render a page directly into a caller-provided buffer instead of allocating a
   djvu_image, eliminating one full-frame copy. dst is top-down with `stride`
   bytes per row; it must match the geometry/format reported by
   djvu_page_render_info (query that first to size dst). When color output goes
   straight into dst (the common no-rotation case) the composite is written in
   one pass. Returns 0 on success, -1 on error. */
int djvu_page_render_into(djvu_doc *doc, int page_no, int subsample,
                          uint8_t *dst, int stride);

/* Same as djvu_page_render / djvu_page_render_into, but cancellable through
   the caller's per-render djvu_abort token (see above; NULL behaves like the
   plain variants). An aborted render returns NULL / -1. */
djvu_image *djvu_page_render_abortable(djvu_doc *doc, int page_no,
                                       int subsample, const djvu_abort *ab);
int djvu_page_render_into_abortable(djvu_doc *doc, int page_no, int subsample,
                                    uint8_t *dst, int stride,
                                    const djvu_abort *ab);

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
