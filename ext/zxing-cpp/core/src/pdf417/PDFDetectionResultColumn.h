/*
* Copyright 2016 Nu-book Inc.
* Copyright 2016 ZXing authors
*/
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "PDFBoundingBox.h"
#include "PDFCodeword.h"
#include "ZXNullable.h"

#include <vector>

namespace ZXing {
namespace Pdf417 {

class BarcodeMetadata;

/**
* @author Guenther Grau
*/
class DetectionResultColumn
{
public:
	enum class RowIndicator {
		None,
		Left,
		Right,
	};

	DetectionResultColumn() {}
	explicit DetectionResultColumn(const BoundingBox& boundingBox, RowIndicator rowInd = RowIndicator::None);

	bool isRowIndicator() const {
		return _rowIndicator != RowIndicator::None;
	}

	bool isLeftRowIndicator() const {
		return _rowIndicator == RowIndicator::Left;
	}

	Nullable<Codeword> codewordNearby(int imageRow) const;

	int imageRowToCodewordIndex(int imageRow) const {
		return imageRow - _boundingBox.minY();
	}

	void setCodeword(int imageRow, Codeword codeword) {
		_codewords[imageRowToCodewordIndex(imageRow)] = codeword;
	}

	Nullable<Codeword> codeword(int imageRow) const {
		return _codewords[imageRowToCodewordIndex(imageRow)];
	}

	const BoundingBox& boundingBox() const {
		return _boundingBox;
	}

	const std::vector<Nullable<Codeword>>& allCodewords() const {
		return _codewords;
	}

	std::vector<Nullable<Codeword>>& allCodewords() {
		return _codewords;
	}

	void adjustCompleteIndicatorColumnRowNumbers(const BarcodeMetadata& barcodeMetadata);
	bool getRowHeights(std::vector<int>& result); // not const, since it modifies object's state
	bool getBarcodeMetadata(BarcodeMetadata& result); // not const, since it modifies object's state

private:
	BoundingBox _boundingBox;
	std::vector<Nullable<Codeword>> _codewords;
	RowIndicator _rowIndicator = RowIndicator::None;

	void setRowNumbers();
	void adjustIncompleteIndicatorColumnRowNumbers(const BarcodeMetadata& barcodeMetadata);
};

} // Pdf417
} // ZXing
