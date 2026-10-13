/*
* Copyright 2016 Huy Cuong Nguyen
* Copyright 2016 ZXing authors
*/
// SPDX-License-Identifier: Apache-2.0

#include "ODEAN8Writer.h"

#include "ODUPCEANCommon.h"
#include "ODWriterHelper.h"
#include "Utf.h"

#include <vector>

namespace ZXing::OneD {

static const int CODE_WIDTH = 3 + // start guard
                              (7 * 4) + // left bars
                              5 + // middle guard
                              (7 * 4) + // right bars
                              3; // end guard

BitMatrix
EAN8Writer::encode(const std::wstring& contents, int width, int height) const
{
	auto digits = UPCEANCommon::DigitString2IntArray<8>(contents);

	std::vector<bool> result(CODE_WIDTH, false);
	int pos = 0;

	pos += WriterHelper::AppendPattern(result, pos, UPCEANCommon::START_END_PATTERN, true);

	for (int i = 0; i <= 3; i++) {
		pos += WriterHelper::AppendPattern(result, pos, UPCEANCommon::L_PATTERNS[digits[i]], false);
	}

	pos += WriterHelper::AppendPattern(result, pos, UPCEANCommon::MIDDLE_PATTERN, false);

	for (int i = 4; i <= 7; i++) {
		pos += WriterHelper::AppendPattern(result, pos, UPCEANCommon::L_PATTERNS[digits[i]], true);
	}
	WriterHelper::AppendPattern(result, pos, UPCEANCommon::START_END_PATTERN, true);
	return WriterHelper::RenderResult(result, width, height, _sidesMargin >= 0 ? _sidesMargin : 9);
}

BitMatrix EAN8Writer::encode(const std::string& contents, int width, int height) const
{
	return encode(FromUtf8(contents), width, height);
}

} // namespace ZXing::OneD
