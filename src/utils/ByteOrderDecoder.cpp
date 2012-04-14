/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "BaseUtil.h"
#include "ByteOrderDecoder.h"

uint16 UInt16BE(const uint8* d)
{
    return d[1] | (d[0] << 8);
}

uint16 UInt16LE(const uint8* d)
{
    return d[0] | (d[1] << 8);
}

uint32 UInt32BE(const uint8* d)
{
    return d[3] | (d[2] << 8) | (d[1] << 16) | (d[0] << 24);
}

uint32 UInt32LE(const uint8* d)
{
    return d[0] | (d[1] << 8) | (d[2] << 16) | (d[3] << 24);
}

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
        v = UInt16LE(curr);
    else
        v = UInt16BE(curr);
    left -= sizeof(v);
    curr += sizeof(v);
    return v;
}

uint32 ByteOrderDecoder::UInt32()
{
    uint32 v;
    CrashIf(left < sizeof(v));

    if (LittleEndian == byteOrder)
        v = UInt32LE(curr);
    else
        v = UInt32BE(curr);
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

