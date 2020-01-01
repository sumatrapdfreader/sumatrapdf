/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"
#include "utils/BitReader.h"

// Bit reader is a streaming reader of bits from underlying memory data

// data has to be valid for the lifetime of this class
BitReader::BitReader(uint8_t* data, size_t len) : data(data), dataLen(len), currBitPos(0) {
    bitsCount = len * 8;
}

BitReader::~BitReader() {
}

uint8_t BitReader::GetByte(size_t pos) {
    if (pos >= dataLen) {
        return 0;
    }
    return data[pos];
}

// advance position in the bit stream
// returns false if we've eaten bits more than we have
bool BitReader::Eat(size_t bitsCount) {
    currBitPos += bitsCount;
    return (currBitPos <= bitsCount);
}

size_t BitReader::BitsLeft() {
    if (currBitPos < bitsCount) {
        return bitsCount - currBitPos;
    }
    return 0;
}

// Read bitsCount (up to 32) bits, without advancing the position in the bit stream
// If asked for more bits than we have left, the extra bits will be 0
uint32_t BitReader::Peek(size_t bitsCount) {
    CrashIf((bitsCount == 0) || (bitsCount > 32));
    size_t currBytePos = currBitPos / 8;
    uint8_t currByte = GetByte(currBytePos);
    uint8_t currBit = currBitPos % 8;
    currByte = currByte << currBit;
    uint8_t bitsLeft = 8 - currBit;
    uint32_t ret = 0;
    while (bitsCount > 0) {
        CrashIf(bitsLeft < 0);
        if (0 == bitsLeft) {
            ++currBytePos;
            currByte = GetByte(currBytePos);
            bitsLeft = 8;
        }
        // being conservative here, could probably handle
        // bitsLeft other than 8
        if ((8 == bitsLeft) && (bitsCount >= 8)) {
            // fast path - 8 bits at a time
            ret = (ret << 8) | currByte;
            bitsLeft = 0;
            bitsCount -= 8;
        } else {
            // slow path - 1 bit at a time
            ret = ret << 1;
            if ((0x80 & currByte) != 0) {
                ret |= 1;
            }
            currByte = currByte << 1;
            bitsLeft--;
            bitsCount--;
        }
    }
    return ret;
}
