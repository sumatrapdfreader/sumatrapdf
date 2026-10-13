/*
* Copyright 2016 Nu-book Inc.
* Copyright 2016 ZXing authors
*/
// SPDX-License-Identifier: Apache-2.0

#include "PDFBoundingBox.h"

#include <algorithm>

namespace ZXing {
namespace Pdf417 {

BoundingBox::BoundingBox() {
	_imgWidth = _imgHeight = _minX = _maxX = _minY = _maxY = 0;
}

bool
BoundingBox::Create(int imgWidth, int imgHeight, const Nullable<ResultPoint>& topLeft, const Nullable<ResultPoint>& bottomLeft, const Nullable<ResultPoint>& topRight, const Nullable<ResultPoint>& bottomRight, BoundingBox& result)
{
	if ((topLeft == nullptr && topRight == nullptr) ||
		(bottomLeft == nullptr && bottomRight == nullptr) ||
		(topLeft != nullptr && bottomLeft == nullptr) ||
		(topRight != nullptr && bottomRight == nullptr)) {
		return false;
	}
	result._imgWidth = imgWidth;
	result._imgHeight = imgHeight;
	result._topLeft = topLeft;
	result._bottomLeft = bottomLeft;
	result._topRight = topRight;
	result._bottomRight = bottomRight;
	result.calculateMinMaxValues();
	return true;
}

void
BoundingBox::calculateMinMaxValues()
{
	if (_topLeft == nullptr) {
		_topLeft = ResultPoint(0.f, _topRight.value().y());
		_bottomLeft = ResultPoint(0.f, _bottomRight.value().y());
	}
	else if (_topRight == nullptr) {
		_topRight = ResultPoint(static_cast<float>(_imgWidth - 1), _topLeft.value().y());
		_bottomRight = ResultPoint(static_cast<float>(_imgWidth - 1), _bottomLeft.value().y());
	}

	_minX = static_cast<int>(std::min(_topLeft.value().x(), _bottomLeft.value().x()));
	_maxX = static_cast<int>(std::max(_topRight.value().x(), _bottomRight.value().x()));
	_minY = static_cast<int>(std::min(_topLeft.value().y(), _topRight.value().y()));
	_maxY = static_cast<int>(std::max(_bottomLeft.value().y(), _bottomRight.value().y()));
}

bool
BoundingBox::Merge(const Nullable<BoundingBox>& leftBox, const Nullable<BoundingBox>& rightBox, Nullable<BoundingBox>& result)
{
	if (leftBox == nullptr) {
		result = rightBox;
		return true;
	}
	if (rightBox == nullptr) {
		result = leftBox;
		return true;
	}
	BoundingBox box;
	if (Create(leftBox.value()._imgWidth, leftBox.value()._imgHeight, leftBox.value()._topLeft, leftBox.value()._bottomLeft, rightBox.value()._topRight, rightBox.value()._bottomRight, box)) {
		result = box;
		return true;
	}
	return false;
}

bool
BoundingBox::AddMissingRows(const BoundingBox& box, int missingStartRows, int missingEndRows, bool isLeft, BoundingBox& result)
{
	auto newTopLeft = box._topLeft;
	auto newBottomLeft = box._bottomLeft;
	auto newTopRight = box._topRight;
	auto newBottomRight = box._bottomRight;

	if (missingStartRows > 0) {
		auto top = isLeft ? box._topLeft : box._topRight;
		int newMinY = static_cast<int>(top.value().y()) - missingStartRows;
		if (newMinY < 0) {
			newMinY = 0;
		}
		ResultPoint newTop(top.value().x(), static_cast<float>(newMinY));
		if (isLeft) {
			newTopLeft = newTop;
		}
		else {
			newTopRight = newTop;
		}
	}

	if (missingEndRows > 0) {
		auto bottom = isLeft ? box._bottomLeft : box._bottomRight;
		int newMaxY = (int)bottom.value().y() + missingEndRows;
		if (newMaxY >= box._imgHeight) {
			newMaxY = box._imgHeight - 1;
		}
		ResultPoint newBottom(bottom.value().x(), static_cast<float>(newMaxY));
		if (isLeft) {
			newBottomLeft = newBottom;
		}
		else {
			newBottomRight = newBottom;
		}
	}

	return Create(box._imgWidth, box._imgHeight, newTopLeft, newBottomLeft, newTopRight, newBottomRight, result);
}

} // Pdf417
} // ZXing
