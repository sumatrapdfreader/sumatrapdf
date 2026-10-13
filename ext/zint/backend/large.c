/* large.c - Handles binary manipulation of large numbers */
/*
    libzint - the open source barcode library
    Copyright (C) 2008-2023 Robin Stuart <rstuart114@gmail.com>

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions
    are met:

    1. Redistributions of source code must retain the above copyright
       notice, this list of conditions and the following disclaimer.
    2. Redistributions in binary form must reproduce the above copyright
       notice, this list of conditions and the following disclaimer in the
       documentation and/or other materials provided with the distribution.
    3. Neither the name of the project nor the names of its contributors
       may be used to endorse or promote products derived from this software
       without specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
    ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
    IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
    ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE
    FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
    DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
    OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
    HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
    LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
    OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
    SUCH DAMAGE.
 */
/* SPDX-License-Identifier: BSD-3-Clause */

/* `large_mul_u64()` and `large_div_u64()` are adapted from articles by F. W. Jacob
 *   https://www.codeproject.com/Tips/618570/UInt-Multiplication-Squaring
 *   "This article, along with any associated source code and files, is licensed under The BSD License"
 *   http://www.codeproject.com/Tips/785014/UInt-Division-Modulus
 *   "This article, along with any associated source code and files, is licensed under The BSD License"
 *
 * These in turn are based on Hacker's Delight (2nd Edition, 2012) by Henry S. Warren, Jr.
 *   "You are free to use, copy, and distribute any of the code on this web site, whether modified by you or not."
 *   https://web.archive.org/web/20190716204559/http://www.hackersdelight.org/permissions.htm
 *
 * `clz_u64()` and other bits and pieces are adapted from r128.h by Alan Hickman (fahickman)
 *   https://github.com/fahickman/r128/blob/master/r128.h
 *   "R128 is released into the public domain. See LICENSE for details." LICENSE is The Unlicense.
 */
#include <assert.h>
#include <stdio.h>
#include "common.h"
#include "large.h"

#define MASK32  0xFFFFFFFF

/* Convert decimal string `s` of (at most) length `length` to 64-bit and place in 128-bit `t` */
INTERNAL void large_load_str_u64(large_uint *t, const unsigned char *s, const int length) {
    uint64_t val = 0;
    const unsigned char *const se = s + length;
    for (; s < se && z_isdigit(*s); s++) {
        val *= 10;
        val += *s - '0';
    }
    t->lo = val;
    t->hi = 0;
}

/* Add 128-bit `s` to 128-bit `t` */
INTERNAL void large_add(large_uint *t, const large_uint *s) {
    t->lo += s->lo;
    t->hi += s->hi + (t->lo < s->lo);
}

/* Add 64-bit `s` to 128-bit `t` */
INTERNAL void large_add_u64(large_uint *t, const uint64_t s) {
    t->lo += s;
    if (t->lo < s) {
        t->hi++;
    }
}

/* Subtract 64-bit `s` from 128-bit `t` */
INTERNAL void large_sub_u64(large_uint *t, const uint64_t s) {
    uint64_t r = t->lo - s;
    if (r > t->lo) {
        t->hi--;
    }
    t->lo = r;
}

/* Multiply 128-bit `t` by 64-bit `s`
 * See Jacob `mult64to128()` and Warren Section 8-2
 * Note '0' denotes low 32-bits, '1' high 32-bits
 * if   p00 == s0 * tlo0
 *      k00 == carry of p00
 *      p01 == s0 * tlo1
 *      k01 == carry of (p01 + k00)
 *      p10 == s1 * tlo0
 *      k10 == carry of p10
 *      p11 == s1 * tlo1 (unmasked, i.e. including unshifted carry if any)
 * then t->lo == (p01 + p10 + k00) << 32 + p00
 * and  t->hi == p11 + k10 + k01 + thi * s
 *
 *      (thi)      tlo1      tlo0
 * x                 s1        s0
 *      -------------------------
 *                            p00
 *      k01        p01 + k00
 *                 p10
 *      p11 + k10
 */
INTERNAL void large_mul_u64(large_uint *t, const uint64_t s) {
    uint64_t thi = t->hi;
    uint64_t tlo0 = t->lo & MASK32;
    uint64_t tlo1 = t->lo >> 32;

    uint64_t s0 = s & MASK32;
    uint64_t s1 = s >> 32;

    uint64_t tmp = s0 * tlo0; /* p00 (unmasked) */
    uint64_t p00 = tmp & MASK32;
    uint64_t k10;

    tmp = (s1 * tlo0) + (tmp >> 32); /* (p10 + k00) (p10 unmasked) */
    k10 = tmp >> 32;

    tmp = (s0 * tlo1) + (tmp & MASK32); /* (p01 + p10 + k00) (p01 unmasked) */

    t->lo = (tmp << 32) + p00; /* (p01 + p10 + k00) << 32 + p00 (note any carry from unmasked p01 shifted out) */
    t->hi = (s1 * tlo1) + k10 + (tmp >> 32) + thi * s; /* p11 + k10 + k01 + thi * s */
}

/* Count leading zeroes. See Hickman `r128__clz64()` */
static int clz_u64(uint64_t x) {
   uint64_t n = 64, y;
   y = x >> 32; if (y) { n -= 32; x = y; }
   y = x >> 16; if (y) { n -= 16; x = y; }
   y = x >>  8; if (y) { n -=  8; x = y; }
   y = x >>  4; if (y) { n -=  4; x = y; }
   y = x >>  2; if (y) { n -=  2; x = y; }
   y = x >>  1; if (y) { n -=  1; x = y; }
   return (int) (n - x);
}

#ifdef ZINT_TEST /* Wrapper for direct testing */
INTERNAL int clz_u64_test(uint64_t x) {
    return clz_u64(x);
}
#endif

/* Divide 128-bit dividend `t` by 64-bit divisor `v`, returning 64-bit remainder
 * See Jacob `divmod128by128/64()` and Warren Section 9–2 (divmu64.c.txt)
 * Note digits are 32-bit parts */
INTERNAL uint64_t large_div_u64(large_uint *t, uint64_t v) {
    const uint64_t b = 0x100000000; /* Number base (2**32) */
    uint64_t qhi = 0; /* High digit of returned quotient */

    uint64_t tnhi, tnlo, tnlo1, tnlo0, vn1, vn0; /* Normalized forms of (parts of) t and v */
    uint64_t rnhilo1; /* Remainder after dividing 1st 3 digits of t by v */
    uint64_t qhat1, qhat0; /* Estimated quotient digits */
    uint64_t rhat; /* Remainder of estimated quotient digit */
    uint64_t tmp;
    int norm_shift;

    /* Deal with single-digit (i.e. 32-bit) divisor here */
    if (v < b) {
        qhi = t->hi / v;
        tmp = ((t->hi - qhi * v) << 32) + (t->lo >> 32); /* k * b + tlo1 */
        qhat1 = tmp / v;
        tmp = ((tmp - qhat1 * v) << 32) + (t->lo & MASK32); /* k * b + tlo0 */
        qhat0 = tmp / v;
        t->lo = (qhat1 << 32) | qhat0;
        t->hi = qhi;
        return tmp - qhat0 * v;
    }

    /* Main algorithm requires t->hi < v */
    if (t->hi >= v) {
        qhi = t->hi / v;
        t->hi %= v;
    }

    /* Normalize by shifting v left just enough so that its high-order
     * bit is on, and shift t left the same amount. Note don't need extra
     * high-end digit for dividend as t->hi < v */

    norm_shift = clz_u64(v);
    v <<= norm_shift;
    vn1 = v >> 32;
    vn0 = v & MASK32;

    if (norm_shift > 0) {
        tnhi = (t->hi << norm_shift) | (t->lo >> (64 - norm_shift));
        tnlo = t->lo << norm_shift;
    } else {
        tnhi = t->hi;
        tnlo = t->lo;
    }

    tnlo1 = tnlo >> 32;
    tnlo0 = tnlo & MASK32;

    /* Compute qhat1 estimate */

    assert(vn1 != 0); /* Suppress clang-tidy-14 clang-analyzer-core.DivideZero */
    qhat1 = tnhi / vn1; /* Divide first digit of v into first 2 digits of t */
    rhat = tnhi % vn1;

    /* Loop until qhat1 one digit and <= (rhat * b + 3rd digit of t) / vn0 */
    for (tmp = qhat1 * vn0; qhat1 >= b || tmp > (rhat << 32) + tnlo1; tmp -= vn0) {
        --qhat1;
        rhat += vn1;
        if (rhat >= b) { /* Must check here as (rhat << 32) would overflow */
            break; /* qhat1 * vn0 < b * b (since vn0 < b) */
        }
    }
    /* Note qhat1 will be exact as have fully divided by 2-digit divisor
     * (can only be too high by 1 (and require "add back" step) if divisor at least 3 digits) */

    /* Note high digit (if any) of both tnhi and (qhat1 * v) shifted out */
    rnhilo1 = (tnhi << 32) + tnlo1 - (qhat1 * v);

    /* Compute qhat0 estimate */

    qhat0 = rnhilo1 / vn1; /* Divide first digit of v into 2-digit remains of first 3 digits of t */
    rhat = rnhilo1 % vn1;

    /* Loop until qhat0 one digit and <= (rhat * b + 4th digit of t) / vn0 */
    for (tmp = qhat0 * vn0; qhat0 >= b || tmp > (rhat << 32) + tnlo0; tmp -= vn0) {
        --qhat0;
        rhat += vn1;
        if (rhat >= b) {
            break;
        }
    }
    /* Similarly qhat0 will be exact */

    t->lo = (qhat1 << 32) | qhat0;
    t->hi = qhi;

    /* Unnormalize remainder */
    return ((rnhilo1 << 32) + tnlo0 - (qhat0 * v)) >> norm_shift;
}

/* Unset a bit (zero-based) */
INTERNAL void large_unset_bit(large_uint *t, const int bit) {
    if (bit < 64) {
        t->lo &= ~(((uint64_t) 1) << bit);
    } else if (bit < 128) {
        t->hi &= ~(((uint64_t) 1) << (bit - 64));
    }
}

/* Output large_uint into an unsigned int array of size `size`, each element containing `bits` bits */
INTERNAL void large_uint_array(const large_uint *t, unsigned int *uint_array, const int size, int bits) {
    int i, j;
    uint64_t mask;
    if (bits <= 0) {
        bits = 8;
    } else if (bits > 32) {
        bits = 32;
    }
    mask = ~(((uint64_t) -1) << bits);
    for (i = 0, j = 0; i < size && j < 64; i++, j += bits) {
        uint_array[size - 1 - i] = (unsigned int) ((t->lo >> j) & mask); /* Little-endian order */
    }
    if (i < size) {
        if (j != 64) {
            j -= 64;
            /* (first j bits of t->hi) << (bits - j) | (last (bits - j) bits of t->lo) */
            uint_array[size - i] = (unsigned int) (((t->hi & ~((((uint64_t) -1) << j))) << (bits - j))
                                                    | (t->lo >> (64 - (bits - j)) & mask));
        } else {
            j = 0;
        }
        for (; i < size && j < 64; i++, j += bits) {
            uint_array[size - 1 - i] = (unsigned int) ((t->hi >> j) & mask);
        }
        if (i < size) {
            memset(uint_array, 0, sizeof(unsigned int) * (size - i));
        }
    }
}

/* As `large_uint_array()` above, except output to unsigned char array */
INTERNAL void large_uchar_array(const large_uint *t, unsigned char *uchar_array, const int size, int bits) {
    int i;
    unsigned int *uint_array = (unsigned int *) z_alloca(sizeof(unsigned int) * (size ? size : 1));

    large_uint_array(t, uint_array, size, bits);

    for (i = 0; i < size; i++) {
        uchar_array[i] = (unsigned char) uint_array[i];
    }
}

/* Format large_uint into buffer, which should be at least 35 chars in size */
INTERNAL char *large_dump(const large_uint *t, char *buf) {
    unsigned int tlo1 = (unsigned int) (large_lo(t) >> 32);
    unsigned int tlo0 = (unsigned int) (large_lo(t) & MASK32);
    unsigned int thi1 = (unsigned int) (large_hi(t) >> 32);
    unsigned int thi0 = (unsigned int) (large_hi(t) & MASK32);

    if (thi1) {
        sprintf(buf, "0x%X%08X%08X%08X", thi1, thi0, tlo1, tlo0);
    } else if (thi0) {
        sprintf(buf, "0x%X%08X%08X", thi0, tlo1, tlo0);
    } else if (tlo1) {
        sprintf(buf, "0x%X%08X", tlo1, tlo0);
    } else {
        sprintf(buf, "0x%X", tlo0);
    }
    return buf;
}

/* Output formatted large_uint to stdout */
INTERNAL void large_print(const large_uint *t) {
    char buf[35]; /* 2 (0x) + 32 (hex) + 1 */

    puts(large_dump(t, buf));
}

/* vim: set ts=4 sw=4 et : */
