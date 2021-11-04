// Copyright (C) 2004-2021 Artifex Software, Inc.
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
// Artifex Software, Inc., 1305 Grant Avenue - Suite 200, Novato,
// CA 94945, U.S.A., +1(415)492-9861, for further information.

#ifndef MUPDF_FITZ_FILTER_H
#define MUPDF_FITZ_FILTER_H

#include "mupdf/fitz/system.h"
#include "mupdf/fitz/context.h"
#include "mupdf/fitz/buffer.h"
#include "mupdf/fitz/store.h"
#include "mupdf/fitz/stream.h"

typedef struct fz_jbig2_globals fz_jbig2_globals;

typedef struct
{
	int64_t offset;
	uint64_t length;
} fz_range;

/**
	The null filter reads a specified amount of data from the
	substream.
*/
fz_stream *fz_open_null_filter(fz_context *ctx, fz_stream *chain, uint64_t len, int64_t offset);

/**
	The range filter copies data from specified ranges of the
	chained stream.
*/
fz_stream *fz_open_range_filter(fz_context *ctx, fz_stream *chain, fz_range *ranges, int nranges);

/**
	The endstream filter reads a PDF substream, and starts to look
	for an 'endstream' token after the specified length.
*/
fz_stream *fz_open_endstream_filter(fz_context *ctx, fz_stream *chain, uint64_t len, int64_t offset);

/**
	Concat filter concatenates several streams into one.
*/
fz_stream *fz_open_concat(fz_context *ctx, int max, int pad);

/**
	Add a chained stream to the end of the concatenate filter.

	Ownership of chain is passed in.
*/
void fz_concat_push_drop(fz_context *ctx, fz_stream *concat, fz_stream *chain);

/**
	arc4 filter performs RC4 decoding of data read from the chained
	filter using the supplied key.
*/
fz_stream *fz_open_arc4(fz_context *ctx, fz_stream *chain, unsigned char *key, unsigned keylen);

/**
	aesd filter performs AES decoding of data read from the chained
	filter using the supplied key.
*/
fz_stream *fz_open_aesd(fz_context *ctx, fz_stream *chain, unsigned char *key, unsigned keylen);

/**
	a85d filter performs ASCII 85 Decoding of data read
	from the chained filter.
*/
fz_stream *fz_open_a85d(fz_context *ctx, fz_stream *chain);

/**
	ahxd filter performs ASCII Hex decoding of data read
	from the chained filter.
*/
fz_stream *fz_open_ahxd(fz_context *ctx, fz_stream *chain);

/**
	rld filter performs Run Length Decoding of data read
	from the chained filter.
*/
fz_stream *fz_open_rld(fz_context *ctx, fz_stream *chain);

/**
	dctd filter performs DCT (JPEG) decoding of data read
	from the chained filter.

	color_transform implements the PDF color_transform option;
	use -1 (unset) as a default.

	For subsampling on decode, set l2factor to the log2 of the
	reduction required (therefore 0 = full size decode).

	jpegtables is an optional stream from which the JPEG tables
	can be read. Use NULL if not required.
*/
fz_stream *fz_open_dctd(fz_context *ctx, fz_stream *chain, int color_transform, int l2factor, fz_stream *jpegtables);

/**
	faxd filter performs FAX decoding of data read from
	the chained filter.

	k: see fax specification (fax default is 0).

	end_of_line: whether we expect end of line markers (fax default
	is 0).

	encoded_byte_align: whether we align to bytes after each line
	(fax default is 0).

	columns: how many columns in the image (fax default is 1728).

	rows: 0 for unspecified or the number of rows of data to expect.

	end_of_block: whether we expect end of block markers (fax
	default is 1).

	black_is_1: determines the polarity of the image (fax default is
	0).
*/
fz_stream *fz_open_faxd(fz_context *ctx, fz_stream *chain,
	int k, int end_of_line, int encoded_byte_align,
	int columns, int rows, int end_of_block, int black_is_1);

/**
	flated filter performs LZ77 decoding (inflating) of data read
	from the chained filter.

	window_bits: How large a decompression window to use. Typically
	15. A negative number, -n, means to use n bits, but to expect
	raw data with no header.
*/
fz_stream *fz_open_flated(fz_context *ctx, fz_stream *chain, int window_bits);

/**
	lzwd filter performs LZW decoding of data read from the chained
	filter.

	early_change: (Default 1) specifies whether to change codes 1
	bit early.

	min_bits: (Default 9) specifies the minimum number of bits to
	use.

	reverse_bits: (Default 0) allows for compatibility with gif and
	old style tiffs (1).

	old_tiff: (Default 0) allows for different handling of the clear
	code, as found in old style tiffs.
*/
fz_stream *fz_open_lzwd(fz_context *ctx, fz_stream *chain, int early_change, int min_bits, int reverse_bits, int old_tiff);

/**
	predict filter performs pixel prediction on data read from
	the chained filter.

	predictor: 1 = copy, 2 = tiff, other = inline PNG predictor

	columns: width of image in pixels

	colors: number of components.

	bpc: bits per component (typically 8)
*/
fz_stream *fz_open_predict(fz_context *ctx, fz_stream *chain, int predictor, int columns, int colors, int bpc);

/**
	Open a filter that performs jbig2 decompression on the chained
	stream, using the optional globals record.
*/
fz_stream *fz_open_jbig2d(fz_context *ctx, fz_stream *chain, fz_jbig2_globals *globals, int embedded);

/**
	Create a jbig2 globals record from a buffer.

	Immutable once created.
*/
fz_jbig2_globals *fz_load_jbig2_globals(fz_context *ctx, fz_buffer *buf);

/**
	Increment the reference count for a jbig2 globals record.

	Never throws an exception.
*/
fz_jbig2_globals *fz_keep_jbig2_globals(fz_context *ctx, fz_jbig2_globals *globals);

/**
	Decrement the reference count for a jbig2 globals record.
	When the reference count hits zero, the record is freed.

	Never throws an exception.
*/
void fz_drop_jbig2_globals(fz_context *ctx, fz_jbig2_globals *globals);

/**
	Special jbig2 globals drop function for use in implementing
	store support.
*/
void fz_drop_jbig2_globals_imp(fz_context *ctx, fz_storable *globals);

/**
	Return buffer containing jbig2 globals data stream.
*/
fz_buffer * fz_jbig2_globals_data(fz_context *ctx, fz_jbig2_globals *globals);

/* Extra filters for tiff */

/**
	SGI Log 16bit (greyscale) decode from the chained filter.
	Decodes lines of w pixels to 8bpp greyscale.
*/
fz_stream *fz_open_sgilog16(fz_context *ctx, fz_stream *chain, int w);

/**
	SGI Log 24bit (LUV) decode from the chained filter.
	Decodes lines of w pixels to 8bpc rgb.
*/
fz_stream *fz_open_sgilog24(fz_context *ctx, fz_stream *chain, int w);

/**
	SGI Log 32bit (LUV) decode from the chained filter.
	Decodes lines of w pixels to 8bpc rgb.
*/
fz_stream *fz_open_sgilog32(fz_context *ctx, fz_stream *chain, int w);

/**
	4bit greyscale Thunderscan decoding from the chained filter.
	Decodes lines of w pixels to 8bpp greyscale.
*/
fz_stream *fz_open_thunder(fz_context *ctx, fz_stream *chain, int w);

#endif
