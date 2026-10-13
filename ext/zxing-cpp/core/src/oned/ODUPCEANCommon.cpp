/*
* Copyright 2016 Huy Cuong Nguyen
* Copyright 2016 ZXing authors
*/
// SPDX-License-Identifier: Apache-2.0

#include "ODUPCEANCommon.h"

namespace ZXing::OneD {

const std::array<int, 3> UPCEANCommon::START_END_PATTERN = { 1, 1, 1 };
const std::array<int, 5> UPCEANCommon::MIDDLE_PATTERN = { 1, 1, 1, 1, 1 };
const std::array<int, 6> UPCEANCommon::UPCE_END_PATTERN = { 1, 1, 1, 1, 1, 1 };

const std::array<std::array<int, 4>, 10> UPCEANCommon::L_PATTERNS = {
	3, 2, 1, 1, // 0
	2, 2, 2, 1, // 1
	2, 1, 2, 2, // 2
	1, 4, 1, 1, // 3
	1, 1, 3, 2, // 4
	1, 2, 3, 1, // 5
	1, 1, 1, 4, // 6
	1, 3, 1, 2, // 7
	1, 2, 1, 3, // 8
	3, 1, 1, 2, // 9
};

const std::array<std::array<int, 4>, 20> UPCEANCommon::L_AND_G_PATTERNS = {
	3, 2, 1, 1, // 0
	2, 2, 2, 1, // 1
	2, 1, 2, 2, // 2
	1, 4, 1, 1, // 3
	1, 1, 3, 2, // 4
	1, 2, 3, 1, // 5
	1, 1, 1, 4, // 6
	1, 3, 1, 2, // 7
	1, 2, 1, 3, // 8
	3, 1, 1, 2, // 9
	// reversed
	1, 1, 2, 3, // 10
	1, 2, 2, 2, // 11
	2, 2, 1, 2, // 12
	1, 1, 4, 1, // 13
	2, 3, 1, 1, // 14
	1, 3, 2, 1, // 15
	4, 1, 1, 1, // 16
	2, 1, 3, 1, // 17
	3, 1, 2, 1, // 18
	2, 1, 1, 3, // 19
};

// For an UPC-E barcode, the final digit is represented by the parities used
// to encode the middle six digits, according to the table below.
//
//                Parity of next 6 digits
//    Digit   0     1     2     3     4     5
//       0    Even   Even  Even Odd  Odd   Odd
//       1    Even   Even  Odd  Even Odd   Odd
//       2    Even   Even  Odd  Odd  Even  Odd
//       3    Even   Even  Odd  Odd  Odd   Even
//       4    Even   Odd   Even Even Odd   Odd
//       5    Even   Odd   Odd  Even Even  Odd
//       6    Even   Odd   Odd  Odd  Even  Even
//       7    Even   Odd   Even Odd  Even  Odd
//       8    Even   Odd   Even Odd  Odd   Even
//       9    Even   Odd   Odd  Even Odd   Even
//
// The encoding is represented by the following array, which is a bit pattern
// using Odd = 0 and Even = 1. For example, 5 is represented by:
//
//              Odd Even Even Odd Odd Even
// in binary:
//                0    1    1   0   0    1   == 0x19
//
const std::array<int, 20> UPCEANCommon::NUMSYS_AND_CHECK_DIGIT_PATTERNS = {
	0x38, 0x34, 0x32, 0x31, 0x2C, 0x26, 0x23, 0x2A, 0x29, 0x25,
	0x07, 0x0B, 0x0D, 0x0E, 0x13, 0x19, 0x1C, 0x15, 0x16, 0x1A,
};

} // namespace ZXing::OneD
