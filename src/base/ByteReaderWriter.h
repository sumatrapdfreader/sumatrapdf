/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

class BitReader {
    u8 GetByte(int pos) const;

  public:
    BitReader(u8* data, int n);
    ~BitReader();
    u32 Peek(int nBits);
    int BitsLeft() const;
    bool Eat(int nBits);

    u8* data = nullptr;
    int dataLen = 0;
    int currBitPos = 0;
    int bitsCount = 0;
};

struct ByteReader {
    const u8* d = nullptr;
    int len = 0;
    int off = 0;
    bool ok = true;

    bool Unpack(void* strct, int size, Str format, int off, bool isBE) const;

    explicit ByteReader(Str data);
    ByteReader(const u8* data, int n);

    u8 UInt8(int off) const;
    u16 UInt16LE(int off) const;
    u16 UInt16BE(int off) const;
    u16 UInt16(int off, bool isBE) const;
    u32 UInt32LE(int off) const;
    u32 UInt32BE(int off) const;
    u32 UInt32(int off, bool isBE) const;
    u64 UInt64LE(int off) const;
    u64 UInt64BE(int off) const;
    u64 UInt64(int off, bool isBE) const;

    u8 UInt8();
    char Char();
    u16 UInt16LE();
    u16 UInt16BE();
    i16 Int16LE();
    i16 Int16BE();
    u32 UInt32LE();
    u32 UInt32BE();
    i32 Int32LE();
    i32 Int32BE();
    u64 UInt64LE();
    u64 UInt64BE();
    i64 Int64LE();
    i64 Int64BE();
    void Bytes(void* dst, int n);
    void Skip(int n);
    void Unskip(int n);
    int Offset() const;
    bool IsOk() const;

    const u8* Find(int off, u8 byte) const;
    bool UnpackLE(void* strct, int size, Str format, int off = 0) const;
    bool UnpackBE(void* strct, int size, Str format, int off = 0) const;
    bool Unpack(void* strct, int size, Str format, bool isBE, int off = 0) const;
};

u16 UInt16BE(const u8* d);
u16 UInt16LE(const u8* d);
u32 UInt32BE(const u8* d);
u32 UInt32LE(const u8* d);

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
