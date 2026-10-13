/*
* Copyright 2016 Nu-book Inc.
* Copyright 2016 ZXing authors
* Copyright 2020 Axel Waggershauser
*/
// SPDX-License-Identifier: Apache-2.0

#include "PerspectiveTransform.h"

#include <array>

namespace ZXing {

PerspectiveTransform PerspectiveTransform::inverse() const
{
	// Here, the adjoint serves as the inverse:
	// Adjoint is the transpose of the cofactor matrix:
	return {
		a22 * a33 - a23 * a32,
		a23 * a31 - a21 * a33,
		a21 * a32 - a22 * a31,
		a13 * a32 - a12 * a33,
		a11 * a33 - a13 * a31,
		a12 * a31 - a11 * a32,
		a12 * a23 - a13 * a22,
		a13 * a21 - a11 * a23,
		a11 * a22 - a12 * a21
	};
}

PerspectiveTransform PerspectiveTransform::times(const PerspectiveTransform& other) const
{
	return {
		a11 * other.a11 + a21 * other.a12 + a31 * other.a13,
		a11 * other.a21 + a21 * other.a22 + a31 * other.a23,
		a11 * other.a31 + a21 * other.a32 + a31 * other.a33,
		a12 * other.a11 + a22 * other.a12 + a32 * other.a13,
		a12 * other.a21 + a22 * other.a22 + a32 * other.a23,
		a12 * other.a31 + a22 * other.a32 + a32 * other.a33,
		a13 * other.a11 + a23 * other.a12 + a33 * other.a13,
		a13 * other.a21 + a23 * other.a22 + a33 * other.a23,
		a13 * other.a31 + a23 * other.a32 + a33 * other.a33
	};
}

PerspectiveTransform PerspectiveTransform::UnitSquareTo(const QuadrilateralF& q)
{
	auto [x0, y0, x1, y1, x2, y2, x3, y3] = reinterpret_cast<const std::array<PointF::value_t, 8>&>(q);

	auto d3 = q[0] - q[1] + q[2] - q[3];
	if (d3 == PointF(0, 0)) {
		// Affine
		return {x1 - x0, x2 - x1, x0,
				y1 - y0, y2 - y1, y0,
				0.0f, 0.0f, 1.0f};
	} else {
		auto d1 = q[1] - q[2];
		auto d2 = q[3] - q[2];
		auto denominator = cross(d1, d2);
		auto a13 = cross(d3, d2) / denominator;
		auto a23 = cross(d1, d3) / denominator;
		return {x1 - x0 + a13 * x1, x3 - x0 + a23 * x3, x0,
				y1 - y0 + a13 * y1, y3 - y0 + a23 * y3, y0,
				a13, a23, 1.0f};
	}
}

PerspectiveTransform::PerspectiveTransform(const QuadrilateralF& src, const QuadrilateralF& dst)
{
	if (!IsConvex(src) || !IsConvex(dst))
		return;
	*this = UnitSquareTo(dst).times(UnitSquareTo(src).inverse());
}

PointF PerspectiveTransform::operator()(PointF p) const
{
	auto denominator = a13 * p.x + a23 * p.y + a33;
	return {(a11 * p.x + a21 * p.y + a31) / denominator, (a12 * p.x + a22 * p.y + a32) / denominator};
}

} // ZXing
