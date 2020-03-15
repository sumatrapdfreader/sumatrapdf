/*
  custom_math.h
  Copyright (C) 2011 and later Lubomir I. Ivanov (neolit123 [at] gmail)

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
*/

/*
  portable definitions for ansi c and cross compiler compatibility.
  contains: numeric constants, custom math functions, macros & other.
*/

#ifndef _CUSTOM_MATH_H_
#define _CUSTOM_MATH_H_

#include "math.h"

/* check for "c89" mode */
#if (defined _MSC_VER && defined __STDC__) || \
    (defined __GNUC__ && defined __STRICT_ANSI__)
  #define _CMATH_ANSI
#endif

/* enable inline */
#if defined __cplusplus || (!defined _CMATH_ANSI && defined _CMATH_USE_INLINE)
  #ifdef _MSC_VER
    #define _CMATH_INLINE __inline
  #else
    #define _CMATH_INLINE inline
  #endif
#else
  #define _CMATH_INLINE
#endif

/* align type to size of type */
#if defined __GNUC__ || defined __TINYC__
  #define _CMATH_ALIGN(x)   __attribute__ ((aligned(x)))
  #define _CMATH_ALIGN_T(x) __attribute__ ((aligned(sizeof(x))))
#else
  #define _CMATH_ALIGN(x)
  #define _CMATH_ALIGN_T(x)
#endif

/* printf max integer */
#ifndef _CMATH_ANSI
  #ifdef _WIN32
    #define _CMATH_PR_STD_UINT  "I64u"
    #define _CMATH_PR_STD_INT   "I64i"
    #define _CMATH_PR_STD_HEX   "I64x"
  #else
    #define _CMATH_PR_STD_UINT  "llu"
    #define _CMATH_PR_STD_INT   "lli"
    #define _CMATH_PR_STD_HEX   "llx"
  #endif
#else
  #define _CMATH_PR_STD_UINT  "u"
  #define _CMATH_PR_STD_INT   "d"
  #define _CMATH_PR_STD_HEX   "x"
#endif

/* msvc specifics */
#ifdef _MSC_VER
  #pragma warning(disable : 4514)

  #define MK_L(x)     (x)
  #define MK_UL(x)    (x)
  #define MK_LL(x)    (x)
  #define MK_ULL(x)   (x)
#else
  #define MK_L(x)     (x##L)
  #define MK_UL(x)    (x##UL)
  #ifdef _CMATH_ANSI
    #define MK_LL(x)  (x##L)
    #define MK_ULL(x) (x##UL)
  #else
    #define MK_LL(x)  (x##LL)
    #define MK_ULL(x) (x##ULL)
  #endif
#endif

/* definitions depending on c standard */
#ifdef _CMATH_ANSI
  #define cmath_std_signbit  MK_UL(0x7fffffff)
  #define cmath_std_float_t  float
  #define cmath_std_int_t    int
#else
  #define cmath_std_signbit  MK_ULL(0x7fffffffffffffff)
  #define cmath_std_float_t  double
  #ifdef _MSC_VER
    #define cmath_std_int_t  __int64
  #else
    #define cmath_std_int_t  long long
  #endif
#endif

/* types and constants */
#ifndef cmath_t
  #define cmath_t   double
#endif

#define cmath_std_uint_t  unsigned cmath_std_int_t

#define cmath_pi          3.1415926535897932384626433832795
#define cmath_pi2         6.2831853071795864769252867665590
#define cmath_pi_2        1.5707963267948966192313216916398
#define cmath_e           2.7182818284590452353602874713526
#define cmath_sqrt2       1.4142135623730950488016887242097
#define cmath_pi_180      0.0174532925199432957692369076848
#define cmath_180_pi      57.295779513082320876798154814105

#define cmath_int8_t      char
#define cmath_uint8_t     unsigned char
#define cmath_int16_t     short
#define cmath_uint16_t    unsigned short
#define cmath_int32_t     int
#define cmath_uint32_t    unsigned int

/* aliased types */
#ifdef __GNUC__
  #define _CMATH_MAY_ALIAS __attribute__((__may_alias__))
#else
  #define _CMATH_MAY_ALIAS
#endif

typedef cmath_t _CMATH_MAY_ALIAS cmath_t_a;

/* possible approximations */
#define cmath_sin     sin
#define cmath_cos     cos
#define cmath_tan     tan
#define cmath_asin    asin
#define cmath_acos    acos
#define cmath_atan    atan
#define cmath_atan2   atan2
#define cmath_sinh    sinh
#define cmath_cosh    cosh
#define cmath_tanh    tanh
#define cmath_exp     exp
#define cmath_pow     pow
#define cmath_sqrt    sqrt
#define cmath_log     log
#define cmath_log2    log2
#define cmath_log10   log10

/* methods */
#define cmath_array_size(x) \
  (sizeof(x) / sizeof(*(x)))

#define poly_order(x) \
  (sizeof(x) / sizeof(*(x)) - 1)

#define cmath_cabs(a, b) \
  cmath_sqrt((a)*(a) + (b)*(b))

#define cmath_carg(a, b) \
  cmath_atan2((b), (a))

#define cmath_radians(x) \
  ((x)*cmath_pi_180)

#define cmath_degrees(x) \
  ((x)*cmath_180_pi)

_CMATH_INLINE
cmath_t cmath_powi(const cmath_t x, register cmath_uint16_t n)
{
  register cmath_t result = 1;
  while (n--)
    result *= x;
  return result;
}

_CMATH_INLINE
cmath_t cmath_abs(const cmath_t x)
{
  register union
  {
    cmath_std_int_t   i;
    cmath_std_float_t j;
  } u;
  u.j = (cmath_std_float_t)x;
  u.i &= cmath_std_signbit;
  return u.j;
}

_CMATH_INLINE
cmath_t cmath_round(const cmath_t x)
{
   if (x < 0.0)
    return (cmath_t)(cmath_std_int_t)(x - 0.5);
  else
    return (cmath_t)(cmath_std_int_t)(x + 0.5);
}

#endif /* _CUSTOM_MATH_H_ */
