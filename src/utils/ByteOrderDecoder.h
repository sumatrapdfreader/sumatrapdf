/* Copyright 2015 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

// class for decoding of byte buffer as a sequence of numbers
class ByteOrderDecoder
{
public:
    enum ByteOrder { LittleEndian, BigEndian };

    ByteOrderDecoder(const char *d, size_t len, ByteOrder order);
    ByteOrderDecoder(const uint8_t *d, size_t len, ByteOrder order);

    void ChangeOrder(ByteOrder newOrder) { CrashIf(byteOrder == newOrder); byteOrder = newOrder; }

    uint8_t  UInt8();
    char   Char() { return (char)UInt8(); }
    uint16_t UInt16();
    int16_t  Int16() { return (int16_t)UInt16(); }
    uint32_t UInt32();
    int32_t  Int32() { return (int32_t)UInt32(); }
    uint64_t UInt64();
    int64_t  Int64() { return (int64_t)UInt64(); }

    void   Bytes(char *dest, size_t len);

    void   Skip(size_t len);
    void   Unskip(size_t len);

    size_t Offset() const { return curr - data; }

protected:
    ByteOrder byteOrder;
    const uint8_t *data;

    const uint8_t *curr;
    size_t left;
};

// decode a given piece of memory
uint16_t UInt16BE(const uint8_t* d);
uint16_t UInt16LE(const uint8_t* d);
uint32_t UInt32BE(const uint8_t* d);
uint32_t UInt32LE(const uint8_t* d);
