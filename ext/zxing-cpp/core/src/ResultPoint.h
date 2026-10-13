/*
* Copyright 2016 Nu-book Inc.
* Copyright 2016 ZXing authors
*/
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "Point.h"

namespace ZXing {

/**
* <p>Encapsulates a point of interest in an image containing a barcode. Typically, this
* would be the location of a finder pattern or the corner of the barcode, for example.</p>
*
* @author Sean Owen
*/
class ResultPoint : public PointF
{
public:
	ResultPoint() = default;
	ResultPoint(float x, float y) : PointF(x, y) {}
	ResultPoint(int x, int y) : PointF(x, y) {}
	template <typename T> ResultPoint(PointT<T> p) : PointF(p) {}

	float x() const { return static_cast<float>(PointF::x); }
	float y() const { return static_cast<float>(PointF::y); }

	void set(float x, float y) { *this = PointF(x, y); }

	static float Distance(int aX, int aY, int bX, int bY);
};

} // ZXing
