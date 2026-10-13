/*
    This is a simple Reed-Solomon encoder
    (C) Cliff Hones 2004
*/
/*
    libzint - the open source barcode library
    Copyright (C) 2009-2022 Robin Stuart <rstuart114@gmail.com>

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

#ifndef Z_REEDSOL_H
#define Z_REEDSOL_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef struct {
    const unsigned char *logt; /* These are static */
    const unsigned char *alog;
    unsigned char rspoly[256]; /* Generated poly */
    unsigned char log_rspoly[256]; /* Logs of poly */
    int nsym; /* Degree of poly */
    int zero; /* Set if poly has a zero coeff */
} rs_t;

typedef struct {
    unsigned int *logt; /* These are malloced */
    unsigned int *alog;
    unsigned short rspoly[4096]; /* Generated poly, 12-bit max - needs to be enlarged if > 12-bit used */
    unsigned int log_rspoly[4096]; /* Logs of poly */
    int nsym; /* Degree of poly */
    int zero; /* Set if poly has a zero coeff */
} rs_uint_t;

INTERNAL void rs_init_gf(rs_t *rs, const unsigned int prime_poly);
INTERNAL void rs_init_code(rs_t *rs, const int nsym, int index);
INTERNAL void rs_encode(const rs_t *rs, const int datalen, const unsigned char *data, unsigned char *res);
INTERNAL void rs_encode_uint(const rs_t *rs, const int datalen, const unsigned int *data, unsigned int *res);
/* No free needed as log tables static */

INTERNAL int rs_uint_init_gf(rs_uint_t *rs_uint, const unsigned int prime_poly, const int logmod);
INTERNAL void rs_uint_init_code(rs_uint_t *rs_uint, const int nsym, int index);
INTERNAL void rs_uint_encode(const rs_uint_t *rs_uint, const int datalen, const unsigned int *data,
                unsigned int *res);
INTERNAL void rs_uint_free(rs_uint_t *rs_uint);

#ifdef __cplusplus
}
#endif /* __cplusplus */

/* vim: set ts=4 sw=4 et : */
#endif /* Z_REEDSOL_H */
