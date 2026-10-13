/*
* Copyright 2016 Nu-book Inc.
* Copyright 2016 ZXing authors
* Copyright 2023 gitlost
*/
// SPDX-License-Identifier: Apache-2.0

#include "QRCodecMode.h"

#include "Error.h"
#include "QRVersion.h"
#include "ZXAlgorithms.h"

#include <array>

namespace ZXing::QRCode {

CodecMode CodecModeForBits(int bits, Type type)
{
	if (type == Type::Micro) {
		constexpr CodecMode Bits2Mode[4] = {CodecMode::NUMERIC, CodecMode::ALPHANUMERIC, CodecMode::BYTE, CodecMode::KANJI};
		if (bits < Size(Bits2Mode))
			return Bits2Mode[bits];
	} else if (type == Type::rMQR) {
		constexpr CodecMode Bits2Mode[8] = {
			CodecMode::TERMINATOR, CodecMode::NUMERIC, CodecMode::ALPHANUMERIC, CodecMode::BYTE,
			CodecMode::KANJI, CodecMode::FNC1_FIRST_POSITION, CodecMode::FNC1_SECOND_POSITION, CodecMode::ECI
		};
		if (bits < Size(Bits2Mode))
			return Bits2Mode[bits];
	} else {
		if ((bits >= 0x00 && bits <= 0x05) || (bits >= 0x07 && bits <= 0x09) || bits == 0x0d)
			return static_cast<CodecMode>(bits);
	}

	throw FormatError("Invalid codec mode");
}

int CharacterCountBits(CodecMode mode, const Version& version)
{
	int number = version.versionNumber();
	if (version.isMicro()) {
		switch (mode) {
		case CodecMode::NUMERIC:      return std::array{3, 4, 5, 6}[number - 1];
		case CodecMode::ALPHANUMERIC: return std::array{3, 4, 5}[number - 2];
		case CodecMode::BYTE:         return std::array{4, 5}[number - 3];
		case CodecMode::KANJI:        [[fallthrough]];
		case CodecMode::HANZI:        return std::array{3, 4}[number - 3];
		default: return 0;
		}
	}
	if (version.isRMQR()) {
		// See ISO/IEC 23941:2022 7.4.1, Table 3 - Number of bits of character count indicator
		constexpr char numeric[32]  = {4, 5, 6, 7, 7, 5, 6, 7, 7, 8, 4, 6, 7, 7, 8, 8, 5, 6, 7, 7, 8, 8, 7, 7, 8, 8, 9, 7, 8, 8, 8, 9};
		constexpr char alphanum[32] = {3, 5, 5, 6, 6, 5, 5, 6, 6, 7, 4, 5, 6, 6, 7, 7, 5, 6, 6, 7, 7, 8, 6, 7, 7, 7, 8, 6, 7, 7, 8, 8};
		constexpr char byte[32]     = {3, 4, 5, 5, 6, 4, 5, 5, 6, 6, 3, 5, 5, 6, 6, 7, 4, 5, 6, 6, 7, 7, 6, 6, 7, 7, 7, 6, 6, 7, 7, 8};
		constexpr char kanji[32]    = {2, 3, 4, 5, 5, 3, 4, 5, 5, 6, 2, 4, 5, 5, 6, 6, 3, 5, 5, 6, 6, 7, 5, 5, 6, 6, 7, 5, 6, 6, 6, 7};
		switch (mode) {
		case CodecMode::NUMERIC: return numeric[number - 1];
		case CodecMode::ALPHANUMERIC: return alphanum[number - 1];
		case CodecMode::BYTE: return byte[number - 1];
		case CodecMode::KANJI: return kanji[number - 1];
		default: return 0;
		}
	}

	int i;
	if (number <= 9)
		i = 0;
	else if (number <= 26)
		i = 1;
	else
		i = 2;

	switch (mode) {
	case CodecMode::NUMERIC:      return std::array{10, 12, 14}[i];
	case CodecMode::ALPHANUMERIC: return std::array{9, 11, 13}[i];
	case CodecMode::BYTE:         return std::array{8, 16, 16}[i];
	case CodecMode::KANJI:        [[fallthrough]];
	case CodecMode::HANZI:        return std::array{8, 10, 12}[i];
	default:                      return 0;
	}
}

int CodecModeBitsLength(const Version& version)
{
	return version.isMicro() ? version.versionNumber() - 1 : 4 - version.isRMQR();
}

int TerminatorBitsLength(const Version& version)
{
	return version.isMicro() ? version.versionNumber() * 2 + 1 : 4 - version.isRMQR();
}

} // namespace ZXing::QRCode
