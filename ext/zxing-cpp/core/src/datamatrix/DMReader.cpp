/*
* Copyright 2016 Nu-book Inc.
* Copyright 2016 ZXing authors
*/
// SPDX-License-Identifier: Apache-2.0

#include "DMReader.h"

#include "BinaryBitmap.h"
#include "DMDecoder.h"
#include "DMDetector.h"
#include "ReaderOptions.h"
#include "DecoderResult.h"
#include "DetectorResult.h"
#include "Barcode.h"

#include <utility>

namespace ZXing::DataMatrix {

Barcode Reader::decode(const BinaryBitmap& image) const
{
#ifdef __cpp_impl_coroutine
	return FirstOrDefault(decode(image, 1));
#else
	auto binImg = image.getBitMatrix();
	if (binImg == nullptr)
		return {};
	
	auto detectorResult = Detect(*binImg, _opts.tryHarder(), _opts.tryRotate(), _opts.isPure());
	if (!detectorResult.isValid())
		return {};

	return Barcode(Decode(detectorResult.bits()), std::move(detectorResult), BarcodeFormat::DataMatrix);
#endif
}

#ifdef __cpp_impl_coroutine
Barcodes Reader::decode(const BinaryBitmap& image, int maxSymbols) const
{
	auto binImg = image.getBitMatrix();
	if (binImg == nullptr)
		return {};

	Barcodes res;
	for (auto&& detRes : Detect(*binImg, _opts.tryHarder(), _opts.tryRotate(), _opts.isPure())) {
		auto decRes = Decode(detRes.bits());
		if (decRes.isValid(_opts.returnErrors())) {
			res.emplace_back(std::move(decRes), std::move(detRes), BarcodeFormat::DataMatrix);
			if (maxSymbols > 0 && Size(res) >= maxSymbols)
				break;
		}
	}

	return res;
}
#endif
} // namespace ZXing::DataMatrix
