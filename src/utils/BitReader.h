/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

class BitReader {
    u8 GetByte(size_t pos);

  public:
    BitReader(u8* data, size_t len);
    ~BitReader();
    u32 Peek(size_t bitsCount);
    size_t BitsLeft();
    bool Eat(size_t bitsCount);

    u8* data = nullptr;
    size_t dataLen = 0;
    size_t currBitPos = 0;
    size_t bitsCount = 0;
};
