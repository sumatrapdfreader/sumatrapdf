/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

// class for decoding of byte buffer as a sequence of numbers
class ByteOrderDecoder {
  public:
    enum ByteOrder { LittleEndian, BigEndian };

    ByteOrderDecoder(const char* d, size_t len, ByteOrder order);
    ByteOrderDecoder(const u8* d, size_t len, ByteOrder order);

    void Bytes(char* dest, size_t len);

    u8 UInt8();
    char Char() {
        return (char)UInt8();
    }

    u16 UInt16();
    i16 Int16() {
        return (i16)UInt16();
    }

    u32 UInt32();
    i32 Int32() {
        return (i32)UInt32();
    }

    u64 UInt64();
    i64 Int64() {
        return (i64)UInt64();
    }

    void Skip(size_t len);
    void Unskip(size_t len);

    size_t Offset() const {
        return curr - data;
    }

    bool IsOk() {
        return ok;
    };

  protected:
    bool ok;
    ByteOrder byteOrder;
    const u8* data;

    const u8* curr;
    size_t left;
};

// decode a given piece of memory
u16 UInt16BE(const u8* d);
u16 UInt16LE(const u8* d);
u32 UInt32BE(const u8* d);
u32 UInt32LE(const u8* d);
