/*
This code is based on the code found from 7-Zip, which has a modified
version of the SHA-256 found from Crypto++ <http://www.cryptopp.com/>.
The code was modified a little to fit into liblzma and fitz.

This file has been put into the public domain.
You can do whatever you want with this file.

SHA-384 and SHA-512 were also taken from Crypto++ and adapted for fitz.
*/

#include "mupdf/fitz.h"

#include <string.h>

static inline int isbigendian(void)
{
	static const int one = 1;
	return *(char*)&one == 0;
}

static inline unsigned int bswap32(unsigned int num)
{
	return	( (((num) << 24))
		| (((num) << 8) & 0x00FF0000)
		| (((num) >> 8) & 0x0000FF00)
		| (((num) >> 24)) );
}

static inline uint64_t bswap64(uint64_t num)
{
	return ( (((num) << 56))
		| (((num) << 40) & 0x00FF000000000000ULL)
		| (((num) << 24) & 0x0000FF0000000000ULL)
		| (((num) << 8) & 0x000000FF00000000ULL)
		| (((num) >> 8) & 0x00000000FF000000ULL)
		| (((num) >> 24) & 0x0000000000FF0000ULL)
		| (((num) >> 40) & 0x000000000000FF00ULL)
		| (((num) >> 56)) );
}

/* At least on x86, GCC is able to optimize this to a rotate instruction. */
#define rotr(num, amount) ((num) >> (amount) | (num) << (8 * sizeof(num) - (amount)))

#define blk0(i) (W[i] = data[i])
#define blk2(i) (W[i & 15] += s1(W[(i - 2) & 15]) + W[(i - 7) & 15] \
		+ s0(W[(i - 15) & 15]))

#define Ch(x, y, z) (z ^ (x & (y ^ z)))
#define Maj(x, y, z) ((x & y) | (z & (x | y)))

#define a(i) T[(0 - i) & 7]
#define b(i) T[(1 - i) & 7]
#define c(i) T[(2 - i) & 7]
#define d(i) T[(3 - i) & 7]
#define e(i) T[(4 - i) & 7]
#define f(i) T[(5 - i) & 7]
#define g(i) T[(6 - i) & 7]
#define h(i) T[(7 - i) & 7]

#define R(i) \
	h(i) += S1(e(i)) + Ch(e(i), f(i), g(i)) + K[i + j] \
		+ (j ? blk2(i) : blk0(i)); \
	d(i) += h(i); \
	h(i) += S0(a(i)) + Maj(a(i), b(i), c(i))

/* For SHA256 */

#define S0(x) (rotr(x, 2) ^ rotr(x, 13) ^ rotr(x, 22))
#define S1(x) (rotr(x, 6) ^ rotr(x, 11) ^ rotr(x, 25))
#define s0(x) (rotr(x, 7) ^ rotr(x, 18) ^ (x >> 3))
#define s1(x) (rotr(x, 17) ^ rotr(x, 19) ^ (x >> 10))

static const unsigned int SHA256_K[64] = {
	0x428A2F98, 0x71374491, 0xB5C0FBCF, 0xE9B5DBA5,
	0x3956C25B, 0x59F111F1, 0x923F82A4, 0xAB1C5ED5,
	0xD807AA98, 0x12835B01, 0x243185BE, 0x550C7DC3,
	0x72BE5D74, 0x80DEB1FE, 0x9BDC06A7, 0xC19BF174,
	0xE49B69C1, 0xEFBE4786, 0x0FC19DC6, 0x240CA1CC,
	0x2DE92C6F, 0x4A7484AA, 0x5CB0A9DC, 0x76F988DA,
	0x983E5152, 0xA831C66D, 0xB00327C8, 0xBF597FC7,
	0xC6E00BF3, 0xD5A79147, 0x06CA6351, 0x14292967,
	0x27B70A85, 0x2E1B2138, 0x4D2C6DFC, 0x53380D13,
	0x650A7354, 0x766A0ABB, 0x81C2C92E, 0x92722C85,
	0xA2BFE8A1, 0xA81A664B, 0xC24B8B70, 0xC76C51A3,
	0xD192E819, 0xD6990624, 0xF40E3585, 0x106AA070,
	0x19A4C116, 0x1E376C08, 0x2748774C, 0x34B0BCB5,
	0x391C0CB3, 0x4ED8AA4A, 0x5B9CCA4F, 0x682E6FF3,
	0x748F82EE, 0x78A5636F, 0x84C87814, 0x8CC70208,
	0x90BEFFFA, 0xA4506CEB, 0xBEF9A3F7, 0xC67178F2,
};

static void
transform256(unsigned int state[8], unsigned int data[16])
{
	const unsigned int *K = SHA256_K;
	unsigned int W[16];
	unsigned int T[8];
	unsigned int j;

	/* ensure big-endian integers */
	if (!isbigendian())
		for (j = 0; j < 16; j++)
			data[j] = bswap32(data[j]);

	/* Copy state[] to working vars. */
	memcpy(T, state, sizeof(T));

	/* 64 operations, partially loop unrolled */
	for (j = 0; j < 64; j += 16) {
		R( 0); R( 1); R( 2); R( 3);
		R( 4); R( 5); R( 6); R( 7);
		R( 8); R( 9); R(10); R(11);
		R(12); R(13); R(14); R(15);
	}

	/* Add the working vars back into state[]. */
	state[0] += a(0);
	state[1] += b(0);
	state[2] += c(0);
	state[3] += d(0);
	state[4] += e(0);
	state[5] += f(0);
	state[6] += g(0);
	state[7] += h(0);
}

#undef S0
#undef S1
#undef s0
#undef s1

void fz_sha256_init(fz_sha256 *context)
{
	context->count[0] = context->count[1] = 0;

	context->state[0] = 0x6A09E667;
	context->state[1] = 0xBB67AE85;
	context->state[2] = 0x3C6EF372;
	context->state[3] = 0xA54FF53A;
	context->state[4] = 0x510E527F;
	context->state[5] = 0x9B05688C;
	context->state[6] = 0x1F83D9AB;
	context->state[7] = 0x5BE0CD19;
}

void fz_sha256_update(fz_sha256 *context, const unsigned char *input, size_t inlen)
{
	/* Copy the input data into a properly aligned temporary buffer.
	 * This way we can be called with arbitrarily sized buffers
	 * (no need to be multiple of 64 bytes), and the code works also
	 * on architectures that don't allow unaligned memory access. */
	while (inlen > 0)
	{
		const unsigned int copy_start = context->count[0] & 0x3F;
		unsigned int copy_size = 64 - copy_start;
		if (copy_size > inlen)
			copy_size = (unsigned int)inlen;

		memcpy(context->buffer.u8 + copy_start, input, copy_size);

		input += copy_size;
		inlen -= copy_size;
		context->count[0] += copy_size;
		/* carry overflow from low to high */
		if (context->count[0] < copy_size)
			context->count[1]++;

		if ((context->count[0] & 0x3F) == 0)
			transform256(context->state, context->buffer.u32);
	}
}

void fz_sha256_final(fz_sha256 *context, unsigned char digest[32])
{
	/* Add padding as described in RFC 3174 (it describes SHA-1 but
	 * the same padding style is used for SHA-256 too). */
	unsigned int j = context->count[0] & 0x3F;
	context->buffer.u8[j++] = 0x80;

	while (j != 56)
	{
		if (j == 64)
		{
			transform256(context->state, context->buffer.u32);
			j = 0;
		}
		context->buffer.u8[j++] = 0x00;
	}

	/* Convert the message size from bytes to bits. */
	context->count[1] = (context->count[1] << 3) + (context->count[0] >> 29);
	context->count[0] = context->count[0] << 3;

	if (!isbigendian())
	{
		context->buffer.u32[14] = bswap32(context->count[1]);
		context->buffer.u32[15] = bswap32(context->count[0]);
	}
	else
	{
		context->buffer.u32[14] = context->count[1];
		context->buffer.u32[15] = context->count[0];
	}
	transform256(context->state, context->buffer.u32);

	if (!isbigendian())
		for (j = 0; j < 8; j++)
			context->state[j] = bswap32(context->state[j]);

	memcpy(digest, &context->state[0], 32);
	memset(context, 0, sizeof(fz_sha256));
}

/* For SHA512 */

#define S0(x) (rotr(x, 28) ^ rotr(x, 34) ^ rotr(x, 39))
#define S1(x) (rotr(x, 14) ^ rotr(x, 18) ^ rotr(x, 41))
#define s0(x) (rotr(x, 1) ^ rotr(x, 8) ^ (x >> 7))
#define s1(x) (rotr(x, 19) ^ rotr(x, 61) ^ (x >> 6))

static const uint64_t SHA512_K[80] = {
	0x428A2F98D728AE22ULL, 0x7137449123EF65CDULL,
	0xB5C0FBCFEC4D3B2FULL, 0xE9B5DBA58189DBBCULL,
	0x3956C25BF348B538ULL, 0x59F111F1B605D019ULL,
	0x923F82A4AF194F9BULL, 0xAB1C5ED5DA6D8118ULL,
	0xD807AA98A3030242ULL, 0x12835B0145706FBEULL,
	0x243185BE4EE4B28CULL, 0x550C7DC3D5FFB4E2ULL,
	0x72BE5D74F27B896FULL, 0x80DEB1FE3B1696B1ULL,
	0x9BDC06A725C71235ULL, 0xC19BF174CF692694ULL,
	0xE49B69C19EF14AD2ULL, 0xEFBE4786384F25E3ULL,
	0x0FC19DC68B8CD5B5ULL, 0x240CA1CC77AC9C65ULL,
	0x2DE92C6F592B0275ULL, 0x4A7484AA6EA6E483ULL,
	0x5CB0A9DCBD41FBD4ULL, 0x76F988DA831153B5ULL,
	0x983E5152EE66DFABULL, 0xA831C66D2DB43210ULL,
	0xB00327C898FB213FULL, 0xBF597FC7BEEF0EE4ULL,
	0xC6E00BF33DA88FC2ULL, 0xD5A79147930AA725ULL,
	0x06CA6351E003826FULL, 0x142929670A0E6E70ULL,
	0x27B70A8546D22FFCULL, 0x2E1B21385C26C926ULL,
	0x4D2C6DFC5AC42AEDULL, 0x53380D139D95B3DFULL,
	0x650A73548BAF63DEULL, 0x766A0ABB3C77B2A8ULL,
	0x81C2C92E47EDAEE6ULL, 0x92722C851482353BULL,
	0xA2BFE8A14CF10364ULL, 0xA81A664BBC423001ULL,
	0xC24B8B70D0F89791ULL, 0xC76C51A30654BE30ULL,
	0xD192E819D6EF5218ULL, 0xD69906245565A910ULL,
	0xF40E35855771202AULL, 0x106AA07032BBD1B8ULL,
	0x19A4C116B8D2D0C8ULL, 0x1E376C085141AB53ULL,
	0x2748774CDF8EEB99ULL, 0x34B0BCB5E19B48A8ULL,
	0x391C0CB3C5C95A63ULL, 0x4ED8AA4AE3418ACBULL,
	0x5B9CCA4F7763E373ULL, 0x682E6FF3D6B2B8A3ULL,
	0x748F82EE5DEFB2FCULL, 0x78A5636F43172F60ULL,
	0x84C87814A1F0AB72ULL, 0x8CC702081A6439ECULL,
	0x90BEFFFA23631E28ULL, 0xA4506CEBDE82BDE9ULL,
	0xBEF9A3F7B2C67915ULL, 0xC67178F2E372532BULL,
	0xCA273ECEEA26619CULL, 0xD186B8C721C0C207ULL,
	0xEADA7DD6CDE0EB1EULL, 0xF57D4F7FEE6ED178ULL,
	0x06F067AA72176FBAULL, 0x0A637DC5A2C898A6ULL,
	0x113F9804BEF90DAEULL, 0x1B710B35131C471BULL,
	0x28DB77F523047D84ULL, 0x32CAAB7B40C72493ULL,
	0x3C9EBE0A15C9BEBCULL, 0x431D67C49C100D4CULL,
	0x4CC5D4BECB3E42B6ULL, 0x597F299CFC657E2AULL,
	0x5FCB6FAB3AD6FAECULL, 0x6C44198C4A475817ULL,
};

static void
transform512(uint64_t state[8], uint64_t data[16])
{
	const uint64_t *K = SHA512_K;
	uint64_t W[16];
	uint64_t T[8];
	unsigned int j;

	/* ensure big-endian integers */
	if (!isbigendian())
		for (j = 0; j < 16; j++)
			data[j] = bswap64(data[j]);

	/* Copy state[] to working vars. */
	memcpy(T, state, sizeof(T));

	/* 80 operations, partially loop unrolled */
	for (j = 0; j < 80; j+= 16) {
		R( 0); R( 1); R( 2); R( 3);
		R( 4); R( 5); R( 6); R( 7);
		R( 8); R( 9); R(10); R(11);
		R(12); R(13); R(14); R(15);
	}

	/* Add the working vars back into state[]. */
	state[0] += a(0);
	state[1] += b(0);
	state[2] += c(0);
	state[3] += d(0);
	state[4] += e(0);
	state[5] += f(0);
	state[6] += g(0);
	state[7] += h(0);
}

#undef S0
#undef S1
#undef s0
#undef s1

void fz_sha512_init(fz_sha512 *context)
{
	context->count[0] = context->count[1] = 0;

	context->state[0] = 0x6A09E667F3BCC908ull;
	context->state[1] = 0xBB67AE8584CAA73Bull;
	context->state[2] = 0x3C6EF372FE94F82Bull;
	context->state[3] = 0xA54FF53A5F1D36F1ull;
	context->state[4] = 0x510E527FADE682D1ull;
	context->state[5] = 0x9B05688C2B3E6C1Full;
	context->state[6] = 0x1F83D9ABFB41BD6Bull;
	context->state[7] = 0x5BE0CD19137E2179ull;
}

void fz_sha512_update(fz_sha512 *context, const unsigned char *input, size_t inlen)
{
	/* Copy the input data into a properly aligned temporary buffer.
	 * This way we can be called with arbitrarily sized buffers
	 * (no need to be multiple of 128 bytes), and the code works also
	 * on architectures that don't allow unaligned memory access. */
	while (inlen > 0)
	{
		const unsigned int copy_start = context->count[0] & 0x7F;
		unsigned int copy_size = 128 - copy_start;
		if (copy_size > inlen)
			copy_size = (unsigned int)inlen;

		memcpy(context->buffer.u8 + copy_start, input, copy_size);

		input += copy_size;
		inlen -= copy_size;
		context->count[0] += copy_size;
		/* carry overflow from low to high */
		if (context->count[0] < copy_size)
			context->count[1]++;

		if ((context->count[0] & 0x7F) == 0)
			transform512(context->state, context->buffer.u64);
	}
}

void fz_sha512_final(fz_sha512 *context, unsigned char digest[64])
{
	/* Add padding as described in RFC 3174 (it describes SHA-1 but
	 * the same padding style is used for SHA-512 too). */
	unsigned int j = context->count[0] & 0x7F;
	context->buffer.u8[j++] = 0x80;

	while (j != 112)
	{
		if (j == 128)
		{
			transform512(context->state, context->buffer.u64);
			j = 0;
		}
		context->buffer.u8[j++] = 0x00;
	}

	/* Convert the message size from bytes to bits. */
	context->count[1] = (context->count[1] << 3) + (context->count[0] >> 29);
	context->count[0] = context->count[0] << 3;

	if (!isbigendian())
	{
		context->buffer.u64[14] = bswap64(context->count[1]);
		context->buffer.u64[15] = bswap64(context->count[0]);
	}
	else
	{
		context->buffer.u64[14] = context->count[1];
		context->buffer.u64[15] = context->count[0];
	}
	transform512(context->state, context->buffer.u64);

	if (!isbigendian())
		for (j = 0; j < 8; j++)
			context->state[j] = bswap64(context->state[j]);

	memcpy(digest, &context->state[0], 64);
	memset(context, 0, sizeof(fz_sha512));
}

void fz_sha384_init(fz_sha384 *context)
{
	context->count[0] = context->count[1] = 0;

	context->state[0] = 0xCBBB9D5DC1059ED8ull;
	context->state[1] = 0x629A292A367CD507ull;
	context->state[2] = 0x9159015A3070DD17ull;
	context->state[3] = 0x152FECD8F70E5939ull;
	context->state[4] = 0x67332667FFC00B31ull;
	context->state[5] = 0x8EB44A8768581511ull;
	context->state[6] = 0xDB0C2E0D64F98FA7ull;
	context->state[7] = 0x47B5481DBEFA4FA4ull;
}

void fz_sha384_update(fz_sha384 *context, const unsigned char *input, size_t inlen)
{
	fz_sha512_update(context, input, inlen);
}

void fz_sha384_final(fz_sha384 *context, unsigned char digest[64])
{
	fz_sha512_final(context, digest);
}
