/*
* Copyright 2016 Nu-book Inc.
* Copyright 2016 ZXing authors
*/
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "QRErrorCorrectionLevel.h"

namespace ZXing {

class BitMatrix;
class ByteArray;

namespace QRCode {

class Version;
class FormatInformation;

/**
 * @brief Reads version information from the QR Code.
 * @return {@link Version} encapsulating the QR Code's version, nullptr if neither location can be parsed
 */
const Version* ReadVersion(const BitMatrix& bitMatrix, Type type);

/**
 * @brief Reads format information from one of its two locations within the QR Code.
 * @return {@link FormatInformation} encapsulating the QR Code's format info, result is invalid if both format
 * information locations cannot be parsed as the valid encoding of format information
 */
FormatInformation ReadFormatInformation(const BitMatrix& bitMatrix);

/**
 * @brief Reads the codewords from the BitMatrix.
 * @return bytes encoded within the QR Code or empty array if the exact number of bytes expected is not read
 */
ByteArray ReadCodewords(const BitMatrix& bitMatrix, const Version& version, const FormatInformation& formatInfo);

} // QRCode
} // ZXing
