/*
 * Copyright 2023 Antoine Mérino
 * Copyright 2023 Axel Waggershauser
 */
// SPDX-License-Identifier: Apache-2.0

#include "ODDXFilmEdgeReader.h"

#include "Barcode.h"

#include <optional>
#include <vector>

namespace ZXing::OneD {

namespace {

// Detection is made from center outward.
// We ensure the clock track is decoded before the data track to avoid false positives.
// They are two version of a DX Edge codes : with and without frame number.
// The clock track is longer if the DX code contains the frame number (more recent version)
constexpr int CLOCK_LENGTH_FN = 31;
constexpr int CLOCK_LENGTH_NO_FN = 23;

// data track length, without the start and stop patterns
constexpr int DATA_LENGTH_FN = 23;
constexpr int DATA_LENGTH_NO_FN = 15;

constexpr auto CLOCK_PATTERN_FN =
	FixedPattern<25, CLOCK_LENGTH_FN>{5, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 3};
constexpr auto CLOCK_PATTERN_NO_FN = FixedPattern<17, CLOCK_LENGTH_NO_FN>{5, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 3};
constexpr auto DATA_START_PATTERN = FixedPattern<5, 5>{1, 1, 1, 1, 1};
constexpr auto DATA_STOP_PATTERN = FixedPattern<3, 3>{1, 1, 1};

template <int N, int SUM>
bool IsPattern(PatternView& view, const FixedPattern<N, SUM>& pattern, float minQuietZone)
{
	view = view.subView(0, N);
	return view.isValid() && IsPattern(view, pattern, view.isAtFirstBar() ? std::numeric_limits<int>::max() : view[-1], minQuietZone);
}

bool DistIsBelowThreshold(PointI a, PointI b, PointI threshold)
{
	return std::abs(a.x - b.x) < threshold.x && std::abs(a.y - b.y) < threshold.y;
}

// DX Film Edge clock track found on 35mm films.
struct Clock
{
	bool hasFrameNr = false; // Clock track (thus data track) with frame number (longer version)
	int rowNumber = 0;
	int xStart = 0; // Beginning of the clock track on the X-axis, in pixels
	int xStop = 0; // End of the clock track on the X-axis, in pixels

	int dataLength() const { return hasFrameNr ? DATA_LENGTH_FN : DATA_LENGTH_NO_FN; }

	float moduleSize() const { return float(xStop - xStart) / (hasFrameNr ? CLOCK_LENGTH_FN : CLOCK_LENGTH_NO_FN); }

	bool isCloseTo(PointI p, int x) const { return DistIsBelowThreshold(p, {x, rowNumber}, PointI(moduleSize() * PointF{0.5, 4})); }

	bool isCloseToStart(int x, int y) const { return isCloseTo({x, y}, xStart); }
	bool isCloseToStop(int x, int y) const { return isCloseTo({x, y}, xStop); }
};

struct DXFEState : public RowReader::DecodingState
{
	int centerRow = 0;
	std::vector<Clock> clocks;

	// see if we a clock that starts near {x, y}
	Clock* findClock(int x, int y)
	{
		auto i = FindIf(clocks, [start = PointI{x, y}](auto& v) { return v.isCloseToStart(start.x, start.y); });
		return i != clocks.end() ? &(*i) : nullptr;
	}

	// add/update clock
	void addClock(const Clock& clock)
	{
		if (Clock* i = findClock(clock.xStart, clock.rowNumber))
			*i = clock;
		else
			clocks.push_back(clock);
	}
};

std::optional<Clock> CheckForClock(int rowNumber, PatternView& view)
{
	Clock clock;

	if (IsPattern(view, CLOCK_PATTERN_FN, 0.5)) // On FN versions, the decimal number can be really close to the clock
		clock.hasFrameNr = true;
	else if (IsPattern(view, CLOCK_PATTERN_NO_FN, 2.0))
		clock.hasFrameNr = false;
	else
		return {};

	clock.rowNumber = rowNumber;
	clock.xStart = view.pixelsInFront();
	clock.xStop = view.pixelsTillEnd();

	return clock;
}

} // namespace

Barcode DXFilmEdgeReader::decodePattern(int rowNumber, PatternView& next, std::unique_ptr<DecodingState>& state) const
{
	if (!state) {
		state.reset(new DXFEState);
		static_cast<DXFEState*>(state.get())->centerRow = rowNumber;
	}

	auto dxState = static_cast<DXFEState*>(state.get());

	// Only consider rows below the center row of the image
	if (!_opts.tryRotate() && rowNumber < dxState->centerRow)
		return {};

	// Look for a pattern that is part of both the clock as well as the data track (ommitting the first bar)
	constexpr auto Is4x1 = [](const PatternView& view, int spaceInPixel) {
		// find min/max of 4 consecutive bars/spaces and make sure they are close together
		auto [m, M] = std::minmax({view[1], view[2], view[3], view[4]});
		return M <= m * 4 / 3 + 1 && spaceInPixel > m / 2;
	};

	// 12 is the minimum size of the data track (at least one product class bit + one parity bit)
	next = FindLeftGuard<4>(next, 10, Is4x1);
	if (!next.isValid())
		return {};

	// Check if the 4x1 pattern is part of a clock track
	if (auto clock = CheckForClock(rowNumber, next)) {
		dxState->addClock(*clock);
		next.skipSymbol();
		return {};
	}

	// Without at least one clock track, we stop here
	if (dxState->clocks.empty())
		return {};

	constexpr float minDataQuietZone = 0.5;

	if (!IsPattern(next, DATA_START_PATTERN, minDataQuietZone))
		return {};

	auto xStart = next.pixelsInFront();

	// Only consider data tracks that are next to a clock track
	auto clock = dxState->findClock(xStart, rowNumber);
	if (!clock)
		return {};

	// Skip the data start pattern (black, white, black, white, black)
	// The first signal bar is always white: this is the
	// separation between the start pattern and the product number
	next.skipSymbol();

	// Read the data bits
	BitArray dataBits;
	while (next.isValid(1) && dataBits.size() < clock->dataLength()) {

		int modules = int(next[0] / clock->moduleSize() + 0.5);
		// even index means we are at a bar, otherwise at a space
		dataBits.appendBits(next.index() % 2 == 0 ? 0xFFFFFFFF : 0x0, modules);

		next.shift(1);
	}

	// Check the data track length
	if (dataBits.size() != clock->dataLength())
		return {};

	next = next.subView(0, DATA_STOP_PATTERN.size());

	// Check there is the Stop pattern at the end of the data track
	if (!next.isValid() || !IsRightGuard(next, DATA_STOP_PATTERN, minDataQuietZone))
		return {};

	// The following bits are always white (=false), they are separators.
	if (dataBits.get(0) || dataBits.get(8) || (clock->hasFrameNr ? (dataBits.get(20) || dataBits.get(22)) : dataBits.get(14)))
		return {};

	// Check the parity bit
	auto signalSum = Reduce(dataBits.begin(), dataBits.end() - 2, 0);
	auto parityBit = *(dataBits.end() - 2);
	if (signalSum % 2 != (int)parityBit)
		return {};

	// Compute the DX 1 number (product number)
	auto productNumber = ToInt(dataBits, 1, 7);
	if (!productNumber)
		return {};

	// Compute the DX 2 number (generation number)
	auto generationNumber = ToInt(dataBits, 9, 4);

	// Generate the textual representation.
	// Eg: 115-10/11A means: DX1 = 115, DX2 = 10, Frame number = 11A
	std::string txt;
	txt.reserve(10);
	txt = std::to_string(productNumber) + "-" + std::to_string(generationNumber);
	if (clock->hasFrameNr) {
		auto frameNr = ToInt(dataBits, 13, 6);
		txt += "/" + std::to_string(frameNr);
		if (dataBits.get(19))
			txt += "A";
	}

	auto xStop = next.pixelsTillEnd();

	// The found data track must end near the clock track
	if (!clock->isCloseToStop(xStop, rowNumber))
		return {};

	// Update the clock coordinates with the latest corresponding data track
	// This may improve signal detection for next row iterations
	clock->xStart = xStart;
	clock->xStop = xStop;

	return Barcode(txt, rowNumber, xStart, xStop, BarcodeFormat::DXFilmEdge, {});
}

} // namespace ZXing::OneD
