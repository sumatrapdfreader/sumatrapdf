/*
* Copyright 2016 Nu-book Inc.
* Copyright 2016 ZXing authors
* Copyright 2020 Axel Waggershauser
*/
// SPDX-License-Identifier: Apache-2.0

#include "PDFReader.h"
#include "PDFDetector.h"
#include "PDFScanningDecoder.h"
#include "PDFCodewordDecoder.h"
#include "PDFDecoderResultExtra.h"
#include "ReaderOptions.h"
#include "DecoderResult.h"
#include "DetectorResult.h"
#include "Barcode.h"

#include "BitMatrixCursor.h"
#include "BinaryBitmap.h"
#include "BitArray.h"
#include "Pattern.h"

#include <vector>
#include <cstdlib>
#include <algorithm>
#include <limits>
#include <utility>

#ifdef PRINT_DEBUG
#include "BitMatrixIO.h"
#include <cstdio>
#endif

namespace ZXing {
namespace Pdf417 {

static const int MODULES_IN_STOP_PATTERN = 18;

static int GetMinWidth(const Nullable<ResultPoint>& p1, const Nullable<ResultPoint>& p2)
{
	if (p1 == nullptr || p2 == nullptr) {
		// the division prevents an integer overflow (see below). 120 million is still sufficiently large.
		return std::numeric_limits<int>::max() / CodewordDecoder::MODULES_IN_CODEWORD;
	}
	return std::abs(static_cast<int>(p1.value().x()) - static_cast<int>(p2.value().x()));
}

static int GetMinCodewordWidth(const std::array<Nullable<ResultPoint>, 8>& p)
{
	return std::min(std::min(GetMinWidth(p[0], p[4]), GetMinWidth(p[6], p[2]) * CodewordDecoder::MODULES_IN_CODEWORD / MODULES_IN_STOP_PATTERN),
					std::min(GetMinWidth(p[1], p[5]), GetMinWidth(p[7], p[3]) * CodewordDecoder::MODULES_IN_CODEWORD / MODULES_IN_STOP_PATTERN));
}

static int GetMaxWidth(const Nullable<ResultPoint>& p1, const Nullable<ResultPoint>& p2)
{
	if (p1 == nullptr || p2 == nullptr) {
		return 0;
	}
	return std::abs(static_cast<int>(p1.value().x()) - static_cast<int>(p2.value().x()));
}

static int GetMaxCodewordWidth(const std::array<Nullable<ResultPoint>, 8>& p)
{
	return std::max(std::max(GetMaxWidth(p[0], p[4]), GetMaxWidth(p[6], p[2]) * CodewordDecoder::MODULES_IN_CODEWORD / MODULES_IN_STOP_PATTERN),
					std::max(GetMaxWidth(p[1], p[5]), GetMaxWidth(p[7], p[3]) * CodewordDecoder::MODULES_IN_CODEWORD / MODULES_IN_STOP_PATTERN));
}

static Barcodes DoDecode(const BinaryBitmap& image, bool multiple, bool tryRotate, bool returnErrors)
{
	Detector::Result detectorResult = Detector::Detect(image, multiple, tryRotate);
	if (detectorResult.points.empty())
		return {};

	auto rotate = [res = detectorResult](PointI p) {
		switch(res.rotation) {
		case 90: return PointI(res.bits->height() - p.y - 1, p.x);
		case 180: return PointI(res.bits->width() - p.x - 1, res.bits->height() - p.y - 1);
		case 270: return PointI(p.y, res.bits->width() - p.x - 1);
		}
		return p;
	};

	Barcodes res;
	for (const auto& points : detectorResult.points) {
		DecoderResult decoderResult =
			ScanningDecoder::Decode(*detectorResult.bits, points[4], points[5], points[6], points[7],
									GetMinCodewordWidth(points), GetMaxCodewordWidth(points));
		if (decoderResult.isValid(returnErrors)) {
			auto point = [&](int i) {
				auto meta = dynamic_cast<DecoderResultExtra*>(decoderResult.extra().get());
				if (points[i].hasValue() || i < 2 || !meta)
					return rotate(PointI(points[i].value()));
				else {
					auto p = rotate(PointI(points[i - 2].value()) + PointI(meta->approxSymbolWidth, 0));
					p.x = std::clamp(p.x, 0, image.width() - 1);
					p.y = std::clamp(p.y, 0, image.height() - 1);
					return p;
				}
			};
			res.emplace_back(std::move(decoderResult), DetectorResult{{}, {point(0), point(2), point(3), point(1)}},
							 BarcodeFormat::PDF417);
			if (!multiple)
				return res;
		}
	}
	return res;
}

// new implementation (only for isPure use case atm.)

using Pattern417 = std::array<uint16_t, 8>;

struct CodeWord
{
	int cluster = -1;
	int code = -1;
	operator bool() const noexcept { return code != -1; }
};

struct SymbolInfo
{
	int width = 0, height = 0;
	int nRows = 0, nCols = 0, firstRow = -1, lastRow = -1;
	int ecLevel = -1;
	int colWidth = 0;
	float rowHeight = 0;
	operator bool() const noexcept { return nRows >= 3 && nCols >= 1 && ecLevel != -1; }
};

template<typename POINT>
CodeWord ReadCodeWord(BitMatrixCursor<POINT>& cur, int expectedCluster = -1)
{
	auto readCodeWord = [expectedCluster](auto& cur) -> CodeWord {
		auto np     = NormalizedPattern<8, 17>(cur.template readPattern<Pattern417>());
		int cluster = (np[0] - np[2] + np[4] - np[6] + 9) % 9;
		int code = expectedCluster == -1 || cluster == expectedCluster ? CodewordDecoder::GetCodeword(ToInt(np)) : -1;

		return {cluster, code};
	};

	auto curBackup = cur;
	auto cw = readCodeWord(cur);
	if (!cw) {
		for (auto offset : {curBackup.left(), curBackup.right()}) {
			auto curAlt = curBackup.movedBy(offset);
			if (!curAlt.isIn()) // curBackup might be the first or last image row
				continue;
			if (auto cwAlt = readCodeWord(curAlt)) {
				cur = curAlt;
				return cwAlt;
			}
		}
	}
	return cw;
}

static int Row(CodeWord rowIndicator)
{
	return (rowIndicator.code / 30) * 3 + rowIndicator.cluster / 3;
}

constexpr FixedPattern<8, 17> START_PATTERN = { 8, 1, 1, 1, 1, 1, 1, 3 };

#ifndef PRINT_DEBUG
#define printf(...){}
#endif

template<typename POINT>
SymbolInfo ReadSymbolInfo(BitMatrixCursor<POINT> topCur, POINT rowSkip, int colWidth, int width, int height)
{
	SymbolInfo res = {width, height};
	res.colWidth = colWidth;
	int clusterMask = 0;
	int rows0 = 0, rows1 = 0; // Suppress GNUC -Wmaybe-uninitialized

	topCur.p += .5f * rowSkip;

	for (auto startCur = topCur; clusterMask != 0b111 && maxAbsComponent(topCur.p - startCur.p) < height / 2; startCur.p += rowSkip) {
		auto cur = startCur;
		if (!IsPattern(cur.template readPatternFromBlack<Pattern417>(1, colWidth + 2), START_PATTERN))
			break;
		auto cw = ReadCodeWord(cur);
#ifdef PRINT_DEBUG
		printf("%3dx%3d:%2d: %4d.%d \n", int(cur.p.x), int(cur.p.y), Row(cw), cw.code, cw.cluster);
		fflush(stdout);
#endif
		if (!cw)
			continue;
		if (res.firstRow == -1)
			res.firstRow = Row(cw);
		switch (cw.cluster) {
		case 0: rows0 = cw.code % 30; break;
		case 3: rows1 = cw.code % 3, res.ecLevel = (cw.code % 30) / 3; break;
		case 6: res.nCols = (cw.code % 30) + 1; break;
		default: continue;
		}
		clusterMask |= (1 << cw.cluster / 3);
	}
	if ((clusterMask & 0b11) == 0b11)
		res.nRows = 3 * rows0 + rows1 + 1;
	return res;
}

template<typename POINT>
SymbolInfo DetectSymbol(BitMatrixCursor<POINT> topCur, int width, int height)
{
	auto pat = topCur.movedBy(height / 2 * topCur.right()).template readPatternFromBlack<Pattern417>(1, width / 3);
	if (!IsPattern(pat, START_PATTERN))
		return {};
	int colWidth = Reduce(pat);
	auto rowSkip = std::max(colWidth / 17.f, 1.f) * bresenhamDirection(topCur.right());
	auto botCur  = topCur.movedBy((height - 1) * topCur.right());

	auto topSI = ReadSymbolInfo(topCur, rowSkip, colWidth, width, height);
	auto botSI = ReadSymbolInfo(botCur, -rowSkip, colWidth, width, height);

	SymbolInfo res = topSI;
	res.lastRow = botSI.firstRow;
	res.rowHeight = float(height) / (std::abs(res.lastRow - res.firstRow) + 1);
	if (topSI.nCols != botSI.nCols)
		// if there is something fishy with the number of cols (aliasing), guess them from the width
		res.nCols = (width + res.colWidth / 2) / res.colWidth - 4;

	return res;
}

template<typename POINT>
std::vector<int> ReadCodeWords(BitMatrixCursor<POINT> topCur, SymbolInfo info)
{
	printf("rows: %d, cols: %d, rowHeight: %.1f, colWidth: %d, firstRow: %d, lastRow: %d, ecLevel: %d\n", info.nRows,
		   info.nCols, info.rowHeight, info.colWidth, info.firstRow, info.lastRow, info.ecLevel);
	auto print = [](CodeWord c [[maybe_unused]]) { printf("%4d.%d ", c.code, c.cluster); };

	auto rowSkip = topCur.right();
	if (info.firstRow > info.lastRow) {
		topCur.p += (info.height - 1) * rowSkip;
		rowSkip = -rowSkip;
		std::swap(info.firstRow, info.lastRow);
	}

	int maxColWidth = info.colWidth * 3 / 2;
	std::vector<int> codeWords(info.nRows * info.nCols, -1);
	for (int row = info.firstRow; row < std::min(info.nRows, info.lastRow + 1); ++row) {
		int cluster = (row % 3) * 3;
		auto cur = topCur.movedBy(int((row - info.firstRow + 0.5f) * info.rowHeight) * rowSkip);
		// skip start pattern
		cur.stepToEdge(8 + cur.isWhite(), maxColWidth);
		// read off left row indicator column
		auto cw [[maybe_unused]] = ReadCodeWord(cur, cluster);
		printf("%3dx%3d:%2d: ", int(cur.p.x), int(cur.p.y), Row(cw));
		print(cw);

		for (int col = 0; col < info.nCols && cur.isIn(); ++col) {
			auto cw = ReadCodeWord(cur, cluster);
			codeWords[row * info.nCols + col] = cw.code;
			print(cw);
		}

#ifdef PRINT_DEBUG
		print(ReadCodeWord(cur));
		printf("\n");
		fflush(stdout);
#endif
	}

	return codeWords;
}

static Barcode DecodePure(const BinaryBitmap& image_)
{
	auto pimage = image_.getBitMatrix();
	if (!pimage)
		return {};
	auto& image = *pimage;

#ifdef PRINT_DEBUG
	SaveAsPBM(image, "weg.pbm");
#endif

	int left, top, width, height;
	if (!image.findBoundingBox(left, top, width, height, 9) || (width < 3 * 17 && height < 3 * 17))
		return {};
	int right  = left + width - 1;
	int bottom = top + height - 1;

	// counter intuitively, using a floating point cursor is about twice as fast an integer one (on an AVX architecture)
	BitMatrixCursorF cur(image, centered(PointI{left, top}), PointF{1, 0});
	SymbolInfo info;

	// try all 4 orientations
	for (int a = 0; a < 4; ++a) {
		info = DetectSymbol(cur, width, height);
		if (info)
			break;
		cur.step(width - 1);
		cur.turnRight();
		std::swap(width, height);
	}

	if (!info)
		return {};

	auto codeWords = ReadCodeWords(cur, info);

	auto res = DecodeCodewords(codeWords, NumECCodeWords(info.ecLevel));

	return Barcode(std::move(res), {{}, {{left, top}, {right, top}, {right, bottom}, {left, bottom}}}, BarcodeFormat::PDF417);
}

Barcode
Reader::decode(const BinaryBitmap& image) const
{
	if (_opts.isPure()) {
		auto res = DecodePure(image);
		if (res.error() != Error::Checksum)
			return res;
		// This falls through and tries the non-pure code path if we have a checksum error. This approach is
		// currently the best option to deal with 'aliased' input like e.g. 03-aliased.png
	}

	return FirstOrDefault(DoDecode(image, false, _opts.tryRotate(), _opts.returnErrors()));
}

Barcodes Reader::decode(const BinaryBitmap& image, [[maybe_unused]] int maxSymbols) const
{
	return DoDecode(image, true, _opts.tryRotate(), _opts.returnErrors());
}

} // Pdf417
} // ZXing
