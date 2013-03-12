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

// Varint decoding/encoding is the same as the scheme used by GOB in Go.
// For unsinged 64-bit integer
// - if the value is <= 0x7f, it's written as a single byte
// - other values are written as:
//  - count of bytes, negated
//  - bytes of the number follow
// Signed 64-bit integer is turned into unsigned by:
//  - negative value has first bit set to 1 and other bits
//    are negated and shifted by one to the left
//  - positive value has first bit set to 0 and other bits
//    shifted by one to the left
// Smmaller values are cast to uint64 or int64, according to their signed-ness
//
// The downside of this scheme is that it's impossible to tell
// if the value is signed or unsigned from the value itself.
// The client needs to know the sign-edness to properly interpret the data

// decodes unsigned 64-bit int from data d of dLen size
// returns 0 on error
// returns the number of consumed bytes from d on success
int GobUVarintDecode(const uint8_t *d, int dLen, uint64_t *resOut)
{
    if (dLen < 1)
        return 0;
    uint8_t b = *d++;
    if (b <= 0x7f) {
        *resOut = b;
        return 1;
    }
    --dLen;
    if (dLen < 1)
        return 0;
    char numLenEncoded = (char)b;
    int numLen = -numLenEncoded;
    CrashIf(numLen < 1);
    CrashIf(numLen > 8);
    if (numLen > dLen)
        return 0;
    uint64_t res = 0;
    for (int i=0; i < numLen; i++) {
        b = *d++;
        res = (res << 8) | b;
    }
    *resOut = res;
    return 1 + numLen;
}

int GobVarintDecode(const uint8_t *d, int dLen, int64_t *resOut)
{
    uint64_t val;
    int n = GobUVarintDecode(d, dLen, &val);
    if (n == 0)
        return 0;

    // TODO: use bit::IsSet() ? Would require #include "BitManip.h" in BaseUtil.h
    bool negative = ((val & 1) != 0);
    val = val >> 1;
    int64_t res = (int64_t)val;
    if (negative)
        res = ~res;
    *resOut = res;
    return n;
}

// max 8 bytes plus 1 to encode size
static const int MinGobEncodeBufferSize = 9;
static const int UInt64SizeOf = 8;

// encodes unsigned integer val into a buffer d of dLen size (must be at least 9 bytes)
// returns number of bytes used
int GobUVarintEncode(uint64_t val, uint8_t *d, int dLen)
{
    uint8_t b;
    CrashIf(dLen < MinGobEncodeBufferSize);
    if (val <= 0x7f) {
        *d = (int8_t)val;
        return 1;
    }

    uint8_t buf[UInt64SizeOf];
    uint8_t *bufPtr = buf + UInt64SizeOf;
    int len8Minus = UInt64SizeOf;
    while (val > 0) {
        b = (uint8_t)(val & 0xff);
        val = val >> 8;
        --bufPtr;
        *bufPtr = b;
        --len8Minus;
    }
    CrashIf(len8Minus < 1);
    int realLen = 8-len8Minus;
    CrashIf(realLen > 8);
    int lenEncoded = (len8Minus - UInt64SizeOf);
    uint8_t lenEncodedU = (uint8_t)lenEncoded;
    *d++ = lenEncodedU;
    memcpy(d, bufPtr, realLen);
    return (int)realLen+1; // +1 for the length byte
}

// encodes signed integer val into a buffer d of dLen size (must be at least 9 bytes)
// returns number of bytes used
int GobVarintEncode(int64_t val, uint8_t *d, int dLen)
{
    uint64_t uVal;
    if (val < 0) {
        val = ~val;
        uVal = (uint64_t)val;
        uVal = (uVal << 1) | 1;
    } else {
        uVal = (uint64_t)val;
        uVal = uVal << 1;
    }
    return GobUVarintEncode(uVal, d, dLen);
}
