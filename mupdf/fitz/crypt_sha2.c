/*
This code is based on the code found from 7-Zip, which has a modified
version of the SHA-256 found from Crypto++ <http://www.cryptopp.com/>.
The code was modified a little to fit into liblzma and fitz.

This file has been put into the public domain.
You can do whatever you want with this file.
*/

#include "fitz-internal.h"

static inline int isbigendian(void)
{
	static const int one = 1;
	return *(char*)&one == 0;
}

static inline unsigned int bswap32(unsigned int num)
{
	if (!isbigendian())
	{
		return	( (((num) << 24))
			| (((num) << 8) & 0x00FF0000)
			| (((num) >> 8) & 0x0000FF00)
			| (((num) >> 24)) );
	}
	return num;
}

/* At least on x86, GCC is able to optimize this to a rotate instruction. */
#define rotr_32(num, amount) ((num) >> (amount) | (num) << (32 - (amount)))

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
	h(i) += S1(e(i)) + Ch(e(i), f(i), g(i)) + SHA256_K[i + j] \
		+ (j ? blk2(i) : blk0(i)); \
	d(i) += h(i); \
	h(i) += S0(a(i)) + Maj(a(i), b(i), c(i))

#define S0(x) (rotr_32(x, 2) ^ rotr_32(x, 13) ^ rotr_32(x, 22))
#define S1(x) (rotr_32(x, 6) ^ rotr_32(x, 11) ^ rotr_32(x, 25))
#define s0(x) (rotr_32(x, 7) ^ rotr_32(x, 18) ^ (x >> 3))
#define s1(x) (rotr_32(x, 17) ^ rotr_32(x, 19) ^ (x >> 10))

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
transform(unsigned int state[8], const unsigned int data_xe[16])
{
	unsigned int data[16];
	unsigned int W[16];
	unsigned int T[8];
	unsigned int j;

	/* ensure big-endian integers */
	for (j = 0; j < 16; j++)
		data[j] = bswap32(data_xe[j]);

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

void fz_sha256_update(fz_sha256 *context, const unsigned char *input, unsigned int inlen)
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
			copy_size = inlen;

		memcpy(context->buffer.u8 + copy_start, input, copy_size);

		input += copy_size;
		inlen -= copy_size;
		context->count[0] += copy_size;
		/* carry overflow from low to high */
		if (context->count[0] < copy_size)
			context->count[1]++;

		if ((context->count[0] & 0x3F) == 0)
			transform(context->state, context->buffer.u32);
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
			transform(context->state, context->buffer.u32);
			j = 0;
		}
		context->buffer.u8[j++] = 0x00;
	}

	/* Convert the message size from bytes to bits. */
	context->count[1] = (context->count[1] << 3) + (context->count[0] >> 29);
	context->count[0] = context->count[0] << 3;

	context->buffer.u32[14] = bswap32(context->count[1]);
	context->buffer.u32[15] = bswap32(context->count[0]);
	transform(context->state, context->buffer.u32);

	for (j = 0; j < 8; j++)
		((unsigned int *)digest)[j] = bswap32(context->state[j]);
	memset(context, 0, sizeof(fz_sha256));
}
