/*
  bessel_polynomial.h
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
  algorithm to calculate coefficients for a bessel polynomial from krall & fink
  series.
*/

#ifndef _BESSEL_POLYNOMIAL_H_
#define _BESSEL_POLYNOMIAL_H_

#include "custom_math.h"
#include "factorial.h"

#ifdef _BESSEL_USE_INLINE_
  #define _BESSEL_INLINE _CMATH_INLINE
#else
  #define _BESSEL_INLINE
#endif

#ifndef _CMATH_ANSI
  #define _BESSEL_MAX_ORDER 10
#else
  #define _BESSEL_MAX_ORDER 3
#endif

/* return a coefficient */
_BESSEL_INLINE cmath_std_int_t
bessel_coefficient(const cmath_uint16_t k, const cmath_uint16_t n)
{
  register cmath_std_int_t c;
  const cmath_uint16_t nmk = (cmath_uint16_t)(n - k);
  c = factorial(2*n - k);
  c /= (factorial(nmk)*factorial(k)) * (1 << nmk);
  return c;
}

/* calculate all coefficients for n-th order polynomial */
_BESSEL_INLINE
void bessel_polynomial( cmath_std_int_t *coeff,
                        const cmath_uint16_t order,
                        const cmath_uint16_t reverse )
{
  register cmath_uint16_t i = (cmath_uint16_t)(order + 1);
  if (reverse)
  {
    while (i--)
      coeff[order-i] = bessel_coefficient(i, order);
  }
  else
  {
    while (i--)
      coeff[i] = bessel_coefficient(i, order);
  }
}

#endif /* _BESSEL_POLYNOMIAL_H */
