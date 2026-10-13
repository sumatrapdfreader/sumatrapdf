/* maxicode.c - Handles MaxiCode */
/*
    libzint - the open source barcode library
    Copyright (C) 2010-2024 Robin Stuart <rstuart114@gmail.com>

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

/* Includes corrections thanks to Monica Swanson @ Source Technologies */
#include <assert.h>
#include <stdio.h>
#include "common.h"
#include "maxicode.h"
#include "reedsol.h"

/* Code Set states. Those with PAD (i.e. A, B and E) are first pick */
#define MX_A      0
#define MX_B      1
#define MX_E      2
#define MX_C      3
#define MX_D      4
#define MX_STATES 5

/* Prior:A        B        E        C        D        Later */
static const char mx_latch_seq[MX_STATES][MX_STATES][2] = {
    { {  0  }, {63   }, {58   }, {58   }, {58   } }, /* A */
    { {63   }, {  0  }, {63   }, {63   }, {63   } }, /* B */
    { {62,62}, {62,62}, {  0  }, {62,62}, {62,62} }, /* E */
    { {60,60}, {60,60}, {60,60}, {  0  }, {60,60} }, /* C */
    { {61,61}, {61,61}, {61,61}, {61,61}, {  0  } }, /* D */

};
static const char mx_latch_len[MX_STATES][MX_STATES] = { /* Lengths of above */
    {    0,       1,       1,       1,       1    }, /* A */
    {    1,       0,       1,       1,       1    }, /* B */
    {    2,       2,       0,       2,       2    }, /* E */
    {    2,       2,       2,       0,       2    }, /* C */
    {    2,       2,       2,       2,       0    }, /* D */
};

/* Op */
struct mx_op {
    unsigned char op;
    unsigned char intake; /* `output` calculated from this */
};

/* Op table ops */
#define MX_OP_DGTS      0
#define MX_OP_SETA      0x01 /* If change these, must change `maxiCodeSet` */
#define MX_OP_SETB      0x02
#define MX_OP_SETE      0x04
#define MX_OP_SETC      0x08
#define MX_OP_SETD      0x10
#define MX_OP_SHA       (0x20 | MX_OP_SETA)
#define MX_OP_2SHA      (0x40 | MX_OP_SETA)
#define MX_OP_3SHA      (0x80 | MX_OP_SETA)
#define MX_OP_SHB       (0x20 | MX_OP_SETB)
#define MX_OP_SHE       (0x20 | MX_OP_SETE)
#define MX_OP_SHC       (0x20 | MX_OP_SETC)
#define MX_OP_SHD       (0x20 | MX_OP_SETD)

/* Op table indexes */
#define MX_OP_DGTS_IDX  0
#define MX_OP_SETA_IDX  1
#define MX_OP_SETB_IDX  2
#define MX_OP_SETE_IDX  3
#define MX_OP_SETC_IDX  4
#define MX_OP_SETD_IDX  5
#define MX_OP_SHA_IDX   6
#define MX_OP_2SHA_IDX  7
#define MX_OP_3SHA_IDX  8
#define MX_OP_SHB_IDX   9
#define MX_OP_SHE_IDX   10
#define MX_OP_SHC_IDX   11
#define MX_OP_SHD_IDX   12
#define MX_OP_IDXS      13

/* Op table (order must match indexes above) */
static const struct mx_op mx_op_tab[MX_OP_IDXS] = {
    /*   op     intake */
    { MX_OP_DGTS, 9 },
    { MX_OP_SETA, 1 },
    { MX_OP_SETB, 1 },
    { MX_OP_SETE, 1 },
    { MX_OP_SETC, 1 },
    { MX_OP_SETD, 1 },
    { MX_OP_SHA,  1 },
    { MX_OP_2SHA, 2 },
    { MX_OP_3SHA, 3 },
    { MX_OP_SHB,  1 },
    { MX_OP_SHE,  1 },
    { MX_OP_SHC,  1 },
    { MX_OP_SHD,  1 },
};

/* Indexes of op sets relevant to each state - MX_OP_DGTS dealt with separately */
static const signed char mx_code_set_op_idxs[MX_STATES][8] = {
    { MX_OP_SETA_IDX, MX_OP_SHB_IDX, MX_OP_SHE_IDX, MX_OP_SHC_IDX, MX_OP_SHD_IDX, -1 }, /* MX_A */
    { MX_OP_SETB_IDX, MX_OP_SHA_IDX, MX_OP_2SHA_IDX, MX_OP_3SHA_IDX, MX_OP_SHE_IDX, MX_OP_SHC_IDX, /* MX_B */
        MX_OP_SHD_IDX, -1 },
    { MX_OP_SETE_IDX, MX_OP_SHC_IDX, MX_OP_SHD_IDX, -1 }, /* MX_E */
    { MX_OP_SETC_IDX, MX_OP_SHE_IDX, MX_OP_SHD_IDX, -1 }, /* MX_C */
    { MX_OP_SETD_IDX, MX_OP_SHE_IDX, MX_OP_SHC_IDX, -1 }, /* MX_D */
};

/* Whether can encode character `ch` with `op` - MX_OP_DGTS dealt with separately */
static int mx_can(const unsigned char op, const unsigned char ch, const int num_a) {
    if (op == MX_OP_2SHA || op == MX_OP_3SHA) {
        return num_a >= 2 + (op == MX_OP_3SHA);
    }
    return maxiCodeSet[ch] & op;
}

/* Get the symbol value for `ch` in Code Set of `op`, accounting for chars in multiple Code Sets */
static int mx_symbol_ch(const unsigned char op, const unsigned char ch) {
    if (maxiCodeSet[ch] == (op & 0x1F) || (op & MX_OP_SETA)) { /* Non-multiple or Code Set A */
        return maxiSymbolChar[ch];
    }
    if (op & MX_OP_SETB) {
        const int p = posn(" ,./:", ch);
        if (p >= 0) {
            return 47 + p;
        }
    }
    if (op & MX_OP_SETE) {
        if (ch >= 28 && ch <= 30) { /* FS GS RS */
            return ch + 4;
        }
    }
    return ch == ' ' ? 59 : ch; /* SP CR FS GS RS */
}

/* Encode according to operation `op` (note done backwards) */
static int mx_enc(unsigned char codewords[144], int ci, const unsigned char source[], const int i,
            const unsigned char op) {
    if (op == MX_OP_DGTS) {
        const int value = (source[i] - '0') * 100000000 + (source[i + 1] - '0') * 10000000
                            + (source[i + 2] - '0') * 1000000 + (source[i + 3] - '0') * 100000
                            + (source[i + 4] - '0') * 10000 + (source[i + 5] - '0') * 1000
                            + (source[i + 6] - '0') * 100 + (source[i + 7] - '0') * 10 + source[i + 8] - '0';
        assert(ci >= 6);
        codewords[--ci] = (value & 0x3F);
        codewords[--ci] = (value & 0xFC0) >> 6;
        codewords[--ci] = (value & 0x3F000) >> 12;
        codewords[--ci] = (value & 0xFC0000) >> 18;
        codewords[--ci] = (value & 0x3F000000) >> 24;
        codewords[--ci] = 31; /* NS */
    } else if (op == MX_OP_2SHA) {
        assert(ci >= 3);
        codewords[--ci] = mx_symbol_ch(op, source[i + 1]);
        codewords[--ci] = mx_symbol_ch(op, source[i]);
        codewords[--ci] = 56;
    } else if (op == MX_OP_3SHA) {
        assert(ci >= 4);
        codewords[--ci] = mx_symbol_ch(op, source[i + 2]);
        codewords[--ci] = mx_symbol_ch(op, source[i + 1]);
        codewords[--ci] = mx_symbol_ch(op, source[i]);
        codewords[--ci] = 57;
    } else {
        assert(ci >= 1);
        codewords[--ci] = mx_symbol_ch(op, source[i]);

        if (op & 0x20) { /* Shift */
            assert(ci >= 1);
            codewords[--ci] = 59 + 1 * (op == MX_OP_SHC) + 2 * (op == MX_OP_SHD) + 3 * (op == MX_OP_SHE);
        }
    }
    return ci;
}

/* Encoding length of ECI */
static int mx_eci_len(const int eci) {
    return eci == 0 ? 0 : 2 + (eci > 31) + (eci > 1023) + (eci > 32767);
}

/* Encode ECI (`eci` non-zero) */
static int mx_enc_eci(const int eci, unsigned char codewords[144], int ci) {
    codewords[--ci] = eci & 0x3F;
    if (eci > 31) {
        if (eci > 1023) {
            codewords[--ci] = (eci & 0xFC0) >> 6;
            if (eci > 32767) {
                codewords[--ci] = (eci & 0x3F000) >> 12;
                codewords[--ci] = 0x38 | ((eci & 0xC0000) >> 18);
            } else {
                codewords[--ci] = 0x30 | ((eci & 0x7000) >> 12);
            }
        } else {
            codewords[--ci] = 0x20 | ((eci & 0x3C0) >> 6);
        }
    }
    codewords[--ci] = 27; /* ECI */

    return ci;
}

/* Get the shortest encoded length for the Code Set (state) and plot the path */
static int mx_get_best_length(const int state, const int i, const unsigned char ch, const int digits, const int num_a,
            const int best_lengths[16][MX_STATES], const char best_origins[16][MX_STATES],
            unsigned char *const path_op, char *const prior_code_set) {
    const char *const latch_length_s = mx_latch_len[state];
    int min_len = 999999;
    int j;

    if (digits >= 9) { /* Nothing can beat digits */
        const int m = (i - 9) & 0x0F;
        const int org = best_origins[m][state];
        const int len = best_lengths[m][org] + latch_length_s[org] + 6;
        if (len < min_len) {
            path_op[state] = MX_OP_DGTS_IDX;
            prior_code_set[state] = org;
            min_len = len;
        }
    } else {
        const signed char *const op_idx_s = mx_code_set_op_idxs[state];
        for (j = 0; op_idx_s[j] != -1; j++) {
            const int op_idx = op_idx_s[j];
            const struct mx_op *const op = &mx_op_tab[op_idx];
            if (mx_can(op->op, ch, num_a)) {
                const int m = (i - op->intake) & 0x0F;
                const int org = best_origins[m][state];
                const int len = best_lengths[m][org] + latch_length_s[org] + op->intake + (op_idx >= MX_OP_SHA_IDX);
                if (len < min_len) {
                    path_op[state] = op_idx;
                    prior_code_set[state] = org;
                    min_len = len;
                }
            }
        }
    }
    return min_len;
}

/* Loop to get the best prior Code Set using a row of best encoded lengths */
static int mx_get_best_origin(const int state, const int *const best_length) {

    const char *const latch_length_s = mx_latch_len[state];
    int orglen = best_length[0] + latch_length_s[0];
    int best_org = 0;
    int org;

    for (org = 1; org < MX_STATES; org++) {
        const int len = best_length[org] + latch_length_s[org];
        if (len < orglen) {
            best_org = org;
            orglen = len;
        }
    }
    return best_org;
}

/* Minimal encoding using backtracking by Bue Jensen, taken from BWIPP - see
   https://github.com/bwipp/postscriptbarcode/pull/279 */
static int mx_text_process_segs(unsigned char codewords[144], const int mode, struct zint_seg segs[],
            const int seg_count, const int structapp_cw, const int scm_vv, const int debug_print) {

    /* The encoder needs 10 history rows. The circular history buffers are 16 long for convenience */
    int best_lengths[16][MX_STATES] = {0};
    char best_origins[16][MX_STATES] = {0};
    int *best_length = NULL; /* Suppress clang-tidy-20 warning */

    int cp = 20 - 9 * (mode > 3); /* Offset the initial codewords index to minimize copying */
    const int max_len = (mode == 5 ? 77 : 93) + 11; /* 11 added to adjust for above offset */
    int ci, ci_top;

    /* Backtracking information */
    const int segs_len = segs_length(segs, seg_count);
    char (*prior_code_sets)[MX_STATES] = (char (*)[MX_STATES]) z_alloca(sizeof(*prior_code_sets) * (segs_len + 9));
    unsigned char (*path_ops)[MX_STATES]
                                    = (unsigned char (*)[MX_STATES]) z_alloca(sizeof(*path_ops) * (segs_len + 9));

    int digits = 0;
    int num_a = 0;

    int min_len = 999999;
    int min_state = 0;
    int state;

    unsigned char *source_scm_vv; /* For SCM prefix `scm_vv` if any */
    int have_eci_scm = 0; /* Set if have ECI and SCM prefix */

    int seg;
    int si = 0; /* Segment offset to `source` position */
    int i, j;

    if (scm_vv != -1) { /* Add SCM prefix */
        source_scm_vv = (unsigned char *) z_alloca(segs[0].length + 9);
        sprintf((char *) source_scm_vv, "[)>\03601\035%02d", scm_vv); /* [)>\R01\Gvv */
        memcpy(source_scm_vv + 9, segs[0].source, segs[0].length);
        segs[0].source = source_scm_vv;
        segs[0].length += 9;
        have_eci_scm = segs[0].eci;
    } else if (segs[0].length >= 9 && memcmp(segs[0].source, "[)>\03601\035", 7) == 0
                && z_isdigit(segs[0].source[7]) && z_isdigit(segs[0].source[8])) {
        have_eci_scm = segs[0].eci;
    }

    /* Insert Structured Append at beginning if needed */
    if (structapp_cw) {
        codewords[cp++] = 33; /* PAD */
        codewords[cp++] = structapp_cw;
    }

    /* Make a table of best path options */
    ci = cp;
    for (seg = 0; seg < seg_count; seg++) {
        /* Suppress NS compaction for SCM prefix if have ECI so can place ECI after it when encoding */
        const int no_eci_scm_check = !have_eci_scm || seg != 0;
        const unsigned char *const source = segs[seg].source;
        const int length = segs[seg].length;
        const int eci_len = mx_eci_len(segs[seg].eci);
        if (eci_len) {
            ci += eci_len;
            if (ci > max_len) {
                return ZINT_ERROR_TOO_LONG;
            }
            digits = 0;
        }

        for (i = 0; i < length; i++) {
            const unsigned char ch = source[i];
            const int si_i = i + si;

            /* Get rows of interest */
            unsigned char *const path_op = path_ops[si_i];
            char *const prior_code_set = prior_code_sets[si_i];
            char *const best_origin = best_origins[(si_i) & 0x0F];
            best_length = best_lengths[(si_i) & 0x0F];

            /* Keep tabs on digits and characters in Code Set A */
            digits = z_isdigit(ch) && (no_eci_scm_check || i >= 9) ? digits + 1 : 0;
            num_a = maxiCodeSet[ch] & MX_OP_SETA ? num_a + 1 : 0;

            /* Get best encoded lengths, then best prior Code Sets */
            for (state = 0; state < MX_STATES; state++) {
                best_length[state] = mx_get_best_length(state, si_i, ch, digits, num_a, best_lengths, best_origins,
                                        path_op, prior_code_set);
            }
            for (state = 0; state < MX_STATES; state++) {
                best_origin[state] = mx_get_best_origin(state, best_length);
            }
        }
        si += length;
    }
    assert(best_length == best_lengths[(segs_len + 9 * (scm_vv != -1) - 1) & 0x0F]); /* Set to last char */

    /* Get the best Code Set to end with */
    for (state = 0; state < MX_STATES; state++) {
        const int len = best_length[state];
        if (len < min_len) {
            min_state = state;
            min_len = len;
        }
    }
    if (ci + min_len > max_len) {
        return ZINT_ERROR_TOO_LONG;
    }

    /* Follow the best path back to the start of the message */
    ci += min_len;
    ci_top = ci;
    state = min_state;
    for (seg = seg_count - 1; seg >= 0; seg--) {
        const unsigned char *const source = segs[seg].source;
        const int length = segs[seg].length;
        const int eci_scm_check = have_eci_scm && seg == 0;

        si -= length;
        assert(si >= 0);

        i = length;
        while (i > 0) {
            const int ch_i = (i + si) - 1;
            const int pcs = prior_code_sets[ch_i][state];
            const int op_idx = path_ops[ch_i][state];
            const struct mx_op *const op = &mx_op_tab[op_idx];

            if (eci_scm_check && i == 9) { /* Place ECI after SCM prefix */
                assert(ci >= cp + mx_eci_len(segs[0].eci));
                ci = mx_enc_eci(segs[0].eci, codewords, ci);
                segs[0].eci = 0;
            }

            i -= op->intake;
            assert(i >= 0);
            ci = mx_enc(codewords, ci, source, i, op->op);

            if (state != pcs) {
                const int latch_len = mx_latch_len[state][pcs];
                assert(ci >= cp + latch_len);
                for (j = 0; j < latch_len; j++) {
                    codewords[--ci] = mx_latch_seq[state][pcs][j];
                }
                state = pcs;
            }
        }
        if (segs[seg].eci) {
            assert(ci >= cp + mx_eci_len(segs[seg].eci));
            ci = mx_enc_eci(segs[seg].eci, codewords, ci);
        }
    }
    assert(ci == cp);

    cp = ci_top;

    /* If end in Code Set C or D, switch to A for padding */
    if (cp < max_len && (min_state == MX_C || min_state == MX_D)) {
        codewords[cp++] = 58; /* Latch A */
    }

    if (debug_print) {
        if (cp < max_len) {
            printf("Pads: %d\n", max_len - cp);
        } else {
            fputs("No Pads\n", stdout);
        }
    }

    if (cp < max_len) {
        /* Add the padding */
        memset(codewords + cp, min_state == MX_E ? 28 : 33, max_len - cp);
    }

    if (debug_print) printf("Length: %d\n", cp);

    if (cp > max_len) {
        return ZINT_ERROR_TOO_LONG;
    }

    /* Adjust the codeword array */
    if (mode > 3) {
        memcpy(codewords + 1, codewords + 20 - 9, 9); /* Primary */
    }

    return 0;
}

/* Handles error correction of primary message */
static void mx_do_primary_ecc(unsigned char codewords[144]) {
    const int datalen = 10, eclen = 10;
    unsigned char ecc[10];
    int j;
    rs_t rs;

    rs_init_gf(&rs, 0x43);
    rs_init_code(&rs, eclen, 1);

    rs_encode(&rs, datalen, codewords, ecc);

    for (j = 0; j < eclen; j++) {
        codewords[datalen + j] = ecc[j];
    }
}

/* Handles error correction of characters in secondary */
static void mx_do_secondary_ecc(unsigned char codewords[144], const int datalen, const int eclen) {
    unsigned char data[42]; /* Half max `datalen` (84) */
    unsigned char ecc[28]; /* Half max `eclen` (56) */
    int j;
    rs_t rs;

    rs_init_gf(&rs, 0x43);
    rs_init_code(&rs, eclen, 1);

    /* Even */
    for (j = 0; j < datalen; j += 2) {
        data[j >> 1] = codewords[j + 20];
    }

    rs_encode(&rs, datalen >> 1, data, ecc);

    for (j = 0; j < eclen; j++) {
        codewords[datalen + (j << 1) + 20] = ecc[j];
    }

    /* Odd */
    for (j = 0; j < datalen; j += 2) {
        data[j >> 1] = codewords[j + 1 + 20];
    }

    rs_encode(&rs, datalen >> 1, data, ecc);

    for (j = 0; j < eclen; j++) {
        codewords[datalen + (j << 1) + 1 + 20] = ecc[j];
    }
}

/* Format structured primary for Mode 2 */
static void mx_do_primary_2(unsigned char codewords[144], const unsigned char postcode[],
            const int postcode_length, const int country, const int service) {

    const int postcode_num = to_int(postcode, postcode_length);

    codewords[0] = ((postcode_num & 0x03) << 4) | 2;
    codewords[1] = ((postcode_num & 0xFC) >> 2);
    codewords[2] = ((postcode_num & 0x3F00) >> 8);
    codewords[3] = ((postcode_num & 0xFC000) >> 14);
    codewords[4] = ((postcode_num & 0x3F00000) >> 20);
    codewords[5] = ((postcode_num & 0x3C000000) >> 26) | ((postcode_length & 0x03) << 4);
    codewords[6] = ((postcode_length & 0x3C) >> 2) | ((country & 0x03) << 4);
    codewords[7] = (country & 0xFC) >> 2;
    codewords[8] = ((country & 0x300) >> 8) | ((service & 0x0F) << 2);
    codewords[9] = ((service & 0x3F0) >> 4);
}

/* Format structured primary for Mode 3 */
static void mx_do_primary_3(unsigned char codewords[144], unsigned char postcode[], const int country,
            const int service) {
    int i;

    /* Convert to Code Set A */
    for (i = 0; i < 6; i++) {
        postcode[i] = maxiSymbolChar[postcode[i]];
    }

    codewords[0] = ((postcode[5] & 0x03) << 4) | 3;
    codewords[1] = ((postcode[4] & 0x03) << 4) | ((postcode[5] & 0x3C) >> 2);
    codewords[2] = ((postcode[3] & 0x03) << 4) | ((postcode[4] & 0x3C) >> 2);
    codewords[3] = ((postcode[2] & 0x03) << 4) | ((postcode[3] & 0x3C) >> 2);
    codewords[4] = ((postcode[1] & 0x03) << 4) | ((postcode[2] & 0x3C) >> 2);
    codewords[5] = ((postcode[0] & 0x03) << 4) | ((postcode[1] & 0x3C) >> 2);
    codewords[6] = ((postcode[0] & 0x3C) >> 2) | ((country & 0x03) << 4);
    codewords[7] = (country & 0xFC) >> 2;
    codewords[8] = ((country & 0x300) >> 8) | ((service & 0x0F) << 2);
    codewords[9] = ((service & 0x3F0) >> 4);
}

INTERNAL int maxicode(struct zint_symbol *symbol, struct zint_seg segs[], const int seg_count) {
    int i, j, mode, lp = 0;
    int error_number;
    unsigned char codewords[144];
    int scm_vv = -1;
    int structapp_cw = 0;
    const int debug_print = symbol->debug & ZINT_DEBUG_PRINT;

    mode = symbol->option_1;

    if (mode <= 0) { /* If mode is unspecified (-1) or to be auto-determined (0) between 2 and 3 */
        lp = (int) strlen(symbol->primary);
        if (lp == 0) {
            if (mode == 0) { /* Require primary message to auto-determine between 2 and 3 */
                return errtxt(ZINT_ERROR_INVALID_DATA, symbol, 554, "Primary Message empty");
            }
            mode = 4;
        } else {
            mode = 2;
            for (i = 0; i < lp - 6; i++) {
                if (!z_isdigit(symbol->primary[i]) && (symbol->primary[i] != ' ')) {
                    mode = 3;
                    break;
                }
            }
        }
    }

    if (mode < 2 || mode > 6) { /* Only codes 2 to 6 supported */
        return errtxtf(ZINT_ERROR_INVALID_OPTION, symbol, 550, "Mode '%d' out of range (2 to 6)", mode);
    }

    if (mode <= 3) { /* Modes 2 and 3 need data in symbol->primary */
        unsigned char postcode[10];
        int countrycode;
        int service;
        int postcode_len;
        if (lp == 0) { /* Mode set manually means lp doesn't get set */
            lp = (int) strlen(symbol->primary);
        }
        if (lp < 7 || lp > 15) { /* 1 to 9 character postcode + 3 digit country code + 3 digit service class */
            if (lp == 0) {
                return errtxt(ZINT_ERROR_INVALID_DATA, symbol, 548, "Primary Message empty");
            }
            return errtxtf(ZINT_ERROR_INVALID_DATA, symbol, 551, "Primary Message length %d wrong (7 to 15 only)",
                            lp);
        }
        postcode_len = lp - 6;

        countrycode = to_int((const unsigned char *) (symbol->primary + postcode_len), 3);
        service = to_int((const unsigned char *) (symbol->primary + postcode_len + 3), 3);

        if (countrycode == -1 || service == -1) { /* Check that country code and service are numeric */
            return errtxt(ZINT_ERROR_INVALID_DATA, symbol, 552,
                            "Non-numeric country code or service class in Primary Message");
        }

        memcpy(postcode, symbol->primary, postcode_len);
        postcode[postcode_len] = '\0';

        if (mode == 2) {
            for (i = 0; i < postcode_len; i++) {
                if (postcode[i] == ' ') {
                    postcode[i] = '\0';
                    postcode_len = i;
                    break;
                }
                if (!z_isdigit(postcode[i])) {
                    return errtxt(ZINT_ERROR_INVALID_DATA, symbol, 555, "Non-numeric postcode in Primary Message");
                }
            }
            if (countrycode == 840 && postcode_len == 5) {
                /* Annex B, section B.1, paragraph 4.a, "In the case of country code 840, if the "+4" is unknown,
                   then fill with zeroes" (adapted from OkaiBarcode, stricter interpretation, props Daniel Gredler) */
                memcpy(postcode + 5, "0000", 5); /* Include NUL char */
                postcode_len = 9;
            }
            mx_do_primary_2(codewords, postcode, postcode_len, countrycode, service);
        } else {
            /* Just truncate and space-pad */
            postcode[6] = '\0';
            for (i = postcode_len; i < 6; i++) {
                postcode[i] = ' ';
            }
            /* Upper-case and check for Code Set A characters only */
            to_upper(postcode, postcode_len);
            for (i = 0; i < 6; i++) {
                /* Don't allow control chars (CR FS GS RS for Code Set A) */
                if (postcode[i] < ' ' || !(maxiCodeSet[postcode[i]] & MX_OP_SETA)) {
                    return errtxt(ZINT_ERROR_INVALID_DATA, symbol, 556,
                                    "Invalid character in postcode in Primary Message");
                }
            }
            mx_do_primary_3(codewords, postcode, countrycode, service);
        }

        if (symbol->option_2) { /* Check for option_2 = vv + 1, where vv is version of SCM prefix "[)>\R01\Gvv" */
            if (symbol->option_2 < 0 || symbol->option_2 > 100) {
                return errtxtf(ZINT_ERROR_INVALID_OPTION, symbol, 557,
                                "SCM prefix version '%d' out of range (1 to 100)", symbol->option_2);
            }
            if (symbol->eci == 25 || (symbol->eci >= 33 && symbol->eci <= 35)) { /* UTF-16/32 */
                return errtxtf(ZINT_ERROR_INVALID_OPTION, symbol, 547,
                                "SCM prefix can not be used with ECI %d (ECI must be ASCII compatible)", symbol->eci);
            }
            scm_vv = symbol->option_2 - 1;
        }

        if (debug_print) {
            printf("Postcode: %s, Country Code: %d, Service Class: %d\n", postcode, countrycode, service);
        }
    } else {
        codewords[0] = mode;
    }

    if (debug_print) {
        printf("Mode: %d\n", mode);
    }

    if (symbol->structapp.count) {
        if (symbol->structapp.count < 2 || symbol->structapp.count > 8) {
            return errtxtf(ZINT_ERROR_INVALID_OPTION, symbol, 558,
                            "Structured Append count '%d' out of range (2 to 8)", symbol->structapp.count);
        }
        if (symbol->structapp.index < 1 || symbol->structapp.index > symbol->structapp.count) {
            return errtxtf(ZINT_ERROR_INVALID_OPTION, symbol, 559,
                            "Structured Append index '%1$d' out of range (1 to count %2$d)",
                            symbol->structapp.index, symbol->structapp.count);
        }
        if (symbol->structapp.id[0]) {
            return errtxt(ZINT_ERROR_INVALID_OPTION, symbol, 549, "Structured Append ID not available for MaxiCode");
        }
        structapp_cw = (symbol->structapp.count - 1) | ((symbol->structapp.index - 1) << 3);
    }

    error_number = mx_text_process_segs(codewords, mode, segs, seg_count, structapp_cw, scm_vv, debug_print);
    if (error_number == ZINT_ERROR_TOO_LONG) {
        return errtxt(error_number, symbol, 553, "Input too long, requires too many codewords (maximum 144)");
    }

    /* All the data is sorted - now do error correction */
    mx_do_primary_ecc(codewords); /* Always Enhanced ECC (EEC) 10 data + 10 error correction */

    if (mode == 5) {
        /* Enhanced ECC (EEC) 68 data + 56 error correction */
        mx_do_secondary_ecc(codewords, 68, 28); /* ECC halved for even/odd */
    } else {
        /* Standard ECC (SEC) 84 data + 40 error correction */
        mx_do_secondary_ecc(codewords, 84, 20); /* ECC halved for even/odd */
    }

    if (debug_print) {
        fputs("Codewords:", stdout);
        for (i = 0; i < 144; i++) printf(" %d", codewords[i]);
        fputc('\n', stdout);
    }
#ifdef ZINT_TEST
    if (symbol->debug & ZINT_DEBUG_TEST) {
        debug_test_codeword_dump(symbol, codewords, 144);
    }
#endif

    /* Copy data into symbol grid */
    for (i = 0; i < 33; i++) {
        for (j = 0; j < 30; j++) {
            const int mod_seq = maxiGrid[(i * 30) + j] + 5;
            const int block = mod_seq / 6;

            if (block != 0) {
                if ((codewords[block - 1] >> (5 - (mod_seq % 6))) & 1) {
                    set_module(symbol, i, j);
                }
            }
        }
    }

    /* Add orientation markings */
    set_module(symbol, 0, 28); /* Top right filler */
    set_module(symbol, 0, 29);
    set_module(symbol, 9, 10); /* Top left marker */
    set_module(symbol, 9, 11);
    set_module(symbol, 10, 11);
    set_module(symbol, 15, 7); /* Left hand marker */
    set_module(symbol, 16, 8);
    set_module(symbol, 16, 20); /* Right hand marker */
    set_module(symbol, 17, 20);
    set_module(symbol, 22, 10); /* Bottom left marker */
    set_module(symbol, 23, 10);
    set_module(symbol, 22, 17); /* Bottom right marker */
    set_module(symbol, 23, 17);

    symbol->width = 30;
    symbol->rows = 33;

    /* Note MaxiCode fixed size so symbol height ignored but set anyway */
    (void) set_height(symbol, 5.0f, 0.0f, 0.0f, 1 /*no_errtxt*/);

    return error_number;
}

/* vim: set ts=4 sw=4 et : */
