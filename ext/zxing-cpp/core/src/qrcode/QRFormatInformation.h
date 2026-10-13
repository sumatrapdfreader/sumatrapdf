/*
* Copyright 2016 Nu-book Inc.
* Copyright 2016 ZXing authors
* Copyright 2023 Axel Waggershauser
*/
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "QRErrorCorrectionLevel.h"

#include <cstdint>

namespace ZXing::QRCode {

static constexpr uint32_t FORMAT_INFO_MASK_MODEL2 = 0x5412;
static constexpr uint32_t FORMAT_INFO_MASK_MODEL1 = 0x2825;
static constexpr uint32_t FORMAT_INFO_MASK_MICRO = 0x4445;
static constexpr uint32_t FORMAT_INFO_MASK_RMQR = 0x1FAB2; // Finder pattern side
static constexpr uint32_t FORMAT_INFO_MASK_RMQR_SUB = 0x20A7B; // Finder sub pattern side

class FormatInformation
{
public:
	uint32_t mask = 0;
	uint8_t data = 255;
	uint8_t hammingDistance = 255;
	uint8_t bitsIndex = 255;

	bool isMirrored = false;
	uint8_t dataMask = 0;
	uint8_t microVersion = 0;
	ErrorCorrectionLevel ecLevel = ErrorCorrectionLevel::Invalid;

	FormatInformation() = default;

	static FormatInformation DecodeQR(uint32_t formatInfoBits1, uint32_t formatInfoBits2);
	static FormatInformation DecodeMQR(uint32_t formatInfoBits);
	static FormatInformation DecodeRMQR(uint32_t formatInfoBits1, uint32_t formatInfoBits2);

	// Hamming distance of the 32 masked codes is 7 (64 and 8 for rMQR), by construction, so <= 3 bits differing means we found a match
	bool isValid() const { return hammingDistance <= 3; }

	Type type() const
	{
		switch (mask) {
		case FORMAT_INFO_MASK_MODEL1: return Type::Model1;
		case FORMAT_INFO_MASK_MICRO: return Type::Micro;
		case FORMAT_INFO_MASK_RMQR: [[fallthrough]];
		case FORMAT_INFO_MASK_RMQR_SUB: return Type::rMQR;
		default: return Type::Model2;
		}
	}

	bool operator==(const FormatInformation& other) const
	{
		return dataMask == other.dataMask && ecLevel == other.ecLevel && type() == other.type();
	}
};

} // namespace ZXing::QRCode
