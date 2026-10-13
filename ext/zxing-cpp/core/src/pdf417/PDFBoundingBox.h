/*
* Copyright 2016 Nu-book Inc.
* Copyright 2016 ZXing authors
*/
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "ZXNullable.h"
#include "ResultPoint.h"

namespace ZXing {
namespace Pdf417 {

/**
* @author Guenther Grau
*/
class BoundingBox
{
	int _imgWidth;
	int _imgHeight;
	Nullable<ResultPoint> _topLeft;
	Nullable<ResultPoint> _bottomLeft;
	Nullable<ResultPoint> _topRight;
	Nullable<ResultPoint> _bottomRight;
	int _minX;
	int _maxX;
	int _minY;
	int _maxY;

public:
	BoundingBox();

	int minX() const {
		return _minX;
	}

	int maxX() const {
		return _maxX;
	}

	int minY() const {
		return _minY;
	}

	int maxY() const {
		return _maxY;
	}

	Nullable<ResultPoint> topLeft() const {
		return _topLeft;
	}

	Nullable<ResultPoint> topRight() const {
		return _topRight;
	}

	Nullable<ResultPoint> bottomLeft() const {
		return _bottomLeft;
	}

	Nullable<ResultPoint> bottomRight() const {
		return _bottomRight;
	}

	static bool Create(int imgWidth, int imgHeight, const Nullable<ResultPoint>& topLeft, const Nullable<ResultPoint>& bottomLeft, const Nullable<ResultPoint>& topRight, const Nullable<ResultPoint>& bottomRight, BoundingBox& result);
	static bool Merge(const Nullable<BoundingBox>& leftBox, const Nullable<BoundingBox>& rightBox, Nullable<BoundingBox>& result);
	static bool AddMissingRows(const BoundingBox&box, int missingStartRows, int missingEndRows, bool isLeft, BoundingBox& result);

private:
	void calculateMinMaxValues();
};

} // Pdf417
} // ZXing
