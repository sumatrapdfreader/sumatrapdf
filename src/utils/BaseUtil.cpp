/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "BaseUtil.h"

// force no inlining because we want to see it on the callstack
#pragma warning(push)
#pragma warning(disable: 6011) // silence /analyze: de-referencing a NULL pointer
// Note: trying doing this via RaiseException(0x40000015, EXCEPTION_NONCONTINUABLE, 0, 0);
// but it seemed to confuse callstack walking
__declspec(noinline) void CrashMe()
{
    char *p = NULL;
    *p = 0;
}
#pragma warning(pop)

size_t roundToPowerOf2(size_t size)
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

uint32_t murmur_hash2(const void *key, size_t len)
{
    /* 'm' and 'r' are mixing constants generated offline.
     They're not really 'magic', they just happen to work well.  */
    uint32_t seed = hash_function_seed;
    const uint32_t m = 0x5bd1e995;
    const int r = 24;

    /* Initialize the hash to a 'random' value */
    uint32_t h = seed ^ len;

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
    };

    /* Do a few final mixes of the hash to ensure the last few
     * bytes are well-incorporated. */
    h ^= h >> 13;
    h *= m;
    h ^= h >> 15;

    return h;
}
