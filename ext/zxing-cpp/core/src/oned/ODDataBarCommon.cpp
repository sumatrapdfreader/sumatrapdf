/*
* Copyright 2020 Axel Waggershauser
*/
// SPDX-License-Identifier: Apache-2.0

#include "ODDataBarCommon.h"

#include <cmath>

namespace ZXing::OneD::DataBar {

static int combins(int n, int r)
{
	int maxDenom;
	int minDenom;
	if (n - r > r) {
		minDenom = r;
		maxDenom = n - r;
	} else {
		minDenom = n - r;
		maxDenom = r;
	}
	int val = 1;
	int j   = 1;
	for (int i = n; i > maxDenom; i--) {
		val *= i;
		if (j <= minDenom) {
			val /= j;
			j++;
		}
	}
	while (j <= minDenom) {
		val /= j;
		j++;
	}
	return val;
}

#ifdef __cpp_lib_span
int GetValue(const std::span<int> widths, int maxWidth, bool noNarrow)
#else
int GetValue(const Array4I& widths, int maxWidth, bool noNarrow)
#endif
{
	int elements = Size(widths);
	int n = Reduce(widths);
	int val = 0;
	int narrowMask = 0;
	for (int bar = 0; bar < elements - 1; bar++) {
		int elmWidth;
		for (elmWidth = 1, narrowMask |= 1 << bar; elmWidth < widths[bar]; elmWidth++, narrowMask &= ~(1 << bar)) {
			int subVal = combins(n - elmWidth - 1, elements - bar - 2);
			if (noNarrow && (narrowMask == 0) && (n - elmWidth - (elements - bar - 1) >= elements - bar - 1)) {
				subVal -= combins(n - elmWidth - (elements - bar), elements - bar - 2);
			}
			if (elements - bar - 1 > 1) {
				int lessVal = 0;
				for (int mxwElement = n - elmWidth - (elements - bar - 2); mxwElement > maxWidth; mxwElement--) {
					lessVal += combins(n - elmWidth - mxwElement - 1, elements - bar - 3);
				}
				subVal -= lessVal * (elements - 1 - bar);
			} else if (n - elmWidth > maxWidth) {
				subVal--;
			}
			val += subVal;
		}
		n -= elmWidth;
	}
	return val;
}

using Array4F = std::array<float, 4>;

bool ReadDataCharacterRaw(const PatternView& view, int numModules, bool reversed, Array4I& oddPattern,
						  Array4I& evnPattern)
{
#if 1
	auto pattern = NormalizedPatternFromE2E<8>(view, numModules, reversed);
	OddEven<Array4I&> res = {oddPattern, evnPattern};

	for (int i = 0; i < Size(pattern); ++i)
		res[i % 2][i / 2] = pattern[i];

#else
	OddEven<Array4I&> res = {oddPattern, evnPattern};
	OddEven<Array4F> rem;

	float moduleSize = static_cast<float>(view.sum(8)) / numModules;
	auto* iter = view.data() + reversed * 7;
	int inc = reversed ? -1 : 1;

	for (int i = 0; i < 8; ++i, iter += inc) {
		float v = *iter / moduleSize;
		res[i % 2][i / 2] = int(v + .5f);
		rem[i % 2][i / 2] = v - res[i % 2][i / 2];
	}
#endif

	// DataBarExpanded data character is 17 modules wide
	// DataBar outer   data character is 16 modules wide
	// DataBar inner   data character is 15 modules wide

	int minSum = 4; // each data character has 4 bars and 4 spaces
	int maxSum = numModules - minSum;
	int oddSum = Reduce(res.odd);
	int evnSum = Reduce(res.evn);

	int sumErr = oddSum + evnSum - numModules;
	// sum < min -> negative error; sum > max -> positive error
	int oddSumErr = std::min(0, oddSum - (minSum + (numModules == 15))) + std::max(0, oddSum - maxSum);
	int evnSumErr = std::min(0, evnSum - minSum) + std::max(0, evnSum - (maxSum - (numModules == 15)));

	int oddParityErr = (oddSum & 1) == (numModules > 15);
	int evnParityErr = (evnSum & 1) == (numModules < 17);

#if 0
	// the 'signal improving' strategy of trying to fix off-by-one errors in the sum or parity leads to a massively
	// increased likelihood of false positives / misreads especially with expanded codes that are composed of many
	// pairs. the combinatorial explosion of possible pair combinations (see FindValidSequence) results in many possible
	// sequences with valid checksums. It can slightly lower the minimum required resolution to detect something at all
	// but the introduced error rate is clearly not worth it.

	if ((sumErr == 0 && oddParityErr != evnParityErr) || (std::abs(sumErr) == 1 && oddParityErr == evnParityErr) ||
		std::abs(sumErr) > 1 || std::abs(oddSumErr) > 1 || std::abs(evnSumErr) > 1)
		return {};

	if (sumErr == -1) {
		oddParityErr *= -1;
		evnParityErr *= -1;
	} else if (sumErr == 0 && oddParityErr != 0) {
		// both parity errors are 1 -> flip one of them
		(oddSum < evnSum ? oddParityErr : evnParityErr) *= -1;
	}

	// check if parity and sum errors have opposite signs
	if (oddParityErr * oddSumErr < 0 || evnParityErr * evnSumErr < 0)
		return {};

	// apparently the spec calls numbers at even indices 'odd'!?!
	constexpr int odd = 0, evn = 1;
	for (int i : {odd, evn}) {
		int err = i == odd ? (oddSumErr | oddParityErr) : (evnSumErr | evnParityErr);
		int mi = err < 0 ? std::max_element(rem[i].begin(), rem[i].end()) - rem[i].begin()
						 : std::min_element(rem[i].begin(), rem[i].end()) - rem[i].begin();
		res[i][mi] -= err;
	}

	return true;
#else
	// instead, we ignore any character that is not exactly fitting the requirements
	return !(sumErr || oddSumErr || evnSumErr || oddParityErr || evnParityErr);
#endif
}

static bool IsStacked(const Pair& first, const Pair& last)
{
	// check if we see two halfes that are far away from each other in y or overlapping in x
	return std::abs(first.y - last.y) > (first.xStop - first.xStart) || last.xStart < (first.xStart + first.xStop) / 2;
}

Position EstimatePosition(const Pair& first, const Pair& last)
{
	if (!IsStacked(first, last))
		return Line((first.y + last.y) / 2, first.xStart, last.xStop);
	else
		return Position{{first.xStart, first.y}, {first.xStop, first.y}, {last.xStop, last.y}, {last.xStart, last.y}};
}

int EstimateLineCount(const Pair& first, const Pair& last)
{
	// see incrementLineCount() in ODReader.cpp for the -1 here
	return std::min(first.count, last.count) - 1 + IsStacked(first, last);
}

} // namespace ZXing::OneD::DataBar
