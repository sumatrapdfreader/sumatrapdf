/*
 * Copyright © 2018, VideoLAN and dav1d authors
 * Copyright © 2018, Two Orioles, LLC
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "common/intops.h"

#include "output/muxer.h"

static const uint32_t k[64] = {
    0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee,
    0xf57c0faf, 0x4787c62a, 0xa8304613, 0xfd469501,
    0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be,
    0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821,
    0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa,
    0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8,
    0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed,
    0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a,
    0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c,
    0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70,
    0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x04881d05,
    0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,
    0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039,
    0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1,
    0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1,
    0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391,
};

#if ENDIANNESS_BIG
#define NE2LE_32(x) (((x & 0x00ff) << 24) |\
                     ((x & 0xff00) <<  8) |\
                     ((x >>  8) & 0xff00) |\
                     ((x >> 24) & 0x00ff))

#define NE2LE_64(x) (((x & 0x000000ff) << 56) |\
                     ((x & 0x0000ff00) << 40) |\
                     ((x & 0x00ff0000) << 24) |\
                     ((x & 0xff000000) <<  8) |\
                     ((x >>  8) & 0xff000000) |\
                     ((x >> 24) & 0x00ff0000) |\
                     ((x >> 40) & 0x0000ff00) |\
                     ((x >> 56) & 0x000000ff))

#else
#define NE2LE_32(x) (x)
#define NE2LE_64(x) (x)
#endif

typedef struct MuxerPriv {
    uint32_t abcd[4];
    union {
        uint8_t data[64];
        uint32_t data32[16];
    };
    uint64_t len;
    FILE *f;
#if ENDIANNESS_BIG
    uint8_t *bswap;
    int bswap_w;
#endif
} MD5Context;

static int md5_open(MD5Context *const md5, const char *const file,
                    const Dav1dPictureParameters *const p,
                    const unsigned fps[2])
{
    if (!strcmp(file, "-")) {
        md5->f = stdout;
    } else if (!(md5->f = fopen(file, "wb"))) {
        fprintf(stderr, "Failed to open %s: %s\n", file, strerror(errno));
        return -1;
    }

#if ENDIANNESS_BIG
    md5->bswap = NULL;
    md5->bswap_w = 0;
#endif

    md5->abcd[0] = 0x67452301;
    md5->abcd[1] = 0xefcdab89;
    md5->abcd[2] = 0x98badcfe;
    md5->abcd[3] = 0x10325476;
    md5->len = 0;

    return 0;
}

static inline uint32_t leftrotate(const uint32_t x, const int c) {
    return (x << c) | (x >> (32 - c));
}

#define F(i) do { \
    a = b + leftrotate(a + ((b & c) | (~b & d)) + k[i + 0] + NE2LE_32(data[i + 0]),  7); \
    d = a + leftrotate(d + ((a & b) | (~a & c)) + k[i + 1] + NE2LE_32(data[i + 1]), 12); \
    c = d + leftrotate(c + ((d & a) | (~d & b)) + k[i + 2] + NE2LE_32(data[i + 2]), 17); \
    b = c + leftrotate(b + ((c & d) | (~c & a)) + k[i + 3] + NE2LE_32(data[i + 3]), 22); \
} while (0)

#define G(i) do { \
    a = b + leftrotate(a + ((d & b) | (~d & c)) + k[i + 0] + NE2LE_32(data[(i +  1) & 15]),  5); \
    d = a + leftrotate(d + ((c & a) | (~c & b)) + k[i + 1] + NE2LE_32(data[(i +  6) & 15]),  9); \
    c = d + leftrotate(c + ((b & d) | (~b & a)) + k[i + 2] + NE2LE_32(data[(i + 11) & 15]), 14); \
    b = c + leftrotate(b + ((a & c) | (~a & d)) + k[i + 3] + NE2LE_32(data[(i +  0) & 15]), 20); \
} while (0)

#define H(i) do { \
    a = b + leftrotate(a + (b ^ c ^ d) + k[i + 0] + NE2LE_32(data[( 5 - i) & 15]),  4); \
    d = a + leftrotate(d + (a ^ b ^ c) + k[i + 1] + NE2LE_32(data[( 8 - i) & 15]), 11); \
    c = d + leftrotate(c + (d ^ a ^ b) + k[i + 2] + NE2LE_32(data[(11 - i) & 15]), 16); \
    b = c + leftrotate(b + (c ^ d ^ a) + k[i + 3] + NE2LE_32(data[(14 - i) & 15]), 23); \
} while (0)

#define I(i) do { \
    a = b + leftrotate(a + (c ^ (b | ~d)) + k[i + 0] + NE2LE_32(data[( 0 - i) & 15]),  6); \
    d = a + leftrotate(d + (b ^ (a | ~c)) + k[i + 1] + NE2LE_32(data[( 7 - i) & 15]), 10); \
    c = d + leftrotate(c + (a ^ (d | ~b)) + k[i + 2] + NE2LE_32(data[(14 - i) & 15]), 15); \
    b = c + leftrotate(b + (d ^ (c | ~a)) + k[i + 3] + NE2LE_32(data[( 5 - i) & 15]), 21); \
} while (0)

static void md5_body(MD5Context *const md5, const uint32_t *const data) {
    uint32_t a = md5->abcd[0];
    uint32_t b = md5->abcd[1];
    uint32_t c = md5->abcd[2];
    uint32_t d = md5->abcd[3];

    F( 0); F( 4); F( 8); F(12);
    G(16); G(20); G(24); G(28);
    H(32); H(36); H(40); H(44);
    I(48); I(52); I(56); I(60);

    md5->abcd[0] += a;
    md5->abcd[1] += b;
    md5->abcd[2] += c;
    md5->abcd[3] += d;
}

static void md5_update(MD5Context *const md5, const uint8_t *data, unsigned len) {
    if (!len) return;

    if (md5->len & 63) {
        const unsigned tmp = umin(len, 64 - (md5->len & 63));

        memcpy(&md5->data[md5->len & 63], data, tmp);
        len -= tmp;
        data += tmp;
        md5->len += tmp;
        if (!(md5->len & 63))
            md5_body(md5, md5->data32);
    }

    while (len >= 64) {
        memcpy(md5->data, data, 64);
        md5_body(md5, md5->data32);
        md5->len += 64;
        data += 64;
        len -= 64;
    }

    if (len) {
        memcpy(md5->data, data, len);
        md5->len += len;
    }
}

static int md5_write(MD5Context *const md5, Dav1dPicture *const p) {
    const int hbd = p->p.bpc > 8;
    const int w = p->p.w, h = p->p.h;
    uint8_t *yptr = p->data[0];

#if ENDIANNESS_BIG
    if (hbd && (!md5->bswap || md5->bswap_w < p->p.w)) {
        free(md5->bswap);
        md5->bswap_w = 0;
        md5->bswap = malloc(p->p.w << 1);
        if (!md5->bswap) return -1;
        md5->bswap_w = p->p.w;
    }
#endif

    for (int y = 0; y < h; y++) {
#if ENDIANNESS_BIG
        if (hbd) {
            for (int x = 0; x < w; x++) {
                md5->bswap[2 * x + 1] = yptr[2 * x];
                md5->bswap[2 * x]     = yptr[2 * x + 1];
            }
            md5_update(md5, md5->bswap, w << hbd);
        } else
#endif
        md5_update(md5, yptr, w << hbd);
        yptr += p->stride[0];
    }

    if (p->p.layout != DAV1D_PIXEL_LAYOUT_I400) {
        const int ss_ver = p->p.layout == DAV1D_PIXEL_LAYOUT_I420;
        const int ss_hor = p->p.layout != DAV1D_PIXEL_LAYOUT_I444;
        const int cw = (w + ss_hor) >> ss_hor;
        const int ch = (h + ss_ver) >> ss_ver;
        for (int pl = 1; pl <= 2; pl++) {
            uint8_t *uvptr = p->data[pl];

            for (int y = 0; y < ch; y++) {
#if ENDIANNESS_BIG
                if (hbd) {
                    for (int x = 0; x < cw; x++){
                        md5->bswap[2 * x + 1] = uvptr[2 * x];
                        md5->bswap[2 * x]     = uvptr[2 * x + 1];
                    }
                    md5_update(md5, md5->bswap, cw << hbd);
                } else
#endif
                md5_update(md5, uvptr, cw << hbd);
                uvptr += p->stride[1];
            }
        }
    }

    dav1d_picture_unref(p);

    return 0;
}

static void md5_finish(MD5Context *const md5) {
    static const uint8_t bit[2] = { 0x80, 0x00 };
    const uint64_t len = NE2LE_64(md5->len << 3);

    md5_update(md5, &bit[0], 1);
    while ((md5->len & 63) != 56)
        md5_update(md5, &bit[1], 1);
    md5_update(md5, (const uint8_t *) &len, 8);
}

static void md5_close(MD5Context *const md5) {
    md5_finish(md5);
    for (int i = 0; i < 4; i++)
        fprintf(md5->f, "%2.2x%2.2x%2.2x%2.2x",
                md5->abcd[i] & 0xff,
                (md5->abcd[i] >> 8) & 0xff,
                (md5->abcd[i] >> 16) & 0xff,
                md5->abcd[i] >> 24);
    fprintf(md5->f, "\n");

#if ENDIANNESS_BIG
    free(md5->bswap);
    md5->bswap_w = 0;
#endif

    if (md5->f != stdout)
        fclose(md5->f);
}

static int md5_verify(MD5Context *const md5, const char *md5_str) {
    md5_finish(md5);

    if (strlen(md5_str) < 32)
        return -1;

    uint32_t abcd[4] = { 0 };
    char t[3] = { 0 };
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 32; j += 8) {
            char *ignore;
            memcpy(t, md5_str, 2);
            md5_str += 2;
            abcd[i] |= (uint32_t) strtoul(t, &ignore, 16) << j;
        }
    }

#if ENDIANNESS_BIG
    free(md5->bswap);
    md5->bswap_w = 0;
#endif

    return !!memcmp(abcd, md5->abcd, sizeof(abcd));
}

const Muxer md5_muxer = {
    .priv_data_size = sizeof(MD5Context),
    .name = "md5",
    .extension = "md5",
    .write_header = md5_open,
    .write_picture = md5_write,
    .write_trailer = md5_close,
    .verify = md5_verify,
};
