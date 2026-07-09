
#ifndef DJVU_H
#define DJVU_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void *(*djvu_alloc_cb)(void *user, void *ctx, size_t size);
typedef void  (*djvu_free_cb)(void *user, void *ctx, void *ptr);

typedef void (*djvu_lock_cb)(void *user, void *ctx);
typedef void (*djvu_unlock_cb)(void *user, void *ctx);

typedef enum {
    DJVU_SEVERITY_DEBUG,
    DJVU_SEVERITY_INFO,
    DJVU_SEVERITY_WARNING,
    DJVU_SEVERITY_ERROR,
    DJVU_SEVERITY_FATAL
} djvu_severity;

typedef void (*djvu_error_cb)(void *user, djvu_severity sev, const char *msg);

typedef struct djvu_ctx djvu_ctx;
typedef struct djvu_doc djvu_doc;

void djvu_init(void);

djvu_ctx *djvu_ctx_new(djvu_alloc_cb alloc, djvu_free_cb free_cb,
                       djvu_lock_cb lock, djvu_unlock_cb unlock,
                       djvu_error_cb error, void *user);
void djvu_ctx_free(djvu_ctx *ctx);

void djvu_ctx_set_cache_per_page(djvu_ctx *ctx, int enable);

void djvu_ctx_set_lazy_iw44(djvu_ctx *ctx, int enable);
void djvu_ctx_set_no_compose(djvu_ctx *ctx, int enable);
void djvu_ctx_set_iw_max_chunks(djvu_ctx *ctx, int max_chunks);

void djvu_ctx_set_bgr(djvu_ctx *ctx, int enable);

void djvu_request_abort(djvu_ctx *ctx);

typedef struct {
    volatile int requested;
} djvu_abort;
void djvu_abort_init(djvu_abort *ab);
void djvu_abort_request(djvu_abort *ab);

djvu_doc *djvu_doc_open(djvu_ctx *ctx, const uint8_t *data, size_t len);
void djvu_doc_close(djvu_doc *doc);

int djvu_doc_page_count(djvu_doc *doc);

typedef struct {
    int width;
    int height;
    int dpi;
    int version;
    int rotation;
} djvu_page_info;

int djvu_doc_page_info(djvu_doc *doc, int page_no, djvu_page_info *info);

typedef enum {
    DJVU_FORMAT_GRAY8 = 1,
    DJVU_FORMAT_RGB24 = 3
} djvu_format;

typedef struct {
    int width;
    int height;
    djvu_format format;
    int stride;
    uint8_t *data;
} djvu_image;

djvu_image *djvu_page_render(djvu_doc *doc, int page_no, int subsample);
void djvu_image_destroy(djvu_ctx *ctx, djvu_image *img);

typedef struct {
    int width;
    int height;
    djvu_format format;
} djvu_render_info;

int djvu_page_render_info(djvu_doc *doc, int page_no, int subsample,
                          djvu_render_info *info);

int djvu_page_render_into(djvu_doc *doc, int page_no, int subsample,
                          uint8_t *dst, int stride);

djvu_image *djvu_page_render_abortable(djvu_doc *doc, int page_no,
                                       int subsample, const djvu_abort *ab);
int djvu_page_render_into_abortable(djvu_doc *doc, int page_no, int subsample,
                                    uint8_t *dst, int stride,
                                    const djvu_abort *ab);

typedef enum {
    DJVU_PAGE_UNKNOWN  = 0,
    DJVU_PAGE_BITONAL  = 1,
    DJVU_PAGE_PHOTO    = 2,
    DJVU_PAGE_COMPOUND = 3
} djvu_page_type;

djvu_page_type djvu_page_get_type(djvu_doc *doc, int page_no);

const char *djvu_doc_page_id(djvu_doc *doc, int page_no);

const char *djvu_doc_page_title(djvu_doc *doc, int page_no);

int djvu_doc_page_by_name(djvu_doc *doc, const char *name);

char *djvu_page_text(djvu_doc *doc, int page_no);
void djvu_text_destroy(djvu_ctx *ctx, char *text);

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
    int x, y, w, h;
    char *text;
    djvu_text_zone *children;
    int nchildren;
};

typedef struct {
    char *text;
    djvu_text_zone *root;
} djvu_page_text_zones;

djvu_page_text_zones *djvu_page_text_get_zones(djvu_doc *doc, int page_no);
void djvu_text_zones_destroy(djvu_ctx *ctx, djvu_page_text_zones *z);

typedef struct djvu_outline_item djvu_outline_item;
struct djvu_outline_item {
    char *title;
    char *url;
    int page_no;
    djvu_outline_item *children;
    int nchildren;
};

djvu_outline_item *djvu_doc_outline(djvu_doc *doc);
void djvu_outline_destroy(djvu_ctx *ctx, djvu_outline_item *root);

typedef enum {
    DJVU_LINK_RECT = 0,
    DJVU_LINK_OVAL = 1,
    DJVU_LINK_TEXT = 2
} djvu_link_shape;

typedef struct {
    char *url;
    char *comment;
    djvu_link_shape shape;
    int x, y, w, h;
} djvu_link;

typedef struct {
    djvu_link *links;
    int nlinks;
} djvu_page_links;

djvu_page_links *djvu_page_get_links(djvu_doc *doc, int page_no);
void djvu_page_links_destroy(djvu_ctx *ctx, djvu_page_links *links);

#ifdef __cplusplus
}
#endif
#endif

#ifndef DJVU_INTERNAL_H
#define DJVU_INTERNAL_H

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#if defined(_WIN32)
#include <windows.h>
#else
#include <time.h>
#include <stdatomic.h>
#endif

#ifndef DJVU_RESTRICT
#if defined(_MSC_VER)
#define DJVU_RESTRICT __restrict
#else
#define DJVU_RESTRICT restrict
#endif
#endif

#ifndef DJVU_LIKELY
#if defined(__GNUC__) || defined(__clang__)
#define DJVU_LIKELY(x) __builtin_expect(!!(x), 1)
#define DJVU_UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
#define DJVU_LIKELY(x) (x)
#define DJVU_UNLIKELY(x) (x)
#endif
#endif

#if defined(_WIN32)
typedef struct { volatile LONG v; } djvu_atomic_epoch;
static inline void djvu_atomic_epoch_init(djvu_atomic_epoch *a) { a->v = 0; }
static inline uint32_t djvu_atomic_epoch_load(const djvu_atomic_epoch *a)
{
    return (uint32_t)InterlockedCompareExchange((LONG *)&a->v, 0, 0);
}
static inline void djvu_atomic_epoch_bump(djvu_atomic_epoch *a)
{
    InterlockedIncrement((LONG *)&a->v);
}
#else
typedef struct { atomic_uint v; } djvu_atomic_epoch;
static inline void djvu_atomic_epoch_init(djvu_atomic_epoch *a)
{
    atomic_init(&a->v, 0);
}
static inline uint32_t djvu_atomic_epoch_load(const djvu_atomic_epoch *a)
{
    return atomic_load_explicit(&a->v, memory_order_relaxed);
}
static inline void djvu_atomic_epoch_bump(djvu_atomic_epoch *a)
{
    atomic_fetch_add_explicit(&a->v, 1, memory_order_relaxed);
}
#endif

struct djvu_ctx {
    djvu_alloc_cb alloc;
    djvu_free_cb  free;
    djvu_lock_cb  lock;
    djvu_unlock_cb unlock;
    djvu_error_cb error;
    void *user;
    djvu_atomic_epoch abort_epoch;
    int cache_per_page;
    int no_compose;
    int iw_max_chunks;
    int bgr;
};

#if defined(_MSC_VER)
extern __declspec(thread) uint32_t djvu_render_epoch_tls;
extern __declspec(thread) const djvu_abort *djvu_render_abort_tls;
#else
extern __thread uint32_t djvu_render_epoch_tls;
extern __thread const djvu_abort *djvu_render_abort_tls;
#endif

static inline void djvu_render_begin(djvu_ctx *ctx, const djvu_abort *ab)
{
    if (ctx)
        djvu_render_epoch_tls = djvu_atomic_epoch_load(&ctx->abort_epoch);
    djvu_render_abort_tls = ab;
}

static inline void djvu_render_end(void)
{
    djvu_render_abort_tls = NULL;
}

static inline int djvu_aborted(djvu_ctx *ctx)
{
    if (!ctx) return 0;
    if (djvu_render_abort_tls && djvu_render_abort_tls->requested) return 1;
    return djvu_atomic_epoch_load(&ctx->abort_epoch) != djvu_render_epoch_tls;
}

static inline int djvu_cache_stores_page(djvu_ctx *ctx)
{
    return ctx && ctx->cache_per_page;
}

static inline void djvu_cache_lock(djvu_ctx *ctx)
{
    if (ctx && ctx->cache_per_page && ctx->lock)
        ctx->lock(ctx->user, ctx);
}

static inline void djvu_cache_unlock(djvu_ctx *ctx)
{
    if (ctx && ctx->cache_per_page && ctx->unlock)
        ctx->unlock(ctx->user, ctx);
}

static inline int djvu_has_lock(djvu_ctx *ctx)
{
    return ctx && ctx->lock && ctx->unlock;
}

static inline void djvu_dict_lock(djvu_ctx *ctx)
{
    if (djvu_has_lock(ctx))
        ctx->lock(ctx->user, ctx);
}

static inline void djvu_dict_unlock(djvu_ctx *ctx)
{
    if (djvu_has_lock(ctx))
        ctx->unlock(ctx->user, ctx);
}

void *djvu_alloc(djvu_ctx *ctx, size_t size);
void  djvu_free(djvu_ctx *ctx, void *ptr);
void  djvu_errorf(djvu_ctx *ctx, djvu_severity sev, const char *fmt, ...);

static inline double djvu_bench_now_ms(void)
{
#if defined(_WIN32)
    static LARGE_INTEGER freq;
    LARGE_INTEGER t;
    if (!freq.QuadPart) QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&t);
    return (double)t.QuadPart * 1000.0 / (double)freq.QuadPart;
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1e6;
#endif
}

typedef struct {
    uint32_t offset;
    uint32_t size;
    char    *id;
    char    *title;
    int      type;
} djvu_component;

typedef struct iw_pixmap iw_pixmap;
typedef struct jb2_image jb2_image;
typedef struct { int w, h; uint8_t *d; } djvu_cpix;

typedef struct {
    char *incl_id;
    jb2_image *dict;
} djvu_jb2_dict_entry;

typedef struct {
    const uint8_t *djbz;
    uint32_t djbz_sz;
    jb2_image *dict;
} djvu_jb2_inline_entry;

#define DJVU_PG_SJBZ 0x01u
#define DJVU_PG_BG44 0x02u
#define DJVU_PG_FG44 0x04u
#define DJVU_PG_FGBZ 0x08u
#define DJVU_PG_DJBZ 0x10u

typedef struct {
    uint32_t form_off;
    uint32_t form_size;
    int has_info;
    djvu_page_info info;
    const char *id;
    const char *title;
    uint32_t chunk_flags;
    iw_pixmap *iw_bg;
    iw_pixmap *iw_fg;
    jb2_image *jb2_mask;
    djvu_cpix bg_native;
    djvu_cpix bg_scaled;
} djvu_page_int;

struct djvu_doc {
    djvu_ctx *ctx;
    const uint8_t *data;
    size_t len;
    uint32_t root_form_off;
    int npages;
    djvu_page_int *pages;
    int ncomp;
    djvu_component *comps;
    char **shared_incl_ids;
    int n_shared_incl;
    djvu_jb2_dict_entry *jb2_dicts;
    int n_jb2_dicts;
    djvu_jb2_inline_entry *jb2_inline;
    int n_jb2_inline;
};

iw_pixmap *djvu_doc_iw44_acquire(djvu_doc *doc, int page_no, const char *chunk_id,
                                 int *owned_out);
void djvu_doc_iw44_release(djvu_ctx *ctx, iw_pixmap *pm, int owned);
iw_pixmap *djvu_doc_iw44_by_form_acquire(djvu_doc *doc, uint32_t form_off,
                                         const char *chunk_id, int *owned_out);

iw_pixmap *djvu_doc_iw44_acquire_under_lock(djvu_doc *doc, djvu_page_int *pg,
                                            const char *chunk_id);
iw_pixmap *djvu_doc_iw44(djvu_doc *doc, int page_no, const char *chunk_id);
iw_pixmap *djvu_doc_iw44_by_form(djvu_doc *doc, uint32_t form_off, const char *chunk_id);
jb2_image *djvu_doc_jb2_mask_acquire(djvu_doc *doc, int page_no, int *owned_out);
void djvu_doc_jb2_mask_release(djvu_doc *doc, jb2_image *mask, int owned);
void djvu_doc_drop_page_iw44(djvu_doc *doc, int page_no);
void djvu_doc_preload_iw44_range(djvu_doc *doc, int lo0, int hi0);
void djvu_doc_preload_jb2_range(djvu_doc *doc, int lo0, int hi0);
void djvu_doc_preload_jb2_masks_range(djvu_doc *doc, int lo0, int hi0);
void djvu_doc_preload_compose_bg_range(djvu_doc *doc, int lo0, int hi0);

jb2_image *djvu_doc_jb2_mask(djvu_doc *doc, int page_no);
jb2_image *djvu_doc_jb2_dict(djvu_doc *doc, const char *incl_id);
jb2_image *djvu_doc_jb2_dict_inline(djvu_doc *doc, uint32_t form_off);
jb2_image *djvu_doc_jb2_dict_for_form(djvu_doc *doc, uint32_t form_off);

uint32_t djvu_doc_component_offset(djvu_doc *doc, const char *id);

const uint8_t *djvu_form_find_chunk(djvu_doc *doc, uint32_t form_off,
                                    const char *id, uint32_t *out_size,
                                    uint32_t *start);

void djvu_trim_incl_id(char *s);

const uint8_t *djvu_form_find_incl_chunk(djvu_doc *doc, uint32_t form_off,
                                         const char *chunk_id, uint32_t *out_size);

static inline uint32_t djvu_rd_u32be(const uint8_t *p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | p[3];
}
static inline uint32_t djvu_rd_u24be(const uint8_t *p) {
    return ((uint32_t)p[0] << 16) | ((uint32_t)p[1] << 8) | p[2];
}
static inline uint32_t djvu_rd_u16be(const uint8_t *p) {
    return ((uint32_t)p[0] << 8) | p[1];
}
static inline uint32_t djvu_rd_u16le(const uint8_t *p) {
    return ((uint32_t)p[1] << 8) | p[0];
}

static inline int djvu_tag_eq(const uint8_t *p, const char *tag) {
    return p[0] == (uint8_t)tag[0] && p[1] == (uint8_t)tag[1] &&
           p[2] == (uint8_t)tag[2] && p[3] == (uint8_t)tag[3];
}

static inline int djvu_y_bottomup_to_topdown(int y, int page_h, int h) {
    return page_h - (y + h);
}

void djvu_flip_rgb_bottomup(uint8_t *dst, const uint8_t *src, int w, int h, int bgr);

typedef struct {
    djvu_ctx *ctx;
    const uint8_t *p;
    size_t len, pos;
    int failed;
} djvu_buf_reader;

static inline void djvu_br_init(djvu_buf_reader *br, djvu_ctx *ctx,
                                const uint8_t *p, size_t len)
{
    br->ctx = ctx;
    br->p = p;
    br->len = len;
    br->pos = 0;
    br->failed = 0;
}

static inline int djvu_br_u8(djvu_buf_reader *br)
{
    if (br->pos >= br->len) { br->failed = 1; return 0; }
    return br->p[br->pos++];
}

static inline int djvu_br_u16be(djvu_buf_reader *br)
{
    int v;
    if (br->pos + 2 > br->len) { br->failed = 1; return 0; }
    v = (int)djvu_rd_u16be(br->p + br->pos);
    br->pos += 2;
    return v;
}

static inline int djvu_br_u24be(djvu_buf_reader *br)
{
    int v;
    if (br->pos + 3 > br->len) { br->failed = 1; return 0; }
    v = (int)djvu_rd_u24be(br->p + br->pos);
    br->pos += 3;
    return v;
}

static inline int djvu_br_s16be_biased(djvu_buf_reader *br)
{
    int v;
    if (br->pos + 2 > br->len) { br->failed = 1; return 0; }
    v = (int)djvu_rd_u16be(br->p + br->pos) - 0x8000;
    br->pos += 2;
    return v;
}

char *djvu_br_strdup(djvu_buf_reader *br, int slen);

typedef struct {
    uint16_t p;
    uint16_t m;
    uint8_t  up;
    uint8_t  dn;
} djvu_zp_table;

extern const djvu_zp_table djvu_zp_default_table[256];

typedef uint8_t djvu_zp_ctx;

typedef struct {
    const uint8_t *data;
    size_t len;
    size_t pos;

    uint32_t a;
    uint32_t code;
    uint32_t fence;
    uint32_t buffer;
    uint8_t  scount;
    uint8_t  delay;
    uint8_t  zbyte;
    int      eof;

    uint32_t p[256];
    uint32_t m[256];
    uint8_t  up[256];
    uint8_t  dn[256];
    int8_t   ffzt[256];
} djvu_zp;

void djvu_zp_init(djvu_zp *zp, const uint8_t *data, size_t len);

static inline int zp_read_byte(djvu_zp *zp)
{
    if (zp->pos < zp->len)
        return zp->data[zp->pos++];
    return -1;
}

static inline void zp_preload(djvu_zp *zp)
{
    unsigned scount = zp->scount;
    int zbyte = zp->zbyte;
    uint32_t buffer = zp->buffer;

    for (; scount <= 24; scount += 8) {
        int b = zp_read_byte(zp);
        if (b == -1) {
            zbyte = 255;
            if (--zp->delay < 1)
                zp->eof = 1;
        } else {
            zbyte = b;
        }
        buffer = (buffer << 8) | (uint8_t)zbyte;
    }

    zp->scount = (uint8_t)scount;
    zp->zbyte = (uint8_t)zbyte;
    zp->buffer = buffer;
}

static inline int zp_ffz(djvu_zp *zp, uint32_t x)
{
    return ((x & 0xffffffffu) < 0xff00u)
        ? zp->ffzt[(x >> 8) & 0xff]
        : (zp->ffzt[x & 0xff] + 8);
}

static inline void zp_renorm_lps(djvu_zp *zp, uint32_t z)
{
    int shift;
    z = 0x10000u - z;
    zp->a += z;
    zp->code += z;
    shift = zp_ffz(zp, zp->a);
    zp->scount -= (uint8_t)shift;
    zp->a = (zp->a << shift) & 0xffff;
    zp->code = ((zp->code << shift) & 0xffff)
             | ((zp->buffer >> zp->scount) & ((1u << shift) - 1));
    if (zp->scount < 16)
        zp_preload(zp);
    zp->fence = zp->code;
    if (zp->code >= 0x8000)
        zp->fence = 0x7fff;
}

static inline void zp_renorm_mps(djvu_zp *zp, uint32_t z)
{
    zp->scount -= 1;
    zp->a = (z << 1) & 0xffff;
    zp->code = ((zp->code << 1) & 0xffff) | ((zp->buffer >> zp->scount) & 1);
    if (zp->scount < 16)
        zp_preload(zp);
    zp->fence = zp->code;
    if (zp->code >= 0x8000)
        zp->fence = 0x7fff;
}

static inline int zp_decode_sub(djvu_zp *zp, djvu_zp_ctx *ctx, uint32_t z)
{
    int bit = *ctx & 1;
    uint32_t d = 0x6000u + ((z + zp->a) >> 2);
    if (z > d)
        z = d;

    if (z > zp->code) {
        *ctx = zp->dn[*ctx];
        zp_renorm_lps(zp, z);
        return bit ^ 1;
    } else {
        if (zp->a >= zp->m[*ctx])
            *ctx = zp->up[*ctx];
        zp_renorm_mps(zp, z);
        return bit;
    }
}

static inline int djvu_zp_decode(djvu_zp *zp, djvu_zp_ctx *ctx)
{
    uint32_t z = zp->a + zp->p[*ctx];
    if (z <= zp->fence) {
        zp->a = z;
        return *ctx & 1;
    }
    return zp_decode_sub(zp, ctx, z);
}

static inline int zp_decode_sub_simple(djvu_zp *zp, int mps, uint32_t z)
{
    if (z > zp->code) {
        zp_renorm_lps(zp, z);
        return mps ^ 1;
    } else {
        zp_renorm_mps(zp, z);
        return mps;
    }
}

static inline int djvu_zp_decode_pass(djvu_zp *zp)
{
    return zp_decode_sub_simple(zp, 0, 0x8000u + (zp->a >> 1));
}

static inline int djvu_zp_decode_iw(djvu_zp *zp)
{
    return zp_decode_sub_simple(zp, 0, 0x8000u + ((zp->a + zp->a + zp->a) >> 3));
}

typedef struct {
    int width, height;
    int border;
    int bytes_per_row;
    int max_offset;
    uint8_t *data;
    uint8_t *guard;
    uint8_t *rle;
    size_t rle_len;
} djvu_bitmap;

static inline int djvu_bm_has_pixels(const djvu_bitmap *bm)
{
    return bm && bm->width > 0 && bm->height > 0 && (bm->data || bm->rle);
}

int  djvu_bm_init(djvu_ctx *ctx, djvu_bitmap *bm, int height, int width, int border);
void djvu_bm_free(djvu_ctx *ctx, djvu_bitmap *bm);

static inline int djvu_bm_rowoffset(const djvu_bitmap *bm, int row) {
    return row * bm->bytes_per_row + bm->border;
}

static inline int jb2_get_direct_context(const uint8_t *up2, const uint8_t *up1,
                                           const uint8_t *up0, int col)
{
    return (up2[col - 1] << 9) | (up2[col] << 8) | (up2[col + 1] << 7) |
           (up1[col - 2] << 6) | (up1[col - 1] << 5) | (up1[col] << 4) |
           (up1[col + 1] << 3) | (up1[col + 2] << 2) |
           (up0[col - 2] << 1) | (up0[col - 1]);
}

static inline int jb2_shift_direct_context(int ctx, int next, const uint8_t *up2,
                                           const uint8_t *up1, int col)
{
    return ((ctx << 1) & 0x37a) | (up1[col + 2] << 2) | (up2[col + 1] << 7) | next;
}

static inline int jb2_get_cross_context(const uint8_t *up1, const uint8_t *up0,
                                        const uint8_t *xup1, const uint8_t *xup0,
                                        const uint8_t *xdn1, int col)
{
    return (up1[col - 1] << 10) | (up1[col] << 9) | (up1[col + 1] << 8) |
           (up0[col - 1] << 7) | (xup1[col] << 6) | (xup0[col - 1] << 5) |
           (xup0[col] << 4) | (xup0[col + 1] << 3) |
           (xdn1[col - 1] << 2) | (xdn1[col] << 1) | (xdn1[col + 1]);
}

static inline int jb2_shift_cross_context(int ctx, int n, const uint8_t *up1,
                                          const uint8_t *xup1, const uint8_t *xup0,
                                          const uint8_t *xdn1, int col)
{
    return ((ctx << 1) & 0x636) | (up1[col + 1] << 8) | (xup1[col] << 6) |
           (xup0[col + 1] << 3) | (xdn1[col + 1]) | (n << 7);
}

int djvu_bm_set_min_border(djvu_ctx *ctx, djvu_bitmap *bm, int value);

void djvu_bm_bbox(const djvu_bitmap *bm, int *xmin, int *ymin, int *xmax, int *ymax);

void djvu_bm_blit(djvu_bitmap *dst, const djvu_bitmap *src, int dx, int dy, int maxval);

void djvu_bm_compress(djvu_ctx *ctx, djvu_bitmap *bm);

void djvu_bm_uncompress(djvu_ctx *ctx, djvu_bitmap *bm);

void djvu_bm_ensure_bytes(djvu_ctx *ctx, djvu_bitmap *bm);

int djvu_bm_uncompress_copy(djvu_ctx *ctx, const djvu_bitmap *src, djvu_bitmap *dst);

void djvu_bm_visit_ink(const djvu_bitmap *src, int left, int bottom,
                       void (*fn)(void *user, int px, int py), void *user);

void djvu_bm_visit_ink_runs(const djvu_bitmap *src, int left, int bottom,
                            void (*fn)(void *user, int x0, int x1, int py),
                            void *user);

uint8_t *djvu_bzz_decode_all(djvu_ctx *ctx, const uint8_t *data, size_t len,
                             size_t *out_len);

typedef struct {
    int parent;

    int bbox_valid;
    int bx0, by0, bx1, by1;
    djvu_bitmap bm;
} jb2_shape;

typedef struct { int left, bottom, shapeno; } jb2_blit;

typedef struct jb2_image {
    djvu_ctx *ctx;

    jb2_shape *shapes;
    int nshapes, cap_shapes;
    int inherited_shapes;
    struct jb2_image *inherited_dict;

    int width, height;
    jb2_blit *blits;
    int nblits, cap_blits;
} jb2_image;

jb2_image *djvu_jb2_decode(djvu_ctx *ctx, const uint8_t *data, size_t len,
                           jb2_image *dict);

jb2_image *djvu_jb2_decode_dict(djvu_ctx *ctx, const uint8_t *data, size_t len);
void djvu_jb2_free(djvu_ctx *ctx, jb2_image *img);

jb2_shape *djvu_jb2_get_shape(jb2_image *img, int shapeno);

iw_pixmap *djvu_iw44_new(djvu_ctx *ctx);
void djvu_iw44_free(iw_pixmap *pm);

int djvu_iw44_decode_chunk(iw_pixmap *pm, const uint8_t *data, size_t len);

int djvu_iw44_decode_form(djvu_doc *doc, uint32_t form_off, const char *chunk_id,
                          iw_pixmap *pm, int max_chunks);

int djvu_iw44_width(iw_pixmap *pm);
int djvu_iw44_height(iw_pixmap *pm);
int djvu_iw44_is_color(iw_pixmap *pm);

int djvu_iw44_render_rgb(iw_pixmap *pm, uint8_t *rgb);

int djvu_iw44_render_rgb_raw(iw_pixmap *pm, uint8_t *rgb);

int djvu_iw44_render_gray(iw_pixmap *pm, uint8_t *gray);

int djvu_iw44_render_plane(iw_pixmap *pm, int plane, uint8_t *gray);

int  djvu_cpix_init(djvu_ctx *ctx, djvu_cpix *p, int w, int h);
void djvu_cpix_free(djvu_ctx *ctx, djvu_cpix *p);
int  djvu_compute_red(int w, int h, int rw, int rh);
int  djvu_cpix_scale(djvu_ctx *ctx, const djvu_cpix *in, djvu_cpix *out,
                     int outw, int outh, int red);

int  djvu_cpix_scale_ratio(djvu_ctx *ctx, const djvu_cpix *in, djvu_cpix *out,
                           int outw, int outh, int numer, int denom);
int  djvu_cpix_scale_to_topdown_rgb(djvu_ctx *ctx, const djvu_cpix *in,
                                    uint8_t *dst, int stride,
                                    int outw, int outh, int red);

void djvu_scaler_init(void);

int djvu_compose_background(djvu_doc *doc, uint32_t form_off, int width, int height,
                            int subsample, djvu_cpix *out);

typedef struct {
    double jb2_ms;
    double iw44_ms;
    double composite_ms;
    double rotate_ms;
} djvu_render_timings;

static inline void djvu_render_timings_clear(djvu_render_timings *t)
{
    t->jb2_ms = t->iw44_ms = t->composite_ms = t->rotate_ms = 0.0;
}

djvu_image *djvu_compose_page(djvu_doc *doc, int page_no, jb2_image *mask,
                              int width, int height, int subsample,
                              djvu_render_timings *t);

int djvu_compose_page_into(djvu_doc *doc, int page_no, jb2_image *mask,
                           int width, int height, int subsample,
                           uint8_t *dst, int stride);

djvu_image *djvu_page_render_timed(djvu_doc *doc, int page_no, int subsample,
                                   djvu_render_timings *t);

void djvu_debug_dump_comps(djvu_doc *doc);
djvu_image *djvu_debug_render_iw(djvu_doc *doc, int page_no, int kind);
djvu_image *djvu_debug_render_iw_gray(djvu_doc *doc, int page_no, int kind);
djvu_image *djvu_debug_render_iw_plane(djvu_doc *doc, int page_no, int kind, int plane);
djvu_image *djvu_debug_render_bg(djvu_doc *doc, int page_no);
int djvu_debug_dump_iw(djvu_doc *doc, int page_no, int kind, const char *path);

void djvu_debug_verify_mem(djvu_doc *doc, int page_no, const char *stage, FILE *out);

#endif

const djvu_zp_table djvu_zp_default_table[256] = {
  { 0x8000, 0x0000, 84, 145 },
  { 0x8000, 0x0000, 3, 4 },
  { 0x8000, 0x0000, 4, 3 },
  { 0x6bbd, 0x10a5, 5, 1 },
  { 0x6bbd, 0x10a5, 6, 2 },
  { 0x5d45, 0x1f28, 7, 3 },
  { 0x5d45, 0x1f28, 8, 4 },
  { 0x51b9, 0x2bd3, 9, 5 },
  { 0x51b9, 0x2bd3, 10, 6 },
  { 0x4813, 0x36e3, 11, 7 },
  { 0x4813, 0x36e3, 12, 8 },
  { 0x3fd5, 0x408c, 13, 9 },
  { 0x3fd5, 0x408c, 14, 10 },
  { 0x38b1, 0x48fd, 15, 11 },
  { 0x38b1, 0x48fd, 16, 12 },
  { 0x3275, 0x505d, 17, 13 },
  { 0x3275, 0x505d, 18, 14 },
  { 0x2cfd, 0x56d0, 19, 15 },
  { 0x2cfd, 0x56d0, 20, 16 },
  { 0x2825, 0x5c71, 21, 17 },
  { 0x2825, 0x5c71, 22, 18 },
  { 0x23ab, 0x615b, 23, 19 },
  { 0x23ab, 0x615b, 24, 20 },
  { 0x1f87, 0x65a5, 25, 21 },
  { 0x1f87, 0x65a5, 26, 22 },
  { 0x1bbb, 0x6962, 27, 23 },
  { 0x1bbb, 0x6962, 28, 24 },
  { 0x1845, 0x6ca2, 29, 25 },
  { 0x1845, 0x6ca2, 30, 26 },
  { 0x1523, 0x6f74, 31, 27 },
  { 0x1523, 0x6f74, 32, 28 },
  { 0x1253, 0x71e6, 33, 29 },
  { 0x1253, 0x71e6, 34, 30 },
  { 0x0fcf, 0x7404, 35, 31 },
  { 0x0fcf, 0x7404, 36, 32 },
  { 0x0d95, 0x75d6, 37, 33 },
  { 0x0d95, 0x75d6, 38, 34 },
  { 0x0b9d, 0x7768, 39, 35 },
  { 0x0b9d, 0x7768, 40, 36 },
  { 0x09e3, 0x78c2, 41, 37 },
  { 0x09e3, 0x78c2, 42, 38 },
  { 0x0861, 0x79ea, 43, 39 },
  { 0x0861, 0x79ea, 44, 40 },
  { 0x0711, 0x7ae7, 45, 41 },
  { 0x0711, 0x7ae7, 46, 42 },
  { 0x05f1, 0x7bbe, 47, 43 },
  { 0x05f1, 0x7bbe, 48, 44 },
  { 0x04f9, 0x7c75, 49, 45 },
  { 0x04f9, 0x7c75, 50, 46 },
  { 0x0425, 0x7d0f, 51, 47 },
  { 0x0425, 0x7d0f, 52, 48 },
  { 0x0371, 0x7d91, 53, 49 },
  { 0x0371, 0x7d91, 54, 50 },
  { 0x02d9, 0x7dfe, 55, 51 },
  { 0x02d9, 0x7dfe, 56, 52 },
  { 0x0259, 0x7e5a, 57, 53 },
  { 0x0259, 0x7e5a, 58, 54 },
  { 0x01ed, 0x7ea6, 59, 55 },
  { 0x01ed, 0x7ea6, 60, 56 },
  { 0x0193, 0x7ee6, 61, 57 },
  { 0x0193, 0x7ee6, 62, 58 },
  { 0x0149, 0x7f1a, 63, 59 },
  { 0x0149, 0x7f1a, 64, 60 },
  { 0x010b, 0x7f45, 65, 61 },
  { 0x010b, 0x7f45, 66, 62 },
  { 0x00d5, 0x7f6b, 67, 63 },
  { 0x00d5, 0x7f6b, 68, 64 },
  { 0x00a5, 0x7f8d, 69, 65 },
  { 0x00a5, 0x7f8d, 70, 66 },
  { 0x007b, 0x7faa, 71, 67 },
  { 0x007b, 0x7faa, 72, 68 },
  { 0x0057, 0x7fc3, 73, 69 },
  { 0x0057, 0x7fc3, 74, 70 },
  { 0x003b, 0x7fd7, 75, 71 },
  { 0x003b, 0x7fd7, 76, 72 },
  { 0x0023, 0x7fe7, 77, 73 },
  { 0x0023, 0x7fe7, 78, 74 },
  { 0x0013, 0x7ff2, 79, 75 },
  { 0x0013, 0x7ff2, 80, 76 },
  { 0x0007, 0x7ffa, 81, 77 },
  { 0x0007, 0x7ffa, 82, 78 },
  { 0x0001, 0x7fff, 81, 79 },
  { 0x0001, 0x7fff, 82, 80 },
  { 0x5695, 0x0000, 9, 85 },
  { 0x24ee, 0x0000, 86, 226 },
  { 0x8000, 0x0000, 5, 6 },
  { 0x0d30, 0x0000, 88, 176 },
  { 0x481a, 0x0000, 89, 143 },
  { 0x0481, 0x0000, 90, 138 },
  { 0x3579, 0x0000, 91, 141 },
  { 0x017a, 0x0000, 92, 112 },
  { 0x24ef, 0x0000, 93, 135 },
  { 0x007b, 0x0000, 94, 104 },
  { 0x1978, 0x0000, 95, 133 },
  { 0x0028, 0x0000, 96, 100 },
  { 0x10ca, 0x0000, 97, 129 },
  { 0x000d, 0x0000, 82, 98 },
  { 0x0b5d, 0x0000, 99, 127 },
  { 0x0034, 0x0000, 76, 72 },
  { 0x078a, 0x0000, 101, 125 },
  { 0x00a0, 0x0000, 70, 102 },
  { 0x050f, 0x0000, 103, 123 },
  { 0x0117, 0x0000, 66, 60 },
  { 0x0358, 0x0000, 105, 121 },
  { 0x01ea, 0x0000, 106, 110 },
  { 0x0234, 0x0000, 107, 119 },
  { 0x0144, 0x0000, 66, 108 },
  { 0x0173, 0x0000, 109, 117 },
  { 0x0234, 0x0000, 60, 54 },
  { 0x00f5, 0x0000, 111, 115 },
  { 0x0353, 0x0000, 56, 48 },
  { 0x00a1, 0x0000, 69, 113 },
  { 0x05c5, 0x0000, 114, 134 },
  { 0x011a, 0x0000, 65, 59 },
  { 0x03cf, 0x0000, 116, 132 },
  { 0x01aa, 0x0000, 61, 55 },
  { 0x0285, 0x0000, 118, 130 },
  { 0x0286, 0x0000, 57, 51 },
  { 0x01ab, 0x0000, 120, 128 },
  { 0x03d3, 0x0000, 53, 47 },
  { 0x011a, 0x0000, 122, 126 },
  { 0x05c5, 0x0000, 49, 41 },
  { 0x00ba, 0x0000, 124, 62 },
  { 0x08ad, 0x0000, 43, 37 },
  { 0x007a, 0x0000, 72, 66 },
  { 0x0ccc, 0x0000, 39, 31 },
  { 0x01eb, 0x0000, 60, 54 },
  { 0x1302, 0x0000, 33, 25 },
  { 0x02e6, 0x0000, 56, 50 },
  { 0x1b81, 0x0000, 29, 131 },
  { 0x045e, 0x0000, 52, 46 },
  { 0x24ef, 0x0000, 23, 17 },
  { 0x0690, 0x0000, 48, 40 },
  { 0x2865, 0x0000, 23, 15 },
  { 0x09de, 0x0000, 42, 136 },
  { 0x3987, 0x0000, 137, 7 },
  { 0x0dc8, 0x0000, 38, 32 },
  { 0x2c99, 0x0000, 21, 139 },
  { 0x10ca, 0x0000, 140, 172 },
  { 0x3b5f, 0x0000, 15, 9 },
  { 0x0b5d, 0x0000, 142, 170 },
  { 0x5695, 0x0000, 9, 85 },
  { 0x078a, 0x0000, 144, 168 },
  { 0x8000, 0x0000, 141, 248 },
  { 0x050f, 0x0000, 146, 166 },
  { 0x24ee, 0x0000, 147, 247 },
  { 0x0358, 0x0000, 148, 164 },
  { 0x0d30, 0x0000, 149, 197 },
  { 0x0234, 0x0000, 150, 162 },
  { 0x0481, 0x0000, 151, 95 },
  { 0x0173, 0x0000, 152, 160 },
  { 0x017a, 0x0000, 153, 173 },
  { 0x00f5, 0x0000, 154, 158 },
  { 0x007b, 0x0000, 155, 165 },
  { 0x00a1, 0x0000, 70, 156 },
  { 0x0028, 0x0000, 157, 161 },
  { 0x011a, 0x0000, 66, 60 },
  { 0x000d, 0x0000, 81, 159 },
  { 0x01aa, 0x0000, 62, 56 },
  { 0x0034, 0x0000, 75, 71 },
  { 0x0286, 0x0000, 58, 52 },
  { 0x00a0, 0x0000, 69, 163 },
  { 0x03d3, 0x0000, 54, 48 },
  { 0x0117, 0x0000, 65, 59 },
  { 0x05c5, 0x0000, 50, 42 },
  { 0x01ea, 0x0000, 167, 171 },
  { 0x08ad, 0x0000, 44, 38 },
  { 0x0144, 0x0000, 65, 169 },
  { 0x0ccc, 0x0000, 40, 32 },
  { 0x0234, 0x0000, 59, 53 },
  { 0x1302, 0x0000, 34, 26 },
  { 0x0353, 0x0000, 55, 47 },
  { 0x1b81, 0x0000, 30, 174 },
  { 0x05c5, 0x0000, 175, 193 },
  { 0x24ef, 0x0000, 24, 18 },
  { 0x03cf, 0x0000, 177, 191 },
  { 0x2b74, 0x0000, 178, 222 },
  { 0x0285, 0x0000, 179, 189 },
  { 0x201d, 0x0000, 180, 218 },
  { 0x01ab, 0x0000, 181, 187 },
  { 0x1715, 0x0000, 182, 216 },
  { 0x011a, 0x0000, 183, 185 },
  { 0x0fb7, 0x0000, 184, 214 },
  { 0x00ba, 0x0000, 69, 61 },
  { 0x0a67, 0x0000, 186, 212 },
  { 0x01eb, 0x0000, 59, 53 },
  { 0x06e7, 0x0000, 188, 210 },
  { 0x02e6, 0x0000, 55, 49 },
  { 0x0496, 0x0000, 190, 208 },
  { 0x045e, 0x0000, 51, 45 },
  { 0x030d, 0x0000, 192, 206 },
  { 0x0690, 0x0000, 47, 39 },
  { 0x0206, 0x0000, 194, 204 },
  { 0x09de, 0x0000, 41, 195 },
  { 0x0155, 0x0000, 196, 202 },
  { 0x0dc8, 0x0000, 37, 31 },
  { 0x00e1, 0x0000, 198, 200 },
  { 0x2b74, 0x0000, 199, 243 },
  { 0x0094, 0x0000, 72, 64 },
  { 0x201d, 0x0000, 201, 239 },
  { 0x0188, 0x0000, 62, 56 },
  { 0x1715, 0x0000, 203, 237 },
  { 0x0252, 0x0000, 58, 52 },
  { 0x0fb7, 0x0000, 205, 235 },
  { 0x0383, 0x0000, 54, 48 },
  { 0x0a67, 0x0000, 207, 233 },
  { 0x0547, 0x0000, 50, 44 },
  { 0x06e7, 0x0000, 209, 231 },
  { 0x07e2, 0x0000, 46, 38 },
  { 0x0496, 0x0000, 211, 229 },
  { 0x0bc0, 0x0000, 40, 34 },
  { 0x030d, 0x0000, 213, 227 },
  { 0x1178, 0x0000, 36, 28 },
  { 0x0206, 0x0000, 215, 225 },
  { 0x19da, 0x0000, 30, 22 },
  { 0x0155, 0x0000, 217, 223 },
  { 0x24ef, 0x0000, 26, 16 },
  { 0x00e1, 0x0000, 219, 221 },
  { 0x320e, 0x0000, 20, 220 },
  { 0x0094, 0x0000, 71, 63 },
  { 0x432a, 0x0000, 14, 8 },
  { 0x0188, 0x0000, 61, 55 },
  { 0x447d, 0x0000, 14, 224 },
  { 0x0252, 0x0000, 57, 51 },
  { 0x5ece, 0x0000, 8, 2 },
  { 0x0383, 0x0000, 53, 47 },
  { 0x8000, 0x0000, 228, 87 },
  { 0x0547, 0x0000, 49, 43 },
  { 0x481a, 0x0000, 230, 246 },
  { 0x07e2, 0x0000, 45, 37 },
  { 0x3579, 0x0000, 232, 244 },
  { 0x0bc0, 0x0000, 39, 33 },
  { 0x24ef, 0x0000, 234, 238 },
  { 0x1178, 0x0000, 35, 27 },
  { 0x1978, 0x0000, 138, 236 },
  { 0x19da, 0x0000, 29, 21 },
  { 0x2865, 0x0000, 24, 16 },
  { 0x24ef, 0x0000, 25, 15 },
  { 0x3987, 0x0000, 240, 8 },
  { 0x320e, 0x0000, 19, 241 },
  { 0x2c99, 0x0000, 22, 242 },
  { 0x432a, 0x0000, 13, 7 },
  { 0x3b5f, 0x0000, 16, 10 },
  { 0x447d, 0x0000, 13, 245 },
  { 0x5695, 0x0000, 10, 2 },
  { 0x5ece, 0x0000, 7, 1 },
  { 0x8000, 0x0000, 244, 83 },
  { 0x8000, 0x0000, 249, 250 },
  { 0x5695, 0x0000, 10, 2 },
  { 0x481a, 0x0000, 89, 143 },
  { 0x481a, 0x0000, 230, 246 },
  { 0x0000, 0x0000, 0, 0 },
  { 0x0000, 0x0000, 0, 0 },
  { 0x0000, 0x0000, 0, 0 },
  { 0x0000, 0x0000, 0, 0 },
  { 0x0000, 0x0000, 0, 0 },
};

void djvu_zp_init(djvu_zp *zp, const uint8_t *data, size_t len)
{
    int i, j, b;

    zp->data = data;
    zp->len = len;
    zp->pos = 0;
    zp->a = 0;
    zp->buffer = 0;
    zp->scount = 0;
    zp->zbyte = 0;
    zp->eof = 0;


    for (i = 0; i < 256; i++) {
        zp->ffzt[i] = 0;
        for (j = i; (j & 0x80) != 0; j <<= 1)
            zp->ffzt[i]++;
    }

    for (i = 0; i < 256; i++) {
        zp->p[i] = djvu_zp_default_table[i].p;
        zp->m[i] = djvu_zp_default_table[i].m;
        zp->up[i] = djvu_zp_default_table[i].up;
        zp->dn[i] = djvu_zp_default_table[i].dn;
    }


    zp->code = 0xff00;
    b = zp_read_byte(zp);
    zp->code = 0xff00u & (uint32_t)(b << 8);
    b = zp_read_byte(zp);
    zp->zbyte = (uint8_t)(0xff & b);
    zp->code |= zp->zbyte;
    zp->delay = 25;
    zp->scount = 0;
    zp_preload(zp);
    zp->fence = zp->code;
    if (zp->code >= 0x8000)
        zp->fence = 0x7fff;
}

#include <stdlib.h>
#include <string.h>

#define BZZ_MAXBLOCK 4096
#define BZZ_FREQMAX  4
#define BZZ_CTXIDS   3

#define BZZ_CXT_SIZE 384

typedef struct {
    djvu_zp zp;
    djvu_zp_ctx cxt[BZZ_CXT_SIZE];
    uint8_t *block;
    int block_cap;
    int *pos;
    int pos_cap;
} bzz_dec;

static int bzz_decode_raw(bzz_dec *d, int bits)
{
    int n = 1;
    int m = (1 << bits);
    while (n < m) {
        int b = djvu_zp_decode_pass(&d->zp);
        n = (n << 1) | b;
    }
    return n - m;
}

static int bzz_decode_binary(bzz_dec *d, int ctxoff, int bits)
{
    int n = 1;
    int m = (1 << bits);
    ctxoff--;
    while (n < m) {
        int b = djvu_zp_decode(&d->zp, &d->cxt[ctxoff + n]);
        n = (n << 1) | b;
    }
    return n - m;
}

static int bzz_decode_block(bzz_dec *d, djvu_ctx *ctx)
{
    int size = bzz_decode_raw(d, 24);
    int fshift = 0, fadd = 4, mtfno = 3, markerpos = -1;
    int i, k, last, j2;
    uint8_t mtf[256];
    int freq[BZZ_FREQMAX];
    int count[256];
    uint8_t *data;

    if (size == 0)
        return 0;
    if (size > BZZ_MAXBLOCK * 1024) {
        djvu_errorf(ctx, DJVU_SEVERITY_ERROR, "bzz: corrupt block size");
        return -1;
    }

    if (d->block_cap < size) {
        djvu_free(ctx, d->block);
        d->block = (uint8_t *)djvu_alloc(ctx, (size_t)size);
        if (!d->block) return -1;
        d->block_cap = size;
    }
    data = d->block;


    if (djvu_zp_decode_pass(&d->zp) != 0) {
        fshift++;
        if (djvu_zp_decode_pass(&d->zp) != 0)
            fshift++;
    }


    for (i = 0; i < 256; i++) mtf[i] = (uint8_t)i;
    for (i = 0; i < BZZ_FREQMAX; i++) freq[i] = 0;

    for (i = 0; i < size; i++) {
        int ctxid = BZZ_CTXIDS - 1;
        int ctxoff = 0;
        int fc;
        if (ctxid > mtfno) ctxid = mtfno;

        if (djvu_zp_decode(&d->zp, &d->cxt[ctxoff + ctxid]) != 0) {
            mtfno = 0; data[i] = mtf[mtfno];
        } else if ((ctxoff += BZZ_CTXIDS),
                   djvu_zp_decode(&d->zp, &d->cxt[ctxoff + ctxid]) != 0) {
            mtfno = 1; data[i] = mtf[mtfno];
        } else if ((ctxoff += BZZ_CTXIDS),
                   djvu_zp_decode(&d->zp, &d->cxt[ctxoff + 0]) != 0) {
            mtfno = 2 + bzz_decode_binary(d, ctxoff + 1, 1); data[i] = mtf[mtfno];
        } else if ((ctxoff += (1 + 1)),
                   djvu_zp_decode(&d->zp, &d->cxt[ctxoff + 0]) != 0) {
            mtfno = 4 + bzz_decode_binary(d, ctxoff + 1, 2); data[i] = mtf[mtfno];
        } else if ((ctxoff += (1 + 3)),
                   djvu_zp_decode(&d->zp, &d->cxt[ctxoff + 0]) != 0) {
            mtfno = 8 + bzz_decode_binary(d, ctxoff + 1, 3); data[i] = mtf[mtfno];
        } else if ((ctxoff += (1 + 7)),
                   djvu_zp_decode(&d->zp, &d->cxt[ctxoff + 0]) != 0) {
            mtfno = 16 + bzz_decode_binary(d, ctxoff + 1, 4); data[i] = mtf[mtfno];
        } else if ((ctxoff += (1 + 15)),
                   djvu_zp_decode(&d->zp, &d->cxt[ctxoff + 0]) != 0) {
            mtfno = 32 + bzz_decode_binary(d, ctxoff + 1, 5); data[i] = mtf[mtfno];
        } else if ((ctxoff += (1 + 31)),
                   djvu_zp_decode(&d->zp, &d->cxt[ctxoff + 0]) != 0) {
            mtfno = 64 + bzz_decode_binary(d, ctxoff + 1, 6); data[i] = mtf[mtfno];
        } else if ((ctxoff += (1 + 63)),
                   djvu_zp_decode(&d->zp, &d->cxt[ctxoff + 0]) != 0) {
            mtfno = 128 + bzz_decode_binary(d, ctxoff + 1, 7); data[i] = mtf[mtfno];
        } else {
            mtfno = 256; data[i] = 0; markerpos = i;
            continue;
        }


        fadd = fadd + (fadd >> fshift);
        if (fadd > 0x10000000) {
            fadd >>= 24;
            freq[0] >>= 24; freq[1] >>= 24; freq[2] >>= 24; freq[3] >>= 24;
        }
        fc = fadd;
        if (mtfno < BZZ_FREQMAX) fc += freq[mtfno];
        for (k = mtfno; k >= BZZ_FREQMAX; k--)
            mtf[k] = mtf[k - 1];
        for (; k > 0 && (uint32_t)fc >= (uint32_t)freq[k - 1]; k--) {
            mtf[k] = mtf[k - 1];
            freq[k] = freq[k - 1];
        }
        mtf[k] = data[i];
        freq[k] = fc;
    }


    if (markerpos < 1 || markerpos >= size) {
        djvu_errorf(ctx, DJVU_SEVERITY_ERROR, "bzz: corrupt (markerpos)");
        return -1;
    }

    if (d->pos_cap < size) {
        djvu_free(ctx, d->pos);
        d->pos = (int *)djvu_alloc(ctx, sizeof(int) * (size_t)size);
        if (!d->pos) return -1;
        d->pos_cap = size;
    }

    for (i = 0; i < 256; i++) count[i] = 0;
    for (i = 0; i < markerpos; i++) {
        signed char c = (signed char)data[i];
        d->pos[i] = (c << 24) | (count[0xff & c] & 0xffffff);
        count[0xff & c]++;
    }
    for (i = markerpos + 1; i < size; i++) {
        signed char c = (signed char)data[i];
        d->pos[i] = (c << 24) | (count[0xff & c] & 0xffffff);
        count[0xff & c]++;
    }

    last = 1;
    for (i = 0; i < 256; i++) {
        int tmp = count[i];
        count[i] = last;
        last += tmp;
    }

    j2 = 0;
    last = size - 1;
    while (last > 0) {
        int n, c;

        if (j2 < 0 || j2 >= size) {
            djvu_errorf(ctx, DJVU_SEVERITY_ERROR, "bzz: corrupt (BWT index)");
            return -1;
        }
        n = d->pos[j2];
        c = (signed char)(d->pos[j2] >> 24);
        data[--last] = (uint8_t)c;
        j2 = count[0xff & c] + (n & 0xffffff);
    }

    if (j2 != markerpos) {
        djvu_errorf(ctx, DJVU_SEVERITY_ERROR, "bzz: corrupt (BWT)");
        return -1;
    }


    return size - 1;
}

uint8_t *djvu_bzz_decode_all(djvu_ctx *ctx, const uint8_t *data, size_t len,
                             size_t *out_len)
{
    bzz_dec *d;
    uint8_t *out = NULL;
    size_t out_cap = 0, out_size = 0;

    d = (bzz_dec *)djvu_alloc(ctx, sizeof(bzz_dec));
    if (!d) return NULL;
    memset(d, 0, sizeof(*d));
    djvu_zp_init(&d->zp, data, len);

    for (;;) {
        int n = bzz_decode_block(d, ctx);
        if (n < 0) { djvu_free(ctx, out); out = NULL; out_size = 0; break; }
        if (n == 0) break;
        if (out_size + (size_t)n + 1 > out_cap) {
            size_t ncap = out_cap ? out_cap * 2 : 65536;
            uint8_t *no;
            while (ncap < out_size + (size_t)n + 1) ncap *= 2;
            no = (uint8_t *)djvu_alloc(ctx, ncap);
            if (!no) { djvu_free(ctx, out); out = NULL; out_size = 0; break; }
            if (out) { memcpy(no, out, out_size); djvu_free(ctx, out); }
            out = no; out_cap = ncap;
        }
        memcpy(out + out_size, d->block, (size_t)n);
        out_size += (size_t)n;
    }

    djvu_free(ctx, d->block);
    djvu_free(ctx, d->pos);
    djvu_free(ctx, d);
    if (out) {
        out[out_size] = 0;
        if (out_len) *out_len = out_size;
    }
    return out;
}

#include <string.h>

enum {
    DJVU_BM_RUN_OVERFLOW = 0xc0,
    DJVU_BM_MAX_RUN = 0x3fff
};

static int djvu_bm_alloc_guard(djvu_ctx *ctx, djvu_bitmap *bm)
{
    size_t gs = (size_t)bm->bytes_per_row + (size_t)bm->border;
    djvu_free(ctx, bm->guard);
    bm->guard = NULL;
    if (gs == 0) return 0;
    bm->guard = (uint8_t *)djvu_alloc(ctx, gs);
    if (!bm->guard) return -1;
    memset(bm->guard, 0, gs);
    return 0;
}

static int bm_read_run(const uint8_t **data)
{
    int z = *(*data)++;
    if (z >= DJVU_BM_RUN_OVERFLOW)
        z = ((z & ~DJVU_BM_RUN_OVERFLOW) << 8) | (int)(*(*data)++);
    return z;
}

static void bm_append_long_run(uint8_t **data, int count)
{
    while (count > DJVU_BM_MAX_RUN) {
        (*data)[0] = (*data)[1] = 0xff;
        (*data)[2] = 0;
        *data += 3;
        count -= DJVU_BM_MAX_RUN;
    }
    if (count < DJVU_BM_RUN_OVERFLOW) {
        (*data)[0] = (uint8_t)count;
        *data += 1;
    } else {
        (*data)[0] = (uint8_t)((count >> 8) + DJVU_BM_RUN_OVERFLOW);
        (*data)[1] = (uint8_t)(count & 0xff);
        *data += 2;
    }
}

static void bm_append_run(uint8_t **data, int count)
{
    if (count < DJVU_BM_RUN_OVERFLOW) {
        (*data)[0] = (uint8_t)count;
        *data += 1;
    } else if (count <= DJVU_BM_MAX_RUN) {
        (*data)[0] = (uint8_t)((count >> 8) + DJVU_BM_RUN_OVERFLOW);
        (*data)[1] = (uint8_t)(count & 0xff);
        *data += 2;
    } else {
        bm_append_long_run(data, count);
    }
}

static void bm_append_line(uint8_t **data, const uint8_t *row, int rowlen)
{
    const uint8_t *rowend = row + rowlen;
    int p = 1;

    while (row < rowend) {
        const uint8_t *start;
        const void *next;
        int count;

        p = !p;
        start = row;
        next = memchr(row, p ? 0 : 1, (size_t)(rowend - row));
        row = next ? (const uint8_t *)next : rowend;
        count = (int)(row - start);
        bm_append_run(data, count);
    }
}

static size_t bm_encode_rle(djvu_ctx *ctx, const djvu_bitmap *bm,
                            uint8_t **out_rle)
{
    int pos = 0, maxpos, n;
    uint8_t *runs, *runs_pos;
    const uint8_t *row;

    if (!bm->data || bm->width <= 0 || bm->height <= 0)
        return 0;

    maxpos = 1024 + bm->width + bm->width;
    runs = (uint8_t *)djvu_alloc(ctx, (size_t)maxpos);
    if (!runs) return 0;

    n = bm->height - 1;
    row = bm->data + djvu_bm_rowoffset(bm, n);
    while (n >= 0) {
        if (maxpos < pos + bm->width + bm->width + 2) {
            uint8_t *nr;
            maxpos += 1024 + bm->width + bm->width;
            nr = (uint8_t *)djvu_alloc(ctx, (size_t)maxpos);
            if (!nr) { djvu_free(ctx, runs); return 0; }
            memcpy(nr, runs, (size_t)pos);
            djvu_free(ctx, runs);
            runs = nr;
        }
        runs_pos = runs + pos;
        {
            const uint8_t *start = runs_pos;
            bm_append_line(&runs_pos, row, bm->width);
            pos += (int)(runs_pos - start);
        }
        row -= bm->bytes_per_row;
        n--;
    }

    *out_rle = runs;
    return (size_t)pos;
}

int djvu_bm_init(djvu_ctx *ctx, djvu_bitmap *bm, int height, int width, int border)
{
    long max;
    djvu_free(ctx, bm->data);
    djvu_free(ctx, bm->guard);
    djvu_free(ctx, bm->rle);
    bm->data = NULL;
    bm->guard = NULL;
    bm->rle = NULL;
    bm->rle_len = 0;
    bm->width = width;
    bm->height = height;
    bm->border = border;
    bm->bytes_per_row = width + border;
    max = (long)height * bm->bytes_per_row + border;
    bm->max_offset = (int)max;
    if (max > 0) {
        bm->data = (uint8_t *)djvu_alloc(ctx, (size_t)max);
        if (!bm->data) return -1;
        memset(bm->data, 0, (size_t)max);
    }
    return djvu_bm_alloc_guard(ctx, bm);
}

void djvu_bm_free(djvu_ctx *ctx, djvu_bitmap *bm)
{
    if (bm) {
        djvu_free(ctx, bm->data);
        djvu_free(ctx, bm->guard);
        djvu_free(ctx, bm->rle);
        bm->data = NULL;
        bm->guard = NULL;
        bm->rle = NULL;
        bm->rle_len = 0;
    }
}

static void bm_decode_rle_rows(djvu_bitmap *bm, const uint8_t *runs)
{
    int c = 0, n = bm->height - 1, p = 0, x;

    while (n >= 0) {
        x = bm_read_run(&runs);
        if (c + x > bm->width)
            break;
        while (x-- > 0)
            bm->data[djvu_bm_rowoffset(bm, n) + c++] = (uint8_t)p;
        p = 1 - p;
        if (c >= bm->width) {
            c = 0;
            p = 0;
            n--;
        }
    }
}

void djvu_bm_uncompress(djvu_ctx *ctx, djvu_bitmap *bm)
{
    if (!bm || bm->data || !bm->rle || bm->width <= 0 || bm->height <= 0)
        return;

    bm->bytes_per_row = bm->width + bm->border;
    bm->max_offset = bm->height * bm->bytes_per_row + bm->border;
    bm->data = (uint8_t *)djvu_alloc(ctx, (size_t)bm->max_offset);
    if (!bm->data) return;
    memset(bm->data, 0, (size_t)bm->max_offset);
    if (djvu_bm_alloc_guard(ctx, bm) != 0) {
        djvu_free(ctx, bm->data);
        bm->data = NULL;
        return;
    }

    bm_decode_rle_rows(bm, bm->rle);

    djvu_free(ctx, bm->rle);
    bm->rle = NULL;
    bm->rle_len = 0;
}

int djvu_bm_uncompress_copy(djvu_ctx *ctx, const djvu_bitmap *src, djvu_bitmap *dst)
{
    memset(dst, 0, sizeof(*dst));
    if (!src || !src->rle || src->width <= 0 || src->height <= 0)
        return -1;

    dst->width = src->width;
    dst->height = src->height;
    dst->border = src->border;
    dst->bytes_per_row = src->width + src->border;
    dst->max_offset = src->height * dst->bytes_per_row + dst->border;
    dst->data = (uint8_t *)djvu_alloc(ctx, (size_t)dst->max_offset);
    if (!dst->data) return -1;
    memset(dst->data, 0, (size_t)dst->max_offset);
    if (djvu_bm_alloc_guard(ctx, dst) != 0) {
        djvu_free(ctx, dst->data);
        memset(dst, 0, sizeof(*dst));
        return -1;
    }

    bm_decode_rle_rows(dst, src->rle);
    return 0;
}

void djvu_bm_ensure_bytes(djvu_ctx *ctx, djvu_bitmap *bm)
{
    if (bm && !bm->data && bm->rle)
        djvu_bm_uncompress(ctx, bm);
}

void djvu_bm_compress(djvu_ctx *ctx, djvu_bitmap *bm)
{
    uint8_t *nrle;
    size_t len;

    if (!bm || !bm->data || bm->width <= 0 || bm->height <= 0)
        return;

    len = bm_encode_rle(ctx, bm, &nrle);
    if (!len) return;

    djvu_free(ctx, bm->data);
    bm->data = NULL;
    djvu_free(ctx, bm->rle);
    bm->rle = nrle;
    bm->rle_len = len;
}

int djvu_bm_set_min_border(djvu_ctx *ctx, djvu_bitmap *bm, int value)
{
    int new_bpr, r;
    long new_max;
    uint8_t *nd;

    if (!bm->data && bm->rle)
        djvu_bm_uncompress(ctx, bm);
    if (bm->border >= value) return 0;
    new_bpr = bm->width + value;
    new_max = (long)bm->height * new_bpr + value;
    nd = (uint8_t *)djvu_alloc(ctx, (size_t)(new_max > 0 ? new_max : 1));
    if (!nd) return -1;
    memset(nd, 0, (size_t)(new_max > 0 ? new_max : 1));
    if (bm->data) {
        for (r = 0; r < bm->height; r++) {
            int src = r * bm->bytes_per_row + bm->border;
            int dst = r * new_bpr + value;
            memcpy(nd + dst, bm->data + src, (size_t)bm->width);
        }
    }
    djvu_free(ctx, bm->data);
    bm->data = nd;
    bm->border = value;
    bm->bytes_per_row = new_bpr;
    bm->max_offset = (int)new_max;
    return djvu_bm_alloc_guard(ctx, bm);
}

static void bm_blit_bytes(djvu_bitmap *dst, const djvu_bitmap *src,
                          int x0, int y0, int x1, int y1, int w, int h, int maxval)
{
    do {
        int off = djvu_bm_rowoffset(dst, y0++) + x0;
        int roff = djvu_bm_rowoffset(src, y1++) + x1;
        int i = w;
        do {
            int g = dst->data[off] + src->data[roff++];
            dst->data[off++] = (uint8_t)(g <= maxval ? g : maxval);
        } while (--i > 0);
    } while (--h > 0);
}

static void bm_blit_rle(djvu_bitmap *dst, const djvu_bitmap *src,
                        int dx, int dy, int maxval)
{
    const uint8_t *runs = src->rle;
    const uint8_t *runs_end = src->rle + src->rle_len;
    int sr = src->height - 1;
    int sc = 0, p = 0;

    if (!dst->data) return;

    while (runs < runs_end && sr >= 0) {
        int z = bm_read_run(&runs);
        int nc;

        if (sc + z > src->width) return;
        nc = sc + z;

        if (p) {
            int dest_row = dy + sr;
            if (dest_row >= 0 && dest_row < dst->height) {
                uint8_t *drow = dst->data + djvu_bm_rowoffset(dst, dest_row);
                int col = sc;
                if (dx < 0) {
                    if (nc <= -dx) goto next_run;
                    col = sc + (-dx);
                }
                while (col < nc) {
                    int px = dx + col;
                    if (px >= dst->width) break;
                    if (px >= 0) {
                        int g = drow[px] + 1;
                        drow[px] = (uint8_t)(g <= maxval ? g : maxval);
                    }
                    col++;
                }
            }
        }
    next_run:
        sc = nc;
        p = 1 - p;
        if (sc >= src->width) {
            sc = 0;
            p = 0;
            sr--;
        }
    }
}

void djvu_bm_blit(djvu_bitmap *dst, const djvu_bitmap *src, int dx, int dy, int maxval)
{
    int x0, y0, x1, y1, w0, w1, w, h0, h1, h;

    if (!dst || !src || !dst->data) return;
    if ((dx >= dst->width) || (dy >= dst->height) ||
        (dx + src->width < 0) || (dy + src->height < 0))
        return;

    if (src->data) {
        x0 = dx > 0 ? dx : 0;
        y0 = dy > 0 ? dy : 0;
        x1 = dx < 0 ? -dx : 0;
        y1 = dy < 0 ? -dy : 0;
        w0 = dst->width - x0;
        w1 = src->width - x1;
        w = w0 < w1 ? w0 : w1;
        h0 = dst->height - y0;
        h1 = src->height - y1;
        h = h0 < h1 ? h0 : h1;
        if (w <= 0 || h <= 0) return;
        bm_blit_bytes(dst, src, x0, y0, x1, y1, w, h, maxval);
        return;
    }

    if (src->rle)
        bm_blit_rle(dst, src, dx, dy, maxval);
}

static void bm_visit_ink_bytes(const djvu_bitmap *src, int left, int bottom,
                               void (*fn)(void *, int, int), void *user)
{
    int rr, cc, sw = src->width, sh = src->height;

    for (rr = 0; rr < sh; rr++) {
        int srow = djvu_bm_rowoffset(src, rr);
        int py = bottom + rr;
        for (cc = 0; cc < sw; cc++) {
            if (src->data[srow + cc])
                fn(user, left + cc, py);
        }
    }
}

static void bm_visit_ink_rle(const djvu_bitmap *src, int left, int bottom,
                             void (*fn)(void *, int, int), void *user)
{
    const uint8_t *runs = src->rle;
    const uint8_t *runs_end = src->rle + src->rle_len;
    int sr = src->height - 1;
    int sc = 0, p = 0;

    while (runs < runs_end && sr >= 0) {
        int z = bm_read_run(&runs);
        int nc;

        if (sc + z > src->width) return;
        nc = sc + z;

        if (p) {
            int py = bottom + sr;
            int col = sc;
            while (col < nc)
                fn(user, left + col++, py);
        }
        sc = nc;
        p = 1 - p;
        if (sc >= src->width) {
            sc = 0;
            p = 0;
            sr--;
        }
    }
}

void djvu_bm_visit_ink(const djvu_bitmap *src, int left, int bottom,
                       void (*fn)(void *user, int px, int py), void *user)
{
    if (!src || !fn) return;
    if (src->data)
        bm_visit_ink_bytes(src, left, bottom, fn, user);
    else if (src->rle)
        bm_visit_ink_rle(src, left, bottom, fn, user);
}

static void bm_visit_ink_runs_bytes(const djvu_bitmap *src, int left, int bottom,
                                    void (*fn)(void *, int, int, int), void *user)
{
    int rr, sw = src->width, sh = src->height;

    for (rr = 0; rr < sh; rr++) {
        const uint8_t *row = src->data + djvu_bm_rowoffset(src, rr);
        const uint8_t *end = row + sw;
        const uint8_t *p = row;
        int py = bottom + rr;

        while (p < end) {
            const uint8_t *start;
            const void *next = memchr(p, 1, (size_t)(end - p));
            if (!next) break;
            start = (const uint8_t *)next;
            next = memchr(start, 0, (size_t)(end - start));
            p = next ? (const uint8_t *)next : end;
            fn(user, left + (int)(start - row), left + (int)(p - row), py);
        }
    }
}

static void bm_visit_ink_runs_rle(const djvu_bitmap *src, int left, int bottom,
                                  void (*fn)(void *, int, int, int), void *user)
{
    const uint8_t *runs = src->rle;
    const uint8_t *runs_end = src->rle + src->rle_len;
    int sr = src->height - 1;
    int sc = 0, p = 0;

    while (runs < runs_end && sr >= 0) {
        int z = bm_read_run(&runs);
        int nc;

        if (sc + z > src->width) return;
        nc = sc + z;
        if (p && nc > sc)
            fn(user, left + sc, left + nc, bottom + sr);
        sc = nc;
        p = 1 - p;
        if (sc >= src->width) {
            sc = 0;
            p = 0;
            sr--;
        }
    }
}

void djvu_bm_visit_ink_runs(const djvu_bitmap *src, int left, int bottom,
                            void (*fn)(void *user, int x0, int x1, int py),
                            void *user)
{
    if (!src || !fn) return;
    if (src->data)
        bm_visit_ink_runs_bytes(src, left, bottom, fn, user);
    else if (src->rle)
        bm_visit_ink_runs_rle(src, left, bottom, fn, user);
}

static void bm_bbox_rle(const djvu_bitmap *bm, int *xmin, int *ymin, int *xmax, int *ymax)
{
    const uint8_t *runs = bm->rle;
    int w = bm->width, h = bm->height;
    int xa = w, ya = h, xb = 0, yb = 0, area = 0;
    int r = h;

    while (--r >= 0) {
        int p = 0, c = 0, n = 0;
        while (c < w) {
            int x = bm_read_run(&runs);
            if (x) {
                if (p) {
                    if (c < xa) xa = c;
                    c += x;
                    if (c - 1 > xb) xb = c - 1;
                    n += x;
                } else {
                    c += x;
                }
            }
            p = 1 - p;
        }
        area += n;
        if (n) {
            ya = r;
            if (r > yb) yb = r;
        }
    }
    if (area == 0) {
        *xmin = 1; *ymin = 1; *xmax = 0; *ymax = 0;
        return;
    }
    *xmin = xa; *ymin = ya; *xmax = xb; *ymax = yb;
}

void djvu_bm_bbox(const djvu_bitmap *bm, int *xmin, int *ymin, int *xmax, int *ymax)
{
    int w = bm->width, h = bm->height, s = bm->bytes_per_row;
    int xa, xb, ya, yb;

    if (!bm->data && bm->rle) {
        bm_bbox_rle(bm, xmin, ymin, xmax, ymax);
        return;
    }

    for (xb = w - 1; xb >= 0; xb--) {
        int p = djvu_bm_rowoffset(bm, 0) + xb;
        int pe = p + s * h;
        while (p < pe && bm->data[p] == 0) p += s;
        if (p < pe) break;
    }
    for (yb = h - 1; yb >= 0; yb--) {
        int p = djvu_bm_rowoffset(bm, yb);
        int pe = p + w;
        while (p < pe && bm->data[p] == 0) ++p;
        if (p < pe) break;
    }
    for (xa = 0; xa <= xb; xa++) {
        int p = djvu_bm_rowoffset(bm, 0) + xa;
        int pe = p + s * h;
        while (p < pe && bm->data[p] == 0) p += s;
        if (p < pe) break;
    }
    for (ya = 0; ya <= yb; ya++) {
        int p = djvu_bm_rowoffset(bm, ya);
        int pe = p + w;
        while (p < pe && bm->data[p] == 0) ++p;
        if (p < pe) break;
    }
    *xmin = xa; *ymin = ya; *xmax = xb; *ymax = yb;
}

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    REC_StartOfData = 0,
    REC_NewMark = 1,
    REC_NewMarkLibraryOnly = 2,
    REC_NewMarkImageOnly = 3,
    REC_MatchedRefine = 4,
    REC_MatchedRefineLibraryOnly = 5,
    REC_MatchedRefineImageOnly = 6,
    REC_MatchedCopy = 7,
    REC_NonMarkData = 8,
    REC_RequiredDictOrReset = 9,
    REC_PreservedComment = 10,
    REC_EndOfData = 11
};

#define BIGPOSITIVE  262142
#define BIGNEGATIVE (-262143)

typedef struct {
    djvu_ctx *ctx;
    djvu_zp zp;


    uint8_t *bitcells;
    int *leftcell;
    int *rightcell;
    int ncells, cap_cells;


    int dist_record_type, dist_match_index;
    int abs_loc_x, abs_loc_y, abs_size_x, abs_size_y;
    int image_size_dist, inherited_shape_count_dist;
    int rel_loc_x_cur, rel_loc_x_last, rel_loc_y_cur, rel_loc_y_last;
    int rel_size_x, rel_size_y;
    int dist_comment_byte, dist_comment_length;

    uint8_t offset_type_dist;
    uint8_t dist_refinement_flag;


    uint8_t bitdist[1024];
    uint8_t cbitdist[2048];


    int *lib2shape; int nlib2shape, cap_lib2shape;
    int *shape2lib; int nshape2lib, cap_shape2lib;
    int *libinfo;
    int nlibinfo, cap_libinfo;

    int shortlist[3], shortlistpos;
    int last_left, last_right, last_bottom, last_row_left, last_row_bottom;
    int image_columns, image_rows;
    int got_start_record;
    int refinementp;

    jb2_image *zdict;
    int error;
} jb2_codec;

jb2_image *jb2_image_new(djvu_ctx *ctx)
{
    jb2_image *im = (jb2_image *)djvu_alloc(ctx, sizeof(jb2_image));
    if (!im) return NULL;
    memset(im, 0, sizeof(*im));
    im->ctx = ctx;
    return im;
}

void djvu_jb2_free(djvu_ctx *ctx, jb2_image *im)
{
    int i;
    if (!im) return;
    for (i = 0; i < im->nshapes; i++)
        djvu_bm_free(ctx, &im->shapes[i].bm);
    djvu_free(ctx, im->shapes);
    djvu_free(ctx, im->blits);
    djvu_free(ctx, im);
}

static int img_shapecount(jb2_image *im)
{
    return im->inherited_shapes + im->nshapes;
}

jb2_shape *djvu_jb2_get_shape(jb2_image *im, int n)
{
    if (n < 0) return NULL;
    if (n >= im->inherited_shapes) {
        int local = n - im->inherited_shapes;

        if (local >= im->nshapes) return NULL;
        return &im->shapes[local];
    }
    if (im->inherited_dict)
        return djvu_jb2_get_shape(im->inherited_dict, n);
    return NULL;
}

static int img_add_shape(jb2_image *im, int parent)
{
    int no = im->inherited_shapes + im->nshapes;
    if (im->nshapes >= im->cap_shapes) {
        int nc = im->cap_shapes ? im->cap_shapes * 2 : 64;
        jb2_shape *ns = (jb2_shape *)djvu_alloc(im->ctx, sizeof(jb2_shape) * nc);
        if (!ns) return -1;
        if (im->shapes) { memcpy(ns, im->shapes, sizeof(jb2_shape) * im->nshapes);
                          djvu_free(im->ctx, im->shapes); }
        im->shapes = ns; im->cap_shapes = nc;
    }
    memset(&im->shapes[im->nshapes], 0, sizeof(jb2_shape));
    im->shapes[im->nshapes].parent = parent;
    im->nshapes++;
    return no;
}

static int img_add_blit(jb2_image *im, jb2_blit b)
{
    if (im->nblits >= im->cap_blits) {
        int nc = im->cap_blits ? im->cap_blits * 2 : 256;
        jb2_blit *nb = (jb2_blit *)djvu_alloc(im->ctx, sizeof(jb2_blit) * nc);
        if (!nb) return -1;
        if (im->blits) { memcpy(nb, im->blits, sizeof(jb2_blit) * im->nblits);
                         djvu_free(im->ctx, im->blits); }
        im->blits = nb; im->cap_blits = nc;
    }
    im->blits[im->nblits++] = b;
    return 0;
}

static void img_set_inherited_dict(jb2_image *im, jb2_image *dict)
{
    im->inherited_dict = dict;
    im->inherited_shapes = dict ? img_shapecount(dict) : 0;
}

static int iarr_push(djvu_ctx *ctx, int **arr, int *n, int *cap, int v)
{
    if (*n >= *cap) {
        int nc = *cap ? *cap * 2 : 64;
        int *na = (int *)djvu_alloc(ctx, sizeof(int) * nc);
        if (!na) return -1;
        if (*arr) { memcpy(na, *arr, sizeof(int) * *n); djvu_free(ctx, *arr); }
        *arr = na; *cap = nc;
    }
    (*arr)[(*n)++] = v;
    return 0;
}

static int code_bit_cell(jb2_codec *c, int cell)
{
    return djvu_zp_decode(&c->zp, &c->bitcells[cell]);
}
static void ensure_cells(jb2_codec *c, int need)
{
    if (need <= c->cap_cells) return;
    {
        int nc = c->cap_cells ? c->cap_cells : 256;
        uint8_t *nb; int *nl, *nr;
        while (nc < need) nc *= 2;
        nb = (uint8_t *)djvu_alloc(c->ctx, sizeof(uint8_t) * nc);
        nl = (int *)djvu_alloc(c->ctx, sizeof(int) * nc);
        nr = (int *)djvu_alloc(c->ctx, sizeof(int) * nc);
        if (!nb || !nl || !nr) { c->error = 1; djvu_free(c->ctx, nb);
            djvu_free(c->ctx, nl); djvu_free(c->ctx, nr); return; }
        if (c->ncells) {
            memcpy(nb, c->bitcells, sizeof(uint8_t) * c->ncells);
            memcpy(nl, c->leftcell, sizeof(int) * c->ncells);
            memcpy(nr, c->rightcell, sizeof(int) * c->ncells);
        }
        djvu_free(c->ctx, c->bitcells); djvu_free(c->ctx, c->leftcell);
        djvu_free(c->ctx, c->rightcell);
        c->bitcells = nb; c->leftcell = nl; c->rightcell = nr;
        c->cap_cells = nc;
    }
}

static int code_num(jb2_codec *c, int low, int high, int *ctxslot)
{
    int negative = 0, cutoff = 0, phase = 1, range = -1;
    int *ctx = ctxslot;

    ensure_cells(c, c->ncells + 80);
    if (c->error) return 0;

    while (range != 1) {
        int ictx = *ctx;
        int decision;
        if (ictx == 0) {
            ictx = c->ncells;
            *ctx = ictx;
            c->bitcells[ictx] = 0;
            c->leftcell[ictx] = 0;
            c->rightcell[ictx] = 0;
            c->ncells++;
        }
        decision = (low >= cutoff) ||
                   ((high >= cutoff) && code_bit_cell(c, ictx) != 0);
        ctx = decision ? &c->rightcell[ictx] : &c->leftcell[ictx];

        switch (phase) {
        case 1:
            negative = !decision;
            if (negative) { int t = -low - 1; low = -high - 1; high = t; }
            phase = 2; cutoff = 1;
            break;
        case 2:
            if (!decision) {
                phase = 3;
                range = (cutoff + 1) >> 1;
                if (range == 1) cutoff = 0;
                else cutoff -= (range >> 1);
            } else {
                cutoff = (cutoff << 1) + 1;
            }
            break;
        case 3:
            range /= 2;
            if (range != 1) {
                if (!decision) cutoff -= (range >> 1);
                else cutoff += (range >> 1);
            } else if (!decision) {
                cutoff--;
            }
            break;
        }
    }
    return negative ? (-cutoff - 1) : cutoff;
}

static inline int jb2_zp_decode_pixel(djvu_zp *DJVU_RESTRICT zp,
                                      uint32_t *DJVU_RESTRICT a,
                                      uint32_t *DJVU_RESTRICT fence,
                                      uint8_t *DJVU_RESTRICT ctx)
{
    uint32_t z = *a + zp->p[*ctx];
    if (DJVU_LIKELY(z <= *fence)) {
        *a = z;
        return *ctx & 1;
    }
    zp->a = *a;
    z = zp_decode_sub(zp, ctx, z);
    *a = zp->a;
    *fence = zp->fence;
    return z;
}

static void code_bitmap_directly(jb2_codec *c, djvu_bitmap *bm)
{
    int dw, dy, bpr, h;
    uint8_t *DJVU_RESTRICT row_base;
    uint8_t *DJVU_RESTRICT guard;
    uint8_t *DJVU_RESTRICT up2;
    uint8_t *DJVU_RESTRICT up1;
    uint8_t *DJVU_RESTRICT up0;
    djvu_zp *DJVU_RESTRICT zp;
    uint8_t *DJVU_RESTRICT bd;
    uint32_t a, fence;

    djvu_bm_set_min_border(c->ctx, bm, 3);
    dw = bm->width;
    h = bm->height;
    dy = h - 1;
    bpr = bm->bytes_per_row;
    row_base = bm->data + bm->border;
    guard = bm->guard + bm->border;
    zp = &c->zp;
    bd = c->bitdist;
    a = zp->a;
    fence = zp->fence;
    up2 = (dy + 2 < h) ? row_base + (dy + 2) * bpr : guard;
    up1 = (dy + 1 < h) ? row_base + (dy + 1) * bpr : guard;
    up0 = row_base + dy * bpr;
    while (dy >= 0) {
        int context = jb2_get_direct_context(up2, up1, up0, 0);
        int dx = 0;
        while (dx < dw) {
            if (context == 0 && (bd[0] & 1) == 0) {
                int run = dw - dx;
                const uint8_t *z1 = (const uint8_t *)memchr(up1 + dx + 2, 1, (size_t)run);
                const uint8_t *z2 = (const uint8_t *)memchr(up2 + dx + 1, 1, (size_t)run);
                if (z1) run = (int)(z1 - (up1 + dx + 2));
                if (z2 && (int)(z2 - (up2 + dx + 1)) < run)
                    run = (int)(z2 - (up2 + dx + 1));
                while (run > 0 && (bd[0] & 1) == 0) {
                    uint32_t p0 = zp->p[bd[0]];
                    if (p0 != 0 && a + p0 <= fence) {
                        uint32_t max_mps = (fence - a) / p0;
                        if (max_mps > (uint32_t)run)
                            max_mps = (uint32_t)run;
                        a += p0 * max_mps;
                        dx += (int)max_mps;
                        run -= (int)max_mps;
                        if (run == 0)
                            break;
                    }
                    {
                        int n = jb2_zp_decode_pixel(zp, &a, &fence, &bd[0]);
                        if (n) {
                            up0[dx++] = 1;
                            context = jb2_get_direct_context(up2, up1, up0, dx);
                            break;
                        }
                        dx++;
                        run--;
                    }
                }
                if (dx >= dw)
                    break;
                if (context == 0)
                    context = jb2_get_direct_context(up2, up1, up0, dx);
                if (context == 0)
                    continue;
            }
            int n = jb2_zp_decode_pixel(zp, &a, &fence, &bd[context]);
            up0[dx++] = (uint8_t)n;
            context = jb2_shift_direct_context(context, n, up2, up1, dx);
        }
        dy--;
        up2 = up1;
        up1 = up0;
        up0 = (dy >= 0) ? row_base + dy * bpr : guard;
    }
    zp->a = a;
}

static void code_bitmap_cross(jb2_codec *c, djvu_bitmap *bm, djvu_bitmap *cbm, int libno)
{
    int cw = cbm->width, dw = bm->width, dh = bm->height, ch = cbm->height;
    int xmin = c->libinfo[libno * 4 + 0];
    int xmax = c->libinfo[libno * 4 + 2];
    int ymin = c->libinfo[libno * 4 + 1];
    int ymax = c->libinfo[libno * 4 + 3];
    int xd2c = ((1 + (dw >> 1)) - dw) - ((((1 + xmax) - xmin) >> 1) - xmax);
    int yd2c = ((1 + (dh >> 1)) - dh) - ((((1 + ymax) - ymin) >> 1) - ymax);
    int dy, cy, bm_bpr, cbm_bpr;
    uint8_t *DJVU_RESTRICT bm_base;
    uint8_t *DJVU_RESTRICT cbm_base;
    uint8_t *DJVU_RESTRICT bm_guard;
    uint8_t *DJVU_RESTRICT cbm_guard;
    uint8_t *DJVU_RESTRICT up1;
    uint8_t *DJVU_RESTRICT up0;
    uint8_t *DJVU_RESTRICT xup1;
    uint8_t *DJVU_RESTRICT xup0;
    uint8_t *DJVU_RESTRICT xdn1;
    djvu_zp *DJVU_RESTRICT zp;
    uint8_t *DJVU_RESTRICT bd;
    uint32_t a, fence;

    djvu_bm_set_min_border(c->ctx, bm, 2);
    djvu_bm_set_min_border(c->ctx, cbm, 2 - xd2c);
    djvu_bm_set_min_border(c->ctx, cbm, (2 + dw + xd2c) - cw);

    bm_bpr = bm->bytes_per_row;
    cbm_bpr = cbm->bytes_per_row;
    bm_base = bm->data + bm->border;
    cbm_base = cbm->data + cbm->border;
    bm_guard = bm->guard + bm->border;
    cbm_guard = cbm->guard + cbm->border;
    zp = &c->zp;
    bd = c->cbitdist;
    a = zp->a;
    fence = zp->fence;
    dy = dh - 1;
    cy = dy + yd2c;
    up1 = (dy + 1 < dh) ? bm_base + (dy + 1) * bm_bpr : bm_guard;
    up0 = bm_base + dy * bm_bpr;

    xup1 = (cy + 1 >= 0 && cy + 1 < ch) ? cbm_base + (cy + 1) * cbm_bpr + xd2c : cbm_guard + xd2c;
    xup0 = (cy >= 0 && cy < ch) ? cbm_base + cy * cbm_bpr + xd2c : cbm_guard + xd2c;
    xdn1 = (cy - 1 >= 0 && cy - 1 < ch) ? cbm_base + (cy - 1) * cbm_bpr + xd2c : cbm_guard + xd2c;

    while (dy >= 0) {
        int context = jb2_get_cross_context(up1, up0, xup1, xup0, xdn1, 0);
        int dx = 0;
        while (dx < dw) {
            int n = jb2_zp_decode_pixel(zp, &a, &fence, &bd[context]);
            up0[dx++] = (uint8_t)n;
            context = jb2_shift_cross_context(context, n, up1, xup1, xup0, xdn1, dx);
        }
        dy--;
        up1 = up0;
        up0 = (dy >= 0) ? bm_base + dy * bm_bpr : bm_guard;
        cy--;
        xup1 = xup0;
        xup0 = xdn1;
        xdn1 = (cy - 1 >= 0 && cy - 1 < ch) ? cbm_base + (cy - 1) * cbm_bpr + xd2c : cbm_guard + xd2c;
    }
    zp->a = a;
}

static void shape2lib_set(jb2_codec *c, int shapeno, int libno)
{
    while (c->nshape2lib <= shapeno)
        iarr_push(c->ctx, &c->shape2lib, &c->nshape2lib, &c->cap_shape2lib, -1);
    c->shape2lib[shapeno] = libno;
}

static void shape_bbox(jb2_shape *jshp, int may_write,
                       int *xmin, int *ymin, int *xmax, int *ymax)
{
    if (!jshp->bbox_valid) {
        int x0, y0, x1, y1;
        djvu_bm_bbox(&jshp->bm, &x0, &y0, &x1, &y1);
        if (may_write) {
            jshp->bx0 = x0; jshp->by0 = y0; jshp->bx1 = x1; jshp->by1 = y1;
            jshp->bbox_valid = 1;
        }
        *xmin = x0; *ymin = y0; *xmax = x1; *ymax = y1;
        return;
    }
    *xmin = jshp->bx0; *ymin = jshp->by0; *xmax = jshp->bx1; *ymax = jshp->by1;
}

static int add_library(jb2_codec *c, int shapeno, jb2_shape *jshp)
{
    int libno = c->nlib2shape;
    int xmin, ymin, xmax, ymax;
    iarr_push(c->ctx, &c->lib2shape, &c->nlib2shape, &c->cap_lib2shape, shapeno);
    shape2lib_set(c, shapeno, libno);
    shape_bbox(jshp, 1, &xmin, &ymin, &xmax, &ymax);
    iarr_push(c->ctx, &c->libinfo, &c->nlibinfo, &c->cap_libinfo, xmin);
    iarr_push(c->ctx, &c->libinfo, &c->nlibinfo, &c->cap_libinfo, ymin);
    iarr_push(c->ctx, &c->libinfo, &c->nlibinfo, &c->cap_libinfo, xmax);
    iarr_push(c->ctx, &c->libinfo, &c->nlibinfo, &c->cap_libinfo, ymax);
    return libno;
}

static void init_library(jb2_codec *c, jb2_image *jim)
{
    int nshape = jim->inherited_shapes;
    int i;
    c->nshape2lib = c->nlib2shape = c->nlibinfo = 0;
    for (i = 0; i < nshape; i++) {
        jb2_shape *jshp = djvu_jb2_get_shape(jim, i);
        int xmin, ymin, xmax, ymax;
        iarr_push(c->ctx, &c->shape2lib, &c->nshape2lib, &c->cap_shape2lib, i);
        iarr_push(c->ctx, &c->lib2shape, &c->nlib2shape, &c->cap_lib2shape, i);

        shape_bbox(jshp, 0, &xmin, &ymin, &xmax, &ymax);
        iarr_push(c->ctx, &c->libinfo, &c->nlibinfo, &c->cap_libinfo, xmin);
        iarr_push(c->ctx, &c->libinfo, &c->nlibinfo, &c->cap_libinfo, ymin);
        iarr_push(c->ctx, &c->libinfo, &c->nlibinfo, &c->cap_libinfo, xmax);
        iarr_push(c->ctx, &c->libinfo, &c->nlibinfo, &c->cap_libinfo, ymax);
    }
}

static void reset_numcoder(jb2_codec *c)
{
    c->dist_comment_byte = 0; c->dist_comment_length = 0;
    c->dist_record_type = 0; c->dist_match_index = 0;
    c->abs_loc_x = 0; c->abs_loc_y = 0; c->abs_size_x = 0; c->abs_size_y = 0;
    c->image_size_dist = 0; c->inherited_shape_count_dist = 0;
    c->rel_loc_x_cur = 0; c->rel_loc_x_last = 0;
    c->rel_loc_y_cur = 0; c->rel_loc_y_last = 0;
    c->rel_size_x = 0; c->rel_size_y = 0;
    c->ncells = 1;
}

static void fill_shortlist(jb2_codec *c, int v)
{
    c->shortlist[0] = c->shortlist[1] = c->shortlist[2] = v;
    c->shortlistpos = 0;
}

static int update_shortlist(jb2_codec *c, int v)
{
    int *s = c->shortlist;
    if (++c->shortlistpos == 3) c->shortlistpos = 0;
    s[c->shortlistpos] = v;
    return (s[0] >= s[1])
        ? ((s[0] > s[2]) ? ((s[1] >= s[2]) ? s[1] : s[2]) : s[0])
        : ((s[0] < s[2]) ? ((s[1] >= s[2]) ? s[2] : s[1]) : s[0]);
}

static int code_record_type(jb2_codec *c)
{
    return code_num(c, REC_StartOfData, REC_EndOfData, &c->dist_record_type);
}

static int code_match_index(jb2_codec *c)
{
    int match = code_num(c, 0, c->nlib2shape - 1, &c->dist_match_index);

    if (match < 0 || match >= c->nlib2shape) { c->error = 1; return 0; }
    return match;
}

static int code_abs_mark_size(jb2_codec *c, djvu_bitmap *bm, int border)
{
    int xsize = code_num(c, 0, BIGPOSITIVE, &c->abs_size_x);
    int ysize = code_num(c, 0, BIGPOSITIVE, &c->abs_size_y);
    if (xsize != (0xffff & xsize) || ysize != (0xffff & ysize)) { c->error = 1; return -1; }
    return djvu_bm_init(c->ctx, bm, ysize, xsize, border);
}

static int code_rel_mark_size(jb2_codec *c, djvu_bitmap *bm, int cw, int ch, int border)
{
    int xdiff = code_num(c, BIGNEGATIVE, BIGPOSITIVE, &c->rel_size_x);
    int ydiff = code_num(c, BIGNEGATIVE, BIGPOSITIVE, &c->rel_size_y);
    int xsize = cw + xdiff, ysize = ch + ydiff;
    if (xsize != (0xffff & xsize) || ysize != (0xffff & ysize)) { c->error = 1; return -1; }
    return djvu_bm_init(c->ctx, bm, ysize, xsize, border);
}

static void code_image_size_image(jb2_codec *c, jb2_image *jim)
{
    c->image_columns = code_num(c, 0, BIGPOSITIVE, &c->image_size_dist);
    c->image_rows = code_num(c, 0, BIGPOSITIVE, &c->image_size_dist);
    if (c->image_columns == 0 || c->image_rows == 0) { c->error = 1; return; }
    jim->width = c->image_columns;
    jim->height = c->image_rows;

    c->last_left = 1 + c->image_columns;
    c->last_row_bottom = c->image_rows;
    c->last_row_left = c->last_right = 0;
    fill_shortlist(c, c->last_row_bottom);
    c->got_start_record = 1;
}

static void code_image_size_dict(jb2_codec *c)
{
    int w = code_num(c, 0, BIGPOSITIVE, &c->image_size_dist);
    int h = code_num(c, 0, BIGPOSITIVE, &c->image_size_dist);
    if (w != 0 || h != 0) { c->error = 1; return; }

    c->last_left = 1;
    c->last_row_bottom = 0;
    c->last_row_left = c->last_right = 0;
    fill_shortlist(c, c->last_row_bottom);
    c->got_start_record = 1;
}

static void code_eventual_refinement(jb2_codec *c)
{
    c->refinementp = djvu_zp_decode(&c->zp, &c->dist_refinement_flag);
}

static void code_inherited_shape_count(jb2_codec *c, jb2_image *jim)
{
    int size = code_num(c, 0, BIGPOSITIVE, &c->inherited_shape_count_dist);
    if (jim->inherited_dict == NULL && size > 0) {
        if (c->zdict) img_set_inherited_dict(jim, c->zdict);
        else { c->error = 1; return; }
    }
    if (jim->inherited_dict && size != img_shapecount(jim->inherited_dict))
        c->error = 1;
}

static void code_comment(jb2_codec *c)
{
    int size = code_num(c, 0, BIGPOSITIVE, &c->dist_comment_length);
    int i;
    for (i = 0; i < size; i++)
        (void)code_num(c, 0, 255, &c->dist_comment_byte);
}

static void code_abs_location(jb2_codec *c, jb2_blit *jblt, int rows)
{
    int left = code_num(c, 1, c->image_columns, &c->abs_loc_x);
    int top = code_num(c, 1, c->image_rows, &c->abs_loc_y);
    jblt->bottom = top - rows;
    jblt->left = left - 1;
}

static void code_rel_location(jb2_codec *c, jb2_blit *jblt, int rows, int columns)
{
    int bottom = 0, left = 0, top = 0, right = 0;
    int new_row = djvu_zp_decode(&c->zp, &c->offset_type_dist);
    if (new_row) {
        int x_diff = code_num(c, BIGNEGATIVE, BIGPOSITIVE, &c->rel_loc_x_last);
        int y_diff = code_num(c, BIGNEGATIVE, BIGPOSITIVE, &c->rel_loc_y_last);
        left = c->last_row_left + x_diff;
        top = c->last_row_bottom + y_diff;
        right = (left + columns) - 1;
        bottom = (top - rows) + 1;
        c->last_left = c->last_row_left = left;
        c->last_right = right;
        c->last_bottom = c->last_row_bottom = bottom;
        fill_shortlist(c, bottom);
    } else {
        int x_diff = code_num(c, BIGNEGATIVE, BIGPOSITIVE, &c->rel_loc_x_cur);
        int y_diff = code_num(c, BIGNEGATIVE, BIGPOSITIVE, &c->rel_loc_y_cur);
        left = c->last_right + x_diff;
        bottom = c->last_bottom + y_diff;
        right = (left + columns) - 1;
        top = (bottom + rows) - 1;
        (void)top;
        c->last_left = left;
        c->last_right = right;
        c->last_bottom = update_shortlist(c, bottom);
    }
    jblt->bottom = bottom - 1;
    jblt->left = left - 1;
}

static int code_record(jb2_codec *c, jb2_image *jim, int jim_is_image)
{
    int rectype = code_record_type(c);
    int shape_parent_init = -1;
    int have_shape = 0, have_blit = 0;
    int need_add_library = 0, need_add_blit = 0;
    jb2_shape tmp_shape;
    jb2_blit blit;
    int match = -1;
    int parent = -1;

    memset(&tmp_shape, 0, sizeof(tmp_shape));
    memset(&blit, 0, sizeof(blit));


    switch (rectype) {
    case REC_NewMark:
    case REC_NewMarkImageOnly:
    case REC_MatchedRefine:
    case REC_MatchedRefineImageOnly:
    case REC_NonMarkData:
        have_blit = 1;
    case REC_NewMarkLibraryOnly:
    case REC_MatchedRefineLibraryOnly:
        shape_parent_init = (rectype == REC_NonMarkData) ? -2 : -1;
        tmp_shape.parent = shape_parent_init;
        have_shape = 1;
        break;
    case REC_MatchedCopy:
        have_blit = 1;
        break;
    }

    switch (rectype) {
    case REC_StartOfData:
        if (jim_is_image) code_image_size_image(c, jim);
        else code_image_size_dict(c);
        code_eventual_refinement(c);
        init_library(c, jim);
        break;

    case REC_NewMark:
        need_add_blit = need_add_library = 1;
        code_abs_mark_size(c, &tmp_shape.bm, 4);
        code_bitmap_directly(c, &tmp_shape.bm);
        code_rel_location(c, &blit, tmp_shape.bm.height, tmp_shape.bm.width);
        break;

    case REC_NewMarkLibraryOnly:
        need_add_library = 1;
        code_abs_mark_size(c, &tmp_shape.bm, 4);
        code_bitmap_directly(c, &tmp_shape.bm);
        break;

    case REC_NewMarkImageOnly:
        need_add_blit = 1;
        code_abs_mark_size(c, &tmp_shape.bm, 3);
        code_bitmap_directly(c, &tmp_shape.bm);
        code_rel_location(c, &blit, tmp_shape.bm.height, tmp_shape.bm.width);
        break;

    case REC_MatchedRefine:
    case REC_MatchedRefineLibraryOnly:
    case REC_MatchedRefineImageOnly: {
        djvu_bitmap *cbm;
        djvu_bitmap cbm_copy;
        int cbm_owned = 0;
        int cw, ch;
        if (rectype == REC_MatchedRefine) { need_add_blit = need_add_library = 1; }
        else if (rectype == REC_MatchedRefineLibraryOnly) { need_add_library = 1; }
        else { need_add_blit = 1; }
        match = code_match_index(c);
        if (c->error) break;
        parent = c->lib2shape[match];
        tmp_shape.parent = parent;
        cbm = &djvu_jb2_get_shape(jim, parent)->bm;

        if (!cbm->data && cbm->rle) {
            if (djvu_bm_uncompress_copy(c->ctx, cbm, &cbm_copy) != 0) {
                c->error = 1;
                break;
            }
            cbm = &cbm_copy;
            cbm_owned = 1;
        }
        cw = (1 + c->libinfo[match * 4 + 2]) - c->libinfo[match * 4 + 0];
        ch = (1 + c->libinfo[match * 4 + 3]) - c->libinfo[match * 4 + 1];
        code_rel_mark_size(c, &tmp_shape.bm, cw, ch, 4);
        code_bitmap_cross(c, &tmp_shape.bm, cbm, match);
        if (cbm_owned) djvu_bm_free(c->ctx, &cbm_copy);
        if (rectype != REC_MatchedRefineLibraryOnly)
            code_rel_location(c, &blit, tmp_shape.bm.height, tmp_shape.bm.width);
        break;
    }

    case REC_MatchedCopy: {
        int xmin, ymin, xmax, ymax;
        match = code_match_index(c);
        if (c->error) break;
        blit.shapeno = c->lib2shape[match];
        xmin = c->libinfo[match * 4 + 0];
        ymin = c->libinfo[match * 4 + 1];
        xmax = c->libinfo[match * 4 + 2];
        ymax = c->libinfo[match * 4 + 3];
        blit.left += xmin;
        blit.bottom += ymin;
        code_rel_location(c, &blit, (1 + ymax) - ymin, (1 + xmax) - xmin);
        blit.left -= xmin;
        blit.bottom -= ymin;
        break;
    }

    case REC_NonMarkData:
        need_add_blit = 1;
        code_abs_mark_size(c, &tmp_shape.bm, 3);
        code_bitmap_directly(c, &tmp_shape.bm);
        code_abs_location(c, &blit, tmp_shape.bm.height);
        break;

    case REC_PreservedComment:
        code_comment(c);
        break;

    case REC_RequiredDictOrReset:
        if (!c->got_start_record) code_inherited_shape_count(c, jim);
        else reset_numcoder(c);
        break;

    case REC_EndOfData:
        break;

    default:
        c->error = 1;
        break;
    }

    if (c->error) { if (have_shape) djvu_bm_free(c->ctx, &tmp_shape.bm); return rectype; }


    if (have_shape && (rectype == REC_NewMark || rectype == REC_NewMarkLibraryOnly ||
                       rectype == REC_MatchedRefine || rectype == REC_MatchedRefineLibraryOnly ||
                       rectype == REC_NewMarkImageOnly || rectype == REC_MatchedRefineImageOnly ||
                       rectype == REC_NonMarkData)) {
        int shapeno = img_add_shape(jim, tmp_shape.parent);
        if (shapeno < 0) { c->error = 1; djvu_bm_free(c->ctx, &tmp_shape.bm); return rectype; }
        jim->shapes[jim->nshapes - 1].bm = tmp_shape.bm;
        shape2lib_set(c, shapeno, -1);
        if (need_add_library)
            add_library(c, shapeno, &jim->shapes[jim->nshapes - 1]);
        if (need_add_blit) {
            blit.shapeno = shapeno;
            img_add_blit(jim, blit);
        }
    } else if (rectype == REC_MatchedCopy) {
        img_add_blit(jim, blit);
    }
    (void)have_blit;

    return rectype;
}

static void codec_free(jb2_codec *c)
{
    if (!c) return;
    djvu_free(c->ctx, c->bitcells);
    djvu_free(c->ctx, c->leftcell);
    djvu_free(c->ctx, c->rightcell);
    djvu_free(c->ctx, c->lib2shape);
    djvu_free(c->ctx, c->shape2lib);
    djvu_free(c->ctx, c->libinfo);
}

static void compress_decoded_shapes(djvu_ctx *ctx, jb2_image *jim, int is_image)
{
    int si;

    if (!is_image) {
        for (si = 0; si < jim->nshapes; si++)
            djvu_bm_compress(ctx, &jim->shapes[si].bm);
        return;
    }

    if (jim->nshapes > 0 && jim->nblits > 0) {
        int *uses = (int *)djvu_alloc(ctx, sizeof(int) * (size_t)jim->nshapes);
        if (uses) {
            int bi;
            memset(uses, 0, sizeof(int) * (size_t)jim->nshapes);
            for (bi = 0; bi < jim->nblits; bi++) {
                int local = jim->blits[bi].shapeno - jim->inherited_shapes;
                if (local >= 0 && local < jim->nshapes)
                    uses[local]++;
            }
            for (si = 0; si < jim->nshapes; si++) {
                if (uses[si] != 1)
                    djvu_bm_compress(ctx, &jim->shapes[si].bm);
            }
            djvu_free(ctx, uses);
            return;
        }
    }

    for (si = 0; si < jim->nshapes; si++)
        djvu_bm_compress(ctx, &jim->shapes[si].bm);
}

static jb2_image *jb2_decode_into(djvu_ctx *ctx, const uint8_t *data, size_t len,
                                  jb2_image *dict, int is_image)
{
    jb2_codec *c;
    jb2_image *jim = jb2_image_new(ctx);
    int rectype;
    if (!jim) return NULL;

    c = (jb2_codec *)djvu_alloc(ctx, sizeof(jb2_codec));
    if (!c) { djvu_jb2_free(ctx, jim); return NULL; }

    memset(c, 0, sizeof(*c));
    c->ctx = ctx;
    c->zdict = dict;

    ensure_cells(c, 1);
    c->ncells = 1;

    djvu_zp_init(&c->zp, data, len);

    rectype = REC_StartOfData;
    {
        int hist[12]; int k; int dbg = getenv("DJVU_JB2_DEBUG") != NULL;
        for (k = 0; k < 12; k++) hist[k] = 0;
        do {
            rectype = code_record(c, jim, is_image);
            if (rectype >= 0 && rectype < 12) hist[rectype]++;
            if (c->error) break;

            if (c->zp.eof) break;
        } while (rectype != REC_EndOfData);
        if (dbg) {
            fprintf(stderr, "JB2 rectypes: SOD=%d NM=%d NMlib=%d NMimg=%d MR=%d "
                "MRlib=%d MRimg=%d MC=%d NMD=%d DICT=%d COM=%d EOD=%d shapes=%d blits=%d\n",
                hist[0],hist[1],hist[2],hist[3],hist[4],hist[5],hist[6],hist[7],
                hist[8],hist[9],hist[10],hist[11], jim->nshapes, jim->nblits);
        }
        {
            const char *shs = getenv("DJVU_JB2_SHAPE");
            if (shs) {
                int sn = atoi(shs);
                jb2_shape *sh = djvu_jb2_get_shape(jim, sn);
                if (sh && djvu_bm_has_pixels(&sh->bm)) {
                    int rr, cc, w = sh->bm.width, h = sh->bm.height;
                    djvu_bm_ensure_bytes(ctx, &sh->bm);
                    fprintf(stderr, "SHAPE %d %dx%d:\n", sn, w, h);
                    for (rr = h - 1; rr >= 0; rr--) {
                        int ro = djvu_bm_rowoffset(&sh->bm, rr);
                        for (cc = 0; cc < w; cc++)
                            fputc(sh->bm.data[ro + cc] ? '#' : '.', stderr);
                        fputc('\n', stderr);
                    }
                }
            }
        }
        if (getenv("DJVU_JB2_BLITS")) {
            int bi;
            for (bi = 0; bi < jim->nblits; bi++) {
                jb2_blit *b = &jim->blits[bi];
                jb2_shape *sh = djvu_jb2_get_shape(jim, b->shapeno);
                unsigned sum = 0; int rr, cc, w = 0, h = 0;
                if (sh && djvu_bm_has_pixels(&sh->bm)) {
                    w = sh->bm.width; h = sh->bm.height;
                    djvu_bm_ensure_bytes(ctx, &sh->bm);
                    for (rr = 0; rr < h; rr++) {
                        int ro = djvu_bm_rowoffset(&sh->bm, rr);
                        for (cc = 0; cc < w; cc++)
                            sum = sum * 31 + (sh->bm.data[ro + cc] ? 1 : 0);
                    }
                }
                fprintf(stderr, "BLIT %d left=%d bottom=%d shape=%d w=%d h=%d sum=%u\n",
                        bi, b->left, b->bottom, b->shapeno, w, h, sum);
            }
        }
    }

    if (c->error || !c->got_start_record) {
        djvu_errorf(ctx, DJVU_SEVERITY_ERROR, "JB2: decode failed");
        codec_free(c);
        djvu_free(ctx, c);
        djvu_jb2_free(ctx, jim);
        return NULL;
    }
    if (!is_image) {

        int si, x0, y0, x1, y1;
        for (si = 0; si < jim->nshapes; si++)
            shape_bbox(&jim->shapes[si], 1, &x0, &y0, &x1, &y1);
    }
    compress_decoded_shapes(ctx, jim, is_image);
    codec_free(c);
    djvu_free(ctx, c);
    return jim;
}

jb2_image *djvu_jb2_decode(djvu_ctx *ctx, const uint8_t *data, size_t len,
                           jb2_image *dict)
{

    return jb2_decode_into(ctx, data, len, dict, 1);
}

jb2_image *djvu_jb2_decode_dict(djvu_ctx *ctx, const uint8_t *data, size_t len)
{
    return jb2_decode_into(ctx, data, len, NULL, 0);
}


#include <stdint.h>
const int16_t djvu_iw44_zigzag[1024] = {
  0, 16, 512, 528, 8, 24, 520, 536, 256, 272, 768, 784, 264, 280, 776, 792,
  4, 20, 516, 532, 12, 28, 524, 540, 260, 276, 772, 788, 268, 284, 780, 796,
  128, 144, 640, 656, 136, 152, 648, 664, 384, 400, 896, 912, 392, 408, 904, 920,
  132, 148, 644, 660, 140, 156, 652, 668, 388, 404, 900, 916, 396, 412, 908, 924,
  2, 18, 514, 530, 10, 26, 522, 538, 258, 274, 770, 786, 266, 282, 778, 794,
  6, 22, 518, 534, 14, 30, 526, 542, 262, 278, 774, 790, 270, 286, 782, 798,
  130, 146, 642, 658, 138, 154, 650, 666, 386, 402, 898, 914, 394, 410, 906, 922,
  134, 150, 646, 662, 142, 158, 654, 670, 390, 406, 902, 918, 398, 414, 910, 926,
  64, 80, 576, 592, 72, 88, 584, 600, 320, 336, 832, 848, 328, 344, 840, 856,
  68, 84, 580, 596, 76, 92, 588, 604, 324, 340, 836, 852, 332, 348, 844, 860,
  192, 208, 704, 720, 200, 216, 712, 728, 448, 464, 960, 976, 456, 472, 968, 984,
  196, 212, 708, 724, 204, 220, 716, 732, 452, 468, 964, 980, 460, 476, 972, 988,
  66, 82, 578, 594, 74, 90, 586, 602, 322, 338, 834, 850, 330, 346, 842, 858,
  70, 86, 582, 598, 78, 94, 590, 606, 326, 342, 838, 854, 334, 350, 846, 862,
  194, 210, 706, 722, 202, 218, 714, 730, 450, 466, 962, 978, 458, 474, 970, 986,
  198, 214, 710, 726, 206, 222, 718, 734, 454, 470, 966, 982, 462, 478, 974, 990,
  1, 17, 513, 529, 9, 25, 521, 537, 257, 273, 769, 785, 265, 281, 777, 793,
  5, 21, 517, 533, 13, 29, 525, 541, 261, 277, 773, 789, 269, 285, 781, 797,
  129, 145, 641, 657, 137, 153, 649, 665, 385, 401, 897, 913, 393, 409, 905, 921,
  133, 149, 645, 661, 141, 157, 653, 669, 389, 405, 901, 917, 397, 413, 909, 925,
  3, 19, 515, 531, 11, 27, 523, 539, 259, 275, 771, 787, 267, 283, 779, 795,
  7, 23, 519, 535, 15, 31, 527, 543, 263, 279, 775, 791, 271, 287, 783, 799,
  131, 147, 643, 659, 139, 155, 651, 667, 387, 403, 899, 915, 395, 411, 907, 923,
  135, 151, 647, 663, 143, 159, 655, 671, 391, 407, 903, 919, 399, 415, 911, 927,
  65, 81, 577, 593, 73, 89, 585, 601, 321, 337, 833, 849, 329, 345, 841, 857,
  69, 85, 581, 597, 77, 93, 589, 605, 325, 341, 837, 853, 333, 349, 845, 861,
  193, 209, 705, 721, 201, 217, 713, 729, 449, 465, 961, 977, 457, 473, 969, 985,
  197, 213, 709, 725, 205, 221, 717, 733, 453, 469, 965, 981, 461, 477, 973, 989,
  67, 83, 579, 595, 75, 91, 587, 603, 323, 339, 835, 851, 331, 347, 843, 859,
  71, 87, 583, 599, 79, 95, 591, 607, 327, 343, 839, 855, 335, 351, 847, 863,
  195, 211, 707, 723, 203, 219, 715, 731, 451, 467, 963, 979, 459, 475, 971, 987,
  199, 215, 711, 727, 207, 223, 719, 735, 455, 471, 967, 983, 463, 479, 975, 991,
  32, 48, 544, 560, 40, 56, 552, 568, 288, 304, 800, 816, 296, 312, 808, 824,
  36, 52, 548, 564, 44, 60, 556, 572, 292, 308, 804, 820, 300, 316, 812, 828,
  160, 176, 672, 688, 168, 184, 680, 696, 416, 432, 928, 944, 424, 440, 936, 952,
  164, 180, 676, 692, 172, 188, 684, 700, 420, 436, 932, 948, 428, 444, 940, 956,
  34, 50, 546, 562, 42, 58, 554, 570, 290, 306, 802, 818, 298, 314, 810, 826,
  38, 54, 550, 566, 46, 62, 558, 574, 294, 310, 806, 822, 302, 318, 814, 830,
  162, 178, 674, 690, 170, 186, 682, 698, 418, 434, 930, 946, 426, 442, 938, 954,
  166, 182, 678, 694, 174, 190, 686, 702, 422, 438, 934, 950, 430, 446, 942, 958,
  96, 112, 608, 624, 104, 120, 616, 632, 352, 368, 864, 880, 360, 376, 872, 888,
  100, 116, 612, 628, 108, 124, 620, 636, 356, 372, 868, 884, 364, 380, 876, 892,
  224, 240, 736, 752, 232, 248, 744, 760, 480, 496, 992, 1008, 488, 504, 1000, 1016,
  228, 244, 740, 756, 236, 252, 748, 764, 484, 500, 996, 1012, 492, 508, 1004, 1020,
  98, 114, 610, 626, 106, 122, 618, 634, 354, 370, 866, 882, 362, 378, 874, 890,
  102, 118, 614, 630, 110, 126, 622, 638, 358, 374, 870, 886, 366, 382, 878, 894,
  226, 242, 738, 754, 234, 250, 746, 762, 482, 498, 994, 1010, 490, 506, 1002, 1018,
  230, 246, 742, 758, 238, 254, 750, 766, 486, 502, 998, 1014, 494, 510, 1006, 1022,
  33, 49, 545, 561, 41, 57, 553, 569, 289, 305, 801, 817, 297, 313, 809, 825,
  37, 53, 549, 565, 45, 61, 557, 573, 293, 309, 805, 821, 301, 317, 813, 829,
  161, 177, 673, 689, 169, 185, 681, 697, 417, 433, 929, 945, 425, 441, 937, 953,
  165, 181, 677, 693, 173, 189, 685, 701, 421, 437, 933, 949, 429, 445, 941, 957,
  35, 51, 547, 563, 43, 59, 555, 571, 291, 307, 803, 819, 299, 315, 811, 827,
  39, 55, 551, 567, 47, 63, 559, 575, 295, 311, 807, 823, 303, 319, 815, 831,
  163, 179, 675, 691, 171, 187, 683, 699, 419, 435, 931, 947, 427, 443, 939, 955,
  167, 183, 679, 695, 175, 191, 687, 703, 423, 439, 935, 951, 431, 447, 943, 959,
  97, 113, 609, 625, 105, 121, 617, 633, 353, 369, 865, 881, 361, 377, 873, 889,
  101, 117, 613, 629, 109, 125, 621, 637, 357, 373, 869, 885, 365, 381, 877, 893,
  225, 241, 737, 753, 233, 249, 745, 761, 481, 497, 993, 1009, 489, 505, 1001, 1017,
  229, 245, 741, 757, 237, 253, 749, 765, 485, 501, 997, 1013, 493, 509, 1005, 1021,
  99, 115, 611, 627, 107, 123, 619, 635, 355, 371, 867, 883, 363, 379, 875, 891,
  103, 119, 615, 631, 111, 127, 623, 639, 359, 375, 871, 887, 367, 383, 879, 895,
  227, 243, 739, 755, 235, 251, 747, 763, 483, 499, 995, 1011, 491, 507, 1003, 1019,
  231, 247, 743, 759, 239, 255, 751, 767, 487, 503, 999, 1015, 495, 511, 1007, 1023,
};

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern const int16_t djvu_iw44_zigzag[1024];

typedef struct {
    int16_t *buckets[64];
} iw_block;

typedef struct {
    int w, h, bw, bh, nb;
    iw_block *blocks;
} iw_map;

static const int band_start[10] = {0, 1, 2, 3, 4, 8, 12, 16, 32, 48};
static const int band_size[10]  = {1, 1, 1, 1, 4, 4, 4, 16, 16, 16};

static const int iwquant[16] = {
    0x10000, 0x20000, 0x20000, 0x40000, 0x40000, 0x40000, 0x80000,
    0x80000, 0x80000, 0x100000, 0x100000, 0x100000, 0x200000,
    0x100000, 0x100000, 0x200000
};

typedef struct {
    djvu_ctx *ctx;
    iw_map *map;
    int8_t coeff_state[256];
    int8_t bucket_state[16];
    uint8_t ctx_start[32];
    uint8_t ctx_bucket[10][8];
    uint8_t ctx_mant, ctx_root;
    int quant_high[10];
    int quant_low[16];
    int curband, curbit;
} iw_codec;

struct iw_pixmap {
    djvu_ctx *ctx;
    iw_map *ymap, *cbmap, *crmap;
    iw_codec *yc, *cbc, *crc;
    int cslices, cserial;
    int crcbdelay, crcbhalf;
    int w, h;
};

static iw_map *map_new(djvu_ctx *ctx, int w, int h)
{
    iw_map *m = (iw_map *)djvu_alloc(ctx, sizeof(iw_map));
    int i;
    if (!m) return NULL;
    memset(m, 0, sizeof(*m));
    m->w = w; m->h = h;
    m->bw = (w + 0x20 - 1) & ~0x1f;
    m->bh = (h + 0x20 - 1) & ~0x1f;
    m->nb = (m->bw * m->bh) / 1024;
    m->blocks = (iw_block *)djvu_alloc(ctx, sizeof(iw_block) * m->nb);
    if (!m->blocks) { djvu_free(ctx, m); return NULL; }
    for (i = 0; i < m->nb; i++)
        memset(&m->blocks[i], 0, sizeof(iw_block));
    return m;
}

static void map_free(djvu_ctx *ctx, iw_map *m)
{
    int i, b;
    if (!m) return;
    for (i = 0; i < m->nb; i++)
        for (b = 0; b < 64; b++)
            djvu_free(ctx, m->blocks[i].buckets[b]);
    djvu_free(ctx, m->blocks);
    djvu_free(ctx, m);
}

static int16_t *block_get(iw_block *blk, int n) { return blk->buckets[n]; }

static int16_t *block_get_init(djvu_ctx *ctx, iw_block *blk, int n)
{
    if (!blk->buckets[n]) {
        blk->buckets[n] = (int16_t *)djvu_alloc(ctx, sizeof(int16_t) * 16);
        if (blk->buckets[n]) memset(blk->buckets[n], 0, sizeof(int16_t) * 16);
    }
    return blk->buckets[n];
}

static void write_lift_block(iw_block *blk, int16_t *coeff)
{
    int n = 0, n1, n2;
    memset(coeff, 0, sizeof(int16_t) * 1024);
    for (n1 = 0; n1 < 64; n1++) {
        int16_t *d = block_get(blk, n1);
        if (d) {
            for (n2 = 0; n2 < 16; n2++, n++)
                coeff[djvu_iw44_zigzag[n]] = d[n2];
        } else {
            n += 16;
        }
    }
}

static void filter_bv(int16_t *p, int w, int h, int rowsize, int scale)
{
    int y = 0;
    int s = scale * rowsize;
    int s3 = s + s + s;
    h = ((h - 1) / scale) + 1;
    while (y - 3 < h) {

        {
            int16_t *q = p;
            int16_t *e = q + w;
            if (y >= 3 && y + 3 < h) {
                while (q < e) {
                    int a = (int)q[-s] + (int)q[s];
                    int b = (int)q[-s3] + (int)q[s3];
                    *q = (int16_t)(*q - (((a << 3) + a - b + 16) >> 5));
                    q += scale;
                }
            } else if (y < h) {
                int16_t *q1 = (y + 1 < h) ? q + s : NULL;
                int16_t *q3 = (y + 3 < h) ? q + s3 : NULL;
                if (y >= 3) {
                    while (q < e) {
                        int a = (int)q[-s] + (q1 ? (int)*q1 : 0);
                        int b = (int)q[-s3] + (q3 ? (int)*q3 : 0);
                        *q = (int16_t)(*q - (((a << 3) + a - b + 16) >> 5));
                        q += scale; if (q1) q1 += scale; if (q3) q3 += scale;
                    }
                } else if (y >= 1) {
                    while (q < e) {
                        int a = (int)q[-s] + (q1 ? (int)*q1 : 0);
                        int b = (q3 ? (int)*q3 : 0);
                        *q = (int16_t)(*q - (((a << 3) + a - b + 16) >> 5));
                        q += scale; if (q1) q1 += scale; if (q3) q3 += scale;
                    }
                } else {
                    while (q < e) {
                        int a = (q1 ? (int)*q1 : 0);
                        int b = (q3 ? (int)*q3 : 0);
                        *q = (int16_t)(*q - (((a << 3) + a - b + 16) >> 5));
                        q += scale; if (q1) q1 += scale; if (q3) q3 += scale;
                    }
                }
            }
        }

        {
            int16_t *q = p - s3;
            int16_t *e = q + w;
            if (y >= 6 && y < h) {
                while (q < e) {
                    int a = (int)q[-s] + (int)q[s];
                    int b = (int)q[-s3] + (int)q[s3];
                    *q = (int16_t)(*q + (((a << 3) + a - b + 8) >> 4));
                    q += scale;
                }
            } else if (y >= 3) {
                int16_t *q1 = (y - 2 < h) ? q + s : q - s;
                while (q < e) {
                    int a = (int)q[-s] + (int)*q1;
                    *q = (int16_t)(*q + ((a + 1) >> 1));
                    q += scale; q1 += scale;
                }
            }
        }
        y += 2;
        p += s + s;
    }
}

static void filter_bh(int16_t *p, int w, int h, int rowsize, int scale)
{
    int y = 0;
    int s = scale;
    int s3 = s + s + s;
    rowsize *= scale;
    while (y < h) {
        int16_t *q = p;
        int16_t *e = p + w;
        int a0 = 0, a1 = 0, a2 = 0, a3 = 0;
        int b0 = 0, b1 = 0, b2 = 0, b3 = 0;
        if (q < e) {
            if (q + s < e) a2 = q[s];
            if (q + s3 < e) a3 = q[s3];
            b2 = b3 = q[0] - ((((a1 + a2) << 3) + (a1 + a2) - a0 - a3 + 16) >> 5);
            q[0] = (int16_t)b3;
            q += s + s;
        }
        if (q < e) {
            a0 = a1; a1 = a2; a2 = a3;
            if (q + s3 < e) a3 = q[s3];
            b3 = q[0] - ((((a1 + a2) << 3) + (a1 + a2) - a0 - a3 + 16) >> 5);
            q[0] = (int16_t)b3;
            q += s + s;
        }
        if (q < e) {
            b1 = b2; b2 = b3; a0 = a1; a1 = a2; a2 = a3;
            if (q + s3 < e) a3 = q[s3];
            b3 = q[0] - ((((a1 + a2) << 3) + (a1 + a2) - a0 - a3 + 16) >> 5);
            q[0] = (int16_t)b3;
            q[-s3] = (int16_t)(q[-s3] + ((b1 + b2 + 1) >> 1));
            q += s + s;
        }
        while (q + s3 < e) {
            a0 = a1; a1 = a2; a2 = a3; a3 = q[s3];
            b0 = b1; b1 = b2; b2 = b3;
            b3 = q[0] - ((((a1 + a2) << 3) + (a1 + a2) - a0 - a3 + 16) >> 5);
            q[0] = (int16_t)b3;
            q[-s3] = (int16_t)(q[-s3] + ((((b1 + b2) << 3) + (b1 + b2) - b0 - b3 + 8) >> 4));
            q += s + s;
        }
        while (q < e) {
            a0 = a1; a1 = a2; a2 = a3; a3 = 0;
            b0 = b1; b1 = b2; b2 = b3;
            b3 = q[0] - ((((a1 + a2) << 3) + (a1 + a2) - a0 - a3 + 16) >> 5);
            q[0] = (int16_t)b3;
            q[-s3] = (int16_t)(q[-s3] + ((((b1 + b2) << 3) + (b1 + b2) - b0 - b3 + 8) >> 4));
            q += s + s;
        }
        while (q - s3 < e) {
            b0 = b1; b1 = b2; b2 = b3;
            if (q - s3 >= p)
                q[-s3] = (int16_t)(q[-s3] + ((b1 + b2 + 1) >> 1));
            q += s + s;
        }
        (void)b0;
        y += scale;
        p += rowsize;
    }
}

static void backward(int16_t *p, int w, int h, int rowsize, int begin, int end)
{
    int scale;
    for (scale = begin >> 1; scale >= end; scale >>= 1) {
        filter_bv(p, w, h, rowsize, scale);
        filter_bh(p, w, h, rowsize, scale);
    }
}

static int16_t *build_unified(djvu_ctx *ctx, iw_map *m)
{

    size_t n = (size_t)m->bw * m->bh + (size_t)m->bw * 4 + 16;
    int16_t *data16 = (int16_t *)djvu_alloc(ctx, sizeof(int16_t) * n);
    int16_t liftblock[1024];
    int blockidx = 0, i, j, ii, p1idx, ppidx, pidx = 0;
    if (!data16) return NULL;
    memset(data16, 0, sizeof(int16_t) * n);

    for (i = 0; i < m->bh; i += 32, pidx += 32 * m->bw) {
        for (j = 0; j < m->bw; j += 32) {
            write_lift_block(&m->blocks[blockidx], liftblock);
            blockidx++;
            ppidx = pidx + j;
            for (ii = 0, p1idx = 0; ii < 32; ii++, p1idx += 32, ppidx += m->bw)
                memcpy(data16 + ppidx, liftblock + p1idx, sizeof(int16_t) * 32);
        }
    }
    return data16;
}

static int map_image(djvu_ctx *ctx, iw_map *m, int index, int8_t *img8,
                     int rowsize, int pixsep, int fast)
{
    int16_t *data16 = build_unified(ctx, m);
    int i, j, pidx, rowidx, pixidx;
    if (!data16) return -1;

    if (fast) {
        backward(data16, m->w, m->h, m->bw, 32, 2);
        pidx = 0;
        for (i = 0; i < m->bh; i += 2, pidx += m->bw) {
            for (j = 0; j < m->bw; j += 2, pidx += 2)
                data16[pidx + m->bw] = data16[pidx + m->bw + 1] =
                    data16[pidx + 1] = data16[pidx];
        }
    } else {
        backward(data16, m->w, m->h, m->bw, 32, 1);
    }

    pidx = 0;
    for (i = 0, rowidx = index; i < m->h; i++, rowidx += rowsize, pidx += m->bw) {
        for (j = 0, pixidx = rowidx; j < m->w; j++, pixidx += pixsep) {
            int x = (data16[pidx + j] + 32) >> 6;
            if (x < -128) x = -128;
            else if (x > 127) x = 127;
            img8[pixidx] = (int8_t)x;
        }
    }
    djvu_free(ctx, data16);
    return 0;
}

static int next_quant(iw_codec *c)
{
    int flag = 0, i;
    for (i = 0; i < 16; i++)
        if ((c->quant_low[i] = c->quant_low[i] >> 1) != 0) flag = 1;
    for (i = 0; i < 10; i++)
        if ((c->quant_high[i] = c->quant_high[i] >> 1) != 0) flag = 1;
    return flag;
}

static void codec_init(iw_codec *c, djvu_ctx *ctx, iw_map *map)
{
    int i, j, qidx;
    memset(c, 0, sizeof(*c));
    c->ctx = ctx;
    c->map = map;
    c->curband = 0;
    c->curbit = 1;

    i = 0; qidx = 0;
    for (j = 0; i < 4; j++) c->quant_low[i++] = iwquant[qidx++];
    for (j = 0; j < 4; j++) c->quant_low[i++] = iwquant[qidx];
    qidx++;
    for (j = 0; j < 4; j++) c->quant_low[i++] = iwquant[qidx];
    qidx++;
    for (j = 0; j < 4; j++) c->quant_low[i++] = iwquant[qidx];
    qidx++;
    c->quant_high[0] = 0;
    for (j = 1; j < 10; j++) c->quant_high[j] = iwquant[qidx++];
    while (c->quant_low[0] >= 32768) next_quant(c);
}

static int is_null_slice(iw_codec *c, int bit, int band)
{
    int i, thr;
    (void)bit;
    if (band == 0) {
        int is_null = 1;
        for (i = 0; i < 16; i++) {
            int threshold = c->quant_low[i];
            c->coeff_state[i] = 1;
            if (threshold > 0 && threshold < 32768) {
                is_null = 0;
                c->coeff_state[i] = 0;
            }
        }
        return is_null;
    }
    thr = c->quant_high[band];
    if (thr <= 0 || thr >= 32768) return 1;
    for (i = 0; i < (band_size[band] << 4); i++)
        c->coeff_state[i] = 0;
    return 0;
}

static void decode_buckets(iw_codec *c, djvu_zp *zp, int bit, int band,
                           iw_block *blk, int fbucket, int nbucket)
{
    int thres = c->quant_high[band];
    int bbstate = 0;
    int8_t *cstate = c->coeff_state;
    int cidx = 0, buckno, i;

    (void)bit;
    for (buckno = 0; buckno < nbucket; buckno++, cidx += 16) {
        int bstatetmp = 0;
        int16_t *pcoeff = block_get(blk, fbucket + buckno);
        if (pcoeff == NULL) {
            bstatetmp = 8;
        } else {
            for (i = 0; i < 16; i++) {
                int cstatetmp = cstate[cidx + i] & 1;
                if (cstatetmp == 0)
                    cstatetmp |= (pcoeff[i] != 0) ? 2 : 8;
                cstate[cidx + i] = (int8_t)cstatetmp;
                bstatetmp |= cstatetmp;
            }
        }
        c->bucket_state[buckno] = (int8_t)bstatetmp;
        bbstate |= bstatetmp;
    }

    if (nbucket < 16 || (bbstate & 2) != 0) {
        bbstate |= 4;
    } else if ((bbstate & 8) != 0) {
        if (djvu_zp_decode(zp, &c->ctx_root) != 0)
            bbstate |= 4;
    }

    if ((bbstate & 4) != 0) {
        for (buckno = 0; buckno < nbucket; buckno++) {
            if ((c->bucket_state[buckno] & 8) != 0) {
                int ctx = 0;
                if (band > 0) {
                    int k = (fbucket + buckno) << 2;
                    int16_t *b = block_get(blk, k >> 4);
                    if (b != NULL) {
                        k &= 0xf;
                        if (b[k] != 0) ctx++;
                        if (b[k + 1] != 0) ctx++;
                        if (b[k + 2] != 0) ctx++;
                        if (ctx < 3 && b[k + 3] != 0) ctx++;
                    }
                }
                if ((bbstate & 2) != 0) ctx |= 4;
                if (djvu_zp_decode(zp, &c->ctx_bucket[band][ctx]) != 0)
                    c->bucket_state[buckno] |= 4;
            }
        }
    }

    if ((bbstate & 4) != 0) {
        cstate = c->coeff_state; cidx = 0;
        for (buckno = 0; buckno < nbucket; buckno++, cidx += 16) {
            if ((c->bucket_state[buckno] & 4) != 0) {
                int16_t *pcoeff = block_get(blk, fbucket + buckno);
                int gotcha = 0, maxgotcha = 7;
                if (pcoeff == NULL) {
                    pcoeff = block_get_init(c->ctx, blk, fbucket + buckno);
                    for (i = 0; i < 16; i++)
                        if ((cstate[cidx + i] & 1) == 0) cstate[cidx + i] = 8;
                }
                for (i = 0; i < 16; i++)
                    if ((cstate[cidx + i] & 8) != 0) gotcha++;
                for (i = 0; i < 16; i++) {
                    if ((cstate[cidx + i] & 8) != 0) {
                        int ctx, coeff, halfthres;
                        if (band == 0) thres = c->quant_low[i];
                        ctx = (gotcha >= maxgotcha) ? maxgotcha : gotcha;
                        if ((c->bucket_state[buckno] & 2) != 0) ctx |= 8;
                        if (djvu_zp_decode(zp, &c->ctx_start[ctx]) != 0) {
                            cstate[cidx + i] |= 4;
                            halfthres = thres >> 1;
                            coeff = (thres + halfthres) - (halfthres >> 2);
                            if (djvu_zp_decode_iw(zp) != 0)
                                pcoeff[i] = (int16_t)(-coeff);
                            else
                                pcoeff[i] = (int16_t)coeff;
                        }
                        if ((cstate[cidx + i] & 4) != 0) gotcha = 0;
                        else if (gotcha > 0) gotcha--;
                    }
                }
            }
        }
    }

    if ((bbstate & 2) != 0) {
        cstate = c->coeff_state; cidx = 0;
        for (buckno = 0; buckno < nbucket; buckno++, cidx += 16) {
            if ((c->bucket_state[buckno] & 2) != 0) {
                int16_t *pcoeff = block_get(blk, fbucket + buckno);
                for (i = 0; i < 16; i++) {
                    if ((cstate[cidx + i] & 2) != 0) {
                        int coeff = pcoeff[i];
                        if (coeff < 0) coeff = -coeff;
                        if (band == 0) thres = c->quant_low[i];
                        if (coeff <= (3 * thres)) {
                            coeff += (thres >> 2);
                            if (djvu_zp_decode(zp, &c->ctx_mant) != 0)
                                coeff += (thres >> 1);
                            else
                                coeff = (coeff - thres) + (thres >> 1);
                        } else {
                            if (djvu_zp_decode_iw(zp) != 0)
                                coeff += (thres >> 1);
                            else
                                coeff = (coeff - thres) + (thres >> 1);
                        }
                        if (pcoeff[i] > 0) pcoeff[i] = (int16_t)coeff;
                        else pcoeff[i] = (int16_t)(-coeff);
                    }
                }
            }
        }
    }
}

static int code_slice(iw_codec *c, djvu_zp *zp)
{
    if (c->curbit < 0) return 0;
    if (!is_null_slice(c, c->curbit, c->curband)) {
        int blockno;
        int fbucket = band_start[c->curband];
        int nbucket = band_size[c->curband];
        for (blockno = 0; blockno < c->map->nb; blockno++)
            decode_buckets(c, zp, c->curbit, c->curband,
                           &c->map->blocks[blockno], fbucket, nbucket);
    }
    if (++c->curband >= 10) {
        c->curband = 0;
        c->curbit++;
        if (next_quant(c) == 0) {
            c->curbit = -1;
            return 0;
        }
    }
    return 1;
}

iw_pixmap *djvu_iw44_new(djvu_ctx *ctx)
{
    iw_pixmap *pm = (iw_pixmap *)djvu_alloc(ctx, sizeof(iw_pixmap));
    if (!pm) return NULL;
    memset(pm, 0, sizeof(*pm));
    pm->ctx = ctx;
    pm->crcbdelay = 10;
    return pm;
}

void djvu_iw44_free(iw_pixmap *pm)
{
    djvu_ctx *ctx;
    if (!pm) return;
    ctx = pm->ctx;
    map_free(ctx, pm->ymap);
    map_free(ctx, pm->cbmap);
    map_free(ctx, pm->crmap);
    djvu_free(ctx, pm->yc);
    djvu_free(ctx, pm->cbc);
    djvu_free(ctx, pm->crc);
    djvu_free(ctx, pm);
}

int djvu_iw44_decode_chunk(iw_pixmap *pm, const uint8_t *data, size_t len)
{
    djvu_ctx *ctx = pm->ctx;
    djvu_zp zp;
    size_t pos = 0;
    int serial, slices, nslices, flag;

    if (len < 2) return -1;
    serial = data[pos++];
    slices = data[pos++];
    if (serial != pm->cserial) {
        djvu_errorf(ctx, DJVU_SEVERITY_ERROR, "IW44: out-of-order chunk");
        return -1;
    }
    nslices = pm->cslices + slices;

    if (pm->cserial == 0) {
        int major, minor, w, h, crcbdelay = 0;
        if (pos + 6 > len) return -1;
        major = data[pos++];
        minor = data[pos++];
        if ((major & 0x7f) != 1) {
            djvu_errorf(ctx, DJVU_SEVERITY_ERROR, "IW44: incompatible codec");
            return -1;
        }
        w = (data[pos] << 8) | data[pos + 1]; pos += 2;
        h = (data[pos] << 8) | data[pos + 1]; pos += 2;
        if ((major & 0x7f) == 1 && minor >= 2) {
            if (pos >= len) return -1;
            crcbdelay = data[pos++];
            pm->crcbdelay = crcbdelay & 0x7f;
        }
        if (minor >= 2)
            pm->crcbhalf = (crcbdelay & 0x80) ? 0 : 1;
        if (major & 0x80)
            pm->crcbdelay = -1;

        pm->w = w; pm->h = h;
        pm->ymap = map_new(ctx, w, h);
        pm->yc = (iw_codec *)djvu_alloc(ctx, sizeof(iw_codec));
        if (!pm->ymap || !pm->yc) return -1;
        codec_init(pm->yc, ctx, pm->ymap);
        if (pm->crcbdelay >= 0) {
            pm->cbmap = map_new(ctx, w, h);
            pm->crmap = map_new(ctx, w, h);
            pm->cbc = (iw_codec *)djvu_alloc(ctx, sizeof(iw_codec));
            pm->crc = (iw_codec *)djvu_alloc(ctx, sizeof(iw_codec));
            if (!pm->cbmap || !pm->crmap || !pm->cbc || !pm->crc) return -1;
            codec_init(pm->cbc, ctx, pm->cbmap);
            codec_init(pm->crc, ctx, pm->crmap);
        }
    }

    if (getenv("DJVU_IW_DEBUG"))
        fprintf(stderr, "IW44 chunk: serial=%d slices=%d nslices=%d w=%d h=%d "
                "crcbdelay=%d crcbhalf=%d color=%d\n", serial, slices, nslices,
                pm->w, pm->h, pm->crcbdelay, pm->crcbhalf, djvu_iw44_is_color(pm));

    djvu_zp_init(&zp, data + pos, len - pos);

    for (flag = 1; flag != 0 && pm->cslices < nslices; pm->cslices++) {
        flag = code_slice(pm->yc, &zp);
        if (pm->crc && pm->cbc && pm->crcbdelay <= pm->cslices) {
            flag |= code_slice(pm->cbc, &zp);
            flag |= code_slice(pm->crc, &zp);
        }
    }
    pm->cserial++;
    return 0;
}

int djvu_iw44_decode_form(djvu_doc *doc, uint32_t form_off, const char *chunk_id,
                          iw_pixmap *pm, int max_chunks)
{
    uint32_t start = 0, sz;
    const uint8_t *chunk;
    int n = 0;

    while ((chunk = djvu_form_find_chunk(doc, form_off, chunk_id, &sz, &start)) != NULL) {
        if (max_chunks > 0 && n >= max_chunks) break;
        if (djvu_iw44_decode_chunk(pm, chunk, sz) != 0) return -1;
        n++;
    }
    return n > 0 ? 0 : -1;
}

int djvu_iw44_width(iw_pixmap *pm) { return pm ? pm->w : 0; }
int djvu_iw44_height(iw_pixmap *pm) { return pm ? pm->h : 0; }
int djvu_iw44_is_color(iw_pixmap *pm)
{
    return pm && pm->crmap && pm->cbmap && pm->crcbdelay >= 0;
}

static int clamp255(int v) { return v < 0 ? 0 : (v > 255 ? 255 : v); }

static int iw44_render_rgb_impl(iw_pixmap *pm, uint8_t *rgb, int flip)
{
    djvu_ctx *ctx;
    int w, h, i, color;
    int8_t *bytes;
    if (!pm || !pm->ymap) return -1;
    ctx = pm->ctx;
    w = pm->w; h = pm->h;
    color = djvu_iw44_is_color(pm);

    bytes = (int8_t *)djvu_alloc(ctx, (size_t)w * h * 3);
    if (!bytes) return -1;
    memset(bytes, 0, (size_t)w * h * 3);


    if (map_image(ctx, pm->ymap, 0, bytes, w * 3, 3, 0) != 0) { djvu_free(ctx, bytes); return -1; }
    if (color) {
        map_image(ctx, pm->cbmap, 1, bytes, w * 3, 3, pm->crcbhalf);
        map_image(ctx, pm->crmap, 2, bytes, w * 3, 3, pm->crcbhalf);
        for (i = 0; i < w * h; i++) {
            int8_t *q = bytes + (size_t)i * 3;
            int yv = q[0], bv = q[1], rv = q[2];
            int t1 = bv >> 2;
            int t2 = rv + (rv >> 1);
            int t3 = yv + 128 - t1;
            int tr = yv + 128 + t2;
            int tg = t3 - (t2 >> 1);
            int tb = t3 + (bv << 1);
            size_t o = (size_t)(flip ? (h - 1 - i / w) * w + (i % w) : i) * 3;
            rgb[o + 0] = (uint8_t)clamp255(tr);
            rgb[o + 1] = (uint8_t)clamp255(tg);
            rgb[o + 2] = (uint8_t)clamp255(tb);
        }
    } else {
        for (i = 0; i < w * h; i++) {
            int g = clamp255(127 - bytes[(size_t)i * 3]);
            size_t o = (size_t)(flip ? (h - 1 - i / w) * w + (i % w) : i) * 3;
            rgb[o + 0] = rgb[o + 1] = rgb[o + 2] = (uint8_t)g;
        }
    }
    djvu_free(ctx, bytes);
    return 0;
}

int djvu_iw44_render_rgb(iw_pixmap *pm, uint8_t *rgb)
{
    return iw44_render_rgb_impl(pm, rgb, 1);
}
int djvu_iw44_render_rgb_raw(iw_pixmap *pm, uint8_t *rgb)
{
    return iw44_render_rgb_impl(pm, rgb, 0);
}

int djvu_iw44_render_plane(iw_pixmap *pm, int plane, uint8_t *gray)
{
    djvu_ctx *ctx; int w, h, i; int8_t *bytes; iw_map *m; int fast;
    if (!pm) return -1;
    ctx = pm->ctx; w = pm->w; h = pm->h;
    m = plane == 1 ? pm->cbmap : plane == 2 ? pm->crmap : pm->ymap;
    fast = plane == 0 ? 0 : pm->crcbhalf;
    if (!m) return -1;
    bytes = (int8_t *)djvu_alloc(ctx, (size_t)w * h);
    if (!bytes) return -1;
    memset(bytes, 0, (size_t)w * h);
    if (map_image(ctx, m, 0, bytes, w, 1, fast) != 0) { djvu_free(ctx, bytes); return -1; }
    for (i = 0; i < w * h; i++) gray[i] = (uint8_t)clamp255(bytes[i] + 128);
    djvu_free(ctx, bytes);
    return 0;
}

int djvu_iw44_render_gray(iw_pixmap *pm, uint8_t *gray)
{
    djvu_ctx *ctx;
    int w, h, i;
    int8_t *bytes;
    if (!pm || !pm->ymap) return -1;
    ctx = pm->ctx;
    w = pm->w; h = pm->h;
    bytes = (int8_t *)djvu_alloc(ctx, (size_t)w * h);
    if (!bytes) return -1;
    memset(bytes, 0, (size_t)w * h);
    if (map_image(ctx, pm->ymap, 0, bytes, w, 1, 0) != 0) { djvu_free(ctx, bytes); return -1; }
    for (i = 0; i < w * h; i++)
        gray[(h - 1 - i / w) * w + (i % w)] = (uint8_t)clamp255(bytes[i] + 128);
    djvu_free(ctx, bytes);
    return 0;
}

#include <string.h>

#define FRACBITS 4
#define FRACSIZE (1 << FRACBITS)
#define FRACSIZE2 (FRACSIZE >> 1)
#define FRACMASK (FRACSIZE - 1)

static short s_interp[FRACSIZE][512];
static int s_interp_ready = 0;

static void prepare_interp(void)
{
    int i, j;
    if (s_interp_ready) return;
    for (i = 0; i < FRACSIZE; i++) {
        short *d = &s_interp[i][256];
        for (j = -256; j < 256; j++)
            d[j] = (short)((j * i + FRACSIZE2) >> FRACBITS);
    }
    s_interp_ready = 1;
}

void djvu_init(void)
{
    prepare_interp();
}

void djvu_scaler_init(void)
{
    djvu_init();
}

static int imini(int a, int b) { return a < b ? a : b; }

static void prepare_coord(int *coord, int inmax, int outmax, int in, int out)
{
    int len = in * FRACSIZE;
    int beg = (len + out) / (2 * out) - FRACSIZE2;
    int y = beg, z = out / 2;
    int inmaxlim = (inmax - 1) * FRACSIZE;
    int x;
    for (x = 0; x < outmax; x++) {
        coord[x] = imini(y, inmaxlim);
        z = z + len;
        y = y + z / out;
        z = z % out;
    }
}

typedef struct {
    djvu_ctx *ctx;
    int inw, inh, outw, outh;
    int xshift, yshift, redw, redh;
    int hnum, hden, vnum, vden;
    int *hcoord, *vcoord;
} scaler;

static void scaler_set_h(scaler *s, int numer, int denom)
{
    if (numer == 0 && denom == 0) { numer = s->outw; denom = s->inw; }
    s->hnum = numer; s->hden = denom;
    s->xshift = 0; s->redw = s->inw;
    while (numer + numer < denom) { s->xshift++; s->redw = (s->redw + 1) >> 1; numer <<= 1; }
    s->hcoord = (int *)djvu_alloc(s->ctx, sizeof(int) * s->outw);
    prepare_coord(s->hcoord, s->redw, s->outw, denom, numer);
}
static void scaler_set_v(scaler *s, int numer, int denom)
{
    if (numer == 0 && denom == 0) { numer = s->outh; denom = s->inh; }
    s->vnum = numer; s->vden = denom;
    s->yshift = 0; s->redh = s->inh;
    while (numer + numer < denom) { s->yshift++; s->redh = (s->redh + 1) >> 1; numer <<= 1; }
    s->vcoord = (int *)djvu_alloc(s->ctx, sizeof(int) * s->outh);
    prepare_coord(s->vcoord, s->redh, s->outh, denom, numer);
}

static void scaler_get_line(scaler *s, int fy, const djvu_cpix *in, int in_x0, int in_y0,
                            int red_xmin, int red_xmax, uint8_t *out)
{
    int sw = 1 << s->xshift;
    int div = s->xshift + s->yshift;
    int rnd = div ? (1 << (div - 1)) : 0;
    int x, idx = 0;
    int ly0 = (fy << s->yshift);
    int ly1 = ((fy + 1) << s->yshift);
    int xmin = red_xmin << s->xshift, xmax = red_xmax << s->xshift;
    int xend = in_x0 + in->w, yend = in_y0 + in->h;
    if (xmax > xend) xmax = xend;
    if (ly0 < in_y0) ly0 = in_y0;
    if (ly1 > yend) ly1 = yend;
    for (x = xmin; x < xmax; x += sw) {
        int r = 0, g = 0, b = 0, ss = 0, sy, sx;
        for (sy = ly0; sy < ly1; sy++) {
            int rowy = sy - in_y0;
            if (rowy < 0 || rowy >= in->h)
                continue;
            for (sx = x; sx < x + sw && sx < xmax; sx++) {
                int px = sx - in_x0;
                if (px < 0 || px >= in->w)
                    continue;
                const uint8_t *p = in->d + ((size_t)rowy * in->w + px) * 3;
                r += p[0]; g += p[1]; b += p[2]; ss++;
            }
        }
        if (ss == rnd + rnd && div) {
            out[idx*3+0]=(uint8_t)((r+rnd)>>div); out[idx*3+1]=(uint8_t)((g+rnd)>>div); out[idx*3+2]=(uint8_t)((b+rnd)>>div);
        } else if (ss) {
            out[idx*3+0]=(uint8_t)((r+ss/2)/ss); out[idx*3+1]=(uint8_t)((g+ss/2)/ss); out[idx*3+2]=(uint8_t)((b+ss/2)/ss);
        }
        idx++;
    }
    {
        int nout = red_xmax - red_xmin;
        while (idx < nout) {
            if (idx > 0) {
                out[idx * 3 + 0] = out[(idx - 1) * 3 + 0];
                out[idx * 3 + 1] = out[(idx - 1) * 3 + 1];
                out[idx * 3 + 2] = out[(idx - 1) * 3 + 2];
            } else {
                out[0] = out[1] = out[2] = 0;
            }
            idx++;
        }
    }
}

static int cpix_init_uninit(djvu_ctx *ctx, djvu_cpix *p, int w, int h)
{
    djvu_free(ctx, p->d);
    p->w = w; p->h = h;
    p->d = (uint8_t *)djvu_alloc(ctx, (size_t)w * h * 3);
    return p->d ? 0 : -1;
}

static uint8_t *scaler_dest_row(uint8_t *dst0, int stride, int outh,
                                int topdown, int y)
{
    return dst0 + (size_t)(topdown ? (outh - 1 - y) : y) * stride;
}

static void scaler_expand_row3(const uint8_t *src, int w, int outw, uint8_t *dst)
{
    const uint8_t *last;
    uint8_t *d = dst;
    int x, ntail = outw - (3 * w - 2);

    d[0] = src[0];
    d[1] = src[1];
    d[2] = src[2];
    d += 3;

    for (x = 0; x < w - 1; x++) {
        const uint8_t *a = src + (size_t)x * 3;
        const uint8_t *b = a + 3;
        int ar = a[0], ag = a[1], ab = a[2];
        int dr = b[0] - ar, dg = b[1] - ag, db = b[2] - ab;

        d[0] = (uint8_t)ar;
        d[1] = (uint8_t)ag;
        d[2] = (uint8_t)ab;
        d[3] = (uint8_t)(ar + ((dr * 6 + FRACSIZE2) >> FRACBITS));
        d[4] = (uint8_t)(ag + ((dg * 6 + FRACSIZE2) >> FRACBITS));
        d[5] = (uint8_t)(ab + ((db * 6 + FRACSIZE2) >> FRACBITS));
        d[6] = (uint8_t)(ar + ((dr * 11 + FRACSIZE2) >> FRACBITS));
        d[7] = (uint8_t)(ag + ((dg * 11 + FRACSIZE2) >> FRACBITS));
        d[8] = (uint8_t)(ab + ((db * 11 + FRACSIZE2) >> FRACBITS));
        d += 9;
    }

    last = src + (size_t)(w - 1) * 3;
    for (x = 0; x < ntail; x++) {
        d[0] = last[0];
        d[1] = last[1];
        d[2] = last[2];
        d += 3;
    }
}

static void scaler_interp_row(const uint8_t *lower, const uint8_t *upper,
                              int w, int vf, uint8_t *dst)
{
    int i, n = w * 3;

    for (i = 0; i < n; i++) {
        int lo = lower[i];
        dst[i] = (uint8_t)(lo + (((upper[i] - lo) * vf + FRACSIZE2) >> FRACBITS));
    }
}

static int scaler_scale_red3_into(scaler *s, const djvu_cpix *in,
                                  uint8_t *dst0, int stride, int topdown)
{
    djvu_ctx *ctx = s->ctx;
    int w = s->inw, h = s->inh, outw = s->outw;
    size_t row = (size_t)w * 3;
    uint8_t *tmp;
    const uint8_t *last;
    int y, vtail = s->outh - (3 * h - 2);

    tmp = (uint8_t *)djvu_alloc(ctx, row);
    if (!tmp) return -1;

    scaler_expand_row3(in->d, w, outw,
                       scaler_dest_row(dst0, stride, s->outh, topdown, 0));
    for (y = 0; y < h - 1; y++) {
        const uint8_t *lower = in->d + (size_t)y * row;
        const uint8_t *upper = lower + row;

        scaler_expand_row3(lower, w, outw,
                           scaler_dest_row(dst0, stride, s->outh, topdown,
                                           y * 3 + 1));
        scaler_interp_row(lower, upper, w, 6, tmp);
        scaler_expand_row3(tmp, w, outw,
                           scaler_dest_row(dst0, stride, s->outh, topdown,
                                           y * 3 + 2));
        scaler_interp_row(lower, upper, w, 11, tmp);
        scaler_expand_row3(tmp, w, outw,
                           scaler_dest_row(dst0, stride, s->outh, topdown,
                                           y * 3 + 3));
    }

    last = in->d + (size_t)(h - 1) * row;
    for (y = 0; y < vtail; y++)
        scaler_expand_row3(last, w, outw,
                           scaler_dest_row(dst0, stride, s->outh, topdown,
                                           h * 3 - 2 + y));
    djvu_free(ctx, tmp);
    return 0;
}

static int scaler_scale_into(scaler *s, const djvu_cpix *in,
                             uint8_t *dst0, int stride, int topdown)
{
    djvu_ctx *ctx = s->ctx;
    int bufw, y;
    uint8_t *lbuf;

    if (!in || !in->d || in->w <= 0 || in->h <= 0 ||
        in->w != s->inw || in->h != s->inh)
        return -1;
    uint8_t *p1 = NULL, *p2 = NULL; int l1 = -1, l2 = -1;
    int red_xmin = 0, red_xmax = s->redw;
    uint16_t *hinfo16 = NULL;
    uint32_t *hinfo32 = NULL;

    prepare_interp();
    if (!s->hcoord) scaler_set_h(s, 0, 0);
    if (!s->vcoord) scaler_set_v(s, 0, 0);
    if (s->xshift == 0 && s->yshift == 0 &&
        s->hnum == 3 && s->hden == 1 && s->vnum == 3 && s->vden == 1 &&
        s->inw > 0 && s->inh > 0 &&
        s->redw == s->inw && s->redh == s->inh &&
        s->outw >= 3 * s->inw - 2 && s->outw <= 3 * s->inw &&
        s->outh >= 3 * s->inh - 2 && s->outh <= 3 * s->inh &&
        in->w == s->inw && in->h == s->inh)
        return scaler_scale_red3_into(s, in, dst0, stride, topdown);
    bufw = s->redw;
    lbuf = (uint8_t *)djvu_alloc(ctx, (size_t)(bufw + 2) * 3);
    if (!lbuf) return -1;
    if (s->xshift > 0 || s->yshift > 0) {
        p1 = (uint8_t *)djvu_alloc(ctx, (size_t)bufw * 3);
        p2 = (uint8_t *)djvu_alloc(ctx, (size_t)bufw * 3);
        if (!p1 || !p2) { djvu_free(ctx, lbuf); djvu_free(ctx, p1); djvu_free(ctx, p2); return -1; }
    }
    if (red_xmin == 0) {
        int x;
        int use16 = (s->redw * 3) <= 0x0fff;
        if (use16)
            hinfo16 = (uint16_t *)djvu_alloc(ctx, sizeof(uint16_t) * s->outw);
        else
            hinfo32 = (uint32_t *)djvu_alloc(ctx, sizeof(uint32_t) * s->outw);
        if ((use16 && !hinfo16) || (!use16 && !hinfo32)) {
            djvu_free(ctx, hinfo16);
            djvu_free(ctx, hinfo32);
            djvu_free(ctx, lbuf);
            djvu_free(ctx, p1);
            djvu_free(ctx, p2);
            return -1;
        }
        if (use16) {
            for (x = 0; x < s->outw; x++) {
                int n = s->hcoord[x];
                int off = (1 + (n >> FRACBITS)) * 3;
                hinfo16[x] = (uint16_t)((off << FRACBITS) | (n & FRACMASK));
            }
        } else {
            for (x = 0; x < s->outw; x++) {
                int n = s->hcoord[x];
                int off = (1 + (n >> FRACBITS)) * 3;
                hinfo32[x] = (uint32_t)((off << FRACBITS) | (n & FRACMASK));
            }
        }
    }

    for (y = 0; y < s->outh; y++) {
        int fy = s->vcoord[y];
        int fy1 = fy >> FRACBITS, fy2 = fy1 + 1;
        const uint8_t *lower, *upper;
        uint8_t *dest;
        int x;
        int vf;

        if (s->xshift > 0 || s->yshift > 0) {
            int want1 = fy1 < 0 ? 0 : (fy1 >= s->redh ? s->redh - 1 : fy1);
            int want2 = fy2 < 0 ? 0 : (fy2 >= s->redh ? s->redh - 1 : fy2);
            if (want1 == l2) lower = p2; else if (want1 == l1) lower = p1;
            else { uint8_t *t = p1; p1 = p2; l1 = l2; p2 = t; l2 = want1;
                   scaler_get_line(s, want1, in, 0, 0, red_xmin, red_xmax, p2); lower = p2; }
            if (want2 == l2) upper = p2; else if (want2 == l1) upper = p1;
            else { uint8_t *t = p1; p1 = p2; l1 = l2; p2 = t; l2 = want2;
                   scaler_get_line(s, want2, in, 0, 0, red_xmin, red_xmax, p2); upper = p2; }
        } else {
            if (fy1 < 0) fy1 = 0;
            if (fy2 < 0) fy2 = 0;
            if (fy1 >= in->h) fy1 = in->h - 1;
            if (fy2 >= in->h) fy2 = in->h - 1;
            if (fy1 > s->redh - 1) fy1 = s->redh - 1;
            if (fy2 > s->redh - 1) fy2 = s->redh - 1;
            lower = in->d + (size_t)fy1 * in->w * 3;
            upper = in->d + (size_t)fy2 * in->w * 3;
        }
        vf = fy & FRACMASK;
        for (x = 0; x < bufw; x++) {
            int lr = lower[x*3+0], lg = lower[x*3+1], lb = lower[x*3+2];
            lbuf[(x+1)*3+0] = (uint8_t)(lr + (((upper[x*3+0] - lr) * vf + FRACSIZE2) >> FRACBITS));
            lbuf[(x+1)*3+1] = (uint8_t)(lg + (((upper[x*3+1] - lg) * vf + FRACSIZE2) >> FRACBITS));
            lbuf[(x+1)*3+2] = (uint8_t)(lb + (((upper[x*3+2] - lb) * vf + FRACSIZE2) >> FRACBITS));
        }
        lbuf[0]=lbuf[3]; lbuf[1]=lbuf[4]; lbuf[2]=lbuf[5];
        lbuf[(bufw+1)*3+0]=lbuf[bufw*3+0]; lbuf[(bufw+1)*3+1]=lbuf[bufw*3+1]; lbuf[(bufw+1)*3+2]=lbuf[bufw*3+2];
        dest = dst0 + (size_t)(topdown ? (s->outh - 1 - y) : y) * stride;
        if (hinfo16) {
            uint8_t *dp = dest;
            for (x = 0; x < s->outw; x++) {
                int n = hinfo16[x];
                const uint8_t *lo = lbuf + (n >> FRACBITS);
                int hf = n & FRACMASK;
                int lr = lo[0], lg = lo[1], lb = lo[2];
                int dr = lo[3] - lr, dg = lo[4] - lg, db = lo[5] - lb;
                dp[0] = (uint8_t)(lr + ((dr * hf + FRACSIZE2) >> FRACBITS));
                dp[1] = (uint8_t)(lg + ((dg * hf + FRACSIZE2) >> FRACBITS));
                dp[2] = (uint8_t)(lb + ((db * hf + FRACSIZE2) >> FRACBITS));
                dp += 3;
            }
        } else if (hinfo32) {
            uint8_t *dp = dest;
            for (x = 0; x < s->outw; x++) {
                int n = (int)hinfo32[x];
                const uint8_t *lo = lbuf + (n >> FRACBITS);
                int hf = n & FRACMASK;
                int lr = lo[0], lg = lo[1], lb = lo[2];
                int dr = lo[3] - lr, dg = lo[4] - lg, db = lo[5] - lb;
                dp[0] = (uint8_t)(lr + ((dr * hf + FRACSIZE2) >> FRACBITS));
                dp[1] = (uint8_t)(lg + ((dg * hf + FRACSIZE2) >> FRACBITS));
                dp[2] = (uint8_t)(lb + ((db * hf + FRACSIZE2) >> FRACBITS));
                dp += 3;
            }
        } else {
            uint8_t *dp = dest;
            for (x = 0; x < s->outw; x++) {
                int n = s->hcoord[x];
                const uint8_t *lo = lbuf + (1 + (n >> FRACBITS) - red_xmin) * 3;
                int hf = n & FRACMASK;
                int lr = lo[0], lg = lo[1], lb = lo[2];
                dp[0] = (uint8_t)(lr + (((lo[3] - lr) * hf + FRACSIZE2) >> FRACBITS));
                dp[1] = (uint8_t)(lg + (((lo[4] - lg) * hf + FRACSIZE2) >> FRACBITS));
                dp[2] = (uint8_t)(lb + (((lo[5] - lb) * hf + FRACSIZE2) >> FRACBITS));
                dp += 3;
            }
        }
    }
    djvu_free(ctx, hinfo16);
    djvu_free(ctx, hinfo32);
    djvu_free(ctx, lbuf); djvu_free(ctx, p1); djvu_free(ctx, p2);
    return 0;
}

static int scaler_scale(scaler *s, const djvu_cpix *in, djvu_cpix *out)
{
    if (cpix_init_uninit(s->ctx, out, s->outw, s->outh) != 0) return -1;
    return scaler_scale_into(s, in, out->d, s->outw * 3, 0);
}

static void scaler_free(scaler *s)
{
    djvu_free(s->ctx, s->hcoord);
    djvu_free(s->ctx, s->vcoord);
}

int djvu_cpix_init(djvu_ctx *ctx, djvu_cpix *p, int w, int h)
{
    djvu_free(ctx, p->d);
    p->w = w; p->h = h;
    p->d = (uint8_t *)djvu_alloc(ctx, (size_t)w * h * 3);
    if (!p->d) return -1;
    memset(p->d, 0, (size_t)w * h * 3);
    return 0;
}

void djvu_cpix_free(djvu_ctx *ctx, djvu_cpix *p)
{
    if (p) { djvu_free(ctx, p->d); p->d = NULL; }
}

int djvu_compute_red(int w, int h, int rw, int rh)
{
    int red;
    for (red = 1; red < 16; red++)
        if (((w + red - 1) / red == rw) && ((h + red - 1) / red == rh))
            return red;
    return 0;
}

int djvu_cpix_scale(djvu_ctx *ctx, const djvu_cpix *in, djvu_cpix *out,
                    int outw, int outh, int red)
{
    return djvu_cpix_scale_ratio(ctx, in, out, outw, outh, red, 1);
}

int djvu_cpix_scale_ratio(djvu_ctx *ctx, const djvu_cpix *in, djvu_cpix *out,
                          int outw, int outh, int numer, int denom)
{
    scaler s;
    memset(&s, 0, sizeof(s));
    s.ctx = ctx;
    s.inw = in->w;
    s.inh = in->h;
    s.outw = outw;
    s.outh = outh;
    scaler_set_h(&s, numer, denom);
    scaler_set_v(&s, numer, denom);
    if (scaler_scale(&s, in, out) != 0) { scaler_free(&s); return -1; }
    scaler_free(&s);
    return 0;
}

int djvu_cpix_scale_to_topdown_rgb(djvu_ctx *ctx, const djvu_cpix *in,
                                   uint8_t *dst, int stride,
                                   int outw, int outh, int red)
{
    scaler s;
    memset(&s, 0, sizeof(s));
    s.ctx = ctx;
    s.inw = in->w;
    s.inh = in->h;
    s.outw = outw;
    s.outh = outh;
    scaler_set_h(&s, red, 1);
    scaler_set_v(&s, red, 1);
    if (scaler_scale_into(&s, in, dst, stride, 1) != 0) {
        scaler_free(&s);
        return -1;
    }
    scaler_free(&s);
    return 0;
}

void djvu_flip_rgb_bottomup(uint8_t *dst, const uint8_t *src, int w, int h, int bgr)
{
    int y, x;
    size_t row = (size_t)w * 3;
    if (!bgr) {
        for (y = 0; y < h; y++)
            memcpy(dst + (size_t)y * row, src + (size_t)(h - 1 - y) * row, row);
        return;
    }

    for (y = 0; y < h; y++) {
        uint8_t *d = dst + (size_t)y * row;
        const uint8_t *s = src + (size_t)(h - 1 - y) * row;
        for (x = 0; x < w; x++) {
            d[0] = s[2];
            d[1] = s[1];
            d[2] = s[0];
            d += 3;
            s += 3;
        }
    }
}

#include <stdlib.h>
#include <string.h>
#include <math.h>

static int compose_bg_page_no(djvu_doc *doc, uint32_t form_off)
{
    int i;
    if (!doc) return -1;
    for (i = 0; i < doc->npages; i++)
        if (doc->pages[i].form_off == form_off)
            return i;
    return -1;
}

static int compose_background_from_native(djvu_ctx *ctx, const djvu_cpix *native,
                                          int width, int height, int subsample,
                                          djvu_cpix *out)
{
    int red, rw, rh;

    if (!native || !native->d || native->w <= 0 || native->h <= 0) return -1;
    red = djvu_compute_red(width, height, native->w, native->h);
    if (red < 1) return -1;
    rw = (width + subsample - 1) / subsample;
    rh = (height + subsample - 1) / subsample;
    if (red == subsample && native->w == rw && native->h == rh) {
        size_t n = (size_t)rw * (size_t)rh * 3;
        if (djvu_cpix_init(ctx, out, rw, rh) != 0) return -1;
        memcpy(out->d, native->d, n);
        return 0;
    }
    return djvu_cpix_scale_ratio(ctx, native, out, rw, rh, red, subsample);
}

static int compose_bg_native_build(djvu_doc *doc, djvu_page_int *pg, int cache_locked)
{
    djvu_ctx *ctx = doc->ctx;
    iw_pixmap *pm;
    int bw, bh, w, h, pm_owned = 0;
    uint32_t sz;

    if (!djvu_cache_stores_page(ctx)) return -1;
    if (!doc || !pg || pg->bg_native.d) return 0;
    if (!pg->has_info || pg->info.width <= 0 || pg->info.height <= 0)
        return -1;
    if (!djvu_form_find_chunk(doc, pg->form_off, "BG44", &sz, NULL))
        return -1;
    if (cache_locked)
        pm = djvu_doc_iw44_acquire_under_lock(doc, pg, "BG44");
    else
        pm = djvu_doc_iw44_by_form_acquire(doc, pg->form_off, "BG44", &pm_owned);
    if (!pm) return -1;
    bw = djvu_iw44_width(pm);
    bh = djvu_iw44_height(pm);
    if (bw <= 0 || bh <= 0) {
        djvu_doc_iw44_release(ctx, pm, pm_owned);
        return -1;
    }
    if (djvu_cpix_init(ctx, &pg->bg_native, bw, bh) != 0) {
        djvu_doc_iw44_release(ctx, pm, pm_owned);
        return -1;
    }
    if (djvu_iw44_render_rgb_raw(pm, pg->bg_native.d) != 0) {
        djvu_cpix_free(ctx, &pg->bg_native);
        djvu_doc_iw44_release(ctx, pm, pm_owned);
        return -1;
    }
    w = pg->info.width;
    h = pg->info.height;
    if (!pg->bg_scaled.d &&
        compose_background_from_native(ctx, &pg->bg_native, w, h, 1, &pg->bg_scaled) != 0) {
        djvu_cpix_free(ctx, &pg->bg_native);
        djvu_doc_iw44_release(ctx, pm, pm_owned);
        return -1;
    }
    djvu_doc_iw44_release(ctx, pm, pm_owned);
    return 0;
}

void djvu_doc_preload_compose_bg_range(djvu_doc *doc, int lo0, int hi0)
{
    int i;

    if (!doc || !djvu_cache_stores_page(doc->ctx)) return;
    if (lo0 < 0) lo0 = 0;
    if (hi0 >= doc->npages) hi0 = doc->npages - 1;
    if (lo0 > hi0) return;
    for (i = lo0; i <= hi0; i++)
        compose_bg_native_build(doc, &doc->pages[i], 0);
}

int djvu_compose_background(djvu_doc *doc, uint32_t form_off, int width, int height,
                            int subsample, djvu_cpix *out)
{
    djvu_ctx *ctx = doc->ctx;
    iw_pixmap *pm;
    int page_no, bw, bh, red, rw, rh, rc = -1, pm_owned = 0;
    djvu_cpix native;
    djvu_page_int *pg;

    if (subsample < 1) subsample = 1;
    rw = (width + subsample - 1) / subsample;
    rh = (height + subsample - 1) / subsample;
    memset(&native, 0, sizeof(native));
    page_no = compose_bg_page_no(doc, form_off);
    if (page_no >= 0 && djvu_cache_stores_page(ctx)) {
        pg = &doc->pages[page_no];
        djvu_cache_lock(ctx);
        if (!pg->bg_native.d)
            compose_bg_native_build(doc, pg, 1);
        if (pg->bg_scaled.d && pg->bg_scaled.w == rw && pg->bg_scaled.h == rh) {
            size_t n = (size_t)rw * (size_t)rh * 3;
            djvu_free(ctx, out->d);
            out->w = rw;
            out->h = rh;
            out->d = (uint8_t *)djvu_alloc(ctx, n);
            if (!out->d) {
                djvu_cache_unlock(ctx);
                return -1;
            }
            memcpy(out->d, pg->bg_scaled.d, n);
            djvu_cache_unlock(ctx);
            return 0;
        }
        if (pg->bg_native.d) {
            rc = compose_background_from_native(ctx, &pg->bg_native, width, height,
                                                subsample, out);
            djvu_cache_unlock(ctx);
            return rc;
        }
        djvu_cache_unlock(ctx);
    }

    pm = djvu_doc_iw44_by_form_acquire(doc, form_off, "BG44", &pm_owned);
    if (!pm) return -1;
    bw = djvu_iw44_width(pm); bh = djvu_iw44_height(pm);
    red = djvu_compute_red(width, height, bw, bh);
    if (red < 1) goto done;
    if (djvu_cpix_init(ctx, &native, bw, bh) != 0) goto done;
    if (djvu_iw44_render_rgb_raw(pm, native.d) != 0) goto done;
    if (red == subsample && bw == rw && bh == rh) {
        *out = native; native.d = NULL; rc = 0;
    } else {
        rc = djvu_cpix_scale_ratio(ctx, &native, out, rw, rh, red, subsample);
    }
done:
    djvu_cpix_free(ctx, &native);
    djvu_doc_iw44_release(ctx, pm, pm_owned);
    return rc;
}

static int build_gamma_lut(double corr, unsigned char lut[256])
{
    int i;
    if (corr < 0.1) corr = 0.1; else if (corr > 10.0) corr = 10.0;
    if (corr > 0.999 && corr < 1.001) {
        for (i = 0; i < 256; i++) lut[i] = (unsigned char)i;
        return 0;
    }
    for (i = 0; i < 256; i++) {
        double x = pow((double)i / 255.0, 1.0 / corr);
        int v = (int)floor(255.0 * x + 0.5);
        lut[i] = (unsigned char)(v < 0 ? 0 : v > 255 ? 255 : v);
    }
    lut[0] = 0; lut[255] = 255;
    return 1;
}

static double page_gamma(djvu_doc *doc, uint32_t form_off)
{
    uint32_t sz;
    const uint8_t *info = djvu_form_find_chunk(doc, form_off, "INFO", &sz, NULL);
    if (info && sz >= 9 && info[8] != 0)
        return (double)info[8] / 10.0;
    return 2.2;
}

typedef struct {
    djvu_cpix *bg;
    int palr, palg, palb;
    int has_pal, has_fg;
    int fgred;
    djvu_cpix *fgnat;
} compose_ink_ctx;

static void compose_stamp_ink(void *user, int px, int py)
{
    compose_ink_ctx *ink = (compose_ink_ctx *)user;
    uint8_t *d;

    if (py < 0 || py >= ink->bg->h || px < 0 || px >= ink->bg->w) return;
    d = ink->bg->d + ((size_t)py * ink->bg->w + px) * 3;
    if (ink->has_pal) {
        d[0] = (uint8_t)ink->palr; d[1] = (uint8_t)ink->palg; d[2] = (uint8_t)ink->palb;
    } else if (ink->has_fg) {
        int fx = px / ink->fgred, fy = py / ink->fgred;
        if (fx >= ink->fgnat->w) fx = ink->fgnat->w - 1;
        if (fy >= ink->fgnat->h) fy = ink->fgnat->h - 1;
        {
            uint8_t *f = ink->fgnat->d + ((size_t)fy * ink->fgnat->w + fx) * 3;
            d[0] = f[0]; d[1] = f[1]; d[2] = f[2];
        }
    } else {
        d[0] = d[1] = d[2] = 0;
    }
}

typedef struct {
    uint32_t *acc;
    int tw, th;
    int cx0, cy0;
    int w, h;
    int sub;
} compose_acc_ctx;

static void compose_accum_ink_sub(void *user, int px, int py)
{
    compose_acc_ctx *c = (compose_acc_ctx *)user;
    int cx, cy;

    if (px < 0 || py < 0 || px >= c->w || py >= c->h) return;
    cx = px / c->sub - c->cx0;
    cy = py / c->sub - c->cy0;
    if (cx < 0 || cx >= c->tw || cy < 0 || cy >= c->th) return;
    c->acc[(size_t)cy * c->tw + cx]++;
}

static int compose_stencil_sub(djvu_ctx *ctx, djvu_cpix *bg, jb2_image *mask,
                               int width, int height, int sub,
                               const uint8_t *pal, int palsize,
                               const short *colordata, int ncolor,
                               const djvu_cpix *fgnat, int fgred, int has_fg)
{
    uint32_t *acc = NULL;
    size_t acc_cap = 0;
    int i;

    for (i = 0; mask && i < mask->nblits; i++) {
        jb2_blit *b = &mask->blits[i];
        jb2_shape *s = djvu_jb2_get_shape(mask, b->shapeno);
        compose_acc_ctx c;
        int bx0, by0, bx1, by1, cx0, cy0, tw, th, tx, ty;
        int has_pal = 0, palr = 0, palg = 0, palb = 0;

        if (!s || !djvu_bm_has_pixels(&s->bm)) continue;

        bx0 = b->left; by0 = b->bottom;
        bx1 = b->left + s->bm.width - 1;
        by1 = b->bottom + s->bm.height - 1;
        if (bx1 < 0 || by1 < 0 || bx0 >= width || by0 >= height) continue;
        if (bx0 < 0) bx0 = 0;
        if (by0 < 0) by0 = 0;
        if (bx1 >= width) bx1 = width - 1;
        if (by1 >= height) by1 = height - 1;
        cx0 = bx0 / sub; cy0 = by0 / sub;
        tw = bx1 / sub - cx0 + 1;
        th = by1 / sub - cy0 + 1;
        if ((size_t)tw * th > acc_cap) {
            djvu_free(ctx, acc);
            acc_cap = (size_t)tw * th;
            acc = (uint32_t *)djvu_alloc(ctx, acc_cap * sizeof(uint32_t));
            if (!acc) return -1;
        }
        memset(acc, 0, (size_t)tw * th * sizeof(uint32_t));
        c.acc = acc; c.tw = tw; c.th = th; c.cx0 = cx0; c.cy0 = cy0;
        c.w = width; c.h = height; c.sub = sub;
        djvu_bm_visit_ink(&s->bm, b->left, b->bottom, compose_accum_ink_sub, &c);

        if (pal && colordata && i < ncolor) {
            int ci = colordata[i];
            if (ci >= 0 && ci < palsize) {
                palb = pal[ci * 3 + 0]; palg = pal[ci * 3 + 1]; palr = pal[ci * 3 + 2];
                has_pal = 1;
            }
        }

        for (ty = 0; ty < th; ty++) {
            int gy = cy0 + ty;
            uint8_t *row;
            int ch = height - gy * sub;
            if (gy >= bg->h) break;
            if (ch > sub) ch = sub;
            row = bg->d + (size_t)gy * bg->w * 3;
            for (tx = 0; tx < tw; tx++) {
                uint32_t cnt = acc[(size_t)ty * tw + tx];
                int gx = cx0 + tx;
                int cw, a, r, g, bl;
                uint32_t area;
                uint8_t *d;
                if (!cnt || gx >= bg->w) continue;
                cw = width - gx * sub;
                if (cw > sub) cw = sub;
                area = (uint32_t)cw * (uint32_t)ch;
                a = cnt >= area ? 255 : (int)(cnt * 255 / area);
                if (has_pal) {
                    r = palr; g = palg; bl = palb;
                } else if (has_fg) {

                    int fx = (gx * sub + sub / 2) / fgred;
                    int fy = (gy * sub + sub / 2) / fgred;
                    const uint8_t *f;
                    if (fx >= fgnat->w) fx = fgnat->w - 1;
                    if (fy >= fgnat->h) fy = fgnat->h - 1;
                    f = fgnat->d + ((size_t)fy * fgnat->w + fx) * 3;
                    r = f[0]; g = f[1]; bl = f[2];
                } else {
                    r = g = bl = 0;
                }
                d = row + (size_t)gx * 3;
                d[0] = (uint8_t)((d[0] * (255 - a) + r * a + 127) / 255);
                d[1] = (uint8_t)((d[1] * (255 - a) + g * a + 127) / 255);
                d[2] = (uint8_t)((d[2] * (255 - a) + bl * a + 127) / 255);
            }
        }
    }
    djvu_free(ctx, acc);
    return 0;
}

static void compose_finalize(uint8_t *dst, int stride, const djvu_cpix *bg,
                             int bgr, const unsigned char *lut)
{
    int x, y;

    if (!lut && !bgr) {
        size_t row = (size_t)bg->w * 3;
        for (y = 0; y < bg->h; y++)
            memcpy(dst + (size_t)y * stride,
                   bg->d + (size_t)(bg->h - 1 - y) * row, row);
        return;
    }

    for (y = 0; y < bg->h; y++) {
        const uint8_t *s = bg->d + (size_t)(bg->h - 1 - y) * bg->w * 3;
        uint8_t *d = dst + (size_t)y * stride;
        for (x = 0; x < bg->w; x++) {
            uint8_t r = s[0], g = s[1], b = s[2];
            if (lut) { r = lut[r]; g = lut[g]; b = lut[b]; }
            if (bgr) { d[0] = b; d[1] = g; d[2] = r; }
            else     { d[0] = r; d[1] = g; d[2] = b; }
            d += 3; s += 3;
        }
    }
}

typedef struct {
    uint8_t *pal;
    int palsize;
    short *colordata;
    int ncolor;
} fgbz_palette;

static void fgbz_palette_free(djvu_ctx *ctx, fgbz_palette *fg)
{
    if (!fg) return;
    djvu_free(ctx, fg->pal);
    djvu_free(ctx, fg->colordata);
    memset(fg, 0, sizeof(*fg));
}

static int fgbz_palette_parse(djvu_ctx *ctx, const uint8_t *fgbz,
                              uint32_t sz, fgbz_palette *fg)
{
    size_t p = 0;
    int version, i;

    memset(fg, 0, sizeof(*fg));
    if (!fgbz || sz < 3) return -1;
    version = fgbz[p++];
    fg->palsize = (fgbz[p] << 8) | fgbz[p + 1];
    p += 2;
    if ((size_t)p + (size_t)fg->palsize * 3 > sz) return -1;
    fg->pal = (uint8_t *)djvu_alloc(ctx, (size_t)fg->palsize * 3);
    if (!fg->pal) return -1;
    memcpy(fg->pal, fgbz + p, (size_t)fg->palsize * 3);
    p += (size_t)fg->palsize * 3;

    if ((version & 0x80) && p + 3 <= sz) {
        int datasize = (fgbz[p] << 16) | (fgbz[p + 1] << 8) | fgbz[p + 2];
        size_t dlen = 0;
        uint8_t *dd;

        p += 3;
        dd = djvu_bzz_decode_all(ctx, fgbz + p, sz - p, &dlen);
        if (dd && (size_t)datasize * 2 <= dlen) {
            fg->colordata = (short *)djvu_alloc(ctx, sizeof(short) * datasize);
            if (fg->colordata) {
                for (i = 0; i < datasize; i++)
                    fg->colordata[i] = (short)((dd[i * 2] << 8) | dd[i * 2 + 1]);
                fg->ncolor = datasize;
            }
        }
        djvu_free(ctx, dd);
    }
    return 0;
}

static int compose_background_topdown_rgb(djvu_doc *doc, uint32_t form_off,
                                          int width, int height,
                                          uint8_t *dst, int stride,
                                          djvu_render_timings *t)
{
    djvu_ctx *ctx = doc->ctx;
    iw_pixmap *pm;
    djvu_cpix native;
    int bw, bh, red, pm_owned = 0, rc = -1;
    double t0 = 0.0;

    memset(&native, 0, sizeof(native));
    if (t) t0 = djvu_bench_now_ms();
    pm = djvu_doc_iw44_by_form_acquire(doc, form_off, "BG44", &pm_owned);
    if (!pm) goto done;
    bw = djvu_iw44_width(pm);
    bh = djvu_iw44_height(pm);
    red = djvu_compute_red(width, height, bw, bh);
    if (red < 1) goto done;
    if (djvu_cpix_init(ctx, &native, bw, bh) != 0) goto done;
    if (djvu_iw44_render_rgb_raw(pm, native.d) != 0) goto done;
    if (red == 1 && bw == width && bh == height) {
        size_t row = (size_t)width * 3;
        int y;
        for (y = 0; y < height; y++)
            memcpy(dst + (size_t)y * stride,
                   native.d + (size_t)(height - 1 - y) * row, row);
        rc = 0;
    } else {
        rc = djvu_cpix_scale_to_topdown_rgb(ctx, &native, dst, stride,
                                            width, height, red);
    }
done:
    if (t) t->iw44_ms += djvu_bench_now_ms() - t0;
    djvu_cpix_free(ctx, &native);
    djvu_doc_iw44_release(ctx, pm, pm_owned);
    return rc;
}

static int compose_read_bm_run(const uint8_t **data)
{
    int z = *(*data)++;
    if (z >= 0xc0)
        z = ((z & ~0xc0) << 8) | (int)(*(*data)++);
    return z;
}

static void compose_fill_rgb_run(uint8_t *d, int n, int r, int g, int b)
{
    while (n-- > 0) {
        d[0] = (uint8_t)r;
        d[1] = (uint8_t)g;
        d[2] = (uint8_t)b;
        d += 3;
    }
}

static void compose_stamp_bitmap_topdown_rgb(const djvu_bitmap *src,
                                             int left, int bottom,
                                             int outw, int outh,
                                             uint8_t *dst, int stride,
                                             int r, int g, int b)
{
    if (!src || outw <= 0 || outh <= 0) return;
    if (src->rle) {
        const uint8_t *runs = src->rle;
        const uint8_t *runs_end = src->rle + src->rle_len;
        int sr = src->height - 1;
        int sc = 0, p = 0;

        while (runs < runs_end && sr >= 0) {
            int z = compose_read_bm_run(&runs);
            int nc;

            if (sc + z > src->width) return;
            nc = sc + z;
            if (p) {
                int py = bottom + sr;
                if (py >= 0 && py < outh) {
                    int x0 = left + sc;
                    int x1 = left + nc;
                    if (x0 < 0) x0 = 0;
                    if (x1 > outw) x1 = outw;
                    if (x0 < x1) {
                        uint8_t *d = dst + (size_t)(outh - 1 - py) * stride
                                   + (size_t)x0 * 3;
                        compose_fill_rgb_run(d, x1 - x0, r, g, b);
                    }
                }
            }
            sc = nc;
            p = 1 - p;
            if (sc >= src->width) {
                sc = 0;
                p = 0;
                sr--;
            }
        }
    } else if (src->data) {
        int rr;
        for (rr = 0; rr < src->height; rr++) {
            int py = bottom + rr;
            int cc;
            const uint8_t *srow;
            if (py < 0 || py >= outh) continue;
            srow = src->data + djvu_bm_rowoffset(src, rr);
            for (cc = 0; cc < src->width; cc++) {
                int px = left + cc;
                if (px >= 0 && px < outw && srow[cc]) {
                    uint8_t *d = dst + (size_t)(outh - 1 - py) * stride
                               + (size_t)px * 3;
                    d[0] = (uint8_t)r;
                    d[1] = (uint8_t)g;
                    d[2] = (uint8_t)b;
                }
            }
        }
    }
}

static int compose_fgbz_stencil_topdown_rgb(jb2_image *mask,
                                            const fgbz_palette *fg,
                                            int width, int height,
                                            uint8_t *dst, int stride)
{
    int i;

    for (i = 0; mask && i < mask->nblits; i++) {
        jb2_blit *b = &mask->blits[i];
        jb2_shape *s = djvu_jb2_get_shape(mask, b->shapeno);
        int r = 0, g = 0, bl = 0;

        if (!s || !djvu_bm_has_pixels(&s->bm)) continue;
        if (fg->pal && fg->colordata && i < fg->ncolor) {
            int ci = fg->colordata[i];
            if (ci >= 0 && ci < fg->palsize) {
                bl = fg->pal[ci * 3 + 0];
                g  = fg->pal[ci * 3 + 1];
                r  = fg->pal[ci * 3 + 2];
            }
        }
        compose_stamp_bitmap_topdown_rgb(&s->bm, b->left, b->bottom,
                                         width, height, dst, stride, r, g, bl);
    }
    return 0;
}

static int compose_to_bg(djvu_doc *doc, int page_no, jb2_image *mask,
                         int width, int height, int subsample,
                         djvu_render_timings *t, djvu_cpix *bgout)
{
    djvu_ctx *ctx = doc->ctx;
    uint32_t form_off = doc->pages[page_no].form_off;
    djvu_cpix bg;
    uint32_t sz; const uint8_t *fgbz;
    uint8_t *pal = NULL; int palsize = 0;
    short *colordata = NULL; int ncolor = 0;
    iw_pixmap *fgpm = NULL; djvu_cpix fgnat; int fgred = 0, fg_owned = 0;
    int i, stencil_rc = 0;
    double t0 = 0.0;

    if (djvu_aborted(ctx)) return -1;
    if (subsample < 1) subsample = 1;
    memset(&bg, 0, sizeof(bg)); memset(&fgnat, 0, sizeof(fgnat));
    if (t) t0 = djvu_bench_now_ms();
    if (djvu_compose_background(doc, form_off, width, height, subsample, &bg) != 0)
        return -1;
    if (t) t->iw44_ms += djvu_bench_now_ms() - t0;

    if (t) t0 = djvu_bench_now_ms();

    fgbz = djvu_form_find_chunk(doc, form_off, "FGbz", &sz, NULL);
    if (fgbz && sz >= 3) {
        size_t p = 0;
        int version = fgbz[p++];
        palsize = (fgbz[p] << 8) | fgbz[p + 1]; p += 2;
        if ((size_t)p + (size_t)palsize * 3 <= sz) {
            pal = (uint8_t *)djvu_alloc(ctx, (size_t)palsize * 3);
            if (pal) memcpy(pal, fgbz + p, (size_t)palsize * 3);
            p += (size_t)palsize * 3;
            if ((version & 0x80) && p + 3 <= sz) {
                int datasize = (fgbz[p] << 16) | (fgbz[p+1] << 8) | fgbz[p+2]; p += 3;
                size_t dlen = 0;
                uint8_t *dd = djvu_bzz_decode_all(ctx, fgbz + p, sz - p, &dlen);
                if (dd && (size_t)datasize * 2 <= dlen) {
                    colordata = (short *)djvu_alloc(ctx, sizeof(short) * datasize);
                    if (colordata) {
                        for (i = 0; i < datasize; i++)
                            colordata[i] = (short)((dd[i*2] << 8) | dd[i*2+1]);
                        ncolor = datasize;
                    }
                }
                djvu_free(ctx, dd);
            }
        }
    }

    if (!pal) {
        double tfg = 0.0;
        if (t) tfg = djvu_bench_now_ms();
        fgpm = djvu_doc_iw44_acquire(doc, page_no, "FG44", &fg_owned);
        if (fgpm) {
            int fw = djvu_iw44_width(fgpm);
            int fh = djvu_iw44_height(fgpm);
            fgred = djvu_compute_red(width, height, fw, fh);
            if (fgred < 1) fgred = 1;
            if (djvu_cpix_init(ctx, &fgnat, fw, fh) != 0 ||
                djvu_iw44_render_rgb_raw(fgpm, fgnat.d) != 0)
                fgpm = NULL;
        }
        if (t) t->iw44_ms += djvu_bench_now_ms() - tfg;
    }

    if (subsample > 1) {
        stencil_rc = compose_stencil_sub(ctx, &bg, mask, width, height, subsample,
                                         pal, palsize, colordata, ncolor,
                                         &fgnat, fgred, fgpm != NULL);
    } else {
        for (i = 0; mask && i < mask->nblits; i++) {
            jb2_blit *b = &mask->blits[i];
            jb2_shape *s = djvu_jb2_get_shape(mask, b->shapeno);
            compose_ink_ctx ink;
            if ((i & 63) == 0 && djvu_aborted(ctx)) return -1;
            if (!s || !djvu_bm_has_pixels(&s->bm)) continue;
            ink.bg = &bg;
            ink.palr = ink.palg = ink.palb = 0;
            ink.has_pal = ink.has_fg = 0;
            ink.fgred = fgred;
            ink.fgnat = &fgnat;
            if (pal && colordata && i < ncolor) {
                int ci = colordata[i];
                if (ci >= 0 && ci < palsize) {
                    ink.palb = pal[ci*3+0]; ink.palg = pal[ci*3+1]; ink.palr = pal[ci*3+2];
                    ink.has_pal = 1;
                }
            } else if (fgpm) {
                ink.has_fg = 1;
            }
            djvu_bm_visit_ink(&s->bm, b->left, b->bottom, compose_stamp_ink, &ink);
        }
    }

    if (t) t->composite_ms += djvu_bench_now_ms() - t0;

    djvu_free(ctx, pal); djvu_free(ctx, colordata);
    djvu_cpix_free(ctx, &fgnat);
    djvu_doc_iw44_release(ctx, fgpm, fg_owned);
    if (stencil_rc != 0) {
        djvu_cpix_free(ctx, &bg);
        return -1;
    }
    *bgout = bg;
    return 0;
}

static int compose_gamma_lut(djvu_doc *doc, uint32_t form_off, unsigned char *lut)
{
    return build_gamma_lut(2.2 / page_gamma(doc, form_off), lut);
}

static int compose_page_fgbz_direct_rgb(djvu_doc *doc, int page_no,
                                        jb2_image *mask,
                                        int width, int height,
                                        uint8_t *dst, int stride,
                                        djvu_render_timings *t)
{
    djvu_ctx *ctx = doc->ctx;
    uint32_t form_off = doc->pages[page_no].form_off;
    uint32_t sz;
    const uint8_t *fgbz;
    unsigned char lut[256];
    fgbz_palette fg;
    double t0 = 0.0;
    int rc = -1;

    memset(&fg, 0, sizeof(fg));
    if (!mask || ctx->bgr) return -1;
    if (compose_gamma_lut(doc, form_off, lut)) return -1;
    if (djvu_form_find_chunk(doc, form_off, "FG44", &sz, NULL) != NULL)
        return -1;
    fgbz = djvu_form_find_chunk(doc, form_off, "FGbz", &sz, NULL);
    if (fgbz_palette_parse(ctx, fgbz, sz, &fg) != 0)
        return -1;

    if (compose_background_topdown_rgb(doc, form_off, width, height,
                                       dst, stride, t) != 0)
        goto done;

    if (t) t0 = djvu_bench_now_ms();
    rc = compose_fgbz_stencil_topdown_rgb(mask, &fg, width, height, dst, stride);
    if (t) t->composite_ms += djvu_bench_now_ms() - t0;

done:
    fgbz_palette_free(ctx, &fg);
    return rc;
}

djvu_image *djvu_compose_page(djvu_doc *doc, int page_no, jb2_image *mask,
                             int width, int height, int subsample,
                             djvu_render_timings *t)
{
    djvu_ctx *ctx = doc->ctx;
    uint32_t form_off = doc->pages[page_no].form_off;
    djvu_cpix bg; djvu_image *out;
    unsigned char lut[256]; const unsigned char *lp = NULL;

    if (subsample < 1) subsample = 1;
    memset(&bg, 0, sizeof(bg));

    if (subsample == 1 && mask && !ctx->bgr &&
        djvu_form_find_chunk(doc, form_off, "FGbz", NULL, NULL) != NULL &&
        djvu_form_find_chunk(doc, form_off, "FG44", NULL, NULL) == NULL &&
        !compose_gamma_lut(doc, form_off, lut)) {
        out = (djvu_image *)djvu_alloc(ctx, sizeof(djvu_image));
        if (out) {
            out->width = width;
            out->height = height;
            out->format = DJVU_FORMAT_RGB24;
            out->stride = width * 3;
            out->data = (uint8_t *)djvu_alloc(ctx, (size_t)width * height * 3);
            if (!out->data) {
                djvu_free(ctx, out);
                out = NULL;
            } else if (compose_page_fgbz_direct_rgb(doc, page_no, mask, width, height,
                                                    out->data, out->stride, t) == 0) {
                return out;
            } else {
                djvu_image_destroy(ctx, out);
                out = NULL;
            }
        }
    }

    if (compose_to_bg(doc, page_no, mask, width, height, subsample, t, &bg) != 0)
        return NULL;
    if (compose_gamma_lut(doc, form_off, lut)) lp = lut;

    out = (djvu_image *)djvu_alloc(ctx, sizeof(djvu_image));
    if (out) {
        out->width = bg.w; out->height = bg.h; out->format = DJVU_FORMAT_RGB24;
        out->stride = bg.w * 3;
        out->data = (uint8_t *)djvu_alloc(ctx, (size_t)bg.w * bg.h * 3);
        if (out->data)
            compose_finalize(out->data, bg.w * 3, &bg, ctx->bgr, lp);
        else { djvu_free(ctx, out); out = NULL; }
    }
    djvu_cpix_free(ctx, &bg);
    return out;
}

int djvu_compose_page_into(djvu_doc *doc, int page_no, jb2_image *mask,
                           int width, int height, int subsample,
                           uint8_t *dst, int stride)
{
    djvu_ctx *ctx = doc->ctx;
    uint32_t form_off = doc->pages[page_no].form_off;
    djvu_cpix bg;
    unsigned char lut[256]; const unsigned char *lp = NULL;

    if (djvu_aborted(ctx)) return -1;
    if (subsample < 1) subsample = 1;
    memset(&bg, 0, sizeof(bg));
    if (subsample == 1 &&
        compose_page_fgbz_direct_rgb(doc, page_no, mask, width, height,
                                     dst, stride, NULL) == 0)
        return 0;
    if (compose_to_bg(doc, page_no, mask, width, height, subsample, NULL, &bg) != 0)
        return -1;
    if (compose_gamma_lut(doc, form_off, lut)) lp = lut;
    compose_finalize(dst, stride, &bg, ctx->bgr, lp);
    djvu_cpix_free(ctx, &bg);
    return 0;
}

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static void *default_alloc(void *user, void *ctx, size_t size) { (void)user; (void)ctx; return malloc(size); }
static void  default_free(void *user, void *ctx, void *ptr) { (void)user; (void)ctx; free(ptr); }

void *djvu_alloc(djvu_ctx *ctx, size_t size) { return ctx->alloc(ctx->user, ctx, size); }
void  djvu_free(djvu_ctx *ctx, void *ptr) { if (ptr) ctx->free(ctx->user, ctx, ptr); }

void djvu_errorf(djvu_ctx *ctx, djvu_severity sev, const char *fmt, ...)
{
    char buf[512];
    va_list ap;
    if (!ctx->error) return;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    ctx->error(ctx->user, sev, buf);
}

djvu_ctx *djvu_ctx_new(djvu_alloc_cb alloc, djvu_free_cb free_cb,
                       djvu_lock_cb lock, djvu_unlock_cb unlock,
                       djvu_error_cb error, void *user)
{
    djvu_ctx *ctx;
    djvu_alloc_cb a = alloc ? alloc : default_alloc;

    ctx = (djvu_ctx *)a(user, NULL, sizeof(djvu_ctx));
    if (!ctx) return NULL;
    ctx->alloc = a;
    ctx->free = free_cb ? free_cb : default_free;
    ctx->lock = lock;
    ctx->unlock = unlock;
    ctx->error = error;
    ctx->user = user;
    ctx->cache_per_page = 0;
    ctx->no_compose = 0;
    ctx->iw_max_chunks = 0;
    ctx->bgr = 0;
    djvu_atomic_epoch_init(&ctx->abort_epoch);
    return ctx;
}

void djvu_ctx_free(djvu_ctx *ctx)
{

    if (ctx) ctx->free(ctx->user, NULL, ctx);
}

void djvu_ctx_set_cache_per_page(djvu_ctx *ctx, int enable)
{
    if (ctx) ctx->cache_per_page = enable ? 1 : 0;
}

void djvu_ctx_set_lazy_iw44(djvu_ctx *ctx, int enable)
{
    djvu_ctx_set_cache_per_page(ctx, enable);
}

void djvu_ctx_set_no_compose(djvu_ctx *ctx, int enable)
{
    if (ctx) ctx->no_compose = enable ? 1 : 0;
}

void djvu_ctx_set_bgr(djvu_ctx *ctx, int enable)
{
    if (ctx) ctx->bgr = enable ? 1 : 0;
}

void djvu_ctx_set_iw_max_chunks(djvu_ctx *ctx, int max_chunks)
{
    if (ctx) ctx->iw_max_chunks = max_chunks < 0 ? 0 : max_chunks;
}

void djvu_request_abort(djvu_ctx *ctx)
{
    if (ctx)
        djvu_atomic_epoch_bump(&ctx->abort_epoch);
}

#if defined(_MSC_VER)
__declspec(thread) uint32_t djvu_render_epoch_tls;
__declspec(thread) const djvu_abort *djvu_render_abort_tls;
#else
__thread uint32_t djvu_render_epoch_tls;
__thread const djvu_abort *djvu_render_abort_tls;
#endif

void djvu_abort_init(djvu_abort *ab)
{
    if (ab) ab->requested = 0;
}

void djvu_abort_request(djvu_abort *ab)
{

    if (ab) ab->requested = 1;
}

static int parse_info(const uint8_t *p, size_t len, djvu_page_info *info)
{
    int flag;
    if (len < 5) return -1;
    info->width = (int)djvu_rd_u16be(p);
    info->height = (int)djvu_rd_u16be(p + 2);
    info->version = p[4];
    info->dpi = 300;
    info->rotation = 0;
    if (len >= 8) info->dpi = (int)djvu_rd_u16le(p + 6);
    if (len >= 10) {
        flag = p[9] & 0x7;
        switch (flag) {
            case 6: info->rotation = 90; break;
            case 2: info->rotation = 180; break;
            case 5: info->rotation = 270; break;
            default: info->rotation = 0; break;
        }
    }
    if (info->dpi <= 0) info->dpi = 300;
    return 0;
}

static int page_load_info(djvu_doc *doc, djvu_page_int *pg)
{
    const uint8_t *data = doc->data;
    size_t len = doc->len;
    uint32_t off = pg->form_off;
    uint32_t form_end;
    uint32_t pos;

    if (pg->has_info) return 0;
    if ((size_t)off + 12 > len) return -1;
    if (!djvu_tag_eq(data + off, "FORM")) return -1;
    form_end = off + 8 + pg->form_size;
    if (form_end > len) form_end = (uint32_t)len;


    pos = off + 12;
    while (pos + 8 <= form_end) {
        const uint8_t *id = data + pos;
        uint32_t csize = djvu_rd_u32be(data + pos + 4);
        uint32_t cdata = pos + 8;

        if (csize > form_end - cdata) csize = form_end - cdata;
        if (djvu_tag_eq(id, "INFO")) {
            if (parse_info(data + cdata, csize, &pg->info) == 0) {
                pg->has_info = 1;
                return 0;
            }
            return -1;
        }
        pos = cdata + csize;
        if (csize & 1) pos++;
    }
    return -1;
}

static char *dup_cstr(djvu_ctx *ctx, const char *s, size_t maxlen, size_t *consumed)
{
    size_t n = 0;
    char *r;
    while (n < maxlen && s[n]) n++;
    r = (char *)djvu_alloc(ctx, n + 1);
    if (r) { memcpy(r, s, n); r[n] = 0; }
    *consumed = (n < maxlen) ? n + 1 : n;
    return r;
}

static int load_djvm(djvu_doc *doc, uint32_t dirm_data, uint32_t dirm_size)
{
    djvu_ctx *ctx = doc->ctx;
    const uint8_t *data = doc->data;
    size_t len = doc->len;
    uint32_t pos = dirm_data;
    uint32_t dirm_end = dirm_data + dirm_size;
    int count, i, npages = 0;
    int version, bundled;
    uint8_t flagbyte;
    uint8_t *dir = NULL;
    size_t dirlen = 0, dp = 0;
    int have_dir = 0;

    if (pos + 3 > dirm_end || dirm_end > len) return -1;
    flagbyte = data[pos++];
    bundled = (flagbyte >> 7) & 1;
    version = flagbyte & 0x7f;
    count = (int)djvu_rd_u16be(data + pos);
    pos += 2;
    if (count <= 0) return -1;

    doc->comps = (djvu_component *)djvu_alloc(ctx, sizeof(djvu_component) * count);
    doc->pages = (djvu_page_int *)djvu_alloc(ctx, sizeof(djvu_page_int) * count);
    if (!doc->comps || !doc->pages) return -1;
    memset(doc->comps, 0, sizeof(djvu_component) * count);
    memset(doc->pages, 0, sizeof(djvu_page_int) * count);
    doc->ncomp = count;


    if (bundled) {
        if ((size_t)pos + (size_t)count * 4 > len) return -1;
        for (i = 0; i < count; i++)
            doc->comps[i].offset = djvu_rd_u32be(data + pos + (uint32_t)i * 4);
        pos += (uint32_t)count * 4;
    }


    if (pos < dirm_end) {
        dir = djvu_bzz_decode_all(ctx, data + pos, dirm_end - pos, &dirlen);
    }
    if (dir) {
        size_t flags_off, sp;

        for (i = 0; i < count && dp + 3 <= dirlen; i++, dp += 3)
            doc->comps[i].size = djvu_rd_u24be(dir + dp);

        flags_off = dp;
        for (i = 0; i < count && dp < dirlen; i++, dp++) {
            uint8_t fl = dir[dp];
            doc->comps[i].type = (version == 0) ? ((fl & 0x01) ? 1 : 0)
                                                : (fl & 0x3f);
        }

        sp = dp;
        for (i = 0; i < count && sp < dirlen; i++) {
            uint8_t fl = (flags_off + (size_t)i < dirlen) ? dir[flags_off + i] : 0;
            int has_name, has_title;
            size_t used;
            if (version == 0) {
                has_name = (fl & 0x02) != 0; has_title = (fl & 0x04) != 0;
            } else {
                has_name = (fl & 0x80) != 0; has_title = (fl & 0x40) != 0;
            }
            doc->comps[i].id = dup_cstr(ctx, (char *)dir + sp, dirlen - sp, &used);
            sp += used;
            if (has_name && sp < dirlen) {
                char *nm = dup_cstr(ctx, (char *)dir + sp, dirlen - sp, &used);
                djvu_free(ctx, nm); sp += used;
            }
            if (has_title && sp < dirlen) {
                doc->comps[i].title = dup_cstr(ctx, (char *)dir + sp, dirlen - sp, &used);
                sp += used;
            }
        }
        djvu_free(ctx, dir);
        dir = NULL;
        have_dir = 1;
    }


    for (i = 0; i < count; i++) {
        uint32_t o = doc->comps[i].offset;
        int is_page = (doc->comps[i].type == 1);
        if (!bundled) continue;
        if ((size_t)o + 12 > len || !djvu_tag_eq(data + o, "FORM")) continue;
        if (!have_dir) is_page = djvu_tag_eq(data + o + 8, "DJVU");
        if (!is_page) continue;
        doc->pages[npages].form_off = o;
        doc->pages[npages].form_size = djvu_rd_u32be(data + o + 4);
        doc->pages[npages].id = doc->comps[i].id;
        doc->pages[npages].title = doc->comps[i].title;
        npages++;
    }
    doc->npages = npages;
    return 0;
}

static void doc_shared_incl_add(djvu_doc *doc, const char *id)
{
    int i, n;
    char *copy;

    if (!doc || !id || !id[0]) return;
    for (i = 0; i < doc->n_shared_incl; i++)
        if (doc->shared_incl_ids[i] && strcmp(doc->shared_incl_ids[i], id) == 0)
            return;
    copy = (char *)djvu_alloc(doc->ctx, strlen(id) + 1);
    if (!copy) return;
    strcpy(copy, id);
    n = doc->n_shared_incl + 1;
    {
        char **ids = (char **)djvu_alloc(doc->ctx, sizeof(char *) * (size_t)n);
        if (!ids) { djvu_free(doc->ctx, copy); return; }
        if (doc->shared_incl_ids) {
            memcpy(ids, doc->shared_incl_ids, sizeof(char *) * (size_t)doc->n_shared_incl);
            djvu_free(doc->ctx, doc->shared_incl_ids);
        }
        doc->shared_incl_ids = ids;
    }
    doc->shared_incl_ids[doc->n_shared_incl] = copy;
    doc->n_shared_incl = n;
}

static void page_index_scan(djvu_doc *doc, djvu_page_int *pg)
{
    uint32_t sz, start = 0;
    const uint8_t *incl;

    if (!doc || !pg) return;
    pg->chunk_flags = 0;
    if (djvu_form_find_chunk(doc, pg->form_off, "Sjbz", &sz, NULL))
        pg->chunk_flags |= DJVU_PG_SJBZ;
    if (djvu_form_find_chunk(doc, pg->form_off, "BG44", &sz, NULL))
        pg->chunk_flags |= DJVU_PG_BG44;
    if (djvu_form_find_chunk(doc, pg->form_off, "FG44", &sz, NULL))
        pg->chunk_flags |= DJVU_PG_FG44;
    if (djvu_form_find_chunk(doc, pg->form_off, "FGbz", &sz, NULL))
        pg->chunk_flags |= DJVU_PG_FGBZ;
    if (djvu_form_find_chunk(doc, pg->form_off, "Djbz", &sz, NULL))
        pg->chunk_flags |= DJVU_PG_DJBZ;
    while ((incl = djvu_form_find_chunk(doc, pg->form_off, "INCL", &sz, &start)) != NULL) {
        char id[64];
        size_t n = sz < sizeof(id) - 1 ? sz : sizeof(id) - 1;
        memcpy(id, incl, n);
        id[n] = 0;
        djvu_trim_incl_id(id);
        doc_shared_incl_add(doc, id);
    }
}

static void doc_build_cache_index(djvu_doc *doc)
{
    int i;

    if (!doc) return;
    for (i = 0; i < doc->ncomp; i++)
        if (doc->comps[i].type == 0 && doc->comps[i].id)
            doc_shared_incl_add(doc, doc->comps[i].id);
    for (i = 0; i < doc->npages; i++)
        page_index_scan(doc, &doc->pages[i]);
}

static void free_shared_incl_index(djvu_ctx *ctx, djvu_doc *doc)
{
    int i;
    if (!doc || !doc->shared_incl_ids) return;
    for (i = 0; i < doc->n_shared_incl; i++)
        djvu_free(ctx, doc->shared_incl_ids[i]);
    djvu_free(ctx, doc->shared_incl_ids);
    doc->shared_incl_ids = NULL;
    doc->n_shared_incl = 0;
}

static void free_page_iw44(djvu_page_int *pg)
{
    if (pg->iw_bg) { djvu_iw44_free(pg->iw_bg); pg->iw_bg = NULL; }
    if (pg->iw_fg) { djvu_iw44_free(pg->iw_fg); pg->iw_fg = NULL; }
}

static void free_page_jb2_mask(djvu_ctx *ctx, djvu_page_int *pg)
{
    if (pg->jb2_mask) {
        djvu_jb2_free(ctx, pg->jb2_mask);
        pg->jb2_mask = NULL;
    }
}

static void free_page_bg_native(djvu_ctx *ctx, djvu_page_int *pg)
{
    djvu_cpix_free(ctx, &pg->bg_native);
    djvu_cpix_free(ctx, &pg->bg_scaled);
}

static iw_pixmap *decode_iw_layer_fresh(djvu_doc *doc, djvu_page_int *pg,
                                        const char *id)
{
    uint32_t sz;
    int maxc;
    iw_pixmap *pm;

    if (!djvu_form_find_chunk(doc, pg->form_off, id, &sz, NULL))
        return NULL;
    pm = djvu_iw44_new(doc->ctx);
    if (!pm) return NULL;
    maxc = doc->ctx->iw_max_chunks;
    if (djvu_iw44_decode_form(doc, pg->form_off, id, pm, maxc) != 0) {
        djvu_iw44_free(pm);
        return NULL;
    }
    return pm;
}

static void preload_iw_layer(djvu_doc *doc, djvu_page_int *pg, const char *id,
                             iw_pixmap **slot)
{
    uint32_t sz;
    int maxc;
    iw_pixmap *pm;

    if (!djvu_cache_stores_page(doc->ctx)) return;
    if (*slot || !djvu_form_find_chunk(doc, pg->form_off, id, &sz, NULL))
        return;
    pm = djvu_iw44_new(doc->ctx);
    if (!pm) return;
    maxc = doc->ctx->iw_max_chunks;
    if (djvu_iw44_decode_form(doc, pg->form_off, id, pm, maxc) != 0) {
        djvu_iw44_free(pm);
        djvu_errorf(doc->ctx, DJVU_SEVERITY_WARNING,
                    "IW44 preload failed (%s at form %u)", id, pg->form_off);
        return;
    }
    *slot = pm;
}

iw_pixmap *djvu_doc_iw44_acquire_under_lock(djvu_doc *doc, djvu_page_int *pg,
                                            const char *chunk_id)
{
    iw_pixmap **slot;

    if (!doc || !pg || !chunk_id) return NULL;
    if (chunk_id[0] == 'B' && chunk_id[1] == 'G' && chunk_id[2] == '4')
        slot = &pg->iw_bg;
    else if (chunk_id[0] == 'F' && chunk_id[1] == 'G' && chunk_id[2] == '4')
        slot = &pg->iw_fg;
    else
        return NULL;

    if (!djvu_cache_stores_page(doc->ctx))
        return decode_iw_layer_fresh(doc, pg, chunk_id);
    if (!*slot)
        preload_iw_layer(doc, pg, chunk_id, slot);
    return *slot;
}

void djvu_doc_preload_iw44_range(djvu_doc *doc, int lo0, int hi0)
{
    int i;

    if (!doc || !djvu_cache_stores_page(doc->ctx)) return;
    if (lo0 < 0) lo0 = 0;
    if (hi0 >= doc->npages) hi0 = doc->npages - 1;
    if (lo0 > hi0) return;
    djvu_cache_lock(doc->ctx);
    for (i = lo0; i <= hi0; i++) {
        djvu_page_int *pg = &doc->pages[i];
        preload_iw_layer(doc, pg, "BG44", &pg->iw_bg);
        preload_iw_layer(doc, pg, "FG44", &pg->iw_fg);
    }
    djvu_cache_unlock(doc->ctx);
}

void djvu_doc_drop_page_iw44(djvu_doc *doc, int page_no)
{
    if (!doc || page_no < 0 || page_no >= doc->npages) return;
    free_page_iw44(&doc->pages[page_no]);
}

iw_pixmap *djvu_doc_iw44_acquire(djvu_doc *doc, int page_no, const char *chunk_id,
                                 int *owned_out)
{
    djvu_page_int *pg;
    iw_pixmap **slot;
    iw_pixmap *pm;

    if (owned_out) *owned_out = 0;
    if (!doc || page_no < 0 || page_no >= doc->npages || !chunk_id) return NULL;
    pg = &doc->pages[page_no];
    if (chunk_id[0] == 'B' && chunk_id[1] == 'G' && chunk_id[2] == '4')
        slot = &pg->iw_bg;
    else if (chunk_id[0] == 'F' && chunk_id[1] == 'G' && chunk_id[2] == '4')
        slot = &pg->iw_fg;
    else
        return NULL;

    if (!djvu_cache_stores_page(doc->ctx)) {
        pm = decode_iw_layer_fresh(doc, pg, chunk_id);
        if (pm && owned_out) *owned_out = 1;
        return pm;
    }
    if (*slot) return *slot;
    djvu_cache_lock(doc->ctx);
    if (!*slot)
        preload_iw_layer(doc, pg, chunk_id, slot);
    djvu_cache_unlock(doc->ctx);
    return *slot;
}

void djvu_doc_iw44_release(djvu_ctx *ctx, iw_pixmap *pm, int owned)
{
    (void)ctx;
    if (owned && pm) djvu_iw44_free(pm);
}

iw_pixmap *djvu_doc_iw44_by_form_acquire(djvu_doc *doc, uint32_t form_off,
                                         const char *chunk_id, int *owned_out)
{
    int i;
    if (!doc) return NULL;
    for (i = 0; i < doc->npages; i++)
        if (doc->pages[i].form_off == form_off)
            return djvu_doc_iw44_acquire(doc, i, chunk_id, owned_out);
    return NULL;
}

iw_pixmap *djvu_doc_iw44(djvu_doc *doc, int page_no, const char *chunk_id)
{
    int owned = 0;
    iw_pixmap *pm = djvu_doc_iw44_acquire(doc, page_no, chunk_id, &owned);
    if (owned) {
        djvu_doc_iw44_release(doc->ctx, pm, 1);
        return NULL;
    }
    return pm;
}

iw_pixmap *djvu_doc_iw44_by_form(djvu_doc *doc, uint32_t form_off, const char *chunk_id)
{
    int owned = 0;
    iw_pixmap *pm = djvu_doc_iw44_by_form_acquire(doc, form_off, chunk_id, &owned);
    if (owned) {
        djvu_doc_iw44_release(doc->ctx, pm, 1);
        return NULL;
    }
    return pm;
}

static jb2_image *jb2_dict_for_form_unlocked(djvu_doc *doc, uint32_t form_off);
static jb2_image *jb2_inline_find_or_decode(djvu_doc *doc, const uint8_t *djbz,
                                            uint32_t sz);

static jb2_image *jb2_dict_find(djvu_doc *doc, const char *incl_id)
{
    int i;
    if (!doc || !incl_id || !incl_id[0]) return NULL;
    for (i = 0; i < doc->n_jb2_dicts; i++)
        if (doc->jb2_dicts[i].incl_id &&
            strcmp(doc->jb2_dicts[i].incl_id, incl_id) == 0)
            return doc->jb2_dicts[i].dict;
    return NULL;
}

static void jb2_dict_cache_add(djvu_doc *doc, const char *incl_id, jb2_image *dict)
{
    djvu_jb2_dict_entry *e;
    int n = doc->n_jb2_dicts + 1;
    char *idcopy;

    if (!doc || !incl_id || !incl_id[0] || !dict || jb2_dict_find(doc, incl_id))
        return;
    idcopy = (char *)djvu_alloc(doc->ctx, strlen(incl_id) + 1);
    if (!idcopy) return;
    strcpy(idcopy, incl_id);
    e = (djvu_jb2_dict_entry *)djvu_alloc(doc->ctx, sizeof(djvu_jb2_dict_entry) * n);
    if (!e) { djvu_free(doc->ctx, idcopy); return; }
    if (doc->jb2_dicts) {
        memcpy(e, doc->jb2_dicts, sizeof(djvu_jb2_dict_entry) * doc->n_jb2_dicts);
        djvu_free(doc->ctx, doc->jb2_dicts);
    }
    doc->jb2_dicts = e;
    doc->jb2_dicts[doc->n_jb2_dicts].incl_id = idcopy;
    doc->jb2_dicts[doc->n_jb2_dicts].dict = dict;
    doc->n_jb2_dicts = n;
}

static void preload_jb2_dict_incl(djvu_doc *doc, const char *incl_id)
{
    uint32_t coff, sz;
    const uint8_t *djbz;
    jb2_image *dict;

    if (!doc || !incl_id || !incl_id[0] || jb2_dict_find(doc, incl_id))
        return;
    coff = djvu_doc_component_offset(doc, incl_id);
    if (!coff) return;
    djbz = djvu_form_find_chunk(doc, coff, "Djbz", &sz, NULL);
    if (!djbz) return;
    dict = djvu_jb2_decode_dict(doc->ctx, djbz, sz);
    if (!dict) {
        djvu_errorf(doc->ctx, DJVU_SEVERITY_WARNING,
                    "JB2 dict preload failed (INCL %s)", incl_id);
        return;
    }
    jb2_dict_cache_add(doc, incl_id, dict);
}

static void preload_jb2_inline_page(djvu_doc *doc, djvu_page_int *pg)
{
    uint32_t sz;
    const uint8_t *djbz;

    if (!doc || !pg || !(pg->chunk_flags & DJVU_PG_DJBZ)) return;
    djbz = djvu_form_find_chunk(doc, pg->form_off, "Djbz", &sz, NULL);
    if (djbz)
        (void)jb2_inline_find_or_decode(doc, djbz, sz);
}

static int jb2_dict_is_shared(const djvu_doc *doc, const jb2_image *dict)
{
    int i;

    if (!doc || !dict) return 0;
    for (i = 0; i < doc->n_jb2_dicts; i++)
        if (doc->jb2_dicts[i].dict == dict) return 1;
    for (i = 0; i < doc->n_jb2_inline; i++)
        if (doc->jb2_inline[i].dict == dict) return 1;
    return 0;
}

static jb2_image *decode_jb2_mask_fresh(djvu_doc *doc, djvu_page_int *pg)
{
    uint32_t sz;
    const uint8_t *sjbz;
    jb2_image *dict, *mask;

    sjbz = djvu_form_find_chunk(doc, pg->form_off, "Sjbz", &sz, NULL);
    if (!sjbz) return NULL;
    djvu_dict_lock(doc->ctx);
    dict = jb2_dict_for_form_unlocked(doc, pg->form_off);
    djvu_dict_unlock(doc->ctx);
    mask = djvu_jb2_decode(doc->ctx, sjbz, sz, dict);
    return mask;
}

static jb2_image *jb2_inline_find(djvu_doc *doc, const uint8_t *djbz, uint32_t sz)
{
    int i;

    if (!doc || !djbz) return NULL;
    for (i = 0; i < doc->n_jb2_inline; i++) {
        if (doc->jb2_inline[i].djbz_sz == sz &&
            memcmp(doc->jb2_inline[i].djbz, djbz, sz) == 0)
            return doc->jb2_inline[i].dict;
    }
    return NULL;
}

static jb2_image *jb2_inline_find_or_decode(djvu_doc *doc, const uint8_t *djbz,
                                            uint32_t sz)
{
    djvu_jb2_inline_entry *e;
    jb2_image *dict;
    int n;

    dict = jb2_inline_find(doc, djbz, sz);
    if (dict) return dict;

    dict = djvu_jb2_decode_dict(doc->ctx, djbz, sz);
    if (!dict) return NULL;
    n = doc->n_jb2_inline + 1;
    e = (djvu_jb2_inline_entry *)djvu_alloc(doc->ctx, sizeof(djvu_jb2_inline_entry) * n);
    if (!e) {
        djvu_jb2_free(doc->ctx, dict);
        return NULL;
    }
    if (doc->jb2_inline) {
        memcpy(e, doc->jb2_inline, sizeof(djvu_jb2_inline_entry) * doc->n_jb2_inline);
        djvu_free(doc->ctx, doc->jb2_inline);
    }
    doc->jb2_inline = e;
    doc->jb2_inline[doc->n_jb2_inline].djbz = djbz;
    doc->jb2_inline[doc->n_jb2_inline].djbz_sz = sz;
    doc->jb2_inline[doc->n_jb2_inline].dict = dict;
    doc->n_jb2_inline = n;
    return dict;
}

static void djvu_doc_preload_shared_range(djvu_doc *doc, int lo0, int hi0)
{
    int i;

    if (!doc) return;
    if (lo0 < 0) lo0 = 0;
    if (hi0 >= doc->npages) hi0 = doc->npages - 1;
    if (lo0 > hi0) return;
    djvu_dict_lock(doc->ctx);
    for (i = 0; i < doc->n_shared_incl; i++)
        preload_jb2_dict_incl(doc, doc->shared_incl_ids[i]);
    for (i = lo0; i <= hi0; i++)
        preload_jb2_inline_page(doc, &doc->pages[i]);
    djvu_dict_unlock(doc->ctx);
}

void djvu_doc_preload_jb2_range(djvu_doc *doc, int lo0, int hi0)
{
    djvu_doc_preload_shared_range(doc, lo0, hi0);
}

static void preload_jb2_mask(djvu_doc *doc, djvu_page_int *pg)
{
    uint32_t sz;
    const uint8_t *sjbz;
    jb2_image *dict, *mask;

    if (djvu_aborted(doc->ctx)) return;
    if (!djvu_cache_stores_page(doc->ctx)) return;
    if (!doc || !pg || pg->jb2_mask) return;
    sjbz = djvu_form_find_chunk(doc, pg->form_off, "Sjbz", &sz, NULL);
    if (!sjbz) return;
    dict = jb2_dict_for_form_unlocked(doc, pg->form_off);
    mask = djvu_jb2_decode(doc->ctx, sjbz, sz, dict);
    if (!mask) {
        djvu_errorf(doc->ctx, DJVU_SEVERITY_WARNING,
                    "JB2 mask preload failed (form %u)", pg->form_off);
        return;
    }
    pg->jb2_mask = mask;
}

void djvu_doc_preload_jb2_masks_range(djvu_doc *doc, int lo0, int hi0)
{
    int i;

    if (!doc || !djvu_cache_stores_page(doc->ctx)) return;
    if (lo0 < 0) lo0 = 0;
    if (hi0 >= doc->npages) hi0 = doc->npages - 1;
    if (lo0 > hi0) return;
    djvu_cache_lock(doc->ctx);
    for (i = lo0; i <= hi0; i++)
        preload_jb2_mask(doc, &doc->pages[i]);
    djvu_cache_unlock(doc->ctx);
}

jb2_image *djvu_doc_jb2_mask_acquire(djvu_doc *doc, int page_no, int *owned_out)
{
    djvu_page_int *pg;
    jb2_image *mask;

    if (owned_out) *owned_out = 0;
    if (!doc || page_no < 0 || page_no >= doc->npages) return NULL;
    pg = &doc->pages[page_no];

    if (!djvu_cache_stores_page(doc->ctx)) {
        mask = decode_jb2_mask_fresh(doc, pg);
        if (mask && owned_out) *owned_out = 1;
        return mask;
    }
    if (pg->jb2_mask) return pg->jb2_mask;
    djvu_cache_lock(doc->ctx);
    if (!pg->jb2_mask)
        preload_jb2_mask(doc, pg);
    djvu_cache_unlock(doc->ctx);
    return pg->jb2_mask;
}

void djvu_doc_jb2_mask_release(djvu_doc *doc, jb2_image *mask, int owned)
{
    djvu_ctx *ctx;
    jb2_image *dict;

    if (!owned || !mask || !doc) return;
    ctx = doc->ctx;
    dict = mask->inherited_dict;
    djvu_jb2_free(ctx, mask);
    if (dict && !jb2_dict_is_shared(doc, dict))
        djvu_jb2_free(ctx, dict);
}

jb2_image *djvu_doc_jb2_mask(djvu_doc *doc, int page_no)
{
    int owned = 0;
    jb2_image *mask = djvu_doc_jb2_mask_acquire(doc, page_no, &owned);
    if (owned) {
        djvu_doc_jb2_mask_release(doc, mask, 1);
        return NULL;
    }
    return mask;
}

static void free_jb2_inline_cache(djvu_ctx *ctx, djvu_doc *doc)
{
    int i;
    if (!doc || !doc->jb2_inline) return;
    for (i = 0; i < doc->n_jb2_inline; i++)
        djvu_jb2_free(ctx, doc->jb2_inline[i].dict);
    djvu_free(ctx, doc->jb2_inline);
    doc->jb2_inline = NULL;
    doc->n_jb2_inline = 0;
}

static void free_jb2_dict_cache(djvu_ctx *ctx, djvu_doc *doc)
{
    int i;
    if (!doc || !doc->jb2_dicts) return;
    for (i = 0; i < doc->n_jb2_dicts; i++) {
        djvu_jb2_free(ctx, doc->jb2_dicts[i].dict);
        djvu_free(ctx, doc->jb2_dicts[i].incl_id);
    }
    djvu_free(ctx, doc->jb2_dicts);
    doc->jb2_dicts = NULL;
    doc->n_jb2_dicts = 0;
}

jb2_image *djvu_doc_jb2_dict(djvu_doc *doc, const char *incl_id)
{
    return jb2_dict_find(doc, incl_id);
}

jb2_image *djvu_doc_jb2_dict_inline(djvu_doc *doc, uint32_t form_off)
{
    uint32_t sz;
    const uint8_t *djbz;

    if (!doc) return NULL;
    djbz = djvu_form_find_chunk(doc, form_off, "Djbz", &sz, NULL);
    if (!djbz) return NULL;
    return jb2_inline_find(doc, djbz, sz);
}

static jb2_image *jb2_dict_for_form_unlocked(djvu_doc *doc, uint32_t form_off)
{
    uint32_t start = 0, incl_sz, chunk_sz;
    const uint8_t *incl, *djbz;
    jb2_image *dict;

    if (!doc) return NULL;
    djbz = djvu_form_find_chunk(doc, form_off, "Djbz", &chunk_sz, NULL);
    if (djbz)
        return jb2_inline_find_or_decode(doc, djbz, chunk_sz);
    while ((incl = djvu_form_find_chunk(doc, form_off, "INCL", &incl_sz, &start)) != NULL) {
        char id[64];
        size_t n = incl_sz < sizeof(id) - 1 ? incl_sz : sizeof(id) - 1;
        memcpy(id, incl, n);
        id[n] = 0;
        djvu_trim_incl_id(id);
        dict = jb2_dict_find(doc, id);
        if (dict) return dict;
        preload_jb2_dict_incl(doc, id);
        dict = jb2_dict_find(doc, id);
        if (dict) return dict;
    }
    return NULL;
}

jb2_image *djvu_doc_jb2_dict_for_form(djvu_doc *doc, uint32_t form_off)
{
    jb2_image *dict;
    if (!doc) return NULL;
    djvu_dict_lock(doc->ctx);
    dict = jb2_dict_for_form_unlocked(doc, form_off);
    djvu_dict_unlock(doc->ctx);
    return dict;
}

uint32_t djvu_doc_component_offset(djvu_doc *doc, const char *id)
{
    int i;
    if (!doc || !id) return 0;
    for (i = 0; i < doc->ncomp; i++)
        if (doc->comps[i].id && strcmp(doc->comps[i].id, id) == 0)
            return doc->comps[i].offset;
    return 0;
}

void djvu_trim_incl_id(char *s)
{
    size_t n = strlen(s);
    while (n > 0 && (s[n - 1] == '\n' || s[n - 1] == '\r' ||
                     s[n - 1] == ' ' || s[n - 1] == '\t' || s[n - 1] == 0x1a))
        s[--n] = 0;
}

const uint8_t *djvu_form_find_incl_chunk(djvu_doc *doc, uint32_t form_off,
                                         const char *chunk_id, uint32_t *out_size)
{
    uint32_t start = 0, incl_sz, chunk_sz;
    const uint8_t *incl;

    while ((incl = djvu_form_find_chunk(doc, form_off, "INCL", &incl_sz, &start)) != NULL) {
        char id[64];
        uint32_t coff;
        const uint8_t *chunk;
        size_t n = incl_sz < sizeof(id) - 1 ? incl_sz : sizeof(id) - 1;

        memcpy(id, incl, n);
        id[n] = 0;
        djvu_trim_incl_id(id);
        coff = djvu_doc_component_offset(doc, id);
        if (!coff) continue;
        chunk = djvu_form_find_chunk(doc, coff, chunk_id, &chunk_sz, NULL);
        if (chunk) {
            if (out_size) *out_size = chunk_sz;
            return chunk;
        }
    }
    return NULL;
}

const uint8_t *djvu_form_find_chunk(djvu_doc *doc, uint32_t form_off,
                                    const char *id, uint32_t *out_size,
                                    uint32_t *start)
{
    const uint8_t *data = doc->data;
    size_t len = doc->len;
    uint32_t form_size, form_end, pos;

    if ((size_t)form_off + 12 > len || !djvu_tag_eq(data + form_off, "FORM"))
        return NULL;
    form_size = djvu_rd_u32be(data + form_off + 4);
    form_end = form_off + 8 + form_size;
    if (form_end > len) form_end = (uint32_t)len;

    pos = start && *start ? *start : form_off + 12;
    while (pos + 8 <= form_end) {
        const uint8_t *cid = data + pos;
        uint32_t csize = djvu_rd_u32be(data + pos + 4);
        uint32_t cdata = pos + 8;
        uint32_t next;
        if (csize > form_end - cdata) csize = form_end - cdata;
        next = cdata + csize + (csize & 1);
        if (djvu_tag_eq(cid, id)) {
            if (out_size) *out_size = csize;
            if (start) *start = next;
            return data + cdata;
        }
        pos = next;
    }
    return NULL;
}

djvu_doc *djvu_doc_open(djvu_ctx *ctx, const uint8_t *data, size_t len)
{
    djvu_doc *doc;
    uint32_t pos = 0;
    uint32_t form_size, form_end;
    const uint8_t *form_type;

    if (!ctx || !data || len < 16) return NULL;


    if (djvu_tag_eq(data, "AT&T"))
        pos = 4;

    if (pos + 12 > len || !djvu_tag_eq(data + pos, "FORM")) {
        djvu_errorf(ctx, DJVU_SEVERITY_ERROR, "not a DjVu file (no FORM)");
        return NULL;
    }
    form_size = djvu_rd_u32be(data + pos + 4);
    form_type = data + pos + 8;
    form_end = pos + 8 + form_size;
    if (form_end > len) form_end = (uint32_t)len;

    doc = (djvu_doc *)djvu_alloc(ctx, sizeof(djvu_doc));
    if (!doc) return NULL;
    memset(doc, 0, sizeof(*doc));
    doc->ctx = ctx;
    doc->data = data;
    doc->len = len;
    doc->root_form_off = pos;

    if (djvu_tag_eq(form_type, "DJVU")) {

        doc->pages = (djvu_page_int *)djvu_alloc(ctx, sizeof(djvu_page_int));
        if (!doc->pages) { djvu_free(ctx, doc); return NULL; }
        memset(doc->pages, 0, sizeof(djvu_page_int));
        doc->pages[0].form_off = pos;
        doc->pages[0].form_size = form_size;
        doc->npages = 1;
    } else if (djvu_tag_eq(form_type, "DJVM")) {

        uint32_t p = pos + 12;
        int found = 0;
        while (p + 8 <= form_end) {
            const uint8_t *id = data + p;
            uint32_t csize = djvu_rd_u32be(data + p + 4);
            uint32_t cdata = p + 8;
            if (csize > form_end - cdata) csize = form_end - cdata;
            if (djvu_tag_eq(id, "DIRM")) {
                if (load_djvm(doc, cdata, csize) != 0) {
                    djvu_errorf(ctx, DJVU_SEVERITY_ERROR, "bad DIRM directory");
                    djvu_doc_close(doc);
                    return NULL;
                }
                found = 1;
                break;
            }
            p = cdata + csize;
            if (csize & 1) p++;
        }
        if (!found) {
            djvu_errorf(ctx, DJVU_SEVERITY_ERROR, "DJVM without DIRM");
            djvu_doc_close(doc);
            return NULL;
        }
    } else {
        djvu_errorf(ctx, DJVU_SEVERITY_ERROR, "unsupported FORM type");
        djvu_doc_close(doc);
        return NULL;
    }

    {
        int i;
        for (i = 0; i < doc->npages; i++)
            page_load_info(doc, &doc->pages[i]);
    }
    doc_build_cache_index(doc);
    if (ctx->cache_per_page && (!ctx->lock || !ctx->unlock)) {
        djvu_errorf(ctx, DJVU_SEVERITY_ERROR,
                    "per-page caching requires lock and unlock callbacks");
        djvu_doc_close(doc);
        return NULL;
    }

    djvu_scaler_init();

    if (!djvu_has_lock(ctx))
        djvu_doc_preload_shared_range(doc, 0, doc->npages - 1);
    return doc;
}

void djvu_doc_close(djvu_doc *doc)
{
    int i;
    if (!doc) return;
    if (doc->comps) {
        for (i = 0; i < doc->ncomp; i++) {
            djvu_free(doc->ctx, doc->comps[i].id);
            djvu_free(doc->ctx, doc->comps[i].title);
        }
        djvu_free(doc->ctx, doc->comps);
    }
    free_jb2_dict_cache(doc->ctx, doc);
    free_jb2_inline_cache(doc->ctx, doc);
    free_shared_incl_index(doc->ctx, doc);
    if (doc->pages) {
        for (i = 0; i < doc->npages; i++) {
            free_page_bg_native(doc->ctx, &doc->pages[i]);
            free_page_jb2_mask(doc->ctx, &doc->pages[i]);
            free_page_iw44(&doc->pages[i]);
        }
        djvu_free(doc->ctx, doc->pages);
    }
    djvu_free(doc->ctx, doc);
}

int djvu_doc_page_count(djvu_doc *doc)
{
    return doc ? doc->npages : 0;
}

int djvu_doc_page_info(djvu_doc *doc, int page_no, djvu_page_info *info)
{
    djvu_page_int *pg;
    if (!doc || !info || page_no < 0 || page_no >= doc->npages) return -1;
    pg = &doc->pages[page_no];
    if (page_load_info(doc, pg) != 0) return -1;
    *info = pg->info;
    return 0;
}

const char *djvu_doc_page_id(djvu_doc *doc, int page_no)
{
    if (!doc || page_no < 0 || page_no >= doc->npages) return NULL;
    return doc->pages[page_no].id;
}

const char *djvu_doc_page_title(djvu_doc *doc, int page_no)
{
    if (!doc || page_no < 0 || page_no >= doc->npages) return NULL;
    return doc->pages[page_no].title;
}

int djvu_doc_page_by_name(djvu_doc *doc, const char *name)
{
    int i;
    if (!doc || !name) return -1;
    if (name[0] == '#') name++;
    for (i = 0; i < doc->npages; i++) {
        const char *id = doc->pages[i].id;
        if (id && strcmp(id, name) == 0) return i;
    }
    return -1;
}

djvu_page_type djvu_page_get_type(djvu_doc *doc, int page_no)
{
    uint32_t form_off, sz;
    int has_mask, has_bg, has_fg;
    if (!doc || page_no < 0 || page_no >= doc->npages) return DJVU_PAGE_UNKNOWN;
    form_off = doc->pages[page_no].form_off;
    has_mask = djvu_form_find_chunk(doc, form_off, "Sjbz", &sz, NULL) != NULL;
    has_bg   = djvu_form_find_chunk(doc, form_off, "BG44", &sz, NULL) != NULL;
    has_fg   = djvu_form_find_chunk(doc, form_off, "FG44", &sz, NULL) != NULL ||
               djvu_form_find_chunk(doc, form_off, "FGbz", &sz, NULL) != NULL;
    if (has_mask && (has_bg || has_fg)) return DJVU_PAGE_COMPOUND;
    if (has_mask) return DJVU_PAGE_BITONAL;
    if (has_bg || has_fg) return DJVU_PAGE_PHOTO;
    return DJVU_PAGE_UNKNOWN;
}

#include <string.h>

char *djvu_br_strdup(djvu_buf_reader *br, int slen)
{
    char *s;
    if (slen < 0 || br->pos + (size_t)slen > br->len) { br->failed = 1; return NULL; }
    s = (char *)djvu_alloc(br->ctx, (size_t)slen + 1);
    if (!s) { br->failed = 1; return NULL; }
    memcpy(s, br->p + br->pos, (size_t)slen);
    s[slen] = 0;
    br->pos += (size_t)slen;
    return s;
}

#include <stdlib.h>
#include <string.h>

static int rotation_quarter_turns(int rotation)
{
    if (rotation == 90) return 3;
    if (rotation == 180) return 2;
    if (rotation == 270) return 1;
    return 0;
}

#define ROT_TILE 32
static djvu_image *image_rotate_cw(djvu_ctx *ctx, djvu_image *src, int k)
{
    int comp = (int)src->format;
    int sw = src->width, sh = src->height;
    int dw = (k == 2) ? sw : sh;
    int dh = (k == 2) ? sh : sw;
    size_t srow = (size_t)sw * comp;
    const uint8_t *S;
    djvu_image *d = (djvu_image *)djvu_alloc(ctx, sizeof(djvu_image));
    int bx, by, x, y;
    if (!d) return NULL;
    d->width = dw; d->height = dh; d->format = src->format; d->stride = dw * comp;
    d->data = (uint8_t *)djvu_alloc(ctx, (size_t)dw * dh * comp);
    if (!d->data) { djvu_free(ctx, d); return NULL; }
    S = src->data;
    for (by = 0; by < dh; by += ROT_TILE) {
        int ymax = by + ROT_TILE < dh ? by + ROT_TILE : dh;
        for (bx = 0; bx < dw; bx += ROT_TILE) {
            int xmax = bx + ROT_TILE < dw ? bx + ROT_TILE : dw;
            for (y = by; y < ymax; y++) {
                uint8_t *dp = d->data + ((size_t)y * dw + bx) * comp;
                const uint8_t *sp;
                if (k == 1) {
                    sp = S + ((size_t)(sh - 1 - bx) * sw + y) * comp;
                    if (comp == 1)
                        for (x = bx; x < xmax; x++) { *dp++ = *sp; sp -= srow; }
                    else
                        for (x = bx; x < xmax; x++) {
                            dp[0] = sp[0]; dp[1] = sp[1]; dp[2] = sp[2];
                            dp += 3; sp -= srow;
                        }
                } else if (k == 3) {
                    sp = S + ((size_t)bx * sw + (sw - 1 - y)) * comp;
                    if (comp == 1)
                        for (x = bx; x < xmax; x++) { *dp++ = *sp; sp += srow; }
                    else
                        for (x = bx; x < xmax; x++) {
                            dp[0] = sp[0]; dp[1] = sp[1]; dp[2] = sp[2];
                            dp += 3; sp += srow;
                        }
                } else {
                    sp = S + ((size_t)(sh - 1 - y) * sw + (sw - 1 - bx)) * comp;
                    if (comp == 1)
                        for (x = bx; x < xmax; x++) { *dp++ = *sp--; }
                    else
                        for (x = bx; x < xmax; x++) {
                            dp[0] = sp[0]; dp[1] = sp[1]; dp[2] = sp[2];
                            dp += 3; sp -= 3;
                        }
                }
            }
        }
    }
    return d;
}

static djvu_image *render_blank(djvu_ctx *ctx, const djvu_page_info *pi, int subsample)
{
    djvu_image *out;
    int sw, sh;

    if (pi->width <= 0 || pi->height <= 0) return NULL;
    sw = (pi->width + subsample - 1) / subsample;
    sh = (pi->height + subsample - 1) / subsample;
    out = (djvu_image *)djvu_alloc(ctx, sizeof(djvu_image));
    if (!out) return NULL;
    out->width = sw;
    out->height = sh;
    out->format = DJVU_FORMAT_GRAY8;
    out->stride = sw;
    out->data = (uint8_t *)djvu_alloc(ctx, (size_t)sw * sh);
    if (!out->data) { djvu_free(ctx, out); return NULL; }
    memset(out->data, 255, (size_t)sw * sh);
    return out;
}

typedef struct {
    uint8_t *dst;
    int w, h;
} bitonal_stamp_ctx;

static void bitonal_stamp_run(void *user, int x0, int x1, int py)
{
    bitonal_stamp_ctx *c = (bitonal_stamp_ctx *)user;
    int ty;

    if (py < 0 || py >= c->h) return;
    if (x0 < 0) x0 = 0;
    if (x1 > c->w) x1 = c->w;
    if (x0 >= x1) return;
    ty = c->h - 1 - py;
    memset(c->dst + (size_t)ty * (size_t)c->w + (size_t)x0, 0,
           (size_t)(x1 - x0));
}

typedef struct {
    uint8_t *acc;
    int sw, sh;
    int h;
    int sub;
} bitonal_acc_ctx;

static void bitonal_accum_ink(void *user, int px, int py)
{
    bitonal_acc_ctx *c = (bitonal_acc_ctx *)user;
    int cx, cy;
    size_t cell;

    if (px < 0 || py < 0 || py >= c->h) return;
    cx = px / c->sub;
    cy = c->sh - 1 - py / c->sub;
    if (cx >= c->sw || cy < 0) return;
    cell = (size_t)cy * c->sw + cx;
    if (c->acc[cell] < 255) c->acc[cell]++;
}

static djvu_image *render_bitonal(djvu_ctx *ctx, jb2_image *img, int subsample)
{
    djvu_image *out;
    int sw, sh, i;

    sw = (img->width + subsample - 1) / subsample;
    sh = (img->height + subsample - 1) / subsample;
    out = (djvu_image *)djvu_alloc(ctx, sizeof(djvu_image));
    if (!out) return NULL;
    out->width = sw;
    out->height = sh;
    out->format = DJVU_FORMAT_GRAY8;
    out->stride = sw;
    out->data = (uint8_t *)djvu_alloc(ctx, (size_t)sw * sh);
    if (!out->data) { djvu_free(ctx, out); return NULL; }

    if (subsample == 1) {
        bitonal_stamp_ctx stamp = { out->data, img->width, img->height };
        memset(out->data, 255, (size_t)sw * sh);
        for (i = 0; i < img->nblits; i++) {
            jb2_blit *b = &img->blits[i];
            jb2_shape *s = djvu_jb2_get_shape(img, b->shapeno);
            if ((i & 63) == 0 && djvu_aborted(ctx)) {
                djvu_free(ctx, out->data);
                djvu_free(ctx, out);
                return NULL;
            }
            if (s && djvu_bm_has_pixels(&s->bm))
                djvu_bm_visit_ink_runs(&s->bm, b->left, b->bottom,
                                        bitonal_stamp_run, &stamp);
        }
        return out;
    }


    {
        uint8_t *acc;
        bitonal_acc_ctx c;
        unsigned char lut[256];
        int sub2 = subsample * subsample;
        size_t n = (size_t)sw * sh, k;

        if (sub2 > 255) sub2 = 255;
        acc = (uint8_t *)djvu_alloc(ctx, n);
        if (!acc) { djvu_free(ctx, out->data); djvu_free(ctx, out); return NULL; }
        memset(acc, 0, n);
        for (k = 0; k < 256; k++) {
            int cov = (int)k < sub2 ? (int)k : sub2;
            lut[k] = (unsigned char)(255 - cov * 255 / sub2);
        }
        c.acc = acc; c.sw = sw; c.sh = sh; c.h = img->height; c.sub = subsample;
        for (i = 0; i < img->nblits; i++) {
            jb2_blit *b = &img->blits[i];
            jb2_shape *s = djvu_jb2_get_shape(img, b->shapeno);
            if ((i & 63) == 0 && djvu_aborted(ctx)) {
                djvu_free(ctx, acc);
                djvu_free(ctx, out->data);
                djvu_free(ctx, out);
                return NULL;
            }
            if (s && djvu_bm_has_pixels(&s->bm))
                djvu_bm_visit_ink(&s->bm, b->left, b->bottom,
                                  bitonal_accum_ink, &c);
        }
        for (k = 0; k < n; k++) out->data[k] = lut[acc[k]];
        djvu_free(ctx, acc);
    }
    return out;
}

static djvu_image *apply_page_rotation(djvu_ctx *ctx, djvu_doc *doc, int page_no,
                                       djvu_image *img, int subsample,
                                       djvu_render_timings *t)
{
    djvu_page_info pi;
    int k;
    djvu_image *r;
    double t0 = 0.0;

    (void)subsample;
    if (!img) return img;
    if (djvu_doc_page_info(doc, page_no, &pi) != 0 || pi.rotation == 0) return img;
    k = rotation_quarter_turns(pi.rotation);
    if (!k) return img;
    if (t) t0 = djvu_bench_now_ms();
    r = image_rotate_cw(ctx, img, k);
    if (t) t->rotate_ms += djvu_bench_now_ms() - t0;
    if (r) { djvu_image_destroy(ctx, img); return r; }
    return img;
}

static djvu_image *page_render_timed_impl(djvu_doc *doc, int page_no, int subsample,
                                          djvu_render_timings *t)
{
    djvu_ctx *ctx;
    uint32_t form_off, sz;
    djvu_page_type type;
    djvu_page_info pi;
    int info_ok;
    jb2_image *mask = NULL;
    int mask_owned = 0;
    djvu_image *out = NULL;
    double t0 = 0.0;

    if (!doc || page_no < 0 || page_no >= doc->npages) return NULL;
    ctx = doc->ctx;
    if (subsample < 1) subsample = 1;
    if (t) djvu_render_timings_clear(t);
    form_off = doc->pages[page_no].form_off;
    type = djvu_page_get_type(doc, page_no);
    info_ok = (djvu_doc_page_info(doc, page_no, &pi) == 0);


    if (type == DJVU_PAGE_BITONAL || type == DJVU_PAGE_COMPOUND) {
        if (!djvu_form_find_chunk(doc, form_off, "Sjbz", &sz, NULL))
            goto done;
        if (t) t0 = djvu_bench_now_ms();
        mask = djvu_doc_jb2_mask_acquire(doc, page_no, &mask_owned);
        if (t) t->jb2_ms += djvu_bench_now_ms() - t0;
        if (!mask || djvu_aborted(ctx)) goto done;
    }


    if (!out && info_ok && !ctx->no_compose &&
        (type == DJVU_PAGE_COMPOUND || type == DJVU_PAGE_PHOTO) &&
        djvu_form_find_chunk(doc, form_off, "BG44", &sz, NULL) != NULL) {
        out = djvu_compose_page(doc, page_no, mask, pi.width, pi.height, subsample, t);
        if (out) goto done;
    }

    if (!out && type == DJVU_PAGE_UNKNOWN) {
        if (info_ok)
            out = render_blank(ctx, &pi, subsample);
        else
            djvu_errorf(ctx, DJVU_SEVERITY_ERROR, "page %d: nothing to render", page_no);
        goto done;
    }

    if (!out && mask) {
        if (t) t0 = djvu_bench_now_ms();
        out = render_bitonal(ctx, mask, subsample);
        if (t) t->composite_ms += djvu_bench_now_ms() - t0;
    }

done:
    djvu_doc_jb2_mask_release(doc, mask, mask_owned);
    return apply_page_rotation(ctx, doc, page_no, out, subsample, t);
}

djvu_image *djvu_page_render_timed(djvu_doc *doc, int page_no, int subsample,
                                   djvu_render_timings *t)
{
    djvu_image *out;

    if (!doc) return NULL;
    djvu_render_begin(doc->ctx, NULL);
    out = page_render_timed_impl(doc, page_no, subsample, t);
    djvu_render_end();
    return out;
}

djvu_image *djvu_page_render(djvu_doc *doc, int page_no, int subsample)
{
    return djvu_page_render_timed(doc, page_no, subsample, NULL);
}

djvu_image *djvu_page_render_abortable(djvu_doc *doc, int page_no, int subsample,
                                       const djvu_abort *ab)
{
    djvu_image *out;

    if (!doc) return NULL;
    djvu_render_begin(doc->ctx, ab);
    out = page_render_timed_impl(doc, page_no, subsample, NULL);
    djvu_render_end();
    return out;
}

static int render_plan(djvu_doc *doc, int page_no, int subsample,
                       int *pw, int *ph, djvu_format *pfmt, int *pcolor,
                       int *protation)
{
    djvu_ctx *ctx = doc->ctx;
    uint32_t form_off, sz;
    djvu_page_type type;
    djvu_page_info pi;
    int info_ok, has_bg, has_mask, color, w, h, k;
    djvu_format fmt;

    if (!doc || page_no < 0 || page_no >= doc->npages) return -1;
    if (subsample < 1) subsample = 1;
    form_off = doc->pages[page_no].form_off;
    type = djvu_page_get_type(doc, page_no);
    info_ok = (djvu_doc_page_info(doc, page_no, &pi) == 0);
    if (!info_ok || pi.width <= 0 || pi.height <= 0) return -1;
    has_bg = djvu_form_find_chunk(doc, form_off, "BG44", &sz, NULL) != NULL;
    has_mask = djvu_form_find_chunk(doc, form_off, "Sjbz", &sz, NULL) != NULL;

    color = !ctx->no_compose &&
            (type == DJVU_PAGE_COMPOUND || type == DJVU_PAGE_PHOTO) && has_bg;

    if (color) {
        w = (pi.width + subsample - 1) / subsample;
        h = (pi.height + subsample - 1) / subsample;
        fmt = DJVU_FORMAT_RGB24;
    } else if (type == DJVU_PAGE_UNKNOWN || has_mask) {
        w = (pi.width + subsample - 1) / subsample;
        h = (pi.height + subsample - 1) / subsample;
        fmt = DJVU_FORMAT_GRAY8;
    } else {
        return -1;
    }

    k = rotation_quarter_turns(pi.rotation);
    if (k == 1 || k == 3) { int tmp = w; w = h; h = tmp; }

    *pw = w; *ph = h; *pfmt = fmt; *pcolor = color; *protation = pi.rotation;
    return 0;
}

int djvu_page_render_info(djvu_doc *doc, int page_no, int subsample,
                          djvu_render_info *info)
{
    int w, h, color, rotation;
    djvu_format fmt;
    if (!info) return -1;
    if (render_plan(doc, page_no, subsample, &w, &h, &fmt, &color, &rotation) != 0)
        return -1;
    info->width = w;
    info->height = h;
    info->format = fmt;
    return 0;
}

static int blit_image_into(djvu_image *img, uint8_t *dst, int stride,
                           int w, int h, djvu_format fmt)
{
    int comp = (fmt == DJVU_FORMAT_GRAY8) ? 1 : 3;
    size_t rowbytes = (size_t)w * comp;
    int y;
    if (img->width != w || img->height != h || img->format != fmt) return -1;
    for (y = 0; y < h; y++)
        memcpy(dst + (size_t)y * stride, img->data + (size_t)y * img->stride, rowbytes);
    return 0;
}

static int page_render_into_impl(djvu_doc *doc, int page_no, int subsample,
                                 uint8_t *dst, int stride)
{
    djvu_ctx *ctx;
    int w, h, color, rotation, k, rc;
    djvu_format fmt;
    djvu_image *img;

    if (render_plan(doc, page_no, subsample, &w, &h, &fmt, &color, &rotation) != 0)
        return -1;
    if (subsample < 1) subsample = 1;
    ctx = doc->ctx;
    k = rotation_quarter_turns(rotation);


    if (color && k == 0) {
        uint32_t form_off = doc->pages[page_no].form_off;
        uint32_t sz;
        jb2_image *mask = NULL;
        int mask_owned = 0;
        djvu_page_info pi;

        if (djvu_doc_page_info(doc, page_no, &pi) != 0)
            return -1;
        if (djvu_form_find_chunk(doc, form_off, "Sjbz", &sz, NULL)) {
            mask = djvu_doc_jb2_mask_acquire(doc, page_no, &mask_owned);
            if (!mask)
                return -1;
        }
        rc = djvu_compose_page_into(doc, page_no, mask, pi.width, pi.height,
                                    subsample, dst, stride);
        djvu_doc_jb2_mask_release(doc, mask, mask_owned);
        return rc;
    }


    img = page_render_timed_impl(doc, page_no, subsample, NULL);
    if (!img) return -1;
    rc = blit_image_into(img, dst, stride, w, h, fmt);
    djvu_image_destroy(ctx, img);
    return rc;
}

int djvu_page_render_into(djvu_doc *doc, int page_no, int subsample,
                          uint8_t *dst, int stride)
{
    int rc;

    if (!dst || !doc || !doc->ctx) return -1;
    djvu_render_begin(doc->ctx, NULL);
    rc = page_render_into_impl(doc, page_no, subsample, dst, stride);
    djvu_render_end();
    return rc;
}

int djvu_page_render_into_abortable(djvu_doc *doc, int page_no, int subsample,
                                    uint8_t *dst, int stride,
                                    const djvu_abort *ab)
{
    int rc;

    if (!dst || !doc || !doc->ctx) return -1;
    djvu_render_begin(doc->ctx, ab);
    rc = page_render_into_impl(doc, page_no, subsample, dst, stride);
    djvu_render_end();
    return rc;
}

void djvu_image_destroy(djvu_ctx *ctx, djvu_image *img)
{
    if (img) {
        djvu_free(ctx, img->data);
        djvu_free(ctx, img);
    }
}

#include <string.h>

static int load_text_payload(djvu_doc *doc, int page_no, uint8_t **owned,
                             const uint8_t **payload, size_t *plen)
{
    djvu_ctx *ctx = doc->ctx;
    uint32_t form_off, sz;
    const uint8_t *chunk;

    *owned = NULL;
    form_off = doc->pages[page_no].form_off;

    chunk = djvu_form_find_chunk(doc, form_off, "TXTz", &sz, NULL);
    if (chunk) {
        size_t dlen;
        uint8_t *dec = djvu_bzz_decode_all(ctx, chunk, sz, &dlen);
        if (!dec) return -1;
        *owned = dec; *payload = dec; *plen = dlen;
        return 0;
    }
    chunk = djvu_form_find_chunk(doc, form_off, "TXTa", &sz, NULL);
    if (!chunk) return -1;
    *payload = chunk; *plen = sz;
    return 0;
}

char *djvu_page_text(djvu_doc *doc, int page_no)
{
    djvu_ctx *ctx;
    uint8_t *owned = NULL;
    const uint8_t *payload;
    size_t plen;
    uint32_t tlen;
    char *out;

    if (!doc || page_no < 0 || page_no >= doc->npages) return NULL;
    ctx = doc->ctx;
    if (load_text_payload(doc, page_no, &owned, &payload, &plen) != 0) return NULL;

    if (plen < 3) { djvu_free(ctx, owned); return NULL; }
    tlen = djvu_rd_u24be(payload);
    if ((size_t)tlen + 3 > plen) tlen = (uint32_t)(plen - 3);

    out = (char *)djvu_alloc(ctx, tlen + 1);
    if (out) {
        memcpy(out, payload + 3, tlen);
        out[tlen] = 0;
    }
    djvu_free(ctx, owned);
    return out;
}

void djvu_text_destroy(djvu_ctx *ctx, char *text)
{
    djvu_free(ctx, text);
}

typedef struct {
    djvu_buf_reader br;
    const char *text;
    size_t textlen;
} zparse;

static char *zone_text(zparse *z, int off, int len)
{
    char *s;
    int avail;
    if (off < 0) off = 0;
    if (off > (int)z->textlen) off = (int)z->textlen;
    avail = (int)z->textlen - off;
    if (len < 0) len = 0;
    if (len > avail) len = avail;
    s = (char *)djvu_alloc(z->br.ctx, (size_t)len + 1);
    if (!s) return NULL;
    memcpy(s, z->text + off, (size_t)len);
    s[len] = 0;
    return s;
}

static void free_zone_kids(djvu_ctx *ctx, djvu_text_zone *z);

static int parse_zone(zparse *z, djvu_text_zone *out,
                      const djvu_text_zone *parent, const djvu_text_zone *sib,
                      int parent_toff, int sib_toff, int sib_tlen,
                      int *out_toff, int *out_tlen)
{
    int type, x, y, w, h, toff, tlen, nkids, i;
    djvu_text_zone *prev = NULL;
    int prev_toff = 0, prev_tlen = 0;

    type = djvu_br_u8(&z->br);
    x = djvu_br_s16be_biased(&z->br);
    y = djvu_br_s16be_biased(&z->br);
    w = djvu_br_s16be_biased(&z->br);
    h = djvu_br_s16be_biased(&z->br);
    toff = djvu_br_s16be_biased(&z->br);
    tlen = djvu_br_u24be(&z->br);
    if (z->br.failed) return -1;


    if (parent == NULL && sib == NULL) {

    } else if (sib == NULL) {
        x += parent->x;
        y = (parent->y + parent->h) - (y + h);
        toff += parent_toff;
    } else {
        if (sib->type == DJVU_ZONE_PAGE || sib->type == DJVU_ZONE_PARAGRAPH ||
            sib->type == DJVU_ZONE_LINE) {
            x += sib->x;
            y = sib->y - (y + h);
        } else if (sib->type == DJVU_ZONE_COLUMN || sib->type == DJVU_ZONE_WORD ||
                   sib->type == DJVU_ZONE_CHAR) {
            x += sib->x + sib->w;
            y += sib->y;
        }

        toff += sib_toff + sib_tlen;
    }

    out->type = (djvu_zone_type)type;
    out->x = x; out->y = y; out->w = w; out->h = h;
    out->text = zone_text(z, toff, tlen);
    out->children = NULL;
    out->nchildren = 0;
    if (out_toff) *out_toff = toff;
    if (out_tlen) *out_tlen = tlen;

    nkids = djvu_br_u24be(&z->br);
    if (z->br.failed || nkids < 0) return -1;
    if (nkids > 0) {
        out->children = (djvu_text_zone *)djvu_alloc(z->br.ctx,
                            sizeof(djvu_text_zone) * (size_t)nkids);
        if (!out->children) return -1;
        memset(out->children, 0, sizeof(djvu_text_zone) * (size_t)nkids);
        for (i = 0; i < nkids; i++) {
            int kt = 0, kl = 0;
            if (parse_zone(z, &out->children[i], out, prev,
                           toff, prev_toff, prev_tlen, &kt, &kl) != 0) {
                out->nchildren = i;
                return -1;
            }
            prev = &out->children[i];
            prev_toff = kt; prev_tlen = kl;
        }
        out->nchildren = nkids;
    }
    return 0;
}

static void flip_zone_y(djvu_text_zone *z, int page_h)
{
    int i;
    z->y = djvu_y_bottomup_to_topdown(z->y, page_h, z->h);
    for (i = 0; i < z->nchildren; i++) flip_zone_y(&z->children[i], page_h);
}

static void free_zone_kids(djvu_ctx *ctx, djvu_text_zone *z)
{
    int i;
    for (i = 0; i < z->nchildren; i++) free_zone_kids(ctx, &z->children[i]);
    djvu_free(ctx, z->children);
    djvu_free(ctx, z->text);
}

djvu_page_text_zones *djvu_page_text_get_zones(djvu_doc *doc, int page_no)
{
    djvu_ctx *ctx;
    uint8_t *owned = NULL;
    const uint8_t *payload;
    size_t plen;
    uint32_t tlen;
    zparse z;
    djvu_page_text_zones *res;
    djvu_page_info info;
    int page_h;

    if (!doc || page_no < 0 || page_no >= doc->npages) return NULL;
    ctx = doc->ctx;
    if (load_text_payload(doc, page_no, &owned, &payload, &plen) != 0) return NULL;
    if (plen < 4) { djvu_free(ctx, owned); return NULL; }

    tlen = djvu_rd_u24be(payload);
    if ((size_t)tlen + 3 > plen) tlen = (uint32_t)(plen - 3);

    res = (djvu_page_text_zones *)djvu_alloc(ctx, sizeof(*res));
    if (!res) { djvu_free(ctx, owned); return NULL; }
    res->text = (char *)djvu_alloc(ctx, tlen + 1);
    res->root = NULL;
    if (!res->text) { djvu_free(ctx, owned); djvu_free(ctx, res); return NULL; }
    memcpy(res->text, payload + 3, tlen);
    res->text[tlen] = 0;


    memset(&z, 0, sizeof(z));
    djvu_br_init(&z.br, ctx, payload, plen);
    z.br.pos = (size_t)3 + tlen + 1;
    z.text = res->text;
    z.textlen = tlen;

    if (z.br.pos < plen) {
        djvu_text_zone *root = (djvu_text_zone *)djvu_alloc(ctx, sizeof(*root));
        if (root) {
            memset(root, 0, sizeof(*root));
            if (parse_zone(&z, root, NULL, NULL, 0, 0, 0, NULL, NULL) == 0) {
                page_h = (djvu_doc_page_info(doc, page_no, &info) == 0)
                             ? info.height : (root->y + root->h);
                flip_zone_y(root, page_h);
                res->root = root;
            } else {
                free_zone_kids(ctx, root);
                djvu_free(ctx, root);
            }
        }
    }

    djvu_free(ctx, owned);
    return res;
}

void djvu_text_zones_destroy(djvu_ctx *ctx, djvu_page_text_zones *z)
{
    if (!z) return;
    if (z->root) {
        free_zone_kids(ctx, z->root);
        djvu_free(ctx, z->root);
    }
    djvu_free(ctx, z->text);
    djvu_free(ctx, z);
}

#include <string.h>
#include <stdlib.h>

typedef struct {
    djvu_ctx *ctx;
    djvu_buf_reader br;
    int count;
} nparse;

static int resolve_url(djvu_doc *doc, const char *url)
{
    const char *s = url;
    int allnum = 1, n;
    if (!s || !*s) return -1;
    if (*s == '#') s++;
    if (!*s) return -1;
    { const char *t = s; for (; *t; t++) if (*t < '0' || *t > '9') { allnum = 0; break; } }
    if (allnum) {
        n = atoi(s) - 1;
        if (n >= 0 && n < doc->npages) return n;
        return -1;
    }
    return djvu_doc_page_by_name(doc, s);
}

static void free_items(djvu_ctx *ctx, djvu_outline_item *items, int n);

static int parse_item(nparse *np, djvu_doc *doc, djvu_outline_item *out)
{
    int nkids, namelen, urllen, i;

    np->count++;
    nkids = djvu_br_u8(&np->br);
    namelen = djvu_br_u24be(&np->br);
    out->title = djvu_br_strdup(&np->br, namelen);
    urllen = djvu_br_u24be(&np->br);
    out->url = djvu_br_strdup(&np->br, urllen);
    out->page_no = out->url ? resolve_url(doc, out->url) : -1;
    out->children = NULL;
    out->nchildren = 0;
    if (np->br.failed) return -1;

    if (nkids > 0) {
        out->children = (djvu_outline_item *)djvu_alloc(np->ctx,
                            sizeof(djvu_outline_item) * (size_t)nkids);
        if (!out->children) return -1;
        memset(out->children, 0, sizeof(djvu_outline_item) * (size_t)nkids);
        for (i = 0; i < nkids; i++) {
            if (parse_item(np, doc, &out->children[i]) != 0) {
                out->nchildren = i;
                return -1;
            }
        }
        out->nchildren = nkids;
    }
    return 0;
}

static void free_items(djvu_ctx *ctx, djvu_outline_item *items, int n)
{
    int i;
    if (!items) return;
    for (i = 0; i < n; i++) {
        free_items(ctx, items[i].children, items[i].nchildren);
        djvu_free(ctx, items[i].title);
        djvu_free(ctx, items[i].url);
    }
    djvu_free(ctx, items);
}

djvu_outline_item *djvu_doc_outline(djvu_doc *doc)
{
    djvu_ctx *ctx;
    const uint8_t *navm;
    uint32_t sz;
    uint8_t *dec;
    size_t dlen;
    nparse np;
    int total, cap, n;
    djvu_outline_item *items = NULL, *root;

    if (!doc) return NULL;
    ctx = doc->ctx;
    navm = djvu_form_find_chunk(doc, doc->root_form_off, "NAVM", &sz, NULL);
    if (!navm) return NULL;

    dec = djvu_bzz_decode_all(ctx, navm, sz, &dlen);
    if (!dec) return NULL;

    memset(&np, 0, sizeof(np));
    np.ctx = ctx;
    djvu_br_init(&np.br, ctx, dec, dlen);
    total = djvu_br_u16be(&np.br);
    if (np.br.failed || total <= 0) { djvu_free(ctx, dec); return NULL; }

    cap = 0; n = 0;
    while (!np.br.failed && np.br.pos < dlen && np.count < total) {
        djvu_outline_item tmp;
        memset(&tmp, 0, sizeof(tmp));
        if (parse_item(&np, doc, &tmp) != 0) {
            free_items(ctx, tmp.children, tmp.nchildren);
            djvu_free(ctx, tmp.title); djvu_free(ctx, tmp.url);
            break;
        }
        if (n == cap) {
            int ncap = cap ? cap * 2 : 8;
            djvu_outline_item *na = (djvu_outline_item *)djvu_alloc(ctx,
                                        sizeof(djvu_outline_item) * (size_t)ncap);
            if (!na) { free_items(ctx, tmp.children, tmp.nchildren);
                       djvu_free(ctx, tmp.title); djvu_free(ctx, tmp.url); break; }
            if (items) { memcpy(na, items, sizeof(djvu_outline_item) * (size_t)n);
                         djvu_free(ctx, items); }
            items = na; cap = ncap;
        }
        items[n++] = tmp;
    }

    djvu_free(ctx, dec);

    if (n == 0) { djvu_free(ctx, items); return NULL; }

    root = (djvu_outline_item *)djvu_alloc(ctx, sizeof(*root));
    if (!root) { free_items(ctx, items, n); return NULL; }
    memset(root, 0, sizeof(*root));
    root->page_no = -1;
    root->children = items;
    root->nchildren = n;
    return root;
}

void djvu_outline_destroy(djvu_ctx *ctx, djvu_outline_item *root)
{
    if (!root) return;
    free_items(ctx, root->children, root->nchildren);
    djvu_free(ctx, root);
}

#include <string.h>
#include <stdlib.h>

typedef struct snode {
    int kind;
    char *text;
    struct snode **kids;
    int nkids, cap;
} snode;

typedef struct {
    djvu_ctx *ctx;
    const char *s;
    size_t len, pos;
} sparse;

static void snode_free(djvu_ctx *ctx, snode *n)
{
    int i;
    if (!n) return;
    for (i = 0; i < n->nkids; i++) snode_free(ctx, n->kids[i]);
    djvu_free(ctx, n->kids);
    djvu_free(ctx, n->text);
    djvu_free(ctx, n);
}

static void snode_add(sparse *sp, snode *list, snode *kid)
{
    if (list->nkids == list->cap) {
        int nc = list->cap ? list->cap * 2 : 8;
        snode **na = (snode **)djvu_alloc(sp->ctx, sizeof(snode *) * (size_t)nc);
        if (!na) { snode_free(sp->ctx, kid); return; }
        if (list->kids) {
            memcpy(na, list->kids, sizeof(snode *) * (size_t)list->nkids);
            djvu_free(sp->ctx, list->kids);
        }
        list->kids = na; list->cap = nc;
    }
    list->kids[list->nkids++] = kid;
}

static void skip_ws(sparse *sp)
{
    while (sp->pos < sp->len) {
        char c = sp->s[sp->pos];
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\f')
            sp->pos++;
        else
            break;
    }
}

static snode *parse_string(sparse *sp)
{
    djvu_ctx *ctx = sp->ctx;
    size_t cap = 16, n = 0;
    char *buf = (char *)djvu_alloc(ctx, cap);
    snode *node;
    if (!buf) return NULL;
    sp->pos++;
    while (sp->pos < sp->len) {
        char c = sp->s[sp->pos++];
        if (c == '"') break;
        if (c == '\\' && sp->pos < sp->len) {
            char e = sp->s[sp->pos++];
            switch (e) {
                case 'n': c = '\n'; break;
                case 't': c = '\t'; break;
                case 'r': c = '\r'; break;
                case 'f': c = '\f'; break;
                case 'b': c = '\b'; break;
                default:  c = e;    break;
            }
        }
        if (n + 1 >= cap) {
            char *nb = (char *)djvu_alloc(ctx, cap * 2);
            if (!nb) { djvu_free(ctx, buf); return NULL; }
            memcpy(nb, buf, n); djvu_free(ctx, buf); buf = nb; cap *= 2;
        }
        buf[n++] = c;
    }
    buf[n] = 0;
    node = (snode *)djvu_alloc(ctx, sizeof(snode));
    if (!node) { djvu_free(ctx, buf); return NULL; }
    memset(node, 0, sizeof(*node));
    node->kind = 1; node->text = buf;
    return node;
}

static snode *parse_atom(sparse *sp)
{
    djvu_ctx *ctx = sp->ctx;
    size_t start = sp->pos;
    snode *node;
    char *buf;
    size_t n;
    while (sp->pos < sp->len) {
        char c = sp->s[sp->pos];
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\f' ||
            c == '(' || c == ')' || c == '"')
            break;
        sp->pos++;
    }
    n = sp->pos - start;
    buf = (char *)djvu_alloc(ctx, n + 1);
    if (!buf) return NULL;
    memcpy(buf, sp->s + start, n); buf[n] = 0;
    node = (snode *)djvu_alloc(ctx, sizeof(snode));
    if (!node) { djvu_free(ctx, buf); return NULL; }
    memset(node, 0, sizeof(*node));
    node->kind = 1; node->text = buf;
    return node;
}

static snode *parse_node(sparse *sp);

static snode *parse_list(sparse *sp)
{
    djvu_ctx *ctx = sp->ctx;
    snode *list = (snode *)djvu_alloc(ctx, sizeof(snode));
    if (!list) return NULL;
    memset(list, 0, sizeof(*list));
    list->kind = 0;
    sp->pos++;
    for (;;) {
        skip_ws(sp);
        if (sp->pos >= sp->len) break;
        if (sp->s[sp->pos] == ')') { sp->pos++; break; }
        {
            snode *kid = parse_node(sp);
            if (!kid) break;
            snode_add(sp, list, kid);
        }
    }
    return list;
}

static snode *parse_node(sparse *sp)
{
    skip_ws(sp);
    if (sp->pos >= sp->len) return NULL;
    if (sp->s[sp->pos] == '(') return parse_list(sp);
    if (sp->s[sp->pos] == '"') return parse_string(sp);
    if (sp->s[sp->pos] == ')') return NULL;
    return parse_atom(sp);
}

static const char *atom_text(const snode *n)
{
    return (n && n->kind == 1) ? n->text : NULL;
}
static int is_list_head(const snode *n, const char *head)
{
    return n && n->kind == 0 && n->nkids > 0 &&
           atom_text(n->kids[0]) && strcmp(atom_text(n->kids[0]), head) == 0;
}

typedef struct {
    djvu_ctx *ctx;
    djvu_doc *doc;
    int page_h;
    djvu_link *links;
    int n, cap;
} lcollect;

static char *dup_str(djvu_ctx *ctx, const char *s)
{
    size_t n;
    char *r;
    if (!s) return NULL;
    n = strlen(s);
    r = (char *)djvu_alloc(ctx, n + 1);
    if (r) memcpy(r, s, n + 1);
    return r;
}

static void collect_maparea(lcollect *lc, const snode *area)
{
    const snode *url_node = NULL, *comment_node = NULL, *shape = NULL;
    const char *url = NULL, *comment = NULL, *head;
    djvu_link_shape stype;
    int i, x, y, w, h;


    for (i = 1; i < area->nkids; i++) {
        const snode *k = area->kids[i];
        if (!shape && k->kind == 0) {
            head = atom_text(k->kids ? k->kids[0] : NULL);
            if (head && (!strcmp(head, "rect") || !strcmp(head, "oval") ||
                         !strcmp(head, "text") || !strcmp(head, "poly") ||
                         !strcmp(head, "line"))) {
                shape = k; continue;
            }
        }
        if (!url_node) { url_node = k; continue; }
        if (!comment_node && k->kind == 1) { comment_node = k; continue; }
    }

    if (url_node) {
        if (url_node->kind == 1) url = url_node->text;
        else if (is_list_head(url_node, "url") && url_node->nkids >= 2)
            url = atom_text(url_node->kids[1]);
    }
    if (comment_node) comment = atom_text(comment_node);
    if (!url || !*url || !shape) return;

    head = atom_text(shape->kids[0]);
    if (!strcmp(head, "oval")) stype = DJVU_LINK_OVAL;
    else if (!strcmp(head, "text")) stype = DJVU_LINK_TEXT;
    else if (!strcmp(head, "rect")) stype = DJVU_LINK_RECT;
    else return;

    if (shape->nkids < 5) return;
    x = atoi(atom_text(shape->kids[1]) ? atom_text(shape->kids[1]) : "0");
    y = atoi(atom_text(shape->kids[2]) ? atom_text(shape->kids[2]) : "0");
    w = atoi(atom_text(shape->kids[3]) ? atom_text(shape->kids[3]) : "0");
    h = atoi(atom_text(shape->kids[4]) ? atom_text(shape->kids[4]) : "0");

    if (lc->n == lc->cap) {
        int nc = lc->cap ? lc->cap * 2 : 8;
        djvu_link *na = (djvu_link *)djvu_alloc(lc->ctx, sizeof(djvu_link) * (size_t)nc);
        if (!na) return;
        if (lc->links) {
            memcpy(na, lc->links, sizeof(djvu_link) * (size_t)lc->n);
            djvu_free(lc->ctx, lc->links);
        }
        lc->links = na; lc->cap = nc;
    }
    {
        djvu_link *L = &lc->links[lc->n++];
        L->url = dup_str(lc->ctx, url);
        L->comment = (comment && *comment) ? dup_str(lc->ctx, comment) : NULL;
        L->shape = stype;
        L->x = x;
        L->y = djvu_y_bottomup_to_topdown(y, lc->page_h, h);
        L->w = w;
        L->h = h;
    }
}

static void walk(lcollect *lc, const snode *n)
{
    int i;
    if (!n || n->kind != 0) return;
    if (is_list_head(n, "maparea")) collect_maparea(lc, n);
    for (i = 0; i < n->nkids; i++) walk(lc, n->kids[i]);
}

static char *load_anno(djvu_doc *doc, uint32_t form_off, size_t *out_len)
{
    djvu_ctx *ctx = doc->ctx;
    uint32_t sz;
    const uint8_t *chunk;

    chunk = djvu_form_find_chunk(doc, form_off, "ANTz", &sz, NULL);
    if (chunk) {
        uint8_t *dec = djvu_bzz_decode_all(ctx, chunk, sz, out_len);
        return (char *)dec;
    }
    chunk = djvu_form_find_chunk(doc, form_off, "ANTa", &sz, NULL);
    if (chunk) {
        char *buf = (char *)djvu_alloc(ctx, sz + 1);
        if (!buf) return NULL;
        memcpy(buf, chunk, sz); buf[sz] = 0;
        *out_len = sz;
        return buf;
    }
    chunk = djvu_form_find_incl_chunk(doc, form_off, "ANTz", &sz);
    if (chunk)
        return (char *)djvu_bzz_decode_all(ctx, chunk, sz, out_len);
    chunk = djvu_form_find_incl_chunk(doc, form_off, "ANTa", &sz);
    if (chunk) {
        char *buf = (char *)djvu_alloc(ctx, sz + 1);
        if (!buf) return NULL;
        memcpy(buf, chunk, sz); buf[sz] = 0;
        *out_len = sz;
        return buf;
    }
    return NULL;
}

djvu_page_links *djvu_page_get_links(djvu_doc *doc, int page_no)
{
    djvu_ctx *ctx;
    char *buf;
    size_t blen = 0;
    sparse sp;
    lcollect lc;
    djvu_page_info info;
    djvu_page_links *res;

    if (!doc || page_no < 0 || page_no >= doc->npages) return NULL;
    ctx = doc->ctx;
    buf = load_anno(doc, doc->pages[page_no].form_off, &blen);
    if (!buf) return NULL;

    memset(&lc, 0, sizeof(lc));
    lc.ctx = ctx; lc.doc = doc;
    lc.page_h = (djvu_doc_page_info(doc, page_no, &info) == 0) ? info.height : 0;

    memset(&sp, 0, sizeof(sp));
    sp.ctx = ctx; sp.s = buf; sp.len = blen;

    while (sp.pos < sp.len) {
        snode *node;
        skip_ws(&sp);
        if (sp.pos >= sp.len || sp.s[sp.pos] != '(') { sp.pos++; continue; }
        node = parse_node(&sp);
        if (!node) break;
        walk(&lc, node);
        snode_free(ctx, node);
    }
    djvu_free(ctx, buf);

    if (lc.n == 0) { djvu_free(ctx, lc.links); return NULL; }

    res = (djvu_page_links *)djvu_alloc(ctx, sizeof(*res));
    if (!res) {
        int i;
        for (i = 0; i < lc.n; i++) { djvu_free(ctx, lc.links[i].url);
                                     djvu_free(ctx, lc.links[i].comment); }
        djvu_free(ctx, lc.links);
        return NULL;
    }
    res->links = lc.links;
    res->nlinks = lc.n;
    return res;
}

void djvu_page_links_destroy(djvu_ctx *ctx, djvu_page_links *links)
{
    int i;
    if (!links) return;
    for (i = 0; i < links->nlinks; i++) {
        djvu_free(ctx, links->links[i].url);
        djvu_free(ctx, links->links[i].comment);
    }
    djvu_free(ctx, links->links);
    djvu_free(ctx, links);
}

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void djvu_debug_dump_comps(djvu_doc *doc)
{
    int i;
    const char *tn[4] = {"incl", "page", "thumb", "anno"};
    if (!doc) return;
    printf("components: %d\n", doc->ncomp);
    for (i = 0; i < doc->ncomp; i++) {
        djvu_component *c = &doc->comps[i];
        printf("  [%d] off=%u size=%u type=%s id=%s\n", i, c->offset, c->size,
               (c->type >= 0 && c->type < 4) ? tn[c->type] : "?",
               c->id ? c->id : "(null)");
    }
}

djvu_image *djvu_debug_render_iw(djvu_doc *doc, int page_no, int kind)
{
    djvu_ctx *ctx;
    const char *id = kind ? "FG44" : "BG44";
    iw_pixmap *pm;
    djvu_image *out = NULL;
    int w, h;

    int pm_owned = 0;
    if (!doc || page_no < 0 || page_no >= doc->npages) return NULL;
    ctx = doc->ctx;
    pm = djvu_doc_iw44_acquire(doc, page_no, id, &pm_owned);
    if (!pm) return NULL;
    w = djvu_iw44_width(pm); h = djvu_iw44_height(pm);
    if (w <= 0 || h <= 0) {
        djvu_doc_iw44_release(ctx, pm, pm_owned);
        return NULL;
    }
    out = (djvu_image *)djvu_alloc(ctx, sizeof(djvu_image));
    if (!out) {
        djvu_doc_iw44_release(ctx, pm, pm_owned);
        return NULL;
    }
    out->width = w; out->height = h; out->format = DJVU_FORMAT_RGB24;
    out->stride = w * 3;
    out->data = (uint8_t *)djvu_alloc(ctx, (size_t)w * h * 3);
    if (!out->data || djvu_iw44_render_rgb(pm, out->data) != 0) {
        djvu_image_destroy(ctx, out);
        djvu_doc_iw44_release(ctx, pm, pm_owned);
        return NULL;
    }
    djvu_doc_iw44_release(ctx, pm, pm_owned);
    return out;
}

djvu_image *djvu_debug_render_iw_gray(djvu_doc *doc, int page_no, int kind)
{
    djvu_ctx *ctx;
    const char *id = kind ? "FG44" : "BG44";
    iw_pixmap *pm;
    djvu_image *out = NULL;
    int w, h;
    int pm_owned = 0;
    if (!doc || page_no < 0 || page_no >= doc->npages) return NULL;
    ctx = doc->ctx;
    pm = djvu_doc_iw44_acquire(doc, page_no, id, &pm_owned);
    if (!pm) return NULL;
    w = djvu_iw44_width(pm); h = djvu_iw44_height(pm);
    if (w <= 0 || h <= 0) {
        djvu_doc_iw44_release(ctx, pm, pm_owned);
        return NULL;
    }
    out = (djvu_image *)djvu_alloc(ctx, sizeof(djvu_image));
    if (!out) {
        djvu_doc_iw44_release(ctx, pm, pm_owned);
        return NULL;
    }
    out->width = w; out->height = h; out->format = DJVU_FORMAT_GRAY8; out->stride = w;
    out->data = (uint8_t *)djvu_alloc(ctx, (size_t)w * h);
    if (!out->data || djvu_iw44_render_gray(pm, out->data) != 0) {
        djvu_image_destroy(ctx, out);
        djvu_doc_iw44_release(ctx, pm, pm_owned);
        return NULL;
    }
    djvu_doc_iw44_release(ctx, pm, pm_owned);
    return out;
}

djvu_image *djvu_debug_render_iw_plane(djvu_doc *doc, int page_no, int kind, int plane)
{
    djvu_ctx *ctx;
    const char *id = kind ? "FG44" : "BG44";
    iw_pixmap *pm;
    djvu_image *out;
    int w, h;
    int pm_owned = 0;
    if (!doc || page_no < 0 || page_no >= doc->npages) return NULL;
    ctx = doc->ctx;
    pm = djvu_doc_iw44_acquire(doc, page_no, id, &pm_owned);
    if (!pm) return NULL;
    w = djvu_iw44_width(pm); h = djvu_iw44_height(pm);
    if (w <= 0 || h <= 0) {
        djvu_doc_iw44_release(ctx, pm, pm_owned);
        return NULL;
    }
    out = (djvu_image *)djvu_alloc(ctx, sizeof(djvu_image));
    if (!out) {
        djvu_doc_iw44_release(ctx, pm, pm_owned);
        return NULL;
    }
    out->width = w; out->height = h; out->format = DJVU_FORMAT_GRAY8; out->stride = w;
    out->data = (uint8_t *)djvu_alloc(ctx, (size_t)w * h);
    if (!out->data || djvu_iw44_render_plane(pm, plane, out->data) != 0) {
        djvu_image_destroy(ctx, out);
        djvu_doc_iw44_release(ctx, pm, pm_owned);
        return NULL;
    }
    djvu_doc_iw44_release(ctx, pm, pm_owned);
    return out;
}

djvu_image *djvu_debug_render_bg(djvu_doc *doc, int page_no)
{
    djvu_ctx *ctx; djvu_page_info info; djvu_cpix bg; djvu_image *out;
    if (!doc || page_no < 0 || page_no >= doc->npages) return NULL;
    ctx = doc->ctx;
    if (djvu_doc_page_info(doc, page_no, &info) != 0) return NULL;
    memset(&bg, 0, sizeof(bg));
    if (djvu_compose_background(doc, doc->pages[page_no].form_off,
                               info.width, info.height, 1, &bg) != 0) return NULL;
    out = (djvu_image *)djvu_alloc(ctx, sizeof(djvu_image));
    if (!out) { djvu_cpix_free(ctx, &bg); return NULL; }
    out->width = bg.w; out->height = bg.h; out->format = DJVU_FORMAT_RGB24;
    out->stride = bg.w * 3;
    out->data = (uint8_t *)djvu_alloc(ctx, (size_t)bg.w * bg.h * 3);
    if (!out->data) { djvu_free(ctx, out); djvu_cpix_free(ctx, &bg); return NULL; }
    djvu_flip_rgb_bottomup(out->data, bg.d, bg.w, bg.h, 0);
    djvu_cpix_free(ctx, &bg);
    return out;
}

static void put_u32be(FILE *f, uint32_t v)
{
    fputc((v >> 24) & 0xff, f); fputc((v >> 16) & 0xff, f);
    fputc((v >> 8) & 0xff, f); fputc(v & 0xff, f);
}

int djvu_debug_dump_iw(djvu_doc *doc, int page_no, int kind, const char *path)
{
    uint32_t form_off, start = 0, sz;
    const uint8_t *chunk;
    const char *id = kind ? "FG44" : "BG44";
    FILE *f;
    uint32_t total = 4;
    const uint8_t *chunks[64]; uint32_t sizes[64]; int n = 0, i;

    if (!doc || page_no < 0 || page_no >= doc->npages) return -1;
    form_off = doc->pages[page_no].form_off;
    {
        int maxc = doc->ctx->iw_max_chunks > 0 ? doc->ctx->iw_max_chunks : 1000;
        while ((chunk = djvu_form_find_chunk(doc, form_off, id, &sz, &start)) != NULL && n < 64) {
            if (n >= maxc) break;
            chunks[n] = chunk; sizes[n] = sz; n++;
            total += 8 + sz + (sz & 1);
        }
    }
    if (n == 0) return -1;
    f = fopen(path, "wb");
    if (!f) return -1;
    fwrite("AT&TFORM", 1, 8, f);
    put_u32be(f, total);
    fwrite("PM44", 1, 4, f);
    for (i = 0; i < n; i++) {
        fwrite("PM44", 1, 4, f);
        put_u32be(f, sizes[i]);
        fwrite(chunks[i], 1, sizes[i], f);
        if (sizes[i] & 1) fputc(0, f);
    }
    fclose(f);
    return 0;
}

void djvu_debug_verify_mem(djvu_doc *doc, int page_no, const char *stage, FILE *out)
{
    int iw_bg = 0, iw_fg = 0, jb2_inline_pg = 0, i;
    size_t doc_len = 0;

    if (!out) return;
    if (!doc) {
        fprintf(out, "mem_dbg\t%d\t%s\tnodoc\n", page_no, stage ? stage : "");
        return;
    }
    doc_len = doc->len;
    for (i = 0; i < doc->npages; i++) {
        if (doc->pages[i].iw_bg) iw_bg++;
        if (doc->pages[i].iw_fg) iw_fg++;
        if (doc->pages[i].chunk_flags & DJVU_PG_DJBZ) jb2_inline_pg++;
    }
    fprintf(out,
            "mem_dbg\t%d\t%s\tnpages=%d doc_bytes=%zu iw_bg=%d iw_fg=%d "
            "jb2_inline=%d jb2_dicts=%d jb2_inline_pg=%d shared_incl=%d\n",
            page_no, stage ? stage : "", doc->npages, doc_len, iw_bg, iw_fg,
            doc->n_jb2_inline, doc->n_jb2_dicts, jb2_inline_pg, doc->n_shared_incl);
}
