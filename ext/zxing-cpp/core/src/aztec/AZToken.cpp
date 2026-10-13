/*
* Copyright 2016 Huy Cuong Nguyen
* Copyright 2016 ZXing authors
*/
// SPDX-License-Identifier: Apache-2.0

#include "AZToken.h"

#include "BitArray.h"

#include <algorithm>

namespace ZXing::Aztec {

void
Token::appendTo(BitArray& bitArray, const std::string& text) const
{
	if (_count < 0) {
		bitArray.appendBits(_value, -_count);
	}
	else {
		for (int i = 0; i < _count; i++) {
			if (i == 0 || (i == 31 && _count <= 62)) {
				// We need a header before the first character, and before
				// character 31 when the total byte code is <= 62
				bitArray.appendBits(31, 5);  // BINARY_SHIFT
				if (_count > 62) {
					bitArray.appendBits(_count - 31, 16);
				}
				else if (i == 0) {
					// 1 <= binaryShiftByteCode <= 62
					bitArray.appendBits(std::min((int)_count, 31), 5);
				}
				else {
					// 32 <= binaryShiftCount <= 62 and i == 31
					bitArray.appendBits(_count - 31, 5);
				}
			}
			bitArray.appendBits(text[_value + i], 8);
		}
	}
}

} // namespace ZXing::Aztec
