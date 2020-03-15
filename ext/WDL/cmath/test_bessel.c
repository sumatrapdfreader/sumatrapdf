/*
  test_bessel.h
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
  test bessel_polynomial.h and other related headers

  gcc -W -Wall -Wextra -ansi pedantic
  cl /W4 /Za

  reduced precisions for ansi c
*/

#include "stdio.h"
#include "custom_math.h"
#include "bessel_polynomial.h"
#include "durand_kerner.h"

int main(void)
{
  register cmath_uint16_t i = 0;
  register cmath_int16_t diff = 0;

  cmath_uint32_t in_order = _BESSEL_MAX_ORDER + 1;
  cmath_uint16_t order;
  const cmath_uint16_t reverse = 1;

  cmath_std_int_t coeff[_BESSEL_MAX_ORDER + 1];

  cnum_t dk_coeff[_BESSEL_MAX_ORDER + 1];
  cnum_s dk_roots[_BESSEL_MAX_ORDER];

  /* */
  #ifdef _CMATH_ANSI
    puts("\n\nansi c is: on");
  #else
    puts("\n\nansi c is: off");
  #endif

  /* */
  while (in_order > _BESSEL_MAX_ORDER)
  {
    printf("\nenter order of bessel polynomial (0 - %d): ", _BESSEL_MAX_ORDER);
    scanf("%u", &in_order);
  }

  order = (cmath_uint16_t)in_order;
  bessel_polynomial(coeff, order, reverse);

  printf("\norder [N]: %d", order);
  printf("\nreversed bessel: %d\n\n", reverse);
  printf("list of coefficients:\n");
  while (i <= order)
  {
    printf("order[%2d]: ", (order - i));
    printf("%"_CMATH_PR_STD_INT"\n", coeff[i]);
    i++;
  }
  puts("\npolynomial:");
  printf("y(x) = ");

  i = 0;
  while (i <= order)
  {
    diff = (cmath_int16_t)(order - i);
    if (diff > 0)
      if (coeff[i] > 1)
      {
        printf("%"_CMATH_PR_STD_INT, coeff[i]);
        if (diff > 1)
          printf("*x^%d + ", diff);
        else
          printf("*x + ");
      }
      else
        printf("x^%d + ", diff);
    else
      printf("%"_CMATH_PR_STD_INT"", coeff[i]);
    i++;
  }

  /* */
  puts("\n\nlist roots:");
  i = 0;
  while (i < order+1)
  {
    dk_coeff[i] = (cnum_t)coeff[i];
    i++;
  }

  durand_kerner(dk_coeff, dk_roots, order);

  i = 0;
  while (i < order)
  {
    printf("root[%2d]: %.15f \t % .15f*i\n",
      i+1, (double)dk_roots[i].r, (double)dk_roots[i].i);
    i++;
  }

  return 0;
}
