/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

class BitReader {
    u8 GetByte(int pos) const;

  public:
    BitReader(u8* data, int n);
    ~BitReader();
    u32 Peek(int nBits);
    int BitsLeft() const;
    bool Eat(int nBits);

    u8* data = nullptr;
    int dataLen = 0;
    int currBitPos = 0;
    int bitsCount = 0;
};