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
#include <string_view>
#include <unordered_set>
#endif

#include "ModuleHelper.h"

#include "IatHook.h"

namespace dmlib_win32api
{
	[[nodiscard]] bool IsWindows11() noexcept;
	[[nodiscard]] bool IsDarkModeActive() noexcept;
} // namespace dmlib_win32api

#ifdef DMLIB_DLL
	#if defined(DMLIB_EXPORTS)
		#define DMLIB_API __declspec(dllexport)
	#else
		#define DMLIB_API __declspec(dllimport)
	#endif
#else
	#define DMLIB_API
#endif

namespace dmlib
{
	extern "C"
	{
		[[nodiscard]] DMLIB_API bool isEnabled();

		[[nodiscard]] DMLIB_API COLORREF getBackgroundColor();
		[[nodiscard]] DMLIB_API COLORREF getCtrlBackgroundColor();
		[[nodiscard]] DMLIB_API COLORREF getDlgBackgroundColor();
		[[nodiscard]] DMLIB_API COLORREF getTextColor();
		[[nodiscard]] DMLIB_API COLORREF getDarkerTextColor();

		[[nodiscard]] DMLIB_API HBRUSH getBackgroundBrush();
		[[nodiscard]] DMLIB_API HBRUSH getCtrlBackgroundBrush();

		DMLIB_API int darkMessageBoxW(HWND hWnd, LPCWSTR lpText, LPCWSTR lpCaption, UINT uType);
	}
}

static constexpr COLORREF kAccentBlue = RGB(0, 120, 215); // same as in dmlib_color

using FindThunkInModule_t = auto (*)(void* moduleBase, const char* dllName, const char* funcName) -> PIMAGE_THUNK_DATA;

template <typename P>
static auto ReplaceFunction(IMAGE_THUNK_DATA* addr, const P& newFunction) noexcept -> P
{
	DWORD oldProtect = 0;
	if (::VirtualProtect(addr, sizeof(IMAGE_THUNK_DATA), PAGE_READWRITE, &oldProtect) == FALSE)
	{
		return nullptr;
	}

	const UINT_PTR oldFunction = addr->u1.Function;
	addr->u1.Function = reinterpret_cast<UINT_PTR>(newFunction);
	::VirtualProtect(addr, sizeof(IMAGE_THUNK_DATA), oldProtect, &oldProtect);
	return reinterpret_cast<P>(oldFunction);
}

template <typename T>
struct HookData
{
	T m_trueFn = nullptr;
	size_t m_ref = 0;
	const char* m_fromDll = nullptr;
	const wchar_t* m_hookedDll = nullptr;

	const char* m_fnName = nullptr;
	FindThunkInModule_t m_findFn = nullptr;

	std::uint16_t m_ord = 0;

	void init(const char* fromDll, const char* funcName, const FindThunkInModule_t& findFn) noexcept
	{
		if (m_fromDll == nullptr)
		{
			m_fromDll = fromDll;
			m_fnName = funcName;
			m_findFn = findFn;

			m_ord = 0;
		}
	}

	void init(const char* fromDll, std::uint16_t ord) noexcept
	{
		if (m_fromDll == nullptr)
		{
			m_fromDll = fromDll;
			m_ord = ord;

			m_fnName = nullptr;
			m_findFn = nullptr;
		}
	}

	[[nodiscard]] IMAGE_THUNK_DATA* findAddr(HMODULE hMod) const noexcept
	{
		if (m_fnName != nullptr
			&& m_findFn != nullptr
			&& m_ord == 0)
		{
			return m_findFn(hMod, m_fromDll, m_fnName);
		}

		if (m_ord != 0)
		{
			return iat_hook::FindDelayLoadThunkInModule(hMod, m_fromDll, m_ord);
		}

		return nullptr;
	}
};

template <typename T, typename... InitArgs>
static auto HookFunction(HookData<T>& hookData, const wchar_t* hookedDll, T newFn, const char* fromDll, InitArgs&&... args) noexcept -> bool
{
	hookData.m_hookedDll = hookedDll;
	const dmlib_module::ModuleHandle hookedMod(hookData.m_hookedDll);
	if (!hookedMod.isLoaded())
	{
		return false;
	}

	if (hookData.m_trueFn == nullptr && hookData.m_ref == 0)
	{
		hookData.init(fromDll, std::forward<InitArgs>(args)...);

		auto* addr = hookData.findAddr(hookedMod.get());
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
static size_t UnhookFunction(HookData<T>& hookData, bool forceDetach = false) noexcept
{
	const dmlib_module::ModuleHandle hookedMod(hookData.m_hookedDll);
	if (!hookedMod.isLoaded())
	{
		return 0;
	}

	if (forceDetach && hookData.m_ref > 1)
	{
		hookData.m_ref = 1;
	}

	if (hookData.m_ref > 0)
	{
		--hookData.m_ref;

		if (hookData.m_trueFn != nullptr && hookData.m_ref == 0)
		{
			auto* addr = hookData.findAddr(hookedMod.get());
			if (addr != nullptr)
			{
				ReplaceFunction<T>(addr, hookData.m_trueFn);
				hookData.m_trueFn = nullptr;
			}
		}
	}
	return hookData.m_ref;
}

#if defined(_DARKMODELIB_USE_SCROLLBAR_FIX) && (_DARKMODELIB_USE_SCROLLBAR_FIX > 0)
using fnOpenNcThemeData = auto (WINAPI*)(HWND hWnd, LPCWSTR pszClassList) -> HTHEME; // ordinal 49
static fnOpenNcThemeData pfOpenNcThemeData = nullptr;

bool dmlib_hook::loadOpenNcThemeData(const HMODULE& hUxtheme) noexcept
{
	return dmlib_module::LoadFn(hUxtheme, pfOpenNcThemeData, 49);
}

#if defined(_DARKMODELIB_USE_SCROLLBAR_FIX) && (_DARKMODELIB_USE_SCROLLBAR_FIX > 1)
// limit dark scroll bar to specific windows and their children
static std::unordered_set<HWND> g_darkScrollBarWindows;
static std::mutex g_darkScrollBarMutex;

/**
 * @brief Makes scroll bars on the specified window and all its children consistent.
 *
 * @note Currently not widely used by default.
 *       If possible, try to use `dmlib::setDarkExplorerTheme`
 *       or `dmlib::setDarkThemeExperimental` instead.
 *
 * @param[in] hWnd Handle to the parent window.
 *
 * @see dmlib::setDarkExplorerTheme()
 * @see dmlib::setDarkThemeExperimental()
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
	const dmlib_module::ModuleHandle moduleComctl(L"comctl32.dll");
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

static HookData<decltype(&::GetSysColor)> g_hookDataGetSysColor{};

static COLORREF g_clrWindow = RGB(32, 32, 32);
static COLORREF g_clrText = RGB(224, 224, 224);
static COLORREF g_clrGridlines = RGB(100, 100, 100);

/**
 * @brief Sets a custom color for specific system color for override function.
 *
 * Currently supports:
 * - `COLOR_WINDOW`: Background of ComboBoxEx list.
 * - `COLOR_WINDOWTEXT`: Text color of ComboBoxEx list.
 * - `COLOR_3DFACE`: Gridline color in ListView (when applicable).
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

		case COLOR_3DFACE:
		{
			g_clrGridlines = clr;
			break;
		}

		default:
		{
			break;
		}
	}
}

extern "C"
{
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

			case COLOR_3DFACE:
			case COLOR_SCROLLBAR:
			{
				return g_clrGridlines;
			}

			default:
			{
				return g_hookDataGetSysColor.m_trueFn(nIndex);
			}
		}
	}
} // extern "C"

/**
 * @brief Hooks `::GetSysColor` to support runtime customization.
 *
 * @return `true` if the hook was installed successfully.
 */
bool dmlib_hook::GetSysColor::hook() noexcept
{
	return HookFunction<decltype(&::GetSysColor)>(
		g_hookDataGetSysColor,
		L"comctl32.dll",
		MyGetSysColor,
		"user32.dll",
		static_cast<const char*>("GetSysColor"),
		iat_hook::FindIatThunkInModule);
}

/**
 * @brief Unhooks `::GetSysColor` overrides.
 *
 * This function is safe to call even if no color hook is currently installed.
 * It ensures that system colors return to normal without requiring
 * prior state checks.
 */
size_t dmlib_hook::GetSysColor::unhook(bool forceDetach) noexcept
{
	return UnhookFunction(g_hookDataGetSysColor, forceDetach);
}

// Hooking `::GetThemeColor` and `::DrawThemeBackgroundEx` for Task Dialog text color

static HookData<decltype(&::GetThemeColor)> g_hookDataGetThemeColor{};
static HookData<decltype(&::DrawThemeBackgroundEx)> g_hookDataDrawThemeBackgroundEx{};

static COLORREF g_mainInstructionTextClr = RGB(153, 235, 255);
static COLORREF g_otherTextClr = RGB(255, 255, 255);

static HTHEME g_hDarkTheme = nullptr;

extern "C"
{
	static HRESULT WINAPI MyGetThemeColor(
		HTHEME hTheme,
		int iPartId,
		int iStateId,
		int iPropId,
		COLORREF* pColor
	) noexcept
	{
		if (!dmlib_win32api::IsDarkModeActive() || pColor == nullptr)
		{
			return g_hookDataGetThemeColor.m_trueFn(hTheme, iPartId, iStateId, iPropId, pColor);
		}

		if (iPropId == TMT_TEXTCOLOR)
		{
			switch (iPartId)
			{
				case TDLG_MAININSTRUCTIONPANE:
				{
					*pColor = g_mainInstructionTextClr;
					return S_OK;
				}

				case TDLG_CONTENTPANE:
				case TDLG_EXPANDOTEXT:
				case TDLG_VERIFICATIONTEXT:
				case TDLG_FOOTNOTEPANE:
				case TDLG_EXPANDEDFOOTERAREA:
				{
					if (g_hDarkTheme != nullptr)
					{
						if (const auto retVal = g_hookDataGetThemeColor.m_trueFn(g_hDarkTheme, iPartId, iStateId, iPropId, pColor);
							SUCCEEDED(retVal))
						{
							return S_OK;
						}
					}
					else
					{
						*pColor = g_otherTextClr;
						return S_OK;
					}
					break;
				}

				default:
				{
					break;
				}
			}
		}
		return g_hookDataGetThemeColor.m_trueFn(hTheme, iPartId, iStateId, iPropId, pColor);
	}
} // extern "C"

static constexpr std::uint16_t kDrawThemeBackgroundExOrdinal = 47;

static constexpr COLORREF kMainPaneBgClr = RGB(44, 44, 44);
static constexpr COLORREF kFooterBgClr = RGB(32, 32, 32);

static HBRUSH g_hBrushBg = nullptr;
static HBRUSH g_hBrushBgFooter = nullptr;

extern "C"
{
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
} // extern "C"

/**
 * @brief Hooks `::GetThemeColor` and `::DrawThemeBackgroundEx` to support dark colors.
 *
 * @return `true` if the hook was installed successfully.
 */
bool dmlib_hook::TaskDlgTheme::hook() noexcept
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

			if (FAILED(::GetThemeColor(g_hDarkTheme, TDLG_FOOTNOTEPANE, 0, TMT_FILLCOLOR, &clrFooter)))
			{
				clrFooter = kFooterBgClr;
			}

			if (FAILED(::GetThemeColor(g_hDarkTheme, TDLG_PRIMARYPANEL, 0, TMT_TEXTCOLOR, &g_otherTextClr)))
			{
				g_otherTextClr = RGB(255, 255, 255);
			}

			if (FAILED(::GetThemeColor(g_hDarkTheme, TDLG_MAININSTRUCTIONPANE, 0, TMT_TEXTCOLOR, &g_mainInstructionTextClr)))
			{
				g_mainInstructionTextClr = RGB(153, 235, 255);
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
		HookFunction<decltype(&::GetThemeColor)>(
			g_hookDataGetThemeColor,
			L"comctl32.dll",
			MyGetThemeColor,
			"uxtheme.dll",
			static_cast<const char*>("GetThemeColor"),
			static_cast<FindThunkInModule_t>(iat_hook::FindDelayLoadThunkInModule))
		&& HookFunction<decltype(&::DrawThemeBackgroundEx)>(
			g_hookDataDrawThemeBackgroundEx,
			L"comctl32.dll",
			MyDrawThemeBackgroundEx,
			"uxtheme.dll",
			kDrawThemeBackgroundExOrdinal);
}


/**
 * @brief Unhooks `::GetThemeColor` and `::DrawThemeBackgroundEx` overrides.
 *
 * This function is safe to call even if no color hook is currently installed.
 * It ensures that theme colors return to normal without requiring
 * prior state checks.
 */
size_t dmlib_hook::TaskDlgTheme::unhook(bool forceDetach) noexcept
{
	const auto count = UnhookFunction(g_hookDataGetThemeColor, forceDetach);
	UnhookFunction(g_hookDataDrawThemeBackgroundEx, forceDetach);

	if (g_hDarkTheme != nullptr && g_hookDataGetThemeColor.m_ref == 0)
	{
		::CloseThemeData(g_hDarkTheme);
		g_hDarkTheme = nullptr;
	}

	if (g_hBrushBg != nullptr && g_hookDataDrawThemeBackgroundEx.m_ref == 0)
	{
		::DeleteObject(g_hBrushBg);
		g_hBrushBg = nullptr;
	}

	if (g_hBrushBgFooter != nullptr && g_hookDataDrawThemeBackgroundEx.m_ref == 0)
	{
		::DeleteObject(g_hBrushBgFooter);
		g_hBrushBgFooter = nullptr;
	}

	return count;
}

// Hooking for ChooseFont and ChooseColor dialogs

static HookData<decltype(&::GetSysColor)> g_hookDataFontGetSysColor{};
static HookData<decltype(&::FillRect)> g_hookDataFontFillRect{};
static HookData<decltype(&::GetSysColorBrush)> g_hookDataClrGetSysColorBrush{};

static HookData<decltype(&::MessageBoxW)> g_hookDataFontMessageBoxW{};

static HBRUSH g_hBrushFontHighlight = nullptr;
static HBRUSH g_hBrushClrLum = nullptr;

/**
 * @brief Updates highlight brush for ChooseFont dialog.
 */
void dmlib_hook::updateFontBrush() noexcept
{
	if (g_hBrushFontHighlight != nullptr)
	{
		::DeleteObject(g_hBrushFontHighlight);
		g_hBrushFontHighlight = nullptr;
	}
	g_hBrushFontHighlight = ::CreateSolidBrush(kAccentBlue);
}

/**
 * @brief Updates luminosity slider brush for ChooseColor dialog.
 */
void dmlib_hook::updateLumSliderBrush() noexcept
{
	if (g_hBrushClrLum != nullptr)
	{
		::DeleteObject(g_hBrushClrLum);
		g_hBrushClrLum = nullptr;
	}
	g_hBrushClrLum = ::CreateSolidBrush(dmlib::getDarkerTextColor());
}

extern "C"
{
/**
 * @brief Returns a custom color instead of a specific system color for ChooseFont and ChooseColor dialogs.
 *
 * Specific replaced system colors:
 * - `COLOR_WINDOW`: Background of ComboBox lists.
 * - `COLOR_WINDOWTEXT`: Text color of ComboBox lists and luminosity slide control.
 * - `COLOR_HIGHLIGHT`: Highlighted background of ComboBox lists.
 * - `COLOR_HIGHLIGHTTEXT`: Highlighted text color of ComboBox lists.
 * - `COLOR_3DFACE`: Background color of font preview.
 *
 * @return DWORD color value.
 */
	static DWORD WINAPI MyFontGetSysColor(int nIndex) noexcept
	{
		if (!dmlib::isEnabled())
		{
			return g_hookDataFontGetSysColor.m_trueFn(nIndex);
		}

		switch (nIndex)
		{
			case COLOR_WINDOW:
			{
				return dmlib_win32api::IsDarkModeActive() ? dmlib::getBackgroundColor() : dmlib::getCtrlBackgroundColor();
			}

			case COLOR_WINDOWTEXT:
			{
				return dmlib::getDarkerTextColor();
			}

			case COLOR_HIGHLIGHT:
			{
				return kAccentBlue;
			}

			case COLOR_HIGHLIGHTTEXT:
			{
				return dmlib::getTextColor();
			}

			case COLOR_BTNFACE:
			{
				return dmlib::getDlgBackgroundColor();
			}

			default:
			{
				break;
			}
		}
		return g_hookDataFontGetSysColor.m_trueFn(nIndex);
	}

	/**
	 * @brief Changes system brushes for `::FillRect` for ChooseFont dialog.
	 *
	 * Changes system brushes (`COLOR_WINDOW` and `COLOR_HIGHLIGHT`)
	 * for `::FillRect` to override background colors for ChooseFont dialog combo box list boxes.
	 *
	 * @return int value, same as `::FillRect`.
	 * @return If the function succeeds, the return value is nonzero.
	 * @return If the function fails, the return value is zero.
	 */
	static int WINAPI MyFontFillRect(HDC hDC, const RECT* lprc, HBRUSH hbr) noexcept
	{
		if (!dmlib::isEnabled())
		{
			return g_hookDataFontFillRect.m_trueFn(hDC, lprc, hbr);
		}

		HBRUSH hBrush = nullptr;

		switch (reinterpret_cast<UINT_PTR>(hbr))
		{
			case COLOR_WINDOW + 1:
			{
				hBrush = dmlib_win32api::IsDarkModeActive() ? dmlib::getBackgroundBrush() : dmlib::getCtrlBackgroundBrush();
				break;
			}

			case COLOR_HIGHLIGHT + 1:
			{
				hBrush = g_hBrushFontHighlight;
				break;
			}

			default:
			{
				hBrush = hbr;
				break;
			}
		}

		return g_hookDataFontFillRect.m_trueFn(hDC, lprc, hBrush);
	}

	/**
	 * @brief Changes returned system brush for `::GetSysColorBrush` for ChooseColor dialog.
	 *
	 * Changes system brush (`COLOR_BTNTEXT`) to override color
	 * for ChooseColor dialog luminosity slider control.
	 *
	 * @return HBRUSH, same as `::GetSysColorBrush`.
	 */
	static HBRUSH WINAPI MyClrGetSysColorBrush(int nIndex) noexcept
	{
		if (!dmlib::isEnabled())
		{
			return g_hookDataClrGetSysColorBrush.m_trueFn(nIndex);
		}

		if (nIndex == COLOR_BTNTEXT && g_hBrushClrLum != nullptr)
		{
			return g_hBrushClrLum;
		}

		return g_hookDataClrGetSysColorBrush.m_trueFn(nIndex);
	}

	/**
	 * @brief Replaces `::MessageBoxW` with `dmlib::darkMessageBoxW` for ChooseFont dialog.
	 *
	 * @return int, same as `::MessageBoxW`.
	 */
	static int WINAPI MyFontMessageBoxW(HWND hWnd, LPCWSTR lpText, LPCWSTR lpCaption, UINT uType) noexcept
	{
		if (!dmlib_win32api::IsDarkModeActive() || uType != MB_ICONINFORMATION)
		{
			return g_hookDataFontMessageBoxW.m_trueFn(hWnd, lpText, lpCaption, uType);
		}
		return dmlib::darkMessageBoxW(hWnd, lpText, lpCaption, uType);
	}
} // extern "C"

/**
* @brief Hooks `::GetSysColor` to support runtime customization for ChooseFont dialog.
*
* @return `true` if the hook was installed successfully.
*/
bool dmlib_hook::FontGetSysColor::hook() noexcept
{
	return
		HookFunction<decltype(&::GetSysColor)>(
			g_hookDataFontGetSysColor,
			L"comdlg32.dll",
			MyFontGetSysColor,
			"user32.dll",
			static_cast<const char*>("GetSysColor"),
			iat_hook::FindIatThunkInModule);
}

/**
* @brief Unhooks `::GetSysColor` overrides for ChooseFont dialog.
*
* This function is safe to call even if no color hook is currently installed.
* It ensures that system colors return to normal without requiring
* prior state checks.
*
* @return size_t count of hook "installs".
*/
size_t dmlib_hook::FontGetSysColor::unhook(bool forceDetach) noexcept
{
	return UnhookFunction(g_hookDataFontGetSysColor, forceDetach);
}

/**
 * @brief Hooks ::FillRect to support runtime customization for ChooseFont dialog.
 *
 * @return `true` if the hook was installed successfully.
 */
bool dmlib_hook::FontFillRect::hook() noexcept
{
	dmlib_hook::updateFontBrush();

	return
		HookFunction<decltype(&::FillRect)>(
			g_hookDataFontFillRect,
			L"comdlg32.dll",
			MyFontFillRect,
			"user32.dll",
			static_cast<const char*>("FillRect"),
			iat_hook::FindIatThunkInModule);
}

/**
 * @brief Unhooks ::FillRect override for ChooseFont dialog.
 *
 * This function is safe to call even if no color hook is currently installed.
 * It ensures that ::FillRect return to normal without requiring
 * prior state checks.
 *
 * @return size_t count of hook "installs".
 */
size_t dmlib_hook::FontFillRect::unhook(bool forceDetach) noexcept
{
	const auto count = UnhookFunction(g_hookDataFontFillRect, forceDetach);

	if (g_hBrushFontHighlight != nullptr)
	{
		::DeleteObject(g_hBrushFontHighlight);
		g_hBrushFontHighlight = nullptr;
	}

	return count;
}

/**
 * @brief Hooks `::GetSysColorBrush` to support runtime customization for ChooseColor dialog luminosity slider control.
 *
 * @return `true` if the hook was installed successfully.
 */
bool dmlib_hook::ClrGetSysColorBrush::hook() noexcept
{
	dmlib_hook::updateLumSliderBrush();

	return
		HookFunction<decltype(&::GetSysColorBrush)>(
			g_hookDataClrGetSysColorBrush,
			L"comdlg32.dll",
			MyClrGetSysColorBrush,
			"user32.dll",
			static_cast<const char*>("GetSysColorBrush"),
			iat_hook::FindIatThunkInModule);
}

/**
 * @brief Unhooks `::GetSysColorBrush` override for ChooseColor dialogs.
 *
 * This function is safe to call even if no color hook is currently installed.
 * It ensures that `::GetSysColorBrush` return to normal without requiring
 * prior state checks.
 *
 * @return size_t count of hook "installs".
 */
size_t dmlib_hook::ClrGetSysColorBrush::unhook(bool forceDetach) noexcept
{
	const auto count = UnhookFunction(g_hookDataClrGetSysColorBrush, forceDetach);

	if (g_hBrushClrLum != nullptr)
	{
		::DeleteObject(g_hBrushClrLum);
		g_hBrushClrLum = nullptr;
	}

	return count;
}

/**
 * @brief Hooks `::MessageBoxW` to apply dark mode for ChooseFont dialog message boxes.
 *
 * @return `true` if the hook was installed successfully.
 */
bool dmlib_hook::FontMB::hook() noexcept
{
	return HookFunction<decltype(&::MessageBoxW)>(
		g_hookDataFontMessageBoxW,
		L"comdlg32.dll",
		MyFontMessageBoxW,
		"user32.dll",
		static_cast<const char*>("MessageBoxW"),
		iat_hook::FindIatThunkInModule);
}
/**
 * @brief Unhooks ChooseFont dialog `::MessageBoxW`.
 *
 * This function is safe to call even if no message box hook is currently installed.
 * It ensures that message box return to normal without requiring
 * prior state checks.
 */
size_t dmlib_hook::FontMB::unhook(bool forceDetach) noexcept
{
	return UnhookFunction(g_hookDataFontMessageBoxW, forceDetach);
}
