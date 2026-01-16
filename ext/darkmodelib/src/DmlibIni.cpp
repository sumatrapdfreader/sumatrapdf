// SPDX-License-Identifier: MPL-2.0

/*
 * Copyright (c) 2025 ozone10
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

// This file is part of darkmodelib library.


#include "StdAfx.h"

#include "DmlibIni.h"

#include <windows.h>

#include <algorithm>
#include <array>
#include <cwchar>
#include <cwctype>
#include <stdexcept>
#include <string>

#include "DmlibColor.h"

/**
 * @brief Constructs a full path to an `.ini` file located next to the executable.
 *
 * Retrieves the directory of the current module (executable or DLL) and appends
 * the specified `.ini` filename to it.
 *
 * @param[in] iniFilename The base name of the `.ini` file (without path or extension).
 * @return Full path to the `.ini` file as a wide string, or an empty string on failure.
 *
 * @note Returns a path like: `C:\\Path\\To\\Executable\\YourFile.ini`
 */
std::wstring dmlib_ini::getIniPath(const std::wstring& iniFilename)
{
	std::array<wchar_t, MAX_PATH> buffer{};
	if (::GetModuleFileNameW(nullptr, buffer.data(), MAX_PATH) == 0)
	{
		return L"";
	}

	wchar_t* lastSlash = std::wcsrchr(buffer.data(), L'\\');
	if (lastSlash == nullptr)
	{
		return L"";
	}

	*lastSlash = L'\0';
	std::wstring iniPath(buffer.data());
	iniPath += L"\\" + iniFilename + L".ini";
	return iniPath;
}

/**
 * @brief Checks whether a file exists at the specified path.
 *
 * Determines if the given file path exists and refers to a regular file.
 *
 * @param[in] filePath Path to the file to check.
 * @return `true` if the file exists and is not a directory, otherwise `false`.
 */
bool dmlib_ini::fileExists(const std::wstring& filePath) noexcept
{
	const DWORD dwAttrib = ::GetFileAttributesW(filePath.c_str());
	return (dwAttrib != INVALID_FILE_ATTRIBUTES && ((dwAttrib & FILE_ATTRIBUTE_DIRECTORY) != FILE_ATTRIBUTE_DIRECTORY));
}

/**
 * @brief Reads a color value from an `.ini` file and converts it to a `COLORREF`.
 *
 * Reads a 6-digit hex color string from the specified section and key, then parses
 * it as a Windows GDI `COLORREF` value.
 *
 * @param[in]   iniFilePath Full path to the `.ini` file.
 * @param[in]   sectionName Section within the `.ini` file.
 * @param[in]   keyName     Key name containing the hex RGB value (e.g., "E0E2E4").
 * @param[out]  clr         Pointer to a `COLORREF` where the parsed color will be stored. **Must not be `nullptr`.**
 * @return `true` if a valid 6-digit hex color was read and parsed, otherwise `false`.
 *
 * @note The value must be exactly 6 hexadecimal digits and represent an RGB color.
 */
bool dmlib_ini::setClrFromIni(
	const std::wstring& iniFilePath,
	const std::wstring& sectionName,
	const std::wstring& keyName,
	COLORREF* clr
)
{
	if (clr == nullptr)
	{
		return false;
	}

	static constexpr size_t maxStrLen = 6;
	std::wstring buffer(maxStrLen + 1, L'\0');

	const auto len = static_cast<size_t>(::GetPrivateProfileStringW(
		sectionName.c_str()
		, keyName.c_str()
		, L""
		, buffer.data()
		, static_cast<DWORD>(buffer.size())
		, iniFilePath.c_str()));

	if (len != maxStrLen)
	{
		return false;
	}

	buffer.resize(len); // remove extra '\0'

	if (!std::all_of(buffer.begin(), buffer.end(), [](wchar_t ch) { return std::iswxdigit(ch); }))
	{
		return false;
	}

	try
	{
		static constexpr int baseHex = 16;
		*clr = dmlib_color::HEXRGB(std::stoul(buffer, nullptr, baseHex));
	}
	catch (const std::invalid_argument&)
	{
		return false;
	}
	catch (const std::out_of_range&)
	{
		return false;
	}

	return true;
}
