/*
* Copyright 2016 Nu-book Inc.
* Copyright 2016 ZXing authors
*/
// SPDX-License-Identifier: Apache-2.0

#include "GenericGF.h"

namespace ZXing {

const GenericGF &
GenericGF::AztecData12()
{
	static GenericGF inst(0x1069, 4096, 1); // x^12 + x^6 + x^5 + x^3 + 1
	return inst;
}

const GenericGF &
GenericGF::AztecData10()
{
	static GenericGF inst(0x409, 1024, 1); // x^10 + x^3 + 1
	return inst;
}

const GenericGF &
GenericGF::AztecData6()
{
	static GenericGF inst(0x43, 64, 1); // x^6 + x + 1
	return inst;
}

const GenericGF &
GenericGF::AztecParam()
{
	static GenericGF inst(0x13, 16, 1); // x^4 + x + 1
	return inst;
}

const GenericGF &
GenericGF::QRCodeField256()
{
	static GenericGF inst(0x011D, 256, 0); // x^8 + x^4 + x^3 + x^2 + 1
	return inst;
}

const GenericGF &
GenericGF::DataMatrixField256()
{
	static GenericGF inst(0x012D, 256, 1); // x^8 + x^5 + x^3 + x^2 + 1
	return inst;
}

const GenericGF &
GenericGF::AztecData8()
{
	static const GenericGF inst(0x012D, 256, 1); // = DATA_MATRIX_FIELD_256;
	return inst;
}

const GenericGF &
GenericGF::MaxiCodeField64()
{
	static const GenericGF inst(0x43, 64, 1); // = AZTEC_DATA_6;
	return inst;
}


/**
* Create a representation of GF(size) using the given primitive polynomial.
*
* @param primitive irreducible polynomial whose coefficients are represented by
*  the bits of an int, where the least-significant bit represents the constant
*  coefficient
* @param size the size of the field
* @param b the factor b in the generator polynomial can be 0- or 1-based
*  (g(x) = (x+a^b)(x+a^(b+1))...(x+a^(b+2t-1))).
*  In most cases it should be 1, but for QR code it is 0.
*/
GenericGF::GenericGF(int primitive, int size, int b) :
	_size(size),
	_generatorBase(b)
{
#ifdef ZX_REED_SOLOMON_USE_MORE_MEMORY_FOR_SPEED
	_expTable.resize(size * 2, 0);
#else
	_expTable.resize(size, 0);
#endif
	_logTable.resize(size, 0);
	int x = 1;
	for (int i = 0; i < size; ++i) {
		_expTable[i] = x;
		x *= 2; // we're assuming the generator alpha is 2
		if (x >= size) {
			x ^= primitive;
			x &= size - 1;
		}
	}

#ifdef ZX_REED_SOLOMON_USE_MORE_MEMORY_FOR_SPEED
	for (int i = size - 1; i < size * 2; ++i)
		_expTable[i] = _expTable[i - (size - 1)];
#endif

	for (int i = 0; i < size - 1; ++i)
		_logTable[_expTable[i]] = i;
	// logTable[0] == 0 but this should never be used
}

} // namespace ZXing
