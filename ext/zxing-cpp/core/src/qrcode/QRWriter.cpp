/*
* Copyright 2016 Huy Cuong Nguyen
* Copyright 2016 ZXing authors
*/
// SPDX-License-Identifier: Apache-2.0

#include "QRWriter.h"

#include "BitMatrix.h"
#include "CharacterSet.h"
#include "QREncodeResult.h"
#include "QREncoder.h"
#include "QRErrorCorrectionLevel.h"
#include "Utf.h"

#include <stdexcept>
#include <utility>

namespace ZXing::QRCode {

static const int QUIET_ZONE_SIZE = 4;

Writer::Writer()
	: _margin(QUIET_ZONE_SIZE),
	  _ecLevel(ErrorCorrectionLevel::Low),
	  _encoding(CharacterSet::Unknown),
	  _version(0),
	  _useGs1Format(false),
	  _maskPattern(-1)
{}

BitMatrix Writer::encode(const std::wstring& contents, int width, int height) const
{
	if (contents.empty()) {
		throw std::invalid_argument("Found empty contents");
	}

	if (width < 0 || height < 0) {
		throw std::invalid_argument("Requested dimensions are invalid");
	}

	EncodeResult code = Encode(contents, _ecLevel, _encoding, _version, _useGs1Format, _maskPattern);
	return Inflate(std::move(code.matrix), width, height, _margin);
}

BitMatrix Writer::encode(const std::string& contents, int width, int height) const
{
	return encode(FromUtf8(contents), width, height);
}

} // namespace ZXing::QRCode
