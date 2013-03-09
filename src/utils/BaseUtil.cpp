/* Copyright 2013 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "BaseUtil.h"

size_t RoundToPowerOf2(size_t size)
{
    size_t n = 1;
    while (n < size) {
        n *= 2;
        if (0 == n)
            return MAX_SIZE_T;
    }
    return n;
}

/* MurmurHash2, by Austin Appleby
 * Note - This code makes a few assumptions about how your machine behaves -
 * 1. We can read a 4-byte value from any address without crashing
 *
 * And it has a few limitations -
 *
 * 1. It will not work incrementally.
 * 2. It will not produce the same results on little-endian and big-endian
 *    machines.
 */
static uint32_t hash_function_seed = 5381;

uint32_t MurmurHash2(const void *key, size_t len)
{
    /* 'm' and 'r' are mixing constants generated offline.
     They're not really 'magic', they just happen to work well.  */
    const uint32_t m = 0x5bd1e995;
    const int r = 24;

    /* Initialize the hash to a 'random' value */
    uint32_t h = hash_function_seed ^ len;

    /* Mix 4 bytes at a time into the hash */
    const uint8_t *data = (const uint8_t *)key;

    while (len >= 4) {
        uint32_t k = *(uint32_t *)data;

        k *= m;
        k ^= k >> r;
        k *= m;

        h *= m;
        h ^= k;

        data += 4;
        len -= 4;
    }

    /* Handle the last few bytes of the input array  */
    switch (len) {
    case 3: h ^= data[2] << 16;
    case 2: h ^= data[1] << 8;
    case 1: h ^= data[0]; h *= m;
    }

    /* Do a few final mixes of the hash to ensure the last few
     * bytes are well-incorporated. */
    h ^= h >> 13;
    h *= m;
    h ^= h >> 15;

    return h;
}

// from varint.c in sqlite4 (http://www.sqlite.org/src4/artifact/4cf09171ba7a3bbb2a5ede38b0e041a9a9aa3490)
// Note: it's probably over-optimized for speed for our purposes
/*
** Decode the varint in the first n bytes z[].  Write the integer value
** into *pResult and return the number of bytes in the varint.
**
** If the decode fails because there are not enough bytes in z[] then
** return 0;
*/
int GetVarint64(const unsigned char *z, int n, uint64_t *pResult)
{
    unsigned int x;
    if (n < 1)
        return 0;
    if (z[0] <= 240) {
        *pResult = z[0];
        return 1;
    }
    if (z[0] <= 248 ) {
        if (n<2)
            return 0;
        *pResult = (z[0]-241)*256 + z[1] + 240;
        return 2;
    }
    if (n < z[0]-246)
        return 0;

    if (z[0]==249) {
        *pResult = 2288 + 256*z[1] + z[2];
        return 3;
    }

    if (z[0] == 250) {
        *pResult = (z[1]<<16) + (z[2]<<8) + z[3];
        return 4;
    }
    x = (z[1]<<24) + (z[2]<<16) + (z[3]<<8) + z[4];
    if (z[0] == 251) {
        *pResult = x;
        return 5;
    }
    if (z[0]==252) {
        *pResult = (((uint64_t)x)<<8) + z[5];
        return 6;
    }
    if (z[0] == 253) {
        *pResult = (((uint64_t)x)<<16) + (z[5]<<8) + z[6];
        return 7;
    }
    if (z[0] == 254) {
        *pResult = (((uint64_t)x)<<24) + (z[5]<<16) + (z[6]<<8) + z[7];
        return 8;
    }
    *pResult = (((uint64_t)x)<<32) +
                (0xffffffff & ((z[5]<<24) + (z[6]<<16) + (z[7]<<8) + z[8]));
    return 9;
}

/*
** Write a 32-bit unsigned integer as 4 big-endian bytes.
*/
static void varintWrite32(unsigned char *z, unsigned int y)
{
  z[0] = (unsigned char)(y>>24);
  z[1] = (unsigned char)(y>>16);
  z[2] = (unsigned char)(y>>8);
  z[3] = (unsigned char)(y);
}

/*
** Write a varint into z[].  The buffer z[] must be at least 9 characters
** long to accommodate the largest possible varint.  Return the number of
** bytes of z[] used.
*/
int PutVarint64(unsigned char *z, uint64_t x)
{
    unsigned int w, y;
    if (x <= 240) {
        z[0] = (unsigned char)x;
        return 1;
    }
    if (x <= 2287) {
        y = (unsigned int)(x - 240);
        z[0] = (unsigned char)(y/256 + 241);
        z[1] = (unsigned char)(y%256);
        return 2;
    }
    if (x <= 67823) {
        y = (unsigned int)(x - 2288);
        z[0] = 249;
        z[1] = (unsigned char)(y/256);
        z[2] = (unsigned char)(y%256);
        return 3;
    }

    y = (unsigned int)x;
    w = (unsigned int)(x>>32);

    if (w==0) {
        if (y <= 16777215) {
            z[0] = 250;
            z[1] = (unsigned char)(y>>16);
            z[2] = (unsigned char)(y>>8);
            z[3] = (unsigned char)(y);
            return 4;
        }
        z[0] = 251;
        varintWrite32(z+1, y);
        return 5;
    }
    if (w <= 255) {
        z[0] = 252;
        z[1] = (unsigned char)w;
        varintWrite32(z+2, y);
        return 6;
    }
    if (w <= 32767) {
        z[0] = 253;
        z[1] = (unsigned char)(w>>8);
        z[2] = (unsigned char)w;
        varintWrite32(z+3, y);
        return 7;
    }
    if (w <= 16777215) {
        z[0] = 254;
        z[1] = (unsigned char)(w>>16);
        z[2] = (unsigned char)(w>>8);
        z[3] = (unsigned char)w;
        varintWrite32(z+4, y);
        return 8;
    }
    z[0] = 255;
    varintWrite32(z+1, w);
    varintWrite32(z+5, y);
    return 9;
}
