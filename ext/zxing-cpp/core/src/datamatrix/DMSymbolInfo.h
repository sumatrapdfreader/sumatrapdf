/*
* Copyright 2016 Huy Cuong Nguyen
* Copyright 2016 ZXing authors
*/
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "DMSymbolShape.h"

namespace ZXing::DataMatrix {

class SymbolInfo
{
	bool _rectangular;
	int _dataCapacity;
	int _errorCodewords;
	int _matrixWidth;
	int _matrixHeight;
	int _dataRegions;
	int _rsBlockData;
	int _rsBlockError;

public:
	constexpr SymbolInfo(bool rectangular, int dataCapacity, int errorCodewords, int matrixWidth, int matrixHeight, int dataRegions)
		: SymbolInfo(rectangular, dataCapacity, errorCodewords, matrixWidth, matrixHeight, dataRegions, dataCapacity, errorCodewords)
	{}

	constexpr SymbolInfo(bool rectangular, int dataCapacity, int errorCodewords, int matrixWidth, int matrixHeight, int dataRegions,
						 int rsBlockData, int rsBlockError)
		: _rectangular(rectangular),
		  _dataCapacity(dataCapacity),
		  _errorCodewords(errorCodewords),
		  _matrixWidth(matrixWidth),
		  _matrixHeight(matrixHeight),
		  _dataRegions(dataRegions),
		  _rsBlockData(rsBlockData),
		  _rsBlockError(rsBlockError)
	{}

	static const SymbolInfo* Lookup(int dataCodewords);
	static const SymbolInfo* Lookup(int dataCodewords, SymbolShape shape);
	static const SymbolInfo* Lookup(int dataCodewords, bool allowRectangular);
	static const SymbolInfo* Lookup(int dataCodewords, SymbolShape shape, int minWidth, int minHeight, int maxWidth, int maxHeight);

	int horizontalDataRegions() const;

	int verticalDataRegions() const;

	int symbolDataWidth() const { return horizontalDataRegions() * _matrixWidth; }

	int symbolDataHeight() const { return verticalDataRegions() * _matrixHeight; }

	int symbolWidth() const { return symbolDataWidth() + (horizontalDataRegions() * 2); }

	int symbolHeight() const { return symbolDataHeight() + (verticalDataRegions() * 2); }

	int matrixWidth() const { return _matrixWidth; }

	int matrixHeight() const { return _matrixHeight; }

	int codewordCount() const { return _dataCapacity + _errorCodewords; }

	int interleavedBlockCount() const { return _rsBlockData > 0 ? _dataCapacity / _rsBlockData : 10; /* Symbol 144 */ }

	int dataCapacity() const { return _dataCapacity; }

	int errorCodewords() const { return _errorCodewords; }

	int dataLengthForInterleavedBlock(int index) const
	{
		return _rsBlockData > 0 ? _rsBlockData : (index <= 8 ? 156 : 155); /* Symbol 144 */
	}

	int errorLengthForInterleavedBlock() const { return _rsBlockError; }
};

} // namespace ZXing::DataMatrix
