/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

struct ByteReader {
    const u8* d = nullptr;
    size_t len = 0;

    // Unpacks a structure from the data according to the given format
    // e.g. the format "32b2w6d" unpacks 32 Bytes, 2 16-bit Words and 6 32-bit Dwords
    bool Unpack(void* strct, size_t size, const char* format, size_t off, bool isBE) const;

    ByteReader(std::string_view data);
    ByteReader(std::span<u8> data);
    ByteReader(const char* data, size_t len);
    ByteReader(const u8* data, size_t len);

    [[nodiscard]] u8 Byte(size_t off) const;
    [[nodiscard]] u16 WordLE(size_t off) const;
    [[nodiscard]] u16 WordBE(size_t off) const;
    [[nodiscard]] u16 Word(size_t off, bool isBE) const;
    [[nodiscard]] u32 DWordLE(size_t off) const;
    [[nodiscard]] u32 DWordBE(size_t off) const;
    [[nodiscard]] u32 DWord(size_t off, bool isBE) const;
    [[nodiscard]] u64 QWordLE(size_t off) const;
    [[nodiscard]] u64 QWordBE(size_t off) const;
    [[nodiscard]] u64 QWord(size_t off, bool isBE) const;
    [[nodiscard]] const u8* Find(size_t off, u8 byte) const;
    bool UnpackLE(void* strct, size_t size, const char* format, size_t off = 0) const;
    bool UnpackBE(void* strct, size_t size, const char* format, size_t off = 0) const;
    bool Unpack(void* strct, size_t size, const char* format, bool isBE, size_t off = 0) const;
};
