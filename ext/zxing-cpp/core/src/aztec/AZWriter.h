/*
* Copyright 2016 Huy Cuong Nguyen
* Copyright 2016 ZXing authors
*/
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "CharacterSet.h"

#include <string>

namespace ZXing {

class BitMatrix;

namespace Aztec {

class Writer
{
public:
	Writer();

	Writer& setMargin(int margin) {
		_margin = margin;
		return *this;
	}

	Writer& setEncoding(CharacterSet encoding) {
		_encoding = encoding;
		return *this;
	}

	Writer& setEccPercent(int percent) {
		_eccPercent = percent;
		return *this;
	}

	Writer& setLayers(int layers) {
		_layers = layers;
		return *this;
	}

	BitMatrix encode(const std::wstring& contents, int width, int height) const;
	BitMatrix encode(const std::string& contents, int width, int height) const;

private:
	CharacterSet _encoding;
	int _eccPercent;
	int _layers;
	int _margin = 0;
};

} // Aztec
} // ZXing
