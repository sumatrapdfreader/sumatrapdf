// SPDX-License-Identifier: MPL-2.0

/*
 * Copyright (c) 2025 oZone10
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

// Based on Notepad++ dark mode code, original by adzm / Adam D. Walling
// with modification from Notepad++ team.
// Heavily modified by ozone10 (contributor of Notepad++)


#pragma once

#include <windows.h>

#if (NTDDI_VERSION >= NTDDI_VISTA) /*\
	&& (defined(__x86_64__) || defined(_M_X64)\
	|| defined(__arm64__) || defined(__arm64) || defined(_M_ARM64))*/

#if defined(_MSC_VER)
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "uxtheme.lib")
#pragma comment(lib, "Comctl32.lib")
#pragma comment(lib, "Gdi32.lib")
#endif

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

	// unsigned char == std::uint8_t

	/**
	 * @brief Represents tooltip from different controls.
	 */
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

	/**
	 * @brief Defines dark mode preset color tones.
	 *
	 * Used as preset to choose default colors in dark mode.
	 * Value `max` is reserved for internal range checking,
	 * do not use in application code.
	 */
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

	/**
	 * @brief Defines the available visual styles for TreeView controls.
	 *
	 * Used to control theming behavior for TreeViews:
	 * - `classic`: Legacy style without theming.
	 * - `light`: Light mode appearance.
	 * - `dark`: Dark mode appearance.
	 *
	 * Set via configuration and used by style evaluators (e.g. @ref DarkMode::calculateTreeViewStyle).
	 *
	 * @see DarkMode::calculateTreeViewStyle()
	 */
	enum class TreeViewStyle : unsigned char
	{
		classic,  ///< Non-themed legacy appearance.
		light,    ///< Light mode.
		dark      ///< Dark mode.
	};

	/**
	 * @brief Describes metadata fields and compile-time features of the dark mode library.
	 *
	 * Values of this enum are used with @ref DarkMode::getLibInfo to retrieve version numbers and
	 * determine whether specific features were enabled during compilation.
	 *
	 * @see DarkMode::getLibInfo()
	 */
	enum class LibInfo : unsigned char
	{
		featureCheck,     ///< Returns maxValue to verify enum coverage.
		verMajor,         ///< Major version number of the library.
		verMinor,         ///< Minor version number of the library.
		verRevision,      ///< Revision/patch number of the library.
		iathookExternal,  ///< Indicates if external IAT hooking is used.
		iniConfigUsed,    ///< True if `.ini` file configuration is supported.
		allowOldOS,       ///< True if older Windows versions are allowed.
		useDlgProcCtl,    ///< True if WM_CTLCOLORxxx can be handled directly in dialog procedure.
		preferTheme,      ///< True if theme is supported and can be used over subclass, e.g. combo box on Windows 10+.
		maxValue          ///< Sentinel value for internal validation (not intended for use).
	};

	/**
	 * @brief Defines the available dark mode types for manual configurations.
	 *
	 * Can be used in DarkMode::initDarkModeConfig and in DarkMode::setDarkModeConfig
	 * with static_cast<UINT>(DarkModeType::'value').
	 *
	 * @note Also used internally to distinguish between light, dark, and classic modes.
	 *
	 * @see DarkMode::initDarkModeConfig()
	 * @see DarkMode::setDarkModeConfig()
	 */
	enum class DarkModeType : unsigned char
	{
		light = 0,  ///< Light mode appearance.
		dark = 1,   ///< Dark mode appearance.
		classic = 3 ///< Classic (non-themed or system) appearance.
	};

	/**
	 * @brief Returns library version information or compile-time feature flags.
	 *
	 * @param libInfoType The type of information to query.
	 * @return Integer representing the requested value or feature flag.
	 *
	 * @see LibInfo
	 */
	[[nodiscard]] int getLibInfo(LibInfo libInfoType);

	// ========================================================================
	// Config
	// ========================================================================

	/**
	 * @brief Initializes the dark mode configuration based on the selected mode.
	 *
	 * For convenience @ref DarkModeType enums values can be used.
	 *
	 * @param dmType Configuration mode:
	 *        - 0: Light mode
	 *        - 1: Dark mode
	 *        - 3: Classic mode
	 *
	 * @note Values 2 and 4 are reserved for internal use only.
	 *       Using them can cause visual glitches.
	 */
	void initDarkModeConfig(UINT dmType);

	/// Sets the preferred window corner style on Windows 11. (DWM_WINDOW_CORNER_PREFERENCE values)
	void setRoundCornerConfig(UINT roundCornerStyle);

	/// Sets the preferred border color for window edge on Windows 11.
	void setBorderColorConfig(COLORREF clr);

	// Sets the Mica effects on Windows 11 setting. (DWM_SYSTEMBACKDROP_TYPE values)
	void setMicaConfig(UINT mica);

	/// Sets Mica effects on the full window setting.
	void setMicaExtendedConfig(bool extendMica);

	/// Sets dialog colors on title bar on Windows 11 setting.
	void setColorizeTitleBarConfig(bool colorize);

	/// Applies dark mode settings based on the given configuration type. (DarkModeType values)
	void setDarkModeConfig(UINT dmType);

	/// Applies dark mode settings based on system mode preference.
	void setDarkModeConfig();

	/// Initializes dark mode experimental features, colors, and other settings.
	void initDarkMode(const wchar_t* iniName);

	///Initializes dark mode without INI settings.
	void initDarkMode();

	// ========================================================================
	// Basic checks
	// ========================================================================

	/// Checks if non-classic mode is enabled.
	[[nodiscard]] bool isEnabled();

	/// Checks if experimental dark mode features are currently active.
	[[nodiscard]] bool isExperimentalActive();

	/// Checks if experimental dark mode features are supported by the system.
	[[nodiscard]] bool isExperimentalSupported();

	/// Checks if follow the system mode behavior is enabled.
	[[nodiscard]] bool isWindowsModeEnabled();

	/// Checks if the host OS is at least Windows 10.
	[[nodiscard]] bool isAtLeastWindows10();

	/// Checks if the host OS is at least Windows 11.
	[[nodiscard]] bool isAtLeastWindows11();

	/// Retrieves the current Windows build number.
	[[nodiscard]] DWORD getWindowsBuildNumber();

	// ========================================================================
	// System Events
	// ========================================================================

	/// Handles system setting changes related to dark mode.
	bool handleSettingChange(LPARAM lParam);

	/// Checks if dark mode is enabled in the Windows registry.
	[[nodiscard]] bool isDarkModeReg();

	// ========================================================================
	// From DarkMode.h
	// ========================================================================

	/**
	 * @brief Overrides a specific system color with a custom color.
	 *
	 * Currently supports:
	 * - `COLOR_WINDOW`: Background of ComboBoxEx list.
	 * - `COLOR_WINDOWTEXT`: Text color of ComboBoxEx list.
	 * - `COLOR_BTNFACE`: Gridline color in ListView (when applicable).
	 *
	 * @param nIndex One of the supported system color indices.
	 * @param color Custom `COLORREF` value to apply.
	 */
	void setSysColor(int nIndex, COLORREF color);

	// ========================================================================
	// Enhancements to DarkMode.h
	// ========================================================================

	/// Makes scroll bars on the specified window and all its children consistent.
	void enableDarkScrollBarForWindowAndChildren(HWND hWnd);

	// ========================================================================
	// Colors
	// ========================================================================

	/// Sets the color tone and its color set for the active theme.
	void setColorTone(ColorTone colorTone);

	/// Retrieves the currently active color tone for the theme.
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

	/// Initializes default color set based on the current mode type.
	void setDefaultColors(bool updateBrushesAndOther);

	// ========================================================================
	// Paint Helpers
	// ========================================================================

	/// Paints a rounded rectangle using the specified pen and brush.
	void paintRoundRect(HDC hdc, const RECT& rect, HPEN hpen, HBRUSH hBrush, int width = 0, int height = 0);
	/// Paints an unfilled rounded rectangle (frame only).
	void paintRoundFrameRect(HDC hdc, const RECT& rect, HPEN hpen, int width = 0, int height = 0);

	// ========================================================================
	// Control Subclassing
	// ========================================================================

	/// Applies themed owner drawn subclassing to a checkbox, radio, or tri-state button control.
	void setCheckboxOrRadioBtnCtrlSubclass(HWND hWnd);
	/// Removes the owner drawn subclass from a a checkbox, radio, or tri-state button control.
	void removeCheckboxOrRadioBtnCtrlSubclass(HWND hWnd);

	/// Applies owner drawn subclassing to a groupbox button control.
	void setGroupboxCtrlSubclass(HWND hWnd);
	/// Removes the owner drawn subclass from a groupbox button control.
	void removeGroupboxCtrlSubclass(HWND hWnd);

	/// Applies owner drawn subclassing and theming to an updown (spinner) control.
	void setUpDownCtrlSubclass(HWND hWnd);
	/// Removes the owner drawn subclass from a updown (spinner) control.
	void removeUpDownCtrlSubclass(HWND hWnd);

	void setTabCtrlUpDownSubclass(HWND hWnd);
	void removeTabCtrlUpDownSubclass(HWND hWnd);
	void setTabCtrlSubclass(HWND hWnd);
	void removeTabCtrlSubclass(HWND hWnd);

	void setCustomBorderForListBoxOrEditCtrlSubclass(HWND hWnd);
	void removeCustomBorderForListBoxOrEditCtrlSubclass(HWND hWnd);

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

	// ========================================================================
	// Child Subclassing
	// ========================================================================

	void setChildCtrlsSubclassAndTheme(HWND hParent, bool subclass = true, bool theme = true);
	void setChildCtrlsTheme(HWND hParent);

	// ========================================================================
	// Window, Parent, And Other Subclassing
	// ========================================================================

	/// Applies window subclassing to handle `WM_ERASEBKGND` message.
	void setWindowEraseBgSubclass(HWND hWnd);
	/// Removes the subclass used for `WM_ERASEBKGND` message handling.
	void removeWindowEraseBgSubclass(HWND hWnd);

	/// Applies window subclassing to handle `WM_CTLCOLOR*` messages.
	void setWindowCtlColorSubclass(HWND hWnd);
	/// Removes the subclass used for `WM_CTLCOLOR*` messages handling.
	void removeWindowCtlColorSubclass(HWND hWnd);

	/// Applies window subclassing for handling `NM_CUSTOMDRAW` notifications for custom drawing.
	void setWindowNotifyCustomDrawSubclass(HWND hWnd);
	/// Removes the subclass used for handling `NM_CUSTOMDRAW` notifications for custom drawing.
	void removeWindowNotifyCustomDrawSubclass(HWND hWnd);

	/// Applies window subclassing for menu bar themed custom drawing.
	void setWindowMenuBarSubclass(HWND hWnd);
	/// Removes the subclass used for menu bar themed custom drawing.
	void removeWindowMenuBarSubclass(HWND hWnd);

	/// Applies window subclassing to handle `WM_SETTINGCHANGE` message.
	void setWindowSettingChangeSubclass(HWND hWnd);
	/// Removes the subclass used for `WM_SETTINGCHANGE` message handling.
	void removeWindowSettingChangeSubclass(HWND hWnd);

	// ========================================================================
	// Theme And Helpers
	// ========================================================================

	/// Configures the SysLink control to be affected by `WM_CTLCOLORSTATIC` message.
	void enableSysLinkCtrlCtlColor(HWND hWnd);

	/// Sets dark title bar and optional Windows 11 features.
	void setDarkTitleBarEx(HWND hWnd, bool useWin11Features);
	/// Sets dark mode title bar on supported Windows versions.
	void setDarkTitleBar(HWND hWnd);

	/// Applies an experimental visual style to the specified window, if supported.
	void setDarkThemeExperimental(HWND hWnd, const wchar_t* themeClassName = L"Explorer");
	/// Applies "DarkMode_Explorer" visual style if experimental mode is active.
	void setDarkExplorerTheme(HWND hWnd);
	/// Applies "DarkMode_Explorer" visual style to scroll bars.
	void setDarkScrollBar(HWND hWnd);
	/// Applies "DarkMode_Explorer" visual style to tooltip controls based on context.
	void setDarkTooltips(HWND hWnd, ToolTipsType type = ToolTipsType::tooltip);

	/// Sets the color of line above a toolbar control for non-classic mode.
	void setDarkLineAbovePanelToolbar(HWND hWnd);
	/// Applies an experimental Explorer visual style to a list view.
	void setDarkListView(HWND hWnd);
	/// Replaces default list view checkboxes with themed dark-mode versions on Windows 11.
	void setDarkListViewCheckboxes(HWND hWnd);
	/// Sets colors and edges for a RichEdit control.
	void setDarkRichEdit(HWND hWnd);

	/// Applies visual styles; ctl color message and child controls subclassings to a window safely.
	void setDarkWndSafe(HWND hWnd, bool useWin11Features = true);
	/// Applies visual styles; ctl color message, child controls, custom drawing, and setting change subclassings to a window safely.
	void setDarkWndNotifySafeEx(HWND hWnd, bool setSettingChangeSubclass = false, bool useWin11Features = true);
	/// Applies visual styles; ctl color message, child controls, and custom drawing subclassings to a window safely.
	void setDarkWndNotifySafe(HWND hWnd, bool useWin11Features = true);

	/// Enables or disables theme-based dialog background textures in classic mode.
	void enableThemeDialogTexture(HWND hWnd, bool theme);

	/// Enables or disables visual styles for a window.
	void disableVisualStyle(HWND hWnd, bool doDisable);

	/// Calculates perceptual lightness of a COLORREF color.
	[[nodiscard]] double calculatePerceivedLightness(COLORREF clr);

	/// Retrieves the current TreeView style configuration.
	[[nodiscard]] const TreeViewStyle& getTreeViewStyle();

	/// Determines appropriate TreeView style based on background perceived lightness.
	void calculateTreeViewStyle();

	/// Applies the appropriate window theme style to the specified TreeView.
	void setTreeViewWindowTheme(HWND hWnd, bool force = false);

	/// Retrieves the previous TreeView style configuration.
	[[nodiscard]] const TreeViewStyle& getPrevTreeViewStyle();

	/// Stores the current TreeView style as the previous style for later comparison.
	void setPrevTreeViewStyle();

	/// Checks whether the current theme is dark.
	[[nodiscard]] bool isThemeDark();

	/// Checks whether the color is dark.
	[[nodiscard]] bool isColorDark(COLORREF clr);

	/// Forces a window to redraw its non-client frame.
	void redrawWindowFrame(HWND hWnd);
	/// Sets a window's standard style flags and redraws window if needed.
	void setWindowStyle(HWND hWnd, bool setStyle, LONG_PTR styleFlag);
	/// Sets a window's extended style flags and redraws window if needed.
	void setWindowExStyle(HWND hWnd, bool setExStyle, LONG_PTR exStyleFlag);
	/// Replaces an extended edge (e.g. client edge) with a standard window border.
	void replaceExEdgeWithBorder(HWND hWnd, bool replace, LONG_PTR exStyleFlag);
	/// Safely toggles `WS_EX_CLIENTEDGE` with `WS_BORDER` based on dark mode state.
	void replaceClientEdgeWithBorderSafe(HWND hWnd);

	/// Applies classic-themed styling to a progress bar in non-classic mode.
	void setProgressBarClassicTheme(HWND hWnd);

	// ========================================================================
	// Ctl Color
	// ========================================================================

	/// Handles text and background colorizing for read-only controls.
	[[nodiscard]] LRESULT onCtlColor(HDC hdc);

	/// Handles text and background colorizing for interactive controls.
	[[nodiscard]] LRESULT onCtlColorCtrl(HDC hdc);

	/// Handles text and background colorizing for window and disabled non-text controls.
	[[nodiscard]] LRESULT onCtlColorDlg(HDC hdc);

	/// Handles text and background colorizing for error state (for specific usage).
	[[nodiscard]] LRESULT onCtlColorError(HDC hdc);

	/// Handles text and background colorizing for static text controls.
	[[nodiscard]] LRESULT onCtlColorDlgStaticText(HDC hdc, bool isTextEnabled);

	/// Handles text and background colorizing for syslink controls.
	[[nodiscard]] LRESULT onCtlColorDlgLinkText(HDC hdc, bool isTextEnabled = true);

	/// Handles text and background colorizing for list box controls.
	[[nodiscard]] LRESULT onCtlColorListbox(WPARAM wParam, LPARAM lParam);

	// ========================================================================
	// Hook Callback Dialog Procedure
	// ========================================================================

	/**
	 * @brief Hook procedure for customizing common dialogs with dark mode.
	 *
	 * This function handles messages for all Windows common dialogs.
	 * When initialized (`WM_INITDIALOG`), it applies dark mode styling to the dialog.
	 *
	 * ## Special Case: Font Dialog Workaround
	 * - When a hook is used with `ChooseFont`, Windows **automatically falls back**
	 *   to an **older template**, losing modern UI elements.
	 * - To prevent this forced downgrade, a **modified template** (based on Font.dlg) is used.
	 * - **CBS_OWNERDRAWFIXED should be removed** from the **Size** and **Script** combo boxes
	 *   to restore proper visualization.
	 * - **Custom owner-draw visuals remain** for other font combo boxes to allow font preview.
	 * - Same for the `"AaBbYyZz"` sample text.
	 * - However **Automatic system translation for captions and static texts is lost** in this workaround.
	 *
	 * ## Custom Font Dialog Template (Resource File)
	 * ```rc
	 * IDD_DARK_FONT_DIALOG DIALOG 13, 54, 243, 234
	 * STYLE DS_MODALFRAME | WS_POPUP | WS_CAPTION | WS_SYSMENU |
	 *       DS_3DLOOK
	 * CAPTION "Font"
	 * FONT 9, "Segoe UI"
	 * BEGIN
	 *     LTEXT           "&Font:", stc1, 7, 7, 98, 9
	 *     COMBOBOX        cmb1, 7, 16, 98, 76,
	 *                     CBS_SIMPLE | CBS_AUTOHSCROLL | CBS_DISABLENOSCROLL |
	 *                     CBS_SORT | WS_VSCROLL | WS_TABSTOP | CBS_HASSTRINGS |
	 *                     CBS_OWNERDRAWFIXED
	 *
	 *     LTEXT           "Font st&yle:", stc2, 114, 7, 74, 9
	 *     COMBOBOX        cmb2, 114, 16, 74, 76,
	 *                     CBS_SIMPLE | CBS_AUTOHSCROLL | CBS_DISABLENOSCROLL |
	 *                     WS_VSCROLL | WS_TABSTOP | CBS_HASSTRINGS |
	 *                     CBS_OWNERDRAWFIXED
	 *
	 *     LTEXT           "&Size:", stc3, 198, 7, 36, 9
	 *     COMBOBOX        cmb3, 198, 16, 36, 76,
	 *                     CBS_SIMPLE | CBS_AUTOHSCROLL | CBS_DISABLENOSCROLL |
	 *                     CBS_SORT | WS_VSCROLL | WS_TABSTOP | CBS_HASSTRINGS |
	 *                     CBS_OWNERDRAWFIXED // remove CBS_OWNERDRAWFIXED
	 *
	 *     GROUPBOX        "Effects", grp1, 7, 97, 98, 76, WS_GROUP
	 *     AUTOCHECKBOX    "Stri&keout", chx1, 13, 111, 90, 10, WS_TABSTOP
	 *     AUTOCHECKBOX    "&Underline", chx2, 13, 127, 90, 10
	 *
	 *     LTEXT           "&Color:", stc4, 13, 144, 89, 9
	 *     COMBOBOX        cmb4, 13, 155, 85, 100,
	 *                     CBS_DROPDOWNLIST | CBS_OWNERDRAWFIXED | CBS_AUTOHSCROLL |
	 *                     CBS_HASSTRINGS | WS_BORDER | WS_VSCROLL | WS_TABSTOP
	 *
	 *     GROUPBOX        "Sample", grp2, 114, 97, 120, 43, WS_GROUP
	 *     CTEXT           "AaBbYyZz", stc5, 116, 106, 117, 33,
	 *                     SS_NOPREFIX | NOT WS_VISIBLE
	 *     LTEXT           "", stc6, 7, 178, 227, 20, SS_NOPREFIX | NOT WS_GROUP
	 *
	 *     LTEXT           "Sc&ript:", stc7, 114, 145, 118, 9
	 *     COMBOBOX        cmb5, 114, 155, 120, 30, CBS_DROPDOWNLIST |
	 *                     CBS_OWNERDRAWFIXED | CBS_AUTOHSCROLL | CBS_HASSTRINGS | // remove CBS_OWNERDRAWFIXED
	 *                     WS_BORDER | WS_VSCROLL | WS_TABSTOP
	 *
	 *     CONTROL         "<A>Show more fonts</A>", IDC_MANAGE_LINK, "SysLink",
	 *                     WS_TABSTOP, 7, 199, 227, 9
	 *
	 *     DEFPUSHBUTTON   "OK", IDOK, 141, 215, 45, 14, WS_GROUP
	 *     PUSHBUTTON      "Cancel", IDCANCEL, 190, 215, 45, 14, WS_GROUP
	 *     PUSHBUTTON      "&Apply", psh3, 92, 215, 45, 14, WS_GROUP
	 *     PUSHBUTTON      "&Help", pshHelp, 43, 215, 45, 14, WS_GROUP
	 * END
	 * ```
	 *
	 * ## Usage Example:
	 * ```cpp
	 * #define IDD_DARK_FONT_DIALOG 1000 // usually in resource.h or other header
	 *
	 * CHOOSEFONT cf{};
	 * cf.Flags |= CF_ENABLEHOOK | CF_ENABLETEMPLATE;
	 * cf.lpfnHook = static_cast<LPCFHOOKPROC>(DarkMode::HookDlgProc);
	 * cf.hInstance = GetModuleHandle(nullptr);
	 * cf.lpTemplateName = MAKEINTRESOURCE(IDD_DARK_FONT_DIALOG);
	 * ```
	 *
	 * @param hWnd Handle to the dialog window.
	 * @param uMsg Message identifier.
	 * @param wParam First message parameter (unused).
	 * @param lParam Second message parameter (unused).
	 * @return A value defined by the hook procedure.
	 */
	UINT_PTR CALLBACK HookDlgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
} // namespace DarkMode

#else
#define _DARKMODELIB_NOT_USED
#endif // (NTDDI_VERSION >= NTDDI_VISTA) //&& (x64 or arm64)
