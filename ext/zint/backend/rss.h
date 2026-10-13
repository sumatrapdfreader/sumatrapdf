/* rss.h - Data tables for Reduced Space Symbology */
/*
    libzint - the open source barcode library
    Copyright (C) 2007-2024 Robin Stuart <rstuart114@gmail.com>

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

#ifndef Z_RSS_H
#define Z_RSS_H

/* RSS-14 Tables */
static const unsigned short dbar_g_sum_table[9] = {
    0, 161, 961, 2015, 2715, 0, 336, 1036, 1516
};

static const char dbar_t_table[9] = {
    1, 10, 34, 70, 126, 4, 20, 48, 81
};

static const char dbar_modules_odd[9] = {
    12, 10, 8, 6, 4, 5, 7, 9, 11
};

static const char dbar_modules_even[9] = {
    4, 6, 8, 10, 12, 10, 8, 6, 4
};

static const char dbar_widest_odd[9] = {
    8, 6, 4, 3, 1, 2, 4, 6, 8
};

static const char dbar_widest_even[9] = {
    1, 3, 5, 6, 8, 7, 5, 3, 1
};

static const char dbar_finder_pattern[45] = {
    3, 8, 2, 1, 1,
    3, 5, 5, 1, 1,
    3, 3, 7, 1, 1,
    3, 1, 9, 1, 1,
    2, 7, 4, 1, 1,
    2, 5, 6, 1, 1,
    2, 3, 8, 1, 1,
    1, 5, 7, 1, 1,
    1, 3, 9, 1, 1
};

/* Table 5 */
static const char dbar_checksum_weight[32] = {
     1,  3,  9, 27,  2,  6, 18, 54,
     4, 12, 36, 29,  8, 24, 72, 58,
    16, 48, 65, 37, 32, 17, 51, 74,
    64, 34, 23, 69, 49, 68, 46, 59
};

/* RSS Limited Tables */
static const unsigned short dbar_ltd_t_even[7] = {
    28, 728, 6454, 203, 2408, 1, 16632
};

static const char dbar_ltd_modules_odd[7] = {
    17, 13, 9, 15, 11, 19, 7
};

static const char dbar_ltd_modules_even[7] = {
    9, 13, 17, 11, 15, 7, 19
};

static const char dbar_ltd_widest_odd[7] = {
    6, 5, 3, 5, 4, 8, 1
};

static const char dbar_ltd_widest_even[7] = {
    3, 4, 6, 4, 5, 1, 8
};

/* Table 7 */
static const char dbar_ltd_checksum_weight[28] = {
     1,  3,  9, 27, 81, 65, 17, 51, 64, 14, 42, 37, 22, 66,
    20, 60,  2,  6, 18, 54, 73, 41, 34, 13, 39, 28, 84, 74
};

static const char dbar_ltd_finder_pattern[1246] = {
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 3, 3, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 3, 2, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 3, 3, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 2, 1, 1, 3, 2, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 2, 1, 2, 3, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 3, 1, 1, 3, 1, 1, 1,
    1, 1, 1, 1, 1, 2, 1, 1, 1, 1, 3, 2, 1, 1,
    1, 1, 1, 1, 1, 2, 1, 1, 1, 2, 3, 1, 1, 1,
    1, 1, 1, 1, 1, 2, 1, 2, 1, 1, 3, 1, 1, 1,
    1, 1, 1, 1, 1, 3, 1, 1, 1, 1, 3, 1, 1, 1,
    1, 1, 1, 2, 1, 1, 1, 1, 1, 1, 3, 2, 1, 1,
    1, 1, 1, 2, 1, 1, 1, 1, 1, 2, 3, 1, 1, 1,
    1, 1, 1, 2, 1, 1, 1, 2, 1, 1, 3, 1, 1, 1,
    1, 1, 1, 2, 1, 2, 1, 1, 1, 1, 3, 1, 1, 1,
    1, 1, 1, 3, 1, 1, 1, 1, 1, 1, 3, 1, 1, 1,
    1, 2, 1, 1, 1, 1, 1, 1, 1, 1, 3, 2, 1, 1,
    1, 2, 1, 1, 1, 1, 1, 1, 1, 2, 3, 1, 1, 1,
    1, 2, 1, 1, 1, 1, 1, 2, 1, 1, 3, 1, 1, 1,
    1, 2, 1, 1, 1, 2, 1, 1, 1, 1, 3, 1, 1, 1,
    1, 2, 1, 2, 1, 1, 1, 1, 1, 1, 3, 1, 1, 1,
    1, 3, 1, 1, 1, 1, 1, 1, 1, 1, 3, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 2, 1, 2, 3, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 2, 3, 2, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 2, 2, 1, 2, 2, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 3, 2, 1, 2, 1, 1, 1,
    1, 1, 1, 1, 1, 2, 1, 1, 2, 1, 2, 2, 1, 1,
    1, 1, 1, 1, 1, 2, 1, 1, 2, 2, 2, 1, 1, 1,
    1, 1, 1, 1, 1, 2, 1, 2, 2, 1, 2, 1, 1, 1,
    1, 1, 1, 1, 1, 3, 1, 1, 2, 1, 2, 1, 1, 1,
    1, 1, 1, 2, 1, 1, 1, 1, 2, 1, 2, 2, 1, 1,
    1, 1, 1, 2, 1, 1, 1, 1, 2, 2, 2, 1, 1, 1,
    1, 1, 1, 2, 1, 1, 1, 2, 2, 1, 2, 1, 1, 1,
    1, 1, 1, 2, 1, 2, 1, 1, 2, 1, 2, 1, 1, 1,
    1, 1, 1, 3, 1, 1, 1, 1, 2, 1, 2, 1, 1, 1,
    1, 2, 1, 1, 1, 1, 1, 1, 2, 1, 2, 2, 1, 1,
    1, 2, 1, 1, 1, 1, 1, 1, 2, 2, 2, 1, 1, 1,
    1, 2, 1, 1, 1, 1, 1, 2, 2, 1, 2, 1, 1, 1,
    1, 2, 1, 1, 1, 2, 1, 1, 2, 1, 2, 1, 1, 1,
    1, 2, 1, 2, 1, 1, 1, 1, 2, 1, 2, 1, 1, 1,
    1, 3, 1, 1, 1, 1, 1, 1, 2, 1, 2, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 3, 1, 1, 3, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 3, 2, 1, 2, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 2, 3, 1, 1, 2, 1, 1,
    1, 1, 1, 2, 1, 1, 1, 1, 3, 1, 1, 2, 1, 1,
    1, 2, 1, 1, 1, 1, 1, 1, 3, 1, 1, 2, 1, 1,
    1, 1, 1, 1, 1, 1, 2, 1, 1, 1, 2, 3, 1, 1,
    1, 1, 1, 1, 1, 1, 2, 1, 1, 2, 2, 2, 1, 1,
    1, 1, 1, 1, 1, 1, 2, 1, 1, 3, 2, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 2, 2, 1, 1, 2, 2, 1, 1,
    1, 1, 1, 2, 1, 1, 2, 1, 1, 1, 2, 2, 1, 1,
    1, 1, 1, 2, 1, 1, 2, 1, 1, 2, 2, 1, 1, 1,
    1, 1, 1, 2, 1, 1, 2, 2, 1, 1, 2, 1, 1, 1,
    1, 1, 1, 2, 1, 2, 2, 1, 1, 1, 2, 1, 1, 1,
    1, 1, 1, 3, 1, 1, 2, 1, 1, 1, 2, 1, 1, 1,
    1, 2, 1, 1, 1, 1, 2, 1, 1, 1, 2, 2, 1, 1,
    1, 2, 1, 1, 1, 1, 2, 1, 1, 2, 2, 1, 1, 1,
    1, 2, 1, 2, 1, 1, 2, 1, 1, 1, 2, 1, 1, 1,
    1, 1, 1, 1, 2, 1, 1, 1, 1, 1, 2, 3, 1, 1,
    1, 1, 1, 1, 2, 1, 1, 1, 1, 2, 2, 2, 1, 1,
    1, 1, 1, 1, 2, 1, 1, 1, 1, 3, 2, 1, 1, 1,
    1, 1, 1, 1, 2, 1, 1, 2, 1, 1, 2, 2, 1, 1,
    1, 1, 1, 1, 2, 1, 1, 2, 1, 2, 2, 1, 1, 1,
    1, 1, 1, 1, 2, 2, 1, 1, 1, 1, 2, 2, 1, 1,
    1, 2, 1, 1, 2, 1, 1, 1, 1, 1, 2, 2, 1, 1,
    1, 2, 1, 1, 2, 1, 1, 1, 1, 2, 2, 1, 1, 1,
    1, 2, 1, 1, 2, 1, 1, 2, 1, 1, 2, 1, 1, 1,
    1, 2, 1, 1, 2, 2, 1, 1, 1, 1, 2, 1, 1, 1,
    1, 2, 1, 2, 2, 1, 1, 1, 1, 1, 2, 1, 1, 1,
    1, 3, 1, 1, 2, 1, 1, 1, 1, 1, 2, 1, 1, 1,
    1, 1, 2, 1, 1, 1, 1, 1, 1, 1, 2, 3, 1, 1,
    1, 1, 2, 1, 1, 1, 1, 1, 1, 2, 2, 2, 1, 1,
    1, 1, 2, 1, 1, 1, 1, 1, 1, 3, 2, 1, 1, 1,
    1, 1, 2, 1, 1, 1, 1, 2, 1, 1, 2, 2, 1, 1,
    1, 1, 2, 1, 1, 1, 1, 2, 1, 2, 2, 1, 1, 1,
    1, 1, 2, 1, 1, 1, 1, 3, 1, 1, 2, 1, 1, 1,
    1, 1, 2, 1, 1, 2, 1, 1, 1, 1, 2, 2, 1, 1,
    1, 1, 2, 1, 1, 2, 1, 1, 1, 2, 2, 1, 1, 1,
    1, 1, 2, 2, 1, 1, 1, 1, 1, 1, 2, 2, 1, 1,
    2, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 1, 1,
    2, 1, 1, 1, 1, 1, 1, 1, 1, 3, 2, 1, 1, 1,
    2, 1, 1, 1, 1, 1, 1, 2, 1, 1, 2, 2, 1, 1,
    2, 1, 1, 1, 1, 1, 1, 2, 1, 2, 2, 1, 1, 1,
    2, 1, 1, 1, 1, 1, 1, 3, 1, 1, 2, 1, 1, 1,
    2, 1, 1, 1, 1, 2, 1, 1, 1, 2, 2, 1, 1, 1,
    2, 1, 1, 1, 1, 2, 1, 2, 1, 1, 2, 1, 1, 1,
    2, 1, 1, 2, 1, 1, 1, 1, 1, 2, 2, 1, 1, 1,
    2, 1, 1, 1, 1, 1, 1, 1, 2, 2, 1, 2, 1, 1
};

/* RSS Expanded Tables */
static const unsigned short dbar_exp_g_sum[5] = {
    0, 348, 1388, 2948, 3988
};

static const unsigned char dbar_exp_t_even[5] = {
    4, 20, 52, 104, 204
};

static const char dbar_exp_modules_odd[5] = {
    12, 10, 8, 6, 4
};

static const char dbar_exp_modules_even[5] = {
    5, 7, 9, 11, 13
};

static const char dbar_exp_widest_odd[5] = {
    7, 5, 4, 3, 1
};

static const char dbar_exp_widest_even[5] = {
    2, 4, 5, 6, 8
};

    /* Table 14 */
static const unsigned char dbar_exp_checksum_weight[184] = {
      1,   3,   9,  27,  81,  32,  96,  77,
     20,  60, 180, 118, 143,   7,  21,  63,
    189, 145,  13,  39, 117, 140, 209, 205,
    193, 157,  49, 147,  19,  57, 171,  91,
     62, 186, 136, 197, 169,  85,  44, 132,
    185, 133, 188, 142,   4,  12,  36, 108,
    113, 128, 173,  97,  80,  29,  87,  50,
    150,  28,  84,  41, 123, 158,  52, 156,
     46, 138, 203, 187, 139, 206, 196, 166,
     76,  17,  51, 153,  37, 111, 122, 155,
     43, 129, 176, 106, 107, 110, 119, 146,
     16,  48, 144,  10,  30,  90,  59, 177,
    109, 116, 137, 200, 178, 112, 125, 164,
     70, 210, 208, 202, 184, 130, 179, 115,
    134, 191, 151,  31,  93,  68, 204, 190,
    148,  22,  66, 198, 172,  94,  71,   2,
      6,  18,  54, 162,  64, 192, 154,  40,
    120, 149,  25,  75,  14,  42, 126, 167,
     79,  26,  78,  23,  69, 207, 199, 175,
    103,  98,  83,  38, 114, 131, 182, 124,
    161,  61, 183, 127, 170,  88,  53, 159,
     55, 165,  73,   8,  24,  72,   5,  15,
     45, 135, 194, 160,  58, 174, 100,  89
};

/* Table 15 */
static const char dbar_exp_finder_pattern[60] = {
    1, 8, 4, 1, 1,
    1, 1, 4, 8, 1,
    3, 6, 4, 1, 1,
    1, 1, 4, 6, 3,
    3, 4, 6, 1, 1,
    1, 1, 6, 4, 3,
    3, 2, 8, 1, 1,
    1, 1, 8, 2, 3,
    2, 6, 5, 1, 1,
    1, 1, 5, 6, 2,
    2, 2, 9, 1, 1,
    1, 1, 9, 2, 2
};

/* Table 16 */
static const char dbar_exp_finder_sequence[198] = {
    1,  2, 0, 0, 0,  0,  0,  0,  0,  0,  0,
    1,  4, 3, 0, 0,  0,  0,  0,  0,  0,  0,
    1,  6, 3, 8, 0,  0,  0,  0,  0,  0,  0,
    1, 10, 3, 8, 5,  0,  0,  0,  0,  0,  0,
    1, 10, 3, 8, 7, 12,  0,  0,  0,  0,  0,
    1, 10, 3, 8, 9, 12, 11,  0,  0,  0,  0,
    1,  2, 3, 4, 5,  6,  7,  8,  0,  0,  0,
    1,  2, 3, 4, 5,  6,  7, 10,  9,  0,  0,
    1,  2, 3, 4, 5,  6,  7, 10, 11, 12,  0,
    1,  2, 3, 4, 5,  8,  7, 10,  9, 12, 11
};

static const char dbar_exp_weight_rows[210] = {
    0,  1,  2, 0, 0,  0, 0,   0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  5,  6, 3, 4,  0, 0,   0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  9, 10, 3, 4, 13, 14,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0, 17, 18, 3, 4, 13, 14,  7,  8,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0, 17, 18, 3, 4, 13, 14, 11, 12, 21, 22,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0, 17, 18, 3, 4, 13, 14, 15, 16, 21, 22, 19, 20,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  1,  2, 3, 4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14,  0,  0,  0,  0,  0,  0,
    0,  1,  2, 3, 4,  5,  6,  7,  8,  9, 10, 11, 12, 17, 18, 15, 16,  0,  0,  0,  0,
    0,  1,  2, 3, 4,  5,  6,  7,  8,  9, 10, 11, 12, 17, 18, 19, 20, 21, 22,  0,  0,
    0,  1,  2, 3, 4,  5,  6,  7,  8, 13, 14, 11, 12, 17, 18, 15, 16, 21, 22, 19, 20
};

/* vim: set ts=4 sw=4 et : */
#endif /* Z_RSS_H */
