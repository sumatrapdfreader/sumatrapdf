/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

struct ByteWriter {
    bool isLE = false;
    str::Str d;

    ByteWriter(size_t sizeHint = 0);
    ByteWriter(const ByteWriter& o);
    ByteWriter& operator=(const ByteWriter&) = delete;

    void Write8(u8 b);
    void Write8x2(u8 b1, u8 b2);
    void Write16(u16 val);
    void Write32(u32 val);
    void Write64(u64 val);

    size_t Size() const;
    ByteSlice AsByteSlice() const;
};

struct ByteWriterLE : ByteWriter {
    ByteWriterLE(size_t sizeHint = 0);
};
