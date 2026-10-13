/* telepen.c - Handles Telepen and Telepen numeric */
/*
    libzint - the open source barcode library
    Copyright (C) 2008-2024 Robin Stuart <rstuart114@gmail.com>

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

/* Telepen Barcode Symbology information and History (BSiH)
   https://telepen.co.uk/wp-content/uploads/2018/10/Barcode-Symbology-information-and-History.pdf */

#define SODIUM_X_F        (IS_NUM_F | IS_UX__F | IS_LX__F) /* SODIUM "0123456789Xx" */

#include <stdio.h>
#include "common.h"

static const char TeleTable[128][16] = {
    { "31313131"       }, { "1131313111"     }, { "33313111"       }, { "1111313131"     },
    { "3111313111"     }, { "11333131"       }, { "13133131"       }, { "111111313111"   },
    { "31333111"       }, { "1131113131"     }, { "33113131"       }, { "1111333111"     },
    { "3111113131"     }, { "1113133111"     }, { "1311133111"     }, { "111111113131"   },
    { "3131113111"     }, { "11313331"       }, { "333331"         }, { "111131113111"   },
    { "31113331"       }, { "1133113111"     }, { "1313113111"     }, { "1111113331"     },
    { "31131331"       }, { "113111113111"   }, { "3311113111"     }, { "1111131331"     },
    { "311111113111"   }, { "1113111331"     }, { "1311111331"     }, { "11111111113111" },
    { "31313311"       }, { "1131311131"     }, { "33311131"       }, { "1111313311"     },
    { "3111311131"     }, { "11333311"       }, { "13133311"       }, { "111111311131"   },
    { "31331131"       }, { "1131113311"     }, { "33113311"       }, { "1111331131"     },
    { "3111113311"     }, { "1113131131"     }, { "1311131131"     }, { "111111113311"   },
    { "3131111131"     }, { "1131131311"     }, { "33131311"       }, { "111131111131"   },
    { "3111131311"     }, { "1133111131"     }, { "1313111131"     }, { "111111131311"   },
    { "3113111311"     }, { "113111111131"   }, { "3311111131"     }, { "111113111311"   },
    { "311111111131"   }, { "111311111311"   }, { "131111111311"   }, { "11111111111131" },
    { "3131311111"     }, { "11313133"       }, { "333133"         }, { "111131311111"   },
    { "31113133"       }, { "1133311111"     }, { "1313311111"     }, { "1111113133"     },
    { "313333"         }, { "113111311111"   }, { "3311311111"     }, { "11113333"       },
    { "311111311111"   }, { "11131333"       }, { "13111333"       }, { "11111111311111" },
    { "31311133"       }, { "1131331111"     }, { "33331111"       }, { "1111311133"     },
    { "3111331111"     }, { "11331133"       }, { "13131133"       }, { "111111331111"   },
    { "3113131111"     }, { "1131111133"     }, { "33111133"       }, { "111113131111"   },
    { "3111111133"     }, { "111311131111"   }, { "131111131111"   }, { "111111111133"   },
    { "31311313"       }, { "113131111111"   }, { "3331111111"     }, { "1111311313"     },
    { "311131111111"   }, { "11331313"       }, { "13131313"       }, { "11111131111111" },
    { "3133111111"     }, { "1131111313"     }, { "33111313"       }, { "111133111111"   },
    { "3111111313"     }, { "111313111111"   }, { "131113111111"   }, { "111111111313"   },
    { "313111111111"   }, { "1131131113"     }, { "33131113"       }, { "11113111111111" },
    { "3111131113"     }, { "113311111111"   }, { "131311111111"   }, { "111111131113"   },
    { "3113111113"     }, { "11311111111111" }, { "331111111111"   }, { "111113111113"   },
    { "31111111111111" }, { "111311111113"   }, { "131111111113"   },
    {'1','1','1','1','1','1','1','1','1','1','1','1','1','1','1','1'},
};

/* Generated by "backend/tests/test_telepen -f generate_lens -g" */
static const char TeleLens[128] = {
     8, 10,  8, 10, 10,  8,  8, 12,  8, 10,  8, 10, 10, 10, 10, 12,
    10,  8,  6, 12,  8, 10, 10, 10,  8, 12, 10, 10, 12, 10, 10, 14,
     8, 10,  8, 10, 10,  8,  8, 12,  8, 10,  8, 10, 10, 10, 10, 12,
    10, 10,  8, 12, 10, 10, 10, 12, 10, 12, 10, 12, 12, 12, 12, 14,
    10,  8,  6, 12,  8, 10, 10, 10,  6, 12, 10,  8, 12,  8,  8, 14,
     8, 10,  8, 10, 10,  8,  8, 12, 10, 10,  8, 12, 10, 12, 12, 12,
     8, 12, 10, 10, 12,  8,  8, 14, 10, 10,  8, 12, 10, 12, 12, 12,
    12, 10,  8, 14, 10, 12, 12, 12, 10, 14, 12, 12, 14, 12, 12, 16
};

INTERNAL int telepen(struct zint_symbol *symbol, unsigned char source[], int length) {
    int i, count, check_digit;
    int error_number;
    char dest[1145]; /* 12 (Start) + 69 * 16 (max for DELs) + 16 (Check) + 12 (stop) + 1 = 1145 */
    char *d = dest;

    error_number = 0;

    count = 0;

    if (length > 69) { /* 16 (Start) + 69 * 16 + 16 (Check) + 16 (Stop) = 1152 */
        return errtxtf(ZINT_ERROR_TOO_LONG, symbol, 390, "Input length %d too long (maximum 69)", length);
    }
    /* Start character */
    memcpy(d, TeleTable['_'], 12);
    d += 12;

    for (i = 0; i < length; i++) {
        if (source[i] > 127) {
            /* Cannot encode extended ASCII */
            return errtxtf(ZINT_ERROR_INVALID_DATA, symbol, 391,
                            "Invalid character at position %d in input, extended ASCII not allowed", i + 1);
        }
        memcpy(d, TeleTable[source[i]], TeleLens[source[i]]);
        d += TeleLens[source[i]];
        count += source[i];
    }

    check_digit = 127 - (count % 127);
    if (check_digit == 127) {
        check_digit = 0;
    }
    memcpy(d, TeleTable[check_digit], TeleLens[check_digit]);
    d += TeleLens[check_digit];

    if (symbol->debug & ZINT_DEBUG_PRINT) printf("Check digit: %d\n", check_digit);

    /* Stop character */
    memcpy(d, TeleTable['z'], 12);
    d += 12;

    expand(symbol, dest, d - dest);

    if (symbol->output_options & COMPLIANT_HEIGHT) {
        /* Default height from various Telepen docs is based on default 26pt at X 0.01125"
           (average of 0.01" - 0.0125") = (26 / 72) / 0.01125 ~ 32; no min height specified */
        (void) set_height(symbol, 0.0f, 32.0f, 0, 1 /*no_errtxt*/);
    } else {
        (void) set_height(symbol, 0.0f, 50.0f, 0, 1 /*no_errtxt*/);
    }

    for (i = 0; i < length; i++) {
        if (source[i] == '\0') {
            symbol->text[i] = ' ';
        } else {
            symbol->text[i] = source[i];
        }
    }
    symbol->text[length] = '\0';
    return error_number;
}

INTERNAL int telepen_num(struct zint_symbol *symbol, unsigned char source[], int length) {
    int count, check_digit, glyph;
    int error_number = 0;
    int i;
    char dest[1129]; /* 12 (Start) + 68 * 16 (max for DELs) + 16 (Check) + 12 (Stop) + 1 = 1129 */
    char *d = dest;
    unsigned char temp[137];

    count = 0;

    if (length > 136) { /* 68*2 */
        return errtxtf(ZINT_ERROR_TOO_LONG, symbol, 392, "Input length %d too long (maximum 136)", length);
    }
    if ((i = not_sane(SODIUM_X_F, source, length))) {
        return errtxtf(ZINT_ERROR_INVALID_DATA, symbol, 393,
                        "Invalid character at position %d in input (digits and \"X\" only)", i);
    }

    /* Add a leading zero if required */
    if (length & 1) {
        memcpy(temp + 1, source, length++);
        temp[0] = '0';
    } else {
        memcpy(temp, source, length);
    }
    temp[length] = '\0';
    to_upper(temp, length);

    /* Start character */
    memcpy(d, TeleTable['_'], 12);
    d += 12;

    for (i = 0; i < length; i += 2) {
        if (temp[i] == 'X') {
            return errtxtf(ZINT_ERROR_INVALID_DATA, symbol, 394, "Invalid odd position %d of \"X\" in Telepen data",
                            i + 1);
        }

        if (temp[i + 1] == 'X') {
            glyph = ctoi(temp[i]) + 17;
            count += glyph;
        } else {
            glyph = (10 * ctoi(temp[i])) + ctoi(temp[i + 1]);
            glyph += 27;
            count += glyph;
        }
        memcpy(d, TeleTable[glyph], TeleLens[glyph]);
        d += TeleLens[glyph];
    }

    check_digit = 127 - (count % 127);
    if (check_digit == 127) {
        check_digit = 0;
    }
    memcpy(d, TeleTable[check_digit], TeleLens[check_digit]);
    d += TeleLens[check_digit];

    if (symbol->debug & ZINT_DEBUG_PRINT) printf("Check digit: %d\n", check_digit);

    /* Stop character */
    memcpy(d, TeleTable['z'], 12);
    d += 12;

    expand(symbol, dest, d - dest);

    if (symbol->output_options & COMPLIANT_HEIGHT) {
        (void) set_height(symbol, 0.0f, 32.0f, 0, 1 /*no_errtxt*/); /* Same as alphanumeric Telepen */
    } else {
        (void) set_height(symbol, 0.0f, 50.0f, 0, 1 /*no_errtxt*/);
    }

    ustrcpy(symbol->text, temp);
    return error_number;
}

/* vim: set ts=4 sw=4 et : */
