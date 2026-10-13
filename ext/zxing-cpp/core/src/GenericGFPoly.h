/*
* Copyright 2016 Nu-book Inc.
* Copyright 2016 ZXing authors
*/
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "ZXAlgorithms.h"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <vector>

namespace ZXing {

class GenericGF;

/**
* <p>Represents a polynomial whose coefficients are elements of a GF.
* Instances of this class are immutable.</p>
*
* <p>Much credit is due to William Rucklidge since portions of this code are an indirect
* port of his C++ Reed-Solomon implementation.</p>
*
* @author Sean Owen
*/
class GenericGFPoly
{
	struct Coefficients : public std::vector<int>
	{
		void reserve(size_t s)
		{
			if (capacity() < s)
				std::vector<int>::reserve(std::max(size_t(32), s));
		}

		void resize(size_t s)
		{
			reserve(s);
			std::vector<int>::resize(s);
		}

		void resize(size_t s, int i)
		{
			reserve(s);
			std::vector<int>::resize(s, i);
		}
	};

public:
	// Build a invalid object, so that this can be used in container or return by reference,
	// any access to invalid object is undefined behavior.
	GenericGFPoly() = default;

	/**
	* @param field the {@link GenericGF} instance representing the field to use
	* to perform computations
	* @param coefficients coefficients as ints representing elements of GF(size), arranged
	* from most significant (highest-power term) coefficient to least significant
	*/
	GenericGFPoly(const GenericGF& field, std::vector<int>&& coefficients) : _field(&field)
	{
		assert(!coefficients.empty());
		_coefficients.swap(coefficients); // _coefficients = coefficients
		normalize();
	}
	GenericGFPoly(const GenericGF& field, const std::vector<int>& coefficients) : GenericGFPoly(field, std::vector<int>(coefficients)) {}

	GenericGFPoly& operator=(GenericGFPoly&& other) noexcept = default;
	GenericGFPoly(GenericGFPoly&& other) noexcept = default;

	GenericGFPoly& operator=(const GenericGFPoly& other) {
		assert(_field == other._field);
		_coefficients.reserve(other._coefficients.size());
		_coefficients = other._coefficients;
		return *this;
	}

	GenericGFPoly(const GenericGFPoly& other) {
		_field = other._field;
		*this = other;
	}

	GenericGFPoly& setField(const GenericGF& field)
	{
		_field = &field;
		return *this;
	}
	const GenericGF& field() const noexcept { return *_field; }
	const auto& coefficients() const noexcept { return _coefficients; }

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
		return _coefficients[0] == 0;
	}

	int leadingCoefficient() const noexcept {
		return _coefficients.front();
	}

	int constant() const noexcept {
		return _coefficients.back();
	}

	/**
	 * @brief set to the monomial representing coefficient * x^degree
	 */
	GenericGFPoly& setMonomial(int coefficient, int degree = 0)
	{
		assert(degree >= 0 && (coefficient != 0 || degree == 0));

		_coefficients.resize(degree + 1);
		std::fill(_coefficients.begin(), _coefficients.end(), 0);
		_coefficients.front() = coefficient;

		return *this;
	}

	/**
	* @return evaluation of this polynomial at a given point
	*/
	int evaluateAt(int a) const;

	GenericGFPoly& addOrSubtract(GenericGFPoly& other);
	GenericGFPoly& multiply(const GenericGFPoly& other);
	GenericGFPoly& multiplyByMonomial(int coefficient, int degree = 0);
	GenericGFPoly& divide(const GenericGFPoly& other, GenericGFPoly& quotient);

	friend void swap(GenericGFPoly& a, GenericGFPoly& b)
	{
		std::swap(a._field, b._field);
		std::swap(a._coefficients, b._coefficients);
	}

private:
	void normalize();

	const GenericGF* _field = nullptr;
	Coefficients _coefficients, _cache; // _cache is used for malloc caching
};

} // ZXing
