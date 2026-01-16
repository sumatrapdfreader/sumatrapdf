// SPDX-License-Identifier: MPL-2.0

/*
 * Copyright (c) 2025 ozone10
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

// This file is part of darkmodelib library.


#pragma once

#include <windows.h>

#include <vsstyle.h>

#include <climits>

#include "DmlibDpi.h"
#include "DmlibPaintHelper.h"
#include "DmlibSubclass.h"
#include "DmlibWinApi.h"

namespace DarkMode
{
	/// Checks if current mode is dark type.
	[[nodiscard]] bool isDarkDmTypeUsed() noexcept;
}

namespace dmlib_subclass
{
	/**
	 * @struct ButtonData
	 * @brief Stores button theming state and original size metadata.
	 *
	 * Used for checkbox, radio, tri-state, or group box buttons. Used in conjunction
	 * with subclassing of button controls to preserve original layout dimensions
	 * and apply consistent visual styling. Captures the control's client size
	 * for checkbox, radio, or tri-state buttons.
	 *
	 * Members:
	 * - `m_themeData` : RAII-managed theme handle for `VSCLASS_BUTTON`.
	 * - `m_szBtn` : Original size extracted from the button rectangle.
	 * - `m_iStateID` : Current visual state ID (e.g. pressed, disabled, ...).
	 * - `m_isSizeSet` : Indicates whether `m_szBtn` holds a valid measurement.
	 *
	 * Constructor behavior:
	 * - When constructed with an `HWND`, attempts to extract the initial size if the button
	 *   is a checkbox/radio/tri-state type without `BS_MULTILINE`.
	 *
	 * @see ThemeData
	 */
	struct ButtonData
	{
		ThemeData m_themeData{ VSCLASS_BUTTON };
		SIZE m_szBtn{};

		int m_iStateID = 0;
		bool m_isSizeSet = false;

		ButtonData() = default;

		// Saves width and height from the resource file for use as restrictions.
		// Currently unused / have no effect.
		explicit ButtonData(HWND hWnd) noexcept
		{
			const auto nBtnStyle = ::GetWindowLongPtrW(hWnd, GWL_STYLE);
			switch (nBtnStyle & BS_TYPEMASK)
			{
				case BS_CHECKBOX:
				case BS_AUTOCHECKBOX:
				case BS_3STATE:
				case BS_AUTO3STATE:
				case BS_RADIOBUTTON:
				case BS_AUTORADIOBUTTON:
				{
					if ((nBtnStyle & BS_MULTILINE) != BS_MULTILINE)
					{
						RECT rcBtn{};
						::GetClientRect(hWnd, &rcBtn);
						const UINT dpi = dmlib_dpi::GetDpiForParent(hWnd);
						m_szBtn.cx = dmlib_dpi::unscale(rcBtn.right - rcBtn.left, dpi);
						m_szBtn.cy = dmlib_dpi::unscale(rcBtn.bottom - rcBtn.top, dpi);
						m_isSizeSet = (m_szBtn.cx != 0 && m_szBtn.cy != 0);
					}
					break;
				}

				default:
				{
					break;
				}
			}
		}
	};

	/**
	 * @struct UpDownData
	 * @brief Stores layout and state for a owner drawn up-down (spinner) control.
	 *
	 * Used to manage rectangle, buffer, and hit-test regions for owner-drawn subclassed
	 * up-down controls, supporting both vertical and horizontal layouts.
	 *
	 * Members:
	 * - `m_bufferData`: Buffer wrapper for flicker-free custom painting.
	 * - `m_rcClient`: Current client rectangle of the control.
	 * - `m_rcPrev`, `m_rcNext`: Rectangles for the up/down or left/right arrow buttons.
	 * - `m_cornerRoundness`: Optional roundness for corners (used in Windows 11+ with tabs).
	 * - `m_isHorizontal`: `true` if the control is horizontal (`UDS_HORZ` style).
	 * - `m_wasHotNext`: Last hover state (used for hover feedback).
	 *
	 * Constructor behavior:
	 * - Detects orientation from `GWL_STYLE`.
	 * - Initializes corner styling based on OS and parent class.
	 * - Extracts rectangles for arrow segments immediately.
	 *
	 * Usage:
	 * - `updateRect(HWND)`: Refreshes rectangle from control handle.
	 * - `updateRect(RECT)`: Checks for rectangle change and updates it.
	 *
	 * @see ThemeData
	 * @see BufferData
	 */
	struct UpDownData
	{
		ThemeData m_themeData{ VSCLASS_SPIN };
		BufferData m_bufferData;

		RECT m_rcClient{};
		RECT m_rcPrev{};
		RECT m_rcNext{};
		int m_cornerRoundness = 0;
		bool m_isHorizontal = false;
		bool m_wasHotNext = false;

		UpDownData() = delete;

		explicit UpDownData(HWND hWnd)
			: m_cornerRoundness(
				(dmlib_win32api::IsWindows11()
					&& dmlib_subclass::cmpWndClassName(::GetParent(hWnd), WC_TABCONTROL))
				? (dmlib_paint::kWin11CornerRoundness + 1)
				: 0)
			, m_isHorizontal((::GetWindowLongPtrW(hWnd, GWL_STYLE)& UDS_HORZ) == UDS_HORZ)
		{
			updateRect(hWnd);
		}

		void updateRectUpDown() noexcept
		{
			if (m_isHorizontal)
			{
				const RECT rcArrowLeft{
					m_rcClient.left, m_rcClient.top,
					m_rcClient.right - ((m_rcClient.right - m_rcClient.left) / 2), m_rcClient.bottom
				};

				const RECT rcArrowRight{
					rcArrowLeft.right, m_rcClient.top,
					m_rcClient.right, m_rcClient.bottom
				};

				m_rcPrev = rcArrowLeft;
				m_rcNext = rcArrowRight;
			}
			else
			{
				static constexpr LONG offset = 2;

				const RECT rcArrowTop{
					m_rcClient.left + offset, m_rcClient.top,
					m_rcClient.right, m_rcClient.bottom - ((m_rcClient.bottom - m_rcClient.top) / 2)
				};

				const RECT rcArrowBottom{
					m_rcClient.left + offset, rcArrowTop.bottom,
					m_rcClient.right, m_rcClient.bottom
				};

				m_rcPrev = rcArrowTop;
				m_rcNext = rcArrowBottom;
			}
		}

		void updateRect(HWND hWnd) noexcept
		{
			::GetClientRect(hWnd, &m_rcClient);
			updateRectUpDown();
		}

		bool updateRect(RECT rcClientNew) noexcept
		{
			if (::EqualRect(&m_rcClient, &rcClientNew) == FALSE)
			{
				m_rcClient = rcClientNew;
				updateRectUpDown();
				return true;
			}
			return false;
		}
	};

	/**
	 * @struct TabData
	 * @brief Simple wrapper for `BufferData`.
	 *
	 * Members:
	 * - `m_bufferData` : Buffer wrapper for flicker-free custom painting.
	 *
	 * @see BufferData
	 */
	struct TabData
	{
		BufferData m_bufferData;
	};

	/**
	 * @struct BorderMetricsData
	 * @brief Stores system border and scroll bar metrics.
	 *
	 * Captures system metrics related to edit or list box control borders and scroll bars,
	 * along with the current DPI setting and a hot state flag.
	 *
	 * Members:
	 * - `m_dpi` : Current DPI value (defaults to `USER_DEFAULT_SCREEN_DPI`).
	 * - `m_xEdge` : Width of a border (`SM_CXEDGE`).
	 * - `m_yEdge` : Height of a border (`SM_CYEDGE`).
	 * - `m_xScroll` : Width of a vertical scroll bar (`SM_CXVSCROLL`).
	 * - `m_yScroll` : Height of a horizontal scroll bar (`SM_CYVSCROLL`).
	 * - `m_isHot` : Indicates whether the border is in a "hot" (hovered) state.
	 *
	 * @note Values are initialized from `GetSystemMetrics()` at construction time.
	 *       Currently there is no dynamic handling for dpi changes.
	 */
	struct BorderMetricsData
	{
		UINT m_dpi = USER_DEFAULT_SCREEN_DPI;
		LONG m_xEdge = ::GetSystemMetrics(SM_CXEDGE);
		LONG m_yEdge = ::GetSystemMetrics(SM_CYEDGE);
		LONG m_xScroll = ::GetSystemMetrics(SM_CXVSCROLL);
		LONG m_yScroll = ::GetSystemMetrics(SM_CYVSCROLL);
		bool m_isHot = false;

		BorderMetricsData() = delete;

		explicit BorderMetricsData(HWND hWnd) noexcept
		{
			setMetricsForDpi(dmlib_dpi::GetDpiForParent(hWnd));
		}

		void setMetricsForDpi(UINT dpi) noexcept
		{
			m_dpi = dpi;
			m_xEdge = dmlib_dpi::GetSystemMetricsForDpi(SM_CXEDGE, m_dpi);
			m_yEdge = dmlib_dpi::GetSystemMetricsForDpi(SM_CYEDGE, m_dpi);
			m_xScroll = dmlib_dpi::GetSystemMetricsForDpi(SM_CXVSCROLL, m_dpi);
			m_yScroll = dmlib_dpi::GetSystemMetricsForDpi(SM_CYVSCROLL, m_dpi);
		}
	};

	/**
	 * @struct ComboBoxData
	 * @brief Stores theme and buffer data for a combo box control, along with its style.
	 *
	 * Used to manage theming and double-buffered painting for combo box controls.
	 * Holds both the visual style information and the control's creation style for
	 * conditional drawing logic.
	 *
	 * Members:
	 * - `m_themeData` : RAII-managed theme handle for `VSCLASS_COMBOBOX`.
	 * - `m_bufferData` : Buffer wrapper for flicker-free custom painting.
	 * - `m_cbStyle` : Combo box style flags (`CBS_SIMPLE`, `CBS_DROPDOWN`, `CBS_DROPDOWNLIST`).
	 *
	 * Constructor behavior:
	 * - Deleted default constructor to enforce explicit style initialization.
	 * - Explicit constructor taking `cbStyle` to set `m_cbStyle`.
	 *
	 * @note The style value is typically retrieved via `GetWindowLongPtr(hWnd, GWL_STYLE)`
	 *       when subclassing the combo box.
	 *
	 * @see ThemeData
	 * @see BufferData
	 */
	struct ComboBoxData
	{
		ThemeData m_themeData{ VSCLASS_COMBOBOX };
		BufferData m_bufferData;

		LONG_PTR m_cbStyle = CBS_SIMPLE;

		ComboBoxData() = delete;

		explicit ComboBoxData(LONG_PTR cbStyle) noexcept
			: m_cbStyle(cbStyle)
		{}
	};

	/**
	 * @struct HeaderData
	 * @brief Stores theme, buffer, and font data for a header control, along with its style and state information.
	 *
	 * Used to manage theming and double-buffered painting for header controls.
	 * Holds the button visual style information and the control's state for
	 * conditional drawing logic.
	 *
	 * Members:
	 * - `m_themeData` : RAII-managed theme handle for `VSCLASS_HEADER`.
	 * - `m_bufferData` : Buffer wrapper for flicker-free custom painting.
	 * - `m_fontData` : Font resource wrapper for text drawing.
	 * - `m_pt` : Last known mouse position in client coordinates (LONG_MIN if uninitialized).
	 * - `m_isHot` : True if the mouse is currently over a header item.
	 * - `m_hasBtnStyle` : True if the header uses button-style items (`HDF_BUTTON`).
	 * - `m_isPressed` : True if a header item is currently pressed.
	 *
	 * Constructor behavior:
	 * - Deleted default constructor to enforce explicit initialization.
	 * - Explicit constructor taking `hasBtnStyle` to set `m_hasBtnStyle`.
	 *
	 * @see ThemeData
	 * @see BufferData
	 * @see FontData
	 */
	struct HeaderData
	{
		ThemeData m_themeData{ VSCLASS_HEADER };
		BufferData m_bufferData;
		FontData m_fontData{ nullptr };

		POINT m_pt{ LONG_MIN, LONG_MIN };
		bool m_isHot = false;
		bool m_hasBtnStyle = true;
		bool m_isPressed = false;

		HeaderData() = delete;

		explicit HeaderData(bool hasBtnStyle) noexcept
			: m_hasBtnStyle(hasBtnStyle)
		{}
	};

	/**
	 * @struct StatusBarData
	 * @brief Stores theme, buffer, and font data for a status bar control.
	 *
	 * Used to manage theming and double-buffered painting for status bar controls.
	 *
	 * Members:
	 * - `m_themeData` : RAII-managed theme handle for `VSCLASS_HEADER`.
	 * - `m_bufferData` : Buffer wrapper for flicker-free custom painting.
	 * - `m_fontData` : Font resource wrapper for text drawing.
	 *
	 * Constructor behavior:
	 * - Deleted default constructor to enforce explicit font initialization.
	 * - Explicit constructor taking `HFONT` to initialize `m_fontData`.
	 *
	 * @see ThemeData
	 * @see BufferData
	 * @see FontData
	 */
	struct StatusBarData
	{
		ThemeData m_themeData{ VSCLASS_STATUS };
		BufferData m_bufferData;
		FontData m_fontData;

		StatusBarData() = delete;

		explicit StatusBarData(const HFONT& hFont) noexcept
			: m_fontData(hFont)
		{}
	};

	/**
	 * @struct ProgressBarData
	 * @brief Stores theme and buffer data for a progress bar control, along with its current state.
	 *
	 * Used to manage theming and double-buffered painting for progress bar controls.
	 * Captures the current visual state (normal, paused, error) via `PBM_GETSTATE`.
	 *
	 * Members:
	 * - `m_themeData` : RAII-managed theme handle for `VSCLASS_PROGRESS`.
	 * - `m_bufferData` : Buffer wrapper for flicker-free custom painting.
	 * - `m_iStateID` : Current progress bar state (e.g., `PBFS_NORMAL`, `PBFS_PAUSED`, `PBFS_ERROR`, `PBFS_PARTIAL`).
	 *
	 * Constructor behavior:
	 * - Initializes `m_iStateID` by querying the control with `PBM_GETSTATE`.
	 *
	 * @see ThemeData
	 * @see BufferData
	 */
	struct ProgressBarData
	{
		ThemeData m_themeData{ VSCLASS_PROGRESS };
		BufferData m_bufferData;

		int m_iStateID = PBFS_PARTIAL;

		explicit ProgressBarData(HWND hWnd) noexcept
			: m_iStateID(static_cast<int>(::SendMessage(hWnd, PBM_GETSTATE, 0, 0)))
		{}
	};

	/**
	 * @struct StaticTextData
	 * @brief Stores enabled status information for a static text control.
	 *
	 * Used to determine whether a static control (e.g., label or caption) should be drawn
	 * using enabled or disabled colors.
	 *
	 * Members:
	 * - `m_isEnabled` : Indicates whether the control is currently enabled (`true`) or disabled (`false`).
	 *
	 * Constructor behavior:
	 * - Default constructor initializes `m_isEnabled` to `true`.
	 * - Explicit constructor queries the control's enabled state via `IsWindowEnabled(hWnd)`.
	 */
	struct StaticTextData
	{
		bool m_isEnabled = true;

		StaticTextData() = default;

		explicit StaticTextData(HWND hWnd) noexcept
			: m_isEnabled(::IsWindowEnabled(hWnd) == TRUE)
		{}
	};

	LRESULT CALLBACK ButtonSubclass(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData);
	LRESULT CALLBACK GroupboxSubclass(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData);
	LRESULT CALLBACK UpDownSubclass(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData);
	LRESULT CALLBACK TabPaintSubclass(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData);
	LRESULT CALLBACK TabUpDownSubclass(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData);
	LRESULT CALLBACK CustomBorderSubclass(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData);
	LRESULT CALLBACK ComboBoxSubclass(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData);
	LRESULT CALLBACK ComboBoxExSubclass(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData);
	LRESULT CALLBACK ListViewSubclass(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData);
	LRESULT CALLBACK HeaderSubclass(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData);
	LRESULT CALLBACK StatusBarSubclass(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData);
	LRESULT CALLBACK ProgressBarSubclass(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData);
	LRESULT CALLBACK StaticTextSubclass(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData);
	LRESULT CALLBACK IPAddressSubclass(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData);
	LRESULT CALLBACK HotKeySubclass(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData);
} // namespace dmlib_subclass
