/*
* Copyright 2023 Axel Waggershauser
*/
// SPDX-License-Identifier: Apache-2.0

#define HIDE_DECODE_HINTS_ALIAS

#include "ReadBarcode.h"

namespace ZXing {

// Provide a struct that is binary compatible with ReaderOptions and is actually called DecodeHints so that
// the compiler generates a correctly mangled pair of ReadBarcode(s) symbols to keep backward ABI compatibility.

struct DecodeHints
{
	char data[sizeof(ReaderOptions)];
};

Barcode ReadBarcode(const ImageView& image, const DecodeHints& hints = {})
{
	return ReadBarcode(image, reinterpret_cast<const ReaderOptions&>(hints));
}

Barcodes ReadBarcodes(const ImageView& image, const DecodeHints& hints = {})
{
	return ReadBarcodes(image, reinterpret_cast<const ReaderOptions&>(hints));
}

} // ZXing
