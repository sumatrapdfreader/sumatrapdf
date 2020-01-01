/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"
#include "utils/ByteOrderDecoder.h"

uint16_t UInt16BE(const u8* d) {
    return d[1] | (d[0] << 8);
}

uint16_t UInt16LE(const u8* d) {
    return d[0] | (d[1] << 8);
}

uint32_t UInt32BE(const u8* d) {
    return d[3] | (d[2] << 8) | (d[1] << 16) | (d[0] << 24);
}

uint32_t UInt32LE(const u8* d) {
    return d[0] | (d[1] << 8) | (d[2] << 16) | (d[3] << 24);
}

ByteOrderDecoder::ByteOrderDecoder(const char* d, size_t len, ByteOrder order)
    : ok(true), byteOrder(order), data((const u8*)d), curr(data), left(len) {
}

ByteOrderDecoder::ByteOrderDecoder(const u8* d, size_t len, ByteOrder order)
    : ok(true), byteOrder(order), data(d), curr(data), left(len) {
}

u8 ByteOrderDecoder::UInt8() {
    if (left < 1) {
        ok = false;
    }
    if (!ok) {
        return false;
    }

    left--;
    return *curr++;
}

uint16_t ByteOrderDecoder::UInt16() {
    uint16_t v;
    if (left < sizeof(v)) {
        ok = false;
    }
    if (!ok) {
        return 0;
    }

    if (LittleEndian == byteOrder)
        v = UInt16LE(curr);
    else
        v = UInt16BE(curr);

    left -= sizeof(v);
    curr += sizeof(v);
    return v;
}

uint32_t ByteOrderDecoder::UInt32() {
    uint32_t v;
    if (left < sizeof(v)) {
        ok = false;
    }
    if (!ok) {
        return 0;
    }

    if (LittleEndian == byteOrder)
        v = UInt32LE(curr);
    else
        v = UInt32BE(curr);

    left -= sizeof(v);
    curr += sizeof(v);
    return v;
}

uint64_t ByteOrderDecoder::UInt64() {
    uint64_t v;
    if (left < sizeof(v)) {
        ok = false;
    }
    if (!ok) {
        return 0;
    }

    v = UInt32();
    uint64_t v2 = UInt32();
    if (LittleEndian == byteOrder) {
        return v | (v2 << 32);
    }
    return (v << 32) | v2;
}

void ByteOrderDecoder::Bytes(char* dst, size_t len) {
    if (left < len) {
        ok = false;
    }
    if (!ok) {
        return;
    }

    memcpy(dst, curr, len);
    left -= len;
    curr += len;
}

void ByteOrderDecoder::Skip(size_t len) {
    if (left < len) {
        ok = false;
    }
    if (!ok) {
        return;
    }

    left -= len;
    curr += len;
}

void ByteOrderDecoder::Unskip(size_t len) {
    if (curr < data + len) {
        ok = false;
    }
    if (!ok) {
        return;
    }
    left += len;
    curr -= len;
}
