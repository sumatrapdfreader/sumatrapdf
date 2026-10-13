/*
* Copyright 2016 Nu-book Inc.
* Copyright 2016 ZXing authors
*/
// SPDX-License-Identifier: Apache-2.0

#include "BitArray.h"

#include "ByteArray.h"

#include <cstddef>
#include <stdexcept>

namespace ZXing {

void
BitArray::bitwiseXOR(const BitArray& other)
{
	if (size() != other.size()) {
		throw std::invalid_argument("BitArray::xor(): Sizes don't match");
	}
	for (size_t i = 0; i < _bits.size(); i++) {
		// The last int could be incomplete (i.e. not have 32 bits in
		// it) but there is no problem since 0 XOR 0 == 0.
		_bits[i] ^= other._bits[i];
	}
}

ByteArray BitArray::toBytes(int bitOffset, int numBytes) const
{
	ByteArray res(numBytes == -1 ? (size() - bitOffset + 7) / 8 : numBytes);
	for (int i = 0; i < Size(res); i++)
		for (int j = 0; j < 8; j++)
			AppendBit(res[i], (numBytes != -1 || bitOffset < size()) ? get(bitOffset++) : 0);
	return res;
}

} // ZXing
