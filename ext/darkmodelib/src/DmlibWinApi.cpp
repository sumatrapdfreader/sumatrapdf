// SPDX-License-Identifier: MPL-2.0

/*
 * Copyright (c) 2025 ozone10
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * This file incorporates work from the win32-darkmode project:
 *  https://github.com/ysc3839/win32-darkmode
 *  which is covered by the MIT License.
 *  See LICENSE-win32-darkmode for more information.
 */

// This file is part of darkmodelib library.


#include "StdAfx.h"

#include "DmlibWinApi.h"

#include <windows.h>

#include <cwchar>

#include "ModuleHelper.h"

#if defined(_DARKMODELIB_USE_SCROLLBAR_FIX) && (_DARKMODELIB_USE_SCROLLBAR_FIX > 0)
namespace dmlib_hook
{
	bool loadOpenNcThemeData(const HMODULE& hUxtheme);
	void fixDarkScrollBar();
}
#endif


enum class IMMERSIVE_HC_CACHE_MODE
{
	IHCM_USE_CACHED_VALUE,
	IHCM_REFRESH
};

// 1903 18362
enum class PreferredAppMode
{
	Default,
	AllowDark,
	ForceDark,
	ForceLight,
	Max
};

#if defined(_DARKMODELIB_ALLOW_OLD_OS) && (_DARKMODELIB_ALLOW_OLD_OS > 0)
static constexpr DWORD g_win10Build1903 = 18362;

enum WINDOWCOMPOSITIONATTRIB
{
	WCA_UNDEFINED = 0,
	WCA_NCRENDERING_ENABLED = 1,
	WCA_NCRENDERING_POLICY = 2,
	WCA_TRANSITIONS_FORCEDISABLED = 3,
	WCA_ALLOW_NCPAINT = 4,
	WCA_CAPTION_BUTTON_BOUNDS = 5,
	WCA_NONCLIENT_RTL_LAYOUT = 6,
	WCA_FORCE_ICONIC_REPRESENTATION = 7,
	WCA_EXTENDED_FRAME_BOUNDS = 8,
	WCA_HAS_ICONIC_BITMAP = 9,
	WCA_THEME_ATTRIBUTES = 10,
	WCA_NCRENDERING_EXILED = 11,
	WCA_NCADORNMENTINFO = 12,
	WCA_EXCLUDED_FROM_LIVEPREVIEW = 13,
	WCA_VIDEO_OVERLAY_ACTIVE = 14,
	WCA_FORCE_ACTIVEWINDOW_APPEARANCE = 15,
	WCA_DISALLOW_PEEK = 16,
	WCA_CLOAK = 17,
	WCA_CLOAKED = 18,
	WCA_ACCENT_POLICY = 19,
	WCA_FREEZE_REPRESENTATION = 20,
	WCA_EVER_UNCLOAKED = 21,
	WCA_VISUAL_OWNER = 22,
	WCA_HOLOGRAPHIC = 23,
	WCA_EXCLUDED_FROM_DDA = 24,
	WCA_PASSIVEUPDATEMODE = 25,
	WCA_USEDARKMODECOLORS = 26,
	WCA_LAST = 27
};

struct WINDOWCOMPOSITIONATTRIBDATA
{
	WINDOWCOMPOSITIONATTRIB Attrib;
	PVOID pvData;
	SIZE_T cbData;
};
#endif

using fnRtlGetNtVersionNumbers = void (WINAPI*)(LPDWORD major, LPDWORD minor, LPDWORD build);
#if defined(_DARKMODELIB_ALLOW_OLD_OS) && (_DARKMODELIB_ALLOW_OLD_OS > 0)
using fnSetWindowCompositionAttribute = auto (WINAPI*)(HWND hWnd, WINDOWCOMPOSITIONATTRIBDATA*) -> BOOL;
#endif

// 1809 17763
#if defined(_DARKMODELIB_ALLOW_OLD_OS) && (_DARKMODELIB_ALLOW_OLD_OS > 0)
using fnShouldAppsUseDarkMode = auto (WINAPI*)() -> bool; // ordinal 132, is not reliable on 1903+
#endif
using fnAllowDarkModeForWindow = auto (WINAPI*)(HWND hWnd, bool allow) -> bool; // ordinal 133
#if defined(_DARKMODELIB_ALLOW_OLD_OS) && (_DARKMODELIB_ALLOW_OLD_OS > 0)
using fnAllowDarkModeForApp = auto (WINAPI*)(bool allow) -> bool; // ordinal 135, in 1809
#endif
using fnFlushMenuThemes = void (WINAPI*)(); // ordinal 136
#if defined(_DARKMODELIB_ALLOW_OLD_OS) && (_DARKMODELIB_ALLOW_OLD_OS > 0)
using fnIsDarkModeAllowedForWindow = auto (WINAPI*)(HWND hWnd) -> bool; // ordinal 137
#endif
using fnRefreshImmersiveColorPolicyState = void (WINAPI*)(); // ordinal 104
using fnGetIsImmersiveColorUsingHighContrast = auto (WINAPI*)(IMMERSIVE_HC_CACHE_MODE mode) -> bool; // ordinal 106

// 1903 18362
using fnSetPreferredAppMode = auto (WINAPI*)(PreferredAppMode appMode)->PreferredAppMode; // ordinal 135, in 1903

#if defined(_DARKMODELIB_ALLOW_OLD_OS) && (_DARKMODELIB_ALLOW_OLD_OS > 0)
static fnSetWindowCompositionAttribute pfSetWindowCompositionAttribute = nullptr;
static fnShouldAppsUseDarkMode pfShouldAppsUseDarkMode = nullptr;
#endif
static fnAllowDarkModeForWindow pfAllowDarkModeForWindow = nullptr;
#if defined(_DARKMODELIB_ALLOW_OLD_OS) && (_DARKMODELIB_ALLOW_OLD_OS > 0)
static fnAllowDarkModeForApp pfAllowDarkModeForApp = nullptr;
#endif
static fnFlushMenuThemes pfFlushMenuThemes = nullptr;
#if defined(_DARKMODELIB_ALLOW_OLD_OS) && (_DARKMODELIB_ALLOW_OLD_OS > 0)
static fnIsDarkModeAllowedForWindow pfIsDarkModeAllowedForWindow = nullptr;
#endif
static fnRefreshImmersiveColorPolicyState pfRefreshImmersiveColorPolicyState = nullptr;
static fnGetIsImmersiveColorUsingHighContrast pfGetIsImmersiveColorUsingHighContrast = nullptr;

// 1903 18362
static fnSetPreferredAppMode pfSetPreferredAppMode = nullptr;

static bool g_darkModeSupported = false;
static bool g_darkModeActive = false;
static DWORD g_buildNumber = 0;

[[nodiscard]] static bool ShouldAppsUseDarkMode() noexcept
{
#if defined(_DARKMODELIB_ALLOW_OLD_OS) && (_DARKMODELIB_ALLOW_OLD_OS > 0)
	if (g_buildNumber < g_win10Build1903)
	{
		if (pfShouldAppsUseDarkMode == nullptr)
		{
			return false;
		}
		return pfShouldAppsUseDarkMode();
	}
	else
#endif
	{
		return true;
	}
}


/**
 * @brief Enables or disables dark mode support for a specific window.
 *
 * @param[in]   hWnd    Window handle to apply dark mode.
 * @param[in]   allow   Whether to allow (`true`) or disallow (`false`) dark mode.
 * @return `true` if successfully applied.
 */
bool dmlib_win32api::AllowDarkModeForWindow(HWND hWnd, bool allow) noexcept
{
	if (g_darkModeSupported && (pfAllowDarkModeForWindow != nullptr))
	{
		return pfAllowDarkModeForWindow(hWnd, allow);
	}
	return false;
}

/**
 * @brief Determines if high contrast mode is currently active.
 *
 * @return `true` if high contrast is enabled via system accessibility settings.
 */
bool dmlib_win32api::IsHighContrast() noexcept
{
	HIGHCONTRASTW highContrast{};
	highContrast.cbSize = sizeof(HIGHCONTRASTW);
	if (::SystemParametersInfoW(SPI_GETHIGHCONTRAST, sizeof(HIGHCONTRASTW), &highContrast, FALSE) == TRUE)
	{
		return (highContrast.dwFlags & HCF_HIGHCONTRASTON) == HCF_HIGHCONTRASTON;
	}
	return false;
}

#if defined(_DARKMODELIB_ALLOW_OLD_OS) && (_DARKMODELIB_ALLOW_OLD_OS > 0)
static void SetTitleBarThemeColor(HWND hWnd, BOOL dark)
{
	if (g_buildNumber < g_win10Build1903)
	{
		::SetPropW(hWnd, L"UseImmersiveDarkModeColors", reinterpret_cast<HANDLE>(static_cast<intptr_t>(dark)));
	}
	else if (pfSetWindowCompositionAttribute != nullptr)
	{
		WINDOWCOMPOSITIONATTRIBDATA data{ WINDOWCOMPOSITIONATTRIB::WCA_USEDARKMODECOLORS, &dark, sizeof(dark) };
		pfSetWindowCompositionAttribute(hWnd, &data);
	}
}

/**
 * @brief Refreshes the title bar theme color for legacy systems.
 *
 * Used only on old Windows 10 systems when `_DARKMODELIB_ALLOW_OLD_OS`
 * is defined with non-zero unsigned value.
 *
 * @param[in] hWnd Handle to the window to update.
 */
void dmlib_win32api::RefreshTitleBarThemeColor(HWND hWnd)
{
	BOOL dark = FALSE;
	if (pfIsDarkModeAllowedForWindow != nullptr && pfShouldAppsUseDarkMode != nullptr)
	{
		if (pfIsDarkModeAllowedForWindow(hWnd) && pfShouldAppsUseDarkMode() && !IsHighContrast())
		{
			dark = TRUE;
		}
	}

	SetTitleBarThemeColor(hWnd, dark);
}
#endif

/**
 * @brief Checks whether a `WM_SETTINGCHANGE` message indicates a color scheme switch.
 *
 * @param[in] lParam LPARAM from a system message.
 * @return `true` if the message signals a theme mode change.
 */
bool dmlib_win32api::IsColorSchemeChangeMessage(LPARAM lParam) noexcept
{
	bool isMsg = false;
	if ((lParam != 0) // NULL
		&& (_wcsicmp(reinterpret_cast<LPCWSTR>(lParam), L"ImmersiveColorSet") == 0))
	{
		isMsg = true;
	}

	if (isMsg)
	{
		if (pfRefreshImmersiveColorPolicyState != nullptr)
		{
			pfRefreshImmersiveColorPolicyState();
		}

		if (pfGetIsImmersiveColorUsingHighContrast != nullptr)
		{
			pfGetIsImmersiveColorUsingHighContrast(IMMERSIVE_HC_CACHE_MODE::IHCM_REFRESH);
		}
	}

	return isMsg;
}

/**
 * @brief Checks whether a message indicates a color scheme switch.
 *
 * Overload that takes uMsg parameter and checks if it is a `WM_SETTINGCHANGE`
 *
 * @param[in]   lParam  LPARAM from a system message.
 * @param[in]   uMsg    System message to check.
 * @return `true` if the message signals a theme mode change.
 */
bool dmlib_win32api::IsColorSchemeChangeMessage(UINT uMsg, LPARAM lParam) noexcept
{
	if (uMsg == WM_SETTINGCHANGE)
	{
		return dmlib_win32api::IsColorSchemeChangeMessage(lParam);
	}
	return false;
}

static void AllowDarkModeForApp(bool allow) noexcept
{
	if (pfSetPreferredAppMode != nullptr)
	{
		pfSetPreferredAppMode(allow ? PreferredAppMode::ForceDark : PreferredAppMode::Default);
	}
#if defined(_DARKMODELIB_ALLOW_OLD_OS) && (_DARKMODELIB_ALLOW_OLD_OS > 0)
	else if (pfAllowDarkModeForApp != nullptr)
	{
		pfAllowDarkModeForApp(allow);
	}
#endif
}

static void FlushMenuThemes() noexcept
{
	if (pfFlushMenuThemes != nullptr)
	{
		pfFlushMenuThemes();
	}
}

#if defined(_DARKMODELIB_ALLOW_OLD_OS) && (_DARKMODELIB_ALLOW_OLD_OS > 0)
static constexpr DWORD g_win10Build = 17763;
#else
static constexpr DWORD g_win10Build = 19044; // 21H2 latest LTSC, 22H2 19045 latest GA
#endif
static constexpr DWORD g_win11Build = 22000;

/**
 * @brief Checks if the host OS is at least Windows 10.
 *
 * @return `true` if running on Windows 10 or newer.
 */
bool dmlib_win32api::IsWindows10() noexcept
{
	return (g_buildNumber >= g_win10Build);
}

/**
 * @brief Checks if the host OS is at least Windows 11.
 *
 * @return `true` if running on Windows 11 or newer.
 */
bool dmlib_win32api::IsWindows11() noexcept
{
	return (g_buildNumber >= g_win11Build);
}

[[nodiscard]] static bool CheckBuildNumber(DWORD buildNumber) noexcept
{
#if defined(_DARKMODELIB_ALLOW_OLD_OS) && (_DARKMODELIB_ALLOW_OLD_OS > 0)
	static constexpr size_t nWin10Builds = 8;
	// Windows 10 builds { 1809, 1903, 1909, 2004, 20H2, 21H1, 21H2, 22H2 }
	static constexpr DWORD win10Builds[nWin10Builds] = { 17763, 18362, 18363, 19041, 19042, 19043, 19044, 19045 };

	// Windows 10 any version >= 22H2 and Windows 11
	if ((buildNumber >= win10Builds[nWin10Builds - 1])) // || buildNumber > g_win11Build
	{
		return true;
	}

	for (size_t i = 0; i < nWin10Builds; ++i)
	{
		if (buildNumber == win10Builds[i])
		{
			return true;
		}
	}
	return false;
#else
	return (buildNumber >= g_win10Build); // || buildNumber > g_win11Build
#endif
}


/**
 * @brief Retrieves the current Windows build number.
 *
 * @return Windows build number reported by the system.
 */
DWORD dmlib_win32api::GetWindowsBuildNumber() noexcept
{
	return g_buildNumber;
}

/**
 * @brief Checks if dark mode API is supported.
 *
 * @return `true` if dark mode API is supported.
 */
bool dmlib_win32api::IsDarkModeSupported() noexcept
{
	return g_darkModeSupported;
}

/**
 * @brief Checks if dark mode is active.
 *
 * @return `true` if dark mode is active.
 */
bool dmlib_win32api::IsDarkModeActive() noexcept
{
	return g_darkModeActive;
}

/**
 * @brief Initializes undocumented dark mode API.
 */
void dmlib_win32api::InitDarkMode() noexcept
{
	static bool isInit = false;
	if (isInit)
	{
		return;
	}

	fnRtlGetNtVersionNumbers RtlGetNtVersionNumbers = nullptr;
	
	if (HMODULE hNtdll = ::GetModuleHandleW(L"ntdll.dll");
		hNtdll == nullptr
		|| !dmlib_module::LoadFn(hNtdll, RtlGetNtVersionNumbers, "RtlGetNtVersionNumbers"))
	{
		return;
	}

	DWORD major = 0;
	DWORD minor = 0;
	RtlGetNtVersionNumbers(&major, &minor, &g_buildNumber);
	g_buildNumber &= ~0xF0000000;
	if (major != 10 || minor != 0 || !CheckBuildNumber(g_buildNumber))
	{
		return;
	}

	if (const dmlib_module::ModuleHandle moduleUxtheme(L"uxtheme.dll");
		moduleUxtheme.isLoaded())
	{
		const HMODULE& hUxtheme = moduleUxtheme.get();

		bool ptrFnOrd135NotNullptr = false;
#if defined(_DARKMODELIB_ALLOW_OLD_OS) && (_DARKMODELIB_ALLOW_OLD_OS > 0)
		bool ptrFnOrd132NotNullptr = true;
		if (g_buildNumber < g_win10Build1903)
		{
			ptrFnOrd132NotNullptr = LoadFn(hUxtheme, pfShouldAppsUseDarkMode, 132);
			ptrFnOrd135NotNullptr = LoadFn(hUxtheme, pfAllowDarkModeForApp, 135);
		}
		else
#endif
		{
			ptrFnOrd135NotNullptr = dmlib_module::LoadFn(hUxtheme, pfSetPreferredAppMode, 135);
		}

		if (ptrFnOrd135NotNullptr
#if defined(_DARKMODELIB_ALLOW_OLD_OS) && (_DARKMODELIB_ALLOW_OLD_OS > 0)
			&& ptrFnOrd132NotNullptr
#endif
#if defined(_DARKMODELIB_USE_SCROLLBAR_FIX) && (_DARKMODELIB_USE_SCROLLBAR_FIX > 0)
			&& dmlib_hook::loadOpenNcThemeData(hUxtheme)
#endif
			&& dmlib_module::LoadFn(hUxtheme, pfRefreshImmersiveColorPolicyState, 104)
			&& dmlib_module::LoadFn(hUxtheme, pfAllowDarkModeForWindow, 133)
			&& dmlib_module::LoadFn(hUxtheme, pfFlushMenuThemes, 136))
		{
			g_darkModeSupported = true;
		}

		dmlib_module::LoadFn(hUxtheme, pfGetIsImmersiveColorUsingHighContrast, 106);
#if defined(_DARKMODELIB_ALLOW_OLD_OS) && (_DARKMODELIB_ALLOW_OLD_OS > 0)
		if (static constexpr DWORD build2004 = 19041;
			g_buildNumber < build2004
			&& g_darkModeSupported
			&& dmlib_module::LoadFn(hUxtheme, pfIsDarkModeAllowedForWindow, 137))
		{
			if (HMODULE hUser32 = ::GetModuleHandleW(L"user32.dll");
				hUser32 != nullptr)
			{
				dmlib_module::LoadFn(hUser32, pfSetWindowCompositionAttribute, "SetWindowCompositionAttribute");
			}
		}
#endif
		isInit = true;
	}
}

/**
 * @brief Enables or disables dark mode using undocumented API.
 *
 * Optionally applies a scroll bar fix for dark mode inconsistencies.
 *
 * @param[in]   useDark             Enable dark mode when `true`, disable when `false`.
 * @param[in]   applyScrollBarFix   Apply scroll bar fix if `true`.
 */
void dmlib_win32api::SetDarkMode(bool useDark, [[maybe_unused]] bool applyScrollBarFix) noexcept
{
	if (g_darkModeSupported)
	{
		AllowDarkModeForApp(useDark);
		FlushMenuThemes();
#if defined(_DARKMODELIB_USE_SCROLLBAR_FIX) && (_DARKMODELIB_USE_SCROLLBAR_FIX > 0)
		if (applyScrollBarFix)
		{
			dmlib_hook::fixDarkScrollBar();
		}
#endif
		g_darkModeActive = useDark && ShouldAppsUseDarkMode() && !dmlib_win32api::IsHighContrast();
	}
}
