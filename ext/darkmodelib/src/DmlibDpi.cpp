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


#include "StdAfx.h"

#include "DmlibDpi.h"

#include <windows.h>

#include <commctrl.h>
#include <uxtheme.h>

#include "ModuleHelper.h"

using fnGetDpiForSystem = auto (WINAPI*)(VOID) -> UINT;
using fnGetDpiForWindow = auto (WINAPI*)(HWND hwnd) -> UINT;
using fnGetSystemMetricsForDpi = auto (WINAPI*)(int nIndex, UINT dpi) -> int;
using fnSystemParametersInfoForDpi = auto (WINAPI*)(UINT uiAction, UINT uiParam, PVOID pvParam, UINT fWinIni, UINT dpi) -> BOOL;
using fnIsValidDpiAwarenessContext = auto (WINAPI*)(DPI_AWARENESS_CONTEXT value) -> BOOL;
using fnSetThreadDpiAwarenessContext = auto (WINAPI*)(DPI_AWARENESS_CONTEXT dpiContext) -> DPI_AWARENESS_CONTEXT;

using fnOpenThemeDataForDpi = auto (WINAPI*)(HWND hwnd, LPCWSTR pszClassList, UINT dpi) -> HTHEME;

extern "C"
{
	static UINT WINAPI DummyGetDpiForSystem() noexcept
	{
		UINT dpi = USER_DEFAULT_SCREEN_DPI;
		if (HDC hdc = ::GetDC(nullptr); hdc != nullptr)
		{
			dpi = static_cast<UINT>(::GetDeviceCaps(hdc, LOGPIXELSX));
			::ReleaseDC(nullptr, hdc);
		}
		return dpi;
	}

	static UINT WINAPI DummyGetDpiForWindow([[maybe_unused]] HWND hwnd) noexcept
	{
		return DummyGetDpiForSystem();
	}

	static int WINAPI DummyGetSystemMetricsForDpi(int nIndex, UINT dpi) noexcept
	{
		return dmlib_dpi::scale(::GetSystemMetrics(nIndex), dpi);
	}

	static BOOL WINAPI DummySystemParametersInfoForDpi(UINT uiAction, UINT uiParam, PVOID pvParam, UINT fWinIni, [[maybe_unused]] UINT dpi) noexcept
	{
		return ::SystemParametersInfoW(uiAction, uiParam, pvParam, fWinIni);
	}

	[[nodiscard]] static BOOL WINAPI DummyIsValidDpiAwarenessContext([[maybe_unused]] DPI_AWARENESS_CONTEXT value) noexcept
	{
		return FALSE;
	}

	static DPI_AWARENESS_CONTEXT WINAPI DummySetThreadDpiAwarenessContext([[maybe_unused]] DPI_AWARENESS_CONTEXT dpiContext) noexcept
	{
		return nullptr;
	}

	static HTHEME WINAPI DummyOpenThemeDataForDpi(HWND hwnd, LPCWSTR pszClassList, [[maybe_unused]] UINT dpi) noexcept
	{
		return ::OpenThemeData(hwnd, pszClassList);
	}
}

static fnGetDpiForSystem pfGetDpiForSystem = DummyGetDpiForSystem;
static fnGetDpiForWindow pfGetDpiForWindow = DummyGetDpiForWindow;
static fnGetSystemMetricsForDpi pfGetSystemMetricsForDpi = DummyGetSystemMetricsForDpi;
static fnSystemParametersInfoForDpi pfSystemParametersInfoForDpi = DummySystemParametersInfoForDpi;
static fnIsValidDpiAwarenessContext pfIsValidDpiAwarenessContext = DummyIsValidDpiAwarenessContext;
static fnSetThreadDpiAwarenessContext pfSetThreadDpiAwarenessContext = DummySetThreadDpiAwarenessContext;
static fnOpenThemeDataForDpi pfOpenThemeDataForDpi = DummyOpenThemeDataForDpi;

bool dmlib_dpi::InitDpiAPI() noexcept
{
	if (HMODULE hUser32 = ::GetModuleHandleW(L"user32.dll"); hUser32 != nullptr)
	{
		if (const auto moduleUxtheme = dmlib_module::ModuleHandle{ L"uxtheme.dll" }; moduleUxtheme.isLoaded())
		{
			bool allLoaded = true;

			allLoaded &= dmlib_module::LoadFn(hUser32, pfGetDpiForSystem, "GetDpiForSystem", DummyGetDpiForSystem);
			allLoaded &= dmlib_module::LoadFn(hUser32, pfGetDpiForWindow, "GetDpiForWindow", DummyGetDpiForWindow);
			allLoaded &= dmlib_module::LoadFn(hUser32, pfGetSystemMetricsForDpi, "GetSystemMetricsForDpi", DummyGetSystemMetricsForDpi);
			allLoaded &= dmlib_module::LoadFn(hUser32, pfSystemParametersInfoForDpi, "SystemParametersInfoForDpi", DummySystemParametersInfoForDpi);
			allLoaded &= dmlib_module::LoadFn(hUser32, pfIsValidDpiAwarenessContext, "IsValidDpiAwarenessContext", DummyIsValidDpiAwarenessContext);
			allLoaded &= dmlib_module::LoadFn(hUser32, pfSetThreadDpiAwarenessContext, "SetThreadDpiAwarenessContext", DummySetThreadDpiAwarenessContext);
			allLoaded &= dmlib_module::LoadFn(moduleUxtheme.get(), pfOpenThemeDataForDpi, "OpenThemeDataForDpi", DummyOpenThemeDataForDpi);

			return allLoaded;
		}
	}
	return false;
}

UINT dmlib_dpi::GetDpiForSystem() noexcept
{
	return pfGetDpiForSystem();
}

UINT dmlib_dpi::GetDpiForWindow(HWND hWnd) noexcept
{
	if (hWnd != nullptr)
	{
		const auto dpi = pfGetDpiForWindow(hWnd);
		if (dpi > 0)
		{
			return dpi;
		}
	}
	return dmlib_dpi::GetDpiForSystem();
}

int dmlib_dpi::GetSystemMetricsForDpi(int nIndex, UINT dpi) noexcept
{
	return pfGetSystemMetricsForDpi(nIndex, dpi);
}

LOGFONT dmlib_dpi::getSysFontForDpi(UINT dpi, FontType type) noexcept
{
	LOGFONT lf{};
	NONCLIENTMETRICS ncm{};
	ncm.cbSize = sizeof(NONCLIENTMETRICS);

	if (pfSystemParametersInfoForDpi(SPI_GETNONCLIENTMETRICS, sizeof(NONCLIENTMETRICS), &ncm, 0, dpi) == TRUE)
	{
		switch (type)
		{
			case FontType::menu:
			{
				lf = ncm.lfMenuFont;
				break;
			}

			case FontType::status:
			{
				lf = ncm.lfStatusFont;
				break;
			}

			case FontType::message:
			{
				lf = ncm.lfMessageFont;
				break;
			}

			case FontType::caption:
			{
				lf = ncm.lfCaptionFont;
				break;
			}

			case FontType::smcaption:
			{
				lf = ncm.lfSmCaptionFont;
				break;
			}
		}
	}
	else // should not happen, fallback
	{
		auto hf = static_cast<HFONT>(::GetStockObject(DEFAULT_GUI_FONT));
		::GetObjectW(hf, sizeof(LOGFONT), &lf);
		lf.lfHeight = scaleFontForDpi(lf.lfHeight, dpi);
	}

	return lf;
}

BOOL dmlib_dpi::IsValidDpiAwarenessContext(DPI_AWARENESS_CONTEXT value) noexcept
{
	return pfIsValidDpiAwarenessContext(value);
}

DPI_AWARENESS_CONTEXT dmlib_dpi::SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT dpiContext) noexcept
{
	return pfSetThreadDpiAwarenessContext(dpiContext);
}

void dmlib_dpi::loadIcon(HINSTANCE hinst, const wchar_t* pszName, int cx, int cy, HICON& hicon) noexcept
{
	if (::LoadIconWithScaleDown(hinst, pszName, cx, cy, &hicon) != S_OK)
	{
		hicon = static_cast<HICON>(::LoadImageW(hinst, pszName, IMAGE_ICON, cx, cy, LR_DEFAULTCOLOR));
	}
}

HTHEME dmlib_dpi::OpenThemeDataForDpi(HWND hwnd, LPCWSTR pszClassList, UINT dpi) noexcept
{
	return pfOpenThemeDataForDpi(hwnd, pszClassList, dpi);
}

/**
 * @brief Get text scale factor from the Windows registry.
 *
 * Queries `HKEY_CURRENT_USER\\Software\\Microsoft\\Accessibility\\TextScaleFactor`.
 *
 * @return DWORD value 100 if there is no key or TextScaleFactor value.
 */
DWORD dmlib_dpi::getTextScaleFactor() noexcept
{
	static constexpr DWORD defaultVal = 100;
	DWORD data = defaultVal;
	DWORD dwBufSize = sizeof(data);
	static constexpr LPCWSTR lpSubKey = L"Software\\Microsoft\\Accessibility";
	static constexpr LPCWSTR lpValue = L"TextScaleFactor";

	if (::RegGetValueW(HKEY_CURRENT_USER, lpSubKey, lpValue, RRF_RT_REG_DWORD, nullptr, &data, &dwBufSize) == ERROR_SUCCESS)
	{
		return data;
	}
	return defaultVal;
}
