/*
* Copyright 2016 Huy Cuong Nguyen
* Copyright 2016 ZXing authors
*/
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <string>

namespace ZXing {

class BitMatrix;

namespace OneD {

/**
* This object renders an UPC-E code as a {@link BitMatrix}.
*
* @author 0979097955s@gmail.com (RX)
*/
class UPCEWriter
{
public:
	UPCEWriter& setMargin(int sidesMargin) { _sidesMargin = sidesMargin; return *this; }
	BitMatrix encode(const std::wstring& contents, int width, int height) const;
	BitMatrix encode(const std::string& contents, int width, int height) const;

private:
	int _sidesMargin = -1;
};

} // OneD
} // ZXing
