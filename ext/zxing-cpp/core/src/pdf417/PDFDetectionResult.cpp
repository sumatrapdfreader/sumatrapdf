/*
* Copyright 2016 Nu-book Inc.
* Copyright 2016 ZXing authors
*/
// SPDX-License-Identifier: Apache-2.0

#include "PDFDetectionResult.h"
#include "PDFCodewordDecoder.h"
#include "ZXAlgorithms.h"

#include <algorithm>
#include <array>

namespace ZXing {
namespace Pdf417 {

static const int ADJUST_ROW_NUMBER_SKIP = 2;

DetectionResult::DetectionResult(const BarcodeMetadata& barcodeMetadata, const Nullable<BoundingBox>& boundingBox) :
	_barcodeMetadata(barcodeMetadata),
	_detectionResultColumns(barcodeMetadata.columnCount() + 2),
	_boundingBox(boundingBox)
{
}

void
DetectionResult::init(const BarcodeMetadata& barcodeMetadata, const Nullable<BoundingBox>& boundingBox)
{
	_barcodeMetadata = barcodeMetadata;
	_boundingBox = boundingBox;
	_detectionResultColumns.resize(barcodeMetadata.columnCount() + 2);
	std::fill(_detectionResultColumns.begin(), _detectionResultColumns.end(), nullptr);
}

static void AdjustIndicatorColumnRowNumbers(Nullable<DetectionResultColumn>& detectionResultColumn, const BarcodeMetadata& barcodeMetadata)
{
	if (detectionResultColumn != nullptr) {
		detectionResultColumn.value().adjustCompleteIndicatorColumnRowNumbers(barcodeMetadata);
	}
}

static void AdjustRowNumbersFromBothRI(std::vector<Nullable<DetectionResultColumn>>& detectionResultColumns)
{
	if (detectionResultColumns.front() == nullptr || detectionResultColumns.back() == nullptr) {
		return;
	}
	auto& LRIcodewords = detectionResultColumns.front().value().allCodewords();
	auto& RRIcodewords = detectionResultColumns.back().value().allCodewords();
	for (size_t codewordsRow = 0; codewordsRow < LRIcodewords.size(); codewordsRow++) {
		if (LRIcodewords[codewordsRow] != nullptr && RRIcodewords[codewordsRow] != nullptr &&
			LRIcodewords[codewordsRow].value().rowNumber() == RRIcodewords[codewordsRow].value().rowNumber()) {
			auto lastColumn = detectionResultColumns.end() - 1;
			for (auto columnIter = detectionResultColumns.begin() + 1; columnIter != lastColumn; ++columnIter) {
				if (!columnIter->hasValue()) {
					continue;
				}
				auto& codeword = columnIter->value().allCodewords()[codewordsRow];
				if (codeword != nullptr) {
					codeword.value().setRowNumber(LRIcodewords[codewordsRow].value().rowNumber());
					if (!codeword.value().hasValidRowNumber()) {
						columnIter->value().allCodewords()[codewordsRow] = nullptr;
					}
				}
			}
		}
	}
}

static int AdjustRowNumberIfValid(int rowIndicatorRowNumber, int invalidRowCounts, Codeword& codeword) {
	if (!codeword.hasValidRowNumber()) {
		if (codeword.isValidRowNumber(rowIndicatorRowNumber)) {
			codeword.setRowNumber(rowIndicatorRowNumber);
			invalidRowCounts = 0;
		}
		else {
			++invalidRowCounts;
		}
	}
	return invalidRowCounts;
}

static int AdjustRowNumbersFromLRI(std::vector<Nullable<DetectionResultColumn>>& detectionResultColumns) {
	if (detectionResultColumns.front() == nullptr) {
		return 0;
	}
	int unadjustedCount = 0;
	auto& codewords = detectionResultColumns.front().value().allCodewords();
	for (size_t codewordsRow = 0; codewordsRow < codewords.size(); codewordsRow++) {
		if (codewords[codewordsRow] == nullptr) {
			continue;
		}
		int rowIndicatorRowNumber = codewords[codewordsRow].value().rowNumber();
		int invalidRowCounts = 0;
		auto lastColumn = detectionResultColumns.end() - 1;
		for (auto columnIter = detectionResultColumns.begin() + 1; columnIter != lastColumn && invalidRowCounts < ADJUST_ROW_NUMBER_SKIP; ++columnIter) {
			if (!columnIter->hasValue()) {
				continue;
			}
			auto& codeword = columnIter->value().allCodewords()[codewordsRow];
			if (codeword != nullptr) {
				invalidRowCounts = AdjustRowNumberIfValid(rowIndicatorRowNumber, invalidRowCounts, codeword.value());
				if (!codeword.value().hasValidRowNumber()) {
					unadjustedCount++;
				}
			}
		}
	}
	return unadjustedCount;
}

static int AdjustRowNumbersFromRRI(std::vector<Nullable<DetectionResultColumn>>& detectionResultColumns) {
	if (detectionResultColumns.back() == nullptr) {
		return 0;
	}
	int unadjustedCount = 0;
	auto& codewords = detectionResultColumns.back().value().allCodewords();
	for (size_t codewordsRow = 0; codewordsRow < codewords.size(); codewordsRow++) {
		if (codewords[codewordsRow] == nullptr) {
			continue;
		}
		int rowIndicatorRowNumber = codewords[codewordsRow].value().rowNumber();
		int invalidRowCounts = 0;
		auto lastColumn = detectionResultColumns.end() - 1;
		for (auto columnIter = detectionResultColumns.begin() + 1; columnIter != lastColumn && invalidRowCounts < ADJUST_ROW_NUMBER_SKIP; ++columnIter) {
			if (!columnIter->hasValue()) {
				continue;
			}
			auto& codeword = columnIter->value().allCodewords()[codewordsRow];
			if (codeword != nullptr) {
				invalidRowCounts = AdjustRowNumberIfValid(rowIndicatorRowNumber, invalidRowCounts, codeword.value());
				if (!codeword.value().hasValidRowNumber()) {
					unadjustedCount++;
				}
			}
		}
	}
	return unadjustedCount;
}


static int AdjustRowNumbersByRow(std::vector<Nullable<DetectionResultColumn>>& detectionResultColumns) {
	AdjustRowNumbersFromBothRI(detectionResultColumns);
	// TODO we should only do full row adjustments if row numbers of left and right row indicator column match.
	// Maybe it's even better to calculated the height (in codeword rows) and divide it by the number of barcode
	// rows. This, together with the LRI and RRI row numbers should allow us to get a good estimate where a row
	// number starts and ends.
	int unadjustedCount = AdjustRowNumbersFromLRI(detectionResultColumns);
	return unadjustedCount + AdjustRowNumbersFromRRI(detectionResultColumns);
}


/**
* @return true, if row number was adjusted, false otherwise
*/
static bool AdjustRowNumber(Nullable<Codeword>& codeword, const Nullable<Codeword>& otherCodeword) {
	if (codeword != nullptr && otherCodeword != nullptr
		&& otherCodeword.value().hasValidRowNumber() && otherCodeword.value().bucket() == codeword.value().bucket()) {
		codeword.value().setRowNumber(otherCodeword.value().rowNumber());
		return true;
	}
	return false;
}

static void AdjustRowNumbers(const std::vector<Nullable<DetectionResultColumn>>& detectionResultColumns, int barcodeColumn, int codewordsRow, std::vector<Nullable<Codeword>>& codewords) {
	auto& codeword = codewords[codewordsRow];
	auto& previousColumnCodewords = detectionResultColumns[barcodeColumn - 1].value().allCodewords();
	auto& nextColumnCodewords = detectionResultColumns[barcodeColumn + 1] != nullptr ? detectionResultColumns[barcodeColumn + 1].value().allCodewords() : previousColumnCodewords;

	std::array<Nullable<Codeword>, 14> otherCodewords;

	otherCodewords[2] = previousColumnCodewords[codewordsRow];
	otherCodewords[3] = nextColumnCodewords[codewordsRow];

	if (codewordsRow > 0) {
		otherCodewords[0] = codewords[codewordsRow - 1];
		otherCodewords[4] = previousColumnCodewords[codewordsRow - 1];
		otherCodewords[5] = nextColumnCodewords[codewordsRow - 1];
	}
	if (codewordsRow > 1) {
		otherCodewords[8] = codewords[codewordsRow - 2];
		otherCodewords[10] = previousColumnCodewords[codewordsRow - 2];
		otherCodewords[11] = nextColumnCodewords[codewordsRow - 2];
	}
	if (codewordsRow < Size(codewords) - 1) {
		otherCodewords[1] = codewords[codewordsRow + 1];
		otherCodewords[6] = previousColumnCodewords[codewordsRow + 1];
		otherCodewords[7] = nextColumnCodewords[codewordsRow + 1];
	}
	if (codewordsRow < Size(codewords) - 2) {
		otherCodewords[9] = codewords[codewordsRow + 2];
		otherCodewords[12] = previousColumnCodewords[codewordsRow + 2];
		otherCodewords[13] = nextColumnCodewords[codewordsRow + 2];
	}
	for (const auto& otherCodeword : otherCodewords) {
		if (AdjustRowNumber(codeword, otherCodeword)) {
			return;
		}
	}
}


// TODO ensure that no detected codewords with unknown row number are left
// we should be able to estimate the row height and use it as a hint for the row number
// we should also fill the rows top to bottom and bottom to top
/**
* @return number of codewords which don't have a valid row number. Note that the count is not accurate as codewords
* will be counted several times. It just serves as an indicator to see when we can stop adjusting row numbers
*/
static int AdjustRowNumbers(std::vector<Nullable<DetectionResultColumn>>& detectionResultColumns) {
	int unadjustedCount = AdjustRowNumbersByRow(detectionResultColumns);
	if (unadjustedCount == 0) {
		return 0;
	}
	for (int barcodeColumn = 1; barcodeColumn < Size(detectionResultColumns) - 1; barcodeColumn++) {
		if (detectionResultColumns[barcodeColumn] == nullptr) {
			continue;
		}
		auto& codewords = detectionResultColumns[barcodeColumn].value().allCodewords();
		for (int codewordsRow = 0; codewordsRow < Size(codewords); codewordsRow++) {
			if (codewords[codewordsRow] == nullptr) {
				continue;
			}
			if (!codewords[codewordsRow].value().hasValidRowNumber()) {
				AdjustRowNumbers(detectionResultColumns, barcodeColumn, codewordsRow, codewords);
			}
		}
	}
	return unadjustedCount;
}


const std::vector<Nullable<DetectionResultColumn>> &
DetectionResult::allColumns()
{
	AdjustIndicatorColumnRowNumbers(_detectionResultColumns.front(), _barcodeMetadata);
	AdjustIndicatorColumnRowNumbers(_detectionResultColumns.back(), _barcodeMetadata);
	int unadjustedCodewordCount = CodewordDecoder::MAX_CODEWORDS_IN_BARCODE;
	int previousUnadjustedCount;
	do {
		previousUnadjustedCount = unadjustedCodewordCount;
		unadjustedCodewordCount = AdjustRowNumbers(_detectionResultColumns);
	} while (unadjustedCodewordCount > 0 && unadjustedCodewordCount < previousUnadjustedCount);
	return _detectionResultColumns;
}

} // Pdf417
} // ZXing
