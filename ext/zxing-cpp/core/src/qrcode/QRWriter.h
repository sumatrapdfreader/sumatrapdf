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

namespace QRCode {

enum class ErrorCorrectionLevel;

/**
* This object renders a QR Code as a BitMatrix 2D array of greyscale values.
*
* @author dswitkin@google.com (Daniel Switkin)
*/
class Writer
{
public:
	Writer();

	Writer& setMargin(int margin) {
		_margin = margin;
		return *this;
	}

	Writer& setErrorCorrectionLevel(ErrorCorrectionLevel ecLevel) {
		_ecLevel = ecLevel;
		return *this;
	}

	Writer& setEncoding(CharacterSet encoding) {
		_encoding = encoding;
		return *this;
	}

	Writer& setVersion(int versionNumber) {
		_version = versionNumber;
		return *this;
	}

	Writer& useGS1Format() {
		_useGs1Format = true;
		return *this;
	}

	Writer& setMaskPattern(int pattern) {
		_maskPattern = pattern;
		return *this;
	}

	BitMatrix encode(const std::wstring& contents, int width, int height) const;
	BitMatrix encode(const std::string& contents, int width, int height) const;

private:
	int _margin;
	ErrorCorrectionLevel _ecLevel;
	CharacterSet _encoding;
	int _version;
	bool _useGs1Format;
	int _maskPattern;
};

} // QRCode
} // ZXing
