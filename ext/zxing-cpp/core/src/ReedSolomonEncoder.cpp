/*
* Copyright 2016 Huy Cuong Nguyen
* Copyright 2016 ZXing authors
*/
// SPDX-License-Identifier: Apache-2.0

#include "ReedSolomonEncoder.h"

#include "GenericGF.h"

#include <algorithm>
#include <iterator>
#include <stdexcept>

namespace ZXing {

ReedSolomonEncoder::ReedSolomonEncoder(const GenericGF& field)
: _field(&field)
{
	_cachedGenerators.push_back(GenericGFPoly(field, { 1 }));
}

const GenericGFPoly&
ReedSolomonEncoder::buildGenerator(int degree)
{
	int cachedGenSize = Size(_cachedGenerators);
	if (degree >= cachedGenSize) {
		GenericGFPoly lastGenerator = _cachedGenerators.back();
		for (int d = cachedGenSize; d <= degree; d++) {
			lastGenerator.multiply(GenericGFPoly(*_field, { 1, _field->exp(d - 1 + _field->generatorBase()) }));
			_cachedGenerators.push_back(lastGenerator);
		}
	}

	return *std::next(_cachedGenerators.begin(), degree);
}

void
ReedSolomonEncoder::encode(std::vector<int>& message, const int numECCodeWords)
{
	if (numECCodeWords == 0 || numECCodeWords >= Size(message))
		throw std::invalid_argument("Invalid number of error correction code words");

	GenericGFPoly info = GenericGFPoly(*_field, std::vector<int>(message.begin(), message.end() - numECCodeWords));
	info.multiplyByMonomial(1, numECCodeWords);
	GenericGFPoly _;
	info.divide(buildGenerator(numECCodeWords), _);
	auto& coefficients = info.coefficients();
	int numZeroCoefficients = numECCodeWords - Size(coefficients);
	std::fill_n(message.end() - numECCodeWords, numZeroCoefficients, 0);
	std::copy(coefficients.begin(), coefficients.end(), message.end() - numECCodeWords + numZeroCoefficients);
}

} // ZXing
