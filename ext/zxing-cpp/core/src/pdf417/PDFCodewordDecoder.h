/*
* Copyright 2016 Nu-book Inc.
* Copyright 2016 ZXing authors
*/
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <array>

namespace ZXing {
namespace Pdf417 {

/**
* @author Guenther Grau
* @author creatale GmbH (christoph.schulz@creatale.de)
*/
class CodewordDecoder
{
public:
	static constexpr const int NUMBER_OF_CODEWORDS = 929;
	// Maximum Codewords (Data + Error).
	static constexpr const int MAX_CODEWORDS_IN_BARCODE = NUMBER_OF_CODEWORDS - 1;
	// One left row indication column + max 30 data columns + one right row indicator column
	//public static final int MAX_CODEWORDS_IN_ROW = 32;
	static constexpr const int MODULES_IN_CODEWORD = 17;
	static constexpr const int BARS_IN_MODULE = 8;

	/**
	* @param symbol encoded symbol to translate to a codeword
	* @return the codeword corresponding to the symbol.
	*/
	static int GetCodeword(int symbol);

	static int GetDecodedValue(const std::array<int, BARS_IN_MODULE>& moduleBitCount);
};

} // Pdf417
} // ZXing
