// SPDX-License-Identifier: MPL-2.0

/*
 * Copyright (c) 2025 ozone10
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

// This file is part of darkmodelib library.


#include "StdAfx.h"

#include "DmlibHook.h"

#include <windows.h>

#include <uxtheme.h>
#include <vsstyle.h>
#include <vssym32.h>

#include <cstdint>
#include <utility>

#if defined(_DARKMODELIB_USE_SCROLLBAR_FIX) && (_DARKMODELIB_USE_SCROLLBAR_FIX > 0)
#include <mutex>
#include <unordered_set>
#endif

#include "ModuleHelper.h"

#include "IatHook.h"

namespace dmlib_win32api
{
	[[nodiscard]] bool IsWindows11() noexcept;
	[[nodiscard]] bool IsDarkModeActive() noexcept;
}

using fnFindThunkInModule = auto (*)(void* moduleBase, const char* dllName, const char* funcName) -> PIMAGE_THUNK_DATA;

using fnGetSysColor = auto (WINAPI*)(int nIndex) -> DWORD;
using fnGetThemeColor = auto (WINAPI*)(HTHEME hTheme, int iPartId, int iStateId, int iPropId, COLORREF* pColor) -> HRESULT;
using fnDrawThemeBackgroundEx = auto (WINAPI*)(HTHEME hTheme, HDC hdc, int iPartId, int iStateId, LPCRECT pRect, const DTBGOPTS* pOptions) -> HRESULT;

template <typename P>
static auto ReplaceFunction(IMAGE_THUNK_DATA* addr, const P& newFunction) noexcept -> P
{
	DWORD oldProtect = 0;
	if (::VirtualProtect(addr, sizeof(IMAGE_THUNK_DATA), PAGE_READWRITE, &oldProtect) == FALSE)
	{
		return nullptr;
	}

	const uintptr_t oldFunction = addr->u1.Function;
	addr->u1.Function = reinterpret_cast<uintptr_t>(newFunction);
	::VirtualProtect(addr, sizeof(IMAGE_THUNK_DATA), oldProtect, &oldProtect);
	return reinterpret_cast<P>(oldFunction);
}

template <typename T>
struct HookData
{
	T m_trueFn = nullptr;
	size_t m_ref = 0;
	const char* m_dllName = nullptr;

	const char* m_fnName = nullptr;
	fnFindThunkInModule m_findFn = nullptr;

	std::uint16_t m_ord = 0;

	void init(const char* dllName, const char* funcName, const fnFindThunkInModule& findFn) noexcept
	{
		if (m_dllName == nullptr)
		{
			m_dllName = dllName;
			m_fnName = funcName;
			m_findFn = findFn;

			m_ord = 0;
		}
	}

	void init(const char* dllName, std::uint16_t ord) noexcept
	{
		if (m_dllName == nullptr)
		{
			m_dllName = dllName;
			m_ord = ord;

			m_fnName = nullptr;
			m_findFn = nullptr;
		}
	}

	[[nodiscard]] IMAGE_THUNK_DATA* findAddr(HMODULE hMod) const noexcept
	{
		if (m_fnName != nullptr && m_findFn != nullptr)
		{
			return m_findFn(hMod, m_dllName, m_fnName);
		}

		if (m_ord != 0)
		{
			return iat_hook::FindDelayLoadThunkInModule(hMod, m_dllName, m_ord);
		}

		return nullptr;
	}
};

template <typename T, typename... InitArgs>
static auto HookFunction(HookData<T>& hookData, T newFn, const char* dllName, InitArgs&&... args) noexcept -> bool
{
	const dmlib_module::ModuleHandle moduleComctl(L"comctl32.dll");
	if (!moduleComctl.isLoaded())
	{
		return false;
	}

	if (hookData.m_trueFn == nullptr && hookData.m_ref == 0)
	{
		hookData.init(dllName, std::forward<InitArgs>(args)...);

		auto* addr = hookData.findAddr(moduleComctl.get());
		if (addr != nullptr)
		{
			hookData.m_trueFn = ReplaceFunction<T>(addr, newFn);
		}
	}

	if (hookData.m_trueFn != nullptr)
	{
		++hookData.m_ref;
		return true;
	}
	return false;
}

template <typename T>
static void UnhookFunction(HookData<T>& hookData) noexcept
{
	const dmlib_module::ModuleHandle moduleComctl(L"comctl32.dll");
	if (!moduleComctl.isLoaded())
	{
		return;
	}

	if (hookData.m_ref > 0)
	{
		--hookData.m_ref;

		if (hookData.m_trueFn != nullptr && hookData.m_ref == 0)
		{
			auto* addr = hookData.findAddr(moduleComctl.get());
			if (addr != nullptr)
			{
				ReplaceFunction<T>(addr, hookData.m_trueFn);
				hookData.m_trueFn = nullptr;
			}
		}
	}
}

#if defined(_DARKMODELIB_USE_SCROLLBAR_FIX) && (_DARKMODELIB_USE_SCROLLBAR_FIX > 0)
using fnOpenNcThemeData = auto (WINAPI*)(HWND hWnd, LPCWSTR pszClassList) -> HTHEME; // ordinal 49
static fnOpenNcThemeData pfOpenNcThemeData = nullptr;

bool dmlib_hook::loadOpenNcThemeData(const HMODULE& hUxtheme) noexcept
{
	return LoadFn(hUxtheme, pfOpenNcThemeData, 49);
}

#if defined(_DARKMODELIB_USE_SCROLLBAR_FIX) && (_DARKMODELIB_USE_SCROLLBAR_FIX > 1)
// limit dark scroll bar to specific windows and their children
static std::unordered_set<HWND> g_darkScrollBarWindows;
static std::mutex g_darkScrollBarMutex;

/**
 * @brief Makes scroll bars on the specified window and all its children consistent.
 *
 * @note Currently not widely used by default.
 *
 * @param[in] hWnd Handle to the parent window.
 */
void dmlib_hook::enableDarkScrollBarForWindowAndChildren(HWND hWnd)
{
	const std::lock_guard<std::mutex> lock(g_darkScrollBarMutex);
	g_darkScrollBarWindows.insert(hWnd);
}

static bool isWindowOrParentUsingDarkScrollBar(HWND hWnd)
{
	HWND hRoot = ::GetAncestor(hWnd, GA_ROOT);

	const std::lock_guard<std::mutex> lock(g_darkScrollBarMutex);
	auto hasElement = [](const auto& container, HWND hWndToCheck) -> bool
	{
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
#endif // defined(_DARKMODELIB_USE_SCROLLBAR_FIX) && (_DARKMODELIB_USE_SCROLLBAR_FIX > 1)

static HTHEME WINAPI MyOpenNcThemeData(HWND hWnd, LPCWSTR pszClassList)
{
	static constexpr std::wstring_view scrollBarClassName = WC_SCROLLBAR;
	if (scrollBarClassName == pszClassList)
	{
#if defined(_DARKMODELIB_USE_SCROLLBAR_FIX) && (_DARKMODELIB_USE_SCROLLBAR_FIX > 1)
		if (isWindowOrParentUsingDarkScrollBar(hWnd))
#endif
		{
			hWnd = nullptr;
			pszClassList = L"Explorer::ScrollBar";
		}
	}
	return pfOpenNcThemeData(hWnd, pszClassList);
}

void dmlib_hook::fixDarkScrollBar()
{
	const ModuleHandle moduleComctl(L"comctl32.dll");
	if (moduleComctl.isLoaded())
	{
		auto* addr = iat_hook::FindDelayLoadThunkInModule(moduleComctl.get(), "uxtheme.dll", 49); // OpenNcThemeData
		if (addr != nullptr) // && pfOpenNcThemeData != nullptr) // checked in InitDarkMode
		{
			ReplaceFunction<fnOpenNcThemeData>(addr, MyOpenNcThemeData);
		}
	}
}
#endif // defined(_DARKMODELIB_USE_SCROLLBAR_FIX) && (_DARKMODELIB_USE_SCROLLBAR_FIX > 0)

// Hooking GetSysColor for combo box ex' list box and list view's gridlines


static HookData<fnGetSysColor> g_hookDataGetSysColor{};

static COLORREF g_clrWindow = RGB(32, 32, 32);
static COLORREF g_clrText = RGB(224, 224, 224);
static COLORREF g_clrTGridlines = RGB(100, 100, 100);


/**
 * @brief Overrides a specific system color with a custom color.
 *
 * Currently supports:
 * - `COLOR_WINDOW`: Background of ComboBoxEx list.
 * - `COLOR_WINDOWTEXT`: Text color of ComboBoxEx list.
 * - `COLOR_BTNFACE`: Gridline color in ListView (when applicable).
 *
 * @param[in]   nIndex  One of the supported system color indices.
 * @param[in]   clr     Custom `COLORREF` value to apply.
 */
void dmlib_hook::setMySysColor(int nIndex, COLORREF clr) noexcept
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

static DWORD WINAPI MyGetSysColor(int nIndex) noexcept
{
	if (!dmlib_win32api::IsDarkModeActive())
	{
		return g_hookDataGetSysColor.m_trueFn(nIndex);
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
			return g_hookDataGetSysColor.m_trueFn(nIndex);
		}
	}
}

/**
 * @brief Hooks system color to support runtime customization.
 *
 * @return `true` if the hook was installed successfully.
 */
bool dmlib_hook::hookSysColor() noexcept
{
	return HookFunction<fnGetSysColor>(
		g_hookDataGetSysColor,
		MyGetSysColor,
		"user32.dll",
		static_cast<const char*>("GetSysColor"),
		iat_hook::FindIatThunkInModule);
}

/**
 * @brief Unhooks system color overrides and restores default color behavior.
 *
 * This function is safe to call even if no color hook is currently installed.
 * It ensures that system colors return to normal without requiring
 * prior state checks.
 */
void dmlib_hook::unhookSysColor() noexcept
{
	UnhookFunction<fnGetSysColor>(g_hookDataGetSysColor);
}

// Hooking GetThemeColor for Task Dialog text color

static HookData<fnGetThemeColor> g_hookDataGetThemeColor{};
static HookData<fnDrawThemeBackgroundEx> g_hookDataDrawThemeBackgroundEx{};

static constexpr COLORREF kMainInstructionTextClr = RGB(255, 255, 0);
static constexpr COLORREF kOtherTextClr = RGB(255, 255, 255);

static HTHEME g_hDarkTheme = nullptr;

static HRESULT WINAPI MyGetThemeColor(
	HTHEME hTheme,
	int iPartId,
	int iStateId,
	int iPropId,
	COLORREF* pColor
) noexcept
{
	const HRESULT retVal = g_hookDataGetThemeColor.m_trueFn(hTheme, iPartId, iStateId, iPropId, pColor);
	if (!dmlib_win32api::IsDarkModeActive() || pColor == nullptr)
	{
		return retVal;
	}

	if (iPropId == TMT_TEXTCOLOR)
	{
		switch (iPartId)
		{
			case TDLG_MAININSTRUCTIONPANE:
			{
				*pColor = kMainInstructionTextClr;
				break;
			}

			case TDLG_CONTENTPANE:
			case TDLG_EXPANDOTEXT:
			case TDLG_VERIFICATIONTEXT:
			case TDLG_FOOTNOTEPANE:
			case TDLG_EXPANDEDFOOTERAREA:
			{
				if (g_hDarkTheme != nullptr)
				{
					g_hookDataGetThemeColor.m_trueFn(g_hDarkTheme, iPartId, iStateId, iPropId, pColor);
				}
				else
				{
					*pColor = kOtherTextClr;
				}
				break;
			}

			default:
			{
				break;
			}
		}
	}
	return retVal;
}

static constexpr std::uint16_t kDrawThemeBackgroundExOrdinal = 47;

static constexpr COLORREF kMainPaneBgClr = RGB(44, 44, 44);
static constexpr COLORREF kFooterBgClr = RGB(32, 32, 32);

static HBRUSH g_hBrushBg = nullptr;
static HBRUSH g_hBrushBgFooter = nullptr;

static HRESULT WINAPI MyDrawThemeBackgroundEx(
	HTHEME hTheme,
	HDC hdc,
	int iPartId,
	int iStateId,
	LPCRECT pRect,
	const DTBGOPTS* pOptions
) noexcept
{
	if (!dmlib_win32api::IsDarkModeActive() || pOptions == nullptr)
	{
		return g_hookDataDrawThemeBackgroundEx.m_trueFn(hTheme, hdc, iPartId, iStateId, pRect, pOptions);
	}

	switch (iPartId)
	{
		case TDLG_PRIMARYPANEL:
		{
			::FillRect(hdc, pRect, g_hBrushBg);
			break;
		}

		case TDLG_SECONDARYPANEL:
		case TDLG_FOOTNOTEPANE:
		{
			::FillRect(hdc, &pOptions->rcClip, g_hBrushBgFooter);
			break;
		}

		default:
		{
			return g_hookDataDrawThemeBackgroundEx.m_trueFn(hTheme, hdc, iPartId, iStateId, pRect, pOptions);
		}
	}
	return S_OK;
}

/**
 * @brief Hooks `GetThemeColor` and `DrawThemeBackgroundEx` to support dark colors.
 *
 * @return `true` if the hook was installed successfully.
 */
bool dmlib_hook::hookThemeColor() noexcept
{
	COLORREF clrMain = kMainPaneBgClr;
	COLORREF clrFooter = kFooterBgClr;

	if (dmlib_win32api::IsWindows11() && g_hDarkTheme == nullptr)
	{
		g_hDarkTheme = ::OpenThemeData(nullptr, L"DarkMode_Explorer::TaskDialog");
		if (g_hDarkTheme != nullptr)
		{
			if (FAILED(::GetThemeColor(g_hDarkTheme, TDLG_PRIMARYPANEL, 0, TMT_FILLCOLOR, &clrMain)))
			{
				clrMain = kMainPaneBgClr;
			}

			if (FAILED(::GetThemeColor(g_hDarkTheme, TDLG_SECONDARYPANEL, 0, TMT_FILLCOLOR, &clrFooter)))
			{
				clrFooter = kFooterBgClr;
			}
		}
	}

	if (g_hBrushBg == nullptr)
	{
		g_hBrushBg = ::CreateSolidBrush(clrMain);
	}

	if (g_hBrushBgFooter == nullptr)
	{
		g_hBrushBgFooter = ::CreateSolidBrush(clrFooter);
	}

	return
		HookFunction<fnGetThemeColor>(g_hookDataGetThemeColor,
			MyGetThemeColor,
			"uxtheme.dll",
			static_cast<const char*>("GetThemeColor"),
			static_cast<fnFindThunkInModule>(iat_hook::FindDelayLoadThunkInModule))
		&& HookFunction<fnDrawThemeBackgroundEx>(g_hookDataDrawThemeBackgroundEx,
			MyDrawThemeBackgroundEx,
			"uxtheme.dll",
			kDrawThemeBackgroundExOrdinal);
}


/**
 * @brief Unhooks `GetThemeColor` and `DrawThemeBackgroundEx` overrides and restores default color behavior.
 *
 * This function is safe to call even if no color hook is currently installed.
 * It ensures that theme colors return to normal without requiring
 * prior state checks.
 */
void dmlib_hook::unhookThemeColor() noexcept
{
	UnhookFunction<fnGetThemeColor>(g_hookDataGetThemeColor);
	UnhookFunction<fnDrawThemeBackgroundEx>(g_hookDataDrawThemeBackgroundEx);
	if (g_hDarkTheme != nullptr && g_hookDataGetThemeColor.m_ref == 0)
	{
		::CloseThemeData(g_hDarkTheme);
		g_hDarkTheme = nullptr;
	}

	if (g_hBrushBg != nullptr)
	{
		::DeleteObject(g_hBrushBg);
		g_hBrushBg = nullptr;
	}

	if (g_hBrushBgFooter != nullptr)
	{
		::DeleteObject(g_hBrushBgFooter);
		g_hBrushBgFooter = nullptr;
	}
}
