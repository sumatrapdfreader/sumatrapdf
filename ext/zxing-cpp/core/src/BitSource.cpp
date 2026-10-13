/*
* Copyright 2016 Nu-book Inc.
* Copyright 2016 ZXing authors
*/
// SPDX-License-Identifier: Apache-2.0

#include "BitSource.h"

#include "ByteArray.h"
#include "ZXAlgorithms.h"

#include <stdexcept>

namespace ZXing {

int
BitSource::available() const
{
	return 8 * (Size(_bytes) - _byteOffset) - _bitOffset;
}

static int ReadBitsImpl(int numBits, const ByteArray& _bytes, int available, int& _byteOffset, int& _bitOffset)
{
	if (numBits < 1 || numBits > 32 || numBits > available) {
		throw std::out_of_range("BitSource::readBits: out of range");
	}

	int result = 0;

	// First, read remainder from current byte
	if (_bitOffset > 0) {
		int bitsLeft = 8 - _bitOffset;
		int toRead = numBits < bitsLeft ? numBits : bitsLeft;
		int bitsToNotRead = bitsLeft - toRead;
		int mask = (0xFF >> (8 - toRead)) << bitsToNotRead;
		result = (_bytes[_byteOffset] & mask) >> bitsToNotRead;
		numBits -= toRead;
		_bitOffset += toRead;
		if (_bitOffset == 8) {
			_bitOffset = 0;
			_byteOffset++;
		}
	}

	// Next read whole bytes
	if (numBits > 0) {
		while (numBits >= 8) {
			result = (result << 8) | _bytes[_byteOffset];
			_byteOffset++;
			numBits -= 8;
		}

		// Finally read a partial byte
		if (numBits > 0) {
			int bitsToNotRead = 8 - numBits;
			int mask = (0xFF >> bitsToNotRead) << bitsToNotRead;
			result = (result << numBits) | ((_bytes[_byteOffset] & mask) >> bitsToNotRead);
			_bitOffset += numBits;
		}
	}

	return result;
}

int BitSource::readBits(int numBits)
{
	return ReadBitsImpl(numBits, _bytes, available(), _byteOffset, _bitOffset);
}

int BitSource::peakBits(int numBits) const
{
	int bitOffset = _bitOffset;
	int byteOffset = _byteOffset;
	return ReadBitsImpl(numBits, _bytes, available(), byteOffset, bitOffset);
}

} // ZXing
