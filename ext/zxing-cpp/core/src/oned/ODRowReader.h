/*
* Copyright 2016 Nu-book Inc.
* Copyright 2016 ZXing authors
* Copyright 2020 Axel Waggershauser
*/
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "BitArray.h"
#include "Pattern.h"
#include "Barcode.h"
#include "ZXAlgorithms.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <iterator>
#include <limits>
#include <memory>

/*
Code39 : 1:2/3, 5+4+1 (0x3|2x1 wide) -> 12-15 mods, v1-? | ToNarrowWide(OMG 1) == *
Codabar: 1:2/3, 4+3+1 (1x1|1x2|3x0 wide) -> 9-13 mods, v1-? | ToNarrowWide(OMG 2) == ABCD
ITF    : 1:2/3, 5+5   (2x2 wide) -> mods, v6-?| .5, .38 == * | qz:10

Code93 : 1-4, 3+3 -> 9 mods  v1-? | round to 1-4 == *
Code128: 1-4, 3+3 -> 11 mods v1-? | .7, .25 == ABC | qz:10
UPC/EAN: 1-4, 2+2 -> 7 mods  f    | .7, .48 == *
  UPC-A: 11d 95m = 3 + 6*4 + 5 + 6*4 + 3 = 59 | qz:3
  EAN-13: 12d 95m
  UPC-E: 6d, 3 + 6*4 + 6 = 33
  EAN-8: 8d, 3 + 4*4 + 5 + 4*4 + 3 = 43

RSS14  : 1-8, finder: (15,2+3), symbol: (15/16,4+4) | .45, .2 (finder only), 14d
  code = 2xguard + 2xfinder + 4xsymbol = (96,23), stacked = 2x50 mods
RSSExp.:  v?-74d/?-41c
*/

namespace ZXing {

class ReaderOptions;

namespace OneD {

/**
* Encapsulates functionality and implementation that is common to all families
* of one-dimensional barcodes.
*/
class RowReader
{
protected:
	const ReaderOptions& _opts;

public:
	explicit RowReader(const ReaderOptions& opts) : _opts(opts) {}
	explicit RowReader(ReaderOptions&&) = delete;

	struct DecodingState
	{
		virtual ~DecodingState() = default;
	};

	virtual ~RowReader() {}

	virtual Barcode decodePattern(int rowNumber, PatternView& next, std::unique_ptr<DecodingState>& state) const = 0;

	/**
	 * Determines how closely a set of observed counts of runs of black/white values matches a given
	 * target pattern. This is reported as the ratio of the total variance from the expected pattern
	 * proportions across all pattern elements, to the length of the pattern.
	 *
	 * @param counters observed counters
	 * @param pattern expected pattern
	 * @param maxIndividualVariance The most any counter can differ before we give up
	 * @return ratio of total variance between counters and pattern compared to total pattern size
	 */
	template <typename CP, typename PP>
	static float PatternMatchVariance(const CP* counters, const PP* pattern, size_t length, float maxIndividualVariance)
	{
		int total = Reduce(counters, counters + length, 0);
		int patternLength = Reduce(pattern, pattern + length, 0);
		if (total < patternLength) {
			// If we don't even have one pixel per unit of bar width, assume this is too small
			// to reliably match, so fail:
			return std::numeric_limits<float>::max();
		}

		float unitBarWidth = (float)total / patternLength;
		maxIndividualVariance *= unitBarWidth;

		float totalVariance = 0.0f;
		for (size_t x = 0; x < length; ++x) {
			float variance = std::abs(counters[x] - pattern[x] * unitBarWidth);
			if (variance > maxIndividualVariance) {
				return std::numeric_limits<float>::max();
			}
			totalVariance += variance;
		}
		return totalVariance / total;
	}

	template <typename Counters, typename Pattern>
	static float PatternMatchVariance(const Counters& counters, const Pattern& pattern, float maxIndividualVariance) {
		assert(Size(counters) == Size(pattern));
		return PatternMatchVariance(std::data(counters), std::data(pattern), std::size(counters), maxIndividualVariance);
	}

	/**
	* Attempts to decode a sequence of black/white lines into single
	* digit.
	*
	* @param counters the counts of runs of observed black/white/black/... values
	* @param patterns the list of patterns to compare the contents of counters to
	* @param requireUnambiguousMatch the 'best match' must be better than all other matches
	* @return The decoded digit index, -1 if no pattern matched
	*/
	template <typename Counters, typename Patterns>
	static int DecodeDigit(const Counters& counters, const Patterns& patterns, float maxAvgVariance,
						   float maxIndividualVariance, bool requireUnambiguousMatch = true)
	{
		float bestVariance = maxAvgVariance; // worst variance we'll accept
		constexpr int INVALID_MATCH = -1;
		int bestMatch = INVALID_MATCH;
		for (int i = 0; i < Size(patterns); i++) {
			float variance = PatternMatchVariance(counters, patterns[i], maxIndividualVariance);
			if (variance < bestVariance) {
				bestVariance = variance;
				bestMatch = i;
			} else if (requireUnambiguousMatch && variance == bestVariance) {
				// if we find a second 'best match' with the same variance, we can not reliably report to have a suitable match
				bestMatch = INVALID_MATCH;
			}
		}
		return bestMatch;
	}

	/**
	 * @brief NarrowWideThreshold calculates width thresholds to separate narrow and wide bars and spaces.
	 *
	 * This is useful for codes like Codabar, Code39 and ITF which distinguish between narrow and wide
	 * bars/spaces. Where wide ones are between 2 and 3 times as wide as the narrow ones.
	 *
	 * @param view containing one character
	 * @return threshold value for bars and spaces
	 */
	static BarAndSpaceI NarrowWideThreshold(const PatternView& view)
	{
		BarAndSpaceI m = {view[0], view[1]};
		BarAndSpaceI M = m;
		for (int i = 2; i < view.size(); ++i)
			UpdateMinMax(m[i], M[i], view[i]);

		BarAndSpaceI res;
		for (int i = 0; i < 2; ++i) {
			// check that
			//  a) wide <= 4 * narrow
			//  b) bars and spaces are not more than a factor of 2 (or 3 for the max) apart from each other
			if (M[i] > 4 * (m[i] + 1) || M[i] > 3 * M[i + 1] || m[i] > 2 * (m[i + 1] + 1))
				return {};
			// the threshold is the average of min and max but at least 1.5 * min
			res[i] = std::max((m[i] + M[i]) / 2, m[i] * 3 / 2);
		}

		return res;
	}

	/**
	 * @brief ToNarrowWidePattern takes a PatternView, calculates a NarrowWideThreshold and returns int where a '0' bit
	 * means narrow and a '1' bit means 'wide'.
	 */
	static int NarrowWideBitPattern(const PatternView& view)
	{
		const auto threshold = NarrowWideThreshold(view);
		if (!threshold.isValid())
			return -1;

		int pattern = 0;
		for (int i = 0; i < view.size(); ++i) {
			if (view[i] > threshold[i] * 2)
				return -1;
			AppendBit(pattern, view[i] > threshold[i]);
		}

		return pattern;
	}

	/**
	 * @brief each bar/space is 1-4 modules wide, we have N bars/spaces, they are SUM modules wide in total
	 */
	template <int LEN, int SUM>
	static int OneToFourBitPattern(const PatternView& view)
	{
		// TODO: make sure none of the elements in the normalized pattern exceeds 4
		return ToInt(NormalizedPattern<LEN, SUM>(view));
	}

	/**
	 * @brief Lookup the pattern in the table and return the character in alphabet at the same index.
	 * @returns 0 if pattern is not found. Used to be -1 but that fails on systems where char is unsigned.
	 */
	template<typename INDEX, typename ALPHABET>
	static char LookupBitPattern(int pattern, const INDEX& table, const ALPHABET& alphabet)
	{
		int i = IndexOf(table, pattern);
		return i == -1 ? 0 : alphabet[i];
	}

	template<typename INDEX, typename ALPHABET>
	static char DecodeNarrowWidePattern(const PatternView& view, const INDEX& table, const ALPHABET& alphabet)
	{
		return LookupBitPattern(NarrowWideBitPattern(view), table, alphabet);
	}
};

template<typename Range>
Barcode DecodeSingleRow(const RowReader& reader, const Range& range)
{
	PatternRow row;
	GetPatternRow(range, row);
	PatternView view(row);

	std::unique_ptr<RowReader::DecodingState> state;
	return reader.decodePattern(0, view, state);
}

} // OneD
} // ZXing
