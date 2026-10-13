/*
* Copyright 2016 Nu-book Inc.
* Copyright 2016 ZXing authors
*/
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <array>

namespace ZXing::QRCode {

/**
* <p>Encapsulates the parameters for one error-correction block in one symbol version.
* This includes the number of data codewords, and the number of times a block with these
* parameters is used consecutively in the QR code version's format.</p>
*
* @author Sean Owen
*/
struct ECB
{
	int count;
	int dataCodewords;
};

/**
* <p>Encapsulates a set of error-correction blocks in one symbol version. Most versions will
* use blocks of differing sizes within one version, so, this encapsulates the parameters for
* each set of blocks. It also holds the number of error-correction codewords per block since it
* will be the same across all blocks within one version.</p>
*
* @author Sean Owen
*/
struct ECBlocks
{
	int codewordsPerBlock;
	std::array<ECB, 2> blocks;

	int numBlocks() const { return blocks[0].count + blocks[1].count; }

	int totalCodewords() const { return codewordsPerBlock * numBlocks(); }

	int totalDataCodewords() const
	{
		return blocks[0].count * (blocks[0].dataCodewords + codewordsPerBlock)
			   + blocks[1].count * (blocks[1].dataCodewords + codewordsPerBlock);
	}

	const std::array<ECB, 2>& blockArray() const { return blocks; }
};

} // namespace ZXing::QRCode
