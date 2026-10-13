/*
* Copyright 2021 Axel Waggershauser
*/
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <string>

namespace ZXing {

struct StructuredAppendInfo
{
	int index = -1;
	int count = -1;
	std::string id;
};

} // ZXing
