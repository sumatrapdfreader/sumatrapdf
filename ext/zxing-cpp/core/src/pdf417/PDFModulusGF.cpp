/*
* Copyright 2016 Nu-book Inc.
* Copyright 2016 ZXing authors
*/
// SPDX-License-Identifier: Apache-2.0

#include "PDFModulusGF.h"
#include <stdexcept>

namespace ZXing {
namespace Pdf417 {

ModulusGF::ModulusGF(int modulus, int generator) :
	_modulus(modulus),
	_zero(*this, { 0 }),
	_one(*this, { 1 })
{
#ifdef ZX_REED_SOLOMON_USE_MORE_MEMORY_FOR_SPEED
	_expTable.resize(modulus * 2, 0);
#else
	_expTable.resize(modulus, 0);
#endif
	_logTable.resize(modulus, 0);
	int x = 1;
	for (int i = 0; i < modulus; i++) {
		_expTable[i] = x;
		x = (x * generator) % modulus;
	}

#ifdef ZX_REED_SOLOMON_USE_MORE_MEMORY_FOR_SPEED
	for (int i = modulus - 1; i < modulus * 2; ++i)
		_expTable[i] = _expTable[i - (modulus - 1)];
#endif

	for (int i = 0; i < modulus - 1; i++) {
		_logTable[_expTable[i]] = i;
	}
	// logTable[0] == 0 but this should never be used
}

ModulusPoly
ModulusGF::buildMonomial(int degree, int coefficient) const
{
	if (degree < 0) {
		throw std::invalid_argument("degree < 0");
	}
	if (coefficient == 0) {
		return _zero;
	}
	std::vector<int> coefficients(degree + 1, 0);
	coefficients[0] = coefficient;
	return ModulusPoly(*this, coefficients);
}

} // Pdf417
} // ZXing
