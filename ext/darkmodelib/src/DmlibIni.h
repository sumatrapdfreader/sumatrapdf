// SPDX-License-Identifier: MPL-2.0

/*
 * Copyright (c) 2025 ozone10
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

// This file is part of darkmodelib library.


#pragma once

#include <windows.h>

#include <string>

namespace dmlib_ini
{
	/// Constructs a full path to an `.ini` file located next to the executable.
	[[nodiscard]] std::wstring getIniPath(const std::wstring& iniFilename);
	/// Checks whether a file exists at the specified path.
	[[nodiscard]] bool fileExists(const std::wstring& filePath) noexcept;
	/// Reads a color value from an `.ini` file and converts it to a `COLORREF`.
	bool setClrFromIni(const std::wstring& iniFilePath, const std::wstring& sectionName, const std::wstring& keyName, COLORREF* clr);
} // namespace dmlib_ini
