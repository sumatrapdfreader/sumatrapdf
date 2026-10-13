/*
* Copyright 2017 Huy Cuong Nguyen
*/
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "BarcodeFormat.h"
#include "CharacterSet.h"

#include <string>

namespace ZXing {

class BitMatrix;

/**
* This class is here just for convenience as it offers single-point service
* to generate barcodes for all supported formats. As a result, this class
* offer very limited customization compared to what are available in each
* individual encoder.
*/
class MultiFormatWriter
{
public:
	explicit MultiFormatWriter(BarcodeFormat format) : _format(format) {}

	/**
	* Used for Aztec, PDF417, and QRCode only.
	*/
	MultiFormatWriter& setEncoding(CharacterSet encoding) {
		_encoding = encoding;
		return *this;
	}

	/**
	* Used for Aztec, PDF417, and QRCode only, [0-8].
	*/
	MultiFormatWriter& setEccLevel(int level) {
		_eccLevel = level;
		return *this;
	}

	/**
	* Used for all formats, sets the minimum number of quiet zone pixels.
	*/
	MultiFormatWriter& setMargin(int margin) {
		_margin = margin;
		return *this;
	}

	BitMatrix encode(const std::wstring& contents, int width, int height) const;
	BitMatrix encode(const std::string& contents, int width, int height) const;

private:
	BarcodeFormat _format;
	CharacterSet _encoding = CharacterSet::Unknown;
	int _margin = -1;
	int _eccLevel = -1;
};

} // ZXing
