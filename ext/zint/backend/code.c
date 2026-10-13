/* code.c - Handles Code 11, 39, 39+, 93, PZN, Channel and VIN */
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

/* In version 0.5 this file was 1,553 lines long! */

#include <assert.h>
#include <stdio.h>
#include "common.h"

#define SODIUM_MNS_F    (IS_NUM_F | IS_MNS_F) /* SODIUM "0123456789-" */

/* Same as TECHNETIUM (HIBC) with "abcd" added for CODE93 */
static const char SILVER[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ-. $/+%abcd";

#define ARSENIC_F       (IS_NUM_F | IS_ARS_F) /* ARSENIC "0123456789ABCDEFGHJKLMNPRSTUVWXYZ" */

static const char C11Table[11 + 1][6] = {
    {'1','1','1','1','2','1'}, {'2','1','1','1','2','1'}, {'1','2','1','1','2','1'}, {'2','2','1','1','1','1'},
    {'1','1','2','1','2','1'}, {'2','1','2','1','1','1'}, {'1','2','2','1','1','1'}, {'1','1','1','2','2','1'},
    {'2','1','1','2','1','1'}, {'2','1','1','1','1','1'}, {'1','1','2','1','1','1'},
    {'1','1','2','2','1','1'} /* Start character (full 6), Stop character (first 5) */
};

/* Code 39 character assignments (ISO/IEC 16388:2007 Table 1 and Table A.1) */
static const char C39Table[43 + 1][10] = {
    {'1','1','1','2','2','1','2','1','1','1'}, {'2','1','1','2','1','1','1','1','2','1'},
    {'1','1','2','2','1','1','1','1','2','1'}, {'2','1','2','2','1','1','1','1','1','1'},
    {'1','1','1','2','2','1','1','1','2','1'}, {'2','1','1','2','2','1','1','1','1','1'},
    {'1','1','2','2','2','1','1','1','1','1'}, {'1','1','1','2','1','1','2','1','2','1'},
    {'2','1','1','2','1','1','2','1','1','1'}, {'1','1','2','2','1','1','2','1','1','1'},
    {'2','1','1','1','1','2','1','1','2','1'}, {'1','1','2','1','1','2','1','1','2','1'},
    {'2','1','2','1','1','2','1','1','1','1'}, {'1','1','1','1','2','2','1','1','2','1'},
    {'2','1','1','1','2','2','1','1','1','1'}, {'1','1','2','1','2','2','1','1','1','1'},
    {'1','1','1','1','1','2','2','1','2','1'}, {'2','1','1','1','1','2','2','1','1','1'},
    {'1','1','2','1','1','2','2','1','1','1'}, {'1','1','1','1','2','2','2','1','1','1'},
    {'2','1','1','1','1','1','1','2','2','1'}, {'1','1','2','1','1','1','1','2','2','1'},
    {'2','1','2','1','1','1','1','2','1','1'}, {'1','1','1','1','2','1','1','2','2','1'},
    {'2','1','1','1','2','1','1','2','1','1'}, {'1','1','2','1','2','1','1','2','1','1'},
    {'1','1','1','1','1','1','2','2','2','1'}, {'2','1','1','1','1','1','2','2','1','1'},
    {'1','1','2','1','1','1','2','2','1','1'}, {'1','1','1','1','2','1','2','2','1','1'},
    {'2','2','1','1','1','1','1','1','2','1'}, {'1','2','2','1','1','1','1','1','2','1'},
    {'2','2','2','1','1','1','1','1','1','1'}, {'1','2','1','1','2','1','1','1','2','1'},
    {'2','2','1','1','2','1','1','1','1','1'}, {'1','2','2','1','2','1','1','1','1','1'},
    {'1','2','1','1','1','1','2','1','2','1'}, {'2','2','1','1','1','1','2','1','1','1'},
    {'1','2','2','1','1','1','2','1','1','1'}, {'1','2','1','2','1','2','1','1','1','1'},
    {'1','2','1','2','1','1','1','2','1','1'}, {'1','2','1','1','1','2','1','2','1','1'},
    {'1','1','1','2','1','2','1','2','1','1'},
    {'1','2','1','1','2','1','2','1','1','1'} /* Start character (full 10), Stop character (first 9) */
};

/* Encoding the full ASCII character set in Code 39 (ISO/IEC 16388:2007 Table A.2) */
static const char EC39Ctrl[128][2] = {
    {'%','U'}, {'$','A'}, {'$','B'}, {'$','C'}, {'$','D'}, {'$','E'}, {'$','F'}, {'$','G'}, {'$','H'}, {'$','I'},
    {'$','J'}, {'$','K'}, {'$','L'}, {'$','M'}, {'$','N'}, {'$','O'}, {'$','P'}, {'$','Q'}, {'$','R'}, {'$','S'},
    {'$','T'}, {'$','U'}, {'$','V'}, {'$','W'}, {'$','X'}, {'$','Y'}, {'$','Z'}, {'%','A'}, {'%','B'}, {'%','C'},
    {'%','D'}, {'%','E'}, {  " "  }, {'/','A'}, {'/','B'}, {'/','C'}, {'/','D'}, {'/','E'}, {'/','F'}, {'/','G'},
    {'/','H'}, {'/','I'}, {'/','J'}, {'/','K'}, {'/','L'}, {  "-"  }, {  "."  }, {'/','O'}, {  "0"  }, {  "1"  },
    {  "2"  }, {  "3"  }, {  "4"  }, {  "5"  }, {  "6"  }, {  "7"  }, {  "8"  }, {  "9"  }, {'/','Z'}, {'%','F'},
    {'%','G'}, {'%','H'}, {'%','I'}, {'%','J'}, {'%','V'}, {  "A"  }, {  "B"  }, {  "C"  }, {  "D"  }, {  "E"  },
    {  "F"  }, {  "G"  }, {  "H"  }, {  "I"  }, {  "J"  }, {  "K"  }, {  "L"  }, {  "M"  }, {  "N"  }, {  "O"  },
    {  "P"  }, {  "Q"  }, {  "R"  }, {  "S"  }, {  "T"  }, {  "U"  }, {  "V"  }, {  "W"  }, {  "X"  }, {  "Y"  },
    {  "Z"  }, {'%','K'}, {'%','L'}, {'%','M'}, {'%','N'}, {'%','O'}, {'%','W'}, {'+','A'}, {'+','B'}, {'+','C'},
    {'+','D'}, {'+','E'}, {'+','F'}, {'+','G'}, {'+','H'}, {'+','I'}, {'+','J'}, {'+','K'}, {'+','L'}, {'+','M'},
    {'+','N'}, {'+','O'}, {'+','P'}, {'+','Q'}, {'+','R'}, {'+','S'}, {'+','T'}, {'+','U'}, {'+','V'}, {'+','W'},
    {'+','X'}, {'+','Y'}, {'+','Z'}, {'%','P'}, {'%','Q'}, {'%','R'}, {'%','S'}, {'%','T'}
};

/* Code 93 ANSI/AIM BC5-1995 Table 3 */
static const char C93Ctrl[128][2] = {
    {'b','U'}, {'a','A'}, {'a','B'}, {'a','C'}, {'a','D'}, {'a','E'}, {'a','F'}, {'a','G'}, {'a','H'}, {'a','I'},
    {'a','J'}, {'a','K'}, {'a','L'}, {'a','M'}, {'a','N'}, {'a','O'}, {'a','P'}, {'a','Q'}, {'a','R'}, {'a','S'},
    {'a','T'}, {'a','U'}, {'a','V'}, {'a','W'}, {'a','X'}, {'a','Y'}, {'a','Z'}, {'b','A'}, {'b','B'}, {'b','C'},
    {'b','D'}, {'b','E'}, {  " "  }, {'c','A'}, {'c','B'}, {'c','C'}, {  "$"  }, {  "%"  }, {'c','F'}, {'c','G'},
    {'c','H'}, {'c','I'}, {'c','J'}, {  "+"  }, {'c','L'}, {  "-"  }, {  "."  }, {  "/"  }, {  "0"  }, {  "1"  },
    {  "2"  }, {  "3"  }, {  "4"  }, {  "5"  }, {  "6"  }, {  "7"  }, {  "8"  }, {  "9"  }, {'c','Z'}, {'b','F'},
    {'b','G'}, {'b','H'}, {'b','I'}, {'b','J'}, {'b','V'}, {  "A"  }, {  "B"  }, {  "C"  }, {  "D"  }, {  "E"  },
    {  "F"  }, {  "G"  }, {  "H"  }, {  "I"  }, {  "J"  }, {  "K"  }, {  "L"  }, {  "M"  }, {  "N"  }, {  "O"  },
    {  "P"  }, {  "Q"  }, {  "R"  }, {  "S"  }, {  "T"  }, {  "U"  }, {  "V"  }, {  "W"  }, {  "X"  }, {  "Y"  },
    {  "Z"  }, {'b','K'}, {'b','L'}, {'b','M'}, {'b','N'}, {'b','O'}, {'b','W'}, {'d','A'}, {'d','B'}, {'d','C'},
    {'d','D'}, {'d','E'}, {'d','F'}, {'d','G'}, {'d','H'}, {'d','I'}, {'d','J'}, {'d','K'}, {'d','L'}, {'d','M'},
    {'d','N'}, {'d','O'}, {'d','P'}, {'d','Q'}, {'d','R'}, {'d','S'}, {'d','T'}, {'d','U'}, {'d','V'}, {'d','W'},
    {'d','X'}, {'d','Y'}, {'d','Z'}, {'b','P'}, {'b','Q'}, {'b','R'}, {'b','S'}, {'b','T'}
};

/* Code 93 ANSI/AIM BC5-1995 Table 2 */
static const char C93Table[47][6] = {
    {'1','3','1','1','1','2'}, {'1','1','1','2','1','3'}, {'1','1','1','3','1','2'}, {'1','1','1','4','1','1'},
    {'1','2','1','1','1','3'}, {'1','2','1','2','1','2'}, {'1','2','1','3','1','1'}, {'1','1','1','1','1','4'},
    {'1','3','1','2','1','1'}, {'1','4','1','1','1','1'}, {'2','1','1','1','1','3'}, {'2','1','1','2','1','2'},
    {'2','1','1','3','1','1'}, {'2','2','1','1','1','2'}, {'2','2','1','2','1','1'}, {'2','3','1','1','1','1'},
    {'1','1','2','1','1','3'}, {'1','1','2','2','1','2'}, {'1','1','2','3','1','1'}, {'1','2','2','1','1','2'},
    {'1','3','2','1','1','1'}, {'1','1','1','1','2','3'}, {'1','1','1','2','2','2'}, {'1','1','1','3','2','1'},
    {'1','2','1','1','2','2'}, {'1','3','1','1','2','1'}, {'2','1','2','1','1','2'}, {'2','1','2','2','1','1'},
    {'2','1','1','1','2','2'}, {'2','1','1','2','2','1'}, {'2','2','1','1','2','1'}, {'2','2','2','1','1','1'},
    {'1','1','2','1','2','2'}, {'1','1','2','2','2','1'}, {'1','2','2','1','2','1'}, {'1','2','3','1','1','1'},
    {'1','2','1','1','3','1'}, {'3','1','1','1','1','2'}, {'3','1','1','2','1','1'}, {'3','2','1','1','1','1'},
    {'1','1','2','1','3','1'}, {'1','1','3','1','2','1'}, {'2','1','1','1','3','1'}, {'1','2','1','2','2','1'},
    {'3','1','2','1','1','1'}, {'3','1','1','1','2','1'}, {'1','2','2','2','1','1'}
};

/* Code 11 */
INTERNAL int code11(struct zint_symbol *symbol, unsigned char source[], int length) {

    int i;
    int h, c_digit, c_weight, c_count, k_digit, k_weight, k_count;
    int weight[141]; /* 140 + 1 extra for 1st check */
    char dest[864]; /* 6 + 140 * 6 + 2 * 6 + 5 + 1 = 864 */
    int error_number = 0;
    char *d = dest;
    int num_check_digits;
    char checkstr[3] = {0};
    static const char checkchrs[11] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '-' };

    /* Suppresses clang-tidy clang-analyzer-core.UndefinedBinaryOperatorResult warning */
    assert(length > 0);

    if (length > 140) { /* 8 (Start) + 140 * 8 + 2 * 8 (Check) + 7 (Stop) = 1151 */
        return errtxtf(ZINT_ERROR_TOO_LONG, symbol, 320, "Input length %d too long (maximum 140)", length);
    }
    if ((i = not_sane(SODIUM_MNS_F, source, length))) {
        return errtxtf(ZINT_ERROR_INVALID_DATA, symbol, 321,
                        "Invalid character at position %d in input (digits and \"-\" only)", i);
    }

    if (symbol->option_2 < 0 || symbol->option_2 > 2) {
        return errtxtf(ZINT_ERROR_INVALID_OPTION, symbol, 339, "Invalid check digit version '%d' (1 or 2 only)",
                        symbol->option_2);
    }
    if (symbol->option_2 == 2) {
        num_check_digits = 0;
    } else if (symbol->option_2 == 1) {
        num_check_digits = 1;
    } else {
        num_check_digits = 2;
    }

    c_weight = 1;
    c_count = 0;
    k_weight = 1;
    k_count = 0;

    /* start character */
    memcpy(d, C11Table[11], 6);
    d +=  6;

    /* Draw main body of barcode */
    for (i = 0; i < length; i++, d += 6) {
        if (source[i] == '-')
            weight[i] = 10;
        else
            weight[i] = ctoi(source[i]);
        memcpy(d, C11Table[weight[i]], 6);
    }

    if (num_check_digits) {
        /* Calculate C checksum */
        for (h = length - 1; h >= 0; h--) {
            c_count += (c_weight * weight[h]);
            c_weight++;

            if (c_weight > 10) {
                c_weight = 1;
            }
        }
        c_digit = c_count % 11;

        checkstr[0] = checkchrs[c_digit];
        memcpy(d, C11Table[c_digit], 6);
        d += 6;

        if (num_check_digits == 2) {
            weight[length] = c_digit;

            /* Calculate K checksum */
            for (h = length; h >= 0; h--) {
                k_count += (k_weight * weight[h]);
                k_weight++;

                if (k_weight > 9) {
                    k_weight = 1;
                }
            }
            k_digit = k_count % 11;

            checkstr[1] = checkchrs[k_digit];
            memcpy(d, C11Table[k_digit], 6);
            d += 6;
        }
    }

    if (symbol->debug & ZINT_DEBUG_PRINT) {
        printf("Check digit (%d): %s\n", num_check_digits, num_check_digits ? checkstr : "<none>");
    }

    /* Stop character */
    memcpy(d, C11Table[11], 5);
    d += 5;

    expand(symbol, dest, d - dest);

    /* TODO: Find documentation on BARCODE_CODE11 dimensions/height */

    ustrcpy(symbol->text, source);
    if (num_check_digits) {
        ustrcat(symbol->text, checkstr);
    }
    return error_number;
}

/* Code 39 */
INTERNAL int code39(struct zint_symbol *symbol, unsigned char source[], int length) {
    int i;
    int counter;
    int error_number = 0;
    int posns[86];
    char dest[890]; /* 10 (Start) + 86 * 10 + 10 (Check) + 9 (Stop) + 1 = 890 */
    char *d = dest;
    char localstr[2] = {0};

    counter = 0;

    if ((symbol->option_2 < 0) || (symbol->option_2 > 2)) {
        symbol->option_2 = 0;
    }

    /* LOGMARS MIL-STD-1189 Rev. B https://apps.dtic.mil/dtic/tr/fulltext/u2/a473534.pdf */
    if ((symbol->symbology == BARCODE_LOGMARS) && (length > 30)) { /* MIL-STD-1189 Rev. B Section 5.2.6.2 */
        return errtxtf(ZINT_ERROR_TOO_LONG, symbol, 322, "Input length %d too long (maximum 30)", length);
    /* Prevent encoded_data out-of-bounds >= 143 for BARCODE_HIBC_39 due to wider 'wide' bars */
    } else if ((symbol->symbology == BARCODE_HIBC_39) && (length > 70)) { /* 16 (Start) + 70*16 + 15 (Stop) = 1151 */
        /* 70 less '+' and check */
        return errtxtf(ZINT_ERROR_TOO_LONG, symbol, 319, "Input length %d too long (maximum 68)", length - 2);
    } else if (length > 86) { /* 13 (Start) + 86*13 + 12 (Stop) = 1143 */
        return errtxtf(ZINT_ERROR_TOO_LONG, symbol, 323, "Input length %d too long (maximum 86)", length);
    }

    to_upper(source, length);
    if ((i = not_sane_lookup(SILVER, 43 /* Up to "%" */, source, length, posns))) {
        return errtxtf(ZINT_ERROR_INVALID_DATA, symbol, 324,
                        "Invalid character at position %d in input (alphanumerics, space and \"-.$/+%%\" only)", i);
    }

    /* Start character */
    memcpy(d, C39Table[43], 10);
    d += 10;

    for (i = 0; i < length; i++, d += 10) {
        memcpy(d, C39Table[posns[i]], 10);
        counter += posns[i];
    }

    if (symbol->option_2 == 1 || symbol->option_2 == 2) { /* Visible or hidden check digit */

        char check_digit;
        counter %= 43;
        check_digit = SILVER[counter];
        memcpy(d, C39Table[counter], 10);
        d += 10;

        /* Display a space check digit as _, otherwise it looks like an error */
        if (check_digit == ' ') {
            check_digit = '_';
        }

        if (symbol->option_2 == 1) { /* Visible check digit */
            localstr[0] = check_digit;
            localstr[1] = '\0';
        }
        if (symbol->debug & ZINT_DEBUG_PRINT) printf("Check digit: %c\n", check_digit);
    }

    /* Stop character */
    memcpy(d, C39Table[43], 9);
    d += 9;

    if ((symbol->symbology == BARCODE_LOGMARS) || (symbol->symbology == BARCODE_HIBC_39)) {
        /* LOGMARS and HIBC use wider 'wide' bars than normal Code 39 */
        counter = d - dest;
        for (i = 0; i < counter; i++) {
            if (dest[i] == '2') {
                dest[i] = '3';
            }
        }
    }

    if (symbol->debug & ZINT_DEBUG_PRINT) {
        printf("Barspaces: %.*s\n", (int) (d - dest), dest);
    }

    expand(symbol, dest, d - dest);

    if (symbol->output_options & COMPLIANT_HEIGHT) {
        if (symbol->symbology == BARCODE_LOGMARS) {
            /* MIL-STD-1189 Rev. B Section 5.2
               Min height 0.25" / 0.04" (X max) = 6.25
               Default height 0.625" (average of 0.375" - 0.875") / 0.01375" (average of 0.0075" - 0.02") ~ 45.45 */
            const float default_height = 45.4545441f; /* 0.625 / 0.01375 */
            const float max_height = 116.666664f; /* 0.875 / 0.0075 */
            error_number = set_height(symbol, 6.25f, default_height, max_height, 0 /*no_errtxt*/);
        } else if (symbol->symbology == BARCODE_CODE39 || symbol->symbology == BARCODE_EXCODE39
                    || symbol->symbology == BARCODE_HIBC_39) {
            /* ISO/IEC 16388:2007 4.4 (e) recommended min height 5.0mm or 15% of width excluding quiet zones;
               as X left to application specification use
               width = (C + 2) * (3 * N + 6) * X + (C + 1) * I = (C + 2) * 9 + C + 1) * X = (10 * C + 19);
               use 50 as default as none recommended */
            const float min_height = stripf((10.0f * (symbol->option_2 == 1 ? length + 1 : length) + 19.0f) * 0.15f);
            error_number = set_height(symbol, min_height, min_height > 50.0f ? min_height : 50.0f, 0.0f,
                                        0 /*no_errtxt*/);
        }
        /* PZN and CODE32 set their own heights */
    } else {
        (void) set_height(symbol, 0.0f, 50.f, 0.0f, 1 /*no_errtxt*/);
    }

    if (symbol->symbology == BARCODE_CODE39) {
        ustrcpy(symbol->text, "*");
        ustrncat(symbol->text, source, length);
        ustrcat(symbol->text, localstr);
        ustrcat(symbol->text, "*");
    } else {
        ustrcpy(symbol->text, source);
        ustrcat(symbol->text, localstr);
    }
    return error_number;
}

/* Pharmazentralnummer (PZN) */
/* PZN https://www.ifaffm.de/mandanten/1/documents/04_ifa_coding_system/IFA_Info_Code_39_EN.pdf */
/* PZN https://www.ifaffm.de/mandanten/1/documents/04_ifa_coding_system/
       IFA-Info_Check_Digit_Calculations_PZN_PPN_UDI_EN.pdf */
INTERNAL int pzn(struct zint_symbol *symbol, unsigned char source[], int length) {

    int i, error_number, zeroes;
    int count, check_digit;
    unsigned char have_check_digit = '\0';
    char localstr[1 + 8 + 1]; /* '-' prefix + 8 digits + NUL */
    const int pzn7 = symbol->option_2 == 1;

    if (length > 8 - pzn7) {
        return errtxtf(ZINT_ERROR_TOO_LONG, symbol, 325, "Input length %1$d too long (maximum %2$d)", length,
                        8 - pzn7);
    }
    if (length == 8 - pzn7) {
        have_check_digit = source[7 - pzn7];
        length--;
    }
    if ((i = not_sane(NEON_F, source, length))) {
        return errtxtf(ZINT_ERROR_INVALID_DATA, symbol, 326,
                        "Invalid character at position %d in input (digits only)", i);
    }

    localstr[0] = '-';
    zeroes = 7 - pzn7 - length + 1;
    for (i = 1; i < zeroes; i++)
        localstr[i] = '0';
    ustrcpy(localstr + zeroes, source);

    count = 0;
    for (i = 1; i < 8 - pzn7; i++) {
        count += (i + pzn7) * ctoi(localstr[i]);
    }

    check_digit = count % 11;

    if (symbol->debug & ZINT_DEBUG_PRINT) {
        printf("PZN: %s, check digit %d\n", localstr, (int) check_digit);
    }

    if (check_digit == 10) {
        return errtxt(ZINT_ERROR_INVALID_DATA, symbol, 327, "Invalid PZN, check digit is '10'");
    }
    if (have_check_digit && ctoi(have_check_digit) != check_digit) {
        return errtxtf(ZINT_ERROR_INVALID_CHECK, symbol, 890, "Invalid check digit '%1$c', expecting '%2$c'",
                        have_check_digit, itoc(check_digit));
    }

    localstr[8 - pzn7] = itoc(check_digit);
    localstr[9 - pzn7] = '\0';

    if (pzn7) {
        symbol->option_2 = 0; /* Need to overwrite this so `code39()` doesn't add a check digit itself */
    }

    error_number = code39(symbol, (unsigned char *) localstr, 9 - pzn7);

    if (pzn7) {
        symbol->option_2 = 1; /* Restore */
    }

    ustrcpy(symbol->text, "PZN - "); /* Note changed to put space after hyphen */
    ustrcat(symbol->text, localstr + 1);

    if (symbol->output_options & COMPLIANT_HEIGHT) {
        /* Technical Information regarding PZN Coding V 2.1 (25 Feb 2019) Code size
           https://www.ifaffm.de/mandanten/1/documents/04_ifa_coding_system/IFA_Info_Code_39_EN.pdf
           "normal" X 0.25mm (0.187mm - 0.45mm), height 8mm - 20mm for 0.25mm X, 10mm mentioned so use that
           as default, 10mm / 0.25mm = 40 */
        if (error_number < ZINT_ERROR) {
            const float min_height = 17.7777786f; /* 8.0 / 0.45 */
            const float max_height = 106.951874f; /* 20.0 / 0.187 */
            error_number = set_height(symbol, min_height, 40.0f, max_height, 0 /*no_errtxt*/);
        }
    } else {
        if (error_number < ZINT_ERROR) {
            (void) set_height(symbol, 0.0f, 50.0f, 0.0f, 1 /*no_errtxt*/);
        }
    }

    return error_number;
}

/* Extended Code 39 - ISO/IEC 16388:2007 Annex A */
INTERNAL int excode39(struct zint_symbol *symbol, unsigned char source[], int length) {

    unsigned char buffer[86 * 2 + 1] = {0};
    unsigned char *b = buffer;
    unsigned char check_digit = '\0';
    int i;
    int error_number;

    if (length > 86) {
        return errtxtf(ZINT_ERROR_TOO_LONG, symbol, 328, "Input length %d too long (maximum 86)", length);
    }

    /* Creates a buffer string and places control characters into it */
    for (i = 0; i < length; i++) {
        if (source[i] > 127) {
            /* Cannot encode extended ASCII */
            return errtxtf(ZINT_ERROR_INVALID_DATA, symbol, 329,
                            "Invalid character at position %d in input, extended ASCII not allowed", i + 1);
        }
        memcpy(b, EC39Ctrl[source[i]], 2);
        b += EC39Ctrl[source[i]][1] ? 2 : 1;
    }
    if (b - buffer > 86) {
        return errtxtf(ZINT_ERROR_TOO_LONG, symbol, 317, "Input too long, requires %d symbol characters (maximum 86)",
                        (int) (b - buffer));
    }
    *b = '\0';

    /* Then sends the buffer to the C39 function */
    error_number = code39(symbol, buffer, b - buffer);

    /* Save visible check digit */
    if (symbol->option_2 == 1) {
        const int len = (int) ustrlen(symbol->text);
        if (len > 0) {
            check_digit = symbol->text[len - 1];
        }
    }

    /* Copy over source to HRT, subbing space for unprintables */
    for (i = 0; i < length; i++)
        symbol->text[i] = source[i] >= ' ' && source[i] != 0x7F ? source[i] : ' ';

    if (check_digit) {
        symbol->text[i++] = check_digit;
    }
    symbol->text[i] = '\0';

    return error_number;
}

/* Code 93 is an advancement on Code 39 and the definition is a lot tighter */
INTERNAL int code93(struct zint_symbol *symbol, unsigned char source[], int length) {

    /* SILVER includes the extra characters a, b, c and d to represent Code 93 specific
       shift characters 1, 2, 3 and 4 respectively. These characters are never used by
       `code39()` and `excode39()` */

    int i;
    int h, weight, c, k, error_number = 0;
    int values[125]; /* 123 + 2 (Checks) */
    char buffer[247]; /* 123*2 (123 full ASCII) + 1 = 247 */
    char *b = buffer;
    char dest[764]; /* 6 (Start) + 123*6 + 2*6 (Checks) + 7 (Stop) + 1 (NUL) = 764 */
    char *d = dest;

    /* Suppresses clang-tidy clang-analyzer-core.CallAndMessage warning */
    assert(length > 0);

    if (length > 123) { /* 9 (Start) + 123*9 + 2*9 (Checks) + 10 (Stop) = 1144 */
        return errtxtf(ZINT_ERROR_TOO_LONG, symbol, 330, "Input length %d too long (maximum 123)", length);
    }

    /* Message Content */
    for (i = 0; i < length; i++) {
        if (source[i] > 127) {
            /* Cannot encode extended ASCII */
            return errtxtf(ZINT_ERROR_INVALID_DATA, symbol, 331,
                            "Invalid character at position %d in input, extended ASCII not allowed", i + 1);
        }
        memcpy(b, C93Ctrl[source[i]], 2);
        b += C93Ctrl[source[i]][1] ? 2 : 1;
        symbol->text[i] = source[i] >= ' ' && source[i] != 0x7F ? source[i] : ' ';
    }

    /* Now we can check the true length of the barcode */
    h = b - buffer;
    if (h > 123) {
        return errtxtf(ZINT_ERROR_TOO_LONG, symbol, 332,
                        "Input too long, requires %d symbol characters (maximum 123)", h);
    }

    for (i = 0; i < h; i++) {
        values[i] = posn(SILVER, buffer[i]);
    }

    /* Putting the data into dest[] is not done until after check digits are calculated */

    /* Check digit C */
    c = 0;
    weight = 1;
    for (i = h - 1; i >= 0; i--) {
        c += values[i] * weight;
        weight++;
        if (weight == 21)
            weight = 1;
    }
    c = c % 47;
    values[h] = c;

    /* Check digit K */
    k = 0;
    weight = 1;
    for (i = h; i >= 0; i--) {
        k += values[i] * weight;
        weight++;
        if (weight == 16)
            weight = 1;
    }
    k = k % 47;
    values[h + 1] = k;
    h += 2;

    if (symbol->debug & ZINT_DEBUG_PRINT) {
        printf("Check digit c: %d, k: %d\n", c, k);
    }

    /* Start character */
    memcpy(d, "111141", 6);
    d += 6;

    for (i = 0; i < h; i++, d += 6) {
        memcpy(d, C93Table[values[i]], 6);
    }

    /* Stop character */
    memcpy(d, "1111411", 7);
    d += 7;

    expand(symbol, dest, d - dest);

    if (symbol->output_options & COMPLIANT_HEIGHT) {
        /* ANSI/AIM BC5-1995 Section 2.6 minimum height 0.2" or 15% of symbol length, whichever is greater
           no max X given so for min height use symbol length = (9 * (C + 4) + 1) * X + 2 * Q = symbol->width + 20;
           use 40 as default height based on figures in spec */
        const float min_height = stripf((symbol->width + 20) * 0.15f);
        error_number = set_height(symbol, min_height, min_height > 40.0f ? min_height : 40.0f, 0.0f, 0 /*no_errtxt*/);
    } else {
        (void) set_height(symbol, 0.0f, 50.0f, 0.0f, 1 /*no_errtxt*/);
    }

    if (symbol->option_2 == 1) {
        symbol->text[length] = SILVER[c];
        symbol->text[length + 1] = SILVER[k];
        symbol->text[length + 2] = '\0';
    }

    return error_number;
}

typedef const struct s_channel_precalc {
    int value; unsigned char B[8]; unsigned char S[8]; unsigned char bmax[7]; unsigned char smax[7];
} channel_precalc;

#if 0
#define CHANNEL_GENERATE_PRECALCS
#endif

#ifdef CHANNEL_GENERATE_PRECALCS
/* To generate precalc tables uncomment CHANNEL_GENERATE_PRECALCS define and run
   "backend/tests/test_channel -f generate -g" and place result in "channel_precalcs.h" */
static void channel_generate_precalc(int channels, int value, int mod, int last, int B[8], int S[8], int bmax[7],
            int smax[7]) {
    int i;
    if (value == mod) printf("static channel_precalc channel_precalcs%d[] = {\n", channels);
    printf("    { %7ld, {", value); for (i = 0; i < 8; i++) printf(" %d,", B[i]); fputs(" },", stdout);
    fputs(" {", stdout); for (i = 0; i < 8; i++) printf(" %d,", S[i]); fputs(" },", stdout);
    fputs(" {", stdout); for (i = 0; i < 7; i++) printf(" %d,", bmax[i]); fputs(" },", stdout);
    fputs(" {", stdout); for (i = 0; i < 7; i++) printf(" %d,", smax[i]); fputs(" }, },\n", stdout);
    if (value == last) fputs("};\n", stdout);
}
#else
#include "channel_precalcs.h"
#endif

static int channel_copy_precalc(channel_precalc *const precalc, int B[8], int S[8], int bmax[7], int smax[7]) {
    int i;

    for (i = 0; i < 7; i++) {
        B[i] = precalc->B[i];
        S[i] = precalc->S[i];
        bmax[i] = precalc->bmax[i];
        smax[i] = precalc->smax[i];
    }
    B[7] = precalc->B[7];
    S[7] = precalc->S[7];

    return precalc->value;
}

/* CHNCHR is adapted from ANSI/AIM BC12-1998 Annex D Figure D5 and is Copyright (c) AIM 1997 */

/* It is used here on the understanding that it forms part of the specification
   for Channel Code and therefore its use is permitted under the following terms
   set out in that document:

   "It is the intent and understanding of AIM [t]hat the symbology presented in this
   specification is entirely in the public domain and free of all use restrictions,
   licenses and fees. AIM USA, its member companies, or individual officers
   assume no liability for the use of this document." */
static void CHNCHR(int channels, int target_value, int B[8], int S[8]) {
    /* Use of initial pre-calculations taken from Barcode Writer in Pure PostScript (BWIPP)
     * Copyright (c) 2004-2020 Terry Burton (MIT/X-Consortium license) */
    static channel_precalc initial_precalcs[6] = {
        { 0, { 1, 1, 1, 1, 1, 2, 1, 2, }, { 1, 1, 1, 1, 1, 1, 1, 3, }, { 1, 1, 1, 1, 1, 3, 2, },
            { 1, 1, 1, 1, 1, 3, 3, }, },
        { 0, { 1, 1, 1, 1, 2, 1, 1, 3, }, { 1, 1, 1, 1, 1, 1, 1, 4, }, { 1, 1, 1, 1, 4, 3, 3, },
            { 1, 1, 1, 1, 4, 4, 4, }, },
        { 0, { 1, 1, 1, 2, 1, 1, 2, 3, }, { 1, 1, 1, 1, 1, 1, 1, 5, }, { 1, 1, 1, 5, 4, 4, 4, },
            { 1, 1, 1, 5, 5, 5, 5, }, },
        { 0, { 1, 1, 2, 1, 1, 2, 1, 4, }, { 1, 1, 1, 1, 1, 1, 1, 6, }, { 1, 1, 6, 5, 5, 5, 4, },
            { 1, 1, 6, 6, 6, 6, 6, }, },
        { 0, { 1, 2, 1, 1, 2, 1, 1, 5, }, { 1, 1, 1, 1, 1, 1, 1, 7, }, { 1, 7, 6, 6, 6, 5, 5, },
            { 1, 7, 7, 7, 7, 7, 7, }, },
        { 0, { 2, 1, 1, 2, 1, 1, 2, 5, }, { 1, 1, 1, 1, 1, 1, 1, 8, }, { 8, 7, 7, 7, 6, 6, 6, },
            { 8, 8, 8, 8, 8, 8, 8, }, },
    };
    int bmax[7], smax[7];
    int value = 0;

    channel_copy_precalc(&initial_precalcs[channels - 3], B, S, bmax, smax);

#ifndef CHANNEL_GENERATE_PRECALCS
    if (channels == 7 && target_value >= channel_precalcs7[0].value) {
        value = channel_copy_precalc(&channel_precalcs7[(target_value / channel_precalcs7[0].value) - 1], B, S, bmax,
                                    smax);
    } else if (channels == 8 && target_value >= channel_precalcs8[0].value) {
        value = channel_copy_precalc(&channel_precalcs8[(target_value / channel_precalcs8[0].value) - 1], B, S, bmax,
                                    smax);
    }
#endif

    goto chkchr;

ls0:smax[1] = smax[0] + 1 - S[0]; B[0] = 1;
    if (S[0] == 1) goto nb0;
lb0:    bmax[1] = bmax[0] + 1 - B[0]; S[1] = 1;
ls1:        smax[2] = smax[1] + 1 - S[1]; B[1] = 1;
            if (S[0] + B[0] + S[1] == 3) goto nb1;
lb1:            bmax[2] = bmax[1] + 1 - B[1]; S[2] = 1;
ls2:                smax[3] = smax[2] + 1 - S[2]; B[2] = 1;
                    if (B[0] + S[1] + B[1] + S[2] == 4) goto nb2;
lb2:                    bmax[3] = bmax[2] + 1 - B[2]; S[3] = 1;
ls3:                        smax[4] = smax[3] + 1 - S[3]; B[3] = 1;
                            if (B[1] + S[2] + B[2] + S[3] == 4) goto nb3;
lb3:                            bmax[4] = bmax[3] + 1 - B[3]; S[4] = 1;
ls4:                                smax[5] = smax[4] + 1 - S[4]; B[4] = 1;
                                    if (B[2] + S[3] + B[3] + S[4] == 4) goto nb4;
lb4:                                    bmax[5] = bmax[4] + 1 - B[4]; S[5] = 1;
ls5:                                        smax[6] = smax[5] + 1 - S[5]; B[5] = 1;
                                            if (B[3] + S[4] + B[4] + S[5] == 4) goto nb5;
lb5:                                            bmax[6] = bmax[5] + 1 - B[5]; S[6] = 1;
ls6:                                                S[7] = smax[6] + 1 - S[6]; B[6] = 1;
                                                    if (B[4] + S[5] + B[5] + S[6] == 4) goto nb6;
lb6:                                                    B[7] = bmax[6] + 1 - B[6];
                                                        if (B[5] + S[6] + B[6] + S[7] + B[7] == 5) goto nb6;
chkchr:
#ifdef CHANNEL_GENERATE_PRECALCS
                                                        /* 115338 == (576688 + 2) / 5 */
                                                        if (channels == 7 && value && value % 115338 == 0) {
                                                            channel_generate_precalc(channels, value, 115338,
                                                                                115338 * (5 - 1), B, S, bmax, smax);
                                                        /* 119121 == (7742862 + 3) / 65 */
                                                        } else if (channels == 8 && value && value % 119121 == 0) {
                                                            channel_generate_precalc(channels, value, 119121,
                                                                                119121 * (65 - 1), B, S, bmax, smax);
                                                        }
#endif
                                                        if (value == target_value) return;
                                                        value++;
nb6:                                                    if (++B[6] <= bmax[6]) goto lb6;
                                                    if (++S[6] <= smax[6]) goto ls6;
nb5:                                            if (++B[5] <= bmax[5]) goto lb5;
                                            if (++S[5] <= smax[5]) goto ls5;
nb4:                                    if (++B[4] <= bmax[4]) goto lb4;
                                    if (++S[4] <= smax[4]) goto ls4;
nb3:                            if (++B[3] <= bmax[3]) goto lb3;
                            if (++S[3] <= smax[3]) goto ls3;
nb2:                    if (++B[2] <= bmax[2]) goto lb2;
                    if (++S[2] <= smax[2]) goto ls2;
nb1:            if (++B[1] <= bmax[1]) goto lb1;
            if (++S[1] <= smax[1]) goto ls1;
nb0:    if (++B[0] <= bmax[0]) goto lb0;
    if (++S[0] <= smax[0]) goto ls0;
}

/* Channel Code - According to ANSI/AIM BC12-1998 */
INTERNAL int channel(struct zint_symbol *symbol, unsigned char source[], int length) {
    static const int max_ranges[] = { -1, -1, -1, 26, 292, 3493, 44072, 576688, 7742862 };
    int S[8] = {0}, B[8] = {0};
    int target_value;
    char dest[30];
    char *d = dest;
    int channels, i;
    int error_number = 0, zeroes;

    if (length > 7) {
        return errtxtf(ZINT_ERROR_TOO_LONG, symbol, 333, "Input length %d too long (maximum 7)", length);
    }
    if ((i = not_sane(NEON_F, source, length))) {
        return errtxtf(ZINT_ERROR_INVALID_DATA, symbol, 334,
                        "Invalid character at position %d in input (digits only)", i);
    }
    target_value = to_int(source, length);

    if ((symbol->option_2 < 3) || (symbol->option_2 > 8)) {
        channels = 0;
    } else {
        channels = symbol->option_2;
    }

    if (channels == 0) {
        channels = length + 1;
        if (target_value > 576688 && channels < 8) {
            channels = 8;
        } else if (target_value > 44072 && channels < 7) {
            channels = 7;
        } else if (target_value > 3493 && channels < 6) {
            channels = 6;
        } else if (target_value > 292 && channels < 5) {
            channels = 5;
        } else if (target_value > 26 && channels < 4) {
            channels = 4;
        }
    }
    if (channels == 2) {
        channels = 3;
    }

    if (target_value > max_ranges[channels]) {
        if (channels == 8) {
            return errtxtf(ZINT_ERROR_INVALID_DATA, symbol, 318, "Input value \"%1$d\" out of range (0 to %2$d)",
                            target_value, max_ranges[channels]);
        }
        return errtxtf(ZINT_ERROR_INVALID_DATA, symbol, 335,
                        "Input value \"%1$d\" out of range (0 to %2$d for %3$d channels)",
                        target_value, max_ranges[channels], channels);
    }

    CHNCHR(channels, target_value, B, S);

    memcpy(d, "111111111", 9); /* Finder pattern */
    d += 9;
    for (i = 8 - channels; i < 8; i++) {
        *d++ = itoc(S[i]);
        *d++ = itoc(B[i]);
    }

    zeroes = channels - 1 - length;
    if (zeroes < 0) {
        zeroes = 0;
    } else if (zeroes) {
        memset(symbol->text, '0', zeroes);
    }
    ustrcpy(symbol->text + zeroes, source);

    expand(symbol, dest, d - dest);

    if (symbol->output_options & COMPLIANT_HEIGHT) {
        /* ANSI/AIM BC12-1998 gives min height as 5mm or 15% of length; X left as application specification so use
           length = 1X (left qz) + (9 (finder) + 4 * 8 - 2) * X + 2X (right qz);
           use 20 as default based on figures in spec */
        const float min_height = stripf((1 + 9 + 4 * channels - 2 + 2) * 0.15f);
        error_number = set_height(symbol, min_height, 20.0f, 0.0f, 0 /*no_errtxt*/);
    } else {
        (void) set_height(symbol, 0.0f, 50.0f, 0.0f, 1 /*no_errtxt*/);
    }

    return error_number;
}

/* Vehicle Identification Number (VIN) */
INTERNAL int vin(struct zint_symbol *symbol, unsigned char source[], int length) {

    /* This code verifies the check digit present in North American VIN codes */

    char dest[200]; /* 10 + 10 + 17 * 10 + 9 + 1 = 200 */
    char *d = dest;
    char input_check;
    char output_check;
    int sum;
    int i;
    static const char weight[17] = { 8, 7, 6, 5, 4, 3, 2, 10, 0, 9, 8, 7, 6, 5, 4, 3, 2 };

    /* Check length */
    if (length != 17) {
        return errtxtf(ZINT_ERROR_TOO_LONG, symbol, 336, "Input length %d wrong (17 only)", length);
    }

    /* Check input characters, I, O and Q are not allowed */
    if ((i = not_sane(ARSENIC_F, source, length))) {
        return errtxtf(ZINT_ERROR_INVALID_DATA, symbol, 337,
                        "Invalid character at position %d in input (alphanumerics only, excluding \"IOQ\")", i);
    }

    to_upper(source, length);

    /* Check digit only valid for North America */
    if (source[0] >= '1' && source[0] <= '5') {
        input_check = source[8];

        sum = 0;
        for (i = 0; i < 17; i++) {
            int value;
            if (source[i] <= '9') {
                value = source[i] - '0';
            } else if (source[i] <= 'H') {
                value = (source[i] - 'A') + 1;
            } else if (source[i] <= 'R') {
                value = (source[i] - 'J') + 1;
            } else { /* (source[i] >= 'S') && (source[i] <= 'Z') */
                value = (source[i] - 'S') + 2;
            }
            sum += value * weight[i];
        }

        output_check = '0' + (sum % 11);

        if (output_check == ':') {
            /* Check digit was 10 */
            output_check = 'X';
        }

        if (symbol->debug & ZINT_DEBUG_PRINT) {
            printf("Producing VIN code: %s\n", source);
            printf("Input check was %c, calculated check is %c\n", input_check, output_check);
        }

        if (input_check != output_check) {
            return errtxtf(ZINT_ERROR_INVALID_CHECK, symbol, 338,
                            "Invalid check digit '%1$c' (position 9), expecting '%2$c'", input_check, output_check);
        }
    }

    /* Start character */
    memcpy(d, C39Table[43], 10);
    d += 10;

    /* Import character 'I' prefix? */
    if (symbol->option_2 & 1) {
        memcpy(d, C39Table[18], 10);
        d += 10;
    }

    /* Copy glyphs to symbol */
    for (i = 0; i < 17; i++, d += 10) {
        memcpy(d, C39Table[posn(SILVER, source[i])], 10);
    }

    /* Stop character */
    memcpy(d, C39Table[43], 9);
    d += 9;

    expand(symbol, dest, d - dest);

    ustrcpy(symbol->text, source);

    /* Specification of dimensions/height for BARCODE_VIN unlikely */

    return 0;
}

/* vim: set ts=4 sw=4 et : */
