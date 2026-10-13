/*
* Copyright 2016 Nu-book Inc.
* Copyright 2016 ZXing authors
*/
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "Reader.h"

#include <memory>
#include <vector>

namespace ZXing {

class ReaderOptions;

namespace OneD {

class RowReader;

class Reader : public ZXing::Reader
{
public:
	explicit Reader(const ReaderOptions& opts);
	~Reader() override;

	Barcode decode(const BinaryBitmap& image) const override;
	Barcodes decode(const BinaryBitmap& image, int maxSymbols) const override;

private:
	std::vector<std::unique_ptr<RowReader>> _readers;
};

} // OneD
} // ZXing
