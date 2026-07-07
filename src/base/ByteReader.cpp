/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "base/ByteReader.h"

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
                *(u8*)((u8*)strct + idx) = Byte(off + idx);
                idx += 1;
                break;
            case 'w':
                if (off + idx + 2 > len || idx + 2 > size) {
                    return false;
                }
                *(u16*)((u8*)strct + idx) = Word(off + idx, isBE);
                idx += 2;
                break;
            case 'd':
                if (off + idx + 4 > len || idx + 4 > size) {
                    return false;
                }
                *(u32*)((u8*)strct + idx) = DWord(off + idx, isBE);
                idx += 4;
                break;
            case 'q':
                if (off + idx + 8 > len || idx + 8 > size) {
                    return false;
                }
                *(u64*)((u8*)strct + idx) = QWord(off + idx, isBE);
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

u8 ByteReader::Byte(int off) const {
    if (off < len) {
        return d[off];
    }
    return 0;
}

u16 ByteReader::WordLE(int off) const {
    if (off + 2 <= len) {
        return d[off] | (d[off + 1] << 8);
    }
    return 0;
}

u16 ByteReader::WordBE(int off) const {
    if (off + 2 <= len) {
        return (d[off] << 8) | d[off + 1];
    }
    return 0;
}

u16 ByteReader::Word(int off, bool isBE) const {
    return isBE ? WordBE(off) : WordLE(off);
}

u32 ByteReader::DWordLE(int off) const {
    if (off + 4 <= len) {
        return d[off] | (d[off + 1] << 8) | (d[off + 2] << 16) | (d[off + 3] << 24);
    }
    return 0;
}

u32 ByteReader::DWordBE(int off) const {
    if (off + 4 <= len) {
        return (d[off] << 24) | (d[off + 1] << 16) | (d[off + 2] << 8) | d[off + 3];
    }
    return 0;
}

u32 ByteReader::DWord(int off, bool isBE) const {
    return isBE ? DWordBE(off) : DWordLE(off);
}

u64 ByteReader::QWordLE(int off) const {
    if (off + 8 <= len) {
        return DWordLE(off) | ((u64)DWordLE(off + 4) << 32);
    }
    return 0;
}

u64 ByteReader::QWordBE(int off) const {
    if (off + 8 <= len) {
        return ((u64)DWordBE(off) << 32) | DWordBE(off + 4);
    }
    return 0;
}

u64 ByteReader::QWord(int off, bool isBE) const {
    return isBE ? QWordBE(off) : QWordLE(off);
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

bool ByteReader::Unpack(void* strct, int size, Str format, bool isBE, int off) const {
    return Unpack(strct, size, format, off, isBE);
}