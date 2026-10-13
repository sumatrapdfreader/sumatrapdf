/*  upcean.c - Handles UPC, EAN and ISBN */
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

#define SODIUM_PLS_F        (IS_NUM_F | IS_PLS_F) /* SODIUM "0123456789+" */
#define ISBNX_SANE_F        (IS_NUM_F | IS_UX__F) /* ISBNX_SANE "0123456789X" */
#define ISBNX_ADDON_SANE_F  (IS_NUM_F | IS_UX__F | IS_LX__F | IS_PLS_F) /* ISBNX_ADDON_SANE "0123456789Xx+" */

#include <assert.h>
#include <stdio.h>
#include "common.h"
#include "gs1.h"

/* UPC and EAN tables checked against EN 797:1996 */

static const char UPCParity0[10][6] = {
    /* Number set for UPC-E symbol (EN Table 4) */
    {'B','B','B','A','A','A'}, {'B','B','A','B','A','A'}, {'B','B','A','A','B','A'}, {'B','B','A','A','A','B'},
    {'B','A','B','B','A','A'}, {'B','A','A','B','B','A'}, {'B','A','A','A','B','B'}, {'B','A','B','A','B','A'},
    {'B','A','B','A','A','B'}, {'B','A','A','B','A','B'}
};

static const char UPCParity1[10][6] = {
    /* Not covered by BS EN 797:1995 */
    {'A','A','A','B','B','B'}, {'A','A','B','A','B','B'}, {'A','A','B','B','A','B'}, {'A','A','B','B','B','A'},
    {'A','B','A','A','B','B'}, {'A','B','B','A','A','B'}, {'A','B','B','B','A','A'}, {'A','B','A','B','A','B'},
    {'A','B','A','B','B','A'}, {'A','B','B','A','B','A'}
};

static const char EAN2Parity[4][2] = {
    /* Number sets for 2-digit add-on (EN Table 6) */
    {'A','A'}, {'A','B'}, {'B','A'}, {'B','B'}
};

static const char EAN5Parity[10][5] = {
    /* Number set for 5-digit add-on (EN Table 7) */
    {'B','B','A','A','A'}, {'B','A','B','A','A'}, {'B','A','A','B','A'}, {'B','A','A','A','B'}, {'A','B','B','A','A'},
    {'A','A','B','B','A'}, {'A','A','A','B','B'}, {'A','B','A','B','A'}, {'A','B','A','A','B'}, {'A','A','B','A','B'}
};

static const char EAN13Parity[10][5] = {
    /* Left hand of the EAN-13 symbol (EN Table 3) */
    {'A','A','A','A','A'}, {'A','B','A','B','B'}, {'A','B','B','A','B'}, {'A','B','B','B','A'}, {'B','A','A','B','B'},
    {'B','B','A','A','B'}, {'B','B','B','A','A'}, {'B','A','B','A','B'}, {'B','A','B','B','A'}, {'B','B','A','B','A'}
};

static const char EANsetA[10][4] = {
    /* Representation set A and C (EN Table 1) */
    {'3','2','1','1'}, {'2','2','2','1'}, {'2','1','2','2'}, {'1','4','1','1'}, {'1','1','3','2'},
    {'1','2','3','1'}, {'1','1','1','4'}, {'1','3','1','2'}, {'1','2','1','3'}, {'3','1','1','2'}
};

static const char EANsetB[10][4] = {
    /* Representation set B (EN Table 1) */
    {'1','1','2','3'}, {'1','2','2','2'}, {'2','2','1','2'}, {'1','1','4','1'}, {'2','3','1','1'},
    {'1','3','2','1'}, {'4','1','1','1'}, {'2','1','3','1'}, {'3','1','2','1'}, {'2','1','1','3'}
};

/* UPC A is usually used for 12 digit numbers, but this function takes a source of any length */
static void upca_draw(const unsigned char source[], const int length, char *d) {
    int i, half_way;

    half_way = length / 2;

    /* start character */
    memcpy(d, "111", 3);
    d += 3;

    for (i = 0; i < length; i++, d += 4) {
        if (i == half_way) {
            /* middle character - separates manufacturer no. from product no. */
            /* also inverts right hand characters */
            memcpy(d, "11111", 5);
            d += 5;
        }

        memcpy(d, EANsetA[source[i] - '0'], 4);
    }

    /* stop character */
    strcpy(d, "111");
}

/* Make a UPC-A barcode, allowing for composite if `cc_rows` set */
static int upca_cc(struct zint_symbol *symbol, const unsigned char source[], int length, char dest[], int cc_rows) {
    unsigned char *gtin = symbol->text;
    int error_number = 0;

    ustrcpy(gtin, source);

    if (length == 11) {
        gtin[length++] = gs1_check_digit(gtin, 11);
        gtin[length] = '\0';
    } else {
        if (source[length - 1] != gs1_check_digit(gtin, 11)) {
            return errtxtf(ZINT_ERROR_INVALID_CHECK, symbol, 270, "Invalid check digit '%1$c', expecting '%2$c'",
                            source[length - 1], gs1_check_digit(gtin, 11));
        }
    }
    if (symbol->debug & ZINT_DEBUG_PRINT) {
        printf("UPC-A: %s, gtin: %s, Check digit: %c\n", source, gtin,
            length == 11 ? gtin[length] : gtin[length - 1]);
    }

    upca_draw(gtin, length, dest);

    if (symbol->output_options & COMPLIANT_HEIGHT) {
        /* BS EN 797:1996 4.5.1 Nominal dimensions 22.85mm / 0.33mm (X) ~ 69.24,
           same as minimum GS1 General Specifications 21.0.1 5.12.3.1 */
        const float height = 69.242424f; /* 22.85 / 0.33 */
        if (symbol->symbology == BARCODE_UPCA_CC) {
            symbol->height = height; /* Pass back min row == default height */
        } else {
            error_number = set_height(symbol, height, height, 0.0f, 0 /*no_errtxt*/);
        }
    } else {
        const float height = 50.0f;
        if (symbol->symbology == BARCODE_UPCA_CC) {
            symbol->height = height - cc_rows * 2 - 6.0f;
        } else {
            (void) set_height(symbol, 0.0f, height, 0.0f, 1 /*no_errtxt*/);
        }
    }

    return error_number;
}

/* UPC-A */
static int upca(struct zint_symbol *symbol, const unsigned char source[], int length, char dest[]) {
    return upca_cc(symbol, source, length, dest, 0 /*cc_rows*/);
}

/* UPC-E, allowing for composite if `cc_rows` set */
static int upce_cc(struct zint_symbol *symbol, unsigned char source[], int length, char *d, int cc_rows) {
    int i, num_system;
    char emode, check_digit;
    const char *parity;
    char src_check_digit = '\0';
    unsigned char equivalent[12];
    unsigned char *hrt = symbol->text;
    int error_number = 0;

    if (length == 8 || symbol->symbology == BARCODE_UPCE_CHK) {
        /* Will validate later */
        src_check_digit = source[--length];
    }

    /* Two number systems can be used - system 0 and system 1 */
    hrt[0] = '\0';
    if (length == 7) {
        switch (source[0]) {
            case '0':
                num_system = 0;
                ustrncat(hrt, source, length);
                break;
            case '1':
                num_system = 1;
                ustrncat(hrt, source, length);
                break;
            default:
                num_system = 0;
                /* First source char ignored */
                ustrncat(hrt, source, length);
                hrt[0] = '0'; /* Overwrite HRT first char with '0' to correct TODO: error/warn in future */
                break;
        }
        for (i = 1; i <= length; i++) {
            source[i - 1] = hrt[i];
        }
        length--;
    } else {
        /* Length 6, insert leading zero */
        num_system = 0;
        hrt[0] = '0';
        hrt[1] = '\0';
        ustrncat(hrt, source, length);
    }

    /* Expand the zero-compressed UPCE code to make a UPCA equivalent (EN Table 5) */
    emode = source[5];
    for (i = 0; i < 11; i++) {
        equivalent[i] = '0';
    }
    if (num_system == 1) {
        equivalent[0] = hrt[0];
    }
    equivalent[1] = source[0];
    equivalent[2] = source[1];
    equivalent[11] = '\0';

    switch (emode) {
        case '0':
        case '1':
        case '2':
            equivalent[3] = emode;
            equivalent[8] = source[2];
            equivalent[9] = source[3];
            equivalent[10] = source[4];
            break;
        case '3':
            equivalent[3] = source[2];
            equivalent[9] = source[3];
            equivalent[10] = source[4];
            if (((source[2] == '0') || (source[2] == '1')) || (source[2] == '2')) {
                /* Note 1 - "X3 shall not be equal to 0, 1 or 2" */
                return errtxtf(ZINT_ERROR_INVALID_DATA, symbol, 271,
                            "For this UPC-E zero suppression, 3rd character cannot be \"0\", \"1\" or \"2\" (%.*s)",
                            length, source);
            }
            break;
        case '4':
            equivalent[3] = source[2];
            equivalent[4] = source[3];
            equivalent[10] = source[4];
            if (source[3] == '0') {
                /* Note 2 - "X4 shall not be equal to 0" */
                return errtxtf(ZINT_ERROR_INVALID_DATA, symbol, 272,
                                "For this UPC-E zero suppression, 4th character cannot be \"0\" (%.*s)",
                                length, source);
            }
            break;
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
            equivalent[3] = source[2];
            equivalent[4] = source[3];
            equivalent[5] = source[4];
            equivalent[10] = emode;
            if (source[4] == '0') {
                /* Note 3 - "X5 shall not be equal to 0" */
                return errtxtf(ZINT_ERROR_INVALID_DATA, symbol, 273,
                                "For this UPC-E zero suppression, 5th character cannot be \"0\" (%.*s)",
                                length, source);
            }
            break;
    }

    /* Get the check digit from the expanded UPCA code */

    check_digit = gs1_check_digit(equivalent, 11);

    if (src_check_digit && src_check_digit != check_digit) {
        return errtxtf(ZINT_ERROR_INVALID_CHECK, symbol, 274, "Invalid check digit '%1$c', expecting '%2$c'",
                        src_check_digit, check_digit);
    }

    /* Use the number system and check digit information to choose a parity scheme */
    if (num_system == 1) {
        parity = UPCParity1[ctoi(check_digit)];
    } else {
        parity = UPCParity0[ctoi(check_digit)];
    }

    /* Take all this information and make the barcode pattern */

    /* start character */
    memcpy(d, "111", 3);
    d += 3;

    for (i = 0; i < length; i++, d += 4) {
        switch (parity[i]) {
            case 'A':
                memcpy(d, EANsetA[source[i] - '0'], 4);
                break;
            case 'B':
                memcpy(d, EANsetB[source[i] - '0'], 4);
                break;
        }
    }

    /* stop character */
    strcpy(d, "111111");

    hrt[7] = check_digit;
    hrt[8] = '\0';

    if (symbol->debug & ZINT_DEBUG_PRINT) {
        printf("UPC-E: %s, equivalent: %s, hrt: %s, Check digit: %c\n", source, equivalent, hrt, check_digit);
    }

    if (symbol->output_options & COMPLIANT_HEIGHT) {
        /* BS EN 797:1996 4.5.1 Nominal dimensions 22.85mm / 0.33mm (X) ~ 69.24,
           same as minimum GS1 General Specifications 21.0.1 5.12.3.1 */
        const float height = 69.242424f; /* 22.85 / 0.33 */
        if (symbol->symbology == BARCODE_UPCE_CC) {
            symbol->height = height; /* Pass back min row == default height */
        } else {
            error_number = set_height(symbol, height, height, 0.0f, 0 /*no_errtxt*/);
        }
    } else {
        const float height = 50.0f;
        if (symbol->symbology == BARCODE_UPCE_CC) {
            symbol->height = height - cc_rows * 2 - 6.0f;
        } else {
            (void) set_height(symbol, 0.0f, height, 0.0f, 1 /*no_errtxt*/);
        }
    }

    return error_number;
}

/* UPC-E is a zero-compressed version of UPC-A */
static int upce(struct zint_symbol *symbol, unsigned char source[], int length, char dest[]) {
    return upce_cc(symbol, source, length, dest, 0 /*cc_rows*/);
}

/* EAN-2 and EAN-5 add-on codes */
static void ean_add_on(const unsigned char source[], const int length, char dest[], const int addon_gap) {
    const char *parity;
    int i;
    char *d = dest + strlen(dest);

    /* If an add-on then append with space */
    if (addon_gap != 0) {
        *d++ = itoc(addon_gap);
    }

    /* Start character */
    memcpy(d, "112", 3);
    d += 3;

    /* Calculate parity */
    if (length == 2) { /* EAN-2 */
        int code_value, parity_bit;

        code_value = (10 * ctoi(source[0])) + ctoi(source[1]);
        parity_bit = code_value % 4;
        parity = EAN2Parity[parity_bit];
    } else { /* EAN-5 */
        int values[6], parity_sum, parity_bit;

        for (i = 0; i < 6; i++) {
            values[i] = ctoi(source[i]);
        }

        parity_sum = (3 * (values[0] + values[2] + values[4]));
        parity_sum += (9 * (values[1] + values[3]));

        parity_bit = parity_sum % 10;
        parity = EAN5Parity[parity_bit];
    }

    for (i = 0; i < length; i++) {
        switch (parity[i]) {
            case 'A':
                memcpy(d, EANsetA[source[i] - '0'], 4);
                d += 4;
                break;
            case 'B':
                memcpy(d, EANsetB[source[i] - '0'], 4);
                d += 4;
                break;
        }

        /* Glyph separator */
        if (i != (length - 1)) {
            memcpy(d, "11", 2);
            d += 2;
        }
    }
    *d = '\0';
}

/* ************************ EAN-13 ****************** */

static int ean13_cc(struct zint_symbol *symbol, const unsigned char source[], int length, char *d,
            int cc_rows) {
    int i, half_way;
    const char *parity;
    unsigned char *gtin = symbol->text;
    int error_number = 0;

    ustrcpy(gtin, source);

    /* Add the appropriate check digit */

    if (length == 12) {
        gtin[length++] = gs1_check_digit(gtin, 12);
        gtin[length] = '\0';
    } else {
        if (source[length - 1] != gs1_check_digit(gtin, 12)) {
            return errtxtf(ZINT_ERROR_INVALID_CHECK, symbol, 275, "Invalid check digit '%1$c', expecting '%2$c'",
                            source[length - 1], gs1_check_digit(gtin, 12));
        }
    }
    if (symbol->debug & ZINT_DEBUG_PRINT) {
        printf("EAN-13: %s, gtin: %s, Check digit: %c\n", source, gtin, gtin[length - 1]);
    }

    /* Get parity for first half of the symbol */
    parity = EAN13Parity[gtin[0] - '0'];

    /* Now get on with the cipher */
    half_way = 7;

    /* start character */
    memcpy(d, "111", 3);
    d += 3;

    for (i = 1; i < length; i++, d += 4) {
        if (i == half_way) {
            /* middle character - separates manufacturer no. from product no. */
            /* also inverses right hand characters */
            memcpy(d, "11111", 5);
            d += 5;
        }

        if (((i > 1) && (i < 7)) && (parity[i - 2] == 'B')) {
            memcpy(d, EANsetB[gtin[i] - '0'], 4);
        } else {
            memcpy(d, EANsetA[gtin[i] - '0'], 4);
        }
    }

    /* stop character */
    strcpy(d, "111");

    if (symbol->output_options & COMPLIANT_HEIGHT) {
        /* BS EN 797:1996 4.5.1 Nominal dimensions 22.85mm / 0.33mm (X) ~ 69.24,
           same as minimum GS1 General Specifications 21.0.1 5.12.3.1 */
        const float height = 69.242424f; /* 22.85 / 0.33 */
        if (symbol->symbology == BARCODE_EANX_CC) {
            symbol->height = height; /* Pass back min row == default height */
        } else {
            error_number = set_height(symbol, height, height, 0.0f, 0 /*no_errtxt*/);
        }
    } else {
        const float height = 50.0f;
        if (symbol->symbology == BARCODE_EANX_CC) {
            symbol->height = height - cc_rows * 2 - 6.0f;
        } else {
            (void) set_height(symbol, 0.0f, height, 0.0f, 1 /*no_errtxt*/);
        }
    }

    return error_number;
}

static int ean13(struct zint_symbol *symbol, const unsigned char source[], int length, char dest[]) {
    return ean13_cc(symbol, source, length, dest, 0 /*cc_rows*/);
}

static int ean8_cc(struct zint_symbol *symbol, const unsigned char source[], int length, char dest[], int cc_rows) {
    /* EAN-8 is basically the same as UPC-A but with fewer digits */
    unsigned char *gtin = symbol->text;
    int error_number = 0;

    ustrcpy(gtin, source);

    if (length == 7) {
        gtin[length++] = gs1_check_digit(gtin, 7);
        gtin[length] = '\0';
    } else {
        if (source[length - 1] != gs1_check_digit(gtin, 7)) {
            return errtxtf(ZINT_ERROR_INVALID_CHECK, symbol, 276, "Invalid check digit '%1$c', expecting '%2$c'",
                            source[length - 1], gs1_check_digit(gtin, 7));
        }
    }
    if (symbol->debug & ZINT_DEBUG_PRINT) {
        printf("EAN-8: %s, gtin: %s, Check digit: %c\n", source, gtin,
            length == 7 ? gtin[length] : gtin[length - 1]);
    }

    upca_draw(gtin, length, dest);

    if (symbol->output_options & COMPLIANT_HEIGHT) {
        /* BS EN 797:1996 4.5.1 Nominal dimensions 18.23mm / 0.33mm (X) ~ 55.24,
           same as minimum GS1 General Specifications 21.0.1 5.12.3.1 */
        const float height = 55.242424f; /* 18.23 / 0.33 */
        if (symbol->symbology == BARCODE_EANX_CC) {
            symbol->height = height; /* Pass back min row == default height */
        } else {
            error_number = set_height(symbol, height, height, 0.0f, 0 /*no_errtxt*/);
        }
    } else {
        const float height = 50.0f;
        if (symbol->symbology == BARCODE_EANX_CC) {
            symbol->height = height - cc_rows * 2 - 6.0f;
        } else {
            (void) set_height(symbol, 0.0f, height, 0.0f, 1 /*no_errtxt*/);
        }
    }

    return error_number;
}

/* Make an EAN-8 barcode when we haven't been given the check digit */
static int ean8(struct zint_symbol *symbol, const unsigned char source[], int length, char dest[]) {
    return ean8_cc(symbol, source, length, dest, 0 /*cc_rows*/);
}

/* For ISBN(10) and SBN only */
static char isbnx_check(const unsigned char source[], const int length) {
    int i, weight, sum, check;
    char check_char;

    sum = 0;
    weight = 1;

    for (i = 0; i < length; i++) { /* Length will always be 9 */
        sum += ctoi(source[i]) * weight;
        weight++;
    }

    check = sum % 11;
    check_char = itoc(check);
    if (check == 10) {
        check_char = 'X';
    }
    return check_char;
}

/* Make an EAN-13 barcode from an SBN or ISBN */
static int isbnx(struct zint_symbol *symbol, unsigned char source[], const int length, char dest[]) {
    int i;
    char check_digit;

    to_upper(source, length);
    if (not_sane(ISBNX_SANE_F, source, length)) { /* As source has been zero-padded, don't report position */
        return errtxt(ZINT_ERROR_INVALID_DATA, symbol, 277, "Invalid character in input (digits and \"X\" only)");
    }

    /* Input must be 9, 10 or 13 characters */
    if (length != 9 && length != 10 && length != 13) {
        return errtxtf(ZINT_ERROR_TOO_LONG, symbol, 278, "Input length %d wrong (9, 10, or 13 only)", length);
    }

    if (length == 13) /* Using 13 character ISBN */ {
        if (!(((source[0] == '9') && (source[1] == '7')) &&
                ((source[2] == '8') || (source[2] == '9')))) {
            return errtxt(ZINT_ERROR_INVALID_DATA, symbol, 279, "Invalid ISBN (must begin with \"978\" or \"979\")");
        }

        /* "X" cannot occur */
        if (not_sane(NEON_F, source, 13)) {
            return errtxt(ZINT_ERROR_INVALID_DATA, symbol, 282,
                            "Invalid character in input, \"X\" not allowed in ISBN-13");
        }

        check_digit = gs1_check_digit(source, 12);
        if (source[12] != check_digit) {
            return errtxtf(ZINT_ERROR_INVALID_CHECK, symbol, 280, "Invalid ISBN check digit '%1$c', expecting '%2$c'",
                            source[12], check_digit);
        }
        source[12] = '\0';

    } else { /* Using 10 digit ISBN or 9 digit SBN padded with leading zero */
        if (length == 9) /* Using 9 digit SBN */ {
            /* Add leading zero */
            for (i = 10; i > 0; i--) {
                source[i] = source[i - 1];
            }
            source[0] = '0';
        }

        /* "X" can only occur in last position */
        if (not_sane(NEON_F, source, 9)) {
            return errtxt(ZINT_ERROR_INVALID_DATA, symbol, 296,
                            "Invalid character in input, \"X\" allowed in last position only");
        }

        check_digit = isbnx_check(source, 9);
        if (check_digit != source[9]) {
            return errtxtf(ZINT_ERROR_INVALID_CHECK, symbol, 281,
                            "Invalid %1$s check digit '%2$c', expecting '%3$c'", length == 9 ? "SBN" : "ISBN",
                            source[9], check_digit);
        }
        for (i = 11; i > 2; i--) { /* This drops the check digit */
            source[i] = source[i - 3];
        }
        source[0] = '9';
        source[1] = '7';
        source[2] = '8';
        source[12] = '\0';
    }

    return ean13(symbol, source, 12, dest);
}

/* Add leading zeroes to EAN and UPC strings */
INTERNAL int ean_leading_zeroes(struct zint_symbol *symbol, const unsigned char source[],
                unsigned char local_source[], int *p_with_addon, unsigned char *zfirst_part,
                unsigned char *zsecond_part) {
    unsigned char first_part[14], second_part[6];
    int with_addon = 0;
    int first_len = 0, second_len = 0, zfirst_len = 0, zsecond_len = 0, i, h;

    h = (int) ustrlen(source);
    for (i = 0; i < h; i++) {
        if (source[i] == '+') {
            with_addon = 1;
        } else {
            if (with_addon == 0) {
                first_len++;
            } else {
                second_len++;
            }
        }
    }
    if (first_len > 13 || second_len > 5) {
        if (first_len > 13) {
            if (!second_len) {
                errtxtf(0, symbol, 294, "Input length %d too long (maximum 13)", first_len);
            } else {
                errtxtf(0, symbol, 298, "Input EAN length %d too long (maximum 13)", first_len);
            }
        } else {
            errtxtf(0, symbol, 297, "Input add-on length %d too long (maximum 5)", second_len);
        }
        if (p_with_addon) {
            *p_with_addon = with_addon;
        }
        return 0;
    }

    /* Split input into two strings */
    for (i = 0; i < first_len; i++) {
        first_part[i] = source[i];
    }
    first_part[first_len] = '\0';

    for (i = 0; i < second_len; i++) {
        second_part[i] = source[i + first_len + 1];
    }
    second_part[second_len] = '\0';

    /* Calculate target lengths */
    if (second_len == 0) {
        zsecond_len = 0;
    } else if (second_len <= 2) {
        zsecond_len = 2;
    } else {
        zsecond_len = 5;
    }
    switch (symbol->symbology) {
        case BARCODE_EANX:
        case BARCODE_EANX_CC:
            if (first_len <= 12) {
                if (first_len <= 7) {
                    zfirst_len = 7;
                } else {
                    zfirst_len = 12;
                }
            }
            if (second_len == 0 && symbol->symbology == BARCODE_EANX) { /* No composite EAN-2/5 */
                if (first_len <= 5) {
                    if (first_len <= 2) {
                        zfirst_len = 2;
                    } else {
                        zfirst_len = 5;
                    }
                }
            }
            break;
        case BARCODE_EANX_CHK:
            if (first_len <= 13) {
                if (first_len <= 8) {
                    zfirst_len = 8;
                } else {
                    zfirst_len = 13;
                }
            }
            if (second_len == 0) {
                if (first_len <= 5) {
                    if (first_len <= 2) {
                        zfirst_len = 2;
                    } else {
                        zfirst_len = 5;
                    }
                }
            }
            break;
        case BARCODE_UPCA:
        case BARCODE_UPCA_CC:
            zfirst_len = 11;
            break;
        case BARCODE_UPCA_CHK:
            zfirst_len = 12;
            break;
        case BARCODE_UPCE:
        case BARCODE_UPCE_CC:
            if (first_len == 7) {
                zfirst_len = 7;
            } else if (first_len <= 6) {
                zfirst_len = 6;
            }
            break;
        case BARCODE_UPCE_CHK:
            if (first_len == 8) {
                zfirst_len = 8;
            } else if (first_len <= 7) {
                zfirst_len = 7;
            }
            break;
        case BARCODE_ISBNX:
            if (first_len <= 9) {
                zfirst_len = 9;
            }
            break;
    }

    /* Copy adjusted data back to local_source */

    /* Add leading zeroes */
    for (i = 0; i < (zfirst_len - first_len); i++) {
        local_source[i] = '0';
    }
    ustrcpy(local_source + i, first_part);
    if (zfirst_part) {
        ustrcpy(zfirst_part, local_source);
    }

    if (with_addon) {
        h = (int) ustrlen(local_source);
        local_source[h++] = '+';
        for (i = 0; i < (zsecond_len - second_len); i++) {
            local_source[h + i] = '0';
        }
        ustrcpy(local_source + h + i, second_part);
        if (zsecond_part) {
            ustrcpy(zsecond_part, local_source + h);
        }
    } else if (zsecond_part) {
        *zsecond_part = '\0';
    }

    if (p_with_addon) {
        *p_with_addon = with_addon;
    }

    return 1; /* Success */
}

INTERNAL int eanx_cc(struct zint_symbol *symbol, unsigned char source[], int length, int cc_rows) {
    unsigned char first_part[14], second_part[6];
    unsigned char local_source[20]; /* Allow 13 + "+" + 5 + 1 */
    char dest[1000] = {0};
    int with_addon;
    int error_number = 0, i, plus_count;
    int addon_gap = 0;
    int first_part_len, second_part_len;

    if (length > 19) {
        return errtxtf(ZINT_ERROR_TOO_LONG, symbol, 283, "Input length %d too long (maximum 19)", length);
    }
    if (symbol->symbology != BARCODE_ISBNX) {
        /* ISBN has its own sanity routine */
        if ((i = not_sane(SODIUM_PLS_F, source, length))) {
            return errtxtf(ZINT_ERROR_INVALID_DATA, symbol, 284,
                            "Invalid character at position %d in input (digits and \"+\" only)", i);
        }
    } else {
        if ((i = not_sane(ISBNX_ADDON_SANE_F, source, length))) {
            return errtxtf(ZINT_ERROR_INVALID_DATA, symbol, 285,
                            "Invalid character at position %d in input (digits, \"X\" and \"+\" only)", i);
        }
        /* Add-on will be checked separately to be numeric only below */
    }

    /* Check for multiple '+' characters */
    plus_count = 0;
    for (i = 0; i < length; i++) {
        if (source[i] == '+') {
            plus_count++;
            if (plus_count > 1) {
                return errtxt(ZINT_ERROR_INVALID_DATA, symbol, 293, "Invalid add-on data (one \"+\" only)");
            }
        }
    }

    /* Add leading zeroes, checking max lengths of parts */
    if (!ean_leading_zeroes(symbol, source, local_source, &with_addon, first_part, second_part)) {
        return ZINT_ERROR_TOO_LONG; /* `ean_leading_zeroes()` sets `errtxt` */
    }

    if (with_addon) {
        if (symbol->symbology == BARCODE_UPCA || symbol->symbology == BARCODE_UPCA_CHK
                || symbol->symbology == BARCODE_UPCA_CC) {
            addon_gap = symbol->option_2 >= 9 && symbol->option_2 <= 12 ? symbol->option_2 : 9;
        } else {
            addon_gap = symbol->option_2 >= 7 && symbol->option_2 <= 12 ? symbol->option_2 : 7;
        }
    }

    first_part_len = (int) ustrlen(first_part);

    switch (symbol->symbology) {
        case BARCODE_EANX:
        case BARCODE_EANX_CHK:
            switch (first_part_len) {
                case 2:
                case 5:
                    ean_add_on(first_part, first_part_len, dest, 0);
                    ustrcpy(symbol->text, first_part);
                    if (symbol->output_options & COMPLIANT_HEIGHT) {
                        /* 21.9mm from GS1 General Specifications 5.2.6.6, Figure 5.2.6.6-6 */
                        const float height = 66.3636398f; /* 21.9 / 0.33 */
                        error_number = set_height(symbol, height, height, 0.0f, 0 /*no_errtxt*/);
                    } else {
                        (void) set_height(symbol, 0.0f, 50.0f, 0.0f, 1 /*no_errtxt*/);
                    }
                    break;
                case 7:
                case 8:
                    error_number = ean8(symbol, first_part, first_part_len, dest);
                    break;
                case 12:
                case 13:
                    error_number = ean13(symbol, first_part, first_part_len, dest);
                    break;
                default:
                    return errtxtf(ZINT_ERROR_TOO_LONG, symbol, 286,
                                    "Input length %d wrong (2, 5, 7, 8, 12 or 13 only)", first_part_len);
                    break;
            }
            break;
        case BARCODE_EANX_CC:
            switch (first_part_len) { /* Adds vertical separator bars according to ISO/IEC 24723 section 11.4 */
                case 7:
                    set_module(symbol, symbol->rows, 1);
                    set_module(symbol, symbol->rows, 67);
                    set_module(symbol, symbol->rows + 1, 0);
                    set_module(symbol, symbol->rows + 1, 68);
                    set_module(symbol, symbol->rows + 2, 1);
                    set_module(symbol, symbol->rows + 2, 67);
                    symbol->row_height[symbol->rows] = 2;
                    symbol->row_height[symbol->rows + 1] = 2;
                    symbol->row_height[symbol->rows + 2] = 2;
                    symbol->rows += 3;
                    error_number = ean8_cc(symbol, first_part, first_part_len, dest, cc_rows);
                    break;
                case 12:
                case 13:
                    set_module(symbol, symbol->rows, 1);
                    set_module(symbol, symbol->rows, 95);
                    set_module(symbol, symbol->rows + 1, 0);
                    set_module(symbol, symbol->rows + 1, 96);
                    set_module(symbol, symbol->rows + 2, 1);
                    set_module(symbol, symbol->rows + 2, 95);
                    symbol->row_height[symbol->rows] = 2;
                    symbol->row_height[symbol->rows + 1] = 2;
                    symbol->row_height[symbol->rows + 2] = 2;
                    symbol->rows += 3;
                    error_number = ean13_cc(symbol, first_part, first_part_len, dest, cc_rows);
                    break;
                default:
                    return errtxtf(ZINT_ERROR_TOO_LONG, symbol, 287, "Input length %d wrong (7, 12 or 13 only)",
                                    first_part_len);
                    break;
            }
            break;
        case BARCODE_UPCA:
        case BARCODE_UPCA_CHK:
            if ((first_part_len == 11) || (first_part_len == 12)) {
                error_number = upca(symbol, first_part, first_part_len, dest);
            } else {
                return errtxtf(ZINT_ERROR_TOO_LONG, symbol, 288, "Input length %d wrong (11 or 12 only)",
                                first_part_len);
            }
            break;
        case BARCODE_UPCA_CC:
            if (first_part_len == 11 || first_part_len == 12) {
                set_module(symbol, symbol->rows, 1);
                set_module(symbol, symbol->rows, 95);
                set_module(symbol, symbol->rows + 1, 0);
                set_module(symbol, symbol->rows + 1, 96);
                set_module(symbol, symbol->rows + 2, 1);
                set_module(symbol, symbol->rows + 2, 95);
                symbol->row_height[symbol->rows] = 2;
                symbol->row_height[symbol->rows + 1] = 2;
                symbol->row_height[symbol->rows + 2] = 2;
                symbol->rows += 3;
                error_number = upca_cc(symbol, first_part, first_part_len, dest, cc_rows);
            } else {
                return errtxtf(ZINT_ERROR_TOO_LONG, symbol, 289, "Input length %d wrong (11 or 12 only)",
                                first_part_len);
            }
            break;
        case BARCODE_UPCE:
        case BARCODE_UPCE_CHK:
            if ((first_part_len >= 6) && (first_part_len <= 8)) {
                error_number = upce(symbol, first_part, first_part_len, dest);
            } else {
                return errtxtf(ZINT_ERROR_TOO_LONG, symbol, 290, "Input length %d wrong (6, 7 or 8 only)",
                                first_part_len);
            }
            break;
        case BARCODE_UPCE_CC:
            if ((first_part_len >= 6) && (first_part_len <= 8)) {
                set_module(symbol, symbol->rows, 1);
                set_module(symbol, symbol->rows, 51);
                set_module(symbol, symbol->rows + 1, 0);
                set_module(symbol, symbol->rows + 1, 52);
                set_module(symbol, symbol->rows + 2, 1);
                set_module(symbol, symbol->rows + 2, 51);
                symbol->row_height[symbol->rows] = 2;
                symbol->row_height[symbol->rows + 1] = 2;
                symbol->row_height[symbol->rows + 2] = 2;
                symbol->rows += 3;
                error_number = upce_cc(symbol, first_part, first_part_len, dest, cc_rows);
            } else {
                return errtxtf(ZINT_ERROR_TOO_LONG, symbol, 291, "Input length %d wrong (6, 7 or 8 only)",
                                first_part_len);
            }
            break;
        case BARCODE_ISBNX:
            error_number = isbnx(symbol, first_part, first_part_len, dest);
            break;
    }

    if (error_number >= ZINT_ERROR) {
        return error_number;
    }

    second_part_len = (int) ustrlen(second_part);

    if (symbol->symbology == BARCODE_ISBNX) { /* Need to further check that add-on numeric only */
        if (not_sane(NEON_F, second_part, second_part_len)) {
            return errtxt(ZINT_ERROR_INVALID_DATA, symbol, 295, "Invalid add-on data (digits only)");
        }
    }

    if (second_part_len) {
        ean_add_on(second_part, second_part_len, dest, addon_gap);
        ustrcat(symbol->text, "+");
        ustrcat(symbol->text, second_part);
    }

    expand(symbol, dest, (int) strlen(dest));

    switch (symbol->symbology) {
        case BARCODE_EANX_CC:
        case BARCODE_UPCA_CC:
        case BARCODE_UPCE_CC:
            /* shift the symbol to the right one space to allow for separator bars */
            for (i = (symbol->width + 1); i >= 1; i--) {
                if (module_is_set(symbol, symbol->rows - 1, i - 1)) {
                    set_module(symbol, symbol->rows - 1, i);
                } else {
                    unset_module(symbol, symbol->rows - 1, i);
                }
            }
            unset_module(symbol, symbol->rows - 1, 0);
            symbol->width += 1 + (second_part_len == 0); /* Only need right space if no add-on */
            break;
    }

    return error_number;
}

/* Handle UPC, EAN, ISBN */
INTERNAL int eanx(struct zint_symbol *symbol, unsigned char source[], int length) {
    return eanx_cc(symbol, source, length, 0 /*cc_rows*/);
}

/* vim: set ts=4 sw=4 et : */
