/*  eci.c - Extended Channel Interpretations */
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

#include <assert.h>
#include "common.h"
#include "eci.h"
#include "eci_sb.h"
#include "big5.h"
#include "gb18030.h"
#include "gb2312.h"
#include "gbk.h"
#include "ksx1001.h"
#include "sjis.h"

/* Single-byte stuff */

/* Base ISO/IEC 8859 routine to convert Unicode codepoint `u` */
static int u_iso8859(const unsigned int u, const unsigned short *tab_s, const unsigned short *tab_u,
            const unsigned char *tab_sb, int e, unsigned char *dest) {
    int s;
    if (u < 0xA0) {
        if (u >= 0x80) { /* U+0080-9F fail */
            return 0;
        }
        *dest = (unsigned char) u;
        return 1;
    }
    if (u <= 0xFF) {
        const unsigned int u2 = u - 0xA0;
        if (tab_s[u2 >> 4] & ((unsigned short) 1 << (u2 & 0xF))) {
            *dest = (unsigned char) u; /* Straight-thru */
            return 1;
        }
    }

    s = 0;
    while (s <= e) {
        const int m = (s + e) >> 1;
        if (tab_u[m] < u) {
            s = m + 1;
        } else if (tab_u[m] > u) {
            e = m - 1;
        } else {
            *dest = tab_sb[m];
            return 1;
        }
    }
    return 0;
}

/* Base Windows-125x routine to convert Unicode codepoint `u` */
static int u_cp125x(const unsigned int u, const unsigned short *tab_s, const unsigned short *tab_u,
            const unsigned char *tab_sb, int e, unsigned char *dest) {
    int s;
    if (u < 0x80) {
        *dest = (unsigned char) u;
        return 1;
    }
    if (u <= 0xFF && u >= 0xA0) {
        const unsigned int u2 = u - 0xA0;
        if (tab_s[u2 >> 4] & ((unsigned short) 1 << (u2 & 0xF))) {
            *dest = (unsigned char) u; /* Straight-thru */
            return 1;
        }
    }

    s = 0;
    while (s <= e) {
        const int m = (s + e) >> 1;
        if (tab_u[m] < u) {
            s = m + 1;
        } else if (tab_u[m] > u) {
            e = m - 1;
        } else {
            *dest = tab_sb[m];
            return 1;
        }
    }
    return 0;
}

/* ECI 27 ASCII (ISO/IEC 646:1991 IRV (US)) */
static int u_ascii(const unsigned int u, unsigned char *dest) {
    if (u < 0x80) {
        *dest = (unsigned char) u;
        return 1;
    }
    return 0;
}

/* ECI 170 ASCII subset (ISO/IEC 646:1991 Invariant), excludes 12 chars that historically had national variants,
    namely "#$@[\]^`{|}~" */
static int u_ascii_inv(const unsigned int u, unsigned char *dest) {
    if (u == 0x7F || (u <= 'z' && u != '#' && u != '$' && u != '@' && (u <= 'Z' || u == '_' || u >= 'a'))) {
        *dest = (unsigned char) u;
        return 1;
    }
    return 0;
}

/* ECI 25 UTF-16 Big Endian (ISO/IEC 10646) - assumes valid Unicode */
static int u_utf16be(const unsigned int u, unsigned char *dest) {
    unsigned int u2, v;
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

/* ECI 33 UTF-16 Little Endian (ISO/IEC 10646) - assumes valid Unicode */
static int u_utf16le(const unsigned int u, unsigned char *dest) {
    unsigned int u2, v;
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

/* ECI 34 UTF-32 Big Endian (ISO/IEC 10646) - assumes valid Unicode */
static int u_utf32be(const unsigned int u, unsigned char *dest) {
    dest[0] = 0;
    dest[1] = (unsigned char) (u >> 16);
    dest[2] = (unsigned char) (u >> 8);
    dest[3] = (unsigned char) u;
    return 4;
}

/* ECI 35 UTF-32 Little Endian (ISO/IEC 10646) - assumes valid Unicode */
static int u_utf32le(const unsigned int u, unsigned char *dest) {
    dest[0] = (unsigned char) u;
    dest[1] = (unsigned char) (u >> 8);
    dest[2] = (unsigned char) (u >> 16);
    dest[3] = 0;
    return 4;
}

/* Multibyte stuff */

/* Acknowledgements to Bruno Haible <bruno@clisp.org> for a no. of techniques used here */

/* Helper to lookup Unicode codepoint `u` in the URO (Unified Repertoire and Ordering) block (U+4E00-9FFF) */
static int eci_u_lookup_uro_int(const unsigned int u, const unsigned short *tab_u, const unsigned short *tab_mb_ind,
            const unsigned short *tab_mb, unsigned int *d) {
    unsigned int u2 = (u - 0x4E00) >> 4; /* Blocks of 16 */
    unsigned int v = (unsigned int) 1 << (u & 0xF);
    if ((tab_u[u2] & v) == 0) {
        return 0;
    }
    v = tab_u[u2] & (v - 1); /* Mask to bits prior to this one */
    /* Count bits set (http://graphics.stanford.edu/~seander/bithacks.html#CountBitsSetParallel) */
    v = v - ((v >> 1) & 0x55555555);
    v = (v & 0x33333333) + ((v >> 2) & 0x33333333);
    v = (((v + (v >> 4)) & 0xF0F0F0F) * 0x1010101) >> 24;
    *d = tab_mb[tab_mb_ind[u2] + v];
    return 2;
}

/* Version of `eci_u_lookup_uro_int()` taking unsigned char destination */
static int eci_u_lookup_uro(const unsigned int u, const unsigned short *tab_u, const unsigned short *tab_mb_ind,
            const unsigned short *tab_mb, unsigned char *dest) {
    unsigned int d;
    int ret = eci_u_lookup_uro_int(u, tab_u, tab_mb_ind, tab_mb, &d);
    if (ret) {
        dest[0] = (unsigned char) (d >> 8);
        dest[1] = (unsigned char) d;
    }
    return ret;
}

/* ECI 20 Shift JIS */
static int u_sjis_int(const unsigned int u, unsigned int *d) {
    unsigned int u2, dv, md;
    int s, e;

    if (u < 0x80 && u != 0x5C && u != 0x7E) { /* Backslash & tilde re-mapped according to JIS X 0201 Roman */
        *d = u;
        return 1;
    }
    /* Special case URO block sequential mappings (considerably lessens size of `sjis_u[]` array) */
    if (u >= 0x4E00 && u <= 0xDFFF) { /* 0xE000 next used value >= 0x4E00 */
        if (u >= 0x9FB0) {
            return 0;
        }
        return eci_u_lookup_uro_int(u, sjis_uro_u, sjis_uro_mb_ind, sjis_mb, d);
    }
    /* PUA to user-defined (Table 4-86, Lunde, 2nd ed.) */
    if (u >= 0xE000 && u <= 0xE757) {
        u2 = u - 0xE000;
        dv = u2 / 188;
        md = u2 - dv * 188;
        *d = ((dv + 0xF0) << 8) | (md + 0x40 + (md >= 0x3F));
        return 2;
    }
    if (u >= sjis_u[0] && u <= sjis_u[ARRAY_SIZE(sjis_u) - 1]) {
        s = 0;
        e = ARRAY_SIZE(sjis_u) - 1;
        while (s <= e) {
            const int m = (s + e) >> 1;
            if (sjis_u[m] < u) {
                s = m + 1;
            } else if (sjis_u[m] > u) {
                e = m - 1;
            } else {
                *d = sjis_mb[u >= 0x4E00 ? m + 6356 : m]; /* Adjust for URO block */
                return 1 + (*d > 0xFF);
            }
        }
    }
    return 0;
}

#ifdef ZINT_TEST /* Wrapper for direct testing */
INTERNAL int u_sjis_int_test(const unsigned int u, unsigned int *d) {
    return u_sjis_int(u, d);
}
#endif

/* Version of `u_sjis_int()` taking unsigned char destination, for use by `utf8_to_eci()` */
static int u_sjis(const unsigned int u, unsigned char *dest) {
    unsigned int d;
    int ret = u_sjis_int(u, &d);
    if (ret) {
        if (ret == 1) {
            dest[0] = (unsigned char) d;
        } else {
            dest[0] = (unsigned char) (d >> 8);
            dest[1] = (unsigned char) d;
        }
    }
    return ret;
}

/* ECI 28 Big5 Chinese (Taiwan) */
static int u_big5(const unsigned int u, unsigned char *dest) {
    int s, e;

    if (u < 0x80) {
        *dest = (unsigned char) u;
        return 1;
    }
    /* Special case URO block sequential mappings (considerably lessens size of `big5_u[]` array) */
    if (u >= 0x4E00 && u <= 0xFA0B) { /* 0xFA0C next used value >= 0x4E00 */
        if (u >= 0x9FB0) {
            return 0;
        }
        return eci_u_lookup_uro(u, big5_uro_u, big5_uro_mb_ind, big5_mb, dest);
    }
    if (u >= big5_u[0] && u <= big5_u[ARRAY_SIZE(big5_u) - 1]) {
        s = 0;
        e = ARRAY_SIZE(big5_u) - 1;
        while (s <= e) {
            const int m = (s + e) >> 1;
            if (big5_u[m] < u) {
                s = m + 1;
            } else if (big5_u[m] > u) {
                e = m - 1;
            } else {
                const unsigned short mb = big5_mb[u >= 0x4E00 ? m + 13061 : m]; /* Adjust for URO block */
                dest[0] = (unsigned char) (mb >> 8);
                dest[1] = (unsigned char) mb;
                return 2;
            }
        }
    }
    return 0;
}

#ifdef ZINT_TEST /* Wrapper for direct testing */
INTERNAL int u_big5_test(const unsigned int u, unsigned char *dest) {
    return u_big5(u, dest);
}
#endif

/* ECI 30 EUC-KR (KS X 1001, formerly KS C 5601) Korean */
static int u_ksx1001(const unsigned int u, unsigned char *dest) {
    int s, e;

    if (u < 0x80) {
        *dest = (unsigned char) u;
        return 1;
    }
    /* Special case URO block sequential mappings (considerably lessens size of `ksx1001_u[]` array) */
    if (u >= 0x4E00 && u <= 0xABFF) { /* 0xAC00 next used value >= 0x4E00 */
        if (u >= 0x9FA0) {
            return 0;
        }
        return eci_u_lookup_uro(u, ksx1001_uro_u, ksx1001_uro_mb_ind, ksx1001_mb, dest);
    }
    if (u >= ksx1001_u[0] && u <= ksx1001_u[ARRAY_SIZE(ksx1001_u) - 1]) {
        s = ksx1001_u_ind[(u - ksx1001_u[0]) >> 8];
        e = s + 0x100 > ARRAY_SIZE(ksx1001_u) ? ARRAY_SIZE(ksx1001_u) - 1 : s + 0x100 - 1;
        while (s <= e) {
            const int m = (s + e) >> 1;
            if (ksx1001_u[m] < u) {
                s = m + 1;
            } else if (ksx1001_u[m] > u) {
                e = m - 1;
            } else {
                const unsigned short mb = ksx1001_mb[u >= 0x4E00 ? m + 4620 : m]; /* Adjust for URO block */
                dest[0] = (unsigned char) (mb >> 8);
                dest[1] = (unsigned char) mb;
                return 2;
            }
        }
    }
    return 0;
}

#ifdef ZINT_TEST /* Wrapper for direct testing */
INTERNAL int u_ksx1001_test(const unsigned int u, unsigned char *dest) {
    return u_ksx1001(u, dest);
}
#endif

/* ECI 29 GB 2312 Chinese (PRC) */
static int u_gb2312_int(const unsigned int u, unsigned int *d) {
    int s, e;

    if (u < 0x80) {
        *d = u;
        return 1;
    }
    /* Special case URO block sequential mappings (considerably lessens size of `gb2312_u[]` array) */
    if (u >= 0x4E00 && u <= 0x9E1E) { /* 0x9E1F next used value >= 0x4E00 */
        if (u >= 0x9CF0) {
            return 0;
        }
        return eci_u_lookup_uro_int(u, gb2312_uro_u, gb2312_uro_mb_ind, gb2312_mb, d);
    }
    if (u >= gb2312_u[0] && u <= gb2312_u[ARRAY_SIZE(gb2312_u) - 1]) {
        s = gb2312_u_ind[(u - gb2312_u[0]) >> 8];
        e = s + 0x100 > ARRAY_SIZE(gb2312_u) ? ARRAY_SIZE(gb2312_u) - 1 : s + 0x100 - 1;
        while (s <= e) {
            const int m = (s + e) >> 1;
            if (gb2312_u[m] < u) {
                s = m + 1;
            } else if (gb2312_u[m] > u) {
                e = m - 1;
            } else {
                *d = gb2312_mb[u > 0x4E00 ? m + 6627 : m]; /* Adjust for URO block */
                return 2;
            }
        }
    }
    return 0;
}

#ifdef ZINT_TEST /* Wrapper for direct testing */
INTERNAL int u_gb2312_int_test(const unsigned int u, unsigned int *d) {
    return u_gb2312_int(u, d);
}
#endif

/* Version of `u_gb2312_int()` taking unsigned char destination, for use by `utf8_to_eci()` */
static int u_gb2312(const unsigned int u, unsigned char *dest) {
    unsigned int d;
    int ret = u_gb2312_int(u, &d);
    if (ret) {
        if (ret == 1) {
            dest[0] = (unsigned char) d;
        } else {
            dest[0] = (unsigned char) (d >> 8);
            dest[1] = (unsigned char) d;
        }
    }
    return ret;
}

/* ECI 31 GBK Chinese */
static int u_gbk_int(const unsigned int u, unsigned int *d) {
    int s, e;

    if (u < 0x80) {
        *d = u;
        return 1;
    }

    /* Check GB 2312 first */
    if (u == 0x30FB) {
        /* KATAKANA MIDDLE DOT, mapped by GB 2312 but not by GBK (U+00B7 MIDDLE DOT mapped to 0xA1A4 instead) */
        return 0;
    }
    if (u == 0x2015) {
        /* HORIZONTAL BAR, mapped to 0xA844 by GBK rather than 0xA1AA (U+2014 EM DASH mapped there instead) */
        *d = 0xA844;
        return 2;
    }
    if (u_gb2312_int(u, d)) { /* Includes the 2 GB 6345.1-86 corrections given in Table 3-22, Lunde, 2nd ed. */
        return 2;
    }

    /* Special case URO block sequential mappings (considerably lessens size of `gbk_u[]` array) */
    if (u >= 0x4E00 && u <= 0xF92B) { /* 0xF92C next used value >= 0x4E00 */
        if (u >= 0x9FB0) {
            return 0;
        }
        return eci_u_lookup_uro_int(u, gbk_uro_u, gbk_uro_mb_ind, gbk_mb, d);
    }
    if (u >= gbk_u[0] && u <= gbk_u[ARRAY_SIZE(gbk_u) - 1]) {
        s = 0;
        e = ARRAY_SIZE(gbk_u) - 1;
        while (s <= e) {
            const int m = (s + e) >> 1;
            if (gbk_u[m] < u) {
                s = m + 1;
            } else if (gbk_u[m] > u) {
                e = m - 1;
            } else {
                *d = gbk_mb[u >= 0x4E00 ? m + 14139 : m]; /* Adjust for URO block */
                return 2;
            }
        }
    }
    return 0;
}

#ifdef ZINT_TEST /* Wrapper for direct testing */
INTERNAL int u_gbk_int_test(const unsigned int u, unsigned int *d) {
    return u_gbk_int(u, d);
}
#endif

/* Version of `u_gbk_int()` taking unsigned char destination, for use by `utf8_to_eci()` */
static int u_gbk(const unsigned int u, unsigned char *dest) {
    unsigned int d;
    int ret = u_gbk_int(u, &d);
    if (ret) {
        if (ret == 1) {
            dest[0] = (unsigned char) d;
        } else {
            dest[0] = (unsigned char) (d >> 8);
            dest[1] = (unsigned char) d;
        }
    }
    return ret;
}

/* Helper for `u_gb18030_int()` to output 4-byte sequential blocks */
static int u_gb18030_4_sequential_int(unsigned int u2, unsigned int mb_lead, unsigned int *d1, unsigned int *d2) {
    unsigned int dv;

    dv = u2 / 10;
    *d2 = u2 - dv * 10 + 0x30;
    u2 = dv;
    dv = u2 / 126;
    *d2 |= (u2 - dv * 126 + 0x81) << 8;
    u2 = dv;
    dv = u2 / 10;
    *d1 = ((dv + mb_lead) << 8) | (u2 - dv * 10 + 0x30);
    return 4;
}

/* ECI 32 GB 18030 Chinese - assumes valid Unicode */
static int u_gb18030_int(const unsigned int u, unsigned int *d1, unsigned int *d2) {
    unsigned int u2, dv;
    int s, e;

    if (u < 0x80) {
        *d1 = u;
        return 1;
    }

    /* Check GBK first */
    if (u_gbk_int(u, d1)) {
        return 2;
    }

    if (u >= 0x10000) {
        /* Non-PUA, non-BMP, see Table 3-37, Lunde, 2nd ed. */
        if (u == 0x20087) {
            *d1 = 0xFE51;
            return 2;
        }
        if (u == 0x20089) {
            *d1 = 0xFE52;
            return 2;
        }
        if (u == 0x200CC) {
            *d1 = 0xFE53;
            return 2;
        }
        if (u == 0x215D7) {
            *d1 = 0xFE6C;
            return 2;
        }
        if (u == 0x2298F) {
            *d1 = 0xFE76;
            return 2;
        }
        if (u == 0x241FE) {
            *d1 = 0xFE91;
            return 2;
        }
        /* All other non-BMP U+10000-10FFFF */
        return u_gb18030_4_sequential_int(u - 0x10000, 0x90, d1, d2);
    }
    if (u >= 0xE000 && u <= 0xE765) { /* PUA to user-defined */
        if (u <= 0xE4C5) {
            u2 = u - 0xE000;
            dv = u2 / 94;
            *d1 = ((dv + (dv < 6 ? 0xAA : 0xF2)) << 8) | (u2 - dv * 94 + 0xA1);
        } else {
            unsigned int md;
            u2 = u - 0xE4C6;
            dv = u2 / 96;
            md = u2 - dv * 96;
            *d1 = ((dv + 0xA1) << 8) | (md + 0x40 + (md >= 0x3F));
        }
        return 2;
    }
    if (u >= gb18030_2_u[0] && u <= gb18030_2_u[ARRAY_SIZE(gb18030_2_u) - 1]) {
        s = 0;
        e = ARRAY_SIZE(gb18030_2_u) - 1;
        while (s <= e) {
            const int m = (s + e) >> 1;
            if (gb18030_2_u[m] < u) {
                s = m + 1;
            } else if (gb18030_2_u[m] > u) {
                e = m - 1;
            } else {
                *d1 = gb18030_2_mb[m];
                return 2;
            }
        }
    }
    /* All other BMP U+0080-FFFF */
    if (u == 0xE7C7) { /* PUA change to non-PUA, see Table 3-39, Lunde, 2nd ed. */
        *d1 = 0x8135;
        *d2 = 0xF437;
        return 4;
    }
    s = 0;
    e = ARRAY_SIZE(gb18030_4_u_e) - 1;
    while (s < e) { /* Lower bound */
        const int m = (s + e) >> 1;
        if (gb18030_4_u_e[m] < u) {
            s = m + 1;
        } else {
            e = m;
        }
    }
    assert(s < ARRAY_SIZE(gb18030_4_u_e));
    return u_gb18030_4_sequential_int(u - gb18030_4_mb_o[s] - 0x80, 0x81, d1, d2);
}

#ifdef ZINT_TEST /* Wrapper for direct testing */
INTERNAL int u_gb18030_int_test(const unsigned int u, unsigned int *d1, unsigned int *d2) {
    return u_gb18030_int(u, d1, d2);
}
#endif

/* Version of `u_gb18030_int()` taking unsigned char destination, for use by `utf8_to_eci()` */
static int u_gb18030(const unsigned int u, unsigned char *dest) {
    unsigned int d1, d2;
    int ret = u_gb18030_int(u, &d1, &d2);
    if (ret) {
        if (ret == 1) {
            dest[0] = (unsigned char) d1;
        } else {
            dest[0] = (unsigned char) (d1 >> 8);
            dest[1] = (unsigned char) d1;
            if (ret == 4) {
                dest[2] = (unsigned char) (d2 >> 8);
                dest[3] = (unsigned char) d2;
            }
        }
    }
    return ret;
}

/* Main ECI stuff */

/* Helper to count the number of chars in a string within a range */
static int chr_range_cnt(const unsigned char string[], const int length, const unsigned char c1,
            const unsigned char c2) {
    int count = 0;
    int i;
    if (c1) {
        for (i = 0; i < length; i++) {
            if (string[i] >= c1 && string[i] <= c2) {
                count++;
            }
        }
    } else {
        for (i = 0; i < length; i++) {
            if (string[i] <= c2) {
                count++;
            }
        }
    }
    return count;
}

/* Is ECI convertible from UTF-8? */
INTERNAL int is_eci_convertible(const int eci) {
    if (eci == 26 || (eci > 35 && eci != 170)) { /* Exclude ECI 170 - ASCII Invariant */
        /* UTF-8 (26) or 8-bit binary data (899) or undefined (> 35 and < 899) or not character set (> 899) */
        return 0;
    }
    return 1;
}

/* Are any of the ECIs in the segments convertible from UTF-8?
   Sets `convertible[]` for each, which must be at least `seg_count` in size */
INTERNAL int is_eci_convertible_segs(const struct zint_seg segs[], const int seg_count, int convertible[]) {
    int ret = 0;
    int i;
    for (i = 0; i < seg_count; i++) {
        convertible[i] = is_eci_convertible(segs[i].eci);
        ret |= convertible[i];
    }
    return ret;
}

/* Calculate length required to convert UTF-8 to (double-byte) encoding */
INTERNAL int get_eci_length(const int eci, const unsigned char source[], int length) {
    if (eci == 20) { /* Shift JIS */
        /* Only ASCII backslash (reverse solidus) exceeds UTF-8 length */
        length += chr_cnt(source, length, '\\');

    } else if (eci == 25 || eci == 33) { /* UTF-16 */
        /* All ASCII chars take 2 bytes */
        length += chr_range_cnt(source, length, 0, 0x7F);
        /* Surrogate pairs are 4 UTF-8 bytes long so fit */

    } else if (eci == 32) { /* GB 18030 */
        /* Allow for GB 18030 4 byters */
        length *= 2;

    } else if (eci == 34 || eci == 35) { /* UTF-32 */
        /* Quadruple-up ASCII and double-up non-ASCII */
        length += chr_range_cnt(source, length, 0, 0x7F) * 2 + length;
    }

    /* Big5, GB 2312, EUC-KR and GBK fit in UTF-8 length */

    return length;
}

/* Call `get_eci_length()` for each segment, returning total */
INTERNAL int get_eci_length_segs(const struct zint_seg segs[], const int seg_count) {
    int length = 0;
    int i;

    for (i = 0; i < seg_count; i++) {
        length += get_eci_length(segs[i].eci, segs[i].source, segs[i].length);
    }

    return length;
}

/* Convert UTF-8 to other character encodings */
typedef int (*eci_func_t)(const unsigned int u, unsigned char *dest);
INTERNAL int utf8_to_eci(const int eci, const unsigned char source[], unsigned char dest[], int *p_length) {

    static const eci_func_t eci_funcs[36] = {
                NULL,         NULL,         NULL,         NULL,  u_iso8859_2, /*0-4*/
         u_iso8859_3,  u_iso8859_4,  u_iso8859_5,  u_iso8859_6,  u_iso8859_7, /*5-9*/
         u_iso8859_8,  u_iso8859_9, u_iso8859_10, u_iso8859_11,         NULL, /*10-14*/
        u_iso8859_13, u_iso8859_14, u_iso8859_15, u_iso8859_16,         NULL, /*15-19*/
              u_sjis,     u_cp1250,     u_cp1251,     u_cp1252,     u_cp1256, /*20-24*/
           u_utf16be,         NULL,      u_ascii,       u_big5,     u_gb2312, /*25-29*/
           u_ksx1001,        u_gbk,    u_gb18030,    u_utf16le,    u_utf32be, /*30-34*/
           u_utf32le,
    };
    eci_func_t eci_func;
    unsigned int codepoint, state = 0;
    int in_posn = 0;
    int out_posn = 0;
    int length = *p_length;

    /* Special case ISO/IEC 8859-1 */
    if (eci == 0 || eci == 3) { /* Default ECI 0 to ISO/IEC 8859-1 */
        while (in_posn < length) {
            do {
                decode_utf8(&state, &codepoint, source[in_posn++]);
            } while (in_posn < length && state != 0 && state != 12);
            if (state != 0) {
                return ZINT_ERROR_INVALID_DATA;
            }
            if (codepoint >= 0x80 && (codepoint < 0xA0 || codepoint >= 0x100)) {
                return ZINT_ERROR_INVALID_DATA;
            }
            dest[out_posn++] = (unsigned char) codepoint;
        }
        dest[out_posn] = '\0';
        *p_length = out_posn;
        return 0;
    }

    if (eci == 170) { /* ASCII Invariant (archaic subset) */
        eci_func = u_ascii_inv;
    } else {
        eci_func = eci_funcs[eci];
        if (eci_func == NULL) {
            return ZINT_ERROR_INVALID_DATA;
        }
    }

    while (in_posn < length) {
        int incr;
        do {
            decode_utf8(&state, &codepoint, source[in_posn++]);
        } while (in_posn < length && state != 0 && state != 12);
        if (state != 0) {
            return ZINT_ERROR_INVALID_DATA;
        }
        incr = (*eci_func)(codepoint, dest + out_posn);
        if (incr == 0) {
            return ZINT_ERROR_INVALID_DATA;
        }
        out_posn += incr;
    }
    dest[out_posn] = '\0';
    *p_length = out_posn;

    return 0;
}

/* Find the lowest single-byte ECI mode which will encode a given set of Unicode text, assuming valid UTF-8 */
INTERNAL int get_best_eci(const unsigned char source[], int length) {
    int eci = 3;
    /* Note: attempting single-byte conversions only, so get_eci_length() unnecessary */
    unsigned char *local_source = (unsigned char *) z_alloca(length + 1);

    do {
        if (eci == 14) { /* Reserved */
            eci = 15;
        } else if (eci == 19) { /* Reserved */
            eci = 21; /* Skip 20 Shift JIS */
        }
        if (utf8_to_eci(eci, source, local_source, &length) == 0) {
            return eci;
        }
        eci++;
    } while (eci < 25);

    assert(is_valid_utf8(source, length));

    return 26; /* If all of these fail, use UTF-8! */
}

/* Call `get_best_eci()` for each segment, assuming valid UTF-8. Returns 0 on failure, first ECI set on success */
INTERNAL int get_best_eci_segs(struct zint_symbol *symbol, struct zint_seg segs[], const int seg_count) {
    const int default_eci = symbol->symbology == BARCODE_GRIDMATRIX ? 29 : symbol->symbology == BARCODE_UPNQR ? 4 : 3;
    int first_eci_set = 0;
    int i;

    for (i = 0; i < seg_count; i++) {
        if (segs[i].eci == 0) {
            const int eci = get_best_eci(segs[i].source, segs[i].length);
            if (eci == default_eci) {
                if (i != 0 && segs[i - 1].eci != 0 && segs[i - 1].eci != default_eci) {
                    segs[i].eci = eci;
                    if (first_eci_set == 0) {
                        first_eci_set = eci;
                    }
                }
            } else {
                segs[i].eci = eci;
                if (first_eci_set == 0) {
                    first_eci_set = eci;
                    if (i == 0) {
                        symbol->eci = eci;
                    }
                }
            }
        }
    }

    return first_eci_set;
}

/* QRCODE Shift JIS helpers */

/* Convert UTF-8 string to Shift JIS and place in array of ints */
INTERNAL int sjis_utf8(struct zint_symbol *symbol, const unsigned char source[], int *p_length,
                unsigned int *ddata) {
    int error_number;
    unsigned int i, length;
    unsigned int *utfdata = (unsigned int *) z_alloca(sizeof(unsigned int) * (*p_length + 1));

    error_number = utf8_to_unicode(symbol, source, utfdata, p_length, 1 /*disallow_4byte*/);
    if (error_number != 0) {
        return error_number;
    }

    for (i = 0, length = *p_length; i < length; i++) {
        if (!u_sjis_int(utfdata[i], ddata + i)) {
            return errtxt(ZINT_ERROR_INVALID_DATA, symbol, 800, "Invalid character in input");
        }
    }

    return 0;
}

/* If `full_multibyte` set, copy byte input stream to array of ints, putting double-bytes that match QR Kanji mode in
 * a single entry. If `full_multibyte` not set, do a straight copy */
INTERNAL void sjis_cpy(const unsigned char source[], int *p_length, unsigned int *ddata, const int full_multibyte) {
    unsigned int i, j, length;
    unsigned char c1, c2;

    if (full_multibyte) {
        for (i = 0, j = 0, length = *p_length; i < length; i++, j++) {
            c1 = source[i];
            /* Now using stricter interpretation of standard, and excluding certain trailing bytes */
            if (((c1 >= 0x81 && c1 <= 0x9F) || (c1 >= 0xE0 && c1 <= 0xEB)) && length - i >= 2) {
                c2 = source[i + 1];
                if ((c2 >= 0x40 && c2 <= 0xFC) && c2 != 0x7F && (c1 != 0xEB || c2 <= 0xBF)) {
                    /* This may or may not be valid Shift JIS, but don't care as long as it can be encoded in
                     * QR Kanji mode */
                    ddata[j] = (c1 << 8) | c2;
                    i++;
                } else {
                    ddata[j] = c1;
                }
            } else {
                ddata[j] = c1;
            }
        }
        *p_length = j;
    } else {
        /* Straight copy */
        for (i = 0, length = *p_length; i < length; i++) {
            ddata[i] = source[i];
        }
    }
}

/* Call `sjis_cpy()` for each segment */
INTERNAL void sjis_cpy_segs(struct zint_seg segs[], const int seg_count, unsigned int *ddata,
                const int full_multibyte) {
    int i;
    unsigned int *dd = ddata;

    for (i = 0; i < seg_count; i++) {
        sjis_cpy(segs[i].source, &segs[i].length, dd, full_multibyte);
        dd += segs[i].length;
    }
}

/* Convert UTF-8 string to ECI and place in array of ints using `sjis_cpy()` */
INTERNAL int sjis_utf8_to_eci(const int eci, const unsigned char source[], int *p_length, unsigned int *ddata,
                const int full_multibyte) {

    if (is_eci_convertible(eci)) {
        int error_number;
        const int eci_length = get_eci_length(eci, source, *p_length);
        unsigned char *converted = (unsigned char *) z_alloca(eci_length + 1);

        error_number = utf8_to_eci(eci, source, converted, p_length);
        if (error_number != 0) {
            /* Note not setting `symbol->errtxt`, up to caller */
            return error_number;
        }

        sjis_cpy(converted, p_length, ddata, full_multibyte || eci == 20);
    } else {
        sjis_cpy(source, p_length, ddata, full_multibyte);
    }

    return 0;
}

/* GRIDMATRIX GB 2312 helpers */

/* Convert UTF-8 string to GB 2312 (EUC-CN) and place in array of ints */
INTERNAL int gb2312_utf8(struct zint_symbol *symbol, const unsigned char source[], int *p_length,
                unsigned int *ddata) {
    int error_number;
    unsigned int i, length;
    unsigned int *utfdata = (unsigned int *) z_alloca(sizeof(unsigned int) * (*p_length + 1));

    error_number = utf8_to_unicode(symbol, source, utfdata, p_length, 1 /*disallow_4byte*/);
    if (error_number != 0) {
        return error_number;
    }

    for (i = 0, length = *p_length; i < length; i++) {
        if (utfdata[i] < 0x80) {
            ddata[i] = utfdata[i];
        } else {
            if (!u_gb2312_int(utfdata[i], ddata + i)) {
                return errtxt(ZINT_ERROR_INVALID_DATA, symbol, 810, "Invalid character in input");
            }
        }
    }

    return 0;
}

/* If `full_multibyte` set, copy byte input stream to array of ints, putting double-bytes that match GRIDMATRIX
 * Chinese mode in a single entry. If `full_multibyte` not set, do a straight copy */
static void gb2312_cpy(const unsigned char source[], int *p_length, unsigned int *ddata,
                const int full_multibyte) {
    unsigned int i, j, length;
    unsigned char c1, c2;

    if (full_multibyte) {
        for (i = 0, j = 0, length = *p_length; i < length; i++, j++) {
            if (length - i >= 2) {
                c1 = source[i];
                c2 = source[i + 1];
                if (((c1 >= 0xA1 && c1 <= 0xA9) || (c1 >= 0xB0 && c1 <= 0xF7)) && c2 >= 0xA1 && c2 <= 0xFE) {
                    /* This may or may not be valid GB 2312 (EUC-CN), but don't care as long as it can be encoded in
                     * GRIDMATRIX Chinese mode */
                    ddata[j] = (c1 << 8) | c2;
                    i++;
                } else {
                    ddata[j] = c1;
                }
            } else {
                ddata[j] = source[i];
            }
        }
        *p_length = j;
    } else {
        /* Straight copy */
        for (i = 0, length = *p_length; i < length; i++) {
            ddata[i] = source[i];
        }
    }
}

#ifdef ZINT_TEST /* Wrapper for direct testing */
INTERNAL void gb2312_cpy_test(const unsigned char source[], int *p_length, unsigned int *ddata,
                const int full_multibyte) {
    gb2312_cpy(source, p_length, ddata, full_multibyte);
}
#endif

/* Call `gb2312_cpy()` for each segment */
INTERNAL void gb2312_cpy_segs(struct zint_seg segs[], const int seg_count, unsigned int *ddata,
                const int full_multibyte) {
    int i;
    unsigned int *dd = ddata;

    for (i = 0; i < seg_count; i++) {
        gb2312_cpy(segs[i].source, &segs[i].length, dd, full_multibyte);
        dd += segs[i].length;
    }
}

/* Convert UTF-8 string to ECI and place in array of ints using `gb2312_cpy()` */
INTERNAL int gb2312_utf8_to_eci(const int eci, const unsigned char source[], int *p_length, unsigned int *ddata,
                const int full_multibyte) {

    if (is_eci_convertible(eci)) {
        int error_number;
        const int eci_length = get_eci_length(eci, source, *p_length);
        unsigned char *converted = (unsigned char *) z_alloca(eci_length + 1);

        error_number = utf8_to_eci(eci, source, converted, p_length);
        if (error_number != 0) {
            /* Note not setting `symbol->errtxt`, up to caller */
            return error_number;
        }

        gb2312_cpy(converted, p_length, ddata, full_multibyte || eci == 29);
    } else {
        gb2312_cpy(source, p_length, ddata, full_multibyte);
    }

    return 0;
}

/* HANXIN GB 18030 helpers */

/* Convert UTF-8 string to GB 18030 and place in array of ints */
INTERNAL int gb18030_utf8(struct zint_symbol *symbol, const unsigned char source[], int *p_length,
                unsigned int *ddata) {
    int error_number, ret;
    unsigned int i, j, length;
    unsigned int *utfdata = (unsigned int *) z_alloca(sizeof(unsigned int) * (*p_length + 1));

    error_number = utf8_to_unicode(symbol, source, utfdata, p_length, 0 /*disallow_4byte*/);
    if (error_number != 0) {
        return error_number;
    }

    for (i = 0, j = 0, length = *p_length; i < length; i++, j++) {
        if (utfdata[i] < 0x80) {
            ddata[j] = utfdata[i];
        } else {
            ret = u_gb18030_int(utfdata[i], ddata + j, ddata + j + 1);
            if (ret == 0) { /* Should never happen, as GB 18030 is a UTF i.e. maps all Unicode codepoints */
                return errtxt(ZINT_ERROR_INVALID_DATA, symbol, 820, "Invalid character in input"); /* Not reached */
            }
            if (ret == 4) {
                j++;
            }
        }
    }

    *p_length = j;

    return 0;
}

/* If `full_multibyte` set, copy byte input stream to array of ints, putting double-bytes that match HANXIN
 * Chinese mode in single entry, and quad-bytes in 2 entries. If `full_multibyte` not set, do a straight copy */
static void gb18030_cpy(const unsigned char source[], int *p_length, unsigned int *ddata,
                const int full_multibyte) {
    unsigned int i, j, length;
    int done;
    unsigned char c1, c2, c3, c4;

    if (full_multibyte) {
        for (i = 0, j = 0, length = *p_length; i < length; i++, j++) {
            done = 0;
            c1 = source[i];
            if (length - i >= 2) {
                if (c1 >= 0x81 && c1 <= 0xFE) {
                    c2 = source[i + 1];
                    if ((c2 >= 0x40 && c2 <= 0x7E) || (c2 >= 0x80 && c2 <= 0xFE)) {
                        ddata[j] = (c1 << 8) | c2;
                        i++;
                        done = 1;
                    } else if (length - i >= 4 && (c2 >= 0x30 && c2 <= 0x39)) {
                        c3 = source[i + 2];
                        c4 = source[i + 3];
                        if ((c3 >= 0x81 && c3 <= 0xFE) && (c4 >= 0x30 && c4 <= 0x39)) {
                            ddata[j++] = (c1 << 8) | c2;
                            ddata[j] = (c3 << 8) | c4;
                            i += 3;
                            done = 1;
                        }
                    }
                }
            }
            if (!done) {
                ddata[j] = c1;
            }
        }
        *p_length = j;
    } else {
        /* Straight copy */
        for (i = 0, length = *p_length; i < length; i++) {
            ddata[i] = source[i];
        }
    }
}

#ifdef ZINT_TEST /* Wrapper for direct testing */
INTERNAL void gb18030_cpy_test(const unsigned char source[], int *p_length, unsigned int *ddata,
                const int full_multibyte) {
    gb18030_cpy(source, p_length, ddata, full_multibyte);
}
#endif

/* Call `gb18030_cpy()` for each segment */
INTERNAL void gb18030_cpy_segs(struct zint_seg segs[], const int seg_count, unsigned int *ddata,
                const int full_multibyte) {
    int i;
    unsigned int *dd = ddata;

    for (i = 0; i < seg_count; i++) {
        gb18030_cpy(segs[i].source, &segs[i].length, dd, full_multibyte);
        dd += segs[i].length;
    }
}

/* Convert UTF-8 string to ECI and place in array of ints using `gb18030_cpy()` */
INTERNAL int gb18030_utf8_to_eci(const int eci, const unsigned char source[], int *p_length, unsigned int *ddata,
                const int full_multibyte) {

    if (is_eci_convertible(eci)) {
        int error_number;
        const int eci_length = get_eci_length(eci, source, *p_length);
        unsigned char *converted = (unsigned char *) z_alloca(eci_length + 1);

        error_number = utf8_to_eci(eci, source, converted, p_length);
        if (error_number != 0) {
            /* Note not setting `symbol->errtxt`, up to caller */
            return error_number;
        }

        /* GB 18030 (ECI 32) superset of GB 2312 (ECI 29) and GBK (ECI 31) */
        gb18030_cpy(converted, p_length, ddata, full_multibyte || eci == 32 || eci == 29 || eci == 31);
    } else {
        gb18030_cpy(source, p_length, ddata, full_multibyte);
    }

    return 0;
}

/* vim: set ts=4 sw=4 et : */
