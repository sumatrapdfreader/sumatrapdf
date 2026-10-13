/* medical.c - Handles 1 track and 2 track pharmacode and Codabar */
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

#include <stdio.h>
#include "common.h"

INTERNAL int code39(struct zint_symbol *symbol, unsigned char source[], int length);

static const char CALCIUM[] = "0123456789-$:/.+ABCD";
#define CALCIUM_INNER_F (IS_NUM_F | IS_MNS_F | IS_CLI_F | IS_PLS_F) /* CALCIUM_INNER "0123456789-$:/.+" */

/* Codabar table checked against EN 798:1995 */
static const char CodaTable[20][8] = {
    {'1','1','1','1','1','2','2','1'}, {'1','1','1','1','2','2','1','1'}, {'1','1','1','2','1','1','2','1'},
    {'2','2','1','1','1','1','1','1'}, {'1','1','2','1','1','2','1','1'}, {'2','1','1','1','1','2','1','1'},
    {'1','2','1','1','1','1','2','1'}, {'1','2','1','1','2','1','1','1'}, {'1','2','2','1','1','1','1','1'},
    {'2','1','1','2','1','1','1','1'}, {'1','1','1','2','2','1','1','1'}, {'1','1','2','2','1','1','1','1'},
    {'2','1','1','1','2','1','2','1'}, {'2','1','2','1','1','1','2','1'}, {'2','1','2','1','2','1','1','1'},
    {'1','1','2','1','2','1','2','1'}, {'1','1','2','2','1','2','1','1'}, {'1','2','1','2','1','1','2','1'},
    {'1','1','1','2','1','2','2','1'}, {'1','1','1','2','2','2','1','1'}
};

INTERNAL int pharma(struct zint_symbol *symbol, unsigned char source[], int length) {
    /* "Pharmacode can represent only a single integer from 3 to 131070. Unlike other
       commonly used one-dimensional barcode schemes, pharmacode does not store the data in a
       form corresponding to the human-readable digits; the number is encoded in binary, rather
       than decimal. Pharmacode is read from right to left: with n as the bar position starting
       at 0 on the right, each narrow bar adds 2^n to the value and each wide bar adds 2(2^n).
       The minimum barcode is 2 bars and the maximum 16, so the smallest number that could
       be encoded is 3 (2 narrow bars) and the biggest is 131070 (16 wide bars)."
       - http://en.wikipedia.org/wiki/Pharmacode */

    /* This code uses the One Track Pharamacode calculating algorithm as recommended by
       the specification at http://www.laetus.com/laetus.php?request=file&id=69
       (http://www.gomaro.ch/ftproot/Laetus_PHARMA-CODE.pdf) */

    int i;
    int tester;
    int counter, error_number = 0, h;
    char inter[18] = {0}; /* 131070 -> 17 bits */
    char *in = inter;
    char dest[64]; /* 17 * 2 + 1 */
    char *d = dest;

    if (length > 6) {
        return errtxtf(ZINT_ERROR_TOO_LONG, symbol, 350, "Input length %d too long (maximum 6)", length);
    }
    if ((i = not_sane(NEON_F, source, length))) {
        return errtxtf(ZINT_ERROR_INVALID_DATA, symbol, 351,
                        "Invalid character at position %d in input (digits only)", i);
    }

    tester = to_int(source, length);
    if ((tester < 3) || (tester > 131070)) {
        return errtxtf(ZINT_ERROR_INVALID_DATA, symbol, 352, "Input value '%d' out of range (3 to 131070)", tester);
    }

    do {
        if (!(tester & 1)) {
            *in++ = 'W';
            tester = (tester - 2) / 2;
        } else {
            *in++ = 'N';
            tester = (tester - 1) / 2;
        }
    } while (tester != 0);

    h = in - inter;
    for (counter = h - 1; counter >= 0; counter--) {
        *d++ = inter[counter] == 'W' ? '3' : '1';
        *d++ = '2';
    }
    *--d = '\0'; /* Chop off final bar */

    expand(symbol, dest, d - dest);

    if (symbol->output_options & COMPLIANT_HEIGHT) {
        /* Laetus Pharmacode Guide 1.2 Standard one-track height 8mm / 0.5mm (X) */
        error_number = set_height(symbol, 16.0f, 0.0f, 0.0f, 0 /*no_errtxt*/);
    } else {
        (void) set_height(symbol, 0.0f, 50.0f, 0.0f, 1 /*no_errtxt*/);
    }

    return error_number;
}

static int pharma_two_calc(int tester, char *d) {
    /* This code uses the Two Track Pharamacode defined in the document at
       http://www.laetus.com/laetus.php?request=file&id=69 and using a modified
       algorithm from the One Track system. This standard accepts integet values
       from 4 to 64570080. */

    int counter, h;
    char inter[17];
    char *in = inter;

    do {
        switch (tester % 3) {
            case 0:
                *in++ = '3';
                tester = (tester - 3) / 3;
                break;
            case 1:
                *in++ = '1';
                tester = (tester - 1) / 3;
                break;
            case 2:
                *in++ = '2';
                tester = (tester - 2) / 3;
                break;
        }
    } while (tester != 0);

    h = in - inter;
    for (counter = h - 1; counter >= 0; counter--) {
        *d++ = inter[counter];
    }
    *d = '\0';

    return h;
}

INTERNAL int pharma_two(struct zint_symbol *symbol, unsigned char source[], int length) {
    /* Draws the patterns for two track pharmacode */
    int i;
    int tester;
    char height_pattern[200];
    unsigned int loopey, h;
    int writer;
    int error_number = 0;

    if (length > 8) {
        return errtxtf(ZINT_ERROR_TOO_LONG, symbol, 354, "Input length %d too long (maximum 8)", length);
    }
    if ((i = not_sane(NEON_F, source, length))) {
        return errtxtf(ZINT_ERROR_INVALID_DATA, symbol, 355,
                        "Invalid character at position %d in input (digits only)", i);
    }

    tester = to_int(source, length);
    if ((tester < 4) || (tester > 64570080)) {
        return errtxtf(ZINT_ERROR_INVALID_DATA, symbol, 353, "Input value '%d' out of range (4 to 64570080)", tester);
    }
    h = pharma_two_calc(tester, height_pattern);

    writer = 0;
    for (loopey = 0; loopey < h; loopey++) {
        if ((height_pattern[loopey] == '2') || (height_pattern[loopey] == '3')) {
            set_module(symbol, 0, writer);
        }
        if ((height_pattern[loopey] == '1') || (height_pattern[loopey] == '3')) {
            set_module(symbol, 1, writer);
        }
        writer += 2;
    }
    symbol->rows = 2;
    symbol->width = writer - 1;

    if (symbol->output_options & COMPLIANT_HEIGHT) {
        /* Laetus Pharmacode Guide 1.4
           Two-track height min 8mm / 2mm (X max) = 4X (2X per row), standard 8mm / 1mm = 8X,
           max 12mm / 0.8mm (X min) = 15X */
        error_number = set_height(symbol, 2.0f, 8.0f, 15.0f, 0 /*no_errtxt*/);
    } else {
        (void) set_height(symbol, 0.0f, 10.0f, 0.0f, 1 /*no_errtxt*/);
    }

    return error_number;
}

/* The Codabar system consisting of simple substitution */
INTERNAL int codabar(struct zint_symbol *symbol, unsigned char source[], int length) {

    int i, error_number = 0;
    int posns[103];
    char dest[833]; /* (103 + 1) * 8 + 1 == 833 */
    char *d = dest;
    int add_checksum, count = 0, checksum = 0;
    int d_chars = 0;

    if (length > 103) { /* No stack smashing please (103 + 1) * 11 = 1144 */
        return errtxtf(ZINT_ERROR_TOO_LONG, symbol, 356, "Input length %d too long (maximum 103)", length);
    }
    /* BS EN 798:1995 4.2 "'Codabar' symbols shall consist of ... b) start character;
       c) one or more symbol characters representing data ... d) stop character ..." */
    if (length < 3) {
        return errtxtf(ZINT_ERROR_TOO_LONG, symbol, 362, "Input length %d too short (minimum 3)", length);
    }
    to_upper(source, length);

    /* Codabar must begin and end with the characters A, B, C or D */
    if ((source[0] != 'A') && (source[0] != 'B') && (source[0] != 'C')
            && (source[0] != 'D')) {
        return errtxt(ZINT_ERROR_INVALID_DATA, symbol, 358, "Does not begin with \"A\", \"B\", \"C\" or \"D\"");
    }
    if ((source[length - 1] != 'A') && (source[length - 1] != 'B') &&
            (source[length - 1] != 'C') && (source[length - 1] != 'D')) {
        return errtxt(ZINT_ERROR_INVALID_DATA, symbol, 359, "Does not end with \"A\", \"B\", \"C\" or \"D\"");
    }
    if ((i = not_sane_lookup(CALCIUM, sizeof(CALCIUM) - 1, source, length, posns))) {
        return errtxtf(ZINT_ERROR_INVALID_DATA, symbol, 357,
                        "Invalid character at position %1$d in input (\"%2$s\" only)", i, CALCIUM);
    }
    /* And must not use A, B, C or D otherwise (BS EN 798:1995 4.3.2) */
    if ((i = not_sane(CALCIUM_INNER_F, source + 1, length - 2))) {
        return errtxtf(ZINT_ERROR_INVALID_DATA, symbol, 363,
                        "Invalid character at position %d in input (cannot contain \"A\", \"B\", \"C\" or \"D\")", i);
    }

    /* Add check character: 1 don't show to HRT, 2 do show to HRT
      (unfortunately to maintain back-compatibility, this is reverse of C25) */
    add_checksum = symbol->option_2 == 1 || symbol->option_2 == 2;

    for (i = 0; i < length; i++, d += 8) {
        if (add_checksum) {
            /* BS EN 798:1995 A.3 suggests using ISO 7064 algorithm but leaves it application defined.
               Following BWIPP and TEC-IT, use this simple mod-16 algorithm (not in ISO 7064) */
            count += posns[i];
            if (i + 1 == length) {
                checksum = count % 16;
                if (checksum) {
                    checksum = 16 - checksum;
                }
                if (symbol->debug & ZINT_DEBUG_PRINT) {
                    printf("Codabar: %s, count %d, checksum %d (%c)\n", source, count, checksum, CALCIUM[checksum]);
                }
                memcpy(d, CodaTable[checksum], 8);
                d += 8;
            }
        }
        memcpy(d, CodaTable[posns[i]], 8);
        if (source[i] == '/' || source[i] == ':' || source[i] == '.' || source[i] == '+') { /* Wide data characters */
            d_chars++;
        }
    }

    expand(symbol, dest, d - dest);

    if (symbol->output_options & COMPLIANT_HEIGHT) {
        /* BS EN 798:1995 4.4.1 (d) max of 5mm / 0.43mm (X max) ~ 11.628 or 15% of width where (taking N =
           narrow/wide ratio as 2 and I = X) width = ((2 * N + 5) * C + (N – 1) * (D + 2)) * X + I * (C – 1) + 2Q
           = ((4 + 5) * C + (D + 2) + C - 1 + 2 * 10) * X = (10 * C + D + 21) * X
           Length (C) includes start/stop chars */
        const float min_height_min = 11.6279068f; /* 5.0 / 0.43 */
        float min_height = stripf((10.0f * ((add_checksum ? length + 1 : length) + 2.0f) + d_chars + 21.0f) * 0.15f);
        if (min_height < min_height_min) {
            min_height = min_height_min;
        }
        /* Using 50 as default as none recommended */
        error_number = set_height(symbol, min_height, min_height > 50.0f ? min_height : 50.0f, 0.0f, 0 /*no_errtxt*/);
    } else {
        (void) set_height(symbol, 0.0f, 50.0f, 0.0f, 1 /*no_errtxt*/);
    }

    ustrcpy(symbol->text, source);
    if (symbol->option_2 == 2) {
        symbol->text[length - 1] = CALCIUM[checksum]; /* Place before final A/B/C/D character (BS EN 798:1995 A.3) */
        symbol->text[length] = source[length - 1];
        symbol->text[length + 1] = '\0';
    }

    return error_number;
}

/* Italian Pharmacode */
INTERNAL int code32(struct zint_symbol *symbol, unsigned char source[], int length) {
    static const char TABELLA[] = "0123456789BCDFGHJKLMNPQRSTUVWXYZ";
    int i, zeroes, error_number = 0, checksum, checkpart, checkdigit;
    char localstr[10], risultante[7];
    unsigned int pharmacode, devisor;
    int codeword[6];

    /* Validate the input */
    if (length > 8) {
        return errtxtf(ZINT_ERROR_TOO_LONG, symbol, 360, "Input length %d too long (maximum 8)", length);
    }
    if ((i = not_sane(NEON_F, source, length))) {
        return errtxtf(ZINT_ERROR_INVALID_DATA, symbol, 361,
                        "Invalid character at position %d in input (digits only)", i);
    }

    /* Add leading zeros as required */
    zeroes = 8 - length;
    memset(localstr, '0', zeroes);
    ustrcpy(localstr + zeroes, source);

    /* Calculate the check digit */
    checksum = 0;
    for (i = 0; i < 4; i++) {
        checkpart = ctoi(localstr[i * 2]);
        checksum += checkpart;
        checkpart = 2 * (ctoi(localstr[(i * 2) + 1]));
        if (checkpart >= 10) {
            checksum += (checkpart - 10) + 1;
        } else {
            checksum += checkpart;
        }
    }

    /* Add check digit to data string */
    checkdigit = checksum % 10;
    localstr[8] = itoc(checkdigit);
    localstr[9] = '\0';

    /* Convert string into an integer value */
    pharmacode = atoi(localstr);

    /* Convert from decimal to base-32 */
    devisor = 33554432;
    for (i = 5; i >= 0; i--) {
        unsigned int remainder;
        codeword[i] = pharmacode / devisor;
        remainder = pharmacode % devisor;
        pharmacode = remainder;
        devisor /= 32;
    }

    /* Look up values in 'Tabella di conversione' */
    for (i = 5; i >= 0; i--) {
        risultante[5 - i] = TABELLA[codeword[i]];
    }
    risultante[6] = '\0';
    /* Plot the barcode using Code 39 */
    error_number = code39(symbol, (unsigned char *) risultante, 6);
    if (error_number != 0) { /* Should never happen */
        return error_number; /* Not reached */
    }

    if (symbol->output_options & COMPLIANT_HEIGHT) {
        /* Allegato A Caratteristiche tecniche del bollino farmaceutico
           (https://www.gazzettaufficiale.it/do/atto/serie_generale/caricaPdf?cdimg=14A0566800100010110001
            &dgu=2014-07-18&art.dataPubblicazioneGazzetta=2014-07-18&art.codiceRedazionale=14A05668&art.num=1
            &art.tiposerie=SG)
           X given as 0.250mm; height (and quiet zones) left to ISO/IEC 16388:2007 (Code 39)
           So min height 5mm = 5mm / 0.25mm = 20 > 15% of width, i.e. (10 * 8 + 19) * 0.15 = 14.85 */
        error_number = set_height(symbol, 20.0f, 20.0f, 0.0f, 0 /*no_errtxt*/); /* Use as default also */
    } else {
        (void) set_height(symbol, 0.0f, 50.0f, 0.0f, 1 /*no_errtxt*/);
    }

    /* Override the normal text output with the Pharmacode number */
    ustrcpy(symbol->text, "A");
    ustrcat(symbol->text, localstr);

    return error_number;
}

/* vim: set ts=4 sw=4 et : */
