/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"
#include "utils/ByteWriter.h"

ByteWriter::ByteWriter(size_t sizeHint) {
    d.cap = (u32)sizeHint;
}

ByteWriter::ByteWriter(const ByteWriter& o) {
    if (this == &o) {
        return;
    }
    d = o.d;
    isLE = o.isLE;
}

void ByteWriter::Write8(u8 b) {
    d.AppendChar((char)b);
}

void ByteWriter::Write8x2(u8 b1, u8 b2) {
    u8 buf[2]{b1, b2};
    d.Append(buf, 2);
}

void ByteWriter::Write16(u16 val) {
    u8 b1 = val & 0xFF;
    u8 b2 = (val >> 8) & 0xFF;
    if (isLE) {
        return Write8x2(b1, b2);
    }
    return Write8x2(b2, b1);
}

void ByteWriter::Write32(u32 val) {
    u8 b1 = val & 0xFF;
    u8 b2 = (val >> 8) & 0xFF;
    u8 b3 = (val >> 16) & 0xFF;
    u8 b4 = (val >> 24) & 0xFF;
    if (isLE) {
        Write8x2(b1, b2);
        Write8x2(b3, b4);
        return;
    }
    Write8x2(b4, b3);
    Write8x2(b2, b1);
}

void ByteWriter::Write64(u64 val) {
    u32 v1 = val & 0xFFFFFFFF;
    u32 v2 = (val >> 32) & 0xFFFFFFFF;
    if (isLE) {
        Write32(v1);
        Write32(v2);
        return;
    }
    Write32(v2);
    Write32(v1);
}

size_t ByteWriter::Size() const {
    return d.size();
}

ByteSlice ByteWriter::AsByteSlice() const {
    return d.AsByteSlice();
}

ByteWriterLE::ByteWriterLE(size_t sizeHint) {
    d.cap = (u32)sizeHint;
    isLE = true;
}
