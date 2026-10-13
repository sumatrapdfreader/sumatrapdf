/*
* Copyright 2016 Nu-book Inc.
* Copyright 2016 ZXing authors
*/
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "ZXAlgorithms.h"

#include <utility>
#include <vector>

namespace ZXing {
namespace Pdf417 {

class ModulusGF;

/**
* @author Sean Owen
* @see com.google.zxing.common.reedsolomon.GenericGFPoly
*/
class ModulusPoly
{
	const ModulusGF* _field = nullptr;
	std::vector<int> _coefficients;

public:
	// Build a invalid object, so that this can be used in container or return by reference,
	// any access to invalid object is undefined behavior.
	ModulusPoly() = default;

	ModulusPoly(const ModulusGF& field, const std::vector<int>& coefficients);

	const std::vector<int>& coefficients() const {
		return _coefficients;
	}

	/**
	* @return degree of this polynomial
	*/
	int degree() const {
		return Size(_coefficients) - 1;
	}

	/**
	* @return true iff this polynomial is the monomial "0"
	*/
	bool isZero() const {
		return _coefficients.at(0) == 0;
	}

	/**
	* @return coefficient of x^degree term in this polynomial
	*/
	int coefficient(int degree) const {
		return _coefficients.at(_coefficients.size() - 1 - degree);
	}

	/**
	* @return evaluation of this polynomial at a given point
	*/
	int evaluateAt(int a) const;

	ModulusPoly add(const ModulusPoly& other) const;
	ModulusPoly subtract(const ModulusPoly& other) const;
	ModulusPoly multiply(const ModulusPoly& other) const;
	ModulusPoly negative() const;
	ModulusPoly multiply(int scalar) const;
	ModulusPoly multiplyByMonomial(int degree, int coefficient) const;
	void divide(const ModulusPoly& other, ModulusPoly& quotient, ModulusPoly& remainder) const;

	friend void swap(ModulusPoly& a, ModulusPoly& b)
	{
		std::swap(a._field, b._field);
		std::swap(a._coefficients, b._coefficients);
	}
};

} // Pdf417
} // ZXing
