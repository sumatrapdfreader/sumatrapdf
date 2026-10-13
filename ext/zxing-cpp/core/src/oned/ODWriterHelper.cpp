/*
* Copyright 2016 Huy Cuong Nguyen
* Copyright 2016 ZXing authors
*/
// SPDX-License-Identifier: Apache-2.0

#include "ODWriterHelper.h"

#include "BitMatrix.h"

#include <algorithm>

namespace ZXing::OneD {

/**
* @return a byte array of horizontal pixels (0 = white, 1 = black)
*/
BitMatrix
WriterHelper::RenderResult(const std::vector<bool>& code, int width, int height, int sidesMargin)
{
	int inputWidth = Size(code);
	// Add quiet zone on both sides.
	int fullWidth = inputWidth + sidesMargin;
	int outputWidth = std::max(width, fullWidth);
	int outputHeight = std::max(1, height);

	int multiple = outputWidth / fullWidth;
	int leftPadding = (outputWidth - (inputWidth * multiple)) / 2;

	BitMatrix result(outputWidth, outputHeight);
	for (int inputX = 0, outputX = leftPadding; inputX < inputWidth; inputX++, outputX += multiple) {
		if (code[inputX]) {
			result.setRegion(outputX, 0, multiple, outputHeight);
		}
	}
	return result;
}

/**
* @param target encode black/white pattern into this array
* @param pos position to start encoding at in {@code target}
* @param pattern lengths of black/white runs to encode
* @param startColor starting color - false for white, true for black
* @return the number of elements added to target.
*/
int
WriterHelper::AppendPattern(std::vector<bool>& target, int pos, const int* pattern, size_t patternCount, bool startColor)
{
	bool color = startColor;
	int numAdded = 0;
	for (size_t i = 0; i < patternCount; ++i) {
		int s = pattern[i];
		for (int j = 0; j < s; j++) {
			target[pos++] = color;
		}
		numAdded += s;
		color = !color; // flip color after each segment
	}
	return numAdded;
}

} // namespace ZXing::OneD
