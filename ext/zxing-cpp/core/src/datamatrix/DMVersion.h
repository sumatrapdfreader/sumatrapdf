/*
* Copyright 2016 Nu-book Inc.
* Copyright 2016 ZXing authors
*/
// SPDX-License-Identifier: Apache-2.0

#pragma once

namespace ZXing::DataMatrix {

/**
 * The Version object encapsulates attributes about a particular Data Matrix Symbol size.
 */
class Version
{
public:
	/**
	 * Encapsulates a set of error-correction blocks in one symbol version. Most versions will
	 * use blocks of differing sizes within one version, so, this encapsulates the parameters for
	 * each set of blocks. It also holds the number of error-correction codewords per block since it
	 * will be the same across all blocks within one version.
	 */
	struct ECBlocks
	{
		int codewordsPerBlock;

		/* Encapsulates the parameters for one error-correction block in one symbol version.
		 * This includes the number of data codewords, and the number of times a block with these
		 * parameters is used consecutively in the Data Matrix code version's format.
		 */
		struct
		{
			int count;
			int dataCodewords;
		} const blocks[2];

		int numBlocks() const { return blocks[0].count + blocks[1].count; }

		int totalDataCodewords() const
		{
			return blocks[0].count * (blocks[0].dataCodewords + codewordsPerBlock) +
				   blocks[1].count * (blocks[1].dataCodewords + codewordsPerBlock);
		}
	};

	const int versionNumber;
	const int symbolHeight;
	const int symbolWidth;
	const int dataBlockHeight;
	const int dataBlockWidth;
	const ECBlocks ecBlocks;

	int totalCodewords() const { return ecBlocks.totalDataCodewords(); }
	int dataWidth() const { return (symbolWidth / dataBlockWidth) * dataBlockWidth; }
	int dataHeight() const { return (symbolHeight / dataBlockHeight) * dataBlockHeight; }
	bool isDMRE() const { return versionNumber >= 31 && versionNumber <= 48; }
};

/**
 * @brief Looks up Version information based on symbol dimensions.
 *
 * @param height Number of rows in modules
 * @param width Number of columns in modules
 * @return Version for a Data Matrix Code of those dimensions, nullputr for invalid dimensions
 */
const Version* VersionForDimensions(int height, int width);

template<typename MAT>
const Version* VersionForDimensionsOf(const MAT& mat)
{
	return VersionForDimensions(mat.height(), mat.width());
}

} // namespace ZXing::DataMatrix
