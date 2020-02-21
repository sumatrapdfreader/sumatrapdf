/* Copyright (C) 2001-2019 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  1305 Grant Avenue - Suite 200, Novato,
   CA 94945, U.S.A., +1(415)492-9861, for further information.
*/

/*
    jbig2dec
*/

/* predefined Huffman table definitions
    -- See Annex B of the JBIG2 specification */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "os_types.h"

#include <stdlib.h>

#include "jbig2.h"
#include "jbig2_priv.h"
#include "jbig2_huffman.h"
#include "jbig2_hufftab.h"

#define JBIG2_COUNTOF(x) (sizeof((x)) / sizeof((x)[0]))

/* Table B.1 */
static const Jbig2HuffmanLine jbig2_huffman_lines_A[] = {
    {1, 4, 0},
    {2, 8, 16},
    {3, 16, 272},
    {0, 32, -1},                /* low */
    {3, 32, 65808}              /* high */
};

const Jbig2HuffmanParams jbig2_huffman_params_A = { FALSE, JBIG2_COUNTOF(jbig2_huffman_lines_A), jbig2_huffman_lines_A };

/* Table B.2 */
static const Jbig2HuffmanLine jbig2_huffman_lines_B[] = {
    {1, 0, 0},
    {2, 0, 1},
    {3, 0, 2},
    {4, 3, 3},
    {5, 6, 11},
    {0, 32, -1},                /* low */
    {6, 32, 75},                /* high */
    {6, 0, 0}                   /* OOB */
};

const Jbig2HuffmanParams jbig2_huffman_params_B = { TRUE, JBIG2_COUNTOF(jbig2_huffman_lines_B), jbig2_huffman_lines_B };

/* Table B.3 */
static const Jbig2HuffmanLine jbig2_huffman_lines_C[] = {
    {8, 8, -256},
    {1, 0, 0},
    {2, 0, 1},
    {3, 0, 2},
    {4, 3, 3},
    {5, 6, 11},
    {8, 32, -257},              /* low */
    {7, 32, 75},                /* high */
    {6, 0, 0}                   /* OOB */
};

const Jbig2HuffmanParams jbig2_huffman_params_C = { TRUE, JBIG2_COUNTOF(jbig2_huffman_lines_C), jbig2_huffman_lines_C };

/* Table B.4 */
static const Jbig2HuffmanLine jbig2_huffman_lines_D[] = {
    {1, 0, 1},
    {2, 0, 2},
    {3, 0, 3},
    {4, 3, 4},
    {5, 6, 12},
    {0, 32, -1},                /* low */
    {5, 32, 76},                /* high */
};

const Jbig2HuffmanParams jbig2_huffman_params_D = { FALSE, JBIG2_COUNTOF(jbig2_huffman_lines_D), jbig2_huffman_lines_D };

/* Table B.5 */
static const Jbig2HuffmanLine jbig2_huffman_lines_E[] = {
    {7, 8, -255},
    {1, 0, 1},
    {2, 0, 2},
    {3, 0, 3},
    {4, 3, 4},
    {5, 6, 12},
    {7, 32, -256},              /* low */
    {6, 32, 76}                 /* high */
};

const Jbig2HuffmanParams jbig2_huffman_params_E = { FALSE, JBIG2_COUNTOF(jbig2_huffman_lines_E), jbig2_huffman_lines_E };

/* Table B.6 */
static const Jbig2HuffmanLine jbig2_huffman_lines_F[] = {
    {5, 10, -2048},
    {4, 9, -1024},
    {4, 8, -512},
    {4, 7, -256},
    {5, 6, -128},
    {5, 5, -64},
    {4, 5, -32},
    {2, 7, 0},
    {3, 7, 128},
    {3, 8, 256},
    {4, 9, 512},
    {4, 10, 1024},
    {6, 32, -2049},             /* low */
    {6, 32, 2048}               /* high */
};

const Jbig2HuffmanParams jbig2_huffman_params_F = { FALSE, JBIG2_COUNTOF(jbig2_huffman_lines_F), jbig2_huffman_lines_F };

/* Table B.7 */
static const Jbig2HuffmanLine jbig2_huffman_lines_G[] = {
    {4, 9, -1024},
    {3, 8, -512},
    {4, 7, -256},
    {5, 6, -128},
    {5, 5, -64},
    {4, 5, -32},
    {4, 5, 0},
    {5, 5, 32},
    {5, 6, 64},
    {4, 7, 128},
    {3, 8, 256},
    {3, 9, 512},
    {3, 10, 1024},
    {5, 32, -1025},             /* low */
    {5, 32, 2048}               /* high */
};

const Jbig2HuffmanParams jbig2_huffman_params_G = { FALSE, JBIG2_COUNTOF(jbig2_huffman_lines_G), jbig2_huffman_lines_G };

/* Table B.8 */
static const Jbig2HuffmanLine jbig2_huffman_lines_H[] = {
    {8, 3, -15},
    {9, 1, -7},
    {8, 1, -5},
    {9, 0, -3},
    {7, 0, -2},
    {4, 0, -1},
    {2, 1, 0},
    {5, 0, 2},
    {6, 0, 3},
    {3, 4, 4},
    {6, 1, 20},
    {4, 4, 22},
    {4, 5, 38},
    {5, 6, 70},
    {5, 7, 134},
    {6, 7, 262},
    {7, 8, 390},
    {6, 10, 646},
    {9, 32, -16},               /* low */
    {9, 32, 1670},              /* high */
    {2, 0, 0}                   /* OOB */
};

const Jbig2HuffmanParams jbig2_huffman_params_H = { TRUE, JBIG2_COUNTOF(jbig2_huffman_lines_H), jbig2_huffman_lines_H };

/* Table B.9 */
static const Jbig2HuffmanLine jbig2_huffman_lines_I[] = {
    {8, 4, -31},
    {9, 2, -15},
    {8, 2, -11},
    {9, 1, -7},
    {7, 1, -5},
    {4, 1, -3},
    {3, 1, -1},
    {3, 1, 1},
    {5, 1, 3},
    {6, 1, 5},
    {3, 5, 7},
    {6, 2, 39},
    {4, 5, 43},
    {4, 6, 75},
    {5, 7, 139},
    {5, 8, 267},
    {6, 8, 523},
    {7, 9, 779},
    {6, 11, 1291},
    {9, 32, -32},               /* low */
    {9, 32, 3339},              /* high */
    {2, 0, 0}                   /* OOB */
};

const Jbig2HuffmanParams jbig2_huffman_params_I = { TRUE, JBIG2_COUNTOF(jbig2_huffman_lines_I), jbig2_huffman_lines_I };

/* Table B.10 */
static const Jbig2HuffmanLine jbig2_huffman_lines_J[] = {
    {7, 4, -21},
    {8, 0, -5},
    {7, 0, -4},
    {5, 0, -3},
    {2, 2, -2},
    {5, 0, 2},
    {6, 0, 3},
    {7, 0, 4},
    {8, 0, 5},
    {2, 6, 6},
    {5, 5, 70},
    {6, 5, 102},
    {6, 6, 134},
    {6, 7, 198},
    {6, 8, 326},
    {6, 9, 582},
    {6, 10, 1094},
    {7, 11, 2118},
    {8, 32, -22},               /* low */
    {8, 32, 4166},              /* high */
    {2, 0, 0}                   /* OOB */
};

const Jbig2HuffmanParams jbig2_huffman_params_J = { TRUE, JBIG2_COUNTOF(jbig2_huffman_lines_J), jbig2_huffman_lines_J };

/* Table B.11 */
static const Jbig2HuffmanLine jbig2_huffman_lines_K[] = {
    {1, 0, 1},
    {2, 1, 2},
    {4, 0, 4},
    {4, 1, 5},
    {5, 1, 7},
    {5, 2, 9},
    {6, 2, 13},
    {7, 2, 17},
    {7, 3, 21},
    {7, 4, 29},
    {7, 5, 45},
    {7, 6, 77},
    {0, 32, -1},                /* low */
    {7, 32, 141}                /* high */
};

const Jbig2HuffmanParams jbig2_huffman_params_K = { FALSE, JBIG2_COUNTOF(jbig2_huffman_lines_K), jbig2_huffman_lines_K };

/* Table B.12 */
static const Jbig2HuffmanLine jbig2_huffman_lines_L[] = {
    {1, 0, 1},
    {2, 0, 2},
    {3, 1, 3},
    {5, 0, 5},
    {5, 1, 6},
    {6, 1, 8},
    {7, 0, 10},
    {7, 1, 11},
    {7, 2, 13},
    {7, 3, 17},
    {7, 4, 25},
    {8, 5, 41},
    {8, 32, 73},
    {0, 32, -1},                /* low */
    {0, 32, 0}                  /* high */
};

const Jbig2HuffmanParams jbig2_huffman_params_L = { FALSE, JBIG2_COUNTOF(jbig2_huffman_lines_L), jbig2_huffman_lines_L };

/* Table B.13 */
static const Jbig2HuffmanLine jbig2_huffman_lines_M[] = {
    {1, 0, 1},
    {3, 0, 2},
    {4, 0, 3},
    {5, 0, 4},
    {4, 1, 5},
    {3, 3, 7},
    {6, 1, 15},
    {6, 2, 17},
    {6, 3, 21},
    {6, 4, 29},
    {6, 5, 45},
    {7, 6, 77},
    {0, 32, -1},                /* low */
    {7, 32, 141}                /* high */
};

const Jbig2HuffmanParams jbig2_huffman_params_M = { FALSE, JBIG2_COUNTOF(jbig2_huffman_lines_M), jbig2_huffman_lines_M };

/* Table B.14 */
static const Jbig2HuffmanLine jbig2_huffman_lines_N[] = {
    {3, 0, -2},
    {3, 0, -1},
    {1, 0, 0},
    {3, 0, 1},
    {3, 0, 2},
    {0, 32, -1},                /* low */
    {0, 32, 3},                 /* high */
};

const Jbig2HuffmanParams jbig2_huffman_params_N = { FALSE, JBIG2_COUNTOF(jbig2_huffman_lines_N), jbig2_huffman_lines_N };

/* Table B.15 */
static const Jbig2HuffmanLine jbig2_huffman_lines_O[] = {
    {7, 4, -24},
    {6, 2, -8},
    {5, 1, -4},
    {4, 0, -2},
    {3, 0, -1},
    {1, 0, 0},
    {3, 0, 1},
    {4, 0, 2},
    {5, 1, 3},
    {6, 2, 5},
    {7, 4, 9},
    {7, 32, -25},               /* low */
    {7, 32, 25}                 /* high */
};

const Jbig2HuffmanParams jbig2_huffman_params_O = { FALSE, JBIG2_COUNTOF(jbig2_huffman_lines_O), jbig2_huffman_lines_O };
