/*
* Copyright 2020 Axel Waggershauser
*/
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "BitHacks.h"
#include "Range.h"
#include "ZXAlgorithms.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

namespace ZXing {

using PatternType = uint16_t;
template<int N> using Pattern = std::array<PatternType, N>;
using PatternRow = std::vector<PatternType>;

class PatternView
{
	using Iterator = PatternRow::const_pointer;
	Iterator _data = nullptr;
	int _size = 0;
	Iterator _base = nullptr;
	Iterator _end = nullptr;

public:
	using value_type = PatternRow::value_type;

	PatternView() = default;

	// A PatternRow always starts with the width of whitespace in front of the first black bar.
	// The first element of the PatternView is the first bar.
	PatternView(const PatternRow& bars)
		: _data(bars.data() + 1), _size(Size(bars) - 1), _base(bars.data()), _end(bars.data() + bars.size())
	{}

	PatternView(Iterator data, int size, Iterator base, Iterator end) : _data(data), _size(size), _base(base), _end(end) {}

	template <size_t N>
	PatternView(const Pattern<N>& row) : _data(row.data()), _size(N)
	{}

	Iterator data() const { return _data; }
	Iterator begin() const { return _data; }
	Iterator end() const { return _data + _size; }

	value_type operator[](int i) const
	{
//		assert(i < _count);
		return _data[i];
	}

	int sum(int n = 0) const { return Reduce(_data, _data + (n == 0 ? _size : n)); }
	int size() const { return _size; }

	// index is the number of bars and spaces from the first bar to the current position
	int index() const { return narrow_cast<int>(_data - _base) - 1; }
	int pixelsInFront() const { return Reduce(_base, _data); }
	int pixelsTillEnd() const { return Reduce(_base, _data + _size) - 1; }
	bool isAtFirstBar() const { return _data == _base + 1; }
	bool isAtLastBar() const { return _data + _size == _end - 1; }
	bool isValid(int n) const { return _data && _data >= _base && _data + n <= _end; }
	bool isValid() const { return isValid(size()); }

	template<bool acceptIfAtFirstBar = false>
	bool hasQuietZoneBefore(float scale) const
	{
		return (acceptIfAtFirstBar && isAtFirstBar()) || _data[-1] >= sum() * scale;
	}

	template<bool acceptIfAtLastBar = true>
	bool hasQuietZoneAfter(float scale) const
	{
		return (acceptIfAtLastBar && isAtLastBar()) || _data[_size] >= sum() * scale;
	}

	PatternView subView(int offset, int size = 0) const
	{
//		if(std::abs(size) > count())
//			printf("%d > %d\n", std::abs(size), _count);
//		assert(std::abs(size) <= count());
		if (size == 0)
			size = _size - offset;
		else if (size < 0)
			size = _size - offset + size;
		return {begin() + offset, std::max(size, 0), _base, _end};
	}

	bool shift(int n)
	{
		return _data && ((_data += n) + _size <= _end);
	}

	bool skipPair()
	{
		return shift(2);
	}

	bool skipSymbol()
	{
		return shift(_size);
	}

	bool skipSingle(int maxWidth)
	{
		return shift(1) && _data[-1] <= maxWidth;
	}

	void extend()
	{
		_size = std::max(0, narrow_cast<int>(_end - _data));
	}
};

/**
 * @brief The BarAndSpace struct is a simple 2 element data structure to hold information about bar(s) and space(s).
 *
 * The operator[](int) can be used in combination with a PatternView
 */
template <typename T>
struct BarAndSpace
{
	using value_type = T;
	T bar = {}, space = {};
	// even index -> bar, odd index -> space
	constexpr T& operator[](int i) noexcept { return reinterpret_cast<T*>(this)[i & 1]; }
	constexpr T operator[](int i) const noexcept { return reinterpret_cast<const T*>(this)[i & 1]; }
	bool isValid() const { return bar != T{} && space != T{}; }
};

using BarAndSpaceI = BarAndSpace<PatternType>;

template <int LEN, typename RT, typename T>
constexpr auto BarAndSpaceSum(const T* view) noexcept
{
	BarAndSpace<RT> res;
	for (int i = 0; i < LEN; ++i)
		res[i] += view[i];
	return res;
}

/**
 * @brief FixedPattern describes a compile-time constant (start/stop) pattern.
 *
 * N = number of bars/spaces
 * SUM = sum over all N elements (size of pattern in modules)
 * IS_SPARCE = whether or not the pattern contains '0's denoting 'wide' bars/spaces
 */
template <int N, int SUM, bool IS_SPARCE = false>
struct FixedPattern
{
	using value_type = PatternRow::value_type;
	value_type _data[N];
	constexpr value_type operator[](int i) const noexcept { return _data[i]; }
	constexpr const value_type* data() const noexcept { return _data; }
	constexpr int size() const noexcept { return N; }
	constexpr BarAndSpace<value_type> sums() const noexcept { return BarAndSpaceSum<N, value_type>(_data); }
};

template <int N, int SUM>
using FixedSparcePattern = FixedPattern<N, SUM, true>;

template <bool E2E = false, int LEN, int SUM>
double IsPattern(const PatternView& view, const FixedPattern<LEN, SUM, false>& pattern, int spaceInPixel = 0,
				double minQuietZone = 0, double moduleSizeRef = 0)
{
	if constexpr (E2E) {
		auto widths = BarAndSpaceSum<LEN, double>(view.data());
		auto sums = pattern.sums();
		BarAndSpace<double> modSize = {widths[0] / sums[0], widths[1] / sums[1]};

		auto [m, M] = std::minmax(modSize[0], modSize[1]);
		if (M > 4 * m) // make sure module sizes of bars and spaces are not too far away from each other
			return 0;

		if (minQuietZone && spaceInPixel < minQuietZone * modSize.space)
			return 0;

		const BarAndSpace<double> thr = {modSize[0] * .75 + .5, modSize[1] / (2 + (LEN < 6)) + .5};

		for (int x = 0; x < LEN; ++x)
			if (std::abs(view[x] - pattern[x] * modSize[x]) > thr[x])
				return 0;

		return (modSize[0] + modSize[1]) / 2;
	}

	double width = view.sum(LEN);
	if (SUM > LEN && width < SUM)
		return 0;

	const auto moduleSize = width / SUM;

	if (minQuietZone && spaceInPixel < minQuietZone * moduleSize - 1)
		return 0;

	if (!moduleSizeRef)
		moduleSizeRef = moduleSize;

	// the offset of 0.5 is to make the code less sensitive to quantization errors for small (near 1) module sizes.
	// TODO: review once we have upsampling in the binarizer in place.
	const auto threshold = moduleSizeRef * (0.5 + E2E * 0.25) + 0.5;

	for (int x = 0; x < LEN; ++x)
		if (std::abs(view[x] - pattern[x] * moduleSizeRef) > threshold)
			return 0;

	return moduleSize;
}

template <bool RELAXED_THRESHOLD = false, int N, int SUM>
double IsPattern(const PatternView& view, const FixedPattern<N, SUM, true>& pattern, int spaceInPixel = 0,
				 double minQuietZone = 0, double moduleSizeRef = 0)
{
	// pattern contains the indices with the bars/spaces that need to be equally wide
	double width = 0;
	for (int x = 0; x < SUM; ++x)
		width += view[pattern[x]];

	const auto moduleSize = width / SUM;

	if (minQuietZone && spaceInPixel < minQuietZone * moduleSize - 1)
		return 0;

	if (!moduleSizeRef)
		moduleSizeRef = moduleSize;

	// the offset of 0.5 is to make the code less sensitive to quantization errors for small (near 1) module sizes.
	// TODO: review once we have upsampling in the binarizer in place.
	const auto threshold = moduleSizeRef * (0.5 + RELAXED_THRESHOLD * 0.25) + 0.5;

	for (int x = 0; x < SUM; ++x)
		if (std::abs(view[pattern[x]] - moduleSizeRef) > threshold)
			return 0;

	return moduleSize;
}

template <int N, int SUM, bool IS_SPARCE>
bool IsRightGuard(const PatternView& view, const FixedPattern<N, SUM, IS_SPARCE>& pattern, double minQuietZone,
				  double moduleSizeRef = 0)
{
	int spaceInPixel = view.isAtLastBar() ? std::numeric_limits<int>::max() : *view.end();
	return IsPattern(view, pattern, spaceInPixel, minQuietZone, moduleSizeRef) != 0;
}

template<int LEN, typename Pred>
PatternView FindLeftGuard(const PatternView& view, int minSize, Pred isGuard)
{
	if (view.size() < minSize)
		return {};

	auto window = view.subView(0, LEN);
	if (window.isAtFirstBar() && isGuard(window, std::numeric_limits<int>::max()))
		return window;
	for (auto end = view.end() - minSize; window.data() < end; window.skipPair())
		if (isGuard(window, window[-1]))
			return window;

	return {};
}

template <int LEN, int SUM, bool IS_SPARCE>
PatternView FindLeftGuard(const PatternView& view, int minSize, const FixedPattern<LEN, SUM, IS_SPARCE>& pattern,
						  double minQuietZone)
{
	return FindLeftGuard<LEN>(view, std::max(minSize, LEN),
							  [&pattern, minQuietZone](const PatternView& window, int spaceInPixel) {
								  return IsPattern(window, pattern, spaceInPixel, minQuietZone);
							  });
}

template <int LEN>
std::array<int, LEN - 2> NormalizedE2EPattern(const PatternView& view, int mods, bool reverse = false)
{
	double moduleSize = static_cast<double>(view.sum(LEN)) / mods;
	std::array<int, LEN - 2> e2e;

	for (int i = 0; i < LEN - 2; i++) {
		int i_v = reverse ? LEN - 2 - i : i;
		double v = (view[i_v] + view[i_v + 1]) / moduleSize;
		e2e[i] = int(v + .5);
	}

	return e2e;
}

template <int LEN, int SUM>
std::array<int, LEN> NormalizedPattern(const PatternView& view)
{
	double moduleSize = static_cast<double>(view.sum(LEN)) / SUM;
#if 1
	int err = SUM;
	std::array<int, LEN> is;
	std::array<double, LEN> rs;
	for (int i = 0; i < LEN; i++) {
		double v = view[i] / moduleSize;
		is[i] = int(v + .5);
		rs[i] = v - is[i];
		err -= is[i];
	}

	if (std::abs(err) > 1)
		return {};

	if (err) {
		auto mi = err > 0 ? std::max_element(std::begin(rs), std::end(rs)) - std::begin(rs)
						  : std::min_element(std::begin(rs), std::end(rs)) - std::begin(rs);
		is[mi] += err;
		rs[mi] -= err;
	}
#else
	std::array<int, LEN> is, e2e;
	int min_v = view[0], min_i = 0;

	for (int i = 1; i < LEN; i++) {
		double v = (view[i - 1] + view[i]) / moduleSize;
		e2e[i] = int(v + .5);
		if (view[i] < min_v) {
			min_v = view[i];
			min_i = i;
		}
	}

	is[min_i] = 1;
	for (int i = min_i + 1; i < LEN; ++i)
		is[i] = e2e[i] - is[i - 1];
	for (int i = min_i - 1; i >= 0; --i)
		is[i] = e2e[i + 1] - is[i + 1];
#endif
	return is;
}

template<typename I>
void GetPatternRow(Range<I> b_row, PatternRow& p_row)
{
	// TODO: if reactivating the bit-packed array (!ZX_FAST_BIT_STORAGE) should be of interest then the following code could be
	// considerably speed up by using a specialized variant along the lines of the old BitArray::getNextSetTo() function that
	// was removed between 1.4 and 2.0.

#if 0
	p_row.reserve(64);
	p_row.clear();

	auto lastPos = b_row.begin();
	if (*lastPos)
		p_row.push_back(0); // first value is number of white pixels, here 0

	for (auto p = b_row.begin() + 1; p < b_row.end(); ++p)
		if (bool(*p) != bool(*lastPos))
			p_row.push_back(p - std::exchange(lastPos, p));

	p_row.push_back(b_row.end() - lastPos);

	if (*lastPos)
		p_row.push_back(0); // last value is number of white pixels, here 0
#else
	p_row.resize(b_row.size() + 2);
	std::fill(p_row.begin(), p_row.end(), 0);

	auto bitPos = b_row.begin();
	const auto bitPosEnd = b_row.end();
	auto intPos = p_row.data();

	if (*bitPos)
		intPos++; // first value is number of white pixels, here 0

	// The following code as been observed to cause a speedup of up to 30% on large images on an AVX cpu
	// and on an a Google Pixel 3 Android phone. Your mileage may vary.
	if constexpr (std::is_pointer_v<I> && sizeof(I) == 8 && sizeof(std::remove_pointer_t<I>) == 1) {
		using simd_t = uint64_t;
		while (bitPos < bitPosEnd - sizeof(simd_t)) {
			auto asSimd0 = BitHacks::LoadU<simd_t>(bitPos);
			auto asSimd1 = BitHacks::LoadU<simd_t>(bitPos + 1);
			auto z = asSimd0 ^ asSimd1;
			if (z) {
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
				int step = BitHacks::NumberOfTrailingZeros(z) / 8 + 1;
#else
				int step = BitHacks::NumberOfLeadingZeros(z) / 8 + 1;
#endif
				(*intPos++) += step;
				bitPos += step;
			} else {
				(*intPos) += sizeof(simd_t);
				bitPos += sizeof(simd_t);
			}
		}
	}

	while (++bitPos != bitPosEnd) {
		++(*intPos);
		intPos += bitPos[0] != bitPos[-1];
	}
	++(*intPos);

	if (bitPos[-1])
		intPos++;

	p_row.resize(intPos - p_row.data() + 1);
#endif
}

} // ZXing
