/* Copyright 2015 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "BaseUtil.h"
#include "ByteOrderDecoder.h"

uint16_t UInt16BE(const uint8_t* d)
{
    return d[1] | (d[0] << 8);
}

uint16_t UInt16LE(const uint8_t* d)
{
    return d[0] | (d[1] << 8);
}

uint32_t UInt32BE(const uint8_t* d)
{
    return d[3] | (d[2] << 8) | (d[1] << 16) | (d[0] << 24);
}

uint32_t UInt32LE(const uint8_t* d)
{
    return d[0] | (d[1] << 8) | (d[2] << 16) | (d[3] << 24);
}

ByteOrderDecoder::ByteOrderDecoder(const char *d, size_t len, ByteOrder order)
    : data((const uint8_t *)d), left(len), byteOrder(order), curr(data) { }

ByteOrderDecoder::ByteOrderDecoder(const uint8_t *d, size_t len, ByteOrder order)
    : data(d), left(len), byteOrder(order), curr(data) { }

uint8_t ByteOrderDecoder::UInt8()
{
    CrashIf(left < 1);

    left--;
    return *curr++;
}

uint16_t ByteOrderDecoder::UInt16()
{
    uint16_t v;
    CrashIf(left < sizeof(v));

    if (LittleEndian == byteOrder)
        v = UInt16LE(curr);
    else
        v = UInt16BE(curr);
    left -= sizeof(v);
    curr += sizeof(v);
    return v;
}

uint32_t ByteOrderDecoder::UInt32()
{
    uint32_t v;
    CrashIf(left < sizeof(v));

    if (LittleEndian == byteOrder)
        v = UInt32LE(curr);
    else
        v = UInt32BE(curr);
    left -= sizeof(v);
    curr += sizeof(v);
    return v;
}

uint64_t ByteOrderDecoder::UInt64()
{
    uint64_t first = UInt32();
    if (LittleEndian == byteOrder)
        return first | ((uint64_t)UInt32() << 32);
    return (first << 32) | UInt32();
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

void ByteOrderDecoder::Unskip(size_t len)
{
    CrashIf(curr < data + len);
    left += len;
    curr -= len;
}
