/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"
#include "utils/ByteWriter.h"

ByteWriter::ByteWriter(u8* dst, size_t bytesLeft, bool isLE) : dst(dst), isLE(isLE) {
    end = dst + bytesLeft;
}

ByteWriter::ByteWriter(const ByteWriter& o) {
    this->dst = o.dst;
    this->end = o.end;
    this->isLE = o.isLE;
}

size_t ByteWriter::Left() const {
    CrashIf(dst > end);
    return end - dst;
}

bool ByteWriter::Write8(u8 b) {
    if (Left() < 1) {
        return false;
    }
    *dst++ = b;
    return true;
}

bool ByteWriter::Write8x2(u8 b1, u8 b2) {
    if (Left() < 2) {
        return false;
    }
    *dst++ = b1;
    *dst++ = b2;
    return true;
}

bool ByteWriter::Write16(u16 val) {
    u8 b1 = val & 0xFF;
    u8 b2 = (val >> 8) & 0xFF;
    if (isLE) {
        return Write8x2(b1, b2);
    }
    return Write8x2(b2, b1);
}

bool ByteWriter::Write32(u32 val) {
    u8 b1 = val & 0xFF;
    u8 b2 = (val >> 8) & 0xFF;
    u8 b3 = (val >> 16) & 0xFF;
    u8 b4 = (val >> 24) & 0xFF;
    if (isLE) {
        return Write8x2(b1, b2) && Write8x2(b3, b4);
    }
    return Write8x2(b4, b3) && Write8x2(b2, b1);
}

bool ByteWriter::Write64(u64 val) {
    u32 v1 = val & 0xFFFFFFFF;
    u32 v2 = (val >> 32) & 0xFFFFFFFF;
    if (isLE) {
        return Write32(v1) && Write32(v2);
    }
    return Write32(v2) && Write32(v1);
}

ByteWriter MakeByteWriterLE(u8* dst, size_t len) {
    return ByteWriter((u8*)dst, len, true);
}

ByteWriter MakeByteWriterLE(char* dst, size_t len) {
    return ByteWriter((u8*)dst, len, true);
}

ByteWriter MakeByteWriterBE(u8* dst, size_t len) {
    return ByteWriter((u8*)dst, len, false);
}

ByteWriter MakeByteWriterBE(char* dst, size_t len) {
    return ByteWriter((u8*)dst, len, false);
}
