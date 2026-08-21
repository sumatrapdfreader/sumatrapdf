#include "mupdf/fitz.h"

#include <assert.h>

/*
	Convert IEEE single precision numbers into decimal ASCII strings, while
	satisfying the following two properties:
	1) Calling strtof or '(float) strtod' on the result must produce the
	original float, independent of the rounding mode used by strtof/strtod.
	2) Minimize the number of produced decimal digits. E.g. the float 0.7f
	should convert to "0.7", not "0.69999999".

	To solve this we use a dedicated single precision version of
	Florian Loitsch's Grisu2 algorithm. See
	http://florian.loitsch.com/publications/dtoa-pldi2010.pdf?attredirects=0

	The code below is derived from Loitsch's C code, which
	implements the same algorithm for IEEE double precision. See
	http://florian.loitsch.com/publications/bench.tar.gz?attredirects=0
*/

/*
	Copyright (c) 2009 Florian Loitsch

	Permission is hereby granted, free of charge, to any person
	obtaining a copy of this software and associated documentation
	files (the "Software"), to deal in the Software without
	restriction, including without limitation the rights to use,
	copy, modify, merge, publish, distribute, sublicense, and/or sell
	copies of the Software, and to permit persons to whom the
	Software is furnished to do so, subject to the following
	conditions:

	The above copyright notice and this permission notice shall be
	included in all copies or substantial portions of the Software.

	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
	EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
	OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
	NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
	HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
	WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
	FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
	OTHER DEALINGS IN THE SOFTWARE.
*/

static uint32_t
float_to_uint32(float d)
{
	union
	{
		float d;
		uint32_t n;
	} tmp;
	tmp.d = d;
	return tmp.n;
}

typedef struct
{
	uint64_t f;
	int e;
} diy_fp_t;

#define DIY_SIGNIFICAND_SIZE 64
#define DIY_LEADING_BIT ((uint64_t) 1 << (DIY_SIGNIFICAND_SIZE - 1))

static diy_fp_t
minus(diy_fp_t x, diy_fp_t y)
{
	diy_fp_t result = {x.f - y.f, x.e};
	assert(x.e == y.e && x.f >= y.f);
	return result;
}

static diy_fp_t
multiply(diy_fp_t x, diy_fp_t y)
{
	uint64_t a, b, c, d, ac, bc, ad, bd, tmp;
	int half = DIY_SIGNIFICAND_SIZE / 2;
	diy_fp_t r; uint64_t mask = ((uint64_t) 1 << half) - 1;
	a = x.f >> half; b = x.f & mask;
	c = y.f >> half; d = y.f & mask;
	ac = a * c; bc = b * c; ad = a * d; bd = b * d;
	tmp = (bd >> half) + (ad & mask) + (bc & mask);
	tmp += ((uint64_t)1U) << (half - 1); /* Round. */
	r.f = ac + (ad >> half) + (bc >> half) + (tmp >> half);
	r.e = x.e + y.e + half * 2;
	return r;
}

#define SP_SIGNIFICAND_SIZE 23
#define SP_EXPONENT_BIAS (127 + SP_SIGNIFICAND_SIZE)
#define SP_MIN_EXPONENT (-SP_EXPONENT_BIAS)
#define SP_EXPONENT_MASK 0x7f800000
#define SP_SIGNIFICAND_MASK 0x7fffff
#define SP_HIDDEN_BIT 0x800000 /* 2^23 */

/* Does not normalize the result. */
static diy_fp_t
float2diy_fp(float d)
{
	uint32_t d32 = float_to_uint32(d);
	int biased_e = (d32 & SP_EXPONENT_MASK) >> SP_SIGNIFICAND_SIZE;
	uint32_t significand = d32 & SP_SIGNIFICAND_MASK;
	diy_fp_t res;

	if (biased_e != 0)
	{
		res.f = significand + SP_HIDDEN_BIT;
		res.e = biased_e - SP_EXPONENT_BIAS;
	}
	else
	{
		res.f = significand;
		res.e = SP_MIN_EXPONENT + 1;
	}
	return res;
}

static diy_fp_t
normalize_boundary(diy_fp_t in)
{
	diy_fp_t res = in;
	/* The original number could have been a denormal. */
	while (! (res.f & (SP_HIDDEN_BIT << 1)))
	{
		res.f <<= 1;
		res.e--;
	}
	/* Do the final shifts in one go. */
	res.f <<= (DIY_SIGNIFICAND_SIZE - SP_SIGNIFICAND_SIZE - 2);
	res.e = res.e - (DIY_SIGNIFICAND_SIZE - SP_SIGNIFICAND_SIZE - 2);
	return res;
}

static void
normalized_boundaries(float f, diy_fp_t* lower_ptr, diy_fp_t* upper_ptr)
{
	diy_fp_t v = float2diy_fp(f);
	diy_fp_t upper, lower;
	int significand_is_zero = v.f == SP_HIDDEN_BIT;

	upper.f = (v.f << 1) + 1; upper.e = v.e - 1;
	upper = normalize_boundary(upper);
	if (significand_is_zero)
	{
		lower.f = (v.f << 2) - 1;
		lower.e = v.e - 2;
	}
	else
	{
		lower.f = (v.f << 1) - 1;
		lower.e = v.e - 1;
	}
	lower.f <<= lower.e - upper.e;
	lower.e = upper.e;

	/* Adjust to double boundaries, so that we can also read the numbers with '(float) strtod'. */
	upper.f -= 1 << 10;
	lower.f += 1 << 10;

	*upper_ptr = upper;
	*lower_ptr = lower;
}

static int
k_comp(int n)
{
	/* Avoid ceil and floating point multiplication for better
	 * performance and portability. Instead use the approximation
	 * log10(2) ~ 1233/(2^12). Tests show that this gives the correct
	 * result for all values of n in the range -500..500. */
	int tmp = n + DIY_SIGNIFICAND_SIZE - 1;
	int k = (tmp * 1233) / (1 << 12);
	return tmp > 0 ? k + 1 : k;
}

/* Cached powers of ten from 10**-37..10**46. Produced using GNU MPFR's mpfr_pow_si. */

/* Significands. */
static uint64_t powers_ten[84] = {
	0x881cea14545c7575ull, 0xaa242499697392d3ull, 0xd4ad2dbfc3d07788ull,
	0x84ec3c97da624ab5ull, 0xa6274bbdd0fadd62ull, 0xcfb11ead453994baull,
	0x81ceb32c4b43fcf5ull, 0xa2425ff75e14fc32ull, 0xcad2f7f5359a3b3eull,
	0xfd87b5f28300ca0eull, 0x9e74d1b791e07e48ull, 0xc612062576589ddbull,
	0xf79687aed3eec551ull, 0x9abe14cd44753b53ull, 0xc16d9a0095928a27ull,
	0xf1c90080baf72cb1ull, 0x971da05074da7befull, 0xbce5086492111aebull,
	0xec1e4a7db69561a5ull, 0x9392ee8e921d5d07ull, 0xb877aa3236a4b449ull,
	0xe69594bec44de15bull, 0x901d7cf73ab0acd9ull, 0xb424dc35095cd80full,
	0xe12e13424bb40e13ull, 0x8cbccc096f5088ccull, 0xafebff0bcb24aaffull,
	0xdbe6fecebdedd5bfull, 0x89705f4136b4a597ull, 0xabcc77118461cefdull,
	0xd6bf94d5e57a42bcull, 0x8637bd05af6c69b6ull, 0xa7c5ac471b478423ull,
	0xd1b71758e219652cull, 0x83126e978d4fdf3bull, 0xa3d70a3d70a3d70aull,
	0xcccccccccccccccdull, 0x8000000000000000ull, 0xa000000000000000ull,
	0xc800000000000000ull, 0xfa00000000000000ull, 0x9c40000000000000ull,
	0xc350000000000000ull, 0xf424000000000000ull, 0x9896800000000000ull,
	0xbebc200000000000ull, 0xee6b280000000000ull, 0x9502f90000000000ull,
	0xba43b74000000000ull, 0xe8d4a51000000000ull, 0x9184e72a00000000ull,
	0xb5e620f480000000ull, 0xe35fa931a0000000ull, 0x8e1bc9bf04000000ull,
	0xb1a2bc2ec5000000ull, 0xde0b6b3a76400000ull, 0x8ac7230489e80000ull,
	0xad78ebc5ac620000ull, 0xd8d726b7177a8000ull, 0x878678326eac9000ull,
	0xa968163f0a57b400ull, 0xd3c21bcecceda100ull, 0x84595161401484a0ull,
	0xa56fa5b99019a5c8ull, 0xcecb8f27f4200f3aull, 0x813f3978f8940984ull,
	0xa18f07d736b90be5ull, 0xc9f2c9cd04674edfull, 0xfc6f7c4045812296ull,
	0x9dc5ada82b70b59eull, 0xc5371912364ce305ull, 0xf684df56c3e01bc7ull,
	0x9a130b963a6c115cull, 0xc097ce7bc90715b3ull, 0xf0bdc21abb48db20ull,
	0x96769950b50d88f4ull, 0xbc143fa4e250eb31ull, 0xeb194f8e1ae525fdull,
	0x92efd1b8d0cf37beull, 0xb7abc627050305aeull, 0xe596b7b0c643c719ull,
	0x8f7e32ce7bea5c70ull, 0xb35dbf821ae4f38cull, 0xe0352f62a19e306full,
};

/* Exponents. */
static int powers_ten_e[84] = {
	-186, -183, -180, -176, -173, -170, -166, -163, -160, -157, -153,
	-150, -147, -143, -140, -137, -133, -130, -127, -123, -120, -117,
	-113, -110, -107, -103, -100, -97, -93, -90, -87, -83, -80,
	-77, -73, -70, -67, -63, -60, -57, -54, -50, -47, -44,
	-40, -37, -34, -30, -27, -24, -20, -17, -14, -10, -7,
	-4, 0, 3, 6, 10, 13, 16, 20, 23, 26, 30,
	33, 36, 39, 43, 46, 49, 53, 56, 59, 63, 66,
	69, 73, 76, 79, 83, 86, 89
};

static diy_fp_t
cached_power(int i)
{
	diy_fp_t result;

	assert (i >= -37 && i <= 46);
	result.f = powers_ten[i + 37];
	result.e = powers_ten_e[i + 37];
	return result;
}

/* Returns buffer length. */
static int
digit_gen_mix_grisu2(diy_fp_t D_upper, diy_fp_t delta, char* buffer, int* K)
{
	int kappa;
	diy_fp_t one = {(uint64_t) 1 << -D_upper.e, D_upper.e};
	unsigned char p1 = D_upper.f >> -one.e;
	uint64_t p2 = D_upper.f & (one.f - 1);
	unsigned char div = 10;
	uint64_t mask = one.f - 1;
	int len = 0;
	for (kappa = 2; kappa > 0; --kappa)
	{
		unsigned char digit = p1 / div;
		if (digit || len)
			buffer[len++] = '0' + digit;
		p1 %= div; div /= 10;
		if ((((uint64_t) p1) << -one.e) + p2 <= delta.f)
		{
			*K += kappa - 1;
			return len;
		}
	}
	do
	{
		p2 *= 10;
		buffer[len++] = '0' + (p2 >> -one.e);
		p2 &= mask;
		kappa--;
		delta.f *= 10;
	}
	while (p2 > delta.f);
	*K += kappa;
	return len;
}

/*
	Compute decimal integer m, exp such that:
		f = m * 10^exp
		m is as short as possible without losing exactness
	Assumes special cases (0, NaN, +Inf, -Inf) have been handled.
*/
int
fz_grisu(float v, char* buffer, int* K)
{
	diy_fp_t w_lower, w_upper, D_upper, D_lower, c_mk, delta;
	int length, mk, alpha = -DIY_SIGNIFICAND_SIZE + 4;

	normalized_boundaries(v, &w_lower, &w_upper);
	mk = k_comp(alpha - w_upper.e - DIY_SIGNIFICAND_SIZE);
	c_mk = cached_power(mk);

	D_upper = multiply(w_upper, c_mk);
	D_lower = multiply(w_lower, c_mk);

	D_upper.f--;
	D_lower.f++;

	delta = minus(D_upper, D_lower);

	*K = -mk;
	length = digit_gen_mix_grisu2(D_upper, delta, buffer, K);

	buffer[length] = 0;
	return length;
}
