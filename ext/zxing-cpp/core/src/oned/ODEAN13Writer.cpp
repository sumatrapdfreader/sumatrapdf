/*
* Copyright 2016 Huy Cuong Nguyen
* Copyright 2016 ZXing authors
*/
// SPDX-License-Identifier: Apache-2.0

#include "ODEAN13Writer.h"

#include "ODUPCEANCommon.h"
#include "ODWriterHelper.h"
#include "Utf.h"

#include <array>
#include <vector>

namespace ZXing::OneD {

static const int FIRST_DIGIT_ENCODINGS[] = {
	0x00, 0x0B, 0x0D, 0xE, 0x13, 0x19, 0x1C, 0x15, 0x16, 0x1A
};

static const int CODE_WIDTH = 3 + // start guard
                              (7 * 6) + // left bars
                              5 + // middle guard
                              (7 * 6) + // right bars
                              3; // end guard

BitMatrix
EAN13Writer::encode(const std::wstring& contents, int width, int height) const
{
	auto digits = UPCEANCommon::DigitString2IntArray<13>(contents);

	int parities = FIRST_DIGIT_ENCODINGS[digits[0]];
	std::vector<bool> result(CODE_WIDTH, false);
	int pos = 0;

	pos += WriterHelper::AppendPattern(result, pos, UPCEANCommon::START_END_PATTERN, true);

	// See {@link #EAN13Reader} for a description of how the first digit & left bars are encoded
	for (int i = 1; i <= 6; i++) {
		int digit = digits[i];
		if ((parities >> (6 - i) & 1) == 1) {
			digit += 10;
		}
		pos += WriterHelper::AppendPattern(result, pos, UPCEANCommon::L_AND_G_PATTERNS[digit], false);
	}

	pos += WriterHelper::AppendPattern(result, pos, UPCEANCommon::MIDDLE_PATTERN, false);

	for (int i = 7; i <= 12; i++) {
		pos += WriterHelper::AppendPattern(result, pos, UPCEANCommon::L_PATTERNS[digits[i]], true);
	}
	WriterHelper::AppendPattern(result, pos, UPCEANCommon::START_END_PATTERN, true);
	return WriterHelper::RenderResult(result, width, height, _sidesMargin >= 0 ? _sidesMargin : 9);
}

BitMatrix EAN13Writer::encode(const std::string& contents, int width, int height) const
{
	return encode(FromUtf8(contents), width, height);
}

} // namespace ZXing::OneD
