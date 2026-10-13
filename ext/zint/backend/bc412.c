/* bc412.c - Handles IBM BC412 (SEMI T1-95) symbology */
/*
    libzint - the open source barcode library
    Copyright (C) 2022-2024 Robin Stuart <rstuart114@gmail.com>

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

/* A little information about this symbology can be found at
 * https://barcodeguide.seagullscientific.com/Content/Symbologies/BC412.htm
 *
 * Partial specification at
 * https://www.wdfxw.net/doc80487518.htm
 *
 * Checked against the encoder at
 * https://www.barcodesoft.com/en/semi/semi-t1-95-bc-412-code */

#include <stdio.h>
#include "common.h"

static const char BROMINE[] = "0R9GLVHA8EZ4NTS1J2Q6C7DYKBUIX3FWP5M";

static const char BC412Table[35][8] = {
    {'1','1','1','1','1','1','1','5'}, {'1','3','1','1','1','2','1','2'},
    {'1','1','1','3','1','1','1','3'}, {'1','2','1','1','1','2','1','3'},
    {'1','2','1','2','1','3','1','1'}, {'1','3','1','3','1','1','1','1'},
    {'1','2','1','1','1','3','1','2'}, {'1','1','1','3','1','2','1','2'},
    {'1','1','1','2','1','4','1','1'}, {'1','1','1','5','1','1','1','1'},
    {'1','5','1','1','1','1','1','1'}, {'1','1','1','1','1','5','1','1'},
    {'1','2','1','3','1','2','1','1'}, {'1','3','1','2','1','1','1','2'},
    {'1','3','1','1','1','3','1','1'}, {'1','1','1','1','1','2','1','4'},
    {'1','2','1','2','1','1','1','3'}, {'1','1','1','1','1','3','1','3'},
    {'1','3','1','1','1','1','1','3'}, {'1','1','1','2','1','2','1','3'},
    {'1','1','1','4','1','1','1','2'}, {'1','1','1','2','1','3','1','2'},
    {'1','1','1','4','1','2','1','1'}, {'1','4','1','2','1','1','1','1'},
    {'1','2','1','2','1','2','1','2'}, {'1','1','1','3','1','3','1','1'},
    {'1','3','1','2','1','2','1','1'}, {'1','2','1','1','1','4','1','1'},
    {'1','4','1','1','1','2','1','1'}, {'1','1','1','1','1','4','1','2'},
    {'1','2','1','1','1','1','1','4'}, {'1','4','1','1','1','1','1','2'},
    {'1','2','1','4','1','1','1','1'}, {'1','1','1','2','1','1','1','4'},
    {'1','2','1','3','1','1','1','2'}
};

INTERNAL int bc412(struct zint_symbol *symbol, unsigned char source[], int length) { /* IBM BC412 */
    unsigned char padded_source[20];
    int posns[35];
    int i, counter_odd = 0, counter_even = 0, check_sum = 0;
    char dest[293]; /* 2 + (36 * 8) + 3 */
    char *d = dest;
    int error_number = 0;

    if (length > 18) {
        return errtxtf(ZINT_ERROR_TOO_LONG, symbol, 790, "Input length %d too long (maximum 18)", length);
    }
    if (length < 7) {
        return errtxtf(ZINT_ERROR_TOO_LONG, symbol, 792, "Input length %d too short (minimum 7)", length);
    }
    to_upper(source, length);

    padded_source[0] = source[0];
    padded_source[1] = '0';

    for (i = 2; i <= length; i++) {
        padded_source[i] = source[i - 1];
    }
    padded_source[length + 1] = 0;

    if ((i = not_sane_lookup(BROMINE, 35, padded_source, length + 1, posns))) {
        return errtxtf(ZINT_ERROR_INVALID_DATA, symbol, 791,
                        "Invalid character at position %d in input (alphanumerics only, excluding \"O\")", i - 1);
    }

    for (i = 0; i <= length; i++) {
        if (i % 2) {
            counter_even += posns[i];
        } else {
            counter_odd += posns[i];
        }
    }

    counter_odd %= 35;
    counter_even %= 35;

    /* Check digit */
    check_sum = counter_odd + (2 * counter_even);
    check_sum %= 35;
    check_sum *= 17;
    check_sum %= 35;

    if (symbol->debug & ZINT_DEBUG_PRINT) {
        printf("BC412 check: %c\n", BROMINE[check_sum]);
    }

    padded_source[1] = BROMINE[check_sum];
    posns[1] = check_sum;

    /* Start character */
    memcpy(d, "12", 2);
    d += 2;

    for (i = 0; i <= length; i++, d += 8) {
        memcpy(d, BC412Table[posns[i]], 8);
    }

    /* Stop character */
    memcpy(d, "111", 3);
    d += 3;

    expand(symbol, dest, d - dest);
    ustrcpy(symbol->text, padded_source);

    if (symbol->output_options & COMPLIANT_HEIGHT) {
        /* SEMI T1-95 Table 1 "Module" (Character) Height 2mm ± 0.025mm, using Module Spacing 0.12mm ± 0.025mm as
           X-dimension */
        const float min_height = 13.6206894f; /* 1.975 / 0.145 */
        const float default_height = 16.666666f; /* 2.0 / 0.12 */
        const float max_height = 21.3157902f; /* 2.025 / 0.095 */
        error_number = set_height(symbol, min_height, default_height, max_height, 0 /*no_errtxt*/);
    } else {
        /* Using compliant height as default as no backwards compatibility to consider */
        const float default_height = 16.666666f; /* 2.0 / 0.12 */
        (void) set_height(symbol, 0.0f, default_height, 0.0f, 1 /*no_errtxt*/);
    }

    return error_number;
}

/* vim: set ts=4 sw=4 et : */
