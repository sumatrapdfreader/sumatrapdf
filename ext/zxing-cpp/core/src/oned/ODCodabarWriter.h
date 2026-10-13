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
* This class renders CodaBar as {@code boolean[]}.
*
* @author dsbnatut@gmail.com (Kazuki Nishiura)
*/
class CodabarWriter
{
public:
	CodabarWriter& setMargin(int sidesMargin) { _sidesMargin = sidesMargin; return *this; }
	BitMatrix encode(const std::wstring& contents, int width, int height) const;
	BitMatrix encode(const std::string& contents, int width, int height) const;

private:
	int _sidesMargin = -1;
};

} // OneD
} // ZXing
