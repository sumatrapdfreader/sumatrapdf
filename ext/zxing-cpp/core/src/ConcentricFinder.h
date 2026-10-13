/*
* Copyright 2020 Axel Waggershauser
*/
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "BitMatrixCursor.h"
#include "Pattern.h"
#include "Quadrilateral.h"
#include "ZXAlgorithms.h"

#include <optional>

namespace ZXing {

template <typename T, size_t N>
static float CenterFromEnd(const std::array<T, N>& pattern, float end)
{
	if (N == 5) {
		float a = pattern[4] + pattern[3] + pattern[2] / 2.f;
		float b = pattern[4] + (pattern[3] + pattern[2] + pattern[1]) / 2.f;
		float c = (pattern[4] + pattern[3] + pattern[2] + pattern[1] + pattern[0]) / 2.f;
		return end - (2 * a + b + c) / 4;
	} else if (N == 3) {
		float a = pattern[2] + pattern[1] / 2.f;
		float b = (pattern[2] + pattern[1] + pattern[0]) / 2.f;
		return end - (2 * a + b) / 3;
	} else { // aztec
		auto a = Reduce(pattern.begin() + (N/2 + 1), pattern.end(), pattern[N/2] / 2.f);
		return end - a;
	}
}

template<int N, typename Cursor>
std::optional<Pattern<N>> ReadSymmetricPattern(Cursor& cur, int range)
{
	static_assert(N % 2 == 1);
	assert(range > 0);
	Pattern<N> res = {};
	auto constexpr s_2 = Size(res)/2;
	auto cuo = cur.turnedBack();

	auto next = [&](auto& cur, int i) {
		auto v = cur.stepToEdge(1, range);
		res[s_2 + i] += v;
		if (range)
			range -= v;
		return v;
	};

	for (int i = 0; i <= s_2; ++i) {
		if (!next(cur, i) || !next(cuo, -i))
			return {};
	}
	res[s_2]--; // the starting pixel has been counted twice, fix this

	return res;
}

template<bool RELAXED_THRESHOLD = false, typename PATTERN>
int CheckSymmetricPattern(BitMatrixCursorI& cur, PATTERN pattern, int range, bool updatePosition)
{
	FastEdgeToEdgeCounter curFwd(cur), curBwd(cur.turnedBack());

	int centerFwd = curFwd.stepToNextEdge(range);
	if (!centerFwd)
		return 0;
	int centerBwd = curBwd.stepToNextEdge(range);
	if (!centerBwd)
		return 0;

	assert(range > 0);
	Pattern<pattern.size()> res = {};
	auto constexpr s_2 = Size(res)/2;
	res[s_2] = centerFwd + centerBwd - 1; // -1 because the starting pixel is counted twice
	range -= res[s_2];

	auto next = [&](auto& cur, int i) {
		auto v = cur.stepToNextEdge(range);
		res[s_2 + i] = v;
		range -= v;
		return v;
	};

	for (int i = 1; i <= s_2; ++i) {
		if (!next(curFwd, i) || !next(curBwd, -i))
			return 0;
	}

	if (!IsPattern<RELAXED_THRESHOLD>(res, pattern))
		return 0;

	if (updatePosition)
		cur.step(res[s_2] / 2 - (centerBwd - 1));

	return Reduce(res);
}

std::optional<PointF> CenterOfRing(const BitMatrix& image, PointI center, int range, int nth, bool requireCircle = true);

std::optional<PointF> FinetuneConcentricPatternCenter(const BitMatrix& image, PointF center, int range, int finderPatternSize);

std::optional<QuadrilateralF> FindConcentricPatternCorners(const BitMatrix& image, PointF center, int range, int ringIndex);

struct ConcentricPattern : public PointF
{
	int size = 0;
};

template <bool E2E = false, typename PATTERN>
std::optional<ConcentricPattern> LocateConcentricPattern(const BitMatrix& image, PATTERN pattern, PointF center, int range)
{
	auto cur = BitMatrixCursor(image, PointI(center), {});
	int minSpread = image.width(), maxSpread = 0;
	// TODO: setting maxError to 1 can subtantially help with detecting symbols with low print quality resulting in damaged
	// finder patterns, but it sutantially increases the runtime (approx. 20% slower for the falsepositive images).
	int maxError = 0;
	for (auto d : {PointI{0, 1}, {1, 0}}) {
		int spread = CheckSymmetricPattern<E2E>(cur.setDirection(d), pattern, range, true);
		if (spread)
			UpdateMinMax(minSpread, maxSpread, spread);
		else if (--maxError < 0)
			return {};
	}

#if 1
	for (auto d : {PointI{1, 1}, {1, -1}}) {
		int spread = CheckSymmetricPattern<true>(cur.setDirection(d), pattern, range * 2, false);
		if (spread)
			UpdateMinMax(minSpread, maxSpread, spread);
		else if (--maxError < 0)
			return {};
	}
#endif

	if (maxSpread > 5 * minSpread)
		return {};

	auto newCenter = FinetuneConcentricPatternCenter(image, PointF(cur.p), range, pattern.size());
	if (!newCenter)
		return {};

	return ConcentricPattern{*newCenter, (maxSpread + minSpread) / 2};
}

} // ZXing

