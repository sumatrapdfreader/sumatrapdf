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

/*
	Fill block with len bytes of pseudo-randomness.
*/
void fz_memrnd(fz_context *ctx, unsigned char *data, int len)
{
	while (len-- > 0)
		*data++ = (unsigned char)fz_lrand48(ctx);
}
