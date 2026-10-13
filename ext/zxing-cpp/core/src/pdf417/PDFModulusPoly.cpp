/*
* Copyright 2016 Nu-book Inc.
* Copyright 2016 ZXing authors
*/
// SPDX-License-Identifier: Apache-2.0

#include "PDFModulusPoly.h"
#include "PDFModulusGF.h"

#include <algorithm>
#include <stdexcept>

namespace ZXing {
namespace Pdf417 {

ModulusPoly::ModulusPoly(const ModulusGF& field, const std::vector<int>& coefficients) :
	_field(&field)
{
	size_t coefficientsLength = coefficients.size();
	if (coefficientsLength > 1 && coefficients[0] == 0) {
		// Leading term must be non-zero for anything except the constant polynomial "0"
		size_t firstNonZero = 1;
		while (firstNonZero < coefficientsLength && coefficients[firstNonZero] == 0) {
			firstNonZero++;
		}
		if (firstNonZero == coefficientsLength) {
			_coefficients.resize(1, 0);
		}
		else {
			_coefficients.resize(coefficientsLength - firstNonZero);
			std::copy(coefficients.begin() + firstNonZero, coefficients.end(), _coefficients.begin());
		}
	}
	else {
		_coefficients = coefficients;
	}
}

/**
* @return evaluation of this polynomial at a given point
*/
int
ModulusPoly::evaluateAt(int a) const
{
	if (a == 0) // return the x^0 coefficient
		return coefficient(0);

	if (a == 1) // return the sum of the coefficients
		return Reduce(_coefficients, 0, [this](auto res, auto coef) { return _field->add(res, coef); });

	return std::accumulate(_coefficients.begin(), _coefficients.end(), 0,
						   [this, a](auto res, auto coef) { return _field->add(_field->multiply(a, res), coef); });
}

ModulusPoly
ModulusPoly::add(const ModulusPoly& other) const
{
	if (_field != other._field) {
		throw std::invalid_argument("ModulusPolys do not have same ModulusGF field");
	}
	if (isZero()) {
		return other;
	}
	if (other.isZero()) {
		return *this;
	}

	auto smallerCoefficients = &_coefficients;
	auto largerCoefficients = &other._coefficients;
	if (smallerCoefficients->size() > largerCoefficients->size()) {
		std::swap(smallerCoefficients, largerCoefficients);
	}
	std::vector<int> sumDiff(largerCoefficients->size());
	size_t lengthDiff = largerCoefficients->size() - smallerCoefficients->size();

	// Copy high-order terms only found in higher-degree polynomial's coefficients
	std::copy_n(largerCoefficients->begin(), lengthDiff, sumDiff.begin());
	for (size_t i = lengthDiff; i < largerCoefficients->size(); i++) {
		sumDiff[i] = _field->add((*smallerCoefficients)[i - lengthDiff], (*largerCoefficients)[i]);
	}
	return ModulusPoly(*_field, sumDiff);
}

ModulusPoly
ModulusPoly::subtract(const ModulusPoly& other) const
{
	if (_field != other._field) {
		throw std::invalid_argument("ModulusPolys do not have same ModulusGF field");
	}
	if (other.isZero()) {
		return *this;
	}
	return add(other.negative());
}

ModulusPoly
ModulusPoly::multiply(const ModulusPoly& other) const
{
	if (_field != other._field) {
		throw std::invalid_argument("ModulusPolys do not have same ModulusGF field");
	}
	if (isZero() || other.isZero()) {
		return _field->zero();
	}
	auto& aCoefficients = _coefficients;
	size_t aLength = aCoefficients.size();
	auto& bCoefficients = other._coefficients;
	size_t bLength = bCoefficients.size();
	std::vector<int> product(aLength + bLength - 1, 0);
	for (size_t i = 0; i < aLength; i++) {
		int aCoeff = aCoefficients[i];
		for (size_t j = 0; j < bLength; j++) {
			product[i + j] = _field->add(product[i + j], _field->multiply(aCoeff, bCoefficients[j]));
		}
	}
	return ModulusPoly(*_field, product);
}

ModulusPoly
ModulusPoly::negative() const
{
	size_t size = _coefficients.size();
	std::vector<int> negativeCoefficients(size);
	for (size_t i = 0; i < size; i++) {
		negativeCoefficients[i] = _field->subtract(0, _coefficients[i]);
	}
	return ModulusPoly(*_field, negativeCoefficients);
}

ModulusPoly
ModulusPoly::multiply(int scalar) const
{
	if (scalar == 0) {
		return _field->zero();
	}
	if (scalar == 1) {
		return *this;
	}
	size_t size = _coefficients.size();
	std::vector<int> product(size);
	for (size_t i = 0; i < size; i++) {
		product[i] = _field->multiply(_coefficients[i], scalar);
	}
	return ModulusPoly(*_field, product);
}

ModulusPoly
ModulusPoly::multiplyByMonomial(int degree, int coefficient) const
{
	if (degree < 0) {
		throw std::invalid_argument("degree < 0");
	}
	if (coefficient == 0) {
		return _field->zero();
	}
	size_t size = _coefficients.size();
	std::vector<int> product(size + degree, 0);
	for (size_t i = 0; i < size; i++) {
		product[i] = _field->multiply(_coefficients[i], coefficient);
	}
	return ModulusPoly(*_field, product);
}

void
ModulusPoly::divide(const ModulusPoly& other, ModulusPoly& quotient, ModulusPoly& remainder) const
{
	if (_field != other._field) {
		throw std::invalid_argument("ModulusPolys do not have same ModulusGF field");
	}
	if (other.isZero()) {
		throw std::invalid_argument("Divide by 0");
	}

	quotient = _field->zero();
	remainder = *this;

	int denominatorLeadingTerm = other.coefficient(other.degree());
	int inverseDenominatorLeadingTerm = _field->inverse(denominatorLeadingTerm);

	while (remainder.degree() >= other.degree() && !remainder.isZero()) {
		int degreeDifference = remainder.degree() - other.degree();
		int scale = _field->multiply(remainder.coefficient(remainder.degree()), inverseDenominatorLeadingTerm);
		ModulusPoly term = other.multiplyByMonomial(degreeDifference, scale);
		ModulusPoly iterationQuotient = _field->buildMonomial(degreeDifference, scale);
		quotient = quotient.add(iterationQuotient);
		remainder = remainder.subtract(term);
	}
}


} // Pdf417
} // ZXing
