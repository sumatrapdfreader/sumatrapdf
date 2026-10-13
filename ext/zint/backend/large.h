/* large.h - Handles binary manipulation of large numbers */

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

#ifndef Z_LARGE_H
#define Z_LARGE_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef struct { uint64_t lo; uint64_t hi; } large_uint;

#define large_lo(s) ((s)->lo)
#define large_hi(s) ((s)->hi)

/* Set 128-bit `t` from 128-bit `s` */
#define large_load(t, s) do { *(t) = *(s); } while (0)

/* Set 128-bit `t` from 64-bit `s` */
#define large_load_u64(t, s) do { (t)->lo = (s); (t)->hi = 0; } while (0)

/* Convert decimal string `s` of (at most) length `length` to 64-bit and place in 128-bit `t` */
INTERNAL void large_load_str_u64(large_uint *t, const unsigned char *s, const int length);

/* Add 128-bit `s` to 128-bit `t` */
INTERNAL void large_add(large_uint *t, const large_uint *s);
/* Add 64-bit `s` to 128-bit `t` */
INTERNAL void large_add_u64(large_uint *t, const uint64_t s);

/* Subtract 64-bit `s` from 128-bit `t` */
INTERNAL void large_sub_u64(large_uint *t, const uint64_t s);

/* Multiply 128-bit `t` by 64-bit `s` */
INTERNAL void large_mul_u64(large_uint *t, const uint64_t s);

/* Divide 128-bit dividend `t` by 64-bit divisor `v`, returning 64-bit remainder */
INTERNAL uint64_t large_div_u64(large_uint *t, uint64_t v);

/* Unset a bit (zero-based) */
INTERNAL void large_unset_bit(large_uint *t, const int bit);

/* Output large_uint into an unsigned int array of size `size`, each element containing `bits` bits */
INTERNAL void large_uint_array(const large_uint *t, unsigned int *uint_array, const int size, int bits);
/* As `large_uint_array()` above, except output to unsigned char array */
INTERNAL void large_uchar_array(const large_uint *t, unsigned char *uchar_array, const int size, int bits);

/* Format large_uint into buffer, which should be at least 35 chars in size */
INTERNAL char *large_dump(const large_uint *t, char *buf);
/* Output formatted large_uint to stdout */
INTERNAL void large_print(const large_uint *t);

#ifdef __cplusplus
}
#endif /* __cplusplus */

/* vim: set ts=4 sw=4 et : */
#endif /* Z_LARGE_H */
