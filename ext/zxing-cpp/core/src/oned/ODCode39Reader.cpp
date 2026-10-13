/*
* Copyright 2016 Nu-book Inc.
* Copyright 2016 ZXing authors
*/
// SPDX-License-Identifier: Apache-2.0

#include "ODCode39Reader.h"

#include "ReaderOptions.h"
#include "Barcode.h"
#include "ZXAlgorithms.h"

#include <array>

namespace ZXing::OneD {

static const char ALPHABET[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ-. $/+%*";

/**
* Each character consists of 5 bars and 4 spaces, 3 of which are wide (i.e. 6 are narrow).
* Each character is followed by a narrow space. The narrow to wide ratio is between 1:2 and 1:3.
* These represent the encodings of characters, as patterns of wide and narrow bars.
* The 9 least-significant bits of each int correspond to the pattern of wide and narrow,
* with 1s representing "wide" and 0s representing "narrow".
*/
static const int CHARACTER_ENCODINGS[] = {
	0x034, 0x121, 0x061, 0x160, 0x031, 0x130, 0x070, 0x025, 0x124, 0x064, // 0-9
	0x109, 0x049, 0x148, 0x019, 0x118, 0x058, 0x00D, 0x10C, 0x04C, 0x01C, // A-J
	0x103, 0x043, 0x142, 0x013, 0x112, 0x052, 0x007, 0x106, 0x046, 0x016, // K-T
	0x181, 0x0C1, 0x1C0, 0x091, 0x190, 0x0D0, 0x085, 0x184, 0x0C4, 0x0A8, // U-$
	0x0A2, 0x08A, 0x02A, 0x094 // /-% , *
};

static_assert(Size(ALPHABET) - 1 == Size(CHARACTER_ENCODINGS), "table size mismatch");

static const char PERCENTAGE_MAPPING[26] = {
	'A' - 38, 'B' - 38, 'C' - 38, 'D' - 38, 'E' - 38,	// %A to %E map to control codes ESC to USep
	'F' - 11, 'G' - 11, 'H' - 11, 'I' - 11, 'J' - 11,	// %F to %J map to ; < = > ?
	'K' + 16, 'L' + 16, 'M' + 16, 'N' + 16, 'O' + 16,	// %K to %O map to [ \ ] ^ _
	'P' + 43, 'Q' + 43, 'R' + 43, 'S' + 43, 'T' + 43,	// %P to %T map to { | } ~ DEL
	'\0', '@', '`',										// %U map to NUL, %V map to @, %W map to `
	127, 127, 127										// %X to %Z all map to DEL (127)
};

using CounterContainer = std::array<int, 9>;

// each character has 5 bars and 4 spaces
constexpr int CHAR_LEN = 9;

/** Decode the full ASCII string. Return empty string if FormatError occurred.
 * ctrl is either "$%/+" for code39 or "abcd" for code93. */
std::string DecodeCode39AndCode93FullASCII(std::string encoded, const char ctrl[4])
{
	auto out = encoded.begin();
	for (auto in = encoded.cbegin(); in != encoded.cend(); ++in) {
		char c = *in;
		if (Contains(ctrl, c)) {
			char next = *++in; // if in is one short of cend(), then next == 0
			if (next < 'A' || next > 'Z')
				return {};
			if (c == ctrl[0])
				c = next - 64; // $A to $Z map to control codes SH to SB
			else if (c == ctrl[1])
				c = PERCENTAGE_MAPPING[next - 'A'];
			else if (c == ctrl[2])
				c = next - 32; // /A to /O map to ! to , and /Z maps to :
			else
				c = next + 32; // +A to +Z map to a to z
		}
		*out++ = c;
	}
	encoded.erase(out, encoded.end());
	return encoded;
}

Barcode Code39Reader::decodePattern(int rowNumber, PatternView& next, std::unique_ptr<RowReader::DecodingState>&) const
{
	// minimal number of characters that must be present (including start, stop and checksum characters)
	int minCharCount = _opts.validateCode39CheckSum() ? 4 : 3;
	auto isStartOrStopSymbol = [](char c) { return c == '*'; };

	// provide the indices with the narrow bars/spaces which have to be equally wide
	constexpr auto START_PATTERN = FixedSparcePattern<CHAR_LEN, 6>{0, 2, 3, 5, 7, 8};
	// quiet zone is half the width of a character symbol
	constexpr float QUIET_ZONE_SCALE = 0.5f;

	next = FindLeftGuard(next, minCharCount * CHAR_LEN, START_PATTERN, QUIET_ZONE_SCALE * 12);
	if (!next.isValid())
		return {};

	if (!isStartOrStopSymbol(DecodeNarrowWidePattern(next, CHARACTER_ENCODINGS, ALPHABET))) // read off the start pattern
		return {};

	int xStart = next.pixelsInFront();
	int maxInterCharacterSpace = next.sum() / 2; // spec actually says 1 narrow space, width/2 is about 4

	std::string txt;
	txt.reserve(20);

	do {
		// check remaining input width and inter-character space
		if (!next.skipSymbol() || !next.skipSingle(maxInterCharacterSpace))
			return {};

		txt += DecodeNarrowWidePattern(next, CHARACTER_ENCODINGS, ALPHABET);
		if (txt.back() == 0)
			return {};
	} while (!isStartOrStopSymbol(txt.back()));

	txt.pop_back(); // remove asterisk

	// check txt length and whitespace after the last char. See also FindStartPattern.
	if (Size(txt) < minCharCount - 2 || !next.hasQuietZoneAfter(QUIET_ZONE_SCALE))
		return {};

	auto lastChar = txt.back();
	txt.pop_back();
	int checksum = TransformReduce(txt, 0, [](char c) { return IndexOf(ALPHABET, c); });
	bool hasValidCheckSum = lastChar == ALPHABET[checksum % 43];
	if (!hasValidCheckSum)
		txt.push_back(lastChar);

	const char shiftChars[] = "$%/+";
	auto fullASCII = _opts.tryCode39ExtendedMode() ? DecodeCode39AndCode93FullASCII(txt, shiftChars) : "";
	bool hasFullASCII = !fullASCII.empty() && std::find_first_of(txt.begin(), txt.end(), shiftChars, shiftChars + 4) != txt.end();
	if (hasFullASCII)
		txt = fullASCII;

	if (hasValidCheckSum)
		txt.push_back(lastChar);

	Error error = _opts.validateCode39CheckSum() && !hasValidCheckSum ? ChecksumError() : Error();

	// Symbology identifier modifiers ISO/IEC 16388:2007 Annex C Table C.1
	constexpr const char symbologyModifiers[4] = { '0', '1' /*checksum*/, '4' /*full ASCII*/, '5' /*checksum + full ASCII*/ };
	SymbologyIdentifier symbologyIdentifier = {'A', symbologyModifiers[(int)hasValidCheckSum + 2 * (int)hasFullASCII]};

	int xStop = next.pixelsTillEnd();
	return {std::move(txt), rowNumber, xStart, xStop, BarcodeFormat::Code39, symbologyIdentifier, error};
}

} // namespace ZXing::OneD
