/*
* Copyright 2022 Axel Waggershauser
*/
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "CharacterSet.h"

#include <string>

namespace ZXing {

enum class ECI : int
{
	Unknown    = -1,
	Cp437      = 2, // obsolete
	ISO8859_1  = 3,
	ISO8859_2  = 4,
	ISO8859_3  = 5,
	ISO8859_4  = 6,
	ISO8859_5  = 7,
	ISO8859_6  = 8,
	ISO8859_7  = 9,
	ISO8859_8  = 10,
	ISO8859_9  = 11,
	ISO8859_10 = 12,
	ISO8859_11 = 13,
	ISO8859_13 = 15,
	ISO8859_14 = 16,
	ISO8859_15 = 17,
	ISO8859_16 = 18,
	Shift_JIS  = 20,
	Cp1250     = 21,
	Cp1251     = 22,
	Cp1252     = 23,
	Cp1256     = 24,
	UTF16BE    = 25,
	UTF8       = 26,
	ASCII      = 27,
	Big5       = 28,
	GB2312     = 29,
	EUC_KR     = 30,
	GB18030    = 32,
	UTF16LE    = 33,
	UTF32BE    = 34,
	UTF32LE    = 35,
	ISO646_Inv = 170,
	Binary     = 899
};

inline constexpr int ToInt(ECI eci)
{
	return static_cast<int>(eci);
}

inline constexpr bool IsText(ECI eci)
{
	return ToInt(eci) >= 0 && ToInt(eci) <= 170;
}

inline constexpr bool CanProcess(ECI eci)
{
	// see https://github.com/zxing-cpp/zxing-cpp/commit/d8587545434d533c4e568181e1c12ef04a8e42d9#r74864359
	return ToInt(eci) <= 899;
}

/**
 * @brief ToString converts the numerical ECI value to a 7 character string as used in the ECI protocol
 * @return e.g. "\000020"
 */
std::string ToString(ECI eci);

CharacterSet ToCharacterSet(ECI eci);

ECI ToECI(CharacterSet cs);

} // namespace ZXing
