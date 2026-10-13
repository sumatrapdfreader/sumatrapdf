/*
* Copyright 2016 Huy Cuong Nguyen
* Copyright 2016 ZXing authors
*/
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "GenericGFPoly.h"

#include <list>
#include <vector>

namespace ZXing {

// public only for testing purposes
class ReedSolomonEncoder
{
public:
	explicit ReedSolomonEncoder(const GenericGF& field);

	void encode(std::vector<int>& message, int numECCodeWords);

private:
	const GenericGF* _field;
	std::list<GenericGFPoly> _cachedGenerators;

	const GenericGFPoly& buildGenerator(int degree);
};

/**
 * @brief ReedSolomonEncode replaces the last numECCodeWords code words in message with error correction code words
 */
inline void ReedSolomonEncode(const GenericGF& field, std::vector<int>& message, int numECCodeWords)
{
	ReedSolomonEncoder(field).encode(message, numECCodeWords);
}

} // namespace ZXing
