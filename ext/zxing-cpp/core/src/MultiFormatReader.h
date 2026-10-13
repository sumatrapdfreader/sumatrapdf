/*
* Copyright 2016 Nu-book Inc.
* Copyright 2016 ZXing authors
*/
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "Barcode.h"

#include <vector>
#include <memory>

namespace ZXing {

class Reader;
class BinaryBitmap;
class ReaderOptions;

class MultiFormatReader
{
public:
	explicit MultiFormatReader(const ReaderOptions& opts);
	explicit MultiFormatReader(ReaderOptions&& opts) = delete;
	~MultiFormatReader();

	Barcode read(const BinaryBitmap& image) const;

	// WARNING: this API is experimental and may change/disappear
	Barcodes readMultiple(const BinaryBitmap& image, int maxSymbols = 0xFF) const;

private:
	std::vector<std::unique_ptr<Reader>> _readers;
	const ReaderOptions& _opts;
};

} // ZXing
