/*
* Copyright 2016 Huy Cuong Nguyen
* Copyright 2016 ZXing authors
*/
// SPDX-License-Identifier: Apache-2.0

#include "QRMatrixUtil.h"

#include "BitArray.h"
#include "BitHacks.h"
#include "QRDataMask.h"
#include "QRErrorCorrectionLevel.h"
#include "QRVersion.h"

#include <stdexcept>
#include <string>

namespace ZXing::QRCode {

// From Appendix D in JISX0510:2004 (p. 67)
static const int VERSION_INFO_POLY = 0x1f25;  // 1 1111 0010 0101

// From Appendix C in JISX0510:2004 (p.65).
static const int TYPE_INFO_POLY = 0x537;
static const int TYPE_INFO_MASK_PATTERN = 0x5412;

static void EmbedTimingPatterns(TritMatrix& matrix)
{
	// -8 is for skipping position detection patterns (size 7), and two horizontal/vertical
	// separation patterns (size 1). Thus, 8 = 7 + 1.
	for (int i = 8; i < matrix.width() - 8; ++i) {
		bool bit = (i + 1) % 2;
		// Horizontal line.
		matrix.set(i, 6, bit);
		// Vertical line.
		matrix.set(6, i, bit);
	}
}

// Note that we cannot unify the function with embedPositionDetectionPattern() despite they are
// almost identical, since we cannot write a function that takes 2D arrays in different sizes in
// C/C++. We should live with the fact.
static void EmbedPositionAdjustmentPattern(int xStart, int yStart, TritMatrix& matrix)
{
	for (int y = 0; y < 5; ++y)
		for (int x = 0; x < 5; ++x)
			matrix.set(xStart + x, yStart + y, maxAbsComponent(PointI(x, y) - PointI(2, 2)) != 1);
}

// Embed position adjustment patterns if need be.
static void EmbedPositionAdjustmentPatterns(const Version& version, TritMatrix& matrix)
{
	if (version.versionNumber() < 2) {  // The patterns appear if version >= 2
		return;
	}
	auto& coordinates = version.alignmentPatternCenters();
	for (int y : coordinates) {
		for (int x : coordinates) {
			// Check x/y is valid: don't place alignment patterns intersecting with the 3 finder patterns
			if ((x == 6 && y == 6) || (x == 6 && y == matrix.height() - 7) || (x == matrix.width() - 7 && y == 6))
				continue;

			// -2 is necessary since the x/y coordinates point to the center of the pattern, not the
			// left top corner.
			EmbedPositionAdjustmentPattern(x - 2, y - 2, matrix);
		}
	}
}

static void EmbedPositionDetectionPattern(int xStart, int yStart, TritMatrix& matrix)
{
	for (int y = 0; y < 7; ++y)
		for (int x = 0; x < 7; ++x)
			matrix.set(xStart + x, yStart + y, maxAbsComponent(PointI(x, y) - PointI(3, 3)) != 2);

	// Surround the 7x7 pattern with one line of white space (separation pattern)
	auto setIfInside = [&](int x, int y) {
		if( x >= 0 && x < matrix.width() && y >= 0 && y < matrix.height())
			matrix.set(x, y, 0);
	};

	for (int i = -1; i < 8; ++i) {
		setIfInside(xStart + i, yStart - 1); // top
		setIfInside(xStart + i, yStart + 7); // bottom
		setIfInside(xStart - 1, yStart + i); // left
		setIfInside(xStart + 7, yStart + i); // right
	}
}

// Embed position detection patterns and surrounding vertical/horizontal separators.
static void EmbedPositionDetectionPatternsAndSeparators(TritMatrix& matrix)
{
	// Left top corner.
	EmbedPositionDetectionPattern(0, 0, matrix);
	// Right top corner.
	EmbedPositionDetectionPattern(matrix.width() - 7, 0, matrix);
	// Left bottom corner.
	EmbedPositionDetectionPattern(0, matrix.width() - 7, matrix);
}


// Embed the lonely dark dot at left bottom corner. JISX0510:2004 (p.46)
static void EmbedDarkDotAtLeftBottomCorner(TritMatrix& matrix)
{
	matrix.set(8, matrix.height() - 8, 1);
}

// Return the position of the most significant bit set (to one) in the "value". The most
// significant bit is position 32. If there is no bit set, return 0. Examples:
// - findMSBSet(0) => 0
// - findMSBSet(1) => 1
// - findMSBSet(255) => 8
static int FindMSBSet(unsigned value)
{
	return 32 - BitHacks::NumberOfLeadingZeros(value);
}

// Calculate BCH (Bose-Chaudhuri-Hocquenghem) code for "value" using polynomial "poly". The BCH
// code is used for encoding type information and version information.
// Example: Calculation of version information of 7.
// f(x) is created from 7.
//   - 7 = 000111 in 6 bits
//   - f(x) = x^2 + x^1 + x^0
// g(x) is given by the standard (p. 67)
//   - g(x) = x^12 + x^11 + x^10 + x^9 + x^8 + x^5 + x^2 + 1
// Multiply f(x) by x^(18 - 6)
//   - f'(x) = f(x) * x^(18 - 6)
//   - f'(x) = x^14 + x^13 + x^12
// Calculate the remainder of f'(x) / g(x)
//         x^2
//         __________________________________________________
//   g(x) )x^14 + x^13 + x^12
//         x^14 + x^13 + x^12 + x^11 + x^10 + x^7 + x^4 + x^2
//         --------------------------------------------------
//                              x^11 + x^10 + x^7 + x^4 + x^2
//
// The remainder is x^11 + x^10 + x^7 + x^4 + x^2
// Encode it in binary: 110010010100
// The return value is 0xc94 (1100 1001 0100)
//
// Since all coefficients in the polynomials are 1 or 0, we can do the calculation by bit
// operations. We don't care if cofficients are positive or negative.
static int CalculateBCHCode(int value, int poly)
{
	// If poly is "1 1111 0010 0101" (version info poly), msbSetInPoly is 13. We'll subtract 1
	// from 13 to make it 12.
	int msbSetInPoly = FindMSBSet(poly);
	value <<= msbSetInPoly - 1;
	// Do the division business using exclusive-or operations.
	while (FindMSBSet(value) >= msbSetInPoly) {
		value ^= poly << (FindMSBSet(value) - msbSetInPoly);
	}
	// Now the "value" is the remainder (i.e. the BCH code)
	return value;
}

// Make bit vector of type information. On success, store the result in "bits" and return true.
// Encode error correction level and mask pattern. See 8.9 of
// JISX0510:2004 (p.45) for details.
static BitArray MakeTypeInfoBits(ErrorCorrectionLevel ecLevel, int maskPattern)
{
	if (maskPattern < 0 || maskPattern >= NUM_MASK_PATTERNS) {
		throw std::invalid_argument("Invalid mask pattern");
	}

	BitArray bits;
	int typeInfo = (BitsFromECLevel(ecLevel) << 3) | maskPattern;
	bits.appendBits(typeInfo, 5);

	int bchCode = CalculateBCHCode(typeInfo, TYPE_INFO_POLY);
	bits.appendBits(bchCode, 10);

	BitArray maskBits;
	maskBits.appendBits(TYPE_INFO_MASK_PATTERN, 15);
	bits.bitwiseXOR(maskBits);

	if (bits.size() != 15) {  // Just in case.
		throw std::logic_error("Should not happen but we got: " + std::to_string(bits.size()));
	}
	return bits;
}

// Embed type information. On success, modify the matrix.
static void EmbedTypeInfo(ErrorCorrectionLevel ecLevel, int maskPattern, TritMatrix& matrix)
{
	// Type info cells at the left top corner.
	constexpr PointI TYPE_INFO_COORDINATES[] = {
		{8, 0}, {8, 1}, {8, 2}, {8, 3}, {8, 4}, {8, 5}, {8, 7}, {8, 8},
		{7, 8}, {5, 8}, {4, 8}, {3, 8}, {2, 8}, {1, 8}, {0, 8}
	};

	BitArray typeInfoBits = MakeTypeInfoBits(ecLevel, maskPattern);

	for (int i = 0; i < typeInfoBits.size(); ++i) {
		// Place bits in LSB to MSB order.  LSB (least significant bit) is the last value in
		// "typeInfoBits".
		bool bit = typeInfoBits.get(typeInfoBits.size() - 1 - i);

		// Type info bits at the left top corner. See 8.9 of JISX0510:2004 (p.46).
		matrix.set(TYPE_INFO_COORDINATES[i], bit);

		if (i < 8) {
			// Right top corner.
			matrix.set(matrix.width() - i - 1, 8, bit);
		}
		else {
			// Left bottom corner.
			matrix.set(8, matrix.height() - 7 + (i - 8), bit);
		}
	}
}

// Make bit vector of version information. On success, store the result in "bits" and return true.
// See 8.10 of JISX0510:2004 (p.45) for details.
static BitArray MakeVersionInfoBits(const Version& version)
{
	BitArray bits;
	bits.appendBits(version.versionNumber(), 6);
	int bchCode = CalculateBCHCode(version.versionNumber(), VERSION_INFO_POLY);
	bits.appendBits(bchCode, 12);

	if (bits.size() != 18) {  // Just in case.
		throw std::logic_error("Should not happen but we got: " + std::to_string(bits.size()));
	}

	return bits;
}

// Embed version information if need be. On success, modify the matrix and return true.
// See 8.10 of JISX0510:2004 (p.47) for how to embed version information.
static void EmbedVersionInfo(const Version& version, TritMatrix& matrix)
{
	if (version.versionNumber() < 7) {  // Version info is necessary if version >= 7.
		return;  // Don't need version info.
	}

	BitArray versionInfoBits = MakeVersionInfoBits(version);

	int bitIndex = 6 * 3 - 1;  // It will decrease from 17 to 0.
	for (int i = 0; i < 6; ++i) {
		for (int j = 0; j < 3; ++j) {
			// Place bits in LSB (least significant bit) to MSB order.
			bool bit = versionInfoBits.get(bitIndex);
			bitIndex--;
			// Left bottom corner.
			matrix.set(i, matrix.height() - 11 + j, bit);
			// Right bottom corner.
			matrix.set(matrix.height() - 11 + j, i, bit);
		}
	}
}

// Embed "dataBits" using "getMaskPattern". On success, modify the matrix and return true.
// For debugging purposes, it skips masking process if "getMaskPattern" is -1.
// See 8.7 of JISX0510:2004 (p.38) for how to embed data bits.
static void EmbedDataBits(const BitArray& dataBits, int maskPattern, TritMatrix& matrix)
{
	int bitIndex = 0;
	int direction = -1;
	// Start from the right bottom cell.
	int x = matrix.width() - 1;
	int y = matrix.height() - 1;
	while (x > 0) {
		// Skip the vertical timing pattern.
		if (x == 6) {
			x -= 1;
		}
		while (y >= 0 && y < matrix.height()) {
			for (int xx = x; xx > x - 2; --xx) {
				// Skip the cell if it's not empty.
				if (!matrix.get(xx, y).isEmpty()) {
					continue;
				}
				// Padding bit. If there is no bit left, we'll fill the left cells with 0, as described
				// in 8.4.9 of JISX0510:2004 (p. 24).
				bool bit = bitIndex < dataBits.size() ? dataBits.get(bitIndex) : false;
				++bitIndex;

				// Skip masking if mask_pattern is -1.
				if (maskPattern != -1 && GetDataMaskBit(maskPattern, xx, y)) {
					bit = !bit;
				}
				matrix.set(xx, y, bit);
			}
			y += direction;
		}
		direction = -direction;  // Reverse the direction.
		y += direction;
		x -= 2;  // Move to the left.
	}
	// All bits should be consumed.
	if (bitIndex < dataBits.size()) {
		throw std::invalid_argument("Not all bits consumed: " + std::to_string(bitIndex) + '/' + std::to_string(dataBits.size()));
	}
}

// Build 2D matrix of QR Code from "dataBits" with "ecLevel", "version" and "getMaskPattern". On
// success, store the result in "matrix" and return true.
void BuildMatrix(const BitArray& dataBits, ErrorCorrectionLevel ecLevel, const Version& version, int maskPattern, TritMatrix& matrix)
{
	matrix.clear();
	// Let's get started with embedding big squares at corners.
	EmbedPositionDetectionPatternsAndSeparators(matrix);
	// Then, embed the dark dot at the left bottom corner.
	EmbedDarkDotAtLeftBottomCorner(matrix);
	// Position adjustment patterns appear if version >= 2.
	EmbedPositionAdjustmentPatterns(version, matrix);
	// Timing patterns should be embedded after position adj. patterns.
	EmbedTimingPatterns(matrix);
	// Type information appear with any version.
	EmbedTypeInfo(ecLevel, maskPattern, matrix);
	// Version info appear if version >= 7.
	EmbedVersionInfo(version, matrix);
	// Data should be embedded at end.
	EmbedDataBits(dataBits, maskPattern, matrix);
}

} // namespace ZXing::QRCode
