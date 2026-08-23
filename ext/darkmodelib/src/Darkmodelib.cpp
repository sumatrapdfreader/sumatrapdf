// SPDX-License-Identifier: MPL-2.0

/*
 * Copyright (c) 2025-2026 ozone10
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

// This file is part of darkmodelib library.

// Based on the Notepad++ dark mode code licensed under GPLv3.
// Originally by adzm / Adam D. Walling, with modifications by the Notepad++ team.
// Heavily modified by ozone10 (Notepad++ contributor).
// Used with permission to relicense under the Mozilla Public License, v. 2.0.


#include "StdAfx.h"

#include "Darkmodelib.h"

#ifndef _DARKMODELIB_NOT_USED

#include <windows.h>

#include <colordlg.h>
#include <commdlg.h>
#include <dlgs.h>
#include <dwmapi.h>
#include <richedit.h>
#include <uxtheme.h>
#include <vsstyle.h>

#include <array>
#include <cstdint>

#ifndef _DARKMODELIB_NO_INI_CONFIG
#include <string>
#endif

#include "DmlibColor.h"
#include "DmlibDpi.h"
#include "DmlibHook.h"
#ifndef _DARKMODELIB_NO_INI_CONFIG
#include "DmlibIni.h"
#endif
#include "DmlibSubclass.h"
#include "DmlibSubclassControl.h"
#include "DmlibSubclassWindow.h"
#include "DmlibWinApi.h"

#include "MemoryHelperDef.h"

#include "Version.h"

/**
 * @brief Returns library version information or compile-time feature flags.
 *
 * Responds to the specified query by returning either:
 * - Version numbers (`verMajor`, `verMinor`, `verRevision`)
 * - Build configuration flags (returns `TRUE` or `FALSE`)
 * - A constant value (`featureCheck`, `maxValue`) used for validation
 *
 * @param[in] libInfoType Integer with `LibInfo` enum value specifying which piece of information to retrieve.
 * @return Integer value:
 * - Version: as defined by `DM_VERSION_MAJOR`, etc.
 * - Boolean flags: `TRUE` (1) if the feature is enabled, `FALSE` (0) otherwise.
 * - `featureCheck`, `maxValue`: returns the numeric max enum value.
 * - `-1`: for invalid or unhandled enum cases (should not occur in correct usage).
 *
 * @see LibInfo
 */
int dmlib::getLibInfo(int libInfoType)
{
	switch (static_cast<LibInfo>(libInfoType))
	{
		case LibInfo::maxValue:
		case LibInfo::featureCheck:
		{
			return static_cast<int>(LibInfo::maxValue);
		}

		case LibInfo::verMajor:
		{
			return DM_VERSION_MAJOR;
		}

		case LibInfo::verMinor:
		{
			return DM_VERSION_MINOR;
		}

		case LibInfo::verRevision:
		{
			return DM_VERSION_REVISION;
		}

		case LibInfo::iniConfigUsed:
		{
#ifndef _DARKMODELIB_NO_INI_CONFIG
			return TRUE;
#else
			return FALSE;
#endif
		}

		case LibInfo::allowOldOS:
		{
#ifdef _DARKMODELIB_ALLOW_OLD_OS
			return _DARKMODELIB_ALLOW_OLD_OS;
#else
			return FALSE;
#endif
		}

		case LibInfo::useDlgProcCtl:
		{
#ifdef _DARKMODELIB_DLG_PROC_CTLCOLOR_RETURNS
			return _DARKMODELIB_DLG_PROC_CTLCOLOR_RETURNS;
#else
			return FALSE;
#endif
		}

		case LibInfo::preferTheme:
		{
#ifdef _DARKMODELIB_PREFER_THEME
			return _DARKMODELIB_PREFER_THEME;
#else
			return FALSE;
#endif
		}

		case LibInfo::useSBFix:
		{
#ifdef _DARKMODELIB_USE_SCROLLBAR_FIX
			return _DARKMODELIB_USE_SCROLLBAR_FIX;
#else
			return FALSE;
#endif
		}

		case LibInfo::memAPI:
		{
#ifdef _DARKMODELIB_CUSTOM_MEM
			return _DARKMODELIB_CUSTOM_MEM;
#else
			return FALSE;
#endif
		}
	}
	return -1; // should never happen
}

/**
 * @brief Describes how the application responds to the system theme.
 *
 * Used to determine behavior when following the system's light/dark mode setting.
 * - `disabled`: Do not follow system; use manually selected appearance.
 * - `light`: Follow system mode; apply light theme when system is in light mode.
 * - `classic`: Follow system mode; apply classic style when system is in light mode.
 */
enum class WinMode : std::uint8_t
{
	disabled,  ///< Manual - system mode is ignored.
	light,     ///< Use light theme if system is in light mode.
	classic    ///< Use classic style if system is in light mode.
};

/// Types of list and tree views checkbox styles
enum class ViewCheckbox : std::uint8_t
{
	listView,   ///< List view with LVS_EX_CHECKBOXES extendend style.
	tvSimple,   ///< Tree view with TVS_CHECKBOXES style.

	/// Tree view with any combination of TVS_EX_PARTIALCHECKBOXES, TVS_EX_EXCLUSIONCHECKBOXES,
	/// TVS_EX_DIMMEDCHECKBOXES extendend styles.
	tvExtended
};

/**
 * @struct DarkModeParams
 * @brief Defines theming and subclassing parameters for child controls.
 *
 * Members:
 * - `m_themeClassName`: Optional theme class name (e.g. `"DarkMode_Explorer"`), or `nullptr` to skip theming.
 * - `m_subclass`: Whether to apply custom subclassing for dark-mode painting and behavior.
 * - `m_theme`: Whether to apply a themed visual style to applicable controls.
 *
 * Used during enumeration to configure dark mode application on a per-control basis.
 */
struct DarkModeParams
{
	const wchar_t* m_themeClassName = nullptr;
	bool m_subclass = false;
	bool m_theme = false;
};

/// Threshold range around 50.0 where TreeView uses classic style instead of light/dark.
static constexpr double kMiddleGrayRange = 2.0;

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wexit-time-destructors"
#pragma clang diagnostic ignored "-Wglobal-constructors"
#endif

namespace // anonymous
{
	/// Global struct
	struct
	{
		DWM_WINDOW_CORNER_PREFERENCE m_roundCorner = DWMWCP_DEFAULT;
		COLORREF m_borderColor = DWMWA_COLOR_DEFAULT;
		DWM_SYSTEMBACKDROP_TYPE m_mica = DWMSBT_AUTO;
		COLORREF m_tvBackground = RGB(41, 49, 52);
		double m_lightness = 50.0;
		dmlib::TreeViewStyle m_tvStylePrev = dmlib::TreeViewStyle::classic;
		dmlib::TreeViewStyle m_tvStyle = dmlib::TreeViewStyle::classic;
		bool m_micaExtend = false;
		bool m_colorizeTitleBar = false;
		dmlib::DarkModeType m_dmType = dmlib::DarkModeType::dark;
		WinMode m_windowsMode = WinMode::disabled;
		bool m_isInit = false;
		bool m_isInitExperimental = false;

#ifndef _DARKMODELIB_NO_INI_CONFIG
		std::wstring m_iniName;
		bool m_isIniNameSet = false;
		bool m_iniExist = false;
#endif
	} g_dmCfg;
} // anonymous namespace

static dmlib_color::Theme& getTheme() noexcept
{
	static dmlib_color::Theme tMain{};
	return tMain;
}

/**
 * @brief Sets the color tone and its color set for the active theme.
 *
 * Applies a color tone (e.g. red, blue, olive) its color set.
 *
 * @param[in] colorTone The tone to apply (see @ref ColorTone enum).
 *
 * @see dmlib::getColorTone()
 * @see dmlib_color::Theme
 */
void dmlib::setColorTone(int colorTone)
{
	getTheme().setToneColors(static_cast<ColorTone>(colorTone));
}

/**
 * @brief Retrieves the currently active color tone for the theme.
 *
 * @return The currently selected @ref ColorTone value.
 *
 * @see dmlib::setColorTone()
 */
int dmlib::getColorTone()
{
	return static_cast<int>(getTheme().getColorTone());
}

static dmlib_color::ThemeView& getThemeView() noexcept
{
	static dmlib_color::ThemeView tView{};
	return tView;
}

#ifdef __clang__
#pragma clang diagnostic pop
#endif

COLORREF dmlib::setBackgroundColor(COLORREF clrNew)         { return getTheme().setColorBackground(clrNew); }
COLORREF dmlib::setCtrlBackgroundColor(COLORREF clrNew)     { return getTheme().setColorCtrlBackground(clrNew); }
COLORREF dmlib::setHotBackgroundColor(COLORREF clrNew)      { return getTheme().setColorHotBackground(clrNew); }
COLORREF dmlib::setDlgBackgroundColor(COLORREF clrNew)      { return getTheme().setColorDlgBackground(clrNew); }
COLORREF dmlib::setErrorBackgroundColor(COLORREF clrNew)    { return getTheme().setColorErrorBackground(clrNew); }
COLORREF dmlib::setTextColor(COLORREF clrNew)               { return getTheme().setColorText(clrNew); }
COLORREF dmlib::setDarkerTextColor(COLORREF clrNew)         { return getTheme().setColorDarkerText(clrNew); }
COLORREF dmlib::setDisabledTextColor(COLORREF clrNew)       { return getTheme().setColorDisabledText(clrNew); }
COLORREF dmlib::setLinkTextColor(COLORREF clrNew)           { return getTheme().setColorLinkText(clrNew); }
COLORREF dmlib::setEdgeColor(COLORREF clrNew)               { return getTheme().setColorEdge(clrNew); }
COLORREF dmlib::setHotEdgeColor(COLORREF clrNew)            { return getTheme().setColorHotEdge(clrNew); }
COLORREF dmlib::setDisabledEdgeColor(COLORREF clrNew)       { return getTheme().setColorDisabledEdge(clrNew); }
COLORREF dmlib::setHighlightColor(COLORREF clrNew)          { return getTheme().setColorHighlight(clrNew); }

void dmlib::setThemeColors(const Colors* colors)
{
	if (colors != nullptr)
	{
		getTheme().updateTheme(*colors);
	}
}

void dmlib::updateThemeBrushesAndPens()
{
	getTheme().updateTheme();
}

COLORREF dmlib::getBackgroundColor()        { return getTheme().getColors().background; }
COLORREF dmlib::getCtrlBackgroundColor()    { return getTheme().getColors().ctrlBackground; }
COLORREF dmlib::getHotBackgroundColor()     { return getTheme().getColors().hotBackground; }
COLORREF dmlib::getDlgBackgroundColor()     { return getTheme().getColors().dlgBackground; }
COLORREF dmlib::getErrorBackgroundColor()   { return getTheme().getColors().errorBackground; }
COLORREF dmlib::getTextColor()              { return getTheme().getColors().text; }
COLORREF dmlib::getDarkerTextColor()        { return getTheme().getColors().darkerText; }
COLORREF dmlib::getDisabledTextColor()      { return getTheme().getColors().disabledText; }
COLORREF dmlib::getLinkTextColor()          { return getTheme().getColors().linkText; }
COLORREF dmlib::getEdgeColor()              { return getTheme().getColors().edge; }
COLORREF dmlib::getHotEdgeColor()           { return getTheme().getColors().hotEdge; }
COLORREF dmlib::getDisabledEdgeColor()      { return getTheme().getColors().disabledEdge; }
COLORREF dmlib::getHighlightColor()         { return getTheme().getColors().highlight; }

HBRUSH dmlib::getBackgroundBrush()          { return getTheme().getBrushes().m_background; }
HBRUSH dmlib::getCtrlBackgroundBrush()      { return getTheme().getBrushes().m_ctrlBackground; }
HBRUSH dmlib::getHotBackgroundBrush()       { return getTheme().getBrushes().m_hotBackground; }
HBRUSH dmlib::getDlgBackgroundBrush()       { return getTheme().getBrushes().m_dlgBackground; }
HBRUSH dmlib::getErrorBackgroundBrush()     { return getTheme().getBrushes().m_errorBackground; }

HBRUSH dmlib::getEdgeBrush()                { return getTheme().getBrushes().m_edge; }
HBRUSH dmlib::getHotEdgeBrush()             { return getTheme().getBrushes().m_hotEdge; }
HBRUSH dmlib::getDisabledEdgeBrush()        { return getTheme().getBrushes().m_disabledEdge; }

HBRUSH dmlib::getHighlightBrush()           { return getTheme().getBrushes().m_highlight; }

HPEN dmlib::getDarkerTextPen()              { return getTheme().getPens().m_darkerText; }
HPEN dmlib::getEdgePen()                    { return getTheme().getPens().m_edge; }
HPEN dmlib::getHotEdgePen()                 { return getTheme().getPens().m_hotEdge; }
HPEN dmlib::getDisabledEdgePen()            { return getTheme().getPens().m_disabledEdge; }

HPEN dmlib::getHighlightPen()               { return getTheme().getPens().m_highlight; }

COLORREF dmlib::setViewBackgroundColor(COLORREF clrNew)         { return getThemeView().setColorBackground(clrNew); }
COLORREF dmlib::setViewTextColor(COLORREF clrNew)               { return getThemeView().setColorText(clrNew); }
COLORREF dmlib::setViewGridlinesColor(COLORREF clrNew)          { return getThemeView().setColorGridlines(clrNew); }

COLORREF dmlib::setHeaderBackgroundColor(COLORREF clrNew)       { return getThemeView().setColorHeaderBackground(clrNew); }
COLORREF dmlib::setHeaderHotBackgroundColor(COLORREF clrNew)    { return getThemeView().setColorHeaderHotBackground(clrNew); }
COLORREF dmlib::setHeaderTextColor(COLORREF clrNew)             { return getThemeView().setColorHeaderText(clrNew); }
COLORREF dmlib::setHeaderEdgeColor(COLORREF clrNew)             { return getThemeView().setColorHeaderEdge(clrNew); }

void dmlib::setViewColors(const ColorsView* colors)
{
	if (colors != nullptr)
	{
		getThemeView().updateView(*colors);
	}
}

void dmlib::updateViewBrushesAndPens()
{
	getThemeView().updateView();
}

COLORREF dmlib::getViewBackgroundColor()        { return getThemeView().getColors().background; }
COLORREF dmlib::getViewTextColor()              { return getThemeView().getColors().text; }
COLORREF dmlib::getViewGridlinesColor()         { return getThemeView().getColors().gridlines; }

COLORREF dmlib::getHeaderBackgroundColor()      { return getThemeView().getColors().headerBackground; }
COLORREF dmlib::getHeaderHotBackgroundColor()   { return getThemeView().getColors().headerHotBackground; }
COLORREF dmlib::getHeaderTextColor()            { return getThemeView().getColors().headerText; }
COLORREF dmlib::getHeaderEdgeColor()            { return getThemeView().getColors().headerEdge; }

HBRUSH dmlib::getViewBackgroundBrush()          { return getThemeView().getViewBrushesAndPens().m_background; }
HBRUSH dmlib::getViewGridlinesBrush()           { return getThemeView().getViewBrushesAndPens().m_gridlines; }

HBRUSH dmlib::getHeaderBackgroundBrush()        { return getThemeView().getViewBrushesAndPens().m_headerBackground; }
HBRUSH dmlib::getHeaderHotBackgroundBrush()     { return getThemeView().getViewBrushesAndPens().m_headerHotBackground; }

HPEN dmlib::getHeaderEdgePen()                  { return getThemeView().getViewBrushesAndPens().m_headerEdge; }

/**
 * @brief Initializes default color set based on the current mode type.
 *
 * Sets up control and view colors depending on the active theme:
 * - `dark`: Applies dark tone color set and view dark color set.
 * - `light`: Applies the predefined light color set and view light color set.
 * - `classic`: Applies only system color on views, other controls are not affected
 *              by theme colors.
 *
 * If `updateBrushesAndOther` is `true`, also updates
 * brushes, pens, and view styles (unless in classic mode).
 *
 * @param[in] updateBrushesAndOther Whether to refresh GDI brushes and pens, and tree view styling.
 *
 * @see dmlib::updateThemeBrushesAndPens
 * @see dmlib::calculateTreeViewStyle
 */
void dmlib::setDefaultColors(bool updateBrushesAndOther)
{
	switch (g_dmCfg.m_dmType)
	{
		case DarkModeType::dark:
		{
			getTheme().setToneColors();
			getThemeView().resetColors(true);
			break;
		}

		case DarkModeType::light:
		{
			getTheme().setLightColors();
			getThemeView().resetColors(false);
			break;
		}

		case DarkModeType::classic:
		{
			dmlib::setViewBackgroundColor(::GetSysColor(COLOR_WINDOW));
			dmlib::setViewTextColor(::GetSysColor(COLOR_WINDOWTEXT));
			break;
		}
	}

	if (updateBrushesAndOther)
	{
		if (g_dmCfg.m_dmType != DarkModeType::classic)
		{
			dmlib::updateThemeBrushesAndPens();
			dmlib::updateViewBrushesAndPens();
		}
	}
	dmlib::calculateTreeViewStyle();
}

/**
 * @brief Initializes the dark mode configuration based on the selected mode.
 *
 * Sets the active dark mode theming and system-following behavior according to the specified `dmType`:
 * - `0`: Light mode, do not follow system.
 * - `1` or default: Dark mode, do not follow system.
 * - `2`: *[Internal]* Follow system - light or dark depending on registry (see `dmlib::isDarkModeReg()`).
 * - `3`: Classic mode, do not follow system.
 * - `4`: *[Internal]* Follow system - classic or dark depending on registry.
 *
 * @param[in] dmType Integer representing the desired mode.
 *
 * @see DarkModeType
 * @see WinMode
 * @see dmlib::isDarkModeReg()
 */
void dmlib::initDarkModeConfig(UINT dmType)
{
	switch (dmType)
	{
		case 0:
		{
			g_dmCfg.m_dmType = DarkModeType::light;
			g_dmCfg.m_windowsMode = WinMode::disabled;
			break;
		}

		case 2:
		{
			g_dmCfg.m_dmType = dmlib::isDarkModeReg() ? DarkModeType::dark : DarkModeType::light;
			g_dmCfg.m_windowsMode = WinMode::light;
			break;
		}

		case 3:
		{
			g_dmCfg.m_dmType = DarkModeType::classic;
			g_dmCfg.m_windowsMode = WinMode::disabled;
			break;
		}

		case 4:
		{
			g_dmCfg.m_dmType = dmlib::isDarkModeReg() ? DarkModeType::dark : DarkModeType::classic;
			g_dmCfg.m_windowsMode = WinMode::classic;
			break;
		}

		case 1:
		default:
		{
			g_dmCfg.m_dmType = DarkModeType::dark;
			g_dmCfg.m_windowsMode = WinMode::disabled;
			break;
		}
	}
}

/**
 * @brief Sets the preferred window corner style on Windows 11.
 *
 * Assigns a valid `DWM_WINDOW_CORNER_PREFERENCE` value to the config,
 * falling back to `DWMWCP_DEFAULT` if the input is out of range.
 *
 * @param[in] roundCornerStyle Integer value representing a `DWM_WINDOW_CORNER_PREFERENCE`.
 *
 * @see https://learn.microsoft.com/windows/win32/api/dwmapi/ne-dwmapi-dwm_window_corner_preference
 * @see dmlib::setDarkTitleBarEx()
 */
void dmlib::setRoundCornerConfig(UINT roundCornerStyle)
{
	const auto cornerStyle = static_cast<DWM_WINDOW_CORNER_PREFERENCE>(roundCornerStyle);
	if (cornerStyle > DWMWCP_ROUNDSMALL) // || cornerStyle < DWMWCP_DEFAULT) // should never be < 0
	{
		g_dmCfg.m_roundCorner = DWMWCP_DEFAULT;
	}
	else
	{
		g_dmCfg.m_roundCorner = cornerStyle;
	}
}

static constexpr DWORD kDwmwaClrDefaultRGBCheck = 0x00FFFFFF;

/**
 * @brief Sets the preferred border color for window edge on Windows 11.
 *
 * Assigns the given `COLORREF` to the configuration. If the value matches
 * `kDwmwaClrDefaultRGBCheck`, the color is reset to `DWMWA_COLOR_DEFAULT`.
 *
 * @param[in] clr Border color value, or sentinel to reset to system default.
 *
 * @see DWMWA_BORDER_COLOR
 * @see dmlib::setDarkTitleBarEx()
 */
void dmlib::setBorderColorConfig(COLORREF clr)
{
	if (clr == kDwmwaClrDefaultRGBCheck)
	{
		g_dmCfg.m_borderColor = DWMWA_COLOR_DEFAULT;
	}
	else
	{
		g_dmCfg.m_borderColor = clr;
	}
}

/**
 * @brief Sets the Mica effects on Windows 11 setting.
 *
 * Assigns a valid `DWM_SYSTEMBACKDROP_TYPE` to the configuration. If the value exceeds
 * `DWMSBT_TABBEDWINDOW`, it falls back to `DWMSBT_AUTO`.
 *
 * @param[in] mica Integer value representing a `DWM_SYSTEMBACKDROP_TYPE`.
 *
 * @see DWM_SYSTEMBACKDROP_TYPE
 * @see dmlib::setDarkTitleBarEx()
 */
void dmlib::setMicaConfig(UINT mica)
{
	const auto micaType = static_cast<DWM_SYSTEMBACKDROP_TYPE>(mica);
	if (micaType > DWMSBT_TABBEDWINDOW) // || micaType < DWMSBT_AUTO)  // should never be < 0
	{
		g_dmCfg.m_mica = DWMSBT_AUTO;
	}
	else
	{
		g_dmCfg.m_mica = micaType;
	}
}

/**
 * @brief Sets Mica effects on the full window setting.
 *
 * Controls whether Mica should be applied to the entire window
 * or limited to the title bar only.
 *
 * @param[in] extendMica `true` to apply Mica to the full window, `false` for title bar only.
 *
 * @see dmlib::setDarkTitleBarEx()
 */
void dmlib::setMicaExtendedConfig(bool extendMica)
{
	g_dmCfg.m_micaExtend = extendMica;
}

/**
 * @brief Sets dialog colors on title bar on Windows 11 setting.
 *
 * Controls whether title bar should have same colors as dialog window.
 *
 * @param[in] colorize `true` to have title bar to have same colors as dialog window.
 *
 * @see dmlib::setDarkTitleBarEx()
 */
void dmlib::setColorizeTitleBarConfig(bool colorize)
{
	g_dmCfg.m_colorizeTitleBar = colorize;
}

#ifndef _DARKMODELIB_NO_INI_CONFIG
/**
 * @brief Initializes dark mode configuration and colors from an INI file.
 *
 * Loads configuration values from the specified INI file path and applies them to the
 * current dark mode settings. This includes:
 * - Base appearance (`DarkModeType`) and system-following mode (`WinMode`)
 * - Optional Mica and rounded corner styling
 * - Custom colors for background, text, borders, and headers (if present)
 * - Tone settings for dark theme (`ColorTone`)
 *
 * If the INI file does not exist, default dark mode behavior is applied via
 * @ref dmlib::setDarkModeConfigEx.
 *
 * @param[in] iniName Name of INI file (resolved via @ref getIniPath).
 *
 * @note When `DarkModeType::classic` is set, system colors are used instead of themed ones.
 */
static void initOptions(const std::wstring& iniName)
{
	if (iniName.empty())
	{
		return;
	}

	const auto iniPath = dmlib_ini::getIniPath(iniName);
	g_dmCfg.m_iniExist = dmlib_ini::fileExists(iniPath);
	if (g_dmCfg.m_iniExist)
	{
		dmlib::initDarkModeConfig(::GetPrivateProfileIntW(L"main", L"mode", 1, iniPath.c_str()));
		if (g_dmCfg.m_dmType == dmlib::DarkModeType::classic)
		{
			dmlib::setDarkModeConfigEx(static_cast<UINT>(dmlib::DarkModeType::classic));
			dmlib::setDefaultColors(false);
			return;
		}

		const bool useDark = g_dmCfg.m_dmType == dmlib::DarkModeType::dark;

		const std::wstring sectionBase = useDark ? L"dark" : L"light";
		const std::wstring sectionColorsView = sectionBase + L".colors.view";
		const std::wstring sectionColors = sectionBase + L".colors";

		dmlib::setMicaConfig(::GetPrivateProfileIntW(sectionBase.c_str(), L"mica", 0, iniPath.c_str()));
		dmlib::setRoundCornerConfig(::GetPrivateProfileIntW(sectionBase.c_str(), L"roundCorner", 0, iniPath.c_str()));
		dmlib_ini::setClrFromIni(iniPath, sectionBase, L"borderColor", &g_dmCfg.m_borderColor);
		if (g_dmCfg.m_borderColor == kDwmwaClrDefaultRGBCheck)
		{
			g_dmCfg.m_borderColor = DWMWA_COLOR_DEFAULT;
		}

		getThemeView().resetColors(useDark);

		if (useDark)
		{
			UINT tone = ::GetPrivateProfileIntW(sectionBase.c_str(), L"tone", 0, iniPath.c_str());
			if (tone >= static_cast<UINT>(dmlib::ColorTone::max))
			{
				tone = 0;
			}

			getTheme().setToneColors(static_cast<dmlib::ColorTone>(tone));
			getThemeView().setColorHeaderBackground(getTheme().getColors().background);
			getThemeView().setColorHeaderHotBackground(getTheme().getColors().hotBackground);
			getThemeView().setColorHeaderText(getTheme().getColors().darkerText);

			if (!dmlib::isWindowsModeEnabled())
			{
				g_dmCfg.m_micaExtend = (::GetPrivateProfileIntW(sectionBase.c_str(), L"micaExtend", 0, iniPath.c_str()) == 1);
			}
		}
		else
		{
			getTheme().setLightColors();
		}

		struct ColorEntry
		{
			const wchar_t* key = nullptr;
			COLORREF* clr = nullptr;
		};

		static constexpr size_t nColorsViewMembers = 7;
		const std::array<ColorEntry, nColorsViewMembers> viewColors{ {
			{L"backgroundView", &getThemeView().getToSetColors().background},
			{L"textView", &getThemeView().getToSetColors().text},
			{L"gridlines", &getThemeView().getToSetColors().gridlines},
			{L"backgroundHeader", &getThemeView().getToSetColors().headerBackground},
			{L"backgroundHotHeader", &getThemeView().getToSetColors().headerHotBackground},
			{L"textHeader", &getThemeView().getToSetColors().headerText},
			{L"edgeHeader", &getThemeView().getToSetColors().headerEdge}
		} };

		static constexpr size_t nColorsMembers = 13;
		const std::array<ColorEntry, nColorsMembers> baseColors{ {
			{L"background", &getTheme().getToSetColors().background},
			{L"backgroundCtrl", &getTheme().getToSetColors().ctrlBackground},
			{L"backgroundHot", &getTheme().getToSetColors().hotBackground},
			{L"backgroundDlg", &getTheme().getToSetColors().dlgBackground},
			{L"backgroundError", &getTheme().getToSetColors().errorBackground},
			{L"text", &getTheme().getToSetColors().text},
			{L"textItem", &getTheme().getToSetColors().darkerText},
			{L"textDisabled", &getTheme().getToSetColors().disabledText},
			{L"textLink", &getTheme().getToSetColors().linkText},
			{L"edge", &getTheme().getToSetColors().edge},
			{L"edgeHot", &getTheme().getToSetColors().hotEdge},
			{L"edgeDisabled", &getTheme().getToSetColors().disabledEdge},
			{L"highlight", &getTheme().getToSetColors().highlight}
		} };

		for (const auto& entry : viewColors)
		{
			dmlib_ini::setClrFromIni(iniPath, sectionColorsView, entry.key, entry.clr);
		}

		for (const auto& entry : baseColors)
		{
			dmlib_ini::setClrFromIni(iniPath, sectionColors, entry.key, entry.clr);
		}

		dmlib::updateThemeBrushesAndPens();
		dmlib::updateViewBrushesAndPens();
		dmlib::calculateTreeViewStyle();

		if (!g_dmCfg.m_micaExtend)
		{
			g_dmCfg.m_colorizeTitleBar = (::GetPrivateProfileIntW(sectionBase.c_str(), L"colorizeTitleBar", 0, iniPath.c_str()) == 1);
		}

		dmlib_win32api::SetDarkMode(g_dmCfg.m_dmType == dmlib::DarkModeType::dark, true);
	}
	else
	{
		dmlib::setDarkModeConfigEx(static_cast<UINT>(dmlib::DarkModeType::dark));
		dmlib::setDefaultColors(true);
	}
}
#endif // !defined(_DARKMODELIB_NO_INI_CONFIG)

/**
 * @brief Applies dark mode settings based on the given configuration type.
 *
 * Initializes the dark mode type settings and system-following behavior.
 * Enables or disables dark mode depending on whether `DarkModeType::dark` is selected.
 * It is recommended to use together with @ref dmlib::setDefaultColors to also set colors.
 *
 * @param[in] dmType Dark mode configuration type; see @ref dmlib::initDarkModeConfig for values.
 *
 * @see dmlib::setDarkModeConfig()
 * @see dmlib::initDarkModeConfig()
 * @see dmlib::setDefaultColors()
 */
void dmlib::setDarkModeConfigEx(UINT dmType)
{
	dmlib::initDarkModeConfig(dmType);

	const bool useDark = g_dmCfg.m_dmType == DarkModeType::dark;
	dmlib_win32api::SetDarkMode(useDark, true);
}

/**
 * @brief Applies dark mode settings based on system mode preference.
 *
 * Determines the appropriate mode using @ref dmlib::isDarkModeReg and forwards
 * the result to @ref dmlib::setDarkModeConfigEx.
 * It is recommended to use together with @ref dmlib::setDefaultColors to also set colors.
 *
 * Uses:
 * - `DarkModeType::dark` if registry prefers dark mode.
 * - `DarkModeType::classic` otherwise.
 *
 * @see dmlib::setDarkModeConfigEx()
 */
void dmlib::setDarkModeConfig()
{
	const auto dmType = static_cast<UINT>(dmlib::isDarkModeReg() ? DarkModeType::dark : DarkModeType::classic);
	dmlib::setDarkModeConfigEx(dmType);
}

/**
 * @brief Initializes dark mode experimental features, colors, and other settings.
 *
 * Performs one-time setup for dark mode, including:
 * - Initializing experimental features if not yet done.
 * - Optionally loading settings from an INI file (if INI config is enabled).
 * - Initializing TreeView style and applying dark mode settings.
 * - Preparing system colors (e.g. `COLOR_WINDOW`, `COLOR_WINDOWTEXT`, `COLOR_BTNFACE`)
 *   for hooking.
 *
 * @param[in] iniName Optional path to an INI file for dark mode settings (ignored if already set).
 *
 * @note This function is only run once per session;
 *       subsequent calls have no effect, unless follow system mode is used,
 *       then only colors are updated each time system changes mode.
 *
 * @see dmlib::initDarkMode()
 * @see dmlib::calculateTreeViewStyle()
 */
void dmlib::initDarkModeEx([[maybe_unused]] const wchar_t* iniName)
{
	if (!g_dmCfg.m_isInit)
	{
		if (!g_dmCfg.m_isInitExperimental)
		{
			dmlib_win32api::InitDarkMode();
			dmlib_dpi::InitDpiAPI();
			g_dmCfg.m_isInitExperimental = true;
		}

#ifndef _DARKMODELIB_NO_INI_CONFIG
		if (!g_dmCfg.m_isIniNameSet)
		{
			g_dmCfg.m_iniName = iniName;
			g_dmCfg.m_isIniNameSet = true;

			if (g_dmCfg.m_iniName.empty())
			{
				dmlib::setDarkModeConfigEx(static_cast<UINT>(DarkModeType::dark));
				dmlib::setDefaultColors(true);
			}
		}
		initOptions(g_dmCfg.m_iniName);
#else
		dmlib::setDarkModeConfig();
		dmlib::setDefaultColors(true);
#endif

		dmlib::setSysColor(COLOR_WINDOW, dmlib::getCtrlBackgroundColor());
		dmlib::setSysColor(COLOR_WINDOWTEXT, dmlib::getTextColor());
		dmlib::setSysColor(COLOR_3DFACE, dmlib::getViewGridlinesColor());

		dmlib::updateCommonDlgsBrushes();

		g_dmCfg.m_isInit = true;
	}
}

/**
 * @brief Initializes dark mode without INI settings.
 *
 * Forwards to @ref dmlib::initDarkModeEx with an empty INI path, effectively disabling INI settings.
 *
 * @see dmlib::initDarkModeEx()
 */
void dmlib::initDarkMode()
{
	dmlib::initDarkModeEx(L"");
}

/**
 * @brief Checks if there is config INI file.
 *
 * @return `true` if there is config INI file that can be used.
 */
bool dmlib::doesConfigFileExist()
{
#ifndef _DARKMODELIB_NO_INI_CONFIG
	return g_dmCfg.m_iniExist;
#else
	return false;
#endif
}

/**
 * @brief Checks if non-classic mode is enabled.
 *
 * If `_DARKMODELIB_ALLOW_OLD_OS` is defined with value larger than '1',
 * this skips Windows version checks. Otherwise, dark mode is only enabled
 * on Windows 10 or newer.
 *
 * @return `true` if a supported dark mode type is active, otherwise `false`.
 */
bool dmlib::isEnabled()
{
#if defined(_DARKMODELIB_ALLOW_OLD_OS) && (_DARKMODELIB_ALLOW_OLD_OS > 1)
	return g_dmCfg.m_dmType != DarkModeType::classic;
#else
	return dmlib::isAtLeastWindows10() && g_dmCfg.m_dmType != DarkModeType::classic;
#endif
}

/**
 * @brief Checks if experimental dark mode features are currently active.
 *
 * @return `true` if experimental dark mode is enabled.
 */
bool dmlib::isExperimentalActive()
{
	return dmlib_win32api::IsDarkModeActive();
}

/**
 * @brief Checks if experimental dark mode features are supported by the system.
 *
 * @return `true` if dark mode experimental APIs are available.
 */
bool dmlib::isExperimentalSupported()
{
	return dmlib_win32api::IsDarkModeSupported();
}

/**
 * @brief Checks if follow the system mode behavior is enabled.
 *
 * @return `true` if "mode" is not `WinMode::disabled`, i.e. system mode is followed.
 */
bool dmlib::isWindowsModeEnabled()
{
	return g_dmCfg.m_windowsMode != WinMode::disabled;
}

/**
 * @brief Checks if the host OS is at least Windows 10.
 *
 * @return `true` if running on Windows 10 or newer.
 */
bool dmlib::isAtLeastWindows10()
{
	return dmlib_win32api::IsWindows10();
}
/**
 * @brief Checks if the host OS is at least Windows 11.
 *
 * @return `true` if running on Windows 11 or newer.
 */
bool dmlib::isAtLeastWindows11()
{
	return dmlib_win32api::IsWindows11();
}

/**
 * @brief Retrieves the current Windows build number.
 *
 * @return Windows build number reported by the system.
 */
DWORD dmlib::getWindowsBuildNumber()
{
	return dmlib_win32api::GetWindowsBuildNumber();
}

/// Check if OS is Windows 11 version 24H2 build 26100 rev 6899+,
/// Windows 11 version 25H2 build 26200 rev 6899+, or later builds.
static bool doesWin11SupportDarkThemeStyle() noexcept
{
	static const DWORD win11Revision = []() noexcept
	{
		DWORD revisionReg = 0;
		DWORD dwBufSize = sizeof(revisionReg);
		static constexpr LPCWSTR lpSubKey = L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion";
		static constexpr LPCWSTR lpValue = L"UBR";

		if (::RegGetValueW(HKEY_LOCAL_MACHINE, lpSubKey, lpValue, RRF_RT_REG_DWORD, nullptr, &revisionReg, &dwBufSize) == ERROR_SUCCESS)
		{
			return revisionReg;
		}
		return 0UL;
	}();

	static constexpr DWORD win11Build24H2 = 26100;
	static constexpr DWORD win11Build25H2 = 26200;
	static constexpr DWORD minWin11Revision = 6899;
	return dmlib_win32api::GetWindowsBuildNumber() > win11Build25H2
		|| (dmlib_win32api::GetWindowsBuildNumber() == win11Build25H2 && win11Revision >= minWin11Revision)
		|| (dmlib_win32api::GetWindowsBuildNumber() == win11Build24H2 && win11Revision >= minWin11Revision);
}

/**
 * @brief Handles system setting changes related to dark mode.
 *
 * Responds to system messages indicating a color scheme change. If the current
 * dark mode state no longer matches the system registry preference, dark mode is
 * re-initialized.
 *
 * - Skips processing if experimental dark mode is unsupported.
 * - Relies on @ref dmlib::isDarkModeReg for theme preference and skips during high contrast.
 *
 * @param[in] lParam Message parameter (typically from `WM_SETTINGCHANGE`).
 * @return `true` if a dark mode change was handled; otherwise `false`.
 *
 * @see dmlib::isDarkModeReg()
 * @see dmlib::initDarkMode()
 */
bool dmlib::handleSettingChange(LPARAM lParam)
{
	if (dmlib::isExperimentalSupported()
		&& dmlib_win32api::IsColorSchemeChangeMessage(lParam))
	{
		// fnShouldAppsUseDarkMode (ordinal 132) is not reliable on 1903+, use dmlib::isDarkModeReg() instead
		if (const bool isDarkModeUsed = (dmlib::isDarkModeReg() && !dmlib_win32api::IsHighContrast());
			dmlib::isExperimentalActive() != isDarkModeUsed
			&& g_dmCfg.m_isInit)
		{
			g_dmCfg.m_isInit = false;
			dmlib::initDarkMode();
		}
		return true;
	}
	return false;
}

/**
 * @brief Checks if dark mode is enabled in the Windows registry.
 *
 * Queries `HKEY_CURRENT_USER\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize\\AppsUseLightTheme`.
 *
 * @return `true` if dark mode is preferred (value is `0`); otherwise `false`.
 */
bool dmlib::isDarkModeReg()
{
	DWORD data{};
	DWORD dwBufSize = sizeof(data);
	static constexpr LPCWSTR lpSubKey = L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize";
	static constexpr LPCWSTR lpValue = L"AppsUseLightTheme";

	if (::RegGetValueW(HKEY_CURRENT_USER, lpSubKey, lpValue, RRF_RT_REG_DWORD, nullptr, &data, &dwBufSize) == ERROR_SUCCESS)
	{
		// dark mode is 0, light mode is 1
		return data == 0UL;
	}
	return false;
}

/**
 * @brief Overrides a specific system color with a custom color.
 *
 * Currently supports:
 * - `COLOR_WINDOW`: Background of ComboBoxEx list.
 * - `COLOR_WINDOWTEXT`: Text color of ComboBoxEx list.
 * - `COLOR_BTNFACE`: Gridline color in ListView (when applicable).
 *
 * @param[in]   nIndex  One of the supported system color indices.
 * @param[in]   color   Custom `COLORREF` value to apply.
 */
void dmlib::setSysColor(int nIndex, COLORREF color)
{
	dmlib_hook::setMySysColor(nIndex, color);
}
/**
 * @brief Updates custom color brushes for ChooseFont and ChooseColor dialogs.
 */
void dmlib::updateCommonDlgsBrushes()
{
	dmlib_hook::updateFontBrush();
	dmlib_hook::updateLumSliderBrush();
}

/**
 * @brief Makes scroll bars on the specified window and all its children consistent.
 *
 * @note Currently not widely used by default.
 *
 * @param[in] hWnd Handle to the parent window.
 */
void dmlib::enableDarkScrollBarForWindowAndChildren([[maybe_unused]] HWND hWnd)
{
#if defined(_DARKMODELIB_USE_SCROLLBAR_FIX) && (_DARKMODELIB_USE_SCROLLBAR_FIX > 0)
	dmlib_hook::enableDarkScrollBarForWindowAndChildren(hWnd);
#endif
}

/**
 * @brief Checks if current mode is dark type.
 */
bool dmlib::isDarkDmTypeUsed() noexcept
{
	return g_dmCfg.m_dmType == dmlib::DarkModeType::dark;
}

/**
 * @brief Applies themed owner drawn subclassing to a checkbox, radio, or tri-state button control.
 *
 * Associates a `ButtonData` instance with the control.
 *
 * @param[in] hWnd Handle to the checkbox, radio, or tri-state button control.
 *
 * @see dmlib_subclass::ButtonSubclass()
 * @see dmlib::removeCheckboxOrRadioBtnCtrlSubclass()
 */
void dmlib::setCheckboxOrRadioBtnCtrlSubclass(HWND hWnd)
{
	dmlib_subclass::SetSubclass<dmlib_subclass::ButtonData>(hWnd, dmlib_subclass::ButtonSubclass, dmlib_subclass::SubclassID::button, hWnd);
}

/**
 * @brief Removes the owner drawn subclass from a checkbox, radio, or tri-state button control.
 *
 * Cleans up the `ButtonData` instance and detaches the control's subclass proc.
 *
 * @param[in] hWnd Handle to the previously subclassed control.
 *
 * @see dmlib_subclass::ButtonSubclass()
 * @see dmlib::setCheckboxOrRadioBtnCtrlSubclass()
 */
void dmlib::removeCheckboxOrRadioBtnCtrlSubclass(HWND hWnd)
{
	dmlib_subclass::RemoveSubclass<dmlib_subclass::ButtonData>(hWnd, dmlib_subclass::ButtonSubclass, dmlib_subclass::SubclassID::button);
}

/**
 * @brief Applies owner drawn subclassing to a groupbox button control.
 *
 * Associates a `ButtonData` instance with the control.
 *
 * @param[in] hWnd Handle to the groupbox button control.
 *
 * @see dmlib_subclass::GroupboxSubclass()
 * @see dmlib::removeGroupboxCtrlSubclass()
 */
void dmlib::setGroupboxCtrlSubclass(HWND hWnd)
{
	dmlib_subclass::SetSubclass<dmlib_subclass::ButtonData>(hWnd, dmlib_subclass::GroupboxSubclass, dmlib_subclass::SubclassID::groupbox);
}

/**
 * @brief Removes the owner drawn subclass from a groupbox button control.
 *
 * Cleans up the `ButtonData` instance and detaches the control's subclass proc.
 *
 * @param[in] hWnd Handle to the previously subclassed control.
 *
 * @see dmlib_subclass::GroupboxSubclass()
 * @see dmlib::setGroupboxCtrlSubclass()
 */
void dmlib::removeGroupboxCtrlSubclass(HWND hWnd)
{
	dmlib_subclass::RemoveSubclass<dmlib_subclass::ButtonData>(hWnd, dmlib_subclass::GroupboxSubclass, dmlib_subclass::SubclassID::groupbox);
}

/**
 * @brief Applies theming and/or subclassing to a button control based on its style.
 *
 * Inspects the control's style (`BS_*`) to determine its visual category and applies
 * apropriate theming and/or subclassing accordingly. Handles:
 * - Checkbox/radio/tri-state buttons: Applies theme (optional) and optional subclassing
 * - Group boxes: Applies subclassing for dark mode drawing
 * - Push buttons: Applies visual theming if requested
 *
 * The behavior varies depending on dark mode support, Windows version, and the flags
 * provided in @ref DarkModeParams.
 *
 * @param[in]   hWnd    Handle to the target button control.
 * @param[in]   p       Parameters defining theming and subclassing behavior.
 *
 * @see DarkModeParams
 * @see dmlib::setCheckboxOrRadioBtnCtrlSubclass()
 * @see dmlib::setGroupboxCtrlSubclass()
 */
static void setBtnCtrlSubclassAndTheme(HWND hWnd, DarkModeParams p) noexcept
{
	const auto nBtnStyle = ::GetWindowLongPtr(hWnd, GWL_STYLE);
	switch (nBtnStyle & BS_TYPEMASK)
	{
		case BS_CHECKBOX:
		case BS_AUTOCHECKBOX:
		case BS_3STATE:
		case BS_AUTO3STATE:
		case BS_RADIOBUTTON:
		case BS_AUTORADIOBUTTON:
		{
			if ((nBtnStyle & BS_PUSHLIKE) == BS_PUSHLIKE)
			{
				if (p.m_theme)
				{
					::SetWindowTheme(hWnd, p.m_themeClassName, nullptr);
				}
				break;
			}

			if (dmlib::isAtLeastWindows11() && p.m_theme)
			{
				::SetWindowTheme(hWnd, p.m_themeClassName, nullptr);
			}

			if (p.m_subclass)
			{
				dmlib::setCheckboxOrRadioBtnCtrlSubclass(hWnd);
			}
			break;
		}

		case BS_GROUPBOX:
		{
			if (p.m_subclass)
			{
				dmlib::setGroupboxCtrlSubclass(hWnd);
			}
			break;
		}

		case BS_PUSHBUTTON:
		case BS_DEFPUSHBUTTON:
		case BS_SPLITBUTTON:
		case BS_DEFSPLITBUTTON:
		{
			if (p.m_theme)
			{
				::SetWindowTheme(hWnd, p.m_themeClassName, nullptr);
			}
			break;
		}

		default:
		{
			break;
		}
	}
}

/**
 * @brief Applies owner drawn subclassing and theming to an up-down (spinner) control.
 *
 * Associates a `UpDownData` instance with the control.
 *
 * @param[in] hWnd Handle to the up-down (spinner) control.
 *
 * @see dmlib_subclass::UpDownSubclass()
 * @see dmlib::removeUpDownCtrlSubclass()
 */
void dmlib::setUpDownCtrlSubclass(HWND hWnd)
{
	dmlib_subclass::SetSubclass<dmlib_subclass::UpDownData>(hWnd, dmlib_subclass::UpDownSubclass, dmlib_subclass::SubclassID::upDown, hWnd);
}

/**
 * @brief Removes the owner drawn subclass from a up-down (spinner) control.
 *
 * Cleans up the `UpDownData` instance and detaches the control's subclass proc.
 *
 * @param[in] hWnd Handle to the previously subclassed control.
 *
 * @see dmlib_subclass::UpDownSubclass()
 * @see dmlib::setUpDownCtrlSubclass()
 */
void dmlib::removeUpDownCtrlSubclass(HWND hWnd)
{
	dmlib_subclass::RemoveSubclass<dmlib_subclass::UpDownData>(hWnd, dmlib_subclass::UpDownSubclass, dmlib_subclass::SubclassID::upDown);
}

/**
 * @brief Applies up-down (spinner) control theming and/or subclassing based on specified parameters.
 *
 * Conditionally applies custom subclassing and/or themed appearance
 * depending on `DarkModeParams`. Subclassing takes priority if both are requested.
 *
 * @param[in]   hWnd    Handle to the up-down control.
 * @param[in]   p       Parameters controlling whether to apply theming and/or subclassing.
 *
 * @see DarkModeParams
 * @see dmlib::setUpDownCtrlSubclass()
 */
static void setUpDownCtrlSubclassAndTheme(HWND hWnd, DarkModeParams p) DMLIB_MEM_NOEXCEPT
{
	if (p.m_theme)
	{
		::SetWindowTheme(hWnd, p.m_themeClassName, nullptr);
	}

	if (p.m_subclass)
	{
		dmlib::setUpDownCtrlSubclass(hWnd);
	}
}

/**
 * @brief Applies owner drawn subclassing to a tab control.
 *
 * @param[in] hWnd Handle to the tab control.
 *
 * @see dmlib_subclass::TabPaintSubclass()
 * @see removeTabCtrlPaintSubclass()
 */
static void setTabCtrlPaintSubclass(HWND hWnd) DMLIB_MEM_NOEXCEPT
{
	dmlib_subclass::SetSubclass<dmlib_subclass::TabData>(hWnd, dmlib_subclass::TabPaintSubclass, dmlib_subclass::SubclassID::tabPaint);
}

/**
 * @brief Removes the owner drawn subclass from a tab control.
 *
 * Cleans up the `TabData` instance and detaches the control's subclass proc.
 *
 * @param[in] hWnd Handle to the previously subclassed tab control.
 *
 * @see dmlib_subclass::TabPaintSubclass()
 * @see setTabCtrlPaintSubclass()
 */
static void removeTabCtrlPaintSubclass(HWND hWnd) noexcept
{
	dmlib_subclass::RemoveSubclass<dmlib_subclass::TabData>(hWnd, dmlib_subclass::TabPaintSubclass, dmlib_subclass::SubclassID::tabPaint);
}

/**
 * @brief Applies a subclass to detect and subclass tab control's up-down (spinner) child.
 *
 * Enable automatic subclassing of the up-down (spinner) control
 * when it's created dynamically (for `TCS_SCROLLOPPOSITE` or overflow).
 *
 * @param[in] hWnd Handle to the tab control.
 *
 * @see dmlib_subclass::TabUpDownSubclass()
 * @see dmlib::removeTabCtrlUpDownSubclass()
 */
void dmlib::setTabCtrlUpDownSubclass(HWND hWnd)
{
	dmlib_subclass::SetSubclass(hWnd, dmlib_subclass::TabUpDownSubclass, dmlib_subclass::SubclassID::tabUpDown);
}

/**
 * @brief Removes the subclass procedure for a tab control's up-down (spinner) child detection.
 *
 * Detaches the control's subclass proc.
 *
 * @param[in] hWnd Handle to the previously subclassed tab control.
 *
 * @see dmlib_subclass::TabUpDownSubclass()
 * @see dmlib::setTabCtrlUpDownSubclass()
 */
void dmlib::removeTabCtrlUpDownSubclass(HWND hWnd)
{
	dmlib_subclass::RemoveSubclass(hWnd, dmlib_subclass::TabUpDownSubclass, dmlib_subclass::SubclassID::tabUpDown);
}

/**
 * @brief Applies owner drawn and up-down (spinner) child detection subclassings for a tab control.
 *
 * Applies both @ref dmlib::TabPaintSubclass() for custom drawing
 * and @ref dmlib::TabUpDownSubclass() for detecting and subclassing
 * the associated up-down (spinner) control.
 *
 * @param[in] hWnd Handle to the tab control.
 *
 * @see dmlib::removeTabCtrlSubclass()
 * @see setTabCtrlPaintSubclass()
 * @see dmlib::setTabCtrlUpDownSubclass()
 */
void dmlib::setTabCtrlSubclass(HWND hWnd)
{
	setTabCtrlPaintSubclass(hWnd);
	dmlib::setTabCtrlUpDownSubclass(hWnd);
}

/**
 * @brief Removes owner drawn and up-down (spinner) child detection subclasses.
 *
 * Detaches the control's subclass procs.
 *
 * @param[in] hWnd Handle to the previously subclassed tab control.
 *
 * @see dmlib::setTabCtrlSubclass()
 * @see removeTabCtrlPaintSubclass()
 * @see dmlib::removeTabCtrlUpDownSubclass()
 */
void dmlib::removeTabCtrlSubclass(HWND hWnd)
{
	removeTabCtrlPaintSubclass(hWnd);
	dmlib::removeTabCtrlUpDownSubclass(hWnd);
}

/**
 * @brief Applies tab control theming and subclassing based on specified parameters.
 *
 * Conditionally applies tooltip theming and tab control subclassing
 * depending on `DarkModeParams`.
 *
 * @param[in]   hWnd    Handle to the tab control.
 * @param[in]   p       Parameters controlling whether to apply theming and/or subclassing.
 *
 * @see DarkModeParams
 * @see dmlib::setDarkTooltips()
 * @see dmlib::setTabCtrlSubclass()
 */
static void setTabCtrlSubclassAndTheme(HWND hWnd, DarkModeParams p) noexcept
{
	if (p.m_theme)
	{
		dmlib::setDarkTooltips(hWnd, static_cast<int>(dmlib::ToolTipsType::tabbar));
		if (doesWin11SupportDarkThemeStyle() && dmlib_subclass::isThemePrefered())
		{
			dmlib::setDarkThemeTheme(hWnd);
		}
	}

	if (p.m_subclass && !dmlib_subclass::isThemePrefered())
	{
		dmlib::setTabCtrlSubclass(hWnd);
	}
}

/**
 * @brief Applies owner drawn custom border subclassing to a list box or edit control.
 *
 * @param[in] hWnd Handle to the list box or edit control.
 *
 * @see dmlib_subclass::CustomBorderSubclass()
 * @see dmlib::removeCustomBorderForListBoxOrEditCtrlSubclass()
 */
void dmlib::setCustomBorderForListBoxOrEditCtrlSubclass(HWND hWnd)
{
	dmlib_subclass::SetSubclass<dmlib_subclass::BorderMetricsData>(hWnd, dmlib_subclass::CustomBorderSubclass, dmlib_subclass::SubclassID::customBorder, hWnd);
}

/**
 * @brief Removes the custom border subclass from a list box or edit control.
 *
 * Cleans up the `BorderMetricsData` and detaches the control's subclass proc,
 * restoring the control's default border drawing.
 *
 * @param[in] hWnd Handle to the previously subclassed control.
 *
 * @see dmlib_subclass::CustomBorderSubclass()
 * @see dmlib::setCustomBorderForListBoxOrEditCtrlSubclass()
 */
void dmlib::removeCustomBorderForListBoxOrEditCtrlSubclass(HWND hWnd)
{
	dmlib_subclass::RemoveSubclass<dmlib_subclass::BorderMetricsData>(hWnd, dmlib_subclass::CustomBorderSubclass, dmlib_subclass::SubclassID::customBorder);
}

/**
 * @brief Applies theming and optional custom border subclassing to a list box or edit control.
 *
 * Conditionally configures the visual style of a list box or edit control
 * depending on `DarkModeParams`, control type, and window styles.
 * Applies a custom border subclass for controls with `WS_EX_CLIENTEDGE` flag.
 * Toggle the client edge style depending on dark mode state.
 *
 * @param[in]   hWnd        Handle to the target list box or edit control.
 * @param[in]   p           Parameters controlling whether to apply theming and/or subclassing.
 * @param[in]   isListBox   `true` if the control is a list box, `false` if it's an edit control.
 *
 * @note Custom border subclassing is skipped for combo box list boxes.
 *
 * @see DarkModeParams
 * @see dmlib::setCustomBorderForListBoxOrEditCtrlSubclass()
 */
static void setCustomBorderForListBoxOrEditCtrlSubclassAndTheme(HWND hWnd, DarkModeParams p, bool isListBox) DMLIB_MEM_NOEXCEPT
{
	const auto nStyle = ::GetWindowLongPtr(hWnd, GWL_STYLE);
	const bool hasScrollBar = ((nStyle & WS_HSCROLL) == WS_HSCROLL) || ((nStyle & WS_VSCROLL) == WS_VSCROLL);

	// edit control without scroll bars
	if (dmlib_subclass::isThemePrefered()
		&& p.m_theme
		&& !isListBox
		&& !hasScrollBar)
	{
		if (doesWin11SupportDarkThemeStyle())
		{
			dmlib::setDarkThemeTheme(hWnd);
		}
		else
		{
			dmlib::setDarkThemeExperimentalEx(hWnd, L"CFD");
		}
	}
	else
	{
		if (p.m_theme && (isListBox || hasScrollBar))
		{
			// dark scroll bars for list box or edit control
			::SetWindowTheme(hWnd, p.m_themeClassName, nullptr);
		}

		const auto nExStyle = ::GetWindowLongPtr(hWnd, GWL_EXSTYLE);
		const bool hasClientEdge = (nExStyle & WS_EX_CLIENTEDGE) == WS_EX_CLIENTEDGE;

		if (const bool isCBoxListBox = isListBox && (nStyle & LBS_COMBOBOX) == LBS_COMBOBOX;
			p.m_subclass && hasClientEdge && !isCBoxListBox)
		{
			dmlib::setCustomBorderForListBoxOrEditCtrlSubclass(hWnd);
		}

		if (::GetWindowSubclass(hWnd, dmlib_subclass::CustomBorderSubclass, static_cast<UINT_PTR>(dmlib_subclass::SubclassID::customBorder), nullptr) == TRUE)
		{
			const bool enableClientEdge = !dmlib::isEnabled();
			dmlib::setWindowExStyle(hWnd, enableClientEdge, WS_EX_CLIENTEDGE);
		}
	}
}

/**
 * @brief Applies owner drawn subclassing to a combo box control.
 *
 * Retrieves the combo box style from the window and passes it to the subclass data
 * (`ComboBoxData`) so the paint routine can adapt to that combo box style.
 *
 * @param[in] hWnd Handle to the combo box control.
 *
 * @note Uses `GetWindowLongPtr` to extract the style bits.
 *
 * @see dmlib_subclass::ComboBoxSubclass()
 * @see dmlib::removeComboBoxCtrlSubclass()
 */
void dmlib::setComboBoxCtrlSubclass(HWND hWnd)
{
	const auto nStyle = ::GetWindowLongPtr(hWnd, GWL_STYLE);
	const auto cbStyle = nStyle & CBS_DROPDOWNLIST;
	if (cbStyle != CBS_DROPDOWNLIST)
	{
		::SetWindowLongPtr(hWnd, GWL_STYLE, nStyle | WS_CLIPCHILDREN);
	}
	dmlib_subclass::SetSubclass<dmlib_subclass::ComboBoxData>(hWnd, dmlib_subclass::ComboBoxSubclass, dmlib_subclass::SubclassID::comboBox, cbStyle);
}

/**
 * @brief Removes the owner drawn subclass from a combo box control.
 *
 * Cleans up the `ComboBoxData` and detaches the control's subclass proc.
 *
 * @param[in] hWnd Handle to the combo box control.
 *
 * @see dmlib_subclass::ComboBoxSubclass()
 * @see dmlib::setComboBoxCtrlSubclass()
 */
void dmlib::removeComboBoxCtrlSubclass(HWND hWnd)
{
	dmlib_subclass::RemoveSubclass<dmlib_subclass::ComboBoxData>(hWnd, dmlib_subclass::ComboBoxSubclass, dmlib_subclass::SubclassID::comboBox);
}

/**
 * @brief Applies theming and optional subclassing to a combo box control.
 *
 * Configures a combo box' appearance and behavior based on its style,
 * the provided parameters, and current theme preferences.
 *
 * Behavior:
 * - If theming is enabled (`p.m_theme`) and the combo box has an associated list box:
 *   - For `CBS_SIMPLE`, replaces the client edge with a custom border for non-classic mode.
 *   - Applies themed scroll bars.
 * - If subclassing is enabled (`p.m_subclass`) and dark mode is not the preferred theme:
 *   - Applies a combo box subclassing unless the parent is a `ComboBoxEx` control.
 * - If theming is enabled (`p.m_theme`):
 *   - Applies the experimental `"CFD"` dark theme to the combo box for a light drop-down arrow.
 *   - Clears the edit selection for non-`CBS_DROPDOWNLIST` styles to avoid visual artifacts.
 *
 * @param[in]   hWnd    Handle to the combo box control.
 * @param[in]   p       Parameters controlling whether to apply theming and/or subclassing.
 *
 * @note Skips subclassing for `ComboBoxEx` parents to avoid conflicts.
 *
 * @see DarkModeParams
 * @see dmlib::setComboBoxCtrlSubclass()
 */
static void setComboBoxCtrlSubclassAndTheme(HWND hWnd, DarkModeParams p) DMLIB_MEM_NOEXCEPT
{
	const auto cbStyle = ::GetWindowLongPtr(hWnd, GWL_STYLE) & CBS_DROPDOWNLIST;
	const bool isCbList = cbStyle == CBS_DROPDOWNLIST;
	const bool isCbSimple = cbStyle == CBS_SIMPLE;

	if (cbStyle == 0) // something wrong happened
	{
		return;
	}

	COMBOBOXINFO cbi{};
	cbi.cbSize = sizeof(COMBOBOXINFO);
	if (::GetComboBoxInfo(hWnd, &cbi) == TRUE
		&& p.m_theme
		&& cbi.hwndList != nullptr)
	{
		if (isCbSimple)
		{
			dmlib::replaceClientEdgeWithBorderSafe(cbi.hwndList);
		}

		// dark scroll bar for list box of combo box
		::SetWindowTheme(cbi.hwndList, p.m_themeClassName, nullptr);
	}

	if (!dmlib_subclass::isThemePrefered() && p.m_subclass)
	{
		if (HWND hParent = ::GetParent(hWnd);
			hParent == nullptr
			|| !dmlib_subclass::WndClassName::cmpWndClassName(hParent, WC_COMBOBOXEX))
		{
			dmlib::setComboBoxCtrlSubclass(hWnd);
		}
	}

	if (p.m_theme) // for light dropdown arrow in dark mode
	{
		if (HWND hParent = ::GetParent(hWnd);
			doesWin11SupportDarkThemeStyle()
			&& (hParent == nullptr
				|| !dmlib_subclass::WndClassName::cmpWndClassName(hParent, WC_COMBOBOXEX)))
		{
			dmlib::setDarkThemeTheme(hWnd);
		}
		else
		{
			dmlib::setDarkThemeExperimentalEx(hWnd, L"CFD");
		}

		if (!isCbList)
		{
			::SendMessage(hWnd, CB_SETEDITSEL, 0, 0); // clear selection
		}
	}
}

/**
 * @brief Applies subclassing to a ComboBoxEx control to handle its child list box and edit controls.
 *
 * @param[in] hWnd Handle to the ComboBoxEx control.
 *
 * @note Uses IAT hooking for custom colors.
 *
 * @see dmlib_subclass::ComboBoxSubclass()
 * @see dmlib::removeComboBoxExCtrlSubclass()
 */
void dmlib::setComboBoxExCtrlSubclass(HWND hWnd)
{
	dmlib_subclass::SetSubclass(hWnd, dmlib_subclass::ComboBoxExSubclass, dmlib_subclass::SubclassID::comboBoxEx);
}

/**
 * @brief Removes the child handling subclass from a ComboBoxEx control.
 *
 * Detaches the control's subclass proc and unhooks system color changes.
 *
 * @param[in] hWnd Handle to the ComboBoxEx control.
 *
 * @see dmlib_subclass::ComboBoxSubclass()
 * @see dmlib::setComboBoxExCtrlSubclass()
 */
void dmlib::removeComboBoxExCtrlSubclass(HWND hWnd)
{
	dmlib_subclass::RemoveSubclass(hWnd, dmlib_subclass::ComboBoxExSubclass, dmlib_subclass::SubclassID::comboBoxEx);
	dmlib_hook::GetSysColor::unhook();
}

/**
 * @brief Applies subclassing to a ComboBoxEx control to handle its child list box and edit controls.
 *
 * Overload wrapper that applies the subclass only if `p.m_subclass` is `true`.
 *
 * @param[in]   hWnd    Handle to the ComboBoxEx control.
 * @param[in]   p       Parameters controlling whether to apply subclassing.
 *
 * @see dmlib::setComboBoxExCtrlSubclass()
 */
static void setComboBoxExCtrlSubclass(HWND hWnd, DarkModeParams p) noexcept
{
	if (p.m_subclass)
	{
		dmlib::setComboBoxExCtrlSubclass(hWnd);
	}
}

/**
 * @brief Applies subclassing to a list view control to handle custom colors.
 *
 * Handles custom colors for gridlines, header text, and in-place edit controls.
 *
 * @param[in] hWnd Handle to the list view control.
 *
 * @note Uses IAT hooking for gridlines colors.
 *
 * @see dmlib_subclass::ListViewSubclass()
 * @see dmlib::removeListViewCtrlSubclass()
 */
void dmlib::setListViewCtrlSubclass(HWND hWnd)
{
	dmlib_subclass::SetSubclass(hWnd, dmlib_subclass::ListViewSubclass, dmlib_subclass::SubclassID::listView);
}

/**
 * @brief Removes the custom colors handling subclass from a list view control.
 *
 * Detaches the control's subclass proc.
 *
 * @param[in] hWnd Handle to the list view control.
 *
 * @see dmlib_subclass::ListViewSubclass()
 * @see dmlib::setListViewCtrlSubclass()
 */
void dmlib::removeListViewCtrlSubclass(HWND hWnd)
{
	dmlib_subclass::RemoveSubclass(hWnd, dmlib_subclass::ListViewSubclass, dmlib_subclass::SubclassID::listView);
}

/**
 * @brief Applies theming and optional subclassing to a list view control.
 *
 * Configures colors, header theming, checkbox styling, and tooltips theming.
 * Optionally installs the list view and header control subclasses for custom drawing.
 * Enables double-buffering via `LVS_EX_DOUBLEBUFFER` flag.
 *
 * @param[in]   hWnd    Handle to the list view control.
 * @param[in]   p       Parameters controlling whether to apply theming and/or subclassing.
 *
 * @see dmlib::setDarkListView()
 * @see dmlib::setDarkListViewCheckboxes()
 * @see dmlib::setDarkTooltips()
 * @see dmlib::setListViewCtrlSubclass()
 * @see dmlib::setHeaderCtrlSubclass()
 */
static void setListViewCtrlSubclassAndTheme(HWND hWnd, DarkModeParams p) DMLIB_MEM_NOEXCEPT
{
	auto* hHeader = ListView_GetHeader(hWnd);

	if (p.m_theme)
	{
		ListView_SetTextColor(hWnd, dmlib::getViewTextColor());
		ListView_SetTextBkColor(hWnd, dmlib::getViewBackgroundColor());
		ListView_SetBkColor(hWnd, dmlib::getViewBackgroundColor());

		dmlib::setDarkListView(hWnd);
		dmlib::setDarkListViewCheckboxes(hWnd);
		dmlib::setDarkTooltips(hWnd, static_cast<int>(dmlib::ToolTipsType::listview));

		if (dmlib_subclass::isThemePrefered())
		{
			if (doesWin11SupportDarkThemeStyle())
			{
				dmlib::setDarkThemeTheme(hHeader);
			}
			else
			{
				dmlib::setDarkThemeExperimentalEx(hHeader, L"ItemsView");
			}
		}

		const bool isDisabled = dmlib::isEnabled() && ::IsWindowEnabled(hWnd) == FALSE;
		dmlib::replaceClientEdgeWithBorderSafeEx(hWnd, isDisabled);
	}

	if (p.m_subclass)
	{
		if (!dmlib_subclass::isThemePrefered())
		{
			dmlib::setHeaderCtrlSubclass(hHeader);
		}

		const auto lvExStyle = ListView_GetExtendedListViewStyle(hWnd);
		ListView_SetExtendedListViewStyle(hWnd, lvExStyle | LVS_EX_DOUBLEBUFFER);
		dmlib::setListViewCtrlSubclass(hWnd);
	}
}

/**
 * @brief Applies owner drawn subclassing to a header control.
 *
 * Retrieves the header button style from the window and passes it to the subclass data
 * (`HeaderData`) so the paint routine can adapt to that header style.
 *
 * @param[in] hWnd Handle to the header control.
 *
 * @note Uses `GetWindowLongPtr` to extract the style bits.
 *
 * @see dmlib_subclass::HeaderSubclass()
 * @see dmlib::removeHeaderCtrlSubclass()
 */
void dmlib::setHeaderCtrlSubclass(HWND hWnd)
{
	dmlib_subclass::SetSubclass<dmlib_subclass::HeaderData>(hWnd, dmlib_subclass::HeaderSubclass, dmlib_subclass::SubclassID::header, hWnd);
}

/**
 * @brief Removes the owner drawn subclass from a header control.
 *
 * Cleans up the `HeaderData` and detaches the control's subclass proc.
 *
 * @param[in] hWnd Handle to the header control.
 *
 * @see dmlib_subclass::HeaderSubclass()
 * @see dmlib::setHeaderCtrlSubclass()
 */
void dmlib::removeHeaderCtrlSubclass(HWND hWnd)
{
	dmlib_subclass::RemoveSubclass<dmlib_subclass::HeaderData>(hWnd, dmlib_subclass::HeaderSubclass, dmlib_subclass::SubclassID::header);
}

/**
 * @brief Applies owner drawn subclassing to a status bar control.
 *
 * Retrieves the status bar system font and passes it to the subclass data
 * (`StatusBarData`).
 *
 * @param[in] hWnd Handle to the status bar control.
 *
 * @note Uses `SystemParametersInfoW` to extract the `lfStatusFont` font.
 *
 * @see dmlib_subclass::StatusBarSubclass()
 * @see dmlib::removeStatusBarCtrlSubclass()
 */
void dmlib::setStatusBarCtrlSubclass(HWND hWnd)
{
	const auto lf = LOGFONT{ dmlib_dpi::getSysFontForDpi(::GetParent(hWnd), dmlib_dpi::FontType::status) };
	dmlib_subclass::SetSubclass<dmlib_subclass::StatusBarData>(hWnd, dmlib_subclass::StatusBarSubclass, dmlib_subclass::SubclassID::statusBar, ::CreateFontIndirectW(&lf));
}

/**
 * @brief Removes the owner drawn subclass from a status bar control.
 *
 * Cleans up the `StatusBarData` and detaches the control's subclass proc.
 *
 * @param[in] hWnd Handle to the status bar control.
 *
 * @see dmlib_subclass::StatusBarSubclass()
 * @see dmlib::setStatusBarCtrlSubclass()
 */
void dmlib::removeStatusBarCtrlSubclass(HWND hWnd)
{
	dmlib_subclass::RemoveSubclass<dmlib_subclass::StatusBarData>(hWnd, dmlib_subclass::StatusBarSubclass, dmlib_subclass::SubclassID::statusBar);
}

/**
 * @brief Applies owner drawn subclassing to a status bar control.
 *
 * Overload wrapper that applies the subclass only if `p.m_subclass` is `true`.
 *
 * @param[in]   hWnd    Handle to the status bar control.
 * @param[in]   p       Parameters controlling whether to apply subclassing.
 *
 * @see dmlib::setStatusBarCtrlSubclass()
 */
static void setStatusBarCtrlSubclass(HWND hWnd, DarkModeParams p) noexcept
{
	if (p.m_theme
		&& dmlib_subclass::isThemePrefered()
		&& doesWin11SupportDarkThemeStyle())
	{
		dmlib::setDarkThemeTheme(hWnd);
	}
	else if (p.m_subclass)
	{
		dmlib::setStatusBarCtrlSubclass(hWnd);
	}
}

/**
 * @brief Applies owner drawn subclassing to a progress bar control.
 *
 * Retrieves the progress bar state information and passes it to the subclass data
 * (`ProgressBarData`).
 *
 * @param[in] hWnd Handle to the progress bar control.
 *
 * @note Uses `PBM_GETSTATE` to determine the current visual state.
 *
 * @see dmlib_subclass::ProgressBarSubclass()
 * @see dmlib::removeProgressBarCtrlSubclass()
 */
void dmlib::setProgressBarCtrlSubclass(HWND hWnd)
{
	dmlib_subclass::SetSubclass<dmlib_subclass::ProgressBarData>(hWnd, dmlib_subclass::ProgressBarSubclass, dmlib_subclass::SubclassID::progressBar, hWnd);
}

/**
 * @brief Removes the owner drawn subclass from a progress bar control.
 *
 * Cleans up the `ProgressBarData` and detaches the control's subclass proc.
 *
 * @param[in] hWnd Handle to the progress bar control.
 *
 * @see dmlib_subclass::ProgressBarSubclass()
 * @see dmlib::setProgressBarCtrlSubclass()
 */
void dmlib::removeProgressBarCtrlSubclass(HWND hWnd)
{
	dmlib_subclass::RemoveSubclass<dmlib_subclass::ProgressBarData>(hWnd, dmlib_subclass::ProgressBarSubclass, dmlib_subclass::SubclassID::progressBar);
}

/**
 * @brief Applies theming or subclassing to a progress bar control based on style and parameters.
 *
 * Conditionally applies either the classic theme or applies the owner drawn subclassing
 * depending on the control style and `DarkModeParams`.
 *
 * Behavior:
 * - If `p.m_theme` is `true` and the control uses `PBS_MARQUEE`, applies classic theme.
 * - Otherwise, if `p.m_subclass` is `true`, applies owner drawn subclassing.
 *
 * @param[in]   hWnd    Handle to the progress bar control.
 * @param[in]   p       Parameters controlling whether to apply theming or subclassing.
 *
 * @see dmlib::setProgressBarClassicTheme()
 * @see dmlib::setProgressBarCtrlSubclass()
 */
static void setProgressBarCtrlSubclass(HWND hWnd, DarkModeParams p) noexcept
{
	const auto nStyle = ::GetWindowLongPtr(hWnd, GWL_STYLE);
	if (p.m_theme)
	{
		if ((nStyle & PBS_MARQUEE) == PBS_MARQUEE)
		{
			dmlib::setProgressBarClassicTheme(hWnd);
		}
		else if (doesWin11SupportDarkThemeStyle())
		{
			::SetWindowTheme(hWnd, dmlib::isExperimentalActive() ? L"DarkMode_CopyEngine" : nullptr, nullptr);
		}
	}

	if (p.m_subclass && ((nStyle & PBS_MARQUEE) != PBS_MARQUEE))
	{
		dmlib::setProgressBarCtrlSubclass(hWnd);
	}
}

/**
 * @brief Applies workaround subclassing to a static control to handle visual glitch in disabled state.
 *
 * Retrieves the static control enabled state information and passes it to the subclass data
 * (`StaticTextData`) to handle visual glitch with static text in disabled state
 * via handling `WM_ENABLE` message.
 *
 * @param[in] hWnd Handle to the static control.
 *
 * @note
 * - Uses `IsWindowEnabled` to determine the current enabled state.
 * - Works only if `WM_ENABLE` message is sent.
 *
 * @see dmlib_subclass::StaticTextSubclass()
 * @see dmlib::removeStaticTextCtrlSubclass()
 */
void dmlib::setStaticTextCtrlSubclass(HWND hWnd)
{
	dmlib_subclass::SetSubclass<dmlib_subclass::StaticTextData>(hWnd, dmlib_subclass::StaticTextSubclass, dmlib_subclass::SubclassID::staticText, hWnd);
}

/**
 * @brief Removes the workaround subclass from a static control.
 *
 * Cleans up the `StaticTextData` and detaches the control's subclass proc.
 *
 * @param[in] hWnd Handle to the static control.
 *
 * @see dmlib_subclass::StaticTextSubclass()
 * @see dmlib::setStaticTextCtrlSubclass()
 */
void dmlib::removeStaticTextCtrlSubclass(HWND hWnd)
{
	dmlib_subclass::RemoveSubclass<dmlib_subclass::StaticTextData>(hWnd, dmlib_subclass::StaticTextSubclass, dmlib_subclass::SubclassID::staticText);
}

/**
 * @brief Applies workaround subclassing to a static control.
 *
 * Overload wrapper that applies the subclass only if `p.m_subclass` is `true`.
 *
 * @param[in]   hWnd    Handle to the static control.
 * @param[in]   p       Parameters controlling whether to apply subclassing.
 *
 * @see dmlib::setStaticTextCtrlSubclass()
 */
static void setStaticTextCtrlSubclass(HWND hWnd, DarkModeParams p) noexcept
{
	if (p.m_subclass)
	{
		dmlib::setStaticTextCtrlSubclass(hWnd);
	}
}

/**
 * @brief Applies owner drawn subclassing to a IP address control.
 *
 * Handles custom colors for itself and its child edit controls.
 *
 * @param[in] hWnd Handle to the IP address control.
 *
 * @see dmlib_subclass::IPAddressSubclass()
 * @see dmlib::removeIPAddressCtrlSubclass()
 */
void dmlib::setIPAddressCtrlSubclass(HWND hWnd)
{
	dmlib_subclass::SetSubclass(hWnd, dmlib_subclass::IPAddressSubclass, dmlib_subclass::SubclassID::ipAddress);
}

/**
 * @brief Removes the owner drawn subclass from a IP address control.
 *
 * @param[in] hWnd Handle to the IP address control.
 *
 * @see dmlib_subclass::IPAddressSubclass()
 * @see dmlib::setIPAddressCtrlSubclass()
 */
void dmlib::removeIPAddressCtrlSubclass(HWND hWnd)
{
	dmlib_subclass::RemoveSubclass(hWnd, dmlib_subclass::IPAddressSubclass, dmlib_subclass::SubclassID::ipAddress);
}

/**
 * @brief Applies owner drawn subclassing to a IP address control and adjusts its border style.
 *
 * Overload wrapper that applies the subclass only if `p.m_subclass` is `true`.
 * Adjusts border style depending on used dark mode state.
 *
 * @param[in]   hWnd    Handle to the IP address control.
 * @param[in]   p       Parameters controlling whether to apply subclassing.
 *
 * @see dmlib::setIPAddressCtrlSubclass()
 */
static void setIPAddressCtrlSubclass(HWND hWnd, DarkModeParams p) noexcept
{
	if (p.m_subclass)
	{
		dmlib::setIPAddressCtrlSubclass(hWnd);
	}

	if (p.m_theme)
	{
		dmlib::replaceClientEdgeWithBorderSafe(hWnd);
	}
}

/**
 * @brief Applies custom color subclassing to a hot key control.
 *
 * Handles custom colors for hot key control via hooks.
 *
 * @param[in] hWnd Handle to the hot key control.
 *
 * @see dmlib_subclass::HotKeySubclass()
 * @see dmlib::removeHotKeyCtrlSubclass()
 */
void dmlib::setHotKeyCtrlSubclass(HWND hWnd)
{
	dmlib_subclass::SetSubclass(hWnd, dmlib_subclass::HotKeySubclass, dmlib_subclass::SubclassID::hotKey);
}

/**
 * @brief Removes the custom color subclass from a hot key control.
 *
 * @param[in] hWnd Handle to the hot key control.
 *
 * @see dmlib_subclass::HotKeySubclass()
 * @see dmlib::setHotKeyCtrlSubclass()
 */
void dmlib::removeHotKeyCtrlSubclass(HWND hWnd)
{
	dmlib_subclass::RemoveSubclass(hWnd, dmlib_subclass::HotKeySubclass, dmlib_subclass::SubclassID::hotKey);
}

/**
 * @brief Applies custom color subclassing to a hot key control and adjusts its border style.
 *
 * Overload wrapper that applies the subclass only if `p.m_subclass` is `true`.
 * Adjusts border style depending on used dark mode state.
 *
 * @param[in]   hWnd    Handle to the hot key control.
 * @param[in]   p       Parameters controlling whether to apply subclassing.
 *
 * @see dmlib::setHotKeyCtrlSubclass()
 */
static void setHotKeyCtrlSubclass(HWND hWnd, DarkModeParams p) noexcept
{
	if (p.m_subclass)
	{
		dmlib::setHotKeyCtrlSubclass(hWnd);
	}

	if (p.m_theme)
	{
		dmlib::replaceClientEdgeWithBorderSafe(hWnd);
	}
}

/**
 * @brief Applies custom color subclassing to a date time picker control.
 *
 * Handles custom colors for date time picker control via hooks.
 *
 * @param[in] hWnd Handle to the date time picker control.
 *
 * @see dmlib_subclass::DTPSubclass()
 * @see dmlib::removeDTPCtrlSubclass()
 */
void dmlib::setDTPCtrlSubclass(HWND hWnd)
{
	dmlib_subclass::SetSubclass(hWnd, dmlib_subclass::DTPSubclass, dmlib_subclass::SubclassID::dtp);
}

/**
 * @brief Removes the custom color subclass from a date time picker control.
 *
 * @param[in] hWnd Handle to the date time picker control.
 *
 * @see dmlib_subclass::DTPSubclass()
 * @see dmlib::setDTPCtrlSubclass()
 */
void dmlib::removeDTPCtrlSubclass(HWND hWnd)
{
	dmlib_subclass::RemoveSubclass(hWnd, dmlib_subclass::DTPSubclass, dmlib_subclass::SubclassID::dtp);
}

/**
 * @brief Applies custom color subclassing to a date time picker control.
 *
 * Applies the subclass only if `p.m_subclass` is `true`.
 * Disable visual style if to allow custom colors.
 *
 * @param[in]   hWnd    Handle to the date time picker control.
 * @param[in]   p       Parameters controlling whether to apply subclassing.
 *
 * @see dmlib::setDTPCtrlSubclass()
 */
static void setDTPCtrlSubclassAndTheme(HWND hWnd, DarkModeParams p) noexcept
{
	if (p.m_subclass)
	{
		dmlib::setDTPCtrlSubclass(hWnd);
	}

	if (p.m_theme)
	{
		dmlib::disableVisualStyle(hWnd, dmlib::isEnabled());
	}
}

/**
 * @brief Applies theming to a tree view control.
 *
 * Sets custom text and background colors, applies a themed window style,
 * and applies themed tooltips for tree view items.
 *
 * @param[in]   hWnd    Handle to the tree view control.
 * @param[in]   p       Parameters controlling whether to apply theming.
 *
 * @see dmlib::setDarkTreeViewCheckboxes(hWnd);
 * @see dmlib::setTreeViewWindowTheme()
 * @see dmlib::setDarkTooltips()
 */
static void setTreeViewCtrlTheme(HWND hWnd, DarkModeParams p) noexcept
{
	if (p.m_theme)
	{
		TreeView_SetTextColor(hWnd, dmlib::getViewTextColor());
		TreeView_SetBkColor(hWnd, dmlib::getViewBackgroundColor());

		dmlib::setTreeViewWindowThemeEx(hWnd, p.m_theme);
		dmlib::setDarkTreeViewCheckboxes(hWnd);
		dmlib::setDarkTooltips(hWnd, static_cast<int>(dmlib::ToolTipsType::treeview));
	}
}

/**
 * @brief Applies subclassing to a rebar control.
 *
 * Applies window subclassing to handle `WM_ERASEBKGND` message.
 *
 * @param[in]   hWnd    Handle to the rebar control.
 * @param[in]   p       Parameters controlling whether to apply subclassing.
 *
 * @see dmlib::setWindowEraseBgSubclass()
 */
static void setRebarCtrlSubclass(HWND hWnd, DarkModeParams p) noexcept
{
	if (p.m_subclass)
	{
		dmlib::setWindowEraseBgSubclass(hWnd);
	}
}

/**
 * @brief Applies theming to a toolbar control.
 *
 * Sets custom colors for line above toolbar panel
 * and applies themed tooltips for toolbar buttons.
 *
 * @param[in]   hWnd    Handle to the toolbar control.
 * @param[in]   p       Parameters controlling whether to apply theming.
 *
 * @see dmlib::setDarkLineAbovePanelToolbar()
 * @see dmlib::setDarkTooltips()
 */
static void setToolbarCtrlTheme(HWND hWnd, DarkModeParams p) noexcept
{
	if (p.m_theme)
	{
		dmlib::setDarkLineAbovePanelToolbar(hWnd);
		dmlib::setDarkTooltips(hWnd, static_cast<int>(dmlib::ToolTipsType::toolbar));
	}
}

/**
 * @brief Applies theming to a scroll bar control.
 *
 * @param[in]   hWnd    Handle to the scroll bar control.
 * @param[in]   p       Parameters controlling whether to apply theming.
 *
 * @see dmlib::setDarkScrollBar()
 */
static void setScrollBarCtrlTheme(HWND hWnd, DarkModeParams p) noexcept
{
	if (p.m_theme)
	{
		dmlib::setDarkScrollBar(hWnd);
	}
}

/**
 * @brief Applies theming to a SysLink control.
 *
 * Overload that enable `WM_CTLCOLORSTATIC` message handling
 * depending on `DarkModeParams` for the syslink control.
 *
 * @param[in]   hWnd    Handle to the SysLink control.
 * @param[in]   p       Parameters controlling whether to apply theming.
 *
 * @see dmlib::enableSysLinkCtrlCtlColor()
 */
static void enableSysLinkCtrlCtlColor(HWND hWnd, DarkModeParams p) noexcept
{
	if (p.m_theme)
	{
		dmlib::enableSysLinkCtrlCtlColor(hWnd);
	}
}

/**
 * @brief Applies theming to a trackbar control.
 *
 * Sets transparent background via `TBS_TRANSPARENTBKGND` flag
 * and applies themed tooltips for trackbar buttons.
 *
 * @param[in]   hWnd    Handle to the trackbar control.
 * @param[in]   p       Parameters controlling whether to apply theming.
 *
 * @see dmlib::setWindowStyle()
 * @see dmlib::setDarkTooltips()
 */
static void setTrackbarCtrlTheme(HWND hWnd, DarkModeParams p) noexcept
{
	if (p.m_theme)
	{
		dmlib::setWindowStyle(hWnd, dmlib::isEnabled(), TBS_TRANSPARENTBKGND);
		dmlib::setDarkTooltips(hWnd, static_cast<int>(dmlib::ToolTipsType::trackbar));
	}
}

/**
 * @brief Applies theming to a rich edit control.
 *
 * @param[in]   hWnd    Handle to the rich edit control.
 * @param[in]   p       Parameters controlling whether to apply theming.
 *
 * @see dmlib::setDarkRichEdit()
 */
static void setRichEditCtrlTheme(HWND hWnd, DarkModeParams p) noexcept
{
	if (p.m_theme)
	{
		dmlib::setDarkRichEdit(hWnd);
	}
}

/**
 * @brief Applies theming to a month calendar control.
 *
 * @param[in]   hWnd    Handle to the month calendar control.
 * @param[in]   p       Parameters controlling whether to apply theming.
 *
 * @see dmlib::setDarkMonthCalendar()
 */
static void setMonthCalendarCtrlTheme(HWND hWnd, DarkModeParams p) noexcept
{
	if (p.m_theme)
	{
		dmlib::setDarkMonthCalendar(hWnd);
	}
}

/**
 * @brief Callback function used to enumerate and apply theming/subclassing to child controls.
 *
 * Called in `setChildCtrlsSubclassAndTheme()` and `setChildCtrlsTheme()`
 * to inspect each child window's class name and apply appropriate theming
 * and/or subclassing logic based on control type.
 *
 * @param[in]   hWnd    Handle to the window being enumerated.
 * @param[in]   lParam  Pointer to a `DarkModeParams` structure containing theming flags and settings.
 * @return `TRUE`   to continue enumeration.
 *
 * @note
 * - Currently handles these controls:
 *      `WC_BUTTON`, `WC_STATIC`, `WC_COMBOBOX`, `WC_EDIT`, `WC_LISTBOX`,
 *      `WC_LISTVIEW`, `WC_TREEVIEW`, `REBARCLASSNAME`, `TOOLBARCLASSNAME`,
 *      `UPDOWN_CLASS`, `WC_TABCONTROL`, `STATUSCLASSNAME`, `WC_SCROLLBAR`,
 *      `WC_COMBOBOXEX`, `PROGRESS_CLASS`, `WC_LINK`, `TRACKBAR_CLASS`,
 *      `RICHEDIT_CLASS`, `MSFTEDIT_CLASS`, `WC_IPADDRESS`, `HOTKEY_CLASS`,
 *      `DATETIMEPICK_CLASS`, and `MONTHCAL_CLASS`
 * - The `#32770` dialog class is commented out for debugging purposes.
 *
 * @see dmlib::setChildCtrlsSubclassAndTheme()
 * @see dmlib::setChildCtrlsTheme()
 * @see DarkModeParams
 * @see setBtnCtrlSubclassAndTheme()
 * @see setStaticTextCtrlSubclass()
 * @see setComboBoxCtrlSubclassAndTheme()
 * @see setCustomBorderForListBoxOrEditCtrlSubclassAndTheme()
 * @see setListViewCtrlSubclassAndTheme()
 * @see setTreeViewCtrlTheme()
 * @see setRebarCtrlSubclass()
 * @see setToolbarCtrlTheme()
 * @see setUpDownCtrlSubclassAndTheme()
 * @see setTabCtrlSubclassAndTheme()
 * @see setStatusBarCtrlSubclass()
 * @see setScrollBarCtrlTheme()
 * @see setComboBoxExCtrlSubclass()
 * @see setProgressBarCtrlSubclass()
 * @see enableSysLinkCtrlCtlColor()
 * @see setTrackbarCtrlTheme()
 * @see setRichEditCtrlTheme()
 * @see setIPAddressCtrlSubclass()
 * @see setHotKeyCtrlSubclass()
 * @see setDTPCtrlSubclassAndTheme()
 * @see setMonthCalendarCtrlTheme()
 */
static BOOL CALLBACK DarkEnumChildProc(HWND hWnd, LPARAM lParam) DMLIB_MEM_NOEXCEPT
{
	const auto& p = *reinterpret_cast<DarkModeParams*>(lParam);
	const auto className = dmlib_subclass::WndClassName(hWnd);

	if (className == WC_BUTTON)
	{
		setBtnCtrlSubclassAndTheme(hWnd, p);
		return TRUE;
	}

	if (className == WC_STATIC)
	{
		setStaticTextCtrlSubclass(hWnd, p);
		return TRUE;
	}

	if (className == WC_COMBOBOX)
	{
		setComboBoxCtrlSubclassAndTheme(hWnd, p);
		return TRUE;
	}

	if (className == WC_EDIT)
	{
		setCustomBorderForListBoxOrEditCtrlSubclassAndTheme(hWnd, p, false);
		return TRUE;
	}

	if (className == WC_LISTBOX)
	{
		setCustomBorderForListBoxOrEditCtrlSubclassAndTheme(hWnd, p, true);
		return TRUE;
	}

	if (className == WC_LISTVIEW)
	{
		setListViewCtrlSubclassAndTheme(hWnd, p);
		return TRUE;
	}

	if (className == WC_TREEVIEW)
	{
		setTreeViewCtrlTheme(hWnd, p);
		return TRUE;
	}

	if (className == REBARCLASSNAME)
	{
		setRebarCtrlSubclass(hWnd, p);
		return TRUE;
	}

	if (className == TOOLBARCLASSNAME)
	{
		setToolbarCtrlTheme(hWnd, p);
		return TRUE;
	}

	if (className == UPDOWN_CLASS)
	{
		setUpDownCtrlSubclassAndTheme(hWnd, p);
		return TRUE;
	}

	if (className == WC_TABCONTROL)
	{
		setTabCtrlSubclassAndTheme(hWnd, p);
		return TRUE;
	}

	if (className == STATUSCLASSNAME)
	{
		setStatusBarCtrlSubclass(hWnd, p);
		return TRUE;
	}

	if (className == WC_SCROLLBAR)
	{
		setScrollBarCtrlTheme(hWnd, p);
		return TRUE;
	}

	if (className == WC_COMBOBOXEX)
	{
		setComboBoxExCtrlSubclass(hWnd, p);
		return TRUE;
	}

	if (className == PROGRESS_CLASS)
	{
		setProgressBarCtrlSubclass(hWnd, p);
		return TRUE;
	}

	if (className == WC_LINK)
	{
		enableSysLinkCtrlCtlColor(hWnd, p);
		return TRUE;
	}

	if (className == TRACKBAR_CLASS)
	{
		setTrackbarCtrlTheme(hWnd, p);
		return TRUE;
	}

	if (className == RICHEDIT_CLASS || className == MSFTEDIT_CLASS) // rich edit controls 2.0, 3.0, and 4.1
	{
		setRichEditCtrlTheme(hWnd, p);
		return TRUE;
	}

	if (className == WC_IPADDRESS)
	{
		setIPAddressCtrlSubclass(hWnd, p);
		return TRUE;
	}

	if (className == HOTKEY_CLASS)
	{
		setHotKeyCtrlSubclass(hWnd, p);
		return TRUE;
	}

	if (className == DATETIMEPICK_CLASS) // date and time picker
	{
		setDTPCtrlSubclassAndTheme(hWnd, p);
		return TRUE;
	}

	if (className == MONTHCAL_CLASS) // month calendar
	{
		setMonthCalendarCtrlTheme(hWnd, p);
		return TRUE;
	}

#if 0 // for debugging
	if (className == L"#32770") // dialog
	{
		return TRUE;
	}
#endif
	return TRUE;
}

/**
 * @brief Applies theming and/or subclassing to all child controls of a parent window.
 *
 * Enumerates all child windows of the specified parent and dispatches them to
 * `DarkEnumChildProc`, which applies control-specific theming and/or subclassing logic
 * based on their class name and the provided parameters.
 *
 * Mainly used when initializing parent control.
 *
 * @param[in]   hParent     Handle to the parent window whose child controls will be themed and/or subclassed.
 * @param[in]   subclass    Whether to apply subclassing.
 * @param[in]   theme       Whether to apply theming.
 *
 * @see dmlib::setChildCtrlsSubclassAndTheme()
 * @see dmlib::DarkEnumChildProc()
 * @see DarkModeParams
 */
void dmlib::setChildCtrlsSubclassAndThemeEx(HWND hParent, bool subclass, bool theme)
{
	DarkModeParams p{
		dmlib::isExperimentalActive() ? L"DarkMode_Explorer" : nullptr
		, subclass
		, theme
	};

	::EnumChildWindows(hParent, DarkEnumChildProc, reinterpret_cast<LPARAM>(&p));
}

/**
 * @brief Wrapper for `dmlib::setChildCtrlsSubclassAndThemeEx`.
 *
 * Forwards to `dmlib::setChildCtrlsSubclassAndThemeEx` with `subclass` and `theme` parameters set as `true`.
 *
 * @param[in] hParent Handle to the parent window whose child controls will be themed and/or subclassed.
 *
 * @see dmlib::setChildCtrlsSubclassAndThemeEx()
 */
void dmlib::setChildCtrlsSubclassAndTheme(HWND hParent)
{
	dmlib::setChildCtrlsSubclassAndThemeEx(hParent, true, true);
}

/**
 * @brief Applies theming to all child controls of a parent window.
 *
 * Enumerates child windows of the specified parent and applies theming without subclassing.
 * The theming behavior adapts based on OS support and compile-time flags.
 * If `_DARKMODELIB_ALLOW_OLD_OS > 1` is true, theming is applied unconditionally.
 * Otherwise, theming is applied only if the OS is Windows 10 or newer.
 * The function delegates to `setChildCtrlsSubclassAndTheme()` with appropriate flags.
 *
 * Mainly used when changing mode.
 *
 * @param[in] hParent Handle to the parent window whose child controls will be themed.
 *
 * @see dmlib::setChildCtrlsSubclassAndTheme()
 */
void dmlib::setChildCtrlsTheme(HWND hParent)
{
#if defined(_DARKMODELIB_ALLOW_OLD_OS) && (_DARKMODELIB_ALLOW_OLD_OS > 1)
	dmlib::setChildCtrlsSubclassAndThemeEx(hParent, false, true);
#else
	dmlib::setChildCtrlsSubclassAndThemeEx(hParent, false, dmlib::isAtLeastWindows10());
#endif
}

/**
 * @brief Applies window subclassing to handle `WM_ERASEBKGND` message.
 *
 * @param[in] hWnd Handle to the control to subclass.
 *
 * @see dmlib_subclass::WindowEraseBgSubclass()
 * @see dmlib::removeWindowEraseBgSubclass()
 */
void dmlib::setWindowEraseBgSubclass(HWND hWnd)
{
	dmlib_subclass::SetSubclass(hWnd, dmlib_subclass::WindowEraseBgSubclass, dmlib_subclass::SubclassID::windowEraseBg);
}

/**
 * @brief Removes the subclass used for `WM_ERASEBKGND` message handling.
 *
 * Detaches the window's subclass proc used for `WM_ERASEBKGND` message handling.
 *
 * @param[in] hWnd Handle to the previously subclassed window.
 *
 * @see dmlib_subclass::WindowEraseBgSubclass()
 * @see dmlib::removeWindowEraseBgSubclass()
 */
void dmlib::removeWindowEraseBgSubclass(HWND hWnd)
{
	dmlib_subclass::RemoveSubclass(hWnd, dmlib_subclass::WindowEraseBgSubclass, dmlib_subclass::SubclassID::windowEraseBg);
}

/**
 * @brief Applies window subclassing to handle `WM_CTLCOLOR*` messages.
 *
 * Enable custom colors for edit, listbox, static, and dialog elements
 * via @ref dmlib_subclass::WindowCtlColorSubclass.
 *
 * @param[in] hWnd Handle to the parent or composite control (dialog, rebar, toolbar, ...) to subclass.
 *
 * @see dmlib_subclass::WindowCtlColorSubclass()
 * @see dmlib::removeWindowCtlColorSubclass()
 */
void dmlib::setWindowCtlColorSubclass(HWND hWnd)
{
	dmlib_subclass::SetSubclass(hWnd, dmlib_subclass::WindowCtlColorSubclass, dmlib_subclass::SubclassID::windowCtlColor);
}

/**
 * @brief Removes the subclass used for `WM_CTLCOLOR*` messages handling.
 *
 * Detaches the window's subclass proc used for `WM_CTLCOLOR*` messages handling.
 *
 * @param[in] hWnd Handle to the previously subclassed window.
 *
 * @see dmlib_subclass::WindowCtlColorSubclass()
 * @see dmlib::setWindowCtlColorSubclass()
 */
void dmlib::removeWindowCtlColorSubclass(HWND hWnd)
{
	dmlib_subclass::RemoveSubclass(hWnd, dmlib_subclass::WindowCtlColorSubclass, dmlib_subclass::SubclassID::windowCtlColor);
}

/**
 * @brief Applies window subclassing for handling `NM_CUSTOMDRAW` notifications for custom drawing.
 *
 * Installs @ref dmlib_subclass::WindowNotifySubclass.
 * Enables handling of `WM_NOTIFY` `NM_CUSTOMDRAW` notifications for custom drawing
 * behavior for supported controls.
 *
 * @param[in] hWnd Handle to the window with child which support `NM_CUSTOMDRAW`.
 *
 * @see dmlib_subclass::WindowNotifySubclass()
 * @see dmlib::removeWindowNotifyCustomDrawSubclass()
 */
void dmlib::setWindowNotifyCustomDrawSubclass(HWND hWnd)
{
	dmlib_subclass::SetSubclass(hWnd, dmlib_subclass::WindowNotifySubclass, dmlib_subclass::SubclassID::windowNotify);
}

/**
 * @brief Removes the subclass used for handling `NM_CUSTOMDRAW` notifications for custom drawing.
 *
 * Detaches the window's subclass proc used for handling `NM_CUSTOMDRAW` notifications for custom drawing.
 *
 * @param[in] hWnd Handle to the previously subclassed window.
 *
 * @see dmlib_subclass::WindowNotifySubclass()
 * @see dmlib::setWindowNotifyCustomDrawSubclass()
 */
void dmlib::removeWindowNotifyCustomDrawSubclass(HWND hWnd)
{
	dmlib_subclass::RemoveSubclass(hWnd, dmlib_subclass::WindowNotifySubclass, dmlib_subclass::SubclassID::windowNotify);
}

/**
 * @brief Applies window subclassing for menu bar themed custom drawing.
 *
 * Installs @ref dmlib::WindowMenuBarSubclass with an associated `ThemeData` instance
 * for the `VSCLASS_MENU` visual style. Enables custom drawing
 * behavior for menu bar.
 *
 * @param[in] hWnd Handle to the window with a menu bar.
 *
 * @see dmlib_subclass::WindowMenuBarSubclass()
 * @see dmlib::removeWindowMenuBarSubclass()
 */
void dmlib::setWindowMenuBarSubclass(HWND hWnd)
{
	dmlib_subclass::SetSubclass<dmlib_subclass::ThemeData>(hWnd, dmlib_subclass::WindowMenuBarSubclass, dmlib_subclass::SubclassID::windowMenuBar, VSCLASS_MENU);
}

/**
 * @brief Removes the subclass used for menu bar themed custom drawing.
 *
 * Detaches the window's subclass proc used for menu bar themed custom drawing.
 *
 * @param[in] hWnd Handle to the previously subclassed window.
 *
 * @see dmlib_subclass::WindowMenuBarSubclass()
 * @see dmlib::setWindowMenuBarSubclass()
 */
void dmlib::removeWindowMenuBarSubclass(HWND hWnd)
{
	dmlib_subclass::RemoveSubclass<dmlib_subclass::ThemeData>(hWnd, dmlib_subclass::WindowMenuBarSubclass, dmlib_subclass::SubclassID::windowMenuBar);
}

/**
 * @brief Applies window subclassing to handle `WM_SETTINGCHANGE` message.
 *
 * Enable monitoring WM_SETTINGCHANGE message,
 * allowing the app to respond to system-wide dark mode change.
 *
 * @param[in] hWnd Handle to the main window.
 *
 * @see dmlib::WindowSettingChangeSubclass()
 * @see dmlib::removeWindowSettingChangeSubclass()
 */
void dmlib::setWindowSettingChangeSubclass(HWND hWnd)
{
	dmlib_subclass::SetSubclass(hWnd, dmlib_subclass::WindowSettingChangeSubclass, dmlib_subclass::SubclassID::windowSettingChange);
}

/**
 * @brief Removes the subclass used for `WM_SETTINGCHANGE` message handling.
 *
 * Detaches the window's subclass proc used for `WM_SETTINGCHANGE` messages handling.
 *
 * @param[in] hWnd Handle to the previously subclassed window.
 *
 * @see dmlib::WindowSettingChangeSubclass()
 * @see dmlib::setWindowSettingChangeSubclass()
 */
void dmlib::removeWindowSettingChangeSubclass(HWND hWnd)
{
	dmlib_subclass::RemoveSubclass(hWnd, dmlib_subclass::WindowSettingChangeSubclass, dmlib_subclass::SubclassID::windowSettingChange);
}

/**
 * @brief Configures the SysLink control to be affected by `WM_CTLCOLORSTATIC` message.
 *
 * Configures all items to either use default system link colors if in classic mode,
 * or to be affected by `WM_CTLCOLORSTATIC` message from its parent.
 *
 * @param[in] hWnd Handle to the SysLink control.
 *
 * @note Will affect all items, even if it's static (non-clickable).
 */
void dmlib::enableSysLinkCtrlCtlColor(HWND hWnd)
{
	LITEM lItem{};
	lItem.iLink = 0;
	lItem.mask = LIF_ITEMINDEX | LIF_STATE;
	lItem.state = dmlib::isEnabled() ? LIS_DEFAULTCOLORS : 0;
	lItem.stateMask = LIS_DEFAULTCOLORS;
	while (::SendMessage(hWnd, LM_SETITEM, 0, reinterpret_cast<LPARAM>(&lItem)) == TRUE)
	{
		++lItem.iLink;
	}
}

/**
 * @brief Sets dark title bar and optional Windows 11 features.
 *
 * For Windows 10 (2004+) and newer, this function configures the dark title bar using
 * `DWMWA_USE_IMMERSIVE_DARK_MODE`. On Windows 11, if `useWin11Features` is `true`, it
 * additionally applies:
 * - Rounded corners (`DWMWA_WINDOW_CORNER_PREFERENCE`)
 * - Border color (`DWMWA_BORDER_COLOR`)
 * - Mica backdrop (`DWMWA_SYSTEMBACKDROP_TYPE`) if allowed and compatible
 * - Static text color for text and dialog background color for background
 *   (`DWMWA_CAPTION_COLOR`, `DWMWA_TEXT_COLOR`),
 *   only when frames are not extended to full window
 *
 * If `_DARKMODELIB_ALLOW_OLD_OS` is defined with non-zero unsigned value
 * and running on pre-2004 builds, fallback behavior will enable dark title bars via undocumented APIs.
 *
 * @param[in]   hWnd                Handle to the top-level window.
 * @param[in]   useWin11Features    `true` to enable Windows 11 specific features such as Mica and rounded corners.
 *
 * @note Requires Windows 10 version 2004 (build 19041) or later.
 *
 * @see DwmSetWindowAttribute
 * @see DwmExtendFrameIntoClientArea
 */
void dmlib::setDarkTitleBarEx(HWND hWnd, bool useWin11Features)
{
	if (static constexpr DWORD win10Build2004 = 19041;
		dmlib::getWindowsBuildNumber() >= win10Build2004)
	{
		const BOOL useDark = dmlib::isExperimentalActive() ? TRUE : FALSE;
		::DwmSetWindowAttribute(hWnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &useDark, sizeof(useDark));
	}
#if defined(_DARKMODELIB_ALLOW_OLD_OS) && (_DARKMODELIB_ALLOW_OLD_OS > 0)
	else
	{
		dmlib_win32api::AllowDarkModeForWindow(hWnd, dmlib::isExperimentalActive());
		dmlib_win32api::RefreshTitleBarThemeColor(hWnd);
	}
#endif

	if (!dmlib::isAtLeastWindows11())
	{
		// on Windows 10 title bar needs refresh when changing colors
		if (dmlib::isAtLeastWindows10())
		{
			const bool isActive = (hWnd == ::GetActiveWindow()) && (hWnd == ::GetForegroundWindow());
			::SendMessage(hWnd, WM_NCACTIVATE, static_cast<WPARAM>(!isActive), 0);
			::SendMessage(hWnd, WM_NCACTIVATE, static_cast<WPARAM>(isActive), 0);
		}
		return;
	}

	if (!useWin11Features)
	{
		return;
	}

	::DwmSetWindowAttribute(hWnd, DWMWA_WINDOW_CORNER_PREFERENCE, &g_dmCfg.m_roundCorner, sizeof(g_dmCfg.m_roundCorner));
	::DwmSetWindowAttribute(hWnd, DWMWA_BORDER_COLOR, &g_dmCfg.m_borderColor, sizeof(g_dmCfg.m_borderColor));

	bool canColorizeTitleBar = true;

	if (static constexpr DWORD win11Mica = 22621;
		dmlib::getWindowsBuildNumber() >= win11Mica)
	{
		if (g_dmCfg.m_micaExtend && g_dmCfg.m_mica != DWMSBT_AUTO
			&& !dmlib::isWindowsModeEnabled()
			&& (g_dmCfg.m_dmType == DarkModeType::dark))
		{
			static constexpr MARGINS margins{ -1, 0, 0, 0 };
			::DwmExtendFrameIntoClientArea(hWnd, &margins);
		}

		::DwmSetWindowAttribute(hWnd, DWMWA_SYSTEMBACKDROP_TYPE, &g_dmCfg.m_mica, sizeof(g_dmCfg.m_mica));

		canColorizeTitleBar = !g_dmCfg.m_micaExtend;
	}

	canColorizeTitleBar = g_dmCfg.m_colorizeTitleBar && canColorizeTitleBar && dmlib::isEnabled();
	const COLORREF clrDlg = canColorizeTitleBar ? dmlib::getDlgBackgroundColor() : DWMWA_COLOR_DEFAULT;
	const COLORREF clrText = canColorizeTitleBar ? dmlib::getTextColor() : DWMWA_COLOR_DEFAULT;
	::DwmSetWindowAttribute(hWnd, DWMWA_CAPTION_COLOR, &clrDlg, sizeof(clrDlg));
	::DwmSetWindowAttribute(hWnd, DWMWA_TEXT_COLOR, &clrText, sizeof(clrText));
}

/**
 * @brief Sets dark mode title bar on supported Windows versions.
 *
 * Delegates to @ref setDarkTitleBarEx with `useWin11Features = false`.
 *
 * @param[in] hWnd Handle to the top-level window.
 *
 * @see dmlib::setDarkTitleBarEx()
 */
void dmlib::setDarkTitleBar(HWND hWnd)
{
	dmlib::setDarkTitleBarEx(hWnd, false);
}

/**
 * @brief Get dark mode theme name.
 *
 * @return "DarkMode_DarkTheme" on Windows 11 25H2+
 * @return "DarkMode_Explorer" on Windows 10+
 * @return nullptr if not on supported OS
 *
 * @see doesWin11SupportDarkThemeStyle()
 */
const wchar_t* dmlib::getDarkModeThemeName()
{
	if (doesWin11SupportDarkThemeStyle())
	{
		return L"DarkMode_DarkTheme";
	}
	if (dmlib::isAtLeastWindows10())
	{
		return L"DarkMode_Explorer";
	}
	return nullptr;
}

/**
 * @brief Applies an experimental visual style to the specified window, if supported.
 *
 * When experimental features are supported and active,
 * this function enables dark experimental visual style on the window.
 *
 * @param[in]   hWnd            Handle to the target window or control.
 * @param[in]   themeClassName  Name of the theme class to apply (e.g. L"Explorer", "ItemsView").
 *
 * @note This function is a no-op if experimental theming is not supported on the current OS.
 *
 * @see dmlib::isExperimentalSupported()
 * @see dmlib::isExperimentalActive()
 * @see dmlib_win32api::AllowDarkModeForWindow()
 * @see dmlib::setDarkThemeExperimental()
 */
void dmlib::setDarkThemeExperimentalEx(HWND hWnd, const wchar_t* themeClassName)
{
	if (dmlib::isExperimentalSupported())
	{
		dmlib_win32api::AllowDarkModeForWindow(hWnd, dmlib::isExperimentalActive());
		::SetWindowTheme(hWnd, themeClassName, nullptr);
	}
}

/**
 * @brief Applies an experimental Explorer visual style to the specified window, if supported.
 *
 * Forwards to `dmlib::setDarkThemeExperimentalEx` with `themeClassName` as `L"Explorer"`.
 *
 * @param[in] hWnd Handle to the target window or control.
 *
 * @see dmlib::setDarkThemeExperimentalEx()
 */
void dmlib::setDarkThemeExperimental(HWND hWnd)
{
	dmlib::setDarkThemeExperimentalEx(hWnd, L"Explorer");
}

/**
 * @brief Applies "DarkMode_Explorer" visual style if experimental mode is active.
 *
 * Useful for controls like list views or tree views to use dark scroll bars
 * and explorer style theme in supported environments.
 *
 * @param[in] hWnd Handle to the control or window to theme.
 */
void dmlib::setDarkExplorerTheme(HWND hWnd)
{
	::SetWindowTheme(hWnd, dmlib::isExperimentalActive() ? L"DarkMode_Explorer" : nullptr, nullptr);
}

/**
 * @brief Applies "DarkMode_Explorer" visual style to scroll bars.
 *
 * Convenience wrapper that calls @ref dmlib::setDarkExplorerTheme to apply dark scroll bar
 * for compatible controls (e.g. list views, tree views).
 *
 * @param[in] hWnd Handle to the control with scroll bars.
 *
 * @see dmlib::setDarkExplorerTheme()
 */
void dmlib::setDarkScrollBar(HWND hWnd)
{
	dmlib::setDarkExplorerTheme(hWnd);
}

/**
 * @brief Applies "DarkMode_Explorer" visual style to tooltip controls based on context.
 *
 * Selects the appropriate `GETTOOLTIPS` message depending on the control type
 * (e.g. toolbar, list view, tree view, tab bar) to retrieve the tooltip handle.
 * If `ToolTipsType::tooltip` is specified, applies theming directly to `hWnd`.
 *
 * Internally calls @ref dmlib::setDarkExplorerTheme to set dark tooltip.
 *
 * @param[in]   hWnd        Handle to the parent control or tooltip.
 * @param[in]   tooltipType The tooltip context type (toolbar, list view, etc.).
 *
 * @see dmlib::setDarkExplorerTheme()
 * @see ToolTipsType
 */
void dmlib::setDarkTooltips(HWND hWnd, UINT tooltipType)
{
	if (tooltipType > static_cast<UINT>(dmlib::ToolTipsType::rebar))
	{
		return;
	}

	const auto type = static_cast<ToolTipsType>(tooltipType);
	UINT msg = 0;
	switch (type)
	{
		case dmlib::ToolTipsType::toolbar:
		{
			msg = TB_GETTOOLTIPS;
			break;
		}

		case dmlib::ToolTipsType::listview:
		{
			msg = LVM_GETTOOLTIPS;
			break;
		}

		case dmlib::ToolTipsType::treeview:
		{
			msg = TVM_GETTOOLTIPS;
			break;
		}

		case dmlib::ToolTipsType::tabbar:
		{
			msg = TCM_GETTOOLTIPS;
			break;
		}

		case dmlib::ToolTipsType::trackbar:
		{
			msg = TBM_GETTOOLTIPS;
			break;
		}

		case dmlib::ToolTipsType::rebar:
		{
			msg = RB_GETTOOLTIPS;
			break;
		}

		case dmlib::ToolTipsType::tooltip:
		{
			msg = 0;
			break;
		}
	}

	if (msg == 0)
	{
		dmlib::setDarkExplorerTheme(hWnd);
	}
	else
	{
		auto hTips = reinterpret_cast<HWND>(::SendMessage(hWnd, msg, 0, 0));
		if (hTips != nullptr)
		{
			dmlib::setDarkExplorerTheme(hTips);
		}
	}
}

/**
 * @brief Applies "DarkMode_DarkTheme" visual style if supported and experimental mode is active.
 *
 * Applies "DarkMode_DarkTheme" visual style if supported,
 * else applies "DarkMode_Explorer" visual style.
 *
 * @param[in] hWnd Handle to the control or window to theme.
 *
 * @see dmlib::getDarkModeThemeName()
 */
void dmlib::setDarkThemeTheme(HWND hWnd)
{
	::SetWindowTheme(hWnd, dmlib::isExperimentalActive() ? dmlib::getDarkModeThemeName() : nullptr, nullptr);
}

/**
 * @brief Sets the color of line above a toolbar control for non-classic mode.
 *
 * Sends `TB_SETCOLORSCHEME` to customize the line drawn above the toolbar.
 * When non-classic mode is enabled, sets both `clrBtnHighlight` and `clrBtnShadow`
 * to the dialog background color, otherwise uses system defaults.
 *
 * @param[in] hWnd Handle to the toolbar control.
 */
void dmlib::setDarkLineAbovePanelToolbar(HWND hWnd)
{
	COLORSCHEME scheme{};
	scheme.dwSize = sizeof(COLORSCHEME);

	if (dmlib::isEnabled())
	{
		scheme.clrBtnHighlight = dmlib::getDlgBackgroundColor();
		scheme.clrBtnShadow = dmlib::getDlgBackgroundColor();
	}
	else
	{
		scheme.clrBtnHighlight = CLR_DEFAULT;
		scheme.clrBtnShadow = CLR_DEFAULT;
	}

	::SendMessage(hWnd, TB_SETCOLORSCHEME, 0, reinterpret_cast<LPARAM>(&scheme));
}

/**
 * @brief Applies an experimental Explorer visual style to a list view.
 *
 * Uses @ref dmlib::setDarkThemeExperimental with the `"Explorer"` theme class to adapt
 * list view visuals (e.g. scroll bars, selection color) for dark mode, if supported.
 *
 * @param[in] hWnd Handle to the list view control.
 *
 * @see dmlib::setDarkThemeExperimental()
 */
void dmlib::setDarkListView(HWND hWnd)
{
	dmlib::setDarkThemeExperimental(hWnd);
}

/**
 * @brief Replaces list view or tree view image list checkbox state images with themed dark mode versions on Windows 11.
 *
 * Uses `"DarkMode_Explorer::Button"` as the theme class if experimental dark mode is active;
 * otherwise falls back to `VSCLASS_BUTTON`.
 *
 * @param[in]   hWnd          Handle to the control to change checkbox style.
 * @param[in]   hImgList      Handle to the image list of control containing checkbox state images.
 * @param[in]   viewCheckbox  Type of checkbox style.
 *
 * @note Does nothing on pre-Windows 11 systems.
 */
static void setDarkCheckboxes(HWND hWnd, HIMAGELIST hImgList, ViewCheckbox viewCheckbox) noexcept
{
	if (!dmlib::isAtLeastWindows11())
	{
		return;
	}

	HDC hdc = ::GetDC(nullptr);

	const bool useDark = dmlib::isExperimentalActive() && dmlib::isThemeDark();
	HTHEME hTheme = dmlib_dpi::OpenThemeDataForDpi(nullptr, useDark ? L"DarkMode_Explorer::Button" : VSCLASS_BUTTON, ::GetParent(hWnd));

	SIZE szBox{};
	::GetThemePartSize(hTheme, hdc, BP_CHECKBOX, CBS_UNCHECKEDNORMAL, nullptr, TS_DRAW, &szBox);

	const RECT rcBox{ 0, 0, szBox.cx, szBox.cy };

	if (hImgList == nullptr)
	{
		::CloseThemeData(hTheme);
		::ReleaseDC(nullptr, hdc);
		return;
	}

	HDC hBoxDC = ::CreateCompatibleDC(hdc);
	HBITMAP hBoxBmp = ::CreateCompatibleBitmap(hdc, szBox.cx, szBox.cy);
	HBITMAP hMaskBmp = ::CreateCompatibleBitmap(hdc, szBox.cx, szBox.cy);

	auto holdBmp = static_cast<HBITMAP>(::SelectObject(hBoxDC, hBoxBmp));
	::DrawThemeBackground(hTheme, hBoxDC, BP_CHECKBOX, CBS_UNCHECKEDNORMAL, &rcBox, nullptr);

	ICONINFO ii{};
	ii.fIcon = TRUE;
	ii.hbmColor = hBoxBmp;
	ii.hbmMask = hMaskBmp;

	int idx = (viewCheckbox == ViewCheckbox::listView) ? 0 : 1; // tree view state images start from index 1

	HICON hIcon = ::CreateIconIndirect(&ii);

	auto addIcon = [&hImgList, &idx, &hIcon]() noexcept
	{
		if (hIcon != nullptr)
		{
			::ImageList_ReplaceIcon(hImgList, idx, hIcon);
			::DestroyIcon(hIcon);
			hIcon = nullptr;
			++idx;
		}
	};

	addIcon(); // unchecked state

	auto addIconState = [&](int iStateId) noexcept
	{
		::DrawThemeBackground(hTheme, hBoxDC, BP_CHECKBOX, iStateId, &rcBox, nullptr);
		ii.hbmColor = hBoxBmp;

		hIcon = ::CreateIconIndirect(&ii);
		addIcon();
	};

	addIconState(CBS_CHECKEDNORMAL);

	if (viewCheckbox == ViewCheckbox::tvExtended)
	{
		const auto tvExStyle = TreeView_GetExtendedStyle(hWnd);

		// Tree view check boxes: The extended check box states
		// https://devblogs.microsoft.com/oldnewthing/20171205-00/?p=97525
		// The image list states are added in the order: Partial, then dimmed, then exclusion.
		if ((tvExStyle & TVS_EX_PARTIALCHECKBOXES) != 0)
		{
			addIconState(CBS_MIXEDNORMAL);
		}

		if ((tvExStyle & TVS_EX_DIMMEDCHECKBOXES) != 0)
		{
			addIconState(CBS_IMPLICITPRESSED); // make it different from CBS_CHECKEDNORMAL
		}

		if ((tvExStyle & TVS_EX_EXCLUSIONCHECKBOXES) != 0)
		{
			addIconState(CBS_EXCLUDEDNORMAL);
		}
	}

	::SelectObject(hBoxDC, holdBmp);
	::DeleteObject(hMaskBmp);
	::DeleteObject(hBoxBmp);
	::DeleteDC(hBoxDC);
	::CloseThemeData(hTheme);
	::ReleaseDC(nullptr, hdc);
}

/**
 * @brief Replaces default list view checkboxes with themed dark-mode versions on Windows 11.
 *
 * If the list view uses `LVS_EX_CHECKBOXES` and is running on Windows 11 or later,
 * this function then manually draws the unchecked and checked checkbox visuals using
 * themed drawing APIs, then inserts the resulting icons into the state image list.
 *
 * Uses `"DarkMode_Explorer::Button"` as the theme class if experimental dark mode is active;
 * otherwise falls back to `VSCLASS_BUTTON`.
 *
 * @param[in] hWnd Handle to the list view control with extended checkbox style.
 *
 * @see setDarkCheckboxes()
 *
 * @note Does nothing on pre-Windows 11 systems or if checkboxes are not enabled.
 */
void dmlib::setDarkListViewCheckboxes(HWND hWnd)
{
	if (const auto lvExStyle = ListView_GetExtendedListViewStyle(hWnd);
		(lvExStyle & LVS_EX_CHECKBOXES) != LVS_EX_CHECKBOXES)
	{
		return;
	}

	setDarkCheckboxes(hWnd, ListView_GetImageList(hWnd, LVSIL_STATE), ViewCheckbox::listView);
}

/**
 * @brief Replaces default tree view checkboxes with themed dark-mode versions on Windows 11.
 *
 * If the tree view uses `TVS_CHECKBOXES` or any combination of `TVS_EX_PARTIALCHECKBOXES`,
 * `TVS_EX_EXCLUSIONCHECKBOXES`, `TVS_EX_DIMMEDCHECKBOXES` extended styles and is running on
 * Windows 11 or later, this function then manually draws the checkbox state visuals using
 * themed drawing APIs, then inserts the resulting icons into the state image list.
 *
 * Uses `"DarkMode_Explorer::Button"` as the theme class if experimental dark mode is active;
 * otherwise falls back to `VSCLASS_BUTTON`.
 *
 * @param[in] hWnd Handle to the tree view control with normal or extended checkbox style.
 *
 * @see setDarkCheckboxes()
 *
 * @note Does nothing on pre-Windows 11 systems or if checkboxes are not enabled.
 */
void dmlib::setDarkTreeViewCheckboxes(HWND hWnd)
{
	if (hWnd == nullptr)
	{
		return;
	}

	ViewCheckbox tvType = ViewCheckbox::tvSimple;
	if (const auto tvStyle = ::GetWindowLongPtr(hWnd, GWL_STYLE);
		(tvStyle & TVS_CHECKBOXES) == TVS_CHECKBOXES)
	{
		// do nothing tvType already has ViewCheckbox::tvSimple
	}
	else if (const auto tvExStyle = TreeView_GetExtendedStyle(hWnd);
		(tvExStyle & (TVS_EX_PARTIALCHECKBOXES | TVS_EX_EXCLUSIONCHECKBOXES | TVS_EX_DIMMEDCHECKBOXES)) != 0)
	{
		tvType = ViewCheckbox::tvExtended;
	}
	else
	{
		return;
	}

	setDarkCheckboxes(hWnd, TreeView_GetImageList(hWnd, TVSIL_STATE), tvType);
}

/**
 * @brief Sets colors and edges for a rich edit control.
 *
 * Determines if the control has `WS_BORDER` or `WS_EX_STATICEDGE`, and sets the background
 * accordingly: uses control background color when edged, otherwise dialog background.
 *
 * In dark mode:
 * - Sets background color via `EM_SETBKGNDCOLOR`
 * - Updates default text color via `EM_SETCHARFORMAT`
 * - Applies themed scroll bars using `DarkMode_Explorer::ScrollBar`
 *
 * When not in dark mode, restores default visual styles and coloring.
 * Also conditionally swaps `WS_BORDER` and `WS_EX_CLIENTEDGE`.
 *
 * @param[in] hWnd Handle to the rich edit control.
 *
 * @see dmlib::setWindowStyle()
 * @see dmlib::setWindowExStyle()
 */
void dmlib::setDarkRichEdit(HWND hWnd)
{
	const auto nStyle = ::GetWindowLongPtrW(hWnd, GWL_STYLE);
	const bool isReadOnly = (nStyle & ES_READONLY) == ES_READONLY;

	CHARFORMATW cf{};
	cf.cbSize = sizeof(CHARFORMATW);
	cf.dwMask = CFM_COLOR;

	if (dmlib::isEnabled())
	{
		const COLORREF clrBg = (!isReadOnly ? dmlib::getCtrlBackgroundColor() : dmlib::getDlgBackgroundColor());
		::SendMessage(hWnd, EM_SETBKGNDCOLOR, 0, static_cast<LPARAM>(clrBg));

		cf.crTextColor = dmlib::getTextColor();
		::SendMessage(hWnd, EM_SETCHARFORMAT, SCF_DEFAULT, reinterpret_cast<LPARAM>(&cf));

		::SetWindowTheme(hWnd, nullptr, dmlib::isExperimentalActive() ? L"DarkMode_Explorer::ScrollBar" : nullptr);
	}
	else
	{
		cf.dwEffects = CFE_AUTOCOLOR;
		::SendMessage(hWnd, EM_SETBKGNDCOLOR, TRUE, 0);
		::SendMessage(hWnd, EM_SETCHARFORMAT, SCF_DEFAULT, reinterpret_cast<LPARAM>(&cf));

		::SetWindowTheme(hWnd, nullptr, nullptr);
	}

	dmlib::replaceClientEdgeWithBorderSafe(hWnd);
}

/**
 * @brief Sets colors for month calendar control.
 *
 * To set colors visual style needs to be disabled.
 * Title background and header day text use same color #0078D7 Windows accent blue.
 *
 * @param[in] hWnd Handle to the month calendar control.
 *
 * @see dmlib::disableVisualStyle()
 */
void dmlib::setDarkMonthCalendar(HWND hWnd)
{
	dmlib::disableVisualStyle(hWnd, dmlib::isEnabled());
	MonthCal_SetColor(hWnd, MCSC_BACKGROUND, dmlib::isEnabled() ? dmlib::getDlgBackgroundColor() : ::GetSysColor(COLOR_3DFACE));
	if (dmlib::isEnabled())
	{
		MonthCal_SetColor(hWnd, MCSC_MONTHBK, dmlib::getCtrlBackgroundColor());
		MonthCal_SetColor(hWnd, MCSC_TEXT, dmlib::getTextColor());
		MonthCal_SetColor(hWnd, MCSC_TITLEBK, dmlib_color::kAccentBlue);
		MonthCal_SetColor(hWnd, MCSC_TITLETEXT, dmlib::getTextColor());
		MonthCal_SetColor(hWnd, MCSC_TRAILINGTEXT, dmlib::getDisabledTextColor());
	}
}

/**
 * @brief Applies visual styles; ctl color message and child controls subclassings to a window safely.
 *
 * Ensures the specified window is not `nullptr` and then:
 * - Enables the dark title bar
 * - Subclasses the window for control ctl coloring
 * - Applies theming and subclassing to child controls
 *
 *
 * @param[in]   hWnd                Handle to the window. No action taken if `nullptr`.
 * @param[in]   useWin11Features    `true` to enable Windows 11 specific styling like Mica or rounded corners.
 *
 * @note Should not be used in combination with @ref dmlib::setDarkWndNotifySafeEx
 *       and @ref dmlib::setDarkWndNotifySafe to avoid overlapping styling logic.
 *
 * @see dmlib::setDarkWndNotifySafeEx()
 * @see dmlib::setDarkWndNotifySafe()
 * @see dmlib::setDarkTitleBarEx()
 * @see dmlib::setWindowCtlColorSubclass()
 * @see dmlib::setChildCtrlsSubclassAndTheme()
 * @see dmlib::setDarkWndSafe()
 */
void dmlib::setDarkWndSafeEx(HWND hWnd, bool useWin11Features)
{
	if (hWnd == nullptr)
	{
		return;
	}

	dmlib::setDarkTitleBarEx(hWnd, useWin11Features);
	dmlib::setWindowCtlColorSubclass(hWnd);
	dmlib::setChildCtrlsSubclassAndTheme(hWnd);
}

/**
 * @brief Applies visual styles; ctl color message and child controls subclassings with Windows 11 features.
 *
 * Forwards to `dmlib::setDarkWndSafeEx` with parameter `useWin11Features` as `true`.
 *
 * @param[in] hWnd Handle to the window.
 *
 * @see dmlib::setDarkWndSafeEx()
 */
void dmlib::setDarkWndSafe(HWND hWnd)
{
	dmlib::setDarkWndSafeEx(hWnd, true);
}

/**
 * @brief Applies visual styles; ctl color message, child controls, custom drawing, and setting change subclassings to a window safely.
 *
 * Ensures the specified window is not `nullptr` and then:
 * - Enables the dark title bar
 * - Subclasses the window for control coloring
 * - Applies theming and subclassing to child controls
 * - Enables custom draw-based theming via notification subclassing
 * - Subclasses the window to handle dark mode change if window mode is enabled.
 *
 * @param[in]   hWnd                        Handle to the window. No action taken if `nullptr`.
 * @param[in]   setSettingChangeSubclass    `true` to set setting change subclass if applicable.
 * @param[in]   useWin11Features            `true` to enable Windows 11 specific styling like Mica or rounded corners.
 *
 * @note `setSettingChangeSubclass = true` should be used only on main window.
 *       For other secondary windows and controls use @ref dmlib::setDarkWndNotifySafe.
 *       Should not be used in combination with @ref dmlib::setDarkWndSafe
 *       and @ref dmlib::setDarkWndNotifySafe to avoid overlapping styling logic.
 *
 * @see dmlib::setDarkWndNotifySafe()
 * @see dmlib::setDarkWndSafe()
 * @see dmlib::setDarkTitleBarEx()
 * @see dmlib::setWindowCtlColorSubclass()
 * @see dmlib::setWindowNotifyCustomDrawSubclass()
 * @see dmlib::setChildCtrlsSubclassAndTheme()
 * @see dmlib::isWindowsModeEnabled()
 * @see dmlib::setWindowSettingChangeSubclass()
 */
void dmlib::setDarkWndNotifySafeEx(HWND hWnd, bool setSettingChangeSubclass, bool useWin11Features)
{
	if (hWnd == nullptr)
	{
		return;
	}

	dmlib::setDarkTitleBarEx(hWnd, useWin11Features);
	dmlib::setWindowCtlColorSubclass(hWnd);
	dmlib::setWindowNotifyCustomDrawSubclass(hWnd);
	dmlib::setChildCtrlsSubclassAndTheme(hWnd);
	if (setSettingChangeSubclass && dmlib::isWindowsModeEnabled())
	{
		dmlib::setWindowSettingChangeSubclass(hWnd);
	}
}

/**
 * @brief Applies visual styles; ctl color message, child controls, and custom drawing subclassings with Windows 11 features.
 *
 * Calls @ref dmlib::setDarkWndNotifySafeEx with `setSettingChangeSubclass = false`
 * and `useWin11Features = true`, streamlining dark mode setup for secondary or transient windows
 * that don't need to track system dark mode changes.
 *
 * @param[in] hWnd Handle to the target window.
 *
 * @note Should not be used in combination with @ref dmlib::setDarkWndSafe
 *       and @ref dmlib::setDarkWndNotifySafeEx to avoid overlapping styling logic.
 *
 * @see dmlib::setDarkWndNotifySafeEx()
 * @see dmlib::setDarkWndSafe()
 */
void dmlib::setDarkWndNotifySafe(HWND hWnd)
{
	dmlib::setDarkWndNotifySafeEx(hWnd, false, true);
}

/**
 * @brief Enables or disables theme-based dialog background textures in classic mode.
 *
 * Applies `ETDT_ENABLETAB` only when `theme` is `true` and the current mode is classic.
 * This replaces the default classic gray background with a lighter themed texture.
 * Otherwise disables themed dialog textures with `ETDT_DISABLE`.
 *
 * @param[in]   hWnd    Handle to the target dialog window.
 * @param[in]   theme   `true` to enable themed tab textures in classic mode.
 *
 * @see EnableThemeDialogTexture
 */
void dmlib::enableThemeDialogTexture(HWND hWnd, bool theme)
{
	::EnableThemeDialogTexture(hWnd, theme && (g_dmCfg.m_dmType == DarkModeType::classic) ? ETDT_ENABLETAB : ETDT_DISABLE);
}

/**
 * @brief Enables or disables visual styles for a window.
 *
 * Applies `SetWindowTheme(hWnd, L"", L"")` when `doDisable` is `true`, effectively removing
 * the current theme. Restores default theming when `doDisable` is `false`.
 *
 * @param[in]   hWnd        Handle to the window.
 * @param[in]   doDisable   `true` to strip visual styles, `false` to re-enable them.
 *
 * @see SetWindowTheme
 */
void dmlib::disableVisualStyle(HWND hWnd, bool doDisable)
{
	if (doDisable)
	{
		::SetWindowTheme(hWnd, L"", L"");
	}
	else
	{
		::SetWindowTheme(hWnd, nullptr, nullptr);
	}
}

/**
 * @brief Calculates perceptual lightness of a COLORREF color.
 *
 * Converts the RGB color to linear space and calculates perceived lightness.
 *
 * @param[in] clr COLORREF in 0xBBGGRR format.
 * @return Lightness value as a double.
 */
double dmlib::calculatePerceivedLightness(COLORREF clr)
{
	return dmlib_color::calculatePerceivedLightness(clr);
}

/**
 * @brief Retrieves the current TreeView style configuration.
 *
 * @return Integer with enum value corresponding to the current `TreeViewStyle`.
 */
int dmlib::getTreeViewStyle()
{
	return static_cast<int>(g_dmCfg.m_tvStyle);
}

/**
 * @brief  Set TreeView style
 *
 * @param tvStyle TreeView style to set.
 */
static void setTreeViewStyle(dmlib::TreeViewStyle tvStyle) noexcept
{
	g_dmCfg.m_tvStyle = tvStyle;
}

/**
 * @brief Determines appropriate TreeView style based on background perceived lightness.
 *
 * Checks the perceived lightness of the current view background and
 * selects a corresponding style: dark, light, or classic. Style selection
 * is based on how far the lightness deviates from the middle gray threshold range
 * around the midpoint value (50.0).
 *
 * @see dmlib::calculatePerceivedLightness()
 */
void dmlib::calculateTreeViewStyle()
{
	static constexpr double middle = 50.0;

	if (const COLORREF bgColor = dmlib::getViewBackgroundColor();
		g_dmCfg.m_tvBackground != bgColor || (g_dmCfg.m_lightness - middle) == 0.0)
	{
		g_dmCfg.m_lightness = dmlib::calculatePerceivedLightness(bgColor);
		g_dmCfg.m_tvBackground = bgColor;
	}

	if (g_dmCfg.m_lightness < (middle - kMiddleGrayRange))
	{
		setTreeViewStyle(TreeViewStyle::dark);
	}
	else if (g_dmCfg.m_lightness > (middle + kMiddleGrayRange))
	{
		setTreeViewStyle(TreeViewStyle::light);
	}
	else
	{
		setTreeViewStyle(TreeViewStyle::classic);
	}
}

/**
 * @brief (Re)applies the appropriate window theme style to the specified TreeView .
 *
 * Updates the TreeView's visual behavior and theme based on the currently selected
 * style @ref dmlib::getTreeViewStyle. It conditionally adjusts the `TVS_TRACKSELECT`
 * style flag and applies a matching visual theme using `SetWindowTheme()`.
 *
 * If `force` is `true`, the style is applied regardless of previous state.
 * Otherwise, the update occurs only if the style has changed since the last update.
 *
 * - `light`: Enables `TVS_TRACKSELECT`, applies "Explorer" theme.
 * - `dark`: If supported, enables `TVS_TRACKSELECT`, applies "DarkMode_Explorer" theme.
 * - `classic`: Disables `TVS_TRACKSELECT`, clears the theme.
 *
 * @param[in]   hWnd    Handle to the TreeView control.
 * @param[in]   force   Whether to forcibly reapply the style even if unchanged.
 *
 * @see TreeViewStyle
 * @see dmlib::getTreeViewStyle()
 * @see dmlib::getPrevTreeViewStyle()
 */
void dmlib::setTreeViewWindowThemeEx(HWND hWnd, bool force)
{
	if (!force && dmlib::getPrevTreeViewStyle() == dmlib::getTreeViewStyle())
	{
		return;
	}

	auto nStyle = ::GetWindowLongPtr(hWnd, GWL_STYLE);
	const bool hasHotStyle = (nStyle & TVS_TRACKSELECT) == TVS_TRACKSELECT;
	bool change = false;
	const wchar_t* strSubAppName = nullptr;

	switch (static_cast<TreeViewStyle>(dmlib::getTreeViewStyle()))
	{
		case TreeViewStyle::light:
		{
			if (!hasHotStyle)
			{
				nStyle |= TVS_TRACKSELECT;
				change = true;
			}
			strSubAppName = L"Explorer";
			break;
		}

		case TreeViewStyle::dark:
		{
			if (dmlib::isExperimentalSupported())
			{
				if (!hasHotStyle)
				{
					nStyle |= TVS_TRACKSELECT;
					change = true;
				}
				strSubAppName = L"DarkMode_Explorer";
				break;
			}
			[[fallthrough]];
		}

		case TreeViewStyle::classic:
		{
			if (hasHotStyle)
			{
				nStyle &= ~TVS_TRACKSELECT;
				change = true;
			}
			break;
		}
	}

	if (change)
	{
		::SetWindowLongPtr(hWnd, GWL_STYLE, nStyle);
	}

	::SetWindowTheme(hWnd, strSubAppName, nullptr);
}

/**
 * @brief Applies the appropriate window theme style to the specified TreeView.
 *
 * Forwards to `dmlib::setTreeViewWindowThemeEx` with `force = false` to change tree view style
 * only if needed.
 *
 * @param[in] hWnd  Handle to the TreeView control.
 *
 * @see dmlib::setTreeViewWindowThemeEx()
 */
void dmlib::setTreeViewWindowTheme(HWND hWnd)
{
	dmlib::setTreeViewWindowThemeEx(hWnd, false);
}

/**
 * @brief Retrieves the previous TreeView style configuration.
 *
 * @return Reference to the previous `TreeViewStyle`.
 */
int dmlib::getPrevTreeViewStyle()
{
	return static_cast<int>(g_dmCfg.m_tvStylePrev);
}

/**
 * @brief Stores the current TreeView style as the previous style for later comparison.
 */
void dmlib::setPrevTreeViewStyle()
{
	g_dmCfg.m_tvStylePrev = static_cast<TreeViewStyle>(dmlib::getTreeViewStyle());
}

/**
 * @brief Checks whether the current theme is dark.
 *
 * Internally it use TreeView style to determine if dark theme is used.
 *
 * @return `true` if the active style is `TreeViewStyle::dark`, otherwise `false`.
 *
 * @see dmlib::getTreeViewStyle()
 */
bool dmlib::isThemeDark()
{
	return static_cast<TreeViewStyle>(dmlib::getTreeViewStyle()) == TreeViewStyle::dark;
}

/**
 * @brief Checks whether the color is dark.
 *
 * @param clr Color to check.
 *
 * @return `true` if the perceived lightness of the color
 *         is less than (50.0 - kMiddleGrayRange), otherwise `false`.
 *
 * @see dmlib::calculatePerceivedLightness()
 */
bool dmlib::isColorDark(COLORREF clr)
{
	static constexpr double middle = 50.0;
	return dmlib::calculatePerceivedLightness(clr) < (middle - kMiddleGrayRange);
}

/**
 * @brief Forces a window to redraw its non-client frame.
 *
 * Triggers a non-client area update by using `SWP_FRAMECHANGED` without changing
 * size, position, or Z-order.
 *
 * @param[in] hWnd Handle to the target window.
 */
void dmlib::redrawWindowFrame(HWND hWnd)
{
	::SetWindowPos(hWnd, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
}

/**
 * @brief Sets or clears a specific window style or extended style.
 *
 * Checks if the specified `dwFlag` is already set and toggles it if needed.
 * Only valid for `GWL_STYLE` or `GWL_EXSTYLE`.
 *
 * @param[in]   hWnd    Handle to the window.
 * @param[in]   setFlag `true` to set the flag, `false` to clear it.
 * @param[in]   dwFlag  Style bitmask to apply.
 * @param[in]   gwlIdx  Either `GWL_STYLE` or `GWL_EXSTYLE`.
 * @return `TRUE` if modified, `FALSE` if unchanged, `-1` if invalid index.
 */
static int setWindowLongPtrStyle(HWND hWnd, bool setFlag, LONG_PTR dwFlag, int gwlIdx) noexcept
{
	if ((gwlIdx != GWL_STYLE) && (gwlIdx != GWL_EXSTYLE))
	{
		return -1;
	}

	auto nStyle = ::GetWindowLongPtrW(hWnd, gwlIdx);

	if (const bool hasFlag = (nStyle & dwFlag) == dwFlag;
		setFlag != hasFlag)
	{
		nStyle ^= dwFlag;
		::SetWindowLongPtrW(hWnd, gwlIdx, nStyle);
		return TRUE;
	}
	return FALSE;
}

/**
 * @brief Sets a window's standard style flags and redraws window if needed.
 *
 * Wraps @ref setWindowLongPtrStyle with `GWL_STYLE`
 * and calls @ref dmlib::redrawWindowFrame if a change occurs.
 *
 * @param[in]   hWnd        Handle to the target window.
 * @param[in]   setStyle    `true` to set the flag, `false` to remove it.
 * @param[in]   styleFlag   Style bit to modify.
 */
void dmlib::setWindowStyle(HWND hWnd, bool setStyle, LONG_PTR styleFlag)
{
	if (setWindowLongPtrStyle(hWnd, setStyle, styleFlag, GWL_STYLE) == TRUE)
	{
		dmlib::redrawWindowFrame(hWnd);
	}
}

/**
 * @brief Sets a window's extended style flags and redraws window if needed.
 *
 * Wraps @ref setWindowLongPtrStyle with `GWL_EXSTYLE`
 * and calls @ref dmlib::redrawWindowFrame if a change occurs.
 *
 * @param[in]   hWnd        Handle to the target window.
 * @param[in]   setExStyle  `true` to set the flag, `false` to remove it.
 * @param[in]   exStyleFlag Extended style bit to modify.
 */
void dmlib::setWindowExStyle(HWND hWnd, bool setExStyle, LONG_PTR exStyleFlag)
{
	if (setWindowLongPtrStyle(hWnd, setExStyle, exStyleFlag, GWL_EXSTYLE) == TRUE)
	{
		dmlib::redrawWindowFrame(hWnd);
	}
}

/**
 * @brief Replaces an extended edge (e.g. client edge) with a standard window border.
 *
 * The given `exStyleFlag` must be a valid edge-related extended window style:
 * - `WS_EX_CLIENTEDGE`
 * - `WS_EX_DLGMODALFRAME`
 * - `WS_EX_STATICEDGE`
 * - `WS_EX_WINDOWEDGE`
 * ...or any combination of these.
 *
 * If `replace` is `true`, the specified extended edge style(s) are removed and
 * `WS_BORDER` is applied. If `false`, the edge style(s) are restored and `WS_BORDER` is cleared.
 *
 * @param[in]   hWnd        Handle to the target window.
 * @param[in]   replace     `true` to apply standard border; `false` to restore extended edge(s).
 * @param[in]   exStyleFlag One or more valid edge-related extended styles.
 *
 * @see dmlib::setWindowExStyle()
 * @see dmlib::setWindowStyle()
 */
void dmlib::replaceExEdgeWithBorder(HWND hWnd, bool replace, LONG_PTR exStyleFlag)
{
	dmlib::setWindowStyle(hWnd, replace, WS_BORDER);
	dmlib::setWindowExStyle(hWnd, !replace, exStyleFlag);
}

/**
 * @brief Safely toggles `WS_EX_CLIENTEDGE` with `WS_BORDER`.
 *
 * @param[in]   hWnd Handle to the target window. No action is taken if `hWnd` is `nullptr`.
 * @param[in]   replace `true` to apply `WS_BORDER`; `false` to restore`WS_EX_CLIENTEDGE`.
 *
 * @note Functions only affects controls, which have `WS_EX_CLIENTEDGE` or `WS_BORDER` (ex)styles.
 *
 * @see dmlib::replaceExEdgeWithBorder()
 */
void dmlib::replaceClientEdgeWithBorderSafeEx(HWND hWnd, bool replace)
{
	if (hWnd == nullptr)
	{
		return;
	}

	const auto nStyle = ::GetWindowLongPtrW(hWnd, GWL_STYLE);
	const bool hasBorder = (nStyle & WS_BORDER) == WS_BORDER;

	const auto nExStyle = ::GetWindowLongPtrW(hWnd, GWL_EXSTYLE);
	const bool hasClientEdge = (nExStyle & WS_EX_CLIENTEDGE) == WS_EX_CLIENTEDGE;

	if (hasBorder || hasClientEdge)
	{
		dmlib::replaceExEdgeWithBorder(hWnd, replace, WS_EX_CLIENTEDGE);
	}
}

/**
 * @brief Safely toggles `WS_EX_CLIENTEDGE` with `WS_BORDER` based on dark mode state.
 *
 * If dark mode is enabled, removes `WS_EX_CLIENTEDGE` and applies `WS_BORDER`.
 * Otherwise restores the extended edge style.
 *
 * @param[in] hWnd Handle to the target window. No action is taken if `hWnd` is `nullptr`.
 *
 * @note Functions only affects controls, which have `WS_EX_CLIENTEDGE` or `WS_BORDER` (ex)styles.
 *
 * @see dmlib::replaceClientEdgeWithBorderSafeEx()
 */
void dmlib::replaceClientEdgeWithBorderSafe(HWND hWnd)
{
	dmlib::replaceClientEdgeWithBorderSafeEx(hWnd, dmlib::isEnabled());
}

/**
 * @brief Applies classic-themed styling to a progress bar in non-classic mode.
 *
 * When dark mode is enabled, applies `WS_BORDER`, removes visual styles
 * to allow to set custom background and fill colors using:
 * - Background: `dmlib::getCtrlBackgroundColor()`
 * - Fill: Hardcoded light green #06B025, dark green #0F7B0F,
 *   or azure #60CDFF via `PBM_SETBARCOLOR`
 *
 * Typically used for marquee style progress bar.
 *
 * @param[in] hWnd Handle to the progress bar control.
 *
 * @see dmlib::setWindowStyle()
 * @see dmlib::disableVisualStyle()
 */
void dmlib::setProgressBarClassicTheme(HWND hWnd)
{
	dmlib::setWindowStyle(hWnd, dmlib::isEnabled(), WS_BORDER);
	dmlib::disableVisualStyle(hWnd, dmlib::isEnabled());
	dmlib::setWindowExStyle(hWnd, false, WS_EX_STATICEDGE);
	if (dmlib::isEnabled())
	{
		::SendMessage(hWnd, PBM_SETBKCOLOR, 0, static_cast<LPARAM>(dmlib::getCtrlBackgroundColor()));
		static constexpr COLORREF greenLight = dmlib_color::HEXRGB(0x06B025);
		static constexpr COLORREF greenDark = dmlib_color::HEXRGB(0x0F7B0F);
		static constexpr COLORREF azureDark = dmlib_color::HEXRGB(0x60CDFF);
		static const auto clrDark = doesWin11SupportDarkThemeStyle() ? azureDark : greenDark;
		::SendMessage(hWnd, PBM_SETBARCOLOR, 0, static_cast<LPARAM>(dmlib::isExperimentalActive() ? clrDark : greenLight));
	}
}

/**
 * @brief Handles text and background colorizing for read-only controls.
 *
 * Sets the text color and background color on the provided HDC.
 * Returns the corresponding background brush for painting.
 * Typically used for read-only controls (e.g. edit control and combo box' list box).
 * Typically used in response to `WM_CTLCOLORSTATIC` or in `WM_CTLCOLORLISTBOX`
 * via @ref dmlib::onCtlColorListbox
 *
 * @param[in] hdc Handle to the device context (HDC) receiving the drawing instructions.
 * @return Background brush to use for painting, or `FALSE` (0) if classic mode is enabled
 *         and `_DARKMODELIB_DLG_PROC_CTLCOLOR_RETURNS` is defined
 *         and has non-zero unsigned value.
 *
 * @see dmlib::WindowCtlColorSubclass()
 * @see dmlib::onCtlColorListbox()
 */
LRESULT dmlib::onCtlColor(HDC hdc)
{
#if defined(_DARKMODELIB_DLG_PROC_CTLCOLOR_RETURNS) && (_DARKMODELIB_DLG_PROC_CTLCOLOR_RETURNS > 0)
	if (!dmlib::_isEnabled())
	{
		return FALSE;
	}
#endif
	::SetTextColor(hdc, dmlib::getTextColor());
	::SetBkColor(hdc, dmlib::getBackgroundColor());
	return reinterpret_cast<LRESULT>(dmlib::getBackgroundBrush());
}

/**
 * @brief Handles text and background colorizing for interactive controls.
 *
 * Sets the text and background colors on the provided HDC.
 * Returns the corresponding brush used to paint the background.
 * Typically used in response to `WM_CTLCOLOREDIT` and `WM_CTLCOLORLISTBOX`
 * via @ref dmlib::onCtlColorListbox
 *
 * @param[in] hdc Handle to the device context for the target control.
 * @return The background brush, or `FALSE` if dark mode is disabled and
 *         `_DARKMODELIB_DLG_PROC_CTLCOLOR_RETURNS` is defined
 *         and has non-zero unsigned value.
 *
 * @see dmlib::WindowCtlColorSubclass()
 * @see dmlib::onCtlColorListbox()
 */
LRESULT dmlib::onCtlColorCtrl(HDC hdc)
{
#if defined(_DARKMODELIB_DLG_PROC_CTLCOLOR_RETURNS) && (_DARKMODELIB_DLG_PROC_CTLCOLOR_RETURNS > 0)
	if (!dmlib::_isEnabled())
	{
		return FALSE;
	}
#endif

	::SetTextColor(hdc, dmlib::getTextColor());
	::SetBkColor(hdc, dmlib::getCtrlBackgroundColor());
	return reinterpret_cast<LRESULT>(dmlib::getCtrlBackgroundBrush());
}

/**
 * @brief Handles text and background colorizing for window and disabled non-text controls.
 *
 * Sets the text and background colors on the provided HDC.
 * Returns the corresponding brush used to paint the background.
 * Typically used in response to `WM_CTLCOLORDLG`, `WM_CTLCOLORSTATIC`
 * and `WM_CTLCOLORLISTBOX` via @ref dmlib::onCtlColorListbox
 *
 * @param[in] hdc Handle to the device context for the target control.
 * @return The background brush, or `FALSE` if dark mode is disabled and
 *         `_DARKMODELIB_DLG_PROC_CTLCOLOR_RETURNS` is defined
 *         and has non-zero unsigned value.
 *
 * @see dmlib::WindowCtlColorSubclass()
 * @see dmlib::onCtlColorListbox()
 */
LRESULT dmlib::onCtlColorDlg(HDC hdc)
{
#if defined(_DARKMODELIB_DLG_PROC_CTLCOLOR_RETURNS) && (_DARKMODELIB_DLG_PROC_CTLCOLOR_RETURNS > 0)
	if (!dmlib::_isEnabled())
	{
		return FALSE;
	}
#endif

	::SetTextColor(hdc, dmlib::getTextColor());
	::SetBkColor(hdc, dmlib::getDlgBackgroundColor());
	return reinterpret_cast<LRESULT>(dmlib::getDlgBackgroundBrush());
}

/**
 * @brief Handles text and background colorizing for error state (for specific usage).
 *
 * Sets the text and background colors on the provided HDC.
 *
 * @param[in] hdc Handle to the device context for the target control.
 * @return The background brush, or `FALSE` if dark mode is disabled and
 *         `_DARKMODELIB_DLG_PROC_CTLCOLOR_RETURNS` is defined
 *         and has non-zero unsigned value.
 *
 * @see dmlib::WindowCtlColorSubclass()
 */
LRESULT dmlib::onCtlColorError(HDC hdc)
{
#if defined(_DARKMODELIB_DLG_PROC_CTLCOLOR_RETURNS) && (_DARKMODELIB_DLG_PROC_CTLCOLOR_RETURNS > 0)
	if (!dmlib::_isEnabled())
	{
		return FALSE;
	}
#endif

	::SetTextColor(hdc, dmlib::getTextColor());
	::SetBkColor(hdc, dmlib::getErrorBackgroundColor());
	return reinterpret_cast<LRESULT>(dmlib::getErrorBackgroundBrush());
}

/**
 * @brief Handles text and background colorizing for static text controls.
 *
 * Sets the text and background colors on the provided HDC.
 * Colors depend on if control is enabled.
 * Returns the corresponding brush used to paint the background.
 * Typically used in response to `WM_CTLCOLORSTATIC`.
 *
 * @param[in]   hdc             Handle to the device context for the target control.
 * @param[in]   isTextEnabled   `true` if control should use enabled colors.
 * @return The background brush, or `FALSE` if dark mode is disabled and
 *         `_DARKMODELIB_DLG_PROC_CTLCOLOR_RETURNS` is defined
 *         and has non-zero unsigned value.
 *
 * @see dmlib::WindowCtlColorSubclass()
 */
LRESULT dmlib::onCtlColorDlgStaticText(HDC hdc, bool isTextEnabled)
{
#if defined(_DARKMODELIB_DLG_PROC_CTLCOLOR_RETURNS) && (_DARKMODELIB_DLG_PROC_CTLCOLOR_RETURNS > 0)
	if (!dmlib::_isEnabled())
	{
		::SetTextColor(hdc, ::GetSysColor(isTextEnabled ? COLOR_WINDOWTEXT : COLOR_GRAYTEXT));
		return FALSE;
	}
#endif
	::SetTextColor(hdc, isTextEnabled ? dmlib::getTextColor() : dmlib::getDisabledTextColor());
	::SetBkColor(hdc, dmlib::getDlgBackgroundColor());
	return reinterpret_cast<LRESULT>(dmlib::getDlgBackgroundBrush());
}

/**
 * @brief Handles text and background colorizing for SysLink controls.
 *
 * Sets the text and background colors on the provided HDC.
 * Colors depend on if control is enabled.
 * Returns the corresponding brush used to paint the background.
 * Typically used in response to `WM_CTLCOLORSTATIC`.
 *
 * @param[in]   hdc             Handle to the device context for the target control.
 * @param[in]   isTextEnabled   `true` if control should use enabled colors.
 * @return The background brush, or `FALSE` if dark mode is disabled and
 *         `_DARKMODELIB_DLG_PROC_CTLCOLOR_RETURNS` is defined
 *         and has non-zero unsigned value.
 *
 * @see dmlib::WindowCtlColorSubclass()
 */
LRESULT dmlib::onCtlColorDlgLinkText(HDC hdc, bool isTextEnabled)
{
#if defined(_DARKMODELIB_DLG_PROC_CTLCOLOR_RETURNS) && (_DARKMODELIB_DLG_PROC_CTLCOLOR_RETURNS > 0)
	if (!dmlib::_isEnabled())
	{
		::SetTextColor(hdc, ::GetSysColor(isTextEnabled ? COLOR_HOTLIGHT : COLOR_GRAYTEXT));
		return FALSE;
	}
#endif
	::SetTextColor(hdc, isTextEnabled ? dmlib::getLinkTextColor() : dmlib::getDisabledTextColor());
	::SetBkColor(hdc, dmlib::getDlgBackgroundColor());
	return reinterpret_cast<LRESULT>(dmlib::getDlgBackgroundBrush());
}

/**
 * @brief Handles text and background colorizing for list box controls.
 *
 * Inspects the list box style flags to detect if it's part of a combo box (via `LBS_COMBOBOX`)
 * and whether experimental feature is active. Based on the context, delegates to:
 * - @ref dmlib::onCtlColorCtrl for standard enabled listboxes
 * - @ref dmlib::onCtlColorDlg for disabled ones or when dark mode is disabled
 * - @ref dmlib::onCtlColor for combo box' listbox
 *
 * @param[in]   wParam  WPARAM from `WM_CTLCOLORLISTBOX`, representing the HDC.
 * @param[in]   lParam  LPARAM from `WM_CTLCOLORLISTBOX`, representing the HWND of the listbox.
 * @return The brush handle as LRESULT for background painting, or `FALSE` if not themed.
 *
 * @see dmlib::WindowCtlColorSubclass()
 * @see dmlib::onCtlColor()
 * @see dmlib::onCtlColorCtrl()
 * @see dmlib::onCtlColorDlg()
 */
LRESULT dmlib::onCtlColorListbox(WPARAM wParam, LPARAM lParam)
{
	auto hdc = reinterpret_cast<HDC>(wParam);
	auto hWnd = reinterpret_cast<HWND>(lParam);

	if (const auto nStyle = ::GetWindowLongPtr(hWnd, GWL_STYLE);
		((nStyle & LBS_COMBOBOX) != LBS_COMBOBOX) // is not child of combo box
		|| !dmlib::isExperimentalActive())
	{
		if (::IsWindowEnabled(hWnd) == TRUE)
		{
			return dmlib::onCtlColorCtrl(hdc);
		}
		return dmlib::onCtlColorDlg(hdc);
	}
	return dmlib::onCtlColor(hdc);
}

/**
 * @brief Window subclass procedure for customizing Font and Color dialog boxes.
 *
 * Hooks are installed only in needed messages to avoid side effects.
 *
 * @param[in]   hWnd        Window handle being subclassed.
 * @param[in]   uMsg        Message identifier.
 * @param[in]   wParam      Message-specific data.
 * @param[in]   lParam      Message-specific data.
 * @param[in]   uIdSubclass Subclass identifier.
 * @param[in]   dwRefData   Reference data (unused).
 * @return LRESULT Result of message processing.
 *
 * @see dmlib::HookDlgProc()
 */
static LRESULT CALLBACK ComDlgSubclass(
	HWND hWnd,
	UINT uMsg,
	WPARAM wParam,
	LPARAM lParam,
	UINT_PTR uIdSubclass,
	[[maybe_unused]] DWORD_PTR dwRefData
) noexcept
{
	if (!dmlib::isEnabled() && uMsg != WM_NCDESTROY)
	{
		return ::DefSubclassProc(hWnd, uMsg, wParam, lParam);
	}

	switch (uMsg)
	{
		case WM_NCDESTROY:
		{
			::RemoveWindowSubclass(hWnd, ComDlgSubclass, uIdSubclass);
			break;
		}

		case WM_MOUSEMOVE: // for Color dialog box luminosity slide control
		{
			if (const auto autoHook = dmlib_hook::AutoHook<dmlib_hook::ClrGetSysColorBrush>();
				autoHook)
			{
				return ::DefSubclassProc(hWnd, uMsg, wParam, lParam);
			}
			break;
		}

		case WM_PAINT: // for Font dialog box preview background (control has id stc5 (0x444)) and for Color dialog box luminosity slide control
		{
			const auto ahFontSysColor = dmlib_hook::AutoHook<dmlib_hook::FontGetSysColor>();
			const auto ahClrGetSysColorBrush = dmlib_hook::AutoHook<dmlib_hook::ClrGetSysColorBrush>();
			return ::DefSubclassProc(hWnd, uMsg, wParam, lParam);
		}

		case WM_DRAWITEM: // for Font dialog box combo boxes and their list boxes
		{
			if (wParam == cmb1 || wParam == cmb2 || wParam == cmb3 || wParam == cmb4 || wParam == cmb5)
			{
				const auto ahFontSysColor = dmlib_hook::AutoHook<dmlib_hook::FontGetSysColor>();
				const auto ahFontFillRect = dmlib_hook::AutoHook<dmlib_hook::FontFillRect>();
				return ::DefSubclassProc(hWnd, uMsg, wParam, lParam);
			}

			break;
		}

		case WM_COMMAND: // for Color dialog box luminosity slide control
		{
			const int id = LOWORD(wParam);
			const int nCode = HIWORD(wParam);

			if (nCode == EN_CHANGE
				&& (id == COLOR_RED
					|| id == COLOR_GREEN
					|| id == COLOR_BLUE))
			{
				if (const auto autoHook = dmlib_hook::AutoHook<dmlib_hook::ClrGetSysColorBrush>();
					autoHook)
				{
					return ::DefSubclassProc(hWnd, uMsg, wParam, lParam);
				}
				break;
			}

			if (nCode == EN_CHANGE && id == COLOR_LUM)
			{
				const auto retVal = ::DefSubclassProc(hWnd, uMsg, wParam, lParam);

				RECT rcClient{};
				::GetClientRect(hWnd, &rcClient);

				RECT rcLum{};
				::GetWindowRect(::GetDlgItem(hWnd, COLOR_LUMSCROLL), &rcLum);
				::MapWindowPoints(nullptr, hWnd, reinterpret_cast<POINT*>(&rcLum), 2);

				const LONG gap = rcLum.top - rcClient.top;
				::InflateRect(&rcLum, 0, gap);
				rcLum.left = rcLum.right;
				rcLum.right = rcClient.right;

				::RedrawWindow(hWnd, &rcLum, nullptr, RDW_INVALIDATE);
				return retVal;
			}

			if (id == IDOK && nCode == BN_CLICKED) // for Font dialog box message boxes
			{
				if (const auto autoHook = dmlib_hook::AutoHook<dmlib_hook::FontMB>();
					autoHook)
				{
					return ::DefSubclassProc(hWnd, uMsg, wParam, lParam);
				}
				break;
			}

			break;
		}

		default:
		{
			break;
		}
	}
	return ::DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

/**
 * @brief Generic hook procedure for customizing Font and Color dialog boxes with custom colors.
 *
 * @see ComDlgSubclass()
 */
UINT_PTR CALLBACK dmlib::HookDlgProc(
	HWND hWnd,
	UINT uMsg,
	[[maybe_unused]] WPARAM wParam,
	[[maybe_unused]] LPARAM lParam
)
{
	if (uMsg == WM_INITDIALOG)
	{
		dmlib::setDarkWndSafe(hWnd);
		dmlib_subclass::SetSubclass(hWnd, ComDlgSubclass, dmlib_subclass::SubclassID::comDlg);
		return TRUE;
	}
	return FALSE;
}

/**
 * @brief Window subclass procedure for removing hooks for a Color dialog box.
 *
 * @param[in]   hWnd        Window handle being subclassed.
 * @param[in]   uMsg        Message identifier.
 * @param[in]   wParam      Message-specific data.
 * @param[in]   lParam      Message-specific data.
 * @param[in]   uIdSubclass Subclass identifier.
 * @param[in]   dwRefData   Reference data (unused).
 * @return LRESULT Result of message processing.
 *
 * @see ChooseColorHookDlgProc()
 */
static LRESULT CALLBACK ChooseColorSubclass(
	HWND hWnd,
	UINT uMsg,
	WPARAM wParam,
	LPARAM lParam,
	UINT_PTR uIdSubclass,
	[[maybe_unused]] DWORD_PTR dwRefData
) noexcept
{
	if (uMsg == WM_NCDESTROY)
	{
		::RemoveWindowSubclass(hWnd, ChooseColorSubclass, uIdSubclass);
		dmlib_hook::ClrGetSysColorBrush::unhook();
	}
	
	return ::DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

/**
 * @brief Hook procedure for customizing a Color dialog box with custom dialog colors.
 *
 * @see ChooseColorSubclass()
 */
static UINT_PTR CALLBACK ChooseColorHookDlgProc(
	HWND hWnd,
	UINT uMsg,
	[[maybe_unused]] WPARAM wParam,
	[[maybe_unused]] LPARAM lParam
) noexcept
{
	if (uMsg == WM_INITDIALOG)
	{
		dmlib::setDarkWndSafe(hWnd);

		dmlib_hook::ClrGetSysColorBrush::hook();
		dmlib_subclass::SetSubclass(hWnd, ChooseColorSubclass, dmlib_subclass::SubclassID::comDlg);
		return TRUE;
	}
	return FALSE;
}

/**
 * @brief Wrapper for `ChooseColorW` to enable dialog box customizing.
 *
 * Creates a Color dialog box that enables the user to select a color.
 *
 * Enables hook with `CC_ENABLEHOOK` to use hook procedure
 * to enable customizing the Color dialog box.
 * 
 * @param[in,out] cc Contains information the `ChooseColor` function uses to initialize the Color dialog box.
 *                   After the user closes the dialog box, the system returns information about the user's selection in this structure.
 * @return BOOL If the user clicks the OK button of the dialog box, the return value is nonzero.
 *              The rgbResult member of the `CHOOSECOLOR` structure contains the RGB color value of the color selected by the user.
 * @return BOOL If the user cancels or closes the Color dialog box or an error occurs, the return value is zero.
 *
 * @see ChooseColorHookDlgProc()
 */
BOOL dmlib::darkChooseColorW(LPCHOOSECOLORW cc)
{
	if (dmlib::isEnabled())
	{
		cc->Flags |= CC_ENABLEHOOK;
		cc->lpfnHook = static_cast<LPCCHOOKPROC>(ChooseColorHookDlgProc);
	}

	return ::ChooseColorW(cc);
}

/**
 * @brief Window subclass procedure for removing hooks for a Font dialog box.
 *
 * @param[in]   hWnd        Window handle being subclassed.
 * @param[in]   uMsg        Message identifier.
 * @param[in]   wParam      Message-specific data.
 * @param[in]   lParam      Message-specific data.
 * @param[in]   uIdSubclass Subclass identifier.
 * @param[in]   dwRefData   Reference data (unused).
 * @return LRESULT Result of message processing.
 *
 * @see ChooseFontHookDlgProc()
 */
static LRESULT CALLBACK ChooseFontSubclass(
	HWND hWnd,
	UINT uMsg,
	WPARAM wParam,
	LPARAM lParam,
	UINT_PTR uIdSubclass,
	[[maybe_unused]] DWORD_PTR dwRefData
) noexcept
{
	if (uMsg == WM_NCDESTROY)
	{
		::RemoveWindowSubclass(hWnd, ChooseFontSubclass, uIdSubclass);
		dmlib_hook::FontGetSysColor::unhook();
		dmlib_hook::FontFillRect::unhook();
		dmlib_hook::FontMB::unhook();
	}

	return ::DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

/**
 * @brief Hook procedure for customizing a Font dialog box with custom dialog colors.
 *
 * @see ChooseFontSubclass()
 */
static UINT_PTR CALLBACK ChooseFontHookDlgProc(
	HWND hWnd,
	UINT uMsg,
	[[maybe_unused]] WPARAM wParam,
	[[maybe_unused]] LPARAM lParam
) noexcept
{
	if (uMsg == WM_INITDIALOG)
	{
		dmlib::setDarkWndSafe(hWnd);

		dmlib_hook::FontGetSysColor::hook();
		dmlib_hook::FontFillRect::hook();
		dmlib_hook::FontMB::hook();
		dmlib_subclass::SetSubclass(hWnd, ChooseFontSubclass, dmlib_subclass::SubclassID::comDlg);
		return TRUE;
	}
	return FALSE;
}

/**
 * @brief Wrapper for `ChooseFontW` to enable dialog box customizing.
 *
 * Creates a Font dialog box that enables the user to choose attributes for a logical font.
 * These attributes include a font family and associated font style, a point size, effects
 * (underline, strikeout, and text color), and a script (or character set).
 *
 * Enables hook with `CF_ENABLEHOOK` to use hook procedure
 * to enable customizing the Color dialog box.
 * Uses template as workaround to hook dialog layout downgrade.
 *
 * @param[in,out]   cf      Contains information that the ChooseFont function uses to initialize the Font dialog box.
 *                          After the user closes the dialog box, the system returns information about the user's selection in this structure.
 * @param[in]       tmplId  ID of template.
 * @return BOOL If the user clicks the OK button of the dialog box, the return value is `TRUE`.
 *              The members of the `CHOOSEFONT` structure indicate the user's selections.
 * @return BOOL If the user cancels or closes the Font dialog box or an error occurs, the return value is `FALSE`.
 *
 * @see ChooseFontHookDlgProc()
 */
BOOL dmlib::darkChooseFontW(LPCHOOSEFONTW cf, int tmplId)
{
	if (dmlib::isEnabled())
	{
		cf->Flags |= CF_ENABLEHOOK;
		cf->lpfnHook = static_cast<LPCFHOOKPROC>(ChooseFontHookDlgProc);
		if ((cf->Flags & CF_ENABLETEMPLATE) != CF_ENABLETEMPLATE && tmplId > 0)
		{
			cf->Flags |= CF_ENABLETEMPLATE;
			cf->hInstance = ::GetModuleHandleW(nullptr);
			cf->lpTemplateName = MAKEINTRESOURCE(tmplId);
		}
	}

	return ::ChooseFontW(cf);
}

/**
 * @brief Applies dark mode visual styles to task dialog.
 *
 * @note Currently colors cannot be customized.
 *
 * @param[in] hWnd Handle to the task dialog.
 *
 * @see dmlib_subclass::setTaskDlgChildCtrlsSubclassAndTheme()
 */
void dmlib::setDarkTaskDlg(HWND hWnd)
{
	if (dmlib::isExperimentalActive())
	{
		dmlib::setDarkTitleBar(hWnd);
		dmlib::setDarkExplorerTheme(hWnd);
		dmlib_subclass::setTaskDlgChildCtrlsSubclassAndTheme(hWnd);
	}
}

/**
 * @brief Simple task dialog callback procedure to enable dark mode support.
 *
 * @param[in]   hWnd        Handle to the task dialog.
 * @param[in]   uMsg        Message identifier.
 * @param[in]   wParam      First message parameter (unused).
 * @param[in]   lParam      Second message parameter (unused).
 * @param[in]   lpRefData   Reserved data (unused).
 * @return HRESULT A value defined by the hook procedure.
 *
 * @see dmlib::setDarkTaskDlg()
 * @see dmlib::darkTaskDialogIndirect()
 */
HRESULT CALLBACK dmlib::DarkTaskDlgCallback(
	HWND hWnd,
	UINT uMsg,
	[[maybe_unused]] WPARAM wParam,
	[[maybe_unused]] LPARAM lParam,
	[[maybe_unused]] LONG_PTR lpRefData
)
{
	if (uMsg == TDN_DIALOG_CONSTRUCTED)
	{
		dmlib::setDarkTaskDlg(hWnd);
	}
	return S_OK;
}

/**
 * @brief Wrapper for `TaskDialogIndirect` with dark mode support.
 */
HRESULT dmlib::darkTaskDialogIndirect(
	const TASKDIALOGCONFIG* pTaskConfig,
	int* pnButton,
	int* pnRadioButton,
	BOOL* pfVerificationFlagChecked
)
{
	const auto autoHook = dmlib_hook::AutoHook<dmlib_hook::TaskDlgTheme>();
	return ::TaskDialogIndirect(pTaskConfig, pnButton, pnRadioButton, pfVerificationFlagChecked);
}

/**
 * @brief Simple task dialog callback procedure for msgBoxParamToTaskDlgConfig.
 *
 * @param[in]   hWnd        Handle to the task dialog.
 * @param[in]   uMsg        Message identifier.
 * @param[in]   wParam      First message parameter (unused).
 * @param[in]   lParam      Second message parameter (unused).
 * @param[in]   lpRefData   Message box flags.
 * @return HRESULT A value defined by the hook procedure.
 *
 * @see DarkTaskDlgMsgBoxCallback()
 * @see dmlib::darkMessageBoxW()
 */
static HRESULT CALLBACK DarkTaskDlgMsgBoxCallback(
	HWND hWnd,
	UINT uMsg,
	[[maybe_unused]] WPARAM wParam,
	[[maybe_unused]] LPARAM lParam,
	[[maybe_unused]] LONG_PTR lpRefData
) noexcept
{
	const auto uType = static_cast<UINT>(lpRefData);

	if (uMsg == TDN_DIALOG_CONSTRUCTED)
	{
		dmlib::setDarkTaskDlg(hWnd);
		if ((uType & (MB_SYSTEMMODAL | MB_TOPMOST)) != 0)
		{
			::SetWindowPos(hWnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
		}

		if ((uType & MB_SETFOREGROUND) == MB_SETFOREGROUND)
		{
			::SetForegroundWindow(hWnd);
		}
	}
	return S_OK;
}

/**
 * @brief Translates a message box parameter to task dialog config.
 *
 * @note Flags MB_HELP, MB_TASKMODAL, MB_DEFAULT_DESKTOP_ONLY, MB_SERVICE_NOTIFICATION are not supported.
 *       Other flags can have limited support. Check parameter uType for more information.
 *
 * @param[in]   hWnd        Handle to the owner window of the message box.
 *                          It can be NULL if the message box has no owner.
 * @param[in]   lpText      Text to be displayed in the message box.
 * @param[in]   lpCaption   Text to be displayed in the title bar of the message box.
 * @param[in]   uType       Specifies the contents and behavior of the message box.
 *                          This parameter can be a allowed combination of the following flags:
 *                          - MB_OK
 *                          - MB_OKCANCEL
 *                          - MB_ABORTRETRYIGNORE
 *                          - MB_YESNOCANCEL
 *                          - MB_YESNO
 *                          - MB_RETRYCANCEL
 *                          - MB_CANCELTRYCONTINUE
 *
 *                          - MB_DEFBUTTON1
 *                          - MB_DEFBUTTON2
 *                          - MB_DEFBUTTON3
 *                          - MB_DEFBUTTON4 - has no effect, there is no 4th button
 *
 *                          - MB_ICONERROR
 *                          - MB_ICONQUESTION
 *                          - MB_ICONWARNING
 *                          - MB_ICONINFORMATION
 *
 *                          - MB_APPLMODAL
 *                          - MB_SYSTEMMODAL
 *
 *                          - MB_RIGHT - has no effect, use MB_RTLREADING instead
 *                          - MB_RTLREADING
 *                          - MB_SETFOREGROUND
 *                          - MB_TOPMOST
 *
 * @return TASKDIALOGCONFIG Returns the translated task dialog config.
 *
 * @see DarkTaskDlgMsgBoxCallback()
 * @see dmlib::darkMessageBoxW()
 */
static TASKDIALOGCONFIG msgBoxParamToTaskDlgConfig(
	HWND hWnd,
	LPCWSTR lpText,
	LPCWSTR lpCaption,
	UINT uType
) noexcept
{
	// base config

#ifdef _MSC_VER
	#pragma warning(push)
	#pragma warning(disable: 26476) // Expression/symbol 'name' uses a naked union 'union' with multiple type pointers: Use variant instead (type.7)
#endif
	TASKDIALOGCONFIG tdc{};
#ifdef _MSC_VER
	#pragma warning(pop)
#endif
	tdc.cbSize = sizeof(TASKDIALOGCONFIG);
	tdc.hwndParent = hWnd;
	tdc.hInstance = nullptr;
	tdc.dwFlags = TDF_ALLOW_DIALOG_CANCELLATION | TDF_SIZE_TO_CONTENT;
	// Unlike message box localized "Error" string, task dialog uses filename if title is nullptr.
	// Maintainer will need to provide localization themself.
	tdc.pszWindowTitle = lpCaption != nullptr ? lpCaption : L"Error";
	tdc.pszContent = lpText;
	tdc.pfCallback = DarkTaskDlgMsgBoxCallback;
	tdc.lpCallbackData = static_cast<LONG_PTR>(uType);

	dmlib_win32api::InitMB_GetString();

	// buttons

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 26446) // Prefer to use gsl::at() instead of unchecked subscript operator (bounds.4).
#pragma warning(disable: 26482) // Only index into arrays using constant expressions.
#endif

	// NOLINTBEGIN(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)

	const UINT btnDefMask = uType & MB_DEFMASK;
	static constexpr UINT maxBtns = 3;
	auto getDefBtn = [&btnDefMask](std::array<int, maxBtns> btnIDs) noexcept
	{
		if (btnDefMask == MB_DEFBUTTON2)
		{
			return btnIDs[1];
		}
		if (btnDefMask == MB_DEFBUTTON3)
		{
			return btnIDs[2];
		}
		return btnIDs[0];
	};

	switch (uType & MB_TYPEMASK)
	{
		case MB_OK:
		{
			tdc.dwCommonButtons = TDCBF_OK_BUTTON;
			break;
		}

		case MB_OKCANCEL:
		{
			tdc.dwCommonButtons = TDCBF_OK_BUTTON | TDCBF_CANCEL_BUTTON;
			tdc.nDefaultButton = (btnDefMask == MB_DEFBUTTON2) ? IDCANCEL : IDOK;
			break;
		}

		case MB_ABORTRETRYIGNORE:
		{
			static const std::array<TASKDIALOG_BUTTON, maxBtns> buttons{ {
				{ IDABORT, dmlib_win32api::MB_GetString(IDABORT) },
				{ IDRETRY, dmlib_win32api::MB_GetString(IDRETRY) },
				{ IDIGNORE, dmlib_win32api::MB_GetString(IDIGNORE) }
			} };

			tdc.cButtons = static_cast<UINT>(buttons.size());
			tdc.pButtons = buttons.data();
			tdc.nDefaultButton = getDefBtn({ { buttons[0].nButtonID, buttons[1].nButtonID, buttons[2].nButtonID } });

			break;
		}

		case MB_YESNOCANCEL:
		{
			tdc.dwCommonButtons = TDCBF_YES_BUTTON | TDCBF_NO_BUTTON | TDCBF_CANCEL_BUTTON;
			tdc.nDefaultButton = getDefBtn({ { IDYES, IDNO, IDCANCEL } });
			break;
		}

		case MB_YESNO:
		{
			tdc.dwCommonButtons = TDCBF_YES_BUTTON | TDCBF_NO_BUTTON;
			tdc.nDefaultButton = (btnDefMask == MB_DEFBUTTON2) ? IDNO : IDYES;
			break;
		}

		case MB_RETRYCANCEL:
		{
			tdc.dwCommonButtons = TDCBF_RETRY_BUTTON | TDCBF_CANCEL_BUTTON;
			tdc.nDefaultButton = (btnDefMask == MB_DEFBUTTON2) ? IDCANCEL : IDRETRY;
			break;
		}

		case MB_CANCELTRYCONTINUE:
		{
			static const std::array<TASKDIALOG_BUTTON, maxBtns> buttons{ {
				{ IDCANCEL, dmlib_win32api::MB_GetString(IDCANCEL) },
				{ IDTRYAGAIN, dmlib_win32api::MB_GetString(IDTRYAGAIN) },
				{ IDCONTINUE, dmlib_win32api::MB_GetString(IDCONTINUE) }
			} };

			tdc.cButtons = static_cast<UINT>(buttons.size());
			tdc.pButtons = buttons.data();
			tdc.nDefaultButton = getDefBtn({ { buttons[0].nButtonID, buttons[1].nButtonID, buttons[2].nButtonID } });

			break;
		}

		default:
		{
			tdc.dwCommonButtons = TDCBF_OK_BUTTON;
			break;
		}
	}

	// NOLINTEND(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)

#ifdef _MSC_VER
#pragma warning(pop)
#endif

	// icons

	switch (uType & MB_ICONMASK)
	{
		case MB_ICONERROR:
		{
			tdc.pszMainIcon = TD_ERROR_ICON;
			break;
		}

		case MB_ICONQUESTION:
		{
			tdc.pszMainIcon = IDI_QUESTION;
			break;
		}

		case MB_ICONWARNING:
		{
			tdc.pszMainIcon = TD_WARNING_ICON;
			break;
		}

		case MB_ICONINFORMATION:
		{
			tdc.pszMainIcon = TD_INFORMATION_ICON;
			break;
		}

		default:
			break;
	}

	// other

	if ((uType & MB_RTLREADING) == MB_RTLREADING)
	{
		tdc.dwFlags |= TDF_RTL_LAYOUT;
	}

	return tdc;
}

/**
 * @brief Displays a message box as task dialog with themed styling.
 *
 * Shows a custom task dialog instead of classic message box if @ref dmlib::isExperimentalActive is true.
 * Otherwise, it falls back to the standard Windows message box function.
 * The message box can present various buttons and icons based on the
 * specified parameters.
 *
 * @param[in]   hWnd        Handle to the owner window of the message box.
 *                          It can be NULL if the message box has no owner.
 * @param[in]   lpText      Text to be displayed in the message box.
 * @param[in]   lpCaption   Text to be displayed in the title bar of the message box.
 * @param[in]   uType       Specifies the contents and behavior of the message box.
 *
 * @return int The returned value indicates which button was pressed by the user.
 *             Or 0 zero if the function has failed.
 *
 * @see DarkTaskDlgMsgBoxCallback()
 * @see msgBoxParamToTaskDlgConfig()
 */
int dmlib::darkMessageBoxW(
	HWND hWnd,
	LPCWSTR lpText,
	LPCWSTR lpCaption,
	UINT uType
)
{
	if (!dmlib::isExperimentalActive())
	{
		return ::MessageBoxW(hWnd, lpText, lpCaption, uType);
	}

	const TASKDIALOGCONFIG tdc = msgBoxParamToTaskDlgConfig(hWnd, lpText, lpCaption, uType);

	int btnPressed = 0;
	if (dmlib::darkTaskDialogIndirect(&tdc, &btnPressed, nullptr, nullptr) != S_OK)
	{
		return ::MessageBoxW(hWnd, lpText, lpCaption, uType);
	}
	return btnPressed;
}

#endif // !defined(_DARKMODELIB_NOT_USED)
