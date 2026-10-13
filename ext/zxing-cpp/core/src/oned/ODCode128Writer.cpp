/*
* Copyright 2016 Huy Cuong Nguyen
* Copyright 2016 ZXing authors
*/
// SPDX-License-Identifier: Apache-2.0

#include "ODCode128Writer.h"

#include "ODCode128Patterns.h"
#include "ODWriterHelper.h"
#include "Utf.h"

#include <list>
#include <numeric>
#include <stdexcept>
#include <vector>

namespace ZXing::OneD {

static const int CODE_START_A = 103;
static const int CODE_START_B = 104;
static const int CODE_START_C = 105;
static const int CODE_CODE_A  = 101;
static const int CODE_CODE_B  = 100;
static const int CODE_CODE_C  = 99;
static const int CODE_STOP    = 106;

// Dummy characters used to specify control characters in input
static const auto ESCAPE_FNC_1 = L'\u00f1';
static const auto ESCAPE_FNC_2 = L'\u00f2';
static const auto ESCAPE_FNC_3 = L'\u00f3';
static const auto ESCAPE_FNC_4 = L'\u00f4';

static const int CODE_FNC_1   = 102; // Code A, Code B, Code C
static const int CODE_FNC_2   = 97;  // Code A, Code B
static const int CODE_FNC_3   = 96;  // Code A, Code B
static const int CODE_FNC_4_A = 101; // Code A
static const int CODE_FNC_4_B = 100; // Code B

// Results of minimal lookahead for code C
enum class CType
{
	UNCODABLE,
	ONE_DIGIT,
	TWO_DIGITS,
	FNC_1
};

static CType FindCType(const std::wstring& value, int start)
{
	int last = Size(value);
	if (start >= last) {
		return CType::UNCODABLE;
	}
	wchar_t c = value[start];
	if (c == ESCAPE_FNC_1) {
		return CType::FNC_1;
	}
	if (c < '0' || c > '9') {
		return CType::UNCODABLE;
	}
	if (start + 1 >= last) {
		return CType::ONE_DIGIT;
	}
	c = value[start + 1];
	if (c < '0' || c > '9') {
		return CType::ONE_DIGIT;
	}
	return CType::TWO_DIGITS;
}

static int ChooseCode(const std::wstring& value, int start, int oldCode)
{
	CType lookahead = FindCType(value, start);
	if (lookahead == CType::ONE_DIGIT) {
		if (oldCode == CODE_CODE_A) {
			return CODE_CODE_A;
		}
		return CODE_CODE_B;
	}
	if (lookahead == CType::UNCODABLE) {
		if (start < Size(value)) {
			int c = value[start];
			if (c < ' ' || (oldCode == CODE_CODE_A && (c < '`' || (c >= ESCAPE_FNC_1 && c <= ESCAPE_FNC_4)))) {
				// can continue in code A, encodes ASCII 0 to 95 or FNC1 to FNC4
				return CODE_CODE_A;
			}
		}
		return CODE_CODE_B; // no choice
	}
	if (oldCode == CODE_CODE_A && lookahead == CType::FNC_1) {
		return CODE_CODE_A;
	}
	if (oldCode == CODE_CODE_C) { // can continue in code C
		return CODE_CODE_C;
	}
	if (oldCode == CODE_CODE_B) {
		if (lookahead == CType::FNC_1) {
			return CODE_CODE_B; // can continue in code B
		}
		// Seen two consecutive digits, see what follows
		lookahead = FindCType(value, start + 2);
		if (lookahead == CType::UNCODABLE || lookahead == CType::ONE_DIGIT) {
			return CODE_CODE_B; // not worth switching now
		}
		if (lookahead == CType::FNC_1) { // two digits, then FNC_1...
			lookahead = FindCType(value, start + 3);
			if (lookahead == CType::TWO_DIGITS) { // then two more digits, switch
				return CODE_CODE_C;
			}
			else {
				return CODE_CODE_B; // otherwise not worth switching
			}
		}
		// At this point, there are at least 4 consecutive digits.
		// Look ahead to choose whether to switch now or on the next round.
		int index = start + 4;
		while ((lookahead = FindCType(value, index)) == CType::TWO_DIGITS) {
			index += 2;
		}
		if (lookahead == CType::ONE_DIGIT) { // odd number of digits, switch later
			return CODE_CODE_B;
		}
		return CODE_CODE_C; // even number of digits, switch now
	}
	// Here oldCode == 0, which means we are choosing the initial code
	if (lookahead == CType::FNC_1) { // ignore FNC_1
		lookahead = FindCType(value, start + 1);
	}
	if (lookahead == CType::TWO_DIGITS) { // at least two digits, start in code C
		return CODE_CODE_C;
	}
	return CODE_CODE_B;
}

BitMatrix
Code128Writer::encode(const std::wstring& contents, int width, int height) const
{
	// Check length
	int length = Size(contents);
	if (length < 1 || length > 80) {
		throw std::invalid_argument("Contents length should be between 1 and 80 characters");
	}

	// Check content
	for (int i = 0; i < length; ++i) {
		int c = contents[i];
		switch (c) {
		case ESCAPE_FNC_1:
		case ESCAPE_FNC_2:
		case ESCAPE_FNC_3:
		case ESCAPE_FNC_4: break;
		default:
			if (c > 127) {
				// support for FNC4 isn't implemented, no full Latin-1 character set available at the moment
				throw std::invalid_argument("Bad character in input: " + ToUtf8(contents.substr(i, 1)));
			}
		}
	}

	std::list<std::array<int, 6>> patterns; // temporary storage for patterns
	int checkSum = 0;
	int checkWeight = 1;
	int codeSet = 0; // selected code (CODE_CODE_B or CODE_CODE_C)
	int position = 0; // position in contents

	while (position < length) {
		//Select code to use
		int newCodeSet = ChooseCode(contents, position, codeSet);

		//Get the pattern index
		int patternIndex;
		if (newCodeSet == codeSet) {
			// Encode the current character
			// First handle escapes
			switch (contents[position]) {
			case ESCAPE_FNC_1: patternIndex = CODE_FNC_1; break;
			case ESCAPE_FNC_2: patternIndex = CODE_FNC_2; break;
			case ESCAPE_FNC_3: patternIndex = CODE_FNC_3; break;
			case ESCAPE_FNC_4: patternIndex = (codeSet == CODE_CODE_A) ? CODE_FNC_4_A : CODE_FNC_4_B; break;
			default:
				// Then handle normal characters otherwise
				if (codeSet == CODE_CODE_A) {
					patternIndex = contents[position] - ' ';
					if (patternIndex < 0) {
						// everything below a space character comes behind the underscore in the code patterns table
						patternIndex += '`';
					}
				} else if (codeSet == CODE_CODE_B) {
					patternIndex = contents[position] - ' ';
				} else { // CODE_CODE_C
					patternIndex =
						(contents[position] - '0') * 10 + (position + 1 < length ? contents[position + 1] - '0' : 0);
					position++; // Also incremented below
				}
			}
			position++;
		}
		else {
			// Should we change the current code?
			// Do we have a code set?
			if (codeSet == 0) {
				// No, we don't have a code set
				if (newCodeSet == CODE_CODE_A) {
					patternIndex = CODE_START_A;
				}
				else if (newCodeSet == CODE_CODE_B) {
					patternIndex = CODE_START_B;
				}
				else {
					// CODE_CODE_C
					patternIndex = CODE_START_C;
				}
			}
			else {
				// Yes, we have a code set
				patternIndex = newCodeSet;
			}
			codeSet = newCodeSet;
		}

		// Get the pattern
		patterns.push_back(Code128::CODE_PATTERNS[patternIndex]);

		// Compute checksum
		checkSum += patternIndex * checkWeight;
		if (position != 0) {
			checkWeight++;
		}
	}

	// Compute and append checksum
	checkSum %= 103;
	patterns.push_back(Code128::CODE_PATTERNS[checkSum]);

	// Append stop code
	patterns.push_back(Code128::CODE_PATTERNS[CODE_STOP]);

	// Compute code width
	int codeWidth = 2; // termination bar
	for (const auto& pattern : patterns) {
		codeWidth += Reduce(pattern);
	}

	// Compute result
	std::vector<bool> result(codeWidth, false);
	const auto op = [&result](auto pos, const auto& pattern){ return pos + WriterHelper::AppendPattern(result, pos, pattern, true);};
	auto pos = std::accumulate(std::begin(patterns), std::end(patterns), int{}, op);
	// Append termination bar
	result[pos++] = true;
	result[pos++] = true;

	return WriterHelper::RenderResult(result, width, height, _sidesMargin >= 0 ? _sidesMargin : 10);
}

BitMatrix Code128Writer::encode(const std::string& contents, int width, int height) const
{
	return encode(FromUtf8(contents), width, height);
}

} // namespace ZXing::OneD
