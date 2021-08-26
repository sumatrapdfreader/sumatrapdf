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

#include "mupdf/fitz.h"

/* The pseudo-random number generator in this file is based on the MIT licensed
 * implementation in musl libc. */

#include <string.h>

/* The seed is initialized in context.c as follows:
 * static uint16_t __seed48[7] = { 0, 0, 0, 0xe66d, 0xdeec, 0x5, 0xb };
 */

static uint64_t fz_rand48_step(uint16_t *xi, uint16_t *lc)
{
	uint64_t a, x;
	x = xi[0] | (xi[1]+0U)<<16 | (xi[2]+0ULL)<<32;
	a = lc[0] | (lc[1]+0U)<<16 | (lc[2]+0ULL)<<32;
	x = a*x + lc[3];
	xi[0] = x;
	xi[1] = x>>16;
	xi[2] = x>>32;
	return x & 0xffffffffffffull;
}

double fz_erand48(fz_context *ctx, uint16_t s[3])
{
	union {
		uint64_t u;
		double f;
	} x = { 0x3ff0000000000000ULL | fz_rand48_step(s, ctx->seed48+3)<<4 };
	return x.f - 1.0;
}

/*
	Pseudo-random numbers using a linear congruential algorithm and 48-bit
	integer arithmetic.
*/
double fz_drand48(fz_context *ctx)
{
	return fz_erand48(ctx, ctx->seed48);
}

int32_t fz_nrand48(fz_context *ctx, uint16_t s[3])
{
	return fz_rand48_step(s, ctx->seed48+3) >> 17;
}

int32_t fz_lrand48(fz_context *ctx)
{
	return fz_nrand48(ctx, ctx->seed48);
}

int32_t fz_jrand48(fz_context *ctx, uint16_t s[3])
{
	return (int32_t)(fz_rand48_step(s, ctx->seed48+3) >> 16);
}

int32_t fz_mrand48(fz_context *ctx)
{
	return fz_jrand48(ctx, ctx->seed48);
}

void fz_lcong48(fz_context *ctx, uint16_t p[7])
{
	memcpy(ctx->seed48, p, sizeof ctx->seed48);
}

uint16_t *fz_seed48(fz_context *ctx, uint16_t *s)
{
	static uint16_t p[3];
	memcpy(p, ctx->seed48, sizeof p);
	memcpy(ctx->seed48, s, sizeof p);
	return p;
}

void fz_srand48(fz_context *ctx, int32_t seed)
{
	uint16_t p[3] = { 0x330e, seed, seed>>16 };
	fz_seed48(ctx, p);
}

void fz_memrnd(fz_context *ctx, unsigned char *data, int len)
{
#ifdef CLUSTER
	memset(data, 0x55, len);
#else
	while (len-- > 0)
		*data++ = (unsigned char)fz_lrand48(ctx);
#endif
}
