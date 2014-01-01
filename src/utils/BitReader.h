/* Copyright 2014 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#ifndef BitReader_h
#define BitReader_h

class BitReader
{
    uint8_t GetByte(size_t pos) {
        if (pos >= dataLen)
            return 0;
        return data[pos];
    }

public:
    BitReader(uint8_t *data, size_t len);
    ~BitReader();
    uint32_t    Peek(size_t bitsCount);
    size_t      BitsLeft();
    bool        Eat(size_t bitsCount);

    uint8_t *   data;
    size_t      dataLen;
    size_t      currBitPos;
    size_t      bitsCount;
};

#endif
