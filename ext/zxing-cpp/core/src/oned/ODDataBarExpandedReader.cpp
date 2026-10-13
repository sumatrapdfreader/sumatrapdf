/*
* Copyright 2016 Nu-book Inc.
* Copyright 2016 ZXing authors
* Copyright 2020 Axel Waggershauser
*/
// SPDX-License-Identifier: Apache-2.0

#include "ODDataBarExpandedReader.h"

#include "BarcodeFormat.h"
#include "DecoderResult.h"
#include "DetectorResult.h"
#include "ODDataBarCommon.h"
#include "ODDataBarExpandedBitDecoder.h"
#include "Barcode.h"

#include <cmath>
#include <map>
#include <vector>

namespace ZXing::OneD {

using namespace DataBar;

static bool IsFinderPattern(int a, int b, int c, int d, int e)
{
	return IsFinder(a, b, c, d, e) && (c > 3 * e);
};

static bool IsCharacterPair(const PatternView& v)
{
	float modSizeRef = ModSizeFinder(v);
	return IsCharacter(LeftChar(v), 17, modSizeRef) &&
		   (v.size() == HALF_PAIR_SIZE || IsCharacter(RightChar(v), 17, modSizeRef));
}

static bool IsL2RPair(const PatternView& v)
{
	return IsFinderPattern(v[8], v[9], v[10], v[11], v[12]) && IsCharacterPair(v);
}

static bool IsR2LPair(const PatternView& v)
{
	return IsFinderPattern(v[12], v[11], v[10], v[9], v[8]) && IsCharacterPair(v);
}

static Character ReadDataCharacter(const PatternView& view, int finder, bool reversed)
{
	constexpr int SYMBOL_WIDEST[]     = {7, 5, 4, 3, 1};
	constexpr int EVEN_TOTAL_SUBSET[] = {4, 20, 52, 104, 204};
	constexpr int GSUM[]              = {0, 348, 1388, 2948, 3988};

	Array4I oddCounts = {}, evnCounts = {};
	if (!ReadDataCharacterRaw(view, 17, reversed, oddCounts, evnCounts))
		return {};

	int weightRow = 4 * (std::abs(finder) - 1) + (finder < 0) * 2 + reversed;
	auto calcChecksum = [weightRow](const Array4I& counts, bool even) {
		static constexpr std::array<std::array<int, 8>, 24> WEIGHTS = {{
			{0, 0, 0, 0, 0, 0, 0, 0}, // the check character itself
			{1, 3, 9, 27, 81, 32, 96, 77},
			{20, 60, 180, 118, 143, 7, 21, 63},
			{189, 145, 13, 39, 117, 140, 209, 205},
			{193, 157, 49, 147, 19, 57, 171, 91},
			{62, 186, 136, 197, 169, 85, 44, 132},
			{185, 133, 188, 142, 4, 12, 36, 108},
			{113, 128, 173, 97, 80, 29, 87, 50},
			{150, 28, 84, 41, 123, 158, 52, 156},
			{46, 138, 203, 187, 139, 206, 196, 166},
			{76, 17, 51, 153, 37, 111, 122, 155},
			{43, 129, 176, 106, 107, 110, 119, 146},
			{16, 48, 144, 10, 30, 90, 59, 177},
			{109, 116, 137, 200, 178, 112, 125, 164},
			{70, 210, 208, 202, 184, 130, 179, 115},
			{134, 191, 151, 31, 93, 68, 204, 190},
			{148, 22, 66, 198, 172, 94, 71, 2},
			{6, 18, 54, 162, 64, 192, 154, 40},
			{120, 149, 25, 75, 14, 42, 126, 167},
			{79, 26, 78, 23, 69, 207, 199, 175},
			{103, 98, 83, 38, 114, 131, 182, 124},
			{161, 61, 183, 127, 170, 88, 53, 159},
			{55, 165, 73, 8, 24, 72, 5, 15},
			{45, 135, 194, 160, 58, 174, 100, 89},
		}};

		const int* weights = &WEIGHTS[weightRow][even];
		return TransformReduce(counts, 0, [i = 0, weights](int c) mutable { return c * weights[2 * i++]; });
	};

	int checksum = calcChecksum(oddCounts, false) + calcChecksum(evnCounts, true);

	int oddSum = Reduce(oddCounts);
	assert ((oddSum & 1) == 0 && oddSum <= 13 && oddSum >= 4);
	int group = (13 - oddSum) / 2;
	int oddWidest = SYMBOL_WIDEST[group];
	int evnWidest = 9 - oddWidest;
	int vOdd = GetValue(oddCounts, oddWidest, true);
	int vEvn = GetValue(evnCounts, evnWidest, false);
	int tEvn = EVEN_TOTAL_SUBSET[group];
	int gSum = GSUM[group];
	int value = vOdd * tEvn + vEvn + gSum;

	return {value, checksum};
}

using Pairs      = std::vector<Pair>;
using Characters = std::vector<Character>;

enum Direction
{
	Right = 1,
	Left  = -1,
};

constexpr int FINDER_A = 1;
constexpr int FINDER_B = 2;
constexpr int FINDER_C = 3;
constexpr int FINDER_D = 4;
constexpr int FINDER_E = 5;
constexpr int FINDER_F = 6;

// A negative number means the finder pattern is laid out right2left. Note: each finder may only occur once per code.
static const std::array<std::vector<int>, 10> FINDER_PATTERN_SEQUENCES = {{
	{FINDER_A, -FINDER_A},
	{FINDER_A, -FINDER_B, FINDER_B},
	{FINDER_A, -FINDER_C, FINDER_B, -FINDER_D},
	{FINDER_A, -FINDER_E, FINDER_B, -FINDER_D, FINDER_C},
	{FINDER_A, -FINDER_E, FINDER_B, -FINDER_D, FINDER_D, -FINDER_F},
	{FINDER_A, -FINDER_E, FINDER_B, -FINDER_D, FINDER_E, -FINDER_F, FINDER_F},
	{FINDER_A, -FINDER_A, FINDER_B, -FINDER_B, FINDER_C, -FINDER_C, FINDER_D, -FINDER_D},
	{FINDER_A, -FINDER_A, FINDER_B, -FINDER_B, FINDER_C, -FINDER_C, FINDER_D, -FINDER_E, FINDER_E},
	{FINDER_A, -FINDER_A, FINDER_B, -FINDER_B, FINDER_C, -FINDER_C, FINDER_D, -FINDER_E, FINDER_F, -FINDER_F},
	{FINDER_A, -FINDER_A, FINDER_B, -FINDER_B, FINDER_C, -FINDER_D, FINDER_D, -FINDER_E, FINDER_E, -FINDER_F, FINDER_F},
}};

static const std::array<int, 7> VALID_HALF_PAIRS = {{-FINDER_A, FINDER_B, -FINDER_D, FINDER_C, -FINDER_F, FINDER_F, FINDER_E}};

static int ParseFinderPattern(const PatternView& view, Direction dir)
{
	static constexpr std::array<std::array<int, 3>, 6> e2ePatterns = {{
		{9, 12, 5 }, // {1, 8, 4, 1, 1}, // A
		{9, 10, 5 }, // {3, 6, 4, 1, 1}, // B
		{7, 10, 7 }, // {3, 4, 6, 1, 1}, // C
		{5, 10, 9 }, // {3, 2, 8, 1, 1}, // D
		{8, 11, 6 }, // {2, 6, 5, 1, 1}, // E
		{4, 11, 10}, // {2, 2, 9, 1, 1}, // F
	}};

	return ParseFinderPattern<6>(view, dir == Direction::Left, e2ePatterns);
}

static bool ChecksumIsValid(const Pairs& pairs)
{
	auto checksum = TransformReduce(pairs, 0, [](auto p) { return p.left.checksum + p.right.checksum; }) % 211 +
					211 * (2 * Size(pairs) - 4 - !pairs.back().right);
	return pairs.front().left.value == checksum;
}

// calculate the index (length of the sequence - 2) of the only valid sequence for the given FINDER_A, based on the
// checksum value stored in the first pair's left value. see also ChecksumIsValid().
static int SequenceIndex(Character first)
{
	return (first.value / 211 + 4 + 1) / 2 - 2;
}

static bool ChecksumIsValid(Character first)
{
	int i = SequenceIndex(first);
	return 0 <= i && i < Size(FINDER_PATTERN_SEQUENCES);
}

static Pair ReadPair(const PatternView& view, Direction dir)
{
	if (int finder = ParseFinderPattern(Finder(view), dir))
		if (auto charL = ReadDataCharacter(LeftChar(view), finder, false))
			if (finder != FINDER_A || ChecksumIsValid(charL)) {
				auto charR = RightChar(view).isValid() && IsCharacter(RightChar(view), 17, ModSizeFinder(view))
								 ? ReadDataCharacter(RightChar(view), finder, true)
								 : Character();
				if (charR || Contains(VALID_HALF_PAIRS, finder))
					return {charL, charR, finder, view.pixelsInFront(),
							(charR ? RightChar(view) : Finder(view)).pixelsTillEnd()};
			}

	return {};
}

template<bool STACKED>
static Pairs ReadRowOfPairs(PatternView& next, int rowNumber)
{
	Pairs pairs;
	Pair pair;

	if constexpr (STACKED) {
		// a possible first pair is either left2right starting on a space or right2left starting on a bar.
		// it might be a half-pair
		next = next.subView(0, HALF_PAIR_SIZE);
		while (next.shift(1)) {
			if (IsL2RPair(next) && (pair = ReadPair(next, Direction::Right)) &&
				(pair.finder != FINDER_A || IsGuard(next[-1], next[11])))
				break;
			if (next.shift(1) && IsR2LPair(next) && (pair = ReadPair(next, Direction::Left)))
				break;
		}
	} else {
		// the only possible first pair is a full, left2right FINDER_A pair starting on a space
		// with a guard bar on the left
		next = next.subView(-1, FULL_PAIR_SIZE);
		while (next.shift(2)) {
			if (IsL2RPair(next) && IsGuard(next[-1], next[11]) &&
				(pair = ReadPair(next, Direction::Right)).finder == FINDER_A)
				break;
		}
		// after the first full pair, the symbol may end anytime with a half pair
		next = next.subView(0, HALF_PAIR_SIZE);
	}

	if (!pair) {
		next = {}; // if we didn't find a single pair, consume the rest of the row
		return {};
	}

	auto flippedDir = [](Pair p) { return p.finder < 0 ? Direction::Right : Direction::Left; };
	auto isValidPair = [](Pair p, PatternView v) { return p.right || IsGuard(v[p.finder < 0 ? 9 : 11], v[13]); };

	do {
		pair.y = rowNumber;
		pairs.push_back(pair);
	} while (pair.right && next.shift(FULL_PAIR_SIZE) && (pair = ReadPair(next, flippedDir(pair))) &&
			 isValidPair(pair, next));

	return pairs;
}

using PairMap = std::map<int, Pairs>;

// inserts all pairs inside row into the PairMap or increases their count respectively.
static bool Insert(PairMap& all, Pairs&& row)
{
	bool res = false;
	for (const Pair& pair : row) {
		auto& pairs = all[pair.finder];
		if (auto i = Find(pairs, pair); i != pairs.end()) {
			i->count++;
			// bubble sort the pairs with the highest view count to the front so we test them first in FindValidSequence
			while (i != pairs.begin() && i[0].count > i[-1].count) {
				std::swap(i[-1], i[0]);
				--i;
			}
		} else
			pairs.push_back(pair);
		res = true;
	}
	return res;
}

template <typename ITER>
static bool FindValidSequence(const PairMap& all, ITER begin, ITER end, Pairs& stack)
{
	if (begin == end)
		return ChecksumIsValid(stack);

	if (auto ppairs = all.find(*begin); ppairs != all.end()) {
		// only try the N most common pairs, this means the absolute maximum number of ChecksumIsValid() evaluations
		// is N^11 (11 is the maximum sequence length).
		constexpr int N = 2;
		// TODO c++20 ranges::views::take()
		auto& pairs = ppairs->second;
		int n = 0;
		for (auto p = pairs.begin(), pend = pairs.end(); p != pend && n < N; ++p, ++n) {
			// skip p if it is a half-pair but not the last one in the sequence
			if (!p->right && std::next(begin) != end)
				continue;
			// to lower the chance of a misread, one can require each pair to have been seen at least N times.
			// e.g: if (p.count < 2) break;
			stack.push_back(*p);
			if (FindValidSequence(all, std::next(begin), end, stack))
				return true;
			stack.pop_back();
		}
	}

	return false;
}

static Pairs FindValidSequence(PairMap& all)
{
	Pairs stack;
	for (const auto& first : all[FINDER_A]) {
		int sequenceIndex = SequenceIndex(first.left);
		// if we have not seen enough pairs to possibly complete the sequence, wait for more
		if (Size(all) < sequenceIndex + 2)
			continue;
		auto& sequence = FINDER_PATTERN_SEQUENCES[sequenceIndex];
		stack.push_back(first);
		// recursively fill the stack with pairs according to the valid finder sequence
		if (FindValidSequence(all, std::next(std::begin(sequence)), std::end(sequence), stack))
			break;
		stack.pop_back();
	}
	return stack;
}

static void RemovePairs(PairMap& all, const Pairs& pairs)
{
	for(const auto& p : pairs)
		if (auto i = Find(all[p.finder], p); i != all[p.finder].end())
			if (--i->count == 0)
				all[p.finder].erase(i);
}

static BitArray BuildBitArray(const Pairs& pairs)
{
	BitArray res;

	res.appendBits(pairs.front().right.value, 12);
	for (auto p = ++pairs.begin(); p != pairs.end(); ++p) {
		res.appendBits(p->left.value, 12);
		if (p->right)
			res.appendBits(p->right.value, 12);
	}

	return res;
}

struct DBERState : public RowReader::DecodingState
{
	PairMap allPairs;
};

Barcode DataBarExpandedReader::decodePattern(int rowNumber, PatternView& view, std::unique_ptr<RowReader::DecodingState>& state) const
{
#if 0 // non-stacked version
	auto pairs = ReadRowOfPairs<false>(view, rowNumber);

	if (pairs.empty() || !ChecksumIsValid(pairs))
		return {};
#else
	if (!state)
		state.reset(new DBERState);
	auto& allPairs = static_cast<DBERState*>(state.get())->allPairs;

	// Stacked codes can be laid out in a number of ways. The following rules apply:
	//  * the first row starts with FINDER_A in left-to-right (l2r) layout
	//  * pairs in l2r layout start with a space, r2l ones with a bar
	//  * l2r and r2l finders always alternate
	//  * rows may contain any number of pairs
	//  * even rows may be reversed
	//  * a l2r pair that starts with a bar is actually a r2l pair on a reversed line
	//  * the last pair of the symbol may be missing the right character
	//
	// 3 examples: (r == l2r, l == r2l, R/L == r/l but reversed)
	//    r l r l    |    r l     |     r l r
	//    L R L R    |    r       |     l

	if (!Insert(allPairs, ReadRowOfPairs<true>(view, rowNumber)))
		return {};

	auto pairs = FindValidSequence(allPairs);
	if (pairs.empty())
		return {};
#endif

	auto txt = DecodeExpandedBits(BuildBitArray(pairs));
	if (txt.empty())
		return {};

	RemovePairs(allPairs, pairs);

	// TODO: EstimatePosition misses part of the symbol in the stacked case where the last row contains less pairs than
	// the first
	// Symbology identifier: ISO/IEC 24724:2011 Section 9 and GS1 General Specifications 5.1.3 Figure 5.1.3-2
	return {DecoderResult(Content(ByteArray(txt), {'e', '0', 0, AIFlag::GS1}))
				.setLineCount(EstimateLineCount(pairs.front(), pairs.back())),
			{{}, EstimatePosition(pairs.front(), pairs.back())}, BarcodeFormat::DataBarExpanded};
}

} // namespace ZXing::OneD
