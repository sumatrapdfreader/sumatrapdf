/* pdf417.c - Handles PDF417 stacked symbology */
/*
    libzint - the open source barcode library
    Copyright (C) 2008-2024 Robin Stuart <rstuart114@gmail.com>
    Portions Copyright (C) 2004 Grandzebu
    Bug Fixes thanks to KL Chin <klchin@users.sourceforge.net>

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

/*  This code is adapted from "Code barre PDF 417 / PDF 417 barcode" v2.5.0
    which is Copyright (C) 2004 (Grandzebu).
    The original code (file pdf417.frm) can be downloaded from https://grandzebu.net/informatique/codbar/pdf417.zip */

/* NOTE: symbol->option_1 is used to specify the security level (i.e. control the
   number of check codewords)

   symbol->option_2 is used to adjust the width of the resulting symbol (i.e. the
   number of codeword columns not including row start and end data)

   symbol->option_3 is used to adjust the rows of the resulting symbol */

#include <assert.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include "common.h"
#include "pdf417.h"
#include "pdf417_tabs.h"

/* Modes */
#define PDF_ALP         1 /* Treating TEX sub-modes as pseudo-modes (minimal encode) */
#define PDF_LOW         2
#define PDF_MIX         3
#define PDF_PNC         4
#define PDF_TEX         4 /* Real modes */
#define PDF_BYT         5
#define PDF_NUM         6

#define PDF_NUM_MODES   6

/* Mode indicators including TEX pseudo-modes */
static const char pdf_smodes[] = { '?', 'A', 'L', 'M', 'P', 'B', 'N' };

/* Return (real) mode text */
static const char *pdf_mode_str(const int mode) {
    static const char modes[3][7] = { "Text", "Byte", "Number" };
    return mode >= PDF_TEX && mode <= PDF_NUM ? modes[mode - PDF_TEX] : "ERROR";
}

#define PDF_REAL_MODE(m) ((m) <= PDF_TEX ? PDF_TEX : (m))

/* TEX mode OR-able sub-modes (tables) */
#define T_ALPHA   1
#define T_LOWER   2
#define T_MIXED   4
#define T_PUNCT   8

#define T_ALWMX   (T_ALPHA | T_LOWER | T_MIXED)
#define T_MXPNC   (T_MIXED | T_PUNCT)

#define PDF_TABLE_TO_MODE(t) (((t) >> 1) + !!((t) & 0x07)) /* Hack to map 1,2,4,8 to 1,2,3,4 */

/*
   Three figure numbers in comments give the location of command equivalents in the
   original Visual Basic source code file pdf417.frm
   this code retains some original (French) procedure and variable names to ease conversion */

/* text mode processing tables */

/* TEX sub-mode assignments */
static const char pdf_asciix[256] = {
          0,       0,       0,       0,       0,       0,       0,       0, /* 00-07 */
          0, T_MXPNC, T_PUNCT,       0,       0, T_MXPNC,       0,       0, /* 08-0F .<HT><LF>..<CR>.. */
          0,       0,       0,       0,       0,       0,       0,       0, /* 10-17 */
          0,       0,       0,       0,       0,       0,       0,       0, /* 18-1F */
    T_ALWMX, T_PUNCT, T_PUNCT, T_MIXED, T_MXPNC, T_MIXED, T_MIXED, T_PUNCT, /* 20-27 <SP>!"#$%&' */
    T_PUNCT, T_PUNCT, T_MXPNC, T_MIXED, T_MXPNC, T_MXPNC, T_MXPNC, T_MXPNC, /* 28-2F ()*+,-./ */
    T_MIXED, T_MIXED, T_MIXED, T_MIXED, T_MIXED, T_MIXED, T_MIXED, T_MIXED, /* 30-37 01234567 */
    T_MIXED, T_MIXED, T_MXPNC, T_PUNCT, T_PUNCT, T_MIXED, T_PUNCT, T_PUNCT, /* 38-3F 89:;<=>? */
    T_PUNCT, T_ALPHA, T_ALPHA, T_ALPHA, T_ALPHA, T_ALPHA, T_ALPHA, T_ALPHA, /* 40-47 @ABCDEFG */
    T_ALPHA, T_ALPHA, T_ALPHA, T_ALPHA, T_ALPHA, T_ALPHA, T_ALPHA, T_ALPHA, /* 48-4F HIJKLMNO */
    T_ALPHA, T_ALPHA, T_ALPHA, T_ALPHA, T_ALPHA, T_ALPHA, T_ALPHA, T_ALPHA, /* 50-57 PQRSTUVW */
    T_ALPHA, T_ALPHA, T_ALPHA, T_PUNCT, T_PUNCT, T_PUNCT, T_MIXED, T_PUNCT, /* 58-5F XYZ[\]^_ */
    T_PUNCT, T_LOWER, T_LOWER, T_LOWER, T_LOWER, T_LOWER, T_LOWER, T_LOWER, /* 60-67 `abcdefg */
    T_LOWER, T_LOWER, T_LOWER, T_LOWER, T_LOWER, T_LOWER, T_LOWER, T_LOWER, /* 68-6F hijklmno */
    T_LOWER, T_LOWER, T_LOWER, T_LOWER, T_LOWER, T_LOWER, T_LOWER, T_LOWER, /* 70-77 pqrstuvw */
    T_LOWER, T_LOWER, T_LOWER, T_PUNCT, T_PUNCT, T_PUNCT, T_PUNCT,       0, /* 78-7E xyz{|}~D */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /*80-9F*/
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /*A0-BF*/
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /*C0-DF*/
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /*E0-FF*/
};

/* TEX sub-mode values */
static const char pdf_asciiy[127] = {
     0,  0,  0,  0,  0,  0,  0,  0, /* 00-07 */
     0, 12, 15,  0,  0, 11,  0,  0, /* 08-0F */
     0,  0,  0,  0,  0,  0,  0,  0, /* 10-17 */
     0,  0,  0,  0,  0,  0,  0,  0, /* 18-1F */
    26, 10, 20, 15, 18, 21, 10, 28, /* 20-27 */
    23, 24, 22, 20, 13, 16, 17, 19, /* 28-2F */
     0,  1,  2,  3,  4,  5,  6,  7, /* 30-37 */
     8,  9, 14,  0,  1, 23,  2, 25, /* 38-3F */
     3,  0,  1,  2,  3,  4,  5,  6, /* 40-47 */
     7,  8,  9, 10, 11, 12, 13, 14, /* 48-4F */
    15, 16, 17, 18, 19, 20, 21, 22, /* 50-57 */
    23, 24, 25,  4,  5,  6, 24,  7, /* 58-5F */
     8,  0,  1,  2,  3,  4,  5,  6, /* 60-67 */
     7,  8,  9, 10, 11, 12, 13, 14, /* 68-6F */
    15, 16, 17, 18, 19, 20, 21, 22, /* 70-77 */
    23, 24, 25, 26, 21, 27,  9      /* 78-7F */
};

/* Automatic sizing table */

static const char pdf_MicroAutosize[56] = {
    4, 6, 7, 8, 10, 12, 13, 14, 16, 18, 19, 20, 24, 29, 30, 33, 34, 37, 39, 46, 54, 58, 70, 72, 82, 90, 108, 126,
    1, 14, 2, 7, 3, 25, 8, 16, 5, 17, 9, 6, 10, 11, 28, 12, 19, 13, 29, 20, 30, 21, 22, 31, 23, 32, 33, 34
};

/* ISO/IEC 15438:2015 5.1.1 c) 3) Max possible number of characters at error correction level 0
   (Numeric Compaction mode) */
#define PDF_MAX_LEN         2710
#define PDF_MAX_LEN_S       "2710" /* String version of above */
#define PDF_MAX_STREAM_LEN  (PDF_MAX_LEN * 3) /* Allow for tripling up due to shifts/latches (ticket #300 (#7)) */

/* ISO/IEC 24728:2006 5.1.1 c) 3) Max possible number of characters (Numeric Compaction mode) */
#define MICRO_PDF_MAX_LEN   366
#define MICRO_PDF_MAX_LEN_S "366" /* String version of above */

/* 866 */
/* Initial non-compressed categorization of input */
static int pdf_quelmode(const unsigned char codeascii) {
    if (z_isdigit(codeascii)) {
        return PDF_NUM;
    }
    if (pdf_asciix[codeascii]) {
        return PDF_TEX;
    }
    /* 876 */

    return PDF_BYT;
}

/* Helper to switch TEX mode sub-mode */
static int pdf_textprocess_switch(const int curtable, const int newtable, unsigned char chainet[PDF_MAX_STREAM_LEN],
            int wnet) {
    switch (curtable) {
        case T_ALPHA:
            switch (newtable) {
                case T_LOWER: chainet[wnet++] = 27; /* LL */
                    break;
                case T_MIXED: chainet[wnet++] = 28; /* ML */
                    break;
                case T_PUNCT: chainet[wnet++] = 28; /* ML+PL */
                    chainet[wnet++] = 25;
                    break;
            }
            break;
        case T_LOWER:
            switch (newtable) {
                case T_ALPHA: chainet[wnet++] = 28; /* ML+AL */
                    chainet[wnet++] = 28;
                    break;
                case T_MIXED: chainet[wnet++] = 28; /* ML */
                    break;
                case T_PUNCT: chainet[wnet++] = 28; /* ML+PL */
                    chainet[wnet++] = 25;
                    break;
            }
            break;
        case T_MIXED:
            switch (newtable) {
                case T_ALPHA: chainet[wnet++] = 28; /* AL */
                    break;
                case T_LOWER: chainet[wnet++] = 27; /* LL */
                    break;
                case T_PUNCT: chainet[wnet++] = 25; /* PL */
                    break;
            }
            break;
        case T_PUNCT:
            switch (newtable) {
                case T_ALPHA: chainet[wnet++] = 29; /* AL */
                    break;
                case T_LOWER: chainet[wnet++] = 29; /* AL+LL */
                    chainet[wnet++] = 27;
                    break;
                case T_MIXED: chainet[wnet++] = 29; /* AL+ML */
                    chainet[wnet++] = 28;
                    break;
            }
            break;
    }

    return wnet;
}

/* Check consecutive segments for text/num and return the length */
static int pdf_text_num_length(short liste[3][PDF_MAX_LEN], const int indexliste, const int start) {
    int i, len = 0;
    for (i = start; i < indexliste; i++) {
        if (liste[1][i] == PDF_BYT)
            break;

        len += liste[0][i];
        if (len >= 5) /* we don't care if it's longer than 5 */
            break;
    }

    return len;
}

/* Calculate length of TEX allowing for sub-mode switches (no-output version of `pdf_textprocess()`) */
static int pdf_text_submode_length(const unsigned char chaine[], const int start, const int length, int *p_curtable) {
    int j, indexlistet, curtable = *p_curtable, listet[PDF_MAX_LEN], wnet = 0;
    unsigned char chainet[PDF_MAX_STREAM_LEN];

    for (indexlistet = 0; indexlistet < length; indexlistet++) {
        assert(pdf_asciix[chaine[start + indexlistet]]); /* Should only be dealing with TEX */
        listet[indexlistet] = pdf_asciix[chaine[start + indexlistet]];
    }

    for (j = 0; j < length; j++) {
        if (listet[j] & curtable) {
            /* The character is in the current table */
            wnet++;
        } else {
            /* Obliged to change table */
            int newtable;
            if (j == (length - 1) || !(listet[j] & listet[j + 1])) {
                /* we change only one character - look for temporary switch */
                if ((listet[j] & T_ALPHA) && (curtable == T_LOWER)) {
                    wnet += 2; /* AS+char */
                    continue;
                }
                if (listet[j] & T_PUNCT) { /* (T_PUNCT and T_ALPHA not both possible) */
                    wnet += 2; /* PS+char */
                    continue;
                }
                /* No temporary switch available */
                newtable = listet[j];
            } else {
                newtable = listet[j] & listet[j + 1];
            }

            /* 599 */

            /* Maintain the first if several tables are possible */
            if (newtable == T_ALWMX) { /* (T_ALPHA | T_LOWER | T_MIXED) */
                newtable = T_ALPHA;
            } else if (newtable == T_MXPNC) { /* (T_MIXED | T_PUNCT) */
                newtable = T_MIXED;
            }

            /* 619 - select the switch */
            wnet = pdf_textprocess_switch(curtable, newtable, chainet, wnet);
            curtable = newtable;
            /* 659 - at last we add the character */
            wnet++;
        }
    }

    *p_curtable = curtable;

    return wnet;
}

/* Whether to stay in numeric mode or not */
static int pdf_num_stay(const unsigned char *chaine, const int indexliste, short liste[3][PDF_MAX_LEN], const int i) {
    int curtable, not_tex, last_len, last_ml, next_len, num_cws, tex_cws;

    if (liste[0][i] >= 13 || (indexliste == 1 && liste[0][i] > 5)) {
        return 1;
    }
    if (liste[0][i] < 11) {
        return 0;
    }

    curtable = T_ALPHA;
    not_tex = i == 0 || liste[1][i - 1] == PDF_BYT;
    last_len = not_tex ? 0 : pdf_text_submode_length(chaine, liste[2][i - 1], liste[0][i - 1], &curtable);
    last_ml = curtable == T_MIXED;

    curtable = T_ALPHA; /* Next len if after NUM, sub-mode will be alpha */
    not_tex = i == indexliste - 1 || liste[1][i + 1] == PDF_BYT;
    next_len = not_tex ? 0 : pdf_text_submode_length(chaine, liste[2][i + 1], liste[0][i + 1], &curtable);
    num_cws = ((last_len + 1) >> 1) + 1 + 4 + (liste[0][i] > 11) + 1 + ((next_len + 1) >> 1);

    curtable = T_MIXED; /* Next len if stay TEX, sub-mode will be mixed */
    next_len = not_tex ? 0 : pdf_text_submode_length(chaine, liste[2][i + 1], liste[0][i + 1], &curtable);
    tex_cws = (last_len + !last_ml + liste[0][i] + next_len + 1) >> 1;

    if (num_cws > tex_cws) {
        return 0;
    }
    return 1;
}

/* Pack segments using the method described in Appendix D of the AIM specification (ISO/IEC 15438:2015 Annex N) */
static void pdf_appendix_d_encode(const unsigned char *chaine, short liste[3][PDF_MAX_LEN], int *p_indexliste) {
    const int indexliste = *p_indexliste;
    int i = 0, next, last = 0, stayintext = 0;

    while (i < indexliste) {

        if ((liste[1][i] == PDF_NUM) && pdf_num_stay(chaine, indexliste, liste, i)) {
            /* leave as numeric */
            liste[0][last] = liste[0][i];
            liste[1][last] = PDF_NUM;
            liste[2][last] = liste[2][i];
            stayintext = 0;
            last++;
        } else if (((liste[1][i] == PDF_TEX) || (liste[1][i] == PDF_NUM))
                && (stayintext || i == indexliste - 1 || (liste[0][i] >= 5)
                    || (pdf_text_num_length(liste, indexliste, i) >= 5))) {
            /* set to text and combine additional text/short numeric segments */
            liste[0][last] = liste[0][i];
            liste[1][last] = PDF_TEX;
            liste[2][last] = liste[2][i];
            stayintext = 0;

            next = i + 1;
            while (next < indexliste) {
                if ((liste[1][next] == PDF_NUM) && pdf_num_stay(chaine, indexliste, liste, next)) {
                    break;
                } else if (liste[1][next] == PDF_BYT) {
                    break;
                }

                liste[0][last] += liste[0][next];
                next++;
            }

            last++;
            i = next;
            continue;
        } else {
            /* build byte segment, including combining numeric/text segments that aren't long enough on their own */
            liste[0][last] = liste[0][i];
            liste[1][last] = PDF_BYT;
            liste[2][last] = liste[2][i];
            stayintext = 0;

            next = i + 1;
            while (next < indexliste) {
                if (liste[1][next] != PDF_BYT) {
                    /* check for case of a single byte shift from text mode */
                    if ((liste[0][last] == 1) && (last > 0) && (liste[1][last - 1] == PDF_TEX)) {
                        stayintext = 1;
                        break;
                    }

                    if ((liste[0][next] >= 5) || (pdf_text_num_length(liste, indexliste, next) >= 5)) {
                        break;
                    }
                }

                liste[0][last] += liste[0][next];
                next++;
            }

            last++;
            i = next;
            continue;
        }

        i++;
    }

    /* set the size of the list based on the last consolidated segment */
    *p_indexliste = last;
}

/* Helper to pad TEX mode, allowing for whether last segment or not, writing out `chainet` */
static void pdf_textprocess_end(short *chainemc, int *p_mclength, const int is_last_seg,
            unsigned char chainet[PDF_MAX_STREAM_LEN], int wnet, int *p_curtable, int *p_tex_padded) {
    int i;

    *p_tex_padded = wnet & 1;
    if (*p_tex_padded) {
        if (is_last_seg) {
            chainet[wnet++] = 29; /* PS or AL */
        } else { /* Can't use PS as may carry over to following segment */
            /* This is sub-optimal if curtable T_ALPHA and following seg lower, or if curtable T_MIXED
               and following seg is lower or punct; TODO: need peek-ahead to table of following seg */
            chainet[wnet++] = 28 + (*p_curtable == T_PUNCT); /* ML (T_ALPHA/T_LOWER) or AL (T_MIXED/T_PUNCT) */
            *p_curtable = *p_curtable == T_ALPHA || *p_curtable == T_LOWER ? T_MIXED : T_ALPHA;
        }
    }

    /* Now translate the string chainet into codewords */

    for (i = 0; i < wnet; i += 2) {
        chainemc[(*p_mclength)++] = (30 * chainet[i]) + chainet[i + 1];
    }
}

/* 547 */
/* Text compaction */
static void pdf_textprocess(short *chainemc, int *p_mclength, const unsigned char chaine[], const int start,
            const int length, const int lastmode, const int is_last_seg, int *p_curtable, int *p_tex_padded) {
    const int real_lastmode = PDF_REAL_MODE(lastmode);
    int j, indexlistet;
    int curtable = real_lastmode == PDF_TEX ? *p_curtable : T_ALPHA; /* Set default table upper alpha */
    int listet[2][PDF_MAX_LEN] = {{0}};
    unsigned char chainet[PDF_MAX_STREAM_LEN];
    int wnet = 0;

    /* add mode indicator if needed */
    if (real_lastmode != PDF_TEX) {
        chainemc[(*p_mclength)++] = 900;
    }

    /* listet will contain the table numbers and the value of each characters */
    for (indexlistet = 0; indexlistet < length; indexlistet++) {
        const int codeascii = chaine[start + indexlistet];
        listet[0][indexlistet] = pdf_asciix[codeascii];
        listet[1][indexlistet] = pdf_asciiy[codeascii];
    }

    /* 570 */

    for (j = 0; j < length; j++) {
        if (listet[0][j] & curtable) {
            /* The character is in the current table */
            chainet[wnet++] = listet[1][j];
        } else {
            /* Obliged to change table */
            int newtable;
            if (j == (length - 1) || !(listet[0][j] & listet[0][j + 1])) {
                /* we change only one character - look for temporary switch */
                if ((listet[0][j] & T_ALPHA) && (curtable == T_LOWER)) {
                    chainet[wnet++] = 27; /* AS */
                    chainet[wnet++] = listet[1][j];
                    continue;
                }
                if (listet[0][j] & T_PUNCT) { /* (T_PUNCT and T_ALPHA not both possible) */
                    chainet[wnet++] = 29; /* PS */
                    chainet[wnet++] = listet[1][j];
                    continue;
                }
                /* No temporary switch available */
                newtable = listet[0][j];
            } else {
                newtable = listet[0][j] & listet[0][j + 1];
            }

            /* 599 */

            /* Maintain the first if several tables are possible */
            if (newtable == T_ALWMX) { /* (T_ALPHA | T_LOWER | T_MIXED) */
                newtable = T_ALPHA;
            } else if (newtable == T_MXPNC) { /* (T_MIXED | T_PUNCT) */
                newtable = T_MIXED;
            }

            /* 619 - select the switch */
            wnet = pdf_textprocess_switch(curtable, newtable, chainet, wnet);
            curtable = newtable;
            /* 659 - at last we add the character */
            chainet[wnet++] = listet[1][j];
        }
    }

    /* 663 */
    *p_curtable = curtable;
    pdf_textprocess_end(chainemc, p_mclength, is_last_seg, chainet, wnet, p_curtable, p_tex_padded);
}

/* Minimal text compaction */
static void pdf_textprocess_minimal(short *chainemc, int *p_mclength, const unsigned char chaine[],
            short liste[3][PDF_MAX_LEN], const int indexliste, const int lastmode, const int is_last_seg,
            int *p_curtable, int *p_tex_padded, int *p_i) {
    const int real_lastmode = PDF_REAL_MODE(lastmode);
    int i, j, k;
    int curtable = real_lastmode == PDF_TEX ? *p_curtable : T_ALPHA; /* Set default table upper alpha */
    unsigned char chainet[PDF_MAX_STREAM_LEN];
    int wnet = 0;

    /* add mode indicator if needed */
    if (real_lastmode != PDF_TEX) {
        chainemc[(*p_mclength)++] = 900;
    }

    for (i = *p_i; i < indexliste && PDF_REAL_MODE(liste[1][i]) == PDF_TEX; i++) {
        static const unsigned char newtables[5] = { 0, T_ALPHA, T_LOWER, T_MIXED, T_PUNCT };
        const int newtable = newtables[liste[1][i]];
        const int from = liste[2][i];
        for (j = 0; j < liste[0][i]; j++) {
            const int c = chaine[from + j];
            const int t_table = pdf_asciix[c];
            if (!t_table) { /* BYT Shift? */
                if (wnet & 1) {
                    chainet[wnet++] = 29; /* PS or AL (T_PUNCT) */
                    if (curtable == T_PUNCT) {
                        curtable = T_ALPHA;
                    }
                }
                for (k = 0; k < wnet; k += 2) {
                    chainemc[(*p_mclength)++] = (30 * chainet[k]) + chainet[k + 1];
                }
                chainemc[(*p_mclength)++] = 913; /* BYT Shift */
                chainemc[(*p_mclength)++] = c;
                wnet = 0;
                continue;
            }

            if (newtable != curtable) {
                wnet = pdf_textprocess_switch(curtable, newtable, chainet, wnet);
                curtable = newtable;
            }
            if (curtable == T_LOWER && t_table == T_ALPHA) {
                chainet[wnet++] = 27; /* AS */
            } else if (curtable != T_PUNCT && (t_table & T_PUNCT) && (curtable != T_MIXED || !(t_table & T_MIXED))) {
                chainet[wnet++] = 29; /* PS */
            }
            /* At last we add the character */
            chainet[wnet++] = pdf_asciiy[c];
        }
    }
    *p_i = i ? i - 1 : 0;

    *p_curtable = curtable;
    pdf_textprocess_end(chainemc, p_mclength, is_last_seg, chainet, wnet, p_curtable, p_tex_padded);
}

/* 671 */
/* Byte compaction */
INTERNAL void pdf_byteprocess(short *chainemc, int *p_mclength, const unsigned char chaine[], int start,
                const int length, const int lastmode) {
    const int real_lastmode = PDF_REAL_MODE(lastmode);

    if (length == 1) {
        /* shift or latch depending on previous mode */
        chainemc[(*p_mclength)++] = real_lastmode == PDF_TEX ? 913 : 901;
        chainemc[(*p_mclength)++] = chaine[start];
    } else {
        int len;
        /* select the switch for multiple of 6 bytes */
        if (length % 6 == 0) {
            chainemc[(*p_mclength)++] = 924;
        } else {
            /* Default mode for MICROPDF417 is Byte Compaction (ISO/IEC 24728:2006 5.4.3), but not emitting it
             * depends on whether an ECI has been emitted previously (or not) it appears, so simpler and safer
             * to always emit it. */
            chainemc[(*p_mclength)++] = 901;
        }

        len = 0;

        while (len < length) {
            uint64_t total;
            unsigned int chunkLen = length - len;
            if (6 <= chunkLen) { /* Take groups of 6 */
                chunkLen = 6;
                len += chunkLen;
                total = 0;

                while (chunkLen--) {
                    const uint64_t mantisa = chaine[start++];
                    total |= mantisa << (chunkLen * 8);
                }

                chunkLen = 5;

                while (chunkLen--) {
                    chainemc[*p_mclength + chunkLen] = (int) (total % 900);
                    total /= 900;
                }
                *p_mclength += 5;
            } else { /* If there remains a group of less than 6 bytes */
                len += chunkLen;
                while (chunkLen--) {
                    chainemc[(*p_mclength)++] = chaine[start++];
                }
            }
        }
    }
}

/* 712 */
/* Numeric compaction */
static void pdf_numbprocess(short *chainemc, int *p_mclength, const unsigned char chaine[], const int start,
            const int length) {
    int j;

    chainemc[(*p_mclength)++] = 902;

    j = 0;
    while (j < length) {
        int dumlength = 0;
        int p, len, loop, nombre, dummy[50];
        char chainemod[45];
        int longueur = length - j;
        if (longueur > 44) {
            longueur = 44;
        }
        len = longueur + 1;
        chainemod[0] = 1;
        for (loop = 1; loop < len; loop++) {
            chainemod[loop] = ctoi(chaine[start + loop + j - 1]);
        }
        do {
            /* 877 - gosub Modulo */
            p = 0;
            nombre = 0;
            for (loop = 0; loop < len; loop++) {
                nombre *= 10;
                nombre += chainemod[loop];
                if (nombre < 900) {
                    if (p) {
                        chainemod[p++] = 0;
                    }
                } else {
                    chainemod[p] = (nombre / 900);
                    nombre -= chainemod[p++] * 900; /* nombre % 900 */
                }
            }
            /* return to 723 */

            dummy[dumlength++] = nombre;
            len = p;
        } while (p);
        for (loop = dumlength - 1; loop >= 0; loop--) {
            chainemc[(*p_mclength)++] = dummy[loop];
        }
        j += longueur;
    }
}

#ifdef ZINT_TEST /* Wrapper for direct testing */
INTERNAL void pdf_numbprocess_test(short *chainemc, int *p_mclength, const unsigned char chaine[], const int start,
                const int length) {
    pdf_numbprocess(chainemc, p_mclength, chaine, start, length);
}
#endif

/* Minimal encoding */

/* Return number of consecutive chars from `position` in table `t_table` */
static int pdf_table_length(const unsigned char source[], const int length, const int position, const int t_table) {
    int i;

    for (i = position; i < length && (pdf_asciix[source[i]] & t_table); i++);

    return i - position;
}

struct pdf_edge {
    unsigned char mode;
    unsigned short from; /* Position in input data, 0-based */
    unsigned short len;
    unsigned short units; /* Cumulative TEX/NUM/BYT units since entering TEX/NUM/BYT mode */
    unsigned short unit_size; /* Number of codewords based on units since entering TEX/NUM/BYT mode */
    unsigned short size; /* Cumulative number of codewords in previous TEX/NUM/BYT modes */
    unsigned short previous; /* Index into edges array */
};

/* Note 1st row of edges not used so valid previous cannot point there, i.e. won't be zero */
#define PDF_PREVIOUS(edges, edge) \
    ((edge)->previous ? (edges) + (edge)->previous : NULL)

#if 0
#define PDF_TRACE
#endif
#include "pdf417_trace.h"

/* Initialize a new edge */
static int pdf_new_Edge(struct pdf_edge *edges, const int mode, const int from, const int len, const int t_table,
            const int lastmode, struct pdf_edge *previous, struct pdf_edge *edge) {
    const int real_mode = PDF_REAL_MODE(mode);
    int previousMode, real_previousMode;
    int units;
    int unit_size = 0; /* Suppress clang-tidy clang-analyzer-core.uninitialized.Assign warning */
    int dv, md;

    edge->mode = mode;
    edge->from = from;
    edge->len = len;
    if (previous) {
        assert(previous->mode && previous->len && (previous->unit_size + previous->size));
        previousMode = previous->mode;
        real_previousMode = PDF_REAL_MODE(previousMode);
        edge->previous = previous - edges;
        if (real_mode != real_previousMode) {
            edge->size = previous->size + previous->unit_size + 1; /* + TEX/NUM/BYT switch */
            units = 0;
        } else {
            edge->size = previous->size;
            units = previous->units;
        }
    } else {
        previousMode = lastmode;
        real_previousMode = PDF_REAL_MODE(previousMode);
        edge->previous = 0;
        edge->size = real_mode != real_previousMode || real_previousMode != PDF_TEX ? 1 : 0;
        units = 0;
    }

    switch (mode) {
        case PDF_ALP:
            assert(!t_table || (t_table & (T_ALPHA | T_PUNCT)));
            if (t_table) {
                if (previousMode != mode && real_previousMode == PDF_TEX) {
                    units += 1 + (previousMode == PDF_LOW); /* AL or ML+AL */
                }
                units += (1 + !(t_table & T_ALPHA)) * len; /* chars or PS + char */
            } else { /* Binary shift */
                assert(len == 1);
                if (units & 1) {
                    units++; /* PS or AL pad */
                }
                if (previousMode != mode && real_previousMode == PDF_TEX) {
                    units += 1 + (previousMode == PDF_LOW); /* AL or ML+AL */
                }
                units += 4; /* BYT SHIFT 913 (2 units) + byte (2 units) */
            }
            unit_size = (units + 1) >> 1;
            break;

        case PDF_LOW:
            assert(!t_table || (t_table & (T_LOWER | T_PUNCT | T_ALPHA)));
            if (t_table) {
                if (previousMode != mode) {
                    units += 1 + (previousMode == PDF_PNC); /* LL or AL+LL */
                }
                units += (1 + !(t_table & T_LOWER)) * len; /* chars or PS/AS + char */
            } else { /* Binary shift */
                assert(len == 1);
                if (units & 1) {
                    units++; /* PS or AL pad */
                }
                if (previousMode != mode) {
                    units += 1 + (previousMode == PDF_PNC); /* LL or AL+LL */
                }
                units += 4; /* BYT SHIFT 913 (2 units) + byte (2 units) */
            }
            unit_size = (units + 1) >> 1;
            break;

        case PDF_MIX:
            assert(!t_table || (t_table & (T_MIXED | T_PUNCT)));
            if (t_table) {
                if (previousMode != mode) {
                    units += 1 + (previousMode == PDF_PNC); /* ML or AL+ML */
                }
                units += (1 + !(t_table & T_MIXED)) * len; /* chars or PS + char */
            } else { /* Binary shift */
                assert(len == 1);
                if (units & 1) {
                    units++; /* PS pad */
                }
                if (previousMode != mode) {
                    units += 1 + (previousMode == PDF_PNC); /* ML or AL+ML */
                }
                units += 4; /* BYT SHIFT 913 (2 units) + byte (2 units) */
            }
            unit_size = (units + 1) >> 1;
            break;

        case PDF_PNC:
            assert(!t_table || (t_table & T_PUNCT));
            if (t_table) {
                if (previousMode != mode) {
                    units += 1 + (previousMode != PDF_MIX); /* PL or ML+PL */
                }
                units += len; /* chars */
            } else { /* Binary shift */
                assert(len == 1);
                if (units & 1) {
                    units += 3; /* AL pad + (after BYT SHIFT) ML+PL to return to PNC */
                } else if (previousMode != mode) {
                    units += 1 + (previousMode != PDF_MIX); /* PL or ML+PL */
                }
                units += 4; /* BYT SHIFT 913 (2 units) + byte (2 units) */
            }
            unit_size = (units + 1) >> 1;
            break;

        case PDF_BYT:
            units += len;
            dv = units / 6;
            unit_size = dv * 5 + (units - dv * 6);
            break;

        case PDF_NUM:
            units += len;
            dv = units / 44;
            md = units - dv * 44;
            unit_size = dv * 15 + (md ? md / 3 + 1 : 0);
            break;
    }
    edge->units = units;
    edge->unit_size = unit_size;

    return edge->size + edge->unit_size;
}

/* Whether `new_units` likely to result in less (fractional) codewords than `existing_units`, allowing for `mode` */
static int pdf_new_units_better(const int mode, const int existing_units, const int new_units) {
    if (PDF_REAL_MODE(mode) == PDF_TEX) {
        if ((new_units & 1) != (existing_units & 1)) {
            return (existing_units & 1);
        }
    } else if (mode == PDF_BYT) {
        const int existing_md = existing_units % 6;
        if (new_units % 6 != existing_md) {
            return existing_md;
        }
    }
    return new_units < existing_units;
}

/* Add an edge for a mode at a vertex if no existing edge or if more optimal than existing edge */
static void pdf_addEdge(const unsigned char *source, const int length, struct pdf_edge *edges, const int mode,
            const int from, const int len, const int t_table, const int lastmode, struct pdf_edge *previous) {
    struct pdf_edge edge;
    const int new_size = pdf_new_Edge(edges, mode, from, len, t_table, lastmode, previous, &edge);
    const int vertexIndex = from + len;
    const int v_ij = vertexIndex * PDF_NUM_MODES + mode - 1;
    const int v_size = edges[v_ij].size + edges[v_ij].unit_size;

    if (edges[v_ij].mode == 0 || v_size > new_size
            || (v_size == new_size && pdf_new_units_better(mode, edge.units, edges[v_ij].units))) {
        PDF_TRACE_AddEdge(source, length, edges, previous, vertexIndex, t_table, &edge);
        edges[v_ij] = edge;
    } else {
        PDF_TRACE_NotAddEdge(source, length, edges, previous, vertexIndex, t_table, &edge);
    }
}

/* Add edges for the various modes at a vertex */
static void pdf_addEdges(const unsigned char source[], const int length, const int lastmode, struct pdf_edge *edges,
            const int from, struct pdf_edge *previous) {
    const unsigned char c = source[from];
    const int t_table = pdf_asciix[c];

    if (t_table & T_ALPHA) {
        const int len = pdf_table_length(source, length, from, T_ALPHA);
        pdf_addEdge(source, length, edges, PDF_ALP, from, len, T_ALPHA, lastmode, previous);
    }
    if (!t_table || (t_table & T_PUNCT)) { /* Binary shift or PS */
        pdf_addEdge(source, length, edges, PDF_ALP, from, 1 /*len*/, t_table & ~T_ALPHA, lastmode, previous);
    }

    if (t_table & T_LOWER) {
        const int len = pdf_table_length(source, length, from, T_LOWER);
        pdf_addEdge(source, length, edges, PDF_LOW, from, len, T_LOWER, lastmode, previous);
    }
    if (!t_table || (t_table & (T_PUNCT | T_ALPHA))) { /* Binary shift or PS/AS */
        pdf_addEdge(source, length, edges, PDF_LOW, from, 1 /*len*/, t_table & ~T_LOWER, lastmode, previous);
    }

    if (t_table & T_MIXED) {
        const int len = pdf_table_length(source, length, from, T_MIXED);
        pdf_addEdge(source, length, edges, PDF_MIX, from, len, T_MIXED, lastmode, previous);
        if (len > 1 && z_isdigit(source[from + 1])) { /* Add single-length edge before digit to compare to NUM */
            pdf_addEdge(source, length, edges, PDF_MIX, from, 1 /*len*/, T_MIXED, lastmode, previous);
        }
    }
    if (!t_table || (t_table & T_PUNCT)) { /* Binary shift or PS */
        pdf_addEdge(source, length, edges, PDF_MIX, from, 1 /*len*/, t_table & ~T_MIXED, lastmode, previous);
    }

    if (t_table & T_PUNCT) {
        const int len = pdf_table_length(source, length, from, T_PUNCT);
        pdf_addEdge(source, length, edges, PDF_PNC, from, len, T_PUNCT, lastmode, previous);
    }
    if (!t_table) { /* Binary shift */
        pdf_addEdge(source, length, edges, PDF_PNC, from, 1 /*len*/, t_table, lastmode, previous);
    }

    if (z_isdigit(c)) {
        const int len = cnt_digits(source, length, from, -1 /*all*/);
        pdf_addEdge(source, length, edges, PDF_NUM, from, len, 0 /*t_table*/, lastmode, previous);
    }

    pdf_addEdge(source, length, edges, PDF_BYT, from, 1 /*len*/, 0 /*t_table*/, lastmode, previous);
}

/* Calculate optimized encoding modes */
static int pdf_define_mode(short liste[3][PDF_MAX_LEN], int *p_indexliste, const unsigned char source[],
            const int length, const int lastmode, const int debug_print) {

    int i, j, v_i;
    int minimalJ, minimalSize;
    struct pdf_edge *edge;
    int mode_start, mode_len;

    struct pdf_edge *edges = (struct pdf_edge *) calloc((length + 1) * PDF_NUM_MODES, sizeof(struct pdf_edge));
    if (!edges) {
        return 0;
    }
    pdf_addEdges(source, length, lastmode, edges, 0, NULL);

    PDF_TRACE_Edges("DEBUG Initial situation\n", source, length, edges, 0);

    for (i = 1; i < length; i++) {
        v_i = i * PDF_NUM_MODES;
        for (j = 0; j < PDF_NUM_MODES; j++) {
            if (edges[v_i + j].mode) {
                pdf_addEdges(source, length, lastmode, edges, i, edges + v_i + j);
            }
        }
        PDF_TRACE_Edges("DEBUG situation after adding edges to vertices at position %d\n", source, length, edges, i);
    }

    PDF_TRACE_Edges("DEBUG Final situation\n", source, length, edges, length);

    v_i = length * PDF_NUM_MODES;
    minimalJ = -1;
    minimalSize = INT_MAX;
    for (j = 0; j < PDF_NUM_MODES; j++) {
        edge = edges + v_i + j;
        if (edge->mode) {
            const int edge_size = edge->size + edge->unit_size;
            if (debug_print) {
                printf("edges[%d][%d][0] size %d(%d,%d)\n", length, j, edge_size, edge->unit_size, edge->size);
            }
            if (edge_size < minimalSize) {
                minimalSize = edge_size;
                minimalJ = j;
                if (debug_print) printf(" set minimalJ %d\n", minimalJ);
            }
        } else {
            if (debug_print) printf("edges[%d][%d][0] NULL\n", length, j);
        }
    }
    assert(minimalJ >= 0);

    edge = edges + v_i + minimalJ;
    mode_len = 0;
    mode_start = length;
    while (edge) {
        const int current_mode = edge->mode;
        const int current_from = edge->from;
        mode_len += edge->len;
        edge = PDF_PREVIOUS(edges, edge);
        if (!edge || edge->mode != current_mode) {
            mode_start--;
            liste[0][mode_start] = mode_len;
            liste[1][mode_start] = current_mode;
            liste[2][mode_start] = current_from;
            mode_len = 0;
        }
    }
    *p_indexliste = length - mode_start;
    if (mode_start) {
        memmove(liste[0], liste[0] + mode_start, sizeof(short) * (*p_indexliste));
        memmove(liste[1], liste[1] + mode_start, sizeof(short) * (*p_indexliste));
        memmove(liste[2], liste[2] + mode_start, sizeof(short) * (*p_indexliste));
    }
    if (debug_print) {
        printf("modes (%d):", *p_indexliste);
        for (i = 0; i < *p_indexliste; i++) printf(" %c(%d,%d)", pdf_smodes[liste[1][i]], liste[2][i], liste[0][i]);
        fputc('\n', stdout);
    }

    free(edges);

    return 1;
}

/* Initial processing of data, shared by `pdf417()` and `micropdf417()` */
static int pdf_initial(struct zint_symbol *symbol, const unsigned char chaine[], const int length, const int eci,
            const int is_micro, const int is_last_seg, int *p_lastmode, int *p_curtable, int *p_tex_padded,
            short chainemc[PDF_MAX_STREAM_LEN], int *p_mclength) {
    int i, indexchaine = 0, indexliste = 0;
    short liste[3][PDF_MAX_LEN] = {{0}};
    int mclength;
    const int debug_print = symbol->debug & ZINT_DEBUG_PRINT;
    const int fast_encode = symbol->input_mode & FAST_MODE;

    /* 456 */

    if (fast_encode) {
        int mode = pdf_quelmode(chaine[0]);

        /* 463 */
        do {
            liste[1][indexliste] = mode;
            liste[2][indexliste] = indexchaine;
            while ((liste[1][indexliste] == mode) && (indexchaine < length)) {
                liste[0][indexliste]++;
                indexchaine++;
                mode = pdf_quelmode(chaine[indexchaine]);
            }
            indexliste++;
        } while (indexchaine < length);

        if (debug_print) {
            fputs("\nInitial block pattern:\n", stdout);
            for (i = 0; i < indexliste; i++) {
                int j;
                for (j = 0; j < liste[0][i]; j++) fputc(pdf_mode_str(liste[1][i])[0], stdout);
            }
            fputc('\n', stdout);
        }

        pdf_appendix_d_encode(chaine, liste, &indexliste);
     } else {
        if (!pdf_define_mode(liste, &indexliste, chaine, length, *p_lastmode, debug_print)) {
            return errtxt(ZINT_ERROR_MEMORY, symbol, 749, "Insufficient memory for mode buffers");
        }
    }

    if (debug_print) {
        fputs("\nCompacted block pattern:\n", stdout);
        for (i = 0; i < indexliste; i++) {
            int j;
            for (j = 0; j < liste[0][i]; j++) fputc(pdf_mode_str(PDF_REAL_MODE(liste[1][i]))[0], stdout);
        }
        fputc('\n', stdout);
    }

    /* 541 - now compress the data */
    indexchaine = 0;
    mclength = *p_mclength;
    if (mclength == 0 && !is_micro) {
        mclength++; /* Allow for length descriptor for full symbol */
    }

    if (*p_mclength == 0 && (symbol->output_options & READER_INIT)) {
        chainemc[mclength++] = 921; /* Reader Initialisation */
    }

    if (eci != 0) {
        if (eci > 811799) {
            return errtxtf(ZINT_ERROR_INVALID_OPTION, symbol, 472, "ECI code '%d' out of range (0 to 811799)",
                            symbol->eci);
        }
        /* Encoding ECI assignment number, according to Table 8 */
        if (eci <= 899) {
            chainemc[mclength++] = 927; /* ECI */
            chainemc[mclength++] = eci;
        } else if (eci <= 810899) {
            chainemc[mclength++] = 926; /* ECI */
            chainemc[mclength++] = (eci / 900) - 1;
            chainemc[mclength++] = eci % 900;
        } else {
            chainemc[mclength++] = 925; /* ECI */
            chainemc[mclength++] = eci - 810900;
        }
    }

    for (i = 0; i < indexliste; i++) {
        const int real_mode = PDF_REAL_MODE(liste[1][i]);
        switch (real_mode) {
            case PDF_TEX: /* 547 - text mode */
                if (fast_encode) {
                    pdf_textprocess(chainemc, &mclength, chaine, indexchaine, liste[0][i], *p_lastmode, is_last_seg,
                                    p_curtable, p_tex_padded);
                    indexchaine += liste[0][i];
                    *p_lastmode = PDF_ALP;
                } else {
                    pdf_textprocess_minimal(chainemc, &mclength, chaine, liste, indexliste, *p_lastmode, is_last_seg,
                                            p_curtable, p_tex_padded, &i);
                    indexchaine = i + 1 < indexliste ? liste[2][i + 1] : length;
                    *p_lastmode = PDF_TABLE_TO_MODE(*p_curtable);
                }
                break;
            case PDF_BYT: /* 670 - octet stream mode */
                pdf_byteprocess(chainemc, &mclength, chaine, indexchaine, liste[0][i], *p_lastmode);
                /* don't switch mode on single byte shift from text mode */
                if (PDF_REAL_MODE(*p_lastmode) != PDF_TEX || liste[0][i] != 1) {
                    *p_lastmode = PDF_BYT;
                } else if (*p_curtable == T_PUNCT && *p_tex_padded) { /* If T_PUNCT and padded with AL */
                    /* Then need to reset to alpha - ISO/IEC 15438:2015 5.4.2.4 b) 2) */
                    *p_curtable = T_ALPHA;
                }
                indexchaine += liste[0][i];
                break;
            case PDF_NUM: /* 712 - numeric mode */
                pdf_numbprocess(chainemc, &mclength, chaine, indexchaine, liste[0][i]);
                *p_lastmode = PDF_NUM;
                indexchaine += liste[0][i];
                break;
        }
    }

    *p_mclength = mclength;

    return 0;
}

/* Call `pdf_initial()` for each segment, dealing with Structured Append beforehand */
static int pdf_initial_segs(struct zint_symbol *symbol, struct zint_seg segs[], const int seg_count,
            const int is_micro, short chainemc[PDF_MAX_STREAM_LEN], int *p_mclength, int structapp_cws[18],
            int *p_structapp_cp) {
    int i;
    int error_number = 0;
    int structapp_cp = 0;
    int lastmode;
    int curtable;
    int tex_padded;

    *p_mclength = 0;

    if (symbol->structapp.count) {
        int id_cnt = 0, ids[10];

        if (symbol->structapp.count < 2 || symbol->structapp.count > 99999) {
            return errtxtf(ZINT_ERROR_INVALID_OPTION, symbol, 740,
                            "Structured Append count '%d' out of range (2 to 99999)", symbol->structapp.count);
        }
        if (symbol->structapp.index < 1 || symbol->structapp.index > symbol->structapp.count) {
            return errtxtf(ZINT_ERROR_INVALID_OPTION, symbol, 741,
                            "Structured Append index '%1$d' out of range (1 to count %2$d)",
                            symbol->structapp.index, symbol->structapp.count);
        }
        if (symbol->structapp.id[0]) {
            int id_len;

            for (id_len = 1; id_len < 31 && symbol->structapp.id[id_len]; id_len++);

            if (id_len > 30) { /* 10 triplets */
                return errtxtf(ZINT_ERROR_INVALID_OPTION, symbol, 742,
                                "Structured Append ID length %d too long (30 digit maximum)", id_len);
            }

            for (i = 0; i < id_len; i += 3, id_cnt++) {
                const int triplet_len = i + 3 < id_len ? 3 : id_len - i;
                ids[id_cnt] = to_int((const unsigned char *) (symbol->structapp.id + i), triplet_len);
                if (ids[id_cnt] == -1) {
                    return errtxt(ZINT_ERROR_INVALID_OPTION, symbol, 743,
                                    "Invalid Structured Append ID (digits only)");
                }
                if (ids[id_cnt] > 899) {
                    return errtxtf(ZINT_ERROR_INVALID_OPTION, symbol, 744,
                                    "Structured Append ID triplet %1$d value '%2$03d' out of range (000 to 899)",
                                    id_cnt + 1, ids[id_cnt]);
                }
            }
        }
        structapp_cws[structapp_cp++] = 928; /* Macro marker */
        structapp_cws[structapp_cp++] = (100000 + symbol->structapp.index - 1) / 900; /* Segment index 1 */
        structapp_cws[structapp_cp++] = (100000 + symbol->structapp.index - 1) % 900; /* Segment index 2 */
        for (i = 0; i < id_cnt; i++) {
            structapp_cws[structapp_cp++] = ids[i];
        }
        structapp_cws[structapp_cp++] = 923; /* Optional field */
        structapp_cws[structapp_cp++] = 1; /* Segment count tag */
        structapp_cws[structapp_cp++] = (100000 + symbol->structapp.count) / 900; /* Segment count 1 */
        structapp_cws[structapp_cp++] = (100000 + symbol->structapp.count) % 900; /* Segment count 2 */
        if (symbol->structapp.index == symbol->structapp.count) {
            structapp_cws[structapp_cp++] = 922; /* Special last segment terminator */
        }
    }
    *p_structapp_cp = structapp_cp;

    /* Default mode for PDF417 is Text Compaction Alpha (ISO/IEC 15438:2015 5.4.2.1), and for MICROPDF417 is Byte
     * Compaction (ISO/IEC 24728:2006 5.4.3) */
    lastmode = is_micro ? PDF_BYT : PDF_ALP;
    /* Start in upper alpha - tracked across calls to `pdf_textprocess()` to allow for interleaving byte shifts */
    curtable = T_ALPHA;

    for (i = 0; i < seg_count; i++) {
        error_number = pdf_initial(symbol, segs[i].source, segs[i].length, segs[i].eci, is_micro, i + 1 == seg_count,
                        &lastmode, &curtable, &tex_padded, chainemc, p_mclength);
        if (error_number) { /* Only errors >= ZINT_ERROR returned */
            return error_number;
        }
    }

    return error_number;
}

/* 366 */
/* Encode PDF417 */
static int pdf_enc(struct zint_symbol *symbol, struct zint_seg segs[], const int seg_count) {
    int i, j, longueur, loop, mccorrection[520] = {0}, offset;
    int total, mclength, c1, c2, c3, dummy[35];
    short chainemc[PDF_MAX_STREAM_LEN];
    int rows, cols, ecc, ecc_cws, padding;
    char pattern[580];
    int bp = 0;
    int structapp_cws[18] = {0}; /* 3 (Index) + 10 (ID) + 4 (Count) + 1 (Last) */
    int structapp_cp = 0;
    int error_number;
    const int debug_print = symbol->debug & ZINT_DEBUG_PRINT;
    static const short ecc_num_cws[] = { 2, 4, 8, 16, 32, 64, 128, 256, 512 };

    if ((i = segs_length(segs, seg_count)) > PDF_MAX_LEN) {
        return errtxtf(ZINT_ERROR_TOO_LONG, symbol, 463, "Input length %d too long (maximum " PDF_MAX_LEN_S ")", i);
    }

    error_number = pdf_initial_segs(symbol, segs, seg_count, 0 /*is_micro*/, chainemc, &mclength, structapp_cws,
                    &structapp_cp);
    if (error_number) { /* Only errors return >= ZINT_ERROR */
        return error_number;
    }

    if (debug_print) {
        printf("\nCompressed data stream (%d):\n", mclength - 1);
        for (i = 1; i < mclength; i++) { /* Skip unset length descriptor */
            printf("%d ", chainemc[i]);
        }
        fputs("\n\n", stdout);
    }

    /* 752 - Now take care of the number of CWs per row */

    /* ECC */
    ecc = symbol->option_1;
    if (ecc < 0) { /* If not specified, set ECC depending on no. of codewords */
        const int data_cws = mclength - 1 + structapp_cp; /* -1 for length descriptor */
        /* ISO/IEC 15438:2015 Annex E Table E.1 Recommended minima */
        if (data_cws <= 40) {
            ecc = 2;
        } else if (data_cws <= 160) {
            ecc = 3;
        } else if (data_cws <= 320) {
            ecc = 4;
        } else if (data_cws <= 863) {
            ecc = 5;
        } else {
            ecc = 6; /* Not mentioned in Table E.1 */
        }
    }
    ecc_cws = ecc_num_cws[ecc];

    longueur = mclength + structapp_cp + ecc_cws;

    if (debug_print) printf("Total No. of Codewords: %d, ECC %d, No. of ECC Codewords: %d\n", longueur, ecc, ecc_cws);

    if (longueur > 928) {
        /* Enforce maximum codeword limit */
        return errtxt(ZINT_ERROR_TOO_LONG, symbol, 464, "Input too long, requires too many codewords (maximum 928)");
    }

    cols = symbol->option_2;
    rows = symbol->option_3;
    if (rows) { /* Rows given */
        if (cols < 1) { /* Cols automatic */
            cols = (longueur + rows - 1) / rows;
            if (cols <= 1) {
                cols = 1;
            } else {
                /* Increase rows if would need > 30 columns */
                for (; cols > 30 && rows < 90; rows++, cols = (longueur + rows - 1) / rows);
                assert(cols <= 30);
                /* Increase rows if multiple too big */
                for (; cols >= 1 && rows < 90 && rows * cols > 928; rows++, cols = (longueur + rows - 1) / rows);
                if (rows * cols > 928) {
                    return errtxt(ZINT_ERROR_TOO_LONG, symbol, 465,
                                    "Input too long, requires too many codewords (maximum 928)");
                }
            }
        } else { /* Cols given */
            /* Increase rows if multiple too big */
            for (; rows <= 90 && rows * cols < longueur; rows++);
            if (rows > 90 || rows * cols > 928) {
                return errtxtf(ZINT_ERROR_TOO_LONG, symbol, 745, "Input too long for number of columns '%d'", cols);
            }
        }
        if (rows != symbol->option_3) {
            error_number = errtxtf(ZINT_WARN_INVALID_OPTION, symbol, 746,
                                    "Number of rows increased from %1$d to %2$d", symbol->option_3, rows);
        }
    } else { /* Rows automatic, cols automatic or given */
        if (cols < 1) { /* Cols automatic */
            cols = (int) round(sqrt((longueur - 1) / 3.0)); /* -1 (length descriptor) for back-compatibility */
        }
        rows = (longueur + cols - 1) / cols;
        if (rows <= 3) {
            rows = 3;
        } else {
            /* Increase cols if would need > 90 rows - do this even if cols specified for better back-compatibility
               (though previously only increased once) */
            for (; rows > 90 && cols < 30; cols++, rows = (longueur + cols - 1) / cols);
            assert(rows <= 90);
            /* Increase cols if multiple too big */
            for (; rows >= 3 && cols < 30 && rows * cols > 928; cols++, rows = (longueur + cols - 1) / cols);
            if (rows * cols > 928) {
                return errtxt(ZINT_ERROR_TOO_LONG, symbol, 747,
                                "Input too long, requires too many codewords (maximum 928)");
            }
            if (symbol->option_2 && cols != symbol->option_2) { /* Note previously did not warn if cols auto-upped */
                error_number = errtxtf(ZINT_WARN_INVALID_OPTION, symbol, 748,
                                        "Number of columns increased from %1$d to %2$d", symbol->option_2, cols);
            }
        }
    }
    assert(rows * cols >= longueur);

    /* 781 - Padding calculation */
    padding = rows * cols - longueur;

    /* We add the padding */
    for (i = 0; i < padding; i++) {
        chainemc[mclength++] = 900;
    }
    if (debug_print) printf("Padding: %d\n", padding);

    /* We add the Structured Append Macro Control Block if any */
    if (structapp_cp) {
        for (i = 0; i < structapp_cp; i++) {
            chainemc[mclength++] = structapp_cws[i];
        }
    }

    /* Set the length descriptor */
    chainemc[0] = mclength;

    /* 796 - we now take care of the Reed Solomon codes */
    switch (ecc) {
        case 1: offset = 2;
            break;
        case 2: offset = 6;
            break;
        case 3: offset = 14;
            break;
        case 4: offset = 30;
            break;
        case 5: offset = 62;
            break;
        case 6: offset = 126;
            break;
        case 7: offset = 254;
            break;
        case 8: offset = 510;
            break;
        default: offset = 0;
            break;
    }

    for (i = 0; i < mclength; i++) {
        total = (chainemc[i] + mccorrection[ecc_cws - 1]) % 929;
        for (j = ecc_cws - 1; j > 0; j--) {
            mccorrection[j] = (mccorrection[j - 1] + 929 - (total * pdf_coefrs[offset + j]) % 929) % 929;
        }
        mccorrection[0] = (929 - (total * pdf_coefrs[offset]) % 929) % 929;
    }

    /* we add these codes to the string */
    for (i = ecc_cws - 1; i >= 0; i--) {
        chainemc[mclength++] = mccorrection[i] ? 929 - mccorrection[i] : 0;
    }

    if (debug_print) {
        printf("Complete CW string (%d):\n", mclength);
        for (i = 0; i < mclength; i++) {
            printf("%d ", chainemc[i]);
        }
        fputc('\n', stdout);
    }
#ifdef ZINT_TEST
    if (symbol->debug & ZINT_DEBUG_TEST) {
        debug_test_codeword_dump_short(symbol, chainemc, mclength);
    }
#endif

    if (debug_print) printf("\nSymbol size:\n%d columns x %d rows\n", cols, rows);

    /* 818 - The CW string is finished */
    c1 = (rows - 1) / 3;
    c2 = ecc * 3 + (rows - 1) % 3;
    c3 = cols - 1;

    /* we now encode each row */
    for (i = 0; i < rows; i++) {
        const int k = (i / 3) * 30;
        bp = 0;
        for (j = 0; j < cols; j++) {
            dummy[j + 1] = chainemc[i * cols + j];
        }
        switch (i % 3) {
            case 0:
                dummy[0] = k + c1;
                dummy[cols + 1] = k + c3;
                offset = 0; /* cluster(0) */
                break;
            case 1:
                dummy[0] = k + c2;
                dummy[cols + 1] = k + c1;
                offset = 929; /* cluster(3) */
                break;
            case 2:
                dummy[0] = k + c3;
                dummy[cols + 1] = k + c2;
                offset = 1858; /* cluster(6) */
                break;
        }
        bp = bin_append_posn(0x1FEA8, 17, pattern, bp); /* Row start */

        for (j = 0; j <= cols; j++) {
            bp = bin_append_posn(pdf_bitpattern[offset + dummy[j]], 16, pattern, bp);
            pattern[bp++] = '0';
        }

        if (symbol->symbology != BARCODE_PDF417COMP) {
            bp = bin_append_posn(pdf_bitpattern[offset + dummy[j]], 16, pattern, bp);
            pattern[bp++] = '0';
            bp = bin_append_posn(0x3FA29, 18, pattern, bp); /* Row Stop */
        } else {
            pattern[bp++] = '1'; /* Compact PDF417 Stop pattern */
        }

        for (loop = 0; loop < bp; loop++) {
            if (pattern[loop] == '1') {
                set_module(symbol, i, loop);
            }
        }
    }
    symbol->width = bp;
    symbol->rows = rows;

    /* ISO/IEC 15438:2015 Section 5.8.2 3X minimum row height */
    if (error_number) {
        (void) set_height(symbol, 3.0f, 0.0f, 0.0f, 1 /*no_errtxt*/);
    } else {
        error_number = set_height(symbol, 3.0f, 0.0f, 0.0f, 0 /*no_errtxt*/);
    }

    /* 843 */
    return error_number;
}

/* 345 */
INTERNAL int pdf417(struct zint_symbol *symbol, struct zint_seg segs[], const int seg_count) {
    int codeerr, error_number;

    error_number = 0;

    if ((symbol->option_1 < -1) || (symbol->option_1 > 8)) {
        errtxtf(0, symbol, 460, "Error correction level '%d' out of range (0 to 8)", symbol->option_1);
        if (symbol->warn_level == WARN_FAIL_ALL) {
            return ZINT_ERROR_INVALID_OPTION;
        }
        error_number = errtxt_adj(ZINT_WARN_INVALID_OPTION, symbol, "%1$s%2$s", ", ignoring");
        symbol->option_1 = -1;
    }
    if ((symbol->option_2 < 0) || (symbol->option_2 > 30)) {
        errtxtf(0, symbol, 461, "Number of columns '%d' out of range (1 to 30)", symbol->option_2);
        if (symbol->warn_level == WARN_FAIL_ALL) {
            return ZINT_ERROR_INVALID_OPTION;
        }
        error_number = errtxt_adj(ZINT_WARN_INVALID_OPTION, symbol, "%1$s%2$s", ", ignoring");
        symbol->option_2 = 0;
    }
    if (symbol->option_3 && (symbol->option_3 < 3 || symbol->option_3 > 90)) {
        return errtxtf(ZINT_ERROR_INVALID_OPTION, symbol, 466, "Number of rows '%d' out of range (3 to 90)",
                        symbol->option_3);
    }
    if (symbol->option_2 && symbol->option_3 && symbol->option_2 * symbol->option_3 > 928) {
        return errtxtf(ZINT_ERROR_INVALID_OPTION, symbol, 475, "Columns x rows value '%d' out of range (1 to 928)",
                        symbol->option_2 * symbol->option_3);
    }

    /* 349 */
    codeerr = pdf_enc(symbol, segs, seg_count);

    /* 352 */
    if (codeerr != 0) {
        error_number = codeerr;
    }

    /* 364 */
    return error_number;
}

/* like PDF417 only much smaller! */
INTERNAL int micropdf417(struct zint_symbol *symbol, struct zint_seg segs[], const int seg_count) {
    int i, k, j, longueur, mccorrection[50] = {0}, offset;
    int total, mclength, error_number = 0;
    short chainemc[PDF_MAX_STREAM_LEN];
    char pattern[580];
    int bp = 0;
    int structapp_cws[18] = {0}; /* 3 (Index) + 10 (ID) + 4 (Count) + 1 (Last) */
    int structapp_cp = 0;
    int variant;
    int LeftRAP, CentreRAP, RightRAP, Cluster, loop;
    const int debug_print = symbol->debug & ZINT_DEBUG_PRINT;
    /* From ISO/IEC 24728:2006 Table 1 — MicroPDF417 version characteristics */
    static char col_max_codewords[5] = { 0, 20, 37, 82, 126 };

    if ((i = segs_length(segs, seg_count)) > MICRO_PDF_MAX_LEN) {
        return errtxtf(ZINT_ERROR_TOO_LONG, symbol, 474, "Input length %d too long (maximum " MICRO_PDF_MAX_LEN_S ")",
                        i);
    }
    if (symbol->option_3) {
        return errtxt(ZINT_ERROR_INVALID_OPTION, symbol, 476, "Cannot specify rows for MicroPDF417");
    }

    /* Encoding starts out the same as PDF417, so use the same code */

    error_number = pdf_initial_segs(symbol, segs, seg_count, 1 /*is_micro*/, chainemc, &mclength, structapp_cws,
                    &structapp_cp);
    if (error_number) { /* Only errors return >= ZINT_ERROR */
        return error_number;
    }

    /* This is where it all changes! */

    if (mclength + structapp_cp > 126) {
        return errtxtf(ZINT_ERROR_TOO_LONG, symbol, 467, "Input too long, requires %d codewords (maximum 126)",
                        mclength + structapp_cp);
    }
    if (symbol->option_2 > 4) {
        if (symbol->warn_level == WARN_FAIL_ALL) {
            return errtxtf(ZINT_ERROR_INVALID_OPTION, symbol, 471, "Number of columns '%d' out of range (1 to 4)",
                            symbol->option_2);
        }
        error_number = errtxtf(ZINT_WARN_INVALID_OPTION, symbol, 468,
                                "Number of columns '%d' out of range (1 to 4), ignoring", symbol->option_2);
        symbol->option_2 = 0;
    }

    if (debug_print) {
        printf("\nEncoded Data Stream (%d):\n", mclength);
        for (i = 0; i < mclength; i++) {
            printf("%3d ", chainemc[i]);
        }
        fputc('\n', stdout);
    }

    /* Now figure out which variant of the symbol to use and load values accordingly */

    variant = 0;

    if (symbol->option_2 >= 1 && mclength + structapp_cp > col_max_codewords[symbol->option_2]) {
        /* The user specified the column but the data doesn't fit - go to automatic */
        if (symbol->warn_level == WARN_FAIL_ALL) {
            return errtxtf(ZINT_ERROR_INVALID_OPTION, symbol, 469,
                            "Input too long for number of columns '%1$d', requires %2$d codewords (maximum %3$d)",
                            symbol->option_2, mclength + structapp_cp, col_max_codewords[symbol->option_2]);
        }
        error_number = errtxtf(ZINT_WARN_INVALID_OPTION, symbol, 470,
                                "Input too long for number of columns '%d', ignoring", symbol->option_2);
        symbol->option_2 = 0;
    }

    if (symbol->option_2 == 1) {
        /* the user specified 1 column and the data does fit */
        if (mclength + structapp_cp <= 4) {
            variant = 1;
        } else if (mclength + structapp_cp <= 7) {
            variant = 2;
        } else if (mclength + structapp_cp <= 10) {
            variant = 3;
        } else if (mclength + structapp_cp <= 12) {
            variant = 4;
        } else if (mclength + structapp_cp <= 16) {
            variant = 5;
        } else {
            variant = 6;
        }
    } else if (symbol->option_2 == 2) {
        /* the user specified 2 columns and the data does fit */
        if (mclength + structapp_cp <= 8) {
            variant = 7;
        } else if (mclength + structapp_cp <= 13) {
            variant = 8;
        } else if (mclength + structapp_cp <= 19) {
            variant = 9;
        } else if (mclength + structapp_cp <= 24) {
            variant = 10;
        } else if (mclength + structapp_cp <= 29) {
            variant = 11;
        } else if (mclength + structapp_cp <= 33) {
            variant = 12;
        } else {
            variant = 13;
        }
    } else if (symbol->option_2 == 3) {
        /* the user specified 3 columns and the data does fit */
        if (mclength + structapp_cp <= 6) {
            variant = 14;
        } else if (mclength + structapp_cp <= 10) {
            variant = 15;
        } else if (mclength + structapp_cp <= 14) {
            variant = 16;
        } else if (mclength + structapp_cp <= 18) {
            variant = 17;
        } else if (mclength + structapp_cp <= 24) {
            variant = 18;
        } else if (mclength + structapp_cp <= 34) {
            variant = 19;
        } else if (mclength + structapp_cp <= 46) {
            variant = 20;
        } else if (mclength + structapp_cp <= 58) {
            variant = 21;
        } else if (mclength + structapp_cp <= 70) {
            variant = 22;
        } else {
            variant = 23;
        }
    } else if (symbol->option_2 == 4) {
        /* the user specified 4 columns and the data does fit */
        if (mclength + structapp_cp <= 8) {
            variant = 24;
        } else if (mclength + structapp_cp <= 12) {
            variant = 25;
        } else if (mclength + structapp_cp <= 18) {
            variant = 26;
        } else if (mclength + structapp_cp <= 24) {
            variant = 27;
        } else if (mclength + structapp_cp <= 30) {
            variant = 28;
        } else if (mclength + structapp_cp <= 39) {
            variant = 29;
        } else if (mclength + structapp_cp <= 54) {
            variant = 30;
        } else if (mclength + structapp_cp <= 72) {
            variant = 31;
        } else if (mclength + structapp_cp <= 90) {
            variant = 32;
        } else if (mclength + structapp_cp <= 108) {
            variant = 33;
        } else {
            variant = 34;
        }
    } else {
        /* Zint can choose automatically from all available variations */
        for (i = 27; i >= 0; i--) {
            /* Note mclength + structapp_cp <= 126 and pdf_MicroAutosize[27] == 126 so variant will be set */
            if (pdf_MicroAutosize[i] >= mclength + structapp_cp) {
                variant = pdf_MicroAutosize[i + 28];
            } else {
                break;
            }
        }
    }
    assert(variant > 0); /* Suppress clang-tidy clang-analyzer-core.uninitialized.Assign */

    /* Now we have the variant we can load the data */
    variant--;
    symbol->option_2 = pdf_MicroVariants[variant]; /* columns */
    symbol->rows = pdf_MicroVariants[variant + 34]; /* rows */
    k = pdf_MicroVariants[variant + 68]; /* number of EC CWs */
    longueur = (symbol->option_2 * symbol->rows) - k; /* number of non-EC CWs */
    i = longueur - (mclength + structapp_cp); /* amount of padding required */
    offset = pdf_MicroVariants[variant + 102]; /* coefficient offset */

    if (debug_print) {
        fputs("\nChoose symbol size:\n", stdout);
        printf("%d columns x %d rows, variant %d\n", symbol->option_2, symbol->rows, variant + 1);
        printf("%d data codewords (including %d pads), %d ecc codewords\n", longueur, i, k);
        fputc('\n', stdout);
    }

    /* We add the padding */
    while (i > 0) {
        chainemc[mclength++] = 900;
        i--;
    }

    /* We add the Structured Append Macro Control Block if any */
    if (structapp_cp) {
        for (i = 0; i < structapp_cp; i++) {
            chainemc[mclength++] = structapp_cws[i];
        }
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

    if (debug_print) {
        printf("Encoded Data Stream with ECC (%d):\n", mclength);
        for (i = 0; i < mclength; i++) {
            printf("%3d ", chainemc[i]);
        }
        fputc('\n', stdout);
    }
#ifdef ZINT_TEST
    if (symbol->debug & ZINT_DEBUG_TEST) {
        debug_test_codeword_dump_short(symbol, chainemc, mclength);
    }
#endif

    /* Now get the RAP (Row Address Pattern) start values */
    LeftRAP = pdf_RAPTable[variant];
    CentreRAP = pdf_RAPTable[variant + 34];
    RightRAP = pdf_RAPTable[variant + 68];
    Cluster = pdf_RAPTable[variant + 102] / 3;

    /* That's all values loaded, get on with the encoding */

    /* Cluster can be 0, 1 or 2 for Cluster(0), Cluster(3) and Cluster(6) */

    if (debug_print) fputs("\nInternal row representation:\n", stdout);
    for (i = 0; i < symbol->rows; i++) {
        if (debug_print) printf("row %d: ", i);
        bp = 0;
        offset = 929 * Cluster;
        k = i * symbol->option_2;

        /* Copy the data into codebarre */
        bp = bin_append_posn(pdf_rap_side[LeftRAP - 1], 10, pattern, bp);
        bp = bin_append_posn(pdf_bitpattern[offset + chainemc[k]], 16, pattern, bp);
        pattern[bp++] = '0';
        if (symbol->option_2 >= 2) {
            if (symbol->option_2 == 3) {
                bp = bin_append_posn(pdf_rap_centre[CentreRAP - 1], 10, pattern, bp);
            }
            bp = bin_append_posn(pdf_bitpattern[offset + chainemc[k + 1]], 16, pattern, bp);
            pattern[bp++] = '0';
            if (symbol->option_2 >= 3) {
                if (symbol->option_2 == 4) {
                    bp = bin_append_posn(pdf_rap_centre[CentreRAP - 1], 10, pattern, bp);
                }
                bp = bin_append_posn(pdf_bitpattern[offset + chainemc[k + 2]], 16, pattern, bp);
                pattern[bp++] = '0';
                if (symbol->option_2 == 4) {
                    bp = bin_append_posn(pdf_bitpattern[offset + chainemc[k + 3]], 16, pattern, bp);
                    pattern[bp++] = '0';
                }
            }
        }
        bp = bin_append_posn(pdf_rap_side[RightRAP - 1], 10, pattern, bp);
        pattern[bp++] = '1'; /* stop */
        if (debug_print) printf("%.*s\n", bp, pattern);

        /* so now pattern[] holds the string of '1's and '0's. - copy this to the symbol */
        for (loop = 0; loop < bp; loop++) {
            if (pattern[loop] == '1') {
                set_module(symbol, i, loop);
            }
        }

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

    /* ISO/IEC 24728:2006 Section 5.8.2 2X minimum row height */
    if (error_number) {
        (void) set_height(symbol, 2.0f, 0.0f, 0.0f, 1 /*no_errtxt*/);
    } else {
        error_number = set_height(symbol, 2.0f, 0.0f, 0.0f, 0 /*no_errtxt*/);
    }

    return error_number;
}

#undef T_ALPHA
#undef T_LOWER
#undef T_MIXED
#undef T_PUNCT
#undef T_ALWMX
#undef T_MXPNC

/* vim: set ts=4 sw=4 et : */
