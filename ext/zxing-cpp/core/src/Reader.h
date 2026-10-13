/*
* Copyright 2016 Nu-book Inc.
* Copyright 2016 ZXing authors
*/
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "ReaderOptions.h"
#include "Barcode.h"

namespace ZXing {

class BinaryBitmap;
class ReaderOptions;

class Reader
{
protected:
	const ReaderOptions& _opts;

public:
	const bool supportsInversion;

	explicit Reader(const ReaderOptions& opts, bool supportsInversion = false) : _opts(opts), supportsInversion(supportsInversion) {}
	explicit Reader(ReaderOptions&& opts) = delete;
	virtual ~Reader() = default;

	virtual Barcode decode(const BinaryBitmap& image) const = 0;

	// WARNING: this API is experimental and may change/disappear
	virtual Barcodes decode(const BinaryBitmap& image, [[maybe_unused]] int maxSymbols) const {
		auto res = decode(image);
		return res.isValid() || (_opts.returnErrors() && res.format() != BarcodeFormat::None) ? Barcodes{std::move(res)} : Barcodes{};
	}
};

} // ZXing
