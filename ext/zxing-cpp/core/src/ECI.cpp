/*
* Copyright 2022 Axel Waggershauser
*/
// SPDX-License-Identifier: Apache-2.0

#include "ECI.h"

#include "ZXAlgorithms.h"

#include <map>

namespace ZXing {

static const std::map<ECI, CharacterSet> ECI_TO_CHARSET = {
	{ECI(0), CharacterSet::Cp437},     // Obsolete
	{ECI(1), CharacterSet::ISO8859_1}, // Obsolete
	{ECI::Cp437, CharacterSet::Cp437}, // Obsolete but still used by PDF417 Macro fields (ISO/IEC 15438:2015 Annex H.2.3)
	{ECI::ISO8859_1, CharacterSet::ISO8859_1},
	{ECI::ISO8859_2, CharacterSet::ISO8859_2},
	{ECI::ISO8859_3, CharacterSet::ISO8859_3},
	{ECI::ISO8859_4, CharacterSet::ISO8859_4},
	{ECI::ISO8859_5, CharacterSet::ISO8859_5},
	{ECI::ISO8859_6, CharacterSet::ISO8859_6},
	{ECI::ISO8859_7, CharacterSet::ISO8859_7},
	{ECI::ISO8859_8, CharacterSet::ISO8859_8},
	{ECI::ISO8859_9, CharacterSet::ISO8859_9},
	{ECI::ISO8859_10, CharacterSet::ISO8859_10},
	{ECI::ISO8859_11, CharacterSet::ISO8859_11},
	{ECI::ISO8859_13, CharacterSet::ISO8859_13},
	{ECI::ISO8859_14, CharacterSet::ISO8859_14},
	{ECI::ISO8859_15, CharacterSet::ISO8859_15},
	{ECI::ISO8859_16, CharacterSet::ISO8859_16},
	{ECI::Shift_JIS, CharacterSet::Shift_JIS},
	{ECI::Cp1250, CharacterSet::Cp1250},
	{ECI::Cp1251, CharacterSet::Cp1251},
	{ECI::Cp1252, CharacterSet::Cp1252},
	{ECI::Cp1256, CharacterSet::Cp1256},
	{ECI::UTF8, CharacterSet::UTF8},
	{ECI::UTF16BE, CharacterSet::UTF16BE},
	{ECI::UTF16LE, CharacterSet::UTF16LE},
	{ECI::UTF32BE, CharacterSet::UTF32BE},
	{ECI::UTF32LE, CharacterSet::UTF32LE},
	{ECI::ASCII, CharacterSet::ASCII},
	{ECI::Big5, CharacterSet::Big5},
	{ECI::GB18030, CharacterSet::GB18030},
	{ECI::GB2312, CharacterSet::GB2312},
	{ECI::EUC_KR, CharacterSet::EUC_KR},
	{ECI::ISO646_Inv, CharacterSet::ASCII},
	{ECI::Binary, CharacterSet::BINARY},
};

std::string ToString(ECI eci)
{
	return '\\' + ToString(ToInt(eci), 6);
}

CharacterSet ToCharacterSet(ECI eci)
{
	if (auto it = ECI_TO_CHARSET.find(eci); it != ECI_TO_CHARSET.end())
		return it->second;

	return CharacterSet::Unknown;
}

ECI ToECI(CharacterSet cs)
{
	// Special case ISO8859_1 to avoid obsolete ECI 1
	if (cs == CharacterSet::ISO8859_1)
		return ECI::ISO8859_1;
	// Special case Cp437 to avoid obsolete ECI 0 for slightly less obsolete ECI 2
	if (cs == CharacterSet::Cp437)
		return ECI::Cp437;

	for (auto& [key, value] : ECI_TO_CHARSET)
		if (value == cs)
			return key;

	return ECI::Unknown;
}

} // namespace ZXing
