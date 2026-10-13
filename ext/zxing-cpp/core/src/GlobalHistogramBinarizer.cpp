/*
* Copyright 2016 Nu-book Inc.
* Copyright 2016 ZXing authors
*/
// SPDX-License-Identifier: Apache-2.0

#include "GlobalHistogramBinarizer.h"

#include "BitMatrix.h"
#include "Pattern.h"
#include "ZXConfig.h"

#include <algorithm>
#include <array>
#include <utility>

namespace ZXing {

static constexpr int LUMINANCE_BITS = 5;
static constexpr int LUMINANCE_SHIFT = 8 - LUMINANCE_BITS;
static constexpr int LUMINANCE_BUCKETS = 1 << LUMINANCE_BITS;

using Histogram = std::array<uint16_t, LUMINANCE_BUCKETS>;

GlobalHistogramBinarizer::GlobalHistogramBinarizer(const ImageView& buffer) : BinaryBitmap(buffer) {}

GlobalHistogramBinarizer::~GlobalHistogramBinarizer() = default;

using ImageLineView = Range<StrideIter<const uint8_t*>>;

inline ImageLineView RowView(const ImageView& iv, int row)
{
	return {{iv.data(0, row), iv.pixStride()}, {iv.data(iv.width(), row), iv.pixStride()}};
}

static void ThresholdSharpened(const ImageLineView in, int threshold, std::vector<uint8_t>& out)
{
	out.resize(in.size());
	auto i = in.begin();
	auto o = out.begin();

	*o++ = (*i++ <= threshold) * BitMatrix::SET_V;
	for (auto end = in.end() - 1; i != end; ++i)
		*o++ = ((-i[-1] + (int(i[0]) * 4) - i[1]) / 2 <= threshold) * BitMatrix::SET_V;
	*o++ = (*i++ <= threshold) * BitMatrix::SET_V;
}

static auto GenHistogram(const ImageLineView line)
{
	// This code causes about 20% of the total runtime on an AVX2 system for a EAN13 search on Lum input data.
	// Trying to increase the performance by performing 2 or 4 "parallel" histograms helped nothing.
	Histogram res = {};
	for (auto pix : line)
		res[pix >> LUMINANCE_SHIFT]++;
	return res;
}

// Return -1 on error
static int EstimateBlackPoint(const Histogram& buckets)
{
	// Find the tallest peak in the histogram.
	auto firstPeakPos = std::max_element(buckets.begin(), buckets.end());
	int firstPeak = narrow_cast<int>(firstPeakPos - buckets.begin());
	int firstPeakSize = *firstPeakPos;
	int maxBucketCount = firstPeakSize;

	// Find the second-tallest peak which is somewhat far from the tallest peak.
	int secondPeak = 0;
	int secondPeakScore = 0;
	for (int x = 0; x < Size(buckets); x++) {
		int distanceToBiggest = x - firstPeak;
		// Encourage more distant second peaks by multiplying by square of distance.
		int score = buckets[x] * distanceToBiggest * distanceToBiggest;
		if (score > secondPeakScore) {
			secondPeak = x;
			secondPeakScore = score;
		}
	}

	// Make sure firstPeak corresponds to the black peak.
	if (firstPeak > secondPeak) {
		std::swap(firstPeak, secondPeak);
	}

	// If there is too little contrast in the image to pick a meaningful black point, throw rather
	// than waste time trying to decode the image, and risk false positives.
	if (secondPeak - firstPeak <= LUMINANCE_BUCKETS / 16) {
		return -1;
	}

	// Find a valley between them that is low and closer to the white peak.
	int bestValley = secondPeak - 1;
	int bestValleyScore = -1;
	for (int x = secondPeak - 1; x > firstPeak; x--) {
		int fromFirst = x - firstPeak;
		int score = fromFirst * fromFirst * (secondPeak - x) * (maxBucketCount - buckets[x]);
		if (score > bestValleyScore) {
			bestValley = x;
			bestValleyScore = score;
		}
	}

	return bestValley << LUMINANCE_SHIFT;
}

bool GlobalHistogramBinarizer::getPatternRow(int row, int rotation, PatternRow& res) const
{
	auto buffer = _buffer.rotated(rotation);
	auto lineView = RowView(buffer, row);

	if (buffer.width() < 3)
		return false; // special casing the code below for a width < 3 makes no sense

#if defined(__AVX__) // or defined(__ARM_NEON)
	// If we are extracting a column (instead of a row), we run into cache misses on every pixel access both
	// during the histogram calculation and during the sharpen+threshold operation. Additionally, if we
	// perform the ThresholdSharpened function on pixStride==1 data, the auto-vectorizer makes that part
	// 8x faster on an AVX2 cpu which easily recovers the extra cost that we pay for the copying.
	ZX_THREAD_LOCAL std::vector<uint8_t> line;
	if (std::abs(buffer.pixStride()) > 4) {
		line.resize(lineView.size());
		std::copy(lineView.begin(), lineView.end(), line.begin());
		lineView = {{line.data(), 1}, {line.data() + line.size(), 1}};
	}
#endif

	auto threshold = EstimateBlackPoint(GenHistogram(lineView)) - 1;
	if (threshold <= 0)
		return false;

	ZX_THREAD_LOCAL std::vector<uint8_t> binarized;
	// the optimizer can generate a specialized version for pixStride==1 (non-rotated input) that is about 8x faster on AVX2 hardware
	if (lineView.begin().stride == 1)
		ThresholdSharpened(lineView, threshold, binarized);
	else
		ThresholdSharpened(lineView, threshold, binarized);
	GetPatternRow(Range(binarized), res);

	return true;
}

// Does not sharpen the data, as this call is intended to only be used by 2D Readers.
std::shared_ptr<const BitMatrix>
GlobalHistogramBinarizer::getBlackMatrix() const
{
	// Quickly calculates the histogram by sampling four rows from the image. This proved to be
	// more robust on the blackbox tests than sampling a diagonal as we used to do.
	Histogram localBuckets = {};
	{
		for (int y = 1; y < 5; y++) {
			int row = height() * y / 5;
			const uint8_t* luminances = _buffer.data(0, row);
			int right = (width() * 4) / 5;
			for (int x = width() / 5; x < right; x++)
				localBuckets[luminances[x] >> LUMINANCE_SHIFT]++;
		}
	}

	int blackPoint = EstimateBlackPoint(localBuckets);
	if (blackPoint <= 0)
		return {};



	return std::make_shared<const BitMatrix>(binarize(blackPoint));
}

} // ZXing
