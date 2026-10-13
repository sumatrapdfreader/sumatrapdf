/*
* Copyright 2016 Nu-book Inc.
* Copyright 2016 ZXing authors
* Copyright 2020 Axel Waggershauser
*/
// SPDX-License-Identifier: Apache-2.0

#include "ODMultiUPCEANReader.h"

#include "BarcodeFormat.h"
#include "BitArray.h"
#include "ReaderOptions.h"
#include "GTIN.h"
#include "ODUPCEANCommon.h"
#include "Barcode.h"

#include <cmath>

namespace ZXing::OneD {

constexpr int CHAR_LEN = 4;

constexpr auto END_PATTERN           = FixedPattern<3, 3>{1, 1, 1};
constexpr auto MID_PATTERN           = FixedPattern<5, 5>{1, 1, 1, 1, 1};
constexpr auto UPCE_END_PATTERN      = FixedPattern<6, 6>{1, 1, 1, 1, 1, 1};
constexpr auto EXT_START_PATTERN     = FixedPattern<3, 4>{1, 1, 2};
constexpr auto EXT_SEPARATOR_PATTERN = FixedPattern<2, 2>{1, 1};

static const int FIRST_DIGIT_ENCODINGS[] = {0x00, 0x0B, 0x0D, 0x0E, 0x13, 0x19, 0x1C, 0x15, 0x16, 0x1A};

// The GS1 specification has the following to say about quiet zones
// Type: EAN-13 | EAN-8 | UPC-A | UPC-E | EAN Add-on | UPC Add-on
// QZ L:   11   |   7   |   9   |   9   |     7-12   |     9-12
// QZ R:    7   |   7   |   9   |   7   |        5   |        5

constexpr float QUIET_ZONE_LEFT = 6;
constexpr float QUIET_ZONE_RIGHT_EAN = 3; // used to be 6, see #526 and #558
constexpr float QUIET_ZONE_RIGHT_UPC = 6;
constexpr float QUIET_ZONE_ADDON = 3;

// There is a single sample (ean13-1/12.png) that fails to decode with these (new) settings because
// it has a right-side quiet zone of only about 4.5 modules, which is clearly out of spec.

static bool DecodeDigit(const PatternView& view, std::string& txt, int* lgPattern = nullptr)
{
#if 1
	// These two values are critical for determining how permissive the decoding will be.
	// We've arrived at these values through a lot of trial and error. Setting them any higher
	// lets false positives creep in quickly.
	static constexpr float MAX_AVG_VARIANCE = 0.48f;
	static constexpr float MAX_INDIVIDUAL_VARIANCE = 0.7f;

	int bestMatch =
		lgPattern ? RowReader::DecodeDigit(view, UPCEANCommon::L_AND_G_PATTERNS, MAX_AVG_VARIANCE, MAX_INDIVIDUAL_VARIANCE, false)
				  : RowReader::DecodeDigit(view, UPCEANCommon::L_PATTERNS, MAX_AVG_VARIANCE, MAX_INDIVIDUAL_VARIANCE, false);
	if (bestMatch == -1)
		return false;

	txt += ToDigit(bestMatch % 10);
	if (lgPattern)
		AppendBit(*lgPattern, bestMatch >= 10);

	return true;
#else
	constexpr int CHAR_SUM = 7;
	auto pattern = RowReader::OneToFourBitPattern<CHAR_LEN, CHAR_SUM>(view);

	// remove first and last bit
	pattern = (~pattern >> 1) & 0b11111;

	// clang-format off
/* pattern now contains the central 5 bits of the L/G/R code
 * L/G-codes always start with 1 and end with 0, R-codes are simply
 * inverted L-codes.

		L-Code  G-Code  R-Code
	___________________________________
	0 	00110 	10011 	11001
	1 	01100 	11001 	10011
	2 	01001 	01101 	10110
	3 	11110 	10000 	00001
	4 	10001 	01110 	01110
	5 	11000 	11100 	00111
	6 	10111 	00010 	01000
	7 	11101 	01000 	00010
	8 	11011 	00100 	00100
	9 	00101 	01011 	11010
*/
	constexpr char I = 0xf0; // invalid pattern

	const char digit[] = {I,    I,    0x16, I,    0x18, 0x09, 0x00, I,
                          0x17, 0x02, I,    0x19, 0x01, 0x12, 0x14, I,
                          0x13, 0x04, I,    0x10, I,    I,    I,    0x06,
                          0x05, 0x11, I,    0x08, 0x15, 0x07, 0x03, I};
	// clang-format on

	char d = digit[pattern];
	txt += ToDigit(d & 0xf);
	if (lgPattern)
		AppendBit(*lgPattern, (d >> 4) & 1);

	return d != I;
#endif
}

static bool DecodeDigits(int digitCount, PatternView& next, std::string& txt, int* lgPattern = nullptr)
{
	for (int j = 0; j < digitCount; ++j, next.skipSymbol())
		if (!DecodeDigit(next, txt, lgPattern))
			return false;
	return true;
}

struct PartialResult
{
	std::string txt;
	PatternView end;
	BarcodeFormat format = BarcodeFormat::None;

	PartialResult() { txt.reserve(14); }
	bool isValid() const { return format != BarcodeFormat::None; }
};

bool _ret_false_debug_helper()
{
	return false;
}
#define CHECK(A) if(!(A)) return _ret_false_debug_helper();

static bool EAN13(PartialResult& res, PatternView begin)
{
	auto mid = begin.subView(27, MID_PATTERN.size());
	auto end = begin.subView(56, END_PATTERN.size());

	CHECK(end.isValid() && IsRightGuard(end, END_PATTERN, QUIET_ZONE_RIGHT_EAN) && IsPattern(mid, MID_PATTERN));

	auto next = begin.subView(END_PATTERN.size(), CHAR_LEN);
	res.txt = " "; // make space for lgPattern character
	int lgPattern = 0;

	CHECK(DecodeDigits(6, next, res.txt, &lgPattern));

	next = next.subView(MID_PATTERN.size(), CHAR_LEN);

	CHECK(DecodeDigits(6, next, res.txt));

	int i = IndexOf(FIRST_DIGIT_ENCODINGS, lgPattern);
	CHECK(i != -1);
	res.txt[0] = ToDigit(i);

	res.end = end;
	res.format = BarcodeFormat::EAN13;
	return true;
}

static bool PlausibleDigitModuleSize(PatternView begin, int start, int i, float moduleSizeRef)
{
	float moduleSizeData = begin.subView(start + i * 4, 4).sum() / 7.f;
	return std::abs(moduleSizeData / moduleSizeRef - 1) < 0.2f;
}

static bool EAN8(PartialResult& res, PatternView begin)
{
	auto mid = begin.subView(19, MID_PATTERN.size());
	auto end = begin.subView(40, END_PATTERN.size());

	CHECK(end.isValid() && IsRightGuard(end, END_PATTERN, QUIET_ZONE_RIGHT_EAN) && IsPattern(mid, MID_PATTERN));

	// additional plausibility check for the module size: it has to be about the same for both
	// the guard patterns and the payload/data part.
	float moduleSizeGuard = (begin.sum() + mid.sum() + end.sum()) / 11.f;
	for (auto start : {3, 24})
		for (int i = 0; i < 4; ++i)
			CHECK(PlausibleDigitModuleSize(begin, start, i, moduleSizeGuard));

	auto next = begin.subView(END_PATTERN.size(), CHAR_LEN);
	res.txt.clear();

	CHECK(DecodeDigits(4, next, res.txt));

	next = next.subView(MID_PATTERN.size(), CHAR_LEN);

	CHECK(DecodeDigits(4, next, res.txt));

	res.end = end;
	res.format = BarcodeFormat::EAN8;
	return true;
}

static bool UPCE(PartialResult& res, PatternView begin)
{
	auto end = begin.subView(27, UPCE_END_PATTERN.size());

	CHECK(end.isValid() && IsRightGuard(end, UPCE_END_PATTERN, QUIET_ZONE_RIGHT_UPC));

	// additional plausibility check for the module size: it has to be about the same for both
	// the guard patterns and the payload/data part. This speeds up the falsepositives use case
	// about 2x and brings the misread count down to 0
	float moduleSizeGuard = (begin.sum() + end.sum()) / 9.f;
	for (int i = 0; i < 6; ++i)
		CHECK(PlausibleDigitModuleSize(begin, 3, i, moduleSizeGuard));

	auto next = begin.subView(END_PATTERN.size(), CHAR_LEN);
	int lgPattern = 0;
	res.txt = " "; // make space for lgPattern character

	CHECK(DecodeDigits(6, next, res.txt, &lgPattern));

	int i = IndexOf(UPCEANCommon::NUMSYS_AND_CHECK_DIGIT_PATTERNS, lgPattern);
	CHECK(i != -1);

	res.txt[0] = ToDigit(i / 10);
	res.txt += ToDigit(i % 10);

	res.end = end;
	res.format = BarcodeFormat::UPCE;
	return true;
}

static int Ean5Checksum(const std::string& s)
{
	int sum = 0, N = Size(s);
	for (int i = N - 2; i >= 0; i -= 2)
		sum += s[i] - '0';
	sum *= 3;
	for (int i = N - 1; i >= 0; i -= 2)
		sum += s[i] - '0';
	sum *= 3;
	return sum % 10;
}

static bool AddOn(PartialResult& res, PatternView begin, int digitCount)
{
	auto ext = begin.subView(0, 3 + digitCount * 4 + (digitCount - 1) * 2);
	CHECK(ext.isValid());
	auto moduleSize = IsPattern(ext, EXT_START_PATTERN);
	CHECK(moduleSize);

	CHECK(ext.isAtLastBar() || *ext.end() > QUIET_ZONE_ADDON * moduleSize - 1);

	res.end = ext;
	ext = ext.subView(EXT_START_PATTERN.size(), CHAR_LEN);
	int lgPattern = 0;
	res.txt.clear();

	for (int i = 0; i < digitCount; ++i) {
		CHECK(DecodeDigit(ext, res.txt, &lgPattern));
		ext.skipSymbol();
		if (i < digitCount - 1) {
			CHECK(IsPattern(ext, EXT_SEPARATOR_PATTERN, 0, 0, moduleSize));
			ext.skipPair();
		}
	}

	if (digitCount == 2) {
		CHECK(std::stoi(res.txt) % 4 == lgPattern);
	} else {
		constexpr int CHECK_DIGIT_ENCODINGS[] = {0x18, 0x14, 0x12, 0x11, 0x0C, 0x06, 0x03, 0x0A, 0x09, 0x05};
		CHECK(Ean5Checksum(res.txt) == IndexOf(CHECK_DIGIT_ENCODINGS, lgPattern));
	}
	res.format = BarcodeFormat::Any; // make sure res.format is valid, see below
	return true;
}

Barcode MultiUPCEANReader::decodePattern(int rowNumber, PatternView& next, std::unique_ptr<RowReader::DecodingState>&) const
{
	const int minSize = 3 + 6*4 + 6; // UPC-E

	next = FindLeftGuard(next, minSize, END_PATTERN, QUIET_ZONE_LEFT);
	if (!next.isValid())
		return {};

	PartialResult res;
	auto begin = next;
	
	if (!(((_opts.hasFormat(BarcodeFormat::EAN13 | BarcodeFormat::UPCA)) && EAN13(res, begin)) ||
		  (_opts.hasFormat(BarcodeFormat::EAN8) && EAN8(res, begin)) ||
		  (_opts.hasFormat(BarcodeFormat::UPCE) && UPCE(res, begin))))
		return {};

	Error error;
	if (!GTIN::IsCheckDigitValid(res.format == BarcodeFormat::UPCE ? UPCEANCommon::ConvertUPCEtoUPCA(res.txt) : res.txt))
		error = ChecksumError();

	// If UPC-A was a requested format and we detected a EAN-13 code with a leading '0', then we drop the '0' and call it
	// a UPC-A code.
	// TODO: this is questionable
	if (_opts.hasFormat(BarcodeFormat::UPCA) && res.format == BarcodeFormat::EAN13 && res.txt.front() == '0') {
		res.txt = res.txt.substr(1);
		res.format = BarcodeFormat::UPCA;
	}

	// if we explicitly requested UPCA but not EAN13, don't return an EAN13 symbol
	if (res.format == BarcodeFormat::EAN13 && ! _opts.hasFormat(BarcodeFormat::EAN13))
		return {};

	// Symbology identifier modifiers ISO/IEC 15420:2009 Annex B Table B.1
	// ISO/IEC 15420:2009 (& GS1 General Specifications 5.1.3) states that the content for "]E0" should be 13 digits,
	// i.e. converted to EAN-13 if UPC-A/E, but not doing this here to maintain backward compatibility
	SymbologyIdentifier symbologyIdentifier = {'E', res.format == BarcodeFormat::EAN8 ? '4' : '0'};

	next = res.end;

	auto ext = res.end;
	PartialResult addOnRes;
	if (_opts.eanAddOnSymbol() != EanAddOnSymbol::Ignore && ext.skipSymbol() && ext.skipSingle(static_cast<int>(begin.sum() * 3.5))
		&& (AddOn(addOnRes, ext, 5) || AddOn(addOnRes, ext, 2))) {
		// ISO/IEC 15420:2009 states that the content for "]E3" should be 15 or 18 digits, i.e. converted to EAN-13
		// and extended with no separator, and that the content for "]E4" should be 8 digits, i.e. no add-on
		res.txt += " " + addOnRes.txt;
		next = addOnRes.end;

		if (res.format != BarcodeFormat::EAN8) // Keeping EAN-8 with add-on as "]E4"
			symbologyIdentifier.modifier = '3'; // Combined packet, EAN-13, UPC-A, UPC-E, with add-on
	}
	
	if (_opts.eanAddOnSymbol() == EanAddOnSymbol::Require && !addOnRes.isValid())
		return {};

	return Barcode(res.txt, rowNumber, begin.pixelsInFront(), next.pixelsTillEnd(), res.format, symbologyIdentifier, error);
}

} // namespace ZXing::OneD
