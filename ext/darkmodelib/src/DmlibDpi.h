// SPDX-License-Identifier: MPL-2.0

/*
 * Copyright (c) 2025 ozone10
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

// This file is part of darkmodelib library.

// Based on parts of the Notepad++ dpi code licensed under GPLv3.
// Originally by ozone10.


#pragma once

#include <windows.h>

#include <uxtheme.h>

#if defined(__GNUC__) || (WINVER < _WIN32_WINNT_WIN7)
	#ifndef WM_DPICHANGED
		#define WM_DPICHANGED 0x02E0
	#endif

	#ifndef WM_DPICHANGED_BEFOREPARENT
		#define WM_DPICHANGED_BEFOREPARENT 0x02E2
	#endif

	#ifndef WM_DPICHANGED_AFTERPARENT
		#define WM_DPICHANGED_AFTERPARENT 0x02E3
	#endif

	#ifndef WM_GETDPISCALEDSIZE
		#define WM_GETDPISCALEDSIZE 0x02E4
	#endif
#endif

namespace dmlib_dpi
{
	enum class FontType
	{
		menu,
		status,
		message,
		caption,
		smcaption
	};

	inline constexpr UINT kDefaultFontDpi = 72;
	inline constexpr UINT kDefaultFontScaleFactor = 100;

	bool InitDpiAPI() noexcept;

	[[nodiscard]] UINT GetDpiForSystem() noexcept;
	[[nodiscard]] UINT GetDpiForWindow(HWND hWnd) noexcept;
	[[nodiscard]] inline UINT GetDpiForParent(HWND hWnd) noexcept
	{
		return dmlib_dpi::GetDpiForWindow(::GetParent(hWnd));
	}

	[[nodiscard]] int GetSystemMetricsForDpi(int nIndex, UINT dpi) noexcept;

	[[nodiscard]] inline int scale(int x, UINT toDpi, UINT fromDpi) noexcept
	{
		return ::MulDiv(x, static_cast<int>(toDpi), static_cast<int>(fromDpi));
	}

	[[nodiscard]] inline int scale(int x, UINT dpi) noexcept
	{
		return dmlib_dpi::scale(x, dpi, USER_DEFAULT_SCREEN_DPI);
	}

	[[nodiscard]] inline int unscale(int x, UINT dpi) noexcept
	{
		return dmlib_dpi::scale(x, USER_DEFAULT_SCREEN_DPI, dpi);
	}

	[[nodiscard]] inline int scale(int x, HWND hWnd) noexcept
	{
		return dmlib_dpi::scale(x, dmlib_dpi::GetDpiForWindow(hWnd), USER_DEFAULT_SCREEN_DPI);
	}

	[[nodiscard]] inline int unscale(int x, HWND hWnd) noexcept
	{
		return dmlib_dpi::scale(x, USER_DEFAULT_SCREEN_DPI, dmlib_dpi::GetDpiForWindow(hWnd));
	}

	[[nodiscard]] inline int scaleFontForDpi(int pt, UINT dpi) noexcept
	{
		return dmlib_dpi::scale(pt, dpi, kDefaultFontDpi);
	}

	[[nodiscard]] inline int scaleFontForDpi(int pt, HWND hWnd) noexcept
	{
		return dmlib_dpi::scale(pt, dmlib_dpi::GetDpiForWindow(hWnd), kDefaultFontDpi);
	}

	[[nodiscard]] LOGFONT getSysFontForDpi(UINT dpi, FontType type) noexcept;
	[[nodiscard]] inline LOGFONT getSysFontForDpi(HWND hWnd, FontType type) noexcept
	{
		return dmlib_dpi::getSysFontForDpi(dmlib_dpi::GetDpiForWindow(hWnd), type);
	}

	[[nodiscard]] BOOL IsValidDpiAwarenessContext(DPI_AWARENESS_CONTEXT value) noexcept;

	DPI_AWARENESS_CONTEXT SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT dpiContext) noexcept;

	void loadIcon(HINSTANCE hinst, const wchar_t* pszName, int cx, int cy, HICON& hicon) noexcept;

	[[nodiscard]] HTHEME OpenThemeDataForDpi(HWND hwnd, LPCWSTR pszClassList, UINT dpi) noexcept;

	[[nodiscard]] inline HTHEME OpenThemeDataForDpi(HWND hwnd, LPCWSTR pszClassList, HWND hWndDpi) noexcept
	{
		return dmlib_dpi::OpenThemeDataForDpi(hwnd, pszClassList, dmlib_dpi::GetDpiForWindow(hWndDpi));
	}

	/// Get text scale factor from the Windows registry.
	[[nodiscard]] DWORD getTextScaleFactor() noexcept;

	[[nodiscard]] inline int scaleFontForFactor(int pt, UINT textScaleFactor) noexcept
	{
		return dmlib_dpi::scale(pt, textScaleFactor, kDefaultFontScaleFactor);
	}

	[[nodiscard]] inline int scaleFontForFactor(int pt) noexcept
	{
		return dmlib_dpi::scale(pt, dmlib_dpi::getTextScaleFactor(), kDefaultFontScaleFactor);
	}

} // namespace dmlib_dpi
