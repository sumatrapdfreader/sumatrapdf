/* Copyright 2014 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#ifndef ByteOrderDecoder_h
#define ByteOrderDecoder_h

// class for decoding of byte buffer as a sequence of numbers
class ByteOrderDecoder
{
public:
    enum ByteOrder { LittleEndian, BigEndian };

    ByteOrderDecoder(const char *d, size_t len, ByteOrder order);
    ByteOrderDecoder(const uint8 *d, size_t len, ByteOrder order);

    void ChangeOrder(ByteOrder newOrder) { CrashIf(byteOrder == newOrder); byteOrder = newOrder; }

    uint8  UInt8();
    char   Char() { return (char)UInt8(); }
    uint16 UInt16();
    int16  Int16() { return (int16)UInt16(); }
    uint32 UInt32();
    int32  Int32() { return (int32)UInt32(); }

    void   Bytes(char *dest, size_t len);

    void   Skip(size_t len);
    void   Unskip(size_t len);

    size_t Offset() const { return curr - data; }

protected:
    ByteOrder byteOrder;
    const uint8 *data;

    const uint8 *curr;
    size_t left;
};

// decode a given piece of memory
uint16 UInt16BE(const uint8* d);
uint16 UInt16LE(const uint8* d);
uint32 UInt32BE(const uint8* d);
uint32 UInt32LE(const uint8* d);

#endif
