/*
* Copyright 2016 Nu-book Inc.
* Copyright 2016 ZXing authors
* Copyright 2023 Axel Waggershauser
*/
// SPDX-License-Identifier: Apache-2.0

#include "DMDataBlock.h"

#include "DMVersion.h"
#include "ZXAlgorithms.h"

#include <vector>

namespace ZXing::DataMatrix {

std::vector<DataBlock> GetDataBlocks(const ByteArray& rawCodewords, const Version& version, bool fix259)
{
	// First count the total number of data blocks
	// Now establish DataBlocks of the appropriate size and number of data codewords
	auto& ecBlocks = version.ecBlocks;
	const int numResultBlocks = ecBlocks.numBlocks();
	std::vector<DataBlock> result;
	result.reserve(numResultBlocks);
	for (auto& ecBlock : ecBlocks.blocks)
		for (int i = 0; i < ecBlock.count; i++)
			result.push_back({ecBlock.dataCodewords, ByteArray(ecBlocks.codewordsPerBlock + ecBlock.dataCodewords)});

	// All blocks have the same amount of data, except that the last n
	// (where n may be 0) have 1 less byte. Figure out where these start.
	// TODO(bbrown): There is only one case where there is a difference for Data Matrix for size 144
	const int numCodewords = Size(result[0].codewords);
	const int numDataCodewords = numCodewords - ecBlocks.codewordsPerBlock;

	// The last elements of result may be 1 element shorter for 144 matrix
	// first fill out as many elements as all of them have minus 1
	int rawCodewordsOffset = 0;
	for (int i = 0; i < numDataCodewords - 1; i++)
		for (int j = 0; j < numResultBlocks; j++)
			result[j].codewords[i] = rawCodewords[rawCodewordsOffset++];

	// Fill out the last data block in the longer ones
	const bool size144x144 = version.symbolHeight == 144;
	const int numLongerBlocks = size144x144 ? 8 : numResultBlocks;
	for (int j = 0; j < numLongerBlocks; j++)
		result[j].codewords[numDataCodewords - 1] = rawCodewords[rawCodewordsOffset++];

	// Now add in error correction blocks
	for (int i = numDataCodewords; i < numCodewords; i++) {
		for (int j = 0; j < numResultBlocks; j++) {
			int jOffset = size144x144 && fix259 ? (j + 8) % numResultBlocks : j;
			int iOffset = size144x144 && jOffset > 7 ? i - 1 : i;
			result[jOffset].codewords[iOffset] = rawCodewords[rawCodewordsOffset++];
		}
	}

	if (rawCodewordsOffset != Size(rawCodewords))
		return {};

	return result;
}

} // namespace ZXing::DataMatrix
