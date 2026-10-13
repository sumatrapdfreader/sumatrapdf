/* mailmark.c - Royal Mail 4-state and 2D Mailmark barcodes */
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

/*
 * Developed in accordance with "Royal Mail Mailmark barcode C encoding and deconding instructions"
 * (https://www.royalmail.com/sites/default/files/
 *  Mailmark-4-state-barcode-C-encoding-and-decoding-instructions-Sept-2015.pdf)
 * and "Royal Mail Mailmark barcode L encoding and decoding"
 * (https://www.royalmail.com/sites/default/files/
 *  Mailmark-4-state-barcode-L-encoding-and-decoding-instructions-Sept-2015.pdf)
 *
 */

#include <stdio.h>
#include "common.h"
#include "large.h"
#include "reedsol.h"

#define RUBIDIUM_F (IS_NUM_F | IS_UPR_F | IS_SPC_F) /* RUBIDIUM "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ " */

/* Allowed character values from Table 3 */
#define SET_F "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
#define SET_L "ABDEFGHJLNPQRSTUWXYZ"
#define SET_N "0123456789"
#define SET_S " "

static const char mailmark_postcode_format[6][9] = {
    {'F','N','F','N','L','L','N','L','S'}, {'F','F','N','N','L','L','N','L','S'},
    {'F','F','N','N','N','L','L','N','L'}, {'F','F','N','F','N','L','L','N','L'},
    {'F','N','N','L','L','N','L','S','S'}, {'F','N','N','N','L','L','N','L','S'}
};

/* Data/Check Symbols from Table 5 */
static const unsigned char mailmark_data_symbol_odd[32] = {
    0x01, 0x02, 0x04, 0x07, 0x08, 0x0B, 0x0D, 0x0E, 0x10, 0x13, 0x15, 0x16,
    0x19, 0x1A, 0x1C, 0x1F, 0x20, 0x23, 0x25, 0x26, 0x29, 0x2A, 0x2C, 0x2F,
    0x31, 0x32, 0x34, 0x37, 0x38, 0x3B, 0x3D, 0x3E
};

static const unsigned char mailmark_data_symbol_even[30] = {
    0x03, 0x05, 0x06, 0x09, 0x0A, 0x0C, 0x0F, 0x11, 0x12, 0x14, 0x17, 0x18,
    0x1B, 0x1D, 0x1E, 0x21, 0x22, 0x24, 0x27, 0x28, 0x2B, 0x2D, 0x2E, 0x30,
    0x33, 0x35, 0x36, 0x39, 0x3A, 0x3C
};

static const unsigned char mailmark_extender_group_c[22] = {
    3, 5, 7, 11, 13, 14, 16, 17, 19, 0, 1, 2, 4, 6, 8, 9, 10, 12, 15, 18, 20, 21
};

static const unsigned char mailmark_extender_group_l[26] = {
    2, 5, 7, 8, 13, 14, 15, 16, 21, 22, 23, 0, 1, 3, 4, 6, 9, 10, 11, 12, 17, 18, 19, 20, 24, 25
};

static int mailmark_verify_character(char input, char type) {
    int val = 0;

    switch (type) {
        case 'F':
            val = posn(SET_F, input);
            break;
        case 'L':
            val = posn(SET_L, input);
            break;
        case 'N':
            val = posn(SET_N, input);
            break;
        case 'S':
            val = posn(SET_S, input);
            break;
    }

    if (val == -1) {
        return 0;
    } else {
        return 1;
    }
}

static int mailmark_verify_postcode(const char postcode[10], int *p_postcode_type) {
    int postcode_type;

    /* Detect postcode type */
    /* postcode_type is used to select which format of postcode
     *
     * 1 = FNFNLLNLS
     * 2 = FFNNLLNLS
     * 3 = FFNNNLLNL
     * 4 = FFNFNLLNL
     * 5 = FNNLLNLSS
     * 6 = FNNNLLNLS
     * 7 = International designation
     */

    if (strcmp(postcode, "XY11     ") == 0) {
        postcode_type = 7;
    } else {
        if (postcode[7] == ' ') {
            postcode_type = 5;
        } else {
            if (postcode[8] == ' ') {
                /* Types 1, 2 and 6 */
                if (z_isdigit(postcode[1])) {
                    if (z_isdigit(postcode[2])) {
                        postcode_type = 6;
                    } else {
                        postcode_type = 1;
                    }
                } else {
                    postcode_type = 2;
                }
            } else {
                /* Types 3 and 4 */
                if (z_isdigit(postcode[3])) {
                    postcode_type = 3;
                } else {
                    postcode_type = 4;
                }
            }
        }
    }

    if (p_postcode_type) {
        *p_postcode_type = postcode_type;
    }

    /* Verify postcode type */
    if (postcode_type != 7) {
        int i;
        const char *const pattern = mailmark_postcode_format[postcode_type - 1];
        for (i = 0; i < 9; i++) {
            if (!(mailmark_verify_character(postcode[i], pattern[i]))) {
                return 1;
            }
        }
    }

    return 0;
}

INTERNAL int daft_set_height(struct zint_symbol *symbol, const float min_height, const float max_height);

/* Royal Mail 4-state Mailmark */
INTERNAL int mailmark_4s(struct zint_symbol *symbol, unsigned char source[], int length) {

    char local_source[28];
    int format;
    int version_id;
    int mail_class;
    int supply_chain_id;
    unsigned int item_id;
    char postcode[10];
    int postcode_type;
    large_uint destination_postcode;
    large_uint b;
    large_uint cdv;
    unsigned char data[26];
    int data_top, data_step;
    unsigned char check[7];
    unsigned int extender[27];
    char bar[80];
    char *d = bar;
    int check_count;
    int i, j, len;
    rs_t rs;
    int error_number = 0;

    if (length > 26) {
        return errtxtf(ZINT_ERROR_TOO_LONG, symbol, 580, "Input length %d too long (maximum 26)", length);
    }

    ustrcpy(local_source, source);

    if (length < 22) {
        if (length < 14) {
            return errtxtf(ZINT_ERROR_TOO_LONG, symbol, 588, "Input length %d too short (minimum 14)", length);
        }
        for (i = length; i <= 22; i++) {
            strcat(local_source, " ");
        }
        length = 22;
    } else if ((length > 22) && (length < 26)) {
        for (i = length; i <= 26; i++) {
            strcat(local_source, " ");
        }
        length = 26;
    }

    to_upper((unsigned char *) local_source, length);

    if (symbol->debug & ZINT_DEBUG_PRINT) {
        printf("Producing 4-state Mailmark (%d): %s<end>\n", length, local_source);
    }

    if ((i = not_sane(RUBIDIUM_F, (unsigned char *) local_source, length))) {
        return errtxtf(ZINT_ERROR_INVALID_DATA, symbol, 581,
                        "Invalid character at position %d in input (alphanumerics and space only)", i);
    }

    /* Format is in the range 0-4 */
    format = ctoi(local_source[0]);
    if ((format < 0) || (format > 4)) {
        return errtxt(ZINT_ERROR_INVALID_DATA, symbol, 582, "Format (1st character) out of range (0 to 4)");
    }

    /* Version ID is in the range 1-4 */
    version_id = ctoi(local_source[1]) - 1;
    if ((version_id < 0) || (version_id > 3)) {
        return errtxt(ZINT_ERROR_INVALID_DATA, symbol, 583, "Version ID (2nd character) out of range (1 to 4)");
    }

    /* Class is in the range 0-9,A-E */
    mail_class = ctoi(local_source[2]);
    if ((mail_class < 0) || (mail_class > 14)) {
        return errtxt(ZINT_ERROR_INVALID_DATA, symbol, 584, "Class (3rd character) out of range (0 to 9 and A to E)");
    }

    /* Supply Chain ID is 2 digits for barcode C and 6 digits for barcode L */
    supply_chain_id = 0;
    for (i = 3; i < (length - 17); i++) {
        if (z_isdigit(local_source[i])) {
            supply_chain_id *= 10;
            supply_chain_id += ctoi(local_source[i]);
        } else {
            return errtxtf(ZINT_ERROR_INVALID_DATA, symbol, 585,
                            "Invalid Supply Chain ID at character %d (digits only)", i);
        }
    }

    /* Item ID is 8 digits */
    item_id = 0;
    for (i = length - 17; i < (length - 9); i++) {
        if (z_isdigit(local_source[i])) {
            item_id *= 10;
            item_id += ctoi(local_source[i]);
        } else {
            return errtxtf(ZINT_ERROR_INVALID_DATA, symbol, 586, "Invalid Item ID at character %d (digits only)", i);
        }
    }

    /* Separate Destination Post Code plus DPS field */
    for (i = 0; i < 9; i++) {
        postcode[i] = local_source[(length - 9) + i];
    }
    postcode[9] = '\0';
    if (mailmark_verify_postcode(postcode, &postcode_type) != 0) {
        return errtxtf(ZINT_ERROR_INVALID_DATA, symbol, 587, "Invalid postcode \"%s\"", postcode);
    }

    /* Convert postcode to internal user field */

    large_load_u64(&destination_postcode, 0);

    if (postcode_type != 7) {
        const char *const pattern = mailmark_postcode_format[postcode_type - 1];

        large_load_u64(&b, 0);

        for (i = 0; i < 9; i++) {
            switch (pattern[i]) {
                case 'F':
                    large_mul_u64(&b, 26);
                    large_add_u64(&b, posn(SET_F, postcode[i]));
                    break;
                case 'L':
                    large_mul_u64(&b, 20);
                    large_add_u64(&b, posn(SET_L, postcode[i]));
                    break;
                case 'N':
                    large_mul_u64(&b, 10);
                    large_add_u64(&b, posn(SET_N, postcode[i]));
                    break;
                /* case 'S' ignored as value is 0 */
            }
        }

        large_load(&destination_postcode, &b);

        /* destination_postcode = a + b */
        large_load_u64(&b, 1);
        if (postcode_type == 1) {
            large_add(&destination_postcode, &b);
        }
        large_add_u64(&b, 5408000000);
        if (postcode_type == 2) {
            large_add(&destination_postcode, &b);
        }
        large_add_u64(&b, 5408000000);
        if (postcode_type == 3) {
            large_add(&destination_postcode, &b);
        }
        large_add_u64(&b, 54080000000);
        if (postcode_type == 4) {
            large_add(&destination_postcode, &b);
        }
        large_add_u64(&b, 140608000000);
        if (postcode_type == 5) {
            large_add(&destination_postcode, &b);
        }
        large_add_u64(&b, 208000000);
        if (postcode_type == 6) {
            large_add(&destination_postcode, &b);
        }
    }

    /* Conversion from Internal User Fields to Consolidated Data Value */
    /* Set CDV to 0 */
    large_load_u64(&cdv, 0);

    /* Add Destination Post Code plus DPS */
    large_add(&cdv, &destination_postcode);

    /* Multiply by 100,000,000 */
    large_mul_u64(&cdv, 100000000);

    /* Add Item ID */
    large_add_u64(&cdv, item_id);

    if (length == 22) {
        /* Barcode C - Multiply by 100 */
        large_mul_u64(&cdv, 100);
    } else {
        /* Barcode L - Multiply by 1,000,000 */
        large_mul_u64(&cdv, 1000000);
    }

    /* Add Supply Chain ID */
    large_add_u64(&cdv, supply_chain_id);

    /* Multiply by 15 */
    large_mul_u64(&cdv, 15);

    /* Add Class */
    large_add_u64(&cdv, mail_class);

    /* Multiply by 5 */
    large_mul_u64(&cdv, 5);

    /* Add Format */
    large_add_u64(&cdv, format);

    /* Multiply by 4 */
    large_mul_u64(&cdv, 4);

    /* Add Version ID */
    large_add_u64(&cdv, version_id);

    if (symbol->debug & ZINT_DEBUG_PRINT) {
        printf("DPC type %d\n", postcode_type);
        fputs("CDV: ", stdout);
        large_print(&cdv);
    }

    if (length == 22) {
        data_top = 15;
        data_step = 8;
        check_count = 6;
    } else {
        data_top = 18;
        data_step = 10;
        check_count = 7;
    }

    /* Conversion from Consolidated Data Value to Data Numbers */

    for (j = data_top; j >= (data_step + 1); j--) {
        data[j] = (unsigned char) large_div_u64(&cdv, 32);
    }

    for (j = data_step; j >= 0; j--) {
        data[j] = (unsigned char) large_div_u64(&cdv, 30);
    }

    /* Generation of Reed-Solomon Check Numbers */
    rs_init_gf(&rs, 0x25);
    rs_init_code(&rs, check_count, 1);
    data_top++;
    rs_encode(&rs, data_top, data, check);

    /* Append check digits to data */
    memcpy(data + data_top, check, check_count);

    if (symbol->debug & ZINT_DEBUG_PRINT) {
        fputs("Codewords:", stdout);
        for (i = 0; i < data_top + check_count; i++) {
            printf("  %d", (int) data[i]);
        }
        fputc('\n', stdout);
    }

    /* Conversion from Data Numbers and Check Numbers to Data Symbols and Check Symbols */
    for (i = 0; i <= data_step; i++) {
        data[i] = mailmark_data_symbol_even[data[i]];
    }
    for (i = data_step + 1; i < (data_top + check_count); i++) {
        data[i] = mailmark_data_symbol_odd[data[i]];
    }

    /* Conversion from Data Symbols and Check Symbols to Extender Groups */
    for (i = 0; i < length; i++) {
        if (length == 22) {
            extender[mailmark_extender_group_c[i]] = data[i];
        } else {
            extender[mailmark_extender_group_l[i]] = data[i];
        }
    }

    /* Conversion from Extender Groups to Bar Identifiers */

    for (i = 0; i < length; i++) {
        for (j = 0; j < 3; j++) {
            switch (extender[i] & 0x24) {
                case 0x24:
                    *d++ = 'F';
                    break;
                case 0x20:
                    if (i % 2) {
                        *d++ = 'D';
                    } else {
                        *d++ = 'A';
                    }
                    break;
                case 0x04:
                    if (i % 2) {
                        *d++ = 'A';
                    } else {
                        *d++ = 'D';
                    }
                    break;
                default:
                    *d++ = 'T';
                    break;
            }
            extender[i] = extender[i] << 1;
        }
    }

    if (symbol->debug & ZINT_DEBUG_PRINT) {
        printf("Bar pattern: %.*s\n", (int) (d - bar), bar);
    }

    /* Translate 4-state data pattern to symbol */
    j = 0;
    for (i = 0, len = d - bar; i < len; i++) {
        if ((bar[i] == 'F') || (bar[i] == 'A')) {
            set_module(symbol, 0, j);
        }
        set_module(symbol, 1, j);
        if ((bar[i] == 'F') || (bar[i] == 'D')) {
            set_module(symbol, 2, j);
        }
        j += 2;
    }

    if (symbol->output_options & COMPLIANT_HEIGHT) {
        /* Royal Mail Mailmark Barcode Definition Document (15 Sept 2015) Section 3.5.1
           (https://www.royalmail.com/sites/default/files/
            Royal-Mail-Mailmark-barcode-definition-document-September-2015.pdf)
           Using bar pitch as X (25.4mm / 42.3) ~ 0.6mm based on 21.2 bars + 21.1 spaces per 25.4mm (bar width
           0.38mm - 0.63mm)
           Using recommended 1.9mm and 1.3mm heights for Ascender/Descenders and Trackers resp. as defaults
           Min height 4.22mm * 39 (max pitch) / 25.4mm ~ 6.47, max height 5.84mm * 47 (min pitch) / 25.4mm ~ 10.8
         */
        const float min_height = 6.47952747f; /* (4.22 * 39) / 25.4 */
        const float max_height = 10.8062992f; /* (5.84 * 47) / 25.4 */
        symbol->row_height[0] = 3.16417313f; /* (1.9 * 42.3) / 25.4 */
        symbol->row_height[1] = 2.16496062f; /* (1.3 * 42.3) / 25.4 */
        /* Note using max X for minimum and min X for maximum */
        error_number = daft_set_height(symbol, min_height, max_height);
    } else {
        symbol->row_height[0] = 4.0f;
        symbol->row_height[1] = 2.0f;
        (void) daft_set_height(symbol, 0.0f, 0.0f);
    }
    symbol->rows = 3;
    symbol->width = j - 1;

    return error_number;
}

INTERNAL int datamatrix(struct zint_symbol *symbol, struct zint_seg segs[], const int seg_count);

/* Royal Mail 2D Mailmark (CMDM) (Data Matrix) */
/* https://www.royalmailtechnical.com/rmt_docs/User_Guides_2021/Mailmark_Barcode_definition_document_20210215.pdf */
INTERNAL int mailmark_2d(struct zint_symbol *symbol, unsigned char source[], int length) {

    unsigned char local_source[90 + 1];
    char postcode[10];
    int i;
    struct zint_seg segs[1];

    if (length > 90) {
        return errtxtf(ZINT_ERROR_TOO_LONG, symbol, 589, "Input length %d too long (maximum 90)", length);
    }

    if (length < 28) { /* After adding prefix (4), blank Return to Sender Post Code (7), Reserved (6): 28 + 17 = 45 */
        return errtxtf(ZINT_ERROR_TOO_LONG, symbol, 860, "Input length %d too short (minimum 28)", length);
    }

    /* Add prefix if missing */
    memcpy(local_source, source, 4);
    to_upper(local_source, 3);
    if (memcmp(local_source, "JGB ", 4) != 0) {
        if (length > 86) {
            return errtxtf(ZINT_ERROR_TOO_LONG, symbol, 861, "Input length %d too long (maximum 86)", length);
        }
        ustrcpy(local_source, "JGB ");
        ustrcpy(local_source + 4, source);
        length += 4;
    } else {
        ustrcpy(local_source, source);
    }

    if (length < 32) {
        return errtxtf(ZINT_ERROR_TOO_LONG, symbol, 862, "Input length %d too short (minimum 32)", length);
    }
    if (length < 39) { /* Space-pad Return to Sender Post Code */
        memset(local_source + length, ' ', 39 - length);
        local_source[39] = '\0';
        length = 39;
    }
    to_upper(local_source, 39);

    if (length < 45) { /* Space-pad Reserved */
        memset(local_source + length, ' ', 45 - length);
        local_source[45] = '\0';
        length = 45;
    }

    /* 8: 24 x 24, 10: 32 x 32, 30: 16 x 48 */
    if (symbol->option_2) {
        if (symbol->option_2 != 8 && symbol->option_2 != 10 && symbol->option_2 != 30) {
            return errtxtf(ZINT_ERROR_INVALID_OPTION, symbol, 863, "Invalid Version '%d' (8, 10 or 30 only)",
                            symbol->option_2);
        }
        if (symbol->option_2 == 8) {
            if (length > 51) {
                return errtxtf(ZINT_ERROR_TOO_LONG, symbol, 864,
                                "Input length %d too long for Version 8 (maximum 51)", length);
            }
        } else if (symbol->option_2 == 30) {
            if (length > 70) {
                return errtxtf(ZINT_ERROR_TOO_LONG, symbol, 865,
                                "Input length %d too long for Version 30 (maximum 70)", length);
            }
        }
    } else {
        if (length <= 51) {
            symbol->option_2 = 8;
        } else if (length <= 70 && (symbol->option_3 & 0x7F) != DM_SQUARE) {
            symbol->option_2 = 30;
        } else {
            symbol->option_2 = 10;
        }
    }

    if (symbol->debug & ZINT_DEBUG_PRINT) {
        printf("Producing 2D Mailmark %d (%d): %s<end>\n", symbol->option_2, length, local_source);
    }

    if ((i = not_sane(RUBIDIUM_F, local_source, 45))) {
        return errtxtf(ZINT_ERROR_INVALID_DATA, symbol, 866,
                        "Invalid character at position %d in input (alphanumerics and space only in first 45)", i);
    }

    /* Information Type ID */
    /* Not checking that matches values listed in Mailmark Definition Document as contradicted by Mailmark Mailing
       Requirements Section 5.7 which says 'P' for poll card is valid, which isn't listed */
    if (local_source[4] == ' ') {
        return errtxt(ZINT_ERROR_INVALID_DATA, symbol, 867, "Invalid Information Type ID (cannot be space)");
    }
    /* Version ID */
    if (local_source[5] != '1') {
        return errtxt(ZINT_ERROR_INVALID_DATA, symbol, 868, "Invalid Version ID (\"1\" only)");
    }
    /* Class */
    if (local_source[6] == ' ') {
        return errtxt(ZINT_ERROR_INVALID_DATA, symbol, 869, "Invalid Class (cannot be space)");
    }
    /* Supply Chain ID */
    if (cnt_digits(local_source, length, 7, 7) != 7) {
        return errtxt(ZINT_ERROR_INVALID_DATA, symbol, 870, "Invalid Supply Chain ID (7 digits only)");
    }
    /* Item ID */
    if (cnt_digits(local_source, length, 14, 8) != 8) {
        return errtxt(ZINT_ERROR_INVALID_DATA, symbol, 871, "Invalid Item ID (8 digits only)");
    }

    /* Destination Post Code plus DPS field */
    for (i = 0; i < 9; i++) {
        postcode[i] = local_source[22 + i];
    }
    postcode[9] = '\0';
    if (mailmark_verify_postcode(postcode, NULL) != 0) {
        return errtxt(ZINT_ERROR_INVALID_DATA, symbol, 872, "Invalid Destination Post Code plus DPS");
    }

    /* Service Type */
    if (local_source[31] < '0' || local_source[31] > '6') {
        return errtxt(ZINT_ERROR_INVALID_DATA, symbol, 873, "Invalid Service Type (\"0\" to \"6\" only)");
    }

    /* Return to Sender Post Code */
    if (memcmp(local_source + 32, "       ", 7) != 0) { /* If not blank (allowed) */
        for (i = 0; i < 7; i++) {
            postcode[i] = local_source[32 + i];
        }
        /* Add dummy DPS */
        for (i = 6; postcode[i] == ' '; i--); /* Skip any terminal spaces */
        i++;
        postcode[i++] = '1';
        postcode[i++] = 'A';
        while (i != 9) {
            postcode[i++] = ' ';
        }
        postcode[9] = '\0';
        if (mailmark_verify_postcode(postcode, NULL) != 0) {
            return errtxt(ZINT_ERROR_INVALID_DATA, symbol, 874, "Invalid Return to Sender Post Code");
        }
    }

    /* Reserved */
    if (memcmp(local_source + 39, "      ", 6) != 0) {
        return errtxt(ZINT_ERROR_INVALID_DATA, symbol, 875, "Invalid Reserved field (must be spaces only)");
    }

    segs[0].eci = 0;
    segs[0].source = local_source;
    segs[0].length = length;

    return datamatrix(symbol, segs, 1);
}

/* vim: set ts=4 sw=4 et : */
