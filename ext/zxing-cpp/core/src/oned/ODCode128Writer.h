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
* This object renders a CODE128 code as a {@link BitMatrix}.
*
* @author erik.barbara@gmail.com (Erik Barbara)
*/
class Code128Writer
{
public:
	Code128Writer& setMargin(int sidesMargin) { _sidesMargin = sidesMargin; return *this; }
	BitMatrix encode(const std::wstring& contents, int width, int height) const;
	BitMatrix encode(const std::string& contents, int width, int height) const;

private:
	int _sidesMargin = -1;
};

} // OneD
} // ZXing
