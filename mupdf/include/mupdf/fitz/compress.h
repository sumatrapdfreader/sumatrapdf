#ifndef MUPDF_FITZ_COMPRESS_H
#define MUPDF_FITZ_COMPRESS_H

#include "mupdf/fitz/system.h"

typedef enum
{
	FZ_DEFLATE_NONE = 0,
	FZ_DEFLATE_BEST_SPEED = 1,
	FZ_DEFLATE_BEST = 9,
	FZ_DEFLATE_DEFAULT = -1
} fz_deflate_level;

size_t fz_deflate_bound(fz_context *ctx, size_t size);

void fz_deflate(fz_context *ctx, unsigned char *dest, size_t *compressed_length, const unsigned char *source, size_t source_length, fz_deflate_level level);

unsigned char *fz_new_deflated_data(fz_context *ctx, size_t *compressed_length, const unsigned char *source, size_t source_length, fz_deflate_level level);

unsigned char *fz_new_deflated_data_from_buffer(fz_context *ctx, size_t *compressed_length, fz_buffer *buffer, fz_deflate_level level);

#endif
