/*
* Copyright 2016 Nu-book Inc.
*/
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "CharacterSet.h"

#include <string>

namespace ZXing {

class TextEncoder
{
	static void GetBytes(const std::string& str, CharacterSet charset, std::string& bytes);
	static void GetBytes(const std::wstring& str, CharacterSet charset, std::string& bytes);
public:
	static std::string FromUnicode(const std::string& str, CharacterSet charset) {
		std::string r;
		GetBytes(str, charset, r);
		return r;
	}
	static std::string FromUnicode(const std::wstring& str, CharacterSet charset) {
		std::string r;
		GetBytes(str, charset, r);
		return r;
	}
};

} // ZXing
