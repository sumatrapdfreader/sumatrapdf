/*
* Copyright 2020 Axel Waggershauser
*/
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "BinaryBitmap.h"
#include "BitMatrix.h"

#include <cstdint>

namespace ZXing {

class ThresholdBinarizer : public BinaryBitmap
{
	const uint8_t _threshold = 0;

public:
	ThresholdBinarizer(const ImageView& buffer, uint8_t threshold = 128) : BinaryBitmap(buffer), _threshold(threshold) {}

	bool getPatternRow(int row, int rotation, PatternRow& res) const override
	{
		auto buffer = _buffer.rotated(rotation);

		const int stride = buffer.pixStride();
		const uint8_t* begin = buffer.data(0, row) + GreenIndex(buffer.format());
		const uint8_t* end = begin + buffer.width() * stride;

		auto* lastPos = begin;
		bool lastVal = false;

		res.clear();

		for (const uint8_t* p = begin; p != end; p += stride) {
			bool val = *p <= _threshold;
			if (val != lastVal) {
				res.push_back(narrow_cast<PatternRow::value_type>((p - lastPos) / stride));
				lastVal = val;
				lastPos = p;
			}
		}

		res.push_back(narrow_cast<PatternRow::value_type>((end - lastPos) / stride));

		if (*(end - stride) <= _threshold)
			res.push_back(0); // last value is number of white pixels, here 0

		return true;
	}

	std::shared_ptr<const BitMatrix> getBlackMatrix() const override
	{
		return std::make_shared<const BitMatrix>(binarize(_threshold));
	}
};

} // ZXing
