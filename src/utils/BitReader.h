/* Copyright 2015 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

class BitReader
{
    uint8_t GetByte(size_t pos);

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
