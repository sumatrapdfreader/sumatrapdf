/*
* Copyright 2016 Nu-book Inc.
* Copyright 2016 ZXing authors
*/
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "PDFBarcodeMetadata.h"
#include "PDFBoundingBox.h"
#include "PDFDetectionResultColumn.h"
#include "ZXNullable.h"
#include <vector>

namespace ZXing {
namespace Pdf417 {

/**
* @author Guenther Grau
*/
class DetectionResult
{
	BarcodeMetadata _barcodeMetadata;
	std::vector<Nullable<DetectionResultColumn>> _detectionResultColumns;
	Nullable<BoundingBox> _boundingBox;

public:
	DetectionResult() = default;
	DetectionResult(const BarcodeMetadata& barcodeMetadata, const Nullable<BoundingBox>& boundingBox);

	void init(const BarcodeMetadata& barcodeMetadata, const Nullable<BoundingBox>& boundingBox);

	const std::vector<Nullable<DetectionResultColumn>> & allColumns();

	int barcodeColumnCount() const {
		return _barcodeMetadata.columnCount();
	}

	int barcodeRowCount() const {
		return _barcodeMetadata.rowCount();
	}

	int barcodeECLevel() const {
		return _barcodeMetadata.errorCorrectionLevel();
	}

	void setBoundingBox(const BoundingBox& boundingBox) {
		_boundingBox = boundingBox;
	}

	const Nullable<BoundingBox> & getBoundingBox() const {
		return _boundingBox;
	}

	void setBoundingBox(const Nullable<BoundingBox>& box) {
		_boundingBox = box;
	}

	void setColumn(int barcodeColumn, const Nullable<DetectionResultColumn>& detectionResultColumn) {
		_detectionResultColumns[barcodeColumn] = detectionResultColumn;
	}

	const Nullable<DetectionResultColumn>& column(int barcodeColumn) const {
		return _detectionResultColumns[barcodeColumn];
	}

	Nullable<DetectionResultColumn>& column(int barcodeColumn) {
		return _detectionResultColumns[barcodeColumn];
	}

};

} // Pdf417
} // ZXing
