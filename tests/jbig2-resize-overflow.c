/* Resize of a striped page must reject a height whose byte size wraps. */
#include "jbig2.h"

#include <stdio.h>
#include <stdlib.h>

Jbig2Image *jbig2_image_new(Jbig2Ctx *ctx, uint32_t width, uint32_t height);
Jbig2Image *jbig2_image_resize(Jbig2Ctx *ctx, Jbig2Image *image, uint32_t width, uint32_t height, int value);
void jbig2_image_release(Jbig2Ctx *ctx, Jbig2Image *image);

/* 49px -> stride 7. 613566758 * 7 == 2^32 + 10. */
enum { kWidth = 49, kStride = 7, kWrapHeight = 613566758u };

static size_t g_largest_realloc = 0;

static void *Alloc(Jbig2Allocator *a, size_t n) {
    (void)a;
    return malloc(n ? n : 1);
}

static void Free(Jbig2Allocator *a, void *p) {
    (void)a;
    free(p);
}

static void *Realloc(Jbig2Allocator *a, void *p, size_t n) {
    (void)a;
    if (n > g_largest_realloc) {
        g_largest_realloc = n;
    }
    /* A wrapped 32-bit size is tiny. A 64-bit size is multi-gigabyte.
       Refuse both rather than allocating them. */
    if (n > 1024u * 1024u) {
        return NULL;
    }
    return realloc(p, n ? n : 1);
}

static void OnError(void *data, const char *msg, Jbig2Severity severity, uint32_t seg) {
    (void)data;
    (void)msg;
    (void)severity;
    (void)seg;
}

static int Fail(const char *msg) {
    fprintf(stderr, "%s\n", msg);
    return 1;
}

int main(void) {
    Jbig2Allocator alloc = {Alloc, Free, Realloc};
    Jbig2Ctx *ctx = jbig2_ctx_new(&alloc, (Jbig2Options)0, NULL, OnError, NULL);
    Jbig2Image *image;
    Jbig2Image *grown;
    Jbig2Image *ok;

    if (!ctx) {
        return Fail("no context");
    }

    image = jbig2_image_new(ctx, kWidth, 1);
    if (!image || image->stride != kStride) {
        return Fail("expected stride 7");
    }

    g_largest_realloc = 0;
    grown = jbig2_image_resize(ctx, image, kWidth, kWrapHeight, 0);
    if (grown != NULL || g_largest_realloc != 0) {
        fprintf(stderr, "overflow height accepted: ptr=%p largest_realloc=%zu height=%u\n", (void *)grown,
                g_largest_realloc, grown ? grown->height : image->height);
        jbig2_image_release(ctx, grown ? grown : image);
        jbig2_ctx_free(ctx);
        return 1;
    }
    jbig2_image_release(ctx, image);

    ok = jbig2_image_new(ctx, kWidth, 1);
    if (!ok) {
        return Fail("no image for a legal resize");
    }
    g_largest_realloc = 0;
    if (jbig2_image_resize(ctx, ok, kWidth, 3, 0) == NULL || ok->height != 3 || g_largest_realloc != (size_t)kStride * 3) {
        fprintf(stderr, "legal resize failed: height=%u largest_realloc=%zu\n", ok->height, g_largest_realloc);
        jbig2_image_release(ctx, ok);
        jbig2_ctx_free(ctx);
        return 1;
    }
    jbig2_image_release(ctx, ok);
    jbig2_ctx_free(ctx);
    printf("ok\n");
    return 0;
}
