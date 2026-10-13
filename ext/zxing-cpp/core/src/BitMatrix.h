/*
* Copyright 2016 Nu-book Inc.
* Copyright 2016 ZXing authors
* Copyright 2022 Axel Waggershauser
*/
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "Matrix.h"
#include "Point.h"
#include "Range.h"

#include <cstdint>
#include <stdexcept>
#include <vector>

namespace ZXing {

class BitArray;
class ByteMatrix;

/**
 * @brief A simple, fast 2D array of bits.
 */
class BitMatrix
{
	int _width = 0;
	int _height = 0;
	using data_t = uint8_t;

	std::vector<data_t> _bits;
	// There is nothing wrong to support this but disable to make it explicit since we may copy something very big here.
	// Use copy() below.
	BitMatrix(const BitMatrix&) = default;
	BitMatrix& operator=(const BitMatrix&) = delete;

	const data_t& get(int i) const
	{
#if 1
		return _bits.at(i);
#else
		return _bits[i];
#endif
	}

	data_t& get(int i) { return const_cast<data_t&>(static_cast<const BitMatrix*>(this)->get(i)); }

	bool getTopLeftOnBit(int &left, int& top) const;
	bool getBottomRightOnBit(int &right, int& bottom) const;

public:
	static constexpr data_t SET_V = 0xff; // allows playing with SIMD binarization
	static constexpr data_t UNSET_V = 0;
	static_assert(bool(SET_V) && !bool(UNSET_V), "SET_V needs to evaluate to true, UNSET_V to false, see iterator usage");

	BitMatrix() = default;

#if defined(__llvm__) || (defined(__GNUC__) && (__GNUC__ > 7))
	__attribute__((no_sanitize("signed-integer-overflow")))
#endif
	BitMatrix(int width, int height) : _width(width), _height(height), _bits(width * height, UNSET_V)
	{
		if (width != 0 && Size(_bits) / width != height)
			throw std::invalid_argument("Invalid size: width * height is too big");
	}

	explicit BitMatrix(int dimension) : BitMatrix(dimension, dimension) {} // Construct a square matrix.

	BitMatrix(BitMatrix&& other) noexcept = default;
	BitMatrix& operator=(BitMatrix&& other) noexcept = default;

	BitMatrix copy() const { return *this; }

	Range<data_t*> row(int y) { return {_bits.data() + y * _width, _bits.data() + (y + 1) * _width}; }
	Range<const data_t*> row(int y) const { return {_bits.data() + y * _width, _bits.data() + (y + 1) * _width}; }

	Range<StrideIter<const data_t*>> col(int x) const
	{
		return {{_bits.data() + x + (_height - 1) * _width, -_width}, {_bits.data() + x - _width, -_width}};
	}

	bool get(int x, int y) const { return get(y * _width + x); }
	void set(int x, int y, bool val = true) { get(y * _width + x) = val * SET_V; }

	/**
	* <p>Flips the given bit.</p>
	*
	* @param x The horizontal component (i.e. which column)
	* @param y The vertical component (i.e. which row)
	*/
	void flip(int x, int y)
	{
		auto& v = get(y * _width + x);
		v = !v;
	}

	void flipAll()
	{
		for (auto& i : _bits)
			i = !i * SET_V;
	}

	/**
	* <p>Sets a square region of the bit matrix to true.</p>
	*
	* @param left The horizontal position to begin at (inclusive)
	* @param top The vertical position to begin at (inclusive)
	* @param width The width of the region
	* @param height The height of the region
	*/
	void setRegion(int left, int top, int width, int height);

	void rotate90();

	void rotate180();

	void mirror();

	/**
	* Find the rectangle that contains all non-white pixels. Useful for detection of 'pure' barcodes.
	*
	* @return True iff this rectangle is at least minWidth x minHeight pixels big
	*/
	bool findBoundingBox(int &left, int& top, int& width, int& height, int minSize = 1) const;

	int width() const { return _width; }

	int height() const { return _height; }

	bool empty() const { return _bits.empty(); }

	friend bool operator==(const BitMatrix& a, const BitMatrix& b)
	{
		return a._width == b._width && a._height == b._height && a._bits == b._bits;
	}

	template <typename T>
	bool isIn(PointT<T> p, int b = 0) const noexcept
	{
		return b <= p.x && p.x < width() - b && b <= p.y && p.y < height() - b;
	}

	bool get(PointI p) const { return get(p.x, p.y); }
	bool get(PointF p) const { return get(PointI(p)); }
	void set(PointI p, bool v = true) { set(p.x, p.y, v); }
	void set(PointF p, bool v = true) { set(PointI(p), v); }
};

void GetPatternRow(const BitMatrix& matrix, int r, std::vector<uint16_t>& pr, bool transpose);

/**
 * @brief Inflate scales a BitMatrix up and adds a quiet Zone plus padding
 * @param input matrix to be expanded
 * @param width new width in bits (pixel)
 * @param height new height in bits (pixel)
 * @param quietZone size of quiet zone to add in modules
 * @return expanded BitMatrix, maybe move(input) if size did not change
 */
BitMatrix Inflate(BitMatrix&& input, int width, int height, int quietZone);

/**
 * @brief Deflate (crop + subsample) a bit matrix
 * @param input matrix to be shrinked
 * @param width new width
 * @param height new height
 * @param top cropping starts at top row
 * @param left cropping starts at left col
 * @param subSampling typically the module size
 * @return deflated input
 */
BitMatrix Deflate(const BitMatrix& input, int width, int height, float top, float left, float subSampling);

template<typename T>
BitMatrix ToBitMatrix(const Matrix<T>& in, T trueValue = {true})
{
	BitMatrix out(in.width(), in.height());
	for (int y = 0; y < in.height(); ++y)
		for (int x = 0; x < in.width(); ++x)
			if (in.get(x, y) == trueValue)
				out.set(x, y);
	return out;
}

template<typename T>
Matrix<T> ToMatrix(const BitMatrix& in, T black = 0, T white = ~0)
{
	Matrix<T> res(in.width(), in.height());
	for (int y = 0; y < in.height(); ++y)
		for (int x = 0; x < in.width(); ++x)
			res.set(x, y, in.get(x, y) ? black : white);
	return res;
}

} // ZXing
