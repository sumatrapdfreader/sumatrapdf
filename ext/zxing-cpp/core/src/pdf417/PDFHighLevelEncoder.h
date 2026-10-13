/*
* Copyright 2016 Huy Cuong Nguyen
* Copyright 2016 ZXing authors
* Copyright 2006 Jeremias Maerki
*/
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "CharacterSet.h"

#include <string>
#include <vector>

namespace ZXing {

namespace Pdf417 {

enum class Compaction;

/**
* PDF417 high-level encoder following the algorithm described in ISO/IEC 15438:2001(E) in
* annex P.
*/
class HighLevelEncoder
{
public:
	static std::vector<int> EncodeHighLevel(const std::wstring& msg, Compaction compaction, CharacterSet encoding);
};

} // Pdf417
} // ZXing
