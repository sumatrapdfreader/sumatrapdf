/*
 * Copyright 2022 Axel Waggershauser
 */
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <string>
#include <string_view>

namespace ZXing {

std::string HRIFromGS1(std::string_view gs1);
std::string HRIFromISO15434(std::string_view str);

} // namespace ZXing
