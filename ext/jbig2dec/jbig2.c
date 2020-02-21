/* Copyright (C) 2001-2019 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  1305 Grant Avenue - Suite 200, Novato,
   CA 94945, U.S.A., +1(415)492-9861, for further information.
*/

/*
    jbig2dec
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "os_types.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <limits.h>

#include "jbig2.h"
#include "jbig2_priv.h"
#include "jbig2_image.h"
#include "jbig2_page.h"
#include "jbig2_segment.h"

static void *
jbig2_default_alloc(Jbig2Allocator *allocator, size_t size)
{
    return malloc(size);
}

static void
jbig2_default_free(Jbig2Allocator *allocator, void *p)
{
    free(p);
}

static void *
jbig2_default_realloc(Jbig2Allocator *allocator, void *p, size_t size)
{
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
    /* Check for integer multiplication overflow when computing
    the full size of the allocation. */
    if (num > 0 && size > SIZE_MAX / num)
        return NULL;
    return allocator->alloc(allocator, size * num);
}

/* jbig2_free and jbig2_realloc moved to the bottom of this file */

static void
jbig2_default_error(void *data, const char *msg, Jbig2Severity severity, int32_t seg_idx)
{
    /* report only fatal errors by default */
    if (severity == JBIG2_SEVERITY_FATAL) {
        fprintf(stderr, "jbig2 decoder FATAL ERROR: %s", msg);
        if (seg_idx != -1)
            fprintf(stderr, " (segment 0x%02x)", seg_idx);
        fprintf(stderr, "\n");
        fflush(stderr);
    }
}

int
jbig2_error(Jbig2Ctx *ctx, Jbig2Severity severity, int32_t segment_number, const char *fmt, ...)
{
    char buf[1024];
    va_list ap;
    int n;

    va_start(ap, fmt);
    n = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    if (n < 0 || n == sizeof(buf))
        strncpy(buf, "failed to generate error string", sizeof(buf));
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
        jbig2_error(&fakectx, JBIG2_SEVERITY_FATAL, -1, "incompatible jbig2dec header (%d.%d) and library (%d.%d) versions",
            jbig2_version_major, jbig2_version_minor, JBIG2_VERSION_MAJOR, JBIG2_VERSION_MINOR);
        return NULL;
    }

    if (allocator == NULL)
        allocator = &jbig2_default_allocator;
    if (error_callback == NULL)
        error_callback = &jbig2_default_error;

    result = (Jbig2Ctx *) jbig2_alloc(allocator, sizeof(Jbig2Ctx), 1);
    if (result == NULL) {
        error_callback(error_callback_data, "failed to allocate initial context", JBIG2_SEVERITY_FATAL, -1);
        return NULL;
    }

    result->allocator = allocator;
    result->options = options;
    result->global_ctx = (const Jbig2Ctx *)global_ctx;
    result->error_callback = error_callback;
    result->error_callback_data = error_callback_data;

    result->state = (options & JBIG2_OPTIONS_EMBEDDED) ? JBIG2_FILE_SEQUENTIAL_HEADER : JBIG2_FILE_HEADER;

    result->buf = NULL;

    result->n_segments = 0;
    result->n_segments_max = 16;
    result->segments = jbig2_new(result, Jbig2Segment *, result->n_segments_max);
    if (result->segments == NULL) {
        error_callback(error_callback_data, "failed to allocate initial segments", JBIG2_SEVERITY_FATAL, -1);
        jbig2_free(allocator, result);
        return NULL;
    }
    result->segment_index = 0;

    result->current_page = 0;
    result->max_page_index = 4;
    result->pages = jbig2_new(result, Jbig2Page, result->max_page_index);
    if (result->pages == NULL) {
        error_callback(error_callback_data, "failed to allocated initial pages", JBIG2_SEVERITY_FATAL, -1);
        jbig2_free(allocator, result->segments);
        jbig2_free(allocator, result);
        return NULL;
    }
    {
        int index;

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

/* coverity[ -tainted_data_return ] */
/* coverity[ -tainted_data_argument : arg-0 ] */
int16_t
jbig2_get_int16(const byte *bptr)
{
    return get_int16(bptr);
}

/* coverity[ -tainted_data_return ] */
/* coverity[ -tainted_data_argument : arg-0 ] */
uint16_t
jbig2_get_uint16(const byte *bptr)
{
    return get_uint16(bptr);
}

/* coverity[ -tainted_data_return ] */
/* coverity[ -tainted_data_argument : arg-0 ] */
int32_t
jbig2_get_int32(const byte *bptr)
{
    return ((int32_t) get_int16(bptr) << 16) | get_uint16(bptr + 2);
}

/* coverity[ -tainted_data_return ] */
/* coverity[ -tainted_data_argument : arg-0 ] */
uint32_t
jbig2_get_uint32(const byte *bptr)
{
    return ((uint32_t) get_uint16(bptr) << 16) | get_uint16(bptr + 2);
}

/**
 * jbig2_data_in: submit data for decoding
 * @ctx: The jbig2dec decoder context
 * @data: a pointer to the data buffer
 * @size: the size of the data buffer in bytes
 *
 * Copies the specified data into internal storage and attempts
 * to (continue to) parse it as part of a jbig2 data stream.
 *
 * Return code: 0 on success
 *             -1 if there is a parsing error
 **/
int
jbig2_data_in(Jbig2Ctx *ctx, const unsigned char *data, size_t size)
{
    const size_t initial_buf_size = 1024;

    if (ctx->buf == NULL) {
        size_t buf_size = initial_buf_size;

        do
            buf_size <<= 1;
        while (buf_size < size);
        ctx->buf = jbig2_new(ctx, byte, buf_size);
        if (ctx->buf == NULL) {
            return jbig2_error(ctx, JBIG2_SEVERITY_FATAL, -1, "failed to allocate buffer when reading data");
        }
        ctx->buf_size = buf_size;
        ctx->buf_rd_ix = 0;
        ctx->buf_wr_ix = 0;
    } else if (ctx->buf_wr_ix + size > ctx->buf_size) {
        if (ctx->buf_rd_ix <= (ctx->buf_size >> 1) && ctx->buf_wr_ix - ctx->buf_rd_ix + size <= ctx->buf_size) {
            memmove(ctx->buf, ctx->buf + ctx->buf_rd_ix, ctx->buf_wr_ix - ctx->buf_rd_ix);
        } else {
            byte *buf;
            size_t buf_size = initial_buf_size;

            do
                buf_size <<= 1;
            while (buf_size < ctx->buf_wr_ix - ctx->buf_rd_ix + size);
            buf = jbig2_new(ctx, byte, buf_size);
            if (buf == NULL) {
                return jbig2_error(ctx, JBIG2_SEVERITY_FATAL, -1, "failed to allocate bigger buffer when reading data");
            }
            memcpy(buf, ctx->buf + ctx->buf_rd_ix, ctx->buf_wr_ix - ctx->buf_rd_ix);
            jbig2_free(ctx->allocator, ctx->buf);
            ctx->buf = buf;
            ctx->buf_size = buf_size;
        }
        ctx->buf_wr_ix -= ctx->buf_rd_ix;
        ctx->buf_rd_ix = 0;
    }
    memcpy(ctx->buf + ctx->buf_wr_ix, data, size);
    ctx->buf_wr_ix += size;

    /* data has now been added to buffer */

    for (;;) {
        const byte jbig2_id_string[8] = { 0x97, 0x4a, 0x42, 0x32, 0x0d, 0x0a, 0x1a, 0x0a };
        Jbig2Segment *segment;
        size_t header_size;
        int code;

        switch (ctx->state) {
        case JBIG2_FILE_HEADER:
            /* D.4.1 */
            if (ctx->buf_wr_ix - ctx->buf_rd_ix < 9)
                return 0;
            if (memcmp(ctx->buf + ctx->buf_rd_ix, jbig2_id_string, 8))
                return jbig2_error(ctx, JBIG2_SEVERITY_FATAL, -1, "not a JBIG2 file header");
            /* D.4.2 */
            ctx->file_header_flags = ctx->buf[ctx->buf_rd_ix + 8];
            /* Check for T.88 amendment 2 */
            if (ctx->file_header_flags & 0x04)
                return jbig2_error(ctx, JBIG2_SEVERITY_FATAL, -1, "file header indicates use of 12 adaptive template pixels (NYI)");
            /* Check for T.88 amendment 3 */
            if (ctx->file_header_flags & 0x08)
                return jbig2_error(ctx, JBIG2_SEVERITY_FATAL, -1, "file header indicates use of colored region segments (NYI)");
            if (ctx->file_header_flags & 0xFC) {
                jbig2_error(ctx, JBIG2_SEVERITY_WARNING, -1, "reserved bits (2-7) of file header flags are not zero (0x%02x)", ctx->file_header_flags);
            }
            /* D.4.3 */
            if (!(ctx->file_header_flags & 2)) {        /* number of pages is known */
                if (ctx->buf_wr_ix - ctx->buf_rd_ix < 13)
                    return 0;
                ctx->n_pages = jbig2_get_uint32(ctx->buf + ctx->buf_rd_ix + 9);
                ctx->buf_rd_ix += 13;
                if (ctx->n_pages == 1)
                    jbig2_error(ctx, JBIG2_SEVERITY_INFO, -1, "file header indicates a single page document");
                else
                    jbig2_error(ctx, JBIG2_SEVERITY_INFO, -1, "file header indicates a %d page document", ctx->n_pages);
            } else {            /* number of pages not known */
                ctx->n_pages = 0;
                ctx->buf_rd_ix += 9;
            }
            /* determine the file organization based on the flags - D.4.2 again */
            if (ctx->file_header_flags & 1) {
                ctx->state = JBIG2_FILE_SEQUENTIAL_HEADER;
                jbig2_error(ctx, JBIG2_SEVERITY_DEBUG, -1, "file header indicates sequential organization");
            } else {
                ctx->state = JBIG2_FILE_RANDOM_HEADERS;
                jbig2_error(ctx, JBIG2_SEVERITY_DEBUG, -1, "file header indicates random-access organization");
            }
            break;
        case JBIG2_FILE_SEQUENTIAL_HEADER:
        case JBIG2_FILE_RANDOM_HEADERS:
            segment = jbig2_parse_segment_header(ctx, ctx->buf + ctx->buf_rd_ix, ctx->buf_wr_ix - ctx->buf_rd_ix, &header_size);
            if (segment == NULL)
                return 0; /* need more data */
            ctx->buf_rd_ix += header_size;

            if (ctx->n_segments == ctx->n_segments_max) {
                Jbig2Segment **segments;

                segments = jbig2_renew(ctx, ctx->segments, Jbig2Segment *, (ctx->n_segments_max <<= 2));
                if (segments == NULL) {
                    ctx->state = JBIG2_FILE_EOF;
                    return jbig2_error(ctx, JBIG2_SEVERITY_FATAL, segment->number, "failed to allocate space for more segments");
                }
                ctx->segments = segments;
            }


            ctx->segments[ctx->n_segments++] = segment;
            if (ctx->state == JBIG2_FILE_RANDOM_HEADERS) {
                if ((segment->flags & 63) == 51)        /* end of file */
                    ctx->state = JBIG2_FILE_RANDOM_BODIES;
            } else              /* JBIG2_FILE_SEQUENTIAL_HEADER */
                ctx->state = JBIG2_FILE_SEQUENTIAL_BODY;
            break;
        case JBIG2_FILE_SEQUENTIAL_BODY:
        case JBIG2_FILE_RANDOM_BODIES:
            segment = ctx->segments[ctx->segment_index];

            /* immediate generic regions may have unknown size */
            if (segment->data_length == 0xffffffff && (segment->flags & 63) == 38) {
                byte *s, *e, *p;
                int mmr;
                byte mmr_marker[2] = { 0x00, 0x00 };
                byte arith_marker[2] = { 0xff, 0xac };
                byte *desired_marker;

                s = p = ctx->buf + ctx->buf_rd_ix;
                e = ctx->buf + ctx->buf_wr_ix;

                if (e - p < 18)
                        return 0; /* need more data */

                mmr = p[17] & 1;
                p += 18;
                desired_marker = mmr ? mmr_marker : arith_marker;

                /* look for two byte marker */
                if (e - p < 2)
                    return 0; /* need more data */

                while (p[0] != desired_marker[0] || p[1] != desired_marker[1]) {
                    p++;
                    if (e - p < 2)
                        return 0; /* need more data */
                }
                p += 2;

                /* the marker is followed by a four byte row count */
                if (e - p < 4)
                        return 0; /* need more data */
                segment->rows = jbig2_get_uint32(p);
                p += 4;

                segment->data_length = p - s;
                jbig2_error(ctx, JBIG2_SEVERITY_INFO, segment->number, "unknown length determined to be %lu", (long) segment->data_length);
            }
            else if (segment->data_length > ctx->buf_wr_ix - ctx->buf_rd_ix)
                    return 0; /* need more data */

            code = jbig2_parse_segment(ctx, segment, ctx->buf + ctx->buf_rd_ix);
            ctx->buf_rd_ix += segment->data_length;
            ctx->segment_index++;
            if (ctx->state == JBIG2_FILE_RANDOM_BODIES) {
                if (ctx->segment_index == ctx->n_segments)
                    ctx->state = JBIG2_FILE_EOF;
            } else {            /* JBIG2_FILE_SEQUENTIAL_BODY */
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
            return jbig2_error(ctx, JBIG2_SEVERITY_WARNING, -1, "garbage beyond end of file");
        }
    }
}

Jbig2Allocator *
jbig2_ctx_free(Jbig2Ctx *ctx)
{
    Jbig2Allocator *ca;
    int i;

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

/* I'm not committed to keeping the word stream interface. It's handy
   when you think you may be streaming your input, but if you're not
   (as is currently the case), it just adds complexity.
*/

typedef struct {
    Jbig2WordStream super;
    const byte *data;
    size_t size;
} Jbig2WordStreamBuf;

static int
jbig2_word_stream_buf_get_next_word(Jbig2WordStream *self, size_t offset, uint32_t *word)
{
    Jbig2WordStreamBuf *z = (Jbig2WordStreamBuf *) self;
    uint32_t val = 0;
    int ret = 0;

    if (self == NULL || word == NULL)
        return -1;
    if (offset >= z->size) {
        *word = 0;
        return 0;
    }

    if (offset < z->size) {
        val |= z->data[offset] << 24;
        ret++;
    }
    if (offset + 1 < z->size) {
        val |= z->data[offset + 1] << 16;
        ret++;
    }
    if (offset + 2 < z->size) {
        val |= z->data[offset + 2] << 8;
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
        jbig2_error(ctx, JBIG2_SEVERITY_FATAL, -1, "failed to allocate word stream");
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

/* When Memento is in use, the ->free and ->realloc calls get
 * turned into ->Memento_free and ->Memento_realloc, which is
 * obviously problematic. Undefine free and realloc here to
 * avoid this. */
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
    /* check for integer multiplication overflow */
    if (num > 0 && size >= SIZE_MAX / num)
        return NULL;
    return allocator->realloc(allocator, p, size * num);
}
