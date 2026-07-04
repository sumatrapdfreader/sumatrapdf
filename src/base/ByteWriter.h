/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

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
