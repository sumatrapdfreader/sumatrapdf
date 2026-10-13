/*
* Copyright 2016 Nu-book Inc.
* Copyright 2016 ZXing authors
*/
// SPDX-License-Identifier: Apache-2.0

#include "QRDataBlock.h"

#include "QRErrorCorrectionLevel.h"
#include "QRVersion.h"
#include "ZXAlgorithms.h"

namespace ZXing::QRCode {

std::vector<DataBlock> DataBlock::GetDataBlocks(const ByteArray& rawCodewords, const Version& version, ErrorCorrectionLevel ecLevel)
{
	if (Size(rawCodewords) != version.totalCodewords())
		return {};

	// Figure out the number and size of data blocks used by this version and
	// error correction level
	auto& ecBlocks = version.ecBlocksForLevel(ecLevel);

	// First count the total number of data blocks
	int totalBlocks = ecBlocks.numBlocks();
	if (totalBlocks == 0)
		return {};

	std::vector<DataBlock> result(totalBlocks);
	// Now establish DataBlocks of the appropriate size and number of data codewords
	int numResultBlocks = 0;
	for (auto& ecBlock : ecBlocks.blockArray()) {
		for (int i = 0; i < ecBlock.count; i++) {
			auto& item = result[numResultBlocks++];
			item._numDataCodewords = ecBlock.dataCodewords;
			item._codewords.resize(ecBlocks.codewordsPerBlock + ecBlock.dataCodewords);
		}
	}

	// All blocks have the same amount of data, except that the last n
	// (where n may be 0) have 1 more byte. Figure out where these start.
	int shorterBlocksTotalCodewords = Size(result[0]._codewords);
	int longerBlocksStartAt = Size(result) - 1;
	while (longerBlocksStartAt >= 0) {
		int numCodewords = Size(result[longerBlocksStartAt]._codewords);
		if (numCodewords == shorterBlocksTotalCodewords) {
			break;
		}
		longerBlocksStartAt--;
	}
	longerBlocksStartAt++;

	int shorterBlocksNumDataCodewords = shorterBlocksTotalCodewords - ecBlocks.codewordsPerBlock;
	// The last elements of result may be 1 element longer;
	// first fill out as many elements as all of them have
	int rawCodewordsOffset = 0;
	for (int i = 0; i < shorterBlocksNumDataCodewords; i++) {
		for (int j = 0; j < numResultBlocks; j++) {
			result[j]._codewords[i] = rawCodewords[rawCodewordsOffset++];
		}
	}
	// Fill out the last data block in the longer ones
	for (int j = longerBlocksStartAt; j < numResultBlocks; j++) {
		result[j]._codewords[shorterBlocksNumDataCodewords] = rawCodewords[rawCodewordsOffset++];
	}
	// Now add in error correction blocks
	int max = Size(result[0]._codewords);
	for (int i = shorterBlocksNumDataCodewords; i < max; i++) {
		for (int j = 0; j < numResultBlocks; j++) {
			int iOffset = j < longerBlocksStartAt ? i : i + 1;
			result[j]._codewords[iOffset] = rawCodewords[rawCodewordsOffset++];
		}
	}
	return result;
}

} // namespace ZXing::QRCode
