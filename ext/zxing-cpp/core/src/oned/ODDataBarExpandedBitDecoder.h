/*
* Copyright 2022 Axel Waggershauser
*/
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <string>

namespace ZXing {

class BitArray;

namespace OneD::DataBar {

std::string DecodeExpandedBits(const BitArray& bits);

} // namespace OneD::DataBar
} // namespace ZXing
