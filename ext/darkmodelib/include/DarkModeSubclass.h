// SPDX-License-Identifier: MPL-2.0

/*
 * Copyright (c) 2025 ozone10
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

// This file is part of darkmodelib library.

// Based on the Notepad++ dark mode code licensed under GPLv3.
// Originally by adzm / Adam D. Walling, with modifications by the Notepad++ team.
// Heavily modified by ozone10 (Notepad++ contributor).
// Used with permission to relicense under the Mozilla Public License, v. 2.0.


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
#pragma comment(lib, "Shlwapi.lib")
#endif

#if defined(DMLIB_DLL)
	#if defined(DMLIB_EXPORTS)
		#define DMLIB_API __declspec(dllexport)
	#else
		#define DMLIB_API __declspec(dllimport)
	#endif
#else
	#define DMLIB_API
#endif

typedef struct _TASKDIALOGCONFIG TASKDIALOGCONFIG; // forward declaration, from <CommCtrl.h>

/**
 * @namespace DarkMode
 * @brief Provides dark mode theming, subclassing, and rendering utilities for most Win32 controls.
 */
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
		iniConfigUsed,    ///< True if `.ini` file configuration is supported.
		allowOldOS,       ///< '1' if older Windows 10 versions are allowed, '2' if all older Windows are allowed.
		useDlgProcCtl,    ///< True if WM_CTLCOLORxxx can be handled directly in dialog procedure.
		preferTheme,      ///< True if theme is supported and can be used over subclass, e.g. combo box on Windows 10+.
		useSBFix,         ///< '1' if scroll bar fix is applied to all scroll bars, '2' if scroll bar fix can be limited to specific window.
		maxValue          ///< Sentinel value for internal validation (not intended for use).
	};

	/**
	 * @brief Defines the available dark mode types for manual configurations.
	 *
	 * Can be used in `DarkMode::initDarkModeConfig` and in `DarkMode::setDarkModeConfigEx`
	 * with static_cast<UINT>(DarkModeType::'value').
	 *
	 * @note Also used internally to distinguish between light, dark, and classic modes.
	 *
	 * @see DarkMode::initDarkModeConfig()
	 * @see DarkMode::setDarkModeConfigEx()
	 */
	enum class DarkModeType : unsigned char
	{
		light = 0,  ///< Light mode appearance.
		dark = 1,   ///< Dark mode appearance.
		classic = 3 ///< Classic (non-themed or system) appearance.
	};

#ifdef __cplusplus
	extern "C" {
#endif

	/**
	 * @brief Returns library version information or compile-time feature flags.
	 *
	 * @param libInfoType The type of information to query.
	 * @return Integer representing the requested value or feature flag.
	 *
	 * @see LibInfo
	 */
	[[nodiscard]] int getLibInfo(int libInfoType);

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
	DMLIB_API void initDarkModeConfig(UINT dmType);

	/// Sets the preferred window corner style on Windows 11. (DWM_WINDOW_CORNER_PREFERENCE values)
	DMLIB_API void setRoundCornerConfig(UINT roundCornerStyle);

	/// Sets the preferred border color for window edge on Windows 11.
	DMLIB_API void setBorderColorConfig(COLORREF clr);

	// Sets the Mica effects on Windows 11 setting. (DWM_SYSTEMBACKDROP_TYPE values)
	DMLIB_API void setMicaConfig(UINT mica);

	/// Sets Mica effects on the full window setting.
	DMLIB_API void setMicaExtendedConfig(bool extendMica);

	/// Sets dialog colors on title bar on Windows 11 setting.
	DMLIB_API void setColorizeTitleBarConfig(bool colorize);

	/// Applies dark mode settings based on the given configuration type. (DarkModeType values)
	DMLIB_API void setDarkModeConfigEx(UINT dmType);

	/// Applies dark mode settings based on system mode preference.
	DMLIB_API void setDarkModeConfig();

	/// Initializes dark mode experimental features, colors, and other settings.
	DMLIB_API void initDarkModeEx(const wchar_t* iniName);

	///Initializes dark mode without INI settings.
	DMLIB_API void initDarkMode();

	/// Checks if there is config INI file.
	[[nodiscard]] DMLIB_API bool doesConfigFileExist();

	// ========================================================================
	// Basic checks
	// ========================================================================

	/// Checks if non-classic mode is enabled.
	[[nodiscard]] DMLIB_API bool isEnabled();

	/// Checks if experimental dark mode features are currently active.
	[[nodiscard]] DMLIB_API bool isExperimentalActive();

	/// Checks if experimental dark mode features are supported by the system.
	[[nodiscard]] DMLIB_API bool isExperimentalSupported();

	/// Checks if follow the system mode behavior is enabled.
	[[nodiscard]] DMLIB_API bool isWindowsModeEnabled();

	/// Checks if the host OS is at least Windows 10.
	[[nodiscard]] DMLIB_API bool isAtLeastWindows10();

	/// Checks if the host OS is at least Windows 11.
	[[nodiscard]] DMLIB_API bool isAtLeastWindows11();

	/// Retrieves the current Windows build number.
	[[nodiscard]] DMLIB_API DWORD getWindowsBuildNumber();

	// ========================================================================
	// System Events
	// ========================================================================

	/// Handles system setting changes related to dark mode.
	DMLIB_API bool handleSettingChange(LPARAM lParam);

	/// Checks if dark mode is enabled in the Windows registry.
	[[nodiscard]] DMLIB_API bool isDarkModeReg();

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
	 * @param[in]   nIndex  One of the supported system color indices.
	 * @param[in]   color   Custom `COLORREF` value to apply.
	 */
	DMLIB_API void setSysColor(int nIndex, COLORREF color);

	// ========================================================================
	// Enhancements to DarkMode.h
	// ========================================================================

	/// Makes scroll bars on the specified window and all its children consistent.
	DMLIB_API void enableDarkScrollBarForWindowAndChildren(HWND hWnd);

	// ========================================================================
	// Colors
	// ========================================================================

	/// Sets the color tone and its color set for the active theme.
	DMLIB_API void setColorTone(int colorTone);

	/// Retrieves the currently active color tone for the theme.
	[[nodiscard]] DMLIB_API int getColorTone();

	DMLIB_API COLORREF setBackgroundColor(COLORREF clrNew);
	DMLIB_API COLORREF setCtrlBackgroundColor(COLORREF clrNew);
	DMLIB_API COLORREF setHotBackgroundColor(COLORREF clrNew);
	DMLIB_API COLORREF setDlgBackgroundColor(COLORREF clrNew);
	DMLIB_API COLORREF setErrorBackgroundColor(COLORREF clrNew);

	DMLIB_API COLORREF setTextColor(COLORREF clrNew);
	DMLIB_API COLORREF setDarkerTextColor(COLORREF clrNew);
	DMLIB_API COLORREF setDisabledTextColor(COLORREF clrNew);
	DMLIB_API COLORREF setLinkTextColor(COLORREF clrNew);

	DMLIB_API COLORREF setEdgeColor(COLORREF clrNew);
	DMLIB_API COLORREF setHotEdgeColor(COLORREF clrNew);
	DMLIB_API COLORREF setDisabledEdgeColor(COLORREF clrNew);

	DMLIB_API void setThemeColors(const Colors* colors);
	DMLIB_API void updateThemeBrushesAndPens();

	[[nodiscard]] DMLIB_API COLORREF getBackgroundColor();
	[[nodiscard]] DMLIB_API COLORREF getCtrlBackgroundColor();
	[[nodiscard]] DMLIB_API COLORREF getHotBackgroundColor();
	[[nodiscard]] DMLIB_API COLORREF getDlgBackgroundColor();
	[[nodiscard]] DMLIB_API COLORREF getErrorBackgroundColor();

	[[nodiscard]] DMLIB_API COLORREF getTextColor();
	[[nodiscard]] DMLIB_API COLORREF getDarkerTextColor();
	[[nodiscard]] DMLIB_API COLORREF getDisabledTextColor();
	[[nodiscard]] DMLIB_API COLORREF getLinkTextColor();

	[[nodiscard]] DMLIB_API COLORREF getEdgeColor();
	[[nodiscard]] DMLIB_API COLORREF getHotEdgeColor();
	[[nodiscard]] DMLIB_API COLORREF getDisabledEdgeColor();

	[[nodiscard]] DMLIB_API HBRUSH getBackgroundBrush();
	[[nodiscard]] DMLIB_API HBRUSH getDlgBackgroundBrush();
	[[nodiscard]] DMLIB_API HBRUSH getCtrlBackgroundBrush();
	[[nodiscard]] DMLIB_API HBRUSH getHotBackgroundBrush();
	[[nodiscard]] DMLIB_API HBRUSH getErrorBackgroundBrush();

	[[nodiscard]] DMLIB_API HBRUSH getEdgeBrush();
	[[nodiscard]] DMLIB_API HBRUSH getHotEdgeBrush();
	[[nodiscard]] DMLIB_API HBRUSH getDisabledEdgeBrush();

	[[nodiscard]] DMLIB_API HPEN getDarkerTextPen();
	[[nodiscard]] DMLIB_API HPEN getEdgePen();
	[[nodiscard]] DMLIB_API HPEN getHotEdgePen();
	[[nodiscard]] DMLIB_API HPEN getDisabledEdgePen();

	DMLIB_API COLORREF setViewBackgroundColor(COLORREF clrNew);
	DMLIB_API COLORREF setViewTextColor(COLORREF clrNew);
	DMLIB_API COLORREF setViewGridlinesColor(COLORREF clrNew);

	DMLIB_API COLORREF setHeaderBackgroundColor(COLORREF clrNew);
	DMLIB_API COLORREF setHeaderHotBackgroundColor(COLORREF clrNew);
	DMLIB_API COLORREF setHeaderTextColor(COLORREF clrNew);
	DMLIB_API COLORREF setHeaderEdgeColor(COLORREF clrNew);

	DMLIB_API void setViewColors(const ColorsView* colors);
	DMLIB_API void updateViewBrushesAndPens();

	[[nodiscard]] DMLIB_API COLORREF getViewBackgroundColor();
	[[nodiscard]] DMLIB_API COLORREF getViewTextColor();
	[[nodiscard]] DMLIB_API COLORREF getViewGridlinesColor();

	[[nodiscard]] DMLIB_API COLORREF getHeaderBackgroundColor();
	[[nodiscard]] DMLIB_API COLORREF getHeaderHotBackgroundColor();
	[[nodiscard]] DMLIB_API COLORREF getHeaderTextColor();
	[[nodiscard]] DMLIB_API COLORREF getHeaderEdgeColor();

	[[nodiscard]] DMLIB_API HBRUSH getViewBackgroundBrush();
	[[nodiscard]] DMLIB_API HBRUSH getViewGridlinesBrush();

	[[nodiscard]] DMLIB_API HBRUSH getHeaderBackgroundBrush();
	[[nodiscard]] DMLIB_API HBRUSH getHeaderHotBackgroundBrush();

	[[nodiscard]] DMLIB_API HPEN getHeaderEdgePen();

	/// Initializes default color set based on the current mode type.
	DMLIB_API void setDefaultColors(bool updateBrushesAndOther);

	// ========================================================================
	// Control Subclassing
	// ========================================================================

	/// Applies themed owner drawn subclassing to a checkbox, radio, or tri-state button control.
	DMLIB_API void setCheckboxOrRadioBtnCtrlSubclass(HWND hWnd);
	/// Removes the owner drawn subclass from a a checkbox, radio, or tri-state button control.
	DMLIB_API void removeCheckboxOrRadioBtnCtrlSubclass(HWND hWnd);

	/// Applies owner drawn subclassing to a groupbox button control.
	DMLIB_API void setGroupboxCtrlSubclass(HWND hWnd);
	/// Removes the owner drawn subclass from a groupbox button control.
	DMLIB_API void removeGroupboxCtrlSubclass(HWND hWnd);

	/// Applies owner drawn subclassing and theming to an up-down (spinner) control.
	DMLIB_API void setUpDownCtrlSubclass(HWND hWnd);
	/// Removes the owner drawn subclass from a up-down (spinner) control.
	DMLIB_API void removeUpDownCtrlSubclass(HWND hWnd);

	/// Applies a subclass to detect and subclass tab control's up-down (spinner) child.
	DMLIB_API void setTabCtrlUpDownSubclass(HWND hWnd);
	/// Removes the subclass procedure for a tab control's up-down (spinner) child detection.
	DMLIB_API void removeTabCtrlUpDownSubclass(HWND hWnd);
	/// Applies owner drawn and up-down (spinner) child detection subclassings for a tab control.
	DMLIB_API void setTabCtrlSubclass(HWND hWnd);
	/// Removes owner drawn and up-down (spinner) child detection subclasses.
	DMLIB_API void removeTabCtrlSubclass(HWND hWnd);

	/// Applies owner drawn custom border subclassing to a list box or edit control.
	DMLIB_API void setCustomBorderForListBoxOrEditCtrlSubclass(HWND hWnd);
	/// Removes the custom border subclass from a list box or edit control.
	DMLIB_API void removeCustomBorderForListBoxOrEditCtrlSubclass(HWND hWnd);

	/// Applies owner drawn subclassing to a combo box control.
	DMLIB_API void setComboBoxCtrlSubclass(HWND hWnd);
	/// Removes the owner drawn subclass from a combo box control.
	DMLIB_API void removeComboBoxCtrlSubclass(HWND hWnd);

	/// Applies subclassing to a ComboBoxEx control to handle its child list box and edit controls.
	DMLIB_API void setComboBoxExCtrlSubclass(HWND hWnd);
	/// Removes the child handling subclass from a ComboBoxEx control.
	DMLIB_API void removeComboBoxExCtrlSubclass(HWND hWnd);

	/// Applies subclassing to a list view control to handle custom colors.
	DMLIB_API void setListViewCtrlSubclass(HWND hWnd);
	/// Removes the custom colors handling subclass from a list view control.
	DMLIB_API void removeListViewCtrlSubclass(HWND hWnd);

	/// Applies owner drawn subclassing to a header control.
	DMLIB_API void setHeaderCtrlSubclass(HWND hWnd);
	/// Removes the owner drawn subclass from a header control.
	DMLIB_API void removeHeaderCtrlSubclass(HWND hWnd);

	/// Applies owner drawn subclassing to a status bar control.
	DMLIB_API void setStatusBarCtrlSubclass(HWND hWnd);
	/// Removes the owner drawn subclass from a status bar control.
	DMLIB_API void removeStatusBarCtrlSubclass(HWND hWnd);

	/// Applies owner drawn subclassing to a progress bar control.
	DMLIB_API void setProgressBarCtrlSubclass(HWND hWnd);
	/// Removes the owner drawn subclass from a progress bar control.
	DMLIB_API void removeProgressBarCtrlSubclass(HWND hWnd);

	/// Applies workaround subclassing to a static control to handle visual glitch in disabled state.
	DMLIB_API void setStaticTextCtrlSubclass(HWND hWnd);
	/// Removes the workaround subclass from a static control.
	DMLIB_API void removeStaticTextCtrlSubclass(HWND hWnd);

	/// Applies owner drawn subclassing to a IP address control.
	DMLIB_API void setIPAddressCtrlSubclass(HWND hWnd);
	/// Removes the owner drawn subclass from a IP address control.
	DMLIB_API void removeIPAddressCtrlSubclass(HWND hWnd);

	/// Applies custom color subclassing to a hot key control.
	DMLIB_API void setHotKeyCtrlSubclass(HWND hWnd);
	/// Removes the custom color subclass from a hot key control.
	DMLIB_API void removeHotKeyCtrlSubclass(HWND hWnd);

	// ========================================================================
	// Child Subclassing
	// ========================================================================

	/// Applies theming and/or subclassing to all child controls of a parent window.
	DMLIB_API void setChildCtrlsSubclassAndThemeEx(HWND hParent, bool subclass, bool theme);
	/// Wrapper for `DarkMode::setChildCtrlsSubclassAndThemeEx`.
	DMLIB_API void setChildCtrlsSubclassAndTheme(HWND hParent);
	/// Applies theming to all child controls of a parent window.
	DMLIB_API void setChildCtrlsTheme(HWND hParent);

	// ========================================================================
	// Window, Parent, And Other Subclassing
	// ========================================================================

	/// Applies window subclassing to handle `WM_ERASEBKGND` message.
	DMLIB_API void setWindowEraseBgSubclass(HWND hWnd);
	/// Removes the subclass used for `WM_ERASEBKGND` message handling.
	DMLIB_API void removeWindowEraseBgSubclass(HWND hWnd);

	/// Applies window subclassing to handle `WM_CTLCOLOR*` messages.
	DMLIB_API void setWindowCtlColorSubclass(HWND hWnd);
	/// Removes the subclass used for `WM_CTLCOLOR*` messages handling.
	DMLIB_API void removeWindowCtlColorSubclass(HWND hWnd);

	/// Applies window subclassing for handling `NM_CUSTOMDRAW` notifications for custom drawing.
	DMLIB_API void setWindowNotifyCustomDrawSubclass(HWND hWnd);
	/// Removes the subclass used for handling `NM_CUSTOMDRAW` notifications for custom drawing.
	DMLIB_API void removeWindowNotifyCustomDrawSubclass(HWND hWnd);

	/// Applies window subclassing for menu bar themed custom drawing.
	DMLIB_API void setWindowMenuBarSubclass(HWND hWnd);
	/// Removes the subclass used for menu bar themed custom drawing.
	DMLIB_API void removeWindowMenuBarSubclass(HWND hWnd);

	/// Applies window subclassing to handle `WM_SETTINGCHANGE` message.
	DMLIB_API void setWindowSettingChangeSubclass(HWND hWnd);
	/// Removes the subclass used for `WM_SETTINGCHANGE` message handling.
	DMLIB_API void removeWindowSettingChangeSubclass(HWND hWnd);

	// ========================================================================
	// Theme And Helpers
	// ========================================================================

	/// Configures the SysLink control to be affected by `WM_CTLCOLORSTATIC` message.
	DMLIB_API void enableSysLinkCtrlCtlColor(HWND hWnd);

	/// Sets dark title bar and optional Windows 11 features.
	DMLIB_API void setDarkTitleBarEx(HWND hWnd, bool useWin11Features);
	/// Sets dark mode title bar on supported Windows versions.
	DMLIB_API void setDarkTitleBar(HWND hWnd);

	/// Applies an experimental visual style to the specified window, if supported.
	DMLIB_API void setDarkThemeExperimentalEx(HWND hWnd, const wchar_t* themeClassName);
	/// Applies an experimental Explorer visual style to the specified window, if supported.
	DMLIB_API void setDarkThemeExperimental(HWND hWnd);
	/// Applies "DarkMode_Explorer" visual style if experimental mode is active.
	DMLIB_API void setDarkExplorerTheme(HWND hWnd);
	/// Applies "DarkMode_Explorer" visual style to scroll bars.
	DMLIB_API void setDarkScrollBar(HWND hWnd);
	/// Applies "DarkMode_Explorer" visual style to tooltip controls based on context.
	DMLIB_API void setDarkTooltips(HWND hWnd, int tooltipType);
	/// Applies "DarkMode_DarkTheme" visual style if supported and experimental mode is active.
	DMLIB_API void setDarkThemeTheme(HWND hWnd);

	/// Sets the color of line above a toolbar control for non-classic mode.
	DMLIB_API void setDarkLineAbovePanelToolbar(HWND hWnd);
	/// Applies an experimental Explorer visual style to a list view.
	DMLIB_API void setDarkListView(HWND hWnd);
	/// Replaces default list view checkboxes with themed dark-mode versions on Windows 11.
	DMLIB_API void setDarkListViewCheckboxes(HWND hWnd);
	/// Sets colors and edges for a RichEdit control.
	DMLIB_API void setDarkRichEdit(HWND hWnd);

	/// Applies visual styles; ctl color message and child controls subclassings to a window safely.
	DMLIB_API void setDarkWndSafeEx(HWND hWnd, bool useWin11Features);
	/// Applies visual styles; ctl color message and child controls subclassings with Windows 11 features.
	DMLIB_API void setDarkWndSafe(HWND hWnd);
	/// Applies visual styles; ctl color message, child controls, custom drawing, and setting change subclassings to a window safely.
	DMLIB_API void setDarkWndNotifySafeEx(HWND hWnd, bool setSettingChangeSubclass, bool useWin11Features);
	/// Applies visual styles; ctl color message, child controls, and custom drawing subclassings with Windows 11 features.
	DMLIB_API void setDarkWndNotifySafe(HWND hWnd);

	/// Enables or disables theme-based dialog background textures in classic mode.
	DMLIB_API void enableThemeDialogTexture(HWND hWnd, bool theme);

	/// Enables or disables visual styles for a window.
	DMLIB_API void disableVisualStyle(HWND hWnd, bool doDisable);

	/// Calculates perceptual lightness of a COLORREF color.
	[[nodiscard]] DMLIB_API double calculatePerceivedLightness(COLORREF clr);

	/// Retrieves the current TreeView style configuration.
	[[nodiscard]] DMLIB_API int getTreeViewStyle();

	/// Determines appropriate TreeView style based on background perceived lightness.
	DMLIB_API void calculateTreeViewStyle();

	/// (Re)applies the appropriate window theme style to the specified TreeView.
	DMLIB_API void setTreeViewWindowThemeEx(HWND hWnd, bool force);
	/// Applies the appropriate window theme style to the specified TreeView.
	DMLIB_API void setTreeViewWindowTheme(HWND hWnd);

	/// Retrieves the previous TreeView style configuration.
	[[nodiscard]] DMLIB_API int getPrevTreeViewStyle();

	/// Stores the current TreeView style as the previous style for later comparison.
	DMLIB_API void setPrevTreeViewStyle();

	/// Checks whether the current theme is dark.
	[[nodiscard]] DMLIB_API bool isThemeDark();

	/// Checks whether the color is dark.
	[[nodiscard]] DMLIB_API bool isColorDark(COLORREF clr);

	/// Forces a window to redraw its non-client frame.
	DMLIB_API void redrawWindowFrame(HWND hWnd);
	/// Sets a window's standard style flags and redraws window if needed.
	DMLIB_API void setWindowStyle(HWND hWnd, bool setStyle, LONG_PTR styleFlag);
	/// Sets a window's extended style flags and redraws window if needed.
	DMLIB_API void setWindowExStyle(HWND hWnd, bool setExStyle, LONG_PTR exStyleFlag);
	/// Replaces an extended edge (e.g. client edge) with a standard window border.
	DMLIB_API void replaceExEdgeWithBorder(HWND hWnd, bool replace, LONG_PTR exStyleFlag);
	/// Safely toggles `WS_EX_CLIENTEDGE` with `WS_BORDER` based on dark mode state.
	DMLIB_API void replaceClientEdgeWithBorderSafe(HWND hWnd);

	/// Applies classic-themed styling to a progress bar in non-classic mode.
	DMLIB_API void setProgressBarClassicTheme(HWND hWnd);

	// ========================================================================
	// Ctl Color
	// ========================================================================

	/// Handles text and background colorizing for read-only controls.
	[[nodiscard]] DMLIB_API LRESULT onCtlColor(HDC hdc);

	/// Handles text and background colorizing for interactive controls.
	[[nodiscard]] DMLIB_API LRESULT onCtlColorCtrl(HDC hdc);

	/// Handles text and background colorizing for window and disabled non-text controls.
	[[nodiscard]] DMLIB_API LRESULT onCtlColorDlg(HDC hdc);

	/// Handles text and background colorizing for error state (for specific usage).
	[[nodiscard]] DMLIB_API LRESULT onCtlColorError(HDC hdc);

	/// Handles text and background colorizing for static text controls.
	[[nodiscard]] DMLIB_API LRESULT onCtlColorDlgStaticText(HDC hdc, bool isTextEnabled);

	/// Handles text and background colorizing for SysLink controls.
	[[nodiscard]] DMLIB_API LRESULT onCtlColorDlgLinkText(HDC hdc, bool isTextEnabled);

	/// Handles text and background colorizing for list box controls.
	[[nodiscard]] DMLIB_API LRESULT onCtlColorListbox(WPARAM wParam, LPARAM lParam);

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
	 * @param[in]   hWnd        Handle to the dialog window.
	 * @param[in]   uMsg        Message identifier.
	 * @param[in]   wParam      First message parameter (unused).
	 * @param[in]   lParam      Second message parameter (unused).
	 * @return UINT_PTR A value defined by the hook procedure.
	 */
	DMLIB_API UINT_PTR CALLBACK HookDlgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

	/// Applies dark mode visual styles to task dialog.
	DMLIB_API void setDarkTaskDlg(HWND hWnd);

	/// Simple task dialog callback procedure to enable dark mode support.
	DMLIB_API HRESULT CALLBACK DarkTaskDlgCallback(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, LONG_PTR lpRefData);

	/**
	 * @brief Wrapper for `TaskDialogIndirect` with dark mode support.
	 *
	 * Parameters are same as for `TaskDialogIndirect`.
	 * Should be used with `DarkMode::setDarkTaskDlg`
	 * used in task dialog callback procedure.
	 *
	 * ## Example of Callback Procedure
	 * ```cpp
	 * static HRESULT CALLBACK DarkTaskDlgCallback(
	 *     HWND hWnd,
	 *     UINT uMsg,
	 *     [[maybe_unused]] WPARAM wParam,
	 *     [[maybe_unused]] LPARAM lParam,
	 *     [[maybe_unused]] LONG_PTR lpRefData
	 * )
	 * {
	 *     if (uMsg == TDN_DIALOG_CONSTRUCTED)
	 *     {
	 *          DarkMode::setDarkTaskDlg(hWnd);
	 *     }
	 *     return S_OK;
	 * }
	 * ```
	 *
	 * @see DarkMode::DarkTaskDlgCallback()
	 * @see DarkMode::setDarkTaskDlg()
	 */
	DMLIB_API HRESULT darkTaskDialogIndirect(const TASKDIALOGCONFIG* pTaskConfig, int* pnButton, int* pnRadioButton, BOOL* pfVerificationFlagChecked);

	/**
	 * @brief Displays a message box as task dialog with themed styling.
	 */
	DMLIB_API HRESULT darkMessageBoxW(HWND hWnd, LPCWSTR lpText, LPCWSTR lpCaption, UINT uType);

#ifdef __cplusplus
	} // extern "C"
#endif

} // namespace DarkMode

#else
#define _DARKMODELIB_NOT_USED
#endif // (NTDDI_VERSION >= NTDDI_VISTA) //&& (x64 or arm64)
