/*
* Copyright 2016 Nu-book Inc.
* Copyright 2016 ZXing authors
*/
// SPDX-License-Identifier: Apache-2.0

#include "ODCode128Reader.h"

#include "ODCode128Patterns.h"
#include "Barcode.h"
#include "ZXAlgorithms.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace ZXing::OneD {

static const float MAX_AVG_VARIANCE = 0.25f;
static const float MAX_INDIVIDUAL_VARIANCE = 0.7f;

static const int CODE_SHIFT = 98;

static const int CODE_CODE_C = 99;
static const int CODE_CODE_B = 100;
static const int CODE_CODE_A = 101;

static const int CODE_FNC_1 = 102;
static const int CODE_FNC_2 = 97;
static const int CODE_FNC_3 = 96;

static const int CODE_START_A = 103;
static const int CODE_START_C = 105;
static const int CODE_STOP = 106;

class Raw2TxtDecoder
{
	int codeSet = 0;
	SymbologyIdentifier _symbologyIdentifier = {'C', '0'}; // ISO/IEC 15417:2007 Annex C Table C.1
	bool _readerInit = false;
	std::string txt;
	size_t lastTxtSize = 0;

	bool fnc4All = false;
	bool fnc4Next = false;
	bool shift = false;

	void fnc1(const bool isCodeSetC)
	{
		if (txt.empty()) {
			// ISO/IEC 15417:2007 Annex B.1 and GS1 General Specifications 21.0.1 Section 5.4.3.7
			// If the first char after the start code is FNC1 then this is GS1-128.
			_symbologyIdentifier.modifier = '1';
			// GS1 General Specifications Section 5.4.6.4
			// "Transmitted data ... is prefixed by the symbology identifier ]C1, if used."
			// Choosing not to use symbology identifier, i.e. to not prefix to data.
			_symbologyIdentifier.aiFlag = AIFlag::GS1;
		}
		else if ((isCodeSetC && txt.size() == 2 && txt[0] >= '0' && txt[0] <= '9' && txt[1] >= '0' && txt[1] <= '9')
				|| (!isCodeSetC && txt.size() == 1 && ((txt[0] >= 'A' && txt[0] <= 'Z')
														|| (txt[0] >= 'a' && txt[0] <= 'z')))) {
			// ISO/IEC 15417:2007 Annex B.2
			// FNC1 in second position following Code Set C "00-99" or Code Set A/B "A-Za-z" - AIM
			_symbologyIdentifier.modifier = '2';
			_symbologyIdentifier.aiFlag = AIFlag::AIM;
		}
		else {
			// ISO/IEC 15417:2007 Annex B.3. Otherwise FNC1 is returned as ASCII 29 (GS)
			txt.push_back((char)29);
		}
	};

public:
	Raw2TxtDecoder(int startCode) : codeSet(204 - startCode)
	{
		txt.reserve(20);
	}

	bool decode(int code)
	{
		lastTxtSize = txt.size();

		if (codeSet == CODE_CODE_C) {
			if (code < 100) {
				txt.append(ToString(code, 2));
			} else if (code == CODE_FNC_1) {
				fnc1(true /*isCodeSetC*/);
			} else {
				codeSet = code; // CODE_A / CODE_B
			}
		} else { // codeSet A or B
			bool unshift = shift;

			switch (code) {
			case CODE_FNC_1: fnc1(false /*isCodeSetC*/); break;
			case CODE_FNC_2:
				// Message Append - do nothing?
				break;
			case CODE_FNC_3:
				_readerInit = true; // Can occur anywhere in the symbol (ISO/IEC 15417:2007 4.3.4.2 (c))
				break;
			case CODE_SHIFT:
				if (shift)
					return false; // two shifts in a row make no sense
				shift = true;
				codeSet = codeSet == CODE_CODE_A ? CODE_CODE_B : CODE_CODE_A;
				break;
			case CODE_CODE_A:
			case CODE_CODE_B:
				if (codeSet == code) {
					// FNC4
					if (fnc4Next)
						fnc4All = !fnc4All;
					fnc4Next = !fnc4Next;
				} else {
					codeSet = code;
				}
				break;
			case CODE_CODE_C: codeSet = CODE_CODE_C; break;

			default: {
				// code < 96 at this point
				int offset;
				if (codeSet == CODE_CODE_A && code >= 64)
					offset = fnc4All == fnc4Next ? -64 : +64;
				else
					offset = fnc4All == fnc4Next ? ' ' : ' ' + 128;
				txt.push_back((char)(code + offset));
				fnc4Next = false;
				break;
			}
			}

			// Unshift back to another code set if we were shifted
			if (unshift) {
				codeSet = codeSet == CODE_CODE_A ? CODE_CODE_B : CODE_CODE_A;
				shift = false;
			}
		}

		return true;
	}

	std::string text() const
	{
		// Need to pull out the check digit(s) from string (if the checksum code happened to
		// be a printable character).
		return txt.substr(0, lastTxtSize);
	}

	SymbologyIdentifier symbologyIdentifier() const { return _symbologyIdentifier; }
	bool readerInit() const { return _readerInit; }
};

// all 3 start patterns share the same 2-1-1 prefix
constexpr auto START_PATTERN_PREFIX = FixedPattern<3, 4>{2, 1, 1};
constexpr int CHAR_LEN = 6;
constexpr float QUIET_ZONE = 5;	// quiet zone spec is 10 modules, real world examples ignore that, see #138
constexpr int CHAR_MODS = 11;

//TODO: make this a constexpr variable initialization
static auto E2E_PATTERNS = [] {
	// This creates an array of ints for fast IndexOf lookup of the edge-2-edge patterns (ISO/IEC 15417:2007(E) Table 2)
	// e.g. a code pattern of { 2, 1, 2, 2, 2, 2 } becomes the e2e pattern { 3, 3, 4, 4 } and the value 0b11100011110000.
	std::array<int, 107> res;
	for (int i = 0; i < Size(res); ++i) {
		const auto& a = Code128::CODE_PATTERNS[i];
		std::array<int, 4> e2e;
		for (int j = 0; j < 4; j++)
			e2e[j] = a[j] + a[j + 1];
		res[i] = ToInt(e2e);
	}
	return res;
}();

Barcode Code128Reader::decodePattern(int rowNumber, PatternView& next, std::unique_ptr<DecodingState>&) const
{
	int minCharCount = 4; // start + payload + checksum + stop
	auto decodePattern = [](const PatternView& view, bool start = false) {
		// This is basically the reference algorithm from the specification
		int code = IndexOf(E2E_PATTERNS, ToInt(NormalizedE2EPattern<CHAR_LEN>(view, CHAR_MODS)));
		if (code == -1 && !start) // if the reference algo fails, give the original upstream version a try (required to decode a few samples)
			code = DecodeDigit(view, Code128::CODE_PATTERNS, MAX_AVG_VARIANCE, MAX_INDIVIDUAL_VARIANCE);
		return code;
	};

	next = FindLeftGuard(next, minCharCount * CHAR_LEN, START_PATTERN_PREFIX, QUIET_ZONE);
	if (!next.isValid())
		return {};

	next = next.subView(0, CHAR_LEN);
	int startCode = decodePattern(next, true);
	if (!(CODE_START_A <= startCode && startCode <= CODE_START_C))
		return {};

	int xStart = next.pixelsInFront();
	ByteArray rawCodes;
	rawCodes.reserve(20);
	rawCodes.push_back(narrow_cast<uint8_t>(startCode));

	Raw2TxtDecoder raw2txt(startCode);

	while (true) {
		if (!next.skipSymbol())
			return {};

		// Decode another code from image
		int code = decodePattern(next);
		if (code == -1)
			return {};
		if (code == CODE_STOP)
			break;
		if (code >= CODE_START_A)
			return {};
		if (!raw2txt.decode(code))
			return {};

		rawCodes.push_back(narrow_cast<uint8_t>(code));
	}

	if (Size(rawCodes) < minCharCount - 1) // stop code is missing in rawCodes
		return {};

	// check termination bar (is present and not wider than about 2 modules) and quiet zone (next is now 13 modules
	// wide, require at least 8)
	next = next.subView(0, CHAR_LEN + 1);
	if (!next.isValid() || next[CHAR_LEN] > next.sum(CHAR_LEN) / 4 || !next.hasQuietZoneAfter(QUIET_ZONE/13))
		return {};

	Error error;
	int checksum = rawCodes.front();
	for (int i = 1; i < Size(rawCodes) - 1; ++i)
		checksum += i * rawCodes[i];
	// the last code is the checksum:
	if (checksum % 103 != rawCodes.back())
		error = ChecksumError();

	int xStop = next.pixelsTillEnd();
	return Barcode(raw2txt.text(), rowNumber, xStart, xStop, BarcodeFormat::Code128, raw2txt.symbologyIdentifier(), error,
				   raw2txt.readerInit());
}

} // namespace ZXing::OneD
