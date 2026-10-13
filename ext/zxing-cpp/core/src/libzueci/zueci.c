/*  zueci.c - UTF-8 to/from Extended Channel Interpretations */
/*
    libzueci - an open source UTF-8 ECI library adapted from libzint
    Copyright (C) 2022 gitlost
 */
/* SPDX-License-Identifier: BSD-3-Clause */

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "zueci.h"
#include "zueci_common.h"

#include "zueci_sb.h"
#include "zueci_big5.h"
#include "zueci_gb18030.h"
#include "zueci_gb2312.h"
#include "zueci_gbk.h"
#include "zueci_ksx1001.h"
#include "zueci_sjis.h"

/* Whether codepoint `u` valid Unicode */
#define ZUECI_IS_VALID_UNICODE(u) ((u) < 0xD800 || ((u) >= 0xE000 && (u) <= 0x10FFFF))

/* Put 4 bytes into `zueci_u32` */
#define ZUECI_4BYTES_U32(c1, c2, c3, c4) \
    (((zueci_u32) (c1) << 24) | ((zueci_u32) (c2) << 16) | ((zueci_u32) (c3) << 8) | (c4))

/* Utility funcs */

/* Whether `eci` valid character set ECI */
static int zueci_is_valid_eci(const int eci) {
    return (eci <= 35 && eci >= 0 && eci != 14 && eci != 19) || eci == 170 || eci == 899;
}

/* State machine to decode UTF-8 to Unicode codepoints (state 0 means done, state 12 means error) */
static unsigned int zueci_decode_utf8(unsigned int *p_state, zueci_u32 *p_u, const unsigned char byte) {
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

    const zueci_u32 type = utf8d[byte];

    *p_u = *p_state ? (byte & 0x3fu) | (*p_u << 6) : (0xff >> type) & byte;

    *p_state = utf8d[256 + *p_state + type];

    return *p_state;
}

#ifdef ZUECI_TEST /* Wrapper to make available for use by tests */
ZUECI_INTERN unsigned int zueci_decode_utf8_test(unsigned int *p_state, zueci_u32 *p_u, const unsigned char byte) {
    return zueci_decode_utf8(p_state, p_u, byte);
}
#endif

/* Whether string valid UTF-8 */
static int zueci_is_valid_utf8(const unsigned char src[], const int len) {
    unsigned int state = 0;
    const unsigned char *s = src;
    const unsigned char *const se = src + len;
    zueci_u32 u;

    while (s < se) {
        if (zueci_decode_utf8(&state, &u, *s++) == 12) {
            return 0;
        }
    }

    return state == 0;
}

#ifndef ZUECI_EMBED_NO_TO_UTF8
/* Convert Unicode codepoint `u` to UTF-8 `dest`, returning UTF-8 length */
static int zueci_encode_utf8(const zueci_u32 u, unsigned char *dest) {
    if (u < 0x80) {
        dest[0] = (unsigned char) u;
        return 1;
    }
    if (u < 0x800) {
        dest[0] = (unsigned char) (0xC0 | (u >> 6));
        dest[1] = (unsigned char) (0x80 | (u & 0x3F));
        return 2;
    }
    if (u < 0x10000) {
        dest[0] = (unsigned char) (0xE0 | (u >> 12));
        dest[1] = (unsigned char) (0x80 | ((u >> 6) & 0x3F));
        dest[2] = (unsigned char) (0x80 | (u & 0x3F));
        return 3;
    }
    dest[0] = (unsigned char) (0xF0 | (u >> 18));
    dest[1] = (unsigned char) (0x80 | ((u >> 12) & 0x3F));
    dest[2] = (unsigned char) (0x80 | ((u >> 6) & 0x3F));
    dest[3] = (unsigned char) (0x80 | (u & 0x3F));
    return 4;
}
#endif /* ZUECI_EMBED_NO_TO_UTF8 */

#ifndef ZUECI_EMBED_NO_TO_ECI
/* Returns the number of times a character occurs in a string */
static int zueci_chr_cnt(const unsigned char src[], const int len, const unsigned char c) {
    int count = 0;
    const unsigned char *const se = src + len;
    const unsigned char *s = src;

    while (s < se) {
        if (*s++ == c) {
            count++;
        }
    }
    return count;
}

/* Returns the number of chars in a string less than or equal to a character */
static int zueci_chr_lte_cnt(const unsigned char src[], const int len, const unsigned char c) {
    int count = 0;
    const unsigned char *const se = src + len;
    const unsigned char *s = src;

    while (s < se) {
        if (*s++ <= c) {
            count++;
        }
    }
    return count;
}
#endif /* ZUECI_EMBED_NO_TO_ECI */

#ifndef ZUECI_EMBED_NO_TO_UTF8
/* Helper to return source increment on using replacement character */
static int zueci_replacement_incr(const int eci, const unsigned char *src, const zueci_u32 len) {
    assert(len);
    assert(eci != 26 && eci != 899); /* Dealt with as special cases */
    if (len == 1) { /* Last char */
        return 1;
    }
    if (eci <= 18 || (eci >= 21 && eci <= 24) || eci == 27 || eci == 170) { /* Single-byte */
        return 1;
    }
    if (eci == 25 || eci == 33) { /* UTF-16BE/LE */
        return 2;
    }
    if (eci == 34 || eci == 35) { /* UTF-32BE/LE */
        return ZUECI_MIN(len, 4);
    }
    if (eci == 32) { /* GB 18030 */
        /* If have 4 bytes and match start range of 4-byter [81..E3][30..39] */
        if (len >= 4 && src[1] <= 0x39 && src[1] >= 0x30 && src[0] >= 0x81 && src[0] <= 0xE3) {
            /* Treat as 4-byter (without checking further) */
            return 4;
        }
        /* Else treat as 2-byter */
    }
    return 2;
}
#endif /* ZUECI_EMBED_NO_TO_UTF8 */

/* Single-byte & UTF-16/32 stuff */

#ifndef ZUECI_EMBED_NO_TO_ECI
/* Unicode to ECIs 0 and 2 (bottom half ASCII, top half IBM CP 437) */
static int zueci_u_cp437(const zueci_u32 u, unsigned char *dest) {
    int s, e;
    if (u < 0x80) {
        *dest = (unsigned char) u;
        return 1;
    }

    s = 0;
    e = ZUECI_ASIZE(zueci_cp437_u_u) - 1;
    while (s <= e) {
        const int m = (s + e) >> 1;
        if (zueci_cp437_u_u[m] < u) {
            s = m + 1;
        } else if (zueci_cp437_u_u[m] > u) {
            e = m - 1;
        } else {
            *dest = zueci_cp437_u_sb[m];
            return 1;
        }
    }
    return 0;
}
#endif /* ZUECI_EMBED_NO_TO_ECI */

#ifndef ZUECI_EMBED_NO_TO_UTF8
/* ECIs 0 and 2 ASCII/CP 437 to Unicode */
static int zueci_cp437_u(const unsigned char *src, const zueci_u32 len, const unsigned int flags, zueci_u32 *p_u) {
    const unsigned char c = *src;
    (void)len; (void)flags;
    if (c < 0x80) {
        *p_u = c;
        return 1;
    }
    *p_u = zueci_cp437_u_u[(int) zueci_cp437_sb_u[c - 0x80]]; /* No undefined */
    return 1;
}
#endif /* ZUECI_EMBED_NO_TO_UTF8 */

#ifndef ZUECI_EMBED_NO_TO_ECI
/* Base ISO/IEC 8859 routine to convert Unicode codepoint `u` */
static int zueci_u_iso8859(const zueci_u32 u, const zueci_u16 *tab_s, const zueci_u16 *tab_u_u,
            const unsigned char *tab_u_sb, int e, unsigned char *dest) {
    int s;
    if (u < 0xA0) {
        if (u >= 0x80) { /* U+0080-9F fail */
            return 0;
        }
        *dest = (unsigned char) u;
        return 1;
    }
    if (u <= 0xFF) {
        const zueci_u32 u2 = u - 0xA0;
        if (tab_s[u2 >> 4] & ((zueci_u16) 1 << (u2 & 0xF))) {
            *dest = (unsigned char) u; /* Straight-thru */
            return 1;
        }
    }

    s = 0;
    while (s <= e) {
        const int m = (s + e) >> 1;
        if (tab_u_u[m] < u) {
            s = m + 1;
        } else if (tab_u_u[m] > u) {
            e = m - 1;
        } else {
            *dest = tab_u_sb[m];
            return 1;
        }
    }
    return 0;
}
#endif /* ZUECI_EMBED_NO_TO_ECI */

#ifndef ZUECI_EMBED_NO_TO_UTF8
/* Base ISO/IEC 8859 routine to convert single-byte `c` */
static int zueci_iso8859_u(const unsigned char c, const unsigned int flags, const zueci_u16 *tab_s,
            const zueci_u16 *tab_u_u, const char *tab_sb_u, const int c2_max, zueci_u32 *p_u) {
    unsigned char c2;
    int idx;
    if (c < 0xA0) {
        if (c >= 0x80 && !(flags & ZUECI_FLAG_SB_STRAIGHT_THRU)) { /* U+0080-9F fail unless straight-thru */
            return 0;
        }
        *p_u = c;
        return 1;
    }
    c2 = c - 0xA0;
    if (tab_s[c2 >> 4] & ((zueci_u16) 1 << (c2 & 0xF))) {
        *p_u = c; /* Straight-thru */
        return 1;
    }
    if (c2 < c2_max && (idx = (int) tab_sb_u[c2]) != -1) {
        *p_u = tab_u_u[idx];
        return 1;
    }
    if (flags & ZUECI_FLAG_SB_STRAIGHT_THRU) {
        *p_u = c;
        return 1;
    }
    return 0;
}
#endif /* ZUECI_EMBED_NO_TO_UTF8 */

#ifndef ZUECI_EMBED_NO_TO_ECI
/* Base Windows-125x routine to convert Unicode codepoint `u` */
static int zueci_u_cp125x(const zueci_u32 u, const zueci_u16 *tab_s, const zueci_u16 *tab_u_u,
            const unsigned char *tab_u_sb, int e, unsigned char *dest) {
    int s;
    if (u < 0x80) {
        *dest = (unsigned char) u;
        return 1;
    }
    if (u <= 0xFF && u >= 0xA0) {
        const zueci_u32 u2 = u - 0xA0;
        if (tab_s[u2 >> 4] & ((zueci_u16) 1 << (u2 & 0xF))) {
            *dest = (unsigned char) u; /* Straight-thru */
            return 1;
        }
    }

    s = 0;
    while (s <= e) {
        const int m = (s + e) >> 1;
        if (tab_u_u[m] < u) {
            s = m + 1;
        } else if (tab_u_u[m] > u) {
            e = m - 1;
        } else {
            *dest = tab_u_sb[m];
            return 1;
        }
    }
    return 0;
}
#endif /* ZUECI_EMBED_NO_TO_ECI */

#ifndef ZUECI_EMBED_NO_TO_UTF8
/* Base Windows-125x routine to convert single-byte `c` */
static int zueci_cp125x_u(const unsigned char c, const unsigned int flags, const zueci_u16 *tab_s,
            const zueci_u16 *tab_u_u, const char *tab_sb_u, const int c_max, zueci_u32 *p_u) {
    int idx;
    if (c < 0x80) {
        *p_u = c;
        return 1;
    }
    if (c >= 0xA0) {
        const unsigned char c2 = c - 0xA0;
        if (tab_s[c2 >> 4] & ((zueci_u16) 1 << (c2 & 0xF))) {
            *p_u = c; /* Straight-thru */
            return 1;
        }
    }
    if (c < c_max && (idx = (int) tab_sb_u[c - 0x80]) != -1) {
        *p_u = tab_u_u[idx];
        return 1;
    }
    if (flags & ZUECI_FLAG_SB_STRAIGHT_THRU) {
        *p_u = c;
        return 1;
    }
    return 0;
}
#endif /* ZUECI_EMBED_NO_TO_UTF8 */

#ifndef ZUECI_EMBED_NO_TO_ECI
/* Unicode to ECI 27 ASCII (ISO/IEC 646:1991 IRV (US)) */
static int zueci_u_ascii(const zueci_u32 u, unsigned char *dest) {
    if (u < 0x80) {
        *dest = (unsigned char) u;
        return 1;
    }
    return 0;
}
#endif /* ZUECI_EMBED_NO_TO_ECI */

#ifndef ZUECI_EMBED_NO_TO_UTF8
/* ECI 27 ASCII to Unicode */
static int zueci_ascii_u(const unsigned char *src, const zueci_u32 len, const unsigned int flags, zueci_u32 *p_u) {
    (void)len;
    if (*src < 0x80 || (flags & ZUECI_FLAG_SB_STRAIGHT_THRU)) {
        *p_u = *src;
        return 1;
    }
    return 0;
}
#endif /* ZUECI_EMBED_NO_TO_UTF8 */

#ifndef ZUECI_EMBED_NO_TO_ECI
/* Unicode to ECI 170 ISO/IEC 646:1991 Invariant, ASCII subset that excludes 12 chars that historically had
   national variants, namely "#$@[\]^`{|}~" */
static int zueci_u_ascii_inv(const zueci_u32 u, unsigned char *dest) {
    if (u == 0x7F || (u <= 'z' && u != '#' && u != '$' && u != '@' && (u <= 'Z' || u == '_' || u >= 'a'))) {
        *dest = (unsigned char) u;
        return 1;
    }
    return 0;
}
#endif /* ZUECI_EMBED_NO_TO_ECI */

#ifndef ZUECI_EMBED_NO_TO_UTF8
/* ECI 170 ISO/IEC 646:1991 Invariant to Unicode */
static int zueci_ascii_inv_u(const unsigned char *src, const zueci_u32 len, const unsigned int flags,
            zueci_u32 *p_u) {
    const unsigned char c = *src;
    (void)len;
    if ((flags & ZUECI_FLAG_SB_STRAIGHT_THRU) || c == 0x7F || (c <= 'z' && c != '#' && c != '$' && c != '@'
            && (c <= 'Z' || c == '_' || c >= 'a'))) {
        *p_u = c;
        return 1;
    }
    return 0;
}
#endif /* ZUECI_EMBED_NO_TO_UTF8 */

#ifndef ZUECI_EMBED_NO_TO_ECI
/* Unicode to ECI 25 UTF-16 Big Endian (ISO/IEC 10646) - assumes valid Unicode */
static int zueci_u_utf16be(const zueci_u32 u, unsigned char *dest) {
    zueci_u32 u2, v;
    if (u < 0x10000) {
        dest[0] = (unsigned char) (u >> 8);
        dest[1] = (unsigned char) u;
        return 2;
    }
    u2 = u - 0x10000;
    v = u2 >> 10;
    dest[0] = (unsigned char) (0xD8 + (v >> 8));
    dest[1] = (unsigned char) v;
    v = u2 & 0x3FF;
    dest[2] = (unsigned char) (0xDC + (v >> 8));
    dest[3] = (unsigned char) v;
    return 4;
}
#endif /* ZUECI_EMBED_NO_TO_ECI */

#ifndef ZUECI_EMBED_NO_TO_UTF8
/* ECI 25 UTF-16 Big Endian to Unicode */
static int zueci_utf16be_u(const unsigned char *src, const zueci_u32 len, const unsigned int flags, zueci_u32 *p_u) {
    zueci_u16 u1, u2;
    (void)flags;
    if (len < 2) {
        return 0;
    }
    u1 = ((zueci_u16) src[0] << 8) | src[1];
    if (u1 < 0xD800 || u1 > 0xDFFF) {
        *p_u = u1;
        return 2;
    }
    if (u1 >= 0xDC00 || len < 4) {
        return 0;
    }
    u2 = ((zueci_u16) src[2] << 8) | src[3];
    if (u2 < 0xDC00 || u2 > 0xDFFF) {
        return 0;
    }
    *p_u = 0x10000 + (((zueci_u32) (u1 - 0xD800) << 10) | (u2 - 0xDC00));
    return 4;

}
#endif /* ZUECI_EMBED_NO_TO_UTF8 */

#ifndef ZUECI_EMBED_NO_TO_ECI
/* Unicode to ECI 33 UTF-16 Little Endian (ISO/IEC 10646) - assumes valid Unicode */
static int zueci_u_utf16le(const zueci_u32 u, unsigned char *dest) {
    zueci_u32 u2, v;
    if (u < 0x10000) {
        dest[0] = (unsigned char) u;
        dest[1] = (unsigned char) (u >> 8);
        return 2;
    }
    u2 = u - 0x10000;
    v = u2 >> 10;
    dest[0] = (unsigned char) v;
    dest[1] = (unsigned char) (0xD8 + (v >> 8));
    v = u2 & 0x3FF;
    dest[2] = (unsigned char) v;
    dest[3] = (unsigned char) (0xDC + (v >> 8));
    return 4;
}
#endif /* ZUECI_EMBED_NO_TO_ECI */

#ifndef ZUECI_EMBED_NO_TO_UTF8
/* ECI 33 UTF-16 Little Endian to Unicode */
static int zueci_utf16le_u(const unsigned char *src, const zueci_u32 len, const unsigned int flags, zueci_u32 *p_u) {
    zueci_u16 u1, u2;
    (void)flags;
    if (len < 2) {
        return 0;
    }
    u1 = ((zueci_u16) src[1] << 8) | src[0];
    if (u1 < 0xD800 || u1 > 0xDFFF) {
        *p_u = u1;
        return 2;
    }
    if (u1 >= 0xDC00 || len < 4) {
        return 0;
    }
    u2 = ((zueci_u16) src[3] << 8) | src[2];
    if (u2 < 0xDC00 || u2 > 0xDFFF) {
        return 0;
    }
    *p_u = 0x10000 + (((zueci_u32) (u1 - 0xD800) << 10) | (u2 - 0xDC00));
    return 4;

}
#endif /* ZUECI_EMBED_NO_TO_UTF8 */

#ifndef ZUECI_EMBED_NO_TO_ECI
/* Unicode to ECI 34 UTF-32 Big Endian (ISO/IEC 10646) - assumes valid Unicode */
static int zueci_u_utf32be(const zueci_u32 u, unsigned char *dest) {
    dest[0] = 0;
    dest[1] = (unsigned char) (u >> 16);
    dest[2] = (unsigned char) (u >> 8);
    dest[3] = (unsigned char) u;
    return 4;
}
#endif /* ZUECI_EMBED_NO_TO_ECI */

#ifndef ZUECI_EMBED_NO_TO_UTF8
/* ECI 34 UTF-32 Big Endian to Unicode */
static int zueci_utf32be_u(const unsigned char *src, const zueci_u32 len, const unsigned int flags, zueci_u32 *p_u) {
    zueci_u32 u;
    (void)flags;
    if (len < 4) {
        return 0;
    }
    u = ZUECI_4BYTES_U32(src[0], src[1], src[2], src[3]);
    if (!ZUECI_IS_VALID_UNICODE(u)) {
        return 0;
    }
    *p_u = u;
    return 4;
}
#endif /* ZUECI_EMBED_NO_TO_UTF8 */

#ifndef ZUECI_EMBED_NO_TO_ECI
/* Unicode to ECI 35 UTF-32 Little Endian (ISO/IEC 10646) - assumes valid Unicode */
static int zueci_u_utf32le(const zueci_u32 u, unsigned char *dest) {
    dest[0] = (unsigned char) u;
    dest[1] = (unsigned char) (u >> 8);
    dest[2] = (unsigned char) (u >> 16);
    dest[3] = 0;
    return 4;
}
#endif /* ZUECI_EMBED_NO_TO_ECI */

#ifndef ZUECI_EMBED_NO_TO_UTF8
/* ECI 35 UTF-32 Little Endian to Unicode */
static int zueci_utf32le_u(const unsigned char *src, const zueci_u32 len, const unsigned int flags, zueci_u32 *p_u) {
    zueci_u32 u;
    (void)flags;
    if (len < 4) {
        return 0;
    }
    u = ZUECI_4BYTES_U32(src[3], src[2], src[1], src[0]);
    if (!ZUECI_IS_VALID_UNICODE(u)) {
        return 0;
    }
    *p_u = u;
    return 4;
}
#endif /* ZUECI_EMBED_NO_TO_UTF8 */

#ifndef ZUECI_EMBED_NO_TO_ECI
/* Unicode to ECI 899 Binary */
static int zueci_u_binary(const zueci_u32 u, unsigned char *dest) {
    if (u <= 0xFF) {
        *dest = (unsigned char) u;
        return 1;
    }
    return 0;
}
#endif /* ZUECI_EMBED_NO_TO_ECI */

/* Multibyte stuff */

/* Acknowledgements to Bruno Haible <bruno@clisp.org> for a no. of techniques used here */

#ifndef ZUECI_EMBED_NO_TO_ECI
/* Helper to lookup Unicode codepoint `u` in the URO (Unified Repertoire and Ordering) block (U+4E00-9FFF) */
static int zueci_u_lookup_uro(const zueci_u32 u, const zueci_u16 *tab_u_u, const zueci_u16 *tab_mb_ind,
            const zueci_u16 *tab_u_mb, unsigned char *dest) {
    zueci_u32 u2 = (u - 0x4E00) >> 4; /* Blocks of 16 */
    zueci_u32 v = (zueci_u32) 1 << (u & 0xF);
    zueci_u16 mb;
    if ((tab_u_u[u2] & v) == 0) {
        return 0;
    }
    v = tab_u_u[u2] & (v - 1); /* Mask to bits prior to this one */
    /* Count bits set (http://graphics.stanford.edu/~seander/bithacks.html#CountBitsSetParallel) */
    v = v - ((v >> 1) & 0x55555555);
    v = (v & 0x33333333) + ((v >> 2) & 0x33333333);
    v = (((v + (v >> 4)) & 0xF0F0F0F) * 0x1010101) >> 24;
    mb = tab_u_mb[tab_mb_ind[u2] + v];
    dest[0] = (unsigned char) (mb >> 8);
    dest[1] = (unsigned char) mb;
    return 2;
}

/* Unicode to ECI 20 Shift JIS */
static int zueci_u_sjis(const zueci_u32 u, unsigned char *dest) {

    if (u < 0x80 && u != 0x5C && u != 0x7E) { /* Backslash & tilde re-mapped according to JIS X 0201 Roman */
        dest[0] = (unsigned char) u;
        return 1;
    }
    /* Special case URO block sequential mappings (considerably lessens size of `zueci_sjis_u_u[]` array) */
    if (u >= 0x4E00 && u < 0xE000) { /* 0xE000 next used value >= 0x4E00 */
        if (u > 0x9FA0) {
            return 0;
        }
        return zueci_u_lookup_uro(u, zueci_sjis_uro_u, zueci_sjis_uro_mb_ind, zueci_sjis_u_mb, dest);
    }
    /* PUA to user-defined (Table 4-86, Lunde, 2nd ed.) */
    if (u >= 0xE000 && u <= 0xE757) {
        const zueci_u32 u2 = u - 0xE000;
        const unsigned char dv = (unsigned char) (u2 / (0xFC - 0x40));
        const unsigned char md = (unsigned char) (u2 - dv * (0xFC - 0x40));
        dest[0] = dv + 0xF0;
        dest[1] = md + 0x40 + (md >= 0x3F);
        return 2;
    }
    if (u >= zueci_sjis_u_u[0] && u <= zueci_sjis_u_u[ZUECI_ASIZE(zueci_sjis_u_u) - 1]) {
        int s = 0;
        int e = ZUECI_ASIZE(zueci_sjis_u_u) - 1;
        while (s <= e) {
            const int m = (s + e) >> 1;
            if (zueci_sjis_u_u[m] < u) {
                s = m + 1;
            } else if (zueci_sjis_u_u[m] > u) {
                e = m - 1;
            } else {
                const zueci_u16 mb = zueci_sjis_u_mb[u >= 0x4E00 ? m + 6356 : m]; /* Adjust for URO block */
                if (mb > 0xFF) {
                    dest[0] = (unsigned char) (mb >> 8);
                    dest[1] = (unsigned char) mb;
                    return 2;
                }
                dest[0] = (unsigned char) mb;
                return 1;
            }
        }
    }
    return 0;
}

#ifdef ZUECI_TEST /* Wrapper for direct testing */
ZUECI_INTERN int zueci_u_sjis_test(const zueci_u32 u, unsigned char *dest) {
    return zueci_u_sjis(u, dest);
}
#endif
#endif /* ZUECI_EMBED_NO_TO_ECI */

#ifndef ZUECI_EMBED_NO_TO_UTF8
/* ECI 20 Shift JIS to Unicode */
static int zueci_sjis_u(const unsigned char *src, const zueci_u32 len, const unsigned int flags, zueci_u32 *p_u) {
    unsigned char c1, c2;
    int ind;
    zueci_u32 u2;

    assert(len);
    c1 = src[0];
    if (c1 < 0x80) {
        if (c1 == 0x5C) { /* Backslash to Yen sign */
            *p_u = (flags & ZUECI_FLAG_SJIS_STRAIGHT_THRU) ? c1 : 0xA5;
        } else if (c1 == 0x7E) { /* Tilde to overline */
            *p_u = (flags & ZUECI_FLAG_SJIS_STRAIGHT_THRU) ? c1 : 0x203E;
        } else {
            *p_u = c1;
        }
        return 1;
    }
    if (c1 >= 0xA1 && c1 <= 0xDF) { /* Half-width katakana */
        *p_u = 0xFEC0 + c1;
        return 1;
    }
    if (len < 2 || c1 == 0x80 || c1 == 0xA0 || (c1 > 0xEA && c1 < 0xF0) || c1 > 0xF9) {
        return 0;
    }
    c2 = src[1];
    if (c2 < 0x40 || c2 == 0x7F || c2 > 0xFC) {
        return 0;
    }
    if (c1 >= 0xF0 && c1 <= 0xF9) { /* User-defined to PUA (Table 4-86, Lunde, 2nd ed.) */
        *p_u = 0xE000 + (0xFC - 0x40) * (c1 - 0xF0) + c2 - 0x40 - (c2 > 0x7F);
        return 2;
    }
    ind = (0xFC - 0x40) * (c1 - 0x81 - (c1 > 0xA0) * (0xE0 - 0xA0)) + c2 - 0x40 - (c2 > 0x7F);
    if (ind < ZUECI_ASIZE(zueci_sjis_mb_u) && (u2 = zueci_sjis_mb_u[ind])) {
        *p_u = u2;
        return 2;
    }
    return 0;
}

#ifdef ZUECI_TEST /* Wrapper for direct testing */
ZUECI_INTERN int zueci_sjis_u_test(const unsigned char *src, const zueci_u32 len, const unsigned int flags,
                    zueci_u32 *p_u) {
    return zueci_sjis_u(src, len, flags, p_u);
}
#endif
#endif /* ZUECI_EMBED_NO_TO_UTF8 */

#ifndef ZUECI_EMBED_NO_TO_ECI
/* Unicode to ECI 28 Big5 Chinese (Taiwan) */
static int zueci_u_big5(const zueci_u32 u, unsigned char *dest) {
    int s, e;

    if (u < 0x80) {
        *dest = (unsigned char) u;
        return 1;
    }
    /* Special case URO block sequential mappings (considerably lessens size of `zueci_big5_u_u[]` array) */
    if (u >= 0x4E00 && u < 0xFA0C) { /* 0xFA0C next used value >= 0x4E00 */
        if (u >= 0x9FB0) {
            return 0;
        }
        return zueci_u_lookup_uro(u, zueci_big5_uro_u, zueci_big5_uro_mb_ind, zueci_big5_u_mb, dest);
    }
    if (u >= zueci_big5_u_u[0] && u <= zueci_big5_u_u[ZUECI_ASIZE(zueci_big5_u_u) - 1]) {
        s = 0;
        e = ZUECI_ASIZE(zueci_big5_u_u) - 1;
        while (s <= e) {
            const int m = (s + e) >> 1;
            if (zueci_big5_u_u[m] < u) {
                s = m + 1;
            } else if (zueci_big5_u_u[m] > u) {
                e = m - 1;
            } else {
                const zueci_u16 mb = zueci_big5_u_mb[u >= 0x4E00 ? m + 13061 : m]; /* Adjust for URO block */
                dest[0] = (unsigned char) (mb >> 8);
                dest[1] = (unsigned char) mb;
                return 2;
            }
        }
    }
    return 0;
}

#ifdef ZUECI_TEST /* Wrapper for direct testing */
ZUECI_INTERN int zueci_u_big5_test(const zueci_u32 u, unsigned char *dest) {
    return zueci_u_big5(u, dest);
}
#endif
#endif /* ZUECI_EMBED_NO_TO_ECI */

#ifndef ZUECI_EMBED_NO_TO_UTF8
/* ECI 28 Big5 to Unicode */
static int zueci_big5_u(const unsigned char *src, const zueci_u32 len, const unsigned int flags, zueci_u32 *p_u) {
    unsigned char c1, c2;
    int ind;
    zueci_u32 u2;

    (void)flags;
    assert(len);

    c1 = src[0];
    if (c1 < 0x80) {
        *p_u = c1;
        return 1;
    }
    if (len < 2 || c1 < 0xA1 || c1 == 0xC8 || c1 > 0xF9) {
        return 0;
    }
    c2 = src[1];
    if (c2 < 0x40 || (c2 > 0x7E && c2 < 0xA1) || c2 == 0xFF) {
        return 0;
    }
    ind = 0x9D * (c1 - 0xA1 - (c1 > 0xC8)) + c2 - 0x40 - (c2 > 0x7E) * (0xA1 - 0x7F);
    if (ind < ZUECI_ASIZE(zueci_big5_mb_u) && (u2 = zueci_big5_mb_u[ind])) {
        *p_u = u2;
        return 2;
    }
    return 0;
}

#ifdef ZUECI_TEST /* Wrapper for direct testing */
ZUECI_INTERN int zueci_big5_u_test(const unsigned char *src, const zueci_u32 len, const unsigned int flags,
                    zueci_u32 *p_u) {
    return zueci_big5_u(src, len, flags, p_u);
}
#endif
#endif /* ZUECI_EMBED_NO_TO_UTF8 */

#ifndef ZUECI_EMBED_NO_TO_ECI
/* Unicode to ECI 30 EUC-KR (KS X 1001, formerly KS C 5601) Korean */
static int zueci_u_ksx1001(const zueci_u32 u, unsigned char *dest) {
    int s, e;

    if (u < 0x80) {
        *dest = (unsigned char) u;
        return 1;
    }
    /* Special case URO block sequential mappings (considerably lessens size of `zueci_ksx1001_u_u[]` array) */
    if (u >= 0x4E00 && u < 0xAC00) { /* 0xAC00 next used value >= 0x4E00 */
        if (u >= 0x9FA0) {
            return 0;
        }
        return zueci_u_lookup_uro(u, zueci_ksx1001_uro_u, zueci_ksx1001_uro_mb_ind, zueci_ksx1001_u_mb, dest);
    }
    if (u >= zueci_ksx1001_u_u[0] && u <= zueci_ksx1001_u_u[ZUECI_ASIZE(zueci_ksx1001_u_u) - 1]) {
        s = zueci_ksx1001_u_ind[(u - zueci_ksx1001_u_u[0]) >> 8];
        e = ZUECI_MIN(s + 0x100, ZUECI_ASIZE(zueci_ksx1001_u_u)) - 1;
        while (s <= e) {
            const int m = (s + e) >> 1;
            if (zueci_ksx1001_u_u[m] < u) {
                s = m + 1;
            } else if (zueci_ksx1001_u_u[m] > u) {
                e = m - 1;
            } else {
                const zueci_u16 mb = zueci_ksx1001_u_mb[u >= 0x4E00 ? m + 4620 : m]; /* Adjust for URO block */
                dest[0] = (unsigned char) (mb >> 8);
                dest[1] = (unsigned char) mb;
                return 2;
            }
        }
    }
    return 0;
}

#ifdef ZUECI_TEST /* Wrapper for direct testing */
ZUECI_INTERN int zueci_u_ksx1001_test(const zueci_u32 u, unsigned char *dest) {
    return zueci_u_ksx1001(u, dest);
}
#endif
#endif /* ZUECI_EMBED_NO_TO_ECI */

#ifndef ZUECI_EMBED_NO_TO_UTF8
/* ECI 30 EUC-KR to Unicode */
static int zueci_ksx1001_u(const unsigned char *src, const zueci_u32 len, const unsigned int flags, zueci_u32 *p_u) {
    unsigned char c1, c2;
    int ind;
    zueci_u32 u2;

    (void)flags;
    assert(len);

    c1 = src[0];
    if (c1 < 0x80) {
        *p_u = c1;
        return 1;
    }
    if (len < 2 || c1 < 0xA1 || (c1 > 0xAC && c1 < 0xB0) || c1 == 0xC9 || c1 > 0xFD) {
        return 0;
    }
    c2 = src[1];
    if (c2 < 0xA1 || c2 == 0xFF) {
        return 0;
    }
    ind = (0xFF - 0xA1) * (c1 - 0xA1 - (c1 > 0xAC) * 3 - (c1 > 0xC9)) + c2 - 0xA1;
    assert(ind < ZUECI_ASIZE(zueci_ksx1001_mb_u));
    if ((u2 = zueci_ksx1001_mb_u[ind])) {
        *p_u = u2;
        return 2;
    }
    return 0;
}

#ifdef ZUECI_TEST /* Wrapper for direct testing */
ZUECI_INTERN int zueci_ksx1001_u_test(const unsigned char *src, const zueci_u32 len, const unsigned int flags,
                    zueci_u32 *p_u) {
    return zueci_ksx1001_u(src, len, flags, p_u);
}
#endif
#endif /* ZUECI_EMBED_NO_TO_UTF8 */

#ifndef ZUECI_EMBED_NO_TO_ECI
/* Unicode to ECI 29 GB 2312 Chinese (PRC) */
static int zueci_u_gb2312(const zueci_u32 u, unsigned char *dest) {

    if (u < 0x80) {
        dest[0] = u;
        return 1;
    }
    /* Special case URO block sequential mappings (considerably lessens size of `zueci_gb2312_u_u[]` array) */
    if (u >= 0x4E00 && u < 0x9E1F) { /* 0x9E1F next used non-sequential value >= 0x4E00 */
        if (u >= 0x9CF0) {
            return 0;
        }
        return zueci_u_lookup_uro(u, zueci_gb2312_uro_u, zueci_gb2312_uro_mb_ind, zueci_gb2312_u_mb, dest);
    }
    if (u >= zueci_gb2312_u_u[0] && u <= zueci_gb2312_u_u[ZUECI_ASIZE(zueci_gb2312_u_u) - 1]) {
        int s = zueci_gb2312_u_ind[(u - zueci_gb2312_u_u[0]) >> 8];
        int e = ZUECI_MIN(s + 0x100, ZUECI_ASIZE(zueci_gb2312_u_u)) - 1;
        while (s <= e) {
            const int m = (s + e) >> 1;
            if (zueci_gb2312_u_u[m] < u) {
                s = m + 1;
            } else if (zueci_gb2312_u_u[m] > u) {
                e = m - 1;
            } else {
                const zueci_u16 mb = zueci_gb2312_u_mb[u > 0x4E00 ? m + 6627 : m]; /* Adjust for URO block */
                dest[0] = (unsigned char) (mb >> 8);
                dest[1] = (unsigned char) mb;
                return 2;
            }
        }
    }
    return 0;
}

#ifdef ZUECI_TEST /* Wrapper for direct testing */
ZUECI_INTERN int zueci_u_gb2312_test(const zueci_u32 u, unsigned char *dest) {
    return zueci_u_gb2312(u, dest);
}
#endif
#endif /* ZUECI_EMBED_NO_TO_ECI */

#ifndef ZUECI_EMBED_NO_TO_UTF8
/* ECI 29 GB 2312 to Unicode */
static int zueci_gb2312_u(const unsigned char *src, const zueci_u32 len, const unsigned int flags, zueci_u32 *p_u) {
    unsigned char c1, c2;
    int ind;
    zueci_u32 u2;

    (void)flags;
    assert(len);

    c1 = src[0];
    if (c1 < 0x80) {
        *p_u = c1;
        return 1;
    }
    if (len < 2 || c1 < 0xA1 || (c1 > 0xA9 && c1 < 0xB0) || c1 > 0xF7) {
        return 0;
    }
    c2 = src[1];
    if (c2 < 0xA1 || c2 == 0xFF) {
        return 0;
    }
    ind = (0xFF - 0xA1) * (c1 - 0xA1 - (c1 > 0xA9) * (0xB0 - 0xAA)) + c2 - 0xA1;
    assert(ind < ZUECI_ASIZE(zueci_gb2312_mb_u));
    if ((u2 = zueci_gb2312_mb_u[ind])) {
        *p_u = u2;
        return 2;
    }
    return 0;
}

#ifdef ZUECI_TEST /* Wrapper for direct testing */
ZUECI_INTERN int zueci_gb2312_u_test(const unsigned char *src, const zueci_u32 len, const unsigned int flags,
                    zueci_u32 *p_u) {
    return zueci_gb2312_u(src, len, flags, p_u);
}
#endif
#endif /* ZUECI_EMBED_NO_TO_UTF8 */

#ifndef ZUECI_EMBED_NO_TO_ECI
/* Unicode to ECI 31 GBK Chinese */
static int zueci_u_gbk(const zueci_u32 u, unsigned char *dest) {
    if (u < 0x80) {
        *dest = (unsigned char) u;
        return 1;
    }

    /* Check GB 2312 first */
    if (u == 0x30FB) {
        /* KATAKANA MIDDLE DOT, mapped by GB 2312 but not by GBK (U+00B7 MIDDLE DOT mapped to 0xA1A4 instead) */
        return 0;
    }
    if (u == 0x2015) {
        /* HORIZONTAL BAR, mapped to 0xA844 by GBK rather than 0xA1AA (U+2014 EM DASH mapped there instead) */
        dest[0] = 0xA8;
        dest[1] = 0x44;
        return 2;
    }
    if (zueci_u_gb2312(u, dest)) { /* Includes the 2 GB 6345.1-86 corrections given in Table 3-22, Lunde, 2nd ed. */
        return 2;
    }

    /* Special case URO block sequential mappings (considerably lessens size of `zueci_gbk_u_u[]` array) */
    if (u >= 0x4E00 && u < 0xF92C) { /* 0xF92C next used value >= 0x4E00 */
        if (u >= 0x9FB0) {
            return 0;
        }
        return zueci_u_lookup_uro(u, zueci_gbk_uro_u, zueci_gbk_uro_mb_ind, zueci_gbk_u_mb, dest);
    }
    if (u >= zueci_gbk_u_u[0] && u <= zueci_gbk_u_u[ZUECI_ASIZE(zueci_gbk_u_u) - 1]) {
        int s = 0;
        int e = ZUECI_ASIZE(zueci_gbk_u_u) - 1;
        while (s <= e) {
            const int m = (s + e) >> 1;
            if (zueci_gbk_u_u[m] < u) {
                s = m + 1;
            } else if (zueci_gbk_u_u[m] > u) {
                e = m - 1;
            } else {
                const zueci_u16 mb = zueci_gbk_u_mb[u >= 0x4E00 ? m + 14139 : m]; /* Adjust for URO block */
                dest[0] = (unsigned char) (mb >> 8);
                dest[1] = (unsigned char) mb;
                return 2;
            }
        }
    }
    return 0;
}

#ifdef ZUECI_TEST /* Wrapper for direct testing */
ZUECI_INTERN int zueci_u_gbk_test(const zueci_u32 u, unsigned char *dest) {
    return zueci_u_gbk(u, dest);
}
#endif
#endif /* ZUECI_EMBED_NO_TO_ECI */

#ifndef ZUECI_EMBED_NO_TO_UTF8
/* ECI 31 GBK Chinese to Unicode */
static int zueci_gbk_u(const unsigned char *src, const zueci_u32 len, const unsigned int flags, zueci_u32 *p_u) {
    unsigned char c1, c2;
    int ind;
    zueci_u32 u2;
    zueci_u16 mb;

    (void)flags;
    assert(len);

    c1 = src[0];
    if (c1 < 0x80) {
        *p_u = c1;
        return 1;
    }
    if (len < 2 || c1 < 0x81 || c1 == 0xFF) {
        return 0;
    }
    c2 = src[1];
    if (c2 < 0x40 || c2 == 0x7F || c2 == 0xFF) {
        return 0;
    }

    /* Try GB 2312 first */
    if (((c1 >= 0xA1 && c1 <= 0xA9) || (c1 >= 0xB0 && c1 <= 0xF7)) && c2 >= 0xA1) {
        if (c1 == 0xA1 && c2 == 0xA4) {
            *p_u = 0x00B7; /* MIDDLE DOT */
            return 2;
        }
        if (c1 == 0xA1 && c2 == 0xAA) {
            *p_u = 0x2014; /* EM DASH */
            return 2;
        }
        if (zueci_gb2312_u(src, len, 0 /*flags*/, p_u)) {
            return 2;
        }
    }

    /* Non-URO? */
    mb = ((zueci_u16) c1 << 8) | c2;
    if (mb >= zueci_gbk_nonuro_mb[0] && mb <= zueci_gbk_nonuro_mb[ZUECI_ASIZE(zueci_gbk_nonuro_mb) - 1]) {
        int s = 0;
        int e = ZUECI_ASIZE(zueci_gbk_nonuro_mb) - 1;
        while (s <= e) {
            const int m = (s + e) >> 1;
            if (zueci_gbk_nonuro_mb[m] < mb) {
                s = m + 1;
            } else if (zueci_gbk_nonuro_mb[m] > mb) {
                e = m - 1;
            } else {
                *p_u = zueci_gbk_nonuro_u[m];
                return 2;
            }
        }
    }

    if (c1 >= 0xA1 && (c1 <= 0xA7 || (c1 >= 0xA8 && c2 >= 0xA1))) {
        return 0;
    }
    if (c1 >= 0xA8) {
        ind = (0xFF - 0x40 - 1) * (0xA1 - 0x81) + (0xA1 - 0x40 - 1) * (c1 - 0xA8) + c2 - 0x40 - (c2 > 0x7F);
    } else {
        ind = (0xFF - 0x40 - 1) * (c1 - 0x81) + c2 - 0x40 - (c2 > 0x7F);
    }
    if (ind < ZUECI_ASIZE(zueci_gbk_mb_u) && (u2 = zueci_gbk_mb_u[ind])) {
        *p_u = u2;
        return 2;
    }
    return 0;
}

#ifdef ZUECI_TEST /* Wrapper for direct testing */
ZUECI_INTERN int zueci_gbk_u_test(const unsigned char *src, const zueci_u32 len, const unsigned int flags,
                    zueci_u32 *p_u) {
    return zueci_gbk_u(src, len, flags, p_u);
}
#endif
#endif /* ZUECI_EMBED_NO_TO_UTF8 */

#ifndef ZUECI_EMBED_NO_TO_ECI
/* Helper for `u_gb18030()` to output 4-byte sequential blocks 0x[81-FE][30-39][81-FE][30-39] */
static int zueci_u_gb18030_4_sequential(zueci_u32 u2, zueci_u32 mb_lead, unsigned char *dest) {
    zueci_u32 dv;

    dv = u2 / 10; /* (0x39 - 0x30) + 1 */
    dest[3] = (unsigned char) (u2 - dv * 10 + 0x30);
    u2 = dv;
    dv = u2 / 126; /* (0xFE - 0x81) + 1 */
    dest[2] = (unsigned char) (u2 - dv * 126 + 0x81);
    u2 = dv;
    dv = u2 / 10; /* (0x39 - 0x30) + 1 */
    dest[0] = (unsigned char) (dv + mb_lead);
    dest[1] = (unsigned char) (u2 - dv * 10 + 0x30);
    return 4;
}

/* Unicode to ECI 32 GB 18030 Chinese - assumes valid Unicode */
static int zueci_u_gb18030(const zueci_u32 u, unsigned char *dest) {
    zueci_u32 u2, dv;
    int s, e;

    if (u < 0x80) {
        *dest = (unsigned char) u;
        return 1;
    }

    /* Check GBK first */
    if (zueci_u_gbk(u, dest)) {
        return 2;
    }

    if (u >= 0x10000) {
        /* Non-BMP that were PUA, see Table 3-37, Lunde, 2nd ed. */
        if (u == 0x20087) {
            dest[0] = 0xFE;
            dest[1] = 0x51;
            return 2;
        }
        if (u == 0x20089) {
            dest[0] = 0xFE;
            dest[1] = 0x52;
            return 2;
        }
        if (u == 0x200CC) {
            dest[0] = 0xFE;
            dest[1] = 0x53;
            return 2;
        }
        if (u == 0x215D7) {
            dest[0] = 0xFE;
            dest[1] = 0x6C;
            return 2;
        }
        if (u == 0x2298F) {
            dest[0] = 0xFE;
            dest[1] = 0x76;
            return 2;
        }
        if (u == 0x241FE) {
            dest[0] = 0xFE;
            dest[1] = 0x91;
            return 2;
        }
        /* All other non-BMP U+10000-10FFFF */
        return zueci_u_gb18030_4_sequential(u - 0x10000, 0x90, dest);
    }
    if (u >= 0xE000 && u <= 0xE765) { /* PUA to user-defined */
        if (u <= 0xE4C5) {
            u2 = u - 0xE000;
            dv = u2 / 94;
            dest[0] = (unsigned char) (dv + (dv < 6 ? 0xAA : 0xF2));
            dest[1] = (unsigned char) (u2 - dv * 94 + 0xA1);
        } else {
            zueci_u32 md;
            u2 = u - 0xE4C6;
            dv = u2 / 96;
            md = u2 - dv * 96;
            dest[0] = (unsigned char) (dv + 0xA1);
            dest[1] = (unsigned char) (md + 0x40 + (md >= 0x3F));
        }
        return 2;
    }
    if (u >= zueci_gb18030_2_u_u[0] && u <= zueci_gb18030_2_u_u[ZUECI_ASIZE(zueci_gb18030_2_u_u) - 1]) {
        int s = 0;
        int e = ZUECI_ASIZE(zueci_gb18030_2_u_u) - 1;
        while (s <= e) {
            const int m = (s + e) >> 1;
            if (zueci_gb18030_2_u_u[m] < u) {
                s = m + 1;
            } else if (zueci_gb18030_2_u_u[m] > u) {
                e = m - 1;
            } else {
                const zueci_u16 mb = zueci_gb18030_2_u_mb[m];
                dest[0] = (unsigned char) (mb >> 8);
                dest[1] = (unsigned char) mb;
                return 2;
            }
        }
    }
    /* All other BMP U+0080-FFFF */
    if (u == 0xE7C7) { /* PUA change to non-PUA, see Table 3-39, Lunde, 2nd ed. */
        dest[0] = 0x81;
        dest[1] = 0x35;
        dest[2] = 0xF4;
        dest[3] = 0x37;
        return 4;
    }
    s = 0;
    e = ZUECI_ASIZE(zueci_gb18030_4_u_e) - 1;
    while (s < e) { /* Lower bound */
        const int m = (s + e) >> 1;
        if (zueci_gb18030_4_u_e[m] < u) {
            s = m + 1;
        } else {
            e = m;
        }
    }
    assert(s < ZUECI_ASIZE(zueci_gb18030_4_u_e));
    return zueci_u_gb18030_4_sequential(u - zueci_gb18030_4_u_mb_o[s] - 0x80, 0x81, dest);
}

#ifdef ZUECI_TEST /* Wrapper for direct testing */
ZUECI_INTERN int zueci_u_gb18030_test(const zueci_u32 u, unsigned char *dest) {
    return zueci_u_gb18030(u, dest);
}
#endif
#endif /* ZUECI_EMBED_NO_TO_ECI */

#ifndef ZUECI_EMBED_NO_TO_UTF8
/* Helper to convert GB 18030 4-byter to Unicode */
static zueci_u32 zueci_gb18030_mb4_u(zueci_u32 mb4) {
    unsigned char c1 = (unsigned char) (mb4 >> 24);
    unsigned char c2 = (unsigned char) (mb4 >> 16);
    unsigned char c3 = (unsigned char) (mb4 >> 8);
    unsigned char c4 = (unsigned char) mb4;

    return (((c1 - 0x81) * 10 + (c2 - 0x30)) * 126 + (c3 - 0x81)) * 10 + c4 - 0x30;
}

/* ECI 32 GB 18030 to Unicode */
static int zueci_gb18030_u(const unsigned char *src, const zueci_u32 len, const unsigned int flags, zueci_u32 *p_u) {
    unsigned char c1, c2, c3, c4;
    int ret;
    zueci_u16 mb2;
    zueci_u32 mb4;

    (void)flags;
    assert(len);

    c1 = src[0];
    if (c1 < 0x80) {
        *p_u = c1;
        return 1;
    }
    if (len < 2 || c1 < 0x81 || c1 == 0xFF) {
        return 0;
    }
    if ((ret = zueci_gbk_u(src, len, 0 /*flags*/, p_u))) {
        return ret;
    }
    c2 = src[1];
    if (len >= 4 && c2 <= 0x39 && c2 >= 0x30 && c1 >= 0x81 && c1 <= 0xE3) {
        c3 = src[2];
        c4 = src[3];
        mb4 = ZUECI_4BYTES_U32(c1, c2, c3, c4);
        if (mb4 < 0x81308130 || (mb4 > 0x8431A439 && mb4 < 0x90308130) || mb4 > 0xE3329A35
                || c3 < 0x81 || c3 > 0xFE || c4 < 0x30 || c4 > 0x39) {
            return 0;
        }
        if (mb4 == 0x8135F437) { /* PUA change to non-PUA, see Table 3-39, Lunde, 2nd ed. */
            *p_u = 0xE7C7;
            return 4;
        }
        if (c1 >= 0x90) { /* Non-BMP */
            *p_u = 0x10000 + (((c1 - 0x90) * 10 + (c2 - 0x30)) * 126 + (c3 - 0x81)) * 10 + c4 - 0x30;
        } else { /* BMP */
            int s = 0;
            int e = ZUECI_ASIZE(zueci_gb18030_4_mb_e) - 1;
            while (s < e) { /* Lower bound */
                const int m = (s + e) >> 1;
                if (zueci_gb18030_4_mb_e[m] < mb4) {
                    s = m + 1;
                } else {
                    e = m;
                }
            }
            assert(s < ZUECI_ASIZE(zueci_gb18030_4_mb_e));
            *p_u = zueci_gb18030_4_u_e[s] - (zueci_gb18030_mb4_u(zueci_gb18030_4_mb_e[s]) - zueci_gb18030_mb4_u(mb4));
        }
        return 4;
    }
    if (c2 < 0x40 || c2 == 0x7F || c2 == 0xFF) {
        return 0;
    }
    if (((c1 >= 0xAA && c1 <= 0xAF) || (c1 >= 0xF8 && c1 <= 0xFE)) && c2 >= 0xA1 && c2 <= 0xFE) { /* UDA-1/2 PUA */
        *p_u = 0xE000 + (0xFF - 0xA0 - 1) * (c1 - (c1 >= 0xF8 ? 0xF2 : 0xAA)) + c2 - 0xA1;
        return 2;
    }
    if (c1 >= 0xA1 && c1 <= 0xA7 && c2 <= 0xA1) { /* UDA-3 PUA */
        *p_u = 0xE4C6 + (0xA1 - 0x40 - 1) * (c1 - 0xA1) + c2 - 0x40 - (c2 > 0x7F);
        return 2;
    }
    if (c1 == 0xFE && c2 >= 0x51 && c2 <= 0x91) {
        /* Non-BMP that were PUA, see Table 3-37, Lunde, 2nd ed. */
        if (c2 == 0x51) {
            *p_u = 0x20087;
            return 2;
        }
        if (c2 == 0x52) {
            *p_u = 0x20089;
            return 2;
        }
        if (c2 == 0x53) {
            *p_u = 0x200CC;
            return 2;
        }
        if (c2 == 0x6C) {
            *p_u = 0x215D7;
            return 2;
        }
        if (c2 == 0x76) {
            *p_u = 0x2298F;
            return 2;
        }
        if (c2 == 0x91) {
            *p_u = 0x241FE;
            return 2;
        }
    }
    mb2 = ((zueci_u16) c1 << 8) | c2;
    if (mb2 >= zueci_gb18030_2_mb_mb[0] && mb2 <= zueci_gb18030_2_mb_mb[ZUECI_ASIZE(zueci_gb18030_2_mb_mb) - 1]) {
        int s = 0;
        int e = ZUECI_ASIZE(zueci_gb18030_2_mb_mb) - 1;
        while (s <= e) {
            const int m = (s + e) >> 1;
            if (zueci_gb18030_2_mb_mb[m] < mb2) {
                s = m + 1;
            } else if (zueci_gb18030_2_mb_mb[m] > mb2) {
                e = m - 1;
            } else {
                *p_u = zueci_gb18030_2_mb_u[m];
                return 2;
            }
        }
    }
    return 0;
}

#ifdef ZUECI_TEST /* Wrapper for direct testing */
ZUECI_INTERN int zueci_gb18030_u_test(const unsigned char *src, const zueci_u32 len, const unsigned int flags,
                    zueci_u32 *p_u) {
    return zueci_gb18030_u(src, len, flags, p_u);
}
#endif
#endif /* ZUECI_EMBED_NO_TO_UTF8 */

/* API */


#ifndef ZUECI_EMBED_NO_TO_ECI
/*
    Convert UTF-8 `src` of length `src_len` to `eci`-encoded `dest`.
    `p_dest_len` is set to length of `dest` on output.
    `dest` must be big enough (4-times the `src_len`, or see `zueci_dest_len_eci()`). It is not NUL-terminated.
    Returns 0 if successful, one of `ZUECI_ERROR_XXX` if not
 */
ZUECI_EXTERN int zueci_utf8_to_eci(const int eci, const unsigned char src[], const int src_len,
                    unsigned char dest[], int *p_dest_len) {
    /* Unicode to ECI function table */
    typedef int (*zueci_eci_func_t)(const zueci_u32 u, unsigned char *dest);
    static const zueci_eci_func_t zueci_eci_funcs[36] = {
             zueci_u_cp437,               NULL,      zueci_u_cp437,               NULL,  zueci_u_iso8859_2, /*0-4*/
         zueci_u_iso8859_3,  zueci_u_iso8859_4,  zueci_u_iso8859_5,  zueci_u_iso8859_6,  zueci_u_iso8859_7, /*5-9*/
         zueci_u_iso8859_8,  zueci_u_iso8859_9, zueci_u_iso8859_10, zueci_u_iso8859_11,               NULL, /*10-14*/
        zueci_u_iso8859_13, zueci_u_iso8859_14, zueci_u_iso8859_15, zueci_u_iso8859_16,               NULL, /*15-19*/
              zueci_u_sjis,     zueci_u_cp1250,     zueci_u_cp1251,     zueci_u_cp1252,     zueci_u_cp1256, /*20-24*/
           zueci_u_utf16be,               NULL,      zueci_u_ascii,       zueci_u_big5,     zueci_u_gb2312, /*25-29*/
           zueci_u_ksx1001,        zueci_u_gbk,    zueci_u_gb18030,    zueci_u_utf16le,    zueci_u_utf32be, /*30-34*/
           zueci_u_utf32le,
    };
    unsigned int state = 0;
    const unsigned char *s = src;
    const unsigned char *const se = src + src_len;
    unsigned char *d = dest;
    zueci_eci_func_t eci_func;
    zueci_u32 u;

    if (!zueci_is_valid_eci(eci)) {
        return ZUECI_ERROR_INVALID_ECI;
    }
    if (!src || !dest || !p_dest_len) {
        return ZUECI_ERROR_INVALID_ARGS;
    }

    /* Special case ISO/IEC 8859-1 */
    if (eci == 1 || eci == 3) {
        while (s < se) {
            do {
                zueci_decode_utf8(&state, &u, *s++);
            } while (s < se && state != 0 && state != 12);
            if (state != 0) {
                return ZUECI_ERROR_INVALID_UTF8;
            }
            if (u >= 0x80 && (u < 0xA0 || u >= 0x100)) {
                return ZUECI_ERROR_INVALID_DATA;
            }
            *d++ = (unsigned char) u;
        }
        *p_dest_len = (int) (d - dest);
        return 0;
    }

    /* Special case UTF-8 */
    if (eci == 26) {
        if (!zueci_is_valid_utf8(src, src_len)) {
            return ZUECI_ERROR_INVALID_UTF8;
        }
        memcpy(dest, src, src_len);
        *p_dest_len = src_len;
        return 0;
    }

    if (eci == 170) { /* ASCII Invariant (archaic subset) */
        eci_func = zueci_u_ascii_inv;
    } else if (eci == 899) { /* Binary */
        eci_func = zueci_u_binary;
    } else {
        eci_func = zueci_eci_funcs[eci];
    }

    while (s < se) {
        int incr;
        do {
            zueci_decode_utf8(&state, &u, *s++);
        } while (s < se && state != 0 && state != 12);
        if (state != 0) {
            return ZUECI_ERROR_INVALID_UTF8;
        }
        incr = (*eci_func)(u, d);
        if (incr == 0) {
            return ZUECI_ERROR_INVALID_DATA;
        }
        d += incr;
    }
    *p_dest_len = (int) (d - dest);

    return 0;
}

/*
    Calculate sufficient (i.e. approx.) length needed to convert UTF-8 `src` of length `src_len` from UTF-8 to ECI
    `eci`, and place in `p_dest_len`.
    Returns 0 if successful, one of `ZUECI_ERROR_XXX` if not
 */
ZUECI_EXTERN int zueci_dest_len_eci(const int eci, const unsigned char src[], const int src_len, int *p_dest_len) {
    int dest_len = src_len;

    if (!zueci_is_valid_eci(eci)) {
        return ZUECI_ERROR_INVALID_ECI;
    }
    if (!src || !p_dest_len) {
        return ZUECI_ERROR_INVALID_ARGS;
    }

    if (eci == 20) { /* Shift JIS */
        /* Only ASCII backslash (reverse solidus) exceeds UTF-8 length */
        dest_len += zueci_chr_cnt(src, src_len, '\\');

    } else if (eci == 25 || eci == 33) { /* UTF-16 */
        /* All ASCII chars take 2 bytes */
        dest_len += zueci_chr_lte_cnt(src, src_len, 0x7F);
        /* Surrogate pairs are 4 UTF-8 bytes long so fit */

    } else if (eci == 32) { /* GB 18030 */
        /* Allow for GB 18030 4 byters */
        dest_len *= 2;

    } else if (eci == 34 || eci == 35) { /* UTF-32 */
        /* Quadruple-up ASCII and double-up non-ASCII */
        dest_len += zueci_chr_lte_cnt(src, src_len, 0x7F) * 2 + src_len;
    }

    /* Big5, GB 2312, EUC-KR and GBK fit in UTF-8 length */

    *p_dest_len = dest_len;

    return 0;
}
#endif /* ZUECI_EMBED_NO_TO_ECI */

#ifndef ZUECI_EMBED_NO_TO_UTF8
/* ECI to Unicode function table */
typedef int (*zueci_utf8_func_t)(const unsigned char *src, const zueci_u32 len, const unsigned int flags,
                                    zueci_u32 *p_u);
static const zueci_utf8_func_t zueci_utf8_funcs[36] = {
         zueci_cp437_u,               NULL,      zueci_cp437_u,               NULL,  zueci_iso8859_2_u, /*0-4*/
     zueci_iso8859_3_u,  zueci_iso8859_4_u,  zueci_iso8859_5_u,  zueci_iso8859_6_u,  zueci_iso8859_7_u, /*5-9*/
     zueci_iso8859_8_u,  zueci_iso8859_9_u, zueci_iso8859_10_u, zueci_iso8859_11_u,               NULL, /*10-14*/
    zueci_iso8859_13_u, zueci_iso8859_14_u, zueci_iso8859_15_u, zueci_iso8859_16_u,               NULL, /*15-19*/
          zueci_sjis_u,     zueci_cp1250_u,     zueci_cp1251_u,     zueci_cp1252_u,     zueci_cp1256_u, /*20-24*/
       zueci_utf16be_u,               NULL,      zueci_ascii_u,       zueci_big5_u,     zueci_gb2312_u, /*25-29*/
       zueci_ksx1001_u,        zueci_gbk_u,    zueci_gb18030_u,    zueci_utf16le_u,    zueci_utf32be_u, /*30-34*/
       zueci_utf32le_u,
};

/*
    Convert ECI-encoded `src` of length `src_len` to UTF-8 `dest`.
    `p_dest_len` is set to length of `dest` on output.
    `dest` must be big enough (4-times the `src_len`, or see `zueci_dest_len_utf8()`). It is not NUL-terminated.
    If the Unicode BMP `replacement_char` (<= 0xFFFF) is non-zero then it will substituted for all source characters
    with no mapping and processing will continue, returning ZUECI_WARN_INVALID_DATA unless other errors.
    Returns 0 if successful, one of `ZUECI_ERROR_XXX` if not.
*/
ZUECI_EXTERN int zueci_eci_to_utf8(const int eci, const unsigned char src[], const int src_len,
                    const unsigned int replacement_char, const unsigned int flags, unsigned char dest[],
                    int *p_dest_len) {
    const unsigned char *s = src;
    const unsigned char *const se = src + src_len;
    unsigned char *d = dest;
    zueci_utf8_func_t utf8_func;
    zueci_u32 u;
    int src_incr;
    unsigned char replacement[5];
    int replacement_len = 0; /* g++ complains with "-Wmaybe-uninitialized" if this isn't set */
    int ret = 0;

    if (!zueci_is_valid_eci(eci)) {
        return ZUECI_ERROR_INVALID_ECI;
    }
    if (!src || !dest || !p_dest_len) {
        return ZUECI_ERROR_INVALID_ARGS;
    }

    /* Special case Binary, and if straight-thru flag set then ISO/IEC 8859-1, ASCII and ISO/IEC 646 Invariant also */
    if (eci == 899 || ((flags & ZUECI_FLAG_SB_STRAIGHT_THRU) && (eci == 1 || eci == 3 || eci == 27 || eci == 170))) {
        while (s < se) {
            d += zueci_encode_utf8(*s++, d);
        }
        *p_dest_len = (int) (d - dest);
        return 0;
    }

    if (replacement_char) {
        if (!ZUECI_IS_VALID_UNICODE(replacement_char) || replacement_char > 0xFFFF) { /* Allow BMP only */
            return ZUECI_ERROR_INVALID_ARGS;
        }
        replacement_len = zueci_encode_utf8(replacement_char, replacement);
    }

    /* Special case ISO/IEC 8859-1 */
    if (eci == 1 || eci == 3) {
        for (; s < se; s++) {
            if (*s >= 0x80 && *s < 0xA0) {
                if (!replacement_char) {
                    return ZUECI_ERROR_INVALID_DATA;
                }
                memcpy(d, replacement, replacement_len);
                d += replacement_len;
                ret = ZUECI_WARN_INVALID_DATA;
            } else {
                d += zueci_encode_utf8(*s, d);
            }
        }
        *p_dest_len = (int) (d - dest);
        return ret;
    }

    /* Special case UTF-8 */
    if (eci == 26) {
        if (replacement_char) {
            unsigned int state = 0;
            while (s < se) {
                do {
                    zueci_decode_utf8(&state, &u, *s++);
                } while (s < se && state != 0 && state != 12);
                if (state != 0) {
                    if (*(s - 1) < 0x80) { /* If previous ASCII, backtrack */
                        s--;
                    } else {
                        while (s < se && (*s & 0xC0) == 0x80) { /* Skip any continuation bytes */
                            s++;
                        }
                    }
                    memcpy(d, replacement, replacement_len);
                    d += replacement_len;
                    ret = ZUECI_WARN_INVALID_DATA;
                    state = 0;
                } else {
                    d += zueci_encode_utf8(u, d);
                }
            }
            *p_dest_len = (int) (d - dest);
            return ret;
        }
        if (!zueci_is_valid_utf8(src, src_len)) {
            return ZUECI_ERROR_INVALID_UTF8;
        }
        memcpy(dest, src, src_len);
        *p_dest_len = src_len;
        return 0;
    }

    if (eci == 170) {
        utf8_func = zueci_ascii_inv_u;
    } else {
        utf8_func = zueci_utf8_funcs[eci];
    }

    while (s < se) {
        if (!(src_incr = (*utf8_func)(s, (int) (se - s), flags, &u))) {
            if (!replacement_char) {
                return ZUECI_ERROR_INVALID_DATA;
            }
            memcpy(d, replacement, replacement_len);
            s += zueci_replacement_incr(eci, s, (int) (se - s));
            d += replacement_len;
            ret = ZUECI_WARN_INVALID_DATA;
        } else {
            s += src_incr;
            d += zueci_encode_utf8(u, d);
        }
    }
    *p_dest_len = (int) (d - dest);
    return ret;
}

/*
    Calculate exact length needed to convert ECI-encoded `src` of length `len` from ECI `eci`, and place in
    `p_dest_len`.
    Returns 0 if successful, one of `ZUECI_ERROR_XXX` if not.
 */
ZUECI_EXTERN int zueci_dest_len_utf8(const int eci, const unsigned char src[], const int src_len,
                const unsigned int replacement_char, const int unsigned flags, int *p_dest_len) {
    const unsigned char *s = src;
    const unsigned char *const se = src + src_len;
    zueci_utf8_func_t utf8_func;
    zueci_u32 u;
    int src_incr;
    unsigned char replacement[5];
    int replacement_len = 0; /* g++ complains with "-Wmaybe-uninitialized" if this isn't set */
    int dest_len = 0;
    int ret = 0;

    /* NOTE: the following is "unrolled" from `zueci_eci_to_utf8()` and should be the same except for the copying */

    if (!zueci_is_valid_eci(eci)) {
        return ZUECI_ERROR_INVALID_ECI;
    }
    if (!src || !p_dest_len) {
        return ZUECI_ERROR_INVALID_ARGS;
    }

    /* Special case Binary, and if straight-thru flag set then ISO/IEC 8859-1, ASCII and ISO/IEC 646 Invariant also */
    if (eci == 899 || ((flags & ZUECI_FLAG_SB_STRAIGHT_THRU) && (eci == 1 || eci == 3 || eci == 27 || eci == 170))) {
        while (s < se) {
            dest_len += 1 + (*s++ >= 0x80);
        }
        *p_dest_len = dest_len;
        return 0;
    }

    if (replacement_char) {
        if (!ZUECI_IS_VALID_UNICODE(replacement_char) || replacement_char > 0xFFFF) { /* Allow BMP only */
            return ZUECI_ERROR_INVALID_ARGS;
        }
        replacement_len = zueci_encode_utf8(replacement_char, replacement);
    }

    /* Special case ISO/IEC 8859-1 */
    if (eci == 1 || eci == 3) {
        for (; s < se; s++) {
            if (*s >= 0x80 && *s < 0xA0) {
                if (!replacement_char) {
                    return ZUECI_ERROR_INVALID_DATA;
                }
                dest_len += replacement_len;
                ret = ZUECI_WARN_INVALID_DATA;
            } else {
                dest_len += 1 + (*s >= 0x80);
            }
        }
        *p_dest_len = dest_len;
        return ret;
    }

    /* Special case UTF-8 */
    if (eci == 26) {
        unsigned int state = 0;
        while (s < se) {
            do {
                zueci_decode_utf8(&state, &u, *s++);
            } while (s < se && state != 0 && state != 12);
            if (state != 0) {
                if (!replacement_char) {
                    return ZUECI_ERROR_INVALID_UTF8;
                }
                if (*(s - 1) < 0x80) { /* If previous ASCII, backtrack */
                    s--;
                } else {
                    while (s < se && (*s & 0xC0) == 0x80) { /* Skip any continuation bytes */
                        s++;
                    }
                }
                dest_len += replacement_len;
                ret = ZUECI_WARN_INVALID_DATA;
                state = 0;
            } else {
                dest_len += 1 + (u >= 0x80) + (u >= 0x800) + (u >= 0x10000);
            }
        }
        *p_dest_len = dest_len;
        return ret;
    }

    if (eci == 170) {
        utf8_func = zueci_ascii_inv_u;
    } else {
        utf8_func = zueci_utf8_funcs[eci];
    }

    while (s < se) {
        if (!(src_incr = (*utf8_func)(s, (int) (se - s), flags, &u))) {
            if (!replacement_char) {
                return ZUECI_ERROR_INVALID_DATA;
            }
            s += zueci_replacement_incr(eci, s, (int) (se - s));
            dest_len += replacement_len;
            ret = ZUECI_WARN_INVALID_DATA;
        } else {
            s += src_incr;
            dest_len += 1 + (u >= 0x80) + (u >= 0x800) + (u >= 0x10000);
        }
    }
    *p_dest_len = dest_len;
    return ret;
}
#endif /* ZUECI_EMBED_NO_TO_UTF8 */

/* vim: set ts=4 sw=4 et : */
