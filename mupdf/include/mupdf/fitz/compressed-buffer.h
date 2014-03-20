#ifndef MUPDF_FITZ_COMPRESSED_BUFFER_H
#define MUPDF_FITZ_COMPRESSED_BUFFER_H

#include "mupdf/fitz/system.h"
#include "mupdf/fitz/context.h"
#include "mupdf/fitz/buffer.h"
#include "mupdf/fitz/stream.h"

typedef struct fz_compression_params_s fz_compression_params;

typedef struct fz_compressed_buffer_s fz_compressed_buffer;
unsigned int fz_compressed_buffer_size(fz_compressed_buffer *buffer);

fz_stream *fz_open_compressed_buffer(fz_context *ctx, fz_compressed_buffer *);
fz_stream *fz_open_image_decomp_stream_from_buffer(fz_context *ctx, fz_compressed_buffer *, int *l2factor);
fz_stream *fz_open_image_decomp_stream(fz_context *ctx, fz_stream *, fz_compression_params *, int *l2factor);

enum
{
	FZ_IMAGE_UNKNOWN = 0,
	FZ_IMAGE_JPEG = 1,
	FZ_IMAGE_JPX = 2, /* Placeholder until supported */
	FZ_IMAGE_FAX = 3,
	FZ_IMAGE_JBIG2 = 4, /* Placeholder until supported */
	FZ_IMAGE_RAW = 5,
	FZ_IMAGE_RLD = 6,
	FZ_IMAGE_FLATE = 7,
	FZ_IMAGE_LZW = 8,
	FZ_IMAGE_PNG = 9,
	FZ_IMAGE_TIFF = 10,
	FZ_IMAGE_JXR = 11, /* Placeholder until supported */
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

void fz_free_compressed_buffer(fz_context *ctx, fz_compressed_buffer *buf);

#endif
