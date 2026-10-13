/*
* Copyright 2016 Nu-book Inc.
* Copyright 2016 ZXing authors
*/
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "Reader.h"

namespace ZXing::MaxiCode {

class Reader : public ZXing::Reader
{
public:
	using ZXing::Reader::Reader;

	Barcode decode(const BinaryBitmap& image) const override;
};

} // namespace ZXing::MaxiCode
