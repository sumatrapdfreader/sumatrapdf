#include <stddef.h>
#include <stdint.h>

#ifndef _JBIG2_H
#define _JBIG2_H

#ifdef __cplusplus
extern "C"
{
#endif

#define JBIG2_VERSION_MAJOR (0)
#define JBIG2_VERSION_MINOR (20)

typedef enum {
    JBIG2_SEVERITY_DEBUG,
    JBIG2_SEVERITY_INFO,
    JBIG2_SEVERITY_WARNING,
    JBIG2_SEVERITY_FATAL
} Jbig2Severity;

typedef enum {
    JBIG2_OPTIONS_EMBEDDED = 1
} Jbig2Options;

typedef struct _Jbig2Allocator Jbig2Allocator;
typedef struct _Jbig2Ctx Jbig2Ctx;
typedef struct _Jbig2GlobalCtx Jbig2GlobalCtx;

typedef struct _Jbig2Image Jbig2Image;
struct _Jbig2Image {
    uint32_t width;
    uint32_t height;
    uint32_t stride;
    uint8_t *data;
    int refcount;
};

#define JBIG2_UNKNOWN_SEGMENT_NUMBER ~0U
typedef void (*Jbig2ErrorCallback)(void *data, const char *msg, Jbig2Severity severity, uint32_t seg_idx);

struct _Jbig2Allocator {
    void *(*alloc)(Jbig2Allocator *allocator, size_t size);
    void (*free)(Jbig2Allocator *allocator, void *p);
    void *(*realloc)(Jbig2Allocator *allocator, void *p, size_t size);
};

#define jbig2_ctx_new(allocator, options, global_ctx, error_callback, error_callback_data) jbig2_ctx_new_imp((allocator), (options), (global_ctx), (error_callback), (error_callback_data), JBIG2_VERSION_MAJOR, JBIG2_VERSION_MINOR)
Jbig2Ctx *jbig2_ctx_new_imp(Jbig2Allocator *allocator,
                        Jbig2Options options,
                        Jbig2GlobalCtx *global_ctx,
                        Jbig2ErrorCallback error_callback,
                        void *error_callback_data,
                        int jbig2_version_major,
                        int jbig2_version_minor);
Jbig2Allocator *jbig2_ctx_free(Jbig2Ctx *ctx);

Jbig2GlobalCtx *jbig2_make_global_ctx(Jbig2Ctx *ctx);
Jbig2Allocator *jbig2_global_ctx_free(Jbig2GlobalCtx *global_ctx);

int jbig2_data_in(Jbig2Ctx *ctx, const unsigned char *data, size_t size);

Jbig2Image *jbig2_page_out(Jbig2Ctx *ctx);

void jbig2_release_page(Jbig2Ctx *ctx, Jbig2Image *image);

int jbig2_complete_page(Jbig2Ctx *ctx);

#ifdef __cplusplus
}
#endif

#endif
