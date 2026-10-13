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
* This object renders an EAN8 code as a {@link BitMatrix}.
*
* @author aripollak@gmail.com (Ari Pollak)
*/
class EAN13Writer
{
public:
	EAN13Writer& setMargin(int sidesMargin) { _sidesMargin = sidesMargin; return *this; }
	BitMatrix encode(const std::wstring& contents, int width, int height) const;
	BitMatrix encode(const std::string& contents, int width, int height) const;

private:
	int _sidesMargin = -1;
};

} // OneD
} // ZXing
