/*
* Copyright 2016 Nu-book Inc.
* Copyright 2016 ZXing authors
*/
// SPDX-License-Identifier: Apache-2.0

#include "ResultPoint.h"

#include <cmath>

namespace ZXing {

float ResultPoint::Distance(int aX, int aY, int bX, int bY)
{
	auto dx = static_cast<float>(aX - bX);
	auto dy = static_cast<float>(aY - bY);
	return std::sqrt(dx * dx + dy * dy);
}

} // ZXing
