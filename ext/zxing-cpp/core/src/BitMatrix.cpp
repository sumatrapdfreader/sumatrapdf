/*
* Copyright 2016 Nu-book Inc.
* Copyright 2016 ZXing authors
*/
// SPDX-License-Identifier: Apache-2.0

#include "BitMatrix.h"

#include "Pattern.h"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace ZXing {

void
BitMatrix::setRegion(int left, int top, int width, int height)
{
	if (top < 0 || left < 0) {
		throw std::invalid_argument("BitMatrix::setRegion(): Left and top must be nonnegative");
	}
	if (height < 1 || width < 1) {
		throw std::invalid_argument("BitMatrix::setRegion(): Height and width must be at least 1");
	}
	int right = left + width;
	int bottom = top + height;
	if (bottom > _height || right > _width) {
		throw std::invalid_argument("BitMatrix::setRegion(): The region must fit inside the matrix");
	}
	for (int y = top; y < bottom; y++) {
		auto offset = y * _width;
		for (int x = left; x < right; x++) {
			_bits[offset + x] = SET_V;
		}
	}
}

void
BitMatrix::rotate90()
{
	BitMatrix result(height(), width());
	for (int x = 0; x < width(); ++x) {
		for (int y = 0; y < height(); ++y) {
			if (get(x, y)) {
				result.set(y, width() - x - 1);
			}
		}
	}
	*this = std::move(result);
}

void
BitMatrix::rotate180()
{
	std::reverse(_bits.begin(), _bits.end());
}

void
BitMatrix::mirror()
{
	for (int x = 0; x < _width; x++) {
		for (int y = x + 1; y < _height; y++) {
			if (get(x, y) != get(y, x)) {
				flip(y, x);
				flip(x, y);
			}
		}
	}
}

bool
BitMatrix::findBoundingBox(int &left, int& top, int& width, int& height, int minSize) const
{
	int right, bottom;
	if (!getTopLeftOnBit(left, top) || !getBottomRightOnBit(right, bottom) || bottom - top + 1 < minSize)
		return false;

	for (int y = top; y <= bottom; y++ ) {
		for (int x = 0; x < left; ++x)
			if (get(x, y)) {
				left = x;
				break;
			}
		for (int x = _width-1; x > right; x--)
			if (get(x, y)) {
				right = x;
				break;
			}
	}

	width = right - left + 1;
	height = bottom - top + 1;
	return width >= minSize && height >= minSize;
}

static auto isSet = [](auto v) { return bool(v); };

bool
BitMatrix::getTopLeftOnBit(int& left, int& top) const
{
	int bitsOffset = (int)std::distance(_bits.begin(), std::find_if(_bits.begin(), _bits.end(), isSet));
	if (bitsOffset == Size(_bits)) {
		return false;
	}
	top = bitsOffset / _width;
	left = (bitsOffset % _width);
	return true;
}

bool
BitMatrix::getBottomRightOnBit(int& right, int& bottom) const
{
	int bitsOffset = Size(_bits) - 1 - (int)std::distance(_bits.rbegin(), std::find_if(_bits.rbegin(), _bits.rend(), isSet));
	if (bitsOffset < 0) {
		return false;
	}

	bottom = bitsOffset / _width;
	right = (bitsOffset % _width);
	return true;
}

void GetPatternRow(const BitMatrix& matrix, int r, std::vector<uint16_t>& pr, bool transpose)
{
	if (transpose)
		GetPatternRow(matrix.col(r), pr);
	else
		GetPatternRow(matrix.row(r), pr);
}

BitMatrix Inflate(BitMatrix&& input, int width, int height, int quietZone)
{
	const int codeWidth = input.width();
	const int codeHeight = input.height();
	const int outputWidth = std::max(width, codeWidth + 2 * quietZone);
	const int outputHeight = std::max(height, codeHeight + 2 * quietZone);

	if (input.width() == outputWidth && input.height() == outputHeight)
		return std::move(input);

	const int scale = std::min((outputWidth - 2*quietZone) / codeWidth, (outputHeight - 2*quietZone) / codeHeight);
	// Padding includes both the quiet zone and the extra white pixels to
	// accommodate the requested dimensions.
	const int leftPadding = (outputWidth - (codeWidth * scale)) / 2;
	const int topPadding = (outputHeight - (codeHeight * scale)) / 2;

	BitMatrix result(outputWidth, outputHeight);

	for (int inputY = 0, outputY = topPadding; inputY < input.height(); ++inputY, outputY += scale) {
		for (int inputX = 0, outputX = leftPadding; inputX < input.width(); ++inputX, outputX += scale) {
			if (input.get(inputX, inputY))
				result.setRegion(outputX, outputY, scale, scale);
		}
	}

	return result;
}

BitMatrix Deflate(const BitMatrix& input, int width, int height, float top, float left, float subSampling)
{
	BitMatrix result(width, height);

	for (int y = 0; y < result.height(); y++) {
		auto yOffset = top + y * subSampling;
		for (int x = 0; x < result.width(); x++) {
			if (input.get(PointF(left + x * subSampling, yOffset)))
				result.set(x, y);
		}
	}

	return result;
}

} // ZXing
