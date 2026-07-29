/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

struct ByteReader {
    const u8* d = nullptr;
    int len = 0;

    // Unpacks a structure from the data according to the given format
    // e.g. the format "32b2w6d" unpacks 32 Bytes, 2 16-bit Words and 6 32-bit Dwords
    bool Unpack(void* strct, int size, Str format, int off, bool isBE) const;

    explicit ByteReader(Str data);
    ByteReader(const u8* data, int n);

    u8 Byte(int off) const;
    u16 WordLE(int off) const;
    u16 WordBE(int off) const;
    u16 Word(int off, bool isBE) const;
    u32 DWordLE(int off) const;
    u32 DWordBE(int off) const;
    u32 DWord(int off, bool isBE) const;
    u64 QWordLE(int off) const;
    u64 QWordBE(int off) const;
    u64 QWord(int off, bool isBE) const;
    const u8* Find(int off, u8 byte) const;
    bool UnpackLE(void* strct, int size, Str format, int off = 0) const;
    bool UnpackBE(void* strct, int size, Str format, int off = 0) const;
    bool Unpack(void* strct, int size, Str format, bool isBE, int off = 0) const;
};

struct ByteWriter {
    bool isLE = false;
    str::Builder d;

    ByteWriter(int sizeHint = 0);
    ByteWriter(const ByteWriter&) = delete;
    ByteWriter& operator=(const ByteWriter&) = delete;

    void Write8(u8 b);
    void Write8x2(u8 b1, u8 b2);
    void Write16(u16 val);
    void Write32(u32 val);
    void Write64(u64 val);

    int Size() const;
    Str AsByteSlice() const;
};

struct ByteWriterLE : ByteWriter {
    ByteWriterLE(int sizeHint = 0);
};
