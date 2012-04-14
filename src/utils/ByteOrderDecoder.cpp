/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "BaseUtil.h"
#include "ByteOrderDecoder.h"

ByteOrderDecoder::ByteOrderDecoder(const char *d, size_t len, ByteOrder order)
    : left(len), byteOrder(order)
{
    data = (const uint8*)d;
    curr = data;
}

ByteOrderDecoder::ByteOrderDecoder(const uint8 *d, size_t len, ByteOrder order)
    : left(len), byteOrder(order)
{
    data = d;
    curr = data;
}

uint16 ByteOrderDecoder::UInt16()
{
    uint16 v;
    CrashIf(left < sizeof(v));

    if (LittleEndian == byteOrder)
        v = curr[0] | (curr[1] << 8);
    else
        v = curr[1] | (curr[0] << 8);
    left -= sizeof(v);
    curr += sizeof(v);
    return v;
}

uint32 ByteOrderDecoder::UInt32()
{
    uint32 v;
    CrashIf(left < sizeof(v));

    if (LittleEndian == byteOrder)
        v = curr[0] | (curr[1] << 8) | (curr[2] << 16) | (curr[3] << 24);
    else
        v = curr[3] | (curr[2] << 8) | (curr[1] << 16) | (curr[0] << 24);
    left -= sizeof(v);
    curr += sizeof(v);
    return v;
}

void ByteOrderDecoder::Bytes(char *dest, size_t len)
{
    CrashIf(left < len);
    memcpy(dest, curr, len);
    left -= len;
    curr += len;
}

void ByteOrderDecoder::Skip(size_t len)
{
    CrashIf(left < len);
    left -= len;
    curr += len;
}

