/*
* Copyright 2016 Nu-book Inc.
* Copyright 2016 ZXing authors
*/
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <vector>

namespace ZXing {

class BitMatrix;

namespace Aztec {

class DetectorResult;

DetectorResult Detect(const BitMatrix& image, bool isPure, bool tryHarder = true);

using DetectorResults = std::vector<DetectorResult>;
DetectorResults Detect(const BitMatrix& image, bool isPure, bool tryHarder, int maxSymbols);

} // Aztec
} // ZXing
