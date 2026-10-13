/*
* Copyright 2016 Nu-book Inc.
* Copyright 2016 ZXing authors
*/
// SPDX-License-Identifier: Apache-2.0

#include "PDFDetectionResultColumn.h"
#include "PDFBarcodeMetadata.h"
#include "PDFBarcodeValue.h"
#include "ZXAlgorithms.h"

#include <algorithm>
#include <stdexcept>

namespace ZXing {
namespace Pdf417 {

static const int MAX_NEARBY_DISTANCE = 5;
static const int MIN_ROWS_IN_BARCODE = 3;
static const int MAX_ROWS_IN_BARCODE = 90;

DetectionResultColumn::DetectionResultColumn(const BoundingBox& boundingBox, RowIndicator rowIndicator) :
	_boundingBox(boundingBox),
	_rowIndicator(rowIndicator)
{
	if (boundingBox.maxY() < boundingBox.minY()) {
		throw std::invalid_argument("Invalid bounding box");
	}
	_codewords.resize(boundingBox.maxY() - boundingBox.minY() + 1);
}

Nullable<Codeword>
DetectionResultColumn::codewordNearby(int imageRow) const
{
	int index = imageRowToCodewordIndex(imageRow);
	if (_codewords[index] != nullptr) {
		return _codewords[index];
	}

	for (int i = 1; i < MAX_NEARBY_DISTANCE; i++) {
		int nearImageRow = imageRowToCodewordIndex(imageRow) - i;
		if (nearImageRow >= 0) {
			if (_codewords[nearImageRow] != nullptr) {
				return _codewords[nearImageRow];
			}
		}
		nearImageRow = imageRowToCodewordIndex(imageRow) + i;
		if (nearImageRow < Size(_codewords)) {
			if (_codewords[nearImageRow] != nullptr) {
				return _codewords[nearImageRow];
			}
		}
	}
	return nullptr;
}

void
DetectionResultColumn::setRowNumbers()
{
	for (auto& codeword : allCodewords()) {
		if (codeword != nullptr) {
			codeword.value().setRowNumberAsRowIndicatorColumn();
		}
	}
}

static void RemoveIncorrectCodewords(bool isLeft, std::vector<Nullable<Codeword>>& codewords, const BarcodeMetadata& barcodeMetadata)
{
	// Remove codewords which do not match the metadata
	// TODO Maybe we should keep the incorrect codewords for the start and end positions?
	for (auto& item : codewords) {
		if (item == nullptr) {
			continue;
		}

		const auto& codeword = item.value();

		int rowIndicatorValue = codeword.value() % 30;
		int codewordRowNumber = codeword.rowNumber();
		if (codewordRowNumber > barcodeMetadata.rowCount()) {
			item = nullptr;
			continue;
		}
		if (!isLeft) {
			codewordRowNumber += 2;
		}
		switch (codewordRowNumber % 3) {
		case 0:
			if (rowIndicatorValue * 3 + 1 != barcodeMetadata.rowCountUpperPart()) {
				item = nullptr;
			}
			break;
		case 1:
			if (rowIndicatorValue / 3 != barcodeMetadata.errorCorrectionLevel() ||
				rowIndicatorValue % 3 != barcodeMetadata.rowCountLowerPart()) {
				item = nullptr;
			}
			break;
		case 2:
			if (rowIndicatorValue + 1 != barcodeMetadata.columnCount()) {
				item = nullptr;
			}
			break;
		}
	}
}

// TODO implement properly
// TODO maybe we should add missing codewords to store the correct row number to make
// finding row numbers for other columns easier
// use row height count to make detection of invalid row numbers more reliable
void
DetectionResultColumn::adjustCompleteIndicatorColumnRowNumbers(const BarcodeMetadata& barcodeMetadata)
{
	if (!isRowIndicator()) {
		return;
	}

	auto& codewords = allCodewords();
	setRowNumbers();
	RemoveIncorrectCodewords(isLeftRowIndicator(), codewords, barcodeMetadata);
	const auto& bb = boundingBox();
	auto top = isLeftRowIndicator() ? bb.topLeft() : bb.topRight();
	auto bottom = isLeftRowIndicator() ? bb.bottomLeft() : bb.bottomRight();
	int firstRow = imageRowToCodewordIndex((int)top.value().y());
	int lastRow = imageRowToCodewordIndex((int)bottom.value().y());
	// We need to be careful using the average row height. Barcode could be skewed so that we have smaller and
	// taller rows
	//float averageRowHeight = (lastRow - firstRow) / (float)barcodeMetadata.rowCount();
	int barcodeRow = -1;
	int maxRowHeight = 1;
	int currentRowHeight = 0;
	int increment = 1;
	for (int codewordsRow = firstRow; codewordsRow < lastRow; codewordsRow++) {
		if (codewords[codewordsRow] == nullptr) {
			continue;
		}
		Codeword codeword = codewords[codewordsRow];
		if (barcodeRow == -1 && codeword.rowNumber() == barcodeMetadata.rowCount() - 1) {
			increment = -1;
			barcodeRow = barcodeMetadata.rowCount();
		}
		//      float expectedRowNumber = (codewordsRow - firstRow) / averageRowHeight;
		//      if (Math.abs(codeword.getRowNumber() - expectedRowNumber) > 2) {
		//        SimpleLog.log(LEVEL.WARNING,
		//            "Removing codeword, rowNumberSkew too high, codeword[" + codewordsRow + "]: Expected Row: " +
		//                expectedRowNumber + ", RealRow: " + codeword.getRowNumber() + ", value: " + codeword.getValue());
		//        codewords[codewordsRow] = null;
		//      }

		int rowDifference = codeword.rowNumber() - barcodeRow;

		if (rowDifference == 0) {
			currentRowHeight++;
		}
		else if (rowDifference == increment) {
			maxRowHeight = std::max(maxRowHeight, currentRowHeight);
			currentRowHeight = 1;
			barcodeRow = codeword.rowNumber();
		}
		else if (rowDifference < 0 ||
			codeword.rowNumber() >= barcodeMetadata.rowCount() ||
			rowDifference > codewordsRow) {
			codewords[codewordsRow] = nullptr;
		}
		else {
			int checkedRows;
			if (maxRowHeight > 2) {
				checkedRows = (maxRowHeight - 2) * rowDifference;
			}
			else {
				checkedRows = rowDifference;
			}
			bool closePreviousCodewordFound = checkedRows >= codewordsRow;
			for (int i = 1; i <= checkedRows && !closePreviousCodewordFound; i++) {
				// there must be (height * rowDifference) number of codewords missing. For now we assume height = 1.
				// This should hopefully get rid of most problems already.
				closePreviousCodewordFound = codewords[codewordsRow - i] != nullptr;
			}
			if (closePreviousCodewordFound) {
				codewords[codewordsRow] = nullptr;
			}
			else {
				barcodeRow = codeword.rowNumber();
				currentRowHeight = 1;
			}
		}
	}
	//return static_cast<int>(averageRowHeight + 0.5);
}

// TODO maybe we should add missing codewords to store the correct row number to make
// finding row numbers for other columns easier
// use row height count to make detection of invalid row numbers more reliable
void
DetectionResultColumn::adjustIncompleteIndicatorColumnRowNumbers(const BarcodeMetadata& barcodeMetadata)
{
	if (!isRowIndicator()) {
		return;
	}

	const auto& bb = boundingBox();
	auto top = isLeftRowIndicator() ? bb.topLeft() : bb.topRight();
	auto bottom = isLeftRowIndicator() ? bb.bottomLeft() : bb.bottomRight();
	int firstRow = imageRowToCodewordIndex((int)top.value().y());
	int lastRow = imageRowToCodewordIndex((int)bottom.value().y());
	//float averageRowHeight = (lastRow - firstRow) / (float)barcodeMetadata.rowCount();
	auto& codewords = allCodewords();
	int barcodeRow = -1;
	int maxRowHeight = 1;
	int currentRowHeight = 0;
	for (int codewordsRow = firstRow; codewordsRow < lastRow; codewordsRow++) {
		if (codewords[codewordsRow] == nullptr) {
			continue;
		}
		auto& item = codewords[codewordsRow];

		auto& codeword = item.value();
		codeword.setRowNumberAsRowIndicatorColumn();

		int rowDifference = codeword.rowNumber() - barcodeRow;

		// TODO improve handling with case where first row indicator doesn't start with 0

		if (rowDifference == 0) {
			currentRowHeight++;
		}
		else if (rowDifference == 1) {
			maxRowHeight = std::max(maxRowHeight, currentRowHeight);
			currentRowHeight = 1;
			barcodeRow = codeword.rowNumber();
		}
		else if (codeword.rowNumber() >= barcodeMetadata.rowCount()) {
			item = nullptr;
		}
		else {
			barcodeRow = codeword.rowNumber();
			currentRowHeight = 1;
		}
	}
	//return static_cast<int>(averageRowHeight + 0.5);
}

// This is example of bad design, a getter should not modify object's state
bool
DetectionResultColumn::getRowHeights(std::vector<int>& result)
{
	BarcodeMetadata barcodeMetadata;
	if (!getBarcodeMetadata(barcodeMetadata)) {
		return false;
	}

	adjustIncompleteIndicatorColumnRowNumbers(barcodeMetadata);
	result.resize(barcodeMetadata.rowCount());
	for (auto& item : allCodewords()) {
		if (item != nullptr) {
			size_t rowNumber = item.value().rowNumber();
			if (rowNumber >= result.size()) {
				// We have more rows than the barcode metadata allows for, ignore them.
				continue;
			}
			result[rowNumber]++;
		} // else throw exception?
	}
	return true;
}


// This is example of bad design, a getter should not modify object's state
bool
DetectionResultColumn::getBarcodeMetadata(BarcodeMetadata& result)
{
	if (!isRowIndicator()) {
		return false;
	}

	auto& codewords = allCodewords();
	BarcodeValue barcodeColumnCount;
	BarcodeValue barcodeRowCountUpperPart;
	BarcodeValue barcodeRowCountLowerPart;
	BarcodeValue barcodeECLevel;
	for (auto& item : codewords) {
		if (item == nullptr) {
			continue;
		}
		auto& codeword = item.value();
		codeword.setRowNumberAsRowIndicatorColumn();
		int rowIndicatorValue = codeword.value() % 30;
		int codewordRowNumber = codeword.rowNumber();
		if (!isLeftRowIndicator()) {
			codewordRowNumber += 2;
		}
		switch (codewordRowNumber % 3) {
		case 0:
			barcodeRowCountUpperPart.setValue(rowIndicatorValue * 3 + 1);
			break;
		case 1:
			barcodeECLevel.setValue(rowIndicatorValue / 3);
			barcodeRowCountLowerPart.setValue(rowIndicatorValue % 3);
			break;
		case 2:
			barcodeColumnCount.setValue(rowIndicatorValue + 1);
			break;
		}
	}
	// Maybe we should check if we have ambiguous values?
	auto cc = barcodeColumnCount.value();
	auto rcu = barcodeRowCountUpperPart.value();
	auto rcl = barcodeRowCountLowerPart.value();
	auto ec = barcodeECLevel.value();
	if (cc.empty() || rcu.empty() || rcl.empty() || ec.empty() || cc[0] < 1 || rcu[0] + rcl[0] < MIN_ROWS_IN_BARCODE || rcu[0] + rcl[0] > MAX_ROWS_IN_BARCODE) {
		return false;
	}
	result = { cc[0], rcu[0], rcl[0], ec[0] };
	RemoveIncorrectCodewords(isLeftRowIndicator(), codewords, result);
	return true;
}


} // Pdf417
} // ZXing
