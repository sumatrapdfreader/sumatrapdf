/*
  factorial.h
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
  methods to return low-order factorials depending on allowed data types.

  20! = 2432902008176640000 is the maximum factorial to be held
  in a unsigned 64bit integer.
  13! = 479001600 is the maximum factorial to be held in a unsigned
  32bit integer.
*/

#ifndef _FACTORIAL_H_
#define _FACTORIAL_H_

#include "custom_math.h"

#define FACTORIAL_LOWER         \
  MK_ULL(1),                    \
  MK_ULL(1),                    \
  MK_ULL(2),                    \
  MK_ULL(6),                    \
  MK_ULL(24),                   \
  MK_ULL(120),                  \
  MK_ULL(720),                  \
  MK_ULL(5040),                 \
  MK_ULL(40320),                \
  MK_ULL(362880),               \
  MK_ULL(3628800),              \
  MK_ULL(39916800),             \
  MK_ULL(479001600)

#define FACTORIAL_HIGHER        \
  MK_ULL(6227020800),           \
  MK_ULL(87178291200),          \
  MK_ULL(1307674368000),        \
  MK_ULL(20922789888000),       \
  MK_ULL(355687428096000),      \
  MK_ULL(6402373705728000),     \
  MK_ULL(121645100408832000),   \
  MK_ULL(2432902008176640000)

static const cmath_std_uint_t _factorials[] =
{
  #ifdef _CMATH_ANSI
    FACTORIAL_LOWER
  #else
    FACTORIAL_LOWER,
    FACTORIAL_HIGHER
  #endif
};

static const cmath_t _inv_factorials[] =
{
  1.00000000000000000000000000000000,
  1.00000000000000000000000000000000,
  0.50000000000000000000000000000000,
  0.16666666666666666666666666666667,
  0.04166666666666666666666666666666,
  0.00833333333333333333333333333333,
  0.00138888888888888888888888888888,
  0.00019841269841269841269841269841,
  0.00002480158730158730158730158730,
  0.00000275573192239858906525573192,
  0.00000027557319223985890652557319,
  0.00000002505210838544171877505210,
  0.00000000208767569878680989792100,
  0.00000000016059043836821614599390,
  0.00000000001147074559772972471385,
  0.00000000000076471637318198164750,
  0.00000000000004779477332387385297,
  0.00000000000000281145725434552076,
  0.00000000000000015619206968586225,
  0.00000000000000000822063524662433,
  0.00000000000000000041103176233122
};

_CMATH_INLINE
cmath_std_uint_t factorial(const cmath_uint32_t x)
{
  if(x >= cmath_array_size(_factorials))
    return 0;
  else
    return _factorials[x];
}

_CMATH_INLINE
cmath_t inv_factorial(const cmath_uint32_t x)
{
  if(x >= cmath_array_size(_inv_factorials))
    return 0;
  else
    return _inv_factorials[x];
}

#endif /* _FACTORIAL_H_ */
