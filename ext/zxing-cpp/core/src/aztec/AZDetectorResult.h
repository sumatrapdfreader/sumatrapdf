/*
* Copyright 2016 Nu-book Inc.
* Copyright 2016 ZXing authors
*/
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "DetectorResult.h"

#include <utility>

namespace ZXing::Aztec {

class DetectorResult : public ZXing::DetectorResult
{
	bool _compact = false;
	int _nbDatablocks = 0;
	int _nbLayers = 0;
	bool _readerInit = false;
	bool _isMirrored = false;
	int _runeValue = -1;

	DetectorResult(const DetectorResult&) = delete;
	DetectorResult& operator=(const DetectorResult&) = delete;

public:
	DetectorResult() = default;
	DetectorResult(DetectorResult&&) noexcept = default;
	DetectorResult& operator=(DetectorResult&&) noexcept = default;

	DetectorResult(ZXing::DetectorResult&& result, bool isCompact, int nbDatablocks, int nbLayers, bool readerInit, bool isMirrored, int runeValue)
		: ZXing::DetectorResult{std::move(result)},
		  _compact(isCompact),
		  _nbDatablocks(nbDatablocks),
		  _nbLayers(nbLayers),
		  _readerInit(readerInit),
		  _isMirrored(isMirrored),
		  _runeValue(runeValue)
	{}

	bool isCompact() const { return _compact; }
	int nbDatablocks() const { return _nbDatablocks; }
	int nbLayers() const { return _nbLayers; }
	bool readerInit() const { return _readerInit; }
	bool isMirrored() const { return _isMirrored; }

	// Only meaningful is nbDatablocks == 0
	int runeValue() const { return _runeValue; }
};

} // namespace ZXing::Aztec
