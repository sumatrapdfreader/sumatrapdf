#include "jbig2.h"

#ifdef HAVE_CONFIG_H
#endif

#ifndef _JBIG2_OS_TYPES_H
#define _JBIG2_OS_TYPES_H

#if defined(HAVE_CONFIG_H)
# include "config_types.h"
#elif defined(_WIN32)

#ifdef _MSC_VER

# if _MSC_VER >= 1700
#  include <stdint.h>
# else
typedef signed char int8_t;
typedef short int int16_t;
typedef int int32_t;
typedef __int64 int64_t;
typedef unsigned char uint8_t;
typedef unsigned short int uint16_t;
typedef unsigned int uint32_t;
typedef unsigned __int64 uint64_t;
#ifndef SIZE_MAX
#define SIZE_MAX (~((size_t) 0))
#endif
# endif

# if _MSC_VER < 1500
#  define vsnprintf _vsnprintf

#  define inline
#else

# if !(defined(inline))
#  define inline __inline
# endif
# endif

# if _MSC_VER >= 1900
#  define STDC99
# else
#  define snprintf _snprintf
# endif

#if _MSC_VER >= 1400
    #define strdup _strdup
#endif

#else

# include <stdint.h>

#endif

#elif defined (STD_INT_USE_SYS_TYPES_H)
# include <sys/types.h>
#elif defined (STD_INT_USE_INTTYPES_H)
# include <inttypes.h>
#elif defined (STD_INT_USE_SYS_INTTYPES_H)
# include <sys/inttypes.h>
#elif defined (STD_INT_USE_SYS_INT_TYPES_H)
# include <sys/int_types.h>
#else
# include <stdint.h>
#endif

#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <limits.h>

#ifndef _JBIG2_PRIV_H
#define _JBIG2_PRIV_H

#ifdef JBIG_EXTERNAL_MEMENTO_H
#include JBIG_EXTERNAL_MEMENTO_H
#else

#ifndef MEMENTO_H

#include <stdlib.h>

#define MEMENTO_H

#ifndef MEMENTO_UNDERLYING_MALLOC
#define MEMENTO_UNDERLYING_MALLOC malloc
#endif
#ifndef MEMENTO_UNDERLYING_FREE
#define MEMENTO_UNDERLYING_FREE free
#endif
#ifndef MEMENTO_UNDERLYING_REALLOC
#define MEMENTO_UNDERLYING_REALLOC realloc
#endif
#ifndef MEMENTO_UNDERLYING_CALLOC
#define MEMENTO_UNDERLYING_CALLOC calloc
#endif

#ifndef MEMENTO_MAXALIGN
#define MEMENTO_MAXALIGN (sizeof(int))
#endif

#define MEMENTO_PREFILL   0xa6
#define MEMENTO_POSTFILL  0xa7
#define MEMENTO_ALLOCFILL 0xa8
#define MEMENTO_FREEFILL  0xa9

#define MEMENTO_FREELIST_MAX 0x2000000

int Memento_checkBlock(void *);
int Memento_checkAllMemory(void);
int Memento_check(void);

int Memento_setParanoia(int);
int Memento_paranoidAt(int);
int Memento_breakAt(int);
void Memento_breakOnFree(void *a);
void Memento_breakOnRealloc(void *a);
int Memento_getBlockNum(void *);
int Memento_find(void *a);
void Memento_breakpoint(void);
int Memento_failAt(int);
int Memento_failThisEvent(void);
void Memento_listBlocks(void);
void Memento_listNewBlocks(void);
size_t Memento_setMax(size_t);
void Memento_stats(void);
void *Memento_label(void *, const char *);
void Memento_tick(void);

void *Memento_malloc(size_t s);
void *Memento_realloc(void *, size_t s);
void  Memento_free(void *);
void *Memento_calloc(size_t, size_t);

void Memento_info(void *addr);
void Memento_listBlockInfo(void);
void *Memento_takeByteRef(void *blk);
void *Memento_dropByteRef(void *blk);
void *Memento_takeShortRef(void *blk);
void *Memento_dropShortRef(void *blk);
void *Memento_takeIntRef(void *blk);
void *Memento_dropIntRef(void *blk);
void *Memento_takeRef(void *blk);
void *Memento_dropRef(void *blk);
void *Memento_adjustRef(void *blk, int adjust);
void *Memento_reference(void *blk);

int Memento_checkPointerOrNull(void *blk);
int Memento_checkBytePointerOrNull(void *blk);
int Memento_checkShortPointerOrNull(void *blk);
int Memento_checkIntPointerOrNull(void *blk);

void Memento_startLeaking(void);
void Memento_stopLeaking(void);

int Memento_sequence(void);

int Memento_squeezing(void);

void Memento_fin(void);

void Memento_bt(void);

#ifdef MEMENTO

#ifndef COMPILING_MEMENTO_C
#define malloc  Memento_malloc
#define free    Memento_free
#define realloc Memento_realloc
#define calloc  Memento_calloc
#endif

#else

#define Memento_malloc  MEMENTO_UNDERLYING_MALLOC
#define Memento_free    MEMENTO_UNDERLYING_FREE
#define Memento_realloc MEMENTO_UNDERLYING_REALLOC
#define Memento_calloc  MEMENTO_UNDERLYING_CALLOC

#define Memento_checkBlock(A)              0
#define Memento_checkAllMemory()           0
#define Memento_check()                    0
#define Memento_setParanoia(A)             0
#define Memento_paranoidAt(A)              0
#define Memento_breakAt(A)                 0
#define Memento_breakOnFree(A)             0
#define Memento_breakOnRealloc(A)          0
#define Memento_getBlockNum(A)             0
#define Memento_find(A)                    0
#define Memento_breakpoint()               do {} while (0)
#define Memento_failAt(A)                  0
#define Memento_failThisEvent()            0
#define Memento_listBlocks()               do {} while (0)
#define Memento_listNewBlocks()            do {} while (0)
#define Memento_setMax(A)                  0
#define Memento_stats()                    do {} while (0)
#define Memento_label(A,B)                 (A)
#define Memento_info(A)                    do {} while (0)
#define Memento_listBlockInfo()            do {} while (0)
#define Memento_takeByteRef(A)             (A)
#define Memento_dropByteRef(A)             (A)
#define Memento_takeShortRef(A)            (A)
#define Memento_dropShortRef(A)            (A)
#define Memento_takeIntRef(A)              (A)
#define Memento_dropIntRef(A)              (A)
#define Memento_takeRef(A)                 (A)
#define Memento_dropRef(A)                 (A)
#define Memento_adjustRef(A,V)             (A)
#define Memento_reference(A)               (A)
#define Memento_checkPointerOrNull(A)      0
#define Memento_checkBytePointerOrNull(A)  0
#define Memento_checkShortPointerOrNull(A) 0
#define Memento_checkIntPointerOrNull(A)   0

#define Memento_tick()                     do {} while (0)
#define Memento_startLeaking()             do {} while (0)
#define Memento_stopLeaking()              do {} while (0)
#define Memento_fin()                      do {} while (0)
#define Memento_bt()                       do {} while (0)
#define Memento_sequence()                 (0)
#define Memento_squeezing()                (0)

#endif

#endif

#endif

#ifndef inline
#define inline
#endif

typedef uint8_t byte;

#define bool int

#ifdef __cplusplus
#define template template_C
#define new new_C
#endif

#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

#ifndef NULL
#define NULL ((void*)0)
#endif

#if !defined (INT32_MIN)
#define INT32_MIN (-0x7fffffff - 1)
#endif
#if !defined (INT32_MAX)
#define INT32_MAX  0x7fffffff
#endif
#if !defined (UINT32_MAX)
#define UINT32_MAX 0xffffffffu
#endif

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L
#define FMTZ "%zu"
#define FMTZ_CAST size_t
#elif defined(_MSC_VER)
#define FMTZ "%llu"
#define FMTZ_CAST _int64
#else
#define FMTZ "%lu"
#define FMTZ_CAST unsigned long
#endif

typedef struct _Jbig2Page Jbig2Page;
typedef struct _Jbig2Segment Jbig2Segment;

typedef enum {
    JBIG2_FILE_HEADER,
    JBIG2_FILE_SEQUENTIAL_HEADER,
    JBIG2_FILE_SEQUENTIAL_BODY,
    JBIG2_FILE_RANDOM_HEADERS,
    JBIG2_FILE_RANDOM_BODIES,
    JBIG2_FILE_EOF,
    JBIG2_FILE_HEADER_MAYBE
} Jbig2FileState;

struct _Jbig2Ctx {
    Jbig2Allocator *allocator;
    Jbig2Options options;
    const Jbig2Ctx *global_ctx;
    Jbig2ErrorCallback error_callback;
    void *error_callback_data;

    byte *buf;
    size_t buf_size;
    size_t buf_rd_ix;
    size_t buf_wr_ix;

    Jbig2FileState state;

    uint8_t file_header_flags;
    uint32_t n_pages;

    uint32_t n_segments_max;
    Jbig2Segment **segments;
    uint32_t n_segments;
    uint32_t segment_index;

    uint32_t current_page;
    uint32_t max_page_index;
    Jbig2Page *pages;
};

uint32_t jbig2_get_uint32(const byte *bptr);

int32_t jbig2_get_int32(const byte *buf);

uint16_t jbig2_get_uint16(const byte *bptr);

int16_t jbig2_get_int16(const byte *buf);

void *jbig2_alloc(Jbig2Allocator *allocator, size_t size, size_t num);

void jbig2_free(Jbig2Allocator *allocator, void *p);

void *jbig2_realloc(Jbig2Allocator *allocator, void *p, size_t size, size_t num);

#define jbig2_new(ctx, t, num) ((t *)jbig2_alloc(ctx->allocator, sizeof(t), num))

#define jbig2_renew(ctx, p, t, num) ((t *)jbig2_realloc(ctx->allocator, (p), sizeof(t), num))

int jbig2_error(Jbig2Ctx *ctx, Jbig2Severity severity, uint32_t seg_idx, const char *fmt, ...)
#ifdef __GNUC__
    __attribute__ ((format (__printf__, 4, 5)))
#endif
    ;

typedef struct _Jbig2WordStream Jbig2WordStream;

struct _Jbig2WordStream {
    int (*get_next_word)(Jbig2Ctx *ctx, Jbig2WordStream *self, size_t offset, uint32_t *word);
};

Jbig2WordStream *jbig2_word_stream_buf_new(Jbig2Ctx *ctx, const byte *data, size_t size);

void jbig2_word_stream_buf_free(Jbig2Ctx *ctx, Jbig2WordStream *ws);

#if defined (__STDC_VERSION_) && (__STDC_VERSION__ >= 199901L)
#define JBIG2_RESTRICT restrict
#elif defined(_MSC_VER) && (_MSC_VER >= 1600)
#define JBIG2_RESTRICT __restrict
#elif defined(__GNUC__) && (__GNUC__ >= 3)
#define JBIG2_RESTRICT __restrict
#else
#define JBIG2_RESTRICT
#endif

#endif

#ifndef _JBIG2_IMAGE_H
#define _JBIG2_IMAGE_H

typedef enum {
    JBIG2_COMPOSE_OR = 0,
    JBIG2_COMPOSE_AND = 1,
    JBIG2_COMPOSE_XOR = 2,
    JBIG2_COMPOSE_XNOR = 3,
    JBIG2_COMPOSE_REPLACE = 4
} Jbig2ComposeOp;

Jbig2Image *jbig2_image_new(Jbig2Ctx *ctx, uint32_t width, uint32_t height);
void jbig2_image_release(Jbig2Ctx *ctx, Jbig2Image *image);
Jbig2Image *jbig2_image_reference(Jbig2Ctx *ctx, Jbig2Image *image);
void jbig2_image_free(Jbig2Ctx *ctx, Jbig2Image *image);
void jbig2_image_clear(Jbig2Ctx *ctx, Jbig2Image *image, int value);
Jbig2Image *jbig2_image_resize(Jbig2Ctx *ctx, Jbig2Image *image, uint32_t width, uint32_t height, int value);
int jbig2_image_compose(Jbig2Ctx *ctx, Jbig2Image *dst, Jbig2Image *src, int64_t x, int64_t y, Jbig2ComposeOp op);

int jbig2_image_get_pixel(Jbig2Image *image, int64_t x, int64_t y);
void jbig2_image_set_pixel(Jbig2Image *image, int64_t x, int64_t y, bool value);

#endif

#ifndef _JBIG2_PAGE_H
#define _JBIG2_PAGE_H

typedef enum {
    JBIG2_PAGE_FREE,
    JBIG2_PAGE_NEW,
    JBIG2_PAGE_COMPLETE,
    JBIG2_PAGE_RETURNED,
    JBIG2_PAGE_RELEASED
} Jbig2PageState;

struct _Jbig2Page {
    Jbig2PageState state;
    uint32_t number;
    uint32_t height, width;
    uint32_t x_resolution, y_resolution;
    uint16_t stripe_size;
    bool striped;
    uint32_t end_row;
    uint8_t flags;
    Jbig2Image *image;
};

int jbig2_page_info(Jbig2Ctx *ctx, Jbig2Segment *segment, const uint8_t *segment_data);
int jbig2_end_of_stripe(Jbig2Ctx *ctx, Jbig2Segment *segment, const uint8_t *segment_data);
int jbig2_end_of_page(Jbig2Ctx *ctx, Jbig2Segment *segment, const uint8_t *segment_data);
int jbig2_page_add_result(Jbig2Ctx *ctx, Jbig2Page *page, Jbig2Image *src, uint32_t x, uint32_t y, Jbig2ComposeOp op);

#endif

#ifndef _JBIG2_SEGMENT_H
#define _JBIG2_SEGMENT_H

struct _Jbig2Segment {
    uint32_t number;
    uint8_t flags;
    uint32_t page_association;
    size_t data_length;
    int referred_to_segment_count;
    uint32_t *referred_to_segments;
    uint32_t rows;
    void *result;
};

Jbig2Segment *jbig2_parse_segment_header(Jbig2Ctx *ctx, uint8_t *buf, size_t buf_size, size_t *p_header_size);
int jbig2_parse_segment(Jbig2Ctx *ctx, Jbig2Segment *segment, const uint8_t *segment_data);
void jbig2_free_segment(Jbig2Ctx *ctx, Jbig2Segment *segment);
Jbig2Segment *jbig2_find_segment(Jbig2Ctx *ctx, uint32_t number);

typedef struct {
    uint32_t width;
    uint32_t height;
    uint32_t x;
    uint32_t y;
    Jbig2ComposeOp op;
    uint8_t flags;
} Jbig2RegionSegmentInfo;

void jbig2_get_region_segment_info(Jbig2RegionSegmentInfo *info, const uint8_t *segment_data);

#endif

static void *
jbig2_default_alloc(Jbig2Allocator *allocator, size_t size)
{
    (void) allocator;
    return malloc(size);
}

static void
jbig2_default_free(Jbig2Allocator *allocator, void *p)
{
    (void) allocator;
    free(p);
}

static void *
jbig2_default_realloc(Jbig2Allocator *allocator, void *p, size_t size)
{
    (void) allocator;
    return realloc(p, size);
}

static Jbig2Allocator jbig2_default_allocator = {
    jbig2_default_alloc,
    jbig2_default_free,
    jbig2_default_realloc
};

void *
jbig2_alloc(Jbig2Allocator *allocator, size_t size, size_t num)
{

    if (num > 0 && size > SIZE_MAX / num)
        return NULL;
    return allocator->alloc(allocator, size * num);
}

static void
jbig2_default_error(void *data, const char *msg, Jbig2Severity severity, uint32_t seg_idx)
{
    (void) data;

    if (severity == JBIG2_SEVERITY_FATAL) {
        fprintf(stderr, "jbig2 decoder FATAL ERROR: %s", msg);
        if (seg_idx != JBIG2_UNKNOWN_SEGMENT_NUMBER)
            fprintf(stderr, " (segment 0x%02x)", seg_idx);
        fprintf(stderr, "\n");
        fflush(stderr);
    }
}

int
jbig2_error(Jbig2Ctx *ctx, Jbig2Severity severity, uint32_t segment_number, const char *fmt, ...)
{
    char buf[1024];
    va_list ap;
    int n;

    va_start(ap, fmt);
    n = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    if (n < 0 || n == sizeof(buf))
        ctx->error_callback(ctx->error_callback_data, "failed to generate error string", severity, segment_number);
    else
        ctx->error_callback(ctx->error_callback_data, buf, severity, segment_number);
    return -1;
}

Jbig2Ctx *
jbig2_ctx_new_imp(Jbig2Allocator *allocator, Jbig2Options options, Jbig2GlobalCtx *global_ctx, Jbig2ErrorCallback error_callback, void *error_callback_data, int jbig2_version_major, int jbig2_version_minor)
{
    Jbig2Ctx *result;

    if (jbig2_version_major != JBIG2_VERSION_MAJOR || jbig2_version_minor != JBIG2_VERSION_MINOR) {
        Jbig2Ctx fakectx;
        fakectx.error_callback = error_callback;
        fakectx.error_callback_data = error_callback_data;
        jbig2_error(&fakectx, JBIG2_SEVERITY_FATAL, JBIG2_UNKNOWN_SEGMENT_NUMBER, "incompatible jbig2dec header (%d.%d) and library (%d.%d) versions",
            jbig2_version_major, jbig2_version_minor, JBIG2_VERSION_MAJOR, JBIG2_VERSION_MINOR);
        return NULL;
    }

    if (allocator == NULL)
        allocator = &jbig2_default_allocator;
    if (error_callback == NULL)
        error_callback = &jbig2_default_error;

    result = (Jbig2Ctx *) jbig2_alloc(allocator, sizeof(Jbig2Ctx), 1);
    if (result == NULL) {
        error_callback(error_callback_data, "failed to allocate initial context", JBIG2_SEVERITY_FATAL, JBIG2_UNKNOWN_SEGMENT_NUMBER);
        return NULL;
    }

    result->allocator = allocator;
    result->options = options;
    result->global_ctx = (const Jbig2Ctx *)global_ctx;
    result->error_callback = error_callback;
    result->error_callback_data = error_callback_data;

    result->state = (options & JBIG2_OPTIONS_EMBEDDED) ? JBIG2_FILE_HEADER_MAYBE : JBIG2_FILE_HEADER;

    result->buf = NULL;

    result->n_segments = 0;
    result->n_segments_max = 16;
    result->segments = jbig2_new(result, Jbig2Segment *, result->n_segments_max);
    if (result->segments == NULL) {
        error_callback(error_callback_data, "failed to allocate initial segments", JBIG2_SEVERITY_FATAL, JBIG2_UNKNOWN_SEGMENT_NUMBER);
        jbig2_free(allocator, result);
        return NULL;
    }
    result->segment_index = 0;

    result->current_page = 0;
    result->max_page_index = 4;
    result->pages = jbig2_new(result, Jbig2Page, result->max_page_index);
    if (result->pages == NULL) {
        error_callback(error_callback_data, "failed to allocated initial pages", JBIG2_SEVERITY_FATAL, JBIG2_UNKNOWN_SEGMENT_NUMBER);
        jbig2_free(allocator, result->segments);
        jbig2_free(allocator, result);
        return NULL;
    }
    {
        uint32_t index;

        for (index = 0; index < result->max_page_index; index++) {
            result->pages[index].state = JBIG2_PAGE_FREE;
            result->pages[index].number = 0;
            result->pages[index].width = 0;
            result->pages[index].height = 0xffffffff;
            result->pages[index].x_resolution = 0;
            result->pages[index].y_resolution = 0;
            result->pages[index].stripe_size = 0;
            result->pages[index].striped = 0;
            result->pages[index].end_row = 0;
            result->pages[index].flags = 0;
            result->pages[index].image = NULL;
        }
    }

    return result;
}

#define get_uint16(bptr)\
    (((bptr)[0] << 8) | (bptr)[1])
#define get_int16(bptr)\
    (((int)get_uint16(bptr) ^ 0x8000) - 0x8000)

int16_t
jbig2_get_int16(const byte *bptr)
{
    return get_int16(bptr);
}

uint16_t
jbig2_get_uint16(const byte *bptr)
{
    return get_uint16(bptr);
}

int32_t
jbig2_get_int32(const byte *bptr)
{
    return ((int32_t) get_int16(bptr) << 16) | get_uint16(bptr + 2);
}

uint32_t
jbig2_get_uint32(const byte *bptr)
{
    return ((uint32_t) get_uint16(bptr) << 16) | get_uint16(bptr + 2);
}

static size_t
jbig2_find_buffer_size(size_t desired)
{
    const size_t initial_buf_size = 1024;
    size_t size = initial_buf_size;

    if (desired == SIZE_MAX)
        return SIZE_MAX;

    while (size < desired)
        size <<= 1;

    return size;
}

int
jbig2_data_in(Jbig2Ctx *ctx, const unsigned char *data, size_t size)
{
    if (ctx->buf == NULL) {
        size_t buf_size = jbig2_find_buffer_size(size);
        ctx->buf = jbig2_new(ctx, byte, buf_size);
        if (ctx->buf == NULL) {
            return jbig2_error(ctx, JBIG2_SEVERITY_FATAL, JBIG2_UNKNOWN_SEGMENT_NUMBER, "failed to allocate buffer when reading data");
        }
        ctx->buf_size = buf_size;
        ctx->buf_rd_ix = 0;
        ctx->buf_wr_ix = 0;
    } else if (size > ctx->buf_size - ctx->buf_wr_ix) {
        size_t already = ctx->buf_wr_ix - ctx->buf_rd_ix;

        if (ctx->buf_rd_ix <= (ctx->buf_size >> 1) && size <= ctx->buf_size - already) {
            memmove(ctx->buf, ctx->buf + ctx->buf_rd_ix, already);
        } else {
            byte *buf;
            size_t buf_size;

            if (already > SIZE_MAX - size) {
                return jbig2_error(ctx, JBIG2_SEVERITY_FATAL, JBIG2_UNKNOWN_SEGMENT_NUMBER, "read data causes buffer to grow too large");
            }

            buf_size = jbig2_find_buffer_size(size + already);

            buf = jbig2_new(ctx, byte, buf_size);
            if (buf == NULL) {
                return jbig2_error(ctx, JBIG2_SEVERITY_FATAL, JBIG2_UNKNOWN_SEGMENT_NUMBER, "failed to allocate bigger buffer when reading data");
            }
            memcpy(buf, ctx->buf + ctx->buf_rd_ix, already);
            jbig2_free(ctx->allocator, ctx->buf);
            ctx->buf = buf;
            ctx->buf_size = buf_size;
        }
        ctx->buf_wr_ix -= ctx->buf_rd_ix;
        ctx->buf_rd_ix = 0;
    }

    memcpy(ctx->buf + ctx->buf_wr_ix, data, size);
    ctx->buf_wr_ix += size;

    for (;;) {
        const byte jbig2_id_string[8] = { 0x97, 0x4a, 0x42, 0x32, 0x0d, 0x0a, 0x1a, 0x0a };
        Jbig2Segment *segment;
        size_t header_size;
        int code;

        switch (ctx->state) {
        case JBIG2_FILE_HEADER_MAYBE:

            if (ctx->buf_wr_ix - ctx->buf_rd_ix < 9)
                return 0;
            if (memcmp(ctx->buf + ctx->buf_rd_ix, jbig2_id_string, 8) == 0)
            {
                ctx->state = JBIG2_FILE_HEADER;
                (void)jbig2_error(ctx, JBIG2_SEVERITY_WARNING, JBIG2_UNKNOWN_SEGMENT_NUMBER, "JBIG2 file header ignored in supposedly embedded stream");
            }
            else
                ctx->state = JBIG2_FILE_SEQUENTIAL_HEADER;
            break;
        case JBIG2_FILE_HEADER:

            if (ctx->buf_wr_ix - ctx->buf_rd_ix < 9)
                return 0;
            if (memcmp(ctx->buf + ctx->buf_rd_ix, jbig2_id_string, 8))
                return jbig2_error(ctx, JBIG2_SEVERITY_FATAL, JBIG2_UNKNOWN_SEGMENT_NUMBER, "not a JBIG2 file header");

            ctx->file_header_flags = ctx->buf[ctx->buf_rd_ix + 8];

            if (ctx->file_header_flags & 0x04)
                return jbig2_error(ctx, JBIG2_SEVERITY_FATAL, JBIG2_UNKNOWN_SEGMENT_NUMBER, "file header indicates use of 12 adaptive template pixels (NYI)");

            if (ctx->file_header_flags & 0x08)
                return jbig2_error(ctx, JBIG2_SEVERITY_FATAL, JBIG2_UNKNOWN_SEGMENT_NUMBER, "file header indicates use of colored region segments (NYI)");
            if (ctx->file_header_flags & 0xFC) {
                jbig2_error(ctx, JBIG2_SEVERITY_WARNING, JBIG2_UNKNOWN_SEGMENT_NUMBER, "reserved bits (2-7) of file header flags are not zero (0x%02x)", ctx->file_header_flags);
            }

            if (!(ctx->file_header_flags & 2)) {
                if (ctx->buf_wr_ix - ctx->buf_rd_ix < 13)
                    return 0;
                ctx->n_pages = jbig2_get_uint32(ctx->buf + ctx->buf_rd_ix + 9);
                ctx->buf_rd_ix += 13;
                if (ctx->n_pages == 1)
                    jbig2_error(ctx, JBIG2_SEVERITY_INFO, JBIG2_UNKNOWN_SEGMENT_NUMBER, "file header indicates a single page document");
                else
                    jbig2_error(ctx, JBIG2_SEVERITY_INFO, JBIG2_UNKNOWN_SEGMENT_NUMBER, "file header indicates a %d page document", ctx->n_pages);
            } else {
                ctx->n_pages = 0;
                ctx->buf_rd_ix += 9;
            }

            if (ctx->file_header_flags & 1) {
                ctx->state = JBIG2_FILE_SEQUENTIAL_HEADER;
                jbig2_error(ctx, JBIG2_SEVERITY_DEBUG, JBIG2_UNKNOWN_SEGMENT_NUMBER, "file header indicates sequential organization");
            } else {
                ctx->state = JBIG2_FILE_RANDOM_HEADERS;
                jbig2_error(ctx, JBIG2_SEVERITY_DEBUG, JBIG2_UNKNOWN_SEGMENT_NUMBER, "file header indicates random-access organization");
            }
            break;
        case JBIG2_FILE_SEQUENTIAL_HEADER:
        case JBIG2_FILE_RANDOM_HEADERS:
            segment = jbig2_parse_segment_header(ctx, ctx->buf + ctx->buf_rd_ix, ctx->buf_wr_ix - ctx->buf_rd_ix, &header_size);
            if (segment == NULL)
                return 0;
            ctx->buf_rd_ix += header_size;

            if (ctx->n_segments >= ctx->n_segments_max) {
                Jbig2Segment **segments;

                if (ctx->n_segments_max == UINT32_MAX) {
                    ctx->state = JBIG2_FILE_EOF;
                    jbig2_free_segment(ctx, segment);
                    return jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number, "too many segments in jbig2 image");
                }
                else if (ctx->n_segments_max > (UINT32_MAX >> 2)) {
                    ctx->n_segments_max = UINT32_MAX;
                } else {
                    ctx->n_segments_max <<= 2;
                }

                segments = jbig2_renew(ctx, ctx->segments, Jbig2Segment *, ctx->n_segments_max);
                if (segments == NULL) {
                    ctx->state = JBIG2_FILE_EOF;
                    jbig2_free_segment(ctx, segment);
                    return jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number, "failed to allocate space for more segments");
                }
                ctx->segments = segments;
            }

            ctx->segments[ctx->n_segments++] = segment;
            if (ctx->state == JBIG2_FILE_RANDOM_HEADERS) {
                if ((segment->flags & 63) == 51)
                    ctx->state = JBIG2_FILE_RANDOM_BODIES;
            } else
                ctx->state = JBIG2_FILE_SEQUENTIAL_BODY;
            break;
        case JBIG2_FILE_SEQUENTIAL_BODY:
        case JBIG2_FILE_RANDOM_BODIES:
            segment = ctx->segments[ctx->segment_index];

            if (segment->data_length == 0xffffffff && (segment->flags & 63) == 38) {
                byte *s, *e, *p;
                int mmr;
                byte mmr_marker[2] = { 0x00, 0x00 };
                byte arith_marker[2] = { 0xff, 0xac };
                byte *desired_marker;

                s = p = ctx->buf + ctx->buf_rd_ix;
                e = ctx->buf + ctx->buf_wr_ix;

                if (e - p < 18)
                        return 0;

                mmr = p[17] & 1;
                p += 18;
                desired_marker = mmr ? mmr_marker : arith_marker;

                if (e - p < 2)
                    return 0;

                while (p[0] != desired_marker[0] || p[1] != desired_marker[1]) {
                    p++;
                    if (e - p < 2)
                        return 0;
                }
                p += 2;

                if (e - p < 4)
                        return 0;
                segment->rows = jbig2_get_uint32(p);
                p += 4;

                segment->data_length = (size_t) (p - s);
                jbig2_error(ctx, JBIG2_SEVERITY_INFO, segment->number, "unknown length determined to be %lu", (long) segment->data_length);
            }
            else if (segment->data_length > ctx->buf_wr_ix - ctx->buf_rd_ix)
                    return 0;

            code = jbig2_parse_segment(ctx, segment, ctx->buf + ctx->buf_rd_ix);
            ctx->buf_rd_ix += segment->data_length;
            ctx->segment_index++;
            if (ctx->state == JBIG2_FILE_RANDOM_BODIES) {
                if (ctx->segment_index == ctx->n_segments)
                    ctx->state = JBIG2_FILE_EOF;
            } else {
                ctx->state = JBIG2_FILE_SEQUENTIAL_HEADER;
            }
            if (code < 0) {
                ctx->state = JBIG2_FILE_EOF;
                return jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "failed to decode; treating as end of file");
            }
            break;
        case JBIG2_FILE_EOF:
            if (ctx->buf_rd_ix == ctx->buf_wr_ix)
                return 0;
            return jbig2_error(ctx, JBIG2_SEVERITY_WARNING, JBIG2_UNKNOWN_SEGMENT_NUMBER, "garbage beyond end of file");
        }
    }
}

Jbig2Allocator *
jbig2_ctx_free(Jbig2Ctx *ctx)
{
    Jbig2Allocator *ca;
    uint32_t i;

    if (ctx == NULL)
        return NULL;

    ca = ctx->allocator;
    jbig2_free(ca, ctx->buf);
    if (ctx->segments != NULL) {
        for (i = 0; i < ctx->n_segments; i++)
            jbig2_free_segment(ctx, ctx->segments[i]);
        jbig2_free(ca, ctx->segments);
    }

    if (ctx->pages != NULL) {
        for (i = 0; i <= ctx->current_page; i++)
            if (ctx->pages[i].image != NULL)
                jbig2_image_release(ctx, ctx->pages[i].image);
        jbig2_free(ca, ctx->pages);
    }

    jbig2_free(ca, ctx);

    return ca;
}

Jbig2GlobalCtx *
jbig2_make_global_ctx(Jbig2Ctx *ctx)
{
    return (Jbig2GlobalCtx *) ctx;
}

Jbig2Allocator *
jbig2_global_ctx_free(Jbig2GlobalCtx *global_ctx)
{
    return jbig2_ctx_free((Jbig2Ctx *) global_ctx);
}

typedef struct {
    Jbig2WordStream super;
    const byte *data;
    size_t size;
} Jbig2WordStreamBuf;

static int
jbig2_word_stream_buf_get_next_word(Jbig2Ctx *ctx, Jbig2WordStream *self, size_t offset, uint32_t *word)
{
    Jbig2WordStreamBuf *z = (Jbig2WordStreamBuf *) self;
    uint32_t val = 0;
    int ret = 0;

    if (self == NULL || word == NULL) {
        return jbig2_error(ctx, JBIG2_SEVERITY_FATAL, JBIG2_UNKNOWN_SEGMENT_NUMBER, "failed to read next word of stream because stream or output missing");
    }
    if (offset >= z->size) {
        *word = 0;
        return 0;
    }

    if (offset < z->size) {
        val = (uint32_t) z->data[offset] << 24;
        ret++;
    }
    if (offset + 1 < z->size) {
        val |= (uint32_t) z->data[offset + 1] << 16;
        ret++;
    }
    if (offset + 2 < z->size) {
        val |= (uint32_t) z->data[offset + 2] << 8;
        ret++;
    }
    if (offset + 3 < z->size) {
        val |= z->data[offset + 3];
        ret++;
    }
    *word = val;
    return ret;
}

Jbig2WordStream *
jbig2_word_stream_buf_new(Jbig2Ctx *ctx, const byte *data, size_t size)
{
    Jbig2WordStreamBuf *result = jbig2_new(ctx, Jbig2WordStreamBuf, 1);

    if (result == NULL) {
        jbig2_error(ctx, JBIG2_SEVERITY_FATAL, JBIG2_UNKNOWN_SEGMENT_NUMBER, "failed to allocate word stream");
        return NULL;
    }

    result->super.get_next_word = jbig2_word_stream_buf_get_next_word;
    result->data = data;
    result->size = size;

    return &result->super;
}

void
jbig2_word_stream_buf_free(Jbig2Ctx *ctx, Jbig2WordStream *ws)
{
    jbig2_free(ctx->allocator, ws);
}

#ifdef MEMENTO
#undef free
#undef realloc
#endif

void
jbig2_free(Jbig2Allocator *allocator, void *p)
{
    allocator->free(allocator, p);
}

void *
jbig2_realloc(Jbig2Allocator *allocator, void *p, size_t size, size_t num)
{

    if (num > 0 && size >= SIZE_MAX / num)
        return NULL;
    return allocator->realloc(allocator, p, size * num);
}

#ifdef HAVE_CONFIG_H
#endif

#include <stdio.h>
#include <stdlib.h>

#ifndef _JBIG2_ARITH_H
#define _JBIG2_ARITH_H

typedef struct _Jbig2ArithState Jbig2ArithState;

typedef unsigned char Jbig2ArithCx;

Jbig2ArithState *jbig2_arith_new(Jbig2Ctx *ctx, Jbig2WordStream *ws);

int jbig2_arith_decode(Jbig2Ctx *ctx, Jbig2ArithState *as, Jbig2ArithCx *pcx);

bool jbig2_arith_has_reached_marker(Jbig2ArithState *as);

#endif

struct _Jbig2ArithState {
    uint32_t C;
    uint32_t A;

    int CT;

    uint32_t next_word;
    size_t next_word_bytes;
    int err;

    Jbig2WordStream *ws;
    size_t offset;
};

static int
jbig2_arith_bytein(Jbig2Ctx *ctx, Jbig2ArithState *as)
{
    byte B;

    if (as->err != 0) {
        jbig2_error(ctx, JBIG2_SEVERITY_FATAL, JBIG2_UNKNOWN_SEGMENT_NUMBER, "failed to read from underlying stream during arithmetic decoding");
        return -1;
    }
    if (as->next_word_bytes == 0) {
        jbig2_error(ctx, JBIG2_SEVERITY_FATAL, JBIG2_UNKNOWN_SEGMENT_NUMBER, "failed to read beyond end of underlying stream during arithmetic decoding");
        return -1;
    }

    B = (byte)((as->next_word >> 24) & 0xFF);
    if (B == 0xFF) {
        byte B1;

        if (as->next_word_bytes <= 1) {
            int ret = as->ws->get_next_word(ctx, as->ws, as->offset, &as->next_word);
            if (ret < 0) {
                as->err = 1;
                return jbig2_error(ctx, JBIG2_SEVERITY_WARNING, JBIG2_UNKNOWN_SEGMENT_NUMBER, "failed to check for marker code due to failure in underlying stream during arithmetic decoding");
            }
            as->next_word_bytes = (size_t) ret;

            if (as->next_word_bytes == 0) {
                jbig2_error(ctx, JBIG2_SEVERITY_WARNING, JBIG2_UNKNOWN_SEGMENT_NUMBER, "failed to read end of possible terminating marker code, assuming terminating marker code");
                as->next_word = 0xFF900000;
                as->next_word_bytes = 2;
                as->C += 0xFF00;
                as->CT = 8;
                return 0;
            }

            as->offset += as->next_word_bytes;

            B1 = (byte)((as->next_word >> 24) & 0xFF);
            if (B1 > 0x8F) {
#ifdef JBIG2_DEBUG_ARITH
                fprintf(stderr, "read %02x (aa)\n", B);
#endif
                as->CT = 8;
                as->next_word = 0xFF000000 | (as->next_word >> 8);
                as->next_word_bytes = 2;
                as->offset--;
            } else {
#ifdef JBIG2_DEBUG_ARITH
                fprintf(stderr, "read %02x (a)\n", B);
#endif
                as->C += 0xFE00 - (B1 << 9);
                as->CT = 7;
            }
        } else {
            B1 = (byte)((as->next_word >> 16) & 0xFF);
            if (B1 > 0x8F) {
#ifdef JBIG2_DEBUG_ARITH
                fprintf(stderr, "read %02x (ba)\n", B);
#endif
                as->CT = 8;
            } else {
                as->next_word_bytes--;
                as->next_word <<= 8;
#ifdef JBIG2_DEBUG_ARITH
                fprintf(stderr, "read %02x (b)\n", B);
#endif

                as->C += 0xFE00 - (B1 << 9);
                as->CT = 7;
            }
        }
    } else {
#ifdef JBIG2_DEBUG_ARITH
        fprintf(stderr, "read %02x\n", B);
#endif
        as->next_word <<= 8;
        as->next_word_bytes--;

        if (as->next_word_bytes == 0) {
            int ret = as->ws->get_next_word(ctx, as->ws, as->offset, &as->next_word);
            if (ret < 0) {
                as->err = 1;
                return jbig2_error(ctx, JBIG2_SEVERITY_WARNING, JBIG2_UNKNOWN_SEGMENT_NUMBER, "failed to read from underlying stream during arithmetic decoding");
            }
            as->next_word_bytes = (size_t) ret;

            if (as->next_word_bytes == 0) {
                jbig2_error(ctx, JBIG2_SEVERITY_WARNING, JBIG2_UNKNOWN_SEGMENT_NUMBER, "failed to find terminating marker code before end of underlying stream, assuming terminating marker code");
                as->next_word = 0xFF900000;
                as->next_word_bytes = 2;
                as->C += 0xFF00;
                as->CT = 8;
                return 0;
            }

            as->offset += as->next_word_bytes;
        }

        B = (byte)((as->next_word >> 24) & 0xFF);
        as->C += 0xFF00 - (B << 8);
        as->CT = 8;
    }

    return 0;
}

Jbig2ArithState *
jbig2_arith_new(Jbig2Ctx *ctx, Jbig2WordStream *ws)
{
    Jbig2ArithState *result;
    int ret;

    result = jbig2_new(ctx, Jbig2ArithState, 1);
    if (result == NULL) {
        jbig2_error(ctx, JBIG2_SEVERITY_FATAL, JBIG2_UNKNOWN_SEGMENT_NUMBER, "failed to allocate arithmetic coding state");
        return NULL;
    }

    result->err = 0;
    result->ws = ws;
    result->offset = 0;

    ret = result->ws->get_next_word(ctx, result->ws, result->offset, &result->next_word);
    if (ret < 0) {
        jbig2_free(ctx->allocator, result);
        jbig2_error(ctx, JBIG2_SEVERITY_WARNING, JBIG2_UNKNOWN_SEGMENT_NUMBER, "failed to initialize underlying stream of arithmetic decoder");
        return NULL;
    }

    result->next_word_bytes = (size_t) ret;
    if (result->next_word_bytes == 0) {
        jbig2_free(ctx->allocator, result);
        jbig2_error(ctx, JBIG2_SEVERITY_FATAL, JBIG2_UNKNOWN_SEGMENT_NUMBER, "failed to read first byte from underlying stream when initializing arithmetic decoder");
        return NULL;
    }

    result->offset += result->next_word_bytes;

    result->C = (~(result->next_word >> 8)) & 0xFF0000;

    if (jbig2_arith_bytein(ctx, result) < 0) {
        jbig2_free(ctx->allocator, result);
        jbig2_error(ctx, JBIG2_SEVERITY_WARNING, JBIG2_UNKNOWN_SEGMENT_NUMBER, "failed to read second byte from underlying stream when initializing arithmetic decoder");
        return NULL;
    }

    result->C <<= 7;
    result->CT -= 7;
    result->A = 0x8000;

    return result;
}

#define MAX_QE_ARRAY_SIZE 47

typedef struct {
    uint16_t Qe;
    byte mps_xor;
    byte lps_xor;
} Jbig2ArithQe;

#define MPS(index, nmps) ((index) ^ (nmps))
#define LPS(index, nlps, swtch) ((index) ^ (nlps) ^ ((swtch) << 7))

static const Jbig2ArithQe jbig2_arith_Qe[MAX_QE_ARRAY_SIZE] = {
    {0x5601, MPS(0, 1), LPS(0, 1, 1)},
    {0x3401, MPS(1, 2), LPS(1, 6, 0)},
    {0x1801, MPS(2, 3), LPS(2, 9, 0)},
    {0x0AC1, MPS(3, 4), LPS(3, 12, 0)},
    {0x0521, MPS(4, 5), LPS(4, 29, 0)},
    {0x0221, MPS(5, 38), LPS(5, 33, 0)},
    {0x5601, MPS(6, 7), LPS(6, 6, 1)},
    {0x5401, MPS(7, 8), LPS(7, 14, 0)},
    {0x4801, MPS(8, 9), LPS(8, 14, 0)},
    {0x3801, MPS(9, 10), LPS(9, 14, 0)},
    {0x3001, MPS(10, 11), LPS(10, 17, 0)},
    {0x2401, MPS(11, 12), LPS(11, 18, 0)},
    {0x1C01, MPS(12, 13), LPS(12, 20, 0)},
    {0x1601, MPS(13, 29), LPS(13, 21, 0)},
    {0x5601, MPS(14, 15), LPS(14, 14, 1)},
    {0x5401, MPS(15, 16), LPS(15, 14, 0)},
    {0x5101, MPS(16, 17), LPS(16, 15, 0)},
    {0x4801, MPS(17, 18), LPS(17, 16, 0)},
    {0x3801, MPS(18, 19), LPS(18, 17, 0)},
    {0x3401, MPS(19, 20), LPS(19, 18, 0)},
    {0x3001, MPS(20, 21), LPS(20, 19, 0)},
    {0x2801, MPS(21, 22), LPS(21, 19, 0)},
    {0x2401, MPS(22, 23), LPS(22, 20, 0)},
    {0x2201, MPS(23, 24), LPS(23, 21, 0)},
    {0x1C01, MPS(24, 25), LPS(24, 22, 0)},
    {0x1801, MPS(25, 26), LPS(25, 23, 0)},
    {0x1601, MPS(26, 27), LPS(26, 24, 0)},
    {0x1401, MPS(27, 28), LPS(27, 25, 0)},
    {0x1201, MPS(28, 29), LPS(28, 26, 0)},
    {0x1101, MPS(29, 30), LPS(29, 27, 0)},
    {0x0AC1, MPS(30, 31), LPS(30, 28, 0)},
    {0x09C1, MPS(31, 32), LPS(31, 29, 0)},
    {0x08A1, MPS(32, 33), LPS(32, 30, 0)},
    {0x0521, MPS(33, 34), LPS(33, 31, 0)},
    {0x0441, MPS(34, 35), LPS(34, 32, 0)},
    {0x02A1, MPS(35, 36), LPS(35, 33, 0)},
    {0x0221, MPS(36, 37), LPS(36, 34, 0)},
    {0x0141, MPS(37, 38), LPS(37, 35, 0)},
    {0x0111, MPS(38, 39), LPS(38, 36, 0)},
    {0x0085, MPS(39, 40), LPS(39, 37, 0)},
    {0x0049, MPS(40, 41), LPS(40, 38, 0)},
    {0x0025, MPS(41, 42), LPS(41, 39, 0)},
    {0x0015, MPS(42, 43), LPS(42, 40, 0)},
    {0x0009, MPS(43, 44), LPS(43, 41, 0)},
    {0x0005, MPS(44, 45), LPS(44, 42, 0)},
    {0x0001, MPS(45, 45), LPS(45, 43, 0)},
    {0x5601, MPS(46, 46), LPS(46, 46, 0)}
};

static int
jbig2_arith_renormd(Jbig2Ctx *ctx, Jbig2ArithState *as)
{

    do {
        if (as->CT == 0 && jbig2_arith_bytein(ctx, as) < 0) {
            return jbig2_error(ctx, JBIG2_SEVERITY_WARNING, JBIG2_UNKNOWN_SEGMENT_NUMBER, "failed to read byte from compressed data stream");
        }
        as->A <<= 1;
        as->C <<= 1;
        as->CT--;
    } while ((as->A & 0x8000) == 0);

    return 0;
}

int
jbig2_arith_decode(Jbig2Ctx *ctx, Jbig2ArithState *as, Jbig2ArithCx *pcx)
{
    Jbig2ArithCx cx = *pcx;
    const Jbig2ArithQe *pqe;
    unsigned int index = cx & 0x7f;
    bool D;

    if (index >= MAX_QE_ARRAY_SIZE) {
        return jbig2_error(ctx, JBIG2_SEVERITY_FATAL, JBIG2_UNKNOWN_SEGMENT_NUMBER, "failed to determine probability estimate because index out of range");
    }

    pqe = &jbig2_arith_Qe[index];

    as->A -= pqe->Qe;
    if ((as->C >> 16) < as->A) {
        if ((as->A & 0x8000) == 0) {

            if (as->A < pqe->Qe) {
                D = 1 - (cx >> 7);
                *pcx ^= pqe->lps_xor;
            } else {
                D = cx >> 7;
                *pcx ^= pqe->mps_xor;
            }
            if (jbig2_arith_renormd(ctx, as) < 0) {
                return jbig2_error(ctx, JBIG2_SEVERITY_WARNING, JBIG2_UNKNOWN_SEGMENT_NUMBER, "failed to renormalize decoder");
            }

            return D;
        } else {
            return cx >> 7;
        }
    } else {
        as->C -= (as->A) << 16;

        if (as->A < pqe->Qe) {
            as->A = pqe->Qe;
            D = cx >> 7;
            *pcx ^= pqe->mps_xor;
        } else {
            as->A = pqe->Qe;
            D = 1 - (cx >> 7);
            *pcx ^= pqe->lps_xor;
        }
        if (jbig2_arith_renormd(ctx, as) < 0) {
            return jbig2_error(ctx, JBIG2_SEVERITY_WARNING, JBIG2_UNKNOWN_SEGMENT_NUMBER, "failed to renormalize decoder");
        }

        return D;
    }
}

#ifdef TEST

static const byte test_stream[] = {
    0x84, 0xC7, 0x3B, 0xFC, 0xE1, 0xA1, 0x43, 0x04, 0x02, 0x20, 0x00, 0x00,
    0x41, 0x0D, 0xBB, 0x86, 0xF4, 0x31, 0x7F, 0xFF, 0x88, 0xFF, 0x37, 0x47,
    0x1A, 0xDB, 0x6A, 0xDF, 0xFF, 0xAC,
    0x00, 0x00
};

#if defined(JBIG2_DEBUG) || defined(JBIG2_DEBUG_ARITH)
static void
jbig2_arith_trace(Jbig2ArithState *as, Jbig2ArithCx cx)
{
    fprintf(stderr, "I = %2d, MPS = %d, A = %04x, CT = %2d, C = %08x\n", cx & 0x7f, cx >> 7, as->A, as->CT, as->C);
}
#endif

static int
test_get_word(Jbig2Ctx *ctx, Jbig2WordStream *self, size_t offset, uint32_t *word)
{
    uint32_t val = 0;
    int ret = 0;

    (void) ctx;

    if (self == NULL || word == NULL)
        return -1;
    if (offset >= sizeof (test_stream))
        return 0;

    if (offset < sizeof(test_stream)) {
        val |= test_stream[offset] << 24;
        ret++;
    }
    if (offset + 1 < sizeof(test_stream)) {
        val |= test_stream[offset + 1] << 16;
        ret++;
    }
    if (offset + 2 < sizeof(test_stream)) {
        val |= test_stream[offset + 2] << 8;
        ret++;
    }
    if (offset + 3 < sizeof(test_stream)) {
        val |= test_stream[offset + 3];
        ret++;
    }
    *word = val;
    return ret;
}

int
main(int argc, char **argv)
{
    Jbig2Ctx *ctx;
    Jbig2WordStream ws;
    Jbig2ArithState *as;
    int i;
    Jbig2ArithCx cx = 0;

    (void) argc;
    (void) argv;

    ctx = jbig2_ctx_new(NULL, 0, NULL, NULL, NULL);

    ws.get_next_word = test_get_word;
    as = jbig2_arith_new(ctx, &ws);
#ifdef JBIG2_DEBUG_ARITH
    jbig2_arith_trace(as, cx);
#endif

    for (i = 0; i < 256; i++) {
#ifdef JBIG2_DEBUG_ARITH
        int D =
#else
        (void)
#endif
            jbig2_arith_decode(ctx, as, &cx);

#ifdef JBIG2_DEBUG_ARITH
        fprintf(stderr, "%3d: D = %d, ", i, D);
        jbig2_arith_trace(as, cx);
#endif
    }

    jbig2_free(ctx->allocator, as);

    jbig2_ctx_free(ctx);

    return 0;
}
#endif

#ifdef HAVE_CONFIG_H
#endif

#include <stddef.h>
#include <string.h>

#ifdef VERBOSE
#include <stdio.h>
#endif

#ifndef _JBIG2_ARITH_IAID_H
#define _JBIG2_ARITH_IAID_H

typedef struct _Jbig2ArithIaidCtx Jbig2ArithIaidCtx;

Jbig2ArithIaidCtx *jbig2_arith_iaid_ctx_new(Jbig2Ctx *ctx, uint8_t SBSYMCODELEN);

int jbig2_arith_iaid_decode(Jbig2Ctx *ctx, Jbig2ArithIaidCtx *actx, Jbig2ArithState *as, int32_t *p_result);

void jbig2_arith_iaid_ctx_free(Jbig2Ctx *ctx, Jbig2ArithIaidCtx *iax);

#endif

struct _Jbig2ArithIaidCtx {
    uint8_t SBSYMCODELEN;
    Jbig2ArithCx *IAIDx;
};

Jbig2ArithIaidCtx *
jbig2_arith_iaid_ctx_new(Jbig2Ctx *ctx, uint8_t SBSYMCODELEN)
{
    Jbig2ArithIaidCtx *result;
    size_t ctx_size;

    if (SBSYMCODELEN > 31 || sizeof(ctx_size) * 8 <= SBSYMCODELEN)
    {
        jbig2_error(ctx, JBIG2_SEVERITY_FATAL, JBIG2_UNKNOWN_SEGMENT_NUMBER, "requested IAID arithmetic coding state size too large");
        return NULL;
    }

    ctx_size = (size_t) 1U << SBSYMCODELEN;

    result = jbig2_new(ctx, Jbig2ArithIaidCtx, 1);
    if (result == NULL) {
        jbig2_error(ctx, JBIG2_SEVERITY_FATAL, JBIG2_UNKNOWN_SEGMENT_NUMBER, "failed to allocate IAID arithmetic coding state");
        return NULL;
    }

    result->SBSYMCODELEN = SBSYMCODELEN;
    result->IAIDx = jbig2_new(ctx, Jbig2ArithCx, ctx_size);
    if (result->IAIDx == NULL)
    {
        jbig2_free(ctx->allocator, result);
        jbig2_error(ctx, JBIG2_SEVERITY_FATAL, JBIG2_UNKNOWN_SEGMENT_NUMBER, "failed to allocate symbol ID in IAID arithmetic coding state");
        return NULL;
    }

    memset(result->IAIDx, 0, ctx_size);
    return result;
}

int
jbig2_arith_iaid_decode(Jbig2Ctx *ctx, Jbig2ArithIaidCtx *actx, Jbig2ArithState *as, int32_t *p_result)
{
    Jbig2ArithCx *IAIDx = actx->IAIDx;
    uint8_t SBSYMCODELEN = actx->SBSYMCODELEN;

    uint32_t PREV = 1;
    int D;
    int i;

    for (i = 0; i < SBSYMCODELEN; i++) {
        D = jbig2_arith_decode(ctx, as, &IAIDx[PREV]);
        if (D < 0)
            return jbig2_error(ctx, JBIG2_SEVERITY_WARNING, JBIG2_UNKNOWN_SEGMENT_NUMBER, "failed to decode IAIDx code");
#ifdef VERBOSE
        fprintf(stderr, "IAID%x: D = %d\n", PREV, D);
#endif
        PREV = (PREV << 1) | (uint32_t)D;
    }

    PREV -= (1U << SBSYMCODELEN);
#ifdef VERBOSE
    fprintf(stderr, "IAID result: %d\n", PREV);
#endif
    *p_result = (int)PREV;
    return 0;
}

void
jbig2_arith_iaid_ctx_free(Jbig2Ctx *ctx, Jbig2ArithIaidCtx *iax)
{
    if (iax != NULL) {
        jbig2_free(ctx->allocator, iax->IAIDx);
        jbig2_free(ctx->allocator, iax);
    }
}

#ifdef HAVE_CONFIG_H
#endif

#include <stddef.h>
#include <string.h>

#ifndef _JBIG2_ARITH_INT_H
#define _JBIG2_ARITH_INT_H

typedef struct _Jbig2ArithIntCtx Jbig2ArithIntCtx;

Jbig2ArithIntCtx *jbig2_arith_int_ctx_new(Jbig2Ctx *ctx);

int jbig2_arith_int_decode(Jbig2Ctx *ctx, Jbig2ArithIntCtx *actx, Jbig2ArithState *as, int32_t *p_result);

void jbig2_arith_int_ctx_free(Jbig2Ctx *ctx, Jbig2ArithIntCtx *iax);

#endif

struct _Jbig2ArithIntCtx {
    Jbig2ArithCx IAx[512];
};

Jbig2ArithIntCtx *
jbig2_arith_int_ctx_new(Jbig2Ctx *ctx)
{
    Jbig2ArithIntCtx *result = jbig2_new(ctx, Jbig2ArithIntCtx, 1);

    if (result == NULL) {
        jbig2_error(ctx, JBIG2_SEVERITY_FATAL, JBIG2_UNKNOWN_SEGMENT_NUMBER, "failed to allocate arithmetic integer coding state");
        return NULL;
    } else {
        memset(result->IAx, 0, sizeof(result->IAx));
    }

    return result;
}

int
jbig2_arith_int_decode(Jbig2Ctx *ctx, Jbig2ArithIntCtx *actx, Jbig2ArithState *as, int32_t *p_result)
{
    Jbig2ArithCx *IAx = actx->IAx;
    int PREV = 1;
    int S;
    int32_t V;
    int bit;
    int n_tail, offset;
    int i;

    S = jbig2_arith_decode(ctx, as, &IAx[PREV]);
    if (S < 0)
        return jbig2_error(ctx, JBIG2_SEVERITY_WARNING, JBIG2_UNKNOWN_SEGMENT_NUMBER, "failed to decode IAx S");
    PREV = (PREV << 1) | S;

    bit = jbig2_arith_decode(ctx, as, &IAx[PREV]);
    if (bit < 0)
        return jbig2_error(ctx, JBIG2_SEVERITY_WARNING, JBIG2_UNKNOWN_SEGMENT_NUMBER, "failed to decode IAx decision bit 0");
    PREV = (PREV << 1) | bit;
    if (bit) {
        bit = jbig2_arith_decode(ctx, as, &IAx[PREV]);
        if (bit < 0)
            return jbig2_error(ctx, JBIG2_SEVERITY_WARNING, JBIG2_UNKNOWN_SEGMENT_NUMBER, "failed to decode IAx decision bit 1");
        PREV = (PREV << 1) | bit;

        if (bit) {
            bit = jbig2_arith_decode(ctx, as, &IAx[PREV]);
            if (bit < 0)
                return jbig2_error(ctx, JBIG2_SEVERITY_WARNING, JBIG2_UNKNOWN_SEGMENT_NUMBER, "failed to decode IAx decision bit 2");
            PREV = (PREV << 1) | bit;

            if (bit) {
                bit = jbig2_arith_decode(ctx, as, &IAx[PREV]);
                if (bit < 0)
                    return jbig2_error(ctx, JBIG2_SEVERITY_WARNING, JBIG2_UNKNOWN_SEGMENT_NUMBER, "failed to decode IAx decision bit 3");
                PREV = (PREV << 1) | bit;

                if (bit) {
                    bit = jbig2_arith_decode(ctx, as, &IAx[PREV]);
                    if (bit < 0)
                        return jbig2_error(ctx, JBIG2_SEVERITY_WARNING, JBIG2_UNKNOWN_SEGMENT_NUMBER, "failed to decode IAx decision bit 4");
                    PREV = (PREV << 1) | bit;

                    if (bit) {
                        n_tail = 32;
                        offset = 4436;
                    } else {
                        n_tail = 12;
                        offset = 340;
                    }
                } else {
                    n_tail = 8;
                    offset = 84;
                }
            } else {
                n_tail = 6;
                offset = 20;
            }
        } else {
            n_tail = 4;
            offset = 4;
        }
    } else {
        n_tail = 2;
        offset = 0;
    }

    V = 0;
    for (i = 0; i < n_tail; i++) {
        bit = jbig2_arith_decode(ctx, as, &IAx[PREV]);
        if (bit < 0)
            return jbig2_error(ctx, JBIG2_SEVERITY_WARNING, JBIG2_UNKNOWN_SEGMENT_NUMBER, "failed to decode IAx V bit %d", i);
        PREV = ((PREV << 1) & 511) | (PREV & 256) | bit;
        V = (V << 1) | bit;
    }

    if (V > INT32_MAX - offset)
        V = INT32_MAX;
    else
        V += offset;
    V = S ? -V : V;
    *p_result = V;
    return S && V == 0 ? 1 : 0;
}

void
jbig2_arith_int_ctx_free(Jbig2Ctx *ctx, Jbig2ArithIntCtx *iax)
{
    jbig2_free(ctx->allocator, iax);
}

#ifdef HAVE_CONFIG_H
#endif

#include <stddef.h>
#include <string.h>

#ifdef OUTPUT_PBM
#include <stdio.h>
#endif

#ifndef _JBIG2_GENERIC_H
#define _JBIG2_GENERIC_H

typedef struct {
    bool MMR;

    int GBTEMPLATE;
    bool TPGDON;
    bool USESKIP;
    Jbig2Image *SKIP;
    int8_t gbat[8];
} Jbig2GenericRegionParams;

int jbig2_generic_stats_size(Jbig2Ctx *ctx, int template);

int
jbig2_decode_generic_region(Jbig2Ctx *ctx,
                            Jbig2Segment *segment, const Jbig2GenericRegionParams *params, Jbig2ArithState *as, Jbig2Image *image, Jbig2ArithCx *GB_stats);

int jbig2_immediate_generic_region(Jbig2Ctx *ctx, Jbig2Segment *segment, const uint8_t *segment_data);

#endif

#ifndef _JBIG2_MMR_H
#define _JBIG2_MMR_H

int
jbig2_decode_generic_mmr(Jbig2Ctx *ctx, Jbig2Segment *segment, const Jbig2GenericRegionParams *params, const byte *data, size_t size, Jbig2Image *image);

int
jbig2_decode_halftone_mmr(Jbig2Ctx *ctx, const Jbig2GenericRegionParams *params, const byte *data, size_t size, Jbig2Image *image, size_t *consumed_bytes);

#endif

static inline int
jbig2_image_get_pixel_fast(Jbig2Image *image, int64_t x, int64_t y)
{
    size_t sx = (size_t) x;
    size_t sy = (size_t) y;
    size_t byte = (sx >> 3) + sy * image->stride;
    int bit = 7 - ((int) (sx & 7));

    return ((image->data[byte] >> bit) & 1);
}

int
jbig2_generic_stats_size(Jbig2Ctx *ctx, int template)
{
    int stats_size = template == 0 ? 1 << 16 : template == 1 ? 1 << 13 : 1 << 10;

    (void) ctx;

    return stats_size;
}

static int
jbig2_decode_generic_template0(Jbig2Ctx *ctx,
                               Jbig2Segment *segment,
                               const Jbig2GenericRegionParams *params, Jbig2ArithState *as,
                               Jbig2Image *image, Jbig2ArithCx *GB_stats)
{
    const int64_t GBW = image->width;
    const int64_t GBH = image->height;
    const uint32_t rowstride = image->stride;
    int64_t x, y;
    byte *line2 = NULL;
    byte *line1 = NULL;
    byte *gbreg_line = (byte *) image->data;

    (void) ctx;
    (void) params;

#ifdef OUTPUT_PBM
    printf("P4\n%u %u\n",
        (uint32_t) GBW,
        (uint32_t) GBH);
#endif

    if (GBW <= 0)
        return 0;

    for (y = 0; y < GBH; y++) {
        uint32_t CONTEXT;
        uint32_t line_m1;
        uint32_t line_m2;
        int64_t padded_width = (GBW + 7) & -8;

        line_m1 = line1 ? line1[0] : 0;
        line_m2 = line2 ? line2[0] << 6 : 0;
        CONTEXT = (line_m1 & 0x7f0) | (line_m2 & 0xf800);

        for (x = 0; x < padded_width; x += 8) {
            byte result = 0;
            int64_t x_minor;
            int64_t minor_width = GBW - x > 8 ? 8 : GBW - x;

            if (line1)
                line_m1 = (line_m1 << 8) | (x + 8 < GBW ? line1[(x >> 3) + 1] : 0);

            if (line2)
                line_m2 = (line_m2 << 8) | (x + 8 < GBW ? line2[(x >> 3) + 1] << 6 : 0);

            for (x_minor = 0; x_minor < minor_width; x_minor++) {
                int bit;

                bit = jbig2_arith_decode(ctx, as, &GB_stats[CONTEXT]);
                if (bit < 0)
                    return jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "failed to decode arithmetic code when handling generic template0 optimized");
                result |= bit << (7 - x_minor);
                CONTEXT = ((CONTEXT & 0x7bf7) << 1) | bit | ((line_m1 >> (7 - x_minor)) & 0x10) | ((line_m2 >> (7 - x_minor)) & 0x800);
            }
            gbreg_line[x >> 3] = result;
        }
#ifdef OUTPUT_PBM
        fwrite(gbreg_line, 1, rowstride, stdout);
#endif
        line2 = line1;
        line1 = gbreg_line;
        gbreg_line += rowstride;
    }

    return 0;
}

#define pixel_outside_field(x, y) \
    ((y) < -128 || (y) > 0 || (x) < -128 || ((y) < 0 && (x) > 127) || ((y) == 0 && (x) >= 0))

static int
jbig2_decode_generic_template0_unopt(Jbig2Ctx *ctx,
                                     Jbig2Segment *segment,
                                     const Jbig2GenericRegionParams *params, Jbig2ArithState *as, Jbig2Image *image, Jbig2ArithCx *GB_stats)
{
    const int64_t GBW = image->width;
    const int64_t GBH = image->height;
    uint32_t CONTEXT;
    int64_t x, y;
    int bit;

    if (pixel_outside_field(params->gbat[0], params->gbat[1]) ||
        pixel_outside_field(params->gbat[2], params->gbat[3]) ||
        pixel_outside_field(params->gbat[4], params->gbat[5]) ||
        pixel_outside_field(params->gbat[6], params->gbat[7]))
        return jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number,
                           "adaptive template pixel is out of field");

    for (y = 0; y < GBH; y++) {
        uint32_t out_byte = 0;
        int out_bits_to_go_in_byte = 8;
        uint8_t *d = &image->data[image->stride * y];
        uint32_t pd = 0;
        uint32_t ppd = 0;
        uint8_t *pline = NULL;
        uint8_t *ppline = NULL;
        if (y >= 1)
        {
            pline  = &image->data[image->stride * (y-1)];
            pd = (*pline++ << 8);
            if (GBW > 8)
                pd |= *pline++;
        }
        if (y >= 2) {
            ppline = &image->data[image->stride * (y-2)];
            ppd = (*ppline++ << 8);
            if (GBW > 8)
                ppd |= *ppline++;
        }
        for (x = 0; x < GBW; x++) {
            if (params->USESKIP && jbig2_image_get_pixel(params->SKIP, x, y)) {
                bit = 0;
            } else {
                CONTEXT  = out_byte & 0x000F;
                CONTEXT |= jbig2_image_get_pixel(image, x + params->gbat[0], y + params->gbat[1]) << 4;
                CONTEXT |= (pd>>8) & 0x03E0;
                CONTEXT |= jbig2_image_get_pixel(image, x + params->gbat[2], y + params->gbat[3]) << 10;
                CONTEXT |= jbig2_image_get_pixel(image, x + params->gbat[4], y + params->gbat[5]) << 11;
                CONTEXT |= (ppd>>2) & 0x7000;
                CONTEXT |= jbig2_image_get_pixel(image, x + params->gbat[6], y + params->gbat[7]) << 15;
                bit = jbig2_arith_decode(ctx, as, &GB_stats[CONTEXT]);
                if (bit < 0)
                    return jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "failed to decode arithmetic code when handling generic template0 unoptimized");
            }
            pd = pd<<1;
            ppd = ppd<<1;
            out_byte = (out_byte<<1) | bit;
            out_bits_to_go_in_byte--;
            *d = out_byte<<out_bits_to_go_in_byte;
            if (out_bits_to_go_in_byte == 0) {
                out_bits_to_go_in_byte = 8;
                d++;
                if (x+9 < GBW && pline != NULL) {
                    pd |= *pline++;
                    if (ppline != NULL)
                        ppd |= *ppline++;
                }
            }
        }
        if (out_bits_to_go_in_byte != 8)
            *d = (uint8_t)out_byte<<out_bits_to_go_in_byte;
    }
    return 0;
}

static int
jbig2_decode_generic_template1_unopt(Jbig2Ctx *ctx,
                                     Jbig2Segment *segment,
                                     const Jbig2GenericRegionParams *params, Jbig2ArithState *as, Jbig2Image *image, Jbig2ArithCx *GB_stats)
{
    const int64_t GBW = image->width;
    const int64_t GBH = image->height;
    uint32_t CONTEXT;
    int64_t x, y;
    int bit;

    if (pixel_outside_field(params->gbat[0], params->gbat[1]))
        return jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number,
                           "adaptive template pixel is out of field");

    for (y = 0; y < GBH; y++) {
        uint32_t out_byte = 0;
        int out_bits_to_go_in_byte = 8;
        uint8_t *d = &image->data[image->stride * y];
        uint32_t pd = 0;
        uint32_t ppd = 0;
        uint8_t *pline = NULL;
        uint8_t *ppline = NULL;
        if (y >= 1) {
            pline  = &image->data[image->stride * (y-1)];
            pd = (*pline++ << 8);
            if (GBW > 8)
                pd |= *pline++;
        }
        if (y >= 2) {
            ppline = &image->data[image->stride * (y-2)];
            ppd = (*ppline++ << 8);
            if (GBW > 8)
                ppd |= *ppline++;
        }
        for (x = 0; x < GBW; x++) {
            if (params->USESKIP && jbig2_image_get_pixel(params->SKIP, x, y)) {
                bit = 0;
            } else {
                CONTEXT  = out_byte & 0x0007;
                CONTEXT |= jbig2_image_get_pixel(image, x + params->gbat[0], y + params->gbat[1]) << 3;
                CONTEXT |= (pd>>9) & 0x01F0;
                CONTEXT |= (ppd>>4) & 0x1E00;
                bit = jbig2_arith_decode(ctx, as, &GB_stats[CONTEXT]);
                if (bit < 0)
                    return jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "failed to decode arithmetic code when handling generic template1 unoptimized");
            }
            pd = pd<<1;
            ppd = ppd<<1;
            out_byte = (out_byte<<1) | bit;
            out_bits_to_go_in_byte--;
            *d = out_byte<<out_bits_to_go_in_byte;
            if (out_bits_to_go_in_byte == 0) {
                out_bits_to_go_in_byte = 8;
                d++;
                if (x+9 < GBW && pline != NULL) {
                    pd |= *pline++;
                    if (ppline != NULL)
                        ppd |= *ppline++;
                }
            }
        }
        if (out_bits_to_go_in_byte != 8)
            *d = (uint8_t)out_byte<<out_bits_to_go_in_byte;
    }
    return 0;
}

static int
jbig2_decode_generic_template1(Jbig2Ctx *ctx,
                               Jbig2Segment *segment,
                               const Jbig2GenericRegionParams *params, Jbig2ArithState *as, Jbig2Image *image, Jbig2ArithCx *GB_stats)
{
    const int64_t GBW = image->width;
    const int64_t GBH = image->height;
    const uint32_t rowstride = image->stride;
    int64_t x, y;
    byte *line2 = NULL;
    byte *line1 = NULL;
    byte *gbreg_line = (byte *) image->data;

    (void) params;

#ifdef OUTPUT_PBM
    printf("P4\n%u %u\n",
        (uint32_t) GBW,
        (uint32_t) GBH);
#endif

    if (GBW <= 0)
        return 0;

    for (y = 0; y < GBH; y++) {
        uint32_t CONTEXT;
        uint32_t line_m1;
        uint32_t line_m2;
        int64_t padded_width = (GBW + 7) & -8;

        line_m1 = line1 ? line1[0] : 0;
        line_m2 = line2 ? line2[0] << 5 : 0;
        CONTEXT = ((line_m1 >> 1) & 0x1f8) | ((line_m2 >> 1) & 0x1e00);

        for (x = 0; x < padded_width; x += 8) {
            byte result = 0;
            int64_t x_minor;
            int64_t minor_width = GBW - x > 8 ? 8 : GBW - x;

            if (line1)
                line_m1 = (line_m1 << 8) | (x + 8 < GBW ? line1[(x >> 3) + 1] : 0);

            if (line2)
                line_m2 = (line_m2 << 8) | (x + 8 < GBW ? line2[(x >> 3) + 1] << 5 : 0);

            for (x_minor = 0; x_minor < minor_width; x_minor++) {
                int bit;

                bit = jbig2_arith_decode(ctx, as, &GB_stats[CONTEXT]);
                if (bit < 0)
                    return jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "failed to decode arithmetic code when handling generic template1 optimized");
                result |= bit << (7 - x_minor);
                CONTEXT = ((CONTEXT & 0xefb) << 1) | bit | ((line_m1 >> (8 - x_minor)) & 0x8) | ((line_m2 >> (8 - x_minor)) & 0x200);
            }
            gbreg_line[x >> 3] = result;
        }
#ifdef OUTPUT_PBM
        fwrite(gbreg_line, 1, rowstride, stdout);
#endif
        line2 = line1;
        line1 = gbreg_line;
        gbreg_line += rowstride;
    }

    return 0;
}

static int
jbig2_decode_generic_template2_unopt(Jbig2Ctx *ctx,
                               Jbig2Segment *segment,
                               const Jbig2GenericRegionParams *params, Jbig2ArithState *as, Jbig2Image *image, Jbig2ArithCx *GB_stats)
{
    const int64_t GBW = image->width;
    const int64_t GBH = image->height;
    uint32_t CONTEXT;
    int64_t x, y;
    int bit;

    if (pixel_outside_field(params->gbat[0], params->gbat[1]))
        return jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number,
                           "adaptive template pixel is out of field");

    for (y = 0; y < GBH; y++) {
        uint32_t out_byte = 0;
        int out_bits_to_go_in_byte = 8;
        uint8_t *d = &image->data[image->stride * y];
        uint8_t *pline  = &image->data[image->stride * (y-1)];
        uint8_t *ppline = &image->data[image->stride * (y-2)];
        uint32_t pd = 0;
        uint32_t ppd = 0;
        if (y > 0) {
            pd = (*pline++ << 8);
            if (GBW > 8)
                pd |= *pline++;
            if (y > 1) {
                ppd = (*ppline++ << 8);
                if (GBW > 8)
                    ppd |= *ppline++;
            }
        }
        for (x = 0; x < GBW; x++) {
            if (params->USESKIP && jbig2_image_get_pixel(params->SKIP, x, y)) {
                bit = 0;
            } else {
                CONTEXT  = out_byte & 0x003;
                CONTEXT |= jbig2_image_get_pixel(image, x + params->gbat[0], y + params->gbat[1]) << 2;
                CONTEXT |= (pd>>11) & 0x078;
                CONTEXT |= (ppd>>7) & 0x380;
                bit = jbig2_arith_decode(ctx, as, &GB_stats[CONTEXT]);
                if (bit < 0)
                    return jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "failed to decode arithmetic code when handling generic template2 unoptimized");
            }
            pd = pd<<1;
            ppd = ppd<<1;
            out_byte = (out_byte<<1) | bit;
            out_bits_to_go_in_byte--;
            *d = (uint8_t)out_byte<<out_bits_to_go_in_byte;
            if (out_bits_to_go_in_byte == 0) {
                out_bits_to_go_in_byte = 8;
                d++;
                if (x+9 < GBW && y > 0) {
                    pd |= *pline++;
                    if (y > 1)
                        ppd |= *ppline++;
                }
            }
        }
        if (out_bits_to_go_in_byte != 8)
            *d = (uint8_t)out_byte<<out_bits_to_go_in_byte;
    }

    return 0;
}

static int
jbig2_decode_generic_template2(Jbig2Ctx *ctx,
                                Jbig2Segment *segment,
                                const Jbig2GenericRegionParams *params, Jbig2ArithState *as, Jbig2Image *image, Jbig2ArithCx *GB_stats)
{
    const int64_t GBW = image->width;
    const int64_t GBH = image->height;
    const uint32_t rowstride = image->stride;
    int64_t x, y;
    byte *line2 = NULL;
    byte *line1 = NULL;
    byte *gbreg_line = (byte *) image->data;

    (void) params;

#ifdef OUTPUT_PBM
    printf("P4\n%u %u\n",
        (uint32_t) GBW,
        (uint32_t) GBH);
#endif

    if (GBW <= 0)
        return 0;

    for (y = 0; y < GBH; y++) {
        uint32_t CONTEXT;
        uint32_t line_m1;
        uint32_t line_m2;
        int64_t padded_width = (GBW + 7) & -8;

        line_m1 = line1 ? line1[0] : 0;
        line_m2 = line2 ? line2[0] << 4 : 0;
        CONTEXT = ((line_m1 >> 3) & 0x7c) | ((line_m2 >> 3) & 0x380);

        for (x = 0; x < padded_width; x += 8) {
            byte result = 0;
            int64_t x_minor;
            int64_t minor_width = GBW - x > 8 ? 8 : GBW - x;

            if (line1)
                line_m1 = (line_m1 << 8) | (x + 8 < GBW ? line1[(x >> 3) + 1] : 0);

            if (line2)
                line_m2 = (line_m2 << 8) | (x + 8 < GBW ? line2[(x >> 3) + 1] << 4 : 0);

            for (x_minor = 0; x_minor < minor_width; x_minor++) {
                int bit;

                bit = jbig2_arith_decode(ctx, as, &GB_stats[CONTEXT]);
                if (bit < 0)
                    return jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "failed to decode arithmetic code when handling generic template2 optimized");
                result |= bit << (7 - x_minor);
                CONTEXT = ((CONTEXT & 0x1bd) << 1) | bit | ((line_m1 >> (10 - x_minor)) & 0x4) | ((line_m2 >> (10 - x_minor)) & 0x80);
            }
            gbreg_line[x >> 3] = result;
        }
#ifdef OUTPUT_PBM
        fwrite(gbreg_line, 1, rowstride, stdout);
#endif
        line2 = line1;
        line1 = gbreg_line;
        gbreg_line += rowstride;
    }

    return 0;
}

static int
jbig2_decode_generic_template3(Jbig2Ctx *ctx,
                               Jbig2Segment *segment,
                               const Jbig2GenericRegionParams *params, Jbig2ArithState *as, Jbig2Image *image, Jbig2ArithCx *GB_stats)
{
    const int64_t GBW = image->width;
    const int64_t GBH = image->height;
    const uint32_t rowstride = image->stride;
    byte *line1 = NULL;
    byte *gbreg_line = (byte *) image->data;
    int64_t x, y;

    (void) params;

#ifdef OUTPUT_PBM
    printf("P4\n%u %u\n",
        (uint32_t) GBW,
        (uint32_t) GBH);
#endif

    if (GBW <= 0)
        return 0;

    for (y = 0; y < GBH; y++) {
        uint32_t CONTEXT;
        uint32_t line_m1;
        int64_t padded_width = (GBW + 7) & -8;

        line_m1 = line1 ? line1[0] : 0;
        CONTEXT = (line_m1 >> 1) & 0x3f0;

        for (x = 0; x < padded_width; x += 8) {
            byte result = 0;
            int64_t x_minor;
            int64_t minor_width = GBW - x > 8 ? 8 : GBW - x;

            if (line1)
                line_m1 = (line_m1 << 8) | (x + 8 < GBW ? line1[(x >> 3) + 1] : 0);

            for (x_minor = 0; x_minor < minor_width; x_minor++) {
                int bit;

                bit = jbig2_arith_decode(ctx, as, &GB_stats[CONTEXT]);
                if (bit < 0)
                    return jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "failed to decode arithmetic code when handling generic template3 optimized");
                result |= bit << (7 - x_minor);
                CONTEXT = ((CONTEXT & 0x1f7) << 1) | bit | ((line_m1 >> (8 - x_minor)) & 0x10);
            }
            gbreg_line[x >> 3] = result;
        }
#ifdef OUTPUT_PBM
        fwrite(gbreg_line, 1, rowstride, stdout);
#endif
        line1 = gbreg_line;
        gbreg_line += rowstride;
    }

    return 0;
}

static int
jbig2_decode_generic_template3_unopt(Jbig2Ctx *ctx,
                                     Jbig2Segment *segment,
                                     const Jbig2GenericRegionParams *params, Jbig2ArithState *as, Jbig2Image *image, Jbig2ArithCx *GB_stats)
{
    const int64_t GBW = image->width;
    const int64_t GBH = image->height;
    uint32_t CONTEXT;
    int64_t x, y;
    int bit;

    if (pixel_outside_field(params->gbat[0], params->gbat[1]))
        return jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number,
                           "adaptive template pixel is out of field");

    for (y = 0; y < GBH; y++) {
        uint32_t out_byte = 0;
        int out_bits_to_go_in_byte = 8;
        uint8_t *d = &image->data[image->stride * y];
        uint8_t *pline  = &image->data[image->stride * (y-1)];
        uint32_t pd = 0;
        if (y > 0) {
            pd = (*pline++ << 8);
            if (GBW > 8)
                pd |= *pline++;
        }
        for (x = 0; x < GBW; x++) {
            if (params->USESKIP && jbig2_image_get_pixel(params->SKIP, x, y)) {
                bit = 0;
            } else {
                CONTEXT  = out_byte & 0x00F;
                CONTEXT |= jbig2_image_get_pixel(image, x + params->gbat[0], y + params->gbat[1]) << 4;
                CONTEXT |= (pd>>9) & 0x3E0;
                bit = jbig2_arith_decode(ctx, as, &GB_stats[CONTEXT]);
                if (bit < 0)
                    return jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "failed to decode arithmetic code when handling generic template3 unoptimized");
            }
            pd = pd<<1;
            out_byte = (out_byte<<1) | bit;
            out_bits_to_go_in_byte--;
            *d = (uint8_t)out_byte<<out_bits_to_go_in_byte;
            if (out_bits_to_go_in_byte == 0) {
                out_bits_to_go_in_byte = 8;
                d++;
                if (x+9 < GBW && y > 0)
                    pd |= *pline++;
            }
        }
        if (out_bits_to_go_in_byte != 8)
            *d = (uint8_t)out_byte<<out_bits_to_go_in_byte;
    }
    return 0;
}

static void
copy_prev_row(Jbig2Image *image, int64_t row)
{
    if (!row) {

        memset(image->data, 0, image->stride);
    } else {

        uint8_t *src = image->data + (row - 1) * image->stride;

        memcpy(src + image->stride, src, image->stride);
    }
}

static int
jbig2_decode_generic_template0_TPGDON(Jbig2Ctx *ctx,
                                      Jbig2Segment *segment,
                                      const Jbig2GenericRegionParams *params, Jbig2ArithState *as, Jbig2Image *image, Jbig2ArithCx *GB_stats)
{
    const int64_t GBW = image->width;
    const int64_t GBH = image->height;
    uint32_t CONTEXT;
    int64_t x, y;
    int LTP = 0;
    int gmin, gmax;
    int64_t left, right, top;

    if (pixel_outside_field(params->gbat[0], params->gbat[1]) ||
        pixel_outside_field(params->gbat[2], params->gbat[3]) ||
        pixel_outside_field(params->gbat[4], params->gbat[5]) ||
        pixel_outside_field(params->gbat[6], params->gbat[7]))
        return jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number,
                           "adaptive template pixel is out of field");

    if (params->gbat[0] ==  3 && params->gbat[1] == -1 &&
        params->gbat[2] == -3 && params->gbat[3] == -1 &&
        params->gbat[4] ==  2 && params->gbat[5] == -2 &&
        params->gbat[6] == -2 && params->gbat[7] == -2)
    {
        for (y = 0; y < GBH; y++) {
            int bit = jbig2_arith_decode(ctx, as, &GB_stats[0x9B25]);
            if (bit < 0)
                return jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "failed to decode arithmetic code when handling generic template0 TPGDON1");
            LTP ^= bit;
            if (!LTP) {
                uint32_t out_byte = 0;
                int out_bits_to_go_in_byte = 8;
                uint8_t *d = &image->data[image->stride * y];
                uint8_t *pline  = y > 0 ? &image->data[image->stride * (y-1)] : NULL;
                uint8_t *ppline = y > 1 ? &image->data[image->stride * (y-2)] : NULL;
                uint32_t pd = 0;
                uint32_t ppd = 0;
                if (y > 0 && pline) {
                    pd = (*pline++ << 8);
                    if (GBW > 8)
                        pd |= *pline++;
                    if (y > 1 && ppline) {
                        ppd = (*ppline++ << 8);
                        if (GBW > 8)
                            ppd |= *ppline++;
                    }
                }
                for (x = 0; x < GBW; x++) {
                    if (params->USESKIP && jbig2_image_get_pixel(params->SKIP, x, y)) {
                        bit = 0;
                    } else {
                        CONTEXT  = out_byte & 0x00F;
                        CONTEXT |= (pd>>8) & 0x7F0;
                        CONTEXT |= (ppd>>2) & 0xF800;
                        bit = jbig2_arith_decode(ctx, as, &GB_stats[CONTEXT]);
                        if (bit < 0)
                            return jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "failed to decode arithmetic code when handling generic template0 TPGDON2");
                    }
                    pd = pd<<1;
                    ppd = ppd<<1;
                    out_byte = (out_byte<<1) | bit;
                    out_bits_to_go_in_byte--;
                    if (out_bits_to_go_in_byte == 0) {
                        out_bits_to_go_in_byte = 8;
                        *d++ = (uint8_t)out_byte;
                        if (x+9 < GBW && y > 0 && pline) {
                            pd |= *pline++;
                            if (y > 1 && ppline)
                                ppd |= *ppline++;
                        }
                    }
                }
                if (out_bits_to_go_in_byte != 8)
                    *d = (uint8_t)out_byte<<out_bits_to_go_in_byte;
            } else {
                copy_prev_row(image, y);
            }
        }
        return 0;
    }

    left = 4;
    right = 2;
    gmin = gmax = params->gbat[0];
    if (params->gbat[2] < gmin)
        gmin = params->gbat[2];
    if (gmax < params->gbat[2])
        gmax = params->gbat[2];
    if (params->gbat[4] < gmin)
        gmin = params->gbat[4];
    if (gmax < params->gbat[4])
        gmax = params->gbat[4];
    if (params->gbat[6] < gmin)
        gmin = params->gbat[6];
    if (gmax < params->gbat[6])
        gmax = params->gbat[6];
    if (left < -((int64_t) gmin))
        left = -((int64_t) gmin);
    if (right < gmax)
        right = gmax;
    if (right > GBW)
        right = GBW;
    right = GBW - right;

    top = 2;
    gmin = params->gbat[1];
    if (params->gbat[3] < gmin)
        gmin = params->gbat[3];
    if (params->gbat[5] < gmin)
        gmin = params->gbat[5];
    if (params->gbat[7] < gmin)
        gmin = params->gbat[7];
    if (top < -((int64_t) gmin))
        top = -((int64_t) gmin);

    for (y = 0; y < GBH; y++) {
        int bit = jbig2_arith_decode(ctx, as, &GB_stats[0x9B25]);
        if (bit < 0)
            return jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "failed to decode arithmetic code when handling generic template0 TPGDON1");
        LTP ^= bit;
        if (!LTP) {
            uint32_t out_byte = 0;
            int out_bits_to_go_in_byte = 8;
            uint8_t *d = &image->data[image->stride * y];
            uint8_t *pline  = y > 0 ? &image->data[image->stride * (y-1)] : NULL;
            uint8_t *ppline = y > 1 ? &image->data[image->stride * (y-2)] : NULL;
            uint32_t pd = 0;
            uint32_t ppd = 0;
            if (y > 0 && pline) {
                pd = (*pline++ << 8);
                if (GBW > 8)
                    pd |= *pline++;
                if (y > 1 && ppline) {
                    ppd = (*ppline++ << 8);
                    if (GBW > 8)
                        ppd |= *ppline++;
                }
            }
            for (x = 0; x < GBW; x++) {
                if (params->USESKIP && jbig2_image_get_pixel(params->SKIP, x, y)) {
                    bit = 0;
                } else {
                    CONTEXT = out_byte & 0x000F;
                    CONTEXT |= (pd>>8) & 0x03E0;
                    CONTEXT |= (ppd>>2) & 0x7000;
                    if (y >= top && x >= left && x < right)
                    {
                        CONTEXT |= jbig2_image_get_pixel_fast(image, x + params->gbat[0], y + params->gbat[1]) << 4;
                        CONTEXT |= jbig2_image_get_pixel_fast(image, x + params->gbat[2], y + params->gbat[3]) << 10;
                        CONTEXT |= jbig2_image_get_pixel_fast(image, x + params->gbat[4], y + params->gbat[5]) << 11;
                        CONTEXT |= jbig2_image_get_pixel_fast(image, x + params->gbat[6], y + params->gbat[7]) << 15;
                    }
                    else
                    {
                        CONTEXT |= jbig2_image_get_pixel(image, x + params->gbat[0], y + params->gbat[1]) << 4;
                        CONTEXT |= jbig2_image_get_pixel(image, x + params->gbat[2], y + params->gbat[3]) << 10;
                        CONTEXT |= jbig2_image_get_pixel(image, x + params->gbat[4], y + params->gbat[5]) << 11;
                        CONTEXT |= jbig2_image_get_pixel(image, x + params->gbat[6], y + params->gbat[7]) << 15;
                    }
                    bit = jbig2_arith_decode(ctx, as, &GB_stats[CONTEXT]);
                    if (bit < 0)
                        return jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "failed to decode arithmetic code when handling generic template0 TPGDON2");
                }
                pd = pd<<1;
                ppd = ppd<<1;
                out_byte = (out_byte<<1) | bit;
                out_bits_to_go_in_byte--;
                *d = (uint8_t)out_byte<<out_bits_to_go_in_byte;
                if (out_bits_to_go_in_byte == 0) {
                    out_bits_to_go_in_byte = 8;
                    d++;
                    if (x+9 < GBW && y > 0 && pline) {
                        pd |= *pline++;
                        if (y > 1 && ppline)
                            ppd |= *ppline++;
                    }
                }
            }
            if (out_bits_to_go_in_byte != 8)
                *d = (uint8_t)out_byte<<out_bits_to_go_in_byte;
        } else {
            copy_prev_row(image, y);
        }
    }

    return 0;
}

static int
jbig2_decode_generic_template1_TPGDON(Jbig2Ctx *ctx,
                                      Jbig2Segment *segment,
                                      const Jbig2GenericRegionParams *params, Jbig2ArithState *as, Jbig2Image *image, Jbig2ArithCx *GB_stats)
{
    const int64_t GBW = image->width;
    const int64_t GBH = image->height;
    uint32_t CONTEXT;
    int64_t x, y;
    int LTP = 0;

    if (pixel_outside_field(params->gbat[0], params->gbat[1]))
        return jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number,
                           "adaptive template pixel is out of field");

    for (y = 0; y < GBH; y++) {
        int bit = jbig2_arith_decode(ctx, as, &GB_stats[0x0795]);
        if (bit < 0)
            return jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "failed to decode arithmetic code when handling generic template1 TPGDON1");
        LTP ^= bit;
        if (!LTP) {
            uint32_t out_byte = 0;
            int out_bits_to_go_in_byte = 8;
            uint8_t *d = &image->data[image->stride * y];
            uint8_t *pline  = y > 0 ? &image->data[image->stride * (y-1)] : NULL;
            uint8_t *ppline = y > 1 ? &image->data[image->stride * (y-2)] : NULL;
            uint32_t pd = 0;
            uint32_t ppd = 0;
            if (y > 0 && pline) {
                pd = (*pline++ << 8);
                if (GBW > 8)
                    pd |= *pline++;
                if (y > 1 && ppline) {
                    ppd = (*ppline++ << 8);
                    if (GBW > 8)
                        ppd |= *ppline++;
                }
            }
            for (x = 0; x < GBW; x++) {
                if (params->USESKIP && jbig2_image_get_pixel(params->SKIP, x, y)) {
                    bit = 0;
                } else {
                    CONTEXT  = out_byte & 0x0007;
                    CONTEXT |= jbig2_image_get_pixel(image, x + params->gbat[0], y + params->gbat[1]) << 3;
                    CONTEXT |= (pd>>9) & 0x01F0;
                    CONTEXT |= (ppd>>4) & 0x1E00;
                    bit = jbig2_arith_decode(ctx, as, &GB_stats[CONTEXT]);
                    if (bit < 0)
                        return jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "failed to decode arithmetic code when handling generic template1 TPGDON2");
                }
                pd = pd<<1;
                ppd = ppd<<1;
                out_byte = (out_byte<<1) | bit;
                out_bits_to_go_in_byte--;
                *d = (uint8_t)out_byte<<out_bits_to_go_in_byte;
                if (out_bits_to_go_in_byte == 0) {
                    out_bits_to_go_in_byte = 8;
                    d++;
                    if (x+9 < GBW && y > 0 && pline) {
                        pd |= *pline++;
                        if (y > 1 && ppline)
                            ppd |= *ppline++;
                    }
                }
            }
            if (out_bits_to_go_in_byte != 8)
                *d = (uint8_t)out_byte<<out_bits_to_go_in_byte;
        } else {
            copy_prev_row(image, y);
        }
    }

    return 0;
}

static int
jbig2_decode_generic_template2_TPGDON(Jbig2Ctx *ctx,
                                      Jbig2Segment *segment,
                                      const Jbig2GenericRegionParams *params, Jbig2ArithState *as, Jbig2Image *image, Jbig2ArithCx *GB_stats)
{
    const int64_t GBW = image->width;
    const int64_t GBH = image->height;
    uint32_t CONTEXT;
    int64_t x, y;
    int LTP = 0;

    if (pixel_outside_field(params->gbat[0], params->gbat[1]))
        return jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number,
                           "adaptive template pixel is out of field");

    for (y = 0; y < GBH; y++) {
        int bit = jbig2_arith_decode(ctx, as, &GB_stats[0xE5]);
        if (bit < 0)
            return jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "failed to decode arithmetic code when handling generic template2 TPGDON1");
        LTP ^= bit;
        if (!LTP) {
            uint32_t out_byte = 0;
            int out_bits_to_go_in_byte = 8;
            uint8_t *d = &image->data[image->stride * y];
            uint8_t *pline  =  y > 0 ? &image->data[image->stride * (y-1)] : NULL;
            uint8_t *ppline = y > 1 ? &image->data[image->stride * (y-2)] : NULL;
            uint32_t pd = 0;
            uint32_t ppd = 0;
            if (y > 0 && pline) {
                pd = (*pline++ << 8);
                if (GBW > 8)
                    pd |= *pline++;
                if (y > 1 && ppline) {
                    ppd = (*ppline++ << 8);
                    if (GBW > 8)
                        ppd |= *ppline++;
                }
            }
            for (x = 0; x < GBW; x++) {
                if (params->USESKIP && jbig2_image_get_pixel(params->SKIP, x, y)) {
                    bit = 0;
                } else {
                    CONTEXT  = out_byte & 0x003;
                    CONTEXT |= jbig2_image_get_pixel(image, x + params->gbat[0], y + params->gbat[1]) << 2;
                    CONTEXT |= (pd>>11) & 0x078;
                    CONTEXT |= (ppd>>7) & 0x380;
                    bit = jbig2_arith_decode(ctx, as, &GB_stats[CONTEXT]);
                    if (bit < 0)
                        return jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "failed to decode arithmetic code when handling generic template2 TPGDON2");
                }
                pd = pd<<1;
                ppd = ppd<<1;
                out_byte = (out_byte<<1) | bit;
                out_bits_to_go_in_byte--;
                *d = (uint8_t)out_byte<<out_bits_to_go_in_byte;
                if (out_bits_to_go_in_byte == 0) {
                    out_bits_to_go_in_byte = 8;
                    d++;
                    if (x+9 < GBW && y > 0 && pline) {
                        pd |= *pline++;
                        if (y > 1 && ppline)
                            ppd |= *ppline++;
                    }
                }
            }
            if (out_bits_to_go_in_byte != 8)
                *d = (uint8_t)out_byte<<out_bits_to_go_in_byte;
        } else {
            copy_prev_row(image, y);
        }
    }

    return 0;
}

static int
jbig2_decode_generic_template3_TPGDON(Jbig2Ctx *ctx,
                                      Jbig2Segment *segment,
                                      const Jbig2GenericRegionParams *params, Jbig2ArithState *as, Jbig2Image *image, Jbig2ArithCx *GB_stats)
{
    const int64_t GBW = image->width;
    const int64_t GBH = image->height;
    uint32_t CONTEXT;
    int64_t x, y;
    int LTP = 0;

    if (pixel_outside_field(params->gbat[0], params->gbat[1]))
        return jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number,
                           "adaptive template pixel is out of field");

    for (y = 0; y < GBH; y++) {
        int bit = jbig2_arith_decode(ctx, as, &GB_stats[0x0195]);
        if (bit < 0)
            return jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "failed to decode arithmetic code when handling generic template3 TPGDON1");
        LTP ^= bit;
        if (!LTP) {
            uint32_t out_byte = 0;
            int out_bits_to_go_in_byte = 8;
            uint8_t *d = &image->data[image->stride * y];
            uint8_t *pline  = y > 0 ? &image->data[image->stride * (y-1)] : NULL;
            uint32_t pd = 0;
            if (y > 0 && pline) {
                pd = (*pline++ << 8);
                if (GBW > 8)
                    pd |= *pline++;
            }
            for (x = 0; x < GBW; x++) {
                if (params->USESKIP && jbig2_image_get_pixel(params->SKIP, x, y)) {
                    bit = 0;
                } else {
                    CONTEXT  = out_byte & 0x0F;
                    CONTEXT |= jbig2_image_get_pixel(image, x + params->gbat[0], y + params->gbat[1]) << 4;
                    CONTEXT |= (pd>>9) & 0x3E0;
                    bit = jbig2_arith_decode(ctx, as, &GB_stats[CONTEXT]);
                    if (bit < 0)
                        return jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "failed to decode arithmetic code when handling generic template3 TPGDON2");
                }
                pd = pd<<1;
                out_byte = (out_byte<<1) | bit;
                out_bits_to_go_in_byte--;
                *d = (uint8_t)out_byte<<out_bits_to_go_in_byte;
                if (out_bits_to_go_in_byte == 0) {
                    out_bits_to_go_in_byte = 8;
                    d++;
                    if (x+9 < GBW && y > 0 && pline)
                        pd |= *pline++;
                }
            }
            if (out_bits_to_go_in_byte != 8)
                *d = (uint8_t)out_byte<<out_bits_to_go_in_byte;
        } else {
            copy_prev_row(image, y);
        }
    }

    return 0;
}

static int
jbig2_decode_generic_region_TPGDON(Jbig2Ctx *ctx,
                                   Jbig2Segment *segment,
                                   const Jbig2GenericRegionParams *params, Jbig2ArithState *as, Jbig2Image *image, Jbig2ArithCx *GB_stats)
{
    switch (params->GBTEMPLATE) {
    case 0:
        return jbig2_decode_generic_template0_TPGDON(ctx, segment, params, as, image, GB_stats);
    case 1:
        return jbig2_decode_generic_template1_TPGDON(ctx, segment, params, as, image, GB_stats);
    case 2:
        return jbig2_decode_generic_template2_TPGDON(ctx, segment, params, as, image, GB_stats);
    case 3:
        return jbig2_decode_generic_template3_TPGDON(ctx, segment, params, as, image, GB_stats);
    }

    return jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number, "unsupported GBTEMPLATE (%d)", params->GBTEMPLATE);
}

int
jbig2_decode_generic_region(Jbig2Ctx *ctx,
                            Jbig2Segment *segment, const Jbig2GenericRegionParams *params, Jbig2ArithState *as, Jbig2Image *image, Jbig2ArithCx *GB_stats)
{
    const int8_t *gbat = params->gbat;

    if (!params->MMR && params->TPGDON)
        return jbig2_decode_generic_region_TPGDON(ctx, segment, params, as, image, GB_stats);

    if (!params->MMR && params->GBTEMPLATE == 0) {
        if (!params->USESKIP && gbat[0] == +3 && gbat[1] == -1 && gbat[2] == -3 && gbat[3] == -1 && gbat[4] == +2 && gbat[5] == -2 && gbat[6] == -2 && gbat[7] == -2)
            return jbig2_decode_generic_template0(ctx, segment, params, as, image, GB_stats);
        else
            return jbig2_decode_generic_template0_unopt(ctx, segment, params, as, image, GB_stats);
    } else if (!params->MMR && params->GBTEMPLATE == 1) {
        if (!params->USESKIP && gbat[0] == +3 && gbat[1] == -1)
            return jbig2_decode_generic_template1(ctx, segment, params, as, image, GB_stats);
        else
            return jbig2_decode_generic_template1_unopt(ctx, segment, params, as, image, GB_stats);
    }
    else if (!params->MMR && params->GBTEMPLATE == 2) {
        if (!params->USESKIP && gbat[0] == 2 && gbat[1] == -1)
            return jbig2_decode_generic_template2(ctx, segment, params, as, image, GB_stats);
        else
            return jbig2_decode_generic_template2_unopt(ctx, segment, params, as, image, GB_stats);
    } else if (!params->MMR && params->GBTEMPLATE == 3) {
        if (!params->USESKIP && gbat[0] == 2 && gbat[1] == -1)
            return jbig2_decode_generic_template3(ctx, segment, params, as, image, GB_stats);
        else
            return jbig2_decode_generic_template3_unopt(ctx, segment, params, as, image, GB_stats);
    }

    {
        int i;

        for (i = 0; i < 8; i++)
            jbig2_error(ctx, JBIG2_SEVERITY_DEBUG, segment->number, "gbat[%d] = %d", i, params->gbat[i]);
    }

    return jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number, "unsupported generic region (MMR=%d, GBTEMPLATE=%d)", params->MMR, params->GBTEMPLATE);
}

int
jbig2_immediate_generic_region(Jbig2Ctx *ctx, Jbig2Segment *segment, const byte *segment_data)
{
    Jbig2RegionSegmentInfo rsi;
    byte seg_flags;
    int8_t gbat[8];
    int offset;
    uint32_t gbat_bytes = 0;
    Jbig2GenericRegionParams params;
    int code = 0;
    Jbig2Image *image = NULL;
    Jbig2WordStream *ws = NULL;
    Jbig2ArithState *as = NULL;
    Jbig2ArithCx *GB_stats = NULL;
    uint32_t height;
    Jbig2Page *page = &ctx->pages[ctx->current_page];

    if (segment->data_length < 18)
        return jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number, "segment too short");

    jbig2_get_region_segment_info(&rsi, segment_data);
    jbig2_error(ctx, JBIG2_SEVERITY_INFO, segment->number, "generic region: %u x %u @ (%u, %u), flags = %02x", rsi.width, rsi.height, rsi.x, rsi.y, rsi.flags);

    height = rsi.height;
    if (segment->rows != UINT32_MAX) {
        if (segment->rows > rsi.height)
            return jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number, "segment contains more rows than stated in header");
        height = segment->rows;
    }

    seg_flags = segment_data[17];
    jbig2_error(ctx, JBIG2_SEVERITY_INFO, segment->number, "segment flags = %02x", seg_flags);
    if ((seg_flags & 1) && (seg_flags & 6))
        jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "MMR is 1, but GBTEMPLATE is not 0");

    if (!(seg_flags & 1)) {
        gbat_bytes = (seg_flags & 6) ? 2 : 8;
        if (18 + gbat_bytes > segment->data_length)
            return jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number, "segment too short");
        memcpy(gbat, segment_data + 18, gbat_bytes);
        jbig2_error(ctx, JBIG2_SEVERITY_INFO, segment->number, "gbat: %d, %d", gbat[0], gbat[1]);
    }

    offset = 18 + gbat_bytes;

    if ((seg_flags >> 5) & 1)
        return jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number, "segment uses 12 adaptive template pixels (NYI)");

    params.MMR = seg_flags & 1;
    params.GBTEMPLATE = (seg_flags & 6) >> 1;
    params.TPGDON = (seg_flags & 8) >> 3;
    params.USESKIP = 0;
    memcpy(params.gbat, gbat, gbat_bytes);

    if (page->height == 0xffffffff && page->striped && page->stripe_size > 0) {
        if (rsi.y >= page->end_row + page->stripe_size) {
            jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "ignoring %u x %u region at (%u, %u) outside of stripe at row %u covering %u rows, on page of height %u", rsi.width, rsi.height, rsi.x, rsi.y, page->end_row, page->stripe_size, page->image->height);
            return 0;
        }
        if (height > page->end_row + page->stripe_size) {
            height = page->end_row + page->stripe_size;
        }
    } else {
        if (rsi.y >= page->height) {
            jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "ignoring %u x %u region at (%u, %u) outside of page of height %u", rsi.width, rsi.height, rsi.x, rsi.y, page->height);
            return 0;
        }
        if (height > page->height - rsi .y) {
            height = page->height - rsi.y;
        }
    }
    if (height == 0) {
        jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "nothing remains of region, ignoring");
        return 0;
    }

    image = jbig2_image_new(ctx, rsi.width, height);
    if (image == NULL)
        return jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "failed to allocate generic image");
    jbig2_error(ctx, JBIG2_SEVERITY_DEBUG, segment->number, "allocated %d x %d image buffer for region decode results", rsi.width, height);

    if (params.MMR) {
        code = jbig2_decode_generic_mmr(ctx, segment, &params, segment_data + offset, segment->data_length - offset, image);
        if (code < 0) {
            code = jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "failed to decode MMR-coded generic region");
            goto cleanup;
        }
    } else {
        int stats_size = jbig2_generic_stats_size(ctx, params.GBTEMPLATE);

        GB_stats = jbig2_new(ctx, Jbig2ArithCx, stats_size);
        if (GB_stats == NULL) {
            code = jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number, "failed to allocate arithmetic decoder states when handling immediate generic region");
            goto cleanup;
        }
        memset(GB_stats, 0, stats_size);

        ws = jbig2_word_stream_buf_new(ctx, segment_data + offset, segment->data_length - offset);
        if (ws == NULL) {
            code = jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "failed to allocated word stream when handling immediate generic region");
            goto cleanup;
        }
        as = jbig2_arith_new(ctx, ws);
        if (as == NULL) {
            code = jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "failed to allocate arithmetic coding state when handling immediate generic region");
            goto cleanup;
        }
        code = jbig2_decode_generic_region(ctx, segment, &params, as, image, GB_stats);
        if (code < 0) {
            code = jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "failed to decode immediate generic region");
            goto cleanup;
        }
    }

    code = jbig2_page_add_result(ctx, &ctx->pages[ctx->current_page], image, rsi.x, rsi.y, rsi.op);
    if (code < 0)
        code = jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "unable to add result to page");

cleanup:
    jbig2_free(ctx->allocator, as);
    jbig2_word_stream_buf_free(ctx, ws);
    jbig2_free(ctx->allocator, GB_stats);
    jbig2_image_release(ctx, image);

    return code;
}

#ifdef HAVE_CONFIG_H
#endif

#include <stdlib.h>
#include <string.h>

#ifdef JBIG2_DEBUG
#include <stdio.h>
#endif

#ifndef _JBIG2_HUFFMAN_H
#define _JBIG2_HUFFMAN_H

typedef struct _Jbig2HuffmanEntry Jbig2HuffmanEntry;
typedef struct _Jbig2HuffmanState Jbig2HuffmanState;
typedef struct _Jbig2HuffmanTable Jbig2HuffmanTable;
typedef struct _Jbig2HuffmanParams Jbig2HuffmanParams;

struct _Jbig2HuffmanEntry {
    union {
        int32_t RANGELOW;
        Jbig2HuffmanTable *ext_table;
    } u;
    byte PREFLEN;
    byte RANGELEN;
    byte flags;
};

struct _Jbig2HuffmanTable {
    int log_table_size;
    Jbig2HuffmanEntry *entries;
};

typedef struct _Jbig2HuffmanLine Jbig2HuffmanLine;

struct _Jbig2HuffmanLine {
    int PREFLEN;
    int RANGELEN;
    int RANGELOW;
};

struct _Jbig2HuffmanParams {
    bool HTOOB;
    int n_lines;
    const Jbig2HuffmanLine *lines;
};

Jbig2HuffmanState *jbig2_huffman_new(Jbig2Ctx *ctx, Jbig2WordStream *ws);

void jbig2_huffman_free(Jbig2Ctx *ctx, Jbig2HuffmanState *hs);

int jbig2_huffman_skip(Jbig2HuffmanState *hs);

int jbig2_huffman_advance(Jbig2HuffmanState *hs, size_t advance);

size_t jbig2_huffman_offset(Jbig2HuffmanState *hs);

int32_t jbig2_huffman_get(Jbig2HuffmanState *hs, const Jbig2HuffmanTable *table, bool *oob);

int32_t jbig2_huffman_get_bits(Jbig2HuffmanState *hs, const int bits, int *err);

#ifdef JBIG2_DEBUG
void jbig2_dump_huffman_state(Jbig2HuffmanState *hs);
void jbig2_dump_huffman_binary(Jbig2HuffmanState *hs);
#endif

Jbig2HuffmanTable *jbig2_build_huffman_table(Jbig2Ctx *ctx, const Jbig2HuffmanParams *params);

void jbig2_release_huffman_table(Jbig2Ctx *ctx, Jbig2HuffmanTable *table);

extern const Jbig2HuffmanParams jbig2_huffman_params_A;
extern const Jbig2HuffmanParams jbig2_huffman_params_B;
extern const Jbig2HuffmanParams jbig2_huffman_params_C;
extern const Jbig2HuffmanParams jbig2_huffman_params_D;
extern const Jbig2HuffmanParams jbig2_huffman_params_E;
extern const Jbig2HuffmanParams jbig2_huffman_params_F;
extern const Jbig2HuffmanParams jbig2_huffman_params_G;
extern const Jbig2HuffmanParams jbig2_huffman_params_H;
extern const Jbig2HuffmanParams jbig2_huffman_params_I;
extern const Jbig2HuffmanParams jbig2_huffman_params_J;
extern const Jbig2HuffmanParams jbig2_huffman_params_K;
extern const Jbig2HuffmanParams jbig2_huffman_params_L;
extern const Jbig2HuffmanParams jbig2_huffman_params_M;
extern const Jbig2HuffmanParams jbig2_huffman_params_N;
extern const Jbig2HuffmanParams jbig2_huffman_params_O;

int jbig2_table(Jbig2Ctx *ctx, Jbig2Segment *segment, const byte *segment_data);

void jbig2_table_free(Jbig2Ctx *ctx, Jbig2HuffmanParams *params);

const Jbig2HuffmanParams *jbig2_find_table(Jbig2Ctx *ctx, Jbig2Segment *segment, int index);

#endif

#ifndef _JBIG2_HUFFTAB_H
#define _JBIG2_HUFFTAB_H

extern const Jbig2HuffmanParams jbig2_huffman_params_A;
extern const Jbig2HuffmanParams jbig2_huffman_params_B;
extern const Jbig2HuffmanParams jbig2_huffman_params_C;
extern const Jbig2HuffmanParams jbig2_huffman_params_D;
extern const Jbig2HuffmanParams jbig2_huffman_params_E;
extern const Jbig2HuffmanParams jbig2_huffman_params_F;
extern const Jbig2HuffmanParams jbig2_huffman_params_G;
extern const Jbig2HuffmanParams jbig2_huffman_params_H;
extern const Jbig2HuffmanParams jbig2_huffman_params_I;
extern const Jbig2HuffmanParams jbig2_huffman_params_J;
extern const Jbig2HuffmanParams jbig2_huffman_params_K;
extern const Jbig2HuffmanParams jbig2_huffman_params_L;
extern const Jbig2HuffmanParams jbig2_huffman_params_M;
extern const Jbig2HuffmanParams jbig2_huffman_params_N;
extern const Jbig2HuffmanParams jbig2_huffman_params_O;

#endif

#define JBIG2_HUFFMAN_FLAGS_ISOOB 1
#define JBIG2_HUFFMAN_FLAGS_ISLOW 2
#define JBIG2_HUFFMAN_FLAGS_ISEXT 4

struct _Jbig2HuffmanState {

    uint32_t this_word;
    uint32_t next_word;
    uint32_t offset_bits;
    size_t offset;
    size_t offset_limit;

    Jbig2WordStream *ws;
    Jbig2Ctx *ctx;
};

#define huff_get_next_word(hs, offset, word) \
    (hs)->ws->get_next_word((hs)->ctx, (hs)->ws, (offset), (word))

Jbig2HuffmanState *
jbig2_huffman_new(Jbig2Ctx *ctx, Jbig2WordStream *ws)
{
    Jbig2HuffmanState *result = NULL;
    int code;

    result = jbig2_new(ctx, Jbig2HuffmanState, 1);

    if (result != NULL) {
        result->offset = 0;
        result->offset_bits = 0;
        result->offset_limit = 0;
        result->ws = ws;
        result->ctx = ctx;
        code = huff_get_next_word(result, 0, &result->this_word);
        if (code < 0) {
            jbig2_error(ctx, JBIG2_SEVERITY_WARNING, JBIG2_UNKNOWN_SEGMENT_NUMBER, "failed to read first huffman word");
            jbig2_huffman_free(ctx, result);
            return NULL;
        }
        code = huff_get_next_word(result, 4, &result->next_word);
        if (code < 0) {
            jbig2_error(ctx, JBIG2_SEVERITY_WARNING, JBIG2_UNKNOWN_SEGMENT_NUMBER, "failed to read second huffman word");
            jbig2_huffman_free(ctx, result);
            return NULL;
        }
    } else {
        jbig2_error(ctx, JBIG2_SEVERITY_FATAL, JBIG2_UNKNOWN_SEGMENT_NUMBER, "failed to allocate new huffman coding state");
        return NULL;
    }

    return result;
}

void
jbig2_huffman_free(Jbig2Ctx *ctx, Jbig2HuffmanState *hs)
{
    jbig2_free(ctx->allocator, hs);
}

#ifdef JBIG2_DEBUG

void
jbig2_dump_huffman_state(Jbig2HuffmanState *hs)
{
    fprintf(stderr, "huffman state %08x %08x offset "FMTZ"."FMTZ"\n",
            hs->this_word, hs->next_word,
            (FMTZ_CAST) hs->offset, (FMTZ_CAST) hs->offset_bits);
}

void
jbig2_dump_huffman_binary(Jbig2HuffmanState *hs)
{
    const uint32_t word = hs->this_word;
    int i;

    fprintf(stderr, "huffman binary ");
    for (i = 31; i >= 0; i--)
        fprintf(stderr, ((word >> i) & 1) ? "1" : "0");
    fprintf(stderr, "\n");
}

void
jbig2_dump_huffman_table(const Jbig2HuffmanTable *table)
{
    int i;
    int table_size = (1 << table->log_table_size);

    fprintf(stderr, "huffman table %p (log_table_size=%d, %d entries, entries=%p):\n", (void *) table, table->log_table_size, table_size, (void *) table->entries);
    for (i = 0; i < table_size; i++) {
        fprintf(stderr, "%6d: PREFLEN=%d, RANGELEN=%d, ", i, table->entries[i].PREFLEN, table->entries[i].RANGELEN);
        if (table->entries[i].flags & JBIG2_HUFFMAN_FLAGS_ISEXT) {
            fprintf(stderr, "ext=%p", (void *) table->entries[i].u.ext_table);
        } else {
            fprintf(stderr, "RANGELOW=%d", table->entries[i].u.RANGELOW);
        }
        if (table->entries[i].flags) {
            int need_comma = 0;

            fprintf(stderr, ", flags=0x%x(", table->entries[i].flags);
            if (table->entries[i].flags & JBIG2_HUFFMAN_FLAGS_ISOOB) {
                fprintf(stderr, "OOB");
                need_comma = 1;
            }
            if (table->entries[i].flags & JBIG2_HUFFMAN_FLAGS_ISLOW) {
                if (need_comma)
                    fprintf(stderr, ",");
                fprintf(stderr, "LOW");
                need_comma = 1;
            }
            if (table->entries[i].flags & JBIG2_HUFFMAN_FLAGS_ISEXT) {
                if (need_comma)
                    fprintf(stderr, ",");
                fprintf(stderr, "EXT");
            }
            fprintf(stderr, ")");
        }
        fprintf(stderr, "\n");
    }
    fprintf(stderr, "\n");
}

#endif

int
jbig2_huffman_skip(Jbig2HuffmanState *hs)
{
    uint32_t bits = hs->offset_bits & 7;
    int code;

    if (bits) {
        bits = 8 - bits;
        hs->offset_bits += bits;
        hs->this_word = (hs->this_word << bits) | (hs->next_word >> (32 - hs->offset_bits));
    }

    if (hs->offset_bits >= 32) {
        hs->this_word = hs->next_word;
        hs->offset += 4;
        code = huff_get_next_word(hs, hs->offset + 4, &hs->next_word);
        if (code < 0) {
            return jbig2_error(hs->ctx, JBIG2_SEVERITY_WARNING, JBIG2_UNKNOWN_SEGMENT_NUMBER, "failed to read next huffman word when skipping");
        }
        hs->offset_bits -= 32;
        if (hs->offset_bits) {
            hs->this_word = (hs->this_word << hs->offset_bits) | (hs->next_word >> (32 - hs->offset_bits));
        }
    }
    return 0;
}

int
jbig2_huffman_advance(Jbig2HuffmanState *hs, size_t advance)
{
    int code;
    hs->offset += advance & ~3;
    hs->offset_bits += ((uint32_t) (advance & 3)) << 3;
    if (hs->offset_bits >= 32) {
        hs->offset += 4;
        hs->offset_bits -= 32;
    }
    code = huff_get_next_word(hs, hs->offset, &hs->this_word);
    if (code < 0) {
        return jbig2_error(hs->ctx, JBIG2_SEVERITY_WARNING, JBIG2_UNKNOWN_SEGMENT_NUMBER, "failed to get first huffman word after advancing");
    }
    code = huff_get_next_word(hs, hs->offset + 4, &hs->next_word);
    if (code < 0) {
        return jbig2_error(hs->ctx, JBIG2_SEVERITY_WARNING, JBIG2_UNKNOWN_SEGMENT_NUMBER, "failed to get second huffman word after advancing");
    }
    if (hs->offset_bits > 0)
        hs->this_word = (hs->this_word << hs->offset_bits) | (hs->next_word >> (32 - hs->offset_bits));
    return 0;
}

size_t
jbig2_huffman_offset(Jbig2HuffmanState *hs)
{
    return hs->offset + (hs->offset_bits >> 3);
}

int32_t
jbig2_huffman_get_bits(Jbig2HuffmanState *hs, const int bits, int *err)
{
    uint32_t this_word = hs->this_word;
    int32_t result;
    int code;

    if (hs->offset_limit && hs->offset >= hs->offset_limit) {
        *err = -1;
        return jbig2_error(hs->ctx, JBIG2_SEVERITY_FATAL, JBIG2_UNKNOWN_SEGMENT_NUMBER, "end of jbig2 buffer reached at offset "FMTZ, (FMTZ_CAST) hs->offset);
    }

    result = this_word >> (32 - bits);
    hs->offset_bits += bits;
    if (hs->offset_bits >= 32) {
        hs->offset += 4;
        hs->offset_bits -= 32;
        hs->this_word = hs->next_word;
        code = huff_get_next_word(hs, hs->offset + 4, &hs->next_word);
        if (code < 0) {
            return jbig2_error(hs->ctx, JBIG2_SEVERITY_WARNING, JBIG2_UNKNOWN_SEGMENT_NUMBER, "failed to get next huffman word");
        }
        if (hs->offset_bits) {
            hs->this_word = (hs->this_word << hs->offset_bits) | (hs->next_word >> (32 - hs->offset_bits));
        } else {
            hs->this_word = (hs->this_word << hs->offset_bits);
        }
    } else {
        hs->this_word = (this_word << bits) | (hs->next_word >> (32 - hs->offset_bits));
    }

    return result;
}

int32_t
jbig2_huffman_get(Jbig2HuffmanState *hs, const Jbig2HuffmanTable *table, bool *oob)
{
    Jbig2HuffmanEntry *entry;
    byte flags;
    int offset_bits = hs->offset_bits;
    uint32_t this_word = hs->this_word;
    uint32_t next_word;
    int RANGELEN;
    int32_t result;

    if (hs->offset_limit && hs->offset >= hs->offset_limit) {
        if (oob)
            *oob = -1;
        return jbig2_error(hs->ctx, JBIG2_SEVERITY_FATAL, JBIG2_UNKNOWN_SEGMENT_NUMBER, "end of Jbig2WordStream reached at offset "FMTZ, (FMTZ_CAST) hs->offset);
    }

    for (;;) {
        int log_table_size = table->log_table_size;
        int PREFLEN;
        int code;

        entry = &table->entries[log_table_size > 0 ? this_word >> (32 - log_table_size) : 0];
        flags = entry->flags;
        PREFLEN = entry->PREFLEN;
        if (flags == (byte) -1 || PREFLEN == (byte) -1) {
            if (oob)
                *oob = -1;
            return jbig2_error(hs->ctx, JBIG2_SEVERITY_FATAL, JBIG2_UNKNOWN_SEGMENT_NUMBER, "encountered unpopulated huffman table entry");
        }

        next_word = hs->next_word;
        offset_bits += PREFLEN;
        if (offset_bits >= 32) {
            this_word = next_word;
            hs->offset += 4;
            code = huff_get_next_word(hs, hs->offset + 4, &next_word);
            if (code < 0) {
                return jbig2_error(hs->ctx, JBIG2_SEVERITY_WARNING, JBIG2_UNKNOWN_SEGMENT_NUMBER, "failed to get next huffman word");
            }
            else if (code < 4)
            {
                hs->offset_limit = hs->offset + 4 + code;
            }
            offset_bits -= 32;
            hs->next_word = next_word;
            PREFLEN = offset_bits;
        }
        if (PREFLEN)
            this_word = (this_word << PREFLEN) | (next_word >> (32 - offset_bits));
        if (flags & JBIG2_HUFFMAN_FLAGS_ISEXT) {
            table = entry->u.ext_table;
        } else
            break;
    }
    result = entry->u.RANGELOW;
    RANGELEN = entry->RANGELEN;
    if (RANGELEN > 0) {
        int32_t HTOFFSET;
        int code;

        HTOFFSET = this_word >> (32 - RANGELEN);
        if (flags & JBIG2_HUFFMAN_FLAGS_ISLOW)
            result -= HTOFFSET;
        else
            result += HTOFFSET;

        offset_bits += RANGELEN;
        if (offset_bits >= 32) {
            this_word = next_word;
            hs->offset += 4;
            code = huff_get_next_word(hs, hs->offset + 4, &next_word);
            if (code < 0) {
                return jbig2_error(hs->ctx, JBIG2_SEVERITY_WARNING, JBIG2_UNKNOWN_SEGMENT_NUMBER, "failed to get next huffman word");
            }
            else if (code < 4)
            {
                hs->offset_limit = hs->offset + 4 + code;
            }
            offset_bits -= 32;
            hs->next_word = next_word;
            RANGELEN = offset_bits;
        }
        if (RANGELEN)
            this_word = (this_word << RANGELEN) | (next_word >> (32 - offset_bits));
    }

    hs->this_word = this_word;
    hs->offset_bits = offset_bits;

    if (oob != NULL)
        *oob = (flags & JBIG2_HUFFMAN_FLAGS_ISOOB);

    return result;
}

#define LOG_TABLE_SIZE_MAX 16

Jbig2HuffmanTable *
jbig2_build_huffman_table(Jbig2Ctx *ctx, const Jbig2HuffmanParams *params)
{
    int *LENCOUNT;
    int LENMAX = -1;
    const int lencountcount = 256;
    const Jbig2HuffmanLine *lines = params->lines;
    int n_lines = params->n_lines;
    int i, j;
    uint32_t max_j;
    int log_table_size = 0;
    Jbig2HuffmanTable *result;
    Jbig2HuffmanEntry *entries;
    int CURLEN;
    int firstcode = 0;
    int CURCODE;
    int CURTEMP;

    LENCOUNT = jbig2_new(ctx, int, lencountcount);

    if (LENCOUNT == NULL) {
        jbig2_error(ctx, JBIG2_SEVERITY_FATAL, JBIG2_UNKNOWN_SEGMENT_NUMBER, "failed to allocate huffman histogram");
        return NULL;
    }
    memset(LENCOUNT, 0, sizeof(int) * lencountcount);

    for (i = 0; i < params->n_lines; i++) {
        int PREFLEN = lines[i].PREFLEN;
        int lts;

        if (PREFLEN > LENMAX) {
            for (j = LENMAX + 1; j < PREFLEN + 1; j++)
                LENCOUNT[j] = 0;
            LENMAX = PREFLEN;
        }
        LENCOUNT[PREFLEN]++;

        lts = PREFLEN + lines[i].RANGELEN;
        if (lts > LOG_TABLE_SIZE_MAX)
            lts = PREFLEN;
        if (lts <= LOG_TABLE_SIZE_MAX && log_table_size < lts)
            log_table_size = lts;
    }
    jbig2_error(ctx, JBIG2_SEVERITY_DEBUG, JBIG2_UNKNOWN_SEGMENT_NUMBER, "constructing huffman table log size %d", log_table_size);
    max_j = 1 << log_table_size;

    result = jbig2_new(ctx, Jbig2HuffmanTable, 1);
    if (result == NULL) {
        jbig2_error(ctx, JBIG2_SEVERITY_FATAL, JBIG2_UNKNOWN_SEGMENT_NUMBER, "failed to allocate result");
        jbig2_free(ctx->allocator, LENCOUNT);
        return NULL;
    }
    result->log_table_size = log_table_size;
    entries = jbig2_new(ctx, Jbig2HuffmanEntry, max_j);
    if (entries == NULL) {
        jbig2_error(ctx, JBIG2_SEVERITY_FATAL, JBIG2_UNKNOWN_SEGMENT_NUMBER, "failed to allocate result entries");
        jbig2_free(ctx->allocator, result);
        jbig2_free(ctx->allocator, LENCOUNT);
        return NULL;
    }

    memset(entries, 0xFF, sizeof(Jbig2HuffmanEntry) * max_j);
    result->entries = entries;

    LENCOUNT[0] = 0;

    for (CURLEN = 1; CURLEN <= LENMAX; CURLEN++) {
        int shift = log_table_size - CURLEN;

        firstcode = (firstcode + LENCOUNT[CURLEN - 1]) << 1;
        CURCODE = firstcode;

        for (CURTEMP = 0; CURTEMP < n_lines; CURTEMP++) {
            int PREFLEN = lines[CURTEMP].PREFLEN;

            if (PREFLEN == CURLEN) {
                int RANGELEN = lines[CURTEMP].RANGELEN;
                uint32_t start_j = CURCODE << shift;
                uint32_t end_j = (CURCODE + 1) << shift;
                uint32_t cur_j;
                byte eflags = 0;

                if (end_j > max_j) {
                    jbig2_error(ctx, JBIG2_SEVERITY_FATAL, JBIG2_UNKNOWN_SEGMENT_NUMBER, "ran off the end of the entries table! (%d >= %d)", end_j, max_j);
                    jbig2_free(ctx->allocator, result->entries);
                    jbig2_free(ctx->allocator, result);
                    jbig2_free(ctx->allocator, LENCOUNT);
                    return NULL;
                }

                if (params->HTOOB && CURTEMP == n_lines - 1)
                    eflags |= JBIG2_HUFFMAN_FLAGS_ISOOB;
                if (CURTEMP == n_lines - (params->HTOOB ? 3 : 2))
                    eflags |= JBIG2_HUFFMAN_FLAGS_ISLOW;
                if (PREFLEN + RANGELEN > LOG_TABLE_SIZE_MAX) {
                    for (cur_j = start_j; cur_j < end_j; cur_j++) {
                        entries[cur_j].u.RANGELOW = lines[CURTEMP].RANGELOW;
                        entries[cur_j].PREFLEN = PREFLEN;
                        entries[cur_j].RANGELEN = RANGELEN;
                        entries[cur_j].flags = eflags;
                    }
                } else {
                    for (cur_j = start_j; cur_j < end_j; cur_j++) {
                        int32_t HTOFFSET = (cur_j >> (shift - RANGELEN)) & ((1 << RANGELEN) - 1);

                        if (eflags & JBIG2_HUFFMAN_FLAGS_ISLOW)
                            entries[cur_j].u.RANGELOW = lines[CURTEMP].RANGELOW - HTOFFSET;
                        else
                            entries[cur_j].u.RANGELOW = lines[CURTEMP].RANGELOW + HTOFFSET;
                        entries[cur_j].PREFLEN = PREFLEN + RANGELEN;
                        entries[cur_j].RANGELEN = 0;
                        entries[cur_j].flags = eflags;
                    }
                }
                CURCODE++;
            }
        }
    }

    jbig2_free(ctx->allocator, LENCOUNT);

    return result;
}

void
jbig2_release_huffman_table(Jbig2Ctx *ctx, Jbig2HuffmanTable *table)
{
    if (table != NULL) {
        jbig2_free(ctx->allocator, table->entries);
        jbig2_free(ctx->allocator, table);
    }
}

static uint32_t
jbig2_table_read_bits(const byte *data, size_t *bitoffset, const int bitlen)
{
    uint32_t result = 0;
    size_t byte_offset = *bitoffset / 8;
    const int endbit = ((int) (*bitoffset & 7)) + bitlen;
    const int n_proc_bytes = (endbit + 7) / 8;
    const int rshift = n_proc_bytes * 8 - endbit;
    int i;

    for (i = n_proc_bytes - 1; i >= 0; i--) {
        uint32_t d = data[byte_offset++];
        const int nshift = i * 8 - rshift;

        if (nshift > 0)
            d <<= nshift;
        else if (nshift < 0)
            d >>= -nshift;
        result |= d;
    }
    result &= ~(UINT32_MAX << bitlen);
    *bitoffset += bitlen;
    return result;
}

int
jbig2_table(Jbig2Ctx *ctx, Jbig2Segment *segment, const byte *segment_data)
{
    Jbig2HuffmanParams *params = NULL;
    Jbig2HuffmanLine *line = NULL;

    segment->result = NULL;
    if (segment->data_length < 10)
        goto too_short;
    if (segment->data_length > SIZE_MAX / 8)
    {
        jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number, "segment data too large for Huffman Table");
        goto error_exit;
    }

    {

        const int code_table_flags = segment_data[0];
        const int HTOOB = code_table_flags & 0x01;

        const int HTPS = (code_table_flags >> 1 & 0x07) + 1;

        const int HTRS = (code_table_flags >> 4 & 0x07) + 1;

        const int32_t HTLOW = jbig2_get_int32(segment_data + 1);

        const int32_t HTHIGH = jbig2_get_int32(segment_data + 5);

        const size_t lines_max = (segment->data_length * 8 - HTPS * (HTOOB ? 3 : 2)) / (HTPS + HTRS) + (HTOOB ? 3 : 2);

        const byte *lines_data = segment_data + 9;
        const size_t lines_data_bitlen = (segment->data_length - 9) * 8;

        size_t boffset = 0;

        int32_t CURRANGELOW = HTLOW;
        size_t NTEMP = 0;

#ifdef JBIG2_DEBUG
        jbig2_error(ctx, JBIG2_SEVERITY_DEBUG, segment->number,
                    "DECODING USER TABLE... Flags: %d, HTOOB: %d, HTPS: %d, HTRS: %d, HTLOW: %d, HTHIGH: %d",
                    code_table_flags, HTOOB, HTPS, HTRS, HTLOW, HTHIGH);
#endif

        if (segment->data_length > SIZE_MAX / 8) {
            jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number, "Huffman Table size too large");
            goto error_exit;
        }
        if (HTLOW >= HTHIGH) {
            jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number, "invalid Huffman Table range");
            goto error_exit;
        }

        params = jbig2_new(ctx, Jbig2HuffmanParams, 1);
        if (params == NULL) {
            jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number, "failed to allocate Huffman Table Parameter");
            goto error_exit;
        }
        line = jbig2_new(ctx, Jbig2HuffmanLine, lines_max);
        if (line == NULL) {
            jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number, "failed to allocate huffman table lines");
            goto error_exit;
        }

        while (CURRANGELOW < HTHIGH) {

            if (boffset + HTPS >= lines_data_bitlen)
                goto too_short;
            if (NTEMP >= lines_max) {
                jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number, "huffman table line count exceeded");
                goto error_exit;
            }
            line[NTEMP].PREFLEN = jbig2_table_read_bits(lines_data, &boffset, HTPS);

            if (boffset + HTRS >= lines_data_bitlen)
                goto too_short;
            line[NTEMP].RANGELEN = jbig2_table_read_bits(lines_data, &boffset, HTRS);

            line[NTEMP].RANGELOW = CURRANGELOW;
            CURRANGELOW += (1 << line[NTEMP].RANGELEN);
            NTEMP++;
        }

        if (boffset + HTPS >= lines_data_bitlen)
            goto too_short;
        if (NTEMP >= lines_max) {
            jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number, "huffman table line count exceeded");
            goto error_exit;
        }
        line[NTEMP].PREFLEN = jbig2_table_read_bits(lines_data, &boffset, HTPS);
        line[NTEMP].RANGELEN = 32;
        line[NTEMP].RANGELOW = HTLOW - 1;
        NTEMP++;

        if (boffset + HTPS >= lines_data_bitlen)
            goto too_short;
        if (NTEMP >= lines_max) {
            jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number, "huffman table line count exceeded");
            goto error_exit;
        }
        line[NTEMP].PREFLEN = jbig2_table_read_bits(lines_data, &boffset, HTPS);
        line[NTEMP].RANGELEN = 32;
        line[NTEMP].RANGELOW = HTHIGH;
        NTEMP++;

        if (HTOOB) {

            if (boffset + HTPS >= lines_data_bitlen)
                goto too_short;
            if (NTEMP >= lines_max) {
                jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number, "huffman table line count exceeded");
                goto error_exit;
            }
            line[NTEMP].PREFLEN = jbig2_table_read_bits(lines_data, &boffset, HTPS);
            line[NTEMP].RANGELEN = 0;
            line[NTEMP].RANGELOW = 0;
            NTEMP++;
        }
        if (NTEMP != lines_max) {
            Jbig2HuffmanLine *new_line = jbig2_renew(ctx, line,
                                                     Jbig2HuffmanLine, NTEMP);

            if (new_line == NULL) {
                jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number, "failed to reallocate huffman table lines");
                goto error_exit;
            }
            line = new_line;
        }
        params->HTOOB = HTOOB;

        if (NTEMP > INT32_MAX) {
            jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number, "implausible number of huffman table lines");
            goto error_exit;
        }
        params->n_lines = (int)NTEMP;
        params->lines = line;
        segment->result = params;

#ifdef JBIG2_DEBUG
        {
            size_t i;

            for (i = 0; i < NTEMP; i++) {
                jbig2_error(ctx, JBIG2_SEVERITY_DEBUG, segment->number,
                            "Line: "FMTZ", PREFLEN: %d, RANGELEN: %d, RANGELOW: %d",
                            (FMTZ_CAST) i, params->lines[i].PREFLEN, params->lines[i].RANGELEN, params->lines[i].RANGELOW);
            }
        }
#endif
    }
    return 0;

too_short:
    jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number, "segment too short");
error_exit:
    jbig2_free(ctx->allocator, line);
    jbig2_free(ctx->allocator, params);
    return -1;
}

void
jbig2_table_free(Jbig2Ctx *ctx, Jbig2HuffmanParams *params)
{
    if (params != NULL) {
        jbig2_free(ctx->allocator, (void *)params->lines);
        jbig2_free(ctx->allocator, params);
    }
}

const Jbig2HuffmanParams *
jbig2_find_table(Jbig2Ctx *ctx, Jbig2Segment *segment, int index)
{
    int i, table_index = 0;

    for (i = 0; i < segment->referred_to_segment_count; i++) {
        const Jbig2Segment *const rsegment = jbig2_find_segment(ctx, segment->referred_to_segments[i]);

        if (rsegment && (rsegment->flags & 63) == 53) {
            if (table_index == index)
                return (const Jbig2HuffmanParams *)rsegment->result;
            ++table_index;
        }
    }

    jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number, "huffman table not found (%d)", index);
    return NULL;
}

#ifdef TEST
#include <stdio.h>

static const byte test_stream[] = { 0xe9, 0xcb, 0xf4, 0x00 };
static const byte test_tabindex[] = { 4, 2, 2, 1 };

static int
test_get_word1(Jbig2Ctx *ctx, Jbig2WordStream *self, size_t offset, uint32_t *word)
{
    uint32_t val = 0;
    int ret = 0;

    (void) ctx;

    if (self == NULL || word == NULL)
        return -1;
    if (offset >= sizeof (test_stream))
        return 0;

    if (offset < sizeof(test_stream)) {
        val |= test_stream[offset] << 24;
        ret++;
    }
    if (offset + 1 < sizeof(test_stream)) {
        val |= test_stream[offset + 1] << 16;
        ret++;
    }
    if (offset + 2 < sizeof(test_stream)) {
        val |= test_stream[offset + 2] << 8;
        ret++;
    }
    if (offset + 3 < sizeof(test_stream)) {
        val |= test_stream[offset + 3];
        ret++;
    }
    *word = val;
    return ret;
}

static int test1(void)
{
    Jbig2Ctx *ctx;
    Jbig2HuffmanTable *tables[5];
    Jbig2HuffmanState *hs = NULL;
    Jbig2WordStream ws;
    bool oob;
    int32_t code;
    int i;
    int success = 0;

    ctx = jbig2_ctx_new(NULL, 0, NULL, NULL, NULL);
    if (ctx == NULL) {
        fprintf(stderr, "Failed to allocate jbig2 context\n");
        goto cleanup;
    }

    tables[0] = NULL;
    tables[1] = jbig2_build_huffman_table(ctx, &jbig2_huffman_params_A);
    tables[2] = jbig2_build_huffman_table(ctx, &jbig2_huffman_params_B);
    tables[3] = NULL;
    tables[4] = jbig2_build_huffman_table(ctx, &jbig2_huffman_params_D);
    if (tables[1] == NULL || tables[2] == NULL || tables[4] == NULL)
    {
        fprintf(stderr, "Failed to build huffman tables");
        goto cleanup;
    }

    ws.get_next_word = test_get_word1;
    hs = jbig2_huffman_new(ctx, &ws);
    if (hs == NULL) {
        fprintf(stderr, "Failed to allocate huffman state");
        goto cleanup;
    }

    printf("testing jbig2 huffman decoding...");
    printf("\t(should be 8 5 (oob) 8)\n");

    {
        int i;
        int sequence_length = sizeof(test_tabindex);

        for (i = 0; i < sequence_length; i++) {
            code = jbig2_huffman_get(hs, tables[test_tabindex[i]], &oob);
            if (oob)
                printf("(oob) ");
            else
                printf("%d ", code);
        }
    }

    printf("\n");

    success = 1;

cleanup:
    jbig2_huffman_free(ctx, hs);
    for (i = 0; i < 5; i++)
        jbig2_release_huffman_table(ctx, tables[i]);
    jbig2_ctx_free(ctx);

    return success;
}

#include <stdio.h>

const int32_t test_output_A[] = {

    0,
    1,
    14,
    15,

    16,
    17,
    270,
    271,

    272,
    273,
    65806,
    65807,

    65808,
    65809,
};
const byte test_input_A[] = {

       0x00,     0x5c,     0xf8,     0x02,

       0x01,     0xbf,     0xaf,     0xfc,

       0x00,     0x01,     0x80,     0x00,

       0x77,     0xff,     0xf6,     0xff,

       0xff,     0xe0,     0x00,     0x00,

       0x00,     0x1c,     0x00,     0x00,

       0x00,     0x04,
};

const int32_t test_output_B[] = {

    0,

    1,

    2,

    3,
    4,
    9,
    10,

    11,
    12,
    73,
    74,

    75,
    76,

};
const byte test_input_B[] = {

       0x5b,     0x87,     0x1e,     0xdd,

       0xfc,     0x07,     0x81,     0xf7,

       0xde,     0xff,     0xe0,     0x00,

       0x00,     0x00,     0x0f,     0x80,

       0x00,     0x00,     0x00,     0x7f,
};

const int32_t test_output_C[] = {

    -256,
    -255,
    -2,
    -1,

    0,

    1,

    2,

    3,
    4,
    9,
    10,

    11,
    12,
    73,
    74,

    -257,
    -258,

    75,
    76,

};
const byte test_input_C[] = {

       0xfe,     0x00,     0xfe,     0x01,

       0xfe,     0xfe,     0xfe,     0xff,

       0x5b,     0x87,     0x1e,     0xdd,

       0xfc,     0x07,     0x81,     0xf7,

       0xde,     0xff,     0xfc,     0x00,

       0x00,     0x00,     0x03,     0xfc,

       0x00,     0x00,     0x00,     0x07,

       0xf0,     0x00,     0x00,     0x00,

       0x07,     0xe0,     0x00,     0x00,

       0x00,     0x1f,     0x80,
};

const int32_t test_output_D[] = {

    1,

    2,

    3,

    4,
    5,
    10,
    11,

    12,
    13,
    74,
    75,

    76,
    77,
};
const byte test_input_D[] = {

       0x5b,     0x87,     0x1e,     0xdd,

       0xfc,     0x07,     0x81,     0xf7,

       0xde,     0xff,     0xe0,     0x00,

       0x00,     0x00,     0x1f,     0x00,

       0x00,     0x00,     0x01,
};

const int32_t test_output_E[] = {

    -255,
    -254,
    -1,
    0,

    1,

    2,

    3,

    4,
    5,
    10,
    11,

    12,
    13,
    74,
    75,

    -256,
    -257,

    76,
    77,
};
const byte test_input_E[] = {

       0xfc,     0x01,     0xf8,     0x07,

       0xf7,     0xf7,     0xef,     0xf5,

       0xb8,     0x71,     0xed,     0xdf,

       0xc0,     0x78,     0x1f,     0x7d,

       0xef,     0xff,     0x80,     0x00,

       0x00,     0x00,     0x7f,     0x00,

       0x00,     0x00,     0x01,     0xf8,

       0x00,     0x00,     0x00,     0x03,

       0xe0,     0x00,     0x00,     0x00,

       0x10,
};

const int32_t test_output_F[] = {

    -2048,
    -2047,
    -1026,
    -1025,

    -1024,
    -1023,
    -514,
    -513,

    -512,
    -511,
    -258,
    -257,

    -256,
    -255,
    -130,
    -129,

    -128,
    -127,
    -66,
    -65,

    -64,
    -63,
    -34,
    -33,

    -32,
    -31,
    -2,
    -1,

    0,
    1,
    126,
    127,

    128,
    129,
    254,
    255,

    256,
    257,
    510,
    511,

    512,
    513,
    1022,
    1023,

    1024,
    1025,
    2046,
    2047,

    -2049,
    -2050,

    2048,
    2049,
};
const byte test_input_F[] = {

       0xe0,     0x01,     0xc0,     0x07,

       0x9f,     0xf7,     0x3f,     0xf8,

       0x00,     0x40,     0x06,     0x3f,

       0xd1,     0xff,     0x90,     0x09,

       0x01,     0x9f,     0xe9,     0xff,

       0xa0,     0x14,     0x06,     0xbf,

       0x57,     0xfe,     0x81,     0xd0,

       0x7b,     0xf7,     0x7f,     0xf0,

       0x3c,     0x1f,     0x7b,     0xdf,

       0xb0,     0x58,     0x6f,     0xd7,

       0xf0,     0x00,     0x04,     0xfc,

       0x7f,     0x40,     0x10,     0x15,

       0xf9,     0x7f,     0x60,     0x0c,

       0x05,     0xff,     0x3f,     0xfc,

       0x00,     0x60,     0x07,     0x3f,

       0xd9,     0xff,     0xd0,     0x03,

       0x40,     0x1d,     0xff,     0xb7,

       0xff,     0xf8,     0x00,     0x00,

       0x00,     0x03,     0xe0,     0x00,

       0x00,     0x00,     0x1f,     0xc0,

       0x00,     0x00,     0x00,     0x3f,

       0x00,     0x00,     0x00,     0x01,
};

const int32_t test_output_G[] = {

    -1024,
    -1023,
    -514,
    -513,

    -512,
    -511,
    -258,
    -257,

    -256,
    -255,
    -130,
    -129,

    -128,
    -127,
    -66,
    -65,

    -64,
    -63,
    -34,
    -33,

    -32,
    -31,
    -2,
    -1,

    0,
    1,
    30,
    31,

    32,
    33,
    62,
    63,

    64,
    65,
    126,
    127,

    128,
    129,
    254,
    255,

    256,
    257,
    510,
    511,

    512,
    513,
    1022,
    1023,

    1024,
    1025,
    2046,
    2047,

    -1025,
    -1026,

    2048,
    2049,
};
const byte test_input_G[] = {

       0x80,     0x04,     0x00,     0x63,

       0xfd,     0x1f,     0xf0,     0x00,

       0x00,     0x47,     0xf0,     0xff,

       0x90,     0x12,     0x06,     0x7f,

       0x4f,     0xfd,     0x01,     0xa0,

       0x75,     0xf6,     0xbf,     0xd8,

       0x36,     0x1d,     0xfb,     0x7f,

       0xa0,     0x50,     0x6b,     0xd5,

       0xfb,     0x05,     0x86,     0xfd,

       0x7f,     0xe0,     0x38,     0x1e,

       0x7b,     0x9f,     0xe8,     0x1d,

       0x07,     0xbf,     0x77,     0xfc,

       0x01,     0x80,     0x73,     0xf6,

       0x7f,     0x20,     0x04,     0x04,

       0xff,     0x1f,     0xf4,     0x00,

       0x40,     0x15,     0xfe,     0x5f,

       0xf6,     0x00,     0x30,     0x05,

       0xff,     0xcf,     0xff,     0xf0,

       0x00,     0x00,     0x00,     0x07,

       0x80,     0x00,     0x00,     0x00,

       0x7e,     0x00,     0x00,     0x00,

       0x01,     0xf0,     0x00,     0x00,

       0x00,     0x10,
};

const int32_t test_output_H[] = {

    -15,
    -14,
    -9,
    -8,

    -7,
    -6,

    -5,
    -4,

    -3,

    -2,

    -1,

    0,
    1,

    2,

    3,

    4,
    5,
    18,
    19,

    20,
    21,

    22,
    23,
    36,
    37,

    38,
    39,
    68,
    69,

    70,
    71,
    132,
    133,

    134,
    135,
    260,
    261,

    262,
    263,
    388,
    389,

    390,
    391,
    644,
    645,

    646,
    647,
    1668,
    1669,

    -16,
    -17,

    1670,
    1671,

};
const byte test_input_H[] = {

       0xfc,     0x1f,     0x87,     0xf3,

       0x7e,     0x7f,     0xe3,     0xf9,

       0xfd,     0x7e,     0xff,     0xbf,

       0x28,     0x1d,     0x75,     0x02,

       0x0c,     0xe9,     0xfd,     0xbb,

       0xd8,     0x58,     0xdf,     0x5f,

       0xe0,     0x30,     0x39,     0xec,

       0xfe,     0xc0,     0xd8,     0x3b,

       0xfb,     0x7f,     0xf0,     0x07,

       0x00,     0xf3,     0xf7,     0x3f,

       0xf8,     0x03,     0xc0,     0x3e,

       0x7e,     0xf3,     0xff,     0xd0,

       0x0f,     0xa0,     0x3f,     0x7f,

       0xbe,     0xff,     0xfa,     0x00,

       0x7a,     0x00,     0xfb,     0xff,

       0x7b,     0xff,     0xff,     0x80,

       0x00,     0x00,     0x00,     0x3f,

       0xc0,     0x00,     0x00,     0x00,

       0x3f,     0xf0,     0x00,     0x00,

       0x00,     0x0f,     0xf8,     0x00,

       0x00,     0x00,     0x0a,
};

const int32_t test_output_I[] = {

    -31,
    -30,
    -17,
    -16,

    -15,
    -14,
    -13,
    -12,

    -11,
    -10,
    -9,
    -8,

    -7,
    -6,

    -5,
    -4,

    -3,
    -2,

    -1,
    0,

    1,
    2,

    3,
    4,

    5,
    6,

    7,
    8,
    37,
    38,

    39,
    40,
    41,
    42,

    43,
    44,
    73,
    74,

    75,
    76,
    137,
    138,

    139,
    140,
    265,
    266,

    267,
    268,
    521,
    522,

    523,
    524,
    777,
    778,

    779,
    780,
    1289,
    1290,

    1291,
    1292,
    3337,
    3338,

    -32,
    -33,

    3339,
    3340,

};
const byte test_input_I[] = {

       0xfc,     0x0f,     0xc1,     0xfc,

       0xef,     0xcf,     0xfe,     0x1f,

       0xc7,     0xf9,     0x7f,     0x3f,

       0xd3,     0xf5,     0xfd,     0xbf,

       0x7f,     0xeb,     0xfb,     0xf8,

       0xf9,     0xa5,     0x51,     0x59,

       0xf4,     0xd7,     0xa7,     0x58,

       0x08,     0x19,     0xe9,     0xfe,

       0xce,     0xde,     0xee,     0xfb,

       0x05,     0x86,     0xfd,     0x7f,

       0xc0,     0x30,     0x1c,     0xfb,

       0x3f,     0xd8,     0x0d,     0x81,

       0xdf,     0xed,     0xff,     0xe0,

       0x07,     0x00,     0x79,     0xfd,

       0xcf,     0xff,     0x00,     0x3c,

       0x01,     0xf3,     0xfb,     0xcf,

       0xff,     0xa0,     0x0f,     0xa0,

       0x1f,     0xbf,     0xef,     0xbf,

       0xff,     0x40,     0x07,     0xa0,

       0x07,     0xdf,     0xfd,     0xef,

       0xff,     0xff,     0x00,     0x00,

       0x00,     0x00,     0x7f,     0x80,

       0x00,     0x00,     0x00,     0x7f,

       0xe0,     0x00,     0x00,     0x00,

       0x1f,     0xf0,     0x00,     0x00,

       0x00,     0x10,
};

const int32_t test_output_J[] = {

    -21,
    -20,
    -7,
    -6,

    -5,

    -4,

    -3,

    -2,
    -1,
    0,
    1,

    2,

    3,

    4,

    5,

    6,
    7,
    68,
    69,

    70,
    71,
    100,
    101,

    102,
    103,
    132,
    133,

    134,
    135,
    196,
    197,

    198,
    199,
    324,
    325,

    326,
    327,
    580,
    581,

    582,
    583,
    1092,
    1093,

    1094,
    1095,
    2116,
    2117,

    2118,
    2119,
    4164,
    4165,

    -22,
    -23,

    4166,
    4167,

};
const byte test_input_J[] = {

       0xf4,     0x1e,     0x87,     0xd7,

       0x7a,     0xff,     0xcf,     0x78,

       0x01,     0x23,     0xce,     0xdf,

       0x3f,     0x50,     0x10,     0x5f,

       0x9f,     0xf4,     0x0d,     0x07,

       0x5e,     0xd7,     0xf7,     0x06,

       0xe1,     0xdf,     0xdb,     0xff,

       0x80,     0x38,     0x07,     0x8f,

       0xb8,     0xff,     0x90,     0x1c,

       0x81,     0xe7,     0xf7,     0x3f,

       0xfa,     0x00,     0xe8,     0x07,

       0xaf,     0xee,     0xbf,     0xfb,

       0x00,     0x76,     0x01,     0xef,

       0xfd,     0xdf,     0xff,     0xc0,

       0x03,     0xc0,     0x07,     0xcf,

       0xfb,     0xcf,     0xff,     0xe8,

       0x00,     0xfa,     0x00,     0x7e,

       0xff,     0xef,     0xbf,     0xff,

       0xf8,     0x00,     0x00,     0x00,

       0x03,     0xf8,     0x00,     0x00,

       0x00,     0x07,     0xfc,     0x00,

       0x00,     0x00,     0x03,     0xfc,

       0x00,     0x00,     0x00,     0x06,
};

const int32_t test_output_K[] = {

    1,

    2,
    3,

    4,

    5,
    6,

    7,
    8,

    9,
    10,
    11,
    12,

    13,
    14,
    15,
    16,

    17,
    18,
    19,
    20,

    21,
    22,
    27,
    28,

    29,
    30,
    43,
    44,

    45,
    46,
    75,
    76,

    77,
    78,
    139,
    140,

    141,
    142,
};
const byte test_input_K[] = {

       0x4b,     0x9a,     0xdf,     0x1c,

       0xf4,     0xeb,     0xdb,     0xbf,

       0x87,     0x8f,     0x97,     0x9f,

       0xa3,     0xd3,     0xea,     0xf5,

       0xfb,     0x1e,     0xcf,     0xbd,

       0xef,     0xfc,     0x0f,     0x83,

       0xf3,     0xbe,     0x7f,     0xd0,

       0x7d,     0x0f,     0xdf,     0x7d,

       0xff,     0xe0,     0x3f,     0x03,

       0xfb,     0xef,     0xdf,     0xff,

       0x00,     0x00,     0x00,     0x00,

       0xfe,     0x00,     0x00,     0x00,

       0x02,
};

const int32_t test_output_L[] = {

    1,

    2,

    3,
    4,

    5,

    6,
    7,

    8,
    9,

    10,

    11,
    12,

    13,
    14,
    15,
    16,

    17,
    18,
    23,
    24,

    25,
    26,
    39,
    40,

    41,
    42,
    71,
    72,

    73,
    74,
};
const byte test_input_L[] = {

       0x59,     0xbc,     0xeb,     0xbf,

       0x1e,     0x7d,     0x7b,     0x7b,

       0xfc,     0x3e,     0x3f,     0x2f,

       0x9f,     0xd1,     0xf4,     0xfd,

       0xdf,     0x7f,     0xe0,     0xfc,

       0x3f,     0xbb,     0xf7,     0xff,

       0x03,     0xf8,     0x3f,     0xde,

       0xfe,     0xff,     0xf8,     0x00,

       0x00,     0x00,     0x07,     0xf8,

       0x00,     0x00,     0x00,     0x08,
};

const int32_t test_output_M[] = {

    1,

    2,

    3,

    4,

    5,
    6,

    7,
    8,
    13,
    14,

    15,
    16,

    17,
    18,
    19,
    20,

    21,
    22,
    27,
    28,

    29,
    30,
    43,
    44,

    45,
    46,
    75,
    76,

    77,
    78,
    139,
    140,

    141,
    142,
};
const byte test_input_M[] = {

       0x4c,     0xe6,     0xb7,     0x45,

       0x37,     0x5f,     0xd3,     0xaf,

       0x67,     0x6f,     0x77,     0x7f,

       0x83,     0xc3,     0xe6,     0xf3,

       0xfa,     0x1e,     0x8f,     0xbd,

       0xef,     0xfc,     0x0f,     0x83,

       0xf7,     0xbe,     0xff,     0xe0,

       0x3f,     0x03,     0xfb,     0xef,

       0xdf,     0xff,     0x00,     0x00,

       0x00,     0x00,     0xfe,     0x00,

       0x00,     0x00,     0x02,
};

const int32_t test_output_N[] = {

    -2,

    -1,

    0,

    1,

    2,
};
const byte test_input_N[] = {

       0x95,     0xb8,
};

const int32_t test_output_O[] = {

    -24,
    -23,
    -10,
    -9,

    -8,
    -7,
    -6,
    -5,

    -4,
    -3,

    -2,

    -1,

    0,

    1,

    2,

    3,
    4,

    5,
    6,
    7,
    8,

    9,
    10,
    23,
    24,

    -25,
    -26,

    25,
    26,
};
const byte test_input_O[] = {

       0xf8,     0x1f,     0x07,     0xe7,

       0x7c,     0xff,     0x0f,     0x1f,

       0x2f,     0x3e,     0x39,     0xc8,

       0xbb,     0xd7,     0x7e,     0x9e,

       0xbe,     0xde,     0xff,     0x43,

       0xe8,     0xfd,     0xef,     0xbf,

       0xf8,     0x00,     0x00,     0x00,

       0x03,     0xf0,     0x00,     0x00,

       0x00,     0x0f,     0xf0,     0x00,

       0x00,     0x00,     0x0f,     0xe0,

       0x00,     0x00,     0x00,     0x20,
};

typedef struct test_huffmancodes {
    const char *name;
    const Jbig2HuffmanParams *params;
    const byte *input;
    const size_t input_len;
    const int32_t *output;
    const size_t output_len;
} test_huffmancodes_t;

#define countof(x) (sizeof((x)) / sizeof((x)[0]))

#define DEF_TEST_HUFFMANCODES(x) { \
    #x, \
    &jbig2_huffman_params_##x, \
    test_input_##x, countof(test_input_##x), \
    test_output_##x, countof(test_output_##x), \
}

test_huffmancodes_t tests[] = {
    DEF_TEST_HUFFMANCODES(A),
    DEF_TEST_HUFFMANCODES(B),
    DEF_TEST_HUFFMANCODES(C),
    DEF_TEST_HUFFMANCODES(D),
    DEF_TEST_HUFFMANCODES(E),
    DEF_TEST_HUFFMANCODES(F),
    DEF_TEST_HUFFMANCODES(G),
    DEF_TEST_HUFFMANCODES(H),
    DEF_TEST_HUFFMANCODES(I),
    DEF_TEST_HUFFMANCODES(J),
    DEF_TEST_HUFFMANCODES(K),
    DEF_TEST_HUFFMANCODES(L),
    DEF_TEST_HUFFMANCODES(M),
    DEF_TEST_HUFFMANCODES(N),
    DEF_TEST_HUFFMANCODES(O),
};

typedef struct test_stream {
    Jbig2WordStream ws;
    test_huffmancodes_t *h;
} test_stream_t;

static int
test_get_word2(Jbig2Ctx *ctx, Jbig2WordStream *self, size_t offset, uint32_t *word)
{
    test_stream_t *st = (test_stream_t *) self;
    uint32_t val = 0;
    int ret = 0;

    (void) ctx;

    if (st == NULL || st->h == NULL || word == NULL)
        return -1;
    if (offset >= st->h->input_len)
        return 0;

    if (offset < st->h->input_len) {
        val |= (st->h->input[offset] << 24);
        ret++;
    }
    if (offset + 1 < st->h->input_len) {
        val |= (st->h->input[offset + 1] << 16);
        ret++;
    }
    if (offset + 2 < st->h->input_len) {
        val |= (st->h->input[offset + 2] << 8);
        ret++;
    }
    if (offset + 3 < st->h->input_len) {
        val |= st->h->input[offset + 3];
        ret++;
    }
    *word = val;
    return ret;
}

static int test2(void)
{
    Jbig2Ctx *ctx;
    int success = 0;
    int i;

    ctx = jbig2_ctx_new(NULL, 0, NULL, NULL, NULL);
    if (ctx == NULL) {
        fprintf(stderr, "Failed to allocate jbig2 context\n");
        return 0;
    }

    for (i = 0; i < (int) countof(tests); i++) {
        Jbig2HuffmanTable *table;
        Jbig2HuffmanState *hs;
        test_stream_t st;
        int32_t code;
        bool oob;
        size_t j;

        st.ws.get_next_word = test_get_word2;
        st.h = &tests[i];
        printf("testing Standard Huffman table %s: ", st.h->name);
        table = jbig2_build_huffman_table(ctx, st.h->params);
        if (table == NULL) {
            fprintf(stderr, "jbig2_build_huffman_table() returned NULL!\n");
            jbig2_ctx_free(ctx);
            return 0;
        }

        hs = jbig2_huffman_new(ctx, &st.ws);
        if (hs == NULL) {
            fprintf(stderr, "jbig2_huffman_new() returned NULL!\n");
            jbig2_release_huffman_table(ctx, table);
            jbig2_ctx_free(ctx);
            return 0;
        }
        for (j = 0; j < st.h->output_len; j++) {
            printf("%d...", st.h->output[j]);
            code = jbig2_huffman_get(hs, table, &oob);
            if (code == st.h->output[j] && !oob) {
                printf("ok, ");
            } else {
                int need_comma = 0;

                printf("NG(");
                if (code != st.h->output[j]) {
                    printf("%d", code);
                    need_comma = 1;
                }
                if (oob) {
                    if (need_comma)
                        printf(",");
                    printf("OOB");
                }
                printf("), ");
            }
        }
        if (st.h->params->HTOOB) {
            printf("OOB...");
            code = jbig2_huffman_get(hs, table, &oob);
            if (oob) {
                printf("ok");
            } else {
                printf("NG(%d)", code);
            }
        }
        printf("\n");
        jbig2_huffman_free(ctx, hs);
        jbig2_release_huffman_table(ctx, table);
    }

    jbig2_ctx_free(ctx);

    if (i == countof(tests))
        success = 1;

    return success;
}

int
main(int argc, char **argv)
{
    (void) argc;
    (void) argv;
    return test1() && test2() ? 0 : 1;
}
#endif

#ifdef HAVE_CONFIG_H
#endif

#include <stdlib.h>

#define JBIG2_COUNTOF(x) (sizeof((x)) / sizeof((x)[0]))

static const Jbig2HuffmanLine jbig2_huffman_lines_A[] = {
    {1, 4, 0},
    {2, 8, 16},
    {3, 16, 272},
    {0, 32, -1},
    {3, 32, 65808}
};

const Jbig2HuffmanParams jbig2_huffman_params_A = { FALSE, JBIG2_COUNTOF(jbig2_huffman_lines_A), jbig2_huffman_lines_A };

static const Jbig2HuffmanLine jbig2_huffman_lines_B[] = {
    {1, 0, 0},
    {2, 0, 1},
    {3, 0, 2},
    {4, 3, 3},
    {5, 6, 11},
    {0, 32, -1},
    {6, 32, 75},
    {6, 0, 0}
};

const Jbig2HuffmanParams jbig2_huffman_params_B = { TRUE, JBIG2_COUNTOF(jbig2_huffman_lines_B), jbig2_huffman_lines_B };

static const Jbig2HuffmanLine jbig2_huffman_lines_C[] = {
    {8, 8, -256},
    {1, 0, 0},
    {2, 0, 1},
    {3, 0, 2},
    {4, 3, 3},
    {5, 6, 11},
    {8, 32, -257},
    {7, 32, 75},
    {6, 0, 0}
};

const Jbig2HuffmanParams jbig2_huffman_params_C = { TRUE, JBIG2_COUNTOF(jbig2_huffman_lines_C), jbig2_huffman_lines_C };

static const Jbig2HuffmanLine jbig2_huffman_lines_D[] = {
    {1, 0, 1},
    {2, 0, 2},
    {3, 0, 3},
    {4, 3, 4},
    {5, 6, 12},
    {0, 32, -1},
    {5, 32, 76},
};

const Jbig2HuffmanParams jbig2_huffman_params_D = { FALSE, JBIG2_COUNTOF(jbig2_huffman_lines_D), jbig2_huffman_lines_D };

static const Jbig2HuffmanLine jbig2_huffman_lines_E[] = {
    {7, 8, -255},
    {1, 0, 1},
    {2, 0, 2},
    {3, 0, 3},
    {4, 3, 4},
    {5, 6, 12},
    {7, 32, -256},
    {6, 32, 76}
};

const Jbig2HuffmanParams jbig2_huffman_params_E = { FALSE, JBIG2_COUNTOF(jbig2_huffman_lines_E), jbig2_huffman_lines_E };

static const Jbig2HuffmanLine jbig2_huffman_lines_F[] = {
    {5, 10, -2048},
    {4, 9, -1024},
    {4, 8, -512},
    {4, 7, -256},
    {5, 6, -128},
    {5, 5, -64},
    {4, 5, -32},
    {2, 7, 0},
    {3, 7, 128},
    {3, 8, 256},
    {4, 9, 512},
    {4, 10, 1024},
    {6, 32, -2049},
    {6, 32, 2048}
};

const Jbig2HuffmanParams jbig2_huffman_params_F = { FALSE, JBIG2_COUNTOF(jbig2_huffman_lines_F), jbig2_huffman_lines_F };

static const Jbig2HuffmanLine jbig2_huffman_lines_G[] = {
    {4, 9, -1024},
    {3, 8, -512},
    {4, 7, -256},
    {5, 6, -128},
    {5, 5, -64},
    {4, 5, -32},
    {4, 5, 0},
    {5, 5, 32},
    {5, 6, 64},
    {4, 7, 128},
    {3, 8, 256},
    {3, 9, 512},
    {3, 10, 1024},
    {5, 32, -1025},
    {5, 32, 2048}
};

const Jbig2HuffmanParams jbig2_huffman_params_G = { FALSE, JBIG2_COUNTOF(jbig2_huffman_lines_G), jbig2_huffman_lines_G };

static const Jbig2HuffmanLine jbig2_huffman_lines_H[] = {
    {8, 3, -15},
    {9, 1, -7},
    {8, 1, -5},
    {9, 0, -3},
    {7, 0, -2},
    {4, 0, -1},
    {2, 1, 0},
    {5, 0, 2},
    {6, 0, 3},
    {3, 4, 4},
    {6, 1, 20},
    {4, 4, 22},
    {4, 5, 38},
    {5, 6, 70},
    {5, 7, 134},
    {6, 7, 262},
    {7, 8, 390},
    {6, 10, 646},
    {9, 32, -16},
    {9, 32, 1670},
    {2, 0, 0}
};

const Jbig2HuffmanParams jbig2_huffman_params_H = { TRUE, JBIG2_COUNTOF(jbig2_huffman_lines_H), jbig2_huffman_lines_H };

static const Jbig2HuffmanLine jbig2_huffman_lines_I[] = {
    {8, 4, -31},
    {9, 2, -15},
    {8, 2, -11},
    {9, 1, -7},
    {7, 1, -5},
    {4, 1, -3},
    {3, 1, -1},
    {3, 1, 1},
    {5, 1, 3},
    {6, 1, 5},
    {3, 5, 7},
    {6, 2, 39},
    {4, 5, 43},
    {4, 6, 75},
    {5, 7, 139},
    {5, 8, 267},
    {6, 8, 523},
    {7, 9, 779},
    {6, 11, 1291},
    {9, 32, -32},
    {9, 32, 3339},
    {2, 0, 0}
};

const Jbig2HuffmanParams jbig2_huffman_params_I = { TRUE, JBIG2_COUNTOF(jbig2_huffman_lines_I), jbig2_huffman_lines_I };

static const Jbig2HuffmanLine jbig2_huffman_lines_J[] = {
    {7, 4, -21},
    {8, 0, -5},
    {7, 0, -4},
    {5, 0, -3},
    {2, 2, -2},
    {5, 0, 2},
    {6, 0, 3},
    {7, 0, 4},
    {8, 0, 5},
    {2, 6, 6},
    {5, 5, 70},
    {6, 5, 102},
    {6, 6, 134},
    {6, 7, 198},
    {6, 8, 326},
    {6, 9, 582},
    {6, 10, 1094},
    {7, 11, 2118},
    {8, 32, -22},
    {8, 32, 4166},
    {2, 0, 0}
};

const Jbig2HuffmanParams jbig2_huffman_params_J = { TRUE, JBIG2_COUNTOF(jbig2_huffman_lines_J), jbig2_huffman_lines_J };

static const Jbig2HuffmanLine jbig2_huffman_lines_K[] = {
    {1, 0, 1},
    {2, 1, 2},
    {4, 0, 4},
    {4, 1, 5},
    {5, 1, 7},
    {5, 2, 9},
    {6, 2, 13},
    {7, 2, 17},
    {7, 3, 21},
    {7, 4, 29},
    {7, 5, 45},
    {7, 6, 77},
    {0, 32, -1},
    {7, 32, 141}
};

const Jbig2HuffmanParams jbig2_huffman_params_K = { FALSE, JBIG2_COUNTOF(jbig2_huffman_lines_K), jbig2_huffman_lines_K };

static const Jbig2HuffmanLine jbig2_huffman_lines_L[] = {
    {1, 0, 1},
    {2, 0, 2},
    {3, 1, 3},
    {5, 0, 5},
    {5, 1, 6},
    {6, 1, 8},
    {7, 0, 10},
    {7, 1, 11},
    {7, 2, 13},
    {7, 3, 17},
    {7, 4, 25},
    {8, 5, 41},
    {8, 32, 73},
    {0, 32, -1},
    {0, 32, 0}
};

const Jbig2HuffmanParams jbig2_huffman_params_L = { FALSE, JBIG2_COUNTOF(jbig2_huffman_lines_L), jbig2_huffman_lines_L };

static const Jbig2HuffmanLine jbig2_huffman_lines_M[] = {
    {1, 0, 1},
    {3, 0, 2},
    {4, 0, 3},
    {5, 0, 4},
    {4, 1, 5},
    {3, 3, 7},
    {6, 1, 15},
    {6, 2, 17},
    {6, 3, 21},
    {6, 4, 29},
    {6, 5, 45},
    {7, 6, 77},
    {0, 32, -1},
    {7, 32, 141}
};

const Jbig2HuffmanParams jbig2_huffman_params_M = { FALSE, JBIG2_COUNTOF(jbig2_huffman_lines_M), jbig2_huffman_lines_M };

static const Jbig2HuffmanLine jbig2_huffman_lines_N[] = {
    {3, 0, -2},
    {3, 0, -1},
    {1, 0, 0},
    {3, 0, 1},
    {3, 0, 2},
    {0, 32, -1},
    {0, 32, 3},
};

const Jbig2HuffmanParams jbig2_huffman_params_N = { FALSE, JBIG2_COUNTOF(jbig2_huffman_lines_N), jbig2_huffman_lines_N };

static const Jbig2HuffmanLine jbig2_huffman_lines_O[] = {
    {7, 4, -24},
    {6, 2, -8},
    {5, 1, -4},
    {4, 0, -2},
    {3, 0, -1},
    {1, 0, 0},
    {3, 0, 1},
    {4, 0, 2},
    {5, 1, 3},
    {6, 2, 5},
    {7, 4, 9},
    {7, 32, -25},
    {7, 32, 25}
};

const Jbig2HuffmanParams jbig2_huffman_params_O = { FALSE, JBIG2_COUNTOF(jbig2_huffman_lines_O), jbig2_huffman_lines_O };

#ifdef HAVE_CONFIG_H
#endif

#include <string.h>

#ifndef _JBIG2_HALFTONE_H
#define _JBIG2_HALFTONE_H

typedef struct {
    int n_patterns;
    Jbig2Image **patterns;
    uint32_t HPW, HPH;
} Jbig2PatternDict;

typedef struct {
    bool HDMMR;
    uint32_t HDPW;
    uint32_t HDPH;
    uint32_t GRAYMAX;
    int HDTEMPLATE;
} Jbig2PatternDictParams;

typedef struct {
    byte flags;
    uint32_t HGW;
    uint32_t HGH;
    int32_t HGX;
    int32_t HGY;
    uint16_t HRX;
    uint16_t HRY;
    bool HMMR;
    int HTEMPLATE;
    bool HENABLESKIP;
    Jbig2ComposeOp HCOMBOP;
    bool HDEFPIXEL;
} Jbig2HalftoneRegionParams;

void jbig2_hd_release(Jbig2Ctx *ctx, Jbig2PatternDict *dict);

int jbig2_halftone_region(Jbig2Ctx *ctx, Jbig2Segment *segment, const byte *segment_data);
int jbig2_pattern_dictionary(Jbig2Ctx *ctx, Jbig2Segment *segment, const byte *segment_data);

#endif

static Jbig2PatternDict *
jbig2_hd_new(Jbig2Ctx *ctx, const Jbig2PatternDictParams *params, Jbig2Image *image)
{
    Jbig2PatternDict *new;
    const uint32_t N = params->GRAYMAX + 1;
    const uint32_t HPW = params->HDPW;
    const uint32_t HPH = params->HDPH;
    int code;
    uint32_t i, j;

    if (N == 0) {

        jbig2_error(ctx, JBIG2_SEVERITY_WARNING, JBIG2_UNKNOWN_SEGMENT_NUMBER, "params->GRAYMAX out of range");
        return NULL;
    }

    new = jbig2_new(ctx, Jbig2PatternDict, 1);
    if (new != NULL) {
        new->patterns = jbig2_new(ctx, Jbig2Image *, N);
        if (new->patterns == NULL) {
            jbig2_error(ctx, JBIG2_SEVERITY_FATAL, JBIG2_UNKNOWN_SEGMENT_NUMBER, "failed to allocate pattern in collective bitmap dictionary");
            jbig2_free(ctx->allocator, new);
            return NULL;
        }
        new->n_patterns = N;
        new->HPW = HPW;
        new->HPH = HPH;

        for (i = 0; i < N; i++) {
            new->patterns[i] = jbig2_image_new(ctx, HPW, HPH);
            if (new->patterns[i] == NULL) {
                jbig2_error(ctx, JBIG2_SEVERITY_WARNING, JBIG2_UNKNOWN_SEGMENT_NUMBER, "failed to allocate pattern element image");

                for (j = 0; j < i; j++)
                    jbig2_image_release(ctx, new->patterns[j]);
                jbig2_free(ctx->allocator, new->patterns);
                jbig2_free(ctx->allocator, new);
                return NULL;
            }

            code = jbig2_image_compose(ctx, new->patterns[i], image, -((int64_t) i) * HPW, 0, JBIG2_COMPOSE_REPLACE);
            if (code < 0) {
                jbig2_error(ctx, JBIG2_SEVERITY_WARNING, JBIG2_UNKNOWN_SEGMENT_NUMBER, "failed to compose image into collective bitmap dictionary");

                for (j = 0; j <= i; j++)
                    jbig2_image_release(ctx, new->patterns[j]);
                jbig2_free(ctx->allocator, new->patterns);
                jbig2_free(ctx->allocator, new);
                return NULL;
            }
        }
    } else {
        jbig2_error(ctx, JBIG2_SEVERITY_FATAL, JBIG2_UNKNOWN_SEGMENT_NUMBER, "failed to allocate collective bitmap dictionary");
    }

    return new;
}

void
jbig2_hd_release(Jbig2Ctx *ctx, Jbig2PatternDict *dict)
{
    int i;

    if (dict == NULL)
        return;
    if (dict->patterns != NULL)
        for (i = 0; i < dict->n_patterns; i++)
            jbig2_image_release(ctx, dict->patterns[i]);
    jbig2_free(ctx->allocator, dict->patterns);
    jbig2_free(ctx->allocator, dict);
}

static Jbig2PatternDict *
jbig2_decode_pattern_dict(Jbig2Ctx *ctx, Jbig2Segment *segment,
                          const Jbig2PatternDictParams *params, const byte *data, const size_t size, Jbig2ArithCx *GB_stats)
{
    Jbig2PatternDict *hd = NULL;
    Jbig2Image *image = NULL;
    Jbig2GenericRegionParams rparams;
    int code = 0;

    image = jbig2_image_new(ctx, params->HDPW * (params->GRAYMAX + 1), params->HDPH);
    if (image == NULL) {
        jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "failed to allocate collective bitmap for halftone dictionary");
        return NULL;
    }

    rparams.MMR = params->HDMMR;
    rparams.GBTEMPLATE = params->HDTEMPLATE;
    rparams.TPGDON = 0;
    rparams.USESKIP = 0;
    rparams.gbat[0] = -(int8_t) params->HDPW;
    rparams.gbat[1] = 0;
    rparams.gbat[2] = -3;
    rparams.gbat[3] = -1;
    rparams.gbat[4] = 2;
    rparams.gbat[5] = -2;
    rparams.gbat[6] = -2;
    rparams.gbat[7] = -2;

    if (params->HDMMR) {
        code = jbig2_decode_generic_mmr(ctx, segment, &rparams, data, size, image);
    } else {
        Jbig2WordStream *ws = jbig2_word_stream_buf_new(ctx, data, size);

        if (ws != NULL) {
            Jbig2ArithState *as = jbig2_arith_new(ctx, ws);

            if (as != NULL) {
                code = jbig2_decode_generic_region(ctx, segment, &rparams, as, image, GB_stats);
            } else {
                code = jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "failed to allocate arithmetic coding state when handling halftone dictionary");
            }

            jbig2_free(ctx->allocator, as);
            jbig2_word_stream_buf_free(ctx, ws);
        } else {
            code = jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "failed to allocate word stream when handling halftone dictionary");
        }
    }

    if (code == 0)
        hd = jbig2_hd_new(ctx, params, image);
    else
        jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "failed to decode immediate generic region");
    jbig2_image_release(ctx, image);

    return hd;
}

int
jbig2_pattern_dictionary(Jbig2Ctx *ctx, Jbig2Segment *segment, const byte *segment_data)
{
    Jbig2PatternDictParams params;
    Jbig2ArithCx *GB_stats = NULL;
    byte flags;
    int offset = 0;

    if (segment->data_length < 7) {
        return jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number, "segment too short");
    }
    flags = segment_data[0];
    params.HDMMR = flags & 1;
    params.HDTEMPLATE = (flags & 6) >> 1;
    params.HDPW = segment_data[1];
    params.HDPH = segment_data[2];
    params.GRAYMAX = jbig2_get_uint32(segment_data + 3);
    offset += 7;

    jbig2_error(ctx, JBIG2_SEVERITY_INFO, segment->number,
                "pattern dictionary, flags=%02x, %d grays (%dx%d cell)", flags, params.GRAYMAX + 1, params.HDPW, params.HDPH);

    if (params.HDMMR && params.HDTEMPLATE) {
        jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "HDTEMPLATE is %d when HDMMR is %d, contrary to spec", params.HDTEMPLATE, params.HDMMR);
    }
    if (flags & 0xf8) {
        jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "reserved flag bits non-zero");
    }

    if (!params.HDMMR) {

        int stats_size = jbig2_generic_stats_size(ctx, params.HDTEMPLATE);

        GB_stats = jbig2_new(ctx, Jbig2ArithCx, stats_size);
        if (GB_stats == NULL)
            return jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "failed to allocate arithmetic coding state when handling pattern dictionary");
        memset(GB_stats, 0, stats_size);
    }

    segment->result = jbig2_decode_pattern_dict(ctx, segment, &params, segment_data + offset, segment->data_length - offset, GB_stats);

    if (!params.HDMMR) {
        jbig2_free(ctx->allocator, GB_stats);
    }

    return (segment->result != NULL) ? 0 : -1;
}

static uint16_t **
jbig2_decode_gray_scale_image(Jbig2Ctx *ctx, Jbig2Segment *segment,
                              const byte *data, const size_t size,
                              bool GSMMR, uint32_t GSW, uint32_t GSH,
                              uint32_t GSBPP, bool GSUSESKIP, Jbig2Image *GSKIP, int GSTEMPLATE, Jbig2ArithCx *GB_stats)
{
    uint16_t **GSVALS = NULL;
    size_t consumed_bytes = 0;
    uint32_t i, j, stride;
    int64_t x, y;
    int code;
    Jbig2Image **GSPLANES;
    Jbig2GenericRegionParams rparams;
    Jbig2WordStream *ws = NULL;
    Jbig2ArithState *as = NULL;

    GSPLANES = jbig2_new(ctx, Jbig2Image *, GSBPP);
    if (GSPLANES == NULL) {
        jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number, "failed to allocate %d bytes for GSPLANES", GSBPP);
        return NULL;
    }

    for (i = 0; i < GSBPP; ++i) {
        GSPLANES[i] = jbig2_image_new(ctx, GSW, GSH);
        if (GSPLANES[i] == NULL) {
            jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "failed to allocate %dx%d image for GSPLANES", GSW, GSH);

            for (j = i; j > 0;)
                jbig2_image_release(ctx, GSPLANES[--j]);
            jbig2_free(ctx->allocator, GSPLANES);
            return NULL;
        }
    }

    rparams.MMR = GSMMR;
    rparams.GBTEMPLATE = GSTEMPLATE;
    rparams.TPGDON = 0;
    rparams.USESKIP = GSUSESKIP;
    rparams.SKIP = GSKIP;
    rparams.gbat[0] = (GSTEMPLATE <= 1 ? 3 : 2);
    rparams.gbat[1] = -1;
    rparams.gbat[2] = -3;
    rparams.gbat[3] = -1;
    rparams.gbat[4] = 2;
    rparams.gbat[5] = -2;
    rparams.gbat[6] = -2;
    rparams.gbat[7] = -2;

    if (GSMMR) {
        code = jbig2_decode_halftone_mmr(ctx, &rparams, data, size, GSPLANES[GSBPP - 1], &consumed_bytes);
    } else {
        ws = jbig2_word_stream_buf_new(ctx, data, size);
        if (ws == NULL) {
            jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "failed to allocate word stream when decoding gray scale image");
            goto cleanup;
        }

        as = jbig2_arith_new(ctx, ws);
        if (as == NULL) {
            jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "failed to allocate arithmetic coding state when decoding gray scale image");
            goto cleanup;
        }

        code = jbig2_decode_generic_region(ctx, segment, &rparams, as, GSPLANES[GSBPP - 1], GB_stats);
    }
    if (code < 0) {
        jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "error decoding GSPLANES for halftone image");
        goto cleanup;
    }

    j = GSBPP - 1;

    while (j > 0) {
        j--;

        if (GSMMR) {
            code = jbig2_decode_halftone_mmr(ctx, &rparams, data + consumed_bytes, size - consumed_bytes, GSPLANES[j], &consumed_bytes);
        } else {
            code = jbig2_decode_generic_region(ctx, segment, &rparams, as, GSPLANES[j], GB_stats);
        }
        if (code < 0) {
            jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "failed to decode GSPLANES for halftone image");
            goto cleanup;
        }

        stride = GSPLANES[j]->stride;
        for (i = 0; i < stride * GSH; ++i)
            GSPLANES[j]->data[i] ^= GSPLANES[j + 1]->data[i];

    }

    GSVALS = jbig2_new(ctx, uint16_t *, GSW);
    if (GSVALS == NULL) {
        jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number, "failed to allocate GSVALS: %d bytes", GSW);
        goto cleanup;
    }
    for (i = 0; i < GSW; ++i) {
        GSVALS[i] = jbig2_new(ctx, uint16_t, GSH);
        if (GSVALS[i] == NULL) {
            jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number, "failed to allocate GSVALS: %d bytes", GSH * GSW);

            for (j = i; j > 0;)
                jbig2_free(ctx->allocator, GSVALS[--j]);
            jbig2_free(ctx->allocator, GSVALS);
            GSVALS = NULL;
            goto cleanup;
        }
    }

    for (x = 0; x < GSW; ++x) {
        for (y = 0; y < GSH; ++y) {
            GSVALS[x][y] = 0;

            for (j = 0; j < GSBPP; ++j)
                GSVALS[x][y] += jbig2_image_get_pixel(GSPLANES[j], x, y) << j;
        }
    }

cleanup:

    if (!GSMMR) {
        jbig2_free(ctx->allocator, as);
        jbig2_word_stream_buf_free(ctx, ws);
    }
    for (i = 0; i < GSBPP; ++i)
        jbig2_image_release(ctx, GSPLANES[i]);

    jbig2_free(ctx->allocator, GSPLANES);

    return GSVALS;
}

static Jbig2PatternDict *
jbig2_decode_ht_region_get_hpats(Jbig2Ctx *ctx, Jbig2Segment *segment)
{
    int index = 0;
    Jbig2PatternDict *pattern_dict = NULL;
    Jbig2Segment *rsegment = NULL;

    while (!pattern_dict && segment->referred_to_segment_count > index) {
        rsegment = jbig2_find_segment(ctx, segment->referred_to_segments[index]);
        if (rsegment) {

            if ((rsegment->flags & 0x3f) == 16 && rsegment->result) {
                pattern_dict = (Jbig2PatternDict *) rsegment->result;
                return pattern_dict;
            }
        }
        index++;
    }
    return pattern_dict;
}

static int
jbig2_decode_halftone_region(Jbig2Ctx *ctx, Jbig2Segment *segment,
                             Jbig2HalftoneRegionParams *params, const byte *data, const size_t size, Jbig2Image *image, Jbig2ArithCx *GB_stats)
{
    uint32_t HBPP;
    uint32_t HNUMPATS;
    uint16_t **GI = NULL;
    Jbig2Image *HSKIP = NULL;
    Jbig2PatternDict *HPATS;
    uint32_t i;
    int64_t mg, ng;
    uint16_t gray_val;
    int code = 0;

    HPATS = jbig2_decode_ht_region_get_hpats(ctx, segment);
    if (!HPATS) {
        code = jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "no pattern dictionary found, skipping halftone image");
        goto cleanup;
    }

    memset(image->data, params->HDEFPIXEL, image->stride * image->height);

    if (params->HENABLESKIP == 1) {
        HSKIP = jbig2_image_new(ctx, params->HGW, params->HGH);
        if (HSKIP == NULL)
            return jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "failed to allocate skip image");

        for (mg = 0; mg < params->HGH; ++mg) {
            for (ng = 0; ng < params->HGW; ++ng) {
                int64_t x = ((int64_t) params->HGX + mg * params->HRY + ng * params->HRX) >> 8;
                int64_t y = ((int64_t) params->HGY + mg * params->HRX - ng * params->HRY) >> 8;

                if (x + HPATS->HPW <= 0 || x >= image->width || y + HPATS->HPH <= 0 || y >= image->height) {
                    jbig2_image_set_pixel(HSKIP, ng, mg, 1);
                } else {
                    jbig2_image_set_pixel(HSKIP, ng, mg, 0);
                }
            }
        }
    }

    HNUMPATS = HPATS->n_patterns;
    HBPP = 0;
    while (HNUMPATS > (1U << ++HBPP));
    if (HBPP > 16) {
        code = jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number, "HBPP is larger than supported (%u)", HBPP);
        goto cleanup;
    }

    GI = jbig2_decode_gray_scale_image(ctx, segment, data, size,
                                       params->HMMR, params->HGW, params->HGH, HBPP, params->HENABLESKIP, HSKIP, params->HTEMPLATE, GB_stats);
    if (!GI) {
        code = jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "unable to acquire gray-scale image, skipping halftone image");
        goto cleanup;
    }

    for (mg = 0; mg < params->HGH; ++mg) {
        for (ng = 0; ng < params->HGW; ++ng) {
            int64_t x = ((int64_t) params->HGX + mg * params->HRY + ng * params->HRX) >> 8;
            int64_t y = ((int64_t) params->HGY + mg * params->HRX - ng * params->HRY) >> 8;

            gray_val = GI[ng][mg];
            if (gray_val >= HNUMPATS) {
                jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "gray-scale index %d out of range, using largest index", gray_val);

                gray_val = HNUMPATS - 1;
            }
            code = jbig2_image_compose(ctx, image, HPATS->patterns[gray_val], x, y, params->HCOMBOP);
            if (code < 0) {
                code = jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "failed to compose pattern with gray-scale image");
                goto cleanup;
            }
        }
    }

cleanup:
    if (GI) {
        for (i = 0; i < params->HGW; ++i) {
            jbig2_free(ctx->allocator, GI[i]);
        }
    }
    jbig2_free(ctx->allocator, GI);
    jbig2_image_release(ctx, HSKIP);

    return code;
}

int
jbig2_halftone_region(Jbig2Ctx *ctx, Jbig2Segment *segment, const byte *segment_data)
{
    int offset = 0;
    Jbig2RegionSegmentInfo region_info;
    Jbig2HalftoneRegionParams params;
    Jbig2Image *image = NULL;
    Jbig2ArithCx *GB_stats = NULL;
    int code = 0;

    if (segment->data_length < 17)
        goto too_short;
    jbig2_get_region_segment_info(&region_info, segment_data);
    offset += 17;

    if (segment->data_length < 18)
        goto too_short;

    params.flags = segment_data[offset];
    params.HMMR = params.flags & 1;
    params.HTEMPLATE = (params.flags & 6) >> 1;
    params.HENABLESKIP = (params.flags & 8) >> 3;
    params.HCOMBOP = (Jbig2ComposeOp)((params.flags & 0x70) >> 4);
    params.HDEFPIXEL = (params.flags & 0x80) >> 7;
    offset += 1;

    jbig2_error(ctx, JBIG2_SEVERITY_INFO, segment->number,
                "halftone region: %u x %u @ (%u, %u), flags = %02x", region_info.width, region_info.height, region_info.x, region_info.y, params.flags);

    if (params.HMMR && params.HTEMPLATE) {
        jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "HTEMPLATE is %d when HMMR is %d, contrary to spec", params.HTEMPLATE, params.HMMR);
    }
    if (params.HMMR && params.HENABLESKIP) {
        jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "HENABLESKIP is %d when HMMR is %d, contrary to spec", params.HENABLESKIP, params.HMMR);
    }

    if (segment->data_length - offset < 16)
        goto too_short;
    params.HGW = jbig2_get_uint32(segment_data + offset);
    params.HGH = jbig2_get_uint32(segment_data + offset + 4);
    params.HGX = jbig2_get_int32(segment_data + offset + 8);
    params.HGY = jbig2_get_int32(segment_data + offset + 12);
    offset += 16;

    if (segment->data_length - offset < 4)
        goto too_short;
    params.HRX = jbig2_get_uint16(segment_data + offset);
    params.HRY = jbig2_get_uint16(segment_data + offset + 2);
    offset += 4;

    jbig2_error(ctx, JBIG2_SEVERITY_INFO, segment->number,
                "grid %d x %d @ (%d.%d,%d.%d) vector (%d.%d,%d.%d)",
                params.HGW, params.HGH,
                params.HGX >> 8, params.HGX & 0xff,
                params.HGY >> 8, params.HGY & 0xff,
                params.HRX >> 8, params.HRX & 0xff,
                params.HRY >> 8, params.HRY & 0xff);

    if (!params.HMMR) {

        int stats_size = jbig2_generic_stats_size(ctx, params.HTEMPLATE);

        GB_stats = jbig2_new(ctx, Jbig2ArithCx, stats_size);
        if (GB_stats == NULL) {
            return jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number, "failed to allocate arithmetic decoder states in halftone region");
        }
        memset(GB_stats, 0, stats_size);
    }

    image = jbig2_image_new(ctx, region_info.width, region_info.height);
    if (image == NULL) {
        jbig2_free(ctx->allocator, GB_stats);
        return jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "failed to allocate halftone image");
    }

    code = jbig2_decode_halftone_region(ctx, segment, &params, segment_data + offset, segment->data_length - offset, image, GB_stats);
    if (code < 0) {
        jbig2_image_release(ctx, image);
        jbig2_free(ctx->allocator, GB_stats);
        return jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "failed to decode halftone region");
    }

    if (!params.HMMR) {
        jbig2_free(ctx->allocator, GB_stats);
    }

    code = jbig2_page_add_result(ctx, &ctx->pages[ctx->current_page], image, region_info.x, region_info.y, region_info.op);
    if (code < 0) {
        jbig2_image_release(ctx, image);
        return jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "unable to add halftone region to page");
    }

    jbig2_image_release(ctx, image);

    return code;

too_short:
    return jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number, "segment too short");
}

#ifdef HAVE_CONFIG_H
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

Jbig2Image *
jbig2_image_new(Jbig2Ctx *ctx, uint32_t width, uint32_t height)
{
    Jbig2Image *image;
    uint32_t stride;

    if (width == 0 || height == 0) {
        jbig2_error(ctx, JBIG2_SEVERITY_FATAL, JBIG2_UNKNOWN_SEGMENT_NUMBER, "failed to create zero sized image");
        return NULL;
    }

    image = jbig2_new(ctx, Jbig2Image, 1);
    if (image == NULL) {
        jbig2_error(ctx, JBIG2_SEVERITY_FATAL, JBIG2_UNKNOWN_SEGMENT_NUMBER, "failed to allocate image");
        return NULL;
    }

    stride = ((width - 1) >> 3) + 1;

    if (height > (INT32_MAX / stride)) {
        jbig2_error(ctx, JBIG2_SEVERITY_FATAL, JBIG2_UNKNOWN_SEGMENT_NUMBER, "integer multiplication overflow (stride=%u, height=%u)", stride, height);
        jbig2_free(ctx->allocator, image);
        return NULL;
    }
    image->data = jbig2_new(ctx, uint8_t, (size_t) height * stride);
    if (image->data == NULL) {
        jbig2_error(ctx, JBIG2_SEVERITY_FATAL, JBIG2_UNKNOWN_SEGMENT_NUMBER, "failed to allocate image data buffer (stride=%u, height=%u)", stride, height);
        jbig2_free(ctx->allocator, image);
        return NULL;
    }

    image->width = width;
    image->height = height;
    image->stride = stride;
    image->refcount = 1;

    return image;
}

Jbig2Image *
jbig2_image_reference(Jbig2Ctx *ctx, Jbig2Image *image)
{
    (void) ctx;
    if (image)
        image->refcount++;
    return image;
}

void
jbig2_image_release(Jbig2Ctx *ctx, Jbig2Image *image)
{
    if (image == NULL)
        return;
    image->refcount--;
    if (image->refcount == 0)
        jbig2_image_free(ctx, image);
}

void
jbig2_image_free(Jbig2Ctx *ctx, Jbig2Image *image)
{
    if (image != NULL) {
        jbig2_free(ctx->allocator, image->data);
        jbig2_free(ctx->allocator, image);
    }
}

Jbig2Image *
jbig2_image_resize(Jbig2Ctx *ctx, Jbig2Image *image, uint32_t width, uint32_t height, int value)
{
    if (width == image->width) {
        uint8_t *data;

        if (image->height > (INT32_MAX / image->stride)) {
            jbig2_error(ctx, JBIG2_SEVERITY_FATAL, JBIG2_UNKNOWN_SEGMENT_NUMBER, "integer multiplication overflow during resize (stride=%u, height=%u)", image->stride, height);
            return NULL;
        }

        data = jbig2_renew(ctx, image->data, uint8_t, (size_t) height * image->stride);
        if (data == NULL) {
            jbig2_error(ctx, JBIG2_SEVERITY_FATAL, JBIG2_UNKNOWN_SEGMENT_NUMBER, "failed to reallocate image");
            return NULL;
        }
        image->data = data;
        if (height > image->height) {
            const uint8_t fill = value ? 0xFF : 0x00;
            memset(image->data + (size_t) image->height * image->stride, fill, ((size_t) height - image->height) * image->stride);
        }
        image->height = height;

    } else {
        Jbig2Image *newimage;
        int code;

        newimage = jbig2_image_new(ctx, width, height);
        if (newimage == NULL) {
            jbig2_error(ctx, JBIG2_SEVERITY_WARNING, JBIG2_UNKNOWN_SEGMENT_NUMBER, "failed to allocate resized image");
            return NULL;
        }
        jbig2_image_clear(ctx, newimage, value);

        code = jbig2_image_compose(ctx, newimage, image, 0, 0, JBIG2_COMPOSE_REPLACE);
        if (code < 0) {
            jbig2_error(ctx, JBIG2_SEVERITY_WARNING, JBIG2_UNKNOWN_SEGMENT_NUMBER, "failed to compose image buffers when resizing");
            jbig2_image_release(ctx, newimage);
            return NULL;
        }

        jbig2_free(ctx->allocator, image->data);
        image->width = newimage->width;
        image->height = newimage->height;
        image->stride = newimage->stride;
        image->data = newimage->data;
        jbig2_free(ctx->allocator, newimage);
    }

    return image;
}

static inline void
template_image_compose_opt(const uint8_t * JBIG2_RESTRICT ss, uint8_t * JBIG2_RESTRICT dd, int early, int late, uint8_t leftmask, uint8_t rightmask, uint32_t bytewidth_, uint32_t h, uint32_t shift, uint32_t dstride, uint32_t sstride, Jbig2ComposeOp op)
{
    int i;
    uint32_t j;
    int bytewidth = (int)bytewidth_;

    if (bytewidth == 1) {
        for (j = 0; j < h; j++) {

            uint8_t v = (((early ? 0 : ss[0]<<8) | (late ? 0 : ss[1]))>>shift);
            if (op == JBIG2_COMPOSE_OR)
                *dd |= v & leftmask;
            else if (op == JBIG2_COMPOSE_AND)
                *dd &= (v & leftmask) | ~leftmask;
            else if (op == JBIG2_COMPOSE_XOR)
                *dd ^= v & leftmask;
            else if (op == JBIG2_COMPOSE_XNOR)
                *dd ^= (~v) & leftmask;
            else
                *dd = (v & leftmask) | (*dd & ~leftmask);
            dd += dstride;
            ss += sstride;
        }
        return;
    }
    bytewidth -= 2;
    if (shift == 0) {
        ss++;
        for (j = 0; j < h; j++) {

            const uint8_t * JBIG2_RESTRICT s = ss;
            uint8_t * JBIG2_RESTRICT d = dd;
            if (op == JBIG2_COMPOSE_OR)
                *d++ |= *s++ & leftmask;
            else if (op == JBIG2_COMPOSE_AND)
                *d++ &= (*s++ & leftmask) | ~leftmask;
            else if (op == JBIG2_COMPOSE_XOR)
                *d++ ^= *s++ & leftmask;
            else if (op == JBIG2_COMPOSE_XNOR)
                *d++ ^= (~*s++) & leftmask;
            else
                *d = (*s++ & leftmask) | (*d & ~leftmask), d++;

            for (i = bytewidth; i != 0; i--) {
                if (op == JBIG2_COMPOSE_OR)
                    *d++ |= *s++;
                else if (op == JBIG2_COMPOSE_AND)
                    *d++ &= *s++;
                else if (op == JBIG2_COMPOSE_XOR)
                    *d++ ^= *s++;
                else if (op == JBIG2_COMPOSE_XNOR)
                    *d++ ^= ~*s++;
                else
                    *d++ = *s++;
            }

            if (op == JBIG2_COMPOSE_OR)
                *d |= *s & rightmask;
            else if (op == JBIG2_COMPOSE_AND)
                *d &= (*s & rightmask) | ~rightmask;
            else if (op == JBIG2_COMPOSE_XOR)
                *d ^= *s & rightmask;
            else if (op == JBIG2_COMPOSE_XNOR)
                *d ^= (~*s) & rightmask;
            else
                *d = (*s & rightmask) | (*d & ~rightmask);
            dd += dstride;
            ss += sstride;
        }
    } else {
        for (j = 0; j < h; j++) {

            const uint8_t * JBIG2_RESTRICT s = ss;
            uint8_t * JBIG2_RESTRICT d = dd;
            uint8_t s0, s1, v;
            s0 = early ? 0 : *s;
            s++;
            s1 = *s++;
            v = ((s0<<8) | s1)>>shift;
            if (op == JBIG2_COMPOSE_OR)
                *d++ |= v & leftmask;
            else if (op == JBIG2_COMPOSE_AND)
                *d++ &= (v & leftmask) | ~leftmask;
            else if (op == JBIG2_COMPOSE_XOR)
                *d++ ^= v & leftmask;
            else if (op == JBIG2_COMPOSE_XNOR)
                *d++ ^= (~v) & leftmask;
            else
                *d = (v & leftmask) | (*d & ~leftmask), d++;

            for (i = bytewidth; i > 0; i--) {
                s0 = s1; s1 = *s++;
                v = ((s0<<8) | s1)>>shift;
                if (op == JBIG2_COMPOSE_OR)
                    *d++ |= v;
                else if (op == JBIG2_COMPOSE_AND)
                    *d++ &= v;
                else if (op == JBIG2_COMPOSE_XOR)
                    *d++ ^= v;
                else if (op == JBIG2_COMPOSE_XNOR)
                    *d++ ^= ~v;
                else
                    *d++ = v;
            }

            s0 = s1; s1 = (late ? 0 : *s);
            v = (((s0<<8) | s1)>>shift);
            if (op == JBIG2_COMPOSE_OR)
                *d |= v & rightmask;
            else if (op == JBIG2_COMPOSE_AND)
                *d &= (v & rightmask) | ~rightmask;
            else if (op == JBIG2_COMPOSE_XOR)
                *d ^= v & rightmask;
            else if (op == JBIG2_COMPOSE_XNOR)
                *d ^= ~v & rightmask;
            else
                *d = (v & rightmask) | (*d & ~rightmask);
            dd += dstride;
            ss += sstride;
        }
    }
}

static void
jbig2_image_compose_opt_OR(const uint8_t *s, uint8_t *d, int early, int late, uint8_t mask, uint8_t rightmask, uint32_t bytewidth, uint32_t h, uint32_t shift, uint32_t dstride, uint32_t sstride)
{
    if (early || late)
        template_image_compose_opt(s, d, early, late, mask, rightmask, bytewidth, h, shift, dstride, sstride, JBIG2_COMPOSE_OR);
    else
        template_image_compose_opt(s, d, 0, 0, mask, rightmask, bytewidth, h, shift, dstride, sstride, JBIG2_COMPOSE_OR);
}

static void
jbig2_image_compose_opt_AND(const uint8_t *s, uint8_t *d, int early, int late, uint8_t mask, uint8_t rightmask, uint32_t bytewidth, uint32_t h, uint32_t shift, uint32_t dstride, uint32_t sstride)
{
    if (early || late)
        template_image_compose_opt(s, d, early, late, mask, rightmask, bytewidth, h, shift, dstride, sstride, JBIG2_COMPOSE_AND);
    else
        template_image_compose_opt(s, d, 0, 0, mask, rightmask, bytewidth, h, shift, dstride, sstride, JBIG2_COMPOSE_AND);
}

static void
jbig2_image_compose_opt_XOR(const uint8_t *s, uint8_t *d, int early, int late, uint8_t mask, uint8_t rightmask, uint32_t bytewidth, uint32_t h, uint32_t shift, uint32_t dstride, uint32_t sstride)
{
    if (early || late)
        template_image_compose_opt(s, d, early, late, mask, rightmask, bytewidth, h, shift, dstride, sstride, JBIG2_COMPOSE_XOR);
    else
        template_image_compose_opt(s, d, 0, 0, mask, rightmask, bytewidth, h, shift, dstride, sstride, JBIG2_COMPOSE_XOR);
}

static void
jbig2_image_compose_opt_XNOR(const uint8_t *s, uint8_t *d, int early, int late, uint8_t mask, uint8_t rightmask, uint32_t bytewidth, uint32_t h, uint32_t shift, uint32_t dstride, uint32_t sstride)
{
    if (early || late)
        template_image_compose_opt(s, d, early, late, mask, rightmask, bytewidth, h, shift, dstride, sstride, JBIG2_COMPOSE_XNOR);
    else
        template_image_compose_opt(s, d, 0, 0, mask, rightmask, bytewidth, h, shift, dstride, sstride, JBIG2_COMPOSE_XNOR);
}

static void
jbig2_image_compose_opt_REPLACE(const uint8_t *s, uint8_t *d, int early, int late, uint8_t mask, uint8_t rightmask, uint32_t bytewidth, uint32_t h, uint32_t shift, uint32_t dstride, uint32_t sstride)
{
    if (early || late)
        template_image_compose_opt(s, d, early, late, mask, rightmask, bytewidth, h, shift, dstride, sstride, JBIG2_COMPOSE_REPLACE);
    else
        template_image_compose_opt(s, d, 0, 0, mask, rightmask, bytewidth, h, shift, dstride, sstride, JBIG2_COMPOSE_REPLACE);
}

int
jbig2_image_compose(Jbig2Ctx *ctx, Jbig2Image *dst, Jbig2Image *src, int64_t x, int64_t y, Jbig2ComposeOp op)
{
    uint32_t w, h;
    uint32_t shift;
    uint32_t leftbyte;
    uint8_t *ss;
    uint8_t *dd;
    uint8_t leftmask, rightmask;
    int early = x >= 0;
    int late;
    uint32_t bytewidth;
    uint32_t syoffset = 0;

    (void) ctx;

    if (src == NULL)
        return 0;

    if (
            (x <= -((int64_t) src->width)) || (x >= (int64_t) dst->width) ||
            (y <= -((int64_t) src->height)) || (y >= (int64_t) dst->height))
    {
        jbig2_error(ctx, JBIG2_SEVERITY_DEBUG, JBIG2_UNKNOWN_SEGMENT_NUMBER, "src image entirely outside dst image in compose_image");
        return 0;
    }

    w = src->width;
    h = src->height;
    shift = (uint32_t) (x & 7);
    ss = src->data - early;

    if (x < 0) {
        uint32_t negx = (uint32_t) (-x);
        w -= negx;
        ss += (negx-1)>>3;
        x = 0;
    }
    if (y < 0) {
        uint32_t negy = (uint32_t) (-y);
        h -= negy;
        syoffset = negy * src->stride;
        y = 0;
    }

    if ((uint32_t)x + w > dst->width)
        w = dst->width - ((uint32_t) x);
    if ((uint32_t)y + h > dst->height)
        h = dst->height - ((uint32_t) y);
#ifdef JBIG2_DEBUG
    jbig2_error(ctx, JBIG2_SEVERITY_DEBUG, JBIG2_UNKNOWN_SEGMENT_NUMBER, "compositing %ux%u at (%u, %u) after clipping",
        w, h, (uint32_t) x, (uint32_t) y);
#endif

    if ((w <= 0) || (h <= 0)) {
#ifdef JBIG2_DEBUG
        jbig2_error(ctx, JBIG2_SEVERITY_DEBUG, JBIG2_UNKNOWN_SEGMENT_NUMBER, "zero clipping region");
#endif
        return 0;
    }

    leftbyte = (uint32_t) x >> 3;
    dd = dst->data + y * dst->stride + leftbyte;
    bytewidth = (((uint32_t) x + w - 1) >> 3) - leftbyte + 1;
    leftmask = 255>>(x&7);
    rightmask = (((x+w)&7) == 0) ? 255 : ~(255>>((x+w)&7));
    if (bytewidth == 1)
        leftmask &= rightmask;
    late = (ss + bytewidth >= src->data + ((src->width+7)>>3));
    ss += syoffset;

    switch(op)
    {
    case JBIG2_COMPOSE_OR:
        jbig2_image_compose_opt_OR(ss, dd, early, late, leftmask, rightmask, bytewidth, h, shift, dst->stride, src->stride);
        break;
    case JBIG2_COMPOSE_AND:
        jbig2_image_compose_opt_AND(ss, dd, early, late, leftmask, rightmask, bytewidth, h, shift, dst->stride, src->stride);
        break;
    case JBIG2_COMPOSE_XOR:
        jbig2_image_compose_opt_XOR(ss, dd, early, late, leftmask, rightmask, bytewidth, h, shift, dst->stride, src->stride);
        break;
    case JBIG2_COMPOSE_XNOR:
        jbig2_image_compose_opt_XNOR(ss, dd, early, late, leftmask, rightmask, bytewidth, h, shift, dst->stride, src->stride);
        break;
    case JBIG2_COMPOSE_REPLACE:
        jbig2_image_compose_opt_REPLACE(ss, dd, early, late, leftmask, rightmask, bytewidth, h, shift, dst->stride, src->stride);
        break;
    }

    return 0;
}

void
jbig2_image_clear(Jbig2Ctx *ctx, Jbig2Image *image, int value)
{
    const uint8_t fill = value ? 0xFF : 0x00;

    (void) ctx;

    memset(image->data, fill, image->stride * image->height);
}

int
jbig2_image_get_pixel(Jbig2Image *image, int64_t x, int64_t y)
{
    const int64_t w = image->width;
    const int64_t h = image->height;
    size_t sx, sy;
    size_t byte;
    int bit;

    if ((x < 0) || (x >= w))
        return 0;
    if ((y < 0) || (y >= h))
        return 0;

    sx = (size_t) x;
    sy = (size_t) y;

    byte = (sx >> 3) + sy * image->stride;
    bit = 7 - ((int) (sx & 7));

    return ((image->data[byte] >> bit) & 1);
}

void
jbig2_image_set_pixel(Jbig2Image *image, int64_t x, int64_t y, bool value)
{
    const int64_t w = image->width;
    const int64_t h = image->height;
    uint8_t scratch, mask;
    size_t sx, sy;
    size_t byte;
    int bit;

    if ((x < 0) || (x >= w))
        return;
    if ((y < 0) || (y >= h))
        return;

    sx = (size_t) x;
    sy = (size_t) y;

    byte = (sx >> 3) + sy * image->stride;
    bit = 7 - ((int) (sx & 7));
    mask = (uint8_t) ((1 << bit) ^ 0xff);

    scratch = image->data[byte] & mask;
    image->data[byte] = scratch | (value << bit);
}

#ifdef HAVE_CONFIG_H
#endif

#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    uint32_t width;
    uint32_t height;
    const byte *data;
    size_t size;
    size_t consumed_bits;
    uint32_t data_index;
    uint32_t bit_index;
    uint32_t word;
} Jbig2MmrCtx;

#define MINUS1 UINT32_MAX
#define ERROR -1
#define ZEROES -2
#define UNCOMPRESSED -3

static void
jbig2_decode_mmr_init(Jbig2MmrCtx *mmr, int width, int height, const byte *data, size_t size)
{
    mmr->width = width;
    mmr->height = height;
    mmr->data = data;
    mmr->size = size;
    mmr->data_index = 0;
    mmr->bit_index = 32;
    mmr->word = 0;
    mmr->consumed_bits = 0;

    while (mmr->bit_index >= 8 && mmr->data_index < mmr->size) {
        mmr->bit_index -= 8;
        mmr->word |= (mmr->data[mmr->data_index] << mmr->bit_index);
        mmr->data_index++;
    }
}

static void
jbig2_decode_mmr_consume(Jbig2MmrCtx *mmr, int n_bits)
{
    mmr->consumed_bits += n_bits;
    if (mmr->consumed_bits > mmr->size * 8)
        mmr->consumed_bits = mmr->size * 8;

    mmr->word <<= n_bits;
    mmr->bit_index += n_bits;
    while (mmr->bit_index >= 8 && mmr->data_index < mmr->size) {
        mmr->bit_index -= 8;
        mmr->word |= (mmr->data[mmr->data_index] << mmr->bit_index);
        mmr->data_index++;
    }
}

typedef struct {
    short val;
    short n_bits;
} mmr_table_node;

const mmr_table_node jbig2_mmr_white_decode[] = {
    {256, 12},
    {272, 12},
    {29, 8},
    {30, 8},
    {45, 8},
    {46, 8},
    {22, 7},
    {22, 7},
    {23, 7},
    {23, 7},
    {47, 8},
    {48, 8},
    {13, 6},
    {13, 6},
    {13, 6},
    {13, 6},
    {20, 7},
    {20, 7},
    {33, 8},
    {34, 8},
    {35, 8},
    {36, 8},
    {37, 8},
    {38, 8},
    {19, 7},
    {19, 7},
    {31, 8},
    {32, 8},
    {1, 6},
    {1, 6},
    {1, 6},
    {1, 6},
    {12, 6},
    {12, 6},
    {12, 6},
    {12, 6},
    {53, 8},
    {54, 8},
    {26, 7},
    {26, 7},
    {39, 8},
    {40, 8},
    {41, 8},
    {42, 8},
    {43, 8},
    {44, 8},
    {21, 7},
    {21, 7},
    {28, 7},
    {28, 7},
    {61, 8},
    {62, 8},
    {63, 8},
    {0, 8},
    {320, 8},
    {384, 8},
    {10, 5},
    {10, 5},
    {10, 5},
    {10, 5},
    {10, 5},
    {10, 5},
    {10, 5},
    {10, 5},
    {11, 5},
    {11, 5},
    {11, 5},
    {11, 5},
    {11, 5},
    {11, 5},
    {11, 5},
    {11, 5},
    {27, 7},
    {27, 7},
    {59, 8},
    {60, 8},
    {288, 9},
    {290, 9},
    {18, 7},
    {18, 7},
    {24, 7},
    {24, 7},
    {49, 8},
    {50, 8},
    {51, 8},
    {52, 8},
    {25, 7},
    {25, 7},
    {55, 8},
    {56, 8},
    {57, 8},
    {58, 8},
    {192, 6},
    {192, 6},
    {192, 6},
    {192, 6},
    {1664, 6},
    {1664, 6},
    {1664, 6},
    {1664, 6},
    {448, 8},
    {512, 8},
    {292, 9},
    {640, 8},
    {576, 8},
    {294, 9},
    {296, 9},
    {298, 9},
    {300, 9},
    {302, 9},
    {256, 7},
    {256, 7},
    {2, 4},
    {2, 4},
    {2, 4},
    {2, 4},
    {2, 4},
    {2, 4},
    {2, 4},
    {2, 4},
    {2, 4},
    {2, 4},
    {2, 4},
    {2, 4},
    {2, 4},
    {2, 4},
    {2, 4},
    {2, 4},
    {3, 4},
    {3, 4},
    {3, 4},
    {3, 4},
    {3, 4},
    {3, 4},
    {3, 4},
    {3, 4},
    {3, 4},
    {3, 4},
    {3, 4},
    {3, 4},
    {3, 4},
    {3, 4},
    {3, 4},
    {3, 4},
    {128, 5},
    {128, 5},
    {128, 5},
    {128, 5},
    {128, 5},
    {128, 5},
    {128, 5},
    {128, 5},
    {8, 5},
    {8, 5},
    {8, 5},
    {8, 5},
    {8, 5},
    {8, 5},
    {8, 5},
    {8, 5},
    {9, 5},
    {9, 5},
    {9, 5},
    {9, 5},
    {9, 5},
    {9, 5},
    {9, 5},
    {9, 5},
    {16, 6},
    {16, 6},
    {16, 6},
    {16, 6},
    {17, 6},
    {17, 6},
    {17, 6},
    {17, 6},
    {4, 4},
    {4, 4},
    {4, 4},
    {4, 4},
    {4, 4},
    {4, 4},
    {4, 4},
    {4, 4},
    {4, 4},
    {4, 4},
    {4, 4},
    {4, 4},
    {4, 4},
    {4, 4},
    {4, 4},
    {4, 4},
    {5, 4},
    {5, 4},
    {5, 4},
    {5, 4},
    {5, 4},
    {5, 4},
    {5, 4},
    {5, 4},
    {5, 4},
    {5, 4},
    {5, 4},
    {5, 4},
    {5, 4},
    {5, 4},
    {5, 4},
    {5, 4},
    {14, 6},
    {14, 6},
    {14, 6},
    {14, 6},
    {15, 6},
    {15, 6},
    {15, 6},
    {15, 6},
    {64, 5},
    {64, 5},
    {64, 5},
    {64, 5},
    {64, 5},
    {64, 5},
    {64, 5},
    {64, 5},
    {6, 4},
    {6, 4},
    {6, 4},
    {6, 4},
    {6, 4},
    {6, 4},
    {6, 4},
    {6, 4},
    {6, 4},
    {6, 4},
    {6, 4},
    {6, 4},
    {6, 4},
    {6, 4},
    {6, 4},
    {6, 4},
    {7, 4},
    {7, 4},
    {7, 4},
    {7, 4},
    {7, 4},
    {7, 4},
    {7, 4},
    {7, 4},
    {7, 4},
    {7, 4},
    {7, 4},
    {7, 4},
    {7, 4},
    {7, 4},
    {7, 4},
    {7, 4},
    {-2, 3},
    {-2, 3},
    {-1, 0},
    {-1, 0},
    {-1, 0},
    {-1, 0},
    {-1, 0},
    {-1, 0},
    {-1, 0},
    {-1, 0},
    {-1, 0},
    {-1, 0},
    {-1, 0},
    {-1, 0},
    {-1, 0},
    {-3, 4},
    {1792, 3},
    {1792, 3},
    {1984, 4},
    {2048, 4},
    {2112, 4},
    {2176, 4},
    {2240, 4},
    {2304, 4},
    {1856, 3},
    {1856, 3},
    {1920, 3},
    {1920, 3},
    {2368, 4},
    {2432, 4},
    {2496, 4},
    {2560, 4},
    {1472, 1},
    {1536, 1},
    {1600, 1},
    {1728, 1},
    {704, 1},
    {768, 1},
    {832, 1},
    {896, 1},
    {960, 1},
    {1024, 1},
    {1088, 1},
    {1152, 1},
    {1216, 1},
    {1280, 1},
    {1344, 1},
    {1408, 1}
};

const mmr_table_node jbig2_mmr_black_decode[] = {
    {128, 12},
    {160, 13},
    {224, 12},
    {256, 12},
    {10, 7},
    {11, 7},
    {288, 12},
    {12, 7},
    {9, 6},
    {9, 6},
    {8, 6},
    {8, 6},
    {7, 5},
    {7, 5},
    {7, 5},
    {7, 5},
    {6, 4},
    {6, 4},
    {6, 4},
    {6, 4},
    {6, 4},
    {6, 4},
    {6, 4},
    {6, 4},
    {5, 4},
    {5, 4},
    {5, 4},
    {5, 4},
    {5, 4},
    {5, 4},
    {5, 4},
    {5, 4},
    {1, 3},
    {1, 3},
    {1, 3},
    {1, 3},
    {1, 3},
    {1, 3},
    {1, 3},
    {1, 3},
    {1, 3},
    {1, 3},
    {1, 3},
    {1, 3},
    {1, 3},
    {1, 3},
    {1, 3},
    {1, 3},
    {4, 3},
    {4, 3},
    {4, 3},
    {4, 3},
    {4, 3},
    {4, 3},
    {4, 3},
    {4, 3},
    {4, 3},
    {4, 3},
    {4, 3},
    {4, 3},
    {4, 3},
    {4, 3},
    {4, 3},
    {4, 3},
    {3, 2},
    {3, 2},
    {3, 2},
    {3, 2},
    {3, 2},
    {3, 2},
    {3, 2},
    {3, 2},
    {3, 2},
    {3, 2},
    {3, 2},
    {3, 2},
    {3, 2},
    {3, 2},
    {3, 2},
    {3, 2},
    {3, 2},
    {3, 2},
    {3, 2},
    {3, 2},
    {3, 2},
    {3, 2},
    {3, 2},
    {3, 2},
    {3, 2},
    {3, 2},
    {3, 2},
    {3, 2},
    {3, 2},
    {3, 2},
    {3, 2},
    {3, 2},
    {2, 2},
    {2, 2},
    {2, 2},
    {2, 2},
    {2, 2},
    {2, 2},
    {2, 2},
    {2, 2},
    {2, 2},
    {2, 2},
    {2, 2},
    {2, 2},
    {2, 2},
    {2, 2},
    {2, 2},
    {2, 2},
    {2, 2},
    {2, 2},
    {2, 2},
    {2, 2},
    {2, 2},
    {2, 2},
    {2, 2},
    {2, 2},
    {2, 2},
    {2, 2},
    {2, 2},
    {2, 2},
    {2, 2},
    {2, 2},
    {2, 2},
    {2, 2},
    {-2, 4},
    {-2, 4},
    {-1, 0},
    {-1, 0},
    {-1, 0},
    {-1, 0},
    {-1, 0},
    {-1, 0},
    {-1, 0},
    {-1, 0},
    {-1, 0},
    {-1, 0},
    {-1, 0},
    {-1, 0},
    {-1, 0},
    {-3, 5},
    {1792, 4},
    {1792, 4},
    {1984, 5},
    {2048, 5},
    {2112, 5},
    {2176, 5},
    {2240, 5},
    {2304, 5},
    {1856, 4},
    {1856, 4},
    {1920, 4},
    {1920, 4},
    {2368, 5},
    {2432, 5},
    {2496, 5},
    {2560, 5},
    {18, 3},
    {18, 3},
    {18, 3},
    {18, 3},
    {18, 3},
    {18, 3},
    {18, 3},
    {18, 3},
    {52, 5},
    {52, 5},
    {640, 6},
    {704, 6},
    {768, 6},
    {832, 6},
    {55, 5},
    {55, 5},
    {56, 5},
    {56, 5},
    {1280, 6},
    {1344, 6},
    {1408, 6},
    {1472, 6},
    {59, 5},
    {59, 5},
    {60, 5},
    {60, 5},
    {1536, 6},
    {1600, 6},
    {24, 4},
    {24, 4},
    {24, 4},
    {24, 4},
    {25, 4},
    {25, 4},
    {25, 4},
    {25, 4},
    {1664, 6},
    {1728, 6},
    {320, 5},
    {320, 5},
    {384, 5},
    {384, 5},
    {448, 5},
    {448, 5},
    {512, 6},
    {576, 6},
    {53, 5},
    {53, 5},
    {54, 5},
    {54, 5},
    {896, 6},
    {960, 6},
    {1024, 6},
    {1088, 6},
    {1152, 6},
    {1216, 6},
    {64, 3},
    {64, 3},
    {64, 3},
    {64, 3},
    {64, 3},
    {64, 3},
    {64, 3},
    {64, 3},
    {13, 1},
    {13, 1},
    {13, 1},
    {13, 1},
    {13, 1},
    {13, 1},
    {13, 1},
    {13, 1},
    {13, 1},
    {13, 1},
    {13, 1},
    {13, 1},
    {13, 1},
    {13, 1},
    {13, 1},
    {13, 1},
    {23, 4},
    {23, 4},
    {50, 5},
    {51, 5},
    {44, 5},
    {45, 5},
    {46, 5},
    {47, 5},
    {57, 5},
    {58, 5},
    {61, 5},
    {256, 5},
    {16, 3},
    {16, 3},
    {16, 3},
    {16, 3},
    {17, 3},
    {17, 3},
    {17, 3},
    {17, 3},
    {48, 5},
    {49, 5},
    {62, 5},
    {63, 5},
    {30, 5},
    {31, 5},
    {32, 5},
    {33, 5},
    {40, 5},
    {41, 5},
    {22, 4},
    {22, 4},
    {14, 1},
    {14, 1},
    {14, 1},
    {14, 1},
    {14, 1},
    {14, 1},
    {14, 1},
    {14, 1},
    {14, 1},
    {14, 1},
    {14, 1},
    {14, 1},
    {14, 1},
    {14, 1},
    {14, 1},
    {14, 1},
    {15, 2},
    {15, 2},
    {15, 2},
    {15, 2},
    {15, 2},
    {15, 2},
    {15, 2},
    {15, 2},
    {128, 5},
    {192, 5},
    {26, 5},
    {27, 5},
    {28, 5},
    {29, 5},
    {19, 4},
    {19, 4},
    {20, 4},
    {20, 4},
    {34, 5},
    {35, 5},
    {36, 5},
    {37, 5},
    {38, 5},
    {39, 5},
    {21, 4},
    {21, 4},
    {42, 5},
    {43, 5},
    {0, 3},
    {0, 3},
    {0, 3},
    {0, 3}
};

#define getbit(buf, x) ( ( buf[x >> 3] >> ( 7 - (x & 7) ) ) & 1 )

#define getword16(b)  ((uint16_t)(b[0] | (b[1] << 8)))
#define getword32(b)  ((uint32_t)(getword16(b) | (getword16((b + 2)) << 16)))

static uint32_t
jbig2_find_changing_element(const byte *line, uint32_t x, uint32_t w)
{
    int a;
    uint8_t     all8;
    uint16_t    all16;
    uint32_t    all32;

    if (line == NULL)
        return w;

    if (x == MINUS1) {
        a = 0;
        x = 0;
    } else if (x < w) {
        a = getbit(line, x);
        x++;
    } else {
        return x;
    }

    all8  = (a) ? 0xff : 0;
    all16 = (a) ? 0xffff : 0;
    all32 = (a) ? 0xffffffff : 0;

    if ( ((uint8_t*) line)[ x / 8] == all8) {

        x = x / 8 * 8 + 8;
        if (x >= w) {
            x = w;
            goto end;
        }
    } else {
        for(;;) {
            if (x == w) {
                goto end;
            }
            if (x % 8 == 0) {
                break;
            }
            if (getbit(line, x) != a) {
                goto end;
            }
            x += 1;
        }
    }

    assert(x % 8 == 0);

    if (x % 16) {
        if (w - x < 8) {
            goto check1;
        }
        if ( ((uint8_t*) line)[ x / 8] != all8) {
            goto check1;
        }
        x += 8;
    }

    assert(x % 16 == 0);

    if (x % 32) {
        if (w - x < 16) {
            goto check8;
        }
        if ( getword16((line + (x / 8))) != all16) {
            goto check8_no_eof;
        }
        x += 16;
    }

    assert(x % 32 == 0);
    for(;;) {
        if (w - x < 32) {

            goto check16;
        }
        if ( getword32((line + (x / 8))) != all32) {
            goto check16_no_eof;
        }
        x += 32;
    }

check16:
    assert(x % 16 == 0);
    if (w - x < 16) {
        goto check8;
    }
check16_no_eof:
    assert(w - x >= 16);
    if ( getword16((line + (x / 8))) != all16) {
        goto check8_no_eof;
    }
    x += 16;

check8:
    assert(x % 8 == 0);
    if (w - x < 8) {
        goto check1;
    }
check8_no_eof:
    assert(w - x >= 8);
    if ( ((uint8_t*) line)[x/8] != all8) {
        goto check1;
    }
    x += 8;

check1:
    assert(x % 8 == 0);
    if ( ((uint8_t*) line)[ x / 8] == all8) {
        x = w;
        goto end;
    }
    {
        for(;;) {
            if (x == w) {
                goto end;
            }
            if (getbit(line, x) != a) {
                goto end;
            }
            x += 1;
        }
    }

end:
    return x;
}

#undef getword16
#undef getword32

static uint32_t
jbig2_find_changing_element_of_color(const byte *line, uint32_t x, uint32_t w, int color)
{
    if (line == NULL)
        return w;
    x = jbig2_find_changing_element(line, x, w);
    if (x < w && getbit(line, x) != color)
        x = jbig2_find_changing_element(line, x, w);
    return x;
}

static const byte lm[8] = { 0xFF, 0x7F, 0x3F, 0x1F, 0x0F, 0x07, 0x03, 0x01 };
static const byte rm[8] = { 0x00, 0x80, 0xC0, 0xE0, 0xF0, 0xF8, 0xFC, 0xFE };

static void
jbig2_set_bits(byte *line, uint32_t x0, uint32_t x1)
{
    uint32_t a0, a1, b0, b1, a;

    a0 = x0 >> 3;
    a1 = x1 >> 3;

    b0 = x0 & 7;
    b1 = x1 & 7;

    if (a0 == a1) {
        line[a0] |= lm[b0] & rm[b1];
    } else {
        line[a0] |= lm[b0];
        for (a = a0 + 1; a < a1; a++)
            line[a] = 0xFF;
        if (b1)
            line[a1] |= rm[b1];
    }
}

static int
jbig2_decode_get_code(Jbig2MmrCtx *mmr, const mmr_table_node *table, int initial_bits)
{
    uint32_t word = mmr->word;
    int table_ix = word >> (32 - initial_bits);
    int val = table[table_ix].val;
    int n_bits = table[table_ix].n_bits;

    if (n_bits > initial_bits) {
        int mask = (1 << (32 - initial_bits)) - 1;

        table_ix = val + ((word & mask) >> (32 - n_bits));
        val = table[table_ix].val;
        n_bits = initial_bits + table[table_ix].n_bits;
    }

    jbig2_decode_mmr_consume(mmr, n_bits);

    return val;
}

static int
jbig2_decode_get_run(Jbig2Ctx *ctx, Jbig2MmrCtx *mmr, const mmr_table_node *table, int initial_bits)
{
    int result = 0;
    int val;

    do {
        val = jbig2_decode_get_code(mmr, table, initial_bits);
        if (val == ERROR)
            return jbig2_error(ctx, JBIG2_SEVERITY_FATAL, JBIG2_UNKNOWN_SEGMENT_NUMBER, "invalid code detected in MMR-coded data");
        else if (val == UNCOMPRESSED)
            return jbig2_error(ctx, JBIG2_SEVERITY_FATAL, JBIG2_UNKNOWN_SEGMENT_NUMBER, "uncompressed code in MMR-coded data");
        else if (val == ZEROES)
            return jbig2_error(ctx, JBIG2_SEVERITY_FATAL, JBIG2_UNKNOWN_SEGMENT_NUMBER, "zeroes code in MMR-coded data");
        result += val;
    } while (val >= 64);

    return result;
}

static int
jbig2_decode_mmr_line(Jbig2Ctx *ctx, Jbig2MmrCtx *mmr, const byte *ref, byte *dst, int *eofb)
{
    uint32_t a0 = MINUS1;
    uint32_t a1, a2, b1, b2;
    int c = 0;

    while (1) {
        uint32_t word = mmr->word;

        if (a0 != MINUS1 && a0 >= mmr->width)
            break;

        if ((word >> (32 - 3)) == 1) {
            int white_run, black_run;

            jbig2_decode_mmr_consume(mmr, 3);

            if (a0 == MINUS1)
                a0 = 0;

            if (c == 0) {
                white_run = jbig2_decode_get_run(ctx, mmr, jbig2_mmr_white_decode, 8);
                if (white_run < 0)
                    return jbig2_error(ctx, JBIG2_SEVERITY_WARNING, JBIG2_UNKNOWN_SEGMENT_NUMBER, "failed to decode white H run");
                black_run = jbig2_decode_get_run(ctx, mmr, jbig2_mmr_black_decode, 7);
                if (black_run < 0)
                    return jbig2_error(ctx, JBIG2_SEVERITY_WARNING, JBIG2_UNKNOWN_SEGMENT_NUMBER, "failed to decode black H run");

                a1 = a0 + white_run;
                a2 = a1 + black_run;
                if (a1 > mmr->width)
                    a1 = mmr->width;
                if (a2 > mmr->width)
                    a2 = mmr->width;
                if (a2 < a1) {
                    jbig2_error(ctx, JBIG2_SEVERITY_WARNING, JBIG2_UNKNOWN_SEGMENT_NUMBER, "ignoring negative black H run");
                    a2 = a1;
                }
                if (a1 < mmr->width)
                    jbig2_set_bits(dst, a1, a2);
                a0 = a2;
            } else {
                black_run = jbig2_decode_get_run(ctx, mmr, jbig2_mmr_black_decode, 7);
                if (black_run < 0)
                    return jbig2_error(ctx, JBIG2_SEVERITY_WARNING, JBIG2_UNKNOWN_SEGMENT_NUMBER, "failed to decode black H run");
                white_run = jbig2_decode_get_run(ctx, mmr, jbig2_mmr_white_decode, 8);
                if (white_run < 0)
                    return jbig2_error(ctx, JBIG2_SEVERITY_WARNING, JBIG2_UNKNOWN_SEGMENT_NUMBER, "failed to decode white H run");

                a1 = a0 + black_run;
                a2 = a1 + white_run;
                if (a1 > mmr->width)
                    a1 = mmr->width;
                if (a2 > mmr->width)
                    a2 = mmr->width;
                if (a1 < a0) {
                    jbig2_error(ctx, JBIG2_SEVERITY_WARNING, JBIG2_UNKNOWN_SEGMENT_NUMBER, "ignoring negative white H run");
                    a1 = a0;
                }
                if (a0 < mmr->width)
                    jbig2_set_bits(dst, a0, a1);
                a0 = a2;
            }
        }

        else if ((word >> (32 - 4)) == 1) {

            jbig2_decode_mmr_consume(mmr, 4);
            b1 = jbig2_find_changing_element_of_color(ref, a0, mmr->width, !c);
            b2 = jbig2_find_changing_element(ref, b1, mmr->width);
            if (c) {
                if (b2 < a0) {
                    jbig2_error(ctx, JBIG2_SEVERITY_WARNING, JBIG2_UNKNOWN_SEGMENT_NUMBER, "ignoring negative P run");
                    b2 = a0;
                }
                if (a0 < mmr->width)
                    jbig2_set_bits(dst, a0, b2);
            }
            a0 = b2;
        }

        else if ((word >> (32 - 1)) == 1) {

            jbig2_decode_mmr_consume(mmr, 1);
            b1 = jbig2_find_changing_element_of_color(ref, a0, mmr->width, !c);
            if (c) {
                if (b1 < a0) {
                    jbig2_error(ctx, JBIG2_SEVERITY_WARNING, JBIG2_UNKNOWN_SEGMENT_NUMBER, "ignoring negative V(0) run");
                    b1 = a0;
                }
                if (a0 < mmr->width)
                    jbig2_set_bits(dst, a0, b1);
            }
            a0 = b1;
            c = !c;
        }

        else if ((word >> (32 - 3)) == 3) {

            jbig2_decode_mmr_consume(mmr, 3);
            b1 = jbig2_find_changing_element_of_color(ref, a0, mmr->width, !c);
            if (b1 + 1 <= mmr->width)
                b1 += 1;
            if (c) {
                if (b1 < a0) {
                    jbig2_error(ctx, JBIG2_SEVERITY_WARNING, JBIG2_UNKNOWN_SEGMENT_NUMBER, "ignoring negative VR(1) run");
                    b1 = a0;
                }
                if (a0 < mmr->width)
                    jbig2_set_bits(dst, a0, b1);
            }
            a0 = b1;
            c = !c;
        }

        else if ((word >> (32 - 6)) == 3) {

            jbig2_decode_mmr_consume(mmr, 6);
            b1 = jbig2_find_changing_element_of_color(ref, a0, mmr->width, !c);
            if (b1 + 2 <= mmr->width)
                b1 += 2;
            if (c) {
                if (b1 < a0) {
                    jbig2_error(ctx, JBIG2_SEVERITY_WARNING, JBIG2_UNKNOWN_SEGMENT_NUMBER, "ignoring negative VR(2) run");
                    b1 = a0;
                }
                if (a0 < mmr->width)
                    jbig2_set_bits(dst, a0, b1);
            }
            a0 = b1;
            c = !c;
        }

        else if ((word >> (32 - 7)) == 3) {

            jbig2_decode_mmr_consume(mmr, 7);
            b1 = jbig2_find_changing_element_of_color(ref, a0, mmr->width, !c);
            if (b1 + 3 <= mmr->width)
                b1 += 3;
            if (c) {
                if (b1 < a0) {
                    jbig2_error(ctx, JBIG2_SEVERITY_WARNING, JBIG2_UNKNOWN_SEGMENT_NUMBER, "ignoring negative VR(3) run");
                    b1 = a0;
                }
                if (a0 < mmr->width)
                    jbig2_set_bits(dst, a0, b1);
            }
            a0 = b1;
            c = !c;
        }

        else if ((word >> (32 - 3)) == 2) {

            jbig2_decode_mmr_consume(mmr, 3);
            b1 = jbig2_find_changing_element_of_color(ref, a0, mmr->width, !c);
            if (b1 >= 1)
                b1 -= 1;
            if (c) {
                if (b1 < a0) {
                    jbig2_error(ctx, JBIG2_SEVERITY_WARNING, JBIG2_UNKNOWN_SEGMENT_NUMBER, "ignoring negative VL(1) run");
                    b1 = a0;
                }
                if (a0 < mmr->width)
                    jbig2_set_bits(dst, a0, b1);
            }
            a0 = b1;
            c = !c;
        }

        else if ((word >> (32 - 6)) == 2) {

            jbig2_decode_mmr_consume(mmr, 6);
            b1 = jbig2_find_changing_element_of_color(ref, a0, mmr->width, !c);
            if (b1 >= 2)
                b1 -= 2;
            if (c) {
                if (b1 < a0) {
                    jbig2_error(ctx, JBIG2_SEVERITY_WARNING, JBIG2_UNKNOWN_SEGMENT_NUMBER, "ignoring negative VL(2) run");
                    b1 = a0;
                }
                if (a0 < mmr->width)
                    jbig2_set_bits(dst, a0, b1);
            }
            a0 = b1;
            c = !c;
        }

        else if ((word >> (32 - 7)) == 2) {

            jbig2_decode_mmr_consume(mmr, 7);
            b1 = jbig2_find_changing_element_of_color(ref, a0, mmr->width, !c);
            if (b1 >= 3)
                b1 -= 3;
            if (c) {
                if (b1 < a0) {
                    jbig2_error(ctx, JBIG2_SEVERITY_WARNING, JBIG2_UNKNOWN_SEGMENT_NUMBER, "ignoring negative VL(3) run");
                    b1 = a0;
                }
                if (a0 < mmr->width)
                    jbig2_set_bits(dst, a0, b1);
            }
            a0 = b1;
            c = !c;
        }

        else if ((word >> (32 - 24)) == 0x1001) {

            jbig2_decode_mmr_consume(mmr, 24);
            *eofb = 1;
            break;
        }

        else
            break;
    }

    return 0;
}

int
jbig2_decode_generic_mmr(Jbig2Ctx *ctx, Jbig2Segment *segment, const Jbig2GenericRegionParams *params, const byte *data, size_t size, Jbig2Image *image)
{
    Jbig2MmrCtx mmr;
    const uint32_t rowstride = image->stride;
    byte *dst = image->data;
    byte *ref = NULL;
    uint32_t y;
    int code = 0;
    int eofb = 0;

    (void) params;

    jbig2_decode_mmr_init(&mmr, image->width, image->height, data, size);

    for (y = 0; !eofb && y < image->height; y++) {
        memset(dst, 0, rowstride);
        code = jbig2_decode_mmr_line(ctx, &mmr, ref, dst, &eofb);
        if (code < 0)
            return jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "failed to decode mmr line");
        ref = dst;
        dst += rowstride;
    }

    if (eofb && y < image->height) {
        memset(dst, 0, rowstride * (image->height - y));
    }

    return code;
}

int
jbig2_decode_halftone_mmr(Jbig2Ctx *ctx, const Jbig2GenericRegionParams *params, const byte *data, size_t size, Jbig2Image *image, size_t *consumed_bytes)
{
    Jbig2MmrCtx mmr;
    const uint32_t rowstride = image->stride;
    byte *dst = image->data;
    byte *ref = NULL;
    uint32_t y;
    int code = 0;
    const uint32_t EOFB = 0x001001;
    int eofb = 0;

    (void) params;

    jbig2_decode_mmr_init(&mmr, image->width, image->height, data, size);

    for (y = 0; !eofb && y < image->height; y++) {
        memset(dst, 0, rowstride);
        code = jbig2_decode_mmr_line(ctx, &mmr, ref, dst, &eofb);
        if (code < 0)
            return jbig2_error(ctx, JBIG2_SEVERITY_WARNING, JBIG2_UNKNOWN_SEGMENT_NUMBER, "failed to decode halftone mmr line");
        ref = dst;
        dst += rowstride;
    }

    if (eofb && y < image->height) {
        memset(dst, 0, rowstride * (image->height - y));
    }

    if (mmr.word >> 8 == EOFB) {
        jbig2_decode_mmr_consume(&mmr, 24);
    }

    *consumed_bytes += (mmr.consumed_bits + 7) / 8;
    return code;
}

#ifdef HAVE_CONFIG_H
#endif

#include <stdlib.h>

#ifdef OUTPUT_PBM
#include <stdio.h>
#endif

#ifdef OUTPUT_PBM

#ifndef _JBIG2_IMAGE_RW_H
#define _JBIG2_IMAGE_RW_H

#include <stdio.h>

int jbig2_image_write_pbm_file(Jbig2Image *image, char *filename);
int jbig2_image_write_pbm(Jbig2Image *image, FILE *out);
Jbig2Image *jbig2_image_read_pbm_file(Jbig2Ctx *ctx, char *filename);
Jbig2Image *jbig2_image_read_pbm(Jbig2Ctx *ctx, FILE *in);

#ifdef HAVE_LIBPNG
int jbig2_image_write_png_file(Jbig2Image *image, char *filename);
int jbig2_image_write_png(Jbig2Image *image, FILE *out);
#endif

#endif

#endif

static void
dump_page_info(Jbig2Ctx *ctx, Jbig2Segment *segment, Jbig2Page *page)
{
    if (page->x_resolution == 0) {
        jbig2_error(ctx, JBIG2_SEVERITY_INFO, segment->number, "page %d image is %dx%d (unknown res)", page->number, page->width, page->height);
    } else if (page->x_resolution == page->y_resolution) {
        jbig2_error(ctx, JBIG2_SEVERITY_INFO, segment->number, "page %d image is %dx%d (%d ppm)", page->number, page->width, page->height, page->x_resolution);
    } else {
        jbig2_error(ctx, JBIG2_SEVERITY_INFO, segment->number,
                    "page %d image is %dx%d (%dx%d ppm)", page->number, page->width, page->height, page->x_resolution, page->y_resolution);
    }
    if (page->striped) {
        jbig2_error(ctx, JBIG2_SEVERITY_INFO, segment->number, "\tmaximum stripe size: %d", page->stripe_size);
    }
}

int
jbig2_page_info(Jbig2Ctx *ctx, Jbig2Segment *segment, const uint8_t *segment_data)
{
    Jbig2Page *page, *pages;

    page = &(ctx->pages[ctx->current_page]);
    if (page->number != 0 && (page->state == JBIG2_PAGE_NEW || page->state == JBIG2_PAGE_FREE)) {
        page->state = JBIG2_PAGE_COMPLETE;
        jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "unexpected page info segment, marking previous page finished");
    }

    {
        uint32_t index, j;

        index = ctx->current_page;
        while (ctx->pages[index].state != JBIG2_PAGE_FREE) {
            index++;
            if (index >= ctx->max_page_index) {

                if (ctx->max_page_index == UINT32_MAX) {
                    return jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number, "too many pages in jbig2 image");
                }
                else if (ctx->max_page_index > (UINT32_MAX >> 2)) {
                    ctx->max_page_index = UINT32_MAX;
                } else {
                    ctx->max_page_index <<= 2;
                }

                pages = jbig2_renew(ctx, ctx->pages, Jbig2Page, ctx->max_page_index);
                if (pages == NULL) {
                    return jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number, "failed to reallocate pages");
                }
                ctx->pages = pages;
                for (j = index; j < ctx->max_page_index; j++) {
                    ctx->pages[j].state = JBIG2_PAGE_FREE;
                    ctx->pages[j].number = 0;
                    ctx->pages[j].image = NULL;
                }
            }
        }
        page = &(ctx->pages[index]);
        ctx->current_page = index;
        page->state = JBIG2_PAGE_NEW;
        page->number = segment->page_association;
    }

    if (segment->data_length < 19) {
        return jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number, "segment too short");
    }

    page->width = jbig2_get_uint32(segment_data);
    page->height = jbig2_get_uint32(segment_data + 4);

    page->x_resolution = jbig2_get_uint32(segment_data + 8);
    page->y_resolution = jbig2_get_uint32(segment_data + 12);
    page->flags = segment_data[16];

    if (page->flags & 0x80)
        return jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number, "page segment indicates use of color segments (NYI)");

    {
        int16_t striping = jbig2_get_int16(segment_data + 17);

        if (striping & 0x8000) {
            page->striped = TRUE;
            page->stripe_size = striping & 0x7FFF;
        } else {
            page->striped = FALSE;
            page->stripe_size = 0;
        }
    }
    if (page->height == 0xFFFFFFFF && page->striped == FALSE) {
        jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "height is unspecified but page is not marked as striped, assuming striped with maximum strip size");
        page->striped = TRUE;
        page->stripe_size = 0x7FFF;
    }
    page->end_row = 0;

    if (segment->data_length > 19) {
        jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "extra data in segment");
    }

    dump_page_info(ctx, segment, page);

    if (page->height == 0xFFFFFFFF) {
        page->image = jbig2_image_new(ctx, page->width, page->stripe_size);
    } else {
        page->image = jbig2_image_new(ctx, page->width, page->height);
    }
    if (page->image == NULL) {
        return jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "failed to allocate buffer for page image");
    } else {

        jbig2_image_clear(ctx, page->image, (page->flags & 4));
        jbig2_error(ctx, JBIG2_SEVERITY_DEBUG, segment->number,
                    "allocated %dx%d page image (%d bytes)", page->image->width, page->image->height, page->image->stride * page->image->height);
    }

    return 0;
}

int
jbig2_end_of_stripe(Jbig2Ctx *ctx, Jbig2Segment *segment, const uint8_t *segment_data)
{
    Jbig2Page *page = &ctx->pages[ctx->current_page];
    uint32_t end_row;

    if (segment->data_length < 4)
        return jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number, "segment too short");
    end_row = jbig2_get_uint32(segment_data);
    if (end_row < page->end_row) {
        jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number,
                    "end of stripe segment with non-positive end row advance (new end row %d vs current end row %d)", end_row, page->end_row);
    } else {
        jbig2_error(ctx, JBIG2_SEVERITY_INFO, segment->number, "end of stripe: advancing end row from %u to %u", page->end_row, end_row);
    }

    page->end_row = end_row;

    return 0;
}

int
jbig2_complete_page(Jbig2Ctx *ctx)
{
    int code;

    if (ctx->segment_index != ctx->n_segments) {
        Jbig2Segment *segment = ctx->segments[ctx->segment_index];

        if ((segment->data_length & 0xffffffff) == 0xffffffff) {
            jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "file has an invalid segment data length; trying to decode using the available data");
            segment->data_length = ctx->buf_wr_ix - ctx->buf_rd_ix;
            code = jbig2_parse_segment(ctx, segment, ctx->buf + ctx->buf_rd_ix);
            ctx->buf_rd_ix += segment->data_length;
            ctx->segment_index++;
            if (code < 0) {
                return jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "failed to parse segment");
            }
        }
    }

    if (ctx->pages[ctx->current_page].image == NULL) {
        return jbig2_error(ctx, JBIG2_SEVERITY_FATAL, JBIG2_UNKNOWN_SEGMENT_NUMBER, "page has no image, cannot be completed");
    }

    ctx->pages[ctx->current_page].state = JBIG2_PAGE_COMPLETE;
    return 0;
}

int
jbig2_end_of_page(Jbig2Ctx *ctx, Jbig2Segment *segment, const uint8_t *segment_data)
{
    uint32_t page_number = ctx->pages[ctx->current_page].number;
    int code;

    (void) segment_data;

    if (segment->page_association != page_number) {
        jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number,
                    "end of page marker for page %d doesn't match current page number %d", segment->page_association, page_number);
    }

    jbig2_error(ctx, JBIG2_SEVERITY_INFO, segment->number, "end of page %d", page_number);

    code = jbig2_complete_page(ctx);
    if (code < 0)
        return jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "failed to complete page");

#ifdef OUTPUT_PBM
    code = jbig2_image_write_pbm(ctx->pages[ctx->current_page].image, stdout);
    if (code < 0)
        return jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "failed to write page image");
#endif

    return 0;
}

int
jbig2_page_add_result(Jbig2Ctx *ctx, Jbig2Page *page, Jbig2Image *image, uint32_t x, uint32_t y, Jbig2ComposeOp op)
{
    int code;

    if (x > INT32_MAX || y > INT32_MAX)
        return jbig2_error(ctx, JBIG2_SEVERITY_FATAL, JBIG2_UNKNOWN_SEGMENT_NUMBER, "unsupported image coordinates");

    if (page->image == NULL)
        return jbig2_error(ctx, JBIG2_SEVERITY_WARNING, JBIG2_UNKNOWN_SEGMENT_NUMBER, "page info possibly missing, no image defined");

    if (page->striped && page->height == 0xFFFFFFFF) {
        uint32_t new_height;

        if (y > UINT32_MAX - image->height)
                return jbig2_error(ctx, JBIG2_SEVERITY_FATAL, JBIG2_UNKNOWN_SEGMENT_NUMBER, "adding image at coordinate would grow page out of bounds");
        new_height = y + image->height;

        if (page->image->height < new_height) {
            Jbig2Image *resized_image = NULL;

            jbig2_error(ctx, JBIG2_SEVERITY_DEBUG, JBIG2_UNKNOWN_SEGMENT_NUMBER, "growing page buffer to %u rows to accommodate new stripe", new_height);
            resized_image = jbig2_image_resize(ctx, page->image, page->image->width, new_height, page->flags & 4);
            if (resized_image == NULL) {
                return jbig2_error(ctx, JBIG2_SEVERITY_FATAL, JBIG2_UNKNOWN_SEGMENT_NUMBER, "unable to resize image to accommodate new stripe");
            }
            page->image = resized_image;
        }
    }

    code = jbig2_image_compose(ctx, page->image, image, x, y, op);
    if (code < 0)
        return jbig2_error(ctx, JBIG2_SEVERITY_WARNING, JBIG2_UNKNOWN_SEGMENT_NUMBER, "failed to compose image with page");

    return 0;
}

Jbig2Image *
jbig2_page_out(Jbig2Ctx *ctx)
{
    uint32_t index;

    for (index = 0; index < ctx->max_page_index; index++) {
        if (ctx->pages[index].state == JBIG2_PAGE_COMPLETE) {
            Jbig2Image *img = ctx->pages[index].image;
            uint32_t page_number = ctx->pages[index].number;

            if (img == NULL) {
                jbig2_error(ctx, JBIG2_SEVERITY_WARNING, JBIG2_UNKNOWN_SEGMENT_NUMBER, "page %d returned with no associated image", page_number);
                continue;
            }

            ctx->pages[index].state = JBIG2_PAGE_RETURNED;
            jbig2_error(ctx, JBIG2_SEVERITY_DEBUG, JBIG2_UNKNOWN_SEGMENT_NUMBER, "page %d returned to the client", page_number);
            return jbig2_image_reference(ctx, img);
        }
    }

    return NULL;
}

void
jbig2_release_page(Jbig2Ctx *ctx, Jbig2Image *image)
{
    uint32_t index;

    if (image == NULL)
        return;

    for (index = 0; index < ctx->max_page_index; index++) {
        if (ctx->pages[index].image == image) {
            jbig2_image_release(ctx, image);
            ctx->pages[index].state = JBIG2_PAGE_RELEASED;
            jbig2_error(ctx, JBIG2_SEVERITY_DEBUG, JBIG2_UNKNOWN_SEGMENT_NUMBER, "page %d released by the client", ctx->pages[index].number);
            return;
        }
    }

    jbig2_error(ctx, JBIG2_SEVERITY_WARNING, JBIG2_UNKNOWN_SEGMENT_NUMBER, "failed to release unknown page");
}

#ifdef HAVE_CONFIG_H
#endif

#include <stddef.h>
#include <string.h>

#include <stdio.h>

#ifndef _JBIG2_REFINEMENT_H
#define _JBIG2_REFINEMENT_H

typedef struct {

    bool GRTEMPLATE;
    Jbig2Image *GRREFERENCE;
    int32_t GRREFERENCEDX, GRREFERENCEDY;
    bool TPGRON;
    int8_t grat[4];
} Jbig2RefinementRegionParams;

int
jbig2_decode_refinement_region(Jbig2Ctx *ctx,
                               Jbig2Segment *segment,
                               const Jbig2RefinementRegionParams *params, Jbig2ArithState *as, Jbig2Image *image, Jbig2ArithCx *GB_stats);

int jbig2_refinement_region(Jbig2Ctx *ctx, Jbig2Segment *segment, const byte *segment_data);

#endif

#ifdef JBIG2_DEBUG_DUMP
#endif

#define pixel_outside_field(x, y) \
    ((y) < -128 || (y) > 0 || (x) < -128 || ((y) < 0 && (x) > 127) || ((y) == 0 && (x) >= 0))
#define refpixel_outside_field(x, y) \
    ((y) < -128 || (y) > 127 || (x) < -128 || (x) > 127)

static int
jbig2_decode_refinement_template0_unopt(Jbig2Ctx *ctx,
                                        Jbig2Segment *segment,
                                        const Jbig2RefinementRegionParams *params, Jbig2ArithState *as, Jbig2Image *image, Jbig2ArithCx *GR_stats)
{
    const int64_t GRW = image->width;
    const int64_t GRH = image->height;
    Jbig2Image *ref = params->GRREFERENCE;
    const int32_t dx = params->GRREFERENCEDX;
    const int32_t dy = params->GRREFERENCEDY;
    uint32_t CONTEXT;
    int64_t x, y;
    int bit;

    if (pixel_outside_field(params->grat[0], params->grat[1]) ||
        refpixel_outside_field(params->grat[2], params->grat[3]))
        return jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number,
                           "adaptive template pixel is out of field");

    for (y = 0; y < GRH; y++) {
        for (x = 0; x < GRW; x++) {
            CONTEXT = 0;
            CONTEXT |= jbig2_image_get_pixel(image, x - 1, y + 0) << 0;
            CONTEXT |= jbig2_image_get_pixel(image, x + 1, y - 1) << 1;
            CONTEXT |= jbig2_image_get_pixel(image, x + 0, y - 1) << 2;
            CONTEXT |= jbig2_image_get_pixel(image, x + params->grat[0], y + params->grat[1]) << 3;
            CONTEXT |= jbig2_image_get_pixel(ref, x - dx + 1, y - dy + 1) << 4;
            CONTEXT |= jbig2_image_get_pixel(ref, x - dx + 0, y - dy + 1) << 5;
            CONTEXT |= jbig2_image_get_pixel(ref, x - dx - 1, y - dy + 1) << 6;
            CONTEXT |= jbig2_image_get_pixel(ref, x - dx + 1, y - dy + 0) << 7;
            CONTEXT |= jbig2_image_get_pixel(ref, x - dx + 0, y - dy + 0) << 8;
            CONTEXT |= jbig2_image_get_pixel(ref, x - dx - 1, y - dy + 0) << 9;
            CONTEXT |= jbig2_image_get_pixel(ref, x - dx + 1, y - dy - 1) << 10;
            CONTEXT |= jbig2_image_get_pixel(ref, x - dx + 0, y - dy - 1) << 11;
            CONTEXT |= jbig2_image_get_pixel(ref, x - dx + params->grat[2], y - dy + params->grat[3]) << 12;
            bit = jbig2_arith_decode(ctx, as, &GR_stats[CONTEXT]);
            if (bit < 0)
                return jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "failed to decode arithmetic code when handling refinement template0");
            jbig2_image_set_pixel(image, x, y, bit);
        }
    }
#ifdef JBIG2_DEBUG_DUMP
    {
        static unsigned int count = 0;
        char name[32];
        int code;

        snprintf(name, 32, "refin-%d.pbm", count);
        code = jbig2_image_write_pbm_file(ref, name);
        if (code < 0)
            return jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "failed write refinement input");
        snprintf(name, 32, "refout-%d.pbm", count);
        code = jbig2_image_write_pbm_file(image, name);
        if (code < 0)
            return jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "failed write refinement output");
        count++;
    }
#endif

    return 0;
}

static int
jbig2_decode_refinement_template1_unopt(Jbig2Ctx *ctx,
                                        Jbig2Segment *segment,
                                        const Jbig2RefinementRegionParams *params, Jbig2ArithState *as, Jbig2Image *image, Jbig2ArithCx *GR_stats)
{
    const int64_t GRW = image->width;
    const int64_t GRH = image->height;
    Jbig2Image *ref = params->GRREFERENCE;
    const int32_t dx = params->GRREFERENCEDX;
    const int32_t dy = params->GRREFERENCEDY;
    uint32_t CONTEXT;
    int64_t x, y;
    int bit;

    for (y = 0; y < GRH; y++) {
        for (x = 0; x < GRW; x++) {
            CONTEXT = 0;
            CONTEXT |= jbig2_image_get_pixel(image, x - 1, y + 0) << 0;
            CONTEXT |= jbig2_image_get_pixel(image, x + 1, y - 1) << 1;
            CONTEXT |= jbig2_image_get_pixel(image, x + 0, y - 1) << 2;
            CONTEXT |= jbig2_image_get_pixel(image, x - 1, y - 1) << 3;
            CONTEXT |= jbig2_image_get_pixel(ref, x - dx + 1, y - dy + 1) << 4;
            CONTEXT |= jbig2_image_get_pixel(ref, x - dx + 0, y - dy + 1) << 5;
            CONTEXT |= jbig2_image_get_pixel(ref, x - dx + 1, y - dy + 0) << 6;
            CONTEXT |= jbig2_image_get_pixel(ref, x - dx + 0, y - dy + 0) << 7;
            CONTEXT |= jbig2_image_get_pixel(ref, x - dx - 1, y - dy + 0) << 8;
            CONTEXT |= jbig2_image_get_pixel(ref, x - dx + 0, y - dy - 1) << 9;
            bit = jbig2_arith_decode(ctx, as, &GR_stats[CONTEXT]);
            if (bit < 0)
                return jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "failed to decode arithmetic code when handling refinement template0");
            jbig2_image_set_pixel(image, x, y, bit);
        }
    }

#ifdef JBIG2_DEBUG_DUMP
    {
        static unsigned int count = 0;
        char name[32];
        int code;

        snprintf(name, 32, "refin-%d.pbm", count);
        code = jbig2_image_write_pbm_file(ref, name);
        if (code < 0)
            return jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "failed to write refinement input");
        snprintf(name, 32, "refout-%d.pbm", count);
        code = jbig2_image_write_pbm_file(image, name);
        if (code < 0)
            return jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "failed to write refinement output");
        count++;
    }
#endif

    return 0;
}

#if 0
static int
jbig2_decode_refinement_template1(Jbig2Ctx *ctx,
                                  Jbig2Segment *segment,
                                  const Jbig2RefinementRegionParams *params, Jbig2ArithState *as, Jbig2Image *image, Jbig2ArithCx *GR_stats)
{
    const int64_t GRW = image->width;
    const int64_t GRH = image->height;
    const int stride = image->stride;
    const int refstride = params->reference->stride;
    const int dy = params->DY;
    byte *grreg_line = (byte *) image->data;
    byte *grref_line = (byte *) params->reference->data;
    int64_t x, y;

    for (y = 0; y < GRH; y++) {
        const int padded_width = (GRW + 7) & -8;
        uint32_t CONTEXT;
        uint32_t refline_m1;
        uint32_t refline_0;
        uint32_t refline_1;
        uint32_t line_m1;

        line_m1 = (y >= 1) ? grreg_line[-stride] : 0;
        refline_m1 = ((y - dy) >= 1) ? grref_line[(-1 - dy) * stride] << 2 : 0;
        refline_0 = (((y - dy) > 0) && ((y - dy) < GRH)) ? grref_line[(0 - dy) * stride] << 4 : 0;
        refline_1 = (y < GRH - 1) ? grref_line[(+1 - dy) * stride] << 7 : 0;
        CONTEXT = ((line_m1 >> 5) & 0x00e) | ((refline_1 >> 5) & 0x030) | ((refline_0 >> 5) & 0x1c0) | ((refline_m1 >> 5) & 0x200);

        for (x = 0; x < padded_width; x += 8) {
            byte result = 0;
            int x_minor;
            const int minor_width = GRW - x > 8 ? 8 : GRW - x;

            if (y >= 1) {
                line_m1 = (line_m1 << 8) | (x + 8 < GRW ? grreg_line[-stride + (x >> 3) + 1] : 0);
                refline_m1 = (refline_m1 << 8) | (x + 8 < GRW ? grref_line[-refstride + (x >> 3) + 1] << 2 : 0);
            }

            refline_0 = (refline_0 << 8) | (x + 8 < GRW ? grref_line[(x >> 3) + 1] << 4 : 0);

            if (y < GRH - 1)
                refline_1 = (refline_1 << 8) | (x + 8 < GRW ? grref_line[+refstride + (x >> 3) + 1] << 7 : 0);
            else
                refline_1 = 0;

            for (x_minor = 0; x_minor < minor_width; x_minor++) {
                int bit;

                bit = jbig2_arith_decode(ctx, as, &GR_stats[CONTEXT]);
                if (bit < 0)
                    return jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "failed to decode arithmetic code when handling refinement template1");
                result |= bit << (7 - x_minor);
                CONTEXT = ((CONTEXT & 0x0d6) << 1) | bit |
                          ((line_m1 >> (9 - x_minor)) & 0x002) |
                          ((refline_1 >> (9 - x_minor)) & 0x010) | ((refline_0 >> (9 - x_minor)) & 0x040) | ((refline_m1 >> (9 - x_minor)) & 0x200);
            }

            grreg_line[x >> 3] = result;

        }

        grreg_line += stride;
        grref_line += refstride;

    }

    return 0;

}
#endif

typedef uint32_t(*ContextBuilder)(const Jbig2RefinementRegionParams *, Jbig2Image *, int64_t, int64_t);

static int
implicit_value(const Jbig2RefinementRegionParams *params, Jbig2Image *image, int64_t x, int64_t y)
{
    Jbig2Image *ref = params->GRREFERENCE;
    const int64_t i = x - params->GRREFERENCEDX;
    const int64_t j = y - params->GRREFERENCEDY;
    int m = jbig2_image_get_pixel(ref, i, j);

    (void) image;

    return ((jbig2_image_get_pixel(ref, i - 1, j - 1) == m) &&
            (jbig2_image_get_pixel(ref, i, j - 1) == m) &&
            (jbig2_image_get_pixel(ref, i + 1, j - 1) == m) &&
            (jbig2_image_get_pixel(ref, i - 1, j) == m) &&
            (jbig2_image_get_pixel(ref, i + 1, j) == m) &&
            (jbig2_image_get_pixel(ref, i - 1, j + 1) == m) &&
            (jbig2_image_get_pixel(ref, i, j + 1) == m) &&
            (jbig2_image_get_pixel(ref, i + 1, j + 1) == m)
           )? m : -1;
}

static uint32_t
mkctx0(const Jbig2RefinementRegionParams *params, Jbig2Image *image, int64_t x, int64_t y)
{
    Jbig2Image *ref = params->GRREFERENCE;
    const int32_t dx = params->GRREFERENCEDX;
    const int32_t dy = params->GRREFERENCEDY;
    uint32_t CONTEXT;

    CONTEXT = jbig2_image_get_pixel(image, x - 1, y + 0);
    CONTEXT |= jbig2_image_get_pixel(image, x + 1, y - 1) << 1;
    CONTEXT |= jbig2_image_get_pixel(image, x + 0, y - 1) << 2;
    CONTEXT |= jbig2_image_get_pixel(image, x + params->grat[0], y + params->grat[1]) << 3;
    CONTEXT |= jbig2_image_get_pixel(ref, x - dx + 1, y - dy + 1) << 4;
    CONTEXT |= jbig2_image_get_pixel(ref, x - dx + 0, y - dy + 1) << 5;
    CONTEXT |= jbig2_image_get_pixel(ref, x - dx - 1, y - dy + 1) << 6;
    CONTEXT |= jbig2_image_get_pixel(ref, x - dx + 1, y - dy + 0) << 7;
    CONTEXT |= jbig2_image_get_pixel(ref, x - dx + 0, y - dy + 0) << 8;
    CONTEXT |= jbig2_image_get_pixel(ref, x - dx - 1, y - dy + 0) << 9;
    CONTEXT |= jbig2_image_get_pixel(ref, x - dx + 1, y - dy - 1) << 10;
    CONTEXT |= jbig2_image_get_pixel(ref, x - dx + 0, y - dy - 1) << 11;
    CONTEXT |= jbig2_image_get_pixel(ref, x - dx + params->grat[2], y - dy + params->grat[3]) << 12;
    return CONTEXT;
}

static uint32_t
mkctx1(const Jbig2RefinementRegionParams *params, Jbig2Image *image, int64_t x, int64_t y)
{
    Jbig2Image *ref = params->GRREFERENCE;
    const int32_t dx = params->GRREFERENCEDX;
    const int32_t dy = params->GRREFERENCEDY;
    uint32_t CONTEXT;

    CONTEXT = jbig2_image_get_pixel(image, x - 1, y + 0);
    CONTEXT |= jbig2_image_get_pixel(image, x + 1, y - 1) << 1;
    CONTEXT |= jbig2_image_get_pixel(image, x + 0, y - 1) << 2;
    CONTEXT |= jbig2_image_get_pixel(image, x - 1, y - 1) << 3;
    CONTEXT |= jbig2_image_get_pixel(ref, x - dx + 1, y - dy + 1) << 4;
    CONTEXT |= jbig2_image_get_pixel(ref, x - dx + 0, y - dy + 1) << 5;
    CONTEXT |= jbig2_image_get_pixel(ref, x - dx + 1, y - dy + 0) << 6;
    CONTEXT |= jbig2_image_get_pixel(ref, x - dx + 0, y - dy + 0) << 7;
    CONTEXT |= jbig2_image_get_pixel(ref, x - dx - 1, y - dy + 0) << 8;
    CONTEXT |= jbig2_image_get_pixel(ref, x - dx + 0, y - dy - 1) << 9;
    return CONTEXT;
}

static int
jbig2_decode_refinement_TPGRON(Jbig2Ctx *ctx, const Jbig2RefinementRegionParams *params, Jbig2ArithState *as, Jbig2Image *image, Jbig2ArithCx *GR_stats)
{
    const int64_t GRW = image->width;
    const int64_t GRH = image->height;
    int64_t x, y;
    int iv, LTP = 0;
    uint32_t start_context = (params->GRTEMPLATE ? 0x40 : 0x100);
    ContextBuilder mkctx = (params->GRTEMPLATE ? mkctx1 : mkctx0);

    if (params->GRTEMPLATE == 0 &&
        (pixel_outside_field(params->grat[0], params->grat[1]) ||
        refpixel_outside_field(params->grat[2], params->grat[3])))
        return jbig2_error(ctx, JBIG2_SEVERITY_FATAL, JBIG2_UNKNOWN_SEGMENT_NUMBER,
                           "adaptive template pixel is out of field");

    for (y = 0; y < GRH; y++) {
        int bit = jbig2_arith_decode(ctx, as, &GR_stats[start_context]);
        if (bit < 0)
            return jbig2_error(ctx, JBIG2_SEVERITY_WARNING, JBIG2_UNKNOWN_SEGMENT_NUMBER, "failed to decode arithmetic code when handling refinement TPGRON1");
        LTP ^= bit;
        if (!LTP) {
            for (x = 0; x < GRW; x++) {
                bit = jbig2_arith_decode(ctx, as, &GR_stats[mkctx(params, image, x, y)]);
                if (bit < 0)
                    return jbig2_error(ctx, JBIG2_SEVERITY_WARNING, JBIG2_UNKNOWN_SEGMENT_NUMBER, "failed to decode arithmetic code when handling refinement TPGRON1");
                jbig2_image_set_pixel(image, x, y, bit);
            }
        } else {
            for (x = 0; x < GRW; x++) {
                iv = implicit_value(params, image, x, y);
                if (iv < 0) {
                    int bit = jbig2_arith_decode(ctx, as, &GR_stats[mkctx(params, image, x, y)]);
                    if (bit < 0)
                        return jbig2_error(ctx, JBIG2_SEVERITY_WARNING, JBIG2_UNKNOWN_SEGMENT_NUMBER, "failed to decode arithmetic code when handling refinement TPGRON1");
                    jbig2_image_set_pixel(image, x, y, bit);
                } else
                    jbig2_image_set_pixel(image, x, y, iv);
            }
        }
    }

    return 0;
}

int
jbig2_decode_refinement_region(Jbig2Ctx *ctx,
                               Jbig2Segment *segment,
                               const Jbig2RefinementRegionParams *params, Jbig2ArithState *as, Jbig2Image *image, Jbig2ArithCx *GR_stats)
{
    jbig2_error(ctx, JBIG2_SEVERITY_DEBUG, segment->number,
                "decoding generic refinement region with offset %d,%x, GRTEMPLATE=%d, TPGRON=%d",
                params->GRREFERENCEDX, params->GRREFERENCEDY, params->GRTEMPLATE, params->TPGRON);

    if (params->TPGRON)
        return jbig2_decode_refinement_TPGRON(ctx, params, as, image, GR_stats);

    if (params->GRTEMPLATE)
        return jbig2_decode_refinement_template1_unopt(ctx, segment, params, as, image, GR_stats);
    else
        return jbig2_decode_refinement_template0_unopt(ctx, segment, params, as, image, GR_stats);
}

static Jbig2Segment *
jbig2_region_find_referred(Jbig2Ctx *ctx, Jbig2Segment *segment)
{
    const int nsegments = segment->referred_to_segment_count;
    Jbig2Segment *rsegment;
    int index;

    for (index = 0; index < nsegments; index++) {
        rsegment = jbig2_find_segment(ctx, segment->referred_to_segments[index]);
        if (rsegment == NULL) {
            jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "failed to find referred to segment %d", segment->referred_to_segments[index]);
            continue;
        }
        switch (rsegment->flags & 63) {
        case 4:
        case 20:
        case 36:
        case 40:
            if (rsegment->result)
                return rsegment;
            break;
        default:
            break;
        }
    }

    return NULL;
}

int
jbig2_refinement_region(Jbig2Ctx *ctx, Jbig2Segment *segment, const byte *segment_data)
{
    Jbig2RefinementRegionParams params;
    Jbig2RegionSegmentInfo rsi;
    int offset = 0;
    byte seg_flags;
    int code = 0;

    if (segment->data_length < 18)
        return jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number, "segment too short");

    jbig2_get_region_segment_info(&rsi, segment_data);
    jbig2_error(ctx, JBIG2_SEVERITY_INFO, segment->number, "generic region: %u x %u @ (%u, %u), flags = %02x", rsi.width, rsi.height, rsi.x, rsi.y, rsi.flags);

    seg_flags = segment_data[17];
    params.GRTEMPLATE = seg_flags & 0x01;
    params.TPGRON = seg_flags & 0x02 ? 1 : 0;
    jbig2_error(ctx, JBIG2_SEVERITY_INFO, segment->number,
                "segment flags = %02x %s%s", seg_flags, params.GRTEMPLATE ? " GRTEMPLATE" : "", params.TPGRON ? " TPGRON" : "");
    if (seg_flags & 0xFC)
        jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "reserved segment flag bits are non-zero");
    offset += 18;

    if (!params.GRTEMPLATE) {
        if (segment->data_length < 22)
            return jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number, "segment too short");
        params.grat[0] = segment_data[offset + 0];
        params.grat[1] = segment_data[offset + 1];
        params.grat[2] = segment_data[offset + 2];
        params.grat[3] = segment_data[offset + 3];
        jbig2_error(ctx, JBIG2_SEVERITY_INFO, segment->number,
                    "grat1: (%d, %d) grat2: (%d, %d)", params.grat[0], params.grat[1], params.grat[2], params.grat[3]);
        offset += 4;
    }

    if (segment->referred_to_segment_count) {
        Jbig2Segment *ref;

        ref = jbig2_region_find_referred(ctx, segment);
        if (ref == NULL)
            return jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "failed to find reference bitmap");
        if (ref->result == NULL)
            return jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number, "reference bitmap has no decoded image");

        params.GRREFERENCE = jbig2_image_reference(ctx, (Jbig2Image *) ref->result);
        jbig2_image_release(ctx, (Jbig2Image *) ref->result);
        ref->result = NULL;
        jbig2_error(ctx, JBIG2_SEVERITY_DEBUG, segment->number, "found reference bitmap in segment %d", ref->number);
    } else {

        if (ctx->pages[ctx->current_page].image == NULL)
            return jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number, "reference page bitmap has no decoded image");
        params.GRREFERENCE = jbig2_image_reference(ctx, ctx->pages[ctx->current_page].image);

    }

    params.GRREFERENCEDX = 0;
    params.GRREFERENCEDY = 0;
    {
        Jbig2WordStream *ws = NULL;
        Jbig2ArithState *as = NULL;
        Jbig2ArithCx *GR_stats = NULL;
        int stats_size;
        Jbig2Image *image = NULL;

        image = jbig2_image_new(ctx, rsi.width, rsi.height);
        if (image == NULL) {
            code = jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "failed to allocate refinement image");
            goto cleanup;
        }
        jbig2_error(ctx, JBIG2_SEVERITY_DEBUG, segment->number, "allocated %d x %d image buffer for region decode results", rsi.width, rsi.height);

        stats_size = params.GRTEMPLATE ? 1 << 10 : 1 << 13;
        GR_stats = jbig2_new(ctx, Jbig2ArithCx, stats_size);
        if (GR_stats == NULL) {
            code = jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number, "failed to allocate arithmetic decoder state for generic refinement regions");
            goto cleanup;
        }
        memset(GR_stats, 0, stats_size);

        ws = jbig2_word_stream_buf_new(ctx, segment_data + offset, segment->data_length - offset);
        if (ws == NULL) {
            code = jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "failed to allocate word stream when handling refinement region");
            goto cleanup;
        }

        as = jbig2_arith_new(ctx, ws);
        if (as == NULL) {
            code = jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "failed to allocate arithmetic coding state when handling refinement region");
            goto cleanup;
        }

        code = jbig2_decode_refinement_region(ctx, segment, &params, as, image, GR_stats);
        if (code < 0) {
            jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "failed to decode refinement region");
            goto cleanup;
        }

        if ((segment->flags & 63) == 40) {

            segment->result = jbig2_image_reference(ctx, image);
        } else {

            jbig2_error(ctx, JBIG2_SEVERITY_DEBUG, segment->number,
                        "composing %dx%d decoded refinement region onto page at (%d, %d)", rsi.width, rsi.height, rsi.x, rsi.y);
            code = jbig2_page_add_result(ctx, &ctx->pages[ctx->current_page], image, rsi.x, rsi.y, rsi.op);
            if (code < 0) {
                jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "unable to add refinement region to page");
                goto cleanup;
            }
        }

cleanup:
        jbig2_image_release(ctx, image);
        jbig2_image_release(ctx, params.GRREFERENCE);
        jbig2_free(ctx->allocator, as);
        jbig2_word_stream_buf_free(ctx, ws);
        jbig2_free(ctx->allocator, GR_stats);
    }

    return code;
}

#ifdef HAVE_CONFIG_H
#endif

#include <stddef.h>

#ifndef _JBIG2_SYMBOL_DICT_H
#define _JBIG2_SYMBOL_DICT_H

typedef struct {
    uint32_t n_symbols;
    Jbig2Image **glyphs;
} Jbig2SymbolDict;

int jbig2_symbol_dictionary(Jbig2Ctx *ctx, Jbig2Segment *segment, const byte *segment_data);

Jbig2Image *jbig2_sd_glyph(Jbig2SymbolDict *dict, unsigned int id);

Jbig2SymbolDict *jbig2_sd_new(Jbig2Ctx *ctx, uint32_t n_symbols);

void jbig2_sd_release(Jbig2Ctx *ctx, Jbig2SymbolDict *dict);

Jbig2SymbolDict *jbig2_sd_cat(Jbig2Ctx *ctx, uint32_t n_dicts, Jbig2SymbolDict **dicts);

uint32_t jbig2_sd_count_referred(Jbig2Ctx *ctx, Jbig2Segment *segment);

Jbig2SymbolDict **jbig2_sd_list_referred(Jbig2Ctx *ctx, Jbig2Segment *segment);

#endif

#ifndef _JBIG2_TEXT_H
#define _JBIG2_TEXT_H

typedef enum {
    JBIG2_CORNER_BOTTOMLEFT = 0,
    JBIG2_CORNER_TOPLEFT = 1,
    JBIG2_CORNER_BOTTOMRIGHT = 2,
    JBIG2_CORNER_TOPRIGHT = 3
} Jbig2RefCorner;

typedef struct {
    bool SBHUFF;
    bool SBREFINE;
    bool SBDEFPIXEL;
    Jbig2ComposeOp SBCOMBOP;
    bool TRANSPOSED;
    Jbig2RefCorner REFCORNER;
    int SBDSOFFSET;

    uint32_t SBNUMINSTANCES;
    int LOGSBSTRIPS;
    int SBSTRIPS;

    Jbig2HuffmanTable *SBHUFFFS;
    Jbig2HuffmanTable *SBHUFFDS;
    Jbig2HuffmanTable *SBHUFFDT;
    Jbig2HuffmanTable *SBHUFFRDW;
    Jbig2HuffmanTable *SBHUFFRDH;
    Jbig2HuffmanTable *SBHUFFRDX;
    Jbig2HuffmanTable *SBHUFFRDY;
    Jbig2HuffmanTable *SBHUFFRSIZE;
    Jbig2ArithIntCtx *IADT;
    Jbig2ArithIntCtx *IAFS;
    Jbig2ArithIntCtx *IADS;
    Jbig2ArithIntCtx *IAIT;
    Jbig2ArithIaidCtx *IAID;
    Jbig2ArithIntCtx *IARI;
    Jbig2ArithIntCtx *IARDW;
    Jbig2ArithIntCtx *IARDH;
    Jbig2ArithIntCtx *IARDX;
    Jbig2ArithIntCtx *IARDY;
    bool SBRTEMPLATE;
    int8_t sbrat[4];
} Jbig2TextRegionParams;

int
jbig2_decode_text_region(Jbig2Ctx *ctx, Jbig2Segment *segment,
                         const Jbig2TextRegionParams *params,
                         const Jbig2SymbolDict *const *dicts, const uint32_t n_dicts,
                         Jbig2Image *image, Jbig2ArithCx *GR_stats, Jbig2ArithState *as, Jbig2WordStream *ws);

int jbig2_text_region(Jbig2Ctx *ctx, Jbig2Segment *segment, const uint8_t *segment_data);

#endif

Jbig2Segment *
jbig2_parse_segment_header(Jbig2Ctx *ctx, uint8_t *buf, size_t buf_size, size_t *p_header_size)
{
    Jbig2Segment *result;
    uint8_t rtscarf;
    uint32_t rtscarf_long;
    uint32_t *referred_to_segments;
    uint32_t referred_to_segment_count;
    uint32_t referred_to_segment_size;
    uint32_t pa_size;
    uint32_t offset;

    if (buf_size < 11)
        return NULL;

    result = jbig2_new(ctx, Jbig2Segment, 1);
    if (result == NULL) {
        jbig2_error(ctx, JBIG2_SEVERITY_FATAL, JBIG2_UNKNOWN_SEGMENT_NUMBER, "failed to allocate segment");
        return NULL;
    }

    result->number = jbig2_get_uint32(buf);
    if (result->number == JBIG2_UNKNOWN_SEGMENT_NUMBER) {
        jbig2_error(ctx, JBIG2_SEVERITY_FATAL, JBIG2_UNKNOWN_SEGMENT_NUMBER, "segment number too large");
        jbig2_free(ctx->allocator, result);
        return NULL;
    }

    result->flags = buf[4];

    rtscarf = buf[5];
    if ((rtscarf & 0xe0) == 0xe0) {
        rtscarf_long = jbig2_get_uint32(buf + 5);
        referred_to_segment_count = rtscarf_long & 0x1fffffff;
        offset = 5 + 4 + (referred_to_segment_count + 1) / 8;
    } else {
        referred_to_segment_count = (rtscarf >> 5);
        offset = 5 + 1;
    }
    result->referred_to_segment_count = referred_to_segment_count;

    referred_to_segment_size = result->number <= 256 ? 1 : result->number <= 65536 ? 2 : 4;
    pa_size = result->flags & 0x40 ? 4 : 1;
    if (offset + referred_to_segment_count * referred_to_segment_size + pa_size + 4 > buf_size) {
        jbig2_error(ctx, JBIG2_SEVERITY_DEBUG, result->number, "attempted to parse segment header with insufficient data, asking for more data");
        jbig2_free(ctx->allocator, result);
        return NULL;
    }

    if (referred_to_segment_count) {
        uint32_t i;

        referred_to_segments = jbig2_new(ctx, uint32_t, referred_to_segment_count * referred_to_segment_size);
        if (referred_to_segments == NULL) {
            jbig2_error(ctx, JBIG2_SEVERITY_FATAL, result->number, "failed to allocate referred to segments");
            jbig2_free(ctx->allocator, result);
            return NULL;
        }

        for (i = 0; i < referred_to_segment_count; i++) {
            referred_to_segments[i] =
                (referred_to_segment_size == 1) ? buf[offset] :
                (referred_to_segment_size == 2) ? jbig2_get_uint16(buf + offset) : jbig2_get_uint32(buf + offset);
            offset += referred_to_segment_size;
            jbig2_error(ctx, JBIG2_SEVERITY_DEBUG, result->number, "segment %d refers to segment %d", result->number, referred_to_segments[i]);
        }
        result->referred_to_segments = referred_to_segments;
    } else {

        result->referred_to_segments = NULL;
    }

    if (pa_size == 4) {
        result->page_association = jbig2_get_uint32(buf + offset);
        offset += 4;
    } else {
        result->page_association = buf[offset++];
    }
    jbig2_error(ctx, JBIG2_SEVERITY_DEBUG, result->number, "segment %d is associated with page %d", result->number, result->page_association);

    result->rows = UINT32_MAX;
    result->data_length = jbig2_get_uint32(buf + offset);
    *p_header_size = offset + 4;

    result->result = NULL;

    return result;
}

void
jbig2_free_segment(Jbig2Ctx *ctx, Jbig2Segment *segment)
{
    if (segment == NULL)
        return;

    jbig2_free(ctx->allocator, segment->referred_to_segments);

    switch (segment->flags & 63) {
    case 0:
        if (segment->result != NULL)
            jbig2_sd_release(ctx, (Jbig2SymbolDict *) segment->result);
        break;
    case 4:
    case 40:
        if (segment->result != NULL)
            jbig2_image_release(ctx, (Jbig2Image *) segment->result);
        break;
    case 16:
        if (segment->result != NULL)
            jbig2_hd_release(ctx, (Jbig2PatternDict *) segment->result);
        break;
    case 53:
        if (segment->result != NULL)
            jbig2_table_free(ctx, (Jbig2HuffmanParams *) segment->result);
        break;
    default:

        break;
    }
    jbig2_free(ctx->allocator, segment);
}

Jbig2Segment *
jbig2_find_segment(Jbig2Ctx *ctx, uint32_t number)
{
    int index, index_max = ctx->segment_index - 1;
    const Jbig2Ctx *global_ctx = ctx->global_ctx;

    for (index = index_max; index >= 0; index--)
        if (ctx->segments[index]->number == number)
            return (ctx->segments[index]);

    if (global_ctx)
        for (index = global_ctx->segment_index - 1; index >= 0; index--)
            if (global_ctx->segments[index]->number == number)
                return (global_ctx->segments[index]);

    return NULL;
}

void
jbig2_get_region_segment_info(Jbig2RegionSegmentInfo *info, const uint8_t *segment_data)
{

    info->width = jbig2_get_uint32(segment_data);
    info->height = jbig2_get_uint32(segment_data + 4);
    info->x = jbig2_get_uint32(segment_data + 8);
    info->y = jbig2_get_uint32(segment_data + 12);
    info->flags = segment_data[16];
    info->op = (Jbig2ComposeOp)(info->flags & 0x7);
}

static int
jbig2_parse_extension_segment(Jbig2Ctx *ctx, Jbig2Segment *segment, const uint8_t *segment_data)
{
    uint32_t type;
    bool reserved;
    bool necessary;

    if (segment->data_length < 4)
        return jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number, "segment too short");

    type = jbig2_get_uint32(segment_data);
    reserved = type & 0x20000000;

    necessary = type & 0x80000000;

    if (necessary && !reserved) {
        jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "extension segment is marked 'necessary' but not 'reserved' contrary to spec");
    }

    switch (type) {
    case 0x20000000:
        jbig2_error(ctx, JBIG2_SEVERITY_INFO, segment->number, "ignoring ASCII comment");
        break;
    case 0x20000002:
        jbig2_error(ctx, JBIG2_SEVERITY_INFO, segment->number, "ignoring UCS-2 comment");
        break;
    default:
        if (necessary) {
            return jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number, "unhandled necessary extension segment type 0x%08x", type);
        } else {
            jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "unhandled non-necessary extension segment, skipping");
        }
    }

    return 0;
}

static int
jbig2_parse_profile_segment(Jbig2Ctx *ctx, Jbig2Segment *segment, const uint8_t *segment_data)
{
    uint32_t profiles;
    uint32_t i;
    uint32_t profile;
    int index;
    const char *requirements;
    const char *generic_region;
    const char *refinement_region;
    const char *halftone_region;
    const char *numerical_data;

    if (segment->data_length < 4)
        return jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number, "Segment too short");
    index = 0;

    profiles = jbig2_get_uint32(&segment_data[index]);
    index += 4;

    for (i = 0; i < profiles; i++) {
        if (segment->data_length - index < 4)
            return jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number, "segment too short to store profile");

        profile = jbig2_get_uint32(&segment_data[index]);
        index += 4;

        switch (profile) {
        case 0x00000001:
            requirements = "All JBIG2 capabilities";
            generic_region = "No restriction";
            refinement_region = "No restriction";
            halftone_region = "No restriction";
            numerical_data = "No restriction";
            break;
        case 0x00000002:
            requirements = "Maximum compression";
            generic_region = "Arithmetic only; any template used";
            refinement_region = "No restriction";
            halftone_region = "No restriction";
            numerical_data = "Arithmetic only";
            break;
        case 0x00000003:
            requirements = "Medium complexity and medium compression";
            generic_region = "Arithmetic only; only 10-pixel and 13-pixel templates";
            refinement_region = "10-pixel template only";
            halftone_region = "No skip mask used";
            numerical_data = "Arithmetic only";
            break;
        case 0x00000004:
            requirements = "Low complexity with progressive lossless capability";
            generic_region = "MMR only";
            refinement_region = "10-pixel template only";
            halftone_region = "No skip mask used";
            numerical_data = "Huffman only";
            break;
        case 0x00000005:
            requirements = "Low complexity";
            generic_region = "MMR only";
            refinement_region = "Not available";
            halftone_region = "No skip mask used";
            numerical_data = "Huffman only";
            break;
        default:
            requirements = "Unknown";
            generic_region = "Unknown";
            refinement_region = "Unknown";
            halftone_region = "Unknown";
            numerical_data = "Unknown";
            break;
        }

        jbig2_error(ctx, JBIG2_SEVERITY_INFO, segment->number, "Supported profile: 0x%08x", profile);
        jbig2_error(ctx, JBIG2_SEVERITY_INFO, segment->number, "  Requirements: %s", requirements);
        jbig2_error(ctx, JBIG2_SEVERITY_INFO, segment->number, "  Generic region coding: %s", generic_region);
        jbig2_error(ctx, JBIG2_SEVERITY_INFO, segment->number, "  Refinement region coding: %s", refinement_region);
        jbig2_error(ctx, JBIG2_SEVERITY_INFO, segment->number, "  Halftone region coding: %s", halftone_region);
        jbig2_error(ctx, JBIG2_SEVERITY_INFO, segment->number, "  Numerical data: %s", numerical_data);
    }

    return 0;
}

int
jbig2_parse_segment(Jbig2Ctx *ctx, Jbig2Segment *segment, const uint8_t *segment_data)
{
    jbig2_error(ctx, JBIG2_SEVERITY_INFO, segment->number,
                "segment %d, flags=%x, type=%d, data_length=%ld", segment->number, segment->flags, segment->flags & 63, (long) segment->data_length);
    switch (segment->flags & 63) {
    case 0:
        return jbig2_symbol_dictionary(ctx, segment, segment_data);
    case 4:
    case 6:
    case 7:
        return jbig2_text_region(ctx, segment, segment_data);
    case 16:
        return jbig2_pattern_dictionary(ctx, segment, segment_data);
    case 20:
    case 22:
    case 23:
        return jbig2_halftone_region(ctx, segment, segment_data);
    case 36:
        return jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number, "unhandled segment type 'intermediate generic region' (NYI)");
    case 38:
    case 39:
        return jbig2_immediate_generic_region(ctx, segment, segment_data);
    case 40:
    case 42:
    case 43:
        return jbig2_refinement_region(ctx, segment, segment_data);
    case 48:
        return jbig2_page_info(ctx, segment, segment_data);
    case 49:
        return jbig2_end_of_page(ctx, segment, segment_data);
    case 50:
        return jbig2_end_of_stripe(ctx, segment, segment_data);
    case 51:
        ctx->state = JBIG2_FILE_EOF;
        jbig2_error(ctx, JBIG2_SEVERITY_INFO, segment->number, "end of file");
        break;
    case 52:
        return jbig2_parse_profile_segment(ctx, segment, segment_data);
    case 53:
        return jbig2_table(ctx, segment, segment_data);
    case 54:
        return jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "unhandled segment type 'color palette' (NYI)");
    case 62:
        return jbig2_parse_extension_segment(ctx, segment, segment_data);
    default:
        jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "unknown segment type %d", segment->flags & 63);
    }
    return 0;
}

#ifdef HAVE_CONFIG_H
#endif

#include <stddef.h>
#include <string.h>

#if defined(OUTPUT_PBM) || defined(DUMP_SYMDICT)
#include <stdio.h>
#endif

#ifdef OUTPUT_PBM
#endif

typedef struct {
    bool SDHUFF;
    bool SDREFAGG;
    uint32_t SDNUMINSYMS;
    Jbig2SymbolDict *SDINSYMS;
    uint32_t SDNUMNEWSYMS;
    uint32_t SDNUMEXSYMS;
    Jbig2HuffmanTable *SDHUFFDH;
    Jbig2HuffmanTable *SDHUFFDW;
    Jbig2HuffmanTable *SDHUFFBMSIZE;
    Jbig2HuffmanTable *SDHUFFAGGINST;
    int SDTEMPLATE;
    int8_t sdat[8];
    bool SDRTEMPLATE;
    int8_t sdrat[4];
} Jbig2SymbolDictParams;

#ifdef DUMP_SYMDICT
void
jbig2_dump_symbol_dict(Jbig2Ctx *ctx, Jbig2Segment *segment)
{
    Jbig2SymbolDict *dict = (Jbig2SymbolDict *) segment->result;
    uint32_t index;
    char filename[24];
    int code;

    if (dict == NULL)
        return;
    jbig2_error(ctx, JBIG2_SEVERITY_INFO, segment->number, "dumping symbol dictionary as %d individual png files", dict->n_symbols);
    for (index = 0; index < dict->n_symbols; index++) {
        snprintf(filename, sizeof(filename), "symbol_%02d-%04d.png", segment->number, index);
        jbig2_error(ctx, JBIG2_SEVERITY_DEBUG, segment->number, "dumping symbol %d/%d as '%s'", index, dict->n_symbols, filename);
#ifdef HAVE_LIBPNG
        code = jbig2_image_write_png_file(dict->glyphs[index], filename);
#else
        code = jbig2_image_write_pbm_file(dict->glyphs[index], filename);
#endif
        if (code < 0)
            return jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "failed to dump symbol %d/%d as '%s'", index, dict->n_symbols, filename);
    }
}
#endif

Jbig2SymbolDict *
jbig2_sd_new(Jbig2Ctx *ctx, uint32_t n_symbols)
{
    Jbig2SymbolDict *new_dict = NULL;

    new_dict = jbig2_new(ctx, Jbig2SymbolDict, 1);
    if (new_dict != NULL) {
        new_dict->glyphs = jbig2_new(ctx, Jbig2Image *, n_symbols);
        new_dict->n_symbols = n_symbols;
    } else {
        jbig2_error(ctx, JBIG2_SEVERITY_FATAL, JBIG2_UNKNOWN_SEGMENT_NUMBER, "failed to allocate new empty symbol dictionary");
        return NULL;
    }

    if (new_dict->glyphs != NULL) {
        memset(new_dict->glyphs, 0, n_symbols * sizeof(Jbig2Image *));
    } else if (new_dict->n_symbols > 0) {
        jbig2_error(ctx, JBIG2_SEVERITY_FATAL, JBIG2_UNKNOWN_SEGMENT_NUMBER, "failed to allocate glyphs for new empty symbol dictionary");
        jbig2_free(ctx->allocator, new_dict);
        return NULL;
    }

    return new_dict;
}

void
jbig2_sd_release(Jbig2Ctx *ctx, Jbig2SymbolDict *dict)
{
    uint32_t i;

    if (dict == NULL)
        return;
    if (dict->glyphs != NULL)
        for (i = 0; i < dict->n_symbols; i++)
            jbig2_image_release(ctx, dict->glyphs[i]);
    jbig2_free(ctx->allocator, dict->glyphs);
    jbig2_free(ctx->allocator, dict);
}

Jbig2Image *
jbig2_sd_glyph(Jbig2SymbolDict *dict, unsigned int id)
{
    if (dict == NULL)
        return NULL;
    return dict->glyphs[id];
}

uint32_t
jbig2_sd_count_referred(Jbig2Ctx *ctx, Jbig2Segment *segment)
{
    int index;
    Jbig2Segment *rsegment;
    uint32_t n_dicts = 0;

    for (index = 0; index < segment->referred_to_segment_count; index++) {
        rsegment = jbig2_find_segment(ctx, segment->referred_to_segments[index]);
        if (rsegment && ((rsegment->flags & 63) == 0) &&
            rsegment->result && (((Jbig2SymbolDict *) rsegment->result)->n_symbols > 0) && ((*((Jbig2SymbolDict *) rsegment->result)->glyphs) != NULL))
            n_dicts++;
    }

    return (n_dicts);
}

Jbig2SymbolDict **
jbig2_sd_list_referred(Jbig2Ctx *ctx, Jbig2Segment *segment)
{
    int index;
    Jbig2Segment *rsegment;
    Jbig2SymbolDict **dicts;
    uint32_t n_dicts = jbig2_sd_count_referred(ctx, segment);
    uint32_t dindex = 0;

    dicts = jbig2_new(ctx, Jbig2SymbolDict *, n_dicts);
    if (dicts == NULL) {
        jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number, "failed to allocate referred list of symbol dictionaries");
        return NULL;
    }

    for (index = 0; index < segment->referred_to_segment_count; index++) {
        rsegment = jbig2_find_segment(ctx, segment->referred_to_segments[index]);
        if (rsegment && ((rsegment->flags & 63) == 0) && rsegment->result &&
                (((Jbig2SymbolDict *) rsegment->result)->n_symbols > 0) && ((*((Jbig2SymbolDict *) rsegment->result)->glyphs) != NULL)) {

            dicts[dindex++] = (Jbig2SymbolDict *) rsegment->result;
        }
    }

    if (dindex != n_dicts) {

        jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number, "counted %d symbol dictionaries but built a list with %d.", n_dicts, dindex);
        jbig2_free(ctx->allocator, dicts);
        return NULL;
    }

    return (dicts);
}

Jbig2SymbolDict *
jbig2_sd_cat(Jbig2Ctx *ctx, uint32_t n_dicts, Jbig2SymbolDict **dicts)
{
    uint32_t i, j, k, symbols;
    Jbig2SymbolDict *new_dict = NULL;

    symbols = 0;
    for (i = 0; i < n_dicts; i++)
    {
        if (dicts[i]->n_symbols > UINT32_MAX - symbols)
        {
            jbig2_error(ctx, JBIG2_SEVERITY_FATAL, JBIG2_UNKNOWN_SEGMENT_NUMBER, "too many symbols in dicts to concat");
            return NULL;
        }
        symbols += dicts[i]->n_symbols;
    }

    new_dict = jbig2_sd_new(ctx, symbols);
    if (new_dict != NULL) {
        k = 0;
        for (i = 0; i < n_dicts; i++)
            for (j = 0; j < dicts[i]->n_symbols; j++)
                new_dict->glyphs[k++] = jbig2_image_reference(ctx, dicts[i]->glyphs[j]);
    } else {
        jbig2_error(ctx, JBIG2_SEVERITY_WARNING, JBIG2_UNKNOWN_SEGMENT_NUMBER, "failed to allocate new symbol dictionary");
    }

    return new_dict;
}

static Jbig2SymbolDict *
jbig2_decode_symbol_dict(Jbig2Ctx *ctx,
                         Jbig2Segment *segment,
                         const Jbig2SymbolDictParams *params, const byte *data, size_t size, Jbig2ArithCx *GB_stats, Jbig2ArithCx *GR_stats)
{
    Jbig2SymbolDict *SDNEWSYMS = NULL;
    Jbig2SymbolDict *SDEXSYMS = NULL;
    uint32_t HCHEIGHT;
    uint32_t NSYMSDECODED;
    uint32_t SYMWIDTH, TOTWIDTH;
    uint32_t HCFIRSTSYM;
    uint32_t *SDNEWSYMWIDTHS = NULL;
    uint8_t SBSYMCODELEN = 0;
    Jbig2WordStream *ws = NULL;
    Jbig2HuffmanState *hs = NULL;
    Jbig2ArithState *as = NULL;
    Jbig2ArithIntCtx *IADH = NULL;
    Jbig2ArithIntCtx *IADW = NULL;
    Jbig2ArithIntCtx *IAEX = NULL;
    Jbig2ArithIntCtx *IAAI = NULL;
    int code = 0;
    Jbig2SymbolDict **refagg_dicts = NULL;
    uint32_t i;
    Jbig2TextRegionParams tparams;
    Jbig2Image *image = NULL;
    Jbig2Image *glyph = NULL;
    uint32_t emptyruns = 0;

    memset(&tparams, 0, sizeof(tparams));

    HCHEIGHT = 0;
    NSYMSDECODED = 0;

    ws = jbig2_word_stream_buf_new(ctx, data, size);
    if (ws == NULL) {
        jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "failed to allocate word stream when decoding symbol dictionary");
        return NULL;
    }

    as = jbig2_arith_new(ctx, ws);
    if (as == NULL) {
        jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "failed to allocate arithmetic coding state when decoding symbol dictionary");
        jbig2_word_stream_buf_free(ctx, ws);
        return NULL;
    }

    for (SBSYMCODELEN = 0; ((uint64_t) 1 << SBSYMCODELEN) < ((uint64_t) params->SDNUMINSYMS + params->SDNUMNEWSYMS); SBSYMCODELEN++);

    if (params->SDHUFF) {
        jbig2_error(ctx, JBIG2_SEVERITY_DEBUG, segment->number, "huffman coded symbol dictionary");
        hs = jbig2_huffman_new(ctx, ws);
        tparams.SBHUFFRDX = jbig2_build_huffman_table(ctx, &jbig2_huffman_params_O);
        tparams.SBHUFFRDY = jbig2_build_huffman_table(ctx, &jbig2_huffman_params_O);
        tparams.SBHUFFRSIZE = jbig2_build_huffman_table(ctx, &jbig2_huffman_params_A);
        if (hs == NULL || tparams.SBHUFFRDX == NULL ||
                tparams.SBHUFFRDY == NULL || tparams.SBHUFFRSIZE == NULL) {
            jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "failed to allocate for symbol bitmap");
            goto cleanup;
        }

        if (!params->SDREFAGG) {
            SDNEWSYMWIDTHS = jbig2_new(ctx, uint32_t, params->SDNUMNEWSYMS);
            if (SDNEWSYMWIDTHS == NULL) {
                jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number, "failed to allocate symbol widths (%u)", params->SDNUMNEWSYMS);
                goto cleanup;
            }
        } else {
            tparams.SBHUFFFS = jbig2_build_huffman_table(ctx, &jbig2_huffman_params_F);
            tparams.SBHUFFDS = jbig2_build_huffman_table(ctx, &jbig2_huffman_params_H);
            tparams.SBHUFFDT = jbig2_build_huffman_table(ctx, &jbig2_huffman_params_K);
            tparams.SBHUFFRDW = jbig2_build_huffman_table(ctx, &jbig2_huffman_params_O);
            tparams.SBHUFFRDH = jbig2_build_huffman_table(ctx, &jbig2_huffman_params_O);
            if (tparams.SBHUFFFS == NULL || tparams.SBHUFFDS == NULL ||
                    tparams.SBHUFFDT == NULL || tparams.SBHUFFRDW == NULL ||
                    tparams.SBHUFFRDH == NULL) {
                jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "out of memory creating text region huffman decoder entries");
                goto cleanup;
            }
        }
    } else {
        IADH = jbig2_arith_int_ctx_new(ctx);
        IADW = jbig2_arith_int_ctx_new(ctx);
        IAEX = jbig2_arith_int_ctx_new(ctx);
        IAAI = jbig2_arith_int_ctx_new(ctx);
        if (IADH == NULL || IADW == NULL || IAEX == NULL || IAAI == NULL) {
            jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "failed to allocate symbol bitmap");
            goto cleanup;
        }
        tparams.IAID = jbig2_arith_iaid_ctx_new(ctx, SBSYMCODELEN);
        tparams.IARDX = jbig2_arith_int_ctx_new(ctx);
        tparams.IARDY = jbig2_arith_int_ctx_new(ctx);
        if (tparams.IAID == NULL || tparams.IARDX == NULL ||
                tparams.IARDY == NULL) {
            jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "failed to allocate text region arithmetic decoder contexts");
            goto cleanup;
        }
        if (params->SDREFAGG) {

            tparams.IADT = jbig2_arith_int_ctx_new(ctx);
            tparams.IAFS = jbig2_arith_int_ctx_new(ctx);
            tparams.IADS = jbig2_arith_int_ctx_new(ctx);
            tparams.IAIT = jbig2_arith_int_ctx_new(ctx);

            tparams.IARI = jbig2_arith_int_ctx_new(ctx);
            tparams.IARDW = jbig2_arith_int_ctx_new(ctx);
            tparams.IARDH = jbig2_arith_int_ctx_new(ctx);
            if (tparams.IADT == NULL || tparams.IAFS == NULL ||
                    tparams.IADS == NULL || tparams.IAIT == NULL ||
                    tparams.IARI == NULL || tparams.IARDW == NULL ||
                    tparams.IARDH == NULL) {
                jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "failed to allocate text region arith decoder contexts");
                goto cleanup;
            }
        }
    }
    tparams.SBHUFF = params->SDHUFF;
    tparams.SBREFINE = 1;
    tparams.SBSTRIPS = 1;
    tparams.SBDEFPIXEL = 0;
    tparams.SBCOMBOP = JBIG2_COMPOSE_OR;
    tparams.TRANSPOSED = 0;
    tparams.REFCORNER = JBIG2_CORNER_TOPLEFT;
    tparams.SBDSOFFSET = 0;
    tparams.SBRTEMPLATE = params->SDRTEMPLATE;

    SDNEWSYMS = jbig2_sd_new(ctx, params->SDNUMNEWSYMS);
    if (SDNEWSYMS == NULL) {
        jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "failed to allocate new symbols (%u)", params->SDNUMNEWSYMS);
        goto cleanup;
    }

    refagg_dicts = jbig2_new(ctx, Jbig2SymbolDict *, 2);
    if (refagg_dicts == NULL) {
        code = jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number, "Out of memory allocating dictionary array");
        goto cleanup;
    }
    refagg_dicts[0] = jbig2_sd_new(ctx, params->SDNUMINSYMS);
    if (refagg_dicts[0] == NULL) {
        code = jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "out of memory allocating symbol dictionary");
        goto cleanup;
    }
    for (i = 0; i < params->SDNUMINSYMS; i++) {
        refagg_dicts[0]->glyphs[i] = jbig2_image_reference(ctx, params->SDINSYMS->glyphs[i]);
    }
    refagg_dicts[1] = SDNEWSYMS;

    while (NSYMSDECODED < params->SDNUMNEWSYMS) {
        int32_t HCDH, DW;

        if (params->SDHUFF) {
            HCDH = jbig2_huffman_get(hs, params->SDHUFFDH, &code);
        } else {
            code = jbig2_arith_int_decode(ctx, IADH, as, &HCDH);
        }
        if (code < 0) {
            jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "failed to decode height class delta");
            goto cleanup;
        }
        if (code > 0) {
            jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "OOB decoding height class delta");
            goto cleanup;
        }

        HCHEIGHT = HCHEIGHT + HCDH;
        SYMWIDTH = 0;
        TOTWIDTH = 0;
        HCFIRSTSYM = NSYMSDECODED;

        if ((int32_t) HCHEIGHT < 0) {
            code = jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number, "invalid HCHEIGHT value");
            goto cleanup;
        }
#ifdef JBIG2_DEBUG
        jbig2_error(ctx, JBIG2_SEVERITY_DEBUG, segment->number, "HCHEIGHT = %d", HCHEIGHT);
#endif
        jbig2_error(ctx, JBIG2_SEVERITY_DEBUG, segment->number, "decoding height class %d with %d syms decoded", HCHEIGHT, NSYMSDECODED);

        for (;;) {

            if (params->SDHUFF) {
                DW = jbig2_huffman_get(hs, params->SDHUFFDW, &code);
            } else {
                code = jbig2_arith_int_decode(ctx, IADW, as, &DW);
            }
            if (code < 0)
            {
                jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "failed to decode DW");
                goto cleanup;
            }

            if (code > 0) {
                jbig2_error(ctx, JBIG2_SEVERITY_DEBUG, segment->number, "OOB when decoding DW signals end of height class %d", HCHEIGHT);
                break;
            }

            if (NSYMSDECODED >= params->SDNUMNEWSYMS) {
                jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "no OOB signaling end of height class %d, continuing", HCHEIGHT);
                break;
            }

            if (DW < 0 && SYMWIDTH < (uint32_t) -DW) {
                code = jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number, "DW value (%d) would make SYMWIDTH (%u) negative at symbol %u", DW, SYMWIDTH, NSYMSDECODED + 1);
                goto cleanup;
            }
            if (DW > 0 && (uint32_t) DW > UINT32_MAX - SYMWIDTH) {
                code = jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number, "DW value (%d) would make SYMWIDTH (%u) too large at symbol %u", DW, SYMWIDTH, NSYMSDECODED + 1);
                goto cleanup;
            }

            SYMWIDTH = SYMWIDTH + DW;
            if (SYMWIDTH > UINT32_MAX - TOTWIDTH) {
                code = jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number, "SYMWIDTH value (%u) would make TOTWIDTH (%u) too large at symbol %u", SYMWIDTH, TOTWIDTH, NSYMSDECODED + 1);
                goto cleanup;
            }

            TOTWIDTH = TOTWIDTH + SYMWIDTH;
#ifdef JBIG2_DEBUG
            jbig2_error(ctx, JBIG2_SEVERITY_DEBUG, segment->number, "SYMWIDTH = %u TOTWIDTH = %u", SYMWIDTH, TOTWIDTH);
#endif

            if (!params->SDHUFF || params->SDREFAGG) {
#ifdef JBIG2_DEBUG
                jbig2_error(ctx, JBIG2_SEVERITY_DEBUG, segment->number, "SDHUFF = %d; SDREFAGG = %d", params->SDHUFF, params->SDREFAGG);
#endif

                if (!params->SDREFAGG) {
                    Jbig2GenericRegionParams region_params;
                    int sdat_bytes;

                    region_params.MMR = 0;
                    region_params.GBTEMPLATE = params->SDTEMPLATE;
                    region_params.TPGDON = 0;
                    region_params.USESKIP = 0;
                    sdat_bytes = params->SDTEMPLATE == 0 ? 8 : 2;
                    memcpy(region_params.gbat, params->sdat, sdat_bytes);

                    image = jbig2_image_new(ctx, SYMWIDTH, HCHEIGHT);
                    if (image == NULL) {
                        code = jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "failed to allocate image");
                        goto cleanup;
                    }

                    code = jbig2_decode_generic_region(ctx, segment, &region_params, as, image, GB_stats);
                    if (code < 0) {
                        jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "failed to decode generic region");
                        goto cleanup;
                    }

                    SDNEWSYMS->glyphs[NSYMSDECODED] = image;
                    image = NULL;
                } else {

                    uint32_t REFAGGNINST;

                    if (params->SDHUFF) {
                        REFAGGNINST = jbig2_huffman_get(hs, params->SDHUFFAGGINST, &code);
                    } else {
                        code = jbig2_arith_int_decode(ctx, IAAI, as, (int32_t *) &REFAGGNINST);
                    }
                    if (code < 0) {
                        code = jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "failed to decode number of symbols in aggregate glyph");
                        goto cleanup;
                    }
                    if (code > 0) {
                        code = jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number, "OOB in number of symbols in aggregate glyph");
                        goto cleanup;
                    }
                    if ((int32_t) REFAGGNINST <= 0) {
                        code = jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number, "invalid number of symbols in aggregate glyph");
                        goto cleanup;
                    }

                    jbig2_error(ctx, JBIG2_SEVERITY_DEBUG, segment->number, "aggregate symbol coding (%d instances)", REFAGGNINST);

                    if (REFAGGNINST > 1) {
                        tparams.SBNUMINSTANCES = REFAGGNINST;

                        image = jbig2_image_new(ctx, SYMWIDTH, HCHEIGHT);
                        if (image == NULL) {
                            code = jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "failed to allocate symbol image");
                            goto cleanup;
                        }

                        code = jbig2_decode_text_region(ctx, segment, &tparams, (const Jbig2SymbolDict * const *)refagg_dicts,
                                                        2, image, GR_stats, as, ws);
                        if (code < 0) {
                            jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "failed to decode text region");
                            goto cleanup;
                        }

                        SDNEWSYMS->glyphs[NSYMSDECODED] = image;
                        image = NULL;
                    } else {

                        Jbig2RefinementRegionParams rparams;
                        uint32_t ID;
                        int32_t RDX, RDY;
                        size_t BMSIZE = 0;
                        uint32_t ninsyms = params->SDNUMINSYMS;
                        int code1 = 0;
                        int code2 = 0;
                        int code3 = 0;
                        int code4 = 0;
                        int code5 = 0;

                        if (params->SDHUFF) {
                            ID = jbig2_huffman_get_bits(hs, SBSYMCODELEN, &code1);
                            RDX = jbig2_huffman_get(hs, tparams.SBHUFFRDX, &code2);
                            RDY = jbig2_huffman_get(hs, tparams.SBHUFFRDY, &code3);
                            BMSIZE = jbig2_huffman_get(hs, tparams.SBHUFFRSIZE, &code4);
                            code5 = jbig2_huffman_skip(hs);
                        } else {
                            code1 = jbig2_arith_iaid_decode(ctx, tparams.IAID, as, (int32_t *) &ID);
                            code2 = jbig2_arith_int_decode(ctx, tparams.IARDX, as, &RDX);
                            code3 = jbig2_arith_int_decode(ctx, tparams.IARDY, as, &RDY);
                        }

                        if (code1 < 0 || code2 < 0 || code3 < 0 || code4 < 0 || code5 < 0) {
                            code = jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "failed to decode data");
                            goto cleanup;
                        }
                        if (code1 > 0 || code2 > 0 || code3 > 0 || code4 > 0 || code5 > 0) {
                            code = jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number, "OOB in single refinement/aggregate coded symbol data");
                            goto cleanup;
                        }

                        if (ID >= ninsyms + NSYMSDECODED) {
                            code = jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number, "refinement references unknown symbol %d", ID);
                            goto cleanup;
                        }

                        jbig2_error(ctx, JBIG2_SEVERITY_DEBUG, segment->number,
                                    "symbol is a refinement of ID %d with the refinement applied at (%d,%d)", ID, RDX, RDY);

                        image = jbig2_image_new(ctx, SYMWIDTH, HCHEIGHT);
                        if (image == NULL) {
                            code = jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "failed to allocate symbol image");
                            goto cleanup;
                        }

                        rparams.GRTEMPLATE = params->SDRTEMPLATE;
                        rparams.GRREFERENCE = (ID < ninsyms) ? params->SDINSYMS->glyphs[ID] : SDNEWSYMS->glyphs[ID - ninsyms];

                        if (rparams.GRREFERENCE == NULL) {
                            code = jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number, "missing glyph %d/%d", ID, ninsyms);
                            goto cleanup;
                        }
                        rparams.GRREFERENCEDX = RDX;
                        rparams.GRREFERENCEDY = RDY;
                        rparams.TPGRON = 0;
                        memcpy(rparams.grat, params->sdrat, 4);
                        code = jbig2_decode_refinement_region(ctx, segment, &rparams, as, image, GR_stats);
                        if (code < 0) {
                            jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "failed to decode refinement region");
                            goto cleanup;
                        }

                        SDNEWSYMS->glyphs[NSYMSDECODED] = image;
                        image = NULL;

                        if (params->SDHUFF) {
                            if (BMSIZE == 0)
                                BMSIZE = (size_t) SDNEWSYMS->glyphs[NSYMSDECODED]->height *
                                    SDNEWSYMS->glyphs[NSYMSDECODED]->stride;
                            code = jbig2_huffman_advance(hs, BMSIZE);
                            if (code < 0) {
                                jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "failed to advance after huffman decoding in refinement region");
                                goto cleanup;
                            }
                        }
                    }
                }

#ifdef OUTPUT_PBM
                {
                    char name[64];
                    FILE *out;
                    int code;

                    snprintf(name, 64, "sd.%04d.%04d.pbm", segment->number, NSYMSDECODED);
                    out = fopen(name, "wb");
                    code = jbig2_image_write_pbm(SDNEWSYMS->glyphs[NSYMSDECODED], out);
                    fclose(out);
                    if (code < 0) {
                        jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "failed to write glyph");
                        goto cleanup;
                    }
                    jbig2_error(ctx, JBIG2_SEVERITY_DEBUG, segment->number, "writing out glyph as '%s' ...", name);
                }
#endif

            }

            if (params->SDHUFF && !params->SDREFAGG) {
                SDNEWSYMWIDTHS[NSYMSDECODED] = SYMWIDTH;
            }

            NSYMSDECODED = NSYMSDECODED + 1;

            jbig2_error(ctx, JBIG2_SEVERITY_DEBUG, segment->number, "decoded symbol %u of %u (%ux%u)", NSYMSDECODED, params->SDNUMNEWSYMS, SYMWIDTH, HCHEIGHT);

        }

        if (params->SDHUFF && !params->SDREFAGG) {

            size_t BMSIZE;
            uint32_t j;
            int x;

            BMSIZE = jbig2_huffman_get(hs, params->SDHUFFBMSIZE, &code);
            if (code < 0) {
                jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "error decoding size of collective bitmap");
                goto cleanup;
            }
            if (code > 0) {
                jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number, "OOB obtained when decoding size of collective bitmap");
                goto cleanup;
            }

            code = jbig2_huffman_skip(hs);
            if (code < 0) {
                jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "failed to skip to next byte when decoding collective bitmap");
            }

            image = jbig2_image_new(ctx, TOTWIDTH, HCHEIGHT);
            if (image == NULL) {
                jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "failed to allocate collective bitmap image");
                goto cleanup;
            }

            if (BMSIZE == 0) {

                const byte *src = data + jbig2_huffman_offset(hs);
                const int stride = (image->width >> 3) + ((image->width & 7) ? 1 : 0);
                byte *dst = image->data;

                if (size < jbig2_huffman_offset(hs) || (size - jbig2_huffman_offset(hs) < (size_t) image->height * stride) || (size < jbig2_huffman_offset(hs))) {
                    jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number, "not enough data for decoding uncompressed (%d/%li)", image->height * stride,
                                (long) (size - jbig2_huffman_offset(hs)));
                    goto cleanup;
                }

                BMSIZE = (size_t) image->height * stride;
                jbig2_error(ctx, JBIG2_SEVERITY_DEBUG, segment->number,
                            "reading %dx%d uncompressed bitmap for %d symbols (%li bytes)", image->width, image->height, NSYMSDECODED - HCFIRSTSYM, (long) BMSIZE);

                for (j = 0; j < image->height; j++) {
                    memcpy(dst, src, stride);
                    dst += image->stride;
                    src += stride;
                }
            } else {
                Jbig2GenericRegionParams rparams;

                if (size < jbig2_huffman_offset(hs) || size < BMSIZE || size - jbig2_huffman_offset(hs) < BMSIZE) {
                    jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number, "not enough data for decoding (%li/%li)", (long) BMSIZE, (long) (size - jbig2_huffman_offset(hs)));
                    goto cleanup;
                }

                jbig2_error(ctx, JBIG2_SEVERITY_DEBUG, segment->number,
                            "reading %dx%d collective bitmap for %d symbols (%li bytes)", image->width, image->height, NSYMSDECODED - HCFIRSTSYM, (long) BMSIZE);

                rparams.MMR = 1;
                code = jbig2_decode_generic_mmr(ctx, segment, &rparams, data + jbig2_huffman_offset(hs), BMSIZE, image);
                if (code) {
                    jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "failed to decode MMR-coded generic region");
                    goto cleanup;
                }
            }

            code = jbig2_huffman_advance(hs, BMSIZE);
            if (code < 0) {
                jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "failed to advance after huffman decoding MMR bitmap image");
                goto cleanup;
            }

            x = 0;
            for (j = HCFIRSTSYM; j < NSYMSDECODED; j++) {
                glyph = jbig2_image_new(ctx, SDNEWSYMWIDTHS[j], HCHEIGHT);
                if (glyph == NULL) {
                    jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "failed to copy the collective bitmap into symbol dictionary");
                    goto cleanup;
                }
                code = jbig2_image_compose(ctx, glyph, image, -x, 0, JBIG2_COMPOSE_REPLACE);
                if (code) {
                    jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "failed to compose image into glyph");
                    goto cleanup;
                }
                x += SDNEWSYMWIDTHS[j];
                SDNEWSYMS->glyphs[j] = glyph;
                glyph = NULL;
            }
            jbig2_image_release(ctx, image);
            image = NULL;
        }

    }

    SDEXSYMS = jbig2_sd_new(ctx, params->SDNUMEXSYMS);
    if (SDEXSYMS == NULL) {
        jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "failed to allocate symbols exported from symbols dictionary");
        goto cleanup;
    } else {
        uint32_t i = 0;
        uint32_t j = 0;
        uint32_t k;
        int exflag = 0;
        uint32_t limit = params->SDNUMINSYMS + params->SDNUMNEWSYMS;
        uint32_t EXRUNLENGTH;

        while (i < limit) {
            if (params->SDHUFF)
                EXRUNLENGTH = jbig2_huffman_get(hs, tparams.SBHUFFRSIZE, &code);
            else
                code = jbig2_arith_int_decode(ctx, IAEX, as, (int32_t *) &EXRUNLENGTH);
            if (code < 0) {
                jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "failed to decode runlength for exported symbols");

                jbig2_sd_release(ctx, SDEXSYMS);
                SDEXSYMS = NULL;
                break;
            }
            if (code > 0) {
                jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number, "OOB when decoding runlength for exported symbols");

                jbig2_sd_release(ctx, SDEXSYMS);
                SDEXSYMS = NULL;
                break;
            }

            if (EXRUNLENGTH <= 0 && ++emptyruns == 1000) {
                jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number, "runlength too small in export symbol table (%u == 0 i = %u limit = %u)", EXRUNLENGTH, i, limit);

                jbig2_sd_release(ctx, SDEXSYMS);
                SDEXSYMS = NULL;
                break;
            } else if (EXRUNLENGTH > 0) {
                emptyruns = 0;
            }

            if (EXRUNLENGTH > limit - i) {
                jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "exporting more symbols than available (%u > %u), capping", i + EXRUNLENGTH, limit);
                EXRUNLENGTH = limit - i;
            }
            if (exflag && j + EXRUNLENGTH > params->SDNUMEXSYMS) {
                jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "exporting more symbols than may be exported (%u > %u), capping", j + EXRUNLENGTH, params->SDNUMEXSYMS);
                EXRUNLENGTH = params->SDNUMEXSYMS - j;
            }

            for (k = 0; k < EXRUNLENGTH; k++) {
                if (exflag) {
                    Jbig2Image *img;
                    if (i < params->SDNUMINSYMS) {
                        img = params->SDINSYMS->glyphs[i];
                    } else {
                        img = SDNEWSYMS->glyphs[i - params->SDNUMINSYMS];
                    }
                    SDEXSYMS->glyphs[j++] = jbig2_image_reference(ctx, img);
                }
                i++;
            }
            exflag = !exflag;
        }
    }

cleanup:
    jbig2_image_release(ctx, glyph);
    jbig2_image_release(ctx, image);
    if (refagg_dicts != NULL) {
        if (refagg_dicts[0] != NULL)
            jbig2_sd_release(ctx, refagg_dicts[0]);

        jbig2_free(ctx->allocator, refagg_dicts);
    }
    jbig2_sd_release(ctx, SDNEWSYMS);
    if (params->SDHUFF) {
        jbig2_release_huffman_table(ctx, tparams.SBHUFFRSIZE);
        jbig2_release_huffman_table(ctx, tparams.SBHUFFRDY);
        jbig2_release_huffman_table(ctx, tparams.SBHUFFRDX);
        jbig2_release_huffman_table(ctx, tparams.SBHUFFRDH);
        jbig2_release_huffman_table(ctx, tparams.SBHUFFRDW);
        jbig2_release_huffman_table(ctx, tparams.SBHUFFDT);
        jbig2_release_huffman_table(ctx, tparams.SBHUFFDS);
        jbig2_release_huffman_table(ctx, tparams.SBHUFFFS);
        if (!params->SDREFAGG) {
            jbig2_free(ctx->allocator, SDNEWSYMWIDTHS);
        }
        jbig2_huffman_free(ctx, hs);
    } else {
        jbig2_arith_int_ctx_free(ctx, tparams.IARDY);
        jbig2_arith_int_ctx_free(ctx, tparams.IARDX);
        jbig2_arith_int_ctx_free(ctx, tparams.IARDH);
        jbig2_arith_int_ctx_free(ctx, tparams.IARDW);
        jbig2_arith_int_ctx_free(ctx, tparams.IARI);
        jbig2_arith_iaid_ctx_free(ctx, tparams.IAID);
        jbig2_arith_int_ctx_free(ctx, tparams.IAIT);
        jbig2_arith_int_ctx_free(ctx, tparams.IADS);
        jbig2_arith_int_ctx_free(ctx, tparams.IAFS);
        jbig2_arith_int_ctx_free(ctx, tparams.IADT);
        jbig2_arith_int_ctx_free(ctx, IAAI);
        jbig2_arith_int_ctx_free(ctx, IAEX);
        jbig2_arith_int_ctx_free(ctx, IADW);
        jbig2_arith_int_ctx_free(ctx, IADH);
    }
    jbig2_free(ctx->allocator, as);
    jbig2_word_stream_buf_free(ctx, ws);

    return SDEXSYMS;
}

int
jbig2_symbol_dictionary(Jbig2Ctx *ctx, Jbig2Segment *segment, const byte *segment_data)
{
    Jbig2SymbolDictParams params;
    uint16_t flags;
    uint32_t sdat_bytes;
    uint32_t offset;
    Jbig2ArithCx *GB_stats = NULL;
    Jbig2ArithCx *GR_stats = NULL;
    int table_index = 0;
    const Jbig2HuffmanParams *huffman_params;

    params.SDHUFF = 0;

    if (segment->data_length < 10)
        goto too_short;

    flags = jbig2_get_uint16(segment_data);

    memset(&params, 0, sizeof(Jbig2SymbolDictParams));

    params.SDHUFF = flags & 1;
    params.SDREFAGG = (flags >> 1) & 1;
    params.SDTEMPLATE = (flags >> 10) & 3;
    params.SDRTEMPLATE = (flags >> 12) & 1;

    if (params.SDHUFF) {
        switch ((flags & 0x000c) >> 2) {
        case 0:
            params.SDHUFFDH = jbig2_build_huffman_table(ctx, &jbig2_huffman_params_D);
            break;
        case 1:
            params.SDHUFFDH = jbig2_build_huffman_table(ctx, &jbig2_huffman_params_E);
            break;
        case 3:
            huffman_params = jbig2_find_table(ctx, segment, table_index);
            if (huffman_params == NULL) {
                jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "custom DH huffman table not found (%d)", table_index);
                goto cleanup;
            }
            params.SDHUFFDH = jbig2_build_huffman_table(ctx, huffman_params);
            ++table_index;
            break;
        case 2:
        default:
            return jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number, "symbol dictionary specified invalid huffman table");
        }
        if (params.SDHUFFDH == NULL) {
            jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "failed to allocate DH huffman table");
            goto cleanup;
        }

        switch ((flags & 0x0030) >> 4) {
        case 0:
            params.SDHUFFDW = jbig2_build_huffman_table(ctx, &jbig2_huffman_params_B);
            break;
        case 1:
            params.SDHUFFDW = jbig2_build_huffman_table(ctx, &jbig2_huffman_params_C);
            break;
        case 3:
            huffman_params = jbig2_find_table(ctx, segment, table_index);
            if (huffman_params == NULL) {
                jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "custom DW huffman table not found (%d)", table_index);
                goto cleanup;
            }
            params.SDHUFFDW = jbig2_build_huffman_table(ctx, huffman_params);
            ++table_index;
            break;
        case 2:
        default:
            jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number, "symbol dictionary specified invalid huffman table");
            goto cleanup;
        }
        if (params.SDHUFFDW == NULL) {
            jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "failed to allocate DW huffman table");
            goto cleanup;
        }

        if (flags & 0x0040) {

            huffman_params = jbig2_find_table(ctx, segment, table_index);
            if (huffman_params == NULL) {
                jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "custom BMSIZE huffman table not found (%d)", table_index);
                goto cleanup;
            }
            params.SDHUFFBMSIZE = jbig2_build_huffman_table(ctx, huffman_params);
            ++table_index;
        } else {

            params.SDHUFFBMSIZE = jbig2_build_huffman_table(ctx, &jbig2_huffman_params_A);
        }
        if (params.SDHUFFBMSIZE == NULL) {
            jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "failed to allocate BMSIZE huffman table");
            goto cleanup;
        }

        if (flags & 0x0080) {

            huffman_params = jbig2_find_table(ctx, segment, table_index);
            if (huffman_params == NULL) {
                jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "custom REFAGG huffman table not found (%d)", table_index);
                goto cleanup;
            }
            params.SDHUFFAGGINST = jbig2_build_huffman_table(ctx, huffman_params);
            ++table_index;
        } else {

            params.SDHUFFAGGINST = jbig2_build_huffman_table(ctx, &jbig2_huffman_params_A);
        }
        if (params.SDHUFFAGGINST == NULL) {
            jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "failed to allocate REFAGG huffman table");
            goto cleanup;
        }
    }

    if (!params.SDHUFF) {
        if (flags & 0x000c) {
            jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number, "SDHUFF is zero, but contrary to spec SDHUFFDH is not.");
            goto cleanup;
        }
        if (flags & 0x0030) {
            jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number, "SDHUFF is zero, but contrary to spec SDHUFFDW is not.");
            goto cleanup;
        }
    }

    sdat_bytes = params.SDHUFF ? 0 : params.SDTEMPLATE == 0 ? 8 : 2;
    memcpy(params.sdat, segment_data + 2, sdat_bytes);
    offset = 2 + sdat_bytes;

    if (params.SDREFAGG && !params.SDRTEMPLATE) {
        if (offset + 4 > segment->data_length)
            goto too_short;
        memcpy(params.sdrat, segment_data + offset, 4);
        offset += 4;
    }

    if (offset + 8 > segment->data_length)
        goto too_short;

    params.SDNUMEXSYMS = jbig2_get_uint32(segment_data + offset);

    params.SDNUMNEWSYMS = jbig2_get_uint32(segment_data + offset + 4);
    offset += 8;

    jbig2_error(ctx, JBIG2_SEVERITY_INFO, segment->number,
                "symbol dictionary, flags=%04x, %u exported syms, %u new syms", flags, params.SDNUMEXSYMS, params.SDNUMNEWSYMS);

    {
        uint32_t n_dicts = jbig2_sd_count_referred(ctx, segment);
        Jbig2SymbolDict **dicts = NULL;

        if (n_dicts > 0) {
            dicts = jbig2_sd_list_referred(ctx, segment);
            if (dicts == NULL) {
                jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "failed to allocate dicts in symbol dictionary");
                goto cleanup;
            }
            params.SDINSYMS = jbig2_sd_cat(ctx, n_dicts, dicts);
            if (params.SDINSYMS == NULL) {
                jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "failed to allocate symbol array in symbol dictionary");
                jbig2_free(ctx->allocator, dicts);
                goto cleanup;
            }
            jbig2_free(ctx->allocator, dicts);
        }
        if (params.SDINSYMS != NULL) {
            params.SDNUMINSYMS = params.SDINSYMS->n_symbols;
        }
    }

    if (flags & 0x0100) {
        jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "segment marks bitmap coding context as used (NYI)");
        goto cleanup;
    } else {
        int stats_size = params.SDTEMPLATE == 0 ? 65536 : params.SDTEMPLATE == 1 ? 8192 : 1024;

        GB_stats = jbig2_new(ctx, Jbig2ArithCx, stats_size);
        if (GB_stats == NULL) {
            jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number, "failed to allocate arithmetic decoder states for generic regions");
            goto cleanup;
        }
        memset(GB_stats, 0, sizeof (Jbig2ArithCx) * stats_size);

        stats_size = params.SDRTEMPLATE ? 1 << 10 : 1 << 13;
        GR_stats = jbig2_new(ctx, Jbig2ArithCx, stats_size);
        if (GR_stats == NULL) {
            jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number, "failed to allocate arithmetic decoder states for generic refinement regions");
            jbig2_free(ctx->allocator, GB_stats);
            goto cleanup;
        }
        memset(GR_stats, 0, sizeof (Jbig2ArithCx) * stats_size);
    }

    segment->result = (void *)jbig2_decode_symbol_dict(ctx, segment, &params, segment_data + offset, segment->data_length - offset, GB_stats, GR_stats);
#ifdef DUMP_SYMDICT
    if (segment->result)
        jbig2_dump_symbol_dict(ctx, segment);
#endif

    if (flags & 0x0200) {

        jbig2_free(ctx->allocator, GR_stats);
        jbig2_free(ctx->allocator, GB_stats);
        jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "segment marks bitmap coding context as retained (NYI)");
        goto cleanup;
    } else {
        jbig2_free(ctx->allocator, GR_stats);
        jbig2_free(ctx->allocator, GB_stats);
    }

cleanup:
    if (params.SDHUFF) {
        jbig2_release_huffman_table(ctx, params.SDHUFFDH);
        jbig2_release_huffman_table(ctx, params.SDHUFFDW);
        jbig2_release_huffman_table(ctx, params.SDHUFFBMSIZE);
        jbig2_release_huffman_table(ctx, params.SDHUFFAGGINST);
    }
    jbig2_sd_release(ctx, params.SDINSYMS);

    return (segment->result != NULL) ? 0 : -1;

too_short:
    if (params.SDHUFF) {
        jbig2_release_huffman_table(ctx, params.SDHUFFDH);
        jbig2_release_huffman_table(ctx, params.SDHUFFDW);
        jbig2_release_huffman_table(ctx, params.SDHUFFBMSIZE);
        jbig2_release_huffman_table(ctx, params.SDHUFFAGGINST);
    }
    return jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number, "segment too short");
}

#ifdef HAVE_CONFIG_H
#endif

#include <stddef.h>
#include <string.h>

int
jbig2_decode_text_region(Jbig2Ctx *ctx, Jbig2Segment *segment,
                         const Jbig2TextRegionParams *params,
                         const Jbig2SymbolDict *const *dicts, const uint32_t n_dicts,
                         Jbig2Image *image, Jbig2ArithCx *GR_stats, Jbig2ArithState *as, Jbig2WordStream *ws)
{

    uint32_t NINSTANCES;
    uint32_t ID;
    int32_t STRIPT;
    int32_t FIRSTS;
    int32_t DT;
    int32_t DFS;
    int32_t IDS;
    int32_t CURS;
    int32_t CURT;
    int S, T;
    int x, y;
    bool first_symbol;
    uint32_t index, SBNUMSYMS;
    Jbig2Image *IB = NULL;
    Jbig2Image *IBO = NULL;
    Jbig2Image *refimage = NULL;
    Jbig2HuffmanState *hs = NULL;
    Jbig2HuffmanTable *SBSYMCODES = NULL;
    int code = 0;
    int RI;

    SBNUMSYMS = 0;
    for (index = 0; index < n_dicts; index++) {
        SBNUMSYMS += dicts[index]->n_symbols;
    }
    jbig2_error(ctx, JBIG2_SEVERITY_DEBUG, segment->number, "symbol list contains %d glyphs in %d dictionaries", SBNUMSYMS, n_dicts);

    if (params->SBHUFF) {
        Jbig2HuffmanTable *runcodes = NULL;
        Jbig2HuffmanParams runcodeparams;
        Jbig2HuffmanLine runcodelengths[35];
        Jbig2HuffmanLine *symcodelengths = NULL;
        Jbig2HuffmanParams symcodeparams;
        int err, len, range, r;

        jbig2_error(ctx, JBIG2_SEVERITY_DEBUG, segment->number, "huffman coded text region");
        hs = jbig2_huffman_new(ctx, ws);
        if (hs == NULL)
            return jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "failed to allocate text region");

        for (index = 0; index < 35; index++) {
            runcodelengths[index].PREFLEN = jbig2_huffman_get_bits(hs, 4, &code);
            if (code < 0) {
                jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "failed to read huffman runcode lengths");
                goto cleanup1;
            }
            if (code > 0) {
                jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number, "OOB decoding huffman runcode lengths");
                goto cleanup1;
            }
            runcodelengths[index].RANGELEN = 0;
            runcodelengths[index].RANGELOW = index;
            jbig2_error(ctx, JBIG2_SEVERITY_DEBUG, segment->number, "  read runcode%d length %d", index, runcodelengths[index].PREFLEN);
        }
        runcodeparams.HTOOB = 0;
        runcodeparams.lines = runcodelengths;
        runcodeparams.n_lines = 35;
        runcodes = jbig2_build_huffman_table(ctx, &runcodeparams);
        if (runcodes == NULL) {
            code = jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "error constructing symbol ID runcode table");
            goto cleanup1;
        }

        symcodelengths = jbig2_new(ctx, Jbig2HuffmanLine, SBNUMSYMS);
        if (symcodelengths == NULL) {
            code = jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number, "failed to allocate memory when reading symbol ID huffman table");
            goto cleanup1;
        }
        index = 0;
        while (index < SBNUMSYMS) {
            code = jbig2_huffman_get(hs, runcodes, &err);
            if (err < 0) {
                code = jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "error reading symbol ID huffman table");
                goto cleanup1;
            }
            if (err > 0) {
                code = jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number, "OOB decoding symbol ID huffman table");
                goto cleanup1;
            }
            if (code < 0 || code >= 35) {
                code = jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number, "symbol ID huffman table out of range");
                goto cleanup1;
            }

            if (code < 32) {
                len = code;
                range = 1;
            } else {
                if (code == 32) {
                    if (index < 1) {
                        code = jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number, "error decoding symbol ID table: run length with no antecedent");
                        goto cleanup1;
                    }
                    len = symcodelengths[index - 1].PREFLEN;
                } else {
                    len = 0;
                }
                err = 0;
                if (code == 32)
                    range = jbig2_huffman_get_bits(hs, 2, &err) + 3;
                else if (code == 33)
                    range = jbig2_huffman_get_bits(hs, 3, &err) + 3;
                else if (code == 34)
                    range = jbig2_huffman_get_bits(hs, 7, &err) + 11;
                if (err < 0) {
                    code = jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "failed to read huffman code");
                    goto cleanup1;
                }
                if (err > 0) {
                    code = jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number, "OOB decoding huffman code");
                    goto cleanup1;
                }
            }
            jbig2_error(ctx, JBIG2_SEVERITY_DEBUG, segment->number, "  read runcode%d at index %d (length %d range %d)", code, index, len, range);
            if (index + range > SBNUMSYMS) {
                jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number,
                            "runlength extends %d entries beyond the end of symbol ID table", index + range - SBNUMSYMS);
                range = SBNUMSYMS - index;
            }
            for (r = 0; r < range; r++) {
                symcodelengths[index + r].PREFLEN = len;
                symcodelengths[index + r].RANGELEN = 0;
                symcodelengths[index + r].RANGELOW = index + r;
            }
            index += r;
        }

        if (index < SBNUMSYMS) {
            code = jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number, "runlength codes do not cover the available symbol set");
            goto cleanup1;
        }

        symcodeparams.HTOOB = 0;
        symcodeparams.lines = symcodelengths;
        symcodeparams.n_lines = SBNUMSYMS;

        err = jbig2_huffman_skip(hs);
        if (err < 0)
        {
            jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "failed to skip to next byte when building huffman table");
            goto cleanup1;
        }

        SBSYMCODES = jbig2_build_huffman_table(ctx, &symcodeparams);

cleanup1:
        jbig2_free(ctx->allocator, symcodelengths);
        jbig2_release_huffman_table(ctx, runcodes);

        if (SBSYMCODES == NULL) {
            jbig2_huffman_free(ctx, hs);
            return jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "failed to construct symbol ID huffman table");
        }
    }

    jbig2_image_clear(ctx, image, params->SBDEFPIXEL);

    if (params->SBHUFF) {
        STRIPT = jbig2_huffman_get(hs, params->SBHUFFDT, &code);
    } else {
        code = jbig2_arith_int_decode(ctx, params->IADT, as, &STRIPT);
    }
    if (code < 0) {
        jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "failed to decode strip T");
        goto cleanup2;
    }
    if (code > 0) {
        code = jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number, "OOB obtained when decoding strip T");
        goto cleanup2;
    }

    STRIPT *= -(params->SBSTRIPS);
    FIRSTS = 0;
    NINSTANCES = 0;

    while (NINSTANCES < params->SBNUMINSTANCES) {

        if (params->SBHUFF) {
            DT = jbig2_huffman_get(hs, params->SBHUFFDT, &code);
        } else {
            code = jbig2_arith_int_decode(ctx, params->IADT, as, &DT);
        }
        if (code < 0) {
            jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "failed to decode delta T");
            goto cleanup2;
        }
        if (code > 0) {
            code = jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number, "OOB obtained when decoding delta T");
            goto cleanup2;
        }
        DT *= params->SBSTRIPS;
        STRIPT += DT;

        first_symbol = TRUE;

        for (;;) {

            if (first_symbol) {

                if (params->SBHUFF) {
                    DFS = jbig2_huffman_get(hs, params->SBHUFFFS, &code);
                } else {
                    code = jbig2_arith_int_decode(ctx, params->IAFS, as, &DFS);
                }
                if (code < 0) {
                    jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "failed to decode strip symbol S-difference");
                    goto cleanup2;
                }
                if (code > 0) {
                    code = jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number, "OOB obtained when decoding strip symbol S-difference");
                    goto cleanup2;
                }
                FIRSTS += DFS;
                CURS = FIRSTS;
                first_symbol = FALSE;
            } else {
                if (NINSTANCES > params->SBNUMINSTANCES) {
                    jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "too many NINSTANCES (%d) decoded", NINSTANCES);
                    break;
                }

                if (params->SBHUFF) {
                    IDS = jbig2_huffman_get(hs, params->SBHUFFDS, &code);
                } else {
                    code = jbig2_arith_int_decode(ctx, params->IADS, as, &IDS);
                }
                if (code < 0) {
                    jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "failed to decode symbol instance S coordinate");
                    goto cleanup2;
                }
                if (code > 0) {
                    jbig2_error(ctx, JBIG2_SEVERITY_DEBUG, segment->number, "OOB obtained when decoding symbol instance S coordinate signals end of strip with T value %d", DT);
                    break;
                }
                CURS += IDS + params->SBDSOFFSET;
            }

            if (params->SBSTRIPS == 1) {
                CURT = 0;
            } else if (params->SBHUFF) {
                CURT = jbig2_huffman_get_bits(hs, params->LOGSBSTRIPS, &code);
            } else {
                code = jbig2_arith_int_decode(ctx, params->IAIT, as, &CURT);
            }
            if (code < 0) {
                jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "failed to decode symbol instance T coordinate");
                goto cleanup2;
            }
            if (code > 0) {
                code = jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "OOB obtained when decoding symbol instance T coordinate");
                goto cleanup2;
            }
            T = STRIPT + CURT;

            if (params->SBHUFF) {
                ID = jbig2_huffman_get(hs, SBSYMCODES, &code);
            } else {
                code = jbig2_arith_iaid_decode(ctx, params->IAID, as, (int *)&ID);
            }
            if (code < 0) {
                code = jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "failed to obtain symbol instance symbol ID");
                goto cleanup2;
            }
            if (code > 0) {
                code = jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number, "OOB obtained when decoding symbol instance symbol ID");
                goto cleanup2;
            }
            if (ID >= SBNUMSYMS) {
                jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "ignoring out of range symbol ID (%d/%d)", ID, SBNUMSYMS);
                IB = NULL;
            } else {

                uint32_t id = ID;

                index = 0;
                while (id >= dicts[index]->n_symbols)
                    id -= dicts[index++]->n_symbols;
                if (dicts[index]->glyphs[id] == NULL) {
                    jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "missing glyph (%d/%d), ignoring", index, id);
                } else {
                    IB = jbig2_image_reference(ctx, dicts[index]->glyphs[id]);
                }
            }
            if (params->SBREFINE) {
                if (params->SBHUFF) {
                    RI = jbig2_huffman_get_bits(hs, 1, &code);
                } else {
                    code = jbig2_arith_int_decode(ctx, params->IARI, as, &RI);
                }
                if (code < 0) {
                    jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "failed to decode symbol bitmap refinement indicator");
                    goto cleanup2;
                }
                if (code > 0) {
                    code = jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number, "OOB obtained when decoding symbol bitmap refinement indicator");
                    goto cleanup2;
                }
            } else {
                RI = 0;
            }
            if (RI) {
                Jbig2RefinementRegionParams rparams;
                int32_t RDW, RDH, RDX, RDY;
                size_t BMSIZE = 0;
                int code1 = 0;
                int code2 = 0;
                int code3 = 0;
                int code4 = 0;
                int code5 = 0;
                int code6 = 0;

                if (!params->SBHUFF) {
                    code1 = jbig2_arith_int_decode(ctx, params->IARDW, as, &RDW);
                    code2 = jbig2_arith_int_decode(ctx, params->IARDH, as, &RDH);
                    code3 = jbig2_arith_int_decode(ctx, params->IARDX, as, &RDX);
                    code4 = jbig2_arith_int_decode(ctx, params->IARDY, as, &RDY);
                } else {
                    RDW = jbig2_huffman_get(hs, params->SBHUFFRDW, &code1);
                    RDH = jbig2_huffman_get(hs, params->SBHUFFRDH, &code2);
                    RDX = jbig2_huffman_get(hs, params->SBHUFFRDX, &code3);
                    RDY = jbig2_huffman_get(hs, params->SBHUFFRDY, &code4);
                    BMSIZE = jbig2_huffman_get(hs, params->SBHUFFRSIZE, &code5);
                    code6 = jbig2_huffman_skip(hs);
                }

                if (code1 < 0 || code2 < 0 || code3 < 0 || code4 < 0 || code5 < 0 || code6 < 0) {
                    code = jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "failed to decode data");
                    goto cleanup2;
                }
                if (code1 > 0 || code2 > 0 || code3 > 0 || code4 > 0 || code5 > 0 || code6 > 0) {
                    code = jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number, "OOB obtained when decoding symbol instance refinement data");
                    goto cleanup2;
                }

                if (IB) {
                    IBO = IB;
                    IB = NULL;
                    if (((int32_t) IBO->width) + RDW < 0 || ((int32_t) IBO->height) + RDH < 0) {
                        code = jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number, "reference image dimensions negative");
                        goto cleanup2;
                    }
                    refimage = jbig2_image_new(ctx, IBO->width + RDW, IBO->height + RDH);
                    if (refimage == NULL) {
                        code = jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "failed to allocate reference image");
                        goto cleanup2;
                    }
                    jbig2_image_clear(ctx, refimage, 0x00);

                    rparams.GRTEMPLATE = params->SBRTEMPLATE;
                    rparams.GRREFERENCE = IBO;
                    rparams.GRREFERENCEDX = (RDW >> 1) + RDX;
                    rparams.GRREFERENCEDY = (RDH >> 1) + RDY;
                    rparams.TPGRON = 0;
                    memcpy(rparams.grat, params->sbrat, 4);
                    code = jbig2_decode_refinement_region(ctx, segment, &rparams, as, refimage, GR_stats);
                    if (code < 0) {
                        jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "failed to decode refinement region");
                        goto cleanup2;
                    }

                    jbig2_image_release(ctx, IBO);
                    IBO = NULL;
                    IB = refimage;
                    refimage = NULL;
                }

                if (params->SBHUFF) {
                    code = jbig2_huffman_advance(hs, BMSIZE);
                    if (code < 0) {
                        jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "failed to advance after huffman decoding refinement region");
                        goto cleanup2;
                    }
                }
            }

            if ((!params->TRANSPOSED) && (params->REFCORNER > 1) && IB) {
                CURS += IB->width - 1;
            } else if ((params->TRANSPOSED) && !(params->REFCORNER & 1) && IB) {
                CURS += IB->height - 1;
            }

            S = CURS;

            if (!params->TRANSPOSED) {
                switch (params->REFCORNER) {
                case JBIG2_CORNER_TOPLEFT:
                    x = S;
                    y = T;
                    break;
                case JBIG2_CORNER_TOPRIGHT:
                    if (IB)
                        x = S - IB->width + 1;
                    else
                        x = S + 1;
                    y = T;
                    break;
                case JBIG2_CORNER_BOTTOMLEFT:
                    x = S;
                    if (IB)
                        y = T - IB->height + 1;
                    else
                        y = T + 1;
                    break;
                default:
                case JBIG2_CORNER_BOTTOMRIGHT:
                    if (IB ) {
                        x = S - IB->width + 1;
                        y = T - IB->height + 1;
                    } else {
                        x = S + 1;
                        y = T + 1;
                    }
                    break;
                }
            } else {
                switch (params->REFCORNER) {
                case JBIG2_CORNER_TOPLEFT:
                    x = T;
                    y = S;
                    break;
                case JBIG2_CORNER_TOPRIGHT:
                    if (IB)
                        x = T - IB->width + 1;
                    else
                        x = T + 1;
                    y = S;
                    break;
                case JBIG2_CORNER_BOTTOMLEFT:
                    x = T;
                    if (IB)
                        y = S - IB->height + 1;
                    else
                        y = S + 1;
                    break;
                default:
                case JBIG2_CORNER_BOTTOMRIGHT:
                    if (IB) {
                        x = T - IB->width + 1;
                        y = S - IB->height + 1;
                    } else {
                        x = T + 1;
                        y = S + 1;
                    }
                    break;
                }
            }

#ifdef JBIG2_DEBUG
            jbig2_error(ctx, JBIG2_SEVERITY_DEBUG, segment->number,
                        "composing glyph ID %d: %dx%d @ (%d,%d) symbol %d/%d", ID, IB->width, IB->height, x, y, NINSTANCES + 1, params->SBNUMINSTANCES);
#endif
            code = jbig2_image_compose(ctx, image, IB, x, y, params->SBCOMBOP);
            if (code < 0) {
                jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "failed to compose symbol instance symbol bitmap into picture");
                goto cleanup2;
            }

            if (IB && (!params->TRANSPOSED) && (params->REFCORNER < 2)) {
                CURS += IB->width - 1;
            } else if (IB && (params->TRANSPOSED) && (params->REFCORNER & 1)) {
                CURS += IB->height - 1;
            }

            NINSTANCES++;

            jbig2_image_release(ctx, IB);
            IB = NULL;
        }

    }

cleanup2:
    jbig2_image_release(ctx, refimage);
    jbig2_image_release(ctx, IBO);
    jbig2_image_release(ctx, IB);
    if (params->SBHUFF) {
        jbig2_release_huffman_table(ctx, SBSYMCODES);
    }
    jbig2_huffman_free(ctx, hs);

    return code;
}

int
jbig2_text_region(Jbig2Ctx *ctx, Jbig2Segment *segment, const byte *segment_data)
{
    uint32_t offset = 0;
    Jbig2RegionSegmentInfo region_info;
    Jbig2TextRegionParams params;
    Jbig2Image *image = NULL;
    Jbig2SymbolDict **dicts = NULL;
    uint32_t n_dicts = 0;
    uint16_t flags = 0;
    uint16_t huffman_flags = 0;
    Jbig2ArithCx *GR_stats = NULL;
    int code = 0;
    Jbig2WordStream *ws = NULL;
    Jbig2ArithState *as = NULL;
    uint32_t table_index = 0;
    const Jbig2HuffmanParams *huffman_params = NULL;

    memset(&params, 0, sizeof(Jbig2TextRegionParams));

    if (segment->data_length < 17) {
        code = jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number, "segment too short");
        goto cleanup2;
    }
    jbig2_get_region_segment_info(&region_info, segment_data);
    offset += 17;

    if (region_info.flags & 8)
        return jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number, "region segment flags indicate use of colored bitmap (NYI)");

    if (segment->data_length - offset < 2) {
        code = jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number, "segment too short");
        goto cleanup2;
    }
    flags = jbig2_get_uint16(segment_data + offset);
    offset += 2;

    jbig2_error(ctx, JBIG2_SEVERITY_DEBUG, segment->number, "text region header flags 0x%04x", flags);

    params.SBHUFF = flags & 0x0001;
    params.SBREFINE = flags & 0x0002;
    params.LOGSBSTRIPS = (flags & 0x000c) >> 2;
    params.SBSTRIPS = 1 << params.LOGSBSTRIPS;
    params.REFCORNER = (Jbig2RefCorner)((flags & 0x0030) >> 4);
    params.TRANSPOSED = flags & 0x0040;
    params.SBCOMBOP = (Jbig2ComposeOp)((flags & 0x0180) >> 7);
    params.SBDEFPIXEL = flags & 0x0200;

    params.SBDSOFFSET = (flags & 0x7C00) >> 10;
    if (params.SBDSOFFSET > 0x0f)
        params.SBDSOFFSET -= 0x20;
    params.SBRTEMPLATE = flags & 0x8000;

    if (params.SBDSOFFSET) {
        jbig2_error(ctx, JBIG2_SEVERITY_DEBUG, segment->number, "text region has SBDSOFFSET %d", params.SBDSOFFSET);
    }

    if (params.SBHUFF) {

        if (segment->data_length - offset < 2) {
            code = jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number, "segment too short");
            goto cleanup2;
        }
        huffman_flags = jbig2_get_uint16(segment_data + offset);
        offset += 2;

        if (huffman_flags & 0x8000)
            jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "reserved bit 15 of text region huffman flags is not zero");
    } else {

        if (segment->data_length - offset < 4) {
            code = jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number, "segment too short");
            goto cleanup2;
        }
        if ((params.SBREFINE) && !(params.SBRTEMPLATE)) {
            params.sbrat[0] = segment_data[offset];
            params.sbrat[1] = segment_data[offset + 1];
            params.sbrat[2] = segment_data[offset + 2];
            params.sbrat[3] = segment_data[offset + 3];
            offset += 4;
        }
    }

    if (segment->data_length - offset < 4) {
        code = jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number, "segment too short");
        goto cleanup2;
    }
    params.SBNUMINSTANCES = jbig2_get_uint32(segment_data + offset);
    offset += 4;

    if (params.SBHUFF) {

        switch (huffman_flags & 0x0003) {
        case 0:
            params.SBHUFFFS = jbig2_build_huffman_table(ctx, &jbig2_huffman_params_F);
            break;
        case 1:
            params.SBHUFFFS = jbig2_build_huffman_table(ctx, &jbig2_huffman_params_G);
            break;
        case 3:
            huffman_params = jbig2_find_table(ctx, segment, table_index);
            if (huffman_params == NULL) {
                code = jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "custom FS huffman table not found (%d)", table_index);
                goto cleanup1;
            }
            params.SBHUFFFS = jbig2_build_huffman_table(ctx, huffman_params);
            ++table_index;
            break;
        case 2:
        default:
            code = jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number, "text region specified invalid FS huffman table");
            goto cleanup1;
            break;
        }
        if (params.SBHUFFFS == NULL) {
            code = jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "failed to allocate text region specified FS huffman table");
            goto cleanup1;
        }

        switch ((huffman_flags & 0x000c) >> 2) {
        case 0:
            params.SBHUFFDS = jbig2_build_huffman_table(ctx, &jbig2_huffman_params_H);
            break;
        case 1:
            params.SBHUFFDS = jbig2_build_huffman_table(ctx, &jbig2_huffman_params_I);
            break;
        case 2:
            params.SBHUFFDS = jbig2_build_huffman_table(ctx, &jbig2_huffman_params_J);
            break;
        case 3:
            huffman_params = jbig2_find_table(ctx, segment, table_index);
            if (huffman_params == NULL) {
                code = jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "custom DS huffman table not found (%d)", table_index);
                goto cleanup1;
            }
            params.SBHUFFDS = jbig2_build_huffman_table(ctx, huffman_params);
            ++table_index;
            break;
        }
        if (params.SBHUFFDS == NULL) {
            code = jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "failed to allocate text region specified DS huffman table");
            goto cleanup1;
        }

        switch ((huffman_flags & 0x0030) >> 4) {
        case 0:
            params.SBHUFFDT = jbig2_build_huffman_table(ctx, &jbig2_huffman_params_K);
            break;
        case 1:
            params.SBHUFFDT = jbig2_build_huffman_table(ctx, &jbig2_huffman_params_L);
            break;
        case 2:
            params.SBHUFFDT = jbig2_build_huffman_table(ctx, &jbig2_huffman_params_M);
            break;
        case 3:
            huffman_params = jbig2_find_table(ctx, segment, table_index);
            if (huffman_params == NULL) {
                code = jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "custom DT huffman table not found (%d)", table_index);
                goto cleanup1;
            }
            params.SBHUFFDT = jbig2_build_huffman_table(ctx, huffman_params);
            ++table_index;
            break;
        }
        if (params.SBHUFFDT == NULL) {
            code = jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "failed to allocate text region specified DT huffman table");
            goto cleanup1;
        }

        switch ((huffman_flags & 0x00c0) >> 6) {
        case 0:
            params.SBHUFFRDW = jbig2_build_huffman_table(ctx, &jbig2_huffman_params_N);
            break;
        case 1:
            params.SBHUFFRDW = jbig2_build_huffman_table(ctx, &jbig2_huffman_params_O);
            break;
        case 3:
            huffman_params = jbig2_find_table(ctx, segment, table_index);
            if (huffman_params == NULL) {
                code = jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "custom RDW huffman table not found (%d)", table_index);
                goto cleanup1;
            }
            params.SBHUFFRDW = jbig2_build_huffman_table(ctx, huffman_params);
            ++table_index;
            break;
        case 2:
        default:
            code = jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number, "text region specified invalid RDW huffman table");
            goto cleanup1;
            break;
        }
        if (params.SBHUFFRDW == NULL) {
            code = jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "failed to allocate text region specified RDW huffman table");
            goto cleanup1;
        }

        switch ((huffman_flags & 0x0300) >> 8) {
        case 0:
            params.SBHUFFRDH = jbig2_build_huffman_table(ctx, &jbig2_huffman_params_N);
            break;
        case 1:
            params.SBHUFFRDH = jbig2_build_huffman_table(ctx, &jbig2_huffman_params_O);
            break;
        case 3:
            huffman_params = jbig2_find_table(ctx, segment, table_index);
            if (huffman_params == NULL) {
                code = jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "custom RDH huffman table not found (%d)", table_index);
                goto cleanup1;
            }
            params.SBHUFFRDH = jbig2_build_huffman_table(ctx, huffman_params);
            ++table_index;
            break;
        case 2:
        default:
            code = jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number, "text region specified invalid RDH huffman table");
            goto cleanup1;
            break;
        }
        if (params.SBHUFFRDH == NULL) {
            code = jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "failed to allocate text region specified RDH huffman table");
            goto cleanup1;
        }

        switch ((huffman_flags & 0x0c00) >> 10) {
        case 0:
            params.SBHUFFRDX = jbig2_build_huffman_table(ctx, &jbig2_huffman_params_N);
            break;
        case 1:
            params.SBHUFFRDX = jbig2_build_huffman_table(ctx, &jbig2_huffman_params_O);
            break;
        case 3:
            huffman_params = jbig2_find_table(ctx, segment, table_index);
            if (huffman_params == NULL) {
                code = jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "custom RDX huffman table not found (%d)", table_index);
                goto cleanup1;
            }
            params.SBHUFFRDX = jbig2_build_huffman_table(ctx, huffman_params);
            ++table_index;
            break;
        case 2:
        default:
            code = jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number, "text region specified invalid RDX huffman table");
            goto cleanup1;
            break;
        }
        if (params.SBHUFFRDX == NULL) {
            code = jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "failed to allocate text region specified RDX huffman table");
            goto cleanup1;
        }

        switch ((huffman_flags & 0x3000) >> 12) {
        case 0:
            params.SBHUFFRDY = jbig2_build_huffman_table(ctx, &jbig2_huffman_params_N);
            break;
        case 1:
            params.SBHUFFRDY = jbig2_build_huffman_table(ctx, &jbig2_huffman_params_O);
            break;
        case 3:
            huffman_params = jbig2_find_table(ctx, segment, table_index);
            if (huffman_params == NULL) {
                code = jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "custom RDY huffman table not found (%d)", table_index);
                goto cleanup1;
            }
            params.SBHUFFRDY = jbig2_build_huffman_table(ctx, huffman_params);
            ++table_index;
            break;
        case 2:
        default:
            code = jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number, "text region specified invalid RDY huffman table");
            goto cleanup1;
            break;
        }
        if (params.SBHUFFRDY == NULL) {
            code = jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "failed to allocate text region specified RDY huffman table");
            goto cleanup1;
        }

        switch ((huffman_flags & 0x4000) >> 14) {
        case 0:
            params.SBHUFFRSIZE = jbig2_build_huffman_table(ctx, &jbig2_huffman_params_A);
            break;
        case 1:
            huffman_params = jbig2_find_table(ctx, segment, table_index);
            if (huffman_params == NULL) {
                code = jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "custom RSIZE huffman table not found (%d)", table_index);
                goto cleanup1;
            }
            params.SBHUFFRSIZE = jbig2_build_huffman_table(ctx, huffman_params);
            ++table_index;
            break;
        }
        if (params.SBHUFFRSIZE == NULL) {
            code = jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "failed to allocate text region specified RSIZE huffman table");
            goto cleanup1;
        }

        if (huffman_flags & 0x8000) {
            jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "text region huffman flags bit 15 is set, contrary to spec");
        }

    }

    jbig2_error(ctx, JBIG2_SEVERITY_INFO, segment->number,
                "text region: %d x %d @ (%d,%d) %d symbols", region_info.width, region_info.height, region_info.x, region_info.y, params.SBNUMINSTANCES);

    n_dicts = jbig2_sd_count_referred(ctx, segment);
    if (n_dicts == 0) {
        jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "text region refers to no symbol dictionaries");
    } else {
        dicts = jbig2_sd_list_referred(ctx, segment);
        if (dicts == NULL) {
            code = jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number, "unable to retrieve symbol dictionaries! previous parsing error?");
            goto cleanup1;
        } else {
            uint32_t index;

            if (dicts[0] == NULL) {
                code = jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "unable to find first referenced symbol dictionary");
                goto cleanup1;
            }
            for (index = 1; index < n_dicts; index++)
                if (dicts[index] == NULL) {
                    jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "unable to find all referenced symbol dictionaries");
                    n_dicts = index;
                }
        }
    }

    {
        int stats_size = params.SBRTEMPLATE ? 1 << 10 : 1 << 13;

        GR_stats = jbig2_new(ctx, Jbig2ArithCx, stats_size);
        if (GR_stats == NULL) {
            code = jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number, "could not allocate arithmetic decoder state");
            goto cleanup1;
        }
        memset(GR_stats, 0, stats_size);
    }

    image = jbig2_image_new(ctx, region_info.width, region_info.height);
    if (image == NULL) {
        code = jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "failed to allocate text region image");
        goto cleanup2;
    }

    if (offset >= segment->data_length) {
        code = jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number, "segment too short");
        goto cleanup2;
    }
    ws = jbig2_word_stream_buf_new(ctx, segment_data + offset, segment->data_length - offset);
    if (ws == NULL) {
        code = jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "failed to allocate word stream when handling text region image");
        goto cleanup2;
    }

    as = jbig2_arith_new(ctx, ws);
    if (as == NULL) {
        code = jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "failed to allocate arithmetic coding context when handling text region image");
        goto cleanup3;
    }

    if (!params.SBHUFF) {
        uint8_t SBSYMCODELEN;
        uint32_t index;
        uint32_t SBNUMSYMS = 0;

        for (index = 0; index < n_dicts; index++) {
            SBNUMSYMS += dicts[index]->n_symbols;
        }

        params.IADT = jbig2_arith_int_ctx_new(ctx);
        params.IAFS = jbig2_arith_int_ctx_new(ctx);
        params.IADS = jbig2_arith_int_ctx_new(ctx);
        params.IAIT = jbig2_arith_int_ctx_new(ctx);
        if (params.IADT == NULL || params.IAFS == NULL || params.IADS == NULL || params.IAIT == NULL) {
            code = jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "failed to allocate text region image data");
            goto cleanup4;
        }

        for (SBSYMCODELEN = 0; ((uint64_t) 1 << SBSYMCODELEN) < (uint64_t) SBNUMSYMS; SBSYMCODELEN++);

        params.IAID = jbig2_arith_iaid_ctx_new(ctx, SBSYMCODELEN);
        params.IARI = jbig2_arith_int_ctx_new(ctx);
        params.IARDW = jbig2_arith_int_ctx_new(ctx);
        params.IARDH = jbig2_arith_int_ctx_new(ctx);
        params.IARDX = jbig2_arith_int_ctx_new(ctx);
        params.IARDY = jbig2_arith_int_ctx_new(ctx);
        if (params.IAID == NULL || params.IARI == NULL ||
            params.IARDW == NULL || params.IARDH == NULL || params.IARDX == NULL || params.IARDY == NULL) {
            code = jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "failed to allocate text region image data");
            goto cleanup5;
        }
    }

    code = jbig2_decode_text_region(ctx, segment, &params,
                                    (const Jbig2SymbolDict * const *)dicts, n_dicts, image, GR_stats, as, ws);
    if (code < 0) {
        jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "failed to decode text region image data");
        goto cleanup5;
    }

    if ((segment->flags & 63) == 4) {

        segment->result = jbig2_image_reference(ctx, image);
    } else {

        jbig2_error(ctx, JBIG2_SEVERITY_DEBUG, segment->number,
                    "composing %dx%d decoded text region onto page at (%d, %d)", region_info.width, region_info.height, region_info.x, region_info.y);
        code = jbig2_page_add_result(ctx, &ctx->pages[ctx->current_page], image, region_info.x, region_info.y, region_info.op);
        if (code < 0)
            jbig2_error(ctx, JBIG2_SEVERITY_WARNING, segment->number, "unable to add text region to page");
    }

cleanup5:
    if (!params.SBHUFF) {
        jbig2_arith_iaid_ctx_free(ctx, params.IAID);
        jbig2_arith_int_ctx_free(ctx, params.IARI);
        jbig2_arith_int_ctx_free(ctx, params.IARDW);
        jbig2_arith_int_ctx_free(ctx, params.IARDH);
        jbig2_arith_int_ctx_free(ctx, params.IARDX);
        jbig2_arith_int_ctx_free(ctx, params.IARDY);
    }

cleanup4:
    if (!params.SBHUFF) {
        jbig2_arith_int_ctx_free(ctx, params.IADT);
        jbig2_arith_int_ctx_free(ctx, params.IAFS);
        jbig2_arith_int_ctx_free(ctx, params.IADS);
        jbig2_arith_int_ctx_free(ctx, params.IAIT);
    }

cleanup3:
    jbig2_free(ctx->allocator, as);
    jbig2_word_stream_buf_free(ctx, ws);

cleanup2:
    jbig2_free(ctx->allocator, GR_stats);
    jbig2_image_release(ctx, image);

cleanup1:
    if (params.SBHUFF) {
        jbig2_release_huffman_table(ctx, params.SBHUFFFS);
        jbig2_release_huffman_table(ctx, params.SBHUFFDS);
        jbig2_release_huffman_table(ctx, params.SBHUFFDT);
        jbig2_release_huffman_table(ctx, params.SBHUFFRDX);
        jbig2_release_huffman_table(ctx, params.SBHUFFRDY);
        jbig2_release_huffman_table(ctx, params.SBHUFFRDW);
        jbig2_release_huffman_table(ctx, params.SBHUFFRDH);
        jbig2_release_huffman_table(ctx, params.SBHUFFRSIZE);
    }
    jbig2_free(ctx->allocator, dicts);

    return code;
}
