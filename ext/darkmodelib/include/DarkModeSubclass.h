// Copyright (C)2024-2025 ozone10

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// at your option any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

// Based on Notepad++ dark mode code, original by adzm / Adam D. Walling
// with modification from Notepad++ team.
// Heavily modified by ozone10 (contributor of Notepad++)


#pragma once

#include <windows.h>

#if (NTDDI_VERSION >= NTDDI_VISTA) /*\
	&& (defined(__x86_64__) || defined(_M_X64)\
	|| defined(__arm64__) || defined(__arm64) || defined(_M_ARM64))*/

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

	enum class ToolTipsType
	{
		tooltip,
		toolbar,
		listview,
		treeview,
		tabbar
	};

	enum class ColorTone
	{
		black       = 0,
		red         = 1,
		green       = 2,
		blue        = 3,
		purple      = 4,
		cyan        = 5,
		olive       = 6
	};

	enum class TreeViewStyle
	{
		classic,
		light,
		dark
	};

	enum LibInfo
	{
		featureCheck    = 0,
		verMajor        = 1,
		verMinor        = 2,
		verRevision     = 3,
		iathookExternal = 4,
		iniConfigUsed   = 5,
		allowOldOS      = 6,
		useDlgProcCtl   = 7,
		maxValue        = 8
	};

	[[nodiscard]] int getLibInfo(LibInfo libInfoType);

	// enum DarkModeType { light = 0, dark = 1, classic = 3 }; values
	void initDarkModeConfig(UINT dmType);
	// DWM_WINDOW_CORNER_PREFERENCE values
	void setRoundCornerConfig(UINT roundCornerStyle);
	void setBorderColorConfig(COLORREF clr);
	// DWM_SYSTEMBACKDROP_TYPE values
	void setMicaConfig(UINT mica);
	void setMicaExtendedConfig(bool extendMica);
	// enum DarkModeType { light = 0, dark = 1, classic = 3 }; values
	void setDarkModeConfig(UINT dmType);
	void setDarkModeConfig();

	void initDarkMode(const wchar_t* iniName);
	void initDarkMode();

	[[nodiscard]] bool isEnabled();
	[[nodiscard]] bool isExperimentalActive();
	[[nodiscard]] bool isExperimentalSupported();

	[[nodiscard]] bool isWindowsModeEnabled();

	[[nodiscard]] bool isWindows10();
	[[nodiscard]] bool isWindows11();
	[[nodiscard]] DWORD getWindowsBuildNumber();

	// handle events

	bool handleSettingChange(LPARAM lParam);
	[[nodiscard]] bool isDarkModeReg();

	// from DarkMode.h

	void setSysColor(int nIndex, COLORREF color);
	bool hookSysColor();
	void unhookSysColor();

	// enhancements to DarkMode.h

	void enableDarkScrollBarForWindowAndChildren(HWND hWnd);

	// colors

	void setToneColors(ColorTone colorTone);
	[[nodiscard]] ColorTone getColorTone();

	COLORREF setBackgroundColor(COLORREF clrNew);
	COLORREF setCtrlBackgroundColor(COLORREF clrNew);
	COLORREF setHotBackgroundColor(COLORREF clrNew);
	COLORREF setDlgBackgroundColor(COLORREF clrNew);
	COLORREF setErrorBackgroundColor(COLORREF clrNew);

	COLORREF setTextColor(COLORREF clrNew);
	COLORREF setDarkerTextColor(COLORREF clrNew);
	COLORREF setDisabledTextColor(COLORREF clrNew);
	COLORREF setLinkTextColor(COLORREF clrNew);

	COLORREF setEdgeColor(COLORREF clrNew);
	COLORREF setHotEdgeColor(COLORREF clrNew);
	COLORREF setDisabledEdgeColor(COLORREF clrNew);

	void setThemeColors(Colors colors);
	void updateThemeBrushesAndPens();

	[[nodiscard]] COLORREF getBackgroundColor();
	[[nodiscard]] COLORREF getCtrlBackgroundColor();
	[[nodiscard]] COLORREF getHotBackgroundColor();
	[[nodiscard]] COLORREF getDlgBackgroundColor();
	[[nodiscard]] COLORREF getErrorBackgroundColor();

	[[nodiscard]] COLORREF getTextColor();
	[[nodiscard]] COLORREF getDarkerTextColor();
	[[nodiscard]] COLORREF getDisabledTextColor();
	[[nodiscard]] COLORREF getLinkTextColor();

	[[nodiscard]] COLORREF getEdgeColor();
	[[nodiscard]] COLORREF getHotEdgeColor();
	[[nodiscard]] COLORREF getDisabledEdgeColor();

	[[nodiscard]] HBRUSH getBackgroundBrush();
	[[nodiscard]] HBRUSH getDlgBackgroundBrush();
	[[nodiscard]] HBRUSH getCtrlBackgroundBrush();
	[[nodiscard]] HBRUSH getHotBackgroundBrush();
	[[nodiscard]] HBRUSH getErrorBackgroundBrush();

	[[nodiscard]] HBRUSH getEdgeBrush();
	[[nodiscard]] HBRUSH getHotEdgeBrush();
	[[nodiscard]] HBRUSH getDisabledEdgeBrush();

	[[nodiscard]] HPEN getDarkerTextPen();
	[[nodiscard]] HPEN getEdgePen();
	[[nodiscard]] HPEN getHotEdgePen();
	[[nodiscard]] HPEN getDisabledEdgePen();

	COLORREF setViewBackgroundColor(COLORREF clrNew);
	COLORREF setViewTextColor(COLORREF clrNew);
	COLORREF setViewGridlinesColor(COLORREF clrNew);

	COLORREF setHeaderBackgroundColor(COLORREF clrNew);
	COLORREF setHeaderHotBackgroundColor(COLORREF clrNew);
	COLORREF setHeaderTextColor(COLORREF clrNew);
	COLORREF setHeaderEdgeColor(COLORREF clrNew);

	void setViewColors(ColorsView colors);
	void updateViewBrushesAndPens();

	[[nodiscard]] COLORREF getViewBackgroundColor();
	[[nodiscard]] COLORREF getViewTextColor();
	[[nodiscard]] COLORREF getViewGridlinesColor();

	[[nodiscard]] COLORREF getHeaderBackgroundColor();
	[[nodiscard]] COLORREF getHeaderHotBackgroundColor();
	[[nodiscard]] COLORREF getHeaderTextColor();
	[[nodiscard]] COLORREF getHeaderEdgeColor();

	[[nodiscard]] HBRUSH getViewBackgroundBrush();
	[[nodiscard]] HBRUSH getViewGridlinesBrush();

	[[nodiscard]] HBRUSH getHeaderBackgroundBrush();
	[[nodiscard]] HBRUSH getHeaderHotBackgroundBrush();

	[[nodiscard]] HPEN getHeaderEdgePen();

	// paint helper

	void paintRoundRect(HDC hdc, const RECT& rect, HPEN hpen, HBRUSH hBrush, int width = 0, int height = 0);
	inline void paintRoundFrameRect(HDC hdc, const RECT& rect, HPEN hpen, int width = 0, int height = 0);

	// control subclassing

	void setCheckboxOrRadioBtnCtrlSubclass(HWND hWnd);
	void removeCheckboxOrRadioBtnCtrlSubclass(HWND hWnd);

	void setGroupboxCtrlSubclass(HWND hWnd);
	void removeGroupboxCtrlSubclass(HWND hWnd);

	void setUpDownCtrlSubclass(HWND hWnd);
	void removeUpDownCtrlSubclass(HWND hWnd);

	void setTabCtrlUpDownSubclass(HWND hWnd);
	void removeTabCtrlUpDownSubclass(HWND hWnd);
	void setTabCtrlSubclass(HWND hWnd);
	void removeTabCtrlSubclass(HWND hWnd);

	void setComboBoxCtrlSubclass(HWND hWnd);
	void removeComboBoxCtrlSubclass(HWND hWnd);

	void setComboBoxExCtrlSubclass(HWND hWnd);
	void removeComboBoxExCtrlSubclass(HWND hWnd);

	void setListViewCtrlSubclass(HWND hWnd);
	void removeListViewCtrlSubclass(HWND hWnd);

	void setHeaderCtrlSubclass(HWND hWnd);
	void removeHeaderCtrlSubclass(HWND hWnd);

	void setStatusBarCtrlSubclass(HWND hWnd);
	void removeStatusBarCtrlSubclass(HWND hWnd);

	void setProgressBarCtrlSubclass(HWND hWnd);
	void removeProgressBarCtrlSubclass(HWND hWnd);

	void setStaticTextCtrlSubclass(HWND hWnd);
	void removeStaticTextCtrlSubclass(HWND hWnd);

	// child subclassing

	void setChildCtrlsSubclassAndTheme(HWND hParent, bool subclass = true, bool theme = true);
	void setChildCtrlsTheme(HWND hParent);

	// window, parent, and other subclassing

	void setWindowEraseBgSubclass(HWND hWnd);
	void removeWindowEraseBgSubclass(HWND hWnd);

	void setWindowCtlColorSubclass(HWND hWnd);
	void removeWindowCtlColorSubclass(HWND hWnd);

	void setWindowNotifyCustomDrawSubclass(HWND hWnd, bool subclassChildren = false);
	void removeWindowNotifyCustomDrawSubclass(HWND hWnd);

	void setWindowMenuBarSubclass(HWND hWnd);
	void removeWindowMenuBarSubclass(HWND hWnd);

	void setWindowSettingChangeSubclass(HWND hWnd);
	void removeWindowSettingChangeSubclass(HWND hWnd);

	// theme and helper

	void enableSysLinkCtrlCtlColor(HWND hWnd);

	void setDarkTitleBarEx(HWND hWnd, bool useWin11Features);
	void setDarkTitleBar(HWND hWnd);
	void setDarkExplorerTheme(HWND hWnd);
	void setDarkScrollBar(HWND hWnd);
	void setDarkTooltips(HWND hWnd, ToolTipsType type = ToolTipsType::tooltip);
	void setDarkLineAbovePanelToolbar(HWND hWnd);
	void setDarkHeader(HWND hWnd);
	void setDarkListView(HWND hWnd);
	void setDarkListViewCheckboxes(HWND hWnd);
	void setDarkThemeExperimental(HWND hWnd, const wchar_t* themeClassName = L"Explorer");
	void setDarkRichEdit(HWND hWnd);

	void setDarkDlgSafe(HWND hWnd, bool useWin11Features = true);
	void setDarkDlgNotifySafe(HWND hWnd, bool useWin11Features = true);

	// only if g_dmType == DarkModeType::classic
	inline void enableThemeDialogTexture(HWND hWnd, bool theme);
	void disableVisualStyle(HWND hWnd, bool doDisable);
	[[nodiscard]] double calculatePerceivedLightness(COLORREF clr);
	void calculateTreeViewStyle();
	void updatePrevTreeViewStyle();
	[[nodiscard]] TreeViewStyle getTreeViewStyle();
	void setTreeViewStyle(HWND hWnd, bool force = false);
	[[nodiscard]] bool isThemeDark();
	inline void redrawWindowFrame(HWND hWnd);
	void setWindowStyle(HWND hWnd, bool setStyle, LONG_PTR styleFlag);
	void setWindowExStyle(HWND hWnd, bool setExStyle, LONG_PTR exStyleFlag);
	void replaceExEdgeWithBorder(HWND hWnd, bool replace, LONG_PTR exStyleFlag);
	void replaceClientEdgeWithBorderSafe(HWND hWnd);
	void setProgressBarClassicTheme(HWND hWnd);

	// ctl color

	[[nodiscard]] LRESULT onCtlColor(HDC hdc);
	[[nodiscard]] LRESULT onCtlColorCtrl(HDC hdc);
	[[nodiscard]] LRESULT onCtlColorDlg(HDC hdc);
	[[nodiscard]] LRESULT onCtlColorError(HDC hdc);
	[[nodiscard]] LRESULT onCtlColorDlgStaticText(HDC hdc, bool isTextEnabled);
	[[nodiscard]] LRESULT onCtlColorDlgLinkText(HDC hdc, bool isTextEnabled = true);
	[[nodiscard]] LRESULT onCtlColorListbox(WPARAM wParam, LPARAM lParam);

	// hook callback dialog procedure for font, color chooser,... dialogs

	UINT_PTR CALLBACK HookDlgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

// Below is copy of Windows 7 new choose font dialog template from Font.dlg.
// Using hook will force ChooseFont function to use older template.
// Workaround is to use modified template (remove CBS_OWNERDRAWFIXED
// from size and script comboboxes) copied from Font.dlg. Other comboboxes
// use custom owner draw, which are needed to show visuals for selected font.
// Same for "AaBbYyZz" text which has NOT WS_VISIBLE.
// This workaround will however remove automatic system translation for caption
// and static texts.
//
// Usage example:

//#define IDD_DARK_FONT_DIALOG 1000 // usually in resource.h or other header

//CHOOSEFONT cf{};
//// some user code
//cf.Flags |= CF_ENABLEHOOK | CF_ENABLETEMPLATE;
//cf.lpfnHook = static_cast<LPCFHOOKPROC>(DarkMode::HookDlgProc);
//cf.hInstance = GetModuleHandle(nullptr);
//cf.lpTemplateName = MAKEINTRESOURCE(IDD_DARK_FONT_DIALOG);

// in rc file

//IDD_DARK_FONT_DIALOG DIALOG 13, 54, 243, 234
//STYLE DS_MODALFRAME | WS_POPUP | WS_CAPTION | WS_SYSMENU |
//      DS_3DLOOK
//CAPTION "Font"
//FONT 9, "Segoe UI"
//BEGIN
//    LTEXT           "&Font:", stc1, 7, 7, 98, 9
//    COMBOBOX        cmb1, 7, 16, 98, 76,
//                    CBS_SIMPLE | CBS_AUTOHSCROLL | CBS_DISABLENOSCROLL |
//                    CBS_SORT | WS_VSCROLL | WS_TABSTOP | CBS_HASSTRINGS |
//                    CBS_OWNERDRAWFIXED
//
//    LTEXT           "Font st&yle:", stc2, 114, 7, 74, 9
//    COMBOBOX        cmb2, 114, 16, 74, 76,
//                    CBS_SIMPLE | CBS_AUTOHSCROLL | CBS_DISABLENOSCROLL |
//                    WS_VSCROLL | WS_TABSTOP | CBS_HASSTRINGS |
//                    CBS_OWNERDRAWFIXED
//
//    LTEXT           "&Size:", stc3, 198, 7, 36, 9
//    COMBOBOX        cmb3, 198, 16, 36, 76,
//                    CBS_SIMPLE | CBS_AUTOHSCROLL | CBS_DISABLENOSCROLL |
//                    CBS_SORT | WS_VSCROLL | WS_TABSTOP | CBS_HASSTRINGS |
//                    CBS_OWNERDRAWFIXED // remove CBS_OWNERDRAWFIXED
//
//    GROUPBOX        "Effects", grp1, 7, 97, 98, 76, WS_GROUP
//    AUTOCHECKBOX    "Stri&keout", chx1, 13, 111, 90, 10, WS_TABSTOP
//    AUTOCHECKBOX    "&Underline", chx2, 13, 127, 90, 10
//
//    LTEXT           "&Color:", stc4, 13, 144, 89, 9
//    COMBOBOX        cmb4, 13, 155, 85, 100,
//                    CBS_DROPDOWNLIST | CBS_OWNERDRAWFIXED | CBS_AUTOHSCROLL |
//                    CBS_HASSTRINGS | WS_BORDER | WS_VSCROLL | WS_TABSTOP
//
//    GROUPBOX        "Sample", grp2, 114, 97, 120, 43, WS_GROUP
//    CTEXT           "AaBbYyZz", stc5, 116, 106, 117, 33,
//                    SS_NOPREFIX | NOT WS_VISIBLE
//    LTEXT           "", stc6, 7, 178, 227, 20, SS_NOPREFIX | NOT WS_GROUP
//
//    LTEXT           "Sc&ript:", stc7, 114, 145, 118, 9
//    COMBOBOX        cmb5, 114, 155, 120, 30, CBS_DROPDOWNLIST |
//                    CBS_OWNERDRAWFIXED | CBS_AUTOHSCROLL | CBS_HASSTRINGS | // remove CBS_OWNERDRAWFIXED
//                    WS_BORDER | WS_VSCROLL | WS_TABSTOP
//    
//    CONTROL         "<A>Show more fonts</A>", IDC_MANAGE_LINK, "SysLink", 
//                    WS_TABSTOP, 7, 199, 227, 9 
//
//    DEFPUSHBUTTON   "OK", IDOK, 141, 215, 45, 14, WS_GROUP
//    PUSHBUTTON      "Cancel", IDCANCEL, 190, 215, 45, 14, WS_GROUP
//    PUSHBUTTON      "&Apply", psh3, 92, 215, 45, 14, WS_GROUP
//    PUSHBUTTON      "&Help", pshHelp, 43, 215, 45, 14, WS_GROUP
//
//END
} // namespace DarkMode

#else
#define _DARKMODELIB_NOT_USED
#endif // (NTDDI_VERSION >= NTDDI_VISTA) //&& (x64 or arm64)
