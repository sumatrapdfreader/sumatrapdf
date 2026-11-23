/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "base/Exif.h"

// must be last due to assert() over-write
#include "base/tests/UtAssert.h"

constexpr u16 kTiffShort = 3;
constexpr u16 kTiffLong = 4;
constexpr int kIfdEntryLen = 12;

// little-endian writer over a zero-filled buffer
struct LeWriter {
    u8* d;
    int off = 0;

    void U16(u16 v) {
        d[off++] = (u8)(v & 0xff);
        d[off++] = (u8)(v >> 8);
    }
    void U32(u32 v) {
        U16((u16)(v & 0xffff));
        U16((u16)(v >> 16));
    }
    void Bytes(const char* s, int n) {
        memcpy(d + off, s, (size_t)n);
        off += n;
    }
    void Entry(u16 tag, u16 type, u32 count, u32 val) {
        U16(tag);
        U16(type);
        U32(count);
        U32(val);
    }
};

// TIFF with IFD0 = { Orientation = 6 }
static void exifValidTiffTest() {
    u8 d[64]{};
    LeWriter w{d};
    w.Bytes("II*\0", 4);
    w.U32(8);
    w.U16(1);
    w.Entry((u16)ExifProp::Orientation, kTiffShort, 1, 6);
    w.U32(0);

    ExifParser parser;
    utassert(parser.Parse(Str((char*)d, w.off)));
    i64 v = 0;
    utassert(parser.GetIntProp(ExifProp::Orientation, &v));
    utassert(v == 6);
}

// HEIC whose bytes hold a TIFF header with IFD offset 0xFFFFFFF0
static void exifHugeIfdOffsetTest() {
    u8 d[48]{};
    LeWriter w{d};
    w.U32(0x18000000); // box size 24 (big-endian), value doesn't matter
    w.Bytes("ftypheic", 8);
    w.off = 24;
    w.Bytes("II*\0", 4);
    w.U32(0xFFFFFFF0);

    ExifParser parser;
    utassert(!parser.Parse(Str((char*)d, dimofi(d))));
}

void ExifTest() {
    exifValidTiffTest();
    exifHugeIfdOffsetTest();
}
