#ifndef MUPDF_FITZ_COMPRESSED_BUFFER_H
#define MUPDF_FITZ_COMPRESSED_BUFFER_H

#include "mupdf/fitz/system.h"
#include "mupdf/fitz/context.h"
#include "mupdf/fitz/buffer.h"
#include "mupdf/fitz/stream.h"
#include "mupdf/fitz/filter.h"

typedef struct fz_compression_params_s fz_compression_params;

typedef struct fz_compressed_buffer_s fz_compressed_buffer;
size_t fz_compressed_buffer_size(fz_compressed_buffer *buffer);

fz_stream *fz_open_compressed_buffer(fz_context *ctx, fz_compressed_buffer *);
fz_stream *fz_open_image_decomp_stream_from_buffer(fz_context *ctx, fz_compressed_buffer *, int *l2factor);
fz_stream *fz_open_image_decomp_stream(fz_context *ctx, fz_stream *, fz_compression_params *, int *l2factor);

int fz_recognize_image_format(fz_context *ctx, unsigned char p[8]);

enum
{
	FZ_IMAGE_UNKNOWN = 0,

	/* Uncompressed samples */
	FZ_IMAGE_RAW,

	/* Compressed samples */
	FZ_IMAGE_FAX,
	FZ_IMAGE_FLATE,
	FZ_IMAGE_LZW,
	FZ_IMAGE_RLD,

	/* Full image formats */
	FZ_IMAGE_BMP,
	FZ_IMAGE_GIF,
	FZ_IMAGE_JBIG2,
	FZ_IMAGE_JPEG,
	FZ_IMAGE_JPX,
	FZ_IMAGE_JXR,
	FZ_IMAGE_PNG,
	FZ_IMAGE_PNM,
	FZ_IMAGE_TIFF,
};

struct fz_compression_params_s
{
	int type;
	union {
		struct {
			int color_transform; /* Use -1 for unset */
		} jpeg;
		struct {
			int smask_in_data;
		} jpx;
		struct {
			fz_jbig2_globals *globals;
		} jbig2;
		struct {
			int columns;
			int rows;
			int k;
			int end_of_line;
			int encoded_byte_align;
			int end_of_block;
			int black_is_1;
			int damaged_rows_before_error;
		} fax;
		struct
		{
			int columns;
			int colors;
			int predictor;
			int bpc;
		}
		flate;
		struct
		{
			int columns;
			int colors;
			int predictor;
			int bpc;
			int early_change;
		} lzw;
	} u;
};

struct fz_compressed_buffer_s
{
	fz_compression_params params;
	fz_buffer *buffer;
};

void fz_drop_compressed_buffer(fz_context *ctx, fz_compressed_buffer *buf);

#endif
