/*
* Copyright 2016 Huy Cuong Nguyen
* Copyright 2016 ZXing authors
*/
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <string>

namespace ZXing {

class BitArray;

namespace Aztec {

class Token
{
public:
	void appendTo(BitArray& bitArray, const std::string& text) const;

	static Token CreateSimple(int value, int bitCount) {
		return {value, -bitCount};
	}
	
	static Token CreateBinaryShift(int start, int byteCount) {
		return {start, byteCount};
	}

private:
	short _value;
	short _count;	// is simple token if negative, 
	
public:
	Token(int value, int count) : _value((short)value), _count((short)count) {}
};

} // Aztec
} // ZXing
