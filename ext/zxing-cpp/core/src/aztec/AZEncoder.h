/*
* Copyright 2016 Huy Cuong Nguyen
* Copyright 2016 ZXing authors
*/
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "BitMatrix.h"

#include <string>

namespace ZXing::Aztec {

/**
* Aztec 2D code representation
*
* @author Rustam Abdullaev
*/
struct EncodeResult
{
	bool compact;
	int size;
	int layers;
	int codeWords;
	BitMatrix matrix;
};

/**
* Generates Aztec 2D barcodes.
*
* @author Rustam Abdullaev
*/
class Encoder
{
public:
	static const int DEFAULT_EC_PERCENT = 33; // default minimal percentage of error check words
	static const int DEFAULT_AZTEC_LAYERS = 0;
	static const int AZTEC_RUNE_LAYERS = 0xFF;

	static EncodeResult Encode(const std::string& data, int minECCPercent, int userSpecifiedLayers);
};

} // namespace ZXing::Aztec
