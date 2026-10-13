/*
* Copyright 2016 Nu-book Inc.
* Copyright 2016 ZXing authors
*/
// SPDX-License-Identifier: Apache-2.0

#pragma once

namespace ZXing {
namespace Pdf417 {

/**
* @author Guenther Grau
*/
class Codeword
{
	static const int BARCODE_ROW_UNKNOWN = -1;

	int _startX = 0;
	int _endX = 0;
	int _bucket = 0;
	int _value = 0;
	int _rowNumber = BARCODE_ROW_UNKNOWN;

public:
	Codeword() {}
	Codeword(int startX, int endX, int bucket, int value) : _startX(startX), _endX(endX), _bucket(bucket), _value(value) {}

	bool hasValidRowNumber() const {
		return isValidRowNumber(_rowNumber);
	}

	bool isValidRowNumber(int rowNumber) const {
		return rowNumber != BARCODE_ROW_UNKNOWN && _bucket == (rowNumber % 3) * 3;
	}

	void setRowNumberAsRowIndicatorColumn() {
		_rowNumber = (_value / 30) * 3 + _bucket / 3;
	}

	int width() const {
		return _endX - _startX;
	}

	int startX() const {
		return _startX;
	}

	int endX() const {
		return _endX;
	}

	int bucket() const {
		return _bucket;
	}

	int value() const {
		return _value;
	}

	int rowNumber() const {
		return _rowNumber;
	}

	void setRowNumber(int rowNumber) {
		_rowNumber = rowNumber;
	}
};

} // Pdf417
} // ZXing
