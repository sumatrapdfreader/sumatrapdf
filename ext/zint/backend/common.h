/* common.h - Header for all common functions in common.c */
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

#ifndef Z_COMMON_H
#define Z_COMMON_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include "zint.h"
#include "zintconfig.h"
#include <stdlib.h>
#include <string.h>

/* Determine if C89 (excluding MSVC, which doesn't define __STDC_VERSION__) */
#ifndef _MSC_VER
#  if !defined(__STDC_VERSION__) || __STDC_VERSION__ < 199000L
#    define ZINT_IS_C89
#  endif
#endif

#ifdef _MSC_VER
typedef unsigned __int8 uint8_t;
typedef unsigned __int16 uint16_t;
typedef __int32 int32_t;
typedef unsigned __int32 uint32_t;
typedef unsigned __int64 uint64_t;
#else
#include <stdint.h> /* Introduced C99 */
#endif

/* Note if change following must also change "frontend/main.c" copy */

#define ARRAY_SIZE(x) ((int) (sizeof(x) / sizeof((x)[0])))

#ifdef _MSC_VER
#  include <malloc.h>
#  define z_alloca(nmemb) _alloca(nmemb)
#elif defined(__COMPCERT__)
#  define z_alloca(nmemb) malloc(nmemb) /* So links - leads to loads of leaks obs */
#else
#  if (defined(__GNUC__) && !defined(alloca) && !defined(__NetBSD__)) || defined(__NuttX__) || defined(_AIX) \
        || (defined(__sun) && defined(__SVR4) /*Solaris*/)
#    include <alloca.h>
#  endif
#  define z_alloca(nmemb) alloca(nmemb)
#endif

/* End of "frontend/main.c" copy */

#ifdef _MSC_VER
#  pragma warning(disable: 4125) /* decimal digit terminates octal escape sequence */
#  pragma warning(disable: 4244) /* conversion from int to float */
#  if _MSC_VER > 1200 /* VC6 */
#    pragma warning(disable: 4996) /* function or variable may be unsafe */
#  endif
#endif

#if defined(__GNUC__) && __GNUC__ >= 3
#  define ZINT_FORMAT_PRINTF(fmt_idx, first_idx) __attribute__((__format__(printf, fmt_idx, first_idx)))
#else
#  define ZINT_FORMAT_PRINTF(fmt_idx, first_idx)
#endif

#if defined(__GNUC__) && __GNUC__ >= 4 && !defined(ZINT_TEST) && !defined(__MINGW32__)
#  define INTERNAL __attribute__((__visibility__("hidden")))
#elif defined(ZINT_TEST)
#  define INTERNAL ZINT_EXTERN /* The test suite references INTERNAL functions, so they need to be exported */
#else
#  define INTERNAL
#endif

#if defined(__GNUC__) && __GNUC__ >= 4 && !defined(__MINGW32__)
#  define INTERNAL_DATA_EXTERN __attribute__((__visibility__("hidden"))) extern
#  define INTERNAL_DATA __attribute__((__visibility__("hidden")))
#else
#  define INTERNAL_DATA_EXTERN extern
#  define INTERNAL_DATA
#endif

#ifdef _MSC_VER
#  if _MSC_VER >= 1400 /* MSVC 2005 (C++ 8.0) */
#    define restrict __restrict
#  else
#    define restrict
#  endif
#else
#  ifdef ZINT_IS_C89
#    define restrict
#  endif
#endif

#if (defined(_MSC_VER) && _MSC_VER <= 1200) || defined(ZINT_IS_C89) /* VC6 or C89 */
#  define ceilf (float) ceil
#  define floorf (float) floor
#  define fmodf (float) fmod
#endif
/* `round()` (C99) not before MSVC 2013 (C++ 12.0) */
#if (defined(_MSC_VER) && _MSC_VER < 1800) || defined(ZINT_IS_C89)
#  define round(arg) floor((arg) + 0.5)
#  define roundf(arg) floorf((arg) + 0.5f)
#endif

/* Is float integral value? (https://stackoverflow.com/a/40404149) */
#define isfintf(arg) (fmodf(arg, 1.0f) == 0.0f)

/* Simple versions of <ctype.h> functions with no dependence on locale */
#define z_isdigit(c) ((c) <= '9' && (c) >= '0')
#define z_isupper(c) ((c) >= 'A' && (c) <= 'Z')
#define z_islower(c) ((c) >= 'a' && (c) <= 'z')

/* Helpers to cast away char pointer signedness */
#define ustrlen(source) strlen((const char *) (source))
#define ustrcpy(target, source) strcpy((char *) (target), (const char *) (source))
#define ustrcat(target, source) strcat((char *) (target), (const char *) (source))
#define ustrncat(target, source, count) strncat((char *) (target), (const char *) (source), (count))

/* Converts an integer value to its value + '0' (so >= 10 becomes ':', ';' etc) */
#define itoc(i) ((i) + '0')

/* Converts a character 0-9, A-F to its equivalent integer value */
INTERNAL int ctoi(const char source);

/* Converts decimal string of length <= 9 to integer value. Returns -1 if not numeric */
INTERNAL int to_int(const unsigned char source[], const int length);

/* Converts lower case characters to upper case in string `source` */
INTERNAL void to_upper(unsigned char source[], const int length);

/* Returns the number of times a character occurs in `source` */
INTERNAL int chr_cnt(const unsigned char source[], const int length, const unsigned char c);

/* `is_chr()` & `not_sane()` flags */
#define IS_SPC_F    0x0001 /* Space */
#define IS_HSH_F    0x0002 /* Hash sign # */
#define IS_AST_F    0x0004 /* Asterisk sign * */
#define IS_PLS_F    0x0008 /* Plus sign + */
#define IS_MNS_F    0x0010 /* Minus sign - */
#define IS_NUM_F    0x0020 /* Number 0-9 */
#define IS_UPO_F    0x0040 /* Uppercase letter, apart from A-F and X */
#define IS_UHX_F    0x0080 /* Uppercase hex A-F */
#define IS_UX__F    0x0100 /* Uppercase X */
#define IS_LWO_F    0x0200 /* Lowercase letter, apart from a-f and x */
#define IS_LHX_F    0x0400 /* Lowercase hex a-f */
#define IS_LX__F    0x0800 /* Lowercase x */
#define IS_C82_F    0x1000 /* CSET82 punctuation (apart from *, + and -) */
#define IS_SIL_F    0x2000 /* SILVER/TECHNETIUM punctuation .$/% (apart from space, + and -) */
#define IS_CLI_F    0x4000 /* CALCIUM INNER punctuation $:/. (apart from + and -) (Codabar) */
#define IS_ARS_F    0x8000 /* ARSENIC uppercase subset (VIN) */

#define IS_UPR_F    (IS_UPO_F | IS_UHX_F | IS_UX__F) /* Uppercase letters */
#define IS_LWR_F    (IS_LWO_F | IS_LHX_F | IS_LX__F) /* Lowercase letters */

/* The most commonly used set */
#define NEON_F      IS_NUM_F /* NEON "0123456789" */

/* Whether a character matches `flg` */
INTERNAL int is_chr(const unsigned int flg, const unsigned int c);

/* Verifies if a string only uses valid characters, returning 1-based position in `source` if not, 0 for success */
INTERNAL int not_sane(const unsigned int flg, const unsigned char source[], const int length);

/* Verifies if a string only uses valid characters as above, but also returns `test_string` position of each in
   `posns` array */
INTERNAL int not_sane_lookup(const char test_string[], const int test_length, const unsigned char source[],
                const int length, int *posns);

/* Returns the position of `data` in `set_string`, or -1 if not found */
INTERNAL int posn(const char set_string[], const char data);


/* Converts `arg` to a string representing its binary equivalent of length `length` and places in `binary` at
  `bin_posn`. Returns `bin_posn` + `length` */
INTERNAL int bin_append_posn(const int arg, const int length, char *binary, const int bin_posn);


#define Z_COMMON_INLINE   1

#ifdef Z_COMMON_INLINE

#  define module_is_set(s, y, x) (((s)->encoded_data[y][(x) >> 3] >> ((x) & 0x07)) & 1)
#  define set_module(s, y, x) do { (s)->encoded_data[y][(x) >> 3] |= 1 << ((x) & 0x07); } while (0)
#  define module_colour_is_set(s, y, x) ((s)->encoded_data[y][x])
#  define set_module_colour(s, y, x, c) do { (s)->encoded_data[y][x] = (c); } while (0)
#  define unset_module(s, y, x) do { (s)->encoded_data[y][(x) >> 3] &= ~(1 << ((x) & 0x07)); } while (0)

#else /* Z_COMMON_INLINE */

/* Returns true (1) if a module is dark/black, otherwise false (0) */
INTERNAL int module_is_set(const struct zint_symbol *symbol, const int y_coord, const int x_coord);

/* Sets a module to dark/black */
INTERNAL void set_module(struct zint_symbol *symbol, const int y_coord, const int x_coord);

/* Returns true (1-8) if a module is colour, otherwise false (0) */
INTERNAL int module_colour_is_set(const struct zint_symbol *symbol, const int y_coord, const int x_coord);

/* Sets a module to a colour */
INTERNAL void set_module_colour(struct zint_symbol *symbol, const int y_coord, const int x_coord,
                const int colour);

/* Sets a dark/black module to white (i.e. unsets) */
INTERNAL void unset_module(struct zint_symbol *symbol, const int y_coord, const int x_coord);

#endif /* Z_COMMON_INLINE */


/* Expands from a width pattern to a bit pattern */
INTERNAL void expand(struct zint_symbol *symbol, const char data[], const int length);


/* Set `symbol->errtxt` to "err_id: msg", returning `error_number`. If `err_id` is -1, the "err_id: " prefix is
   omitted */
INTERNAL int errtxt(const int error_number, struct zint_symbol *symbol, const int err_id, const char *msg);

/* Set `symbol->errtxt` to "err_id: msg" with restricted subset of `printf()` formatting, returning `error_number`.
   If `err_id` is -1, the "err_id: " prefix is omitted. Only the following specifiers are supported: "c", "d", "f",
   "g" and "s", with no modifiers apart from "<n>$" numbering for l10n ("<n>" 1-9), in which case all specifiers must
   be numbered, "%s" with length precisions: "%.*s", "%<n+1>$.*<n>$s", "%.<p>s" and "%<n>$.<p>s", and "%d" with
   zero-padded minimum field lengths: "%0<m>d" or %<n>$0<m>d" ("<m>" 1-99) */
INTERNAL int errtxtf(const int error_number, struct zint_symbol *symbol, const int err_id, const char *fmt, ...)
    ZINT_FORMAT_PRINTF(4, 5);

/* Helper to prepend/append to existing `symbol->errtxt` by calling `errtxtf(fmt)` with 2 arguments (copy of `errtxt`
   & `msg`) if `msg` not NULL, or 1 argument (just copy of `errtxt`) if `msg` NULL, returning `error_number` */
INTERNAL int errtxt_adj(const int error_number, struct zint_symbol *symbol, const char *fmt, const char *msg);


/* Whether `symbology` can have row binding */
INTERNAL int is_stackable(const int symbology);

/* Whether `symbology` is EAN/UPC */
INTERNAL int is_upcean(const int symbology);

/* Whether `symbology` can have composite 2D component data */
INTERNAL int is_composite(const int symbology);

/* Whether `symbology` is a matrix design renderable as dots */
INTERNAL int is_dotty(const int symbology);

/* Whether `symbology` has a fixed aspect ratio (matrix design) */
INTERNAL int is_fixed_ratio(const int symbology);


/* Whether next two characters are digits */
INTERNAL int is_twodigits(const unsigned char source[], const int length, const int position);

/* Returns how many consecutive digits lie immediately ahead up to `max`, or all if `max` is -1 */
INTERNAL int cnt_digits(const unsigned char source[], const int length, const int position, const int max);


/* State machine to decode UTF-8 to Unicode codepoints (state 0 means done, state 12 means error) */
INTERNAL unsigned int decode_utf8(unsigned int *state, unsigned int *codep, const unsigned char byte);

/* Is string valid UTF-8? */
INTERNAL int is_valid_utf8(const unsigned char source[], const int length);

/* Converts UTF-8 to Unicode. If `disallow_4byte` unset, allows all values (UTF-32). If `disallow_4byte` set,
 * only allows codepoints <= U+FFFF (ie four-byte sequences not allowed) (UTF-16, no surrogates) */
INTERNAL int utf8_to_unicode(struct zint_symbol *symbol, const unsigned char source[], unsigned int vals[],
                int *length, const int disallow_4byte);

/* Treats source as ISO/IEC 8859-1 and copies into `symbol->text`, converting to UTF-8. Control chars (incl. DEL) and
   non-ISO/IEC 8859-1 (0x80-9F) are replaced with spaces. Returns warning if truncated, else 0 */
INTERNAL int hrt_cpy_iso8859_1(struct zint_symbol *symbol, const unsigned char source[], const int length);


/* Sets symbol height, returning a warning if not within minimum and/or maximum if given.
   `default_height` does not include height of fixed-height rows (i.e. separators/composite data) */
INTERNAL int set_height(struct zint_symbol *symbol, const float min_row_height, const float default_height,
                const float max_height, const int no_errtxt);


/* Removes excess precision from floats - see https://stackoverflow.com/q/503436 */
INTERNAL float stripf(const float arg);


/* Returns total length of segments */
INTERNAL int segs_length(const struct zint_seg segs[], const int seg_count);

/* Shallow copies segments, adjusting default ECIs */
INTERNAL void segs_cpy(const struct zint_symbol *symbol, const struct zint_seg segs[], const int seg_count,
                struct zint_seg local_segs[]);


/* Helper for ZINT_DEBUG_PRINT to put all but graphical ASCII in hex escapes. Output to `buf` if non-NULL, else
   stdout */
INTERNAL char *debug_print_escape(const unsigned char *source, const int first_len, char *buf);

#ifdef ZINT_TEST
/* Dumps hex-formatted codewords in symbol->errtxt (for use in testing) */
INTERNAL void debug_test_codeword_dump(struct zint_symbol *symbol, const unsigned char *codewords, const int length);
/* Dumps decimal-formatted codewords in symbol->errtxt (for use in testing) */
INTERNAL void debug_test_codeword_dump_short(struct zint_symbol *symbol, const short *codewords, const int length);
/* Dumps decimal-formatted codewords in symbol->errtxt (for use in testing) */
INTERNAL void debug_test_codeword_dump_int(struct zint_symbol *symbol, const int *codewords, const int length);
#endif

#ifdef __cplusplus
}
#endif /* __cplusplus */

/* vim: set ts=4 sw=4 et : */
#endif /* Z_COMMON_H */
