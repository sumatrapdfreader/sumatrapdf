/*
* Copyright 2016 Nu-book Inc.
* Copyright 2016 ZXing authors
*/
// SPDX-License-Identifier: Apache-2.0

#pragma once

namespace ZXing {

class DecoderResult;

namespace Aztec {

class DetectorResult;

DecoderResult Decode(const DetectorResult& detectorResult);

} // Aztec
} // ZXing
