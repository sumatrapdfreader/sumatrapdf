/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

struct ByteReader {
    const u8* d = nullptr;
    size_t len = 0;

    // Unpacks a structure from the data according to the given format
    // e.g. the format "32b2w6d" unpacks 32 Bytes, 2 16-bit Words and 6 32-bit Dwords
    bool Unpack(void* strct, size_t size, const char* format, size_t off, bool isBE) const;

    explicit ByteReader(const ByteSlice&);
    ByteReader(const char* data, size_t len);
    ByteReader(const u8* data, size_t len);

    u8 Byte(size_t off) const;
    u16 WordLE(size_t off) const;
    u16 WordBE(size_t off) const;
    u16 Word(size_t off, bool isBE) const;
    u32 DWordLE(size_t off) const;
    u32 DWordBE(size_t off) const;
    u32 DWord(size_t off, bool isBE) const;
    u64 QWordLE(size_t off) const;
    u64 QWordBE(size_t off) const;
    u64 QWord(size_t off, bool isBE) const;
    const u8* Find(size_t off, u8 byte) const;
    bool UnpackLE(void* strct, size_t size, const char* format, size_t off = 0) const;
    bool UnpackBE(void* strct, size_t size, const char* format, size_t off = 0) const;
    bool Unpack(void* strct, size_t size, const char* format, bool isBE, size_t off = 0) const;
};
