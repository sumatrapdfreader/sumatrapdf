/* This is a simple Reed-Solomon encoder */
/*
  (C) Cliff Hones 2004

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

/* It is not written with high efficiency in mind, so is probably
// not suitable for real-time encoding.  The aim was to keep it
// simple, general and clear.
//
// <Some notes on the theory and implementation need to be added here>

// Usage:
// First call rs_init_gf(&rs, prime_poly) to set up the Galois Field parameters.
// Then  call rs_init_code(&rs, nsym, index) to set the encoding size
// Then  call rs_encode(&rs, datalen, data, out) to encode the data.
//
// These can be called repeatedly as required - but note that
// rs_init_code must be called following any rs_init_gf call.
//
// If the parameters are fixed, some of the statics below can be
// replaced with constants in the obvious way, and additionally
// malloc/free can be avoided by using static arrays of a suitable
// size.
// Note: use of statics has been done for (up to) 8-bit tables.
*/

#include "common.h"
#include "reedsol.h"
#include "reedsol_logs.h"

/* rs_init_gf(&rs, prime_poly) initialises the parameters for the Galois Field.
// The symbol size is determined from the highest bit set in poly
// This implementation will support sizes up to 8 bits (see rs_uint_init_gf()
// for sizes > 8 bits and <= 30 bits) - bit sizes of 8 or 4 are typical
//
// The poly is the bit pattern representing the GF characteristic
// polynomial.  e.g. for ECC200 (8-bit symbols) the polynomial is
// a**8 + a**5 + a**3 + a**2 + 1, which translates to 0x12d.
*/

INTERNAL void rs_init_gf(rs_t *rs, const unsigned int prime_poly) {
    struct item {
        const unsigned char *logt;
        const unsigned char *alog;
    };
    /* To add a new prime poly of degree <= 8 add its details to this table and to the table in `test_generate()`
       in "backend/tests/test_reedsol.c" and regenerate the log tables by running
       "backend/tests/test_reedsol -f generate -g". Paste the result in "reedsol_logs.h" */
    static const struct item data[] = {
        { logt_0x13, alog_0x13 },   /* 0 000- */
        { logt_0x25, alog_0x25 },   /* 0 001- */
        { logt_0x43, alog_0x43 },   /* 0 010- */
        { NULL, NULL },
        { logt_0x89, alog_0x89 },   /* 0 100- */
        { NULL, NULL },
        { NULL, NULL },
        { NULL, NULL },
        { logt_0x11d, alog_0x11d }, /* 1 000- */
        { logt_0x12d, alog_0x12d }, /* 1 001- */
        { NULL, NULL },
        { logt_0x163, alog_0x163 }, /* 1 011- */
    };

    /* Using bits 9-6 as hash to save a few cycles */
    /* Alter this hash or just iterate if new prime poly added that doesn't fit */
    const unsigned int hash = prime_poly >> 5;

    rs->logt = data[hash].logt;
    rs->alog = data[hash].alog;
}

/* rs_init_code(&rs, nsym, index) initialises the Reed-Solomon encoder
// nsym is the number of symbols to be generated (to be appended
// to the input data).  index is usually 1 - it is the index of
// the constant in the first term (i) of the RS generator polynomial:
// (x + 2**i)*(x + 2**(i+1))*...   [nsym terms]
// For ECC200, index is 1.
*/

INTERNAL void rs_init_code(rs_t *rs, const int nsym, int index) {
    int i, k;
    const unsigned char *const logt = rs->logt;
    const unsigned char *const alog = rs->alog;
    unsigned char *rspoly = rs->rspoly;
    unsigned char *log_rspoly = rs->log_rspoly;

    rs->nsym = nsym;

    rspoly[0] = 1;
    for (i = 1; i <= nsym; i++) {
        rspoly[i] = 1;
        for (k = i - 1; k > 0; k--) {
            if (rspoly[k])
                rspoly[k] = alog[logt[rspoly[k]] + index]; /* Multiply coeff by 2**index */
            rspoly[k] ^= rspoly[k - 1]; /* Add coeff of x**(k-1) * x */
        }
        rspoly[0] = alog[logt[rspoly[0]] + index]; /* 2**(i + (i+1) + ... + index) */
        index++;
    }

    /* Set logs of poly and check if have zero coeffs */
    rs->zero = 0;
    for (i = 0; i <= nsym; i++) {
        log_rspoly[i] = logt[rspoly[i]]; /* For simplicity allow log of 0 */
        rs->zero |= rspoly[i] == 0;
    }
}

/* rs_encode(&rs, datalen, data, res) generates nsym Reed-Solomon codes (nsym as given in rs_init_code()) */
INTERNAL void rs_encode(const rs_t *rs, const int datalen, const unsigned char *data, unsigned char *res) {
    int i, k;
    const unsigned char *const logt = rs->logt;
    const unsigned char *const alog = rs->alog;
    const unsigned char *const rspoly = rs->rspoly;
    const unsigned char *const log_rspoly = rs->log_rspoly;
    const int nsym = rs->nsym;
    const int nsym_halved = nsym >> 1;

    memset(res, 0, nsym);
    if (rs->zero) { /* Poly has a zero coeff so need to check in inner loop */
        for (i = 0; i < datalen; i++) {
            const unsigned int m = res[nsym - 1] ^ data[i];
            if (m) {
                const unsigned int log_m = logt[m];
                for (k = nsym - 1; k > 0; k--) {
                    if (rspoly[k])
                        res[k] = (unsigned char) (res[k - 1] ^ alog[log_m + log_rspoly[k]]);
                    else
                        res[k] = res[k - 1];
                }
                res[0] = alog[log_m + log_rspoly[0]];
            } else {
                memmove(res + 1, res, nsym - 1);
                res[0] = 0;
            }
        }
    } else { /* Avoid a branch in inner loop */
        for (i = 0; i < datalen; i++) {
            const unsigned int m = res[nsym - 1] ^ data[i];
            if (m) {
                const unsigned int log_m = logt[m];
                for (k = nsym - 1; k > 0; k--) {
                    res[k] = (unsigned char) (res[k - 1] ^ alog[log_m + log_rspoly[k]]);
                }
                res[0] = alog[log_m + log_rspoly[0]];
            } else {
                memmove(res + 1, res, nsym - 1);
                res[0] = 0;
            }
        }
    }
    /* Reverse the result */
    for (i = 0; i < nsym_halved; i++) {
        const unsigned char tmp = res[i];
        res[i] = res[nsym - 1 - i];
        res[nsym - 1 - i] = tmp;
    }
}

/* The same as above but for unsigned int data and result - Aztec code compatible */

INTERNAL void rs_encode_uint(const rs_t *rs, const int datalen, const unsigned int *data, unsigned int *res) {
    int i, k;
    const unsigned char *const logt = rs->logt;
    const unsigned char *const alog = rs->alog;
    const unsigned char *const rspoly = rs->rspoly;
    const unsigned char *const log_rspoly = rs->log_rspoly;
    const int nsym = rs->nsym;
    const int nsym_halved = nsym >> 1;

    memset(res, 0, sizeof(unsigned int) * nsym);
    if (rs->zero) { /* Poly has a zero coeff so need to check in inner loop */
        for (i = 0; i < datalen; i++) {
            const unsigned int m = res[nsym - 1] ^ data[i];
            if (m) {
                const unsigned int log_m = logt[m];
                for (k = nsym - 1; k > 0; k--) {
                    if (rspoly[k])
                        res[k] = res[k - 1] ^ alog[log_m + log_rspoly[k]];
                    else
                        res[k] = res[k - 1];
                }
                res[0] = alog[log_m + log_rspoly[0]];
            } else {
                memmove(res + 1, res, sizeof(unsigned int) * (nsym - 1));
                res[0] = 0;
            }
        }
    } else { /* Avoid a branch in inner loop */
        for (i = 0; i < datalen; i++) {
            const unsigned int m = res[nsym - 1] ^ data[i];
            if (m) {
                const unsigned int log_m = logt[m];
                for (k = nsym - 1; k > 0; k--) {
                    res[k] = res[k - 1] ^ alog[log_m + log_rspoly[k]];
                }
                res[0] = alog[log_m + log_rspoly[0]];
            } else {
                memmove(res + 1, res, sizeof(unsigned int) * (nsym - 1));
                res[0] = 0;
            }
        }
    }
    /* Reverse the result */
    for (i = 0; i < nsym_halved; i++) {
        const unsigned int tmp = res[i];
        res[i] = res[nsym - 1 - i];
        res[nsym - 1 - i] = tmp;
    }
}

/* Versions of the above for bitlengths > 8 and <= 30 and unsigned int data and results - Aztec code compatible */

/* Usage:
// First call rs_uint_init_gf(&rs_uint, prime_poly, logmod) to set up the Galois Field parameters.
// Then  call rs_uint_init_code(&rs_uint, nsym, index) to set the encoding size
// Then  call rs_uint_encode(&rs_uint, datalen, data, out) to encode the data.
// Then  call rs_uint_free(&rs_uint) to free the log tables.
*/

/* `logmod` (field characteristic) will be 2**bitlength - 1, eg 1023 for bitlength 10, 4095 for bitlength 12 */
INTERNAL int rs_uint_init_gf(rs_uint_t *rs_uint, const unsigned int prime_poly, const int logmod) {
    int b, p, v;
    unsigned int *logt, *alog;

    b = logmod + 1;

    rs_uint->logt = NULL;
    rs_uint->alog = NULL;

    if (!(logt = (unsigned int *) calloc(b, sizeof(unsigned int)))) {
        return 0;
    }
    if (!(alog = (unsigned int *) calloc(b * 2, sizeof(unsigned int)))) {
        free(logt);
        return 0;
    }

    /* Calculate the log/alog tables */
    for (p = 1, v = 0; v < logmod; v++) {
        alog[v] = p;
        alog[logmod + v] = p; /* Double up, avoids mod */
        logt[p] = v;
        p <<= 1;
        if (p & b) /* If overflow */
            p ^= prime_poly; /* Subtract prime poly */
    }
    rs_uint->logt = logt;
    rs_uint->alog = alog;
    return 1;
}

INTERNAL void rs_uint_init_code(rs_uint_t *rs_uint, const int nsym, int index) {
    int i, k;
    const unsigned int *const logt = rs_uint->logt;
    const unsigned int *const alog = rs_uint->alog;
    unsigned short *rspoly = rs_uint->rspoly;
    unsigned int *log_rspoly = rs_uint->log_rspoly;

    if (logt == NULL || alog == NULL) {
        return;
    }
    rs_uint->nsym = nsym;

    rspoly[0] = 1;
    for (i = 1; i <= nsym; i++) {
        rspoly[i] = 1;
        for (k = i - 1; k > 0; k--) {
            if (rspoly[k])
                rspoly[k] = alog[(logt[rspoly[k]] + index)];
            rspoly[k] ^= rspoly[k - 1];
        }
        rspoly[0] = alog[(logt[rspoly[0]] + index)];
        index++;
    }

    /* Set logs of poly and check if have zero coeffs */
    rs_uint->zero = 0;
    for (i = 0; i <= nsym; i++) {
        log_rspoly[i] = logt[rspoly[i]]; /* For simplicity allow log of 0 */
        rs_uint->zero |= rspoly[i] == 0;
    }
}

INTERNAL void rs_uint_encode(const rs_uint_t *rs_uint, const int datalen, const unsigned int *data,
                unsigned int *res) {
    int i, k;
    const unsigned int *const logt = rs_uint->logt;
    const unsigned int *const alog = rs_uint->alog;
    const unsigned short *const rspoly = rs_uint->rspoly;
    const unsigned int *const log_rspoly = rs_uint->log_rspoly;
    const int nsym = rs_uint->nsym;
    const int nsym_halved = nsym >> 1;

    memset(res, 0, sizeof(unsigned int) * nsym);
    if (logt == NULL || alog == NULL) {
        return;
    }
    if (rs_uint->zero) { /* Poly has a zero coeff so need to check in inner loop */
        for (i = 0; i < datalen; i++) {
            const unsigned int m = res[nsym - 1] ^ data[i];
            if (m) {
                const unsigned int log_m = logt[m];
                for (k = nsym - 1; k > 0; k--) {
                    if (rspoly[k])
                        res[k] = res[k - 1] ^ alog[log_m + log_rspoly[k]];
                    else
                        res[k] = res[k - 1];
                }
                res[0] = alog[log_m + log_rspoly[0]];
            } else {
                memmove(res + 1, res, sizeof(unsigned int) * (nsym - 1));
                res[0] = 0;
            }
        }
    } else { /* Avoid a branch in inner loop */
        for (i = 0; i < datalen; i++) {
            const unsigned int m = res[nsym - 1] ^ data[i];
            if (m) {
                const unsigned int log_m = logt[m];
                for (k = nsym - 1; k > 0; k--) {
                    res[k] = res[k - 1] ^ alog[log_m + log_rspoly[k]];
                }
                res[0] = alog[log_m + log_rspoly[0]];
            } else {
                memmove(res + 1, res, sizeof(unsigned int) * (nsym - 1));
                res[0] = 0;
            }
        }
    }
    /* Reverse the result */
    for (i = 0; i < nsym_halved; i++) {
        const unsigned int tmp = res[i];
        res[i] = res[nsym - 1 - i];
        res[nsym - 1 - i] = tmp;
    }
}

INTERNAL void rs_uint_free(rs_uint_t *rs_uint) {
    if (rs_uint->logt) {
        free(rs_uint->logt);
        rs_uint->logt = NULL;
    }
    if (rs_uint->alog) {
        free(rs_uint->alog);
        rs_uint->alog = NULL;
    }
}

/* vim: set ts=4 sw=4 et : */
