/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

class BitReader {
    uint8_t GetByte(size_t pos);

  public:
    BitReader(uint8_t* data, size_t len);
    ~BitReader();
    uint32_t Peek(size_t bitsCount);
    size_t BitsLeft();
    bool Eat(size_t bitsCount);

    uint8_t* data = nullptr;
    size_t dataLen = 0;
    size_t currBitPos = 0;
    size_t bitsCount = 0;
};
