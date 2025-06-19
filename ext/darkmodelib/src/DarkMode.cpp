// SPDX-License-Identifier: MIT

// Copyright (c) 2024-2025 ozone10
// MIT license

// This file contains parts of code from the win32-darkmode project
// https://github.com/ysc3839/win32-darkmode
// which is licensed under the MIT License.
// See LICENSE-win32-darkmode for more information.

#include "StdAfx.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef VC_EXTRALEAN
#define VC_EXTRALEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "DarkMode.h"

#include <uxtheme.h>
#include <vsstyle.h>

#include <mutex>
#include <unordered_set>

#if !defined(_DARKMODELIB_EXTERNAL_IATHOOK)
#include "IatHook.h"
#else
extern PIMAGE_THUNK_DATA FindAddressByName(void* moduleBase, PIMAGE_THUNK_DATA impName, PIMAGE_THUNK_DATA impAddr, const char* funcName);
extern PIMAGE_THUNK_DATA FindAddressByOrdinal(void* moduleBase, PIMAGE_THUNK_DATA impName, PIMAGE_THUNK_DATA impAddr, uint16_t ordinal);
extern PIMAGE_THUNK_DATA FindIatThunkInModule(void* moduleBase, const char* dllName, const char* funcName);
extern PIMAGE_THUNK_DATA FindDelayLoadThunkInModule(void* moduleBase, const char* dllName, const char* funcName);
extern PIMAGE_THUNK_DATA FindDelayLoadThunkInModule(void* moduleBase, const char* dllName, uint16_t ordinal);
#endif

#if defined(_MSC_VER) && _MSC_VER >= 1800
#pragma warning(disable : 4191)
#elif defined(__GNUC__)
#include <cwchar>
#endif

template <typename P>
static auto ReplaceFunction(IMAGE_THUNK_DATA* addr, const P& newFunction) -> P
{
	DWORD oldProtect = 0;
	if (VirtualProtect(addr, sizeof(IMAGE_THUNK_DATA), PAGE_READWRITE, &oldProtect) == FALSE)
	{
		return nullptr;
	}

	const uintptr_t oldFunction = addr->u1.Function;
	addr->u1.Function = reinterpret_cast<uintptr_t>(newFunction);
	VirtualProtect(addr, sizeof(IMAGE_THUNK_DATA), oldProtect, &oldProtect);
	return reinterpret_cast<P>(oldFunction);
}

template <typename P>
static auto loadFn(HMODULE handle, P& pointer, const char* name) -> bool
{
	if (auto proc = ::GetProcAddress(handle, name); proc != nullptr)
	{
		pointer = reinterpret_cast<P>(proc);
		return true;
	}
	return false;
}

template <typename P>
static auto loadFn(HMODULE handle, P& pointer, WORD index) -> bool
{
	return loadFn(handle, pointer, MAKEINTRESOURCEA(index));
}

class ModuleHandle
{
public:
	ModuleHandle() = delete;

	explicit ModuleHandle(const wchar_t* moduleName)
		: hModule(LoadLibraryEx(moduleName, nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32))
	{}

	ModuleHandle(const ModuleHandle&) = delete;
	ModuleHandle& operator=(const ModuleHandle&) = delete;

	ModuleHandle(ModuleHandle&&) = delete;
	ModuleHandle& operator=(ModuleHandle&&) = delete;

	~ModuleHandle()
	{
		if (hModule != nullptr)
		{
			FreeLibrary(hModule);
		}
	}

	[[nodiscard]] HMODULE get() const
	{
		return hModule;
	}

	[[nodiscard]] bool isLoaded() const
	{
		return hModule != nullptr;
	}

private:
	HMODULE hModule = nullptr;
};

enum IMMERSIVE_HC_CACHE_MODE
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

#if defined(_DARKMODELIB_ALLOW_OLD_OS)
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
#if defined(_DARKMODELIB_ALLOW_OLD_OS)
using fnSetWindowCompositionAttribute = BOOL (WINAPI*)(HWND hWnd, WINDOWCOMPOSITIONATTRIBDATA*);
#endif
// 1809 17763
using fnShouldAppsUseDarkMode = auto (WINAPI*)() -> bool; // ordinal 132
using fnAllowDarkModeForWindow = auto (WINAPI*)(HWND hWnd, bool allow) -> bool; // ordinal 133
#if defined(_DARKMODELIB_ALLOW_OLD_OS)
using fnAllowDarkModeForApp = auto (WINAPI*)(bool allow) -> bool; // ordinal 135, in 1809
#endif
using fnFlushMenuThemes = void (WINAPI*)(); // ordinal 136
using fnRefreshImmersiveColorPolicyState = void (WINAPI*)(); // ordinal 104
using fnIsDarkModeAllowedForWindow = auto (WINAPI*)(HWND hWnd) -> bool; // ordinal 137
using fnGetIsImmersiveColorUsingHighContrast = auto (WINAPI*)(IMMERSIVE_HC_CACHE_MODE mode) -> bool; // ordinal 106
using fnOpenNcThemeData = auto (WINAPI*)(HWND hWnd, LPCWSTR pszClassList) -> HTHEME; // ordinal 49
// 1903 18362
//using fnShouldSystemUseDarkMode = auto (WINAPI*)() -> bool; // ordinal 138
using fnSetPreferredAppMode = auto (WINAPI*)(PreferredAppMode appMode) -> PreferredAppMode; // ordinal 135, in 1903
//using fnIsDarkModeAllowedForApp = auto (WINAPI*)() -> bool; // ordinal 139

#if defined(_DARKMODELIB_ALLOW_OLD_OS)
static fnSetWindowCompositionAttribute pfSetWindowCompositionAttribute = nullptr;
#endif
static fnShouldAppsUseDarkMode pfShouldAppsUseDarkMode = nullptr;
static fnAllowDarkModeForWindow pfAllowDarkModeForWindow = nullptr;
#if defined(_DARKMODELIB_ALLOW_OLD_OS)
static fnAllowDarkModeForApp _AllowDarkModeForApp = nullptr;
#endif
static fnFlushMenuThemes pfFlushMenuThemes = nullptr;
static fnRefreshImmersiveColorPolicyState pfRefreshImmersiveColorPolicyState = nullptr;
static fnIsDarkModeAllowedForWindow pfIsDarkModeAllowedForWindow = nullptr;
static fnGetIsImmersiveColorUsingHighContrast pfGetIsImmersiveColorUsingHighContrast = nullptr;
static fnOpenNcThemeData pfOpenNcThemeData = nullptr;
// 1903 18362
//static fnShouldSystemUseDarkMode _ShouldSystemUseDarkMode = nullptr;
static fnSetPreferredAppMode pfSetPreferredAppMode = nullptr;

bool g_darkModeSupported = false;
bool g_darkModeEnabled = false;
static DWORD g_buildNumber = 0;

bool ShouldAppsUseDarkMode()
{
	if (pfShouldAppsUseDarkMode == nullptr)
	{
		return false;
	}
	return pfShouldAppsUseDarkMode();
}

bool AllowDarkModeForWindow(HWND hWnd, bool allow)
{
	if (g_darkModeSupported && (pfAllowDarkModeForWindow != nullptr))
	{
		return pfAllowDarkModeForWindow(hWnd, allow);
	}
	return false;
}

bool IsHighContrast()
{
	HIGHCONTRASTW highContrast{};
	highContrast.cbSize = sizeof(HIGHCONTRASTW);
	if (SystemParametersInfoW(SPI_GETHIGHCONTRAST, sizeof(HIGHCONTRASTW), &highContrast, FALSE) == TRUE)
	{
		return (highContrast.dwFlags & HCF_HIGHCONTRASTON) == HCF_HIGHCONTRASTON;
	}
	return false;
}

#if defined(_DARKMODELIB_ALLOW_OLD_OS)
void SetTitleBarThemeColor(HWND hWnd, BOOL dark)
{

	if (g_buildNumber < 18362)
		SetPropW(hWnd, L"UseImmersiveDarkModeColors", reinterpret_cast<HANDLE>(static_cast<intptr_t>(dark)));
	else if (pfSetWindowCompositionAttribute != nullptr)
	{
		WINDOWCOMPOSITIONATTRIBDATA data{ WCA_USEDARKMODECOLORS, &dark, sizeof(dark) };
		pfSetWindowCompositionAttribute(hWnd, &data);
	}
}

void RefreshTitleBarThemeColor(HWND hWnd)
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

bool IsColorSchemeChangeMessage(LPARAM lParam)
{
	bool isMsg = false;
	if ((lParam != 0) // NULL
		&& (_wcsicmp(reinterpret_cast<LPCWSTR>(lParam), L"ImmersiveColorSet") == 0)
		&& pfRefreshImmersiveColorPolicyState != nullptr)
	{
		pfRefreshImmersiveColorPolicyState();
		isMsg = true;
	}

	if (pfGetIsImmersiveColorUsingHighContrast != nullptr)
	{
		pfGetIsImmersiveColorUsingHighContrast(IHCM_REFRESH);
	}

	return isMsg;
}

bool IsColorSchemeChangeMessage(UINT uMsg, LPARAM lParam)
{
	if (uMsg == WM_SETTINGCHANGE)
	{
		return IsColorSchemeChangeMessage(lParam);
	}
	return false;
}

void AllowDarkModeForApp(bool allow)
{
	if (pfSetPreferredAppMode != nullptr)
	{
		pfSetPreferredAppMode(allow ? PreferredAppMode::ForceDark : PreferredAppMode::Default);
	}
#if defined(_DARKMODELIB_ALLOW_OLD_OS)
	else if (_AllowDarkModeForApp != nullptr)
	{
		_AllowDarkModeForApp(allow);
	}
#endif
}

static void FlushMenuThemes()
{
	if (pfFlushMenuThemes != nullptr)
	{
		pfFlushMenuThemes();
	}
}

// limit dark scroll bar to specific windows and their children

static std::unordered_set<HWND> g_darkScrollBarWindows;
static std::mutex g_darkScrollBarMutex;

void EnableDarkScrollBarForWindowAndChildren(HWND hWnd)
{
	const std::lock_guard<std::mutex> lock(g_darkScrollBarMutex);
	g_darkScrollBarWindows.insert(hWnd);
}

static bool IsWindowOrParentUsingDarkScrollBar(HWND hWnd)
{
	HWND hRoot = GetAncestor(hWnd, GA_ROOT);

	const std::lock_guard<std::mutex> lock(g_darkScrollBarMutex);
	auto hasElement = [](const auto& container, HWND hWndToCheck) -> bool {
#if (defined(_MSC_VER) && (_MSVC_LANG >= 202002L)) || (__cplusplus >= 202002L)
		return container.contains(hWndToCheck);
#else
		return container.count(hWndToCheck) != 0;
#endif
	};

	if (hasElement(g_darkScrollBarWindows, hWnd))
	{
		return true;
	}
	return (hWnd != hRoot && hasElement(g_darkScrollBarWindows, hRoot));
}

static HTHEME WINAPI MyOpenNcThemeData(HWND hWnd, LPCWSTR pszClassList)
{
	if (std::wcscmp(pszClassList, WC_SCROLLBAR) == 0)
	{
		if (IsWindowOrParentUsingDarkScrollBar(hWnd))
		{
			hWnd = nullptr;
			pszClassList = L"Explorer::ScrollBar";
		}
	}
	return pfOpenNcThemeData(hWnd, pszClassList);
}

static void FixDarkScrollBar()
{
	const ModuleHandle moduleComctl(L"comctl32.dll");
	if (moduleComctl.isLoaded())
	{
		auto* addr = FindDelayLoadThunkInModule(moduleComctl.get(), "uxtheme.dll", 49); // OpenNcThemeData
		if (addr != nullptr) // && pfOpenNcThemeData != nullptr) // checked in InitDarkMode
		{
			ReplaceFunction<fnOpenNcThemeData>(addr, MyOpenNcThemeData);
		}
	}
}

#if defined(_DARKMODELIB_ALLOW_OLD_OS)
static constexpr DWORD g_win10Build = 17763;
#else
static constexpr DWORD g_win10Build = 19045;
#endif
static constexpr DWORD g_win11Build = 22000;

bool IsWindows10() // or later OS version
{
	return (g_buildNumber >= g_win10Build);
}

bool IsWindows11() // or later OS version
{
	return (g_buildNumber >= g_win11Build);
}

static constexpr bool CheckBuildNumber(DWORD buildNumber)
{
#if defined(_DARKMODELIB_ALLOW_OLD_OS)
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

DWORD GetWindowsBuildNumber()
{
	return g_buildNumber;
}

void InitDarkMode()
{
	static bool isInit = false;
	if (isInit)
	{
		return;
	}

	fnRtlGetNtVersionNumbers RtlGetNtVersionNumbers = nullptr;
	HMODULE hNtdll = GetModuleHandleW(L"ntdll.dll");
	if (hNtdll != nullptr && loadFn(hNtdll, RtlGetNtVersionNumbers, "RtlGetNtVersionNumbers"))
	{
		DWORD major = 0;
		DWORD minor = 0;
		RtlGetNtVersionNumbers(&major, &minor, &g_buildNumber);
		g_buildNumber &= ~0xF0000000;
		if (major == 10 && minor == 0 && CheckBuildNumber(g_buildNumber))
		{
			const ModuleHandle moduleUxtheme(L"uxtheme.dll");
			if (moduleUxtheme.isLoaded())
			{
				const HMODULE& hUxtheme = moduleUxtheme.get();

				bool ptrFnOrd135NotNullptr = false;
#if defined(_DARKMODELIB_ALLOW_OLD_OS)
				if (g_buildNumber < 18362)
					ptrFnOrd135NotNullptr = loadFn(hUxtheme, _AllowDarkModeForApp, 135);
				else
#endif
					ptrFnOrd135NotNullptr = loadFn(hUxtheme, pfSetPreferredAppMode, 135);

				if (ptrFnOrd135NotNullptr
					&& loadFn(hUxtheme, pfOpenNcThemeData, 49)
					&& loadFn(hUxtheme, pfRefreshImmersiveColorPolicyState, 104)
					&& loadFn(hUxtheme, pfShouldAppsUseDarkMode, 132)
					&& loadFn(hUxtheme, pfAllowDarkModeForWindow, 133)
					&& loadFn(hUxtheme, pfFlushMenuThemes, 136)
					&& loadFn(hUxtheme, pfIsDarkModeAllowedForWindow, 137))
				{
					g_darkModeSupported = true;
				}

				loadFn(hUxtheme, pfGetIsImmersiveColorUsingHighContrast, 106);
#if defined(_DARKMODELIB_ALLOW_OLD_OS)
				if (g_buildNumber < 19041)
				{
					HMODULE hUser32 = GetModuleHandleW(L"user32.dll");
					if (hUser32 != nullptr)
					{
						loadFn(hUser32, pfSetWindowCompositionAttribute, "SetWindowCompositionAttribute");
					}
				}
#endif
				isInit = true;
			}
		}
	}
}

void SetDarkMode(bool useDark, bool fixDarkScrollbar)
{
	if (g_darkModeSupported)
	{
		AllowDarkModeForApp(useDark);
		FlushMenuThemes();
		if (fixDarkScrollbar)
		{
			FixDarkScrollBar();
		}
		g_darkModeEnabled = useDark && ShouldAppsUseDarkMode() && !IsHighContrast();
	}
}

// Hooking GetSysColor for comboboxex' list box and list view's gridlines

using fnGetSysColor = auto (WINAPI*)(int nIndex) -> DWORD;

static fnGetSysColor pfGetSysColor = nullptr;

static COLORREF g_clrWindow = RGB(32, 32, 32);
static COLORREF g_clrText = RGB(224, 224, 224);
static COLORREF g_clrTGridlines = RGB(100, 100, 100);

static bool g_isGetSysColorHooked = false;
static int g_hookRef = 0;

void SetMySysColor(int nIndex, COLORREF clr)
{
	switch (nIndex)
	{
		case COLOR_WINDOW:
		{
			g_clrWindow = clr;
			break;
		}

		case COLOR_WINDOWTEXT:
		{
			g_clrText = clr;
			break;
		}

		case COLOR_BTNFACE:
		{
			g_clrTGridlines = clr;
			break;
		}

		default:
		{
			break;
		}
	}
}

static DWORD WINAPI MyGetSysColor(int nIndex)
{
	if (!g_darkModeEnabled)
	{
		return GetSysColor(nIndex);
	}

	switch (nIndex)
	{
		case COLOR_WINDOW:
		{
			return g_clrWindow;
		}

		case COLOR_WINDOWTEXT:
		{
			return g_clrText;
		}

		case COLOR_BTNFACE:
		{
			return g_clrTGridlines;
		}

		default:
		{
			return GetSysColor(nIndex);
		}
	}
}

bool HookSysColor()
{
	const ModuleHandle moduleComctl(L"comctl32.dll");
	if (moduleComctl.isLoaded())
	{
		if (pfGetSysColor == nullptr || !g_isGetSysColorHooked)
		{
			auto* addr = FindIatThunkInModule(moduleComctl.get(), "user32.dll", "GetSysColor");
			if (addr != nullptr)
			{
				pfGetSysColor = ReplaceFunction<fnGetSysColor>(addr, MyGetSysColor);
				g_isGetSysColorHooked = true;
			}
			else
			{
				return false;
			}
		}

		if (g_isGetSysColorHooked)
		{
			++g_hookRef;
		}

		return true;
	}
	return false;
}

void UnhookSysColor()
{
	const ModuleHandle moduleComctl(L"comctl32.dll");
	if (moduleComctl.isLoaded())
	{
		if (g_isGetSysColorHooked)
		{
			if (g_hookRef > 0)
			{
				--g_hookRef;
			}

			if (g_hookRef == 0)
			{
				auto* addr = FindIatThunkInModule(moduleComctl.get(), "user32.dll", "GetSysColor");
				if (addr != nullptr)
				{
					ReplaceFunction<fnGetSysColor>(addr, pfGetSysColor);
					g_isGetSysColorHooked = false;
				}
			}
		}
	}
}
