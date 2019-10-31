#ifndef MUPDF_FITZ_POOL_H
#define MUPDF_FITZ_POOL_H

#include "mupdf/fitz/system.h"
#include "mupdf/fitz/context.h"

typedef struct fz_pool_s fz_pool;

fz_pool *fz_new_pool(fz_context *ctx);
void *fz_pool_alloc(fz_context *ctx, fz_pool *pool, size_t size);
char *fz_pool_strdup(fz_context *ctx, fz_pool *pool, const char *s);
size_t fz_pool_size(fz_context *ctx, fz_pool *pool);
void fz_drop_pool(fz_context *ctx, fz_pool *pool);

#endif
