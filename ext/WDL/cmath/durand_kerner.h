/*
  durand_kerner.h
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
  durand-kerner (weierstrass) algorithm for finding complex roots
  of polynomials.
  accuracy depends a lot on data type precision.
*/

#ifndef _DURAND_KERNER_H_
#define _DURAND_KERNER_H_

#include "horner.h"
#include "custom_math.h"
#include "complex_number.h"

/* settings */
#ifdef _DURAND_KERNER_USE_INLINE_
  #define _DURAND_KERNER_INLINE _CMATH_INLINE
#else
  #define _DURAND_KERNER_INLINE
#endif

#define DK_EPSILON  1E-16
#define DK_MAX_ITR  1E+3
#define DK_MAX_N    256

const cnum_s dk_demoivre_c = {0.4, 0.9};

/* accepts an array of complex numbers */
_DURAND_KERNER_INLINE
void durand_kerner_c
(const cnum_s *coeff, cnum_s *roots, const cmath_uint16_t order)
{
  register cmath_uint16_t i, j;
  register cmath_uint32_t itr;
  cnum_s coeff_sc[DK_MAX_N];
  cnum_s x;
  cnum_s hor; /* needs an address or breaks g++ 4.x */

  i = 0;
  while(i < order)
  {
    cnum_from(&roots[i], cnum_pow(dk_demoivre_c, i));
    i++;
  }

  cnum_from(&coeff_sc[0], cnum_r1);
  i = 1;
  while(i < order+1)
  {
    cnum_from(&coeff_sc[i], cnum_div(coeff[i], coeff[0]));
    i++;
  }

  itr = 0;
  while(itr < DK_MAX_ITR)
  {
    i = 0;
    while(i < order)
    {
      j = 0;
      x = cnum_r1;
      while (j < order)
      {
        if (i != j)
          x = cnum_mul(cnum_sub(roots[i], roots[j]), x);
        j++;
      }
      hor = horner_eval_c(coeff_sc, roots[i], order);
      x = cnum_div(hor, x);
      x = cnum_sub(roots[i], x);
      if (cmath_abs(cmath_abs(x.r) - cmath_abs(roots[i].r)) < DK_EPSILON &&
          cmath_abs(cmath_abs(x.i) - cmath_abs(roots[i].i)) < DK_EPSILON)
        return;
      cnum_from(&roots[i], x);
      i++;
    }
    itr++;
  }
}

/* accepts an array of real numbers */
_DURAND_KERNER_INLINE
void durand_kerner
(const cmath_t *coeff, cnum_s *roots, const cmath_uint16_t order)
{
  register cmath_uint16_t i;
  cnum_s coeff_c[DK_MAX_N];
  i = 0;
  while(i < (order+1))
  {
    cnum_set(&coeff_c[i], coeff[i], 0);
    i++;
  }
  durand_kerner_c(coeff_c, roots, order);
}

#endif /* _DURAND_KERNER_H_ */
