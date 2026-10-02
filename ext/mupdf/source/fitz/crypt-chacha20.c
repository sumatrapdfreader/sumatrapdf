// Copyright (C) 2004-2026 Artifex Software, Inc.
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

#include "mupdf/fitz.h"

#include <string.h>

/*
	This file implements the ChaCha20 stream cipher.

	See D. J. Bernstein's site and RFC 7539 for details.

	https://cr.yp.to/chacha.html

	https://datatracker.ietf.org/doc/rfc7539/
*/

#define ROTL(a,b) (((a) << (b)) | ((a) >> (32 - (b))))

#define QR(a, b, c, d) ( \
	a += b, d ^= a, d = ROTL(d,16), \
	c += d, b ^= c, b = ROTL(b,12), \
	a += b, d ^= a, d = ROTL(d, 8), \
	c += d, b ^= c, b = ROTL(b, 7))

static void fz_chacha20_block(unsigned char *out, const uint32_t *in)
{
	int i;
	uint32_t x[16];
	memcpy(x, in, 64);
	for (i = 0; i < 10; ++i)
	{
		QR(x[0], x[4], x[8], x[12]);
		QR(x[1], x[5], x[9], x[13]);
		QR(x[2], x[6], x[10], x[14]);
		QR(x[3], x[7], x[11], x[15]);
		QR(x[0], x[5], x[10], x[15]);
		QR(x[1], x[6], x[11], x[12]);
		QR(x[2], x[7], x[8], x[13]);
		QR(x[3], x[4], x[9], x[14]);
	}
	for (i = 0; i < 16; ++i)
	{
		fz_pack_uint32_le(&out[i * 4], x[i] + in[i]);
	}
}

void fz_chacha20_init(fz_chacha20 *stm, unsigned char *key, unsigned char *nonce, uint32_t counter)
{
	// first four words are constants
	stm->s[0] = 0x61707865;
	stm->s[1] = 0x3320646e;
	stm->s[2] = 0x79622d32;
	stm->s[3] = 0x6b206574;

	// 256-bit key
	stm->s[4] = fz_unpack_uint32_le(key + 0);
	stm->s[5] = fz_unpack_uint32_le(key + 4);
	stm->s[6] = fz_unpack_uint32_le(key + 8);
	stm->s[7] = fz_unpack_uint32_le(key + 12);
	stm->s[8] = fz_unpack_uint32_le(key + 16);
	stm->s[9] = fz_unpack_uint32_le(key + 20);
	stm->s[10] = fz_unpack_uint32_le(key + 24);
	stm->s[11] = fz_unpack_uint32_le(key + 28);

	// block counter
	stm->s[12] = counter;

	// 96-bit nonce
	stm->s[13] = fz_unpack_uint32_le(nonce + 0);
	stm->s[14] = fz_unpack_uint32_le(nonce + 4);
	stm->s[15] = fz_unpack_uint32_le(nonce + 8);
}

void fz_chacha20_encrypt(fz_chacha20 *stm, unsigned char *dst, const unsigned char *src, uint32_t size)
{
	unsigned char out[64];
	uint32_t i;

	for (;;)
	{
		fz_chacha20_block(out, stm->s);

		stm->s[12]++;

		if (size <= 64)
		{
			for (i = 0; i < size; ++i)
				dst[i] = src[i] ^ out[i];
			return;
		}

		for (i = 0; i < 64; ++i)
			dst[i] = src[i] ^ out[i];
		size -= 64;
		src += 64;
		dst += 64;
	}
}
