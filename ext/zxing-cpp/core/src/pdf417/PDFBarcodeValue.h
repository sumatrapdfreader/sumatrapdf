/*
* Copyright 2016 Nu-book Inc.
* Copyright 2016 ZXing authors
*/
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <map>
#include <vector>

namespace ZXing {
namespace Pdf417 {

/**
* @author Guenther Grau
*/
class BarcodeValue
{
	std::map<int, int> _values;

public:
	/**
	* Add an occurrence of a value
	*/
	void setValue(int value);

	/**
	* Determines the maximum occurrence of a set value and returns all values which were set with this occurrence.
	* @return an array of int, containing the values with the highest occurrence, or null, if no value was set
	*/
	std::vector<int> value() const;

	int confidence(int value) const;
};

} // Pdf417
} // ZXing
