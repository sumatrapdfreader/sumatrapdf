/*
* Copyright 2016 Nu-book Inc.
* Copyright 2016 ZXing authors
*/
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "ByteArray.h"

#include <vector>

namespace ZXing::DataMatrix {

class Version;

/**
* <p>Encapsulates a block of data within a Data Matrix Code. Data Matrix Codes may split their data into
* multiple blocks, each of which is a unit of data and error-correction codewords. Each
* is represented by an instance of this class.</p>
*/
struct DataBlock
{
	const int numDataCodewords = 0;
	ByteArray codewords;
};

/**
 * <p>When Data Matrix Codes use multiple data blocks, they actually interleave the bytes of each of them.
 * That is, the first byte of data block 1 to n is written, then the second bytes, and so on. This
 * method will separate the data into original blocks.</p>
 *
 * @param rawCodewords bytes as read directly from the Data Matrix Code
 * @param version version of the Data Matrix Code
 * @param fix259 see https://github.com/zxing-cpp/zxing-cpp/issues/259
 * @return DataBlocks containing original bytes, "de-interleaved" from representation in the
 *         Data Matrix Code
 */
std::vector<DataBlock> GetDataBlocks(const ByteArray& rawCodewords, const Version& version, bool fix259 = false);

} // namespace ZXing::DataMatrix
