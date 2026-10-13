/*
* Copyright 2016 Nu-book Inc.
* Copyright 2016 ZXing authors
*/
// SPDX-License-Identifier: Apache-2.0

#include "ODITFReader.h"

#include "ReaderOptions.h"
#include "GTIN.h"
#include "Barcode.h"
#include "ZXAlgorithms.h"

namespace ZXing::OneD {

Barcode ITFReader::decodePattern(int rowNumber, PatternView& next, std::unique_ptr<DecodingState>&) const
{
	const int minCharCount = _opts.formats().count() == 1 ? 4 : 6; // if we are only looking for ITF, we accept shorter symbols
	const int minQuietZone = 6; // spec requires 10

	next = FindLeftGuard(next, 4 + minCharCount/2 + 3, FixedPattern<4, 4>{1, 1, 1, 1}, minQuietZone);
	if (!next.isValid())
		return {};

	// get threshold of first character pair
	auto threshold = NarrowWideThreshold(next.subView(4, 10));
	if (!threshold.isValid())
		return {};
	// check that each bar/space in the start pattern is < threshold
	for (int i = 0; i < 4; ++i)
		if (next[i] > threshold[i])
			return {};

	constexpr int weights[] = {1, 2, 4, 7, 0};
	int xStart = next.pixelsInFront();
	bool startsAtFirstBar = next.isAtFirstBar();

	next = next.subView(4, 10);

	std::string txt;
	txt.reserve(20);

	while (next.isValid()) {
		// look for end-of-symbol
		if (next[3] > threshold.space * 3)
			break;

		BarAndSpace<int> digits, numWide;
		bool bad = false;
		for (int i = 0; i < 10; ++i) {
			bad |= next[i] > threshold[i] * 3 || next[i] < threshold[i] / 3;
			numWide[i] += next[i] > threshold[i];
			digits[i] += weights[i/2] * (next[i] > threshold[i]);
		}

		if (bad || numWide.bar != 2 || numWide.space != 2)
			break;

		for (int i = 0; i < 2; ++i)
			txt.push_back(ToDigit(digits[i] == 11 ? 0 : digits[i]));

		// update threshold to support scanning slanted symbols (scanned non-perpendicular)
		threshold = NarrowWideThreshold(next);

		next.skipSymbol();
	}

	next = next.subView(0, 3);

	// Check stop pattern
	if (!next.isValid() || !threshold.isValid()
		|| next[0] < threshold[0] || next[1] > threshold[1] || next[2] > threshold[2])
		return {};

	// Check quiet zone size (full quiet zone or cropped on both ends)
	if (!(next[3] > minQuietZone * (threshold.bar + threshold.space) / 3
		  || (next.isAtLastBar() && startsAtFirstBar && std::max(xStart, (int)next[3]) < 2 * std::min(xStart, (int)next[3]) + 2)))
		return {};

	// Check min length depending on whether the code covers the complete image or not
	if (Size(txt) < (startsAtFirstBar && next.isAtLastBar() ? (minCharCount / 2) : minCharCount))
		return {};

	Error error = _opts.validateITFCheckSum() && !GTIN::IsCheckDigitValid(txt) ? ChecksumError() : Error();

	// Symbology identifier ISO/IEC 16390:2007 Annex C Table C.1
	// See also GS1 General Specifications 5.1.2 Figure 5.1.2-2
	SymbologyIdentifier symbologyIdentifier = {'I', GTIN::IsCheckDigitValid(txt) ? '1' : '0'};
	
	int xStop = next.pixelsTillEnd();
	return Barcode(txt, rowNumber, xStart, xStop, BarcodeFormat::ITF, symbologyIdentifier, error);
}

} // namespace ZXing::OneD
