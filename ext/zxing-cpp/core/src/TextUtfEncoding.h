/*
* Copyright 2016 Nu-book Inc.
*/
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <string>
#include <string_view>

namespace ZXing::TextUtfEncoding {

// The following functions are not required anymore after Barcode::text() now returns utf8 natively and the encoders accept utf8 as well.

[[deprecated]] std::string ToUtf8(std::wstring_view str);
[[deprecated]] std::string ToUtf8(std::wstring_view str, const bool angleEscape);
[[deprecated]] std::wstring FromUtf8(std::string_view utf8);

} // namespace ZXing::TextUtfEncoding
