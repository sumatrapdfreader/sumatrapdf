/*
* Copyright 2016 Huy Cuong Nguyen
* Copyright 2016 ZXing authors
*/
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "BitMatrix.h"
#include "ByteArray.h"
#include "QRCodecMode.h"
#include "QRVersion.h"

namespace ZXing::QRCode {

/**
* @author satorux@google.com (Satoru Takabayashi) - creator
* @author dswitkin@google.com (Daniel Switkin) - ported from C++
*
* Original class name in Java was QRCode, as this name is taken already for the namespace,
* so it's renamed here EncodeResult.
*/
class EncodeResult
{
public:
	ErrorCorrectionLevel ecLevel = ErrorCorrectionLevel::Invalid;
	CodecMode mode = CodecMode::TERMINATOR;
	const Version* version = nullptr;
	int maskPattern = -1;
	BitMatrix matrix;
};

} // namespace ZXing::QRCode
