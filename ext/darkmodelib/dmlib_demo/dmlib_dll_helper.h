// SPDX-License-Identifier: MPL-2.0

/*
 * Copyright (c) 2025 ozone10
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

// This file is part of darkmodelib demo.


#pragma once

#include <windows.h>

#if defined(_MSC_VER)
#pragma comment(lib, "Comctl32.lib")
#endif

typedef struct _TASKDIALOGCONFIG TASKDIALOGCONFIG; // forward declaration, from <commctrl.h>

namespace dmlib_module
{
	template <typename P>
	inline auto LoadFn(HMODULE handle, P& pointer, const char* name) noexcept -> bool
	{
		if (auto proc = ::GetProcAddress(handle, name); proc != nullptr)
		{
			pointer = reinterpret_cast<P>(reinterpret_cast<INT_PTR>(proc));
			return true;
		}
		return false;
	}

	template <typename P>
	inline auto LoadFn(HMODULE handle, P& pointer, WORD index) noexcept -> bool
	{
		return  dmlib_module::LoadFn(handle, pointer, MAKEINTRESOURCEA(index));
	};

	template <typename P, typename D>
	inline auto LoadFn(HMODULE handle, P& pointer, const char* name, D& dummy) noexcept -> bool
	{
		const bool retVal = dmlib_module::LoadFn(handle, pointer, name);
		if (!retVal)
		{
			pointer = static_cast<P>(dummy);
		}
		return retVal;
	}

	inline bool getModuleFullPath(const wchar_t* dllName, wchar_t* outPath)
	{
		if (dllName == nullptr || outPath == nullptr)
		{
			return false;
		}

		wchar_t fullPath[MAX_PATH]{};
		if (::GetModuleFileNameW(nullptr, fullPath, MAX_PATH) == 0)
		{
			return false;
		}

		wchar_t* lastSlash = ::wcsrchr(fullPath, L'\\');
		if (lastSlash == nullptr)
		{
			return false;
		}

		*(lastSlash + 1) = L'\0'; // keep slash

		if (::wcslen(fullPath) + ::wcslen(dllName) + 1 >= MAX_PATH)
		{
			return false;
		}

		::wcscpy_s(outPath, MAX_PATH, fullPath);
		::wcscat_s(outPath, MAX_PATH, dllName);

		return true;
	}
}

namespace DarkMode
{
	struct Colors
	{
		COLORREF background = 0;
		COLORREF ctrlBackground = 0;
		COLORREF hotBackground = 0;
		COLORREF dlgBackground = 0;
		COLORREF errorBackground = 0;
		COLORREF text = 0;
		COLORREF darkerText = 0;
		COLORREF disabledText = 0;
		COLORREF linkText = 0;
		COLORREF edge = 0;
		COLORREF hotEdge = 0;
		COLORREF disabledEdge = 0;
	};

	struct ColorsView
	{
		COLORREF background = 0;
		COLORREF text = 0;
		COLORREF gridlines = 0;
		COLORREF headerBackground = 0;
		COLORREF headerHotBackground = 0;
		COLORREF headerText = 0;
		COLORREF headerEdge = 0;
	};

	enum class ToolTipsType : unsigned char
	{
		tooltip,   ///< Standard tooltip control.
		toolbar,   ///< Tooltips associated with toolbar buttons.
		listview,  ///< Tooltips associated with list views.
		treeview,  ///< Tooltips associated with tree views.
		tabbar,    ///< Tooltips associated with tab controls.
		trackbar,  ///< Tooltips associated with trackbar (slider) controls.
		rebar      ///< Tooltips associated with rebar controls.
	};

	enum class ColorTone : unsigned char
	{
		black   = 0,  ///< Black
		red     = 1,  ///< Red
		green   = 2,  ///< Green
		blue    = 3,  ///< Blue
		purple  = 4,  ///< Purple
		cyan    = 5,  ///< Cyan
		olive   = 6,  ///< Olive
		max     = 7   ///< Don't use, for internal checks
	};

	enum class TreeViewStyle : unsigned char
	{
		classic,  ///< Non-themed legacy appearance.
		light,    ///< Light mode.
		dark      ///< Dark mode.
	};

	enum class LibInfo : unsigned char
	{
		featureCheck,     ///< Returns maxValue to verify enum coverage.
		verMajor,         ///< Major version number of the library.
		verMinor,         ///< Minor version number of the library.
		verRevision,      ///< Revision/patch number of the library.
		iniConfigUsed,    ///< True if `.ini` file configuration is supported.
		allowOldOS,       ///< '1' if older Windows 10 versions are allowed, '2' if all older Windows are allowed.
		useDlgProcCtl,    ///< True if WM_CTLCOLORxxx can be handled directly in dialog procedure.
		preferTheme,      ///< True if theme is supported and can be used over subclass, e.g. combo box on Windows 10+.
		useSBFix,         ///< '1' if scroll bar fix is applied to all scroll bars, '2' if scroll bar fix can be limited to specific window.
		maxValue          ///< Sentinel value for internal validation (not intended for use).
	};

	enum class DarkModeType : unsigned char
	{
		light = 0,  ///< Light mode appearance.
		dark = 1,   ///< Dark mode appearance.
		classic = 3 ///< Classic (non-themed or system) appearance.
	};

	[[nodiscard]] inline int DummyGetLibInfo(int) { return -1; }

	inline void DummyInitDarkModeConfig(UINT) {}
	inline void DummySetRoundCornerConfig(UINT) {}
	inline void DummySetBorderColorConfig(COLORREF) {}
	inline void DummySetMicaConfig(UINT) {}
	inline void DummySetMicaExtendedConfig(bool) {}
	inline void DummySetColorizeTitleBarConfig(bool) {}
	inline void DummySetDarkModeConfigEx(UINT) {}
	inline void DummySetDarkModeConfig() {}
	inline void DummyInitDarkModeEx(const wchar_t*) {}
	inline void DummyInitDarkMode() {}
	[[nodiscard]] inline bool DummyDoesConfigFileExist() { return false; }

	[[nodiscard]] inline bool DummyIsEnabled() { return false; }
	[[nodiscard]] inline bool DummyIsExperimentalActive() { return false; }
	[[nodiscard]] inline bool DummyIsExperimentalSupported() { return false; }
	[[nodiscard]] inline bool DummyIsWindowsModeEnabled() { return false; }
	[[nodiscard]] inline bool DummyIsAtLeastWindows10() { return false; }
	[[nodiscard]] inline bool DummyIsAtLeastWindows11() { return false; }

	[[nodiscard]] inline DWORD DummyGetWindowsBuildNumber() { return 0; }

	inline bool DummyHandleSettingChange(LPARAM) { return false; }
	[[nodiscard]] inline bool DummyIsDarkModeReg() { return false; }

	inline void DummySetSysColor(int, COLORREF) {}
	inline void DummyEnableDarkScrollBarForWindowAndChildren(HWND) {}

	inline void DummySetColorTone(int) {}
	[[nodiscard]] inline int DummyGetColorTone() { return -1; }

	inline COLORREF DummySetBackgroundColor(COLORREF) { return CLR_INVALID; }
	inline COLORREF DummySetCtrlBackgroundColor(COLORREF) { return CLR_INVALID; }
	inline COLORREF DummySetHotBackgroundColor(COLORREF) { return CLR_INVALID; }
	inline COLORREF DummySetDlgBackgroundColor(COLORREF) { return CLR_INVALID; }
	inline COLORREF DummySetErrorBackgroundColor(COLORREF) { return CLR_INVALID; }
	inline COLORREF DummySetTextColor(COLORREF) { return CLR_INVALID; }
	inline COLORREF DummySetDarkerTextColor(COLORREF) { return CLR_INVALID; }
	inline COLORREF DummySetDisabledTextColor(COLORREF) { return CLR_INVALID; }
	inline COLORREF DummySetLinkTextColor(COLORREF) { return CLR_INVALID; }
	inline COLORREF DummySetEdgeColor(COLORREF) { return CLR_INVALID; }
	inline COLORREF DummySetHotEdgeColor(COLORREF) { return CLR_INVALID; }
	inline COLORREF DummySetDisabledEdgeColor(COLORREF) { return CLR_INVALID; }

	inline void DummySetThemeColors(Colors) {}
	inline void DummyUpdateThemeBrushesAndPens() {}

	[[nodiscard]] inline COLORREF DummyGetBackgroundColor() { return CLR_INVALID; }
	[[nodiscard]] inline COLORREF DummyGetCtrlBackgroundColor() { return CLR_INVALID; }
	[[nodiscard]] inline COLORREF DummyGetHotBackgroundColor() { return CLR_INVALID; }
	[[nodiscard]] inline COLORREF DummyGetDlgBackgroundColor() { return CLR_INVALID; }
	[[nodiscard]] inline COLORREF DummyGetErrorBackgroundColor() { return CLR_INVALID; }

	[[nodiscard]] inline COLORREF DummyGetTextColor() { return CLR_INVALID; }
	[[nodiscard]] inline COLORREF DummyGetDarkerTextColor() { return CLR_INVALID; }
	[[nodiscard]] inline COLORREF DummyGetDisabledTextColor() { return CLR_INVALID; }
	[[nodiscard]] inline COLORREF DummyGetLinkTextColor() { return CLR_INVALID; }

	[[nodiscard]] inline COLORREF DummyGetEdgeColor() { return CLR_INVALID; }
	[[nodiscard]] inline COLORREF DummyGetHotEdgeColor() { return CLR_INVALID; }
	[[nodiscard]] inline COLORREF DummyGetDisabledEdgeColor() { return CLR_INVALID; }

	[[nodiscard]] inline HBRUSH DummyGetBackgroundBrush() { return nullptr; }
	[[nodiscard]] inline HBRUSH DummyGetDlgBackgroundBrush() { return nullptr; }
	[[nodiscard]] inline HBRUSH DummyGetCtrlBackgroundBrush() { return nullptr; }
	[[nodiscard]] inline HBRUSH DummyGetHotBackgroundBrush() { return nullptr; }
	[[nodiscard]] inline HBRUSH DummyGetErrorBackgroundBrush() { return nullptr; }

	[[nodiscard]] inline HBRUSH DummyGetEdgeBrush() { return nullptr; }
	[[nodiscard]] inline HBRUSH DummyGetHotEdgeBrush() { return nullptr; }
	[[nodiscard]] inline HBRUSH DummyGetDisabledEdgeBrush() { return nullptr; }

	[[nodiscard]] inline HPEN DummyGetDarkerTextPen() { return nullptr; }
	[[nodiscard]] inline HPEN DummyGetEdgePen() { return nullptr; }
	[[nodiscard]] inline HPEN DummyGetHotEdgePen() { return nullptr; }
	[[nodiscard]] inline HPEN DummyGetDisabledEdgePen() { return nullptr; }

	inline COLORREF DummySetViewBackgroundColor(COLORREF) { return CLR_INVALID; }
	inline COLORREF DummySetViewTextColor(COLORREF) { return CLR_INVALID; }
	inline COLORREF DummySetViewGridlinesColor(COLORREF) { return CLR_INVALID; }
	inline COLORREF DummySetHeaderBackgroundColor(COLORREF) { return CLR_INVALID; }
	inline COLORREF DummySetHeaderHotBackgroundColor(COLORREF) { return CLR_INVALID; }
	inline COLORREF DummySetHeaderTextColor(COLORREF) { return CLR_INVALID; }
	inline COLORREF DummySetHeaderEdgeColor(COLORREF) { return CLR_INVALID; }

	inline void DummySetViewColors(ColorsView) {}
	inline void DummyUpdateViewBrushesAndPens() {}

	[[nodiscard]] inline COLORREF DummyGetViewBackgroundColor() { return CLR_INVALID; }
	[[nodiscard]] inline COLORREF DummyGetViewTextColor() { return CLR_INVALID; }
	[[nodiscard]] inline COLORREF DummyGetViewGridlinesColor() { return CLR_INVALID; }

	[[nodiscard]] inline COLORREF DummyGetHeaderBackgroundColor() { return CLR_INVALID; }
	[[nodiscard]] inline COLORREF DummyGetHeaderHotBackgroundColor() { return CLR_INVALID; }
	[[nodiscard]] inline COLORREF DummyGetHeaderTextColor() { return CLR_INVALID; }
	[[nodiscard]] inline COLORREF DummyGetHeaderEdgeColor() { return CLR_INVALID; }

	[[nodiscard]] inline HBRUSH DummyGetViewBackgroundBrush() { return nullptr; }
	[[nodiscard]] inline HBRUSH DummyGetViewGridlinesBrush() { return nullptr; }

	[[nodiscard]] inline HBRUSH DummyGetHeaderBackgroundBrush() { return nullptr; }
	[[nodiscard]] inline HBRUSH DummyGetHeaderHotBackgroundBrush() { return nullptr; }

	[[nodiscard]] inline HPEN DummyGetHeaderEdgePen() { return nullptr; }

	inline void DummySetDefaultColors(bool) {}

	inline void DummySetCheckboxOrRadioBtnCtrlSubclass(HWND) {}
	inline void DummyRemoveCheckboxOrRadioBtnCtrlSubclass(HWND) {}

	inline void DummySetGroupboxCtrlSubclass(HWND) {}
	inline void DummyRemoveGroupboxCtrlSubclass(HWND) {}

	inline void DummySetUpDownCtrlSubclass(HWND) {}
	inline void DummyRemoveUpDownCtrlSubclass(HWND) {}

	inline void DummySetTabCtrlUpDownSubclass(HWND) {}
	inline void DummyRemoveTabCtrlUpDownSubclass(HWND) {}

	inline void DummySetTabCtrlSubclass(HWND) {}
	inline void DummyRemoveTabCtrlSubclass(HWND) {}

	inline void DummySetCustomBorderForListBoxOrEditCtrlSubclass(HWND) {}
	inline void DummyRemoveCustomBorderForListBoxOrEditCtrlSubclass(HWND) {}

	inline void DummySetComboBoxCtrlSubclass(HWND) {}
	inline void DummyRemoveComboBoxCtrlSubclass(HWND) {}

	inline void DummySetComboBoxExCtrlSubclass(HWND) {}
	inline void DummyRemoveComboBoxExCtrlSubclass(HWND) {}

	inline void DummySetListViewCtrlSubclass(HWND) {}
	inline void DummyRemoveListViewCtrlSubclass(HWND) {}

	inline void DummySetHeaderCtrlSubclass(HWND) {}
	inline void DummyRemoveHeaderCtrlSubclass(HWND) {}

	inline void DummySetStatusBarCtrlSubclass(HWND) {}
	inline void DummyRemoveStatusBarCtrlSubclass(HWND) {}

	inline void DummySetProgressBarCtrlSubclass(HWND) {}
	inline void DummyRemoveProgressBarCtrlSubclass(HWND) {}

	inline void DummySetStaticTextCtrlSubclass(HWND) {}
	inline void DummyRemoveStaticTextCtrlSubclass(HWND) {}

	inline void DummySetIPAddressCtrlSubclass(HWND) {}
	inline void DummyRemoveIPAddressCtrlSubclass(HWND) {}

	inline void DummySetHotKeyCtrlSubclass(HWND) {}
	inline void DummyRemoveHotKeyCtrlSubclass(HWND) {}

	inline void DummySetChildCtrlsSubclassAndThemeEx(HWND, bool, bool) {}
	inline void DummySetChildCtrlsSubclassAndTheme(HWND) {}
	inline void DummySetChildCtrlsTheme(HWND) {}

	inline void DummySetWindowEraseBgSubclass(HWND) {}
	inline void DummyRemoveWindowEraseBgSubclass(HWND) {}

	inline void DummySetWindowCtlColorSubclass(HWND) {}
	inline void DummyRemoveWindowCtlColorSubclass(HWND) {}

	inline void DummySetWindowNotifyCustomDrawSubclass(HWND) {}
	inline void DummyRemoveWindowNotifyCustomDrawSubclass(HWND) {}

	inline void DummySetWindowMenuBarSubclass(HWND) {}
	inline void DummyRemoveWindowMenuBarSubclass(HWND) {}

	inline void DummySetWindowSettingChangeSubclass(HWND) {}
	inline void DummyRemoveWindowSettingChangeSubclass(HWND) {}

	inline void DummyEnableSysLinkCtrlCtlColor(HWND) {}

	inline void DummySetDarkTitleBarEx(HWND, bool) {}
	inline void DummySetDarkTitleBar(HWND) {}

	inline void DummySetDarkThemeExperimentalEx(HWND, const wchar_t*) {}
	inline void DummySetDarkThemeExperimental(HWND) {}

	inline void DummySetDarkExplorerTheme(HWND) {}
	inline void DummySetDarkScrollBar(HWND) {}
	inline void DummySetDarkTooltips(HWND, int) {}
	inline void DummySetDarkThemeTheme(HWND) {}

	inline void DummySetDarkLineAbovePanelToolbar(HWND) {}

	inline void DummySetDarkListView(HWND) {}
	inline void DummySetDarkListViewCheckboxes(HWND) {}

	inline void DummySetDarkRichEdit(HWND) {}

	inline void DummySetDarkWndSafeEx(HWND, bool) {}
	inline void DummySetDarkWndSafe(HWND) {}
	inline void DummySetDarkWndNotifySafeEx(HWND, bool, bool) {}
	inline void DummySetDarkWndNotifySafe(HWND) {}

	inline void DummyEnableThemeDialogTexture(HWND, bool) {}

	inline void DummyDisableVisualStyle(HWND, bool) {}
	[[nodiscard]] inline double DummyCalculatePerceivedLightness(COLORREF) { return 0.0; }
	[[nodiscard]] inline int DummyGetTreeViewStyle() { return -1; }
	inline void DummyCalculateTreeViewStyle() {}
	inline void DummySetTreeViewWindowThemeEx(HWND, bool) {}
	inline void DummySetTreeViewWindowTheme(HWND) {}
	[[nodiscard]] inline int DummyGetPrevTreeViewStyle() { return -1; }
	inline void DummySetPrevTreeViewStyle() {}

	[[nodiscard]] inline bool DummyIsThemeDark() { return false; }
	[[nodiscard]] inline bool DummyIsColorDark(COLORREF) { return false; }

	inline void DummyRedrawWindowFrame(HWND) {}

	inline void DummySetWindowStyle(HWND, bool, LONG_PTR) {}
	inline void DummySetWindowExStyle(HWND, bool, LONG_PTR) {}
	inline void DummyReplaceExEdgeWithBorder(HWND, bool, LONG_PTR) {}
	inline void DummyReplaceClientEdgeWithBorderSafe(HWND) {}

	inline void DummySetProgressBarClassicTheme(HWND) {}

	[[nodiscard]] inline LRESULT DummyOnCtlColor(HDC) { return FALSE; }
	[[nodiscard]] inline LRESULT DummyOnCtlColorCtrl(HDC) { return FALSE; }
	[[nodiscard]] inline LRESULT DummyOnCtlColorDlg(HDC) { return FALSE; }
	[[nodiscard]] inline LRESULT DummyOnCtlColorError(HDC) { return FALSE; }
	[[nodiscard]] inline LRESULT DummyOnCtlColorDlgStaticText(HDC, bool) { return FALSE; }
	[[nodiscard]] inline LRESULT DummyOnCtlColorDlgLinkText(HDC, bool) { return FALSE; }
	[[nodiscard]] inline LRESULT DummyOnCtlColorListbox(WPARAM, LPARAM) { return FALSE; }

	inline UINT_PTR CALLBACK DummyHookDlgProc(HWND, UINT, WPARAM, LPARAM) { return 0; }
	inline void DummySetDarkTaskDlg(HWND) {}
	inline HRESULT CALLBACK DummyDarkTaskDlgCallback(HWND, UINT, WPARAM, LPARAM, LONG_PTR) { return S_OK; }
	inline HRESULT DummyDarkTaskDialogIndirect(const TASKDIALOGCONFIG*, int*, int*, BOOL*) { return S_OK; }

	using fnGetLibInfo = auto (*)(int libInfoType) -> int;
	inline fnGetLibInfo getLibInfo = nullptr;

	using fnInitDarkModeConfig = void (*)(UINT dmType);
	inline fnInitDarkModeConfig initDarkModeConfig = nullptr;

	using fnSetRoundCornerConfig = void (*)(UINT roundCornerStyle);
	inline fnSetRoundCornerConfig setRoundCornerConfig = nullptr;

	using fnSetBorderColorConfig = void (*)(COLORREF clr);
	inline fnSetBorderColorConfig setBorderColorConfig = nullptr;

	using fnSetMicaConfig = void (*)(UINT mica);
	inline fnSetMicaConfig setMicaConfig = nullptr;

	using fnSetMicaExtendedConfig = void (*)(bool extendMica);
	inline fnSetMicaExtendedConfig setMicaExtendedConfig = nullptr;

	using fnSetColorizeTitleBarConfig = void (*)(bool colorize);
	inline fnSetColorizeTitleBarConfig setColorizeTitleBarConfig = nullptr;

	using fnSetDarkModeConfigEx = void (*)(UINT dmType);
	inline fnSetDarkModeConfigEx setDarkModeConfigEx = nullptr;

	using fnSetDarkModeConfig = void (*)();
	inline fnSetDarkModeConfig setDarkModeConfig = nullptr;

	using fnInitDarkModeEx = void (*)(const wchar_t* iniName);
	inline fnInitDarkModeEx initDarkModeEx = nullptr;

	using fnInitDarkMode = void (*)();
	inline fnInitDarkMode initDarkMode = nullptr;

	using fnDoesConfigFileExist = auto (*)() -> bool;
	inline fnDoesConfigFileExist doesConfigFileExist = nullptr;

	using fnIsEnabled = auto (*)() -> bool;
	inline fnIsEnabled isEnabled = nullptr;

	using fnIsExperimentalActive = auto (*)() -> bool;
	inline fnIsExperimentalActive isExperimentalActive = nullptr;

	using fnIsExperimentalSupported = auto (*)() -> bool;
	inline fnIsExperimentalSupported isExperimentalSupported = nullptr;

	using fnIsWindowsModeEnabled = auto (*)() -> bool;
	inline fnIsWindowsModeEnabled isWindowsModeEnabled = nullptr;

	using fnIsAtLeastWindows10 = auto (*)() -> bool;
	inline fnIsAtLeastWindows10 isAtLeastWindows10 = nullptr;

	using fnIsAtLeastWindows11 = auto (*)() -> bool;
	inline fnIsAtLeastWindows11 isAtLeastWindows11 = nullptr;

	using fnGetWindowsBuildNumber = auto (*)() -> DWORD;
	inline fnGetWindowsBuildNumber getWindowsBuildNumber = nullptr;

	using fnHandleSettingChange = auto (*)(LPARAM lParam) -> bool;
	inline fnHandleSettingChange handleSettingChange = nullptr;

	using fnIsDarkModeReg = auto (*)() -> bool;
	inline fnIsDarkModeReg isDarkModeReg = nullptr;

	using fnSetSysColor = auto (*)(int nIndex, COLORREF color) -> void;
	inline fnSetSysColor setSysColor = nullptr;

	using fnEnableDarkScrollBarForWindowAndChildren = auto (*)(HWND hWnd) -> void;
	inline fnEnableDarkScrollBarForWindowAndChildren enableDarkScrollBarForWindowAndChildren = nullptr;

	using fnSetColorTone = void (*)(int colorTone);
	inline fnSetColorTone setColorTone = nullptr;

	using fnGetColorTone = auto (*)() -> int;
	inline fnGetColorTone getColorTone = nullptr;

	using fnSetBackgroundColor = auto (*)(COLORREF clrNew) -> COLORREF;
	inline fnSetBackgroundColor setBackgroundColor = nullptr;

	using fnSetCtrlBackgroundColor = auto (*)(COLORREF clrNew) -> COLORREF;
	inline fnSetCtrlBackgroundColor setCtrlBackgroundColor = nullptr;

	using fnSetHotBackgroundColor = auto (*)(COLORREF clrNew) -> COLORREF;
	inline fnSetHotBackgroundColor setHotBackgroundColor = nullptr;

	using fnSetDlgBackgroundColor = auto (*)(COLORREF clrNew) -> COLORREF;
	inline fnSetDlgBackgroundColor setDlgBackgroundColor = nullptr;

	using fnSetErrorBackgroundColor = auto (*)(COLORREF clrNew) -> COLORREF;
	inline fnSetErrorBackgroundColor setErrorBackgroundColor = nullptr;

	using fnSetTextColor = auto (*)(COLORREF clrNew) -> COLORREF;
	inline fnSetTextColor setTextColor = nullptr;

	using fnSetDarkerTextColor = auto (*)(COLORREF clrNew) -> COLORREF;
	inline fnSetDarkerTextColor setDarkerTextColor = nullptr;

	using fnSetDisabledTextColor = auto (*)(COLORREF clrNew) -> COLORREF;
	inline fnSetDisabledTextColor setDisabledTextColor = nullptr;

	using fnSetLinkTextColor = auto (*)(COLORREF clrNew) -> COLORREF;
	inline fnSetLinkTextColor setLinkTextColor = nullptr;

	using fnSetEdgeColor = auto (*)(COLORREF clrNew) -> COLORREF;
	inline fnSetEdgeColor setEdgeColor = nullptr;

	using fnSetHotEdgeColor = auto (*)(COLORREF clrNew) -> COLORREF;
	inline fnSetHotEdgeColor setHotEdgeColor = nullptr;

	using fnSetDisabledEdgeColor = auto (*)(COLORREF clrNew) -> COLORREF;
	inline fnSetDisabledEdgeColor setDisabledEdgeColor = nullptr;

	using fnSetThemeColors = void (*)(Colors colors);
	inline fnSetThemeColors setThemeColors = nullptr;

	using fnUpdateThemeBrushesAndPens = void (*)();
	inline fnUpdateThemeBrushesAndPens updateThemeBrushesAndPens = nullptr;

	using fnGetBackgroundColor = auto (*)() -> COLORREF;
	inline fnGetBackgroundColor getBackgroundColor = nullptr;

	using fnGetCtrlBackgroundColor = auto (*)() -> COLORREF;
	inline fnGetCtrlBackgroundColor getCtrlBackgroundColor = nullptr;

	using fnGetHotBackgroundColor = auto (*)() -> COLORREF;
	inline fnGetHotBackgroundColor getHotBackgroundColor = nullptr;

	using fnGetDlgBackgroundColor = auto (*)() -> COLORREF;
	inline fnGetDlgBackgroundColor getDlgBackgroundColor = nullptr;

	using fnGetErrorBackgroundColor = auto (*)() -> COLORREF;
	inline fnGetErrorBackgroundColor getErrorBackgroundColor = nullptr;

	using fnGetTextColor = auto (*)() -> COLORREF;
	inline fnGetTextColor getTextColor = nullptr;

	using fnGetDarkerTextColor = auto (*)() -> COLORREF;
	inline fnGetDarkerTextColor getDarkerTextColor = nullptr;

	using fnGetDisabledTextColor = auto (*)() -> COLORREF;
	inline fnGetDisabledTextColor getDisabledTextColor = nullptr;

	using fnGetLinkTextColor = auto (*)() -> COLORREF;
	inline fnGetLinkTextColor getLinkTextColor = nullptr;

	using fnGetEdgeColor = auto (*)() -> COLORREF;
	inline fnGetEdgeColor getEdgeColor = nullptr;

	using fnGetHotEdgeColor = auto (*)() -> COLORREF;
	inline fnGetHotEdgeColor getHotEdgeColor = nullptr;

	using fnGetDisabledEdgeColor = auto (*)() -> COLORREF;
	inline fnGetDisabledEdgeColor getDisabledEdgeColor = nullptr;

	using fnGetBackgroundBrush = auto (*)() -> HBRUSH;
	inline fnGetBackgroundBrush getBackgroundBrush = nullptr;

	using fnGetDlgBackgroundBrush = auto (*)() -> HBRUSH;
	inline fnGetDlgBackgroundBrush getDlgBackgroundBrush = nullptr;

	using fnGetCtrlBackgroundBrush = auto (*)() -> HBRUSH;
	inline fnGetCtrlBackgroundBrush getCtrlBackgroundBrush = nullptr;

	using fnGetHotBackgroundBrush = auto (*)() -> HBRUSH;
	inline fnGetHotBackgroundBrush getHotBackgroundBrush = nullptr;

	using fnGetErrorBackgroundBrush = auto (*)() -> HBRUSH;
	inline fnGetErrorBackgroundBrush getErrorBackgroundBrush = nullptr;

	using fnGetEdgeBrush = auto (*)() -> HBRUSH;
	inline fnGetEdgeBrush getEdgeBrush = nullptr;

	using fnGetHotEdgeBrush = auto (*)() -> HBRUSH;
	inline fnGetHotEdgeBrush getHotEdgeBrush = nullptr;

	using fnGetDisabledEdgeBrush = auto (*)() -> HBRUSH;
	inline fnGetDisabledEdgeBrush getDisabledEdgeBrush = nullptr;

	using fnGetDarkerTextPen = auto (*)() -> HPEN;
	inline fnGetDarkerTextPen getDarkerTextPen = nullptr;

	using fnGetEdgePen = auto (*)() -> HPEN;
	inline fnGetEdgePen getEdgePen = nullptr;

	using fnGetHotEdgePen = auto (*)() -> HPEN;
	inline fnGetHotEdgePen getHotEdgePen = nullptr;

	using fnGetDisabledEdgePen = auto (*)() -> HPEN;
	inline fnGetDisabledEdgePen getDisabledEdgePen = nullptr;

	using fnSetViewBackgroundColor = auto (*)(COLORREF clrNew) -> COLORREF;
	inline fnSetViewBackgroundColor setViewBackgroundColor = nullptr;

	using fnSetViewTextColor = auto (*)(COLORREF clrNew) -> COLORREF;
	inline fnSetViewTextColor setViewTextColor = nullptr;

	using fnSetViewGridlinesColor = auto (*)(COLORREF clrNew) -> COLORREF;
	inline fnSetViewGridlinesColor setViewGridlinesColor = nullptr;

	using fnSetHeaderBackgroundColor = auto (*)(COLORREF clrNew) -> COLORREF;
	inline fnSetHeaderBackgroundColor setHeaderBackgroundColor = nullptr;

	using fnSetHeaderHotBackgroundColor = auto (*)(COLORREF clrNew) -> COLORREF;
	inline fnSetHeaderHotBackgroundColor setHeaderHotBackgroundColor = nullptr;

	using fnSetHeaderTextColor = auto (*)(COLORREF clrNew) -> COLORREF;
	inline fnSetHeaderTextColor setHeaderTextColor = nullptr;

	using fnSetHeaderEdgeColor = auto (*)(COLORREF clrNew) -> COLORREF;
	inline fnSetHeaderEdgeColor setHeaderEdgeColor = nullptr;

	using fnSetViewColors = void (*)(ColorsView colors);
	inline fnSetViewColors setViewColors = nullptr;

	using fnUpdateViewBrushesAndPens = void (*)();
	inline fnUpdateViewBrushesAndPens updateViewBrushesAndPens = nullptr;

	using fnGetViewBackgroundColor = auto (*)() -> COLORREF;
	inline fnGetViewBackgroundColor getViewBackgroundColor = nullptr;

	using fnGetViewTextColor = auto (*)() -> COLORREF;
	inline fnGetViewTextColor getViewTextColor = nullptr;

	using fnGetViewGridlinesColor = auto (*)() -> COLORREF;
	inline fnGetViewGridlinesColor getViewGridlinesColor = nullptr;

	using fnGetHeaderBackgroundColor = auto (*)() -> COLORREF;
	inline fnGetHeaderBackgroundColor getHeaderBackgroundColor = nullptr;

	using fnGetHeaderHotBackgroundColor = auto (*)() -> COLORREF;
	inline fnGetHeaderHotBackgroundColor getHeaderHotBackgroundColor = nullptr;

	using fnGetHeaderTextColor = auto (*)() -> COLORREF;
	inline fnGetHeaderTextColor getHeaderTextColor = nullptr;

	using fnGetHeaderEdgeColor = auto (*)() -> COLORREF;
	inline fnGetHeaderEdgeColor getHeaderEdgeColor = nullptr;

	using fnGetViewBackgroundBrush = auto (*)() -> HBRUSH;
	inline fnGetViewBackgroundBrush getViewBackgroundBrush = nullptr;

	using fnGetViewGridlinesBrush = auto (*)() -> HBRUSH;
	inline fnGetViewGridlinesBrush getViewGridlinesBrush = nullptr;

	using fnGetHeaderBackgroundBrush = auto (*)() -> HBRUSH;
	inline fnGetHeaderBackgroundBrush getHeaderBackgroundBrush = nullptr;

	using fnGetHeaderHotBackgroundBrush = auto (*)() -> HBRUSH;
	inline fnGetHeaderHotBackgroundBrush getHeaderHotBackgroundBrush = nullptr;

	using fnGetHeaderEdgePen = auto (*)() -> HPEN;
	inline fnGetHeaderEdgePen getHeaderEdgePen = nullptr;

	using fnSetDefaultColors = void (*)(bool updateBrushesAndOther);
	inline fnSetDefaultColors setDefaultColors = nullptr;

	using fnSetCheckboxOrRadioBtnCtrlSubclass = void (*)(HWND hWnd);
	inline fnSetCheckboxOrRadioBtnCtrlSubclass setCheckboxOrRadioBtnCtrlSubclass = nullptr;

	using fnRemoveCheckboxOrRadioBtnCtrlSubclass = void (*)(HWND hWnd);
	inline fnRemoveCheckboxOrRadioBtnCtrlSubclass removeCheckboxOrRadioBtnCtrlSubclass = nullptr;

	using fnSetGroupboxCtrlSubclass = void (*)(HWND hWnd);
	inline fnSetGroupboxCtrlSubclass setGroupboxCtrlSubclass = nullptr;

	using fnRemoveGroupboxCtrlSubclass = void (*)(HWND hWnd);
	inline fnRemoveGroupboxCtrlSubclass removeGroupboxCtrlSubclass = nullptr;

	using fnSetUpDownCtrlSubclass = void (*)(HWND hWnd);
	inline fnSetUpDownCtrlSubclass setUpDownCtrlSubclass = nullptr;

	using fnRemoveUpDownCtrlSubclass = void (*)(HWND hWnd);
	inline fnRemoveUpDownCtrlSubclass removeUpDownCtrlSubclass = nullptr;

	using fnSetTabCtrlUpDownSubclass = void (*)(HWND hWnd);
	inline fnSetTabCtrlUpDownSubclass setTabCtrlUpDownSubclass = nullptr;

	using fnRemoveTabCtrlUpDownSubclass = void (*)(HWND hWnd);
	inline fnRemoveTabCtrlUpDownSubclass removeTabCtrlUpDownSubclass = nullptr;

	using fnSetTabCtrlSubclass = void (*)(HWND hWnd);
	inline fnSetTabCtrlSubclass setTabCtrlSubclass = nullptr;

	using fnRemoveTabCtrlSubclass = void (*)(HWND hWnd);
	inline fnRemoveTabCtrlSubclass removeTabCtrlSubclass = nullptr;

	using fnSetCustomBorderForListBoxOrEditCtrlSubclass = void (*)(HWND hWnd);
	inline fnSetCustomBorderForListBoxOrEditCtrlSubclass setCustomBorderForListBoxOrEditCtrlSubclass = nullptr;

	using fnRemoveCustomBorderForListBoxOrEditCtrlSubclass = void (*)(HWND hWnd);
	inline fnRemoveCustomBorderForListBoxOrEditCtrlSubclass removeCustomBorderForListBoxOrEditCtrlSubclass = nullptr;

	using fnSetComboBoxCtrlSubclass = void (*)(HWND hWnd);
	inline fnSetComboBoxCtrlSubclass setComboBoxCtrlSubclass = nullptr;

	using fnRemoveComboBoxCtrlSubclass = void (*)(HWND hWnd);
	inline fnRemoveComboBoxCtrlSubclass removeComboBoxCtrlSubclass = nullptr;

	using fnSetComboBoxExCtrlSubclass = void (*)(HWND hWnd);
	inline fnSetComboBoxExCtrlSubclass setComboBoxExCtrlSubclass = nullptr;

	using fnRemoveComboBoxExCtrlSubclass = void (*)(HWND hWnd);
	inline fnRemoveComboBoxExCtrlSubclass removeComboBoxExCtrlSubclass = nullptr;

	using fnSetListViewCtrlSubclass = void (*)(HWND hWnd);
	inline fnSetListViewCtrlSubclass setListViewCtrlSubclass = nullptr;

	using fnRemoveListViewCtrlSubclass = void (*)(HWND hWnd);
	inline fnRemoveListViewCtrlSubclass removeListViewCtrlSubclass = nullptr;

	using fnSetHeaderCtrlSubclass = void (*)(HWND hWnd);
	inline fnSetHeaderCtrlSubclass setHeaderCtrlSubclass = nullptr;

	using fnRemoveHeaderCtrlSubclass = void (*)(HWND hWnd);
	inline fnRemoveHeaderCtrlSubclass removeHeaderCtrlSubclass = nullptr;

	using fnSetStatusBarCtrlSubclass = void (*)(HWND hWnd);
	inline fnSetStatusBarCtrlSubclass setStatusBarCtrlSubclass = nullptr;

	using fnRemoveStatusBarCtrlSubclass = void (*)(HWND hWnd);
	inline fnRemoveStatusBarCtrlSubclass removeStatusBarCtrlSubclass = nullptr;

	using fnSetProgressBarCtrlSubclass = void (*)(HWND hWnd);
	inline fnSetProgressBarCtrlSubclass setProgressBarCtrlSubclass = nullptr;

	using fnRemoveProgressBarCtrlSubclass = void (*)(HWND hWnd);
	inline fnRemoveProgressBarCtrlSubclass removeProgressBarCtrlSubclass = nullptr;

	using fnSetStaticTextCtrlSubclass = void (*)(HWND hWnd);
	inline fnSetStaticTextCtrlSubclass setStaticTextCtrlSubclass = nullptr;

	using fnSetIPAddressCtrlSubclass = void (*)(HWND hWnd);
	inline fnSetIPAddressCtrlSubclass setIPAddressCtrlSubclass = nullptr;

	using fnRemoveStaticTextCtrlSubclass = void (*)(HWND hWnd);
	inline fnRemoveStaticTextCtrlSubclass removeStaticTextCtrlSubclass = nullptr;

	using fnSetChildCtrlsSubclassAndThemeEx = void (*)(HWND hParent, bool subclass, bool theme);
	inline fnSetChildCtrlsSubclassAndThemeEx setChildCtrlsSubclassAndThemeEx = nullptr;

	using fnSetChildCtrlsSubclassAndTheme = void (*)(HWND hParent);
	inline fnSetChildCtrlsSubclassAndTheme setChildCtrlsSubclassAndTheme = nullptr;

	using fnSetChildCtrlsTheme = void (*)(HWND hParent);
	inline fnSetChildCtrlsTheme setChildCtrlsTheme = nullptr;

	using fnSetWindowEraseBgSubclass = void (*)(HWND hWnd);
	inline fnSetWindowEraseBgSubclass setWindowEraseBgSubclass = nullptr;

	using fnRemoveWindowEraseBgSubclass = void (*)(HWND hWnd);
	inline fnRemoveWindowEraseBgSubclass removeWindowEraseBgSubclass = nullptr;

	using fnSetWindowCtlColorSubclass = void (*)(HWND hWnd);
	inline fnSetWindowCtlColorSubclass setWindowCtlColorSubclass = nullptr;

	using fnRemoveWindowCtlColorSubclass = void (*)(HWND hWnd);
	inline fnRemoveWindowCtlColorSubclass removeWindowCtlColorSubclass = nullptr;

	using fnSetWindowNotifyCustomDrawSubclass = void (*)(HWND hWnd);
	inline fnSetWindowNotifyCustomDrawSubclass setWindowNotifyCustomDrawSubclass = nullptr;

	using fnRemoveWindowNotifyCustomDrawSubclass = void (*)(HWND hWnd);
	inline fnRemoveWindowNotifyCustomDrawSubclass removeWindowNotifyCustomDrawSubclass = nullptr;

	using fnSetWindowMenuBarSubclass = void (*)(HWND hWnd);
	inline fnSetWindowMenuBarSubclass setWindowMenuBarSubclass = nullptr;

	using fnRemoveWindowMenuBarSubclass = void (*)(HWND hWnd);
	inline fnRemoveWindowMenuBarSubclass removeWindowMenuBarSubclass = nullptr;

	using fnSetWindowSettingChangeSubclass = void (*)(HWND hWnd);
	inline fnSetWindowSettingChangeSubclass setWindowSettingChangeSubclass = nullptr;

	using fnRemoveWindowSettingChangeSubclass = void (*)(HWND hWnd);
	inline fnRemoveWindowSettingChangeSubclass removeWindowSettingChangeSubclass = nullptr;

	using fnEnableSysLinkCtrlCtlColor = void (*)(HWND hWnd);
	inline fnEnableSysLinkCtrlCtlColor enableSysLinkCtrlCtlColor = nullptr;

	using fnSetDarkTitleBarEx = void (*)(HWND hWnd, bool useWin11Features);
	inline fnSetDarkTitleBarEx setDarkTitleBarEx = nullptr;

	using fnSetDarkTitleBar = void (*)(HWND hWnd);
	inline fnSetDarkTitleBar setDarkTitleBar = nullptr;

	using fnSetDarkThemeExperimentalEx = void (*)(HWND hWnd, const wchar_t* themeClassName);
	inline fnSetDarkThemeExperimentalEx setDarkThemeExperimentalEx = nullptr;

	using fnSetDarkThemeExperimental = void (*)(HWND hWnd);
	inline fnSetDarkThemeExperimental setDarkThemeExperimental = nullptr;

	using fnSetDarkExplorerTheme = void (*)(HWND hWnd);
	inline fnSetDarkExplorerTheme setDarkExplorerTheme = nullptr;

	using fnSetDarkScrollBar = void (*)(HWND hWnd);
	inline fnSetDarkScrollBar setDarkScrollBar = nullptr;

	using fnSetDarkTooltips = void (*)(HWND hWnd, int tooltipType);
	inline fnSetDarkTooltips setDarkTooltips = nullptr;

	using fnSetDarkLineAbovePanelToolbar = void (*)(HWND hWnd);
	inline fnSetDarkLineAbovePanelToolbar setDarkLineAbovePanelToolbar = nullptr;

	using fnSetDarkListView = void (*)(HWND hWnd);
	inline fnSetDarkListView setDarkListView = nullptr;

	using fnSetDarkListViewCheckboxes = void (*)(HWND hWnd);
	inline fnSetDarkListViewCheckboxes setDarkListViewCheckboxes = nullptr;

	using fnSetDarkRichEdit = void (*)(HWND hWnd);
	inline fnSetDarkRichEdit setDarkRichEdit = nullptr;

	using fnSetDarkWndSafeEx = void (*)(HWND hWnd, bool useWin11Features);
	inline fnSetDarkWndSafeEx setDarkWndSafeEx = nullptr;

	using fnSetDarkWndSafe = void (*)(HWND hWnd);
	inline fnSetDarkWndSafe setDarkWndSafe = nullptr;

	using fnSetDarkWndNotifySafeEx = void (*)(HWND hWnd, bool setSettingChangeSubclass, bool useWin11Features);
	inline fnSetDarkWndNotifySafeEx setDarkWndNotifySafeEx = nullptr;

	using fnSetDarkWndNotifySafe = void (*)(HWND hWnd);
	inline fnSetDarkWndNotifySafe setDarkWndNotifySafe = nullptr;

	using fnEnableThemeDialogTexture = void (*)(HWND hWnd, bool theme);
	inline fnEnableThemeDialogTexture enableThemeDialogTexture = nullptr;

	using fnDisableVisualStyle = void (*)(HWND hWnd, bool doDisable);
	inline fnDisableVisualStyle disableVisualStyle = nullptr;

	using fnCalculatePerceivedLightness = auto (*)(COLORREF clr) -> double;
	inline fnCalculatePerceivedLightness calculatePerceivedLightness = nullptr;

	using fnGetTreeViewStyle = auto (*)() -> int;
	inline fnGetTreeViewStyle getTreeViewStyle = nullptr;

	using fnCalculateTreeViewStyle = void (*)();
	inline fnCalculateTreeViewStyle calculateTreeViewStyle = nullptr;

	using fnSetTreeViewWindowThemeEx = void (*)(HWND hWnd, bool force);
	inline fnSetTreeViewWindowThemeEx setTreeViewWindowThemeEx = nullptr;

	using fnSetTreeViewWindowTheme = void (*)(HWND hWnd);
	inline fnSetTreeViewWindowTheme setTreeViewWindowTheme = nullptr;

	using fnGetPrevTreeViewStyle = auto (*)() -> int;
	inline fnGetPrevTreeViewStyle getPrevTreeViewStyle = nullptr;

	using fnSetPrevTreeViewStyle = void (*)();
	inline fnSetPrevTreeViewStyle setPrevTreeViewStyle = nullptr;

	using fnIsThemeDark = auto (*)() -> bool;
	inline fnIsThemeDark isThemeDark = nullptr;

	using fnIsColorDark = auto (*)(COLORREF clr) -> bool;
	inline fnIsColorDark isColorDark = nullptr;

	using fnRedrawWindowFrame = void (*)(HWND hWnd);
	inline fnRedrawWindowFrame redrawWindowFrame = nullptr;

	using fnSetWindowStyle = void (*)(HWND hWnd, bool setStyle, LONG_PTR styleFlag);
	inline fnSetWindowStyle setWindowStyle = nullptr;

	using fnSetWindowExStyle = void (*)(HWND hWnd, bool setExStyle, LONG_PTR exStyleFlag);
	inline fnSetWindowExStyle setWindowExStyle = nullptr;

	using fnReplaceExEdgeWithBorder = void (*)(HWND hWnd, bool replace, LONG_PTR exStyleFlag);
	inline fnReplaceExEdgeWithBorder replaceExEdgeWithBorder = nullptr;

	using fnReplaceClientEdgeWithBorderSafe = void (*)(HWND hWnd);
	inline fnReplaceClientEdgeWithBorderSafe replaceClientEdgeWithBorderSafe = nullptr;

	using fnSetProgressBarClassicTheme = void (*)(HWND hWnd);
	inline fnSetProgressBarClassicTheme setProgressBarClassicTheme = nullptr;

	using fnOnCtlColor = void (*)(HDC hdc);
	inline fnOnCtlColor onCtlColor = nullptr;

	using fnOnCtlColorCtrl = void (*)(HDC hdc);
	inline fnOnCtlColorCtrl onCtlColorCtrl = nullptr;

	using fnOnCtlColorDlg = void (*)(HDC hdc);
	inline fnOnCtlColorDlg onCtlColorDlg = nullptr;

	using fnOnCtlColorError = void (*)(HDC hdc);
	inline fnOnCtlColorError onCtlColorError = nullptr;

	using fnOnCtlColorDlgStaticText = void (*)(HDC hdc, bool isTextEnabled);
	inline fnOnCtlColorDlgStaticText onCtlColorDlgStaticText = nullptr;

	using fnOnCtlColorDlgLinkText = void (*)(HDC hdc, bool isTextEnabled);
	inline fnOnCtlColorDlgLinkText onCtlColorDlgLinkText = nullptr;

	using fnOnCtlColorListbox = void (*)(WPARAM wParam, LPARAM lParam);
	inline fnOnCtlColorListbox onCtlColorListbox = nullptr;

	using fnHookDlgProc = auto (CALLBACK*)(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) -> UINT_PTR;
	inline fnHookDlgProc HookDlgProc = nullptr;

	using fnSetDarkTaskDlg = void (*)(HWND hWnd);
	inline fnSetDarkTaskDlg setDarkTaskDlg = nullptr;

	using fnDarkTaskDlgCallback = auto (CALLBACK*)(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, LONG_PTR lpRefData) -> HRESULT;
	inline fnDarkTaskDlgCallback DarkTaskDlgCallback = nullptr;

	using fnDarkTaskDialogIndirect = auto (*)(const TASKDIALOGCONFIG* pTaskConfig, int* pnButton, int* pnRadioButton, BOOL* pfVerificationFlagChecked) -> HRESULT;
	inline fnDarkTaskDialogIndirect darkTaskDialogIndirect = nullptr;

	bool loadDarkModeFunctionsFromDll(const wchar_t* dllName);
}
