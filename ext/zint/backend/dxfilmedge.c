/* dxfilmedge.c - Handles DX Film Edge symbology */
/*
    libzint - the open source barcode library
    Copyright (C) 2024 Antoine Merino <antoine.merino.dev@gmail.com>

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

/* DX Film Edge Barcode is used on 35mm and APS films:
 * https://en.wikipedia.org/wiki/DX_encoding
 *
 * A little information about decoding this symbology can be found at
 * https://www.merinorus.com/blog/identifying-manufacturer-35-mm-films/
 *
 * Partial specification and history can be found on this Kodak patent:
 * https://patents.google.com/patent/US4965628A/en
 */

#include <assert.h>
#include <stdio.h>
#include "common.h"

#define DX_DEBUG_STR_LEN    20
/* Max length of the DX info part. Include the \0. Eg: "018500\0", "150-10\0" */
#define DX_MAX_DX_INFO_LENGTH       6
#define DX_MAX_DX_INFO_MAX_STR      "6" /* String version of above */
/* Max length of the frame info part. Eg: "00A\0", "23A\0" */
#define DX_MAX_FRAME_INFO_LENGTH    3
#define DX_MAX_FRAME_INFO_MAX_STR   "3" /* String version of above */

static void dx_int_to_binary(const int value, const int width, char *output) {
    int i;
    for (i = 0; i < width; i++) {
        output[width - 1 - i] = (value & (1 << i)) ? '1' : '0';
    }
    output[width] = '\0';
}

static int dx_parse_code(struct zint_symbol *symbol, const unsigned char *source, const int length,
            char *binary_output, int *output_length, int *has_frame_info) {
    int i;
    int parity_bit = 0;
    int dx_extract = -1, dx_code_1 = -1, dx_code_2 = -1, frame_number = -1;
    char binary_dx_code_1[8], binary_dx_code_2[5], binary_frame_number[7];
    char half_frame_flag = '\0';
    char dx_info[DX_MAX_DX_INFO_LENGTH + 1] = "\0";
    char frame_info[DX_MAX_FRAME_INFO_LENGTH + 1] = "\0";
    int dx_length;
    const char *frame_start;
    const int debug_print = symbol->debug & ZINT_DEBUG_PRINT;

    *has_frame_info = 0;

    /* All codes should start with a digit*/
    if (!z_isdigit(source[0])) {
        return errtxtf(ZINT_ERROR_INVALID_DATA, symbol, 970,
                        "Invalid first character \"%c\", DX code should start with a number", source[0]);
    }

    /* Check if there is the '/' separator, which indicates the frame number is present. */
    dx_length = posn((const char *) source, '/');
    if (dx_length != -1) {
        /* Split the DX information from the frame number */
        int frame_info_len;
        if (dx_length > DX_MAX_DX_INFO_LENGTH) {
            return errtxtf(ZINT_ERROR_INVALID_DATA, symbol, 971,
                            "DX information length %d too long (maximum " DX_MAX_DX_INFO_MAX_STR ")", dx_length);
        }
        ustrncat(dx_info, source, dx_length);
        dx_info[dx_length] = '\0';
        frame_start = (const char *) source + dx_length + 1;
        frame_info_len = (int) strlen(frame_start);
        if (frame_info_len > DX_MAX_FRAME_INFO_LENGTH) {
            return errtxtf(ZINT_ERROR_INVALID_DATA, symbol, 972,
                    "Frame number part length %d too long (maximum " DX_MAX_FRAME_INFO_MAX_STR ")", frame_info_len);
        }
        ustrcpy(frame_info, frame_start);
        *has_frame_info = 1;
        to_upper((unsigned char *) frame_info, frame_info_len);
        if (not_sane(IS_UPR_F | IS_NUM_F | IS_MNS_F, (const unsigned char *) frame_info, frame_info_len)) {
            return errtxtf(ZINT_ERROR_INVALID_DATA, symbol, 973,
                            "Frame number \"%s\" is invalid (expected digits, optionally followed by a single \"A\")",
                            frame_info);
        }
    } else {
        /* No "/" found, store the entire input in dx_info */
        dx_length = length;
        if (dx_length > DX_MAX_DX_INFO_LENGTH) {
            return errtxtf(ZINT_ERROR_INVALID_DATA, symbol, 974,
                            "DX information length %d too long (maximum " DX_MAX_DX_INFO_MAX_STR ")", dx_length);
        }
        ustrcpy(dx_info, source);
    }

    if ((i = not_sane(IS_NUM_F | IS_MNS_F, (const unsigned char *) dx_info, dx_length))) {
        return errtxtf(ZINT_ERROR_INVALID_DATA, symbol, 975,
                        "Invalid character at position %d in DX info (digits and \"-\" character only)", i);
    }

    if (debug_print) printf("\nDX info part: \"%s\", Frame info part: \"%s\"\n", dx_info, frame_info);
    /* Parse the DX information */
    if (strchr(dx_info, '-')) {
        /* DX code parts 1 and 2 are given directly, separated by a '-'. Eg: "79-7" */
        if (debug_print) printf("DX code 1 and 2 are separated by a dash \"-\"\n");
        if (chr_cnt((const unsigned char *) dx_info, dx_length, '-') > 1) {
            return errtxt(ZINT_ERROR_INVALID_DATA, symbol, 976,
                            "The \"-\" is used to separate DX parts 1 and 2, and should be used no more than once");
        }
        if (sscanf(dx_info, "%d-%d", &dx_code_1, &dx_code_2) < 2) {
            return errtxt(ZINT_ERROR_INVALID_DATA, symbol, 977,
                            "Wrong format for DX parts 1 and 2 (expected format: XXX-XX, digits)");
        }
        if (dx_code_1 <= 0 || dx_code_1 > 127) {
            return errtxtf(ZINT_ERROR_INVALID_DATA, symbol, 978, "DX part 1 \"%d\" out of range (1 to 127)",
                            dx_code_1);
        }
        if (dx_code_2 < 0 || dx_code_2 > 15) {
            return errtxtf(ZINT_ERROR_INVALID_DATA, symbol, 979, "DX part 2 \"%d\" out of range (0 to 15)",
                            dx_code_2);
        }
    } else {
        /* DX format is either 4 digits (DX Extract, eg: 1271) or 6 digits (DX Full, eg: 012710) */
        if (debug_print) printf("No \"-\" separator, computing from DX Extract (4 digits) or DX Full (6 digits)\n");
        if (dx_length == 5 || dx_length > 6) {
            return errtxtf(ZINT_ERROR_INVALID_DATA, symbol, 980,
                    "DX number \"%s\" is incorrect; expected 4 digits (DX extract) or 6 digits (DX full)", dx_info);
        }
        if (dx_length == 6) {
            if (debug_print) {
                printf("DX full format detected: %s. Removing the first and the last characters.\n", dx_info);
            }
            /* Convert DX Full to DX Extract (remove first and last character) */
            for (i = 0; i <= 3; ++i) {
                dx_info[i] = dx_info[i + 1];
            }
            dx_info[4] = '\0';
            dx_length = 4;
        }
        /* Compute the DX parts 1 and 2 from the DX extract */
        dx_extract = to_int((const unsigned char *) dx_info, dx_length);
        assert(dx_extract != -1);
        if (dx_extract < 16 || dx_extract > 2047) {
            return errtxtf(ZINT_ERROR_INVALID_DATA, symbol, 981, "DX extract \"%d\" out of range (16 to 2047)",
                            dx_extract);
        }
        if (debug_print) printf("Computed DX extract: %04d\n", dx_extract);
        dx_code_1 = dx_extract / 16;
        dx_code_2 = dx_extract % 16;
    }

    /* Convert components to binary strings */
    dx_int_to_binary(dx_code_1, 7, binary_dx_code_1);
    dx_int_to_binary(dx_code_2, 4, binary_dx_code_2);

    if (debug_print) {
        printf("%-*s%d\t-> %s\n", DX_DEBUG_STR_LEN, "DX code 1:", dx_code_1, binary_dx_code_1);
        printf("%-*s%d\t-> %s\n", DX_DEBUG_STR_LEN, "DX code 2:", dx_code_2, binary_dx_code_2);
    }

    if (*has_frame_info) {
        int ret_sscanf, n;
        if (strlen(frame_info) < 1) {
            return errtxtf(ZINT_ERROR_INVALID_DATA, symbol, 982,
                            "Frame number indicator \"/\" at position %d, but frame number is empty",
                            dx_length + 1);
        }
        /* Some frame numbers are special values, convert them their equivalent number */
        if (strcmp(frame_info, "S") == 0 || strcmp(frame_info, "X") == 0) {
            strcpy(frame_info, "62");
        } else if (strcmp(frame_info, "SA") == 0 || strcmp(frame_info, "XA") == 0) {
            strcpy(frame_info, "62A");
        } else if (strcmp(frame_info, "K") == 0 || strcmp(frame_info, "00") == 0) {
            strcpy(frame_info, "63");
        } else if (strcmp(frame_info, "KA") == 0 || strcmp(frame_info, "00A") == 0) {
            strcpy(frame_info, "63A");
        } else if (strcmp(frame_info, "F") == 0) {
            strcpy(frame_info, "0");
        } else if (strcmp(frame_info, "FA") == 0) {
            strcpy(frame_info, "0A");
        }

        ret_sscanf = sscanf(frame_info, "%d%c%n", &frame_number, &half_frame_flag, &n);
        if (ret_sscanf < 1 || (ret_sscanf == 2 && frame_info[n] != '\0')) {
            return errtxtf(ZINT_ERROR_INVALID_DATA, symbol, 983,
                            "Frame number \"%s\" is invalid (expected digits, optionally followed by a single \"A\")",
                            frame_info);
        }
        if (frame_number < 0 || frame_number > 63) {
            return errtxtf(ZINT_ERROR_INVALID_DATA, symbol, 984, "Frame number \"%d\" out of range (0 to 63)",
                            frame_number);
        }
        dx_int_to_binary(frame_number, 6, binary_frame_number);
        if (debug_print) {
            printf("%-*s%d\t-> %s\n", DX_DEBUG_STR_LEN, "Frame number:", frame_number, binary_frame_number);
        }
    }

    /* Build the binary output */
    strcpy(binary_output, "101010"); /* Start pattern */
    strcat(binary_output, binary_dx_code_1);
    strcat(binary_output, "0"); /* Separator between DX part 1 and DX part 2 */
    strcat(binary_output, binary_dx_code_2);
    if (*has_frame_info) {
        strcat(binary_output, binary_frame_number);
        to_upper((unsigned char *) &half_frame_flag, 1);
        if (half_frame_flag == 'A') {
            if (debug_print) printf("%-*s'%c'\t-> 1\n", DX_DEBUG_STR_LEN, "Half frame flag:", half_frame_flag);
            strcat(binary_output, "1"); /* Half-frame is set */
        } else {
            if (half_frame_flag) {
                return errtxtf(ZINT_ERROR_INVALID_DATA, symbol, 985,
                            "Frame number \"%s\" is invalid (expected digits, optionally followed by a single \"A\")",
                            frame_info);
            }
            if (debug_print) printf("%-*s'%c'\t-> 0\n", DX_DEBUG_STR_LEN, "Half frame flag:", half_frame_flag);
            strcat(binary_output, "0"); /* Half-frame is NOT set */
        }
        strcat(binary_output, "0"); /* Separator between half frame flag and parity bit*/
    }

    /* Parity bit */
    for (i = 6; binary_output[i] != '\0'; i++) {
        if (binary_output[i] == '1') {
            parity_bit++;
        }
    }
    parity_bit %= 2;
    if (debug_print) {
        printf("%-*s%s\t-> %d\n", DX_DEBUG_STR_LEN, "Parity bit:", parity_bit ? "yes" : "no", parity_bit);
    }
    if (parity_bit) {
        strcat(binary_output, "1");
    } else {
        strcat(binary_output, "0");
    }

    strcat(binary_output, "0101"); /* Stop pattern */

    *output_length = (int) strlen(binary_output);
    return 0;
}

INTERNAL int dxfilmedge(struct zint_symbol *symbol, unsigned char source[], int length) {
    int i;
    int writer = 0;
    int error_number = 0;

    char char_data[32];
    int data_length;
    int has_frame_info;

    const char long_clock_pattern[] = "1111101010101010101010101010111";
    const char short_clock_pattern[] = "11111010101010101010111";
    const char *clock_pattern;
    int clock_length;
    int parse_result = -1;
    const int debug_print = symbol->debug & ZINT_DEBUG_PRINT;

    if (length > 10) {
        return errtxtf(ZINT_ERROR_TOO_LONG, symbol, 986, "Input length %d too long (maximum 10)", length);
    }

    parse_result = dx_parse_code(symbol, source, length, char_data, &data_length, &has_frame_info);
    if (parse_result != 0) {
        if (debug_print) printf("Error %s\n\n", symbol->errtxt);
        return parse_result;
    }

    /* Clock signal is longer if the frame number is provided */
    if (has_frame_info) {
        clock_pattern = long_clock_pattern;
        clock_length = sizeof(long_clock_pattern) -1;
    } else {
        clock_pattern = short_clock_pattern;
        clock_length = sizeof(short_clock_pattern) -1;
    }

    /* First row: clock pattern */
    for (i = 0; i < clock_length; i++) {
        if (clock_pattern[i] == '1') {
            set_module(symbol, 0, writer);
        } else if (clock_pattern[i] == '0') {
            unset_module(symbol, 0, writer);
        }
        writer++;
    }

    /* Reset writer X position for the second row */
    writer = 0;

    /* Second row: data signal */
    for (i = 0; i < clock_length; i++) {
        if (char_data[i] == '1') {
            set_module(symbol, 1, writer);
        } else if (char_data[i] == '0') {
            unset_module(symbol, 1, writer);
        }
        writer++;
    }
    symbol->rows = 2;
    symbol->width = clock_length;

    if (symbol->output_options & COMPLIANT_HEIGHT) {
        /* Measured ratio on 35mm films. Depending on the brands, one symbol height is about 3 * the X-dim.*/
        const float default_height = 6.0f;

        /* AFAIK There is no standard on minimum and maximum height, so we stay close to the measurements */
        const float min_row_height = 2.2f;
        const float max_height = 7.5f;
        error_number = set_height(symbol, min_row_height, default_height, max_height, 0 /*no_errtxt*/);
    } else {
        /* Using compliant height as default as no backwards compatibility to consider */
        const float default_height = 6.0f;
        (void) set_height(symbol, 0.0f, default_height, 0.0f, 1 /*no_errtxt*/);
    }

    return error_number;
}

/* vim: set ts=4 sw=4 et : */
