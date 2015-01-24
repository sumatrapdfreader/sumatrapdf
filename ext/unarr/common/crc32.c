/* Copyright 2015 the unarr project authors (see AUTHORS file).
   License: LGPLv3 */

#include "unarr-imp.h"

#ifndef HAVE_ZLIB

/* code adapted from https://gnunet.org/svn/gnunet/src/util/crypto_crc.c (public domain) */

static bool crc_table_ready = false;
static uint32_t crc_table[256];

uint32_t ar_crc32(uint32_t crc32, const unsigned char *data, size_t data_len)
{
    if (!crc_table_ready) {
        uint32_t i, j;
        uint32_t h = 1;
        crc_table[0] = 0;
        for (i = 128; i; i >>= 1) {
            h = (h >> 1) ^ ((h & 1) ? 0xEDB88320 : 0);
            for (j = 0; j < 256; j += 2 * i) {
                crc_table[i + j] = crc_table[j] ^ h;
            }
        }
        crc_table_ready = true;
    }

    crc32 = crc32 ^ 0xFFFFFFFF;
    while (data_len-- > 0) {
        crc32 = (crc32 >> 8) ^ crc_table[(crc32 ^ *data++) & 0xFF];
    }
    return crc32 ^ 0xFFFFFFFF;
}

#else

#include <zlib.h>

uint32_t ar_crc32(uint32_t crc, const unsigned char *data, size_t data_len)
{
#if SIZE_MAX > UINT32_MAX
    while (data_len > UINT32_MAX) {
        crc = crc32(crc, data, UINT32_MAX);
        data += UINT32_MAX;
        data_len -= UINT32_MAX;
    }
#endif
    return crc32(crc, data, (uint32_t)data_len);
}

#endif
