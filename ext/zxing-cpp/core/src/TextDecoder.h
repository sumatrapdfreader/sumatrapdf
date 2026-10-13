/*
* Copyright 2016 Nu-book Inc.
*/
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "CharacterSet.h"

#include <cstddef>
#include <cstdint>
#include <string>

namespace ZXing {

class TextDecoder
{
public:
	static CharacterSet DefaultEncoding();
	static CharacterSet GuessEncoding(const uint8_t* bytes, size_t length, CharacterSet fallback = DefaultEncoding());

	// If `sjisASCII` set then for Shift_JIS maps ASCII directly (straight-thru), i.e. does not map ASCII backslash & tilde
	// to Yen sign & overline resp. (JIS X 0201 Roman)
	static void Append(std::string& str, const uint8_t* bytes, size_t length, CharacterSet charset, bool sjisASCII = true);

	static void Append(std::wstring& str, const uint8_t* bytes, size_t length, CharacterSet charset);

	static void AppendLatin1(std::wstring& str, const std::string& latin1) {
		auto ptr = (const uint8_t*)latin1.data();
		str.append(ptr, ptr + latin1.length());
	}
	
	static std::wstring FromLatin1(const std::string& latin1) {
		auto ptr = (const uint8_t*)latin1.data();
		return std::wstring(ptr, ptr + latin1.length());
	}
};

} // ZXing
