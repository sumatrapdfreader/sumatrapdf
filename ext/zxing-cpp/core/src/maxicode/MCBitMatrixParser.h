/*
* Copyright 2016 Nu-book Inc.
* Copyright 2016 ZXing authors
*/
// SPDX-License-Identifier: Apache-2.0

#pragma once

namespace ZXing {

class ByteArray;
class BitMatrix;

namespace MaxiCode {

/**
* @author mike32767
* @author Manuel Kasten
*/
class BitMatrixParser
{
public:
	static ByteArray ReadCodewords(const BitMatrix& image);

	static const int MATRIX_WIDTH = 30;
	static const int MATRIX_HEIGHT = 33;
};

} // MaxiCode
} // ZXing
