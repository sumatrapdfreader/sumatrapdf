/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"
#include "utils/ByteReader.h"

// Unpacks a structure from the data according to the given format
// e.g. the format "32b2w6d" unpacks 32 Bytes, 2 16-bit Words and 6 32-bit Dwords
bool ByteReader::Unpack(void* strct, size_t size, const char* format, size_t off, bool isBE) const {
    int repeat = 0;
    size_t idx = 0;
    for (const char* c = format; *c; c++) {
        if (isdigit((u8)*c)) {
            repeat = atoi(c);
            for (c++; isdigit((u8)*c); c++) {
                ;
            }
        }
        switch (*c) {
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
            c--;
        }
    }
    return idx == size;
}

ByteReader::ByteReader(std::string_view data) {
    d = (const u8*)data.data();
    len = data.size();
}

ByteReader::ByteReader(std::span<u8> data) {
    d = (const u8*)data.data();
    len = data.size();
}

ByteReader::ByteReader(const char* data, size_t len) : d((const u8*)data), len(len) {
}

ByteReader::ByteReader(const u8* data, size_t len) : d((const u8*)data), len(len) {
}

u8 ByteReader::Byte(size_t off) const {
    if (off < len) {
        return d[off];
    }
    return 0;
}

u16 ByteReader::WordLE(size_t off) const {
    if (off + 2 <= len) {
        return d[off] | (d[off + 1] << 8);
    }
    return 0;
}

u16 ByteReader::WordBE(size_t off) const {
    if (off + 2 <= len) {
        return (d[off] << 8) | d[off + 1];
    }
    return 0;
}

u16 ByteReader::Word(size_t off, bool isBE) const {
    return isBE ? WordBE(off) : WordLE(off);
}

u32 ByteReader::DWordLE(size_t off) const {
    if (off + 4 <= len) {
        return d[off] | (d[off + 1] << 8) | (d[off + 2] << 16) | (d[off + 3] << 24);
    }
    return 0;
}

u32 ByteReader::DWordBE(size_t off) const {
    if (off + 4 <= len) {
        return (d[off] << 24) | (d[off + 1] << 16) | (d[off + 2] << 8) | d[off + 3];
    }
    return 0;
}

u32 ByteReader::DWord(size_t off, bool isBE) const {
    return isBE ? DWordBE(off) : DWordLE(off);
}

u64 ByteReader::QWordLE(size_t off) const {
    if (off + 8 <= len) {
        return DWordLE(off) | ((u64)DWordLE(off + 4) << 32);
    }
    return 0;
}

u64 ByteReader::QWordBE(size_t off) const {
    if (off + 8 <= len) {
        return ((u64)DWordBE(off) << 32) | DWordBE(off + 4);
    }
    return 0;
}

u64 ByteReader::QWord(size_t off, bool isBE) const {
    return isBE ? QWordBE(off) : QWordLE(off);
}

const u8* ByteReader::Find(size_t off, u8 byte) const {
    if (off >= len) {
        return nullptr;
    }
    return (const u8*)memchr(d + off, byte, len - off);
}

bool ByteReader::UnpackLE(void* strct, size_t size, const char* format, size_t off) const {
    return Unpack(strct, size, format, off, false);
}

bool ByteReader::UnpackBE(void* strct, size_t size, const char* format, size_t off) const {
    return Unpack(strct, size, format, off, true);
}

bool ByteReader::Unpack(void* strct, size_t size, const char* format, bool isBE, size_t off) const {
    return Unpack(strct, size, format, off, isBE);
}
