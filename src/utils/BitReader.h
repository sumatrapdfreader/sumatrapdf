/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

class BitReader {
    u8 GetByte(size_t pos) const;

  public:
    BitReader(u8* data, size_t len);
    ~BitReader();
    u32 Peek(size_t bitsCount);
    size_t BitsLeft() const;
    bool Eat(size_t bitsCount);

    u8* data = nullptr;
    size_t dataLen = 0;
    size_t currBitPos = 0;
    size_t bitsCount = 0;
};
