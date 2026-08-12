/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "base/ByteReaderWriter.h"

// --- BitReader

// Bit reader is a streaming reader of bits from underlying memory data

// data has to be valid for the lifetime of this class
BitReader::BitReader(u8* data, int n) : data(data), dataLen(n) {
    bitsCount = n * 8;
}

BitReader::~BitReader() = default;

u8 BitReader::GetByte(int pos) const {
    if (pos >= dataLen) {
        return 0;
    }
    return data[pos];
}

// advance position in the bit stream
// returns false if we've eaten bits more than we have
bool BitReader::Eat(int nBits) {
    currBitPos += nBits;
    return currBitPos <= bitsCount;
}

int BitReader::BitsLeft() const {
    if (currBitPos < bitsCount) {
        return bitsCount - currBitPos;
    }
    return 0;
}

// Read nBits (up to 32) bits, without advancing the position in the bit stream
// If asked for more bits than we have left, the extra bits will be 0
u32 BitReader::Peek(int nBits) {
    ReportIf((nBits == 0) || (nBits > 32));
    int currBytePos = currBitPos / 8;
    u8 currByte = GetByte(currBytePos);
    u8 currBit = currBitPos % 8;
    currByte = currByte << currBit;
    u8 bitsLeft = 8 - currBit;
    u32 ret = 0;
    while (nBits > 0) {
        if (0 == bitsLeft) {
            ++currBytePos;
            currByte = GetByte(currBytePos);
            bitsLeft = 8;
        }
        // being conservative here, could probably handle
        // bitsLeft other than 8
        if ((8 == bitsLeft) && (nBits >= 8)) {
            // fast path - 8 bits at a time
            ret = (ret << 8) | currByte;
            bitsLeft = 0;
            nBits -= 8;
        } else {
            // slow path - 1 bit at a time
            ret = ret << 1;
            if ((0x80 & currByte) != 0) {
                ret |= 1;
            }
            currByte = currByte << 1;
            bitsLeft--;
            nBits--;
        }
    }
    return ret;
}

// --- ByteReader

// Unpacks a structure from the data according to the given format
// e.g. the format "32b2w6d" unpacks 32 Bytes, 2 16-bit Words and 6 32-bit Dwords
bool ByteReader::Unpack(void* strct, int size, Str format, int off, bool isBE) const {
    if (!format) {
        return false;
    }
    int repeat = 0;
    int idx = 0;
    for (int i = 0; i < format.len; i++) {
        char c = format.s[i];
        if (isdigit((u8)c)) {
            repeat = ParseInt(Str(format.s + i, format.len - i));
            for (i++; i < format.len && isdigit((u8)format.s[i]); i++) {
                ;
            }
            if (i >= format.len) {
                break;
            }
            c = format.s[i];
        }
        switch (c) {
            case 'b':
                if (off + idx + 1 > len || idx + 1 > size) {
                    return false;
                }
                *(u8*)((u8*)strct + idx) = UInt8(off + idx);
                idx += 1;
                break;
            case 'w':
                if (off + idx + 2 > len || idx + 2 > size) {
                    return false;
                }
                *(u16*)((u8*)strct + idx) = UInt16(off + idx, isBE);
                idx += 2;
                break;
            case 'd':
                if (off + idx + 4 > len || idx + 4 > size) {
                    return false;
                }
                *(u32*)((u8*)strct + idx) = UInt32(off + idx, isBE);
                idx += 4;
                break;
            case 'q':
                if (off + idx + 8 > len || idx + 8 > size) {
                    return false;
                }
                *(u64*)((u8*)strct + idx) = UInt64(off + idx, isBE);
                idx += 8;
                break;
            default:
                return false;
        }
        if (--repeat > 0) {
            i--;
        }
    }
    return idx == size;
}

ByteReader::ByteReader(Str data) : d((const u8*)data.s), len(data.len) {}

ByteReader::ByteReader(const u8* data, int n) : d(data), len(n) {}

u8 ByteReader::UInt8(int off) const {
    if (off >= 0 && off < len) {
        return d[off];
    }
    return 0;
}

u16 ByteReader::UInt16LE(int off) const {
    if (off >= 0 && off + 2 <= len) {
        return d[off] | (d[off + 1] << 8);
    }
    return 0;
}

u16 ByteReader::UInt16BE(int off) const {
    if (off >= 0 && off + 2 <= len) {
        return (d[off] << 8) | d[off + 1];
    }
    return 0;
}

u16 ByteReader::UInt16(int off, bool isBE) const {
    return isBE ? UInt16BE(off) : UInt16LE(off);
}

u32 ByteReader::UInt32LE(int off) const {
    if (off >= 0 && off + 4 <= len) {
        return d[off] | (d[off + 1] << 8) | (d[off + 2] << 16) | (d[off + 3] << 24);
    }
    return 0;
}

u32 ByteReader::UInt32BE(int off) const {
    if (off >= 0 && off + 4 <= len) {
        return (d[off] << 24) | (d[off + 1] << 16) | (d[off + 2] << 8) | d[off + 3];
    }
    return 0;
}

u32 ByteReader::UInt32(int off, bool isBE) const {
    return isBE ? UInt32BE(off) : UInt32LE(off);
}

u64 ByteReader::UInt64LE(int off) const {
    if (off >= 0 && off + 8 <= len) {
        return UInt32LE(off) | ((u64)UInt32LE(off + 4) << 32);
    }
    return 0;
}

u64 ByteReader::UInt64BE(int off) const {
    if (off >= 0 && off + 8 <= len) {
        return ((u64)UInt32BE(off) << 32) | UInt32BE(off + 4);
    }
    return 0;
}

u64 ByteReader::UInt64(int off, bool isBE) const {
    return isBE ? UInt64BE(off) : UInt64LE(off);
}

u8 ByteReader::UInt8() {
    if (!ok || off >= len) {
        ok = false;
        return 0;
    }
    return d[off++];
}

char ByteReader::Char() {
    return (char)UInt8();
}

#define SEQUENTIAL_READ(name, type, size) \
    type ByteReader::name() {             \
        if (!ok || off > len - (size)) {  \
            ok = false;                   \
            return 0;                     \
        }                                 \
        type res = name(off);             \
        off += (size);                    \
        return res;                       \
    }

SEQUENTIAL_READ(UInt16LE, u16, 2)
SEQUENTIAL_READ(UInt16BE, u16, 2)
SEQUENTIAL_READ(UInt32LE, u32, 4)
SEQUENTIAL_READ(UInt32BE, u32, 4)
SEQUENTIAL_READ(UInt64LE, u64, 8)
SEQUENTIAL_READ(UInt64BE, u64, 8)

#undef SEQUENTIAL_READ

i16 ByteReader::Int16LE() {
    return (i16)UInt16LE();
}

i16 ByteReader::Int16BE() {
    return (i16)UInt16BE();
}

i32 ByteReader::Int32LE() {
    return (i32)UInt32LE();
}

i32 ByteReader::Int32BE() {
    return (i32)UInt32BE();
}

i64 ByteReader::Int64LE() {
    return (i64)UInt64LE();
}

i64 ByteReader::Int64BE() {
    return (i64)UInt64BE();
}

void ByteReader::Bytes(void* dst, int n) {
    if (!ok || n < 0 || off > len - n) {
        ok = false;
        return;
    }
    memcpy(dst, d + off, (size_t)n);
    off += n;
}

void ByteReader::Skip(int n) {
    if (!ok || n < 0 || off > len - n) {
        ok = false;
        return;
    }
    off += n;
}

void ByteReader::Unskip(int n) {
    if (!ok || n < 0 || off < n) {
        ok = false;
        return;
    }
    off -= n;
}

int ByteReader::Offset() const {
    return off;
}

bool ByteReader::IsOk() const {
    return ok;
}

const u8* ByteReader::Find(int off, u8 byte) const {
    if (off >= len) {
        return nullptr;
    }
    return (const u8*)memchr(d + off, byte, (size_t)(len - off));
}

bool ByteReader::UnpackLE(void* strct, int size, Str format, int off) const {
    return Unpack(strct, size, format, off, false);
}

bool ByteReader::UnpackBE(void* strct, int size, Str format, int off) const {
    return Unpack(strct, size, format, off, true);
}

// Unpacks a structure from the data according to the given format
// e.g. the format "32b2w6d" unpacks 32 Bytes, 2 16-bit Words and 6 32-bit Dwords
bool ByteReader::Unpack(void* strct, int size, Str format, bool isBE, int off) const {
    return Unpack(strct, size, format, off, isBE);
}

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

// --- ByteWriter

ByteWriter::ByteWriter(int sizeHint) {
    d.cap = sizeHint;
}

void ByteWriter::Write8(u8 b) {
    d.AppendChar((char)b);
}

void ByteWriter::Write8x2(u8 b1, u8 b2) {
    u8 buf[2]{b1, b2};
    d.Append(Str((char*)buf, 2));
}

void ByteWriter::Write16(u16 val) {
    u8 b1 = val & 0xFF;
    u8 b2 = (val >> 8) & 0xFF;
    if (isLE) {
        Write8x2(b1, b2);
        return;
    }
    Write8x2(b2, b1);
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

int ByteWriter::Size() const {
    return len(d);
}

Str ByteWriter::AsByteSlice() const {
    return ToStr(d);
}

ByteWriterLE::ByteWriterLE(int sizeHint) {
    d.cap = sizeHint;
    isLE = true;
}
