/*
* Copyright 2016 Huy Cuong Nguyen
* Copyright 2016 ZXing authors
*/
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "CharacterSet.h"

#include <string>

namespace ZXing::QRCode {

enum class ErrorCorrectionLevel;
class EncodeResult;

EncodeResult Encode(const std::wstring& content, ErrorCorrectionLevel ecLevel, CharacterSet encoding, int versionNumber,
					bool useGs1Format, int maskPattern = -1);

} // namespace ZXing::QRCode
