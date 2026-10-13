/*
* Copyright 2016 Huy Cuong Nguyen
* Copyright 2016 ZXing authors
*/
// SPDX-License-Identifier: Apache-2.0

#include "QRMaskUtil.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdlib>
#include <utility>

namespace ZXing::QRCode::MaskUtil {

// Penalty weights from section 6.8.2.1
static const int N1 = 3;
static const int N2 = 3;
static const int N3 = 40;
static const int N4 = 10;

/**
* Helper function for applyMaskPenaltyRule1. We need this for doing this calculation in both
* vertical and horizontal orders respectively.
*/
static int ApplyMaskPenaltyRule1Internal(const TritMatrix& matrix, bool isHorizontal)
{
	int penalty = 0;
	int width = matrix.width();
	int height = matrix.height();
	int iLimit = isHorizontal ? height : width;
	int jLimit = isHorizontal ? width : height;
	for (int i = 0; i < iLimit; i++) {
		int numSameBitCells = 0;
		int prevBit = -1;
		for (int j = 0; j < jLimit; j++) {
			int bit = isHorizontal ? matrix.get(j, i) : matrix.get(i, j);
			if (bit == prevBit) {
				numSameBitCells++;
			}
			else {
				if (numSameBitCells >= 5) {
					penalty += N1 + (numSameBitCells - 5);
				}
				numSameBitCells = 1;  // Include the cell itself.
				prevBit = bit;
			}
		}
		if (numSameBitCells >= 5) {
			penalty += N1 + (numSameBitCells - 5);
		}
	}
	return penalty;
}

/**
* Apply mask penalty rule 1 and return the penalty. Find repetitive cells with the same color and
* give penalty to them. Example: 00000 or 11111.
*/
static int ApplyMaskPenaltyRule1(const TritMatrix& matrix)
{
	return ApplyMaskPenaltyRule1Internal(matrix, true) + ApplyMaskPenaltyRule1Internal(matrix, false);
}

/**
* Apply mask penalty rule 2 and return the penalty. Find 2x2 blocks with the same color and give
* penalty to them. This is actually equivalent to the spec's rule, which is to find MxN blocks and give a
* penalty proportional to (M-1)x(N-1), because this is the number of 2x2 blocks inside such a block.
*/
static int ApplyMaskPenaltyRule2(const TritMatrix& matrix)
{
	int penalty = 0;
	for (int y = 0; y < matrix.height() - 1; y++) {
		for (int x = 0; x < matrix.width() - 1; x++) {
			auto value = matrix.get(x, y);
			if (value == matrix.get(x+1, y) && value == matrix.get(x, y+1) && value == matrix.get(x+1, y+1)) {
				penalty++;
			}
		}
	}
	return N2 * penalty;
}

template <size_t N>
static bool HasPatternAt(const std::array<bool, N>& pattern, const Trit* begin, int count, int stride)
{
	assert(std::abs(count) <= (int)N);
	auto end = begin + count * stride;
	if (count < 0)
		std::swap(begin, end);
	auto a = begin;
	for (auto b = pattern.begin(); a < end && b != pattern.end(); a += stride, ++b)
		if (*a != *b)
			return false;
	return true;
}

/**
* Apply mask penalty rule 3 and return the penalty. Find consecutive runs of 1:1:3:1:1:4
* starting with black, or 4:1:1:3:1:1 starting with white, and give penalty to them.  If we
* find patterns like 000010111010000, we give penalty once.
*/
static int ApplyMaskPenaltyRule3(const TritMatrix& matrix)
{
	const std::array<bool, 4> white = {0, 0, 0, 0};
	const std::array<bool, 7> finder = {1, 0, 1, 1, 1, 0, 1};
	const int whiteSize = Size(white);
	const int finderSize = Size(finder);

	int numPenalties = 0;
	int width = matrix.width();
	int height = matrix.height();
	for (int y = 0; y < height; y++) {
		for (int x = 0; x < width; x++) {
			auto i = &matrix.get(x, y);
			if (x <= width - finderSize && HasPatternAt(finder, i, finderSize, 1)
				&& (HasPatternAt(white, i, -std::min(x, whiteSize), 1)
					|| HasPatternAt(white, i + finderSize, std::min(width - x - finderSize, whiteSize), 1))) {
				numPenalties++;
			}
			if (y <= height - finderSize && HasPatternAt(finder, i, finderSize, width)
				&& (HasPatternAt(white, i, -std::min(y, whiteSize), width)
					|| HasPatternAt(white, i + finderSize * width, std::min(height - y - finderSize, whiteSize), width))) {
				numPenalties++;
			}
		}
	}
	return numPenalties * N3;
}

/**
* Apply mask penalty rule 4 and return the penalty. Calculate the ratio of dark cells and give
* penalty if the ratio is far from 50%. It gives 10 penalty for 5% distance.
*/
static int ApplyMaskPenaltyRule4(const TritMatrix& matrix)
{
	auto numDarkCells = std::count_if(matrix.begin(), matrix.end(), [](Trit cell) { return cell; });
	auto numTotalCells = matrix.size();
	auto fivePercentVariances = std::abs(numDarkCells * 2 - numTotalCells) * 10 / numTotalCells;
	return narrow_cast<int>(fivePercentVariances * N4);
}

// The mask penalty calculation is complicated.  See Table 21 of JISX0510:2004 (p.45) for details.
// Basically it applies four rules and summate all penalties.
int CalculateMaskPenalty(const TritMatrix& matrix)
{
	return MaskUtil::ApplyMaskPenaltyRule1(matrix)
		   + MaskUtil::ApplyMaskPenaltyRule2(matrix)
		   + MaskUtil::ApplyMaskPenaltyRule3(matrix)
		   + MaskUtil::ApplyMaskPenaltyRule4(matrix);
}

} // namespace ZXing::QRCode::MaskUtil
