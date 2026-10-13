/* general_field.c - Handles general field compaction (GS1 DataBar and composites) */
/*
    libzint - the open source barcode library
    Copyright (C) 2019-2024 Robin Stuart <rstuart114@gmail.com>

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

#include "common.h"
#include "general_field.h"

static const char alphanum_puncs[] = "*,-./";
static const char isoiec_puncs[] = "!\"%&'()*+,-./:;<=>?_ "; /* Note contains space, not in cset82 */
#define IS_ISOIEC_F (IS_LWR_F | IS_C82_F | IS_AST_F | IS_PLS_F | IS_MNS_F | IS_SPC_F)

/* Returns type of char at `i`. FNC1 counted as NUMERIC. Returns 0 if invalid char */
static int general_field_type(const char *general_field, const int i) {
    if (general_field[i] == '\x1D' || z_isdigit(general_field[i])) {
        return NUMERIC;
    }
    if (z_isupper(general_field[i]) || posn(alphanum_puncs, general_field[i]) != -1) {
        return ALPHANUMERIC;
    }
    if (!not_sane(IS_ISOIEC_F, (const unsigned char *) general_field + i, 1)) {
        return ISOIEC;
    }
    return 0;
}

/* Returns true if next (including `i`) `num` chars of type `type`, or if given (non-zero), `type2` */
static int general_field_next(const char *general_field, int i, const int general_field_len, int num, const int type,
            const int type2) {
    if (i + num > general_field_len) {
        return 0;
    }
    for (; i < general_field_len && num; i++, num--) {
        int type_i = general_field_type(general_field, i);
        if ((type_i != type && !type2) || (type_i != type && type_i != type2)) {
            return 0;
        }
    }
    return num == 0;
}

/* Returns true if next (including `i`) `num` up to `max_num` chars of type `type` and occur at end */
static int general_field_next_terminate(const char *general_field, int i, const int general_field_len, int num,
            const int max_num, const int type) {
    if (i + max_num < general_field_len) {
        return 0;
    }
    for (; i < general_field_len; i++, num--) {
        if (general_field_type(general_field, i) != type) {
            return 0;
        }
    }
    return i == general_field_len && num <= 0;
}

/* Returns true if none of the next (including `i`) `num` chars (or end occurs) of type `type` */
static int general_field_next_none(const char *general_field, int i, const int general_field_len, int num,
            const int type) {
    for (; i < general_field_len && num; i++, num--) {
        if (general_field_type(general_field, i) == type) {
            return 0;
        }
    }
    return num == 0 || i == general_field_len;
}

/* Attempts to apply encoding rules from sections 7.2.5.5.1 to 7.2.5.5.3
 * of ISO/IEC 24724:2011 (same as sections 5.4.1 to 5.4.3 of ISO/IEC 24723:2010) */
INTERNAL int general_field_encode(const char *general_field, const int general_field_len, int *p_mode,
                char *p_last_digit, char binary_string[], int *p_bp) {
    int i, d1, d2;
    int mode = *p_mode;
    char last_digit = '\0'; /* Set to odd remaining digit at end if any */
    int bp = *p_bp;

    for (i = 0; i < general_field_len; ) {
        int type = general_field_type(general_field, i);
        if (!type) {
            return 0;
        }
        switch (mode) {
            case NUMERIC:
                if (i < general_field_len - 1) { /* If at least 2 characters remain */
                    if (type != NUMERIC || general_field_type(general_field, i + 1) != NUMERIC) {
                        /* 7.2.5.5.1/5.4.1 a) */
                        bp = bin_append_posn(0, 4, binary_string, bp); /* Alphanumeric latch "0000" */
                        mode = ALPHANUMERIC;
                    } else {
                        d1 = general_field[i] == '\x1D' ? 10 : ctoi(general_field[i]);
                        d2 = general_field[i + 1] == '\x1D' ? 10 : ctoi(general_field[i + 1]);
                        bp = bin_append_posn((11 * d1) + d2 + 8, 7, binary_string, bp);
                        i += 2;
                    }
                } else { /* If 1 character remains */
                    if (type != NUMERIC) {
                        /* 7.2.5.5.1/5.4.1 b) */
                        bp = bin_append_posn(0, 4, binary_string, bp); /* Alphanumeric latch "0000" */
                        mode = ALPHANUMERIC;
                    } else {
                        /* Ending with single digit.
                         * 7.2.5.5.1 c) and 5.4.1 c) dealt with separately outside this procedure */
                        last_digit = general_field[i];
                        i++;
                    }
                }
                break;
            case ALPHANUMERIC:
                if (general_field[i] == '\x1D') {
                    /* 7.2.5.5.2/5.4.2 a) */
                    bp = bin_append_posn(15, 5, binary_string, bp); /* "01111" */
                    mode = NUMERIC;
                    i++;
                } else if (type == ISOIEC) {
                    /* 7.2.5.5.2/5.4.2 b) */
                    bp = bin_append_posn(4, 5, binary_string, bp); /* ISO/IEC 646 latch "00100" */
                    mode = ISOIEC;
                } else if (general_field_next(general_field, i, general_field_len, 6, NUMERIC, 0)) {
                    /* 7.2.5.5.2/5.4.2 c) */
                    bp = bin_append_posn(0, 3, binary_string, bp); /* Numeric latch "000" */
                    mode = NUMERIC;
                } else if (general_field_next_terminate(general_field, i, general_field_len, 4,
                            5 /*Can limit to 5 max due to above*/, NUMERIC)) {
                    /* 7.2.5.5.2/5.4.2 d) */
                    bp = bin_append_posn(0, 3, binary_string, bp); /* Numeric latch "000" */
                    mode = NUMERIC;
                } else if (z_isdigit(general_field[i])) {
                    bp = bin_append_posn(general_field[i] - 43, 5, binary_string, bp);
                    i++;
                } else if (z_isupper(general_field[i])) {
                    bp = bin_append_posn(general_field[i] - 33, 6, binary_string, bp);
                    i++;
                } else {
                    bp = bin_append_posn(posn(alphanum_puncs, general_field[i]) + 58, 6, binary_string, bp);
                    i++;
                }
                break;
            case ISOIEC:
                if (general_field[i] == '\x1D') {
                    /* 7.2.5.5.3/5.4.3 a) */
                    bp = bin_append_posn(15, 5, binary_string, bp); /* "01111" */
                    mode = NUMERIC;
                    i++;
                } else {
                    int next_10_not_isoiec = general_field_next_none(general_field, i, general_field_len, 10, ISOIEC);
                    if (next_10_not_isoiec && general_field_next(general_field, i, general_field_len, 4,
                                                NUMERIC, 0)) {
                        /* 7.2.5.5.3/5.4.3 b) */
                        bp = bin_append_posn(0, 3, binary_string, bp); /* Numeric latch "000" */
                        mode = NUMERIC;
                    } else if (next_10_not_isoiec && general_field_next(general_field, i, general_field_len, 5,
                                                        ALPHANUMERIC, NUMERIC)) {
                        /* 7.2.5.5.3/5.4.3 c) */
                        /* Note this rule can produce longer bitstreams if most of the alphanumerics are numeric */
                        bp = bin_append_posn(4, 5, binary_string, bp); /* Alphanumeric latch "00100" */
                        mode = ALPHANUMERIC;
                    } else if (z_isdigit(general_field[i])) {
                        bp = bin_append_posn(general_field[i] - 43, 5, binary_string, bp);
                        i++;
                    } else if (z_isupper(general_field[i])) {
                        bp = bin_append_posn(general_field[i] - 1, 7, binary_string, bp);
                        i++;
                    } else if (z_islower(general_field[i])) {
                        bp = bin_append_posn(general_field[i] - 7, 7, binary_string, bp);
                        i++;
                    } else {
                        bp = bin_append_posn(posn(isoiec_puncs, general_field[i]) + 232, 8, binary_string, bp);
                        i++;
                    }
                }
                break;
        }
    }

    *p_mode = mode;
    *p_last_digit = last_digit;
    *p_bp = bp;

    return 1;
}

/* vim: set ts=4 sw=4 et : */
