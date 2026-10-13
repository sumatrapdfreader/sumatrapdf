/*
* Copyright 2016 Nu-book Inc.
* Copyright 2016 ZXing authors
* Copyright 2017 Axel Waggershauser
*/
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "Range.h"
#include "ZXAlgorithms.h"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <stdexcept>
#include <type_traits>
#include <vector>

namespace ZXing {

class ByteArray;

/**
* A simple, fast array of bits.
*/
class BitArray
{
	std::vector<uint8_t> _bits;

	friend class BitMatrix;

	// Nothing wrong to support it, just to make it explicit, instead of by mistake.
	// Use copy() below.
	BitArray(const BitArray &) = default;
	BitArray& operator=(const BitArray &) = delete;

public:

	using Iterator = std::vector<uint8_t>::const_iterator;

	BitArray() = default;

	explicit BitArray(int size) : _bits(size, 0) {}

	BitArray(BitArray&& other) noexcept = default;
	BitArray& operator=(BitArray&& other) noexcept = default;

	BitArray copy() const { return *this; }

	int size() const noexcept { return Size(_bits); }

	int sizeInBytes() const noexcept { return (size() + 7) / 8; }

	bool get(int i) const { return _bits.at(i) != 0; }
	void set(int i, bool val) { _bits.at(i) = val; }

	// If you know exactly how may bits you are going to iterate
	// and that you access bit in sequence, iterator is faster than get().
	// However, be extremely careful since there is no check whatsoever.
	// (Performance is the reason for the iterator to exist in the first place.)
	Iterator iterAt(int i) const noexcept { return {_bits.cbegin() + i}; }
	Iterator begin() const noexcept { return _bits.cbegin(); }
	Iterator end() const noexcept { return _bits.cend(); }

	/**
	* Appends the least-significant bits, from value, in order from most-significant to
	* least-significant. For example, appending 6 bits from 0x000001E will append the bits
	* 0, 1, 1, 1, 1, 0 in that order.
	*
	* @param value {@code int} containing bits to append
	* @param numBits bits from value to append
	*/
	void appendBits(int value, int numBits)
	{
		for (; numBits; --numBits)
			_bits.push_back((value >> (numBits-1)) & 1);
	}

	void appendBit(bool bit) { _bits.push_back(bit); }

	void appendBitArray(const BitArray& other) { _bits.insert(_bits.end(), other.begin(), other.end()); }

	/**
	* Reverses all bits in the array.
	*/
	void reverse() { std::reverse(_bits.begin(), _bits.end()); }

	void bitwiseXOR(const BitArray& other);

	/**
	* @param bitOffset first bit to extract
	* @param numBytes how many bytes to extract (-1 == until the end, padded with '0')
	* @return Bytes are written most-significant bit first.
	*/
	ByteArray toBytes(int bitOffset = 0, int numBytes = -1) const;

	using Range = ZXing::Range<Iterator>;
	Range range() const { return {begin(), end()}; }

	friend bool operator==(const BitArray& a, const BitArray& b) { return a._bits == b._bits; }
};

template<typename T, typename = std::enable_if_t<std::is_integral_v<T>>>
T& AppendBit(T& val, bool bit)
{
	return (val <<= 1) |= static_cast<T>(bit);
}

template <typename ARRAY, typename = std::enable_if_t<std::is_integral_v<typename ARRAY::value_type>>>
int ToInt(const ARRAY& a)
{
	assert(Reduce(a) <= 32);

	int pattern = 0;
	for (int i = 0; i < Size(a); i++)
		pattern = (pattern << a[i]) | ~(0xffffffff << a[i]) * (~i & 1);
	return pattern;
}

template <typename T = int, typename = std::enable_if_t<std::is_integral_v<T>>>
T ToInt(const BitArray& bits, int pos = 0, int count = 8 * sizeof(T))
{
	assert(0 <= count && count <= 8 * (int)sizeof(T));
	assert(0 <= pos && pos + count <= bits.size());

	count = std::min(count, bits.size());
	int res = 0;
	auto it = bits.iterAt(pos);
	for (int i = 0; i < count; ++i, ++it)
		AppendBit(res, *it);

	return res;
}

template <typename T = int, typename = std::enable_if_t<std::is_integral_v<T>>>
std::vector<T> ToInts(const BitArray& bits, int wordSize, int totalWords, int offset = 0)
{
	assert(totalWords >= bits.size() / wordSize);
	assert(wordSize <= 8 * (int)sizeof(T));

	std::vector<T> res(totalWords, 0);
	for (int i = offset; i < bits.size(); i += wordSize)
		res[(i - offset) / wordSize] = ToInt(bits, i, wordSize);

	return res;
}

class BitArrayView
{
	const BitArray& bits;
	BitArray::Iterator cur;

public:
	BitArrayView(const BitArray& bits) : bits(bits), cur(bits.begin()) {}

	BitArrayView& skipBits(int n)
	{
		if (cur + n > bits.end())
			throw std::out_of_range("BitArrayView::skipBits() out of range.");
		cur += n;
		return *this;
	}

	int peakBits(int n) const
	{
		assert(n <= 32);
		if (cur + n > bits.end())
			throw std::out_of_range("BitArrayView::peakBits() out of range.");
		int res = 0;
		for (auto i = cur; n > 0; --n, i++)
			AppendBit(res, *i);
		return res;
	}

	int readBits(int n)
	{
		int res = peakBits(n);
		cur += n;
		return res;
	}

	int size() const
	{
		return narrow_cast<int>(bits.end() - cur);
	}

	explicit operator bool() const { return size(); }
};

} // ZXing
