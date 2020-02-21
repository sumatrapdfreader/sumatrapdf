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

#ifdef __cplusplus
extern "C"
{
#endif

#ifndef _JBIG2_H
#define _JBIG2_H

#define JBIG2_VERSION_MAJOR (0)
#define JBIG2_VERSION_MINOR (18)

/* warning levels */
typedef enum {
    JBIG2_SEVERITY_DEBUG,
    JBIG2_SEVERITY_INFO,
    JBIG2_SEVERITY_WARNING,
    JBIG2_SEVERITY_FATAL
} Jbig2Severity;

typedef enum {
    JBIG2_OPTIONS_EMBEDDED = 1
} Jbig2Options;

/* forward public structure declarations */
typedef struct _Jbig2Allocator Jbig2Allocator;
typedef struct _Jbig2Ctx Jbig2Ctx;
typedef struct _Jbig2GlobalCtx Jbig2GlobalCtx;

/*
   this is the general image structure used by the jbig2dec library
   images are 1 bpp, packed into rows a byte at a time. stride gives
   the byte offset to the next row, while width and height define
   the size of the image area in pixels.
*/
typedef struct _Jbig2Image Jbig2Image;
struct _Jbig2Image {
    uint32_t width;
    uint32_t height;
    uint32_t stride;
    uint8_t *data;
    int refcount;
};

/* errors are returned from the library via a callback. If no callback
   is provided (a NULL argument is passed to jbig2_ctx_new) a default
   handler is used which prints fatal errors to the stderr stream. */

/* error callback */
typedef void (*Jbig2ErrorCallback)(void *data, const char *msg, Jbig2Severity severity, int32_t seg_idx);

/* memory allocation is likewise done via a set of callbacks so that
   clients can better control memory usage. If a NULL is passed for
   this argument of jbig2_ctx_new, a default allocator based on malloc()
   is used. */

/* dynamic memory callbacks */
struct _Jbig2Allocator {
    void *(*alloc)(Jbig2Allocator *allocator, size_t size);
    void (*free)(Jbig2Allocator *allocator, void *p);
    void *(*realloc)(Jbig2Allocator *allocator, void *p, size_t size);
};

/* decoder context */
#define jbig2_ctx_new(allocator, options, global_ctx, error_callback, error_callback_data) jbig2_ctx_new_imp((allocator), (options), (global_ctx), (error_callback), (error_callback_data), JBIG2_VERSION_MAJOR, JBIG2_VERSION_MINOR)
Jbig2Ctx *jbig2_ctx_new_imp(Jbig2Allocator *allocator,
                        Jbig2Options options,
                        Jbig2GlobalCtx *global_ctx,
                        Jbig2ErrorCallback error_callback,
                        void *error_callback_data,
                        int jbig2_version_major,
                        int jbig2_version_minor);
Jbig2Allocator *jbig2_ctx_free(Jbig2Ctx *ctx);

/* global context for embedded streams */
Jbig2GlobalCtx *jbig2_make_global_ctx(Jbig2Ctx *ctx);
Jbig2Allocator *jbig2_global_ctx_free(Jbig2GlobalCtx *global_ctx);

/* submit data to the decoder */
int jbig2_data_in(Jbig2Ctx *ctx, const unsigned char *data, size_t size);

/* get the next available decoded page image. NULL means there isn't one. */
Jbig2Image *jbig2_page_out(Jbig2Ctx *ctx);
/* mark a returned page image as no longer needed. */
void jbig2_release_page(Jbig2Ctx *ctx, Jbig2Image *image);
/* mark the current page as complete, simulating an end-of-page segment (for broken streams) */
int jbig2_complete_page(Jbig2Ctx *ctx);

#endif                          /* _JBIG2_H */

/* If we don't have a definition for inline, make it nothing so the code will compile */
#ifndef inline
#define inline
#endif

#ifdef __cplusplus
}
#endif
