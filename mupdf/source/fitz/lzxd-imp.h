// Copyright (C) 2023 Artifex Software, Inc.
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

#ifndef MUPDF_FITZ_LZXD_IMP_H
#define MUPDF_FITZ_LZXD_IMP_H

#include "mupdf/fitz.h"

typedef enum
{
	/// Window size of 32 KB (2^15 bytes).
	KB32 = 0x00008000,
	/// Window size of 64 KB (2^16 bytes).
	KB64 = 0x00010000,
	/// Window size of 128 KB (2^17 bytes).
	KB128 = 0x00020000,
	/// Window size of 256 KB (2^18 bytes).
	KB256 = 0x00040000,
	/// Window size of 512 KB (2^19 bytes).
	KB512 = 0x00080000,
	/// Window size of 1 MB (2^20 bytes).
	MB1 = 0x00100000,
	/// Window size of 2 MB (2^21 bytes).
	MB2 = 0x00200000,
	/// Window size of 4 MB (2^22 bytes).
	MB4 = 0x00400000,
	/// Window size of 8 MB (2^23 bytes).
	MB8 = 0x00800000,
	/// Window size of 16 MB (2^24 bytes).
	MB16 = 0x01000000,
	/// Window size of 32 MB (2^25 bytes).
	MB32 = 0x02000000,
} fz_lzxd_window_size_t;

typedef struct fz_lzxd_t fz_lzxd_t;

/*
	Create an lzx decoder.
*/
fz_lzxd_t *
fz_new_lzxd(fz_context *ctx, fz_lzxd_window_size_t window_size, size_t reset_interval);

/*
	Pass in a variably sized chunk of compressed data, and get a
	32k size chunk of decompressed data out.

	input data must be an even number of bytes in size.
*/
void
fz_lzxd_decompress_chunk(fz_context *ctx, fz_lzxd_t *self, const uint8_t *chunk, size_t chunk_len, uint8_t *uncompressed32k);

/*
	Drop the lzx decoder structure.
*/
void
fz_drop_lzxd(fz_context *ctx, fz_lzxd_t *self);

#endif
