/*
* Copyright 2016 Nu-book Inc.
* Copyright 2016 ZXing authors
*/
// SPDX-License-Identifier: Apache-2.0

#pragma once

namespace ZXing::QRCode {

/**
* <p>See ISO 18004:2006, 6.5.1. This enum encapsulates the four error correction levels
* defined by the QR code standard.</p>
*
* @author Sean Owen
*/
enum class ErrorCorrectionLevel
{
	Low,     // L = ~7 % correction
	Medium,  // M = ~15% correction
	Quality, // Q = ~25% correction
	High,    // H = ~30% correction
	Invalid, // denotes in invalid/unknown value
};

const char* ToString(ErrorCorrectionLevel l);
ErrorCorrectionLevel ECLevelFromString(const char* str);
ErrorCorrectionLevel ECLevelFromBits(int bits, const bool isMicro = false);
int BitsFromECLevel(ErrorCorrectionLevel l);

enum class Type
{
	Model1,
	Model2,
	Micro,
	rMQR,
};

} // namespace ZXing::QRCode
