/*
* Copyright 2016 Nu-book Inc.
* Copyright 2016 ZXing authors
* Copyright 2023 Axel Waggershauser
* Copyright 2023 gitlost
*/
// SPDX-License-Identifier: Apache-2.0

#include "QRFormatInformation.h"

#include "BitHacks.h"
#include "ZXAlgorithms.h"

namespace ZXing::QRCode {

static uint32_t MirrorBits(uint32_t bits)
{
	return BitHacks::Reverse(bits) >> 17;
}

static FormatInformation FindBestFormatInfo(const std::vector<uint32_t>& masks, const std::vector<uint32_t>& bits)
{
	// See ISO 18004:2015, Annex C, Table C.1
	constexpr uint32_t MODEL2_MASKED_PATTERNS[] = {
		0x5412, 0x5125, 0x5E7C, 0x5B4B, 0x45F9, 0x40CE, 0x4F97, 0x4AA0, 0x77C4, 0x72F3, 0x7DAA, 0x789D, 0x662F, 0x6318, 0x6C41, 0x6976,
		0x1689, 0x13BE, 0x1CE7, 0x19D0, 0x0762, 0x0255, 0x0D0C, 0x083B, 0x355F, 0x3068, 0x3F31, 0x3A06, 0x24B4, 0x2183, 0x2EDA, 0x2BED,
	};

	FormatInformation fi;

	for (auto mask : masks)
		for (int bitsIndex = 0; bitsIndex < Size(bits); ++bitsIndex)
			for (uint32_t pattern : MODEL2_MASKED_PATTERNS) {
				// 'unmask' the pattern first to get the original 5-data bits + 10-ec bits back
				pattern ^= FORMAT_INFO_MASK_MODEL2;
				// Find the pattern with fewest bits differing
				if (int hammingDist = BitHacks::CountBitsSet((bits[bitsIndex] ^ mask) ^ pattern);
					hammingDist < fi.hammingDistance) {
					fi.mask = mask; // store the used mask to discriminate between types/models
					fi.data = pattern >> 10; // drop the 10 BCH error correction bits
					fi.hammingDistance = hammingDist;
					fi.bitsIndex = bitsIndex;
				}
			}

	return fi;
}

static FormatInformation FindBestFormatInfoRMQR(const std::vector<uint32_t>& bits, const std::vector<uint32_t>& subbits)
{
	// See ISO/IEC 23941:2022, Annex C, Table C.1 - Valid format information sequences
	constexpr uint32_t MASKED_PATTERNS[64] = { // Finder pattern side
		0x1FAB2, 0x1E597, 0x1DBDD, 0x1C4F8, 0x1B86C, 0x1A749, 0x19903, 0x18626,
		0x17F0E, 0x1602B, 0x15E61, 0x14144, 0x13DD0, 0x122F5, 0x11CBF, 0x1039A,
		0x0F1CA, 0x0EEEF, 0x0D0A5, 0x0CF80, 0x0B314, 0x0AC31, 0x0927B, 0x08D5E,
		0x07476, 0x06B53, 0x05519, 0x04A3C, 0x036A8, 0x0298D, 0x017C7, 0x008E2,
		0x3F367, 0x3EC42, 0x3D208, 0x3CD2D, 0x3B1B9, 0x3AE9C, 0x390D6, 0x38FF3,
		0x376DB, 0x369FE, 0x357B4, 0x34891, 0x33405, 0x32B20, 0x3156A, 0x30A4F,
		0x2F81F, 0x2E73A, 0x2D970, 0x2C655, 0x2BAC1, 0x2A5E4, 0x29BAE, 0x2848B,
		0x27DA3, 0x26286, 0x25CCC, 0x243E9, 0x23F7D, 0x22058, 0x21E12, 0x20137,
	};
	constexpr uint32_t MASKED_PATTERNS_SUB[64] = { // Finder sub pattern side
		0x20A7B, 0x2155E, 0x22B14, 0x23431, 0x248A5, 0x25780, 0x269CA, 0x276EF,
		0x28FC7, 0x290E2, 0x2AEA8, 0x2B18D, 0x2CD19, 0x2D23C, 0x2EC76, 0x2F353,
		0x30103, 0x31E26, 0x3206C, 0x33F49, 0x343DD, 0x35CF8, 0x362B2, 0x37D97,
		0x384BF, 0x39B9A, 0x3A5D0, 0x3BAF5, 0x3C661, 0x3D944, 0x3E70E, 0x3F82B,
		0x003AE, 0x01C8B, 0x022C1, 0x03DE4, 0x04170, 0x05E55, 0x0601F, 0x07F3A,
		0x08612, 0x09937, 0x0A77D, 0x0B858, 0x0C4CC, 0x0DBE9, 0x0E5A3, 0x0FA86,
		0x108D6, 0x117F3, 0x129B9, 0x1369C, 0x14A08, 0x1552D, 0x16B67, 0x17442,
		0x18D6A, 0x1924F, 0x1AC05, 0x1B320, 0x1CFB4, 0x1D091, 0x1EEDB, 0x1F1FE,
	};

	FormatInformation fi;

	auto best = [&fi](const std::vector<uint32_t>& bits, const uint32_t (&patterns)[64], uint32_t mask)
	{
		for (int bitsIndex = 0; bitsIndex < Size(bits); ++bitsIndex)
			for (uint32_t pattern : patterns) {
				// 'unmask' the pattern first to get the original 6-data bits + 12-ec bits back
				pattern ^= mask;
				// Find the pattern with fewest bits differing
				if (int hammingDist = BitHacks::CountBitsSet((bits[bitsIndex] ^ mask) ^ pattern);
					hammingDist < fi.hammingDistance) {
					fi.mask = mask; // store the used mask to discriminate between types/models
					fi.data = pattern >> 12; // drop the 12 BCH error correction bits
					fi.hammingDistance = hammingDist;
					fi.bitsIndex = bitsIndex;
				}
			}
	};

	best(bits, MASKED_PATTERNS, FORMAT_INFO_MASK_RMQR);
	if (Size(subbits)) // TODO probably remove if `sampleRMQR()` done properly
		best(subbits, MASKED_PATTERNS_SUB, FORMAT_INFO_MASK_RMQR_SUB);

	return fi;
}

/**
* @param formatInfoBits1 format info indicator, with mask still applied
* @param formatInfoBits2 second copy of same info; both are checked at the same time to establish best match
*/
FormatInformation FormatInformation::DecodeQR(uint32_t formatInfoBits1, uint32_t formatInfoBits2)
{
	// maks out the 'Dark Module' for mirrored and non-mirrored case (see Figure 25 in ISO/IEC 18004:2015)
	uint32_t mirroredFormatInfoBits2 = MirrorBits(((formatInfoBits2 >> 1) & 0b111111110000000) | (formatInfoBits2 & 0b1111111));
	formatInfoBits2 = ((formatInfoBits2 >> 1) & 0b111111100000000) | (formatInfoBits2 & 0b11111111);
	// Some (Model2) QR codes apparently do not apply the XOR mask. Try with (standard) and without (quirk) masking.
	auto fi = FindBestFormatInfo({FORMAT_INFO_MASK_MODEL2, 0, FORMAT_INFO_MASK_MODEL1},
								 {formatInfoBits1, formatInfoBits2, MirrorBits(formatInfoBits1), mirroredFormatInfoBits2});

	// Use bits 3/4 for error correction, and 0-2 for mask.
	fi.ecLevel = ECLevelFromBits((fi.data >> 3) & 0x03);
	fi.dataMask = static_cast<uint8_t>(fi.data & 0x07);
	fi.isMirrored = fi.bitsIndex > 1;

	return fi;
}

/**
 * @param formatInfoBits format info indicator, with mask still applied
 */
FormatInformation FormatInformation::DecodeMQR(uint32_t formatInfoBits)
{
	auto fi = FindBestFormatInfo({FORMAT_INFO_MASK_MICRO}, {formatInfoBits, MirrorBits(formatInfoBits)});

	constexpr uint8_t BITS_TO_VERSION[] = {1, 2, 2, 3, 3, 4, 4, 4};

	// Bits 2/3/4 contain both error correction level and version, 0/1 contain mask.
	fi.ecLevel = ECLevelFromBits((fi.data >> 2) & 0x07, true);
	fi.dataMask = static_cast<uint8_t>(fi.data & 0x03);
	fi.microVersion = BITS_TO_VERSION[(fi.data >> 2) & 0x07];
	fi.isMirrored = fi.bitsIndex == 1;

	return fi;
}

/**
* @param formatInfoBits1 format info indicator, with mask still applied
* @param formatInfoBits2 second copy of same info; both are checked at the same time to establish best match
*/
FormatInformation FormatInformation::DecodeRMQR(uint32_t formatInfoBits1, uint32_t formatInfoBits2)
{
	FormatInformation fi;
	if (formatInfoBits2)
		fi = FindBestFormatInfoRMQR({formatInfoBits1}, {formatInfoBits2});
	else // TODO probably remove if `sampleRMQR()` done properly
		fi = FindBestFormatInfoRMQR({formatInfoBits1}, {});

	// Bit 6 is error correction (M/H), and bits 0-5 version.
	fi.ecLevel = ECLevelFromBits(((fi.data >> 5) & 1) << 1); // Shift to match QRCode M/H
	fi.dataMask = 4; // ((y / 2) + (x / 3)) % 2 == 0
	fi.microVersion = (fi.data & 0x1F) + 1;
	fi.isMirrored = false; // TODO: implement mirrored format bit reading

	return fi;
}

} // namespace ZXing::QRCode
