/* auspost.c - Handles Australia Post 4-State Barcode */
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

static const char GDSET[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz #";
#define GDSET_F (IS_NUM_F | IS_UPR_F | IS_LWR_F | IS_SPC_F | IS_HSH_F)

static const char AusNTable[10][2] = {
    {'0','0'}, {'0','1'}, {'0','2'}, {'1','0'}, {'1','1'}, {'1','2'}, {'2','0'}, {'2','1'}, {'2','2'}, {'3','0'}
};

static const char AusCTable[64][3] = {
    {'2','2','2'}, {'3','0','0'}, {'3','0','1'}, {'3','0','2'}, {'3','1','0'}, {'3','1','1'},
    {'3','1','2'}, {'3','2','0'}, {'3','2','1'}, {'3','2','2'}, {'0','0','0'}, {'0','0','1'},
    {'0','0','2'}, {'0','1','0'}, {'0','1','1'}, {'0','1','2'}, {'0','2','0'}, {'0','2','1'},
    {'0','2','2'}, {'1','0','0'}, {'1','0','1'}, {'1','0','2'}, {'1','1','0'}, {'1','1','1'},
    {'1','1','2'}, {'1','2','0'}, {'1','2','1'}, {'1','2','2'}, {'2','0','0'}, {'2','0','1'},
    {'2','0','2'}, {'2','1','0'}, {'2','1','1'}, {'2','1','2'}, {'2','2','0'}, {'2','2','1'},
    {'0','2','3'}, {'0','3','0'}, {'0','3','1'}, {'0','3','2'}, {'0','3','3'}, {'1','0','3'},
    {'1','1','3'}, {'1','2','3'}, {'1','3','0'}, {'1','3','1'}, {'1','3','2'}, {'1','3','3'},
    {'2','0','3'}, {'2','1','3'}, {'2','2','3'}, {'2','3','0'}, {'2','3','1'}, {'2','3','2'},
    {'2','3','3'}, {'3','0','3'}, {'3','1','3'}, {'3','2','3'}, {'3','3','0'}, {'3','3','1'},
    {'3','3','2'}, {'3','3','3'}, {'0','0','3'}, {'0','1','3'}
};

static const char AusBarTable[64][3] = {
    {'0','0','0'}, {'0','0','1'}, {'0','0','2'}, {'0','0','3'}, {'0','1','0'}, {'0','1','1'},
    {'0','1','2'}, {'0','1','3'}, {'0','2','0'}, {'0','2','1'}, {'0','2','2'}, {'0','2','3'},
    {'0','3','0'}, {'0','3','1'}, {'0','3','2'}, {'0','3','3'}, {'1','0','0'}, {'1','0','1'},
    {'1','0','2'}, {'1','0','3'}, {'1','1','0'}, {'1','1','1'}, {'1','1','2'}, {'1','1','3'},
    {'1','2','0'}, {'1','2','1'}, {'1','2','2'}, {'1','2','3'}, {'1','3','0'}, {'1','3','1'},
    {'1','3','2'}, {'1','3','3'}, {'2','0','0'}, {'2','0','1'}, {'2','0','2'}, {'2','0','3'},
    {'2','1','0'}, {'2','1','1'}, {'2','1','2'}, {'2','1','3'}, {'2','2','0'}, {'2','2','1'},
    {'2','2','2'}, {'2','2','3'}, {'2','3','0'}, {'2','3','1'}, {'2','3','2'}, {'2','3','3'},
    {'3','0','0'}, {'3','0','1'}, {'3','0','2'}, {'3','0','3'}, {'3','1','0'}, {'3','1','1'},
    {'3','1','2'}, {'3','1','3'}, {'3','2','0'}, {'3','2','1'}, {'3','2','2'}, {'3','2','3'},
    {'3','3','0'}, {'3','3','1'}, {'3','3','2'}, {'3','3','3'}
};

#include <stdio.h>
#include "common.h"
#include "reedsol.h"

static char aus_convert_pattern(char data, int shift) {
    return (data - '0') << shift;
}

/* Adds Reed-Solomon error correction to auspost */
static char *aus_rs_error(char data_pattern[], char *d) {
    int reader, length, triple_writer = 0;
    unsigned char triple[31];
    unsigned char result[5];
    rs_t rs;

    for (reader = 2, length = d - data_pattern; reader < length; reader += 3, triple_writer++) {
        triple[triple_writer] = aus_convert_pattern(data_pattern[reader], 4)
                + aus_convert_pattern(data_pattern[reader + 1], 2)
                + aus_convert_pattern(data_pattern[reader + 2], 0);
    }

    rs_init_gf(&rs, 0x43);
    rs_init_code(&rs, 4, 1);
    rs_encode(&rs, triple_writer, triple, result);

    for (reader = 0; reader < 4; reader++, d += 3) {
        memcpy(d, AusBarTable[result[reader]], 3);
    }

    return d;
}

INTERNAL int daft_set_height(struct zint_symbol *symbol, const float min_height, const float max_height);

/* Handles Australia Posts's 4 State Codes */
INTERNAL int auspost(struct zint_symbol *symbol, unsigned char source[], int length) {
    /* Customer Standard Barcode, Barcode 2 or Barcode 3 system determined automatically
       (i.e. the FCC doesn't need to be specified by the user) dependent
       on the length of the input string */

    /* The contents of data_pattern conform to the following standard:
       0 = Tracker, Ascender and Descender
       1 = Tracker and Ascender
       2 = Tracker and Descender
       3 = Tracker only */
    int i;
    int error_number;
    int writer;
    int loopey, reader;
    int h;

    char data_pattern[200];
    char *d = data_pattern;
    char fcc[3] = {0}, dpid[10];
    char localstr[30];

    /* Do all of the length checking first to avoid stack smashing */
    if (symbol->symbology == BARCODE_AUSPOST) {
        if (length != 8 && length != 13 && length != 16 && length != 18 && length != 23) {
            return errtxtf(ZINT_ERROR_TOO_LONG, symbol, 401, "Input length %d wrong (8, 13, 16, 18 or 23 only)",
                            length);
        }
    } else if (length > 8) {
        return errtxtf(ZINT_ERROR_TOO_LONG, symbol, 403, "Input length %d too long (maximum 8)", length);
    }

    /* Check input immediately to catch nuls */
    if ((i = not_sane(GDSET_F, source, length))) {
        return errtxtf(ZINT_ERROR_INVALID_DATA, symbol, 404,
                        "Invalid character at position %d in input (alphanumerics, space and \"#\" only)", i);
    }

    localstr[0] = '\0';

    if (symbol->symbology == BARCODE_AUSPOST) {
        /* Format control code (FCC) */
        switch (length) {
            case 8:
                strcpy(fcc, "11");
                break;
            case 13:
                strcpy(fcc, "59");
                break;
            case 16:
                strcpy(fcc, "59");
                if ((i = not_sane(NEON_F, source, length))) {
                    return errtxtf(ZINT_ERROR_INVALID_DATA, symbol, 402,
                                    "Invalid character at position %d in input (digits only for FCC 59 length 16)",
                                    i);
                }
                break;
            case 18:
                strcpy(fcc, "62");
                break;
            case 23:
                strcpy(fcc, "62");
                if ((i = not_sane(NEON_F, source, length))) {
                    return errtxtf(ZINT_ERROR_INVALID_DATA, symbol, 406,
                                    "Invalid character at position %d in input (digits only for FCC 62 length 23)",
                                    i);
                }
                break;
        }
    } else {
        int zeroes;
        switch (symbol->symbology) {
            case BARCODE_AUSREPLY: strcpy(fcc, "45");
                break;
            case BARCODE_AUSROUTE: strcpy(fcc, "87");
                break;
            case BARCODE_AUSREDIRECT: strcpy(fcc, "92");
                break;
        }

        /* Add leading zeros as required */
        zeroes = 8 - length;
        memset(localstr, '0', zeroes);
        localstr[zeroes] = '\0';
    }

    if (symbol->debug & ZINT_DEBUG_PRINT) {
        printf("AUSPOST FCC: %s\n", fcc);
    }

    ustrncat(localstr, source, length);
    h = (int) strlen(localstr);
    /* Verify that the first 8 characters are numbers */
    memcpy(dpid, localstr, 8);
    dpid[8] = '\0';
    if ((i = not_sane(NEON_F, (unsigned char *) dpid, 8))) {
        return errtxtf(ZINT_ERROR_INVALID_DATA, symbol, 405,
                        "Invalid character at position %d in DPID (first 8 characters) (digits only)", i);
    }

    /* Start character */
    memcpy(d, "13", 2);
    d += 2;

    /* Encode the FCC */
    for (reader = 0; reader < 2; reader++, d += 2) {
        memcpy(d, AusNTable[fcc[reader] - '0'], 2);
    }

    /* Delivery Point Identifier (DPID) */
    for (reader = 0; reader < 8; reader++, d += 2) {
        memcpy(d, AusNTable[dpid[reader] - '0'], 2);
    }

    /* Customer Information */
    if (h > 8) {
        if ((h == 13) || (h == 18)) {
            for (reader = 8; reader < h; reader++, d += 3) {
                memcpy(d, AusCTable[posn(GDSET, localstr[reader])], 3);
            }
        } else if ((h == 16) || (h == 23)) {
            for (reader = 8; reader < h; reader++, d += 2) {
                memcpy(d, AusNTable[localstr[reader] - '0'], 2);
            }
        }
    }

    /* Filler bar */
    h = d - data_pattern;
    switch (h) {
        case 22:
        case 37:
        case 52:
            *d++ = '3';
            break;
        default:
            break;
    }

    /* Reed Solomon error correction */
    d = aus_rs_error(data_pattern, d);

    /* Stop character */
    memcpy(d, "13", 2);
    d += 2;

    /* Turn the symbol into a bar pattern ready for plotting */
    writer = 0;
    h = d - data_pattern;
    for (loopey = 0; loopey < h; loopey++) {
        if ((data_pattern[loopey] == '1') || (data_pattern[loopey] == '0')) {
            set_module(symbol, 0, writer);
        }
        set_module(symbol, 1, writer);
        if ((data_pattern[loopey] == '2') || (data_pattern[loopey] == '0')) {
            set_module(symbol, 2, writer);
        }
        writer += 2;
    }

    if (symbol->output_options & COMPLIANT_HEIGHT) {
        /* Australia Post Customer Barcoding Technical Specifications (Revised Aug 2012) Dimensions, placement and
           printing p.12
           (https://auspost.com.au/content/dam/auspost_corp/media/documents/
            customer-barcode-technical-specifications-aug2012.pdf)
           X 0.5mm (average of 0.4mm - 0.6mm), min height 4.2mm / 0.6mm (X max) = 7, max 5.6mm / 0.4mm (X min) = 14
           Tracker 1.3mm (average of 1mm - 1.6mm)
           Ascender/Descender 3.15mm (average of 2.6mm - 3.7mm) less T = 1.85mm
         */
        symbol->row_height[0] = 3.7f; /* 1.85f / 0.5f */
        symbol->row_height[1] = 2.6f; /* 1.3f / 0.5f */
        error_number = daft_set_height(symbol, 7.0f, 14.0f); /* Note using max X for minimum and min X for maximum */
    } else {
        symbol->row_height[0] = 3.0f;
        symbol->row_height[1] = 2.0f;
        error_number = daft_set_height(symbol, 0.0f, 0.0f);
    }
    symbol->rows = 3;
    symbol->width = writer - 1;

    return error_number;
}

/* vim: set ts=4 sw=4 et : */
