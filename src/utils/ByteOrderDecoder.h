/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
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

    uint16_t UInt16();
    int16_t Int16() {
        return (int16_t)UInt16();
    }

    uint32_t UInt32();
    int32_t Int32() {
        return (int32_t)UInt32();
    }

    uint64_t UInt64();
    int64_t Int64() {
        return (int64_t)UInt64();
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
    const uint8_t* data;

    const uint8_t* curr;
    size_t left;
};

// decode a given piece of memory
uint16_t UInt16BE(const u8* d);
uint16_t UInt16LE(const u8* d);
uint32_t UInt32BE(const u8* d);
uint32_t UInt32LE(const u8* d);
