/*
* Copyright 2016 Nu-book Inc.
* Copyright 2016 ZXing authors
*/
// SPDX-License-Identifier: Apache-2.0

#include "CharacterSet.h"

#include "ZXAlgorithms.h"

#include <algorithm>
#include <cctype>

namespace ZXing {

struct CharacterSetName
{
	std::string_view name;
	CharacterSet cs;
};

static CharacterSetName NAME_TO_CHARSET[] = {
	{"Cp437",		CharacterSet::Cp437},
	{"ISO-8859-1",	CharacterSet::ISO8859_1},
	{"ISO-8859-2",	CharacterSet::ISO8859_2},
	{"ISO-8859-3",	CharacterSet::ISO8859_3},
	{"ISO-8859-4",	CharacterSet::ISO8859_4},
	{"ISO-8859-5",	CharacterSet::ISO8859_5},
	{"ISO-8859-6",	CharacterSet::ISO8859_6},
	{"ISO-8859-7",	CharacterSet::ISO8859_7},
	{"ISO-8859-8",	CharacterSet::ISO8859_8},
	{"ISO-8859-9",	CharacterSet::ISO8859_9},
	{"ISO-8859-10",	CharacterSet::ISO8859_10},
	{"ISO-8859-11",	CharacterSet::ISO8859_11},
	{"ISO-8859-13",	CharacterSet::ISO8859_13},
	{"ISO-8859-14",	CharacterSet::ISO8859_14},
	{"ISO-8859-15",	CharacterSet::ISO8859_15},
	{"ISO-8859-16",	CharacterSet::ISO8859_16},
	{"SJIS",		CharacterSet::Shift_JIS},
	{"Shift_JIS",	CharacterSet::Shift_JIS},
	{"Cp1250",		CharacterSet::Cp1250},
	{"windows-1250",CharacterSet::Cp1250},
	{"Cp1251",		CharacterSet::Cp1251},
	{"windows-1251",CharacterSet::Cp1251},
	{"Cp1252",		CharacterSet::Cp1252},
	{"windows-1252",CharacterSet::Cp1252},
	{"Cp1256",		CharacterSet::Cp1256},
	{"windows-1256",CharacterSet::Cp1256},
	{"UTF-16BE",	CharacterSet::UTF16BE},
	{"UTF-16LE",	CharacterSet::UTF16LE},
	{"UTF-32BE",	CharacterSet::UTF32BE},
	{"UTF-32LE",	CharacterSet::UTF32LE},
	{"UnicodeBigUnmarked", CharacterSet::UTF16BE},
	{"UnicodeBig",	CharacterSet::UTF16BE},
	{"UTF-8",		CharacterSet::UTF8},
	{"ASCII",		CharacterSet::ASCII},
	{"US-ASCII",	CharacterSet::ASCII},
	{"Big5",		CharacterSet::Big5},
	{"GB2312",		CharacterSet::GB2312},
	{"GB18030",		CharacterSet::GB18030},
	{"EUC-CN",		CharacterSet::GB18030},
	{"GBK",			CharacterSet::GB18030},
	{"EUC-KR",		CharacterSet::EUC_KR},
	{"BINARY",		CharacterSet::BINARY},
};

static std::string NormalizeName(std::string_view sv)
{
	std::string str(sv);
	std::transform(str.begin(), str.end(), str.begin(), [](char c) { return (char)std::tolower(c); });
#ifdef __cpp_lib_erase_if
	std::erase_if(str, [](char c) { return Contains("_-[] ", c); });
#else
	str.erase(std::remove_if(str.begin(), str.end(), [](char c) { return Contains("_-[] ", c); }), str.end());
#endif
	return str;
}

CharacterSet CharacterSetFromString(std::string_view name)
{
	auto i = FindIf(NAME_TO_CHARSET, [str = NormalizeName(name)](auto& v) { return NormalizeName(v.name) == str; });
	return i == std::end(NAME_TO_CHARSET) ? CharacterSet::Unknown : i->cs;
}

std::string ToString(CharacterSet cs)
{
	auto i = FindIf(NAME_TO_CHARSET, [cs](auto& v) { return v.cs == cs; });
	return i == std::end(NAME_TO_CHARSET) ? "" : std::string(i->name);
}

} // namespace ZXing
