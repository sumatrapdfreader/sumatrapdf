/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"

#include "LitDoc.h"

#include "base/tests/UtAssert.h"

// little-endian writer over a zero-filled buffer
struct LeWriter {
    u8* d;
    int cap = 0;
    int off = 0;

    void Need(int n) { ReportIf(off < 0 || n < 0 || off + n > cap); }
    void Zeros(int n) {
        Need(n);
        off += n;
    }
    void U8(u8 v) {
        Need(1);
        d[off++] = v;
    }
    void U16(u16 v) {
        U8((u8)v);
        U8((u8)(v >> 8));
    }
    void U32(u32 v) {
        U16((u16)v);
        U16((u16)(v >> 16));
    }
    void U64(u64 v) {
        U32((u32)v);
        U32((u32)(v >> 32));
    }
    void Bytes(const char* s, int n) {
        Need(n);
        memcpy(d + off, s, (size_t)n);
        off += n;
    }
    void EncInt(u32 v) {
        u8 tmp[8];
        int n = 0;
        do {
            tmp[n++] = (u8)(v & 0x7f);
            v >>= 7;
        } while (v);
        for (int i = n - 1; i > 0; i--) {
            U8(tmp[i] | 0x80);
        }
        U8(tmp[0]);
    }
};

static void PutU32(u8* d, int off, u32 v) {
    d[off] = (u8)v;
    d[off + 1] = (u8)(v >> 8);
    d[off + 2] = (u8)(v >> 16);
    d[off + 3] = (u8)(v >> 24);
}

static Str MkLitBuf(int fileLen, int hdrLen, int nPieces, int secHdrLen) {
    u8* d = AllocArray<u8>(fileLen);
    LeWriter w{d, fileLen};
    w.Bytes("ITOLITLS", 8);
    w.U32(1);
    w.U32((u32)hdrLen);
    w.U32((u32)nPieces);
    w.U32((u32)secHdrLen);
    ReportIf(w.off > hdrLen);
    return Str((char*)d, fileLen);
}

static void LitMustReject(Str lit) {
    Str epub = LitToEpubConvert(lit);
    utassert(str::IsNull(epub));
    str::Free(epub);
    str::Free(lit);
}

// hdrLen near INT_MAX: hdrLen + nPieces*16 used to wrap to a negative offset
static Str MkLitHdrLenWrap() {
    Str s = MkLitBuf(40, 0x7ffffff0, 5, 0);
    return s;
}

// secondary-header pos = 0x7fffffff: pos+8 used to overflow the loop bound
static Str MkLitPosOverflow() {
    constexpr int kHdrLen = 40;
    constexpr int kNPieces = 5;
    constexpr int kSecHdrLen = 16;
    constexpr int kFileLen = kHdrLen + (kNPieces * 16) + kSecHdrLen;
    Str s = MkLitBuf(kFileLen, kHdrLen, kNPieces, kSecHdrLen);
    PutU32((u8*)s.s, kHdrLen + (kNPieces * 16) + 4, 0x7fffffff);
    return s;
}

// directory offset/length whose signed i64 sum wraps past the file size
static Str MkLitDirRangeWrap() {
    constexpr int kHdrLen = 40;
    constexpr int kNPieces = 5;
    constexpr int kSecHdrLen = 56;
    constexpr int kFileLen = kHdrLen + (kNPieces * 16) + kSecHdrLen;
    Str s = MkLitBuf(kFileLen, kHdrLen, kNPieces, kSecHdrLen);
    u8* d = (u8*)s.s;
    LeWriter w{d, kFileLen};
    w.off = kHdrLen + 16; // piece 1
    w.U64(0x7ffffffff0000000ull);
    w.U64(0x10000010ull);
    w.off = kHdrLen + (kNPieces * 16);
    w.U32(0);
    w.U32(8); // pos of ITSF
    w.Bytes("ITSF", 4);
    w.U32(4);
    return s;
}

// ITSF contentOffset = 0x80000000 used to narrow to a negative int
static Str MkLitContentOffsetNeg() {
    constexpr int kHdrLen = 40;
    constexpr int kNPieces = 5;
    constexpr int kSecHdrLen = 56;
    constexpr int kDirOff = kHdrLen + (kNPieces * 16) + kSecHdrLen; // 176
    constexpr int kChunkSize = 128;
    constexpr int kDirLen = 32 + kChunkSize;
    constexpr int kFileLen = kDirOff + kDirLen; // 336

    Str s = MkLitBuf(kFileLen, kHdrLen, kNPieces, kSecHdrLen);
    u8* d = (u8*)s.s;
    LeWriter w{d, kFileLen};
    w.off = kHdrLen + 16;
    w.U64(kDirOff);
    w.U64(kDirLen);

    w.off = kHdrLen + (kNPieces * 16);
    w.U32(0);
    w.U32(8);
    w.Bytes("ITSF", 4);
    w.U32(4);
    w.Zeros(8);
    w.U32(0x80000000);
    w.U32(0);

    w.off = kDirOff;
    w.Bytes("IFCM", 4);
    w.U32(0);
    w.U32(kChunkSize);
    w.Zeros(12);
    w.U32(1); // nChunks
    w.Bytes("AOLL", 4);
    w.U32(54); // freeSpace: dataEnd = 128-54-2 = 72
    w.off = kDirOff + 32 + 48;
    w.EncInt(20);
    w.Bytes("::DataSpace/NameList", 20);
    w.EncInt(0);
    w.EncInt(0);
    w.EncInt(4);
    d[kDirOff + kDirLen - 2] = 1; // nEntries
    return s;
}

// /manifest in section 1 with offset INT_MAX: offset+size used to wrap
static Str MkLitSectionOffsetWrap() {
    constexpr int kHdrLen = 40;
    constexpr int kNPieces = 5;
    constexpr int kSecHdrLen = 56;
    constexpr int kDirOff = kHdrLen + (kNPieces * 16) + kSecHdrLen; // 176
    constexpr int kChunkSize = 256;
    constexpr int kDirLen = 32 + kChunkSize;       // 288
    constexpr int kContentOff = kDirOff + kDirLen; // 464
    constexpr int kFileLen = kContentOff + 64;     // 528

    Str s = MkLitBuf(kFileLen, kHdrLen, kNPieces, kSecHdrLen);
    u8* d = (u8*)s.s;
    LeWriter w{d, kFileLen};
    w.off = kHdrLen + 16;
    w.U64(kDirOff);
    w.U64(kDirLen);

    w.off = kHdrLen + (kNPieces * 16);
    w.U32(0);
    w.U32(8);
    w.Bytes("ITSF", 4);
    w.U32(4);
    w.Zeros(8);
    w.U32(kContentOff);
    w.U32(0);

    w.off = kDirOff;
    w.Bytes("IFCM", 4);
    w.U32(0);
    w.U32(kChunkSize);
    w.Zeros(12);
    w.U32(1);
    w.Bytes("AOLL", 4);
    w.U32(132); // freeSpace: dataEnd = 256-132-2 = 122
    w.off = kDirOff + 32 + 48;
    w.EncInt(20);
    w.Bytes("::DataSpace/NameList", 20);
    w.EncInt(0);
    w.EncInt(0);
    w.EncInt(16);
    w.EncInt(29);
    w.Bytes("::DataSpace/Storage/X/Content", 29);
    w.EncInt(0);
    w.EncInt(32);
    w.EncInt(4);
    w.EncInt(9);
    w.Bytes("/manifest", 9);
    w.EncInt(1);
    w.EncInt(0x7fffffff);
    w.EncInt(2);
    d[kDirOff + kDirLen - 2] = 3;

    // NameList: 2 sections "A" and "X"
    w.off = kContentOff;
    w.U16(0);
    w.U16(2);
    w.U16(1);
    w.U16('A');
    w.U16(0);
    w.U16(1);
    w.U16('X');
    w.U16(0);
    w.off = kContentOff + 32;
    w.Bytes("ABCD", 4);
    return s;
}

// directory nameLen = INT_MAX: pos+nameLen used to wrap past dataEnd
static Str MkLitNameLenWrap() {
    Str s = MkLitContentOffsetNeg();
    u8* d = (u8*)s.s;
    constexpr int kEntry = 176 + 32 + 48;
    LeWriter w{d, len(s)};
    w.off = kEntry;
    w.EncInt(0x7fffffff);
    PutU32(d, 144, 0); // valid contentOffset so ParseHeader reaches the directory
    return s;
}

extern "C" {
struct LZXstate;
struct LZXstate* LZXinit(int window);
void LZXteardown(struct LZXstate* pState);
int LZXdecompress(struct LZXstate* pState, u8* inpos, u8* outpos, int inlen, int outlen);
}

// 16-bit little-endian words; the decompressor consumes bit 15 first.
struct LzxBitWriter {
    u8* d;
    int cap = 0;
    int nbits = 0;

    void Bit(int b) {
        int bitIndex = 15 - (nbits % 16);
        int byteOff = (nbits / 16) * 2 + (bitIndex >= 8 ? 1 : 0);
        ReportIf(byteOff < 0 || byteOff >= cap);
        d[byteOff] |= (u8)((b & 1) << (bitIndex & 7));
        nbits++;
    }
    void Bits(u32 v, int n) {
        for (int i = n - 1; i >= 0; i--) {
            Bit((int)(v >> i) & 1);
        }
    }
    // Pad to the 16-bit boundary where an uncompressed block's bytes start.
    int Align16() {
        int bytes = ((nbits + 15) / 16) * 2;
        nbits = bytes * 8;
        return bytes;
    }
};

// One uncompressed LZX block. intelBit is the stream header (first block only).
static int WriteLzxUncomp(u8* d, int cap, int len, const u8* payload, bool intelBit) {
    memset(d, 0, (size_t)cap);
    LzxBitWriter w{d, cap, 0};
    if (intelBit) {
        w.Bits(0, 1);
    }
    w.Bits(3, 3);
    w.Bits(((u32)len >> 8) & 0xffff, 16);
    w.Bits((u32)len & 0xff, 8);
    int off = w.Align16();
    off += 12;
    ReportIf(off < 0 || len < 0 || off + len > cap);
    memcpy(d + off, payload, (size_t)len);
    return off + len;
}

// A frame whose two blocks cross the end of the window must return the tail
// then the head, not bytes from before the window allocation.
static void LzxWindowWrap() {
    constexpr int kWnd = 15;
    constexpr int kSize = 1 << kWnd;
    constexpr int kTail = 4;

    LZXstate* st = LZXinit(kWnd);
    ReportIf(!st);

    int n1 = kSize - kTail;
    u8* pay1 = (u8*)malloc((size_t)n1);
    u8* in1 = (u8*)malloc((size_t)n1 + 64);
    u8* out1 = (u8*)malloc((size_t)n1);
    ReportIf(!pay1 || !in1 || !out1);
    memset(pay1, 0xA1, (size_t)n1);
    int nIn1 = WriteLzxUncomp(in1, n1 + 32, n1, pay1, true);
    int rc = LZXdecompress(st, in1, out1, nIn1, n1);
    utassert(rc == 0);
    utassert(memcmp(out1, pay1, (size_t)n1) == 0);

    u8 tail[4] = {1, 2, 3, 4};
    u8 head[4] = {5, 6, 7, 8};
    u8 in2[128];
    int nA = WriteLzxUncomp(in2, 64, 4, tail, false);
    int nB = WriteLzxUncomp(in2 + nA, 64, 4, head, false);
    u8 out2[8];
    memset(out2, 0x5a, sizeof(out2));
    rc = LZXdecompress(st, in2, out2, nA + nB, 8);
    utassert(rc == 0);
    u8 expect[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    utassert(memcmp(out2, expect, sizeof(expect)) == 0);

    LZXteardown(st);
    free(pay1);
    free(in1);
    free(out1);
}

void LitDoc_UnitTests() {
    LzxWindowWrap();
    LitMustReject(MkLitHdrLenWrap());
    LitMustReject(MkLitPosOverflow());
    LitMustReject(MkLitDirRangeWrap());
    LitMustReject(MkLitContentOffsetNeg());
    LitMustReject(MkLitSectionOffsetWrap());
    LitMustReject(MkLitNameLenWrap());
}
