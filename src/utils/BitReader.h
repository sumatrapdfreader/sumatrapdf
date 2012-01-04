/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#ifndef BitReader_h
#define BitReader_h

#include <stdint.h>

class BitReader
{
public:
    BitReader(unsigned char *data, int len);
    ~BitReader();
    uint32_t Peek(int n);
    size_t BitsLeft();
    bool Eat(int n);

    unsigned char *data;
    int pos;
    int bitsCount;
};

#endif
