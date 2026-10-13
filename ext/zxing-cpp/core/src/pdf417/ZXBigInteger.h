/*
* Copyright 2016 Huy Cuong Nguyen
*/
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstdlib>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace ZXing {

/**
* All credits on BigInteger below go to Matt McCutchen, as the code below is extracted/modified from his C++ Big Integer Library (https://mattmccutchen.net/bigint/)
*/
class BigInteger
{
public:
	using Block = size_t;

	// The number of bits in a block
	//static const unsigned int N = 8 * sizeof(Block);

	// Constructs zero.
	BigInteger() = default;

	template <typename T>
	BigInteger(T x, typename std::enable_if_t<std::is_integral_v<T> && std::is_unsigned_v<T>>* = nullptr) : mag(1, x) {}

	template <typename T>
	BigInteger(T x, typename std::enable_if_t<std::is_integral_v<T> && std::is_signed_v<T>>* = nullptr) : negative(x < 0), mag(1, std::abs(x)) {}

	static bool TryParse(const std::string& str, BigInteger& result);
	static bool TryParse(const std::wstring& str, BigInteger& result);

	bool isZero() const { return mag.empty(); }

	std::string toString() const;
	int toInt() const;

	BigInteger& operator+=(BigInteger&& a) {
		if (mag.empty())
			*this = std::move(a);
		else
			Add(*this, a, *this);
		return *this;
	}

	friend BigInteger operator+(const BigInteger& a, const BigInteger& b) {
		BigInteger c;
		BigInteger::Add(a, b, c);
		return c;
	}

	friend BigInteger operator-(const BigInteger& a, const BigInteger& b) {
		BigInteger c;
		BigInteger::Subtract(a, b, c);
		return c;
	}

	friend BigInteger operator*(const BigInteger& a, const BigInteger& b) {
		BigInteger c;
		BigInteger::Multiply(a, b, c);
		return c;
	}

	static void Add(const BigInteger& a, const BigInteger &b, BigInteger& c);
	static void Subtract(const BigInteger& a, const BigInteger &b, BigInteger& c);
	static void Multiply(const BigInteger& a, const BigInteger &b, BigInteger& c);
	static void Divide(const BigInteger& a, const BigInteger &b, BigInteger& quotient, BigInteger& remainder);

private:
	bool negative = false;
	std::vector<Block> mag;
};


} // ZXing
