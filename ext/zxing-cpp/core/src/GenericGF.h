/*
* Copyright 2016 Nu-book Inc.
* Copyright 2016 ZXing authors
*/
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "GenericGFPoly.h"
#include "ZXConfig.h"

#include <stdexcept>
#include <vector>

namespace ZXing {

/**
* <p>This class contains utility methods for performing mathematical operations over
* the Galois Fields. Operations use a given primitive polynomial in calculations.</p>
*
* <p>Throughout this package, elements of the GF are represented as an {@code int}
* for convenience and speed (but at the cost of memory).
* </p>
*
* @author Sean Owen
* @author David Olivier
*/
class GenericGF
{
	const int _size;
	int _generatorBase;
	std::vector<short> _expTable;
	std::vector<short> _logTable;

	/**
	* Create a representation of GF(size) using the given primitive polynomial.
	*
	* @param primitive irreducible polynomial whose coefficients are represented by
	*  the bits of an int, where the least-significant bit represents the constant
	*  coefficient
	* @param size the size of the field (m = log2(size) is called the word size of the encoding)
	* @param b the factor b in the generator polynomial can be 0- or 1-based
	*  (g(x) = (x+a^b)(x+a^(b+1))...(x+a^(b+2t-1))).
	*  In most cases it should be 1, but for QR code it is 0.
	*/
	GenericGF(int primitive, int size, int b);

public:
	static const GenericGF& AztecData12();
	static const GenericGF& AztecData10();
	static const GenericGF& AztecData6();
	static const GenericGF& AztecParam();
	static const GenericGF& QRCodeField256();
	static const GenericGF& DataMatrixField256();
	static const GenericGF& AztecData8();
	static const GenericGF& MaxiCodeField64();

	// note: replaced addOrSubstract calls with '^' / '^='. everyone trying to understand this code needs to look into
	// Galois Fields with characteristic 2 and will then understand that XOR is addition/subtraction. And those
	// operators are way more readable than a noisy member function name

	/**
	* @return 2 to the power of a in GF(size)
	*/
	int exp(int a) const {
		return _expTable.at(a);
	}

	/**
	* @return base 2 log of a in GF(size)
	*/
	int log(int a) const {
		if (a == 0) {
			throw std::invalid_argument("a == 0");
		}
		return _logTable.at(a);
	}

	/**
	* @return multiplicative inverse of a
	*/
	int inverse(int a) const {
		return _expTable[_size - log(a) - 1];
	}

	/**
	* @return product of a and b in GF(size)
	*/
	int multiply(int a, int b) const noexcept {
		if (a == 0 || b == 0)
			return 0;

#ifdef ZX_REED_SOLOMON_USE_MORE_MEMORY_FOR_SPEED
		return _expTable[_logTable[a] + _logTable[b]];
#else
		auto fast_mod = [](const int input, const int ceil) {
			// avoid using the '%' modulo operator => ReedSolomon computation is more than twice as fast
			// see also https://stackoverflow.com/a/33333636/2088798
			return input < ceil ? input : input - ceil;
		};
		return _expTable[fast_mod(_logTable[a] + _logTable[b], _size - 1)];
#endif
	}

	int size() const noexcept {
		return _size;
	}

	int generatorBase() const noexcept {
		return _generatorBase;
	}
};

} // namespace ZXing
