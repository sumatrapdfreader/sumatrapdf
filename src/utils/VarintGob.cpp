/* Copyright 2014 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "BaseUtil.h"
#include "VarintGob.h"
#include "BitManip.h"

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
int UVarintGobDecode(const uint8_t *d, int dLen, uint64_t *resOut)
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

int VarintGobDecode(const uint8_t *d, int dLen, int64_t *resOut)
{
    uint64_t val;
    int n = UVarintGobDecode(d, dLen, &val);
    if (n == 0)
        return 0;

    bool negative = bit::IsSet(val, 0);
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
int UVarintGobEncode(uint64_t val, uint8_t *d, int dLen)
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
int VarintGobEncode(int64_t val, uint8_t *d, int dLen)
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
    return UVarintGobEncode(uVal, d, dLen);
}
