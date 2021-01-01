/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"
#include "utils/ByteOrderDecoder.h"

u16 UInt16BE(const u8* d) {
    return d[1] | (d[0] << 8);
}

u16 UInt16LE(const u8* d) {
    return d[0] | (d[1] << 8);
}

u32 UInt32BE(const u8* d) {
    return d[3] | (d[2] << 8) | (d[1] << 16) | (d[0] << 24);
}

u32 UInt32LE(const u8* d) {
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

u16 ByteOrderDecoder::UInt16() {
    u16 v;
    if (left < sizeof(v)) {
        ok = false;
    }
    if (!ok) {
        return 0;
    }

    if (LittleEndian == byteOrder) {
        v = UInt16LE(curr);
    } else {
        v = UInt16BE(curr);
    }

    left -= sizeof(v);
    curr += sizeof(v);
    return v;
}

u32 ByteOrderDecoder::UInt32() {
    u32 v;
    if (left < sizeof(v)) {
        ok = false;
    }
    if (!ok) {
        return 0;
    }

    if (LittleEndian == byteOrder) {
        v = UInt32LE(curr);
    } else {
        v = UInt32BE(curr);
    }

    left -= sizeof(v);
    curr += sizeof(v);
    return v;
}

u64 ByteOrderDecoder::UInt64() {
    u64 v;
    if (left < sizeof(v)) {
        ok = false;
    }
    if (!ok) {
        return 0;
    }

    v = UInt32();
    u64 v2 = UInt32();
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
