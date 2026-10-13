/*
* Copyright 2016 Nu-book Inc.
* Copyright 2016 ZXing authors
*/
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "GlobalHistogramBinarizer.h"

namespace ZXing {

/**
* This class implements a local thresholding algorithm, which while slower than the
* GlobalHistogramBinarizer, is fairly efficient for what it does. It is designed for
* high frequency images of barcodes with black data on white backgrounds. For this application,
* it does a much better job than a global blackpoint with severe shadows and gradients.
* However it tends to produce artifacts on lower frequency images and is therefore not
* a good general purpose binarizer for uses outside ZXing.
*
* This class extends GlobalHistogramBinarizer, using the older histogram approach for 1D readers,
* and the newer local approach for 2D readers. 1D decoding using a per-row histogram is already
* inherently local, and only fails for horizontal gradients. We can revisit that problem later,
* but for now it was not a win to use local blocks for 1D.
*
* This Binarizer is the default for the unit tests and the recommended class for library users.
*
* @author dswitkin@google.com (Daniel Switkin)
*/
class HybridBinarizer : public GlobalHistogramBinarizer
{
public:
	explicit HybridBinarizer(const ImageView& iv);
	~HybridBinarizer() override;

	bool getPatternRow(int row, int rotation, PatternRow &res) const override;
	std::shared_ptr<const BitMatrix> getBlackMatrix() const override;
};

} // ZXing
