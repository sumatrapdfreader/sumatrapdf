/*
* Copyright 2016 Huy Cuong Nguyen
* Copyright 2016 ZXing authors
*/
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "BitMatrix.h"

#include <cstddef>
#include <vector>

namespace ZXing::OneD {

/**
* <p>Encapsulates functionality and implementation that is common to one-dimensional barcodes.</p>
*
* @author dsbnatut@gmail.com (Kazuki Nishiura)
*/
class WriterHelper
{
	static int AppendPattern(std::vector<bool>& target, int pos, const int* pattern, size_t patternCount, bool startColor);
public:
	/**
	* @return a byte array of horizontal pixels (0 = white, 1 = black)
	*/
	static BitMatrix RenderResult(const std::vector<bool>& code, int width, int height, int sidesMargin);

	/**
	* @param target encode black/white pattern into this array
	* @param pos position to start encoding at in {@code target}
	* @param pattern lengths of black/white runs to encode
	* @param startColor starting color - false for white, true for black
	* @return the number of elements added to target.
	*/
	template <typename Container>
	static int AppendPattern(std::vector<bool>& target, int pos, const Container& pattern, bool startColor) {
		return AppendPattern(target, pos, pattern.data(), pattern.size(), startColor);
	}
};

} // namespace ZXing::OneD
