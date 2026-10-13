/*
* Copyright 2016 Huy Cuong Nguyen
* Copyright 2016 ZXing authors
*/
// SPDX-License-Identifier: Apache-2.0

#include "ODCodabarWriter.h"

#include "ODWriterHelper.h"
#include "Utf.h"
#include "ZXAlgorithms.h"

#include <stdexcept>
#include <vector>

namespace ZXing::OneD {

static constexpr wchar_t START_END_CHARS[] =  L"ABCD";
static constexpr wchar_t ALT_START_END_CHARS[] = L"TN*E";
static constexpr wchar_t CHARS_WHICH_ARE_TEN_LENGTH_EACH_AFTER_DECODED[] = L"/:+.";
static constexpr wchar_t DEFAULT_GUARD = START_END_CHARS[0];

static constexpr wchar_t ALPHABET[] = L"0123456789-$:/.+ABCD";

static constexpr int WIDE_TO_NARROW_BAR_RATIO = 2; //TODO: spec says 2.25 to 3 is the valid range. So this is technically illformed.

/**
* These represent the encodings of characters, as patterns of wide and narrow bars. The 7 least-significant bits of
* each int correspond to the pattern of wide and narrow, with 1s representing "wide" and 0s representing narrow.
*/
static const int CHARACTER_ENCODINGS[] = {
	0x003, 0x006, 0x009, 0x060, 0x012, 0x042, 0x021, 0x024, 0x030, 0x048, // 0-9
	0x00c, 0x018, 0x045, 0x051, 0x054, 0x015, 0x01A, 0x029, 0x00B, 0x00E, // -$:/.+ABCD
};

static_assert(Size(ALPHABET) - 1 == Size(CHARACTER_ENCODINGS), "table size mismatch");

BitMatrix
CodabarWriter::encode(const std::wstring& contents_, int width, int height) const
{
	std::wstring contents = contents_;
	if (contents.empty()) {
		throw std::invalid_argument("Found empty contents");
	}
	if (contents.length() < 2) {
		// Can't have a start/end guard, so tentatively add default guards
		contents = DEFAULT_GUARD + contents + DEFAULT_GUARD;
	}
	else {
		// Verify input and calculate decoded length.
		bool startsNormal = Contains(START_END_CHARS, contents.front());
		bool endsNormal = Contains(START_END_CHARS, contents.back());
		bool startsAlt = Contains(ALT_START_END_CHARS, contents.front());
		bool endsAlt = Contains(ALT_START_END_CHARS, contents.back());
		if (startsNormal) {
			if (!endsNormal) {
				throw std::invalid_argument("Invalid start/end guards");
			}
			// else already has valid start/end
		}
		else if (startsAlt) {
			if (!endsAlt) {
				throw std::invalid_argument("Invalid start/end guards");
			}
			// else already has valid start/end

			// map alt characters to normal once so they are found in the ALPHABET
			auto map_alt_guard_char = [](wchar_t& c) {
				switch (c) {
				case 'T': c = 'A'; break;
				case 'N': c = 'B'; break;
				case '*': c = 'C'; break;
				case 'E': c = 'D'; break;
				}
			};
			map_alt_guard_char(contents.front());
			map_alt_guard_char(contents.back());
		}
		else {
			// Doesn't start with a guard
			if (endsNormal || endsAlt) {
				throw std::invalid_argument("Invalid start/end guards");
			}
			// else doesn't end with guard either, so add a default
			contents = DEFAULT_GUARD + contents + DEFAULT_GUARD;
		}
	}

	// The start character and the end character are decoded to 10 length each.
	size_t resultLength = 20;
	for (size_t i = 1; i + 1 < contents.length(); ++i) {
		auto c = contents[i];
		if ((c >= '0' && c <= '9') || c == '-' || c == '$') {
			resultLength += 9;
		}
		else if (Contains(CHARS_WHICH_ARE_TEN_LENGTH_EACH_AFTER_DECODED, c)) {
			resultLength += 10;
		}
		else {
			throw std::invalid_argument(std::string("Cannot encode : '") + static_cast<char>(c) + std::string("'"));
		}
	}
	// A blank is placed between each character.
	resultLength += contents.length() - 1;

	std::vector<bool> result(resultLength, false);
	auto position = result.begin();
	for (wchar_t c : contents) {
		int code = CHARACTER_ENCODINGS[IndexOf(ALPHABET, c)]; // c is checked above to be in ALPHABET
		bool isBlack = true;
		int counter = 0; // count the width of the current bar
		int bit = 0;
		while (bit < 7) { // A character consists of 7 digit.
			*position++ = isBlack;
			++counter;
			if (((code >> (6 - bit)) & 1) == 0 || counter == WIDE_TO_NARROW_BAR_RATIO) { // if current bar is short or we
				isBlack = !isBlack; // Flip the color.
				bit++;
				counter = 0;
			}
		}
		if (position != result.end()) {
			*position++ = false; // inter character whitespace
		}
	}

	return WriterHelper::RenderResult(result, width, height, _sidesMargin >= 0 ? _sidesMargin : 10);
}

BitMatrix CodabarWriter::encode(const std::string& contents, int width, int height) const
{
	return encode(FromUtf8(contents), width, height);
}

} // namespace ZXing::OneD
