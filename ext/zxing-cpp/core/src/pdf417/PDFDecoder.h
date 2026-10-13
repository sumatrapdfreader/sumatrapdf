/*
* Copyright 2016 Nu-book Inc.
* Copyright 2016 ZXing authors
*/
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <vector>

namespace ZXing {

class DecoderResult;

namespace Pdf417 {

DecoderResult Decode(const std::vector<int>& codewords);

} // namespace Pdf417
} // namespace ZXing
