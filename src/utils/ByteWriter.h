/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

struct ByteWriter {
    u8* dst = nullptr;
    u8* end = nullptr;
    bool isLE = false;

    ByteWriter(u8* dst, size_t bytesLeft, bool isLE);

    ByteWriter(const ByteWriter& o);

    size_t Left() const;
    bool Write8(u8 b);
    bool Write8x2(u8 b1, u8 b2);
    bool Write16(u16 val);
    bool Write32(u32 val);
    bool Write64(u64 val);
};

ByteWriter MakeByteWriterLE(u8* dst, size_t len);
ByteWriter MakeByteWriterLE(char* dst, size_t len);
ByteWriter MakeByteWriterBE(u8* dst, size_t len);
ByteWriter MakeByteWriterBE(char* dst, size_t len);