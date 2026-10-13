/*
* Copyright 2017 Huy Cuong Nguyen
*/
// SPDX-License-Identifier: Apache-2.0

#include "MultiFormatWriter.h"

#include "BitMatrix.h"
#include "aztec/AZWriter.h"
#include "datamatrix/DMWriter.h"
#include "oned/ODCodabarWriter.h"
#include "oned/ODCode128Writer.h"
#include "oned/ODCode39Writer.h"
#include "oned/ODCode93Writer.h"
#include "oned/ODEAN13Writer.h"
#include "oned/ODEAN8Writer.h"
#include "oned/ODITFWriter.h"
#include "oned/ODUPCAWriter.h"
#include "oned/ODUPCEWriter.h"
#include "pdf417/PDFWriter.h"
#include "qrcode/QRErrorCorrectionLevel.h"
#include "qrcode/QRWriter.h"
#include "Utf.h"

#include <stdexcept>

namespace ZXing {

BitMatrix
MultiFormatWriter::encode(const std::wstring& contents, int width, int height) const
{
	auto exec0 = [&](auto&& writer) {
		if (_margin >=0)
			writer.setMargin(_margin);
		return writer.encode(contents, width, height);
	};

	auto AztecEccLevel = [&](Aztec::Writer& writer, int eccLevel) { writer.setEccPercent(eccLevel * 100 / 8); };
	auto Pdf417EccLevel = [&](Pdf417::Writer& writer, int eccLevel) { writer.setErrorCorrectionLevel(eccLevel); };
	auto QRCodeEccLevel = [&](QRCode::Writer& writer, int eccLevel) {
		writer.setErrorCorrectionLevel(static_cast<QRCode::ErrorCorrectionLevel>(--eccLevel / 2));
	};

	auto exec1 = [&](auto&& writer, auto setEccLevel) {
		if (_encoding != CharacterSet::Unknown)
			writer.setEncoding(_encoding);
		if (_eccLevel >= 0 && _eccLevel <= 8)
			setEccLevel(writer, _eccLevel);
		return exec0(std::move(writer));
	};

	auto exec2 = [&](auto&& writer) {
		if (_encoding != CharacterSet::Unknown)
			writer.setEncoding(_encoding);
		return exec0(std::move(writer));
	};

	switch (_format) {
	case BarcodeFormat::Aztec: return exec1(Aztec::Writer(), AztecEccLevel);
	case BarcodeFormat::DataMatrix: return exec2(DataMatrix::Writer());
	case BarcodeFormat::PDF417: return exec1(Pdf417::Writer(), Pdf417EccLevel);
	case BarcodeFormat::QRCode: return exec1(QRCode::Writer(), QRCodeEccLevel);
	case BarcodeFormat::Codabar: return exec0(OneD::CodabarWriter());
	case BarcodeFormat::Code39: return exec0(OneD::Code39Writer());
	case BarcodeFormat::Code93: return exec0(OneD::Code93Writer());
	case BarcodeFormat::Code128: return exec0(OneD::Code128Writer());
	case BarcodeFormat::EAN8: return exec0(OneD::EAN8Writer());
	case BarcodeFormat::EAN13: return exec0(OneD::EAN13Writer());
	case BarcodeFormat::ITF: return exec0(OneD::ITFWriter());
	case BarcodeFormat::UPCA: return exec0(OneD::UPCAWriter());
	case BarcodeFormat::UPCE: return exec0(OneD::UPCEWriter());
	default: throw std::invalid_argument(std::string("Unsupported format: ") + ToString(_format));
	}
}

BitMatrix MultiFormatWriter::encode(const std::string& contents, int width, int height) const
{
	return encode(FromUtf8(contents), width, height);
}

} // ZXing
