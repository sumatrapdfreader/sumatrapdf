/*
 * Copyright 2022 Axel Waggershauser
 */
// SPDX-License-Identifier: Apache-2.0

#include "Error.h"

namespace ZXing {

std::string Error::location() const
{
	if (!_file)
		return {};
	std::string file(_file);
	return file.substr(file.find_last_of("/\\") + 1) + ":" + std::to_string(_line);
}

std::string ToString(const Error& e)
{
	const char* name[] = {"", "FormatError", "ChecksumError", "Unsupported"};
	std::string ret = name[static_cast<int>(e.type())];
	if (!e.msg().empty())
		ret += " (" + e.msg() + ")";
	if (auto location = e.location(); !location.empty())
		ret += " @ " + e.location();
	return ret;
}

}
