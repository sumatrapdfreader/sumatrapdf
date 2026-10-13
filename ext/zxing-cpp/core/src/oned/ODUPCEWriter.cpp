/*
* Copyright 2016 Huy Cuong Nguyen
* Copyright 2016 ZXing authors
*/
// SPDX-License-Identifier: Apache-2.0

#include "ODUPCEWriter.h"

#include "ODUPCEANCommon.h"
#include "ODWriterHelper.h"
#include "Utf.h"

#include <stdexcept>
#include <vector>

namespace ZXing::OneD {

static const int CODE_WIDTH = 3 + // start guard
                              (7 * 6) + // bars
                              6; // end guard

BitMatrix
UPCEWriter::encode(const std::wstring& contents, int width, int height) const
{
	auto digits = UPCEANCommon::DigitString2IntArray<8>(
		contents, GTIN::ComputeCheckDigit(UPCEANCommon::ConvertUPCEtoUPCA(contents), contents.size() == 8));

	int firstDigit = digits[0];
	if (firstDigit != 0 && firstDigit != 1) {
		throw std::invalid_argument("Number system must be 0 or 1");
	}

	int parities = UPCEANCommon::NUMSYS_AND_CHECK_DIGIT_PATTERNS[firstDigit * 10 + digits[7]];
	std::vector<bool> result(CODE_WIDTH, false);
	int pos = 0;

	pos += WriterHelper::AppendPattern(result, pos, UPCEANCommon::START_END_PATTERN, true);

	for (int i = 1; i <= 6; i++) {
		int digit = digits[i];
		if ((parities >> (6 - i) & 1) == 1) {
			digit += 10;
		}
		pos += WriterHelper::AppendPattern(result, pos, UPCEANCommon::L_AND_G_PATTERNS[digit], false);
	}

	WriterHelper::AppendPattern(result, pos, UPCEANCommon::UPCE_END_PATTERN, false);
	return WriterHelper::RenderResult(result, width, height, _sidesMargin >= 0 ? _sidesMargin : 9);
}

BitMatrix UPCEWriter::encode(const std::string& contents, int width, int height) const
{
	return encode(FromUtf8(contents), width, height);
}

} // namespace ZXing::OneD
