/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "BitReader.h"
#include "BaseUtil.h"

BitReader::BitReader(unsigned char *data, int len) {
    this->data = new unsigned char[len+8];
    memcpy(this->data, data, len);
    memset(this->data+len, 0, 8);
    pos = 0;
    bitsCount = len * 8;
}

BitReader::~BitReader() {
    delete data;
}

bool BitReader::Eat(int n) {
    pos += n;
    return (pos <= bitsCount);
}

size_t BitReader::BitsLeft() {
    return bitsCount - pos;
}

uint32_t BitReader::Peek(int n) {
    int currBytePos = pos / 8;
    uint8_t currByte = data[currBytePos];
    uint8_t currBit = pos % 8;
    currByte = currByte << currBit;
    uint8_t bitsLeft = 8 - currBit;
    uint32_t ret = 0;
    while (n > 0) {
        assert(bitsLeft >= 0);
        if (0 == bitsLeft) {
            ++currBytePos;
            currByte = data[currBytePos];
            bitsLeft = 8;
        }
        ret = ret << 1;
        if ((0x80 & currByte) != 0)
            ret |= 1;
        currByte = currByte << 1;
        --bitsLeft;
        --n;
    }
    return ret;
}
