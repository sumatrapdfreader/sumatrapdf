#include "mupdf/fitz.h"

#include <assert.h>
#include <errno.h>
#include <float.h>

#ifndef INFINITY
#define INFINITY (DBL_MAX+DBL_MAX)
#endif
#ifndef NAN
#define NAN (INFINITY-INFINITY)
#endif

/*
   We use "Algorithm D" from "Contributions to a Proposed Standard for Binary
   Floating-Point Arithmetic" by Jerome Coonen (1984).

   The implementation uses a self-made floating point type, 'strtof_fp_t', with
   a 32-bit significand. The steps of the algorithm are

   INPUT: Up to 9 decimal digits d1, ... d9 and an exponent dexp.
   OUTPUT: A float corresponding to the number d1 ... d9 * 10^dexp.

   1) Convert the integer d1 ... d9 to an strtof_fp_t x.
   2) Lookup the strtof_fp_t  power = 10 ^ |dexp|.
   3) If dexp is positive set x = x * power, else set x = x / power. Use rounding mode 'round to odd'.
   4) Round x to a float using rounding mode 'to even'.

   Step 1) is always lossless as the strtof_fp_t's significand can hold a 9-digit integer.
   In the case |dexp| <= 13 the cached power is exact and the algorithm returns
   the exactly rounded result (with rounding mode 'to even').
   There is no double-rounding in 3), 4) as the multiply/divide uses 'round to odd'.

   For |dexp| > 13 the maximum error is bounded by (1/2 + 1/256) ulp.
   This is small enough to ensure that binary to decimal to binary conversion
   is the identity if the decimal format uses 9 correctly rounded significant digits.
*/
typedef struct strtof_fp_t
{
	uint32_t f;
	int e;
} strtof_fp_t;

/* Multiply/Divide x by y with 'round to odd'. Assume that x and y are normalized.  */

static strtof_fp_t
strtof_multiply(strtof_fp_t x, strtof_fp_t y)
{
	uint64_t tmp;
	strtof_fp_t res;

	assert(x.f & y.f & 0x80000000);

	res.e = x.e + y.e + 32;
	tmp = (uint64_t) x.f * y.f;
	/* Normalize.  */
	if ((tmp < ((uint64_t) 1 << 63)))
	{
		tmp <<= 1;
		--res.e;
	}

	res.f = tmp >> 32;

	/* Set the last bit of the significand to 1 if the result is
	   inexact. */
	if (tmp & 0xffffffff)
		res.f |= 1;
	return res;
}

static strtof_fp_t
divide(strtof_fp_t x, strtof_fp_t y)
{
	uint64_t product, quotient;
	uint32_t remainder;
	strtof_fp_t res;

	res.e = x.e - y.e - 32;
	product = (uint64_t) x.f << 32;
	quotient = product / y.f;
	remainder = product % y.f;
	/* 2^31 <= quotient <= 2^33 - 2.  */
	if (quotient <= 0xffffffff)
		res.f = quotient;
	else
	{
		++res.e;
		/* If quotient % 2 != 0 we have remainder != 0.  */
		res.f = quotient >> 1;
	}
	if (remainder)
		res.f |= 1;
	return res;
}

/* From 10^0 to 10^54. Generated with GNU MPFR.  */
static const uint32_t strtof_powers_ten[55] = {
	0x80000000, 0xa0000000, 0xc8000000, 0xfa000000, 0x9c400000, 0xc3500000,
	0xf4240000, 0x98968000, 0xbebc2000, 0xee6b2800, 0x9502f900, 0xba43b740,
	0xe8d4a510, 0x9184e72a, 0xb5e620f4, 0xe35fa932, 0x8e1bc9bf, 0xb1a2bc2f,
	0xde0b6b3a, 0x8ac72305, 0xad78ebc6, 0xd8d726b7, 0x87867832, 0xa968163f,
	0xd3c21bcf, 0x84595161, 0xa56fa5ba, 0xcecb8f28, 0x813f3979, 0xa18f07d7,
	0xc9f2c9cd, 0xfc6f7c40, 0x9dc5ada8, 0xc5371912, 0xf684df57, 0x9a130b96,
	0xc097ce7c, 0xf0bdc21b, 0x96769951, 0xbc143fa5, 0xeb194f8e, 0x92efd1b9,
	0xb7abc627, 0xe596b7b1, 0x8f7e32ce, 0xb35dbf82, 0xe0352f63, 0x8c213d9e,
	0xaf298d05, 0xdaf3f046, 0x88d8762c, 0xab0e93b7, 0xd5d238a5, 0x85a36367,
	0xa70c3c41
};
static const int strtof_powers_ten_e[55] = {
	-31, -28, -25, -22, -18, -15, -12, -8, -5, -2,
	2, 5, 8, 12, 15, 18, 22, 25, 28, 32, 35, 38, 42, 45, 48, 52, 55, 58, 62, 65,
	68, 71, 75, 78, 81, 85, 88, 91, 95, 98, 101, 105, 108, 111, 115, 118, 121,
	125, 128, 131, 135, 138, 141, 145, 148
};

static strtof_fp_t
strtof_cached_power(int i)
{
	strtof_fp_t result;
	assert (i >= 0 && i <= 54);
	result.f = strtof_powers_ten[i];
	result.e = strtof_powers_ten_e[i];
	return result;
}

/* Find number of leading zero bits in an uint32_t. Derived from the
   "Bit Twiddling Hacks" at graphics.stanford.edu/~seander/bithacks.html.  */
static unsigned char clz_table[256] = {
	8, 7, 6, 6, 5, 5, 5, 5, 4, 4, 4, 4, 4, 4, 4, 4,
# define sixteen_times(N) N, N, N, N, N, N, N, N, N, N, N, N, N, N, N, N,
	sixteen_times (3) sixteen_times (2) sixteen_times (2)
	sixteen_times (1) sixteen_times (1) sixteen_times (1) sixteen_times (1)
	/* Zero for the rest.  */
};
static unsigned
leading_zeros (uint32_t x)
{
	unsigned tmp1, tmp2;

	tmp1 = x >> 16;
	if (tmp1)
	{
		tmp2 = tmp1 >> 8;
		if (tmp2)
			return clz_table[tmp2];
		else
			return 8 + clz_table[tmp1];
	}
	else
	{
		tmp1 = x >> 8;
		if (tmp1)
			return 16 + clz_table[tmp1];
		else
			return 24 + clz_table[x];
	}
}

static strtof_fp_t
uint32_to_diy (uint32_t x)
{
	strtof_fp_t result = {x, 0};
	unsigned shift = leading_zeros(x);

	result.f <<= shift;
	result.e -= shift;
	return result;
}

#define SP_SIGNIFICAND_SIZE 23
#define SP_EXPONENT_BIAS (127 + SP_SIGNIFICAND_SIZE)
#define SP_MIN_EXPONENT (-SP_EXPONENT_BIAS)
#define SP_EXPONENT_MASK 0x7f800000
#define SP_SIGNIFICAND_MASK 0x7fffff
#define SP_HIDDEN_BIT 0x800000 /* 2^23 */

/* Convert normalized strtof_fp_t to IEEE-754 single with 'round to even'.
   See "Implementing IEEE 754-2008 Rounding" in the
   "Handbook of Floating-Point Arithmetik".
*/
static float
diy_to_float(strtof_fp_t x, int negative)
{
	uint32_t result;
	union
	{
		float f;
		uint32_t n;
	} tmp;

	assert(x.f & 0x80000000);

	/* We have 2^32 - 2^7 = 0xffffff80.  */
	if (x.e > 96 || (x.e == 96 && x.f >= 0xffffff80))
	{
		/* Overflow. Set result to infinity.  */
		errno = ERANGE;
		result = 0xff << SP_SIGNIFICAND_SIZE;
	}
	/* We have 2^32 - 2^8 = 0xffffff00.  */
	else if (x.e > -158)
	{
		/* x is greater or equal to FLT_MAX. So we get a normalized number. */
		result = (uint32_t) (x.e + 158) << SP_SIGNIFICAND_SIZE;
		result |= (x.f >> 8) & SP_SIGNIFICAND_MASK;

		if (x.f & 0x80)
		{
			/* Round-bit is set.  */
			if (x.f & 0x7f)
				/* Sticky-bit is set.  */
				++result;
			else if (x.f & 0x100)
				/* Significand is odd.  */
				++result;
		}
	}
	else if (x.e == -158 && x.f >= 0xffffff00)
	{
		/* x is in the range (2^32, 2^32 - 2^8] * 2^-158, so its smaller than
		   FLT_MIN but still rounds to it. */
		result = 1U << SP_SIGNIFICAND_SIZE;
	}
	else if (x.e > -181)
	{
		/* Non-zero Denormal.  */
		int shift = -149 - x.e; 	/* 9 <= shift <= 31.  */

		result = x.f >> shift;

		if (x.f & (1U << (shift - 1)))
			/* Round-bit is set.  */
		{
			if (x.f & ((1U << (shift - 1)) - 1))
				/* Sticky-bit is set.  */
				++result;
			else if (x.f & 1U << shift)
				/* Significand is odd. */
				++result;
		}
	}
	else if (x.e == -181 && x.f > 0x80000000)
	{
		/* x is in the range (0.5,1) *  2^-149 so it rounds to the smallest
		   denormal. Can't handle this in the previous case as shifting a
		   uint32_t 32 bits to the right is undefined behaviour.  */
		result = 1;
	}
	else
	{
		/* Underflow. */
		errno = ERANGE;
		result = 0;
	}

	if (negative)
		result |= 0x80000000;

	tmp.n = result;
	return tmp.f;
}

static float
scale_integer_to_float(uint32_t M, int N, int negative)
{
	strtof_fp_t result, x, power;

	if (M == 0)
		return negative ? -0.f : 0.f;
	if (N > 38)
	{
		/* Overflow.  */
		errno = ERANGE;
		return negative ? -INFINITY : INFINITY;
	}
	if (N < -54)
	{
		/* Underflow.  */
		errno = ERANGE;
		return negative ? -0.f : 0.f;
	}
	/* If N is in the range {-13, ..., 13} the conversion is exact.
	   Try to scale N into this region.  */
	while (N > 13 && M <= 0xffffffff / 10)
	{
		M *= 10;
		--N;
	}

	while (N < -13 && M % 10 == 0)
	{
		M /= 10;
		++N;
	}

	x = uint32_to_diy (M);
	if (N >= 0)
	{
		power = strtof_cached_power(N);
		result = strtof_multiply(x, power);
	}
	else
	{
		power = strtof_cached_power(-N);
		result = divide(x, power);
	}

	return diy_to_float(result, negative);
}

/* Return non-zero if *s starts with string (must be uppercase), ignoring case,
   and increment *s by its length.   */
static int
starts_with(const char **s, const char *string)
{
	const char *x = *s, *y = string;
	while (*x && *y && (*x == *y || *x == *y + 32))
		++x, ++y;
	if (*y == 0)
	{
		/* Match.  */
		*s = x;
		return 1;
	}
	else
		return 0;
}
#define SET_TAILPTR(tailptr, s)			\
	do					\
		if (tailptr)			\
			*tailptr = (char *) s;	\
	while (0)

/*
	Locale-independent decimal to binary
	conversion. On overflow return (-)INFINITY and set errno to ERANGE. On
	underflow return 0 and set errno to ERANGE. Special inputs (case
	insensitive): "NAN", "INF" or "INFINITY".
*/
float
fz_strtof(const char *string, char **tailptr)
{
	/* FIXME: error (1/2 + 1/256) ulp  */
	const char *s;
	uint32_t M = 0;
	int N = 0;
	/* If decimal_digits gets 9 we truncate all following digits.  */
	int decimal_digits = 0;
	int negative = 0;
	const char *number_start = 0;

	/* Skip leading whitespace (isspace in "C" locale).  */
	s = string;
	while (*s == ' ' || *s == '\f' || *s == '\n' || *s == '\r' || *s ==  '\t' || *s == '\v')
		++s;

	/* Parse sign.  */
	if (*s == '+')
		++s;
	if (*s == '-')
	{
		negative = 1;
		++s;
	}
	number_start = s;
	/* Parse digits before decimal point.  */
	while (*s >= '0' && *s <= '9')
	{
		if (decimal_digits)
		{
			if (decimal_digits < 9)
			{
				++decimal_digits;
				M = M * 10 + *s - '0';
			}
			/* Really arcane strings might overflow N.  */
			else if (N < 1000)
				++N;
		}
		else if (*s > '0')
		{
			M = *s - '0';
			++decimal_digits;
		}
		++s;
	}

	/* Parse decimal point.  */
	if (*s == '.')
		++s;

	/* Parse digits after decimal point. */
	while (*s >= '0' && *s <= '9')
	{
		if (decimal_digits < 9)
		{
			if (decimal_digits || *s > '0')
			{
				++decimal_digits;
				M = M * 10 + *s - '0';
			}
			--N;
		}
		++s;
	}
	if ((s  == number_start + 1 && *number_start == '.') || number_start == s)
	{
		/* No Number. Check for INF and NAN strings.  */
		s = number_start;
		if (starts_with(&s, "INFINITY") || starts_with(&s, "INF"))
		{
			errno = ERANGE;
			SET_TAILPTR(tailptr, s);
			return negative ? -INFINITY : +INFINITY;
		}
		else if (starts_with(&s, "NAN"))
		{
			SET_TAILPTR(tailptr, s);
			return (float)NAN;
		}
		else
		{
			SET_TAILPTR(tailptr, string);
			return 0.f;
		}
	}

	/* Parse exponent. */
	if (*s == 'e' || *s == 'E')
	{
		int exp_negative = 0;
		int exp = 0;
		const char *int_start;
		const char *exp_start = s;

		++s;
		if (*s == '+')
			++s;
		else if (*s == '-')
		{
			++s;
			exp_negative = 1;
		}
		int_start = s;
		/* Parse integer.  */
		while (*s >= '0' && *s <= '9')
		{
			/* Make sure exp does not get overflowed.  */
			if (exp < 100)
				exp = exp * 10 + *s - '0';
			++s;
		}
		if (exp_negative)
			exp = -exp;
		if (s == int_start)
			/* No Number.  */
			s = exp_start;
		else
			N += exp;
	}

	SET_TAILPTR(tailptr, s);
	return scale_integer_to_float(M, N, negative);
}
