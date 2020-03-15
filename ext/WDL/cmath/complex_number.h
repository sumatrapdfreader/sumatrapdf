/*
  complex_number.h
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
  portable complex number operations
*/

#ifndef _COMPLEX_NUMBER_H_
#define _COMPLEX_NUMBER_H_

#include "custom_math.h"

/* settings */
#ifndef _CNUM_NO_INLINE
  #define _CNUM_INLINE _CMATH_INLINE
#else
  #define _CNUM_INLINE
#endif

#ifndef _CNUM_NO_ALIAS
  #define _CNUM_ALIAS _CMATH_MAY_ALIAS
#else
  #define _CNUM_ALIAS
#endif

/* types & constants */
#ifndef cnum_t
  #define cnum_t cmath_t
#endif

typedef struct
{
  cnum_t r _CMATH_ALIGN(8);
  cnum_t i _CMATH_ALIGN(8);
} _CNUM_ALIAS cnum_s;

const cnum_s cnum_zero  = {0, 0};
const cnum_s cnum_i1    = {0, 1};
const cnum_s cnum_r1    = {1, 0};
const cnum_s cnum_r2    = {2, 0};

/* methods */
#define _CNUM(r, i)     cnum_new(r, i)
#define _CNUMD(x, r, i) cnum_s x = {r, i}

_CNUM_INLINE
cnum_s cnum_set(cnum_s *x, const cnum_t r, const cnum_t i)
{
  x->r = r;
  x->i = i;
  return *x;
}

_CNUM_INLINE
cnum_s cnum_from(cnum_s *x, const cnum_s y)
{
  x->r = y.r;
  x->i = y.i;
  return *x;
}

_CNUM_INLINE
cnum_s cnum_new(const cnum_t r, const cnum_t i)
{
  cnum_s x;
  x.r = r;
  x.i = i;
  return x;
}

_CNUM_INLINE
cnum_s cnum_cartesian(const cnum_s x)
{
  return cnum_new(x.r * cmath_cos(x.i), x.r * cmath_sin(x.i));
}

_CNUM_INLINE
cnum_s cnum_polar(const cnum_s x)
{
  return cnum_new(cmath_cabs(x.r, x.i), cmath_carg(x.r, x.i));
}

_CNUM_INLINE
cnum_s cnum_conjugate(const cnum_s x)
{
  return cnum_new(x.r, -x.i);
}

_CNUM_INLINE
cnum_s cnum_negative(const cnum_s x)
{
  return cnum_new(-x.r, -x.i);
}

_CNUM_INLINE
cnum_s cnum_swap(const cnum_s x)
{
  return cnum_new(x.i, x.r);
}

_CNUM_INLINE
cnum_s cnum_add(const cnum_s x, const cnum_s y)
{
  return cnum_new(x.r + y.r, x.i + y.i);
}

_CNUM_INLINE
cnum_s cnum_add_r(const cnum_s x, const cnum_t y)
{
  return cnum_new(x.r + y, x.i + y);
}

_CNUM_INLINE
cnum_s cnum_sub(const cnum_s x, const cnum_s y)
{
  return cnum_new(x.r - y.r, x.i - y.i);
}

_CNUM_INLINE
cnum_s cnum_sub_r(register cnum_s x, const cnum_t y)
{
  return cnum_new(x.r - y, x.i - y);
}

_CNUM_INLINE
cnum_s cnum_r_sub(const cnum_t x, register cnum_s y)
{
  return cnum_new(x - y.r, x - y.i);
}

_CNUM_INLINE
cnum_s cnum_mul(const cnum_s x, const cnum_s y)
{
  return cnum_new(x.r*y.r - x.i*y.i, x.r*y.i + x.i*y.r);
}

_CNUM_INLINE
cnum_s cnum_mul_r(const cnum_s x, const cnum_t y)
{
  return cnum_new(x.r*y, x.i*y);
}

#define cnum_sqr(x) \
  cnum_mul(x, x)

_CNUM_INLINE
cnum_s cnum_div_r(const cnum_s x, const cnum_t y)
{
  return cnum_new(x.r/y, x.i/y);
}

_CNUM_INLINE
cnum_s cnum_r_div(const cnum_t x, const cnum_s y)
{
  return cnum_new(x/y.r, x/y.i);
}

_CNUM_INLINE
cnum_s cnum_div(const cnum_s x, const cnum_s y)
{
  return  cnum_div_r(cnum_mul(x, cnum_conjugate(y)),
                    (y.r*y.r + cmath_abs(y.i*y.i)));
}

#define cnum_inv(x) \
  cnum_div(cnum_r1, x)

#define _CNUM_CHECK_EXP_D_ \
  cmath_abs(deg - cmath_round(deg)) == 0

_CNUM_INLINE
cnum_s cnum_exp(const cnum_s x)
{
  cnum_t sin_i = cmath_sin(x.i);
  cnum_t cos_i = cmath_cos(x.i);
  const cnum_t exp_r = cmath_exp(x.r);

  #ifndef _CNUM_NO_CHECK_EXP_
    register cnum_t deg;

    if (x.r == 0)
      return cnum_zero;
    deg = x.i / cmath_pi;
    if (_CNUM_CHECK_EXP_D_)
      sin_i = 0;
    deg += 0.5;
    if (_CNUM_CHECK_EXP_D_)
      cos_i = 0;
    deg = x.i / cmath_pi2;
    if (_CNUM_CHECK_EXP_D_)
      cos_i = 1;
  #endif

  return cnum_new(exp_r*cos_i, exp_r*sin_i);
}

_CNUM_INLINE
cnum_s cnum_log_k(const cnum_s x, const cmath_int32_t k)
{
  return  cnum_new(cmath_log(cmath_cabs(x.r, x.i)),
                  (cmath_carg(x.r, x.i) + (cmath_pi2*k)));
}

#define cnum_log(x) \
  cnum_log_k(x, 0)

#define cnum_log_b_k(x, b, k) \
  cnum_div(cnum_log_k(x, k), cnum_log_k(b, k))

#define cnum_log_b(b, x) \
  cnum_div(cnum_log(x), cnum_log(b))

#define cnum_log2(x) \
  cnum_log_b(x, 2)

#define cnum_log2_k(x, k) \
  cnum_log_b_k(x, 2, k)

#define cnum_log10(x) \
  cnum_log_b(x, 2)

#define cnum_log10_k(x, k) \
  cnum_log_b_k(x, 10, k)

#define _CNUM_CHECK_POW_C_      \
  if (x.r == 0 && x.i == 0)     \
    return cnum_zero;           \
  if (y.r == 0 && y.i == 0)     \
    return cnum_r1              \

_CNUM_INLINE
cnum_s cnum_pow_c_k(const cnum_s x, const cnum_s y, const cmath_int32_t k)
{
  _CNUM_CHECK_POW_C_;
  return cnum_exp(cnum_mul(cnum_log_k(x, k), y));
}

_CNUM_INLINE
cnum_s cnum_pow_c(const cnum_s x, const cnum_s y)
{
  _CNUM_CHECK_POW_C_;
  return cnum_exp(cnum_mul(cnum_log(x), y));
}

_CNUM_INLINE
cnum_s cnum_pow(const cnum_s x, const cnum_t n)
{
  const cnum_t r_pow_n = cmath_pow(cmath_cabs(x.r, x.i), n);
  const cnum_t theta_n = cmath_carg(x.r, x.i) * n;
  if (n == 0)
    return cnum_new(1, 0);
  if (n == 1)
    return x;
  return cnum_new(r_pow_n * cmath_cos(theta_n), r_pow_n * cmath_sin(theta_n));
}

#define cnum_root_c_k(x, y, k) \
  cnum_exp(cnum_div(cnum_log_k(x, k), y))

#define cnum_root_c(x, y) \
  cnum_exp(cnum_div(cnum_log(x), y))

#define cnum_root(x, n) \
  cnum_pow(x, 1/n)

#define cnum_sqrt(x) \
  cnum_pow(x, 0.5)

#define cnum_sin(x) \
  cnum_new(cmath_sin((x).r)*cmath_cosh((x).i), \
  cmath_cos((x).r)*cmath_sinh((x).i))

#define cnum_sinh(x) \
  cnum_new(cmath_sinh(x.r)*cmath_cos(x.i), cmath_cosh(x.r)*sin(x.i))

#define cnum_cos(x) \
  cnum_new(cmath_cos(x.r)*cmath_cosh(x.i), -cmath_sin(x.r)*cmath_sinh(x.i))

#define cnum_cosh(x) \
  cnum_new(cmath_cosh(x.r)*cmath_cos(x.i), cmath_sinh(x.r)*cmath_sin(x.i))

#define cnum_tan(x) \
  cnum_div(cnum_sin(x), cnum_cos(x))

#define cnum_tanh(x) \
  cnum_div(cnum_sinh(x), cnum_cosh(x))

#define cnum_csc(x) \
  cnum_inv(cnum_sin(x))

#define cnum_sec(x) \
  cnum_inv(cnum_cos(x))

#define cnum_cotan(x) \
  cnum_inv(cnum_tan(x))

#define cnum_asin(x) \
  cnum_negative(cnum_mul(cnum_i1, cnum_log(cnum_add(cnum_mul(cnum_i1, x), \
    cnum_sqrt(cnum_sub(cnum_r1, cnum_sqr(x)))))))

#define cnum_acos(x) \
  cnum_negative(cnum_mul(cnum_i1, cnum_log(cnum_add(x, cnum_mul(cnum_i1, \
    cnum_sqrt(cnum_sub(cnum_r1, cnum_sqr(x))))))))

#define cnum_atan(x) \
  cnum_div(cnum_mul(cnum_i1, cnum_log(cnum_div(cnum_sub(cnum_r1, \
    cnum_mul(cnum_i1, x)), cnum_add(cnum_r1, cnum_mul(cnum_i1, x))))), cnum_r2)

#define cnum_acsc(x) \
  cnum_asin(cnum_inv(x))

#define cnum_asec(x) \
  cnum_acos(cnum_inv(x))

#define cnum_acot(x) \
  cnum_atan(cnum_inv(x))

#define cnum_csch(x) \
  cnum_inv(cnum_sinh(x))

#define cnum_sech(x) \
  cnum_inv(cnum_cosh(x))

#define cnum_coth(x) \
  cnum_inv(cnum_tanh(x))

#define cnum_asinh(x) \
  cnum_log(cnum_add(x, cnum_sqrt(cnum_add(cnum_r1, cnum_sqr(x)))))

#define cnum_acosh(x) \
  cnum_log(cnum_add(x, cnum_mul(cnum_sqrt(cnum_add(x, cnum_r1)), \
    cnum_sqrt(cnum_sub(x, cnum_r1)))))

#define cnum_atanh(x) \
  cnum_div(cnum_sub(cnum_log(cnum_add(cnum_r1, x)), \
    cnum_log(cnum_sub(cnum_r1, x))), cnum_r2)

#define cnum_acsch(x) \
  cnum_asinh(cnum_inv(x))

#define cnum_asech(x) \
  cnum_acosh(cnum_inv(x))

#define cnum_asech(x) \
  cnum_acosh(cnum_inv(x))

#define cnum_acoth(x) \
  cnum_atanh(cnum_inv(x))

#endif /* _COMPLEX_NUMBER_H_ */
