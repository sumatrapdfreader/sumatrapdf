/*
* Copyright 2016 Nu-book Inc.
* Copyright 2016 ZXing authors
* Copyright 2022 Axel Waggershauser
*/
// SPDX-License-Identifier: Apache-2.0

#include "QRReader.h"

#include "BinaryBitmap.h"
#include "ConcentricFinder.h"
#include "ReaderOptions.h"
#include "DecoderResult.h"
#include "DetectorResult.h"
#include "LogMatrix.h"
#include "QRDecoder.h"
#include "QRDetector.h"
#include "Barcode.h"

#include <utility>

namespace ZXing::QRCode {

Barcode Reader::decode(const BinaryBitmap& image) const
{
#if 1
	if (!_opts.isPure())
		return FirstOrDefault(decode(image, 1));
#endif

	auto binImg = image.getBitMatrix();
	if (binImg == nullptr)
		return {};

	DetectorResult detectorResult;
	if (_opts.hasFormat(BarcodeFormat::QRCode))
		detectorResult = DetectPureQR(*binImg);
	if (_opts.hasFormat(BarcodeFormat::MicroQRCode) && !detectorResult.isValid())
		detectorResult = DetectPureMQR(*binImg);
	if (_opts.hasFormat(BarcodeFormat::RMQRCode) && !detectorResult.isValid())
		detectorResult = DetectPureRMQR(*binImg);

	if (!detectorResult.isValid())
		return {};

	auto decoderResult = Decode(detectorResult.bits());
	auto format = detectorResult.bits().width() != detectorResult.bits().height() ? BarcodeFormat::RMQRCode
				  : detectorResult.bits().width() < 21                            ? BarcodeFormat::MicroQRCode
																				  : BarcodeFormat::QRCode;

	return Barcode(std::move(decoderResult), std::move(detectorResult), format);
}

void logFPSet(const FinderPatternSet& fps [[maybe_unused]])
{
#ifdef PRINT_DEBUG
	auto drawLine = [](PointF a, PointF b) {
		int steps = maxAbsComponent(b - a);
		PointF dir = bresenhamDirection(PointF(b - a));
		for (int i = 0; i < steps; ++i)
			log(a + i * dir, 2);
	};

	drawLine(fps.bl, fps.tl);
	drawLine(fps.tl, fps.tr);
	drawLine(fps.tr, fps.bl);
#endif
}

Barcodes Reader::decode(const BinaryBitmap& image, int maxSymbols) const
{
	auto binImg = image.getBitMatrix();
	if (binImg == nullptr)
		return {};

#ifdef PRINT_DEBUG
	LogMatrixWriter lmw(log, *binImg, 5, "qr-log.pnm");
#endif
	
	auto allFPs = FindFinderPatterns(*binImg, _opts.tryHarder());

#ifdef PRINT_DEBUG
	printf("allFPs: %d\n", Size(allFPs));
#endif

	std::vector<ConcentricPattern> usedFPs;
	Barcodes res;
	
	if (_opts.hasFormat(BarcodeFormat::QRCode)) {
		auto allFPSets = GenerateFinderPatternSets(allFPs);
		for (const auto& fpSet : allFPSets) {
			if (Contains(usedFPs, fpSet.bl) || Contains(usedFPs, fpSet.tl) || Contains(usedFPs, fpSet.tr))
				continue;

			logFPSet(fpSet);

			auto detectorResult = SampleQR(*binImg, fpSet);
			if (detectorResult.isValid()) {
				auto decoderResult = Decode(detectorResult.bits());
				if (decoderResult.isValid()) {
					usedFPs.push_back(fpSet.bl);
					usedFPs.push_back(fpSet.tl);
					usedFPs.push_back(fpSet.tr);
				}
				if (decoderResult.isValid(_opts.returnErrors())) {
					res.emplace_back(std::move(decoderResult), std::move(detectorResult), BarcodeFormat::QRCode);
					if (maxSymbols && Size(res) == maxSymbols)
						break;
				}
			}
		}
	}
	
	if (_opts.hasFormat(BarcodeFormat::MicroQRCode) && !(maxSymbols && Size(res) == maxSymbols)) {
		for (const auto& fp : allFPs) {
			if (Contains(usedFPs, fp))
				continue;

			auto detectorResult = SampleMQR(*binImg, fp);
			if (detectorResult.isValid()) {
				auto decoderResult = Decode(detectorResult.bits());
				if (decoderResult.isValid(_opts.returnErrors())) {
					res.emplace_back(std::move(decoderResult), std::move(detectorResult), BarcodeFormat::MicroQRCode);
					if (maxSymbols && Size(res) == maxSymbols)
						break;
				}

			}
		}
	}
	
	if (_opts.hasFormat(BarcodeFormat::RMQRCode) && !(maxSymbols && Size(res) == maxSymbols)) {
		// TODO proper
		for (const auto& fp : allFPs) {
			if (Contains(usedFPs, fp))
				continue;

			auto detectorResult = SampleRMQR(*binImg, fp);
			if (detectorResult.isValid()) {
				auto decoderResult = Decode(detectorResult.bits());
				if (decoderResult.isValid(_opts.returnErrors())) {
					res.emplace_back(std::move(decoderResult), std::move(detectorResult), BarcodeFormat::RMQRCode);
					if (maxSymbols && Size(res) == maxSymbols)
						break;
				}

			}
		}
	}

	return res;
}

} // namespace ZXing::QRCode
