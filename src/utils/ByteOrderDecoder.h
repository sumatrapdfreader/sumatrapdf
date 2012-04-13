/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#ifndef ByteOrderDecoder_h
#define ByteOrderDecoder_h

class ByteOrderDecoder
{
public:
    enum ByteOrder { LittleEndian, BigEndian };

    ByteOrderDecoder(const char *d, size_t len, ByteOrder order);
    ByteOrderDecoder(const uint8 *d, size_t len, ByteOrder order);

    void ChangeOrder(ByteOrder newOrder) { CrashIf(byteOrder == newOrder); byteOrder = newOrder; }

    uint16 UInt16();
    int16  Int16() { return (int16)UInt16(); }
    uint32 UInt32();
    int32  Int32() { return (int32)UInt32(); }

    void   Skip(size_t len);
    size_t Offset() const { return curr - data; } // for debugging

protected:
    ByteOrder byteOrder;
    const uint8 *data;

    const uint8 *curr;
    size_t left;
};

#endif

