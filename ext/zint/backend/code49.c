/* code49.c - Handles Code 49 */
/*
    libzint - the open source barcode library
    Copyright (C) 2009-2024 Robin Stuart <rstuart114@gmail.com>

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
#include "code49.h"

static const char C49_INSET[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ-. $/+%!&*";

/* "!" represents Shift 1 and "&" represents Shift 2, "*" represents FNC1 */

INTERNAL int code49(struct zint_symbol *symbol, unsigned char source[], int length) {
    int i, j, rows, M, x_count, y_count, z_count, posn_val, local_value;
    char intermediate[170] = "";
    char *d = intermediate;
    int codewords[170], codeword_count;
    int c_grid[8][8]; /* Refers to table 3 */
    int w_grid[8][4]; /* Refets to table 2 */
    int pad_count = 0;
    char pattern[80];
    int bp = 0; /* Initialize to suppress gcc -Wmaybe-uninitialized warning */
    int gs1;
    int h;
    int error_number = 0;

    if (length > 81) {
        return errtxtf(ZINT_ERROR_TOO_LONG, symbol, 430, "Input length %d too long (maximum 81)", length);
    }
    if ((symbol->input_mode & 0x07) == GS1_MODE) {
        gs1 = 1;
        *d++ = '*'; /* FNC1 */
    } else {
        gs1 = 0;
    }

    for (i = 0; i < length; i++) {
        if (source[i] > 127) {
            return errtxtf(ZINT_ERROR_INVALID_DATA, symbol, 431,
                            "Invalid character at position %d in input, extended ASCII not allowed", i + 1);
        }
        if (gs1 && source[i] == '\x1D') {
            *d++ = '*'; /* FNC1 */
        } else {
            const char *const entry = c49_table7[source[i]];
            memcpy(d, entry, 2);
            d += entry[1] ? 2 : 1;
        }
    }

    codeword_count = 0;
    i = 0;
    h = d - intermediate;
    do {
        if (z_isdigit(intermediate[i])) {
            /* Numeric data */
            for (j = 0; z_isdigit(intermediate[i + j]); j++);
            if (j >= 5) {
                /* Use Numeric Encodation Method */
                int block_count, c;
                int block_remain;
                int block_value;

                codewords[codeword_count] = 48; /* Numeric Shift */
                codeword_count++;

                block_count = j / 5;
                block_remain = j % 5;

                for (c = 0; c < block_count; c++) {
                    if ((c == block_count - 1) && (block_remain == 2)) {
                        /* Rule (d) */
                        block_value = 100000;
                        block_value += ctoi(intermediate[i]) * 1000;
                        block_value += ctoi(intermediate[i + 1]) * 100;
                        block_value += ctoi(intermediate[i + 2]) * 10;
                        block_value += ctoi(intermediate[i + 3]);

                        codewords[codeword_count] = block_value / (48 * 48);
                        block_value = block_value - (48 * 48) * codewords[codeword_count];
                        codeword_count++;
                        codewords[codeword_count] = block_value / 48;
                        block_value = block_value - 48 * codewords[codeword_count];
                        codeword_count++;
                        codewords[codeword_count] = block_value;
                        codeword_count++;
                        i += 4;
                        block_value = ctoi(intermediate[i]) * 100;
                        block_value += ctoi(intermediate[i + 1]) * 10;
                        block_value += ctoi(intermediate[i + 2]);

                        codewords[codeword_count] = block_value / 48;
                        block_value = block_value - 48 * codewords[codeword_count];
                        codeword_count++;
                        codewords[codeword_count] = block_value;
                        codeword_count++;
                        i += 3;
                    } else {
                        block_value = ctoi(intermediate[i]) * 10000;
                        block_value += ctoi(intermediate[i + 1]) * 1000;
                        block_value += ctoi(intermediate[i + 2]) * 100;
                        block_value += ctoi(intermediate[i + 3]) * 10;
                        block_value += ctoi(intermediate[i + 4]);

                        codewords[codeword_count] = block_value / (48 * 48);
                        block_value = block_value - (48 * 48) * codewords[codeword_count];
                        codeword_count++;
                        codewords[codeword_count] = block_value / 48;
                        block_value = block_value - 48 * codewords[codeword_count];
                        codeword_count++;
                        codewords[codeword_count] = block_value;
                        codeword_count++;
                        i += 5;
                    }
                }

                switch (block_remain) {
                    case 1:
                        /* Rule (a) */
                        codewords[codeword_count] = posn(C49_INSET, intermediate[i]);
                        codeword_count++;
                        i++;
                        break;
                    case 3:
                        /* Rule (b) */
                        block_value = ctoi(intermediate[i]) * 100;
                        block_value += ctoi(intermediate[i + 1]) * 10;
                        block_value += ctoi(intermediate[i + 2]);

                        codewords[codeword_count] = block_value / 48;
                        block_value = block_value - 48 * codewords[codeword_count];
                        codeword_count++;
                        codewords[codeword_count] = block_value;
                        codeword_count++;
                        i += 3;
                        break;
                    case 4:
                        /* Rule (c) */
                        block_value = 100000;
                        block_value += ctoi(intermediate[i]) * 1000;
                        block_value += ctoi(intermediate[i + 1]) * 100;
                        block_value += ctoi(intermediate[i + 2]) * 10;
                        block_value += ctoi(intermediate[i + 3]);

                        codewords[codeword_count] = block_value / (48 * 48);
                        block_value = block_value - (48 * 48) * codewords[codeword_count];
                        codeword_count++;
                        codewords[codeword_count] = block_value / 48;
                        block_value = block_value - 48 * codewords[codeword_count];
                        codeword_count++;
                        codewords[codeword_count] = block_value;
                        codeword_count++;
                        i += 4;
                        break;
                }
                if (i < h) {
                    /* There is more to add */
                    codewords[codeword_count] = 48; /* Numeric Shift */
                    codeword_count++;
                }
            } else {
                codewords[codeword_count] = posn(C49_INSET, intermediate[i]);
                codeword_count++;
                i++;
            }
        } else {
            codewords[codeword_count] = posn(C49_INSET, intermediate[i]);
            codeword_count++;
            i++;
        }
    } while (i < h);

    switch (codewords[0]) {
            /* Set starting mode value */
        case 48: M = 2;
            break;
        case 43: M = 4;
            break;
        case 44: M = 5;
            break;
        default: M = 0;
            break;
    }

    if (M != 0) {
        codeword_count--;
        for (i = 0; i < codeword_count; i++) {
            codewords[i] = codewords[i + 1];
        }
    }

    if (codeword_count > 49) {
        return errtxtf(ZINT_ERROR_TOO_LONG, symbol, 432, "Input too long, requires %d codewords (maximum 49)",
                        codeword_count);
    }

    /* Place codewords in code character array (c grid) */
    rows = 0;
    do {
        for (i = 0; i < 7; i++) {
            if (((rows * 7) + i) < codeword_count) {
                c_grid[rows][i] = codewords[(rows * 7) + i];
            } else {
                c_grid[rows][i] = 48; /* Pad */
                pad_count++;
            }
        }
        rows++;
    } while ((rows * 7) < codeword_count);

    if ((((rows <= 6) && (pad_count < 5))) || (rows > 6) || (rows == 1)) {
        /* Add a row */
        for (i = 0; i < 7; i++) {
            c_grid[rows][i] = 48; /* Pad */
        }
        rows++;
    }

    if (symbol->option_1 >= 2 && symbol->option_1 <= 8) { /* Minimum no. of rows */
        if (symbol->option_1 > rows) {
            for (j = symbol->option_1 - rows; j > 0; j--) {
                for (i = 0; i < 7; i++) {
                    c_grid[rows][i] = 48; /* Pad */
                }
                rows++;
            }
        }
    } else if (symbol->option_1 >= 1) {
        strcpy(symbol->errtxt, "433: Minimum number of rows out of range (2 to 8)");
        return ZINT_ERROR_INVALID_OPTION;
    }

    /* Add row count and mode character */
    c_grid[rows - 1][6] = (7 * (rows - 2)) + M;

    /* Add row check character */
    for (i = 0; i < rows - 1; i++) {
        int row_sum = 0;

        for (j = 0; j < 7; j++) {
            row_sum += c_grid[i][j];
        }
        c_grid[i][7] = row_sum % 49;
    }

    /* Calculate Symbol Check Characters */
    posn_val = 0;
    x_count = c_grid[rows - 1][6] * 20;
    y_count = c_grid[rows - 1][6] * 16;
    z_count = c_grid[rows - 1][6] * 38;
    for (i = 0; i < rows - 1; i++) {
        for (j = 0; j < 4; j++) {
            local_value = (c_grid[i][2 * j] * 49) + c_grid[i][(2 * j) + 1];
            x_count += c49_x_weight[posn_val] * local_value;
            y_count += c49_y_weight[posn_val] * local_value;
            z_count += c49_z_weight[posn_val] * local_value;
            posn_val++;
        }
    }

    if (rows > 6) {
        /* Add Z Symbol Check */
        c_grid[rows - 1][0] = (z_count % 2401) / 49;
        c_grid[rows - 1][1] = (z_count % 2401) % 49;
    }

    local_value = (c_grid[rows - 1][0] * 49) + c_grid[rows - 1][1];
    x_count += c49_x_weight[posn_val] * local_value;
    y_count += c49_y_weight[posn_val] * local_value;
    posn_val++;

    /* Add Y Symbol Check */
    c_grid[rows - 1][2] = (y_count % 2401) / 49;
    c_grid[rows - 1][3] = (y_count % 2401) % 49;

    local_value = (c_grid[rows - 1][2] * 49) + c_grid[rows - 1][3];
    x_count += c49_x_weight[posn_val] * local_value;

    /* Add X Symbol Check */
    c_grid[rows - 1][4] = (x_count % 2401) / 49;
    c_grid[rows - 1][5] = (x_count % 2401) % 49;

    /* Add last row check character */
    j = 0;
    for (i = 0; i < 7; i++) {
        j += c_grid[rows - 1][i];
    }
    c_grid[rows - 1][7] = j % 49;

    if (symbol->debug & ZINT_DEBUG_PRINT) {
        fputs("Codewords:\n", stdout);
        for (i = 0; i < rows; i++) {
            for (j = 0; j < 8; j++) {
                printf(" %2d", c_grid[i][j]);
            }
            fputc('\n', stdout);
        }
    }
#ifdef ZINT_TEST
    if (symbol->debug & ZINT_DEBUG_TEST) {
        debug_test_codeword_dump_int(symbol, (int *)c_grid, rows * 8);
    }
#endif

    /* Transfer data to symbol character array (w grid) */
    for (i = 0; i < rows; i++) {
        for (j = 0; j < 4; j++) {
            w_grid[i][j] = (c_grid[i][2 * j] * 49) + c_grid[i][(2 * j) + 1];
        }
    }

    for (i = 0; i < rows; i++) {
        bp = 0;
        bp = bin_append_posn(2, 2, pattern, bp); /* Start character "10" */
        for (j = 0; j < 4; j++) {
            if (i != (rows - 1)) {
                if (c49_table4[i][j] == 'E') {
                    /* Even Parity */
                    bp = bin_append_posn(c49_even_bitpattern[w_grid[i][j]], 16, pattern, bp);
                } else {
                    /* Odd Parity */
                    bp = bin_append_posn(c49_odd_bitpattern[w_grid[i][j]], 16, pattern, bp);
                }
            } else {
                /* Last row uses all even parity */
                bp = bin_append_posn(c49_even_bitpattern[w_grid[i][j]], 16, pattern, bp);
            }
        }
        bp = bin_append_posn(15, 4, pattern, bp); /* Stop character "1111" */

        /* Expand into symbol */
        for (j = 0; j < bp; j++) {
            if (pattern[j] == '1') {
                set_module(symbol, i, j);
            }
        }
    }

    symbol->rows = rows;
    symbol->width = bp;

    if (symbol->output_options & COMPLIANT_HEIGHT) {
        /* ANSI/AIM BC6-2000 Section 2.6 minimum 8X; use 10X as default
           Formula 2 H = ((h + g)r + g)X = rows * row_height + (rows - 1) * separator as borders not included
           in symbol->height (added on) */
        const int separator = symbol->option_3 >= 1 && symbol->option_3 <= 4 ? symbol->option_3 : 1;
        const float min_row_height = stripf((8.0f * rows + separator * (rows - 1)) / rows);
        const float default_height = 10.0f * rows + separator * (rows - 1);
        error_number = set_height(symbol, min_row_height, default_height, 0.0f, 0 /*no_errtxt*/);
    } else {
        (void) set_height(symbol, 0.0f, 10.0f * rows, 0.0f, 1 /*no_errtxt*/);
    }

    symbol->output_options |= BARCODE_BIND;

    if (symbol->border_width == 0) { /* Allow override if non-zero */
        symbol->border_width = 1; /* ANSI/AIM BC6-2000 Section 2.1 (note change from previous default 2) */
    }

    return error_number;
}

/* vim: set ts=4 sw=4 et : */
