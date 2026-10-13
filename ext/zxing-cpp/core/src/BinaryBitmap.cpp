/*
* Copyright 2020 Axel Waggershauser
*/
// SPDX-License-Identifier: Apache-2.0

#include "BinaryBitmap.h"

#include "BitMatrix.h"

#include <mutex>

namespace ZXing {

struct BinaryBitmap::Cache
{
	std::once_flag once;
	std::shared_ptr<const BitMatrix> matrix;
};

BitMatrix BinaryBitmap::binarize(const uint8_t threshold) const
{
	BitMatrix res(width(), height());

	if (_buffer.pixStride() == 1 && _buffer.rowStride() == _buffer.width()) {
		// Specialize for a packed buffer with pixStride 1 to support auto vectorization (16x speedup on AVX2)
		auto dst = res.row(0).begin();
		for (auto src = _buffer.data(0, 0), end = _buffer.data(0, height()); src != end; ++src, ++dst)
			*dst = (*src <= threshold) * BitMatrix::SET_V;
	} else {
		auto processLine = [&res, threshold](int y, const auto* src, const int stride) {
			for (auto& dst : res.row(y)) {
				dst = (*src <= threshold) * BitMatrix::SET_V;
				src += stride;
			}
		};
		for (int y = 0; y < res.height(); ++y) {
			auto src = _buffer.data(0, y) + GreenIndex(_buffer.format());
			// Specialize the inner loop for strides 1 and 4 to support auto vectorization
			switch (_buffer.pixStride()) {
			case 1: processLine(y, src, 1); break;
			case 4: processLine(y, src, 4); break;
			default: processLine(y, src, _buffer.pixStride()); break;
			}
		}
	}

	return res;
}

BinaryBitmap::BinaryBitmap(const ImageView& buffer) : _cache(new Cache), _buffer(buffer) {}

BinaryBitmap::~BinaryBitmap() = default;

const BitMatrix* BinaryBitmap::getBitMatrix() const
{
	std::call_once(_cache->once, [&](){_cache->matrix = getBlackMatrix();});
	return _cache->matrix.get();
}

void BinaryBitmap::invert()
{
	if (_cache->matrix) {
		auto matrix = const_cast<BitMatrix*>(_cache->matrix.get());
		matrix->flipAll();
	}
	_inverted = true;
}

template <typename F>
void SumFilter(const BitMatrix& in, BitMatrix& out, F func)
{
	assert(in.height() >= 3);

	const auto* in0 = in.row(0).begin();
	const auto* in1 = in.row(1).begin();
	const auto* in2 = in.row(2).begin();

	for (auto *out1 = out.row(1).begin() + 1, *end = out.row(out.height() - 1).begin() - 1; out1 != end; ++in0, ++in1, ++in2, ++out1) {
		int sum = 0;
		for (int j = 0; j < 3; ++j)
			sum += in0[j] + in1[j] + in2[j];

		*out1 = func(sum);
	}
}

void BinaryBitmap::close()
{
	if (_cache->matrix) {
		auto& matrix = *const_cast<BitMatrix*>(_cache->matrix.get());
		BitMatrix tmp(matrix.width(), matrix.height());

		// dilate
		SumFilter(matrix, tmp, [](int sum) { return (sum > 0 * BitMatrix::SET_V) * BitMatrix::SET_V; });
		// erode
		SumFilter(tmp, matrix, [](int sum) { return (sum == 9 * BitMatrix::SET_V) * BitMatrix::SET_V; });
	}
	_closed = true;
}

} // ZXing
