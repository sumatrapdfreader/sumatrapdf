/*
* Copyright 2016 Huy Cuong Nguyen
* Copyright 2016 ZXing authors
*/
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "CharacterSet.h"
#include "DMSymbolShape.h"

#include <string>

namespace ZXing {

class BitMatrix;

namespace DataMatrix {

class Writer
{
public:
	Writer();

	Writer& setMargin(int margin) {
		_quietZone = margin;
		return *this;
	}

	Writer& setShapeHint(SymbolShape shape) {
		_shapeHint = shape;
		return *this;
	}

	Writer& setMinSize(int width, int height) {
		_minWidth = width;
		_minHeight = height;
		return *this;
	}

	Writer& setMaxSize(int width, int height) {
		_maxWidth = width;
		_maxHeight = height;
		return *this;
	}

	Writer& setEncoding(CharacterSet encoding) {
		_encoding = encoding;
		return *this;
	}

	BitMatrix encode(const std::wstring& contents, int width, int height) const;
	BitMatrix encode(const std::string& contents, int width, int height) const;

private:
	SymbolShape _shapeHint;
	int _quietZone = 1, _minWidth = -1, _minHeight = -1, _maxWidth = -1, _maxHeight = -1;
	CharacterSet _encoding;
};

} // DataMatrix
} // ZXing
