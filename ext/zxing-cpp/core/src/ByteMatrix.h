/*
* Copyright 2016 Huy Cuong Nguyen
* Copyright 2016 ZXing authors
*/
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "Matrix.h"

#include <cstdint>

namespace ZXing {

// TODO: If kept at all, this should be replaced by `using ByteMatrix = Matrix<uint8_t>;` to be consistent with ByteArray
// This non-template class is kept for now to stay source-compatible with older versions of the library.
// [[deprecated]]
class ByteMatrix : public Matrix<int8_t>
{
public:
	ByteMatrix() = default;
	ByteMatrix(int width, int height, int8_t val = 0) : Matrix<int8_t>(width, height, val) {}
	ByteMatrix(ByteMatrix&&) noexcept = default;
	ByteMatrix& operator=(ByteMatrix&&) noexcept = default;
};

} // ZXing
