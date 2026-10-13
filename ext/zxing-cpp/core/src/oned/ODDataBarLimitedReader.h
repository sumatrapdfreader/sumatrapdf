/*
* Copyright 2024 Axel Waggershauser
*/
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "ODRowReader.h"

namespace ZXing::OneD {

/**
* Decodes DataBarLimited sybmols. See ISO/IEC 24724:2011.
*/
class DataBarLimitedReader : public RowReader
{
public:
	using RowReader::RowReader;

	Barcode decodePattern(int rowNumber, PatternView& next, std::unique_ptr<DecodingState>& state) const override;
};

} // namespace ZXing::OneD
