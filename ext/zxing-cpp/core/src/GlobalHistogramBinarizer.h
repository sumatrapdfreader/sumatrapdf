/*
* Copyright 2016 Nu-book Inc.
* Copyright 2016 ZXing authors
*/
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "BinaryBitmap.h"

namespace ZXing {

/**
* This Binarizer implementation uses the old ZXing global histogram approach. It is suitable
* for low-end mobile devices which don't have enough CPU or memory to use a local thresholding
* algorithm. However, because it picks a global black point, it cannot handle difficult shadows
* and gradients.
*
* Faster mobile devices and all desktop applications should probably use HybridBinarizer instead.
*
* @author dswitkin@google.com (Daniel Switkin)
* @author Sean Owen
*/
class GlobalHistogramBinarizer : public BinaryBitmap
{
public:
	explicit GlobalHistogramBinarizer(const ImageView& buffer);
	~GlobalHistogramBinarizer() override;

	bool getPatternRow(int row, int rotation, PatternRow &res) const override;
	std::shared_ptr<const BitMatrix> getBlackMatrix() const override;
};

} // ZXing
