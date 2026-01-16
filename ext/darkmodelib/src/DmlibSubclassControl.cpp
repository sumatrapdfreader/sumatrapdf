// SPDX-License-Identifier: MPL-2.0

/*
 * Copyright (c) 2025 ozone10
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

// This file is part of darkmodelib library.


#include "StdAfx.h"

#include "DmlibSubclassControl.h"

#include <windows.h>

#include <uxtheme.h>
#include <vsstyle.h>
#include <vssym32.h>
#include <windowsx.h>

#include <algorithm>
#include <array>
#include <climits>
#include <memory>
#include <string>

#include "DarkModeSubclass.h"
#include "DmlibDpi.h"
#include "DmlibGlyph.h"
#include "DmlibHook.h"
#include "DmlibPaintHelper.h"
#include "DmlibSubclass.h"

#if defined(__GNUC__)
static constexpr int CP_DROPDOWNITEM = 9; // for some reason mingw use only enum up to 8
#endif

/**
 * @brief Draws a themed owner drawn checkbox, radio, or tri-state button (excluding push-like buttons).
 *
 * Internally used by @ref paintButton to draw visual elements such as checkbox glyphs
 * or radio indicators alongside styled text. Not used for buttons with `BS_PUSHLIKE`,
 * which require different handling and theming logic.
 *
 * - Retrieves themed or fallback font for consistent appearance.
 * - Handles alignment, word wrapping, and prefix visibility per style flags.
 * - Draws themed background and glyph using `DrawThemeBackground`.
 * - Uses themed text drawing and applies focus cue when needed.
 *
 * @param[in]   hWnd        Handle to the button control.
 * @param[in]   hdc         Device context for drawing.
 * @param[in]   hTheme      Active visual style theme handle.
 * @param[in]   iPartID     Part ID (`BP_CHECKBOX`, `BP_RADIOBUTTON`, etc.).
 * @param[in]   iStateID    State ID (`CBS_CHECKEDHOT`, `RBS_UNCHECKEDNORMAL`, etc.).
 *
 * @see paintButton()
 */
static void renderButton(
	HWND hWnd,
	HDC hdc,
	HTHEME hTheme,
	int iPartID,
	int iStateID
) noexcept
{
	// Font part

	HFONT hFont = nullptr;
	bool isFontCreated = false;
	LOGFONT lf{};
	if (SUCCEEDED(::GetThemeFont(hTheme, hdc, iPartID, iStateID, TMT_FONT, &lf)))
	{
		hFont = ::CreateFontIndirectW(&lf);
		isFontCreated = true;
	}

	if (hFont == nullptr)
	{
		hFont = reinterpret_cast<HFONT>(::SendMessage(hWnd, WM_GETFONT, 0, 0));
		isFontCreated = false;
	}

	const auto holdFont = dmlib_paint::GdiObject{ hdc, hFont, !isFontCreated };

	// Style part

	const auto nStyle = ::GetWindowLongPtr(hWnd, GWL_STYLE);
	const bool isMultiline = (nStyle & BS_MULTILINE) == BS_MULTILINE;
	const bool isTop = (nStyle & BS_TOP) == BS_TOP;
	const bool isBottom = (nStyle & BS_BOTTOM) == BS_BOTTOM;
	const bool isCenter = (nStyle & BS_CENTER) == BS_CENTER;
	const bool isRight = (nStyle & BS_RIGHT) == BS_RIGHT;
	const bool isVCenter = (nStyle & BS_VCENTER) == BS_VCENTER;

	DWORD dtFlags = DT_LEFT;
	if (isMultiline)
	{
		dtFlags |= DT_WORDBREAK;
	}
	else
	{
		dtFlags |= DT_SINGLELINE;
	}

	if (isCenter)
	{
		dtFlags |= DT_CENTER;
	}
	else if (isRight)
	{
		dtFlags |= DT_RIGHT;
	}

	if (isVCenter || (!isMultiline && !isBottom && !isTop))
	{
		dtFlags |= DT_VCENTER;
	}
	else if (isBottom)
	{
		dtFlags |= DT_BOTTOM;
	}

	const auto uiState = static_cast<DWORD>(::SendMessage(hWnd, WM_QUERYUISTATE, 0, 0));

	// hide prefix
	if ((uiState & UISF_HIDEACCEL) == UISF_HIDEACCEL)
	{
		dtFlags |= DT_HIDEPREFIX;
	}

	// Text and box part

	RECT rcClient{};
	::GetClientRect(hWnd, &rcClient);

	std::wstring buffer;
	const auto bufferLen = static_cast<size_t>(::GetWindowTextLengthW(hWnd));
	buffer.resize(bufferLen + 1, L'\0');
	::GetWindowTextW(hWnd, buffer.data(), static_cast<int>(buffer.length()));

	SIZE szBox{};
	::GetThemePartSize(hTheme, hdc, iPartID, iStateID, nullptr, TS_DRAW, &szBox);

	RECT rcText{};
	::GetThemeBackgroundContentRect(hTheme, hdc, iPartID, iStateID, &rcClient, &rcText);

	RECT rcBackground{ rcClient };
	if (!isMultiline)
	{
		rcBackground.top += (rcText.bottom - rcText.top - szBox.cy) / 2;
	}
	rcBackground.bottom = rcBackground.top + szBox.cy;
	rcBackground.right = rcBackground.left + szBox.cx;
	rcText.left = rcBackground.right + 3;

	::DrawThemeParentBackground(hWnd, hdc, &rcClient);
	::DrawThemeBackground(hTheme, hdc, iPartID, iStateID, &rcBackground, nullptr); // draw box

	DTTOPTS dtto{};
	dtto.dwSize = sizeof(DTTOPTS);
	dtto.dwFlags = DTT_TEXTCOLOR;
	dtto.crText = (::IsWindowEnabled(hWnd) == FALSE) ? DarkMode::getDisabledTextColor() : DarkMode::getTextColor();

	::DrawThemeTextEx(hTheme, hdc, iPartID, iStateID, buffer.c_str(), -1, dtFlags, &rcText, &dtto);

	// Focus rect

	const auto nState = static_cast<DWORD>(::SendMessage(hWnd, BM_GETSTATE, 0, 0));
	if (((nState & BST_FOCUS) == BST_FOCUS) && ((uiState & UISF_HIDEFOCUS) != UISF_HIDEFOCUS))
	{
		dtto.dwFlags |= DTT_CALCRECT;
		::DrawThemeTextEx(hTheme, hdc, iPartID, iStateID, buffer.c_str(), -1, dtFlags | DT_CALCRECT, &rcText, &dtto);
		const RECT rcFocus{ rcText.left - 1, rcText.top, rcText.right + 1, rcText.bottom + 1 };
		::DrawFocusRect(hdc, &rcFocus);
	}
}

/**
 * @brief Paints a checkbox, radio, or tri-state button with state-based visuals.
 *
 * Determines the appropriate themed part and state ID based on the control's
 * style (e.g. `BS_CHECKBOX`, `BS_RADIOBUTTON`) and current button state flags
 * such as `BST_CHECKED`, `BST_PUSHED`, or `BST_HOT`.
 *
 * Paint logic:
 * - Uses buffered animation (if available) to smoothly transition between states.
 * - Falls back to direct drawing via @ref renderButton if animation is not used.
 * - Internally updates the `buttonData.m_iStateID` to preserve the last rendered state.
 * - Not used for `BS_PUSHLIKE` buttons.
 *
 * @param[in]       hWnd        Handle to the checkbox or radio button control.
 * @param[in]       hdc         Device context used for drawing.
 * @param[in,out]   buttonData  Theming and state info, including current theme and last state.
 *
 * @see renderButton()
 */
static void paintButton(HWND hWnd, HDC hdc, dmlib_subclass::ButtonData& buttonData) noexcept
{
	const auto& hTheme = buttonData.m_themeData.getHTheme();

	const auto nState = static_cast<DWORD>(::SendMessage(hWnd, BM_GETSTATE, 0, 0));
	const auto nStyle = ::GetWindowLongPtr(hWnd, GWL_STYLE);
	const auto nBtnStyle = nStyle & BS_TYPEMASK;

	int iPartID = 0;
	int iStateID = 0;

	static constexpr int checkedOffset = 4;
	static constexpr int mixedOffset = 8;

	// Get style
	switch (nBtnStyle)
	{
		case BS_CHECKBOX:
		case BS_AUTOCHECKBOX:
		case BS_3STATE:
		case BS_AUTO3STATE:
		{
			iPartID = BP_CHECKBOX;

			if (::IsWindowEnabled(hWnd) == FALSE)
			{
				iStateID = CBS_UNCHECKEDDISABLED;
			}
			else if ((nState & BST_PUSHED) == BST_PUSHED)
			{
				iStateID = CBS_UNCHECKEDPRESSED;
			}
			else if ((nState & BST_HOT) == BST_HOT)
			{
				iStateID = CBS_UNCHECKEDHOT;
			}
			else
			{
				iStateID = CBS_UNCHECKEDNORMAL;
			}

			if ((nState & BST_CHECKED) == BST_CHECKED)
			{
				iStateID += checkedOffset;
			}
			else if ((nState & BST_INDETERMINATE) == BST_INDETERMINATE)
			{
				iStateID += mixedOffset;
			}

			break;
		}

		case BS_RADIOBUTTON:
		case BS_AUTORADIOBUTTON:
		{
			iPartID = BP_RADIOBUTTON;

			if (::IsWindowEnabled(hWnd) == FALSE)
			{
				iStateID = RBS_UNCHECKEDDISABLED;
			}
			else if ((nState & BST_PUSHED) == BST_PUSHED)
			{
				iStateID = RBS_UNCHECKEDPRESSED;
			}
			else if ((nState & BST_HOT) == BST_HOT)
			{
				iStateID = RBS_UNCHECKEDHOT;
			}
			else
			{
				iStateID = RBS_UNCHECKEDNORMAL;
			}

			if ((nState & BST_CHECKED) == BST_CHECKED)
			{
				iStateID += 4;
			}

			break;
		}

		default: // should never happen
		{
			iPartID = BP_CHECKBOX;
			iStateID = CBS_UNCHECKEDDISABLED;
			break;
		}
	}

	if (::BufferedPaintRenderAnimation(hWnd, hdc) == TRUE)
	{
		return;
	}

	// Animation part - hover transition

	BP_ANIMATIONPARAMS animParams{};
	animParams.cbSize = sizeof(BP_ANIMATIONPARAMS);
	animParams.style = BPAS_LINEAR;
	if (iStateID != buttonData.m_iStateID)
	{
		::GetThemeTransitionDuration(hTheme, iPartID, buttonData.m_iStateID, iStateID, TMT_TRANSITIONDURATIONS, &animParams.dwDuration);
	}

	RECT rcClient{};
	::GetClientRect(hWnd, &rcClient);

	HDC hdcFrom = nullptr;
	HDC hdcTo = nullptr;
	HANIMATIONBUFFER hbpAnimation = ::BeginBufferedAnimation(hWnd, hdc, &rcClient, BPBF_COMPATIBLEBITMAP, nullptr, &animParams, &hdcFrom, &hdcTo);
	if (hbpAnimation != nullptr)
	{
		if (hdcFrom != nullptr)
		{
			renderButton(hWnd, hdcFrom, hTheme, iPartID, buttonData.m_iStateID);
		}
		if (hdcTo != nullptr)
		{
			renderButton(hWnd, hdcTo, hTheme, iPartID, iStateID);
		}

		buttonData.m_iStateID = iStateID;

		::EndBufferedAnimation(hbpAnimation, TRUE);
	}
	else
	{
		renderButton(hWnd, hdc, hTheme, iPartID, iStateID);

		buttonData.m_iStateID = iStateID;
	}
}

/**
 * @brief Window subclass procedure for themed owner drawn checkbox, radio, and tri-state buttons.
 *
 * @param[in]   hWnd        Window handle being subclassed.
 * @param[in]   uMsg        Message identifier.
 * @param[in]   wParam      Message-specific data.
 * @param[in]   lParam      Message-specific data.
 * @param[in]   uIdSubclass Subclass identifier.
 * @param[in]   dwRefData   ButtonData instance.
 * @return LRESULT Result of message processing.
 *
 * @see paintButton()
 * @see DarkMode::setCheckboxOrRadioBtnCtrlSubclass()
 * @see DarkMode::removeCheckboxOrRadioBtnCtrlSubclass()
 */
LRESULT CALLBACK dmlib_subclass::ButtonSubclass(
	HWND hWnd,
	UINT uMsg,
	WPARAM wParam,
	LPARAM lParam,
	UINT_PTR uIdSubclass,
	DWORD_PTR dwRefData
)
{
	auto* pButtonData = reinterpret_cast<ButtonData*>(dwRefData);
	auto& themeData = pButtonData->m_themeData;

	switch (uMsg)
	{
		case WM_NCDESTROY:
		{
			::RemoveWindowSubclass(hWnd, ButtonSubclass, uIdSubclass);
			std::unique_ptr<ButtonData> u_u_ptrData(pButtonData);
			u_u_ptrData.reset(nullptr);
			break;
		}

		case WM_ERASEBKGND:
		{
			if (!DarkMode::isEnabled() || !themeData.ensureTheme(hWnd))
			{
				break;
			}
			return TRUE;
		}

		case WM_PRINTCLIENT:
		case WM_PAINT:
		{
			if (!DarkMode::isEnabled() || !themeData.ensureTheme(hWnd))
			{
				break;
			}

			PAINTSTRUCT ps{};
			auto hdc = reinterpret_cast<HDC>(wParam);
			if (hdc == nullptr)
			{
				hdc = ::BeginPaint(hWnd, &ps);
			}

			paintButton(hWnd, hdc, *pButtonData);

			if (ps.hdc != nullptr)
			{
				::EndPaint(hWnd, &ps);
			}

			return 0;
		}

		case WM_DPICHANGED_AFTERPARENT:
		{
			themeData.closeTheme();
			if (pButtonData->m_isSizeSet)
			{
				if (SIZE szBtn{};
					Button_GetIdealSize(hWnd, &szBtn) == TRUE)
				{
					const UINT dpi = dmlib_dpi::GetDpiForParent(hWnd);
					const int cx = std::min<LONG>(szBtn.cx, dmlib_dpi::scale(pButtonData->m_szBtn.cx, dpi));
					const int cy = std::min<LONG>(szBtn.cy, dmlib_dpi::scale(pButtonData->m_szBtn.cy, dpi));
					::SetWindowPos(hWnd, nullptr, 0, 0, cx, cy, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOOWNERZORDER);
				}
			}
			return 0;
		}

		case WM_THEMECHANGED:
		{
			themeData.closeTheme();
			break;
		}

		case WM_SIZE:
		case WM_DESTROY:
		{
			::BufferedPaintStopAllAnimations(hWnd);
			break;
		}

		case WM_ENABLE:
		{
			if (!DarkMode::isEnabled())
			{
				break;
			}

			// Skip the button's normal wndproc so it won't redraw out of wm_paint
			const LRESULT retVal = ::DefWindowProcW(hWnd, uMsg, wParam, lParam);
			::InvalidateRect(hWnd, nullptr, FALSE);
			return retVal;
		}

		case WM_UPDATEUISTATE:
		{
			if ((HIWORD(wParam) & (UISF_HIDEACCEL | UISF_HIDEFOCUS)) != 0)
			{
				::InvalidateRect(hWnd, nullptr, FALSE);
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
 * @brief Paints a group box frame and text with custom colors.
 *
 * Handles drawing a themed group box with optional centered text, styled borders,
 * and font fallback. If a caption text is present, the frame is clipped to avoid overdrawing
 * behind the text. The function adapts layout for both centered and left-aligned titles.
 *
 * Paint logic:
 * - Determines current visual state (`GBS_DISABLED`, `GBS_NORMAL`).
 * - Retrieves themed font via `GetThemeFont` or falls back to dialog font.
 * - Measures caption text, computes layout and exclusion for frame clipping.
 * - Paints the outer rounded frame via @ref DarkMode::paintRoundFrameRect
 *   using `DarkMode::getEdgePen()`.
 * - Restores clip region and draws text using `DrawThemeTextEx` with custom colors.
 *
 * @param[in]   hWnd        Handle to the group box control.
 * @param[in]   hdc         Device context to draw into.
 * @param[in]   buttonData  Reference to the theming and state info (theme handle).
 *
 * @note Ensures proper cleanup of temporary GDI objects (font, clip region).
 *
 * @see DarkMode::paintRoundFrameRect()
 */
static void paintGroupbox(HWND hWnd, HDC hdc, const dmlib_subclass::ButtonData& buttonData) noexcept
{
	const auto& hTheme = buttonData.m_themeData.getHTheme();

	// Style part

	const bool isDisabled = ::IsWindowEnabled(hWnd) == FALSE;
	static constexpr int iPartID = BP_GROUPBOX;
	const int iStateID = isDisabled ? GBS_DISABLED : GBS_NORMAL;

	// Font part

	bool isFontCreated = false;
	HFONT hFont = nullptr;
	LOGFONT lf{};
	if (SUCCEEDED(::GetThemeFont(hTheme, hdc, iPartID, iStateID, TMT_FONT, &lf)))
	{
		hFont = ::CreateFontIndirectW(&lf);
		isFontCreated = true;
	}

	if (hFont == nullptr)
	{
		hFont = reinterpret_cast<HFONT>(::SendMessage(hWnd, WM_GETFONT, 0, 0));
		isFontCreated = false;
	}

	const auto holdFont = dmlib_paint::GdiObject{ hdc, hFont, !isFontCreated };

	// Text rectangle part

	std::wstring buffer;
	const auto bufferLen = static_cast<size_t>(::GetWindowTextLengthW(hWnd));
	if (bufferLen > 0)
	{
		buffer.resize(bufferLen + 1, L'\0');
		::GetWindowTextW(hWnd, buffer.data(), static_cast<int>(buffer.length()));
	}

	const auto nStyle = ::GetWindowLongPtr(hWnd, GWL_STYLE);
	const bool isCenter = (nStyle & BS_CENTER) == BS_CENTER;

	RECT rcClient{};
	::GetClientRect(hWnd, &rcClient);

	rcClient.bottom -= 1;

	RECT rcText{ rcClient };
	RECT rcBackground{ rcClient };
	if (!buffer.empty())
	{
		SIZE szText{};
		::GetTextExtentPoint32W(hdc, buffer.c_str(), static_cast<int>(bufferLen), &szText);

		const int centerPosX = isCenter ? ((rcClient.right - rcClient.left - szText.cx) / 2) : 7;

		rcBackground.top += szText.cy / 2;
		rcText.left += centerPosX;
		rcText.bottom = rcText.top + szText.cy;
		rcText.right = rcText.left + szText.cx + 4;

		::ExcludeClipRect(hdc, rcText.left, rcText.top, rcText.right, rcText.bottom);
	}
	else // There is no text, use "M" to get metrics to move top edge down
	{
		SIZE szText{};
		::GetTextExtentPoint32W(hdc, L"M", 1, &szText);
		rcBackground.top += szText.cy / 2;
	}

	RECT rcContent = rcBackground;
	::GetThemeBackgroundContentRect(hTheme, hdc, BP_GROUPBOX, iStateID, &rcBackground, &rcContent);
	::ExcludeClipRect(hdc, rcContent.left, rcContent.top, rcContent.right, rcContent.bottom);

	dmlib_paint::paintFrameRect(hdc, rcBackground, DarkMode::getEdgePen()); // main frame

	::SelectClipRgn(hdc, nullptr);

	// Text part

	if (!buffer.empty())
	{
		::InflateRect(&rcText, -2, 0);

		DTTOPTS dtto{};
		dtto.dwSize = sizeof(DTTOPTS);
		dtto.dwFlags = DTT_TEXTCOLOR;
		dtto.crText = isDisabled ? DarkMode::getDisabledTextColor() : DarkMode::getTextColor();

		DWORD dtFlags = isCenter ? DT_CENTER : DT_LEFT;

		if (::SendMessage(hWnd, WM_QUERYUISTATE, 0, 0) != 0) // NULL
		{
			dtFlags |= DT_HIDEPREFIX;
		}

		::DrawThemeTextEx(hTheme, hdc, BP_GROUPBOX, iStateID, buffer.c_str(), -1, dtFlags | DT_SINGLELINE, &rcText, &dtto);
	}
}

/**
 * @brief Window subclass procedure for owner drawn groupbox button control.
 *
 * @param[in]   hWnd        Window handle being subclassed.
 * @param[in]   uMsg        Message identifier.
 * @param[in]   wParam      Message-specific data.
 * @param[in]   lParam      Message-specific data.
 * @param[in]   uIdSubclass Subclass identifier.
 * @param[in]   dwRefData   ButtonData instance.
 * @return LRESULT Result of message processing.
 *
 * @see paintGroupbox()
 * @see DarkMode::setGroupboxCtrlSubclass()
 * @see DarkMode::removeGroupboxCtrlSubclass()
 */
LRESULT CALLBACK dmlib_subclass::GroupboxSubclass(
	HWND hWnd,
	UINT uMsg,
	WPARAM wParam,
	LPARAM lParam,
	UINT_PTR uIdSubclass,
	DWORD_PTR dwRefData
)
{
	auto* pButtonData = reinterpret_cast<ButtonData*>(dwRefData);
	auto& themeData = pButtonData->m_themeData;

	switch (uMsg)
	{
		case WM_NCDESTROY:
		{
			::RemoveWindowSubclass(hWnd, GroupboxSubclass, uIdSubclass);
			std::unique_ptr<ButtonData> u_ptrData(pButtonData);
			u_ptrData.reset(nullptr);
			break;
		}

		case WM_ERASEBKGND:
		{
			if (!DarkMode::isEnabled() || !themeData.ensureTheme(hWnd))
			{
				break;
			}
			return TRUE;
		}

		case WM_PRINTCLIENT:
		case WM_PAINT:
		{
			if (!DarkMode::isEnabled() || !themeData.ensureTheme(hWnd))
			{
				break;
			}

			PAINTSTRUCT ps{};
			auto hdc = reinterpret_cast<HDC>(wParam);
			if (hdc == nullptr)
			{
				hdc = ::BeginPaint(hWnd, &ps);
			}

			paintGroupbox(hWnd, hdc, *pButtonData);

			if (ps.hdc != nullptr)
			{
				::EndPaint(hWnd, &ps);
			}

			return 0;
		}

		case WM_DPICHANGED_AFTERPARENT:
		{
			themeData.closeTheme();
			return 0;
		}

		case WM_THEMECHANGED:
		{
			themeData.closeTheme();
			break;
		}

		case WM_ENABLE:
		{
			::RedrawWindow(hWnd, nullptr, nullptr, RDW_INVALIDATE);
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
 * @brief Retrieves the appropriate color based on the control's state.
 *
 * This function determines the color to be used for a control based on its
 * current state. The disabled state takes precedence over the hot state.
 *
 * @param[in]   isDisabled  Boolean indicating if the control is in a disabled state.
 *                          If true, the function prioritizes this state when determining
 *                          the returned color.
 * @param[in]   isHot       Boolean indicating if the control is in a hot state.
 *                          This state is considered only if the control is not disabled.
 *
 * @return      COLORREF    The color reference corresponding to the control's state:
 *                          - Disabled: color from `DarkMode::getDisabledTextColor()`.
 *                          - Hot: color from `DarkMode::getTextColor()`.
 *                          - Default: color from `DarkMode::getDarkerTextColor()`.
 */
static COLORREF getColorFromState(bool isDisabled, bool isHot) noexcept
{
	if (isDisabled)
	{
		return DarkMode::getDisabledTextColor();
	}
	if (isHot)
	{
		return DarkMode::getTextColor();
	}
	return DarkMode::getDarkerTextColor();
};

/**
 * @brief Retrieves the appropriate HBRUSH based on the control's state.
 *
 * This function determines the HBRUSH to be used for a control based on its
 * current state. The disabled state takes precedence over the hot state.
 *
 * @param[in]   isDisabled  Boolean indicating if the control is in a disabled state.
 *                          If true, the function prioritizes this state when determining
 *                          the returned HBRUSH.
 * @param[in]   isHot       Boolean indicating if the control is in a hot state.
 *                          This state is considered only if the control is not disabled.
 *
 * @return      HBRUSH      The color reference corresponding to the control's state:
 *                          - Disabled: color from `DarkMode::getDlgBackgroundBrush()`.
 *                          - Hot: color from `DarkMode::getHotBackgroundBrush()`.
 *                          - Default: color from `DarkMode::getCtrlBackgroundBrush()`.
 */
static HBRUSH getBrushFromState(bool isDisabled, bool isHot) noexcept
{
	if (isDisabled)
	{
		return DarkMode::getDlgBackgroundBrush();
	}
	if (isHot)
	{
		return DarkMode::getHotBackgroundBrush();
	}
	return DarkMode::getCtrlBackgroundBrush();
};

/**
 * @brief Retrieves the appropriate HPEN based on the control's state.
 *
 * This function determines the HPEN to be used for a control based on its
 * current state. The disabled state takes precedence over the hot state.
 *
 * @param[in]   isDisabled  Boolean indicating if the control is in a disabled state.
 *                          If true, the function prioritizes this state when determining
 *                          the returned HPEN.
 * @param[in]   isHot       Boolean indicating if the control is in a hot state.
 *                          This state is considered only if the control is not disabled.
 *
 * @return      HPEN        The color reference corresponding to the control's state:
 *                          - Disabled: color from `DarkMode::getDisabledEdgePen()`.
 *                          - Hot: color from `DarkMode::getHotEdgePen()`.
 *                          - Default: color from `DarkMode::getEdgePen()`.
 */
static HPEN getEdgePenFromState(bool isDisabled, bool isHot) noexcept
{
	if (isDisabled)
	{
		return DarkMode::getDisabledEdgePen();
	}
	if (isHot)
	{
		return DarkMode::getHotEdgePen();
	}
	return DarkMode::getEdgePen();
};

/**
 * @brief Paints an up-down button with the appropriate background and edge based on its state.
 *
 * This function determines the brush and pen to use for painting an up-down button
 * based on whether it is disabled, hot, or in normal state. The painting includes
 * rounded corners for aesthetics.
 *
 * @param[in]   hdc         Handle to the device context used for painting.
 * @param[in]   rect        Rectangle that defines the area in which to paint the button.
 * @param[in]   isDisabled  Boolean indicating if the button is in a disabled state.
 * @param[in]   isHot       Boolean indicating if the button is in a hot state.
 */
static void paintUpDownBtn(
	HDC hdc,
	const RECT& rect,
	bool isDisabled,
	bool isHot,
	int roundness
) noexcept
{
	HBRUSH hBrush = nullptr;
	HPEN hPen = nullptr;

	if (isDisabled)
	{
		hBrush = DarkMode::getDlgBackgroundBrush();
		hPen = DarkMode::getDisabledEdgePen();
	}
	else if (isHot)
	{
		hBrush = DarkMode::getHotBackgroundBrush();
		hPen = DarkMode::getHotEdgePen();
	}
	else
	{
		hBrush = DarkMode::getCtrlBackgroundBrush();
		hPen = DarkMode::getEdgePen();
	}

	dmlib_paint::paintRoundRect(hdc, rect, hPen, hBrush, roundness, roundness);
}

/**
 * @brief Paints an arrow in a specified direction based on the provided state.
 *
 * This function calculates the appropriate size and position to draw an arrow
 * within the given rectangle based on whether it is in a hot state, its direction,
 * and other parameters.
 *
 * @param[in]   hdc         Handle to the device context used for painting.
 * @param[in]   hWnd        Handle to the control for dpi calculation.
 * @param[in]   upDownData  Reference to layout and state information (segments, orientation, corner radius).
 * @param[in]   rect        Rectangle that defines the area in which to paint the arrow.
 * @param[in]   isHot       Boolean indicating if the arrow should appear hot (hovered).
 * @param[in]   isPrev      Boolean indicating the direction of the arrow:
 *                          true for "previous" (left/up) and false for "next" (right/down).
 * @param[in]   isDisabled  Boolean indicating if the arrow is in a disabled state.
 */
static void paintArrow(
	HDC hdc,
	HWND hWnd,
	const dmlib_subclass::UpDownData& upDownData,
	const RECT& rect,
	bool isHot,
	bool isPrev,
	bool isDisabled
) noexcept
{
	SIZE size{};
	::GetThemePartSize(upDownData.m_themeData.getHTheme(), nullptr, SPNP_UP, UPS_NORMAL, nullptr, TS_TRUE, &size);

	static constexpr std::array<POINTFLOAT, 3> ptsArrowLeft{ { {1.0F, 0.0F}, {0.0F, 0.5F}, {1.0F, 1.0F} } };
	static constexpr std::array<POINTFLOAT, 3> ptsArrowRight{ { {0.0F, 0.0F}, {1.0F, 0.5F}, {0.0F, 1.0F} } };
	static constexpr std::array<POINTFLOAT, 3> ptsArrowUp{ { {0.0F, 1.0F}, {0.5F, 0.0F}, {1.0F, 1.0F} } };
	static constexpr std::array<POINTFLOAT, 3> ptsArrowDown{ { {0.0F, 0.0F}, {0.5F, 1.0F}, {1.0F, 0.0F} } };

	static constexpr auto scaleFactor = 3L;
	static constexpr auto offsetSize = scaleFactor % 2;
	const auto baseSize = static_cast<float>(dmlib_dpi::scale(((size.cy - offsetSize) / scaleFactor) + offsetSize, ::GetParent(hWnd)));

	auto sizeArrow = POINTFLOAT{ baseSize, baseSize };
	auto offsetPosX = 0.0F;
	auto offsetPosY = 0.0F;
	std::array<POINTFLOAT, 3> ptsArrowSelected{};
	if (upDownData.m_isHorizontal)
	{
		if (isPrev)
		{
			ptsArrowSelected = ptsArrowLeft;
			offsetPosX = 1.0F;
		}
		else
		{
			ptsArrowSelected = ptsArrowRight;
			offsetPosX = -1.0F;
		}
		sizeArrow.x *= 0.5F; // ratio adjustment
	}
	else
	{
		if (isPrev)
		{
			ptsArrowSelected = ptsArrowUp;
			offsetPosY = 1.0F;
		}
		else
		{
			ptsArrowSelected = ptsArrowDown;
		}
		sizeArrow.y *= 0.5F;
	}

	const auto xPos = static_cast<float>(rect.left) + ((static_cast<float>(rect.right - rect.left) - sizeArrow.x - offsetPosX) / 2.0F);
	const auto yPos = static_cast<float>(rect.top) + ((static_cast<float>(rect.bottom - rect.top) - sizeArrow.y - offsetPosY) / 2.0F);

	std::array<POINT, 3> ptsArrow{};
	for (size_t i = 0; i < 3; ++i)
	{
		ptsArrow.at(i).x = static_cast<LONG>((ptsArrowSelected.at(i).x * sizeArrow.x) + xPos);
		ptsArrow.at(i).y = static_cast<LONG>((ptsArrowSelected.at(i).y * sizeArrow.y) + yPos);
	}

	const COLORREF clrSelected = getColorFromState(isDisabled, isHot);
	const auto hBrush = dmlib_paint::GdiObject{ hdc, ::CreateSolidBrush(clrSelected) };
	const auto hPen = dmlib_paint::GdiObject{ hdc, ::CreatePen(PS_SOLID, 1, clrSelected) };

	::Polygon(hdc, ptsArrow.data(), static_cast<int>(ptsArrow.size()));
}

/**
 * @brief Custom paints an up-down (spinner) control.
 *
 * Draws the two-button spinner control using either themed drawing or manual
 * owner-drawn logic depending on OS version and theme availability. Supports both
 * vertical and horizontal orientations and adapts to hover and disabled states.
 *
 * Paint logic:
 * - Background fill with dialog background brush
 * - Rounded corners (optional, based on Windows 11 and parent class)
 * - Direction-aware layout and glyph placement
 *
 * @param[in]       hWnd        Handle to the up-down control.
 * @param[in]       hdc         Device context to draw into.
 * @param[in,out]   upDownData  Reference to layout and state information (segments, orientation, corner radius).
 *
 * @see UpDownData
 */
static void paintUpDown(HWND hWnd, HDC hdc, dmlib_subclass::UpDownData& upDownData) noexcept
{
	auto& themeData = upDownData.m_themeData;
	const bool hasTheme = themeData.ensureTheme(hWnd);
	const auto& hTheme = themeData.getHTheme();

	const bool isDisabled = ::IsWindowEnabled(hWnd) == FALSE;
	const bool isHorz = upDownData.m_isHorizontal;

	::FillRect(hdc, &upDownData.m_rcClient, DarkMode::getDlgBackgroundBrush());
	::SetBkMode(hdc, TRANSPARENT);

	POINT ptCursor{};
	::GetCursorPos(&ptCursor);
	::ScreenToClient(hWnd, &ptCursor);

	const bool isHotPrev = ::PtInRect(&upDownData.m_rcPrev, ptCursor) == TRUE;
	const bool isHotNext = ::PtInRect(&upDownData.m_rcNext, ptCursor) == TRUE;

	upDownData.m_wasHotNext = !isHotPrev && (::PtInRect(&upDownData.m_rcClient, ptCursor) == TRUE);

	if (hasTheme && DarkMode::isAtLeastWindows11() && dmlib_subclass::isThemePrefered())
	{
		// all 4 variants of up-down control buttons have enums with same values
		auto getStateId = [&isDisabled](bool isHot) noexcept
		{
			if (isDisabled)
			{
				return UPS_DISABLED;
			}
			if (isHot)
			{
				return UPS_HOT;
			}
			return UPS_NORMAL;
		};

		const int stateIdPrev = getStateId(isHotPrev);
		const int stateIdNext = getStateId(isHotNext);

		RECT rcPrev{ upDownData.m_rcPrev };
		RECT rcNext{ upDownData.m_rcNext };

		int partIdPrev = SPNP_DOWNHORZ;
		int partIdNext = SPNP_UPHORZ;

		if (!isHorz)
		{
			--rcPrev.left;
			--rcNext.left;

			partIdPrev = SPNP_UP;
			partIdNext = SPNP_DOWN;
		}

		::DrawThemeBackground(hTheme, hdc, partIdPrev, stateIdPrev, &rcPrev, nullptr);
		::DrawThemeBackground(hTheme, hdc, partIdNext, stateIdNext, &rcNext, nullptr);
	}
	else
	{
		// Button part

		paintUpDownBtn(hdc, upDownData.m_rcPrev, isDisabled, isHotPrev, upDownData.m_cornerRoundness);
		paintUpDownBtn(hdc, upDownData.m_rcNext, isDisabled, isHotNext, upDownData.m_cornerRoundness);

		// Glyph part

		if (hasTheme)
		{
			paintArrow(hdc, hWnd, upDownData, upDownData.m_rcPrev, isHotPrev, true, isDisabled);
			paintArrow(hdc, hWnd, upDownData, upDownData.m_rcNext, isHotNext, false, isDisabled);
		}
		else
		{
			const auto hFont = dmlib_paint::GdiObject{ hdc, hWnd };

			static constexpr UINT dtFlags = DT_NOPREFIX | DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOCLIP;
			const LONG offset = isHorz ? 1 : 0;

			RECT rcTextPrev{ upDownData.m_rcPrev.left, upDownData.m_rcPrev.top, upDownData.m_rcPrev.right, upDownData.m_rcPrev.bottom - offset };
			::SetTextColor(hdc, getColorFromState(isDisabled, isHotPrev));
			::DrawText(hdc, isHorz ? dmlib_glyph::kArrowLeft : dmlib_glyph::kArrowUp, -1, &rcTextPrev, dtFlags);

			RECT rcTextNext{ upDownData.m_rcNext.left + offset, upDownData.m_rcNext.top, upDownData.m_rcNext.right, upDownData.m_rcNext.bottom - offset };
			::SetTextColor(hdc, getColorFromState(isDisabled, isHotNext));
			::DrawText(hdc, isHorz ? dmlib_glyph::kArrowRight : dmlib_glyph::kArrowDown, -1, &rcTextNext, dtFlags);
		}
	}
}

/**
 * @brief Window subclass procedure for owner drawn up-down (spinner) control.
 *
 * @param[in]   hWnd        Window handle being subclassed.
 * @param[in]   uMsg        Message identifier.
 * @param[in]   wParam      Message-specific data.
 * @param[in]   lParam      Message-specific data.
 * @param[in]   uIdSubclass Subclass identifier.
 * @param[in]   dwRefData   UpDownData instance.
 * @return LRESULT Result of message processing.
 *
 * @see paintUpDown()
 * @see DarkMode::setUpDownCtrlSubclass()
 * @see DarkMode::removeUpDownCtrlSubclass()
 */
LRESULT CALLBACK dmlib_subclass::UpDownSubclass(
	HWND hWnd,
	UINT uMsg,
	WPARAM wParam,
	LPARAM lParam,
	UINT_PTR uIdSubclass,
	DWORD_PTR dwRefData
)
{
	auto* pUpDownData = reinterpret_cast<UpDownData*>(dwRefData);
	auto& themeData = pUpDownData->m_themeData;
	const auto& hMemDC = pUpDownData->m_bufferData.getHMemDC();

	switch (uMsg)
	{
		case WM_NCDESTROY:
		{
			::RemoveWindowSubclass(hWnd, UpDownSubclass, uIdSubclass);
			std::unique_ptr<UpDownData> u_ptrData(pUpDownData);
			u_ptrData.reset(nullptr);
			break;
		}

		case WM_ERASEBKGND:
		{
			if (!DarkMode::isEnabled() || !themeData.ensureTheme(hWnd))
			{
				break;
			}

			if (reinterpret_cast<HDC>(wParam) != hMemDC)
			{
				return FALSE;
			}
			return TRUE;
		}

		case WM_PAINT:
		{
			if (!DarkMode::isEnabled())
			{
				break;
			}

			PAINTSTRUCT ps{};
			HDC hdc = ::BeginPaint(hWnd, &ps);

			if (!dmlib_paint::isRectValid(ps.rcPaint))
			{
				::EndPaint(hWnd, &ps);
				return 0;
			}

			if (!pUpDownData->m_isHorizontal)
			{
				::OffsetRect(&ps.rcPaint, 2, 0);
			}

			RECT rcClient{};
			::GetClientRect(hWnd, &rcClient);
			pUpDownData->updateRect(rcClient);
			if (!pUpDownData->m_isHorizontal)
			{
				::OffsetRect(&rcClient, 2, 0);
			}

			dmlib_paint::PaintWithBuffer<UpDownData>(*pUpDownData, hdc, ps,
				[&]() { paintUpDown(hWnd, hMemDC, *pUpDownData); },
				rcClient);

			::EndPaint(hWnd, &ps);
			return 0;
		}

		case WM_DPICHANGED_AFTERPARENT:
		{
			pUpDownData->updateRect(hWnd);
			themeData.closeTheme();
			return 0;
		}

		case WM_THEMECHANGED:
		{
			themeData.closeTheme();
			break;
		}

		case WM_MOUSEMOVE:
		{
			if (!DarkMode::isEnabled())
			{
				break;
			}

			if (pUpDownData->m_wasHotNext)
			{
				pUpDownData->m_wasHotNext = false;
				::RedrawWindow(hWnd, nullptr, nullptr, RDW_INVALIDATE);
			}

			break;
		}

		case WM_MOUSELEAVE:
		{
			if (!DarkMode::isEnabled())
			{
				break;
			}

			pUpDownData->m_wasHotNext = false;
			::RedrawWindow(hWnd, nullptr, nullptr, RDW_INVALIDATE);

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
 * @brief Paints a tab item in a tab control.
 *
 * This function handles the rendering of a tab item, determining its appearance
 * based on whether it is selected, hot (hovered), or has an image. It also applies
 * appropriate colors and styles depending on the current visual state of the tab.
 *
 * @param[in]   hdc             Handle to the device context used for painting.
 * @param[in]   hWnd            Handle to the parent tab control window.
 * @param[in]   rcItem          Rectangle defining the area of the tab item.
 * @param[in]   i               Index of the tab item being painted.
 * @param[in]   iSelTab         Index of the currently selected tab.
 * @param[in]   nTabs           Total number of tabs in the tab control.
 * @param[in]   ptCursor        Point representing the current cursor position.
 */
static void paintTabItem(
	HDC hdc,
	HWND hWnd,
	RECT& rcItem,
	int i,
	int iSelTab,
	int nTabs,
	const POINT& ptCursor
) noexcept
{
	RECT rcFrame{ rcItem };

	const bool isHot = ::PtInRect(&rcItem, ptCursor) == TRUE;
	const bool isSelectedTab = (i == iSelTab);

	::InflateRect(&rcItem, -1, -1);
	rcItem.right += 1;

	std::wstring label(MAX_PATH, L'\0');
	TCITEM tci{};
	tci.mask = TCIF_TEXT | TCIF_IMAGE | TCIF_STATE;
	tci.dwStateMask = TCIS_HIGHLIGHTED;
	tci.pszText = label.data();
	tci.cchTextMax = MAX_PATH - 1;

	TabCtrl_GetItem(hWnd, i, &tci);

	RECT rcText{ rcItem };

	if (const auto nStyle = ::GetWindowLongPtr(hWnd, GWL_STYLE);
		(nStyle & TCS_BUTTONS) == TCS_BUTTONS) // is button
	{
		const bool isHighlighted = (tci.dwState & TCIS_HIGHLIGHTED) == TCIS_HIGHLIGHTED;
		::FillRect(hdc, &rcItem, isHighlighted ? DarkMode::getHotBackgroundBrush() : DarkMode::getDlgBackgroundBrush());
		::SetTextColor(hdc, isHighlighted ? DarkMode::getLinkTextColor() : DarkMode::getDarkerTextColor());
	}
	else
	{
		// For consistency getBackgroundBrush()
		// would be better, than getCtrlBackgroundBrush(),
		// however default getBackgroundBrush() has almost same color
		// as getDlgBackgroundBrush()

		::FillRect(hdc, &rcItem, getBrushFromState(isSelectedTab, isHot));
		::SetTextColor(hdc, (isHot || isSelectedTab) ? DarkMode::getTextColor() : DarkMode::getDarkerTextColor());

		if (isSelectedTab)
		{
			::OffsetRect(&rcText, 0, -1);
			::InflateRect(&rcFrame, 0, 1);
		}

		if (i != nTabs - 1)
		{
			rcFrame.right += 1;
		}
	}

	// Draw image
	if (tci.iImage != -1)
	{
		int cx = 0;
		int cy = 0;
		auto hImagelist = TabCtrl_GetImageList(hWnd);
		static constexpr int offset = 2;
		::ImageList_GetIconSize(hImagelist, &cx, &cy);
		::ImageList_Draw(hImagelist, tci.iImage, hdc, rcText.left + offset, rcText.top + (((rcText.bottom - rcText.top) - cy) / 2), ILD_NORMAL);
		rcText.left += cx;
	}

	::DrawText(hdc, label.c_str(), -1, &rcText, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

	::FrameRect(hdc, &rcFrame, DarkMode::getEdgeBrush());

	// Draw focus keyboard cue
	if (isSelectedTab && ::GetFocus() == hWnd)
	{
		if (const auto uiState = static_cast<DWORD>(::SendMessage(hWnd, WM_QUERYUISTATE, 0, 0));
			(uiState & UISF_HIDEFOCUS) != UISF_HIDEFOCUS)
		{
			::InflateRect(&rcFrame, -2, -1);
			::DrawFocusRect(hdc, &rcFrame);
		}
	}
}

/**
 * @brief Custom paints tab items.
 *
 * Iterates through all tabs in a `SysTabControl32`, applying customized backgrounds,
 * text colors, focus indicators, and optional icon drawing. Handles both button-style
 * (`TCS_BUTTONS`) and standard tab layouts, adapting based on hover state, selection,
 * and focus cue.
 *
 * Paint logic includes:
 * - Retrieves label and optional image via `TCITEM` and `ImageList_Draw`
 * - Applies coloring based on selection, hover, and tab style
 * - Clips each tab to avoid flickering during overlapping redraw
 * - Draws optional focus rectangle if control has input focus via keyboard
 *
 * @note Currently only works for horizontal style.
 *
 * @param[in]   hWnd    Handle to the tab control.
 * @param[in]   hdc     Device context to draw into.
 * @param[in]   rect    Tab control rectangle.
 */
static void paintTab(HWND hWnd, HDC hdc, const RECT& rect) noexcept
{
	::FillRect(hdc, &rect, DarkMode::getDlgBackgroundBrush());

	const auto hPen = dmlib_paint::GdiObject{ hdc, DarkMode::getEdgePen(), true };
	const auto hFont = dmlib_paint::GdiObject{ hdc, hWnd };

	auto holdClip = ::CreateRectRgn(0, 0, 0, 0);
	if (::GetClipRgn(hdc, holdClip) != 1)
	{
		::DeleteObject(holdClip);
		holdClip = nullptr;
	}

	POINT ptCursor{};
	::GetCursorPos(&ptCursor);
	::ScreenToClient(hWnd, &ptCursor);

	::SetBkMode(hdc, TRANSPARENT);

	const auto iSelTab = TabCtrl_GetCurSel(hWnd);
	const auto nTabs = TabCtrl_GetItemCount(hWnd);
	for (int i = 0; i < nTabs; ++i)
	{
		RECT rcItem{};
		TabCtrl_GetItemRect(hWnd, i, &rcItem);

		if (RECT rcIntersect{};
			::IntersectRect(&rcIntersect, &rect, &rcItem) == FALSE)
		{
			continue; // Skip to the next iteration when there is no intersection
		}

		HRGN hClip = ::CreateRectRgnIndirect(&rcItem);
		::SelectClipRgn(hdc, hClip);

		paintTabItem(hdc, hWnd, rcItem, i, iSelTab, nTabs, ptCursor);

		::SelectClipRgn(hdc, holdClip);
		::DeleteObject(hClip);
	}

	::SelectClipRgn(hdc, holdClip);
	if (holdClip != nullptr)
	{
		::DeleteObject(holdClip);
		holdClip = nullptr;
	}
}

/**
 * @brief Window subclass procedure for owner drawn tab control.
 *
 * @param[in]   hWnd        Window handle being subclassed.
 * @param[in]   uMsg        Message identifier.
 * @param[in]   wParam      Message-specific data.
 * @param[in]   lParam      Message-specific data.
 * @param[in]   uIdSubclass Subclass identifier.
 * @param[in]   dwRefData   TabData instance.
 * @return LRESULT Result of message processing.
 *
 * @see paintTab()
 * @see DarkMode::setTabCtrlPaintSubclass()
 * @see DarkMode::removeTabCtrlPaintSubclass()
 */
LRESULT CALLBACK dmlib_subclass::TabPaintSubclass(
	HWND hWnd,
	UINT uMsg,
	WPARAM wParam,
	LPARAM lParam,
	UINT_PTR uIdSubclass,
	DWORD_PTR dwRefData
)
{
	auto* pTabData = reinterpret_cast<TabData*>(dwRefData);
	const auto& hMemDC = pTabData->m_bufferData.getHMemDC();

	if (const auto nStyle = ::GetWindowLongPtrW(hWnd, GWL_STYLE);
		((nStyle & (TCS_VERTICAL | TCS_OWNERDRAWFIXED)) != 0)
		&& (uMsg != WM_NCDESTROY))
	{
		return ::DefSubclassProc(hWnd, uMsg, wParam, lParam);
	}

	switch (uMsg)
	{
		case WM_NCDESTROY:
		{
			::RemoveWindowSubclass(hWnd, TabPaintSubclass, uIdSubclass);
			std::unique_ptr<TabData> u_ptrData(pTabData);
			u_ptrData.reset(nullptr);
			break;
		}

		case WM_ERASEBKGND:
		{
			if (!DarkMode::isEnabled())
			{
				break;
			}

			if (reinterpret_cast<HDC>(wParam) != hMemDC)
			{
				return FALSE;
			}
			return TRUE;
		}

		case WM_PAINT:
		{
			if (!DarkMode::isEnabled())
			{
				break;
			}

			PAINTSTRUCT ps{};
			HDC hdc = ::BeginPaint(hWnd, &ps);

			if (!dmlib_paint::isRectValid(ps.rcPaint))
			{
				::EndPaint(hWnd, &ps);
				return 0;
			}

			RECT rcClient{};
			::GetClientRect(hWnd, &rcClient);
			dmlib_paint::PaintWithBuffer<TabData>(*pTabData, hdc, ps,
				[&]() { paintTab(hWnd, hMemDC, rcClient); },
				hWnd);

			::EndPaint(hWnd, &ps);
			return 0;
		}

		case WM_UPDATEUISTATE:
		{
			if ((HIWORD(wParam) & (UISF_HIDEACCEL | UISF_HIDEFOCUS)) != 0)
			{
				::InvalidateRect(hWnd, nullptr, FALSE);
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
 * @brief Window subclass procedure for tab control's up-down control subclassing.
 *
 * @param[in]   hWnd        Window handle being subclassed.
 * @param[in]   uMsg        Message identifier.
 * @param[in]   wParam      Message-specific data.
 * @param[in]   lParam      Message-specific data.
 * @param[in]   uIdSubclass Subclass identifier.
 * @param[in]   dwRefData   Reserved data (unused).
 * @return LRESULT Result of message processing.
 *
 * @see DarkMode::setUpDownCtrlSubclass()
 * @see DarkMode::setTabCtrlUpDownSubclass()
 * @see DarkMode::removeTabCtrlUpDownSubclass()
 */
LRESULT CALLBACK dmlib_subclass::TabUpDownSubclass(
	HWND hWnd,
	UINT uMsg,
	WPARAM wParam,
	LPARAM lParam,
	UINT_PTR uIdSubclass,
	[[maybe_unused]] DWORD_PTR dwRefData
)
{
	switch (uMsg)
	{
		case WM_NCDESTROY:
		{
			::RemoveWindowSubclass(hWnd, TabUpDownSubclass, uIdSubclass);
			break;
		}

		case WM_PARENTNOTIFY:
		{
			if (LOWORD(wParam) == WM_CREATE)
			{
				auto hUpDown = reinterpret_cast<HWND>(lParam);
				if (dmlib_subclass::cmpWndClassName(hUpDown, UPDOWN_CLASS))
				{
					DarkMode::setUpDownCtrlSubclass(hUpDown);
					return 0;
				}
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
 * @brief Paints a custom non-client border for list box and edit controls.
 *
 * Paints an inner and outer border using custom colors.
 * The outer border highlights when the window is hot (hovered) or focused.
 *
 * @param[in]   hWnd                Handle to the target list box or edit control.
 * @param[in]   borderMetricsData   Precomputed system metrics and hot state.
 */
static void ncPaintCustomBorder(HWND hWnd, const dmlib_subclass::BorderMetricsData& borderMetricsData) noexcept
{
	HDC hdc = ::GetWindowDC(hWnd);
	RECT rcClient{};
	::GetClientRect(hWnd, &rcClient);
	rcClient.right += (2 * borderMetricsData.m_xEdge);

	const auto nStyle = ::GetWindowLongPtr(hWnd, GWL_STYLE);
	if ((nStyle & WS_VSCROLL) == WS_VSCROLL)
	{
		rcClient.right += borderMetricsData.m_xScroll;
	}

	rcClient.bottom += (2 * borderMetricsData.m_yEdge);

	if ((nStyle & WS_HSCROLL) == WS_HSCROLL)
	{
		rcClient.bottom += borderMetricsData.m_yScroll;
	}

	HPEN hPen = ::CreatePen(PS_SOLID, 1, (::IsWindowEnabled(hWnd) == TRUE) ? DarkMode::getBackgroundColor() : DarkMode::getDlgBackgroundColor());
	RECT rcInner{ rcClient };
	::InflateRect(&rcInner, -1, -1);
	dmlib_paint::paintFrameRect(hdc, rcInner, hPen);
	::DeleteObject(hPen);

	POINT ptCursor{};
	::GetCursorPos(&ptCursor);
	::ScreenToClient(hWnd, &ptCursor);

	const bool isHot = ::PtInRect(&rcClient, ptCursor) == TRUE;
	const bool hasFocus = ::GetFocus() == hWnd;

	HPEN hEnabledPen = ((borderMetricsData.m_isHot && isHot) || hasFocus ? DarkMode::getHotEdgePen() : DarkMode::getEdgePen());

	dmlib_paint::paintFrameRect(hdc, rcClient, (::IsWindowEnabled(hWnd) == TRUE) ? hEnabledPen : DarkMode::getDisabledEdgePen());

	::ReleaseDC(hWnd, hdc);
}

/**
 * @brief Window subclass procedure for owner drawn border for list box and edit controls.
 *
 * @param[in]   hWnd        Window handle being subclassed.
 * @param[in]   uMsg        Message identifier.
 * @param[in]   wParam      Message-specific data.
 * @param[in]   lParam      Message-specific data.
 * @param[in]   uIdSubclass Subclass identifier.
 * @param[in]   dwRefData   BorderMetricsData instance.
 * @return LRESULT Result of message processing.
 *
 * @see DarkMode::setCustomBorderForListBoxOrEditCtrlSubclass()
 * @see DarkMode::removeCustomBorderForListBoxOrEditCtrlSubclass()
 */
LRESULT CALLBACK dmlib_subclass::CustomBorderSubclass(
	HWND hWnd,
	UINT uMsg,
	WPARAM wParam,
	LPARAM lParam,
	UINT_PTR uIdSubclass,
	DWORD_PTR dwRefData
)
{
	auto* pBorderMetricsData = reinterpret_cast<BorderMetricsData*>(dwRefData);

	switch (uMsg)
	{
		case WM_NCDESTROY:
		{
			::RemoveWindowSubclass(hWnd, CustomBorderSubclass, uIdSubclass);
			std::unique_ptr<BorderMetricsData> u_ptrData(pBorderMetricsData);
			u_ptrData.reset(nullptr);
			break;
		}

		case WM_NCPAINT:
		{
			if (!DarkMode::isEnabled())
			{
				break;
			}

			::DefSubclassProc(hWnd, uMsg, wParam, lParam);

			ncPaintCustomBorder(hWnd, *pBorderMetricsData);

			return 0;
		}

		case WM_NCCALCSIZE:
		{
			if (!DarkMode::isEnabled())
			{
				break;
			}

			auto* lpRect = reinterpret_cast<LPRECT>(lParam);
			::InflateRect(lpRect, -(pBorderMetricsData->m_xEdge), -(pBorderMetricsData->m_yEdge));

			break;
		}

		case WM_DPICHANGED_AFTERPARENT:
		{
			pBorderMetricsData->setMetricsForDpi(dmlib_dpi::GetDpiForParent(hWnd));
			DarkMode::redrawWindowFrame(hWnd);
			return 0;
		}

		case WM_MOUSEMOVE:
		{
			if (!DarkMode::isEnabled())
			{
				break;
			}

			if (::GetFocus() == hWnd)
			{
				break;
			}

			TRACKMOUSEEVENT tme{};
			tme.cbSize = sizeof(TRACKMOUSEEVENT);
			tme.dwFlags = TME_LEAVE;
			tme.hwndTrack = hWnd;
			tme.dwHoverTime = HOVER_DEFAULT;
			::TrackMouseEvent(&tme);

			if (!pBorderMetricsData->m_isHot)
			{
				pBorderMetricsData->m_isHot = true;
				DarkMode::redrawWindowFrame(hWnd);
			}
			break;
		}

		case WM_MOUSELEAVE:
		{
			if (!DarkMode::isEnabled())
			{
				break;
			}

			if (pBorderMetricsData->m_isHot)
			{
				pBorderMetricsData->m_isHot = false;
				DarkMode::redrawWindowFrame(hWnd);
			}

			TRACKMOUSEEVENT tme{};
			tme.cbSize = sizeof(TRACKMOUSEEVENT);
			tme.dwFlags = TME_LEAVE | TME_CANCEL;
			tme.hwndTrack = hWnd;
			tme.dwHoverTime = HOVER_DEFAULT;
			::TrackMouseEvent(&tme);
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
 * @brief Custom paints a combo box control.
 *
 * This function handles owner-drawn drawing of a combo box, adapting its
 * appearance based on:
 * - Control style (`CBS_SIMPLE`, `CBS_DROPDOWN`, `CBS_DROPDOWNLIST`)
 * - Enabled/disabled state
 * - Hot (hover) state
 * - Focus state
 * - Dark mode theme availability
 *
 * Paint logic:
 * - Draws background with different brushes for normal, hot, and disabled states
 * - Uses `COMBOBOXINFO` to retrieve subcomponent rectangles.
 * - Draws text using theme APIs if available, otherwise GDI
 * - For `CBS_DROPDOWNLIST`, draws the selected item text directly.
 * - For `CBS_DROPDOWN` and `CBS_SIMPLE`, text is handled by the child edit control.
 * - The drop-down arrow is drawn either via `DrawThemeBackground` or a manual glyph.
 * - Borders are drawn with pens with custom colors depending on state (rounded corners on Windows 11+).
 * - Uses `ExcludeClipRect` to avoid overpainting the text/edit area.
 *
 * @param[in]       hWnd            Handle to the combo box control.
 * @param[in]       hdc             Device context to draw into.
 * @param[in,out]   comboBoxData    Reference to the combo box' theme and style data.
 *
 * @see ComboBoxData
 */
static void paintCombobox(HWND hWnd, HDC hdc, dmlib_subclass::ComboBoxData& comboBoxData) noexcept
{
	auto& themeData = comboBoxData.m_themeData;
	const auto& hTheme = themeData.getHTheme();

	const bool hasTheme = themeData.ensureTheme(hWnd);

	COMBOBOXINFO cbi{};
	cbi.cbSize = sizeof(COMBOBOXINFO);
	::GetComboBoxInfo(hWnd, &cbi);

	RECT rcClient{};
	::GetClientRect(hWnd, &rcClient);

	POINT ptCursor{};
	::GetCursorPos(&ptCursor);
	::ScreenToClient(hWnd, &ptCursor);

	const bool isDisabled = ::IsWindowEnabled(hWnd) == FALSE;
	const bool isHot = ::PtInRect(&rcClient, ptCursor) == TRUE && !isDisabled;

	bool hasFocus = false;

	const auto holdFont = dmlib_paint::GdiObject{ hdc, hWnd };
	::SetBkMode(hdc, TRANSPARENT); // for non-theme DrawText

	RECT rcArrow{ cbi.rcButton };
	rcArrow.left -= 1;

	HBRUSH hBrush = getBrushFromState(isDisabled, isHot);

	// Text part

	// CBS_DROPDOWN and CBS_SIMPLE text is handled by parent by WM_CTLCOLOREDIT
	if (comboBoxData.m_cbStyle == CBS_DROPDOWNLIST)
	{
		// erase background on item change
		::FillRect(hdc, &rcClient, hBrush);

		if (const auto index = static_cast<int>(::SendMessage(hWnd, CB_GETCURSEL, 0, 0));
			index != CB_ERR)
		{
			const auto bufferLen = static_cast<size_t>(::SendMessage(hWnd, CB_GETLBTEXTLEN, static_cast<WPARAM>(index), 0));
			std::wstring buffer(bufferLen + 1, L'\0');
			::SendMessage(hWnd, CB_GETLBTEXT, static_cast<WPARAM>(index), reinterpret_cast<LPARAM>(buffer.data()));

			RECT rcText{ cbi.rcItem };
			::InflateRect(&rcText, -2, 0);

			static constexpr DWORD dtFlags = DT_NOPREFIX | DT_LEFT | DT_VCENTER | DT_SINGLELINE;
			if (hasTheme)
			{
				DTTOPTS dtto{};
				dtto.dwSize = sizeof(DTTOPTS);
				dtto.dwFlags = DTT_TEXTCOLOR;
				dtto.crText = isDisabled ? DarkMode::getDisabledTextColor() : DarkMode::getTextColor();

				::DrawThemeTextEx(hTheme, hdc, CP_DROPDOWNITEM, isDisabled ? CBXSR_DISABLED : CBXSR_NORMAL, buffer.c_str(), -1, dtFlags, &rcText, &dtto);
			}
			else
			{
				::SetTextColor(hdc, isDisabled ? DarkMode::getDisabledTextColor() : DarkMode::getTextColor());
				::DrawText(hdc, buffer.c_str(), -1, &rcText, dtFlags);
			}
		}

		hasFocus = ::GetFocus() == hWnd;
		if (!isDisabled && hasFocus && ::SendMessage(hWnd, CB_GETDROPPEDSTATE, 0, 0) == FALSE)
		{
			::DrawFocusRect(hdc, &cbi.rcItem);
		}
	}
	else if (cbi.hwndItem != nullptr)
	{
		hasFocus = ::GetFocus() == cbi.hwndItem;

		::FillRect(hdc, &rcArrow, hBrush);
	}

	const HPEN hPen = getEdgePenFromState(isDisabled, isHot || hasFocus || comboBoxData.m_cbStyle == CBS_SIMPLE);
	const auto holdPen = dmlib_paint::GdiObject{ hdc, hPen, true};

	// Drop down arrow part
	if (comboBoxData.m_cbStyle != CBS_SIMPLE)
	{
		if (hasTheme
			&& (DarkMode::isExperimentalSupported()
				|| !DarkMode::isDarkDmTypeUsed()))
		{
			const RECT rcThemedArrow{ rcArrow.left, rcArrow.top - 1, rcArrow.right, rcArrow.bottom - 1 };
			::DrawThemeBackground(hTheme, hdc, CP_DROPDOWNBUTTONRIGHT, isDisabled ? CBXSR_DISABLED : CBXSR_NORMAL, &rcThemedArrow, nullptr);
		}
		else
		{
			::SetTextColor(hdc, getColorFromState(isDisabled, isHot));
			static constexpr UINT dtFlags = DT_NOPREFIX | DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOCLIP;
			::DrawText(hdc, dmlib_glyph::kArrowDown, -1, &rcArrow, dtFlags);
		}
	}

	// Frame part
	if (comboBoxData.m_cbStyle == CBS_DROPDOWNLIST)
	{
		::ExcludeClipRect(hdc, rcClient.left + 1, rcClient.top + 1, rcClient.right - 1, rcClient.bottom - 1);
	}
	else
	{
		::ExcludeClipRect(hdc, cbi.rcItem.left, cbi.rcItem.top, cbi.rcItem.right, cbi.rcItem.bottom);

		if (comboBoxData.m_cbStyle == CBS_SIMPLE && cbi.hwndList != nullptr)
		{
			RECT rcItem{ cbi.rcItem };
			::MapWindowPoints(cbi.hwndItem, hWnd, reinterpret_cast<LPPOINT>(&rcItem), 2);
			rcClient.bottom = rcItem.bottom;
		}

		RECT rcInner{ rcClient };
		::InflateRect(&rcInner, -1, -1);

		if (comboBoxData.m_cbStyle == CBS_DROPDOWN)
		{
			const std::array<POINT, 2> edge{ {
				{ rcArrow.left - 1, rcArrow.top },
				{ rcArrow.left - 1, rcArrow.bottom }
			} };
			::Polyline(hdc, edge.data(), static_cast<int>(edge.size()));

			::ExcludeClipRect(hdc, rcArrow.left - 1, rcArrow.top, rcArrow.right, rcArrow.bottom);

			rcInner.right = rcArrow.left - 1;
		}

		HPEN hInnerPen = ::CreatePen(PS_SOLID, 1, isDisabled ? DarkMode::getDlgBackgroundColor() : DarkMode::getBackgroundColor());
		dmlib_paint::paintFrameRect(hdc, rcInner, hInnerPen);
		::DeleteObject(hInnerPen);
		::InflateRect(&rcInner, -1, -1);
		::FillRect(hdc, &rcInner, isDisabled ? DarkMode::getDlgBackgroundBrush() : DarkMode::getCtrlBackgroundBrush());
	}

	static const int roundness = DarkMode::isAtLeastWindows11() ? dmlib_paint::kWin11CornerRoundness : 0;
	dmlib_paint::paintRoundFrameRect(hdc, rcClient, hPen, roundness, roundness);
}

/**
 * @brief Window subclass procedure for owner drawn combo box control.
 *
 * @param[in]   hWnd        Window handle being subclassed.
 * @param[in]   uMsg        Message identifier.
 * @param[in]   wParam      Message-specific data.
 * @param[in]   lParam      Message-specific data.
 * @param[in]   uIdSubclass Subclass identifier.
 * @param[in]   dwRefData   ComboBoxData instance.
 * @return LRESULT Result of message processing.
 *
 * @see DarkMode::setComboBoxCtrlSubclass()
 * @see DarkMode::removeComboBoxCtrlSubclass()
 */
LRESULT CALLBACK dmlib_subclass::ComboBoxSubclass(
	HWND hWnd,
	UINT uMsg,
	WPARAM wParam,
	LPARAM lParam,
	UINT_PTR uIdSubclass,
	DWORD_PTR dwRefData
)
{
	auto* pComboboxData = reinterpret_cast<ComboBoxData*>(dwRefData);
	auto& themeData = pComboboxData->m_themeData;
	const auto& hMemDC = pComboboxData->m_bufferData.getHMemDC();

	switch (uMsg)
	{
		case WM_NCDESTROY:
		{
			::RemoveWindowSubclass(hWnd, ComboBoxSubclass, uIdSubclass);
			std::unique_ptr<ComboBoxData> u_ptrData(pComboboxData);
			u_ptrData.reset(nullptr);
			break;
		}

		case WM_ERASEBKGND:
		{
			if (!DarkMode::isEnabled() || !themeData.ensureTheme(hWnd))
			{
				break;
			}

			if (pComboboxData->m_cbStyle != CBS_DROPDOWN
				&& reinterpret_cast<HDC>(wParam) != hMemDC)
			{
				return FALSE;
			}
			return TRUE;
		}

		case WM_PAINT:
		{
			if (!DarkMode::isEnabled())
			{
				break;
			}

			PAINTSTRUCT ps{};
			HDC hdc = ::BeginPaint(hWnd, &ps);

			if (pComboboxData->m_cbStyle != CBS_DROPDOWN)
			{
				if (!dmlib_paint::isRectValid(ps.rcPaint))
				{
					::EndPaint(hWnd, &ps);
					return 0;
				}

				dmlib_paint::PaintWithBuffer<ComboBoxData>(*pComboboxData, hdc, ps,
					[&]() { paintCombobox(hWnd, hMemDC, *pComboboxData); },
					hWnd);
			}
			else
			{
				paintCombobox(hWnd, hdc, *pComboboxData);
			}

			::EndPaint(hWnd, &ps);
			return 0;
		}

		case WM_ENABLE:
		{
			if (!DarkMode::isEnabled())
			{
				break;
			}

			const LRESULT retVal = ::DefSubclassProc(hWnd, uMsg, wParam, lParam);
			::RedrawWindow(hWnd, nullptr, nullptr, RDW_INVALIDATE);
			return retVal;
		}

		case WM_DPICHANGED_AFTERPARENT:
		{
			themeData.closeTheme();
			return 0;
		}

		case WM_THEMECHANGED:
		{
			themeData.closeTheme();
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
 * @brief Window subclass procedure for custom color for ComboBoxEx' list box and edit control.
 *
 * @param[in]   hWnd        Window handle being subclassed.
 * @param[in]   uMsg        Message identifier.
 * @param[in]   wParam      Message-specific data.
 * @param[in]   lParam      Message-specific data.
 * @param[in]   uIdSubclass Subclass identifier.
 * @param[in]   dwRefData   Reserved data (unused).
 * @return LRESULT Result of message processing.
 *
 * @see DarkMode::setComboBoxExCtrlSubclass()
 * @see DarkMode::removeComboBoxExCtrlSubclass()
 */
LRESULT CALLBACK dmlib_subclass::ComboBoxExSubclass(
	HWND hWnd,
	UINT uMsg,
	WPARAM wParam,
	LPARAM lParam,
	UINT_PTR uIdSubclass,
	[[maybe_unused]] DWORD_PTR dwRefData
)
{
	switch (uMsg)
	{
		case WM_NCDESTROY:
		{
			::RemoveWindowSubclass(hWnd, ComboBoxExSubclass, uIdSubclass);
			dmlib_hook::unhookSysColor();
			break;
		}

		case WM_ERASEBKGND:
		{
			if (!DarkMode::isEnabled())
			{
				break;
			}

			RECT rcClient{};
			::GetClientRect(hWnd, &rcClient);
			::FillRect(reinterpret_cast<HDC>(wParam), &rcClient, DarkMode::getDlgBackgroundBrush());
			return TRUE;
		}

		case WM_CTLCOLOREDIT:
		{
			if (!DarkMode::isEnabled())
			{
				break;
			}
			return DarkMode::onCtlColorCtrl(reinterpret_cast<HDC>(wParam));
		}

		case WM_CTLCOLORLISTBOX:
		{
			if (!DarkMode::isEnabled())
			{
				break;
			}
			return DarkMode::onCtlColorListbox(wParam, lParam);
		}

		// ComboBoxEx has only one child combo box, so only control-defined notification code is checked.
		// Hooking is done only when list box is about to show. And unhook when list box is closed.
		// This process is used to avoid visual glitches in other GUI.
		case WM_COMMAND:
		{
			if (!DarkMode::isEnabled())
			{
				break;
			}

			switch (HIWORD(wParam))
			{
				case CBN_DROPDOWN:
				{
					dmlib_hook::hookSysColor();
					break;
				}

				case CBN_CLOSEUP:
				{
					dmlib_hook::unhookSysColor();
					break;
				}

				default:
				{
					break;
				}
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
 * @brief Handles custom draw notifications for a list view's header control.
 *
 * Processes `NM_CUSTOMDRAW` message to provide custom color for header text.
 *
 * @param[in] lParam Pointer to `LPNMCUSTOMDRAW`.
 * @return `LRESULT` containing draw flags.
 */
[[nodiscard]] static LRESULT onCustomDrawLVHeader(LPARAM lParam) noexcept
{
	auto* lpnmcd = reinterpret_cast<LPNMCUSTOMDRAW>(lParam);
	switch (lpnmcd->dwDrawStage)
	{
		case CDDS_PREPAINT:
		{
			if (DarkMode::isExperimentalActive())
			{
				return CDRF_NOTIFYITEMDRAW;
			}
			return CDRF_DODEFAULT;
		}

		case CDDS_ITEMPREPAINT:
		{
			::SetTextColor(lpnmcd->hdc, DarkMode::getDarkerTextColor());

			return CDRF_NEWFONT;
		}

		default:
		{
			return CDRF_DODEFAULT;
		}
	}
}

/**
 * @brief Window subclass procedure for custom color for list view's gridlines and edit control.
 *
 * @param[in]   hWnd        Window handle being subclassed.
 * @param[in]   uMsg        Message identifier.
 * @param[in]   wParam      Message-specific data.
 * @param[in]   lParam      Message-specific data.
 * @param[in]   uIdSubclass Subclass identifier.
 * @param[in]   dwRefData   Reserved data (unused).
 * @return LRESULT Result of message processing.
 *
 * @see DarkMode::setListViewCtrlSubclass()
 * @see DarkMode::removeListViewCtrlSubclass()
 */
LRESULT CALLBACK dmlib_subclass::ListViewSubclass(
	HWND hWnd,
	UINT uMsg,
	WPARAM wParam,
	LPARAM lParam,
	UINT_PTR uIdSubclass,
	[[maybe_unused]] DWORD_PTR dwRefData
)
{
	switch (uMsg)
	{
		case WM_NCDESTROY:
		{
			::RemoveWindowSubclass(hWnd, ListViewSubclass, uIdSubclass);
			dmlib_hook::unhookSysColor();
			break;
		}

		// For gridlines
		case WM_PAINT:
		{
			if (!DarkMode::isEnabled())
			{
				break;
			}

			const auto lvStyle = ::GetWindowLongPtr(hWnd, GWL_STYLE) & LVS_TYPEMASK;
			const bool isReport = (lvStyle == LVS_REPORT);
			bool hasGridlines = false;
			if (isReport)
			{
				const auto lvExStyle = ListView_GetExtendedListViewStyle(hWnd);
				hasGridlines = (lvExStyle & LVS_EX_GRIDLINES) == LVS_EX_GRIDLINES;
			}

			if (hasGridlines)
			{
				dmlib_hook::hookSysColor();
				const LRESULT retVal = ::DefSubclassProc(hWnd, uMsg, wParam, lParam);
				dmlib_hook::unhookSysColor();
				return retVal;
			}
			break;
		}

		case WM_DPICHANGED_AFTERPARENT:
		{
			DarkMode::setDarkListViewCheckboxes(hWnd);
			return 0;
		}

		// For edit control, which is created when renaming/editing items
		case WM_CTLCOLOREDIT:
		{
			if (!DarkMode::isEnabled())
			{
				break;
			}
			return DarkMode::onCtlColorCtrl(reinterpret_cast<HDC>(wParam));
		}

		case WM_NOTIFY:
		{
			if (!DarkMode::isEnabled())
			{
				break;
			}

			if (reinterpret_cast<LPNMHDR>(lParam)->code == NM_CUSTOMDRAW)
			{
				return onCustomDrawLVHeader(lParam);
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
 * @brief Paints a single header control item (column header).
 *
 * Draws the item background (including hot/pressed visuals), optional sort arrow,
 * vertical separator edge, and item text. Uses visual styles (HTHEME) when available;
 * falls back to classic GDI text drawing otherwise.
 *
 * Paint logic:
 * - Draws sort arrows if `HDF_SORTUP` or `HDF_SORTDOWN` is set.
 * - Draws a vertical separator line with alignment between items.
 * - Draws the item text with alignment and pressed offset adjustments.
 * - Uses `DrawThemeTextEx` for themed text drawing, or `DrawText` otherwise.
 *
 * @param[in]   hWnd            Handle to the header control.
 * @param[in]   hdc             Device context to draw into.
 * @param[in]   headerData      Reference to the header's theme, state, and style data.
 * @param[in]   i               Zero-based index of the header item to paint.
 * @param[in]   rcItem          Rect used by Header_GetItemRect.
 * @param[in]   hasGridlines    True when parent ListView displays gridlines.
 * @param[in]   dtto            DTTOPTS for DrawThemeTextEx.
 */
static void paintHeaderItem(
	HWND hWnd,
	HDC hdc,
	const dmlib_subclass::HeaderData& headerData,
	int i,
	RECT& rcItem,
	bool hasGridlines,
	const DTTOPTS& dtto
) noexcept
{
	const HTHEME& hTheme = headerData.m_themeData.getHTheme();

	Header_GetItemRect(hWnd, i, &rcItem);
	const bool isOnItem = ::PtInRect(&rcItem, headerData.m_pt) == TRUE;

	// Different visual styles have different vertical alignments.
	// This part is for header item rectangle.
	if (headerData.m_hasBtnStyle && isOnItem)
	{
		RECT rcTmp{ rcItem };
		if (hasGridlines)
		{
			::OffsetRect(&rcTmp, 1, 0);
		}
		else if (DarkMode::isExperimentalActive())
		{
			::OffsetRect(&rcTmp, -1, 0);
		}
		::FillRect(hdc, &rcTmp, DarkMode::getHeaderHotBackgroundBrush());
	}

	std::wstring buffer(MAX_PATH, L'\0');
	HDITEM hdi{};
	hdi.mask = HDI_TEXT | HDI_FORMAT;
	hdi.pszText = buffer.data();
	hdi.cchTextMax = MAX_PATH - 1;

	Header_GetItem(hWnd, i, &hdi);

	// Sort arrows
	if (hTheme != nullptr
		&& ((hdi.fmt & HDF_SORTUP) == HDF_SORTUP
			|| (hdi.fmt & HDF_SORTDOWN) == HDF_SORTDOWN))
	{
		const int iStateID = ((hdi.fmt & HDF_SORTUP) == HDF_SORTUP) ? HSAS_SORTEDUP : HSAS_SORTEDDOWN;
		RECT rcArrow{ rcItem };
		SIZE szArrow{};
		if (SUCCEEDED(::GetThemePartSize(hTheme, hdc, HP_HEADERSORTARROW, iStateID, nullptr, TS_DRAW, &szArrow)))
		{
			rcArrow.bottom = szArrow.cy;
		}

		::DrawThemeBackground(hTheme, hdc, HP_HEADERSORTARROW, iStateID, &rcArrow, nullptr);
	}

	// Aligment for border
	LONG edgeX = rcItem.right;
	if (!hasGridlines)
	{
		--edgeX;
		if (DarkMode::isExperimentalActive())
		{
			--edgeX;
		}
	}

	const std::array<POINT, 2> edge{ {
		{ edgeX, rcItem.top },
		{ edgeX, rcItem.bottom }
	} };
	::Polyline(hdc, edge.data(), static_cast<int>(edge.size()));

	// Text draw part

	DWORD dtFlags = DT_VCENTER | DT_SINGLELINE | DT_WORD_ELLIPSIS | DT_HIDEPREFIX;
	if ((hdi.fmt & HDF_RIGHT) == HDF_RIGHT)
	{
		dtFlags |= DT_RIGHT;
	}
	else if ((hdi.fmt & HDF_CENTER) == HDF_CENTER)
	{
		dtFlags |= DT_CENTER;
	}

	static constexpr LONG lOffset = 6;
	static constexpr LONG rOffset = 8;

	rcItem.left += lOffset;
	rcItem.right -= rOffset;

	if (headerData.m_isPressed && isOnItem)
	{
		::OffsetRect(&rcItem, 1, 1);
	}

	if (hTheme != nullptr)
	{
		::DrawThemeTextEx(hTheme, hdc, HP_HEADERITEM, HIS_NORMAL, hdi.pszText, -1, dtFlags, &rcItem, &dtto);
	}
	else
	{
		::DrawText(hdc, hdi.pszText, -1, &rcItem, dtFlags);
	}
}

/**
 * @brief Custom paints a header control.
 *
 * Initializes variables for @ref paintHeaderItem.
 *
 * Paint logic:
 * - Determines if the parent list view is in report mode and has gridlines.
 * - Iterates over all header items
 *
 * @param[in]       hWnd        Handle to the header control.
 * @param[in]       hdc         Device context to draw into.
 * @param[in,out]   headerData  Reference to the header's theme, state, and style data.
 *
 * @see HeaderData
 * @see paintHeaderItem()
 */
static void paintHeader(HWND hWnd, HDC hdc, dmlib_subclass::HeaderData& headerData) noexcept
{
	auto& themeData = headerData.m_themeData;
	const auto& hTheme = themeData.getHTheme();
	const bool hasTheme = themeData.ensureTheme(hWnd);
	auto& fontData = headerData.m_fontData;

	::SetBkMode(hdc, TRANSPARENT);
	const auto holdPen = dmlib_paint::GdiObject{ hdc, DarkMode::getHeaderEdgePen(), true };

	RECT rcHeader{};
	::GetClientRect(hWnd, &rcHeader);
	::FillRect(hdc, &rcHeader, DarkMode::getHeaderBackgroundBrush());

	// Font part

	if (LOGFONT lf{};
		!fontData.hasFont()
		&& hasTheme
		&& SUCCEEDED(::GetThemeFont(hTheme, hdc, HP_HEADERITEM, HIS_NORMAL, TMT_FONT, &lf)))
	{
		fontData.setFont(::CreateFontIndirectW(&lf));
	}

	const auto holdFont = dmlib_paint::GdiObject{
		hdc,
		(fontData.hasFont()) ? fontData.getFont() : reinterpret_cast<HFONT>(::SendMessage(hWnd, WM_GETFONT, 0, 0)),
		true
	};

	DTTOPTS dtto{};
	dtto.dwSize = sizeof(DTTOPTS);
	if (hasTheme)
	{
		dtto.dwFlags = DTT_TEXTCOLOR;
		dtto.crText = DarkMode::getHeaderTextColor();
	}
	else
	{
		::SetTextColor(hdc, DarkMode::getHeaderTextColor());
	}

	// Special handling with gridlines

	HWND hList = ::GetParent(hWnd);
	bool hasGridlines = false;
	if (const auto lvStyle = ::GetWindowLongPtr(hList, GWL_STYLE) & LVS_TYPEMASK;
		lvStyle == LVS_REPORT)
	{
		const auto lvExStyle = ListView_GetExtendedListViewStyle(hList);
		hasGridlines = (lvExStyle & LVS_EX_GRIDLINES) == LVS_EX_GRIDLINES;
	}

	const auto count = Header_GetItemCount(hWnd);
	RECT rcItem{};
	for (int i = 0; i < count; i++)
	{
		paintHeaderItem(hWnd, hdc, headerData, i, rcItem, hasGridlines, dtto);
	}
}

/**
 * @brief Window subclass procedure for owner drawn header control.
 *
 * @param[in]   hWnd        Window handle being subclassed.
 * @param[in]   uMsg        Message identifier.
 * @param[in]   wParam      Message-specific data.
 * @param[in]   lParam      Message-specific data.
 * @param[in]   uIdSubclass Subclass identifier.
 * @param[in]   dwRefData   HeaderData instance.
 * @return LRESULT Result of message processing.
 *
 * @see DarkMode::setHeaderCtrlSubclass()
 * @see DarkMode::removeHeaderCtrlSubclass()
 */
LRESULT CALLBACK dmlib_subclass::HeaderSubclass(
	HWND hWnd,
	UINT uMsg,
	WPARAM wParam,
	LPARAM lParam,
	UINT_PTR uIdSubclass,
	DWORD_PTR dwRefData
)
{
	auto* pHeaderData = reinterpret_cast<HeaderData*>(dwRefData);
	auto& themeData = pHeaderData->m_themeData;
	const auto& hMemDC = pHeaderData->m_bufferData.getHMemDC();

	switch (uMsg)
	{
		case WM_NCDESTROY:
		{
			::RemoveWindowSubclass(hWnd, HeaderSubclass, uIdSubclass);
			std::unique_ptr<HeaderData> u_ptrData(pHeaderData);
			u_ptrData.reset(nullptr);
			break;
		}

		case WM_ERASEBKGND:
		{
			if (!DarkMode::isEnabled() || !themeData.ensureTheme(hWnd))
			{
				break;
			}

			if (reinterpret_cast<HDC>(wParam) != hMemDC)
			{
				return FALSE;
			}
			return TRUE;
		}

		case WM_PAINT:
		{
			if (!DarkMode::isEnabled())
			{
				break;
			}

			PAINTSTRUCT ps{};
			HDC hdc = ::BeginPaint(hWnd, &ps);

			if (!dmlib_paint::isRectValid(ps.rcPaint))
			{
				::EndPaint(hWnd, &ps);
				return 0;
			}

			dmlib_paint::PaintWithBuffer<HeaderData>(*pHeaderData, hdc, ps,
				[&]() { paintHeader(hWnd, hMemDC, *pHeaderData); },
				hWnd);

			::EndPaint(hWnd, &ps);
			return 0;
		}

		case WM_DPICHANGED_AFTERPARENT:
		{
			themeData.closeTheme();
			return 0;
		}

		case WM_THEMECHANGED:
		{
			themeData.closeTheme();
			break;
		}

		case WM_LBUTTONDOWN:
		{
			if (!pHeaderData->m_hasBtnStyle)
			{
				break;
			}

			pHeaderData->m_isPressed = true;
			break;
		}

		case WM_LBUTTONUP:
		{
			if (!pHeaderData->m_hasBtnStyle)
			{
				break;
			}

			pHeaderData->m_isPressed = false;
			break;
		}

		case WM_MOUSEMOVE:
		{
			if (!pHeaderData->m_hasBtnStyle || pHeaderData->m_isPressed)
			{
				break;
			}

			TRACKMOUSEEVENT tme{};

			if (!pHeaderData->m_isHot)
			{
				tme.cbSize = sizeof(TRACKMOUSEEVENT);
				tme.dwFlags = TME_LEAVE;
				tme.hwndTrack = hWnd;

				::TrackMouseEvent(&tme);

				pHeaderData->m_isHot = true;
			}

			pHeaderData->m_pt.x = GET_X_LPARAM(lParam);
			pHeaderData->m_pt.y = GET_Y_LPARAM(lParam);

			::InvalidateRect(hWnd, nullptr, FALSE);
			break;
		}

		case WM_MOUSELEAVE:
		{
			if (!pHeaderData->m_hasBtnStyle)
			{
				break;
			}

			const LRESULT retVal = ::DefSubclassProc(hWnd, uMsg, wParam, lParam);

			pHeaderData->m_isHot = false;
			pHeaderData->m_pt.x = LONG_MIN;
			pHeaderData->m_pt.y = LONG_MIN;

			::InvalidateRect(hWnd, nullptr, TRUE);

			return retVal;
		}

		default:
		{
			break;
		}
	}
	return ::DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

/**
 * @brief Custom paints a status bar control.
 *
 * Draws the background, text, part separators, and optional size grip using
 * custom brushes, pens, and fonts. Supports owner-drawn parts and adapts
 * to the control's style flags and part configuration.
 *
 * @param[in]       hWnd            Handle to the status bar control.
 * @param[in]       hdc             Device context to paint into.
 * @param[in,out]   statusBarData   Reference to the control's theme, buffer, and font data.
 *
 * @see StatusBarData
 */
static void paintStatusBar(HWND hWnd, HDC hdc, dmlib_subclass::StatusBarData& statusBarData) noexcept
{
	struct
	{
		int : sizeof(int) * CHAR_BIT; // horizontal not used
		int vertical = 0;
		int between = 0;
	} borders{};

	::SendMessage(hWnd, SB_GETBORDERS, 0, reinterpret_cast<LPARAM>(&borders));

	const auto nStyle = ::GetWindowLongPtr(hWnd, GWL_STYLE);
	const bool hasSizeGrip = (nStyle & SBARS_SIZEGRIP) == SBARS_SIZEGRIP;

	const auto holdPen = dmlib_paint::GdiObject{ hdc, DarkMode::getEdgePen(), true };
	const auto holdFont = dmlib_paint::GdiObject{ hdc, statusBarData.m_fontData.getFont(), true };

	::SetBkMode(hdc, TRANSPARENT);
	::SetTextColor(hdc, DarkMode::getTextColor());

	RECT rcClient{};
	::GetClientRect(hWnd, &rcClient);

	::FillRect(hdc, &rcClient, DarkMode::getBackgroundBrush());

	const auto nParts = static_cast<int>(::SendMessage(hWnd, SB_GETPARTS, 0, 0));
	std::wstring str;
	RECT rcPart{};
	RECT rcIntersect{};
	// no edge before size grip
	const int iLastDiv = nParts - (hasSizeGrip ? 1 : 0);
	// Don't draw edge if there is only one part without size grip.
	const bool drawEdge = (nParts >= 2 || !hasSizeGrip);
	for (int i = 0; i < nParts; ++i)
	{
		::SendMessage(hWnd, SB_GETRECT, static_cast<WPARAM>(i), reinterpret_cast<LPARAM>(&rcPart));
		if (::IntersectRect(&rcIntersect, &rcPart, &rcClient) == FALSE)
		{
			continue;
		}

		if (drawEdge && (i < iLastDiv))
		{
			const std::array<POINT, 2> edges{ {
				{ rcPart.right - borders.between, rcPart.top + 1 },
				{ rcPart.right - borders.between, rcPart.bottom - 3 }
			} };
			::Polyline(hdc, edges.data(), static_cast<int>(edges.size()));
		}

		rcPart.left += borders.between;
		rcPart.right -= borders.vertical;

		const LRESULT retValLen = ::SendMessage(hWnd, SB_GETTEXTLENGTH, static_cast<WPARAM>(i), 0);
		const DWORD cchText = LOWORD(retValLen);

		str.resize(static_cast<size_t>(cchText) + 1);
		const LRESULT retValText = ::SendMessage(hWnd, SB_GETTEXT, static_cast<WPARAM>(i), reinterpret_cast<LPARAM>(str.data()));

		// With `SBT_OWNERDRAW` flag parent will draw status bar.
		if (cchText == 0 && (HIWORD(retValLen) & SBT_OWNERDRAW) != 0)
		{
			const auto id = static_cast<UINT>(::GetDlgCtrlID(hWnd));
			DRAWITEMSTRUCT dis{
				0
				, 0
				, static_cast<UINT>(i)
				, ODA_DRAWENTIRE
				, id
				, hWnd
				, hdc
				, rcPart
				, static_cast<ULONG_PTR>(retValText)
			};

			::SendMessage(::GetParent(hWnd), WM_DRAWITEM, id, reinterpret_cast<LPARAM>(&dis));
		}
		else
		{
			::DrawText(hdc, str.c_str(), -1, &rcPart, DT_SINGLELINE | DT_VCENTER | DT_LEFT);
		}
	}

#if 0 // for horizontal edge
	POINT edgeHor[]{
		{rcClient.left, rcClient.top},
		{rcClient.right, rcClient.top}
	};
	Polyline(hdc, edgeHor, _countof(edgeHor));
#endif

		// draw optional size grip
	if (hasSizeGrip)
	{
		auto& themeData = statusBarData.m_themeData;
		if (themeData.ensureTheme(hWnd))
		{
			const auto& hTheme = themeData.getHTheme();
			SIZE szGrip{};
			::GetThemePartSize(hTheme, hdc, SP_GRIPPER, 0, &rcClient, TS_DRAW, &szGrip);
			RECT rcGrip{ rcClient };
			rcGrip.left = rcGrip.right - szGrip.cx;
			rcGrip.top = rcGrip.bottom - szGrip.cy;
			::DrawThemeBackground(hTheme, hdc, SP_GRIPPER, 0, &rcGrip, nullptr);
		}
	}
}

/**
 * @brief Window subclass procedure for owner drawn status bar control.
 *
 * @param[in]   hWnd        Window handle being subclassed.
 * @param[in]   uMsg        Message identifier.
 * @param[in]   wParam      Message-specific data.
 * @param[in]   lParam      Message-specific data.
 * @param[in]   uIdSubclass Subclass identifier.
 * @param[in]   dwRefData   StatusBarData instance.
 * @return LRESULT Result of message processing.
 *
 * @see DarkMode::setStatusBarCtrlSubclass()
 * @see DarkMode::removeStatusBarCtrlSubclass()
 */
LRESULT CALLBACK dmlib_subclass::StatusBarSubclass(
	HWND hWnd,
	UINT uMsg,
	WPARAM wParam,
	LPARAM lParam,
	UINT_PTR uIdSubclass,
	DWORD_PTR dwRefData
)
{
	auto* pStatusBarData = reinterpret_cast<StatusBarData*>(dwRefData);
	auto& themeData = pStatusBarData->m_themeData;
	const auto& hMemDC = pStatusBarData->m_bufferData.getHMemDC();

	switch (uMsg)
	{
		case WM_NCDESTROY:
		{
			::RemoveWindowSubclass(hWnd, StatusBarSubclass, uIdSubclass);
			std::unique_ptr<StatusBarData> u_ptrData(pStatusBarData);
			u_ptrData.reset(nullptr);
			break;
		}

		case WM_ERASEBKGND:
		{
			if (!DarkMode::isEnabled() || !themeData.ensureTheme(hWnd))
			{
				break;
			}

			if (reinterpret_cast<HDC>(wParam) != hMemDC)
			{
				return FALSE;
			}
			return TRUE;
		}

		case WM_PAINT:
		{
			if (!DarkMode::isEnabled())
			{
				break;
			}

			PAINTSTRUCT ps{};
			HDC hdc = ::BeginPaint(hWnd, &ps);

			if (!dmlib_paint::isRectValid(ps.rcPaint))
			{
				::EndPaint(hWnd, &ps);
				return 0;
			}

			dmlib_paint::PaintWithBuffer<StatusBarData>(*pStatusBarData, hdc, ps,
				[&]() { paintStatusBar(hWnd, hMemDC, *pStatusBarData); },
				hWnd);

			::EndPaint(hWnd, &ps);
			return 0;
		}

		case WM_DPICHANGED_AFTERPARENT:
		case WM_THEMECHANGED:
		{
			themeData.closeTheme();

			const auto lf = LOGFONT{ dmlib_dpi::getSysFontForDpi(::GetParent(hWnd), dmlib_dpi::FontType::status) };
			pStatusBarData->m_fontData.setFont(::CreateFontIndirectW(&lf));

			if (uMsg != WM_THEMECHANGED)
			{
				return 0;
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
 * @brief Calculates the filled and empty portions of a progress bar based on its current position.
 *
 * Retrieves the current progress position and range using `PBM_GETPOS` and `PBM_GETRANGE`,
 * then computes two rectangles:
 * - `rcFilled`: the portion of the progress bar that is filled.
 * - `rcEmpty`: the remaining portion that is unfilled.
 *
 * The function modifies `rcEmpty->left` to avoid overpainting the filled area.
 *
 * @param[in]   hWnd        Handle to the progress bar control.
 * @param[in]   rcEmpty     Pointer to the full client rectangle of the progress bar (in/out).
 * @param[in]   rcFilled    Pointer to a rectangle that will receive the filled portion (out).
 *
 * @note This function assumes horizontal progress bars.
 */
static void getProgressBarRects(HWND hWnd, RECT* rcEmpty, RECT* rcFilled) noexcept
{
	const auto pos = static_cast<int>(::SendMessage(hWnd, PBM_GETPOS, 0, 0));

	PBRANGE range{};
	::SendMessage(hWnd, PBM_GETRANGE, TRUE, reinterpret_cast<LPARAM>(&range));
	const int iMin = range.iLow;

	const int currPos = pos - iMin;
	if (currPos != 0)
	{
		const int totalWidth = rcEmpty->right - rcEmpty->left;
		rcFilled->left = rcEmpty->left;
		rcFilled->top = rcEmpty->top;
		rcFilled->bottom = rcEmpty->bottom;
		rcFilled->right = rcEmpty->left + static_cast<int>(static_cast<double>(currPos) / (range.iHigh - iMin) * totalWidth);

		rcEmpty->left = rcFilled->right; // to avoid painting under filled part
	}
}

/**
 * @brief Custom paints a progress bar control with dark mode styling.
 *
 * Draws the progress bar frame, filled portion, and background using custom
 * brushes and themed drawing. Uses the current progress state to determine the
 * visual style (e.g., normal, paused, error).
 *
 * @param[in]   hWnd            Handle to the progress bar control.
 * @param[in]   hdc             Device context to paint into.
 * @param[in]   progressBarData Reference to the control's theme and state data.
 *
 * @see ProgressBarData
 * @see DarkMode::getProgressBarRects()
 */
static void paintProgressBar(HWND hWnd, HDC hdc, const dmlib_subclass::ProgressBarData& progressBarData) noexcept
{
	const auto& hTheme = progressBarData.m_themeData.getHTheme();

	RECT rcClient{};
	::GetClientRect(hWnd, &rcClient);

	dmlib_paint::paintRoundFrameRect(hdc, rcClient, DarkMode::getEdgePen(), 0, 0);

	::InflateRect(&rcClient, -1, -1);
	rcClient.left = 1;

	RECT rcFill{};
	getProgressBarRects(hWnd, &rcClient, &rcFill);
	::DrawThemeBackground(hTheme, hdc, PP_FILL, progressBarData.m_iStateID, &rcFill, nullptr);
	::FillRect(hdc, &rcClient, DarkMode::getCtrlBackgroundBrush());
}

/**
 * @brief Get progress bar state when handling `PBM_SETSTATE` message.
 *
 * @param[in] wParam State of the progress bar.
 * @return int Fill state enum of the progress bar.
 *
 * @see dmlib_subclass::ProgressBarSubclass()
 */
[[nodiscard]] static int getProgressBarState(WPARAM wParam) noexcept
{
	switch (wParam)
	{
		case PBST_NORMAL:
		{
			return PBFS_NORMAL; // green
		}

		case PBST_ERROR:
		{
			return  PBFS_ERROR; // red
		}

		case PBST_PAUSED:
		{
			return PBFS_PAUSED; // yellow
		}

		default:
		{
			return PBFS_PARTIAL; // cyan
		}
	}
}

/**
 * @brief Window subclass procedure for owner drawn progress bar control.
 *
 * @param[in]   hWnd        Window handle being subclassed.
 * @param[in]   uMsg        Message identifier.
 * @param[in]   wParam      Message-specific data.
 * @param[in]   lParam      Message-specific data.
 * @param[in]   uIdSubclass Subclass identifier.
 * @param[in]   dwRefData   ProgressBarData instance.
 * @return LRESULT Result of message processing.
 *
 * @see DarkMode::setProgressBarCtrlSubclass()
 * @see DarkMode::removeProgressBarCtrlSubclass()
 */
LRESULT CALLBACK dmlib_subclass::ProgressBarSubclass(
	HWND hWnd,
	UINT uMsg,
	WPARAM wParam,
	LPARAM lParam,
	UINT_PTR uIdSubclass,
	DWORD_PTR dwRefData
)
{
	auto* pProgressBarData = reinterpret_cast<ProgressBarData*>(dwRefData);
	auto& themeData = pProgressBarData->m_themeData;
	const auto& hMemDC = pProgressBarData->m_bufferData.getHMemDC();

	switch (uMsg)
	{
		case WM_NCDESTROY:
		{
			::RemoveWindowSubclass(hWnd, ProgressBarSubclass, uIdSubclass);
			std::unique_ptr<ProgressBarData> u_ptrData(pProgressBarData);
			u_ptrData.reset(nullptr);
			break;
		}

		case WM_ERASEBKGND:
		{
			if (!DarkMode::isEnabled() || !themeData.ensureTheme(hWnd))
			{
				break;
			}

			if (reinterpret_cast<HDC>(wParam) != hMemDC)
			{
				return FALSE;
			}
			return TRUE;
		}

		case WM_PAINT:
		{
			if (!DarkMode::isEnabled())
			{
				break;
			}

			PAINTSTRUCT ps{};
			HDC hdc = ::BeginPaint(hWnd, &ps);

			if (!dmlib_paint::isRectValid(ps.rcPaint))
			{
				::EndPaint(hWnd, &ps);
				return 0;
			}

			dmlib_paint::PaintWithBuffer<ProgressBarData>(*pProgressBarData, hdc, ps,
				[&]() { paintProgressBar(hWnd, hMemDC, *pProgressBarData); },
				hWnd);

			::EndPaint(hWnd, &ps);
			return 0;
		}

		case WM_DPICHANGED_AFTERPARENT:
		{
			themeData.closeTheme();
			return 0;
		}

		case WM_THEMECHANGED:
		{
			themeData.closeTheme();
			break;
		}

		case PBM_SETSTATE:
		{
			pProgressBarData->m_iStateID = getProgressBarState(wParam);
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
 * @brief Window subclass procedure for better disabled state appearence for static control with text.
 *
 * @param[in]   hWnd        Window handle being subclassed.
 * @param[in]   uMsg        Message identifier.
 * @param[in]   wParam      Message-specific data.
 * @param[in]   lParam      Message-specific data.
 * @param[in]   uIdSubclass Subclass identifier.
 * @param[in]   dwRefData   StaticTextData instance.
 * @return LRESULT Result of message processing.
 *
 * @see DarkMode::setStaticTextCtrlSubclass()
 * @see DarkMode::removeStaticTextCtrlSubclass()
 */
LRESULT CALLBACK dmlib_subclass::StaticTextSubclass(
	HWND hWnd,
	UINT uMsg,
	WPARAM wParam,
	LPARAM lParam,
	UINT_PTR uIdSubclass,
	DWORD_PTR dwRefData
)
{
	auto* pStaticTextData = reinterpret_cast<StaticTextData*>(dwRefData);

	switch (uMsg)
	{
		case WM_NCDESTROY:
		{
			::RemoveWindowSubclass(hWnd, StaticTextSubclass, uIdSubclass);
			std::unique_ptr<StaticTextData> u_ptrData(pStaticTextData);
			u_ptrData.reset(nullptr);
			break;
		}

		case WM_ENABLE:
		{
			pStaticTextData->m_isEnabled = (wParam == TRUE);

			const auto nStyle = ::GetWindowLongPtr(hWnd, GWL_STYLE);
			if (!pStaticTextData->m_isEnabled)
			{
				::SetWindowLongPtr(hWnd, GWL_STYLE, nStyle & ~WS_DISABLED);
			}

			RECT rcClient{};
			::GetClientRect(hWnd, &rcClient);
			::MapWindowPoints(hWnd, ::GetParent(hWnd), reinterpret_cast<LPPOINT>(&rcClient), 2);
			::RedrawWindow(::GetParent(hWnd), &rcClient, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);

			if (!pStaticTextData->m_isEnabled)
			{
				::SetWindowLongPtr(hWnd, GWL_STYLE, nStyle | WS_DISABLED);
			}

			return 0;
		}

		default:
		{
			break;
		}
	}
	return ::DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

/**
 * @brief Custom paints a IP address control.
 *
 * Draws the IP address background and dot separators.
 *
 * @param[in]   hWnd    Handle to the IP address control.
 * @param[in]   hdc     Device context to paint into.
 *
 * @see dmlib_subclass::IPAddressSubclass()
 */
static void paintIPAddress(HWND hWnd, HDC hdc) noexcept
{
	const bool isEnabled = ::IsWindowEnabled(hWnd) == TRUE;

	RECT rcClient{};
	::GetClientRect(hWnd, &rcClient);

	if (isEnabled)
	{
		::FillRect(hdc, &rcClient, DarkMode::getCtrlBackgroundBrush());
		::SetTextColor(hdc, DarkMode::getDarkerTextColor());
		::SetBkColor(hdc, DarkMode::getCtrlBackgroundColor());
	}
	else
	{
		::FillRect(hdc, &rcClient, DarkMode::getDlgBackgroundBrush());
		::SetTextColor(hdc, DarkMode::getDisabledTextColor());
		::SetBkColor(hdc, DarkMode::getDlgBackgroundColor());
	}

	RECT rcDot{ rcClient };
	::InflateRect(&rcDot, -1, 0);
	const LONG wSection = ((rcDot.right - rcDot.left) / 4);
	rcDot.right = rcDot.left + (2 * wSection);
	::OffsetRect(&rcDot, 0, -1);

	const auto holdFont = dmlib_paint::GdiObject{ hdc, reinterpret_cast<HFONT>(::SendMessage(hWnd, WM_GETFONT, 0, 0)), true };
	static constexpr UINT dtFlags = DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX;

	for (int i = 0; i < 3; ++i)
	{
		::DrawText(hdc, L".", -1, &rcDot, dtFlags);
		rcDot.left += wSection;
		rcDot.right += wSection;
	}
}

/**
 * @brief Window subclass procedure for custom color for IP address control.
 *
 * @param[in]   hWnd        Window handle being subclassed.
 * @param[in]   uMsg        Message identifier.
 * @param[in]   wParam      Message-specific data.
 * @param[in]   lParam      Message-specific data.
 * @param[in]   uIdSubclass Subclass identifier.
 * @param[in]   dwRefData   Reserved data (unused).
 * @return LRESULT Result of message processing.
 *
 * @see dmlib_subclass::paintIPAddress()
 * @see DarkMode::setIPAddressCtrlSubclass()
 * @see DarkMode::removeIPAddressCtrlSubclass()
 */
LRESULT CALLBACK dmlib_subclass::IPAddressSubclass(
	HWND hWnd,
	UINT uMsg,
	WPARAM wParam,
	LPARAM lParam,
	UINT_PTR uIdSubclass,
	[[maybe_unused]] DWORD_PTR dwRefData
)
{
	switch (uMsg)
	{
		case WM_NCDESTROY:
		{
			::RemoveWindowSubclass(hWnd, IPAddressSubclass, uIdSubclass);
			break;
		}

		case WM_ERASEBKGND:
		{
			if (!DarkMode::isEnabled())
			{
				break;
			}

			RECT rcClient{};
			::GetClientRect(hWnd, &rcClient);
			::FillRect(
				reinterpret_cast<HDC>(wParam),
				&rcClient,
				(::IsWindowEnabled(hWnd) == TRUE)
					? DarkMode::getCtrlBackgroundBrush()
					: DarkMode::getDlgBackgroundBrush());
			return TRUE;
		}

		case WM_CTLCOLOREDIT:
		{
			if (!DarkMode::isEnabled())
			{
				break;
			}
			return DarkMode::onCtlColorCtrl(reinterpret_cast<HDC>(wParam));
		}

		case WM_PAINT:
		{
			if (!DarkMode::isEnabled())
			{
				break;
			}

			PAINTSTRUCT ps{};
			HDC hdc = ::BeginPaint(hWnd, &ps);

			paintIPAddress(hWnd, hdc);

			::EndPaint(hWnd, &ps);
			return 0;
		}

		default:
		{
			break;
		}
	}
	return ::DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

/**
 * @brief Window subclass procedure for custom color for hot key control.
 *
 * @param[in]   hWnd        Window handle being subclassed.
 * @param[in]   uMsg        Message identifier.
 * @param[in]   wParam      Message-specific data.
 * @param[in]   lParam      Message-specific data.
 * @param[in]   uIdSubclass Subclass identifier.
 * @param[in]   dwRefData   Reserved data (unused).
 * @return LRESULT Result of message processing.
 *
 * @see DarkMode::setHotKeyCtrlSubclass()
 * @see DarkMode::removeHotKeyCtrlSubclass()
 */
LRESULT CALLBACK dmlib_subclass::HotKeySubclass(
	HWND hWnd,
	UINT uMsg,
	WPARAM wParam,
	LPARAM lParam,
	UINT_PTR uIdSubclass,
	[[maybe_unused]] DWORD_PTR dwRefData
)
{
	switch (uMsg)
	{
		case WM_NCDESTROY:
		{
			::RemoveWindowSubclass(hWnd, HotKeySubclass, uIdSubclass);
			dmlib_hook::unhookSysColor();
			break;
		}

		case WM_ERASEBKGND:
		{
			if (!DarkMode::isEnabled())
			{
				break;
			}

			RECT rcClient{};
			::GetClientRect(hWnd, &rcClient);
			::FillRect(reinterpret_cast<HDC>(wParam), &rcClient, DarkMode::getDlgBackgroundBrush());
			return TRUE;
		}

		case WM_PAINT:
		{
			if (!DarkMode::isEnabled())
			{
				break;
			}

			dmlib_hook::hookSysColor();
			const LRESULT resVal = ::DefSubclassProc(hWnd, uMsg, wParam, lParam);
			dmlib_hook::unhookSysColor();
			return resVal;
		}

		default:
		{
			break;
		}
	}
	return ::DefSubclassProc(hWnd, uMsg, wParam, lParam);
}
