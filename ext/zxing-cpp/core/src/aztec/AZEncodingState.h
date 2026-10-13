/*
* Copyright 2016 Huy Cuong Nguyen
* Copyright 2016 ZXing authors
*/
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "AZToken.h"

#include <vector>

namespace ZXing::Aztec {

class Token;

/**
* State represents all information about a sequence necessary to generate the current output.
* Note that a state is immutable.
*/
class EncodingState
{
public:
	// The list of tokens that we output.  If we are in Binary Shift mode, this
	// token list does *not* yet included the token for those bytes
	std::vector<Token> tokens;

	// The current mode of the encoding (or the mode to which we'll return if
	// we're in Binary Shift mode.
	int mode = 0;
	
	// If non-zero, the number of most recent bytes that should be output
	// in Binary Shift mode.
	int binaryShiftByteCount = 0;
	
	// The total number of bits generated (including Binary Shift).
	int bitCount = 0;
};

} // namespace ZXing::Aztec
