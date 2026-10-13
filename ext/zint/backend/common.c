/* common.c - Contains functions needed for a number of barcodes */
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

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include "common.h"

/* Converts a character 0-9, A-F to its equivalent integer value */
INTERNAL int ctoi(const char source) {
    if (z_isdigit(source))
        return (source - '0');
    if ((source >= 'A') && (source <= 'F'))
        return (source - 'A' + 10);
    if ((source >= 'a') && (source <= 'f'))
        return (source - 'a' + 10);
    return -1;
}

/* Converts decimal string of length <= 9 to integer value. Returns -1 if not numeric */
INTERNAL int to_int(const unsigned char source[], const int length) {
    int val = 0;
    int non_digit = 0;
    int i;

    for (i = 0; i < length; i++) {
        val *= 10;
        val += source[i] - '0';
        non_digit |= !z_isdigit(source[i]);
    }

    return non_digit ? -1 : val;
}

/* Converts lower case characters to upper case in string `source` */
INTERNAL void to_upper(unsigned char source[], const int length) {
    int i;

    for (i = 0; i < length; i++) {
        source[i] &= z_islower(source[i]) ? 0x5F : 0xFF;
    }
}

/* Returns the number of times a character occurs in `source` */
INTERNAL int chr_cnt(const unsigned char source[], const int length, const unsigned char c) {
    int count = 0;
    int i;
    for (i = 0; i < length; i++) {
        count += source[i] == c;
    }
    return count;
}

/* Flag table for `is_chr()` and `not_sane()` */
#define IS_CLS_F    (IS_CLI_F | IS_SIL_F)
static const unsigned short flgs[256] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /*00-1F*/
               IS_SPC_F,            IS_C82_F,            IS_C82_F,            IS_HSH_F, /*20-23*/ /*  !"# */
               IS_CLS_F, IS_SIL_F | IS_C82_F,            IS_C82_F,            IS_C82_F, /*24-27*/ /* $%&' */
               IS_C82_F,            IS_C82_F,            IS_AST_F,            IS_PLS_F, /*28-2B*/ /* ()*+ */
               IS_C82_F,            IS_MNS_F, IS_CLS_F | IS_C82_F, IS_CLS_F | IS_C82_F, /*2B-2F*/ /* ,-./ */
               IS_NUM_F,            IS_NUM_F,            IS_NUM_F,            IS_NUM_F, /*30-33*/ /* 0123 */
               IS_NUM_F,            IS_NUM_F,            IS_NUM_F,            IS_NUM_F, /*34-37*/ /* 4567 */
               IS_NUM_F,            IS_NUM_F, IS_CLI_F | IS_C82_F,            IS_C82_F, /*38-3B*/ /* 89:; */
               IS_C82_F,            IS_C82_F,            IS_C82_F,            IS_C82_F, /*3B-3F*/ /* <=>? */
                      0, IS_UHX_F | IS_ARS_F, IS_UHX_F | IS_ARS_F, IS_UHX_F | IS_ARS_F, /*40-43*/ /* @ABC */
    IS_UHX_F | IS_ARS_F, IS_UHX_F | IS_ARS_F, IS_UHX_F | IS_ARS_F, IS_UPO_F | IS_ARS_F, /*44-47*/ /* DEFG */
    IS_UPO_F | IS_ARS_F,            IS_UPO_F, IS_UPO_F | IS_ARS_F, IS_UPO_F | IS_ARS_F, /*48-4B*/ /* HIJK */
    IS_UPO_F | IS_ARS_F, IS_UPO_F | IS_ARS_F, IS_UPO_F | IS_ARS_F,            IS_UPO_F, /*4B-4F*/ /* LMNO */
    IS_UPO_F | IS_ARS_F,            IS_UPO_F, IS_UPO_F | IS_ARS_F, IS_UPO_F | IS_ARS_F, /*50-53*/ /* PQRS */
    IS_UPO_F | IS_ARS_F, IS_UPO_F | IS_ARS_F, IS_UPO_F | IS_ARS_F, IS_UPO_F | IS_ARS_F, /*53-57*/ /* TUVW */
    IS_UX__F | IS_ARS_F, IS_UPO_F | IS_ARS_F, IS_UPO_F | IS_ARS_F,                   0, /*58-5B*/ /* XYZ[ */
                      0,                   0,                   0,            IS_C82_F, /*5B-5F*/ /* \]^_ */
                      0,            IS_LHX_F,            IS_LHX_F,            IS_LHX_F, /*60-63*/ /* `abc */
               IS_LHX_F,            IS_LHX_F,            IS_LHX_F,            IS_LWO_F, /*64-67*/ /* defg */
               IS_LWO_F,            IS_LWO_F,            IS_LWO_F,            IS_LWO_F, /*68-6B*/ /* hijk */
               IS_LWO_F,            IS_LWO_F,            IS_LWO_F,            IS_LWO_F, /*6B-6F*/ /* lmno */
               IS_LWO_F,            IS_LWO_F,            IS_LWO_F,            IS_LWO_F, /*70-73*/ /* pqrs */
               IS_LWO_F,            IS_LWO_F,            IS_LWO_F,            IS_LWO_F, /*74-77*/ /* tuvw */
               IS_LX__F,            IS_LWO_F,            IS_LWO_F,                   0, /*78-7B*/ /* xyz{ */
                      0,                   0,                   0,                   0, /*7B-7F*/ /* |}~D */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /*80-9F*/
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /*A0-BF*/
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /*C0-DF*/
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /*E0-FF*/
};

/* Whether a character matches `flg` */
INTERNAL int is_chr(const unsigned int flg, const unsigned int c) {
    return c < 0x80 && (flgs[c] & flg) != 0;
}

/* Verifies if a string only uses valid characters, returning 1-based position in `source` if not, 0 for success */
INTERNAL int not_sane(const unsigned int flg, const unsigned char source[], const int length) {
    int i;

    for (i = 0; i < length; i++) {
        if (!(flgs[source[i]] & flg)) {
            return i + 1;
        }
    }
    return 0;
}

/* Replaces huge switch statements for looking up in tables */
/* Verifies if a string only uses valid characters as above, but also returns `test_string` position of each in
   `posns` array */
INTERNAL int not_sane_lookup(const char test_string[], const int test_length, const unsigned char source[],
                const int length, int *posns) {
    int i, j;

    for (i = 0; i < length; i++) {
        posns[i] = -1;
        for (j = 0; j < test_length; j++) {
            if (source[i] == test_string[j]) {
                posns[i] = j;
                break;
            }
        }
        if (posns[i] == -1) {
            return i + 1;
        }
    }

    return 0;
}

/* Returns the position of `data` in `set_string`, or -1 if not found */
INTERNAL int posn(const char set_string[], const char data) {
    const char *s;

    for (s = set_string; *s; s++) {
        if (data == *s) {
            return s - set_string;
        }
    }
    return -1;
}

/* Converts `arg` to a string representing its binary equivalent of length `length` and places in `binary` at
  `bin_posn`. Returns `bin_posn` + `length` */
INTERNAL int bin_append_posn(const int arg, const int length, char *binary, const int bin_posn) {
    int i;
    const int end = length - 1;

    for (i = 0; i < length; i++) {
        binary[bin_posn + i] = '0' + ((arg >> (end - i)) & 1);
    }
    return bin_posn + length;
}

#ifndef Z_COMMON_INLINE

/* Returns true (1) if a module is dark/black, otherwise false (0) */
INTERNAL int module_is_set(const struct zint_symbol *symbol, const int y_coord, const int x_coord) {
    return (symbol->encoded_data[y_coord][x_coord >> 3] >> (x_coord & 0x07)) & 1;
}

/* Sets a module to dark/black */
INTERNAL void set_module(struct zint_symbol *symbol, const int y_coord, const int x_coord) {
    symbol->encoded_data[y_coord][x_coord >> 3] |= 1 << (x_coord & 0x07);
}

/* Returns true (1-8) if a module is colour, otherwise false (0) */
INTERNAL int module_colour_is_set(const struct zint_symbol *symbol, const int y_coord, const int x_coord) {
    return symbol->encoded_data[y_coord][x_coord];
}

/* Sets a module to a colour */
INTERNAL void set_module_colour(struct zint_symbol *symbol, const int y_coord, const int x_coord, const int colour) {
    symbol->encoded_data[y_coord][x_coord] = colour;
}

/* Sets a dark/black module to white (i.e. unsets) */
INTERNAL void unset_module(struct zint_symbol *symbol, const int y_coord, const int x_coord) {
    symbol->encoded_data[y_coord][x_coord >> 3] &= ~(1 << (x_coord & 0x07));
}

#endif /* Z_COMMON_INLINE */

/* Expands from a width pattern to a bit pattern */
INTERNAL void expand(struct zint_symbol *symbol, const char data[], const int length) {

    int reader;
    int writer = 0;
    int latch = 1;
    const int row = symbol->rows;

    symbol->rows++;

    for (reader = 0; reader < length; reader++) {
        int i;
        const int num = data[reader] - '0';
        assert(num >= 0);
        for (i = 0; i < num; i++) {
            if (latch) {
                set_module(symbol, row, writer);
            }
            writer++;
        }

        latch = !latch;
    }

    if (writer > symbol->width) {
        symbol->width = writer;
    }
}

/* Helper for `errtxt()` & `errtxtf()` to set "err_id: " part of error message, returning length */
static int errtxt_id_str(char *errtxt, int num) {
    int len = 0;
    if (num == -1) {
        errtxt[0] = '\0';
        return 0;
    }
    if (num < 0 || num > 9999) { /* Restrict to 4 digits */
        num = 9999;
    }
    if (num >= 1000) {
        errtxt[len++] = '0' + (num / 1000);
        num %= 1000;
    }
    errtxt[len++] = '0' + (num / 100);
    num %= 100;
    errtxt[len++] = '0' + (num / 10);
    num %= 10;
    errtxt[len++] = '0' + num;
    errtxt[len++] = ':';
    errtxt[len++] = ' ';
    return len;
}

/* Set `symbol->errtxt` to "err_id: msg", returning `error_number`. If `err_id` is -1, the "err_id: " prefix is
   omitted */
INTERNAL int errtxt(const int error_number, struct zint_symbol *symbol, const int err_id, const char *msg) {
    const int max_len = (int) sizeof(symbol->errtxt) - 1;
    const int id_len = errtxt_id_str(symbol->errtxt, err_id);
    int msg_len = (int) strlen(msg);

    if (id_len + msg_len > max_len) {
        if (!(symbol->debug & ZINT_DEBUG_TEST)) assert(0); /* Catch truncations */
        msg_len = max_len - id_len;
    }
    memcpy(symbol->errtxt + id_len, msg, msg_len);

    symbol->errtxt[id_len + msg_len] = '\0';

    return error_number;
}

static int errtxtf_dpad(const char *fmt); /* Forward reference */

/* Helper for `errtxtf()` to parse numbered specifier "n$" (where "n" 1-9), returning `fmt` advance increment */
static int errtxtf_num_arg(const char *fmt, int *p_arg) {
    int ret = 0;
    int arg = -2;
    if (!errtxtf_dpad(fmt) && z_isdigit(fmt[0])) {
        arg = fmt[1] == '$' ? fmt[0] - '0' - 1 : -1;
        ret = 2;
    }
    if (p_arg) {
        *p_arg = arg;
    }
    return ret;
}

/* Helper for `errtxtf()` to parse length precision, returning `fmt` advance increment */
static int errtxtf_slen(const char *fmt, const int arg, int *p_arg_cnt, int *p_len) {
    int ret = 0;
    int len = -1;
    if (fmt[0] == '.') {
        if (z_isdigit(fmt[1]) && fmt[1] != '0') {
            len = fmt[1] - '0';
            for (ret = 2; z_isdigit(fmt[ret]); ret++) {
                len = len * 10 + fmt[ret] - '0';
            }
            if (fmt[ret] != 's') {
                len = -1;
            }
        } else if (fmt[1] == '*' && fmt[2] == 's' && arg < 0) {
            len = 0;
            ret = 2;
        } else if (fmt[1] == '*' && z_isdigit(fmt[2]) && fmt[3] == '$' && fmt[4] == 's') {
            if (arg == -1 || arg == fmt[2] - '0') {
                len = 0;
                if (p_arg_cnt) {
                    (*p_arg_cnt)++;
                }
            }
            ret = 4;
        } else {
            ret = 1;
        }
    }
    if (p_len) {
        *p_len = len;
    }
    return ret;
}

/* Helper for `errtxtf()` to parse zero-padded minimum field length for "%d", returning `fmt` advance increment */
static int errtxtf_dpad(const char *fmt) {
    /* Allow one leading zero plus one or two digits only */
    if (fmt[0] == '0' && z_isdigit(fmt[1])) {
        if (fmt[1] != '0' && fmt[2] == 'd') {
            return 2;
        }
        if (z_isdigit(fmt[1]) && fmt[1] != '0' && z_isdigit(fmt[2]) && fmt[3] == 'd') {
            return 3;
        }
    }
    return 0;
}

/* Set `symbol->errtxt` to "err_id: msg" with restricted subset of `printf()` formatting, returning `error_number`.
   If `err_id` is -1, the "err_id: " prefix is omitted. Only the following specifiers are supported: "c", "d", "f",
   "g" and "s", with no modifiers apart from "<n>$" numbering for l10n ("<n>" 1-9), in which case all specifiers must
   be numbered, "%s" with length precisions: "%.*s", "%<n+1>$.*<n>$s", "%.<p>s" and "%<n>$.<p>s", and "%d" with
   zero-padded minimum field lengths: "%0<m>d" or %<n>$0<m>d" ("<m>" 1-99) */
INTERNAL int errtxtf(const int error_number, struct zint_symbol *symbol, const int err_id, const char *fmt, ...) {
    const int max_len = (int) sizeof(symbol->errtxt) - 1;
    int p = errtxt_id_str(symbol->errtxt, err_id);
    const char *f;
    int i;
    int arg_cnt = 0;
    int have_num_arg = 0, have_unnum_arg = 0;
    va_list ap;
    int idxs[9] = {0}; /* Argument order */
    char specs[9] = {0}; /* Format specifiers */
    const char *ss[9] = { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL }; /* "%s" */
    int slens[9] = {0}; /* "%s" length precisions */
    int have_slens[9] = {0}; /* Bools for if "%s" has length precision */
    char dpads[9][3] = {{0}}; /* 2-digit minimum field length */
    char dfgs[9][100] = {{0}}; /* "%d", "%f" and "%g", allowing for padding up to 99 */
    int cs[9] = {0}; /* "%c" */

    /* Get argument order and specifiers */
    for (f = fmt, i = 0; *f; f++) {
        if (*f == '%') {
            int inc, arg, len;
            if (*++f == '%') {
                continue;
            }
            if ((inc = errtxtf_num_arg(f, &arg))) {
                if (arg == -1) {
                    if (!(symbol->debug & ZINT_DEBUG_TEST)) assert(0);
                    return errtxt(ZINT_ERROR_ENCODING_PROBLEM, symbol, 0,
                                    "Internal error: invalid numbered format specifer");
                }
                if (i >= 9) {
                    if (!(symbol->debug & ZINT_DEBUG_TEST)) assert(0);
                    return errtxt(ZINT_ERROR_ENCODING_PROBLEM, symbol, 0,
                                    "Internal error: too many format specifiers (9 maximum)");
                }
                f += inc;
                have_num_arg = 1;
                idxs[i] = arg;
            } else {
                if (i >= 9) {
                    if (!(symbol->debug & ZINT_DEBUG_TEST)) assert(0);
                    return errtxt(ZINT_ERROR_ENCODING_PROBLEM, symbol, 0,
                                    "Internal error: too many format specifiers (9 maximum)");
                }
                have_unnum_arg = 1;
                idxs[i] = i;
            }
            if ((inc = errtxtf_slen(f, arg, &arg_cnt, &len))) {
                if (len == -1) {
                    if (!(symbol->debug & ZINT_DEBUG_TEST)) assert(0);
                    return errtxt(ZINT_ERROR_ENCODING_PROBLEM, symbol, 0, "Internal error: invalid length precision");
                }
                slens[idxs[i]] = len == 0 ? -1 : len; /* TODO: keep `slens` separate else last mentioned trumps */
                have_slens[idxs[i]] = 1;
                f += inc;
            }
            if ((inc = errtxtf_dpad(f))) {
                memcpy(dpads[idxs[i]], f + 1, inc - 1); /* TODO: keep `dpads` separate else last mentioned trumps */
                dpads[idxs[i]][inc - 1] = '\0';
                f += inc;
            }
            if (*f != 'c' && *f != 'd' && *f != 'f' && *f != 'g' && *f != 's') {
                if (!(symbol->debug & ZINT_DEBUG_TEST)) assert(0);
                return errtxt(ZINT_ERROR_ENCODING_PROBLEM, symbol, 0,
                                "Internal error: unknown format specifier ('%c','%d','%f','%g','%s' only)");
            }
            specs[idxs[i++]] = *f;
            arg_cnt++;
        }
    }
    if (have_num_arg && have_unnum_arg) {
        if (!(symbol->debug & ZINT_DEBUG_TEST)) assert(0);
        return errtxt(ZINT_ERROR_ENCODING_PROBLEM, symbol, 0,
                        "Internal error: mixed numbered and unnumbered format specifiers");
    }

    /* Get arguments */
    va_start(ap, fmt);
    for (i = 0; i < arg_cnt; i++) {
        if (specs[i] == 'c') {
            cs[i] = va_arg(ap, int);
        } else if (specs[i] == 'd') {
            if (dpads[i][0]) {
                char dpad_fmt[30]; /* Make 30 to suppress gcc 14 "-Wformat-overflow=" false positive */
                sprintf(dpad_fmt, "%%0%sd", dpads[i]); /* TODO: keep `dpads` separate else last mentioned trumps */
                sprintf(dfgs[i], dpad_fmt, va_arg(ap, int));
            } else {
                sprintf(dfgs[i], "%d", va_arg(ap, int));
            }
        } else if (specs[i] == 'f' || specs[i] == 'g') {
            sprintf(dfgs[i], specs[i] == 'f' ? "%f" : "%g", va_arg(ap, double));
        } else if (specs[i] == 's') {
            if (have_slens[i] && slens[i] == -1) {
                slens[i] = va_arg(ap, int); /* TODO: keep `slens` separate else last mentioned trumps */
            }
            ss[i] = va_arg(ap, char *);
        }
    }
    va_end(ap);

    /* Populate `errtxt` */
    for (f = fmt, i = 0; *f && p < max_len; f++) {
        if (*f == '%') {
            int idx;
            if (*++f == '%') {
                symbol->errtxt[p++] = '%';
                continue;
            }
            f += errtxtf_num_arg(f, NULL /*p_arg*/);
            f += errtxtf_slen(f, -1 /*arg*/, NULL /*arg_cnt*/, NULL /*p_len*/);
            f += errtxtf_dpad(f);
            idx = idxs[i];
            if (specs[idx] == 'c') {
                symbol->errtxt[p++] = cs[idx];
            } else {
                int len;
                if (specs[idx] == 's') {
                    if (have_slens[idx]) {
                        const char *si = ss[idx];
                        for (len = 0; len < slens[idx] && si[len]; len++);
                    } else {
                        len = (int) strlen(ss[idx]);
                    }
                } else {
                    len = (int) strlen(dfgs[idx]);
                }
                if (len) {
                    if (p + len > max_len) {
                        if (!(symbol->debug & ZINT_DEBUG_TEST)) assert(0); /* Catch truncations */
                        len = max_len - p;
                    }
                    memcpy(symbol->errtxt + p, specs[idx] == 's' ? ss[idx] : dfgs[idx], len);
                    p += len;
                }
            }
            i++;
        } else {
            symbol->errtxt[p++] = *f;
        }
    }
    if (*f) {
        if (!(symbol->debug & ZINT_DEBUG_TEST)) assert(0); /* Catch truncations */
    }

    symbol->errtxt[p] = '\0';

    return error_number;
}

/* Helper to prepend/append to existing `symbol->errtxt` by calling `errtxtf(fmt)` with 2 arguments (copy of `errtxt`
   & `msg`) if `msg` not NULL, or 1 argument (just copy of `errtxt`) if `msg` NULL, returning `error_number` */
INTERNAL int errtxt_adj(const int error_number, struct zint_symbol *symbol, const char *fmt, const char *msg) {
    char err_buf[sizeof(symbol->errtxt)];

    err_buf[0] = '\0';

/* Suppress gcc 14 warning output may be truncated */
#if defined(__GNUC__) && !defined(__clang__) && __GNUC__ >= 14
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstringop-truncation"
#endif

    strncat(err_buf, symbol->errtxt, sizeof(symbol->errtxt) - 1);

#if defined(__GNUC__) && !defined(__clang__) && __GNUC__ >= 14
#pragma GCC diagnostic pop
#endif

    if (msg) {
        errtxtf(0, symbol, -1, fmt, err_buf, msg);
    } else {
        errtxtf(0, symbol, -1, fmt, err_buf);
    }

    return error_number;
}

/* Whether `symbology` can have row binding */
INTERNAL int is_stackable(const int symbology) {
    if (symbology < BARCODE_PHARMA_TWO && symbology != BARCODE_POSTNET) {
        return 1;
    }

    switch (symbology) {
        case BARCODE_CODE128AB:
        case BARCODE_ISBNX:
        case BARCODE_EAN14:
        case BARCODE_NVE18:
        case BARCODE_KOREAPOST:
        case BARCODE_PLESSEY:
        case BARCODE_TELEPEN_NUM:
        case BARCODE_ITF14:
        case BARCODE_CODE32:
        case BARCODE_CODABLOCKF:
        case BARCODE_HIBC_BLOCKF:
            return 1;
            break;
    }

    return 0;
}

/* Whether `symbology` is EAN/UPC */
INTERNAL int is_upcean(const int symbology) {

    switch (symbology) {
        case BARCODE_EANX:
        case BARCODE_EANX_CHK:
        case BARCODE_UPCA:
        case BARCODE_UPCA_CHK:
        case BARCODE_UPCE:
        case BARCODE_UPCE_CHK:
        case BARCODE_ISBNX:
        case BARCODE_EANX_CC:
        case BARCODE_UPCA_CC:
        case BARCODE_UPCE_CC:
            return 1;
            break;
    }

    return 0;
}

/* Whether `symbology` can have composite 2D component data */
INTERNAL int is_composite(const int symbology) {
    return symbology >= BARCODE_EANX_CC && symbology <= BARCODE_DBAR_EXPSTK_CC;
}

/* Whether `symbology` is a matrix design renderable as dots */
INTERNAL int is_dotty(const int symbology) {

    switch (symbology) {
        /* Note MAXICODE and ULTRA absent */
        case BARCODE_QRCODE:
        case BARCODE_DATAMATRIX:
        case BARCODE_MICROQR:
        case BARCODE_HIBC_DM:
        case BARCODE_AZTEC:
        case BARCODE_HIBC_QR:
        case BARCODE_HIBC_AZTEC:
        case BARCODE_AZRUNE:
        case BARCODE_CODEONE:
        case BARCODE_GRIDMATRIX:
        case BARCODE_HANXIN:
        case BARCODE_MAILMARK_2D:
        case BARCODE_DOTCODE:
        case BARCODE_UPNQR:
        case BARCODE_RMQR:
            return 1;
            break;
    }

    return 0;
}

/* Whether `symbology` has a fixed aspect ratio (matrix design) */
INTERNAL int is_fixed_ratio(const int symbology) {

    if (is_dotty(symbology)) {
        return 1;
    }

    switch (symbology) {
        case BARCODE_MAXICODE:
        case BARCODE_ULTRA:
            return 1;
            break;
    }

    return 0;
}

/* Whether next two characters are digits */
INTERNAL int is_twodigits(const unsigned char source[], const int length, const int position) {
    if ((position + 1 < length) && z_isdigit(source[position]) && z_isdigit(source[position + 1])) {
        return 1;
    }

    return 0;
}

/* Returns how many consecutive digits lie immediately ahead up to `max`, or all if `max` is -1 */
INTERNAL int cnt_digits(const unsigned char source[], const int length, const int position, const int max) {
    int i;
    const int max_length = max == -1 || position + max > length ? length : position + max;

    for (i = position; i < max_length && z_isdigit(source[i]); i++);

    return i - position;
}

/* State machine to decode UTF-8 to Unicode codepoints (state 0 means done, state 12 means error) */
INTERNAL unsigned int decode_utf8(unsigned int *state, unsigned int *codep, const unsigned char byte) {
    /*
        Copyright (c) 2008-2009 Bjoern Hoehrmann <bjoern@hoehrmann.de>

        Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated
        documentation files (the "Software"), to deal in the Software without restriction, including without
        limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the
        Software, and to permit persons to whom the Software is furnished to do so, subject to the following
        conditions:

        The above copyright notice and this permission notice shall be included in all copies or substantial portions
        of the Software.

        See https://bjoern.hoehrmann.de/utf-8/decoder/dfa/ for details.
     */

    static const unsigned char utf8d[] = {
        /* The first part of the table maps bytes to character classes that
         * reduce the size of the transition table and create bitmasks. */
         0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
         0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
         0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
         0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
         1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,  9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,
         7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,  7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
         8,8,2,2,2,2,2,2,2,2,2,2,2,2,2,2,  2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
        10,3,3,3,3,3,3,3,3,3,3,3,3,4,3,3, 11,6,6,6,5,8,8,8,8,8,8,8,8,8,8,8,

        /* The second part is a transition table that maps a combination
         * of a state of the automaton and a character class to a state. */
         0,12,24,36,60,96,84,12,12,12,48,72, 12,12,12,12,12,12,12,12,12,12,12,12,
        12, 0,12,12,12,12,12, 0,12, 0,12,12, 12,24,12,12,12,12,12,24,12,24,12,12,
        12,12,12,12,12,12,12,24,12,12,12,12, 12,24,12,12,12,12,12,12,12,24,12,12,
        12,12,12,12,12,12,12,36,12,36,12,12, 12,36,12,12,12,12,12,36,12,36,12,12,
        12,36,12,12,12,12,12,12,12,12,12,12,
    };

    const unsigned int type = utf8d[byte];

    *codep = *state != 0 ? (byte & 0x3fu) | (*codep << 6) : (0xff >> type) & byte;

    *state = utf8d[256 + *state + type];

    return *state;
}

/* Is string valid UTF-8? */
INTERNAL int is_valid_utf8(const unsigned char source[], const int length) {
    int i;
    unsigned int codepoint, state = 0;

    for (i = 0; i < length; i++) {
        if (decode_utf8(&state, &codepoint, source[i]) == 12) {
            return 0;
        }
    }

    return state == 0;
}

/* Converts UTF-8 to Unicode. If `disallow_4byte` unset, allows all values (UTF-32). If `disallow_4byte` set,
 * only allows codepoints <= U+FFFF (ie four-byte sequences not allowed) (UTF-16, no surrogates) */
INTERNAL int utf8_to_unicode(struct zint_symbol *symbol, const unsigned char source[], unsigned int vals[],
                int *length, const int disallow_4byte) {
    int bpos;
    int jpos;
    unsigned int codepoint, state = 0;

    bpos = 0;
    jpos = 0;

    while (bpos < *length) {
        do {
            decode_utf8(&state, &codepoint, source[bpos++]);
        } while (bpos < *length && state != 0 && state != 12);

        if (state != 0) {
            strcpy(symbol->errtxt, "240: Corrupt Unicode data");
            return ZINT_ERROR_INVALID_DATA;
        }
        if (disallow_4byte && codepoint > 0xffff) {
            strcpy(symbol->errtxt, "242: Unicode sequences of more than 3 bytes not supported");
            return ZINT_ERROR_INVALID_DATA;
        }

        vals[jpos] = codepoint;
        jpos++;
    }

    *length = jpos;

    return 0;
}

/* Treats source as ISO/IEC 8859-1 and copies into `symbol->text`, converting to UTF-8. Control chars (incl. DEL) and
   non-ISO/IEC 8859-1 (0x80-9F) are replaced with spaces. Returns warning if truncated, else 0 */
INTERNAL int hrt_cpy_iso8859_1(struct zint_symbol *symbol, const unsigned char source[], const int length) {
    int i, j;
    int warn_number = 0;

    for (i = 0, j = 0; i < length && j < (int) sizeof(symbol->text); i++) {
        if (source[i] < 0x80) {
            symbol->text[j++] = source[i] >= ' ' && source[i] != 0x7F ? source[i] : ' ';
        } else if (source[i] < 0xC0) {
            if (source[i] >= 0xA0) { /* 0x80-0x9F not valid ISO/IEC 8859-1 */
                if (j + 2 >= (int) sizeof(symbol->text)) {
                    warn_number = ZINT_WARN_HRT_TRUNCATED;
                    break;
                }
                symbol->text[j++] = 0xC2;
                symbol->text[j++] = source[i];
            } else {
                symbol->text[j++] = ' ';
            }
        } else {
            if (j + 2 >= (int) sizeof(symbol->text)) {
                warn_number = ZINT_WARN_HRT_TRUNCATED;
                break;
            }
            symbol->text[j++] = 0xC3;
            symbol->text[j++] = source[i] - 0x40;
        }
    }
    if (j == sizeof(symbol->text)) {
        warn_number = ZINT_WARN_HRT_TRUNCATED;
        j--;
    }
    symbol->text[j] = '\0';

    if (warn_number) {
        errtxt(0, symbol, 249, "Human Readable Text truncated");
    }
    return warn_number;
}

/* Sets symbol height, returning a warning if not within minimum and/or maximum if given.
   `default_height` does not include height of fixed-height rows (i.e. separators/composite data) */
INTERNAL int set_height(struct zint_symbol *symbol, const float min_row_height, const float default_height,
                const float max_height, const int no_errtxt) {
    int error_number = 0;
    float fixed_height = 0.0f;
    int zero_count = 0;
    float row_height;
    int i;
    const int rows = symbol->rows ? symbol->rows : 1; /* Sometimes called before expand() */

    for (i = 0; i < rows; i++) {
        if (symbol->row_height[i]) {
            fixed_height += symbol->row_height[i];
        } else {
            zero_count++;
        }
    }

    if (zero_count) {
        if (symbol->height) {
            if (symbol->input_mode & HEIGHTPERROW_MODE) {
                row_height = stripf(symbol->height);
            } else {
                row_height = stripf((symbol->height - fixed_height) / zero_count);
            }
        } else if (default_height) {
            row_height = stripf(default_height / zero_count);
        } else {
            row_height = stripf(min_row_height);
        }
        if (row_height < 0.5f) { /* Absolute minimum */
            row_height = 0.5f;
        }
        if (min_row_height) {
            if (stripf(row_height) < stripf(min_row_height)) {
                error_number = ZINT_WARN_NONCOMPLIANT;
                if (!no_errtxt) {
                    errtxt(0, symbol, 247, "Height not compliant with standards");
                }
            }
        }
        symbol->height = stripf(row_height * zero_count + fixed_height);
    } else {
        symbol->height = stripf(fixed_height); /* Ignore any given height */
    }
    if (max_height) {
        if (stripf(symbol->height) > stripf(max_height)) {
            error_number = ZINT_WARN_NONCOMPLIANT;
            if (!no_errtxt) {
                errtxt(0, symbol, 248, "Height not compliant with standards");
            }
        }
    }

    return error_number;
}

/* Prevent inlining of `stripf()` which can optimize away its effect */
#if defined(__GNUC__) && (__GNUC__ >= 4 || (__GNUC__ == 3 && __GNUC_MINOR__ >= 1))
#define ZINT_NOINLINE __attribute__((__noinline__))
#elif defined(_MSC_VER) && _MSC_VER >= 1310 /* MSVC 2003 (VC++ 7.1) */
#define ZINT_NOINLINE __declspec(noinline)
#else
#define ZINT_NOINLINE
#endif

/* Removes excess precision from floats - see https://stackoverflow.com/q/503436 */
INTERNAL ZINT_NOINLINE float stripf(const float arg) {
    return *((volatile const float *) &arg);
}

/* Returns total length of segments */
INTERNAL int segs_length(const struct zint_seg segs[], const int seg_count) {
    int total_len = 0;
    int i;

    for (i = 0; i < seg_count; i++) {
        total_len += segs[i].length == -1 ? (int) ustrlen(segs[i].source) : segs[i].length;
    }

    return total_len;
}

/* Shallow copies segments, adjusting default ECIs */
INTERNAL void segs_cpy(const struct zint_symbol *symbol, const struct zint_seg segs[], const int seg_count,
                struct zint_seg local_segs[]) {
    const int default_eci = symbol->symbology == BARCODE_GRIDMATRIX ? 29 : symbol->symbology == BARCODE_UPNQR ? 4 : 3;
    int i;

    local_segs[0] = segs[0];
    for (i = 1; i < seg_count; i++) {
        local_segs[i] = segs[i];
        /* Ensure default ECI set if follows non-default ECI */
        if (local_segs[i].eci == 0 && local_segs[i - 1].eci != 0 && local_segs[i - 1].eci != default_eci) {
            local_segs[i].eci = default_eci;
        }
    }
}

/* Helper for ZINT_DEBUG_PRINT to put all but graphical ASCII in hex escapes. Output to `buf` if non-NULL, else
   stdout */
INTERNAL char *debug_print_escape(const unsigned char *source, const int first_len, char *buf) {
    int i;
    if (buf) {
        int j = 0;
        for (i = 0; i < first_len; i++) {
            const unsigned char ch = source[i];
            if (ch < 32 || ch >= 127) {
                j += sprintf(buf + j, "\\x%02X", ch & 0xFF);
            } else {
                buf[j++] = ch;
            }
        }
        buf[j] = '\0';
    } else {
        for (i = 0; i < first_len; i++) {
            const unsigned char ch = source[i];
            if (ch < 32 || ch >= 127) {
                printf("\\x%02X", ch & 0xFF);
            } else {
                fputc(ch, stdout);
            }
        }
    }
    return buf;
}

#ifdef ZINT_TEST
/* Suppress gcc warning null destination pointer [-Wformat-overflow=] false-positive */
#if defined(__GNUC__) && !defined(__clang__) && __GNUC__ >= 7
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-overflow="
#endif
/* Dumps hex-formatted codewords in symbol->errtxt (for use in testing) */
INTERNAL void debug_test_codeword_dump(struct zint_symbol *symbol, const unsigned char *codewords, const int length) {
    int i, max = length, cnt_len = 0;
    assert(sizeof(symbol->errtxt) >= 100);
    if (length > 30) { /* 30*3 < errtxt 92 (100 - "Warning ") chars */
        sprintf(symbol->errtxt, "(%d) ", length); /* Place the number of codewords at the front */
        cnt_len = (int) strlen(symbol->errtxt);
        max = 30 - (cnt_len + 2) / 3;
    }
    for (i = 0; i < max; i++) {
        sprintf(symbol->errtxt + cnt_len + i * 3, "%02X ", codewords[i]);
    }
    symbol->errtxt[strlen(symbol->errtxt) - 1] = '\0'; /* Zap last space */
}

/* Dumps decimal-formatted codewords in symbol->errtxt (for use in testing) */
INTERNAL void debug_test_codeword_dump_short(struct zint_symbol *symbol, const short *codewords, const int length) {
    int i, max = 0, cnt_len, errtxt_len;
    char temp[20];
    assert(sizeof(symbol->errtxt) >= 100);
    errtxt_len = sprintf(symbol->errtxt, "(%d) ", length); /* Place the number of codewords at the front */
    for (i = 0, cnt_len = errtxt_len; i < length; i++) {
        cnt_len += sprintf(temp, "%d ", codewords[i]);
        if (cnt_len > 92) {
            break;
        }
        max++;
    }
    for (i = 0; i < max; i++) {
        errtxt_len += sprintf(symbol->errtxt + errtxt_len, "%d ", codewords[i]);
    }
    symbol->errtxt[strlen(symbol->errtxt) - 1] = '\0'; /* Zap last space */
}

/* Dumps decimal-formatted codewords in symbol->errtxt (for use in testing) */
INTERNAL void debug_test_codeword_dump_int(struct zint_symbol *symbol, const int *codewords, const int length) {
    int i, max = 0, cnt_len, errtxt_len;
    char temp[20];
    assert(sizeof(symbol->errtxt) >= 100);
    errtxt_len = sprintf(symbol->errtxt, "(%d) ", length); /* Place the number of codewords at the front */
    for (i = 0, cnt_len = errtxt_len; i < length; i++) {
        cnt_len += sprintf(temp, "%d ", codewords[i]);
        if (cnt_len > 92) {
            break;
        }
        max++;
    }
    for (i = 0; i < max; i++) {
        errtxt_len += sprintf(symbol->errtxt + errtxt_len, "%d ", codewords[i]);
    }
    symbol->errtxt[strlen(symbol->errtxt) - 1] = '\0'; /* Zap last space */
}
#if defined(__GNUC__) && !defined(__clang__) && __GNUC__ >= 7
#pragma GCC diagnostic pop
#endif
#endif /* ZINT_TEST */

/* vim: set ts=4 sw=4 et : */
