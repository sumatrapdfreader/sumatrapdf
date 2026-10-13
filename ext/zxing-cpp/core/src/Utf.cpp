/*
* Copyright 2016 Nu-book Inc.
* Copyright 2021 gitlost
* Copyright 2022 Axel Waggershauser
*/
// SPDX-License-Identifier: Apache-2.0

#include "Utf.h"

#include "ZXTestSupport.h"
#include "ZXAlgorithms.h"

#include <iomanip>
#include <cstdint>
#include <sstream>

namespace ZXing {

// TODO: c++20 has char8_t
#ifndef ZXING_HAS_CHAR8
#if __cplusplus <= 201703L
using char8_t = uint8_t;
#endif
#endif
using utf8_t = std::basic_string_view<char8_t>;

using state_t = uint8_t;
constexpr state_t kAccepted = 0;
constexpr state_t kRejected [[maybe_unused]] = 12;

inline char32_t Utf8Decode(char8_t byte, state_t& state, char32_t& codep)
{
	// Copyright (c) 2008-2009 Bjoern Hoehrmann <bjoern@hoehrmann.de>
	// See http://bjoern.hoehrmann.de/utf-8/decoder/dfa/ for details.
	static constexpr const state_t kUtf8Data[] = {
		/* The first part of the table maps bytes to character classes that
		 * reduce the size of the transition table and create bitmasks. */
		0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
		0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
		0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
		0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
		1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,  9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,
		7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,  7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
		8,8,2,2,2,2,2,2,2,2,2,2,2,2,2,2,  2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
		10,3,3,3,3,3,3,3,3,3,3,3,3,4,3,3, 11,6,6,6,5,8,8,8,8,8,8,8,8,8,8,8,

		/* The second part is a transition table that maps a combination
		 * of a state of the automaton and a character class to a state. */
		0,12,24,36,60,96,84,12,12,12,48,72, 12,12,12,12,12,12,12,12,12,12,12,12,
		12, 0,12,12,12,12,12, 0,12, 0,12,12, 12,24,12,12,12,12,12,24,12,24,12,12,
		12,12,12,12,12,12,12,24,12,12,12,12, 12,24,12,12,12,12,12,12,12,24,12,12,
		12,12,12,12,12,12,12,36,12,36,12,12, 12,36,12,12,12,12,12,36,12,36,12,12,
		12,36,12,12,12,12,12,12,12,12,12,12,
	};

	state_t type = kUtf8Data[byte];
	codep = (state != kAccepted) ? (byte & 0x3fu) | (codep << 6) : (0xff >> type) & (byte);
	state = kUtf8Data[256 + state + type];
	return state;
}

static_assert(sizeof(wchar_t) == 4 || sizeof(wchar_t) == 2, "wchar_t needs to be 2 or 4 bytes wide");

inline bool IsUtf16SurrogatePair(std::wstring_view str)
{
	return sizeof(wchar_t) == 2 && str.size() >= 2 && (str[0] & 0xfc00) == 0xd800 && (str[1] & 0xfc00) == 0xdc00;
}

inline char32_t Utf32FromUtf16Surrogates(std::wstring_view str)
{
	return (static_cast<char32_t>(str[0]) << 10) + str[1] - 0x35fdc00;
}

static size_t Utf8CountCodePoints(utf8_t utf8)
{
	size_t count = 0;

	for (size_t i = 0; i < utf8.size();) {
		if (utf8[i] < 128) {
			++i;
		} else {
			switch (utf8[i] & 0xf0) {
			case 0xc0: [[fallthrough]];
			case 0xd0: i += 2; break;
			case 0xe0: i += 3; break;
			case 0xf0: i += 4; break;
			default: // we are in middle of a sequence
				++i;
				while (i < utf8.size() && (utf8[i] & 0xc0) == 0x80)
					++i;
				break;
			}
		}
		++count;
	}

	return count;
}

static void AppendFromUtf8(utf8_t utf8, std::wstring& buffer)
{
	buffer.reserve(buffer.size() + Utf8CountCodePoints(utf8));

	char32_t codePoint = 0;
	state_t state = kAccepted;

	for (auto b : utf8) {
		if (Utf8Decode(b, state, codePoint) != kAccepted)
			continue;

		if (sizeof(wchar_t) == 2 && codePoint > 0xffff) { // surrogate pair
			buffer.push_back(narrow_cast<wchar_t>(0xd7c0 + (codePoint >> 10)));
			buffer.push_back(narrow_cast<wchar_t>(0xdc00 + (codePoint & 0x3ff)));
		} else {
			buffer.push_back(narrow_cast<wchar_t>(codePoint));
		}
	}
}

std::wstring FromUtf8(std::string_view utf8)
{
	std::wstring str;
	AppendFromUtf8({reinterpret_cast<const char8_t*>(utf8.data()), utf8.size()}, str);
	return str;
}

#if __cplusplus > 201703L
std::wstring FromUtf8(std::u8string_view utf8)
{
	std::wstring str;
	AppendFromUtf8(utf8, str);
	return str;
}
#endif

// Count the number of bytes required to store given code points in UTF-8.
static size_t Utf8CountBytes(std::wstring_view str)
{
	int result = 0;
	for (; str.size(); str.remove_prefix(1)) {
		if (str.front() < 0x80)
			result += 1;
		else if (str.front() < 0x800)
			result += 2;
		else if (sizeof(wchar_t) == 4) {
			if (str.front() < 0x10000)
				result += 3;
			else
				result += 4;
		} else {
			if (IsUtf16SurrogatePair(str)) {
				result += 4;
				str.remove_prefix(1);
			} else
				result += 3;
		}
	}
	return result;
}

ZXING_EXPORT_TEST_ONLY
int Utf32ToUtf8(char32_t utf32, char* out)
{
	if (utf32 < 0x80) {
		*out++ = narrow_cast<char8_t>(utf32);
		return 1;
	}
	if (utf32 < 0x800) {
		*out++ = narrow_cast<char8_t>((utf32 >> 6) | 0xc0);
		*out++ = narrow_cast<char8_t>((utf32 & 0x3f) | 0x80);
		return 2;
	}
	if (utf32 < 0x10000) {
		*out++ = narrow_cast<char8_t>((utf32 >> 12) | 0xe0);
		*out++ = narrow_cast<char8_t>(((utf32 >> 6) & 0x3f) | 0x80);
		*out++ = narrow_cast<char8_t>((utf32 & 0x3f) | 0x80);
		return 3;
	}

	*out++ = narrow_cast<char8_t>((utf32 >> 18) | 0xf0);
	*out++ = narrow_cast<char8_t>(((utf32 >> 12) & 0x3f) | 0x80);
	*out++ = narrow_cast<char8_t>(((utf32 >> 6) & 0x3f) | 0x80);
	*out++ = narrow_cast<char8_t>((utf32 & 0x3f) | 0x80);
	return 4;
}

static void AppendToUtf8(std::wstring_view str, std::string& utf8)
{
	utf8.reserve(utf8.size() + Utf8CountBytes(str));

	char buffer[4];
	for (; str.size(); str.remove_prefix(1))
	{
		uint32_t cp;
		if (IsUtf16SurrogatePair(str)) {
			cp = Utf32FromUtf16Surrogates(str);
			str.remove_prefix(1);
		} else
			cp = str.front();

		auto bufLength = Utf32ToUtf8(cp, buffer);
		utf8.append(buffer, bufLength);
	}
}

std::string ToUtf8(std::wstring_view str)
{
	std::string utf8;
	AppendToUtf8(str, utf8);
	return utf8;
}

static bool iswgraph(wchar_t wc)
{
	/* Consider all legal codepoints as graphical except for:
	 * - whitespace
	 * - C0 and C1 control characters
	 * - U+2028 and U+2029 (line/para break)
	 * - U+FFF9 through U+FFFB (interlinear annotation controls)
	 * The following code is based on libmusls implementation */

	if (wc == ' ' || (unsigned)wc - '\t' < 5)
		return false;
	if (wc < 0xff)
		return ((wc + 1) & 0x7f) >= 0x21;
	if (wc < 0x2028 || wc - 0x202a < 0xd800 - 0x202a || wc - 0xe000 < 0xfff9 - 0xe000)
		return true;
	if (wc - 0xfffc > 0x10ffff - 0xfffc || (wc & 0xfffe) == 0xfffe)
		return false;
	return true;
}

std::wstring EscapeNonGraphical(std::wstring_view str)
{
	static const char* const ascii_nongraphs[33] = {
		"NUL", "SOH", "STX", "ETX", "EOT", "ENQ", "ACK", "BEL",
		"BS",  "HT",  "LF",  "VT",  "FF",  "CR",  "SO",  "SI",
		"DLE", "DC1", "DC2", "DC3", "DC4", "NAK", "SYN", "ETB",
		"CAN",  "EM", "SUB", "ESC",  "FS",  "GS",  "RS",  "US",
		"DEL",
	};

	std::wostringstream ws;
	ws.fill(L'0');

	for (; str.size(); str.remove_prefix(1)) {
		wchar_t wc = str.front();
		if (wc < 32 || wc == 127) // Non-graphical ASCII, excluding space
			ws << "<" << ascii_nongraphs[wc == 127 ? 32 : wc] << ">";
		else if (wc < 128) // ASCII
			ws << wc;
		else if (IsUtf16SurrogatePair(str)) {
			ws.write(str.data(), 2);
			str.remove_prefix(1);
		}
		// Exclude unpaired surrogates and NO-BREAK spaces NBSP and NUMSP
		else if ((wc < 0xd800 || wc >= 0xe000) && (iswgraph(wc) && wc != 0xA0 && wc != 0x2007 && wc != 0x2000 && wc != 0xfffd))
			ws << wc;
		else // Non-graphical Unicode
			ws << "<U+" << std::setw(wc < 256 ? 2 : 4) << std::uppercase << std::hex << static_cast<uint32_t>(wc) << ">";
	}

	return ws.str();
}

std::string EscapeNonGraphical(std::string_view utf8)
{
	return ToUtf8(EscapeNonGraphical(FromUtf8(utf8)));
}

} // namespace ZXing
