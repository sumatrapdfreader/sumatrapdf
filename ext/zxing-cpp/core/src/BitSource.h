/*
* Copyright 2016 Nu-book Inc.
* Copyright 2016 ZXing authors
*/
// SPDX-License-Identifier: Apache-2.0

#pragma once

namespace ZXing {

class ByteArray;

/**
* <p>This provides an easy abstraction to read bits at a time from a sequence of bytes, where the
* number of bits read is not often a multiple of 8.</p>
*
* <p>This class is thread-safe but not reentrant -- unless the caller modifies the bytes array
* it passed in, in which case all bets are off.</p>
*
* @author Sean Owen
*/
class BitSource
{
	const ByteArray& _bytes;
	int _byteOffset = 0;
	int _bitOffset = 0;

public:
	/**
	* @param bytes bytes from which this will read bits. Bits will be read from the first byte first.
	* Bits are read within a byte from most-significant to least-significant bit.
	* IMPORTANT: Bit source DOES NOT copy data byte, thus make sure that the bytes outlive the bit source object.
	*/
	explicit BitSource(const ByteArray& bytes) : _bytes(bytes) {}
	
	BitSource(BitSource &) = delete;
	BitSource& operator=(const BitSource &) = delete;

	/**
	* @return index of next bit in current byte which would be read by the next call to {@link #readBits(int)}.
	*/
	int bitOffset() const {
		return _bitOffset;
	}

	/**
	* @return index of next byte in input byte array which would be read by the next call to {@link #readBits(int)}.
	*/
	int byteOffset() const {
		return _byteOffset;
	}

	/**
	* @param numBits number of bits to read
	* @return int representing the bits read. The bits will appear as the least-significant bits of the int
	*/
	int readBits(int numBits);

	/**
	* @param numBits number of bits to peak
	* @return int representing the bits peaked.  The bits will appear as the least-significant
	*         bits of the int
	*/
	int peakBits(int numBits) const;

	/**
	* @return number of bits that can be read successfully
	*/
	int available() const;
};

} // ZXing
