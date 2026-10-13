/*
* Copyright 2016 Huy Cuong Nguyen
* Copyright 2016 ZXing authors
*/
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "Point.h"
#include "ZXAlgorithms.h"

#include <stdexcept>
#include <algorithm>
#include <cassert>
#include <vector>

namespace ZXing {

template <class T>
class Matrix
{
public:
	using value_t = T;

private:
	int _width = 0;
	int _height = 0;
	std::vector<value_t> _data;

	// Nothing wrong to support it, just to make it explicit, instead of by mistake.
	// Use copy() below.
	Matrix(const Matrix &) = default;
	Matrix& operator=(const Matrix &) = delete;

public:
	Matrix() = default;

#if defined(__llvm__) || (defined(__GNUC__) && (__GNUC__ > 7))
	__attribute__((no_sanitize("signed-integer-overflow")))
#endif
	Matrix(int width, int height, value_t val = {}) : _width(width), _height(height), _data(_width * _height, val) {
		if (width != 0 && Size(_data) / width != height)
			throw std::invalid_argument("Invalid size: width * height is too big");
	}

	Matrix(Matrix&&) noexcept = default;
	Matrix& operator=(Matrix&&) noexcept = default;

	Matrix copy() const {
		return *this;
	}

	int height() const {
		return _height;
	}

	int width() const {
		return _width;
	}

	int size() const {
		return Size(_data);
	}

	value_t& operator()(int x, int y)
	{
		assert(x >= 0 && x < _width && y >= 0 && y < _height);
		return _data[y * _width + x];
	}

	const T& operator()(int x, int y) const
	{
		assert(x >= 0 && x < _width && y >= 0 && y < _height);
		return _data[y * _width + x];
	}

	const value_t& get(int x, int y) const {
		return operator()(x, y);
	}

	value_t& set(int x, int y, value_t value) {
		return operator()(x, y) = value;
	}

	const value_t& get(PointI p) const {
		return operator()(p.x, p.y);
	}

	value_t& set(PointI p, value_t value) {
		return operator()(p.x, p.y) = value;
	}

	const value_t* data() const {
		return _data.data();
	}

	const value_t* begin() const {
		return _data.data();
	}

	const value_t* end() const {
		return _data.data() + _width * _height;
	}

	value_t* begin() {
		return _data.data();
	}

	value_t* end() {
		return _data.data() + _width * _height;
	}

	void clear(value_t value = {}) {
		std::fill(_data.begin(), _data.end(), value);
	}
};

} // ZXing
