/* qr.h Data for QR Code, Micro QR Code and rMQR */
/*
    libzint - the open source barcode library
    Copyright (C) 2008-2024 Robin Stuart <rstuart114@gmail.com>
    Copyright (C) 2006 Kentaro Fukuchi <fukuchi@megaui.net>

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

#ifndef Z_QR_H
#define Z_QR_H

/* From ISO/IEC 18004:2015 Table 5 - Encoding/decoding table for Alphanumeric mode */
static const signed char qr_alphanumeric[59] = {
    36, -1, -1, -1, 37, 38, -1, -1, -1, -1, 39, 40, -1, 41, 42, 43, /* SP-/ */
     0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 44, -1, -1, -1, -1, -1, /* 0-? */
    -1, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, /* @-O */
    25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35                      /* P-Z */
};

/* From ISO/IEC 18004:2015 Table 7 - Number of symbol characters and input data capacity for QR Code */
static const unsigned short qr_data_codewords[4][40] = { {
          19,   34,   55,   80,  108,  136,  156,  194,  232,  274, /* L */
         324,  370,  428,  461,  523,  589,  647,  721,  795,  861,
         932, 1006, 1094, 1174, 1276, 1370, 1468, 1531, 1631, 1735,
        1843, 1955, 2071, 2191, 2306, 2434, 2566, 2702, 2812, 2956
    }, {
          16,   28,   44,   64,   86,  108,  124,  154,  182,  216, /* M */
         254,  290,  334,  365,  415,  453,  507,  563,  627,  669,
         714,  782,  860,  914, 1000, 1062, 1128, 1193, 1267, 1373,
        1455, 1541, 1631, 1725, 1812, 1914, 1992, 2102, 2216, 2334
    }, {
          13,   22,   34,   48,   62,   76,   88,  110,  132,  154, /* Q */
         180,  206,  244,  261,  295,  325,  367,  397,  445,  485,
         512,  568,  614,  664,  718,  754,  808,  871,  911,  985,
        1033, 1115, 1171, 1231, 1286, 1354, 1426, 1502, 1582, 1666
    }, {
           9,   16,   26,   36,   46,   60,   66,   86,  100,  122, /* H */
         140,  158,  180,  197,  223,  253,  283,  313,  341,  385,
         406,  442,  464,  514,  538,  596,  628,  661,  701,  745,
         793,  845,  901,  961,  986, 1054, 1096, 1142, 1222, 1276
    }
};

/* From ISO/IEC 18004:2015 Table 1 - Codeword capacity of all versions of QRCode */
static const unsigned short qr_total_codewords[40] = {
      26,   44,   70,  100,  134,  172,  196,  242,  292,  346,
     404,  466,  532,  581,  655,  733,  815,  901,  991, 1085,
    1156, 1258, 1364, 1474, 1588, 1706, 1828, 1921, 2051, 2185,
    2323, 2465, 2611, 2761, 2876, 3034, 3196, 3362, 3532, 3706
};

/* From ISO/IEC 23941:2022 Table 1 - Codeword capacity of all versions of rMQR symbols */
static const unsigned char rmqr_height[32] = {
     7,  7,  7,  7,  7,
     9,  9,  9,  9,  9,
    11, 11, 11, 11, 11, 11,
    13, 13, 13, 13, 13, 13,
    15, 15, 15, 15, 15,
    17, 17, 17, 17, 17
};

static const unsigned char rmqr_width[32] = {
    43, 59, 77, 99, 139,
    43, 59, 77, 99, 139,
    27, 43, 59, 77,  99, 139,
    27, 43, 59, 77,  99, 139,
    43, 59, 77, 99, 139,
    43, 59, 77, 99, 139
};

/* From ISO/IEC 23941:2022 Table 6 - Number of data codewords and input data capacity for rMQR */
static const unsigned char rmqr_data_codewords[2][32] = { {
         6,  12,  20,  28,  44,      /* R7x */  /* M */
        12,  21,  31,  42,  63,      /* R9x */
         7,  19,  31,  43,  57,  84, /* R11x */
        12,  27,  38,  53,  73, 106, /* R13x */
        33,  48,  67,  88, 127,      /* R15x */
        39,  56,  78, 100, 152       /* R17x */
    }, {
         3,   7,  10,  14,  24,      /* R7x */ /* H */
         7,  11,  17,  22,  33,      /* R9x */
         5,  11,  15,  23,  29,  42, /* R11x */
         7,  13,  20,  29,  35,  54, /* R13x */
        15,  26,  31,  48,  69,      /* R15x */
        21,  28,  38,  56,  76       /* R17x */
    }
};

/* Highest index in `rmqr_total_codewords` for each given row (R7x, R9x etc) */
static const signed char rmqr_fixed_height_upper_bound[7] = {
    -1, 4, 9, 15, 21, 26, 31
};

/* From ISO/IEC 23941:2022 Table 1 - Codeword capacity of all versions of rMQR symbols */
static const unsigned char rmqr_total_codewords[32] = {
    13, 21,  32,  44,  68, /* R7x */
    21, 33,  49,  66,  99, /* R9x */
    15, 31,  47,  67,  89, 132, /* R11x */
    21, 41,  60,  85, 113, 166, /* R13x */
    51, 74, 103, 136, 199, /* R15x */
    61, 88, 122, 160, 232 /* R17x */
};

/* From ISO/IEC 23941:2022 Table 3 - Number of bits of character count indicator */
static const unsigned char rmqr_numeric_cci[32] = {
    4, 5, 6, 7, 7,
    5, 6, 7, 7, 8,
    4, 6, 7, 7, 8, 8,
    5, 6, 7, 7, 8, 8,
    7, 7, 8, 8, 9,
    7, 8, 8, 8, 9
};

static const unsigned char rmqr_alphanum_cci[32] = {
    3, 5, 5, 6, 6,
    5, 5, 6, 6, 7,
    4, 5, 6, 6, 7, 7,
    5, 6, 6, 7, 7, 8,
    6, 7, 7, 7, 8,
    6, 7, 7, 8, 8
};

static const unsigned char rmqr_byte_cci[32] = {
    3, 4, 5, 5, 6,
    4, 5, 5, 6, 6,
    3, 5, 5, 6, 6, 7,
    4, 5, 6, 6, 7, 7,
    6, 6, 7, 7, 7,
    6, 6, 7, 7, 8
};

static const unsigned char rmqr_kanji_cci[32] = {
    2, 3, 4, 5, 5,
    3, 4, 5, 5, 6,
    2, 4, 5, 5, 6, 6,
    3, 5, 5, 6, 6, 7,
    5, 5, 6, 6, 7,
    5, 6, 6, 6, 7
};

/* From ISO/IEC 18004:2015 Table 9 - Error correction characteristics for QR Code */
static const char qr_blocks[4][40] = { {
         1,  1,  1,  1,  1,  2,  2,  2,  2,  4, /* L */
         4,  4,  4,  4,  6,  6,  6,  6,  7,  8,
         8,  9,  9, 10, 12, 12, 12, 13, 14, 15,
        16, 17, 18, 19, 19, 20, 21, 22, 24, 25
    }, {
         1,  1,  1,  2,  2,  4,  4,  4,  5,  5, /* M */
         5,  8,  9,  9, 10, 10, 11, 13, 14, 16,
        17, 17, 18, 20, 21, 23, 25, 26, 28, 29,
        31, 33, 35, 37, 38, 40, 43, 45, 47, 49
    }, {
         1,  1,  2,  2,  4,  4,  6,  6,  8,  8, /* Q */
         8, 10, 12, 16, 12, 17, 16, 18, 21, 20,
        23, 23, 25, 27, 29, 34, 34, 35, 38, 40,
        43, 45, 48, 51, 53, 56, 59, 62, 65, 68
    }, {
         1,  1,  2,  4,  4,  4,  5,  6,  8,  8, /* H */
        11, 11, 16, 16, 18, 16, 19, 21, 25, 25,
        25, 34, 30, 32, 35, 37, 40, 42, 45, 48,
        51, 54, 57, 60, 63, 66, 70, 74, 77, 81
    }
};

/* From ISO/IEC 23941:2022 Table 8 - Error correction characteristics for rMQR */
static const char rmqr_blocks[2][32] = { {
        1, 1, 1, 1, 1,    /* R7x */ /* M */
        1, 1, 1, 1, 2,    /* R9x */
        1, 1, 1, 1, 2, 2, /* R11x */
        1, 1, 1, 2, 2, 3, /* R13x */
        1, 1, 2, 2, 3,    /* R15x */
        1, 2, 2, 3, 4     /* R17x */
    }, {
        1, 1, 1, 1, 2,    /* R7x */ /* H */
        1, 1, 2, 2, 3,    /* R9x */
        1, 1, 2, 2, 2, 3, /* R11x */
        1, 1, 2, 2, 3, 4, /* R13x */
        2, 2, 3, 4, 5,    /* R15x */
        2, 2, 3, 4, 6     /* R17x */
    }
};

/* From ISO/IEC 18004:2015 Table 1 - Codeword capacity of all versions of QR Code */
static const unsigned char qr_sizes[40] = {
     21,  25,  29,  33,  37,  41,  45,  49,  53,  57,
     61,  65,  69,  73,  77,  81,  85,  89,  93,  97,
    101, 105, 109, 113, 117, 121, 125, 129, 133, 137,
    141, 145, 149, 153, 157, 161, 165, 169, 173, 177
};

static const char microqr_sizes[4] = {
    11, 13, 15, 17
};

/* From ISO/IEC 18004:2015 Table 7 - Number of symbol characters and input data capacity for QR Code and
   Table 9 — Error correction characteristics for QR Code, M1-4 only, indexed by ecc_level & version as
   { data bits, data codewords, ecc codewords } */
static const unsigned char microqr_data[3][4][3] = { {
        { 20, 3, 2 }, { 40, 5, 5 }, { 84, 11, 6 }, { 128, 16,  8 } /* L */
    }, {
        {  0, 0, 0 }, { 32, 4, 6 }, { 68,  9, 8 }, { 112, 14, 10 } /* M */
    }, {
        {  0, 0, 0 }, {  0, 0, 0 }, {  0,  0, 0 }, {  80, 10, 14 } /* Q */
    }
};

/* No. of entries in `qr_table_e1` (Table E.1) per version */
static const char qr_align_loopsize[40] = {
    0, 2, 2, 2, 2, 2, 3, 3, 3, 3,
    3, 3, 3, 4, 4, 4, 4, 4, 4, 4,
    5, 5, 5, 5, 5, 5, 5, 6, 6, 6,
    6, 6, 6, 6, 7, 7, 7, 7, 7, 7
};

/* Table E.1 - Row/column coordinates of center module of alignment patterns */
static const unsigned char qr_table_e1[40 * 7] = {
    6, 18,  0,  0,   0,   0,   0,
    6, 22,  0,  0,   0,   0,   0,
    6, 26,  0,  0,   0,   0,   0,
    6, 30,  0,  0,   0,   0,   0,
    6, 34,  0,  0,   0,   0,   0,
    6, 22, 38,  0,   0,   0,   0,
    6, 24, 42,  0,   0,   0,   0,
    6, 26, 46,  0,   0,   0,   0,
    6, 28, 50,  0,   0,   0,   0,
    6, 30, 54,  0,   0,   0,   0,
    6, 32, 58,  0,   0,   0,   0,
    6, 34, 62,  0,   0,   0,   0,
    6, 26, 46, 66,   0,   0,   0,
    6, 26, 48, 70,   0,   0,   0,
    6, 26, 50, 74,   0,   0,   0,
    6, 30, 54, 78,   0,   0,   0,
    6, 30, 56, 82,   0,   0,   0,
    6, 30, 58, 86,   0,   0,   0,
    6, 34, 62, 90,   0,   0,   0,
    6, 28, 50, 72,  94,   0,   0,
    6, 26, 50, 74,  98,   0,   0,
    6, 30, 54, 78, 102,   0,   0,
    6, 28, 54, 80, 106,   0,   0,
    6, 32, 58, 84, 110,   0,   0,
    6, 30, 58, 86, 114,   0,   0,
    6, 34, 62, 90, 118,   0,   0,
    6, 26, 50, 74,  98, 122,   0,
    6, 30, 54, 78, 102, 126,   0,
    6, 26, 52, 78, 104, 130,   0,
    6, 30, 56, 82, 108, 134,   0,
    6, 34, 60, 86, 112, 138,   0,
    6, 30, 58, 86, 114, 142,   0,
    6, 34, 62, 90, 118, 146,   0,
    6, 30, 54, 78, 102, 126, 150,
    6, 24, 50, 76, 102, 128, 154,
    6, 28, 54, 80, 106, 132, 158,
    6, 32, 58, 84, 110, 136, 162,
    6, 26, 54, 82, 110, 138, 166,
    6, 30, 58, 86, 114, 142, 170
};

/* Table D.1 - Column coordinates of centre module of alignment patterns */
static const unsigned char rmqr_table_d1[20] = {
    21,  0,  0,   0,
    19, 39,  0,   0,
    25, 51,  0,   0,
    23, 49, 75,   0,
    27, 55, 83, 111
};

/* Format information bit sequences */
static const unsigned short qr_annex_c[32] = {
    0x5412, 0x5125, 0x5e7c, 0x5b4b, 0x45f9, 0x40ce, 0x4f97, 0x4aa0, 0x77c4, 0x72f3,
    0x7daa, 0x789d, 0x662f, 0x6318, 0x6c41, 0x6976, 0x1689, 0x13be, 0x1ce7, 0x19d0,
    0x0762, 0x0255, 0x0d0c, 0x083b, 0x355f, 0x3068, 0x3f31, 0x3a06, 0x24b4, 0x2183,
    0x2eda, 0x2bed
};

/* Version information bit sequences */
static const unsigned int qr_annex_d[34] = {
    0x07c94, 0x085bc, 0x09a99, 0x0a4d3, 0x0bbf6, 0x0c762, 0x0d847, 0x0e60d, 0x0f928, 0x10b78,
    0x1145d, 0x12a17, 0x13532, 0x149a6, 0x15683, 0x168c9, 0x177ec, 0x18ec4, 0x191e1, 0x1afab,
    0x1b08e, 0x1cc1a, 0x1d33f, 0x1ed75, 0x1f250, 0x209d5, 0x216f0, 0x228ba, 0x2379f, 0x24b0b,
    0x2542e, 0x26a64, 0x27541, 0x28c69
};

/* Micro QR Code format information */
static const unsigned short qr_annex_c1[32] = {
    0x4445, 0x4172, 0x4e2b, 0x4b1c, 0x55ae, 0x5099, 0x5fc0, 0x5af7, 0x6793, 0x62a4,
    0x6dfd, 0x68ca, 0x7678, 0x734f, 0x7c16, 0x7921, 0x06de, 0x03e9, 0x0cb0, 0x0987,
    0x1735, 0x1202, 0x1d5b, 0x186c, 0x2508, 0x203f, 0x2f66, 0x2a51, 0x34e3, 0x31d4,
    0x3e8d, 0x3bba
};

/* rMQR format information for finder pattern side */
static const unsigned int rmqr_format_info_left[64] = {
    0x1FAB2, 0x1E597, 0x1DBDD, 0x1C4F8, 0x1B86C, 0x1A749, 0x19903, 0x18626, 0x17F0E, 0x1602B,
    0x15E61, 0x14144, 0x13DD0, 0x122F5, 0x11CBF, 0x1039A, 0x0F1CA, 0x0EEEF, 0x0D0A5, 0x0CF80,
    0x0B314, 0x0AC31, 0x0927B, 0x08D5E, 0x07476, 0x06B53, 0x05519, 0x04A3C, 0x036A8, 0x0298D,
    0x017C7, 0x008E2, 0x3F367, 0x3EC42, 0x3D208, 0x3CD2D, 0x3B1B9, 0x3AE9C, 0x390D6, 0x38FF3,
    0x376DB, 0x369FE, 0x357B4, 0x34891, 0x33405, 0x32B20, 0x3156A, 0x30A4F, 0x2F81F, 0x2E73A,
    0x2D970, 0x2C655, 0x2BAC1, 0x2A5E4, 0x29BAE, 0x2848B, 0x27DA3, 0x26286, 0x25CCC, 0x243E9,
    0x23F7D, 0x22058, 0x21E12, 0x20137
};

/* rMQR format information for subfinder pattern side */
static const unsigned int rmqr_format_info_right[64] = {
    0x20A7B, 0x2155E, 0x22B14, 0x23431, 0x248A5, 0x25780, 0x269CA, 0x276EF, 0x28FC7, 0x290E2,
    0x2AEA8, 0x2B18D, 0x2CD19, 0x2D23C, 0x2EC76, 0x2F353, 0x30103, 0x31E26, 0x3206C, 0x33F49,
    0x343DD, 0x35CF8, 0x362B2, 0x37D97, 0x384BF, 0x39B9A, 0x3A5D0, 0x3BAF5, 0x3C661, 0x3D944,
    0x3E70E, 0x3F82B, 0x003AE, 0x01C8B, 0x022C1, 0x03DE4, 0x04170, 0x05E55, 0x0601F, 0x07F3A,
    0x08612, 0x09937, 0x0A77D, 0x0B858, 0x0C4CC, 0x0DBE9, 0x0E5A3, 0x0FA86, 0x108D6, 0x117F3,
    0x129B9, 0x1369C, 0x14A08, 0x1552D, 0x16B67, 0x17442, 0x18D6A, 0x1924F, 0x1AC05, 0x1B320,
    0x1CFB4, 0x1D091, 0x1EEDB, 0x1F1FE
};

/* vim: set ts=4 sw=4 et : */
#endif /* Z_QR_H */
