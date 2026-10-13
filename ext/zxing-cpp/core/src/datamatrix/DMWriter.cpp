/*
* Copyright 2016 Huy Cuong Nguyen
* Copyright 2016 ZXing authors
*/
// SPDX-License-Identifier: Apache-2.0

#include "DMWriter.h"

#include "BitMatrix.h"
#include "ByteArray.h"
#include "CharacterSet.h"
#include "DMBitLayout.h"
#include "DMECEncoder.h"
#include "DMHighLevelEncoder.h"
#include "DMSymbolInfo.h"
#include "Utf.h"

#include <stdexcept>
#include <string>
#include <utility>

namespace ZXing::DataMatrix {

/**
* Encode the given symbol info to a bit matrix.
*
* @param placement  The DataMatrix placement.
* @param symbolInfo The symbol info to encode.
* @return The bit matrix generated.
*/
static BitMatrix EncodeLowLevel(const BitMatrix& placement, const SymbolInfo& symbolInfo) {
	int symbolWidth = symbolInfo.symbolDataWidth();
	int symbolHeight = symbolInfo.symbolDataHeight();

	BitMatrix matrix(symbolInfo.symbolWidth(), symbolInfo.symbolHeight());
	int matrixY = 0;
	for (int y = 0; y < symbolHeight; y++) {
		// Fill the top edge with alternate 0 / 1
		int matrixX;
		if ((y % symbolInfo.matrixHeight()) == 0) {
			matrixX = 0;
			for (int x = 0; x < matrix.width(); x++) {
				matrix.set(matrixX, matrixY, (x % 2) == 0);
				matrixX++;
			}
			matrixY++;
		}
		matrixX = 0;
		for (int x = 0; x < symbolWidth; x++) {
			// Fill the right edge with full 1
			if ((x % symbolInfo.matrixWidth()) == 0) {
				matrix.set(matrixX, matrixY, true);
				matrixX++;
			}
			matrix.set(matrixX, matrixY, placement.get(x, y) == 1);
			matrixX++;
			// Fill the right edge with alternate 0 / 1
			if ((x % symbolInfo.matrixWidth()) == symbolInfo.matrixWidth() - 1) {
				matrix.set(matrixX, matrixY, (y % 2) == 0);
				matrixX++;
			}
		}
		matrixY++;
		// Fill the bottom edge with full 1
		if ((y % symbolInfo.matrixHeight()) == symbolInfo.matrixHeight() - 1) {
			matrixX = 0;
			for (int x = 0; x < matrix.width(); x++) {
				matrix.set(matrixX, matrixY, true);
				matrixX++;
			}
			matrixY++;
		}
	}

	return matrix;
}

Writer::Writer() :
	_shapeHint(SymbolShape::NONE),
	_encoding(CharacterSet::Unknown)
{
}

BitMatrix
Writer::encode(const std::wstring& contents, int width, int height) const
{
	if (contents.empty()) {
		throw std::invalid_argument("Found empty contents");
	}

	if (width < 0 || height < 0) {
		throw std::invalid_argument("Requested dimensions are invalid");
	}

	//1. step: Data encodation
	auto encoded = Encode(contents, _encoding, _shapeHint, _minWidth, _minHeight, _maxWidth, _maxHeight);
	const SymbolInfo* symbolInfo = SymbolInfo::Lookup(Size(encoded), _shapeHint, _minWidth, _minHeight, _maxWidth, _maxHeight);
	if (symbolInfo == nullptr) {
		throw std::invalid_argument("Can't find a symbol arrangement that matches the message. Data codewords: " + std::to_string(encoded.size()));
	}

	//2. step: ECC generation
	EncodeECC200(encoded, *symbolInfo);

	//3. step: Module placement in Matrix
	BitMatrix symbolData = BitMatrixFromCodewords(encoded, symbolInfo->symbolDataWidth(), symbolInfo->symbolDataHeight());

	//4. step: low-level encoding
	BitMatrix result = EncodeLowLevel(symbolData, *symbolInfo);

	//5. step: scale-up to requested size, minimum required quiet zone is 1
	return Inflate(std::move(result), width, height, _quietZone);
}

BitMatrix Writer::encode(const std::string& contents, int width, int height) const
{
	return encode(FromUtf8(contents), width, height);
}

} // namespace ZXing::DataMatrix
