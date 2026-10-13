/*
* Copyright 2016 Huy Cuong Nguyen
* Copyright 2016 ZXing authors
*/
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "TritMatrix.h"

namespace ZXing {

class BitArray;

namespace QRCode {

enum class ErrorCorrectionLevel;
class Version;

constexpr int NUM_MASK_PATTERNS = 8;

void BuildMatrix(const BitArray& dataBits, ErrorCorrectionLevel ecLevel, const Version& version, int maskPattern, TritMatrix& matrix);

} // QRCode
} // ZXing
