/*
* Copyright 2016 Huy Cuong Nguyen
* Copyright 2016 ZXing authors
*/
// SPDX-License-Identifier: Apache-2.0

#include "ODUPCAWriter.h"

#include "BitMatrix.h"
#include "ODEAN13Writer.h"
#include "Utf.h"

#include <stdexcept>

namespace ZXing::OneD {

BitMatrix
UPCAWriter::encode(const std::wstring& contents, int width, int height) const
{
	// Transform a UPC-A code into the equivalent EAN-13 code, and add a check digit if it is not already present.
	size_t length = contents.length();
	if (length != 11 && length != 12) {
		throw std::invalid_argument("Requested contents should be 11 or 12 digits long");
	}
	return EAN13Writer().setMargin(_sidesMargin).encode(L'0' + contents, width, height);
}

BitMatrix UPCAWriter::encode(const std::string& contents, int width, int height) const
{
	return encode(FromUtf8(contents), width, height);
}

} // namespace ZXing::OneD
