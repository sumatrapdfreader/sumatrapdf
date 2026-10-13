/*
* Copyright 2016 Nu-book Inc.
* Copyright 2016 ZXing authors
* Copyright 2023 gitlost
*/
// SPDX-License-Identifier: Apache-2.0

#include "QRBitMatrixParser.h"

#include "BitArray.h"
#include "BitMatrix.h"
#include "ByteArray.h"
#include "QRDataMask.h"
#include "QRFormatInformation.h"
#include "QRVersion.h"

#include <utility>

namespace ZXing::QRCode {

static bool getBit(const BitMatrix& bitMatrix, int x, int y, bool mirrored = false)
{
	return mirrored ? bitMatrix.get(y, x) : bitMatrix.get(x, y);
}

const Version* ReadVersion(const BitMatrix& bitMatrix, Type type)
{
	assert(Version::HasValidSize(bitMatrix));

	int number = Version::Number(bitMatrix);

	switch (type) {
	case Type::Micro: return Version::Micro(number);
	case Type::rMQR: return Version::rMQR(number);
	case Type::Model1: return Version::Model1(number);
	case Type::Model2: return Version::Model2(number);
	}

	return nullptr;
}

FormatInformation ReadFormatInformation(const BitMatrix& bitMatrix)
{
	if (Version::HasValidSize(bitMatrix, Type::Micro)) {
		// Read top-left format info bits
		int formatInfoBits = 0;
		for (int x = 1; x < 9; x++)
			AppendBit(formatInfoBits, getBit(bitMatrix, x, 8));
		for (int y = 7; y >= 1; y--)
			AppendBit(formatInfoBits, getBit(bitMatrix, 8, y));

		return FormatInformation::DecodeMQR(formatInfoBits);
	}
	if (Version::HasValidSize(bitMatrix, Type::rMQR)) {
		// Read top-left format info bits
		uint32_t formatInfoBits1 = 0;
		for (int y = 3; y >= 1; y--)
			AppendBit(formatInfoBits1, getBit(bitMatrix, 11, y));
		for (int x = 10; x >= 8; x--)
			for (int y = 5; y >= 1; y--)
				AppendBit(formatInfoBits1, getBit(bitMatrix, x, y));

		// Read bottom-right format info bits
		uint32_t formatInfoBits2 = 0;
		const int width = bitMatrix.width();
		const int height = bitMatrix.height();
		for (int x = 3; x <= 5; x++)
			AppendBit(formatInfoBits2, getBit(bitMatrix, width - x, height - 6));
		for (int x = 6; x <= 8; x++)
			for (int y = 2; y <= 6; y++)
				AppendBit(formatInfoBits2, getBit(bitMatrix, width - x, height - y));

		return FormatInformation::DecodeRMQR(formatInfoBits1, formatInfoBits2);
	}

	// Read top-left format info bits
	int formatInfoBits1 = 0;
	for (int x = 0; x < 6; x++)
		AppendBit(formatInfoBits1, getBit(bitMatrix, x, 8));
	// .. and skip a bit in the timing pattern ...
	AppendBit(formatInfoBits1, getBit(bitMatrix, 7, 8));
	AppendBit(formatInfoBits1, getBit(bitMatrix, 8, 8));
	AppendBit(formatInfoBits1, getBit(bitMatrix, 8, 7));
	// .. and skip a bit in the timing pattern ...
	for (int y = 5; y >= 0; y--)
		AppendBit(formatInfoBits1, getBit(bitMatrix, 8, y));

	// Read the top-right/bottom-left pattern including the 'Dark Module' from the bottom-left
	// part that has to be considered separately when looking for mirrored symbols.
	// See also FormatInformation::DecodeQR
	int dimension = bitMatrix.height();
	int formatInfoBits2 = 0;
	for (int y = dimension - 1; y >= dimension - 8; y--)
		AppendBit(formatInfoBits2, getBit(bitMatrix, 8, y));
	for (int x = dimension - 8; x < dimension; x++)
		AppendBit(formatInfoBits2, getBit(bitMatrix, x, 8));

	return FormatInformation::DecodeQR(formatInfoBits1, formatInfoBits2);
}

static ByteArray ReadQRCodewords(const BitMatrix& bitMatrix, const Version& version, const FormatInformation& formatInfo)
{
	BitMatrix functionPattern = version.buildFunctionPattern();

	ByteArray result;
	result.reserve(version.totalCodewords());
	uint8_t currentByte = 0;
	bool readingUp = true;
	int bitsRead = 0;
	int dimension = bitMatrix.height();
	// Read columns in pairs, from right to left
	for (int x = dimension - 1; x > 0; x -= 2) {
		// Skip whole column with vertical timing pattern.
		if (x == 6)
			x--;
		// Read alternatingly from bottom to top then top to bottom
		for (int row = 0; row < dimension; row++) {
			int y = readingUp ? dimension - 1 - row : row;
			for (int col = 0; col < 2; col++) {
				int xx = x - col;
				// Ignore bits covered by the function pattern
				if (!functionPattern.get(xx, y)) {
					// Read a bit
					AppendBit(currentByte,
							  GetDataMaskBit(formatInfo.dataMask, xx, y) != getBit(bitMatrix, xx, y, formatInfo.isMirrored));
					// If we've made a whole byte, save it off
					if (++bitsRead % 8 == 0)
						result.push_back(std::exchange(currentByte, 0));
				}
			}
		}
		readingUp = !readingUp; // switch directions
	}
	if (Size(result) != version.totalCodewords())
		return {};

	return result;
}

static ByteArray ReadQRCodewordsModel1(const BitMatrix& bitMatrix, const Version& version, const FormatInformation& formatInfo)
{
	ByteArray result;
	result.reserve(version.totalCodewords());
	int dimension = bitMatrix.height();
	int columns = dimension / 4 + 1 + 2;
	for (int j = 0; j < columns; j++) {
		if (j <= 1) { // vertical symbols on the right side
			int rows = (dimension - 8) / 4;
			for (int i = 0; i < rows; i++) {
				if (j == 0 && i % 2 == 0 && i > 0 && i < rows - 1) // extension
					continue;
				int x = (dimension - 1) - (j * 2);
				int y = (dimension - 1) - (i * 4);
				uint8_t currentByte = 0;
				for (int b = 0; b < 8; b++) {
					AppendBit(currentByte, GetDataMaskBit(formatInfo.dataMask, x - b % 2, y - (b / 2))
											   != getBit(bitMatrix, x - b % 2, y - (b / 2), formatInfo.isMirrored));
				}
				result.push_back(currentByte);
			}
		} else if (columns - j <= 4) { // vertical symbols on the left side
			int rows = (dimension - 16) / 4;
			for (int i = 0; i < rows; i++) {
				int x = (columns - j - 1) * 2 + 1 + (columns - j == 4 ? 1 : 0); // timing
				int y = (dimension - 1) - 8 - (i * 4);
				uint8_t currentByte = 0;
				for (int b = 0; b < 8; b++) {
					AppendBit(currentByte, GetDataMaskBit(formatInfo.dataMask, x - b % 2, y - (b / 2))
											   != getBit(bitMatrix, x - b % 2, y - (b / 2), formatInfo.isMirrored));
				}
				result.push_back(currentByte);
			}
		} else { // horizontal symbols
			int rows = dimension / 2;
			for (int i = 0; i < rows; i++) {
				if (j == 2 && i >= rows - 4) // alignment & finder
					continue;
				if (i == 0 && j % 2 == 1 && j + 1 != columns - 4) // extension
					continue;
				int x = (dimension - 1) - (2 * 2) - (j - 2) * 4;
				int y = (dimension - 1) - (i * 2) - (i >= rows - 3 ? 1 : 0); // timing
				uint8_t currentByte = 0;
				for (int b = 0; b < 8; b++) {
					AppendBit(currentByte, GetDataMaskBit(formatInfo.dataMask, x - b % 4, y - (b / 4))
											   != getBit(bitMatrix, x - b % 4, y - (b / 4), formatInfo.isMirrored));
				}
				result.push_back(currentByte);
			}
		}
	}

	result[0] &= 0xf; // ignore corner
	if (Size(result) != version.totalCodewords())
		return {};

	return result;
}

static ByteArray ReadMQRCodewords(const BitMatrix& bitMatrix, const QRCode::Version& version, const FormatInformation& formatInfo)
{
	BitMatrix functionPattern = version.buildFunctionPattern();

	// D3 in a Version M1 symbol, D11 in a Version M3-L symbol and D9
	// in a Version M3-M symbol is a 2x2 square 4-module block.
	// See ISO 18004:2006 6.7.3.
	bool hasD4mBlock = version.versionNumber() % 2 == 1;
	int d4mBlockIndex =
		version.versionNumber() == 1 ? 3 : (formatInfo.ecLevel == QRCode::ErrorCorrectionLevel::Low ? 11 : 9);

	ByteArray result;
	result.reserve(version.totalCodewords());
	uint8_t currentByte = 0;
	bool readingUp = true;
	int bitsRead = 0;
	int dimension = bitMatrix.height();
	// Read columns in pairs, from right to left
	for (int x = dimension - 1; x > 0; x -= 2) {
		// Read alternatingly from bottom to top then top to bottom
		for (int row = 0; row < dimension; row++) {
			int y = readingUp ? dimension - 1 - row : row;
			for (int col = 0; col < 2; col++) {
				int xx = x - col;
				// Ignore bits covered by the function pattern
				if (!functionPattern.get(xx, y)) {
					// Read a bit
					AppendBit(currentByte,
							  GetDataMaskBit(formatInfo.dataMask, xx, y, true) != getBit(bitMatrix, xx, y, formatInfo.isMirrored));
					++bitsRead;
					// If we've made a whole byte, save it off; save early if 2x2 data block.
					if (bitsRead == 8 || (bitsRead == 4 && hasD4mBlock && Size(result) == d4mBlockIndex - 1)) {
						result.push_back(std::exchange(currentByte, 0));
						bitsRead = 0;
					}
				}
			}
		}
		readingUp = !readingUp; // switch directions
	}
	if (Size(result) != version.totalCodewords())
		return {};

	return result;
}

static ByteArray ReadRMQRCodewords(const BitMatrix& bitMatrix, const Version& version, const FormatInformation& formatInfo)
{
	BitMatrix functionPattern = version.buildFunctionPattern();

	ByteArray result;
	result.reserve(version.totalCodewords());
	uint8_t currentByte = 0;
	bool readingUp = true;
	int bitsRead = 0;
	const int width = bitMatrix.width();
	const int height = bitMatrix.height();
	// Read columns in pairs, from right to left
	for (int x = width - 1 - 1; x > 0; x -= 2) { // Skip right edge alignment
		// Read alternatingly from bottom to top then top to bottom
		for (int row = 0; row < height; row++) {
			int y = readingUp ? height - 1 - row : row;
			for (int col = 0; col < 2; col++) {
				int xx = x - col;
				// Ignore bits covered by the function pattern
				if (!functionPattern.get(xx, y)) {
					// Read a bit
					AppendBit(currentByte,
							  GetDataMaskBit(formatInfo.dataMask, xx, y) != getBit(bitMatrix, xx, y, formatInfo.isMirrored));
					// If we've made a whole byte, save it off
					if (++bitsRead % 8 == 0)
						result.push_back(std::exchange(currentByte, 0));
				}
			}
		}
		readingUp = !readingUp; // switch directions
	}
	if (Size(result) != version.totalCodewords())
		return {};

	return result;
}

ByteArray ReadCodewords(const BitMatrix& bitMatrix, const Version& version, const FormatInformation& formatInfo)
{
	switch (version.type()) {
	case Type::Micro: return ReadMQRCodewords(bitMatrix, version, formatInfo);
	case Type::rMQR: return ReadRMQRCodewords(bitMatrix, version, formatInfo);
	case Type::Model1: return ReadQRCodewordsModel1(bitMatrix, version, formatInfo);
	case Type::Model2: return ReadQRCodewords(bitMatrix, version, formatInfo);
	}

	return {};
}

} // namespace ZXing::QRCode
