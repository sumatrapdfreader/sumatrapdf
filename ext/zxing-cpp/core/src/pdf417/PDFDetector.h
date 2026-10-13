/*
* Copyright 2016 Nu-book Inc.
* Copyright 2016 ZXing authors
*/
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "ResultPoint.h"
#include "ZXNullable.h"

#include <list>
#include <array>
#include <memory>

namespace ZXing {

class BitMatrix;
class BinaryBitmap;

namespace Pdf417 {

/**
* <p>Encapsulates logic that can detect a PDF417 Code in an image, even if the
* PDF417 Code is rotated or skewed, or partially obscured.< / p>
*
* @author SITA Lab(kevin.osullivan@sita.aero)
* @author dswitkin@google.com(Daniel Switkin)
* @author Guenther Grau
*/
class Detector
{
public:
	struct Result
	{
		std::shared_ptr<const BitMatrix> bits;
		std::list<std::array<Nullable<ResultPoint>, 8>> points;
		int rotation = -1;
	};

	static Result Detect(const BinaryBitmap& image, bool multiple, bool tryRotate);
};

} // Pdf417
} // ZXing
