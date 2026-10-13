/*
* Copyright 2016 Nu-book Inc.
* Copyright 2016 ZXing authors
* Copyright 2020 Axel Waggershauser
*/
// SPDX-License-Identifier: Apache-2.0

#include "ODDataBarReader.h"

#include "BarcodeFormat.h"
#include "DecoderResult.h"
#include "DetectorResult.h"
#include "GTIN.h"
#include "ODDataBarCommon.h"
#include "Barcode.h"

#include <cmath>
#include <unordered_set>

namespace ZXing::OneD {

using namespace DataBar;

static bool IsCharacterPair(PatternView v, int modsLeft, int modsRight)
{
	float modSizeRef = ModSizeFinder(v);
	return IsCharacter(LeftChar(v), modsLeft, modSizeRef) && IsCharacter(RightChar(v), modsRight, modSizeRef);
}

static bool IsLeftPair(const PatternView& v)
{
	return IsFinder(v[8], v[9], v[10], v[11], v[12]) && IsGuard(v[-1], v[11]) && IsCharacterPair(v, 16, 15);
}

static bool IsRightPair(const PatternView& v)
{
	return IsFinder(v[12], v[11], v[10], v[9], v[8]) && IsGuard(v[9], v[21]) && IsCharacterPair(v, 15, 16);
}

static Character ReadDataCharacter(const PatternView& view, bool outsideChar, bool rightPair)
{
	constexpr int OUTSIDE_EVEN_TOTAL_SUBSET[] = {1, 10, 34, 70, 126};
	constexpr int INSIDE_ODD_TOTAL_SUBSET[]   = {4, 20, 48, 81};
	constexpr int OUTSIDE_GSUM[]              = {0, 161, 961, 2015, 2715};
	constexpr int INSIDE_GSUM[]               = {0, 336, 1036, 1516};
	constexpr int OUTSIDE_ODD_WIDEST[]        = {8, 6, 4, 3, 1};
	constexpr int INSIDE_ODD_WIDEST[]         = {2, 4, 6, 8};

	Array4I oddPattern = {}, evnPattern = {};
	if (!ReadDataCharacterRaw(view, outsideChar ? 16 : 15, outsideChar == rightPair, oddPattern, evnPattern))
		return {};

	auto calcChecksumPortion = [](const Array4I& counts) {
		int res = 0;
		for (auto it = counts.rbegin(); it != counts.rend(); ++it)
			res = 9 * res + *it;
		return res;
	};

	int checksumPortion = calcChecksumPortion(oddPattern) + 3 * calcChecksumPortion(evnPattern);

	if (outsideChar) {
		int oddSum = Reduce(oddPattern);
		assert((oddSum & 1) == 0 && oddSum <= 12 && oddSum >= 4); // checked in ReadDataCharacterRaw
		int group = (12 - oddSum) / 2;
		int oddWidest = OUTSIDE_ODD_WIDEST[group];
		int evnWidest = 9 - oddWidest;
		int vOdd = GetValue(oddPattern, oddWidest, false);
		int vEvn = GetValue(evnPattern, evnWidest, true);
		int tEvn = OUTSIDE_EVEN_TOTAL_SUBSET[group];
		int gSum = OUTSIDE_GSUM[group];
		return {vOdd * tEvn + vEvn + gSum, checksumPortion};
	} else {
		int evnSum = Reduce(evnPattern);
		assert((evnSum & 1) == 0 && evnSum <= 12 && evnSum >= 4); // checked in ReadDataCharacterRaw
		int group = (10 - evnSum) / 2;
		int oddWidest = INSIDE_ODD_WIDEST[group];
		int evnWidest = 9 - oddWidest;
		int vOdd = GetValue(oddPattern, oddWidest, true);
		int vEvn = GetValue(evnPattern, evnWidest, false);
		int tOdd = INSIDE_ODD_TOTAL_SUBSET[group];
		int gSum = INSIDE_GSUM[group];
		return {vEvn * tOdd + vOdd + gSum, checksumPortion};
	}
}

int ParseFinderPattern(const PatternView& view, bool reversed)
{
	static constexpr std::array<std::array<int, 3>, 9> e2ePatterns = {{
		{11, 10, 3 }, // {3, 8, 2, 1, 1}
		{8 , 10, 6 }, // {3, 5, 5, 1, 1}
		{6 , 10, 8 }, // {3, 3, 7, 1, 1}
		{4 , 10, 10}, // {3, 1, 9, 1, 1}
		{9 , 11, 5 }, // {2, 7, 4, 1, 1}
		{7 , 11, 7 }, // {2, 5, 6, 1, 1}
		{5 , 11, 9 }, // {2, 3, 8, 1, 1}
		{6 , 11, 8 }, // {1, 5, 7, 1, 1}
		{4 , 12, 10}, // {1, 3, 9, 1, 1}
	}};

	return ParseFinderPattern<9>(view, reversed, e2ePatterns);
}

static Pair ReadPair(const PatternView& view, bool rightPair)
{
	if (int pattern = ParseFinderPattern(Finder(view), rightPair))
		if (auto outside = ReadDataCharacter(rightPair ? RightChar(view) : LeftChar(view), true, rightPair))
			if (auto inside = ReadDataCharacter(rightPair ? LeftChar(view) : RightChar(view), false, rightPair)) {
				// include left and right guards
				int xStart = view.pixelsInFront() - view[-1];
				int xStop  = view.pixelsTillEnd() + 2 * view[FULL_PAIR_SIZE];
				return {outside, inside, pattern, xStart, xStop};
			}

	return {};
}

static long long Value(Pair leftPair, Pair rightPair)
{
	auto value = [](Pair p) { return 1597 * p.left.value + p.right.value; };
	auto res = 4537077LL * value(leftPair) + value(rightPair);
	if (res >= 10000000000000LL) { // Strip 2D linkage flag (GS1 Composite) if any (ISO/IEC 24724:2011 Section 5.2.3)
		res -= 10000000000000LL;
	}
	return res;
}

static bool ChecksumIsValid(Pair leftPair, Pair rightPair)
{
	auto checksum = [](Pair p) { return p.left.checksum + 4 * p.right.checksum; };
	int a = (checksum(leftPair) + 16 * checksum(rightPair)) % 79;
	int b = 9 * (std::abs(leftPair.finder) - 1) + (std::abs(rightPair.finder) - 1);
	if (b > 72)
		b--;
	if (b > 8)
		b--;
	return a == b && Value(leftPair, rightPair) <= 9999999999999LL; // 13 digits
}

static std::string ConstructText(Pair leftPair, Pair rightPair)
{
	auto txt = ToString(Value(leftPair, rightPair), 13);
	return txt + GTIN::ComputeCheckDigit(txt);
}

struct State : public RowReader::DecodingState
{
	std::unordered_set<Pair, PairHash> leftPairs;
	std::unordered_set<Pair, PairHash> rightPairs;
};

Barcode DataBarReader::decodePattern(int rowNumber, PatternView& next, std::unique_ptr<RowReader::DecodingState>& state) const
{
#if 0 // non-stacked version
	next = next.subView(-1, FULL_PAIR_SIZE + 1); // +1 reflects the guard pattern on the right, see IsRightPair());
	// yes: the first view we test is at index 1 (black bar at 0 would be the guard pattern)
	while (next.shift(2)) {
		if (IsLeftPair(next)) {
			if (auto leftPair = ReadPair(next, false); leftPair && next.shift(FULL_PAIR_SIZE) && IsRightPair(next)) {
				if (auto rightPair = ReadPair(next, true); rightPair && ChecksumIsValid(leftPair, rightPair)) {
					return {ConstructText(leftPair, rightPair), rowNumber, leftPair.xStart, rightPair.xStop,
							BarcodeFormat::DataBar};
				}
			}
		}
	}
#else
	if (!state)
		state.reset(new State);
	auto* prevState = static_cast<State*>(state.get());

	next = next.subView(0, FULL_PAIR_SIZE + 1); // +1 reflects the guard pattern on the right, see IsRightPair()
	// yes: the first view we test is at index 1 (black bar at 0 would be the guard pattern)
	while (next.shift(1)) {
		if (IsLeftPair(next)) {
			if (auto leftPair = ReadPair(next, false)) {
				leftPair.y = rowNumber;
				prevState->leftPairs.insert(leftPair);
				next.shift(FULL_PAIR_SIZE - 1);
			}
		}

		if (next.shift(1) && IsRightPair(next)) {
			if (auto rightPair = ReadPair(next, true)) {
				rightPair.y = rowNumber;
				prevState->rightPairs.insert(rightPair);
				next.shift(FULL_PAIR_SIZE + 2);
			}
		}
	}

	for (const auto& leftPair : prevState->leftPairs)
		for (const auto& rightPair : prevState->rightPairs)
			if (ChecksumIsValid(leftPair, rightPair)) {
				// Symbology identifier ISO/IEC 24724:2011 Section 9 and GS1 General Specifications 5.1.3 Figure 5.1.3-2
				Barcode res{DecoderResult(Content(ByteArray(ConstructText(leftPair, rightPair)), {'e', '0'}))
								.setLineCount(EstimateLineCount(leftPair, rightPair)),
							{{}, EstimatePosition(leftPair, rightPair)},
							BarcodeFormat::DataBar};

				prevState->leftPairs.erase(leftPair);
				prevState->rightPairs.erase(rightPair);
				return res;
			}
#endif

	// guarantee progress (see loop in ODReader.cpp)
	next = {};

	return {};
}

} // namespace ZXing::OneD
