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

#ifndef _JBIG2_PRIV_H
#define _JBIG2_PRIV_H

/* To enable Memento predefine MEMENTO while building by setting
   CFLAGS=-DMEMENTO. */

/* If we are being compiled as part of a larger project that includes
 * Memento, that project should define JBIG_EXTERNAL_MEMENTO_H to point
 * to the include file to use.
 */
#ifdef JBIG_EXTERNAL_MEMENTO_H
#include JBIG_EXTERNAL_MEMENTO_H
#else
#include "memento.h"
#endif

/* library internals */

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

typedef struct _Jbig2Page Jbig2Page;
typedef struct _Jbig2Segment Jbig2Segment;

typedef enum {
    JBIG2_FILE_HEADER,
    JBIG2_FILE_SEQUENTIAL_HEADER,
    JBIG2_FILE_SEQUENTIAL_BODY,
    JBIG2_FILE_RANDOM_HEADERS,
    JBIG2_FILE_RANDOM_BODIES,
    JBIG2_FILE_EOF
} Jbig2FileState;

struct _Jbig2Ctx {
    Jbig2Allocator *allocator;
    Jbig2Options options;
    const Jbig2Ctx *global_ctx;
    Jbig2ErrorCallback error_callback;
    void *error_callback_data;

    byte *buf;
    size_t buf_size;
    unsigned int buf_rd_ix;
    unsigned int buf_wr_ix;

    Jbig2FileState state;

    uint8_t file_header_flags;
    uint32_t n_pages;

    int n_segments_max;
    Jbig2Segment **segments;
    int n_segments;             /* index of last segment header parsed */
    int segment_index;          /* index of last segment body parsed */

    /* list of decoded pages, including the one in progress,
       currently stored as a contiguous, 0-indexed array. */
    int current_page;
    int max_page_index;
    Jbig2Page *pages;
};

uint32_t jbig2_get_uint32(const byte *bptr);

int32_t jbig2_get_int32(const byte *buf);

uint16_t jbig2_get_uint16(const byte *bptr);

int16_t jbig2_get_int16(const byte *buf);

/* dynamic memory management */
void *jbig2_alloc(Jbig2Allocator *allocator, size_t size, size_t num);

void jbig2_free(Jbig2Allocator *allocator, void *p);

void *jbig2_realloc(Jbig2Allocator *allocator, void *p, size_t size, size_t num);

#define jbig2_new(ctx, t, size) ((t *)jbig2_alloc(ctx->allocator, size, sizeof(t)))

#define jbig2_renew(ctx, p, t, size) ((t *)jbig2_realloc(ctx->allocator, (p), size, sizeof(t)))

int jbig2_error(Jbig2Ctx *ctx, Jbig2Severity severity, int32_t seg_idx, const char *fmt, ...)
#ifdef __GNUC__
    __attribute__ ((format (__printf__, 4, 5)))
#endif
    ;

/* The word stream design is a compromise between simplicity and
   trying to amortize the number of method calls. Each ::get_next_word
   invocation pulls 4 bytes from the stream, packed big-endian into a
   32 bit word. The offset argument is provided as a convenience. It
   begins at 0 and increments by 4 for each successive invocation. */
typedef struct _Jbig2WordStream Jbig2WordStream;

struct _Jbig2WordStream {
    int (*get_next_word)(Jbig2WordStream *self, size_t offset, uint32_t *word);
};

Jbig2WordStream *jbig2_word_stream_buf_new(Jbig2Ctx *ctx, const byte *data, size_t size);

void jbig2_word_stream_buf_free(Jbig2Ctx *ctx, Jbig2WordStream *ws);

/* restrict is standard in C99, but not in all C++ compilers. */
#if defined (__STDC_VERSION_) && (__STDC_VERSION__ >= 199901L) /* C99 */
#define JBIG2_RESTRICT restrict
#elif defined(_MSC_VER) && (_MSC_VER >= 1600) /* MSVC 10 or newer */
#define JBIG2_RESTRICT __restrict
#elif defined(__GNUC__) && (__GNUC__ >= 3) /* GCC 3 or newer */
#define JBIG2_RESTRICT __restrict
#else /* Unknown or ancient */
#define JBIG2_RESTRICT
#endif

#endif /* _JBIG2_PRIV_H */
