/* composite.c - Handles GS1 Composite Symbols */
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

/* The functions "getBit", "init928" and "encode928" are copyright BSI and are
   released with permission under the following terms:

   "Copyright subsists in all BSI publications. BSI also holds the copyright, in the
   UK, of the international standardisation bodies. Except as
   permitted under the Copyright, Designs and Patents Act 1988 no extract may be
   reproduced, stored in a retrieval system or transmitted in any form or by any
   means - electronic, photocopying, recording or otherwise - without prior written
   permission from BSI.

   "This does not preclude the free use, in the course of implementing the standard,
   of necessary details such as symbols, and size, type or grade designations. If these
   details are to be used for any other purpose than implementation then the prior
   written permission of BSI must be obtained."

   The date of publication for these functions is 31 May 2006
 */

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include "common.h"
#include "pdf417.h"
#include "gs1.h"
#include "general_field.h"

#include "composite.h"

INTERNAL int gs1_128_cc(struct zint_symbol *symbol, unsigned char source[], int length, const int cc_mode,
                const int cc_rows);

INTERNAL int eanx_cc(struct zint_symbol *symbol, unsigned char source[], int length, const int cc_rows);
INTERNAL int ean_leading_zeroes(struct zint_symbol *symbol, const unsigned char source[],
                unsigned char local_source[], int *p_with_addon, unsigned char *zfirst_part,
                unsigned char *zsecond_part);

INTERNAL int dbar_omnstk_set_height(struct zint_symbol *symbol, const int first_row);
INTERNAL int dbar_omn_cc(struct zint_symbol *symbol, unsigned char source[], int length, const int cc_rows);
INTERNAL int dbar_ltd_cc(struct zint_symbol *symbol, unsigned char source[], int length, const int cc_rows);
INTERNAL int dbar_exp_cc(struct zint_symbol *symbol, unsigned char source[], int length, const int cc_rows);
INTERNAL int dbar_date(const unsigned char source[], const int length, const int src_posn);

static int cc_min(const int first, const int second) {

    if (first <= second)
        return first;
    else
        return second;
}

/* gets bit in bitString at bitPos */
static int cc_getBit(const unsigned short *bitStr, const int bitPos) {
    return !!(bitStr[bitPos >> 4] & (0x8000 >> (bitPos & 15)));
}

/* converts bit string to base 928 values, codeWords[0] is highest order */
static int cc_encode928(const unsigned short bitString[], unsigned short codeWords[], const int bitLng) {
    int i, j, b, cwNdx, cwLng;
    for (cwNdx = cwLng = b = 0; b < bitLng; b += 69, cwNdx += 7) {
        const int bitCnt = cc_min(bitLng - b, 69);
        int cwCnt;
        cwLng += cwCnt = bitCnt / 10 + 1;
        for (i = 0; i < cwCnt; i++)
            codeWords[cwNdx + i] = 0; /* init 0 */
        for (i = 0; i < bitCnt; i++) {
            if (cc_getBit(bitString, b + bitCnt - i - 1)) {
                for (j = 0; j < cwCnt; j++)
                    codeWords[cwNdx + j] += cc_pwr928[i][j + 7 - cwCnt];
            }
        }
        for (i = cwCnt - 1; i > 0; i--) {
            /* add "carries" */
            codeWords[cwNdx + i - 1] += codeWords[cwNdx + i] / 928;
            codeWords[cwNdx + i] %= 928;
        }
    }
    return (cwLng);
}

/* CC-A 2D component */
static void cc_a(struct zint_symbol *symbol, const char source[], const int cc_width) {
    int i, segment, bitlen, cwCnt, variant, rows;
    int k, offset, j, total, rsCodeWords[8] = {0};
    int LeftRAPStart, RightRAPStart, CentreRAPStart, StartCluster;
    int LeftRAP, RightRAP, CentreRAP, Cluster;
    int loop;
    unsigned short codeWords[28] = {0};
    unsigned short bitStr[13] = {0};
    char pattern[580];
    int bp = 0;
    const int debug_print = symbol->debug & ZINT_DEBUG_PRINT;

    variant = 0;

    bitlen = (int) strlen(source);

    for (segment = 0; segment < 13; segment++) {
        const int strpos = segment * 16;
        if (strpos >= bitlen) {
            break;
        }
        for (i = 0; i < 16 && strpos + i < bitlen; i++) {
            if (source[strpos + i] == '1') {
                bitStr[segment] |= (0x8000 >> i);
            }
        }
    }

    /* encode codeWords from bitStr */
    cwCnt = cc_encode928(bitStr, codeWords, bitlen);

    switch (cc_width) {
        case 2:
            switch (cwCnt) {
                case 6: variant = 0;
                    break;
                case 8: variant = 1;
                    break;
                case 9: variant = 2;
                    break;
                case 11: variant = 3;
                    break;
                case 12: variant = 4;
                    break;
                case 14: variant = 5;
                    break;
                case 17: variant = 6;
                    break;
            }
            break;
        case 3:
            switch (cwCnt) {
                case 8: variant = 7;
                    break;
                case 10: variant = 8;
                    break;
                case 12: variant = 9;
                    break;
                case 14: variant = 10;
                    break;
                case 17: variant = 11;
                    break;
            }
            break;
        case 4:
            switch (cwCnt) {
                case 8: variant = 12;
                    break;
                case 11: variant = 13;
                    break;
                case 14: variant = 14;
                    break;
                case 17: variant = 15;
                    break;
                case 20: variant = 16;
                    break;
            }
            break;
    }

    rows = cc_aVariants[variant];
    k = cc_aVariants[17 + variant];
    offset = cc_aVariants[34 + variant];

    /* Reed-Solomon error correction */

    for (i = 0; i < cwCnt; i++) {
        total = (codeWords[i] + rsCodeWords[k - 1]) % 929;
        for (j = k - 1; j >= 0; j--) {
            if (j == 0) {
                rsCodeWords[j] = (929 - (total * cc_aCoeffs[offset + j]) % 929) % 929;
            } else {
                rsCodeWords[j] = (rsCodeWords[j - 1] + 929 - (total * cc_aCoeffs[offset + j]) % 929) % 929;
            }
        }
    }

    for (j = 0; j < k; j++) {
        if (rsCodeWords[j] != 0) {
            rsCodeWords[j] = 929 - rsCodeWords[j];
        }
    }

    for (i = k - 1; i >= 0; i--) {
        codeWords[cwCnt] = rsCodeWords[i];
        cwCnt++;
    }

    /* Place data into table */
    LeftRAPStart = cc_aRAPTable[variant];
    CentreRAPStart = cc_aRAPTable[variant + 17];
    RightRAPStart = cc_aRAPTable[variant + 34];
    StartCluster = cc_aRAPTable[variant + 51] / 3;

    LeftRAP = LeftRAPStart;
    CentreRAP = CentreRAPStart;
    RightRAP = RightRAPStart;
    Cluster = StartCluster; /* Cluster can be 0, 1 or 2 for Cluster(0), Cluster(3) and Cluster(6) */

    for (i = 0; i < rows; i++) {
        bp = 0;
        offset = 929 * Cluster;
        k = i * cc_width;
        /* Copy the data into codebarre */
        if (cc_width != 3) {
            bp = bin_append_posn(pdf_rap_side[LeftRAP - 1], 10, pattern, bp);
        }
        bp = bin_append_posn(pdf_bitpattern[offset + codeWords[k]], 16, pattern, bp);
        pattern[bp++] = '0';
        if (cc_width >= 2) {
            if (cc_width == 3) {
                bp = bin_append_posn(pdf_rap_centre[CentreRAP - 1], 10, pattern, bp);
            }
            bp = bin_append_posn(pdf_bitpattern[offset + codeWords[k + 1]], 16, pattern, bp);
            pattern[bp++] = '0';
            if (cc_width >= 3) {
                if (cc_width == 4) {
                    bp = bin_append_posn(pdf_rap_centre[CentreRAP - 1], 10, pattern, bp);
                }
                bp = bin_append_posn(pdf_bitpattern[offset + codeWords[k + 2]], 16, pattern, bp);
                pattern[bp++] = '0';
                if (cc_width == 4) {
                    bp = bin_append_posn(pdf_bitpattern[offset + codeWords[k + 3]], 16, pattern, bp);
                    pattern[bp++] = '0';
                }
            }
        }
        bp = bin_append_posn(pdf_rap_side[RightRAP - 1], 10, pattern, bp);
        pattern[bp++] = '1'; /* stop */

        /* so now pattern[] holds the string of '1's and '0's. - copy this to the symbol */
        for (loop = 0; loop < bp; loop++) {
            if (pattern[loop] == '1') {
                set_module(symbol, i, loop);
            }
        }
        symbol->row_height[i] = 2;
        symbol->rows++;

        /* Set up RAPs and Cluster for next row */
        LeftRAP++;
        CentreRAP++;
        RightRAP++;
        Cluster++;

        if (LeftRAP == 53) {
            LeftRAP = 1;
        }
        if (CentreRAP == 53) {
            CentreRAP = 1;
        }
        if (RightRAP == 53) {
            RightRAP = 1;
        }
        if (Cluster == 3) {
            Cluster = 0;
        }
    }
    symbol->width = bp;

    if (debug_print) {
        printf("CC-A Columns: %d, Rows: %d, Variant: %d, CodeWords: %d\n", cc_width, symbol->rows, variant, cwCnt);
    }
}

/* CC-B 2D component */
static void cc_b(struct zint_symbol *symbol, const char source[], const int cc_width) {
    const int length = (int) strlen(source) / 8;
    int i;
    unsigned char *data_string = (unsigned char *) z_alloca(length + 3);
    short chainemc[180];
    int mclength = 0;
    int k, j, p, longueur, mccorrection[50] = {0}, offset;
    int total;
    char pattern[580];
    int variant, LeftRAPStart, CentreRAPStart, RightRAPStart, StartCluster;
    int LeftRAP, CentreRAP, RightRAP, Cluster, loop;
    int columns;
    int bp = 0;
    const int debug_print = symbol->debug & ZINT_DEBUG_PRINT;

    for (i = 0; i < length; i++) {
        const int binloc = i * 8;

        data_string[i] = 0;
        for (p = 0; p < 8; p++) {
            if (source[binloc + p] == '1') {
                data_string[i] |= (0x80 >> p);
            }
        }
    }

    /* "the CC-B component shall have codeword 920 in the first symbol character position" (section 9a) */
    chainemc[mclength++] = 920;

    pdf_byteprocess(chainemc, &mclength, data_string, 0, length, 0);

    /* Now figure out which variant of the symbol to use and load values accordingly */

    variant = 0;

    if (cc_width == 2) {
        if (mclength <= 8) {
            variant = 7;
        } else if (mclength <= 13) {
            variant = 8;
        } else if (mclength <= 19) {
            variant = 9;
        } else if (mclength <= 24) {
            variant = 10;
        } else if (mclength <= 29) {
            variant = 11;
        } else if (mclength <= 33) {
            variant = 12;
        } else {
            variant = 13;
        }
    } else if (cc_width == 3) {
        if (mclength <= 6) {
            variant = 14;
        } else if (mclength <= 10) {
            variant = 15;
        } else if (mclength <= 14) {
            variant = 16;
        } else if (mclength <= 18) {
            variant = 17;
        } else if (mclength <= 24) {
            variant = 18;
        } else if (mclength <= 34) {
            variant = 19;
        } else if (mclength <= 46) {
            variant = 20;
        } else if (mclength <= 58) {
            variant = 21;
        } else if (mclength <= 70) {
            variant = 22;
        } else {
            variant = 23;
        }
    } else if (cc_width == 4) {
        if (mclength <= 8) {
            variant = 24;
        } else if (mclength <= 12) {
            variant = 25;
        } else if (mclength <= 18) {
            variant = 26;
        } else if (mclength <= 24) {
            variant = 27;
        } else if (mclength <= 30) {
            variant = 28;
        } else if (mclength <= 39) {
            variant = 29;
        } else if (mclength <= 54) {
            variant = 30;
        } else if (mclength <= 72) {
            variant = 31;
        } else if (mclength <= 90) {
            variant = 32;
        } else if (mclength <= 108) {
            variant = 33;
        } else {
            variant = 34;
        }
    }

    /* Now we have the variant we can load the data - from here on the same as MicroPDF417 code */
    variant--;
    assert(variant >= 0);
    columns = pdf_MicroVariants[variant]; /* columns */
    symbol->rows = pdf_MicroVariants[variant + 34]; /* rows */
    k = pdf_MicroVariants[variant + 68]; /* number of EC CWs */
    longueur = (columns * symbol->rows) - k; /* number of non-EC CWs */
    i = longueur - mclength; /* amount of padding required */
    offset = pdf_MicroVariants[variant + 102]; /* coefficient offset */

    /* Binary input padded to target length so no padding should be necessary */
    while (i > 0) {
        chainemc[mclength++] = 900; /* Not reached */
        i--;
    }

    /* Reed-Solomon error correction */
    longueur = mclength;
    for (i = 0; i < longueur; i++) {
        total = (chainemc[i] + mccorrection[k - 1]) % 929;
        for (j = k - 1; j >= 0; j--) {
            if (j == 0) {
                mccorrection[j] = (929 - (total * pdf_Microcoeffs[offset + j]) % 929) % 929;
            } else {
                mccorrection[j] = (mccorrection[j - 1] + 929 - (total * pdf_Microcoeffs[offset + j]) % 929) % 929;
            }
        }
    }

    for (j = 0; j < k; j++) {
        if (mccorrection[j] != 0) {
            mccorrection[j] = 929 - mccorrection[j];
        }
    }
    /* we add these codes to the string */
    for (i = k - 1; i >= 0; i--) {
        chainemc[mclength++] = mccorrection[i];
    }

    /* Now get the RAP (Row Address Pattern) start values */
    LeftRAPStart = pdf_RAPTable[variant];
    CentreRAPStart = pdf_RAPTable[variant + 34];
    RightRAPStart = pdf_RAPTable[variant + 68];
    StartCluster = pdf_RAPTable[variant + 102] / 3;

    /* That's all values loaded, get on with the encoding */

    LeftRAP = LeftRAPStart;
    CentreRAP = CentreRAPStart;
    RightRAP = RightRAPStart;
    Cluster = StartCluster;
    /* Cluster can be 0, 1 or 2 for Cluster(0), Cluster(3) and Cluster(6) */

    for (i = 0; i < symbol->rows; i++) {
        bp = 0;
        offset = 929 * Cluster;
        k = i * columns;
        /* Copy the data into codebarre */
        bp = bin_append_posn(pdf_rap_side[LeftRAP - 1], 10, pattern, bp);
        bp = bin_append_posn(pdf_bitpattern[offset + chainemc[k]], 16, pattern, bp);
        pattern[bp++] = '0';
        if (cc_width >= 2) {
            if (cc_width == 3) {
                bp = bin_append_posn(pdf_rap_centre[CentreRAP - 1], 10, pattern, bp);
            }
            bp = bin_append_posn(pdf_bitpattern[offset + chainemc[k + 1]], 16, pattern, bp);
            pattern[bp++] = '0';
            if (cc_width >= 3) {
                if (cc_width == 4) {
                    bp = bin_append_posn(pdf_rap_centre[CentreRAP - 1], 10, pattern, bp);
                }
                bp = bin_append_posn(pdf_bitpattern[offset + chainemc[k + 2]], 16, pattern, bp);
                pattern[bp++] = '0';
                if (cc_width == 4) {
                    bp = bin_append_posn(pdf_bitpattern[offset + chainemc[k + 3]], 16, pattern, bp);
                    pattern[bp++] = '0';
                }
            }
        }
        bp = bin_append_posn(pdf_rap_side[RightRAP - 1], 10, pattern, bp);
        pattern[bp++] = '1'; /* stop */

        /* so now pattern[] holds the string of '1's and '0's. - copy this to the symbol */
        for (loop = 0; loop < bp; loop++) {
            if (pattern[loop] == '1') {
                set_module(symbol, i, loop);
            }
        }
        symbol->row_height[i] = 2;

        /* Set up RAPs and Cluster for next row */
        LeftRAP++;
        CentreRAP++;
        RightRAP++;
        Cluster++;

        if (LeftRAP == 53) {
            LeftRAP = 1;
        }
        if (CentreRAP == 53) {
            CentreRAP = 1;
        }
        if (RightRAP == 53) {
            RightRAP = 1;
        }
        if (Cluster == 3) {
            Cluster = 0;
        }
    }
    symbol->width = bp;

    if (debug_print) {
        printf("CC-B Columns: %d, Rows: %d, Variant: %d, CodeWords: %d\n",
                cc_width, symbol->rows, variant + 1, mclength);
    }
}

/* CC-C 2D component - byte compressed PDF417 */
static void cc_c(struct zint_symbol *symbol, const char source[], const int cc_width, const int ecc_level) {
    const int length = (int) strlen(source) / 8;
    int i, p;
    unsigned char *data_string = (unsigned char *) z_alloca(length + 4);
    short chainemc[1000];
    int mclength = 0, k;
    int offset, longueur, loop, total, j, mccorrection[520] = {0};
    int c1, c2, c3, dummy[35];
    char pattern[580];
    int bp = 0;
    const int debug_print = symbol->debug & ZINT_DEBUG_PRINT;

    for (i = 0; i < length; i++) {
        const int binloc = i * 8;

        data_string[i] = 0;
        for (p = 0; p < 8; p++) {
            if (source[binloc + p] == '1') {
                data_string[i] |= (0x80 >> p);
            }
        }
    }

    chainemc[mclength++] = 0; /* space for length descriptor */
    chainemc[mclength++] = 920; /* CC-C identifier */

    pdf_byteprocess(chainemc, &mclength, data_string, 0, length, 0);

    chainemc[0] = mclength;

    if (debug_print) {
        printf("CC-C Codewords (%d):", mclength);
        for (i = 0; i < mclength; i++) printf(" %d", chainemc[i]);
        fputc('\n', stdout);
    }

    k = 1;
    for (i = 1; i <= (ecc_level + 1); i++) {
        k *= 2;
    }

    /* 796 - we now take care of the Reed Solomon codes */
    switch (ecc_level) {
        case 1: offset = 2; /* Not reached */
            break;
        case 2: offset = 6; /* Min ECC currently used is 2 */
            break;
        case 3: offset = 14;
            break;
        case 4: offset = 30;
            break;
        case 5: offset = 62; /* Max ECC currently used is 5 */
            break;
        case 6: offset = 126; /* Not reached */
            break;
        case 7: offset = 254; /* Not reached */
            break;
        case 8: offset = 510; /* Not reached */
            break;
        default: offset = 0; /* Not reached */
            break;
    }

    longueur = mclength;
    for (i = 0; i < longueur; i++) {
        total = (chainemc[i] + mccorrection[k - 1]) % 929;
        for (j = k - 1; j >= 0; j--) {
            if (j == 0) {
                mccorrection[j] = (929 - (total * pdf_coefrs[offset + j]) % 929) % 929;
            } else {
                mccorrection[j] = (mccorrection[j - 1] + 929 - (total * pdf_coefrs[offset + j]) % 929) % 929;
            }
        }
    }

    for (j = 0; j < k; j++) {
        if (mccorrection[j] != 0) {
            mccorrection[j] = 929 - mccorrection[j];
        }
    }
    /* we add these codes to the string */
    for (i = k - 1; i >= 0; i--) {
        chainemc[mclength++] = mccorrection[i];
    }

    /* 818 - The CW string is finished */
    symbol->rows = mclength / cc_width;
    c1 = (symbol->rows - 1) / 3;
    c2 = ecc_level * 3 + (symbol->rows - 1) % 3;
    c3 = cc_width - 1;

    /* we now encode each row */
    for (i = 0; i <= symbol->rows - 1; i++) {
        for (j = 0; j < cc_width; j++) {
            dummy[j + 1] = chainemc[i * cc_width + j];
        }
        k = (i / 3) * 30;
        switch (i % 3) {
            case 0:
                dummy[0] = k + c1;
                dummy[cc_width + 1] = k + c3;
                offset = 0; /* cluster(0) */
                break;
            case 1:
                dummy[0] = k + c2;
                dummy[cc_width + 1] = k + c1;
                offset = 929; /* cluster(3) */
                break;
            case 2:
                dummy[0] = k + c3;
                dummy[cc_width + 1] = k + c2;
                offset = 1858; /* cluster(6) */
                break;
        }
        bp = 0;
        bp = bin_append_posn(0x1FEA8, 17, pattern, bp); /* Row start */

        for (j = 0; j <= cc_width + 1; j++) {
            bp = bin_append_posn(pdf_bitpattern[offset + dummy[j]], 16, pattern, bp);
            pattern[bp++] = '0';
        }
        bp = bin_append_posn(0x3FA29, 18, pattern, bp); /* Row Stop */

        for (loop = 0; loop < bp; loop++) {
            if (pattern[loop] == '1') {
                set_module(symbol, i, loop);
            }
        }
        symbol->row_height[i] = 3;
    }
    symbol->width = bp;

    if (debug_print) {
        printf("CC-C Columns: %d, Rows: %d, CodeWords: %d, ECC Level: %d\n",
                cc_width, symbol->rows, mclength, ecc_level);
    }
}

static int cc_a_calc_padding(const int binary_length, const int cc_width) {
    int target_bitsize = 0;

    switch (cc_width) {
        case 2:
            if (binary_length <= 59) {
                target_bitsize = 59;
            } else if (binary_length <= 78) {
                target_bitsize = 78;
            } else if (binary_length <= 88) {
                target_bitsize = 88;
            } else if (binary_length <= 108) {
                target_bitsize = 108;
            } else if (binary_length <= 118) {
                target_bitsize = 118;
            } else if (binary_length <= 138) {
                target_bitsize = 138;
            } else if (binary_length <= 167) {
                target_bitsize = 167;
            }
            break;
        case 3:
            if (binary_length <= 78) {
                target_bitsize = 78;
            } else if (binary_length <= 98) {
                target_bitsize = 98;
            } else if (binary_length <= 118) {
                target_bitsize = 118;
            } else if (binary_length <= 138) {
                target_bitsize = 138;
            } else if (binary_length <= 167) {
                target_bitsize = 167;
            }
            break;
        case 4:
            if (binary_length <= 78) {
                target_bitsize = 78;
            } else if (binary_length <= 108) {
                target_bitsize = 108;
            } else if (binary_length <= 138) {
                target_bitsize = 138;
            } else if (binary_length <= 167) {
                target_bitsize = 167;
            } else if (binary_length <= 197) {
                target_bitsize = 197;
            }
            break;
    }

    return target_bitsize;
}

static int cc_b_calc_padding(const int binary_length, const int cc_width) {
    int target_bitsize = 0;

    switch (cc_width) {
        case 2:
            if (binary_length <= 56) {
                target_bitsize = 56;
            } else if (binary_length <= 104) {
                target_bitsize = 104;
            } else if (binary_length <= 160) {
                target_bitsize = 160;
            } else if (binary_length <= 208) {
                target_bitsize = 208;
            } else if (binary_length <= 256) {
                target_bitsize = 256;
            } else if (binary_length <= 296) {
                target_bitsize = 296;
            } else if (binary_length <= 336) {
                target_bitsize = 336;
            }
            break;
        case 3:
            if (binary_length <= 32) {
                target_bitsize = 32;
            } else if (binary_length <= 72) {
                target_bitsize = 72;
            } else if (binary_length <= 112) {
                target_bitsize = 112;
            } else if (binary_length <= 152) {
                target_bitsize = 152;
            } else if (binary_length <= 208) {
                target_bitsize = 208;
            } else if (binary_length <= 304) {
                target_bitsize = 304;
            } else if (binary_length <= 416) {
                target_bitsize = 416;
            } else if (binary_length <= 536) {
                target_bitsize = 536;
            } else if (binary_length <= 648) {
                target_bitsize = 648;
            } else if (binary_length <= 768) {
                target_bitsize = 768;
            }
            break;
        case 4:
            if (binary_length <= 56) {
                target_bitsize = 56;
            } else if (binary_length <= 96) {
                target_bitsize = 96;
            } else if (binary_length <= 152) {
                target_bitsize = 152;
            } else if (binary_length <= 208) {
                target_bitsize = 208;
            } else if (binary_length <= 264) {
                target_bitsize = 264;
            } else if (binary_length <= 352) {
                target_bitsize = 352;
            } else if (binary_length <= 496) {
                target_bitsize = 496;
            } else if (binary_length <= 672) {
                target_bitsize = 672;
            } else if (binary_length <= 840) {
                target_bitsize = 840;
            } else if (binary_length <= 1016) {
                target_bitsize = 1016;
            } else if (binary_length <= 1184) {
                target_bitsize = 1184;
            }
            break;
    }

    return target_bitsize;
}

static int cc_c_calc_padding(const int binary_length, int *p_cc_width, const int linear_width, int *p_ecc_level) {
    int target_bitsize = 0;
    int byte_length, codewords_used, ecc_level, ecc_codewords, rows;
    int codewords_total, target_codewords, target_bytesize;

    byte_length = binary_length / 8;
    if (binary_length % 8 != 0) {
        byte_length++;
    }

    codewords_used = (byte_length / 6) * 5;
    codewords_used += byte_length % 6;

    /* Recommended minimum ecc levels ISO/IEC 1543:2015 (PDF417) Annex E Table E.1,
       restricted by CC-C codeword max 900 (30 cols * 30 rows), GS1 General Specifications 19.1 5.9.2.3 */
    if (codewords_used <= 40) {
        ecc_level = 2;
    } else if (codewords_used <= 160) {
        ecc_level = 3;
    } else if (codewords_used <= 320) {
        ecc_level = 4;
    } else if (codewords_used <= 833) { /* 900 - 3 - 64 */
        ecc_level = 5;
    } else if (codewords_used <= 865) { /* 900 - 3 - 32 */
        ecc_level = 4; /* Not recommended but allow to meet advertised "up to 2361 digits" (allows max 2372) */
    } else {
        return 0;
    }
    *p_ecc_level = ecc_level;
    ecc_codewords = 1 << (ecc_level + 1);

    codewords_used += ecc_codewords;
    codewords_used += 3;

    /* Minimum possible linear width (with GS1_NO_CHECK) is 11*5 (start, FNC1, linkage, data, check) + 13 stop */
    assert(linear_width >= 68);
    /* -52 = 7 left shift (section 12.3 f) + 10 right quiet zone - 17 start + 2x17 row indicators + 18 stop */
    *p_cc_width = linear_width == 68 ? 1 : (linear_width - 52) / 17; /* Ensure > 0 */
    if (*p_cc_width > 30) {
        *p_cc_width = 30;
    }
    assert(*p_cc_width > 0);
    rows = (int) ceil((double) codewords_used / *p_cc_width);
    /* stop the symbol from becoming too high */
    while (rows > 30 && *p_cc_width < 30) {
        (*p_cc_width)++;
        rows = (int) ceil((double) codewords_used / *p_cc_width);
    }

    if (rows > 30) { /* Should never happen given `codewords_used` check above (865 / 30 ~ 28.83) */
        return 0; /* Not reached */
    }
    if (rows < 3) {
        rows = 3;
    }

    codewords_total = *p_cc_width * rows;

    target_codewords = codewords_total - ecc_codewords;
    target_codewords -= 3;

    target_bytesize = 6 * (target_codewords / 5);
    target_bytesize += target_codewords % 5;

    target_bitsize = 8 * target_bytesize;

    return target_bitsize;
}

/* Handles all data encodation from section 5 of ISO/IEC 24723 */
static int cc_binary_string(struct zint_symbol *symbol, const unsigned char source[], const int length,
            char binary_string[], const int cc_mode, int *p_cc_width, int *p_ecc_level, const int linear_width) {
    int encoding_method, read_posn, alpha_pad;
    int i, j, ai_crop, ai_crop_posn, fnc1_latch;
    int ai90_mode, remainder;
    char last_digit = '\0';
    int mode;
    char *general_field = (char *) z_alloca(length + 1);
    int target_bitsize;
    int bp = 0;
    const int debug_print = symbol->debug & ZINT_DEBUG_PRINT;

    encoding_method = 1;
    read_posn = 0;
    ai_crop = 0;
    ai_crop_posn = -1;
    fnc1_latch = 0;
    alpha_pad = 0;
    *p_ecc_level = 0;
    target_bitsize = 0;
    mode = NUMERIC;

    if (length > 1 && (source[0] == '1') && ((source[1] == '0') || (source[1] == '1') || (source[1] == '7'))) {
        /* Source starts (10), (11) or (17) */
        if (source[1] == '0' || dbar_date(source, length, 2) >= 0) { /* Check date valid if (11) or (17) */
            encoding_method = 2;
        }
    } else if (length > 1 && (source[0] == '9') && (source[1] == '0')) {
        /* Source starts (90) */
        encoding_method = 3;
    }

    if (encoding_method == 1) {
        binary_string[bp++] = '0';
        if (debug_print) printf("CC-%c Encodation Method: 0\n", 'A' + (cc_mode - 1));

    } else if (encoding_method == 2) {
        /* Encoding Method field "10" - date and lot number */

        bp = bin_append_posn(2, 2, binary_string, bp); /* "10" */

        if (source[1] == '0') {
            /* No date data */
            bp = bin_append_posn(3, 2, binary_string, bp); /* "11" */
            read_posn = 2;
        } else {
            /* Production Date (11) or Expiration Date (17) */
            assert(length >= 8); /* Due to `dbar_date()` check above */

            bp = bin_append_posn(dbar_date(source, length, 2), 16, binary_string, bp);

            if (source[1] == '1') {
                /* Production Date AI 11 */
                binary_string[bp++] = '0';
            } else {
                /* Expiration Date AI 17 */
                binary_string[bp++] = '1';
            }
            read_posn = 8;

            if (read_posn + 1 < length && (source[read_posn] == '1') && (source[read_posn + 1] == '0')) {
                /* Followed by AI 10 - strip this from general field */
                read_posn += 2;
            } else if (source[read_posn]) {
                /* ISO/IEC 24723:2010 5.3.1 "If a lot number does not directly follow the date element string,
                   a FNC1 is encoded following the date element string ..." */
                fnc1_latch = 1;
            } else {
                /* "... even if no more data follows the date element string" */
                /* So still need FNC1 character but can't do single FNC1 in numeric mode, so insert alphanumeric latch
                   "0000" and alphanumeric FNC1 "01111" (this implementation detail taken from BWIPP
                   https://github.com/bwipp/postscriptbarcode Copyright (c) 2004-2019 Terry Burton) */
                bp = bin_append_posn(15, 9, binary_string, bp); /* "000001111" */
                /* Note an alphanumeric FNC1 is also a numeric latch, so now in numeric mode */
            }
        }

        if (debug_print) {
            printf("CC-%c Encodation Method: 10, Compaction Field: %.*s\n", 'A' + (cc_mode - 1), read_posn, source);
        }

    } else if (encoding_method == 3) {
        /* Encodation Method field of "11" - AI 90 */
        unsigned char *ninety = (unsigned char *) z_alloca(length + 1);
        int ninety_len, alpha, alphanum, numeric, alpha_posn;

        /* "This encodation method may be used if an element string with an AI
        90 occurs at the start of the data message, and if the data field
        following the two-digit AI 90 starts with an alphanumeric string which
        complies with a specific format." (para 5.3.2) */

        i = 0;
        if (length > 2) {
            do {
                ninety[i] = source[i + 2];
                i++;
            } while (i + 2 < length && source[i + 2] != '\x1D');
        }
        ninety[i] = '\0';
        ninety_len = i;

        /* Find out if the AI 90 data is alphabetic or numeric or both */

        alpha = 0;
        alphanum = 0;
        numeric = 0;

        for (i = 0; i < ninety_len; i++) {

            if (z_isupper(ninety[i])) {
                /* Character is alphabetic */
                alpha += 1;
            } else if (z_isdigit(ninety[i])) {
                /* Character is numeric */
                numeric += 1;
            } else {
                alphanum += 1;
            }
        }

        /* must start with 0, 1, 2 or 3 digits followed by an uppercase character */
        alpha_posn = -1;
        if (ninety_len && ninety[0] != '0') { /* Leading zeros are not permitted */
            for (i = 0; i < ninety_len && i < 4; i++) {
                if (z_isupper(ninety[i])) {
                    alpha_posn = i;
                    break;
                }
                if (!z_isdigit(ninety[i])) {
                    break;
                }
            }
        }

        if (alpha_posn != -1) {
            int next_ai_posn;
            int numeric_value;
            int table3_letter;
            /* Encodation method "11" can be used */
            bp = bin_append_posn(3, 2, binary_string, bp); /* "11" */

            numeric -= alpha_posn;
            alpha--;

            /* Decide on numeric, alpha or alphanumeric mode */
            /* Alpha mode is a special mode for AI 90 */

            if (alphanum == 0 && alpha > numeric) {
                /* Alpha mode */
                bp = bin_append_posn(3, 2, binary_string, bp); /* "11" */
                ai90_mode = 2;
            } else if (alphanum == 0 && alpha == 0) {
                /* Numeric mode */
                bp = bin_append_posn(2, 2, binary_string, bp); /* "10" */
                ai90_mode = 3;
            } else {
                /* Note if first 4 are digits then it would be shorter to go into NUMERIC mode first; not
                   implemented */
                /* Alphanumeric mode */
                binary_string[bp++] = '0';
                ai90_mode = 1;
                mode = ALPHANUMERIC;
            }

            next_ai_posn = 2 + ninety_len;

            if (next_ai_posn < length && source[next_ai_posn] == '\x1D') {
                /* There are more AIs afterwards */
                if (next_ai_posn + 2 < length
                        && (source[next_ai_posn + 1] == '2') && (source[next_ai_posn + 2] == '1')) {
                    /* AI 21 follows */
                    ai_crop = 1;
                } else if (next_ai_posn + 4 < length
                        && (source[next_ai_posn + 1] == '8') && (source[next_ai_posn + 2] == '0')
                        && (source[next_ai_posn + 3] == '0') && (source[next_ai_posn + 4] == '4')) {
                    /* AI 8004 follows */
                    ai_crop = 3;
                }
            }

            switch (ai_crop) {
                case 0: binary_string[bp++] = '0';
                    break;
                case 1: bp = bin_append_posn(2, 2, binary_string, bp); /* "10" */
                    ai_crop_posn = next_ai_posn + 1;
                    break;
                case 3: bp = bin_append_posn(3, 2, binary_string, bp); /* "11" */
                    ai_crop_posn = next_ai_posn + 1;
                    break;
            }

            numeric_value = alpha_posn ? to_int(ninety, alpha_posn) : 0;

            table3_letter = -1;
            if (numeric_value < 31) {
                table3_letter = posn("BDHIJKLNPQRSTVWZ", ninety[alpha_posn]);
            }

            if (table3_letter != -1) {
                /* Encoding can be done according to 5.3.2 c) 2) */
                /* five bit binary string representing value before letter */
                bp = bin_append_posn(numeric_value, 5, binary_string, bp);

                /* followed by four bit representation of letter from Table 3 */
                bp = bin_append_posn(table3_letter, 4, binary_string, bp);
            } else {
                /* Encoding is done according to 5.3.2 c) 3) */
                bp = bin_append_posn(31, 5, binary_string, bp);
                /* ten bit representation of number */
                bp = bin_append_posn(numeric_value, 10, binary_string, bp);

                /* five bit representation of ASCII character */
                bp = bin_append_posn(ninety[alpha_posn] - 65, 5, binary_string, bp);
            }

            read_posn = alpha_posn + 3; /* +2 for 90 and +1 to go beyond alpha position */

            /* Do Alpha mode encoding of the rest of the AI 90 data field here */
            if (ai90_mode == 2) {
                /* Alpha encodation (section 5.3.3) */
                do {
                    if (z_isupper(source[read_posn])) {
                        bp = bin_append_posn(source[read_posn] - 65, 5, binary_string, bp);

                    } else if (z_isdigit(source[read_posn])) {
                        bp = bin_append_posn(source[read_posn] + 4, 6, binary_string, bp);

                    } else if (source[read_posn] == '\x1D') {
                        bp = bin_append_posn(31, 5, binary_string, bp);
                    }

                    read_posn++;
                } while (source[read_posn - 1] != '\x1D' && source[read_posn - 1] != '\0');
                alpha_pad = 1; /* This is overwritten if a general field is encoded */
            }

            if (debug_print) {
                printf("CC-%c Encodation Method: 11, Compaction Field: %.*s, Binary: %.*s (%d)\n",
                        'A' + (cc_mode - 1), read_posn, source, bp, binary_string, bp);
            }
        } else {
            /* Use general field encodation instead */
            binary_string[bp++] = '0';
            read_posn = 0;
            if (debug_print) printf("CC-%c Encodation Method: 0\n", 'A' + (cc_mode - 1));
        }
    }

    /* The compressed data field has been processed if appropriate - the
    rest of the data (if any) goes into a general-purpose data compaction field */

    j = 0;
    if (fnc1_latch == 1) {
        /* Encodation method "10" has been used but it is not followed by
           AI 10, so a FNC1 character needs to be added */
        general_field[j] = '\x1D';
        j++;
    }

    for (i = read_posn; i < length; i++) {
        /* Skip "[21" or "[8004" AIs if encodation method "11" used */
        if (i == ai_crop_posn) {
            i += ai_crop;
        } else {
            general_field[j] = source[i];
            j++;
        }
    }
    general_field[j] = '\0';

    if (debug_print) {
        printf("Mode %s, General Field: %.40s%s\n",
                mode == NUMERIC ? "NUMERIC" : mode == ALPHANUMERIC ? "ALPHANUMERIC" : "ISO646",
                general_field, j > 40 ? "..." : "");
    }

    if (j != 0) { /* If general field not empty */
        alpha_pad = 0;

        if (!general_field_encode(general_field, j, &mode, &last_digit, binary_string, &bp)) {
            /* Invalid character in input data */
            return errtxt(ZINT_ERROR_INVALID_DATA, symbol, 441, "Invalid character in input (2D component)");
        }
    }

    switch (cc_mode) {
        case 1:
            target_bitsize = cc_a_calc_padding(bp, *p_cc_width);
            break;
        case 2:
            target_bitsize = cc_b_calc_padding(bp, *p_cc_width);
            break;
        case 3:
            target_bitsize = cc_c_calc_padding(bp, p_cc_width, linear_width, p_ecc_level);
            break;
    }

    if (target_bitsize == 0) {
        return errtxt(ZINT_ERROR_TOO_LONG, symbol, 442, "Input too long (2D component)");
    }

    remainder = target_bitsize - bp;

    if (last_digit) {
        /* There is still one more numeric digit to encode */

        if ((remainder >= 4) && (remainder <= 6)) {
            /* ISO/IEC 24723:2010 5.4.1 c) 2) "If four to six bits remain, add 1 to the digit value and encode the
               result in the next four bits. ..." */
            bp = bin_append_posn(ctoi(last_digit) + 1, 4, binary_string, bp);
            if (remainder > 4) {
                /* "... The fifth and sixth bits, if present, shall be “0”s." (Covered by adding truncated
                   alphanumeric latch below but do explicitly anyway) */
                bp = bin_append_posn(0, remainder - 4, binary_string, bp);
            }
        } else {
            bp = bin_append_posn((11 * ctoi(last_digit)) + 18, 7, binary_string, bp);
            /* This may push the symbol up to the next size */
        }
    }

    switch (cc_mode) {
        case 1:
            target_bitsize = cc_a_calc_padding(bp, *p_cc_width);
            break;
        case 2:
            target_bitsize = cc_b_calc_padding(bp, *p_cc_width);
            break;
        case 3:
            target_bitsize = cc_c_calc_padding(bp, p_cc_width, linear_width, p_ecc_level);
            break;
    }

    if (target_bitsize == 0) {
        return errtxt(ZINT_ERROR_TOO_LONG, symbol, 444, "Input too long (2D component)");
    }

    if (bp < target_bitsize) {
        /* Now add padding to binary string */
        if (alpha_pad == 1) {
            bp = bin_append_posn(31, 5, binary_string, bp); /* "11111" */
            /* Extra FNC1 character required after Alpha encodation (section 5.3.3) */
        }

        if (mode == NUMERIC) {
            bp = bin_append_posn(0, 4, binary_string, bp); /* "0000" */
        }

        while (bp < target_bitsize) {
            bp = bin_append_posn(4, 5, binary_string, bp); /* "00100" */
        }
    }
    binary_string[target_bitsize] = '\0';

    if (debug_print) {
        printf("ECC: %d, CC width %d\n", *p_ecc_level, *p_cc_width);
        printf("Binary: %s (%d)\n", binary_string, target_bitsize);
    }

    return 0;
}

/* Calculate the width of the linear part (primary) */
static int cc_linear_dummy_run(int input_mode, unsigned char *source, const int length, const int debug,
            char *errtxt) {
    struct zint_symbol dummy = {0};
    int error_number;
    int linear_width;

    dummy.symbology = BARCODE_GS1_128_CC;
    dummy.option_1 = -1;
    dummy.input_mode = input_mode;
    dummy.debug = debug;
    error_number = gs1_128_cc(&dummy, source, length, 3 /*cc_mode*/, 0 /*cc_rows*/);
    linear_width = dummy.width;
    if (error_number >= ZINT_ERROR || (debug & ZINT_DEBUG_TEST)) {
        strcpy(errtxt, dummy.errtxt);
    }

    if (error_number >= ZINT_ERROR) {
        return 0;
    }
    return linear_width;
}

INTERNAL int composite(struct zint_symbol *symbol, unsigned char source[], int length) {
    int error_number, cc_mode, cc_width = 0, ecc_level = 0;
    int j, i, k;
    /* Allow for 8 bits + 5-bit latch per char + 1000 bits overhead/padding */
    const unsigned int bs = 13 * length + 1000 + 1;
    char *binary_string = (char *) z_alloca(bs);
    unsigned int pri_len;
    struct zint_symbol *linear;
    int top_shift, bottom_shift;
    int linear_width = 0;
    const int debug_print = symbol->debug & ZINT_DEBUG_PRINT;

    if (debug_print) printf("Reduced length: %d\n", length);

    /* Perform sanity checks on input options first */
    error_number = 0;
    pri_len = (int) strlen(symbol->primary);
    if (pri_len == 0) {
        /* TODO: change to more appropiate ZINT_ERROR_INVALID_DATA */
        return errtxt(ZINT_ERROR_INVALID_OPTION, symbol, 445, "No primary (linear) message");
    }

    if (length > 2990) {
        return errtxtf(ZINT_ERROR_TOO_LONG, symbol, 446,
                        "2D component input too long, requires %d characters (maximum 2990)", length);
    }

    cc_mode = symbol->option_1;
    if ((cc_mode == 3) && (symbol->symbology != BARCODE_GS1_128_CC)) {
        /* CC-C can only be used with a GS1-128 linear part */
        return errtxt(ZINT_ERROR_INVALID_OPTION, symbol, 447,
                        "Invalid mode (CC-C only valid with GS1-128 linear component)");
    }

    if (symbol->symbology == BARCODE_GS1_128_CC) {
        /* Do a test run of encoding the linear component to establish its width */
        linear_width = cc_linear_dummy_run(symbol->input_mode, (unsigned char *) symbol->primary, pri_len,
                                            symbol->debug, symbol->errtxt);
        if (linear_width == 0) {
            return errtxt_adj(ZINT_ERROR_INVALID_DATA, symbol, "%1$s%2$s", " (linear component)");
        }
        if (debug_print) {
            printf("GS1-128 linear width: %d\n", linear_width);
        }
    }

    switch (symbol->symbology) {
            /* Determine width of 2D component according to ISO/IEC 24723 Table 1 */
        case BARCODE_EANX_CC:
            if (pri_len < 20) {
                int padded_pri_len;
                int with_addon;
                unsigned char padded_pri[21];
                if (!ean_leading_zeroes(symbol, (unsigned char *) symbol->primary, padded_pri, &with_addon, NULL,
                                        NULL)) {
                    return errtxt_adj(ZINT_ERROR_TOO_LONG, symbol, "%1$s%2$s", " (linear component)");
                }
                padded_pri_len = (int) ustrlen(padded_pri);
                if (padded_pri_len <= 7) { /* EAN-8 */
                    cc_width = 3;
                } else {
                    switch (padded_pri_len) {
                        case 10: /* EAN-8 + 2 */
                            cc_width = 3;
                            break;
                        case 13: /* EAN-13 CHK or EAN-8 + 5 */
                            cc_width = with_addon ? 3 : 4;
                            break;
                        case 12: /* EAN-13 */
                        case 15: /* EAN-13 + 2 */
                        case 16: /* EAN-13 CHK + 2 */
                        case 18: /* EAN-13 + 5 */
                        case 19: /* EAN-13 CHK + 5 */
                            cc_width = 4;
                            break;
                    }
                }
            }
            if (cc_width == 0) {
                return errtxtf(ZINT_ERROR_TOO_LONG, symbol, 449, "Input length %d wrong (linear component)", pri_len);
            }
            break;
        case BARCODE_GS1_128_CC: cc_width = 4;
            break;
        case BARCODE_DBAR_OMN_CC: cc_width = 4;
            break;
        case BARCODE_DBAR_LTD_CC: cc_width = 3;
            break;
        case BARCODE_DBAR_EXP_CC: cc_width = 4;
            break;
        case BARCODE_UPCA_CC: cc_width = 4;
            break;
        case BARCODE_UPCE_CC: cc_width = 2;
            break;
        case BARCODE_DBAR_STK_CC: cc_width = 2;
            break;
        case BARCODE_DBAR_OMNSTK_CC: cc_width = 2;
            break;
        case BARCODE_DBAR_EXPSTK_CC: cc_width = 4;
            break;
    }

    if (cc_mode < 1 || cc_mode > 3) {
        cc_mode = 1;
    }

    if (cc_mode == 1) {
        i = cc_binary_string(symbol, source, length, binary_string, cc_mode, &cc_width, &ecc_level, linear_width);
        if (i == ZINT_ERROR_TOO_LONG) {
            symbol->errtxt[0] = '\0'; /* Unset error text */
            cc_mode = 2;
        } else if (i != 0) {
            return i;
        }
    }

    if (cc_mode == 2) {
        /* If the data didn't fit into CC-A it is recalculated for CC-B */
        i = cc_binary_string(symbol, source, length, binary_string, cc_mode, &cc_width, &ecc_level, linear_width);
        if (i == ZINT_ERROR_TOO_LONG) {
            if (symbol->symbology != BARCODE_GS1_128_CC) {
                return ZINT_ERROR_TOO_LONG;
            }
            symbol->errtxt[0] = '\0'; /* Unset error text */
            cc_mode = 3;
        } else if (i != 0) {
            return i;
        }
    }

    if (cc_mode == 3) {
        /* If the data didn't fit in CC-B (and linear part is GS1-128) it is recalculated for CC-C */
        i = cc_binary_string(symbol, source, length, binary_string, cc_mode, &cc_width, &ecc_level, linear_width);
        if (i != 0) {
            return i;
        }
    }

    switch (cc_mode) {
            /* Note that ecc_level is only relevant to CC-C */
        case 1: cc_a(symbol, binary_string, cc_width);
            break;
        case 2: cc_b(symbol, binary_string, cc_width);
            break;
        case 3: cc_c(symbol, binary_string, cc_width, ecc_level);
            break;
    }

    /* 2D component done, now calculate linear component */
    linear = ZBarcode_Create(); /* Symbol contains the 2D component and Linear contains the rest */

    linear->symbology = symbol->symbology;
    linear->input_mode = symbol->input_mode;
    linear->output_options = symbol->output_options;
    linear->show_hrt = symbol->show_hrt;
    linear->option_2 = symbol->option_2;
    linear->option_3 = symbol->option_3;
    /* If symbol->height given minimum row height will be returned, else default height */
    linear->height = symbol->height;
    linear->debug = symbol->debug;

    if (linear->symbology != BARCODE_GS1_128_CC) {
        /* Set the "component linkage" flag in the linear component */
        linear->option_1 = 2;
    }

    switch (symbol->symbology) {
        case BARCODE_EANX_CC:
            error_number = eanx_cc(linear, (unsigned char *) symbol->primary, pri_len, symbol->rows);
            break;
        case BARCODE_GS1_128_CC:
            /* GS1-128 needs to know which type of 2D component is used */
            error_number = gs1_128_cc(linear, (unsigned char *) symbol->primary, pri_len, cc_mode, symbol->rows);
            break;
        case BARCODE_DBAR_OMN_CC:
            error_number = dbar_omn_cc(linear, (unsigned char *) symbol->primary, pri_len, symbol->rows);
            break;
        case BARCODE_DBAR_LTD_CC:
            error_number = dbar_ltd_cc(linear, (unsigned char *) symbol->primary, pri_len, symbol->rows);
            break;
        case BARCODE_DBAR_EXP_CC:
            error_number = dbar_exp_cc(linear, (unsigned char *) symbol->primary, pri_len, symbol->rows);
            break;
        case BARCODE_UPCA_CC:
            error_number = eanx_cc(linear, (unsigned char *) symbol->primary, pri_len, symbol->rows);
            break;
        case BARCODE_UPCE_CC:
            error_number = eanx_cc(linear, (unsigned char *) symbol->primary, pri_len, symbol->rows);
            break;
        case BARCODE_DBAR_STK_CC:
            error_number = dbar_omn_cc(linear, (unsigned char *) symbol->primary, pri_len, symbol->rows);
            break;
        case BARCODE_DBAR_OMNSTK_CC:
            error_number = dbar_omn_cc(linear, (unsigned char *) symbol->primary, pri_len, symbol->rows);
            break;
        case BARCODE_DBAR_EXPSTK_CC:
            error_number = dbar_exp_cc(linear, (unsigned char *) symbol->primary, pri_len, symbol->rows);
            break;
    }

    if (error_number) {
        errtxtf(0, symbol, -1, "%1$s%2$s", linear->errtxt, " (linear component)");
        if (error_number >= ZINT_ERROR) {
            ZBarcode_Delete(linear);
            return error_number;
        }
    }

    /* Merge the linear component with the 2D component */

    top_shift = 0;
    bottom_shift = 0;

    switch (symbol->symbology) {
            /* Determine horizontal alignment (according to section 12.3) */
        case BARCODE_EANX_CC:
            switch (ustrlen(linear->text)) { /* Use zero-padded length */
                case 8: /* EAN-8 */
                case 11: /* EAN-8 + 2 */
                case 14: /* EAN-8 + 5 */
                    if (cc_mode == 1) {
                        bottom_shift = 3;
                    } else {
                        bottom_shift = 13;
                    }
                    break;
                case 13: /* EAN-13 */
                case 16: /* EAN-13 + 2 */
                case 19: /* EAN-13 + 5 */
                    bottom_shift = 2;
                    break;
            }
            break;
        case BARCODE_GS1_128_CC:
            if (cc_mode == 3) {
                bottom_shift = 7; /* ISO/IEC 24723:2010 12.3 f) */
            } else {
                /* ISO/IEC 24723:2010 12.3 g) "GS1-128 components linked to the right quiet zone of the CC-A or CC-B:
                   the CC-A or CC-B component is aligned with the last space module of one of the rightmost symbol
                   characters of the linear component. To calculate the target Code 128 symbol character position for
                   alignment, number the positions from right to left (0 is the Stop character, 1 is the Check
                   character, etc.), and then Position = (total number of Code 128 symbol characters – 9) div 2"
                 */
                const int num_symbols = (linear_width - 2) / 11;
                const int position = (num_symbols - 9) / 2;
                /* Less 1 to align with last space module */
                int calc_shift = linear->width - position * 11 - 1 - symbol->width;
                if (position) {
                    calc_shift -= 2; /* Less additional stop modules */
                }
                if (calc_shift > 0) {
                    top_shift = calc_shift;
                } else if (calc_shift < 0) {
                    bottom_shift = -calc_shift;
                }
            }
            break;
        case BARCODE_DBAR_OMN_CC: bottom_shift = 4;
            break;
        case BARCODE_DBAR_LTD_CC:
            if (cc_mode == 1) {
                top_shift = 1;
            } else {
                bottom_shift = 9;
            }
            break;
        case BARCODE_DBAR_EXP_CC:
            for (k = 1; !module_is_set(linear, 1, k - 1) && module_is_set(linear, 1, k); k++);
            top_shift = k;
            break;
        case BARCODE_UPCA_CC: bottom_shift = 2;
            break;
        case BARCODE_UPCE_CC: bottom_shift = 2;
            break;
        case BARCODE_DBAR_STK_CC: top_shift = 1;
            break;
        case BARCODE_DBAR_OMNSTK_CC: top_shift = 1;
            break;
        case BARCODE_DBAR_EXPSTK_CC:
            for (k = 1; !module_is_set(linear, 1, k - 1) && module_is_set(linear, 1, k); k++);
            top_shift = k;
            break;
    }

    if (debug_print) {
        printf("Top shift: %d, Bottom shift: %d\n", top_shift, bottom_shift);
    }

    if (top_shift != 0) {
        /* Move the 2D component of the symbol horizontally */
        for (i = 0; i <= symbol->rows; i++) {
            for (j = (symbol->width + top_shift); j >= top_shift; j--) {
                if (module_is_set(symbol, i, j - top_shift)) {
                    set_module(symbol, i, j);
                } else {
                    unset_module(symbol, i, j);
                }
            }
            for (j = 0; j < top_shift; j++) {
                unset_module(symbol, i, j);
            }
        }
    }

    /* Merge linear and 2D components into one structure */
    for (i = 0; i <= linear->rows; i++) {
        symbol->row_height[symbol->rows + i] = linear->row_height[i];
        for (j = 0; j <= linear->width; j++) {
            if (module_is_set(linear, i, j)) {
                set_module(symbol, i + symbol->rows, j + bottom_shift);
            } else {
                unset_module(symbol, i + symbol->rows, j + bottom_shift);
            }
        }
    }
    if ((linear->width + bottom_shift) > symbol->width + top_shift) {
        symbol->width = linear->width + bottom_shift;
    } else if ((symbol->width + top_shift) > linear->width + bottom_shift) {
        symbol->width += top_shift;
    }
    symbol->rows += linear->rows;
    if (symbol->output_options & COMPLIANT_HEIGHT) {
        if (symbol->symbology == BARCODE_DBAR_STK_CC) {
            /* Databar Stacked needs special treatment due to asymmetric rows */
            error_number = dbar_omnstk_set_height(symbol, symbol->rows - linear->rows + 1 /*first_row*/);
        } else if (symbol->symbology == BARCODE_DBAR_EXP_CC || symbol->symbology == BARCODE_DBAR_EXPSTK_CC) {
            /* If symbol->height given then min row height was returned, else default height */
            if (error_number == 0) { /* Avoid overwriting any `gs1_verify()` warning */
                error_number = set_height(symbol, symbol->height ? linear->height : 0.0f,
                                        symbol->height ? 0.0f : linear->height, 0.0f, 0 /*no_errtxt*/);
            } else {
                (void) set_height(symbol, symbol->height ? linear->height : 0.0f,
                                        symbol->height ? 0.0f : linear->height, 0.0f, 1 /*no_errtxt*/);
            }
        } else {
            /* If symbol->height given then min row height was returned, else default height */
            error_number = set_height(symbol, symbol->height ? linear->height : 0.0f,
                                    symbol->height ? 0.0f : linear->height, 0.0f, 0 /*no_errtxt*/);
        }
    } else {
        if (symbol->symbology == BARCODE_DBAR_STK_CC) {
            (void) dbar_omnstk_set_height(symbol, symbol->rows - linear->rows + 1 /*first_row*/);
        } else {
            (void) set_height(symbol, symbol->height ? linear->height : 0.0f, symbol->height ? 0.0f : linear->height,
                                0.0f, 1 /*no_errtxt*/);
        }
    }

    ustrcpy(symbol->text, linear->text);

    ZBarcode_Delete(linear);

    return error_number;
}

/* vim: set ts=4 sw=4 et : */
