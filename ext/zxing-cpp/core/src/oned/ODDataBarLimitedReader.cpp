/*
* Copyright 2024 Axel Waggershauser
*/
// SPDX-License-Identifier: Apache-2.0

#include "ODDataBarLimitedReader.h"

#include "BarcodeFormat.h"
#include "GTIN.h"
#include "ODDataBarCommon.h"
#include "Barcode.h"

//#define PRINT_DEBUG
#ifndef PRINT_DEBUG
#define printf(...){}
#define printv(...){}
#else
#define printv(fmt, ...) \
for (auto v : __VA_ARGS__) \
	printf(fmt, v);
#endif

namespace ZXing::OneD {

using namespace DataBar;

constexpr int CHAR_LEN = 14;
constexpr int SYMBOL_LEN = 1 + 3 * CHAR_LEN + 2;

static Character ReadDataCharacter(const PatternView& view)
{
	constexpr int G_SUM[] = {0, 183064, 820064, 1000776, 1491021, 1979845, 1996939};
	constexpr int T_EVEN[] = {28, 728, 6454, 203, 2408, 1, 16632};
	constexpr int ODD_SUM[] = {17, 13, 9, 15, 11, 19, 7};
	constexpr int ODD_WIDEST[] = {6, 5, 3, 5, 4, 8, 1};

	auto pattern = NormalizedPatternFromE2E<14>(view, 26);

	int checkSum = 0;
	for (auto it = pattern.rbegin(); it != pattern.rend(); ++it)
		checkSum = 3 * checkSum + *it;

	using Array7I = std::array<int, 7>;
	Array7I oddPattern = {}, evnPattern = {};
	OddEven<Array7I&> res = {oddPattern, evnPattern};

	for (int i = 0; i < Size(pattern); ++i)
		res[i % 2][i / 2] = pattern[i];

	printf(" o: ");
	printv("%d ", oddPattern);
	printf(" e: ");
	printv("%d ", evnPattern);

	int group = IndexOf(ODD_SUM, Reduce(oddPattern));
	if (group == -1)
		return {};

	int oddWidest = ODD_WIDEST[group];
	int evnWidest = 9 - oddWidest;
#ifndef __cpp_lib_span
#pragma message("DataBarLimited not supported without std::span<> (c++20 feature)")
	int vOdd = 0;
	int vEvn = 0;
#else
	int vOdd = GetValue(oddPattern, oddWidest, false);
	int vEvn = GetValue(evnPattern, evnWidest, true);
#endif
	int tEvn = T_EVEN[group];
	int gSum = G_SUM[group];

	return {vOdd * tEvn + vEvn + gSum, checkSum};
}

static std::string ConstructText(Character left, Character right)
{
	auto symVal = 2'013'571LL * left.value + right.value;

	// Strip 2D linkage flag (GS1 Composite) if any (ISO/IEC 24724:2011 Section 6.2.3)
	if (symVal >= 2'015'133'531'096LL) {
		symVal -= 2'015'133'531'096LL;
		assert(symVal <= 1'999'999'999'999LL); // 13 digits
	}
	auto txt = ToString(symVal, 13);
	return "01" + txt + GTIN::ComputeCheckDigit(txt);
}

static inline bool Has26to18Ratio(int v26, int v18)
{
	return v26 + 1.5 * v26 / 26 > v18 / 18. * 26. && v26 - 1.5 * v26 / 26 < v18 / 18. * 26.;
}

std::array<int, 89> CheckChars = {
	0b10'10101010'11100010, 0b10'10101010'01110010, 0b10'10101010'00111010, 0b10'10101001'01110010, 0b10'10101001'00111010,
	0b10'10101000'10111010, 0b10'10100101'01110010, 0b10'10100101'00111010, 0b10'10100100'10111010, 0b10'10100010'10111010,
	0b10'10010101'01110010, 0b10'10010101'00111010, 0b10'10010100'10111010, 0b10'10010010'10111010, 0b10'10001010'10111010,
	0b10'01010101'01110010, 0b10'01010101'00111010, 0b10'01010100'10111010, 0b10'01010010'10111010, 0b10'01001010'10111010,
	0b10'00101010'10111010, 0b10'10101011'01100010, 0b10'10101011'00110010, 0b10'10101011'00011010, 0b10'10101001'10110010,
	0b10'10101001'10011010, 0b10'10101000'11011010, 0b10'10100101'10110010, 0b10'10100101'10011010, 0b10'10100100'11011010,
	0b10'10100010'11011010, 0b10'10010101'10110010, 0b10'10010101'10011010, 0b10'10010100'11011010, 0b10'10010010'11011010,
	0b10'10001010'11011010, 0b10'01010101'10110010, 0b10'01010101'10011010, 0b10'01010100'11011010, 0b10'01010010'11011010,
	0b10'01001010'11011010, 0b10'00101010'11011010, 0b10'10101011'10100010, 0b10'10101011'10010010, 0b10'10101001'11010010,
	0b10'10010101'11010010, 0b10'01010101'11010010, 0b10'10101101'01100010, 0b10'10101101'00110010, 0b10'10101101'00011010,
	0b10'10101100'10110010, 0b10'10010110'10110010, 0b10'10010110'10011010, 0b10'10010110'01011010, 0b10'10010011'01011010,
	0b10'10001011'01011010, 0b10'01010110'10110010, 0b10'01010110'10011010, 0b10'01001011'01011010, 0b10'10110101'01100010,
	0b10'10110101'00110010, 0b10'10110101'00011010, 0b10'10110100'10110010, 0b10'10110100'10011010, 0b10'10110010'10110010,
	0b10'01011010'10110010, 0b10'01011010'10011010, 0b10'01011010'01011010, 0b10'01011001'01011010, 0b10'01001101'01011010,
	0b10'00101101'01011010, 0b10'11010101'01100010, 0b10'11010101'00110010, 0b10'11010101'00011010, 0b10'11010100'10110010,
	0b10'11010100'10011010, 0b10'11010100'01011010, 0b10'11010010'10110010, 0b10'11010010'10011010, 0b10'11001010'10110010,
	0b11'01010101'00110010, 0b11'01010101'00011010, 0b11'01010100'10110010, 0b11'01010100'10011010, 0b11'01010100'01011010,
	0b11'01010010'10011010, 0b11'01010010'01011010, 0b11'01001010'10011010, 0b11'01010101'10010010,
};

Barcode DataBarLimitedReader::decodePattern(int rowNumber, PatternView& next, std::unique_ptr<RowReader::DecodingState>&) const
{
	next = next.subView(-2, SYMBOL_LEN);
	while (next.shift(2)) {
		if (!IsGuard(next[27], next[43]))
			continue;
		auto spaceSize = (next[27] + next[43]) / 2;
		if ((!next.isAtFirstBar() && next[-1] < spaceSize) || (!next.isAtLastBar() && next[SYMBOL_LEN] < 4 * spaceSize))
			continue;
		auto [mBar, MBar] = std::minmax({next[0], next[28], next[44]});
		if (MBar > mBar * 4 / 3 + 1)
			continue;

		auto leftView = next.subView(1 + 0 * CHAR_LEN, CHAR_LEN);
		auto checkView = next.subView(1 + 1 * CHAR_LEN, CHAR_LEN);
		auto rightView = next.subView(1 + 2 * CHAR_LEN, CHAR_LEN);
		auto leftWidth = leftView.sum();
		auto checkWith = checkView.sum();
		auto rightWidth = rightView.sum();
		if (!Has26to18Ratio(leftWidth, checkWith) || !Has26to18Ratio(rightWidth, checkWith))
			continue;

		auto modSize = double(leftWidth + checkWith + rightWidth) / (26 + 18 + 26);
		if ((!next.isAtFirstBar() && next[-1] < modSize) || (!next.isAtLastBar() && next[SYMBOL_LEN] < 5 * modSize))
			continue;

		auto checkCharPattern = ToInt(NormalizedPatternFromE2E<CHAR_LEN>(checkView, 18));
		int checkSum = IndexOf(CheckChars, checkCharPattern);
		if (checkSum == -1)
			continue;

		printf("%f - ", modSize);
		printv("%d ", NormalizedPatternFromE2E<CHAR_LEN>(checkView, 18));

		auto left = ReadDataCharacter(leftView);
		auto right = ReadDataCharacter(rightView);

		printf("- %d, %d, %d\n", checkSum, left.value, right.value);

		if ((left.checksum + 20 * right.checksum) % 89 != checkSum)
			continue;

		return {ConstructText(left, right),    rowNumber, next.pixelsInFront(), next.pixelsTillEnd(),
				BarcodeFormat::DataBarLimited, {'e', '0'}};
	}

	// guarantee progress (see loop in ODReader.cpp)
	next = {};

	return {};
}

} // namespace ZXing::OneD
