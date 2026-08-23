// SPDX-License-Identifier: MPL-2.0

/*
 * Copyright (c) 2025-2026 ozone10
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

// This file is part of darkmodelib demo.


#pragma once

#include <windows.h>

#if (defined(_MSC_VER) && (_MSVC_LANG >= 202002L)) || (__cplusplus >= 202002L)
#include <bit>
#else
#include <cstring>
#endif

#include <cwchar>

#if defined(_MSC_VER)
#pragma comment(lib, "Comctl32.lib")
#endif

typedef struct _TASKDIALOGCONFIG TASKDIALOGCONFIG; // forward declaration, from <commctrl.h>
typedef struct tagCHOOSECOLORW* LPCHOOSECOLORW; // forward declaration, from <commdlg.h>
typedef struct tagCHOOSEFONTW* LPCHOOSEFONTW; // forward declaration, from <commdlg.h>

namespace dmlib_module
{
	template <typename P>
	inline auto LoadFn(HMODULE handle, P& pointer, const char* name) noexcept -> bool
	{
		if (auto proc = ::GetProcAddress(handle, name);
			proc != nullptr)
		{
#if (defined(_MSC_VER) && (_MSVC_LANG >= 202002L)) || (__cplusplus >= 202002L)
			pointer = std::bit_cast<P>(proc);
#else
			static_assert(sizeof(P) == sizeof(proc));
			std::memcpy(&pointer, &proc, sizeof(P));
#endif
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

		wchar_t* lastSlash = std::wcsrchr(fullPath, L'\\');
		if (lastSlash == nullptr)
		{
			return false;
		}

		*(lastSlash + 1) = L'\0'; // keep slash

		if (std::wcslen(fullPath) + std::wcslen(dllName) + 1 >= MAX_PATH)
		{
			return false;
		}

		::wcscpy_s(outPath, MAX_PATH, fullPath);
		::wcscat_s(outPath, MAX_PATH, dllName);

		return true;
	}
}

namespace dmlib
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

	extern "C"
	{
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
		inline void DummyUpdateCommonDlgsBrushes() {}
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
		inline COLORREF DummySetHighlightColor(COLORREF) { return CLR_INVALID; }

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
		[[nodiscard]] inline COLORREF DummyGetHighlightColor() { return CLR_INVALID; }

		[[nodiscard]] inline HBRUSH DummyGetBackgroundBrush() { return nullptr; }
		[[nodiscard]] inline HBRUSH DummyGetDlgBackgroundBrush() { return nullptr; }
		[[nodiscard]] inline HBRUSH DummyGetCtrlBackgroundBrush() { return nullptr; }
		[[nodiscard]] inline HBRUSH DummyGetHotBackgroundBrush() { return nullptr; }
		[[nodiscard]] inline HBRUSH DummyGetErrorBackgroundBrush() { return nullptr; }

		[[nodiscard]] inline HBRUSH DummyGetEdgeBrush() { return nullptr; }
		[[nodiscard]] inline HBRUSH DummyGetHotEdgeBrush() { return nullptr; }
		[[nodiscard]] inline HBRUSH DummyGetDisabledEdgeBrush() { return nullptr; }
		[[nodiscard]] inline HBRUSH DummyGetHighlightBrush() { return nullptr; }

		[[nodiscard]] inline HPEN DummyGetDarkerTextPen() { return nullptr; }
		[[nodiscard]] inline HPEN DummyGetEdgePen() { return nullptr; }
		[[nodiscard]] inline HPEN DummyGetHotEdgePen() { return nullptr; }
		[[nodiscard]] inline HPEN DummyGetDisabledEdgePen() { return nullptr; }
		[[nodiscard]] inline HPEN DummyGetHighlightPen() { return nullptr; }

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

		inline void DummySetDTPCtrlSubclass(HWND) {}
		inline void DummyRemoveDTPCtrlSubclass(HWND) {}

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

		inline const wchar_t* DummyGetDarkModeThemeName() {return nullptr; }

		inline void DummySetDarkThemeExperimentalEx(HWND, const wchar_t*) {}
		inline void DummySetDarkThemeExperimental(HWND) {}

		inline void DummySetDarkExplorerTheme(HWND) {}
		inline void DummySetDarkScrollBar(HWND) {}
		inline void DummySetDarkTooltips(HWND, int) {}
		inline void DummySetDarkThemeTheme(HWND) {}

		inline void DummySetDarkLineAbovePanelToolbar(HWND) {}

		inline void DummySetDarkListView(HWND) {}
		inline void DummySetDarkListViewCheckboxes(HWND) {}

		inline void DummySetDarkTreeViewCheckboxes(HWND) {}

		inline void DummySetDarkRichEdit(HWND) {}
		inline void DummySetDarkMonthCalendar(HWND) {}

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
		inline void DummyReplaceClientEdgeWithBorderSafeEx(HWND) {}
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
		inline BOOL DummyDarkChooseColorW(LPCHOOSECOLORW) { return FALSE; }
		inline BOOL DummyDarkChooseFontW(LPCHOOSEFONTW, int) { return FALSE; }

		inline void DummySetDarkTaskDlg(HWND) {}
		inline HRESULT CALLBACK DummyDarkTaskDlgCallback(HWND, UINT, WPARAM, LPARAM, LONG_PTR) { return S_OK; }
		inline HRESULT DummyDarkTaskDialogIndirect(const TASKDIALOGCONFIG*, int*, int*, BOOL*) { return S_OK; }
		inline int DummyDarkMessageBoxW(HWND, LPCWSTR, LPCWSTR, UINT) { return 0; }
	}

	using getLibInfo_t = auto (*)(int libInfoType) -> int;
	inline getLibInfo_t getLibInfo = nullptr;

	using initDarkModeConfig_t = void (*)(UINT dmType);
	inline initDarkModeConfig_t initDarkModeConfig = nullptr;

	using setRoundCornerConfig_t = void (*)(UINT roundCornerStyle);
	inline setRoundCornerConfig_t setRoundCornerConfig = nullptr;

	using setBorderColorConfig_t = void (*)(COLORREF clr);
	inline setBorderColorConfig_t setBorderColorConfig = nullptr;

	using setMicaConfig_t = void (*)(UINT mica);
	inline setMicaConfig_t setMicaConfig = nullptr;

	using setMicaExtendedConfig_t = void (*)(bool extendMica);
	inline setMicaExtendedConfig_t setMicaExtendedConfig = nullptr;

	using setColorizeTitleBarConfig_t = void (*)(bool colorize);
	inline setColorizeTitleBarConfig_t setColorizeTitleBarConfig = nullptr;

	using setDarkModeConfigEx_t = void (*)(UINT dmType);
	inline setDarkModeConfigEx_t setDarkModeConfigEx = nullptr;

	using setDarkModeConfig_t = void (*)();
	inline setDarkModeConfig_t setDarkModeConfig = nullptr;

	using initDarkModeEx_t = void (*)(const wchar_t* iniName);
	inline initDarkModeEx_t initDarkModeEx = nullptr;

	using initDarkMode_t = void (*)();
	inline initDarkMode_t initDarkMode = nullptr;

	using doesConfigFileExist_t = auto (*)() -> bool;
	inline doesConfigFileExist_t doesConfigFileExist = nullptr;

	using isEnabled_t = auto (*)() -> bool;
	inline isEnabled_t isEnabled = nullptr;

	using isExperimentalActive_t = auto (*)() -> bool;
	inline isExperimentalActive_t isExperimentalActive = nullptr;

	using isExperimentalSupported_t = auto (*)() -> bool;
	inline isExperimentalSupported_t isExperimentalSupported = nullptr;

	using isWindowsModeEnabled_t = auto (*)() -> bool;
	inline isWindowsModeEnabled_t isWindowsModeEnabled = nullptr;

	using isAtLeastWindows10_t = auto (*)() -> bool;
	inline isAtLeastWindows10_t isAtLeastWindows10 = nullptr;

	using isAtLeastWindows11_t = auto (*)() -> bool;
	inline isAtLeastWindows11_t isAtLeastWindows11 = nullptr;

	using getWindowsBuildNumber_t = auto (*)() -> DWORD;
	inline getWindowsBuildNumber_t getWindowsBuildNumber = nullptr;

	using handleSettingChange_t = auto (*)(LPARAM lParam) -> bool;
	inline handleSettingChange_t handleSettingChange = nullptr;

	using isDarkModeReg_t = auto (*)() -> bool;
	inline isDarkModeReg_t isDarkModeReg = nullptr;

	using setSysColor_t = auto (*)(int nIndex, COLORREF color) -> void;
	inline setSysColor_t setSysColor = nullptr;

	using updateCommonDlgsBrushes_t = auto (*)() -> void;
	inline updateCommonDlgsBrushes_t updateCommonDlgsBrushes = nullptr;

	using enableDarkScrollBarForWindowAndChildren_t = auto (*)(HWND hWnd) -> void;
	inline enableDarkScrollBarForWindowAndChildren_t enableDarkScrollBarForWindowAndChildren = nullptr;

	using setColorTone_t = void (*)(int colorTone);
	inline setColorTone_t setColorTone = nullptr;

	using getColorTone_t = auto (*)() -> int;
	inline getColorTone_t getColorTone = nullptr;

	using setBackgroundColor_t = auto (*)(COLORREF clrNew) -> COLORREF;
	inline setBackgroundColor_t setBackgroundColor = nullptr;

	using setCtrlBackgroundColor_t = auto (*)(COLORREF clrNew) -> COLORREF;
	inline setCtrlBackgroundColor_t setCtrlBackgroundColor = nullptr;

	using setHotBackgroundColor_t = auto (*)(COLORREF clrNew) -> COLORREF;
	inline setHotBackgroundColor_t setHotBackgroundColor = nullptr;

	using setDlgBackgroundColor_t = auto (*)(COLORREF clrNew) -> COLORREF;
	inline setDlgBackgroundColor_t setDlgBackgroundColor = nullptr;

	using setErrorBackgroundColor_t = auto (*)(COLORREF clrNew) -> COLORREF;
	inline setErrorBackgroundColor_t setErrorBackgroundColor = nullptr;

	using setTextColor_t = auto (*)(COLORREF clrNew) -> COLORREF;
	inline setTextColor_t setTextColor = nullptr;

	using setDarkerTextColor_t = auto (*)(COLORREF clrNew) -> COLORREF;
	inline setDarkerTextColor_t setDarkerTextColor = nullptr;

	using setDisabledTextColor_t = auto (*)(COLORREF clrNew) -> COLORREF;
	inline setDisabledTextColor_t setDisabledTextColor = nullptr;

	using setLinkTextColor_t = auto (*)(COLORREF clrNew) -> COLORREF;
	inline setLinkTextColor_t setLinkTextColor = nullptr;

	using setEdgeColor_t = auto (*)(COLORREF clrNew) -> COLORREF;
	inline setEdgeColor_t setEdgeColor = nullptr;

	using setHotEdgeColor_t = auto (*)(COLORREF clrNew) -> COLORREF;
	inline setHotEdgeColor_t setHotEdgeColor = nullptr;

	using setDisabledEdgeColor_t = auto (*)(COLORREF clrNew) -> COLORREF;
	inline setDisabledEdgeColor_t setDisabledEdgeColor = nullptr;

	using setHighlightColor_t = auto (*)(COLORREF clrNew) -> COLORREF;
	inline setHighlightColor_t setHighlightColor = nullptr;

	using setThemeColors_t = void (*)(Colors colors);
	inline setThemeColors_t setThemeColors = nullptr;

	using updateThemeBrushesAndPens_t = void (*)();
	inline updateThemeBrushesAndPens_t updateThemeBrushesAndPens = nullptr;

	using getBackgroundColor_t = auto (*)() -> COLORREF;
	inline getBackgroundColor_t getBackgroundColor = nullptr;

	using getCtrlBackgroundColor_t = auto (*)() -> COLORREF;
	inline getCtrlBackgroundColor_t getCtrlBackgroundColor = nullptr;

	using getHotBackgroundColor_t = auto (*)() -> COLORREF;
	inline getHotBackgroundColor_t getHotBackgroundColor = nullptr;

	using getDlgBackgroundColor_t = auto (*)() -> COLORREF;
	inline getDlgBackgroundColor_t getDlgBackgroundColor = nullptr;

	using getErrorBackgroundColor_t = auto (*)() -> COLORREF;
	inline getErrorBackgroundColor_t getErrorBackgroundColor = nullptr;

	using getTextColor_t = auto (*)() -> COLORREF;
	inline getTextColor_t getTextColor = nullptr;

	using getDarkerTextColor_t = auto (*)() -> COLORREF;
	inline getDarkerTextColor_t getDarkerTextColor = nullptr;

	using getDisabledTextColor_t = auto (*)() -> COLORREF;
	inline getDisabledTextColor_t getDisabledTextColor = nullptr;

	using getLinkTextColor_t = auto (*)() -> COLORREF;
	inline getLinkTextColor_t getLinkTextColor = nullptr;

	using getEdgeColor_t = auto (*)() -> COLORREF;
	inline getEdgeColor_t getEdgeColor = nullptr;

	using getHotEdgeColor_t = auto (*)() -> COLORREF;
	inline getHotEdgeColor_t getHotEdgeColor = nullptr;

	using getDisabledEdgeColor_t = auto (*)() -> COLORREF;
	inline getDisabledEdgeColor_t getDisabledEdgeColor = nullptr;

	using getHighlightColor_t = auto (*)() -> COLORREF;
	inline getHighlightColor_t getHighlightColor = nullptr;

	using getBackgroundBrush_t = auto (*)() -> HBRUSH;
	inline getBackgroundBrush_t getBackgroundBrush = nullptr;

	using getDlgBackgroundBrush_t = auto (*)() -> HBRUSH;
	inline getDlgBackgroundBrush_t getDlgBackgroundBrush = nullptr;

	using getCtrlBackgroundBrush_t = auto (*)() -> HBRUSH;
	inline getCtrlBackgroundBrush_t getCtrlBackgroundBrush = nullptr;

	using getHotBackgroundBrush_t = auto (*)() -> HBRUSH;
	inline getHotBackgroundBrush_t getHotBackgroundBrush = nullptr;

	using getErrorBackgroundBrush_t = auto (*)() -> HBRUSH;
	inline getErrorBackgroundBrush_t getErrorBackgroundBrush = nullptr;

	using getEdgeBrush_t = auto (*)() -> HBRUSH;
	inline getEdgeBrush_t getEdgeBrush = nullptr;

	using getHotEdgeBrush_t = auto (*)() -> HBRUSH;
	inline getHotEdgeBrush_t getHotEdgeBrush = nullptr;

	using getDisabledEdgeBrush_t = auto (*)() -> HBRUSH;
	inline getDisabledEdgeBrush_t getDisabledEdgeBrush = nullptr;

	using getHighlightBrush_t = auto (*)() -> HBRUSH;
	inline getHighlightBrush_t getHighlightBrush = nullptr;

	using getDarkerTextPen_t = auto (*)() -> HPEN;
	inline getDarkerTextPen_t getDarkerTextPen = nullptr;

	using getEdgePen_t = auto (*)() -> HPEN;
	inline getEdgePen_t getEdgePen = nullptr;

	using getHotEdgePen_t = auto (*)() -> HPEN;
	inline getHotEdgePen_t getHotEdgePen = nullptr;

	using getDisabledEdgePen_t = auto (*)() -> HPEN;
	inline getDisabledEdgePen_t getDisabledEdgePen = nullptr;

	using getHighlightPen_t = auto (*)() -> HPEN;
	inline getHighlightPen_t getHighlightPen = nullptr;

	using setViewBackgroundColor_t = auto (*)(COLORREF clrNew) -> COLORREF;
	inline setViewBackgroundColor_t setViewBackgroundColor = nullptr;

	using setViewTextColor_t = auto (*)(COLORREF clrNew) -> COLORREF;
	inline setViewTextColor_t setViewTextColor = nullptr;

	using setViewGridlinesColor_t = auto (*)(COLORREF clrNew) -> COLORREF;
	inline setViewGridlinesColor_t setViewGridlinesColor = nullptr;

	using setHeaderBackgroundColor_t = auto (*)(COLORREF clrNew) -> COLORREF;
	inline setHeaderBackgroundColor_t setHeaderBackgroundColor = nullptr;

	using setHeaderHotBackgroundColor_t = auto (*)(COLORREF clrNew) -> COLORREF;
	inline setHeaderHotBackgroundColor_t setHeaderHotBackgroundColor = nullptr;

	using setHeaderTextColor_t = auto (*)(COLORREF clrNew) -> COLORREF;
	inline setHeaderTextColor_t setHeaderTextColor = nullptr;

	using setHeaderEdgeColor_t = auto (*)(COLORREF clrNew) -> COLORREF;
	inline setHeaderEdgeColor_t setHeaderEdgeColor = nullptr;

	using setViewColors_t = void (*)(ColorsView colors);
	inline setViewColors_t setViewColors = nullptr;

	using updateViewBrushesAndPens_t = void (*)();
	inline updateViewBrushesAndPens_t updateViewBrushesAndPens = nullptr;

	using getViewBackgroundColor_t = auto (*)() -> COLORREF;
	inline getViewBackgroundColor_t getViewBackgroundColor = nullptr;

	using getViewTextColor_t = auto (*)() -> COLORREF;
	inline getViewTextColor_t getViewTextColor = nullptr;

	using getViewGridlinesColor_t = auto (*)() -> COLORREF;
	inline getViewGridlinesColor_t getViewGridlinesColor = nullptr;

	using getHeaderBackgroundColor_t = auto (*)() -> COLORREF;
	inline getHeaderBackgroundColor_t getHeaderBackgroundColor = nullptr;

	using getHeaderHotBackgroundColor_t = auto (*)() -> COLORREF;
	inline getHeaderHotBackgroundColor_t getHeaderHotBackgroundColor = nullptr;

	using getHeaderTextColor_t = auto (*)() -> COLORREF;
	inline getHeaderTextColor_t getHeaderTextColor = nullptr;

	using getHeaderEdgeColor_t = auto (*)() -> COLORREF;
	inline getHeaderEdgeColor_t getHeaderEdgeColor = nullptr;

	using getViewBackgroundBrush_t = auto (*)() -> HBRUSH;
	inline getViewBackgroundBrush_t getViewBackgroundBrush = nullptr;

	using getViewGridlinesBrush_t = auto (*)() -> HBRUSH;
	inline getViewGridlinesBrush_t getViewGridlinesBrush = nullptr;

	using getHeaderBackgroundBrush_t = auto (*)() -> HBRUSH;
	inline getHeaderBackgroundBrush_t getHeaderBackgroundBrush = nullptr;

	using getHeaderHotBackgroundBrush_t = auto (*)() -> HBRUSH;
	inline getHeaderHotBackgroundBrush_t getHeaderHotBackgroundBrush = nullptr;

	using getHeaderEdgePen_t = auto (*)() -> HPEN;
	inline getHeaderEdgePen_t getHeaderEdgePen = nullptr;

	using setDefaultColors_t = void (*)(bool updateBrushesAndOther);
	inline setDefaultColors_t setDefaultColors = nullptr;

	using setCheckboxOrRadioBtnCtrlSubclass_t = void (*)(HWND hWnd);
	inline setCheckboxOrRadioBtnCtrlSubclass_t setCheckboxOrRadioBtnCtrlSubclass = nullptr;

	using removeCheckboxOrRadioBtnCtrlSubclass_t = void (*)(HWND hWnd);
	inline removeCheckboxOrRadioBtnCtrlSubclass_t removeCheckboxOrRadioBtnCtrlSubclass = nullptr;

	using setGroupboxCtrlSubclass_t = void (*)(HWND hWnd);
	inline setGroupboxCtrlSubclass_t setGroupboxCtrlSubclass = nullptr;

	using removeGroupboxCtrlSubclass_t = void (*)(HWND hWnd);
	inline removeGroupboxCtrlSubclass_t removeGroupboxCtrlSubclass = nullptr;

	using setUpDownCtrlSubclass_t = void (*)(HWND hWnd);
	inline setUpDownCtrlSubclass_t setUpDownCtrlSubclass = nullptr;

	using removeUpDownCtrlSubclass_t = void (*)(HWND hWnd);
	inline removeUpDownCtrlSubclass_t removeUpDownCtrlSubclass = nullptr;

	using setTabCtrlUpDownSubclass_t = void (*)(HWND hWnd);
	inline setTabCtrlUpDownSubclass_t setTabCtrlUpDownSubclass = nullptr;

	using removeTabCtrlUpDownSubclass_t = void (*)(HWND hWnd);
	inline removeTabCtrlUpDownSubclass_t removeTabCtrlUpDownSubclass = nullptr;

	using setTabCtrlSubclass_t = void (*)(HWND hWnd);
	inline setTabCtrlSubclass_t setTabCtrlSubclass = nullptr;

	using removeTabCtrlSubclass_t = void (*)(HWND hWnd);
	inline removeTabCtrlSubclass_t removeTabCtrlSubclass = nullptr;

	using setCustomBorderForListBoxOrEditCtrlSubclass_t = void (*)(HWND hWnd);
	inline setCustomBorderForListBoxOrEditCtrlSubclass_t setCustomBorderForListBoxOrEditCtrlSubclass = nullptr;

	using removeCustomBorderForListBoxOrEditCtrlSubclass_t = void (*)(HWND hWnd);
	inline removeCustomBorderForListBoxOrEditCtrlSubclass_t removeCustomBorderForListBoxOrEditCtrlSubclass = nullptr;

	using setComboBoxCtrlSubclass_t = void (*)(HWND hWnd);
	inline setComboBoxCtrlSubclass_t setComboBoxCtrlSubclass = nullptr;

	using removeComboBoxCtrlSubclass_t = void (*)(HWND hWnd);
	inline removeComboBoxCtrlSubclass_t removeComboBoxCtrlSubclass = nullptr;

	using setComboBoxExCtrlSubclass_t = void (*)(HWND hWnd);
	inline setComboBoxExCtrlSubclass_t setComboBoxExCtrlSubclass = nullptr;

	using removeComboBoxExCtrlSubclass_t = void (*)(HWND hWnd);
	inline removeComboBoxExCtrlSubclass_t removeComboBoxExCtrlSubclass = nullptr;

	using setListViewCtrlSubclass_t = void (*)(HWND hWnd);
	inline setListViewCtrlSubclass_t setListViewCtrlSubclass = nullptr;

	using removeListViewCtrlSubclass_t = void (*)(HWND hWnd);
	inline removeListViewCtrlSubclass_t removeListViewCtrlSubclass = nullptr;

	using setHeaderCtrlSubclass_t = void (*)(HWND hWnd);
	inline setHeaderCtrlSubclass_t setHeaderCtrlSubclass = nullptr;

	using removeHeaderCtrlSubclass_t = void (*)(HWND hWnd);
	inline removeHeaderCtrlSubclass_t removeHeaderCtrlSubclass = nullptr;

	using setStatusBarCtrlSubclass_t = void (*)(HWND hWnd);
	inline setStatusBarCtrlSubclass_t setStatusBarCtrlSubclass = nullptr;

	using removeStatusBarCtrlSubclass_t = void (*)(HWND hWnd);
	inline removeStatusBarCtrlSubclass_t removeStatusBarCtrlSubclass = nullptr;

	using setProgressBarCtrlSubclass_t = void (*)(HWND hWnd);
	inline setProgressBarCtrlSubclass_t setProgressBarCtrlSubclass = nullptr;

	using removeProgressBarCtrlSubclass_t = void (*)(HWND hWnd);
	inline removeProgressBarCtrlSubclass_t removeProgressBarCtrlSubclass = nullptr;

	using setStaticTextCtrlSubclass_t = void (*)(HWND hWnd);
	inline setStaticTextCtrlSubclass_t setStaticTextCtrlSubclass = nullptr;

	using removeStaticTextCtrlSubclass_t = void (*)(HWND hWnd);
	inline removeStaticTextCtrlSubclass_t removeStaticTextCtrlSubclass = nullptr;

	using setIPAddressCtrlSubclass_t = void (*)(HWND hWnd);
	inline setIPAddressCtrlSubclass_t setIPAddressCtrlSubclass = nullptr;

	using removeIPAddressCtrlSubclass_t = void (*)(HWND hWnd);
	inline removeIPAddressCtrlSubclass_t removeIPAddressCtrlSubclass = nullptr;

	using setHotKeyCtrlSubclass_t = void (*)(HWND hWnd);
	inline setHotKeyCtrlSubclass_t setHotKeyCtrlSubclass = nullptr;

	using removeHotKeyCtrlSubclass_t = void (*)(HWND hWnd);
	inline removeHotKeyCtrlSubclass_t removeHotKeyCtrlSubclass = nullptr;

	using setDTPCtrlSubclass_t = void (*)(HWND hWnd);
	inline setDTPCtrlSubclass_t setDTPCtrlSubclass = nullptr;

	using removeDTPCtrlSubclass_t = void (*)(HWND hWnd);
	inline removeDTPCtrlSubclass_t removeDTPCtrlSubclass = nullptr;

	using setChildCtrlsSubclassAndThemeEx_t = void (*)(HWND hParent, bool subclass, bool theme);
	inline setChildCtrlsSubclassAndThemeEx_t setChildCtrlsSubclassAndThemeEx = nullptr;

	using setChildCtrlsSubclassAndTheme_t = void (*)(HWND hParent);
	inline setChildCtrlsSubclassAndTheme_t setChildCtrlsSubclassAndTheme = nullptr;

	using setChildCtrlsTheme_t = void (*)(HWND hParent);
	inline setChildCtrlsTheme_t setChildCtrlsTheme = nullptr;

	using setWindowEraseBgSubclass_t = void (*)(HWND hWnd);
	inline setWindowEraseBgSubclass_t setWindowEraseBgSubclass = nullptr;

	using removeWindowEraseBgSubclass_t = void (*)(HWND hWnd);
	inline removeWindowEraseBgSubclass_t removeWindowEraseBgSubclass = nullptr;

	using setWindowCtlColorSubclass_t = void (*)(HWND hWnd);
	inline setWindowCtlColorSubclass_t setWindowCtlColorSubclass = nullptr;

	using removeWindowCtlColorSubclass_t = void (*)(HWND hWnd);
	inline removeWindowCtlColorSubclass_t removeWindowCtlColorSubclass = nullptr;

	using setWindowNotifyCustomDrawSubclass_t = void (*)(HWND hWnd);
	inline setWindowNotifyCustomDrawSubclass_t setWindowNotifyCustomDrawSubclass = nullptr;

	using removeWindowNotifyCustomDrawSubclass_t = void (*)(HWND hWnd);
	inline removeWindowNotifyCustomDrawSubclass_t removeWindowNotifyCustomDrawSubclass = nullptr;

	using setWindowMenuBarSubclass_t = void (*)(HWND hWnd);
	inline setWindowMenuBarSubclass_t setWindowMenuBarSubclass = nullptr;

	using removeWindowMenuBarSubclass_t = void (*)(HWND hWnd);
	inline removeWindowMenuBarSubclass_t removeWindowMenuBarSubclass = nullptr;

	using setWindowSettingChangeSubclass_t = void (*)(HWND hWnd);
	inline setWindowSettingChangeSubclass_t setWindowSettingChangeSubclass = nullptr;

	using removeWindowSettingChangeSubclass_t = void (*)(HWND hWnd);
	inline removeWindowSettingChangeSubclass_t removeWindowSettingChangeSubclass = nullptr;

	using enableSysLinkCtrlCtlColor_t = void (*)(HWND hWnd);
	inline enableSysLinkCtrlCtlColor_t enableSysLinkCtrlCtlColor = nullptr;

	using setDarkTitleBarEx_t = void (*)(HWND hWnd, bool useWin11Features);
	inline setDarkTitleBarEx_t setDarkTitleBarEx = nullptr;

	using setDarkTitleBar_t = void (*)(HWND hWnd);
	inline setDarkTitleBar_t setDarkTitleBar = nullptr;

	using getDarkModeThemeName_t = const wchar_t* (*)();
	inline getDarkModeThemeName_t getDarkModeThemeName = nullptr;

	using setDarkThemeExperimentalEx_t = void (*)(HWND hWnd, const wchar_t* themeClassName);
	inline setDarkThemeExperimentalEx_t setDarkThemeExperimentalEx = nullptr;

	using setDarkThemeExperimental_t = void (*)(HWND hWnd);
	inline setDarkThemeExperimental_t setDarkThemeExperimental = nullptr;

	using setDarkExplorerTheme_t = void (*)(HWND hWnd);
	inline setDarkExplorerTheme_t setDarkExplorerTheme = nullptr;

	using setDarkScrollBar_t = void (*)(HWND hWnd);
	inline setDarkScrollBar_t setDarkScrollBar = nullptr;

	using setDarkTooltips_t = void (*)(HWND hWnd, int tooltipType);
	inline setDarkTooltips_t setDarkTooltips = nullptr;

	using setDarkLineAbovePanelToolbar_t = void (*)(HWND hWnd);
	inline setDarkLineAbovePanelToolbar_t setDarkLineAbovePanelToolbar = nullptr;

	using setDarkListView_t = void (*)(HWND hWnd);
	inline setDarkListView_t setDarkListView = nullptr;

	using setDarkListViewCheckboxes_t = void (*)(HWND hWnd);
	inline setDarkListViewCheckboxes_t setDarkListViewCheckboxes = nullptr;

	using setDarkTreeViewCheckboxes_t = void (*)(HWND hWnd);
	inline setDarkTreeViewCheckboxes_t setDarkTreeViewCheckboxes = nullptr;

	using setDarkRichEdit_t = void (*)(HWND hWnd);
	inline setDarkRichEdit_t setDarkRichEdit = nullptr;

	using setDarkMonthCalendar_t = void (*)(HWND hWnd);
	inline setDarkMonthCalendar_t setDarkMonthCalendar = nullptr;

	using setDarkWndSafeEx_t = void (*)(HWND hWnd, bool useWin11Features);
	inline setDarkWndSafeEx_t setDarkWndSafeEx = nullptr;

	using setDarkWndSafe_t = void (*)(HWND hWnd);
	inline setDarkWndSafe_t setDarkWndSafe = nullptr;

	using setDarkWndNotifySafeEx_t = void (*)(HWND hWnd, bool setSettingChangeSubclass, bool useWin11Features);
	inline setDarkWndNotifySafeEx_t setDarkWndNotifySafeEx = nullptr;

	using setDarkWndNotifySafe_t = void (*)(HWND hWnd);
	inline setDarkWndNotifySafe_t setDarkWndNotifySafe = nullptr;

	using enableThemeDialogTexture_t = void (*)(HWND hWnd, bool theme);
	inline enableThemeDialogTexture_t enableThemeDialogTexture = nullptr;

	using disableVisualStyle_t = void (*)(HWND hWnd, bool doDisable);
	inline disableVisualStyle_t disableVisualStyle = nullptr;

	using calculatePerceivedLightness_t = auto (*)(COLORREF clr) -> double;
	inline calculatePerceivedLightness_t calculatePerceivedLightness = nullptr;

	using getTreeViewStyle_t = auto (*)() -> int;
	inline getTreeViewStyle_t getTreeViewStyle = nullptr;

	using calculateTreeViewStyle_t = void (*)();
	inline calculateTreeViewStyle_t calculateTreeViewStyle = nullptr;

	using setTreeViewWindowThemeEx_t = void (*)(HWND hWnd, bool force);
	inline setTreeViewWindowThemeEx_t setTreeViewWindowThemeEx = nullptr;

	using setTreeViewWindowTheme_t = void (*)(HWND hWnd);
	inline setTreeViewWindowTheme_t setTreeViewWindowTheme = nullptr;

	using getPrevTreeViewStyle_t = auto (*)() -> int;
	inline getPrevTreeViewStyle_t getPrevTreeViewStyle = nullptr;

	using setPrevTreeViewStyle_t = void (*)();
	inline setPrevTreeViewStyle_t setPrevTreeViewStyle = nullptr;

	using isThemeDark_t = auto (*)() -> bool;
	inline isThemeDark_t isThemeDark = nullptr;

	using isColorDark_t = auto (*)(COLORREF clr) -> bool;
	inline isColorDark_t isColorDark = nullptr;

	using redrawWindowFrame_t = void (*)(HWND hWnd);
	inline redrawWindowFrame_t redrawWindowFrame = nullptr;

	using setWindowStyle_t = void (*)(HWND hWnd, bool setStyle, LONG_PTR styleFlag);
	inline setWindowStyle_t setWindowStyle = nullptr;

	using setWindowExStyle_t = void (*)(HWND hWnd, bool setExStyle, LONG_PTR exStyleFlag);
	inline setWindowExStyle_t setWindowExStyle = nullptr;

	using replaceExEdgeWithBorder_t = void (*)(HWND hWnd, bool replace, LONG_PTR exStyleFlag);
	inline replaceExEdgeWithBorder_t replaceExEdgeWithBorder = nullptr;

	using replaceClientEdgeWithBorderSafeEx_t = void (*)(HWND hWnd);
	inline replaceClientEdgeWithBorderSafeEx_t replaceClientEdgeWithBorderSafeEx = nullptr;

	using replaceClientEdgeWithBorderSafe_t = void (*)(HWND hWnd);
	inline replaceClientEdgeWithBorderSafe_t replaceClientEdgeWithBorderSafe = nullptr;

	using setProgressBarClassicTheme_t = void (*)(HWND hWnd);
	inline setProgressBarClassicTheme_t setProgressBarClassicTheme = nullptr;

	using onCtlColor_t = void (*)(HDC hdc);
	inline onCtlColor_t onCtlColor = nullptr;

	using onCtlColorCtrl_t = void (*)(HDC hdc);
	inline onCtlColorCtrl_t onCtlColorCtrl = nullptr;

	using onCtlColorDlg_t = void (*)(HDC hdc);
	inline onCtlColorDlg_t onCtlColorDlg = nullptr;

	using onCtlColorError_t = void (*)(HDC hdc);
	inline onCtlColorError_t onCtlColorError = nullptr;

	using onCtlColorDlgStaticText_t = void (*)(HDC hdc, bool isTextEnabled);
	inline onCtlColorDlgStaticText_t onCtlColorDlgStaticText = nullptr;

	using onCtlColorDlgLinkText_t = void (*)(HDC hdc, bool isTextEnabled);
	inline onCtlColorDlgLinkText_t onCtlColorDlgLinkText = nullptr;

	using onCtlColorListbox_t = void (*)(WPARAM wParam, LPARAM lParam);
	inline onCtlColorListbox_t onCtlColorListbox = nullptr;

	using HookDlgProc_t = auto (CALLBACK*)(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) -> UINT_PTR;
	inline HookDlgProc_t HookDlgProc = nullptr;

	using darkChooseColorW_t = auto (*)(LPCHOOSECOLORW cc) -> BOOL;
	inline darkChooseColorW_t darkChooseColorW = nullptr;

	using darkChooseFontW_t = auto (*)(LPCHOOSEFONTW cf, int tmplId) -> BOOL;
	inline darkChooseFontW_t darkChooseFontW = nullptr;

	using setDarkTaskDlg_t = void (*)(HWND hWnd);
	inline setDarkTaskDlg_t setDarkTaskDlg = nullptr;

	using DarkTaskDlgCallback_t = auto (CALLBACK*)(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, LONG_PTR lpRefData) -> HRESULT;
	inline DarkTaskDlgCallback_t DarkTaskDlgCallback = nullptr;

	using darkTaskDialogIndirect_t = auto (*)(const TASKDIALOGCONFIG* pTaskConfig, int* pnButton, int* pnRadioButton, BOOL* pfVerificationFlagChecked) -> HRESULT;
	inline darkTaskDialogIndirect_t darkTaskDialogIndirect = nullptr;

	using darkMessageBoxW_t = auto (*)(HWND hWnd, LPCWSTR lpText, LPCWSTR lpCaption, UINT uType) -> int;
	inline darkMessageBoxW_t darkMessageBoxW = nullptr;

	bool loadDarkModeFunctionsFromDll(const wchar_t* dllName);
}
