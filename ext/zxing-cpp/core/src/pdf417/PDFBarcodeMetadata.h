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
class BarcodeMetadata
{
	int _columnCount = 0;
	int _errorCorrectionLevel = 0;
	int _rowCountUpperPart = 0;
	int _rowCountLowerPart = 0;

public:
	BarcodeMetadata() = default;
	BarcodeMetadata(int columnCount, int rowCountUpperPart, int rowCountLowerPart, int errorCorrectionLevel)
	    : _columnCount(columnCount), _errorCorrectionLevel(errorCorrectionLevel), _rowCountUpperPart(rowCountUpperPart),
	      _rowCountLowerPart(rowCountLowerPart)
	{
	}

	int columnCount() const {
		return _columnCount;
	}

	int errorCorrectionLevel() const {
		return _errorCorrectionLevel;
	}

	int rowCount() const {
		return _rowCountUpperPart + _rowCountLowerPart;
	}

	int rowCountUpperPart() const {
		return _rowCountUpperPart;
	}

	int rowCountLowerPart() const {
		return _rowCountLowerPart;
	}

};

} // Pdf417
} // ZXing
