/*
* Copyright 2016 Nu-book Inc.
* Copyright 2016 ZXing authors
*/
// SPDX-License-Identifier: Apache-2.0

#include "HybridBinarizer.h"

#include "BitMatrix.h"
#include "Matrix.h"

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <memory>

#define USE_NEW_ALGORITHM

namespace ZXing {

// This class uses 5x5 blocks to compute local luminance, where each block is 8x8 pixels.
// So this is the smallest dimension in each axis we can accept.
static constexpr int BLOCK_SIZE = 8;
static constexpr int WINDOW_SIZE = BLOCK_SIZE * (1 + 2 * 2);
static constexpr int MIN_DYNAMIC_RANGE = 24;

HybridBinarizer::HybridBinarizer(const ImageView& iv) : GlobalHistogramBinarizer(iv) {}

HybridBinarizer::~HybridBinarizer() = default;

bool HybridBinarizer::getPatternRow(int row, int rotation, PatternRow& res) const
{
#if 1
	// This is the original "hybrid" behavior: use GlobalHistogram for the 1D case
	return GlobalHistogramBinarizer::getPatternRow(row, rotation, res);
#else
	// This is an alternative that can be faster in general and perform better in unevenly lit sitations like
	// https://github.com/zxing-cpp/zxing-cpp/blob/master/test/samples/ean13-2/21.png. That said, it fairs
	// worse in borderline low resolution situations. With the current black box sample set we'd loose 94
	// test cases while gaining 53 others.
	auto bits = getBitMatrix();
	if (bits)
		GetPatternRow(*bits, row, res, rotation % 180 != 0);
	return bits != nullptr;
#endif
}

using T_t = uint8_t;

/**
* Applies a single threshold to a block of pixels.
*/
static void ThresholdBlock(const uint8_t* __restrict luminances, int xoffset, int yoffset, T_t threshold, int rowStride,
						   BitMatrix& matrix)
{
	for (int y = yoffset; y < yoffset + BLOCK_SIZE; ++y) {
		auto* src = luminances + y * rowStride + xoffset;
		auto* const dstBegin = matrix.row(y).begin() + xoffset;
		// TODO: fix pixelStride > 1 case
		for (auto* dst = dstBegin; dst < dstBegin + BLOCK_SIZE; ++dst, ++src)
			*dst = (*src <= threshold) * BitMatrix::SET_V;
	}
}

#ifndef USE_NEW_ALGORITHM

/**
* Calculates a single black point for each block of pixels and saves it away.
* See the following thread for a discussion of this algorithm:
*  http://groups.google.com/group/zxing/browse_thread/thread/d06efa2c35a7ddc0
*/
static Matrix<T_t> CalculateBlackPoints(const uint8_t* __restrict luminances, int subWidth, int subHeight, int width, int height,
										int rowStride)
{
	Matrix<T_t> blackPoints(subWidth, subHeight);

	for (int y = 0; y < subHeight; y++) {
		int yoffset = std::min(y * BLOCK_SIZE, height - BLOCK_SIZE);
		for (int x = 0; x < subWidth; x++) {
			int xoffset = std::min(x * BLOCK_SIZE, width - BLOCK_SIZE);
			int sum = 0;
			uint8_t min = luminances[yoffset * rowStride + xoffset];
			uint8_t max = min;
			for (int yy = 0, offset = yoffset * rowStride + xoffset; yy < BLOCK_SIZE; yy++, offset += rowStride) {
				for (int xx = 0; xx < BLOCK_SIZE; xx++) {
					auto pixel = luminances[offset + xx];
					sum += pixel;
					if (pixel < min)
						min = pixel;
					if (pixel > max)
						max = pixel;
				}
				// short-circuit min/max tests once dynamic range is met
				if (max - min > MIN_DYNAMIC_RANGE) {
					// finish the rest of the rows quickly
					for (yy++, offset += rowStride; yy < BLOCK_SIZE; yy++, offset += rowStride) {
						for (int xx = 0; xx < BLOCK_SIZE; xx++) {
							sum += luminances[offset + xx];
						}
					}
				}
			}

			// The default estimate is the average of the values in the block.
			int average = sum / (BLOCK_SIZE * BLOCK_SIZE);
			if (max - min <= MIN_DYNAMIC_RANGE) {
				// If variation within the block is low, assume this is a block with only light or only
				// dark pixels. In that case we do not want to use the average, as it would divide this
				// low contrast area into black and white pixels, essentially creating data out of noise.
				//
				// The default assumption is that the block is light/background. Since no estimate for
				// the level of dark pixels exists locally, use half the min for the block.
				average = min / 2;

				if (y > 0 && x > 0) {
					// Correct the "white background" assumption for blocks that have neighbors by comparing
					// the pixels in this block to the previously calculated black points. This is based on
					// the fact that dark barcode symbology is always surrounded by some amount of light
					// background for which reasonable black point estimates were made. The bp estimated at
					// the boundaries is used for the interior.

					// The (min < bp) is arbitrary but works better than other heuristics that were tried.
					int averageNeighborBlackPoint =
						(blackPoints(x, y - 1) + (2 * blackPoints(x - 1, y)) + blackPoints(x - 1, y - 1)) / 4;
					if (min < averageNeighborBlackPoint) {
						average = averageNeighborBlackPoint;
					}
				}
			}
			blackPoints(x, y) = average;
		}
	}
	return blackPoints;
}

/**
* For each block in the image, calculate the average black point using a 5x5 grid
* of the blocks around it. Also handles the corner cases (fractional blocks are computed based
* on the last pixels in the row/column which are also used in the previous block).
*/
static std::shared_ptr<BitMatrix> CalculateMatrix(const uint8_t* __restrict luminances, int subWidth, int subHeight, int width,
												  int height, int rowStride, const Matrix<T_t>& blackPoints)
{
	auto matrix = std::make_shared<BitMatrix>(width, height);

#ifdef PRINT_DEBUG
	Matrix<uint8_t> out(width, height);
	Matrix<uint8_t> out2(width, height);
#endif

	for (int y = 0; y < subHeight; y++) {
		int yoffset = std::min(y * BLOCK_SIZE, height - BLOCK_SIZE);
		for (int x = 0; x < subWidth; x++) {
			int xoffset = std::min(x * BLOCK_SIZE, width - BLOCK_SIZE);
			int left = std::clamp(x, 2, subWidth - 3);
			int top = std::clamp(y, 2, subHeight - 3);
			int sum = 0;
			for (int dy = -2; dy <= 2; ++dy) {
				for (int dx = -2; dx <= 2; ++dx) {
					sum += blackPoints(left + dx, top + dy);
				}
			}
			int average = sum / 25;
			ThresholdBlock(luminances, xoffset, yoffset, average, rowStride, *matrix);

#ifdef PRINT_DEBUG
			for (int yy = 0; yy < 8; ++yy)
				for (int xx = 0; xx < 8; ++xx) {
					out.set(xoffset + xx, yoffset + yy, blackPoints(x, y));
					out2.set(xoffset + xx, yoffset + yy, average);
				}
#endif
		}
	}

#ifdef PRINT_DEBUG
	std::ofstream file("thresholds.pnm");
	file << "P5\n" << out.width() << ' ' << out.height() << "\n255\n";
	file.write(reinterpret_cast<const char*>(out.data()), out.size());
	std::ofstream file2("thresholds_avg.pnm");
	file2 << "P5\n" << out.width() << ' ' << out.height() << "\n255\n";
	file2.write(reinterpret_cast<const char*>(out2.data()), out2.size());
#endif

	return matrix;
}

#else

// Subdivide the image in blocks of BLOCK_SIZE and calculate one treshold value per block as
// (max - min > MIN_DYNAMIC_RANGE) ? (max + min) / 2 : 0
static Matrix<T_t> BlockThresholds(const ImageView iv)
{
	int subWidth = (iv.width() + BLOCK_SIZE - 1) / BLOCK_SIZE; // ceil(width/BS)
	int subHeight = (iv.height() + BLOCK_SIZE - 1) / BLOCK_SIZE; // ceil(height/BS)

	Matrix<T_t> thresholds(subWidth, subHeight);

	for (int y = 0; y < subHeight; y++) {
		int y0 = std::min(y * BLOCK_SIZE, iv.height() - BLOCK_SIZE);
		for (int x = 0; x < subWidth; x++) {
			int x0 = std::min(x * BLOCK_SIZE, iv.width() - BLOCK_SIZE);
			uint8_t min = 255;
			uint8_t max = 0;
			for (int yy = 0; yy < BLOCK_SIZE; yy++) {
				auto line = iv.data(x0, y0 + yy);
				for (int xx = 0; xx < BLOCK_SIZE; xx++)
					UpdateMinMax(min, max, line[xx]);
			}

			thresholds(x, y) = (max - min > MIN_DYNAMIC_RANGE) ? (int(max) + min) / 2 : 0;
		}
	}

	return thresholds;
}

// Apply gaussian-like smoothing filter over all non-zero thresholds and fill any remainig gaps with nearest neighbor
static Matrix<T_t> SmoothThresholds(Matrix<T_t>&& in)
{
	Matrix<T_t> out(in.width(), in.height());

	constexpr int R = WINDOW_SIZE / BLOCK_SIZE / 2;
	for (int y = 0; y < in.height(); y++) {
		for (int x = 0; x < in.width(); x++) {
			int left = std::clamp(x, R, in.width() - R - 1);
			int top = std::clamp(y, R, in.height() - R - 1);

			int sum = in(x, y) * 2;
			int n = (sum > 0) * 2;
			auto add = [&](int x, int y) {
				int t = in(x, y);
				sum += t;
				n += t > 0;
			};

			for (int dy = -R; dy <= R; ++dy)
				for (int dx = -R; dx <= R; ++dx)
					add(left + dx, top + dy);

			out(x, y) = n > 0 ? sum / n : 0;
		}
	}

	// flood fill any remaing gaps of (very large) no-contrast regions
	auto last = out.begin() - 1;
	for (auto* i = out.begin(); i != out.end(); ++i) {
		if (*i) {
			if (last != i - 1)
				std::fill(last + 1, i, *i);
			last = i;
		}
	}
	std::fill(last + 1, out.end(), *(std::max(last, out.begin())));

	return out;
}

static std::shared_ptr<BitMatrix> ThresholdImage(const ImageView iv, const Matrix<T_t>& thresholds)
{
	auto matrix = std::make_shared<BitMatrix>(iv.width(), iv.height());

#ifdef PRINT_DEBUG
	Matrix<uint8_t> out(iv.width(), iv.height());
#endif

	for (int y = 0; y < thresholds.height(); y++) {
		int yoffset = std::min(y * BLOCK_SIZE, iv.height() - BLOCK_SIZE);
		for (int x = 0; x < thresholds.width(); x++) {
			int xoffset = std::min(x * BLOCK_SIZE, iv.width() - BLOCK_SIZE);
			ThresholdBlock(iv.data(), xoffset, yoffset, thresholds(x, y), iv.rowStride(), *matrix);

#ifdef PRINT_DEBUG
			for (int yy = 0; yy < 8; ++yy)
				for (int xx = 0; xx < 8; ++xx)
					out.set(xoffset + xx, yoffset + yy, thresholds(x, y));
#endif
		}
	}

#ifdef PRINT_DEBUG
	std::ofstream file("thresholds_new.pnm");
	file << "P5\n" << out.width() << ' ' << out.height() << "\n255\n";
	file.write(reinterpret_cast<const char*>(out.data()), out.size());
#endif

	return matrix;
}

#endif

std::shared_ptr<const BitMatrix> HybridBinarizer::getBlackMatrix() const
{
	if (width() >= WINDOW_SIZE && height() >= WINDOW_SIZE) {
#ifdef USE_NEW_ALGORITHM
		auto thrs = SmoothThresholds(BlockThresholds(_buffer));
		return ThresholdImage(_buffer, thrs);
#else
		const uint8_t* luminances = _buffer.data();
		int subWidth = (width() + BLOCK_SIZE - 1) / BLOCK_SIZE; // ceil(width/BS)
		int subHeight = (height() + BLOCK_SIZE - 1) / BLOCK_SIZE; // ceil(height/BS)
		auto blackPoints =
			CalculateBlackPoints(luminances, subWidth, subHeight, width(), height(), _buffer.rowStride());

		return CalculateMatrix(luminances, subWidth, subHeight, width(), height(), _buffer.rowStride(), blackPoints);
#endif
	} else {
		// If the image is too small, fall back to the global histogram approach.
		return GlobalHistogramBinarizer::getBlackMatrix();
	}
}

} // ZXing
