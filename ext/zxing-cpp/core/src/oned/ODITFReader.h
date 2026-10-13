/*
* Copyright 2016 Nu-book Inc.
* Copyright 2016 ZXing authors
*/
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "ODRowReader.h"

namespace ZXing::OneD {

/**
* <p>Implements decoding of the ITF format, or Interleaved Two of Five.</p>
*
* <p>This Reader will scan ITF barcodes of certain lengths only.
* At the moment it reads length >= 6. Not all lengths are scanned, especially shorter ones, to avoid false positives.
* This in turn is due to a lack of required checksum function.</p>
*
* <p>According to the specification, the modifier (3rd char) of the symbologyIdentifier is '1' iff the symbol has a valid checksum</p>
*
* <p><a href="http://en.wikipedia.org/wiki/Interleaved_2_of_5">http://en.wikipedia.org/wiki/Interleaved_2_of_5</a>
* is a great reference for Interleaved 2 of 5 information.</p>
*/
class ITFReader : public RowReader
{
public:
	using RowReader::RowReader;

	Barcode decodePattern(int rowNumber, PatternView& next, std::unique_ptr<DecodingState>&) const override;
};

} // namespace ZXing::OneD
