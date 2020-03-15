/*
  horner.h
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
  algorithm to evaluate integer order polynomials using horner's scheme.
*/

#ifndef _HORNER_H_
#define _HORNER_H_

#include "custom_math.h"
#include "complex_number.h"

/* settings */
#ifndef _HORNER_INLINE
  #define _HORNER_INLINE _CMATH_INLINE
#else
  #define _HORNER_INLINE
#endif

/* real */
_HORNER_INLINE
cmath_t horner_eval
(const cmath_t *coeff, const cmath_t x, cmath_uint16_t order)
{
  register cmath_t y = coeff[0];
  register cmath_uint16_t n = 1;
  order += 1;
  while(n < order)
  {
    y = y*x + coeff[n];
    n++;
  }
  return y;
}

/* complex */
_HORNER_INLINE
cnum_s horner_eval_c
(const cnum_s *coeff, const cnum_s x, cmath_uint16_t order)
{
  register cmath_uint16_t n = 1;
  cnum_s y = coeff[0];
  order += 1;
  while(n < order)
  {
    y = cnum_add(cnum_mul(y, x), coeff[n]);
    n++;
  }
  return y;
}

#endif /* _HORNER_H_ */
