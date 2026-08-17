// Copyright (C) 2004-2025 Artifex Software, Inc.
//
// This file is part of MuPDF.
//
// MuPDF is free software: you can redistribute it and/or modify it under the
// terms of the GNU Affero General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option)
// any later version.
//
// MuPDF is distributed in the hope that it will be useful, but WITHOUT ANY
// WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
// FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for more
// details.
//
// You should have received a copy of the GNU Affero General Public License
// along with MuPDF. If not, see <https://www.gnu.org/licenses/agpl-3.0.en.html>
//
// Alternative licensing terms are available from the licensor.
// For commercial licensing, see <https://www.artifex.com/> or contact
// Artifex Software, Inc., 39 Mesa Street, Suite 108A, San Francisco,
// CA 94129, USA, for further information.

#ifndef MUPDF_FITZ_COMPRESSED_BUFFER_H
#define MUPDF_FITZ_COMPRESSED_BUFFER_H

#include "mupdf/fitz/system.h"
#include "mupdf/fitz/context.h"
#include "mupdf/fitz/buffer.h"
#include "mupdf/fitz/stream.h"
#include "mupdf/fitz/filter.h"

/**
	Compression parameters used for buffers of compressed data;
	typically for the source data for images.
*/
typedef struct
{
	int type;
	union {
		struct {
			int color_transform; /* Use -1 for unset */
			int invert_cmyk; /* Use 1 for standalone JPEG files */
		} jpeg;
		struct {
			int smask_in_data;
		} jpx;
		struct {
			fz_jbig2_globals *globals;
			int embedded;
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
		}
		brotli;
		struct
		{
			int columns;
			int colors;
			int predictor;
			int bpc;
			int early_change;
		} lzw;
	} u;
} fz_compression_params;

/**
	Buffers of compressed data; typically for the source data
	for images.
*/
typedef struct
{
	int refs;
	fz_compression_params params;
	fz_buffer *buffer;
} fz_compressed_buffer;

/**
	Take a reference to an fz_compressed_buffer.
*/
fz_compressed_buffer *fz_keep_compressed_buffer(fz_context *ctx, fz_compressed_buffer *cbuf);

/**
	Return the storage size used for a buffer and its data.
	Used in implementing store handling.

	Never throws exceptions.
*/
size_t fz_compressed_buffer_size(fz_compressed_buffer *buffer);

/**
	Open a stream to read the decompressed version of a buffer.
*/
fz_stream *fz_open_compressed_buffer(fz_context *ctx, fz_compressed_buffer *);

/**
	Open a stream to read the decompressed version of a buffer,
	with optional log2 subsampling.

	l2factor = NULL for no subsampling, or a pointer to an integer
	containing the maximum log2 subsample factor acceptable (0 =
	none, 1 = halve dimensions, 2 = quarter dimensions etc). If
	non-NULL, then *l2factor will be updated on exit with the actual
	log2 subsample factor achieved.
*/
fz_stream *fz_open_image_decomp_stream_from_buffer(fz_context *ctx, fz_compressed_buffer *, int *l2factor);

/**
	Open a stream to read the decompressed version of another stream
	with optional log2 subsampling.
*/
fz_stream *fz_open_image_decomp_stream(fz_context *ctx, fz_stream *, fz_compression_params *, int *l2factor);

/**
	Recognise image format strings in the first 8 bytes from image
	data.
*/
int fz_recognize_image_format(fz_context *ctx, unsigned char p[8]);

/**
	Map from FZ_IMAGE_* value to string.

	The returned string is static and therefore must not be freed.
*/
const char *fz_image_type_name(int type);

/**
	Map from (case sensitive) image type string to FZ_IMAGE_*
	type value.
*/
int fz_lookup_image_type(const char *type);

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
	FZ_IMAGE_BROTLI,

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
	FZ_IMAGE_PSD,
};

/**
	Drop a reference to a compressed buffer. Destroys the buffer
	and frees any storage/other references held by it.

	Never throws exceptions.
*/
void fz_drop_compressed_buffer(fz_context *ctx, fz_compressed_buffer *buf);

/**
	Create a new, UNKNOWN format, compressed_buffer.
*/
fz_compressed_buffer *fz_new_compressed_buffer(fz_context *ctx);

#endif
