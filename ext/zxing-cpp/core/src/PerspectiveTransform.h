/*
* Copyright 2016 Nu-book Inc.
* Copyright 2016 ZXing authors
* Copyright 2020 Axel Waggershauser
*/
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "Point.h"
#include "Quadrilateral.h"

namespace ZXing {

/**
* <p>This class implements a perspective transform in two dimensions. Given four source and four
* destination points, it will compute the transformation implied between them. The code is based
* directly upon section 3.4.2 of George Wolberg's "Digital Image Warping"; see pages 54-56.</p>
*/
class PerspectiveTransform
{
	using value_t = PointF::value_t;
	value_t a11, a12, a13, a21, a22, a23, a31, a32, a33 = NAN;

	PerspectiveTransform(value_t a11, value_t a21, value_t a31, value_t a12, value_t a22, value_t a32, value_t a13,
						 value_t a23, value_t a33)
		: a11(a11), a12(a12), a13(a13), a21(a21), a22(a22), a23(a23), a31(a31), a32(a32), a33(a33)
	{}

	PerspectiveTransform inverse() const;
	PerspectiveTransform times(const PerspectiveTransform& other) const;

	static PerspectiveTransform UnitSquareTo(const QuadrilateralF& q);

public:
	PerspectiveTransform() = default;
	PerspectiveTransform(const QuadrilateralF& src, const QuadrilateralF& dst);

	/// Project from the destination space (grid of modules) into the image space (bit matrix)
	PointF operator()(PointF p) const;

	bool isValid() const { return !std::isnan(a33); }
};

} // ZXing
