/*
* Copyright 2016 Nu-book Inc.
* Copyright 2016 ZXing authors
*/
// SPDX-License-Identifier: Apache-2.0

#include "PDFDetector.h"
#include "BinaryBitmap.h"
#include "BitMatrix.h"
#include "ZXNullable.h"
#include "Pattern.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <limits>
#include <list>
#include <vector>

namespace ZXing {
namespace Pdf417 {

static const int INDEXES_START_PATTERN[] = { 0, 4, 1, 5 };
static const int INDEXES_STOP_PATTERN[] = { 6, 2, 7, 3 };
static const float MAX_AVG_VARIANCE = 0.42f;
static const float MAX_INDIVIDUAL_VARIANCE = 0.8f;

static const int MAX_PIXEL_DRIFT = 3;
static const int MAX_PATTERN_DRIFT = 5;
// if we set the value too low, then we don't detect the correct height of the bar if the start patterns are damaged.
// if we set the value too high, then we might detect the start pattern from a neighbor barcode.
static const int SKIPPED_ROW_COUNT_MAX = 25;
// A PDF471 barcode should have at least 3 rows, with each row being >= 3 times the module width. Therefore it should be at least
// 9 pixels tall. To be conservative, we use about half the size to ensure we don't miss it.
static const int ROW_STEP = 8; // used to be 5, but 8 is enough for conforming symbols
static const int BARCODE_MIN_HEIGHT = 10;

/**
* Determines how closely a set of observed counts of runs of black/white
* values matches a given target pattern. This is reported as the ratio of
* the total variance from the expected pattern proportions across all
* pattern elements, to the length of the pattern.
*
* @param counters observed counters
* @param pattern expected pattern
* @param maxIndividualVariance The most any counter can differ before we give up
* @return ratio of total variance between counters and pattern compared to total pattern size
*/
static float
PatternMatchVariance(const std::vector<int>& counters, const std::vector<int>& pattern, float maxIndividualVariance)
{
	int total = 0;
	int patternLength = 0;
	for (size_t i = 0; i < counters.size(); i++) {
		total += counters[i];
		patternLength += pattern[i];
	}
	if (total < patternLength) {
		// If we don't even have one pixel per unit of bar width, assume this
		// is too small to reliably match, so fail:
		return std::numeric_limits<float>::max();
	}
	// We're going to fake floating-point math in integers. We just need to use more bits.
	// Scale up patternLength so that intermediate values below like scaledCounter will have
	// more "significant digits".
	float unitBarWidth = (float)total / patternLength;
	maxIndividualVariance *= unitBarWidth;

	float totalVariance = 0.0f;
	for (size_t x = 0; x < counters.size(); x++) {
		int counter = counters[x];
		float scaledPattern = pattern[x] * unitBarWidth;
		float variance = counter > scaledPattern ? counter - scaledPattern : scaledPattern - counter;
		if (variance > maxIndividualVariance) {
			return std::numeric_limits<float>::max();
		}
		totalVariance += variance;
	}
	return totalVariance / total;
}


/**
* @param matrix row of black/white values to search
* @param column x position to start search
* @param row y position to start search
* @param width the number of pixels to search on this row
* @param pattern pattern of counts of number of black and white pixels that are
*                 being searched for as a pattern
* @param counters array of counters, as long as pattern, to re-use
* @return start/end horizontal offset of guard pattern, as an array of two ints.
*/
static bool
FindGuardPattern(const BitMatrix& matrix, int column, int row, int width, bool whiteFirst, const std::vector<int>& pattern, std::vector<int>& counters, int& startPos, int& endPos)
{
	std::fill(counters.begin(), counters.end(), 0);
	int patternLength = Size(pattern);
	bool isWhite = whiteFirst;
	int patternStart = column;
	int pixelDrift = 0;

	// if there are black pixels left of the current pixel shift to the left, but only for MAX_PIXEL_DRIFT pixels 
	while (matrix.get(patternStart, row) && patternStart > 0 && pixelDrift++ < MAX_PIXEL_DRIFT) {
		patternStart--;
	}
	int x = patternStart;
	int counterPosition = 0;
	for (; x < width; x++) {
		bool pixel = matrix.get(x, row);
		if (pixel != isWhite) {
			counters[counterPosition]++;
		}
		else {
			if (counterPosition == patternLength - 1) {
				if (PatternMatchVariance(counters, pattern, MAX_INDIVIDUAL_VARIANCE) < MAX_AVG_VARIANCE) {
					startPos = patternStart;
					endPos = x;
					return true;
				}
				patternStart += counters[0] + counters[1];
				std::copy(counters.begin() + 2, counters.end(), counters.begin());
				counters[patternLength - 2] = 0;
				counters[patternLength - 1] = 0;
				counterPosition--;
			}
			else {
				counterPosition++;
			}
			counters[counterPosition] = 1;
			isWhite = !isWhite;
		}
	}
	if (counterPosition == patternLength - 1) {
		if (PatternMatchVariance(counters, pattern, MAX_INDIVIDUAL_VARIANCE) < MAX_AVG_VARIANCE) {
			startPos = patternStart;
			endPos = x - 1;
			return true;
		}
	}
	return false;
}

static std::array<Nullable<ResultPoint>, 4>&
FindRowsWithPattern(const BitMatrix& matrix, int height, int width, int startRow, int startColumn, const std::vector<int>& pattern, std::array<Nullable<ResultPoint>, 4>& result)
{
	bool found = false;
	int startPos, endPos;
	int minStartRow = startRow;
	std::vector<int> counters(pattern.size(), 0);
	for (; startRow < height; startRow += ROW_STEP) {
		if (FindGuardPattern(matrix, startColumn, startRow, width, false, pattern, counters, startPos, endPos)) {
			while (startRow > minStartRow + 1) {
				if (!FindGuardPattern(matrix, startColumn, --startRow, width, false, pattern, counters, startPos, endPos)) {
					startRow++;
					break;
				}
			}
			result[0] = ResultPoint(startPos, startRow);
			result[1] = ResultPoint(endPos, startRow);
			found = true;
			break;
		}
	}
	int stopRow = startRow + 1;
	// Last row of the current symbol that contains pattern
	if (found) {
		int skippedRowCount = 0;
		int previousRowStart = static_cast<int>(result[0].value().x());
		int previousRowEnd = static_cast<int>(result[1].value().x());
		for (; stopRow < height; stopRow++) {
			int startPos, endPos;
			found = FindGuardPattern(matrix, previousRowStart, stopRow, width, false, pattern, counters, startPos, endPos);
			// a found pattern is only considered to belong to the same barcode if the start and end positions
			// don't differ too much. Pattern drift should be not bigger than two for consecutive rows. With
			// a higher number of skipped rows drift could be larger. To keep it simple for now, we allow a slightly
			// larger drift and don't check for skipped rows.
			if (found && std::abs(previousRowStart - startPos) < MAX_PATTERN_DRIFT && std::abs(previousRowEnd - endPos) < MAX_PATTERN_DRIFT) {
				previousRowStart = startPos;
				previousRowEnd = endPos;
				skippedRowCount = 0;
			}
			else if (skippedRowCount > SKIPPED_ROW_COUNT_MAX) {
				break;
			}
			else {
				skippedRowCount++;
			}
		}
		stopRow -= skippedRowCount + 1;
		result[2] = ResultPoint(previousRowStart, stopRow);
		result[3] = ResultPoint(previousRowEnd, stopRow);
	}
	if (stopRow - startRow < BARCODE_MIN_HEIGHT) {
		std::fill(result.begin(), result.end(), nullptr);
	}
	return result;
}

static void
CopyToResult(std::array<Nullable<ResultPoint>, 8>& result, const std::array<Nullable<ResultPoint>, 4>& tmpResult, const int destinationIndexes[4])
{
	for (int i = 0; i < 4; i++) {
		result[destinationIndexes[i]] = tmpResult[i];
	}
}

/**
* Locate the vertices and the codewords area of a black blob using the Start
* and Stop patterns as locators.
*
* @param matrix the scanned barcode image.
* @return an array containing the vertices:
*           vertices[0] x, y top left barcode
*           vertices[1] x, y bottom left barcode
*           vertices[2] x, y top right barcode
*           vertices[3] x, y bottom right barcode
*           vertices[4] x, y top left codeword area
*           vertices[5] x, y bottom left codeword area
*           vertices[6] x, y top right codeword area
*           vertices[7] x, y bottom right codeword area
*/
static std::array<Nullable<ResultPoint>, 8> FindVertices(const BitMatrix& matrix, int startRow, int startColumn)
{
	// B S B S B S B S Bar/Space pattern
	// 11111111 0 1 0 1 0 1 000
	static const std::vector<int> START_PATTERN = { 8, 1, 1, 1, 1, 1, 1, 3 };
	// 1111111 0 1 000 1 0 1 00 1
	static const std::vector<int> STOP_PATTERN = { 7, 1, 1, 3, 1, 1, 1, 2, 1 };

	int width = matrix.width();
	int height = matrix.height();

	std::array<Nullable<ResultPoint>, 4> tmp;
	std::array<Nullable<ResultPoint>, 8> result;
	CopyToResult(result, FindRowsWithPattern(matrix, height, width, startRow, startColumn, START_PATTERN, tmp), INDEXES_START_PATTERN);

	if (result[4] != nullptr) {
		startColumn = static_cast<int>(result[4].value().x());
		startRow = static_cast<int>(result[4].value().y());
#if 1 // 2x speed improvement for images with no PDF417 symbol by not looking for symbols without start guard (which are not conforming to spec anyway)
		CopyToResult(result, FindRowsWithPattern(matrix, height, width, startRow, startColumn, STOP_PATTERN, tmp), INDEXES_STOP_PATTERN);
	}
#else
	}
	CopyToResult(result, FindRowsWithPattern(matrix, height, width, startRow, startColumn, STOP_PATTERN, tmp), INDEXES_STOP_PATTERN);
#endif
	return result;
}

/**
* Detects PDF417 codes in an image. Only checks 0 degree rotation
* @param multiple if true, then the image is searched for multiple codes. If false, then at most one code will
* be found and returned
* @param bitMatrix bit matrix to detect barcodes in
* @return List of ResultPoint arrays containing the coordinates of found barcodes
*/
static std::list<std::array<Nullable<ResultPoint>, 8>> DetectBarcode(const BitMatrix& bitMatrix, bool multiple)
{
	int row = 0;
	int column = 0;
	bool foundBarcodeInRow = false;
	std::list<std::array<Nullable<ResultPoint>, 8>> barcodeCoordinates;

	while (row < bitMatrix.height()) {
		auto vertices = FindVertices(bitMatrix, row, column);

		if (vertices[0] == nullptr && vertices[3] == nullptr) {
			if (!foundBarcodeInRow) {
				// we didn't find any barcode so that's the end of searching 
				break;
			}
			// we didn't find a barcode starting at the given column and row. Try again from the first column and slightly
			// below the lowest barcode we found so far.
			foundBarcodeInRow = false;
			column = 0;
			for (auto& barcodeCoordinate : barcodeCoordinates) {
				if (barcodeCoordinate[1] != nullptr) {
					row = std::max(row, static_cast<int>(barcodeCoordinate[1].value().y()));
				}
				if (barcodeCoordinate[3] != nullptr) {
					row = std::max(row, static_cast<int>(barcodeCoordinate[3].value().y()));
				}
			}
			row += ROW_STEP;
			continue;
		}
		foundBarcodeInRow = true;
		barcodeCoordinates.push_back(vertices);
		if (!multiple) {
			break;
		}
		// if we didn't find a right row indicator column, then continue the search for the next barcode after the 
		// start pattern of the barcode just found.
		if (vertices[2] != nullptr) {
			column = static_cast<int>(vertices[2].value().x());
			row = static_cast<int>(vertices[2].value().y());
		}
		else {
			column = static_cast<int>(vertices[4].value().x());
			row = static_cast<int>(vertices[4].value().y());
		}
	}
	return barcodeCoordinates;
}

bool HasStartPattern(const BitMatrix& m, bool rotate90)
{
	constexpr FixedPattern<8, 17> START_PATTERN = { 8, 1, 1, 1, 1, 1, 1, 3 };
	constexpr int minSymbolWidth = 3*8+1; // compact symbol

	PatternRow row;
	int end = rotate90 ? m.width() : m.height();

	for (int r = ROW_STEP; r < end; r += ROW_STEP) {
		GetPatternRow(m, r, row, rotate90);

		if (FindLeftGuard(row, minSymbolWidth, START_PATTERN, 2).isValid())
			return true;
		std::reverse(row.begin(), row.end());
		if (FindLeftGuard(row, minSymbolWidth, START_PATTERN, 2).isValid())
			return true;
	}

	return false;
}

/**
* <p>Detects a PDF417 Code in an image. Only checks 0 and 180 degree rotations.</p>
*
* @param image barcode image to decode
* @param multiple if true, then the image is searched for multiple codes. If false, then at most one code will
* be found and returned
*/
Detector::Result Detector::Detect(const BinaryBitmap& image, bool multiple, bool tryRotate)
{
	// construct a 'dummy' shared pointer, just be able to pass it up the call chain in DetectorResult
	// TODO: reimplement PDF Detector
	auto binImg = std::shared_ptr<const BitMatrix>(image.getBitMatrix(), [](const BitMatrix*){});
	if (!binImg)
		return {};

	Result result;

	for (int rotate90 = 0; rotate90 <= static_cast<int>(tryRotate); ++rotate90) {
		if (!HasStartPattern(*binImg, rotate90))
			continue;

		result.rotation = 90 * rotate90;
		if (rotate90) {
			auto newBits = std::make_shared<BitMatrix>(binImg->copy());
			newBits->rotate90();
			binImg = newBits;
		}

		result.points = DetectBarcode(*binImg, multiple);
		result.bits = binImg;
		if (result.points.empty()) {
			auto newBits = std::make_shared<BitMatrix>(binImg->copy());
			newBits->rotate180();
			result.points = DetectBarcode(*newBits, multiple);
			result.rotation += 180;
			result.bits = newBits;
		}

		if (!result.points.empty())
			return result;
	}

	return {};
}

} // Pdf417
} // ZXing
