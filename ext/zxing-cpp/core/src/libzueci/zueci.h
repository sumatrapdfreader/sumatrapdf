/* zueci.h - UTF-8 to/from Extended Channel Interpretations */
/*
    libzueci - an open source UTF-8 ECI library adapted from libzint
    Copyright (C) 2022 gitlost
 */
/* SPDX-License-Identifier: BSD-3-Clause */

#ifndef ZUECI_H
#define ZUECI_H

/* Version: 1.0.1 */

/* Warning and error returns from API functions below */
#define ZUECI_WARN_INVALID_DATA     1   /* Invalid data but replacement character used */
#define ZUECI_ERROR                 5   /* Warn/error marker, not returned */
#define ZUECI_ERROR_INVALID_DATA    6   /* Source data invalid or unmappable */
#define ZUECI_ERROR_INVALID_ECI     7   /* ECI not a valid Character Set ECI */
#define ZUECI_ERROR_INVALID_ARGS    8   /* One or more arguments invalid (e.g. NULL) */
#define ZUECI_ERROR_INVALID_UTF8    9   /* Source data not valid UTF-8 */

#ifdef _WIN32
#  if defined(DLL_EXPORT) || defined(PIC) || defined(_USRDLL)
#    define ZUECI_EXTERN __declspec(dllexport)
#  elif defined(ZUECI_DLL)
#    define ZUECI_EXTERN __declspec(dllimport)
#  else
#    define ZUECI_EXTERN extern
#  endif
#else
#  define ZUECI_EXTERN extern
#endif

#ifdef __cplusplus
extern "C" {
#endif

/*
    ECI arg `eci` must be a valid Interpretative Character Set ECI, i.e. 0-13, 15-18, 20-35, 170 or 899,
    as defined by AIM ITS/04-023 International Technical Standard - Extended Channel Interpretations
    Part 3: Register (Version 2, February 2022):

        0    IBM CP437 (top)
        1    ISO/IEC 8859-1 - Latin alphabet No. 1 (top)
        2    IBM CP437 (top)
        3    ISO/IEC 8859-1 - Latin alphabet No. 1 (top)
        4    ISO/IEC 8859-2 - Latin alphabet No. 2 (top)
        5    ISO/IEC 8859-3 - Latin alphabet No. 3 (top)
        6    ISO/IEC 8859-4 - Latin alphabet No. 4 (top)
        7    ISO/IEC 8859-5 - Latin/Cyrillic alphabet (top)
        8    ISO/IEC 8859-6 - Latin/Arabic alphabet (top)
        9    ISO/IEC 8859-7 - Latin/Greek alphabet (top)
        10   ISO/IEC 8859-8 - Latin/Hebrew alphabet (top)
        11   ISO/IEC 8859-9 - Latin alphabet No. 5 (Turkish) (top)
        12   ISO/IEC 8859-10 - Latin alphabet No. 6 (Nordic) (top)
        13   ISO/IEC 8859-11 - Latin/Thai alphabet (top)
        15   ISO/IEC 8859-13 - Latin alphabet No. 7 (Baltic) (top)
        16   ISO/IEC 8859-14 - Latin alphabet No. 8 (Celtic) (top)
        17   ISO/IEC 8859-15 - Latin alphabet No. 9 (top)
        18   ISO/IEC 8859-16 - Latin alphabet No. 10 (top)
        20   Shift JIS (JIS X 0208 and JIS X 0201) Japanese
        21   Windows 1250 - Latin 2 (Central Europe)
        22   Windows 1251 - Cyrillic
        23   Windows 1252 - Latin 1
        24   Windows 1256 - Arabic
        25   UTF-16BE (big-endian)
        26   UTF-8
        27   ASCII (ISO/IEC 646 IRV)
        28   Big5 (Taiwan) Chinese
        29   GB 2312 (PRC) Chinese
        30   EUC-KR (KS X 1001:2002) Korean
        31   GBK Chinese
        32   GB 18030 Chinese
        33   UTF-16LE (little-endian)
        34   UTF-32BE (big-endian)
        35   UTF-32LE (little-endian)
        170  ISO/IEC 646 Invariant
        899  8-bit binary data

    "(top)" means encoding applies to codepoints 0x80..FF (or 0xA0..FF for ISO/IEC 8859) with 0x00..7F as ASCII

    ECIs 0, 1 and 2 are obsolete, however ECI 2 is still referenced by ISO/IEC 15438:2015 (PDF417) Annex H.2.3

    All except ECI 20 (Shift JIS) and ECI 170 (ISO/IEC 646 Invariant) map ASCII one-to-one (but see
    `ZUECI_FLAG_XXX` flags below).
    ECI 20 re-maps 2 characters (backslash and tilde), and ECI 170 has no mapping for 12 characters (#$@[\]^`{|}~).

    All mappings are the same as libiconv with the following exception for ECI 20 (Shift JIS):
                    Unicode     Shift JIS   Unicode
        libzueci    U+005C  ->  0x815F  ->  U+005C  (U+005C REVERSE SOLIDUS)
                    U+FF3C  ->  no mapping          (U+FF3C FULLWIDTH REVERSE SOLIDUS)

        libiconv    U+005C  ->  no mapping
                    U+FF3C  ->  0x815F  ->  U+FF3C
    The rationale for this difference is that libzueci is following the "official" source
        https://unicode.org/Public/MAPPINGS/OBSOLETE/EASTASIA/JIS/SHIFTJIS.TXT
    (2015-12-02) which gives those mappings. (Note "official" is used loosely, there's no such thing unfortunately.)
    Could not find a reason for libiconv doing it its way from reading the source.

    All other mappings are the same; in particular:
                    Unicode     Shift JIS   Unicode
                    U+007E  ->  no mapping          (U+007E TILDE)
                    U+203E  ->  0x7E    ->  U+202E  (U+203E OVERLINE)
                    U+00A5  ->  0x5C    ->  U+00A5  (U+00A5 YEN SIGN)
 */

/*
    If embedding the library (i.e. including the 10 files directly) and only want ECI-to-UTF-8 functionality,
    define `ZUECI_EMBED_NO_TO_ECI`
*/
#ifndef ZUECI_EMBED_NO_TO_ECI

/*
    Convert UTF-8 `src` of length `src_len` to `eci`-encoded `dest`.
    `p_dest_len` is set to length of `dest` on output.
    `dest` must be big enough (4-times the `src_len`, or see `zueci_dest_len_eci()`). It is not NUL-terminated.
    Returns 0 if successful, one of `ZUECI_ERROR_XXX` if not.
 */
ZUECI_EXTERN int zueci_utf8_to_eci(const int eci, const unsigned char src[], const int src_len,
                    unsigned char dest[], int *p_dest_len);

/*
    Calculate sufficient (i.e. approx.) length needed to convert UTF-8 `src` of length `len` from UTF-8 to ECI
    `eci`, and place in `p_dest_len`.
    Returns 0 if successful, one of `ZUECI_ERROR_XXX` if not.
 */
ZUECI_EXTERN int zueci_dest_len_eci(const int eci, const unsigned char src[], const int src_len, int *p_dest_len);

#endif /* ZUECI_EMBED_NO_TO_ECI */

/*
    These flags can be OR-ed together to change the behaviour of `zueci_eci_to_utf8()` and `zueci_dest_len_utf8()`
 */

/*
    For single-byte ECIs copy the source straight-thru rather than erroring or replacing if undefined. Affects
    ISO/IEC 8859 (ECIs 1, 3-13, 15-18), Windows 125x (ECIs 21-24), ASCII (ECI 27) & ISO/IEC 646 Invariant (ECI 170).
 */
#define ZUECI_FLAG_SB_STRAIGHT_THRU     1

/*
    For ECI 20 Shift JIS, copy backslash & tilde straight-thru rather than mapping to Yen sign & overline resp.
 */
#define ZUECI_FLAG_SJIS_STRAIGHT_THRU   2

/*
    If embedding the library (i.e. including the 10 files directly) and only want UTF-8-to-ECI functionality,
    define `ZUECI_EMBED_NO_TO_UTF8`
*/
#ifndef ZUECI_EMBED_NO_TO_UTF8

/*
    Convert ECI-encoded `src` of length `src_len` to UTF-8 `dest`.
    `p_dest_len` is set to length of `dest` on output.
    `dest` must be big enough (4-times the `src_len`, or see `zueci_dest_len_utf8()`). It is not NUL-terminated.
    If the Unicode BMP `replacement_char` (<= 0xFFFF) is non-zero then it will substituted for all source characters
    with no mapping and processing will continue, returning ZUECI_WARN_INVALID_DATA unless other errors.
    `flags` can be set with `ZUECI_FLAG_XXX` to change behaviour.
    Returns 0 if successful, one of `ZUECI_ERROR_XXX` if not.
 */
ZUECI_EXTERN int zueci_eci_to_utf8(const int eci, const unsigned char src[], const int src_len,
                    const unsigned int replacement_char, const unsigned int flags, unsigned char dest[],
                    int *p_dest_len);

/*
    Calculate exact length needed to convert ECI-encoded `src` of length `len` from ECI `eci`, and place in
    `p_dest_len`.
    Returns 0 if successful, one of `ZUECI_ERROR_XXX` if not.
 */
ZUECI_EXTERN int zueci_dest_len_utf8(const int eci, const unsigned char src[], const int src_len,
                    const unsigned int replacement_char, const unsigned int flags, int *p_dest_len);

#endif /* ZUECI_EMBED_NO_TO_UTF8 */

#ifdef __cplusplus
}
#endif

/* vim: set ts=4 sw=4 et : */
#endif /* ZUECI_H */
