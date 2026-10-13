/*
* Copyright 2022 Axel Waggershauser
*/
// SPDX-License-Identifier: Apache-2.0

#include "TextUtfEncoding.h"

#include "Utf.h"

namespace ZXing::TextUtfEncoding {

std::string ToUtf8(std::wstring_view str)
{
	return ZXing::ToUtf8(str);
}

// Same as `ToUtf8()` above, except if angleEscape set, places non-graphical characters in angle brackets with text name
std::string ToUtf8(std::wstring_view str, const bool angleEscape)
{
	return angleEscape ? ZXing::ToUtf8(EscapeNonGraphical(str)) : ZXing::ToUtf8(str);
}

std::wstring FromUtf8(std::string_view utf8)
{
	return ZXing::FromUtf8(utf8);
}

} // namespace ZXing::TextUtfEncoding
