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

/**
* This produces nearly optimal encodings of text into the first-level of
* encoding used by Aztec code.
*
* It uses a dynamic algorithm.  For each prefix of the string, it determines
* a set of encodings that could lead to this prefix.  We repeatedly add a
* character and generate a new set of optimal encodings until we have read
* through the entire input.
*
* @author Frank Yellin
* @author Rustam Abdullaev
*/
class HighLevelEncoder
{
public:
	static BitArray Encode(const std::string& text);
};

} // Aztec
} // ZXing
