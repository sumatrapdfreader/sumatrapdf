// SPDX-License-Identifier: MPL-2.0

/*
 * Copyright (c) 2025 ozone10
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

// This file is part of darkmodelib library.


#include "StdAfx.h"

#include "DmlibSubclassWindow.h"

#include <windows.h>

#include <uxtheme.h>
#include <vsstyle.h>
#include <vssym32.h>

#include <array>
#include <memory>
#include <string>

#include "DarkModeSubclass.h"
#include "DmlibDpi.h"
#include "DmlibGlyph.h"
#include "DmlibPaintHelper.h"
#include "DmlibSubclass.h"
#include "DmlibSubclassControl.h"

#include "UAHMenuBar.h"

/**
 * @brief Window subclass procedure for handling `WM_ERASEBKGND` message.
 *
 * Handles `WM_ERASEBKGND` to fill the window's client area with the custom color brush,
 * preventing default light gray flicker or mismatched fill.
 *
 * @param[in]   hWnd        Window handle being subclassed.
 * @param[in]   uMsg        Message identifier.
 * @param[in]   wParam      Message-specific data.
 * @param[in]   lParam      Message-specific data.
 * @param[in]   uIdSubclass Subclass identifier.
 * @param[in]   dwRefData   Reserved data (unused).
 * @return LRESULT Result of message processing.
 *
 * @see DarkMode::setWindowEraseBgSubclass()
 * @see DarkMode::removeWindowEraseBgSubclass()
 */
LRESULT CALLBACK dmlib_subclass::WindowEraseBgSubclass(
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
			::RemoveWindowSubclass(hWnd, WindowEraseBgSubclass, uIdSubclass);
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

		default:
		{
			break;
		}
	}
	return ::DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

/**
 * @brief Helper function to get correct colors depending on control's classname and state.
 *
 * @param[in]   wParam      Message-specific data to get HDC.
 * @param[in]   lParam      Message-specific data to get child HWND.
 * @return The brush handle as LRESULT for background painting.
 */
static LRESULT onCtlColorStaticHelper(LPARAM lParam, WPARAM wParam)
{
	auto* hdc = reinterpret_cast<HDC>(wParam);
	auto hChild = reinterpret_cast<HWND>(lParam);

	const bool isChildEnabled = ::IsWindowEnabled(hChild) == TRUE;
	const std::wstring className = dmlib_subclass::getWndClassName(hChild);

	if (className == WC_EDIT)
	{
		return isChildEnabled ? DarkMode::onCtlColor(hdc) : DarkMode::onCtlColorDlg(hdc);
	}

	if (className == WC_LINK)
	{
		return DarkMode::onCtlColorDlgLinkText(hdc, isChildEnabled);
	}

	if (DWORD_PTR dwRefDataStaticText = 0;
		::GetWindowSubclass(hChild, dmlib_subclass::StaticTextSubclass, static_cast<UINT_PTR>(dmlib_subclass::SubclassID::staticText), &dwRefDataStaticText) == TRUE)
	{
		const bool isTextEnabled = (reinterpret_cast<dmlib_subclass::StaticTextData*>(dwRefDataStaticText))->m_isEnabled;
		return DarkMode::onCtlColorDlgStaticText(hdc, isTextEnabled);
	}
	return DarkMode::onCtlColorDlg(hdc);
}

/**
 * @brief Window subclass procedure for handling `WM_CTLCOLOR*` messages.
 *
 * Handles control drawing messages to apply foreground and background
 * styling based on control type and class.
 *
 * Handles:
 * - `WM_CTLCOLOREDIT`, `WM_CTLCOLORLISTBOX`, `WM_CTLCOLORDLG`, `WM_CTLCOLORSTATIC`
 * - `WM_PRINTCLIENT` for removing light border for push buttons in dark mode
 *
 * Cleans up subclass on `WM_NCDESTROY`
 *
 * Uses `DarkMode::onCtlColor*` utilities.
 *
 * @param[in]   hWnd        Window handle being subclassed.
 * @param[in]   uMsg        Message identifier.
 * @param[in]   wParam      Message-specific data.
 * @param[in]   lParam      Message-specific data.
 * @param[in]   uIdSubclass Subclass identifier.
 * @param[in]   dwRefData   Reserved data (unused).
 * @return LRESULT Result of message processing.
 *
 * @see DarkMode::onCtlColor()
 * @see DarkMode::onCtlColorDlg()
 * @see DarkMode::onCtlColorDlgStaticText()
 * @see DarkMode::onCtlColorDlgLinkText()
 * @see DarkMode::onCtlColorListbox()
 */
LRESULT CALLBACK dmlib_subclass::WindowCtlColorSubclass(
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
			::RemoveWindowSubclass(hWnd, WindowCtlColorSubclass, uIdSubclass);
			break;
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

		case WM_CTLCOLORDLG:
		{

			if (!DarkMode::isEnabled())
			{
				break;
			}
			return DarkMode::onCtlColorDlg(reinterpret_cast<HDC>(wParam));
		}

		case WM_CTLCOLORSTATIC:
		{
			if (!DarkMode::isEnabled())
			{
				break;
			}

			return onCtlColorStaticHelper(lParam, wParam);
		}

		case WM_PRINTCLIENT:
		{
			if (!DarkMode::isEnabled())
			{
				break;
			}
			return TRUE;
		}

		default:
		{
			break;
		}
	}
	return ::DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

/**
 * @brief Applies custom drawing to a toolbar items (buttons) during `CDDS_ITEMPREPAINT`
 *
 * Handles color assignment and background painting for toolbar buttons during the
 * `CDDS_ITEMPREPAINT` stage of `NMTBCUSTOMDRAW`. Applies appropriate brushes, pens,
 * and background drawing depending on the button state:
 * - **Hot**: Uses hot background and edge styling.
 * - **Checked**: Uses control background and standard edge styling.
 * - **Drop-down**: Calculates and paints iconic split-button drop arrow.
 *
 * Also configures transparency and color usage for text, hot-tracking, and background fills.
 * Ensures hot/checked states are visually overridden by custom color highlights.
 *
 * @param[in,out] lptbcd Reference to the toolbar's custom draw structure.
 * @return Flags to control draw behavior (`TBCDRF_USECDCOLORS`, `TBCDRF_NOBACKGROUND`, `CDRF_NOTIFYPOSTPAINT`).
 *
 * @note This function clears `CDIS_HOT`/`CDIS_CHECKED` to allow manual visual overrides.
 *
 * @see postpaintToolbarItem()
 * @see darkToolbarNotifyCustomDraw()
 */
[[nodiscard]] static LRESULT prepaintToolbarItem(LPNMTBCUSTOMDRAW& lptbcd) noexcept
{
	// Set colors

	lptbcd->hbrMonoDither = DarkMode::getBackgroundBrush();
	lptbcd->hbrLines = DarkMode::getEdgeBrush();
	lptbcd->hpenLines = DarkMode::getEdgePen();
	lptbcd->clrText = DarkMode::getDarkerTextColor();
	lptbcd->clrTextHighlight = DarkMode::getTextColor();
	lptbcd->clrBtnFace = DarkMode::getBackgroundColor();
	lptbcd->clrBtnHighlight = DarkMode::getCtrlBackgroundColor();
	lptbcd->clrHighlightHotTrack = DarkMode::getHotBackgroundColor();
	lptbcd->nStringBkMode = TRANSPARENT;
	lptbcd->nHLStringBkMode = TRANSPARENT;

	// Get styles and rectangles

	const bool isHot = (lptbcd->nmcd.uItemState & CDIS_HOT) == CDIS_HOT;
	const bool isChecked = (lptbcd->nmcd.uItemState & CDIS_CHECKED) == CDIS_CHECKED;

	RECT rcItem{ lptbcd->nmcd.rc };
	RECT rcDrop{};

	TBBUTTONINFOW tbi{};
	tbi.cbSize = sizeof(TBBUTTONINFOW);
	tbi.dwMask = TBIF_IMAGE | TBIF_STYLE;
	::SendMessage(lptbcd->nmcd.hdr.hwndFrom, TB_GETBUTTONINFO, lptbcd->nmcd.dwItemSpec, reinterpret_cast<LPARAM>(&tbi));

	const bool isIcon = tbi.iImage != I_IMAGENONE;
	const bool isDropDown = ((WORD{ tbi.fsStyle } & BTNS_DROPDOWN) == BTNS_DROPDOWN) && isIcon; // has 2 "buttons"
	if (isDropDown)
	{
		const auto idx = ::SendMessage(lptbcd->nmcd.hdr.hwndFrom, TB_COMMANDTOINDEX, lptbcd->nmcd.dwItemSpec, 0);
		::SendMessage(lptbcd->nmcd.hdr.hwndFrom, TB_GETITEMDROPDOWNRECT, static_cast<WPARAM>(idx), reinterpret_cast<LPARAM>(&rcDrop));

		rcItem.right = rcDrop.left;
	}

	static const int roundness = DarkMode::isAtLeastWindows11() ? dmlib_paint::kWin11CornerRoundness + 1 : 0;

	// Paint part

	if (isHot) // hot must have higher priority to overwrite checked state
	{
		if (!isIcon)
		{
			::FillRect(lptbcd->nmcd.hdc, &rcItem, DarkMode::getHotBackgroundBrush());
		}
		else
		{
			dmlib_paint::paintRoundRect(lptbcd->nmcd.hdc, rcItem, DarkMode::getHotEdgePen(), DarkMode::getHotBackgroundBrush(), roundness, roundness);
			if (isDropDown)
			{
				dmlib_paint::paintRoundRect(lptbcd->nmcd.hdc, rcDrop, DarkMode::getHotEdgePen(), DarkMode::getHotBackgroundBrush(), roundness, roundness);
			}
		}

		lptbcd->nmcd.uItemState &= ~static_cast<UINT>(CDIS_CHECKED | CDIS_HOT); // clears states to use custom highlight
	}
	else if (isChecked)
	{
		if (!isIcon)
		{
			::FillRect(lptbcd->nmcd.hdc, &rcItem, DarkMode::getCtrlBackgroundBrush());
		}
		else
		{
			dmlib_paint::paintRoundRect(lptbcd->nmcd.hdc, rcItem, DarkMode::getEdgePen(), DarkMode::getCtrlBackgroundBrush(), roundness, roundness);
			if (isDropDown)
			{
				dmlib_paint::paintRoundRect(lptbcd->nmcd.hdc, rcDrop, DarkMode::getEdgePen(), DarkMode::getCtrlBackgroundBrush(), roundness, roundness);
			}
		}

		lptbcd->nmcd.uItemState &= ~static_cast<UINT>(CDIS_CHECKED); // clears state to use custom highlight
	}

	LRESULT retVal = TBCDRF_USECDCOLORS;
	if ((lptbcd->nmcd.uItemState & CDIS_SELECTED) == CDIS_SELECTED)
	{
		retVal |= TBCDRF_NOBACKGROUND;
	}

	if (isDropDown)
	{
		retVal |= CDRF_NOTIFYPOSTPAINT;
	}

	return retVal;
}

/**
 * @brief Applies custom drawing to a toolbar items (buttons) during `CDDS_ITEMPOSTPAINT.
 *
 * Paints arrow glyph with custom color over system black "down triangle" for button with style `BTNS_DROPDOWN`.
 * Triggered by `CDRF_NOTIFYPOSTPAINT` from @ref prepaintToolbarItem.
 *
 * Logic:
 * - Retrieves the drop-down rectangle via `TB_GETITEMDROPDOWNRECT`.
 * - Selects the toolbar font and draws a centered arrow glyph with custom text color.
 *
 * @param[in] lptbcd Reference to `LPNMTBCUSTOMDRAW`.
 * @return `CDRF_DODEFAULT` to let default text/icon drawing proceed normally.
 *
 * @note Only applies to iconic buttons.
 *
 * @see prepaintToolbarItem()
 * @see darkToolbarNotifyCustomDraw()
 */
[[nodiscard]] static LRESULT postpaintToolbarItem(const LPNMTBCUSTOMDRAW& lptbcd) noexcept
{
	TBBUTTONINFOW tbi{};
	tbi.cbSize = sizeof(TBBUTTONINFOW);
	tbi.dwMask = TBIF_IMAGE;
	::SendMessage(lptbcd->nmcd.hdr.hwndFrom, TB_GETBUTTONINFO, lptbcd->nmcd.dwItemSpec, reinterpret_cast<LPARAM>(&tbi));

	if (tbi.iImage == I_IMAGENONE)
	{
		return CDRF_DODEFAULT;
	}

	RECT rcArrow{};
	const auto idx = ::SendMessage(lptbcd->nmcd.hdr.hwndFrom, TB_COMMANDTOINDEX, lptbcd->nmcd.dwItemSpec, 0);
	::SendMessage(lptbcd->nmcd.hdr.hwndFrom, TB_GETITEMDROPDOWNRECT, static_cast<WPARAM>(idx), reinterpret_cast<LPARAM>(&rcArrow));
	rcArrow.left += 1;
	rcArrow.bottom -= dmlib_dpi::scale(3, lptbcd->nmcd.hdr.hwndFrom);

	::SetBkMode(lptbcd->nmcd.hdc, TRANSPARENT);
	::SetTextColor(lptbcd->nmcd.hdc, DarkMode::getTextColor());

	const auto hFont = dmlib_paint::GdiObject{ lptbcd->nmcd.hdc, lptbcd->nmcd.hdr.hwndFrom };
	static constexpr UINT dtFlags = DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOCLIP | DT_NOPREFIX;
	::DrawText(lptbcd->nmcd.hdc, dmlib_glyph::kTriangleDown, -1, &rcArrow, dtFlags);

	return CDRF_DODEFAULT;
}

/**
 * @brief Handles custom draw notifications for a toolbar control.
 *
 * Processes `NMTBCUSTOMDRAW` messages to provide custom color painting
 * at each stage of the custom draw cycle:
 * - **CDDS_PREPAINT**: Fills the toolbar background and requests item-level drawing.
 * - **CDDS_ITEMPREPAINT**: Applies custom item painting via @ref prepaintToolbarItem.
 * - **CDDS_ITEMPOSTPAINT**: Paints dropdown arrows glyphs via @ref postpaintToolbarItem.
 *
 * @param[in]   hWnd        Handle to the toolbar control.
 * @param[in]   uMsg        Should be `WM_NOTIFY` with custom draw type (forwarded to default subclass processing).
 * @param[in]   wParam      Message parameter (forwarded to default subclass processing).
 * @param[in]   lParam      Pointer to `NMTBCUSTOMDRAW`.
 * @return `LRESULT` containing draw flags or the result of default subclass processing.
 *
 * @see prepaintToolbarItem()
 * @see postpaintToolbarItem()
 */
[[nodiscard]] static LRESULT darkToolbarNotifyCustomDraw(
	HWND hWnd,
	UINT uMsg,
	WPARAM wParam,
	LPARAM lParam
) noexcept
{
	switch (auto* lptbcd = reinterpret_cast<LPNMTBCUSTOMDRAW>(lParam);
		lptbcd->nmcd.dwDrawStage)
	{
		case CDDS_PREPAINT:
		{
			::FillRect(lptbcd->nmcd.hdc, &lptbcd->nmcd.rc, DarkMode::getDlgBackgroundBrush());
			return CDRF_NOTIFYITEMDRAW | CDRF_NOTIFYPOSTPAINT;
		}

		case CDDS_ITEMPREPAINT:
		{
			return prepaintToolbarItem(lptbcd);
		}

		case CDDS_ITEMPOSTPAINT:
		{
			return postpaintToolbarItem(lptbcd);
		}

		default:
		{
			break;
		}
	}
	return ::DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

/**
 * @brief Applies custom drawing to a list view item during `CDDS_ITEMPREPAINT`.
 *
 * Sets text/background colors and fills the item rectangle based on state and style.
 * Handles list view custom colors and styles, and adapts to grid line configuration.
 *
 * Behavior:
 * - **Selected**: Uses `DarkMode::getCtrlBackground*()` colors and text brush.
 * - **Hot**: Uses `DarkMode::getHotBackground()` colors with optional hover frame.
 * - **Gridlines active**: Fills the entire row background, column by column.
 *
 * @param[in,out]   lplvcd          Reference to `LPNMLVCUSTOMDRAW`.
 * @param[in]       isReport        Whether list view is in `LVS_REPORT` mode.
 * @param[in]       hasGridLines    Whether grid lines are enabled (`LVS_EX_GRIDLINES`).
 *
 * @see darkListViewNotifyCustomDraw()
 */
static void prepaintListViewItem(LPNMLVCUSTOMDRAW& lplvcd, bool isReport, bool hasGridLines) noexcept
{
	const auto& hList = lplvcd->nmcd.hdr.hwndFrom;
	const bool isSelected = ListView_GetItemState(hList, lplvcd->nmcd.dwItemSpec, LVIS_SELECTED) == LVIS_SELECTED;
	const bool isFocused = ListView_GetItemState(hList, lplvcd->nmcd.dwItemSpec, LVIS_FOCUSED) == LVIS_FOCUSED;
	const bool isHot = (lplvcd->nmcd.uItemState & CDIS_HOT) == CDIS_HOT;

	HBRUSH hBrush = nullptr;

	if (isSelected)
	{
		lplvcd->clrText = DarkMode::getTextColor();
		lplvcd->clrTextBk = DarkMode::getCtrlBackgroundColor();
		hBrush = DarkMode::getCtrlBackgroundBrush();
	}
	else if (isHot)
	{
		lplvcd->clrText = DarkMode::getTextColor();
		lplvcd->clrTextBk = DarkMode::getHotBackgroundColor();
		hBrush = DarkMode::getHotBackgroundBrush();
	}

	if (hBrush != nullptr)
	{
		if (!isReport || hasGridLines)
		{
			::FillRect(lplvcd->nmcd.hdc, &lplvcd->nmcd.rc, hBrush);
		}
		else
		{
			auto* hHeader = ListView_GetHeader(hList);
			const auto nCol = Header_GetItemCount(hHeader);
			const LONG paddingLeft = DarkMode::isThemeDark() ? 1 : 0;
			const LONG paddingRight = DarkMode::isThemeDark() ? 2 : 1;

			const auto lvii = LVITEMINDEX{ static_cast<int>(lplvcd->nmcd.dwItemSpec), 0 };
			RECT rcSubitem{
				lplvcd->nmcd.rc.left
				, lplvcd->nmcd.rc.top
				, lplvcd->nmcd.rc.left + ListView_GetColumnWidth(hList, 0) - paddingRight
				, lplvcd->nmcd.rc.bottom
			};
			::FillRect(lplvcd->nmcd.hdc, &rcSubitem, hBrush);

			for (int i = 1; i < nCol; ++i)
			{
				ListView_GetItemIndexRect(hList, &lvii, i, LVIR_BOUNDS, &rcSubitem);
				rcSubitem.left -= paddingLeft;
				rcSubitem.right -= paddingRight;
				::FillRect(lplvcd->nmcd.hdc, &rcSubitem, hBrush);
			}
		}
	}
	else if (hasGridLines)
	{
		::FillRect(lplvcd->nmcd.hdc, &lplvcd->nmcd.rc, DarkMode::getViewBackgroundBrush());
	}

	if (isFocused)
	{
#if 0 // for testing
		::DrawFocusRect(lplvcd->nmcd.hdc, &lplvcd->nmcd.rc);
#endif
	}
	else if (!isSelected && isHot && !hasGridLines)
	{
		::FrameRect(lplvcd->nmcd.hdc, &lplvcd->nmcd.rc, DarkMode::getHotEdgeBrush());
	}
}

/**
 * @brief Handles custom draw notifications for a list view control.
 *
 * Processes `NMLVCUSTOMDRAW` messages to provide custom color painting
 * at each stage of the custom draw cycle:
 * - **CDDS_PREPAINT**: Optionally fills the list view with grid lines
 *                      with custom background color and requests item-level drawing.
 * - **CDDS_ITEMPREPAINT**: Applies custom item painting via @ref prepaintListViewItem.
 *
 * @param[in]   hWnd        Handle to the list view control.
 * @param[in]   uMsg        Should be `WM_NOTIFY` with custom draw type (forwarded to default subclass processing).
 * @param[in]   wParam      Message parameter (forwarded to default subclass processing).
 * @param[in]   lParam      Pointer to `NMLVCUSTOMDRAW`.
 * @return `LRESULT` containing draw flags or the result of default subclass processing.
 *
 * @see prepaintListViewItem()
 */
[[nodiscard]] static LRESULT darkListViewNotifyCustomDraw(
	HWND hWnd,
	UINT uMsg,
	WPARAM wParam,
	LPARAM lParam
) noexcept
{
	auto* lplvcd = reinterpret_cast<LPNMLVCUSTOMDRAW>(lParam);
	const auto& hList = lplvcd->nmcd.hdr.hwndFrom;
	const auto lvStyle = ::GetWindowLongPtr(hList, GWL_STYLE) & LVS_TYPEMASK;
	const bool isReport = (lvStyle == LVS_REPORT);
	bool hasGridlines = false;
	if (isReport)
	{
		const auto lvExStyle = ListView_GetExtendedListViewStyle(hList);
		hasGridlines = (lvExStyle & LVS_EX_GRIDLINES) == LVS_EX_GRIDLINES;
	}

	switch (lplvcd->nmcd.dwDrawStage)
	{
		case CDDS_PREPAINT:
		{
			if (isReport && hasGridlines)
			{
				::FillRect(lplvcd->nmcd.hdc, &lplvcd->nmcd.rc, DarkMode::getViewBackgroundBrush());
			}

			return CDRF_NOTIFYITEMDRAW;
		}

		case CDDS_ITEMPREPAINT:
		{
			prepaintListViewItem(lplvcd, isReport, hasGridlines);
			return CDRF_NEWFONT;
		}

		default:
		{
			break;
		}
	}
	return ::DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

/**
 * @brief Applies custom drawing to a tree view node during `CDDS_ITEMPREPAINT`.
 *
 * Colors the node background for selection/hot states, assigns text color,
 * and requests optional post-paint framing.
 *
 * @param[in,out] lptvcd Reference to `LPNMTVCUSTOMDRAW`.
 * @return Bitmask with `CDRF_NEWFONT`, `CDRF_NOTIFYPOSTPAINT` if drawing was applied.
 *
 * @see postpaintTreeViewItem()
 * @see darkTreeViewNotifyCustomDraw()
 */
[[nodiscard]] static LRESULT prepaintTreeViewItem(LPNMTVCUSTOMDRAW& lptvcd) noexcept
{
	LRESULT retVal = CDRF_DODEFAULT;

	if ((lptvcd->nmcd.uItemState & CDIS_SELECTED) == CDIS_SELECTED)
	{
		lptvcd->clrText = DarkMode::getTextColor();
		lptvcd->clrTextBk = DarkMode::getCtrlBackgroundColor();
		::FillRect(lptvcd->nmcd.hdc, &lptvcd->nmcd.rc, DarkMode::getCtrlBackgroundBrush());

		retVal |= CDRF_NEWFONT | CDRF_NOTIFYPOSTPAINT;
	}
	else if ((lptvcd->nmcd.uItemState & CDIS_HOT) == CDIS_HOT)
	{
		lptvcd->clrText = DarkMode::getTextColor();
		lptvcd->clrTextBk = DarkMode::getHotBackgroundColor();

		if (DarkMode::isAtLeastWindows10()
			|| static_cast<DarkMode::TreeViewStyle>(DarkMode::getTreeViewStyle()) == DarkMode::TreeViewStyle::light)
		{
			::FillRect(lptvcd->nmcd.hdc, &lptvcd->nmcd.rc, DarkMode::getHotBackgroundBrush());
			retVal |= CDRF_NOTIFYPOSTPAINT;
		}
		retVal |= CDRF_NEWFONT;
	}

	return retVal;
}

/**
 * @brief Applies custom drawing to a tree view node during `CDDS_ITEMPOSTPAINT`.
 *
 * Paints a frame around a tree view node after painting based on state.
 *
 * @param[in] lptvcd Reference to `LPNMTVCUSTOMDRAW`.
 *
 * @see prepaintTreeViewItem()
 * @see darkTreeViewNotifyCustomDraw()
 */
static void postpaintTreeViewItem(const LPNMTVCUSTOMDRAW& lptvcd) noexcept
{
	RECT rcFrame{ lptvcd->nmcd.rc };
	::InflateRect(&rcFrame, 1, 0);

	if ((lptvcd->nmcd.uItemState & CDIS_HOT) == CDIS_HOT)
	{
		dmlib_paint::paintRoundFrameRect(lptvcd->nmcd.hdc, rcFrame, DarkMode::getHotEdgePen(), 0, 0);
	}
	else if ((lptvcd->nmcd.uItemState & CDIS_SELECTED) == CDIS_SELECTED)
	{
		dmlib_paint::paintRoundFrameRect(lptvcd->nmcd.hdc, rcFrame, DarkMode::getEdgePen(), 0, 0);
	}
}

/**
 * @brief Handles custom draw notifications for a tree view control.
 *
 * Processes `NMTVCUSTOMDRAW` messages to provide custom color painting
 * at each stage of the custom draw cycle:
 * - **CDDS_PREPAINT**: Requests item-level drawing.
 * - **CDDS_ITEMPREPAINT**: Applies custom item painting based on state via @ref prepaintTreeViewItem.
 * - **CDDS_ITEMPOSTPAINT**: Paints frames based on state via @ref postpaintTreeViewItem.
 *
 * @param[in]   hWnd        Handle to the tree view control.
 * @param[in]   uMsg        Should be `WM_NOTIFY` with custom draw type (forwarded to default subclass processing).
 * @param[in]   wParam      Message parameter (forwarded to default subclass processing).
 * @param[in]   lParam      Pointer to `NMTVCUSTOMDRAW`.
 * @return `LRESULT` containing draw flags or the result of default subclass processing.
 *
 * @see prepaintTreeViewItem()
 * @see postpaintTreeViewItem()
 */
[[nodiscard]] static LRESULT darkTreeViewNotifyCustomDraw(
	HWND hWnd,
	UINT uMsg,
	WPARAM wParam,
	LPARAM lParam
) noexcept
{
	switch (auto* lptvcd = reinterpret_cast<LPNMTVCUSTOMDRAW>(lParam);
		lptvcd->nmcd.dwDrawStage)
	{
		case CDDS_PREPAINT:
		{
			return CDRF_NOTIFYITEMDRAW;
		}

		case CDDS_ITEMPREPAINT:
		{
			const LRESULT retVal = prepaintTreeViewItem(lptvcd);
			if (retVal == CDRF_DODEFAULT)
			{
				break;
			}
			return retVal;
		}

		case CDDS_ITEMPOSTPAINT:
		{
			postpaintTreeViewItem(lptvcd);
			return CDRF_DODEFAULT;
		}

		default:
		{
			break;
		}
	}
	return ::DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

/**
 * @brief Applies custom drawing to a trackbar items during `CDDS_ITEMPREPAINT`.
 *
 * Colors the trackbar thumb background for selection state,
 * and colors the trackbar slider based on if tracbar is enabled.
 * For trackbar with style `TBS_AUTOTICKS` default handling is used.
 *
 * @param[in] lpnmcd Reference to `LPNMCUSTOMDRAW`.
 * @return `CDRF_SKIPDEFAULT` if drawing was applied.
 *
 * @see darkTrackbarNotifyCustomDraw()
 */
[[nodiscard]] static LRESULT prepaintTrackbarItem(const LPNMCUSTOMDRAW& lpnmcd) noexcept
{
	LRESULT retVal = CDRF_DODEFAULT;

	switch (lpnmcd->dwItemSpec)
	{
		case TBCD_TICS:
		{
			break;
		}

		case TBCD_THUMB:
		{
			if ((lpnmcd->uItemState & CDIS_SELECTED) == CDIS_SELECTED)
			{
				::FillRect(lpnmcd->hdc, &lpnmcd->rc, DarkMode::getCtrlBackgroundBrush());
				retVal = CDRF_SKIPDEFAULT;
			}
			break;
		}

		case TBCD_CHANNEL: // slider
		{
			if (::IsWindowEnabled(lpnmcd->hdr.hwndFrom) == FALSE)
			{
				::FillRect(lpnmcd->hdc, &lpnmcd->rc, DarkMode::getDlgBackgroundBrush());
				dmlib_paint::paintRoundFrameRect(lpnmcd->hdc, lpnmcd->rc, DarkMode::getEdgePen(), 0, 0);
			}
			else
			{
				::FillRect(lpnmcd->hdc, &lpnmcd->rc, DarkMode::getCtrlBackgroundBrush());
			}

			retVal = CDRF_SKIPDEFAULT;
			break;
		}

		default:
		{
			break;
		}
	}

	return retVal;
}

/**
 * @brief Handles custom draw notifications for a trackbar control.
 *
 * Processes `NMCUSTOMDRAW` messages to provide custom color painting
 * at each stage of the custom draw cycle:
 * - **CDDS_PREPAINT**: Requests item-level drawing.
 * - **CDDS_ITEMPREPAINT**: Applies custom item painting based on item type via @ref prepaintTrackbarItem.
 *
 * @param[in]   hWnd        Handle to the trackbar control.
 * @param[in]   uMsg        Should be `WM_NOTIFY` with custom draw type (forwarded to default subclass processing).
 * @param[in]   wParam      Message parameter (forwarded to default subclass processing).
 * @param[in]   lParam      Pointer to `NMCUSTOMDRAW`.
 * @return `LRESULT` containing draw flags or the result of default subclass processing.
 *
 * @see prepaintTrackbarItem()
 */
[[nodiscard]] static LRESULT darkTrackbarNotifyCustomDraw(
	HWND hWnd,
	UINT uMsg,
	WPARAM wParam,
	LPARAM lParam
) noexcept
{
	switch (auto* lpnmcd = reinterpret_cast<LPNMCUSTOMDRAW>(lParam);
		lpnmcd->dwDrawStage)
	{
		case CDDS_PREPAINT:
		{
			return CDRF_NOTIFYITEMDRAW;
		}

		case CDDS_ITEMPREPAINT:
		{
			const LRESULT retVal = prepaintTrackbarItem(lpnmcd);
			if (retVal == CDRF_DODEFAULT)
			{
				break;
			}
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
 * @brief Applies custom drawing to a rebar control during `CDDS_PREPAINT`.
 *
 * Paints chevrons and 'gripper' edges for all bands if applicable.
 *
 * @param[in] lpnmcd Reference to `LPNMCUSTOMDRAW`.
 * @return `CDRF_SKIPDEFAULT` if drawing was applied.
 *
 * @see darkRebarNotifyCustomDraw()
 */
[[nodiscard]] static LRESULT prepaintRebar(const LPNMCUSTOMDRAW& lpnmcd) noexcept
{
	::FillRect(lpnmcd->hdc, &lpnmcd->rc, DarkMode::getDlgBackgroundBrush());

	REBARBANDINFO rbBand{};
	rbBand.cbSize = sizeof(REBARBANDINFO);
	rbBand.fMask = RBBIM_STYLE | RBBIM_CHEVRONLOCATION | RBBIM_CHEVRONSTATE;

	const auto nBands = static_cast<UINT>(::SendMessage(lpnmcd->hdr.hwndFrom, RB_GETBANDCOUNT, 0, 0));
	for (UINT i = 0; i < nBands; ++i)
	{
		::SendMessage(lpnmcd->hdr.hwndFrom, RB_GETBANDINFO, static_cast<WPARAM>(i), reinterpret_cast<LPARAM>(&rbBand));

		// paints chevron
		if ((rbBand.fStyle & RBBS_USECHEVRON) == RBBS_USECHEVRON
			&& (rbBand.rcChevronLocation.right - rbBand.rcChevronLocation.left) > 0)
		{
			static const int roundness = DarkMode::isAtLeastWindows11() ? dmlib_paint::kWin11CornerRoundness + 1 : 0;

			const bool isHot = (rbBand.uChevronState & STATE_SYSTEM_HOTTRACKED) == STATE_SYSTEM_HOTTRACKED;
			const bool isPressed = (rbBand.uChevronState & STATE_SYSTEM_PRESSED) == STATE_SYSTEM_PRESSED;

			if (isHot)
			{
				dmlib_paint::paintRoundRect(lpnmcd->hdc, rbBand.rcChevronLocation, DarkMode::getHotEdgePen(), DarkMode::getHotBackgroundBrush(), roundness, roundness);
			}
			else if (isPressed)
			{
				dmlib_paint::paintRoundRect(lpnmcd->hdc, rbBand.rcChevronLocation, DarkMode::getEdgePen(), DarkMode::getCtrlBackgroundBrush(), roundness, roundness);
			}

			::SetTextColor(lpnmcd->hdc, isHot ? DarkMode::getTextColor() : DarkMode::getDarkerTextColor());
			::SetBkMode(lpnmcd->hdc, TRANSPARENT);

			const auto hFont = dmlib_paint::GdiObject{ lpnmcd->hdc, lpnmcd->hdr.hwndFrom };
			static constexpr UINT dtFlags = DT_CENTER | DT_TOP | DT_SINGLELINE | DT_NOCLIP | DT_NOPREFIX;
			::DrawText(lpnmcd->hdc, dmlib_glyph::kChevron, -1, &rbBand.rcChevronLocation, dtFlags);
		}

		// paints gripper edge
		if ((rbBand.fStyle & RBBS_GRIPPERALWAYS) == RBBS_GRIPPERALWAYS
			&& ((rbBand.fStyle & RBBS_FIXEDSIZE) != RBBS_FIXEDSIZE
				|| (rbBand.fStyle & RBBS_NOGRIPPER) != RBBS_NOGRIPPER))
		{
			auto holdPen = static_cast<HPEN>(::SelectObject(lpnmcd->hdc, DarkMode::getDarkerTextPen()));

			RECT rcBand{};
			::SendMessage(lpnmcd->hdr.hwndFrom, RB_GETRECT, static_cast<WPARAM>(i), reinterpret_cast<LPARAM>(&rcBand));

			static constexpr LONG offset = 5;
			const std::array<POINT, 2> edges{ {
				{ rcBand.left, rcBand.top + offset},
				{ rcBand.left, rcBand.bottom - offset}
			} };
			::Polyline(lpnmcd->hdc, edges.data(), static_cast<int>(edges.size()));

			::SelectObject(lpnmcd->hdc, holdPen);
		}
	}
	return CDRF_SKIPDEFAULT;
}

/**
 * @brief Handles custom draw notifications for a rebar control.
 *
 * Processes `NMCUSTOMDRAW` messages to provide custom color painting
 * at each stage of the custom draw cycle:
 * - **CDDS_PREPAINT**: Applies custom painting based on item type via @ref prepaintRebar.
 *
 * @param[in]   hWnd        Handle to the rebar control.
 * @param[in]   uMsg        Should be `WM_NOTIFY` with custom draw type (forwarded to default subclass processing).
 * @param[in]   wParam      Message parameter (forwarded to default subclass processing).
 * @param[in]   lParam      Pointer to `NMCUSTOMDRAW`.
 * @return `LRESULT` containing draw flags or the result of default subclass processing.
 *
 * @see prepaintRebar()
 */
[[nodiscard]] static LRESULT darkRebarNotifyCustomDraw(
	HWND hWnd,
	UINT uMsg,
	WPARAM wParam,
	LPARAM lParam
) noexcept
{
	if (auto* lpnmcd = reinterpret_cast<LPNMCUSTOMDRAW>(lParam);
		lpnmcd->dwDrawStage == CDDS_PREPAINT)
	{
		return prepaintRebar(lpnmcd);
	}
	return ::DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

/**
 * @brief Helper for handling `NM_CUSTOMDRAW` notification code.
 *
 * Handles `NM_CUSTOMDRAW` for custom draw for supported controls:
 * - toolbar, list view, tree view, trackbar, and rebar.
 *
 * @param[in]   hWnd        Window handle for specific control.
 * @param[in]   uMsg        Message identifier.
 * @param[in]   wParam      Message-specific data.
 * @param[in]   lParam      Message-specific data.
 * @return LRESULT Result of message processing.
 *
 * @see dmlib_subclass::WindowNotifySubclass()
 */
static LRESULT onNotifyCustomDraw(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if (auto* lpnmhdr = reinterpret_cast<LPNMHDR>(lParam);
		lpnmhdr->code == NM_CUSTOMDRAW)
	{
		const std::wstring className = dmlib_subclass::getWndClassName(lpnmhdr->hwndFrom);

		if (className == TOOLBARCLASSNAME)
		{
			return darkToolbarNotifyCustomDraw(hWnd, uMsg, wParam, lParam);
		}

		if (className == WC_LISTVIEW)
		{
			return darkListViewNotifyCustomDraw(hWnd, uMsg, wParam, lParam);
		}

		if (className == WC_TREEVIEW)
		{
			return darkTreeViewNotifyCustomDraw(hWnd, uMsg, wParam, lParam);
		}

		if (className == TRACKBAR_CLASS)
		{
			return darkTrackbarNotifyCustomDraw(hWnd, uMsg, wParam, lParam);
		}

		if (className == REBARCLASSNAME)
		{
			return darkRebarNotifyCustomDraw(hWnd, uMsg, wParam, lParam);
		}
	}
	return ::DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

/**
 * @brief Window subclass procedure for handling `WM_NOTIFY` message for custom draw for supported controls.
 *
 * Handles `WM_NOTIFY` for custom draw for supported controls:
 * - toolbar, list view, tree view, trackbar, and rebar.
 *
 * @param[in]   hWnd        Window handle being subclassed.
 * @param[in]   uMsg        Message identifier.
 * @param[in]   wParam      Message-specific data.
 * @param[in]   lParam      Message-specific data.
 * @param[in]   uIdSubclass Subclass identifier.
 * @param[in]   dwRefData   Reserved data (unused).
 * @return LRESULT Result of message processing.
 *
 * @see onNotifyCustomDraw()
 * @see DarkMode::setWindowNotifyCustomDrawSubclass()
 * @see DarkMode::removeWindowNotifyCustomDrawSubclass()
 */
LRESULT CALLBACK dmlib_subclass::WindowNotifySubclass(
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
			::RemoveWindowSubclass(hWnd, WindowNotifySubclass, uIdSubclass);
			break;
		}

		case WM_NOTIFY:
		{
			if (!DarkMode::isEnabled())
			{
				break;
			}

			return onNotifyCustomDraw(hWnd, uMsg, wParam, lParam);
		}

		default:
		{
			break;
		}
	}
	return ::DefSubclassProc(hWnd, uMsg, wParam, lParam);
}


/**
 * @brief Fills the menu bar background custom color.
 *
 * Uses `GetMenuBarInfo` and `GetWindowRect` to compute the menu bar rectangle
 * in client-relative coordinates, then fills it with @ref DarkMode::getDlgBackgroundBrush.
 *
 * @param[in]   hWnd    Handle to the window with a menu bar.
 * @param[in]   hdc     Target device context for painting.
 *
 * @note Offsets top slightly to account for non-client overlap.
 *
 * @see dmlib_subclass::WindowMenuBarSubclass()
 */
static void paintMenuBar(HWND hWnd, HDC hdc) noexcept
{
	// get the menubar rect
	MENUBARINFO mbi{};
	mbi.cbSize = sizeof(MENUBARINFO);
	::GetMenuBarInfo(hWnd, OBJID_MENU, 0, &mbi);

	RECT rcWindow{};
	::GetWindowRect(hWnd, &rcWindow);

	// the rcBar is offset by the window rect
	RECT rcBar{ mbi.rcBar };
	::OffsetRect(&rcBar, -rcWindow.left, -rcWindow.top);

	rcBar.top -= 1;

	::FillRect(hdc, &rcBar, DarkMode::getDlgBackgroundBrush());
}

/**
 * @brief Paints a single menu bar item with custom colors based on state.
 *
 * Measures and draws menu item text using `DrawThemeTextEx`, and
 * fills background using appropriate brush based on `ODS_*` item state.
 *
 * @param[in,out]   UDMI    Reference to `UAHDRAWMENUITEM` struct from `WM_UAHDRAWMENUITEM`.
 * @param[in]       hTheme  The themed handle to `VSCLASS_MENU` (via @ref ThemeData).
 *
 * @see dmlib_subclass::WindowMenuBarSubclass()
 */
static void paintMenuBarItems(UAHDRAWMENUITEM& UDMI, const HTHEME& hTheme)
{
	// get the menu item string
	std::wstring buffer(MAX_PATH, L'\0');
	MENUITEMINFO mii{};
	mii.cbSize = sizeof(MENUITEMINFO);
	mii.fMask = MIIM_STRING;
	mii.dwTypeData = buffer.data();
	mii.cch = MAX_PATH - 1;

	::GetMenuItemInfoW(UDMI.um.hmenu, static_cast<UINT>(UDMI.umi.iPosition), TRUE, &mii);

	// get the item state for drawing

	DWORD dwFlags = DT_CENTER | DT_SINGLELINE | DT_VCENTER;

	int iTextStateID = MBI_NORMAL;
	int iBackgroundStateID = MBI_NORMAL;
	if ((UDMI.dis.itemState & ODS_SELECTED) == ODS_SELECTED)
	{
		// clicked
		iTextStateID = MBI_PUSHED;
		iBackgroundStateID = MBI_PUSHED;
	}
	else if ((UDMI.dis.itemState & ODS_HOTLIGHT) == ODS_HOTLIGHT)
	{
		// hot tracking
		iTextStateID = ((UDMI.dis.itemState & ODS_INACTIVE) == ODS_INACTIVE) ? MBI_DISABLEDHOT : MBI_HOT;
		iBackgroundStateID = MBI_HOT;
	}
	else if (((UDMI.dis.itemState & ODS_GRAYED) == ODS_GRAYED)
		|| ((UDMI.dis.itemState & ODS_DISABLED) == ODS_DISABLED)
		|| ((UDMI.dis.itemState & ODS_INACTIVE) == ODS_INACTIVE))
	{
		// disabled / grey text / inactive
		iTextStateID = MBI_DISABLED;
		iBackgroundStateID = MBI_DISABLED;
	}
	else if ((UDMI.dis.itemState & ODS_DEFAULT) == ODS_DEFAULT)
	{
		// normal display
		iTextStateID = MBI_NORMAL;
		iBackgroundStateID = MBI_NORMAL;
	}

	if ((UDMI.dis.itemState & ODS_NOACCEL) == ODS_NOACCEL)
	{
		dwFlags |= DT_HIDEPREFIX;
	}

	switch (iBackgroundStateID)
	{
		case MBI_NORMAL:
		case MBI_DISABLED:
		{
			::FillRect(UDMI.um.hdc, &UDMI.dis.rcItem, DarkMode::getDlgBackgroundBrush());
			break;
		}

		case MBI_HOT:
		case MBI_DISABLEDHOT:
		{
			::FillRect(UDMI.um.hdc, &UDMI.dis.rcItem, DarkMode::getHotBackgroundBrush());
			break;
		}

		case MBI_PUSHED:
		case MBI_DISABLEDPUSHED:
		{
			::FillRect(UDMI.um.hdc, &UDMI.dis.rcItem, DarkMode::getCtrlBackgroundBrush());
			break;
		}

		default:
		{
			::DrawThemeBackground(hTheme, UDMI.um.hdc, MENU_BARITEM, iBackgroundStateID, &UDMI.dis.rcItem, nullptr);
			break;
		}
	}

	DTTOPTS dttopts{};
	dttopts.dwSize = sizeof(DTTOPTS);
	dttopts.dwFlags = DTT_TEXTCOLOR;
	switch (iTextStateID)
	{
		case MBI_NORMAL:
		case MBI_HOT:
		case MBI_PUSHED:
		{
			dttopts.crText = DarkMode::getTextColor();
			break;
		}

		case MBI_DISABLED:
		case MBI_DISABLEDHOT:
		case MBI_DISABLEDPUSHED:
		{
			dttopts.crText = DarkMode::getDisabledTextColor();
			break;
		}

		default:
		{
			break;
		}
	}

	::DrawThemeTextEx(hTheme, UDMI.um.hdc, MENU_BARITEM, iTextStateID, buffer.c_str(), static_cast<int>(mii.cch), dwFlags, &UDMI.dis.rcItem, &dttopts);
}

/**
 * @brief Over-paints the 1-pixel light line under a menu bar with custom color.
 *
 * Called post-paint to overwrite non-client leftovers that break custom color styling.
 * Computes exact line position based on `MenuBarInfo`, and fills with custom color.
 *
 * @param[in] hWnd Handle to the window with a menu bar.
 *
 * @see dmlib_subclass::WindowMenuBarSubclass()
 */
static void drawUAHMenuNCBottomLine(HWND hWnd) noexcept
{
	MENUBARINFO mbi{};
	mbi.cbSize = sizeof(MENUBARINFO);
	if (::GetMenuBarInfo(hWnd, OBJID_MENU, 0, &mbi) == FALSE)
	{
		return;
	}

	RECT rcClient{};
	::GetClientRect(hWnd, &rcClient);
	::MapWindowPoints(hWnd, nullptr, reinterpret_cast<POINT*>(&rcClient), 2);

	RECT rcWindow{};
	::GetWindowRect(hWnd, &rcWindow);

	::OffsetRect(&rcClient, -rcWindow.left, -rcWindow.top);

	// the rcBar is offset by the window rect
	RECT rcAnnoyingLine{ rcClient };
	rcAnnoyingLine.bottom = rcAnnoyingLine.top;
	rcAnnoyingLine.top--;


	HDC hdc = ::GetWindowDC(hWnd);
	::FillRect(hdc, &rcAnnoyingLine, DarkMode::getDlgBackgroundBrush());
	::ReleaseDC(hWnd, hdc);
}

/**
 * @brief Window subclass procedure for custom color for themed menu bar.
 *
 * Applies custom colors for menu bar, but not for popup menus.
 *
 * @param[in]   hWnd        Window handle being subclassed.
 * @param[in]   uMsg        Message identifier.
 * @param[in]   wParam      Message-specific data.
 * @param[in]   lParam      Message-specific data.
 * @param[in]   uIdSubclass Subclass identifier.
 * @param[in]   dwRefData   ThemeData instance.
 * @return LRESULT Result of message processing.
 *
 * @see DarkMode::setWindowMenuBarSubclass()
 * @see DarkMode::removeWindowMenuBarSubclass()
 */
LRESULT CALLBACK dmlib_subclass::WindowMenuBarSubclass(
	HWND hWnd,
	UINT uMsg,
	WPARAM wParam,
	LPARAM lParam,
	UINT_PTR uIdSubclass,
	DWORD_PTR dwRefData
)
{
	auto* pMenuThemeData = reinterpret_cast<ThemeData*>(dwRefData);

	if (uMsg != WM_NCDESTROY && (!DarkMode::isEnabled() || !pMenuThemeData->ensureTheme(hWnd)))
	{
		return ::DefSubclassProc(hWnd, uMsg, wParam, lParam);
	}

	switch (uMsg)
	{
		case WM_NCDESTROY:
		{
			::RemoveWindowSubclass(hWnd, WindowMenuBarSubclass, uIdSubclass);
			std::unique_ptr<ThemeData> ptrData(pMenuThemeData);
			break;
		}

		case WM_UAHDRAWMENU:
		{
			auto* pUDM = reinterpret_cast<UAHMENU*>(lParam);
			paintMenuBar(hWnd, pUDM->hdc);

			return 0;
		}

		case WM_UAHDRAWMENUITEM:
		{
			const auto& hTheme = pMenuThemeData->getHTheme();
			auto* pUDMI = reinterpret_cast<UAHDRAWMENUITEM*>(lParam);
			paintMenuBarItems(*pUDMI, hTheme);

			return 0;
		}

#if 0 // for debugging
		case WM_UAHMEASUREMENUITEM:
		{
			auto* pMMI = reinterpret_cast<UAHMEASUREMENUITEM*>(lParam);
			return ::DefSubclassProc(hWnd, uMsg, wParam, lParam);
		}
#endif

		case WM_DPICHANGED:
		case WM_DPICHANGED_AFTERPARENT:
		case WM_THEMECHANGED:
		{
			pMenuThemeData->closeTheme();
			break;
		}

		case WM_NCACTIVATE:
		case WM_NCPAINT:
		{
			const LRESULT retVal = ::DefSubclassProc(hWnd, uMsg, wParam, lParam);
			drawUAHMenuNCBottomLine(hWnd);
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
 * @brief Window subclass procedure for handling `WM_SETTINGCHANGE` message.
 *
 * Handles `WM_SETTINGCHANGE` to perform changes for dark mode based on system setting.
 *
 * @param[in]   hWnd        Window handle being subclassed.
 * @param[in]   uMsg        Message identifier.
 * @param[in]   wParam      Message-specific data.
 * @param[in]   lParam      Message-specific data.
 * @param[in]   uIdSubclass Subclass identifier.
 * @param[in]   dwRefData   Reserved data (unused).
 * @return LRESULT Result of message processing.
 *
 * @see DarkMode::setWindowSettingChangeSubclass()
 * @see DarkMode::removeWindowSettingChangeSubclass()
 */
LRESULT CALLBACK dmlib_subclass::WindowSettingChangeSubclass(
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
			::RemoveWindowSubclass(hWnd, WindowSettingChangeSubclass, uIdSubclass);
			break;
		}

		case WM_SETTINGCHANGE:
		{
			if (DarkMode::handleSettingChange(lParam))
			{
				DarkMode::setDarkTitleBarEx(hWnd, true);
				DarkMode::setChildCtrlsTheme(hWnd);
				::RedrawWindow(hWnd, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_UPDATENOW | RDW_FRAME);
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
 * @class TaskDlgData
 * @brief Class to handle colors for task dialog.
 *
 * Members:
 * - `m_themeData`: Theme data with "DarkMode_Explorer::TaskDialog" theme to get colors.
 * - `m_clrText`: Color for text.
 * - `m_clrBg`: Color for background.
 * - `m_hBrushBg`: Brush for background.
 *
 * Copying and moving are explicitly disabled to preserve exclusive ownership.
 */
class TaskDlgData
{
public:
	TaskDlgData() noexcept
	{
		if (m_themeData.ensureTheme(nullptr))
		{
			COLORREF clrTmp = 0;
			if (SUCCEEDED(::GetThemeColor(m_themeData.getHTheme(), TDLG_PRIMARYPANEL, 0, TMT_TEXTCOLOR, &clrTmp)))
			{
				m_clrText = clrTmp;
			}

			if (SUCCEEDED(::GetThemeColor(m_themeData.getHTheme(), TDLG_PRIMARYPANEL, 0, TMT_FILLCOLOR, &clrTmp)))
			{
				m_clrBg = clrTmp;
			}
		}

		m_hBrushBg = ::CreateSolidBrush(m_clrBg);
	}

	TaskDlgData(const TaskDlgData&) = delete;
	TaskDlgData& operator=(const TaskDlgData&) = delete;

	TaskDlgData(TaskDlgData&&) = delete;
	TaskDlgData& operator=(TaskDlgData&&) = delete;

	~TaskDlgData()
	{
		::DeleteObject(m_hBrushBg);
	}

	[[nodiscard]] COLORREF getTextColor() const noexcept
	{
		return m_clrText;
	}

	[[nodiscard]] COLORREF getBgColor() const noexcept
	{
		return m_clrBg;
	}

	[[nodiscard]] const HBRUSH& getBgBrush() const noexcept
	{
		return m_hBrushBg;
	}

	[[nodiscard]] bool shouldErase() const noexcept
	{
		return m_needErase;
	}

	void stopErase() noexcept
	{
		m_needErase = false;
	}

private:
	dmlib_subclass::ThemeData m_themeData{ L"DarkMode_Explorer::TaskDialog" };
	COLORREF m_clrText = RGB(255, 255, 255);
	COLORREF m_clrBg = RGB(44, 44, 44);
	HBRUSH m_hBrushBg = nullptr;
	bool m_needErase = true;
};

/**
 * @brief Window subclass procedure for handling dark mode for task dialog and its children.
 *
 * @param[in]   hWnd        Window handle being subclassed.
 * @param[in]   uMsg        Message identifier.
 * @param[in]   wParam      Message-specific data.
 * @param[in]   lParam      Message-specific data.
 * @param[in]   uIdSubclass Subclass identifier.
 * @param[in]   dwRefData   TaskDlgData instance.
 * @return LRESULT Result of message processing.
 *
 * @see setDarkTaskDlgSubclass()
 */
static LRESULT CALLBACK DarkTaskDlgSubclass(
	HWND hWnd,
	UINT uMsg,
	WPARAM wParam,
	LPARAM lParam,
	UINT_PTR uIdSubclass,
	DWORD_PTR dwRefData
)
{
	auto* pTaskDlgData = reinterpret_cast<TaskDlgData*>(dwRefData);

	switch (uMsg)
	{
		case WM_NCDESTROY:
		{
			::RemoveWindowSubclass(hWnd, DarkTaskDlgSubclass, uIdSubclass);
			std::unique_ptr<TaskDlgData> ptrData(pTaskDlgData);
			break;
		}

		case WM_ERASEBKGND:
		{
			const std::wstring className = dmlib_subclass::getWndClassName(hWnd);

			if (className == L"CtrlNotifySink")
			{
				break;
			}

			if ((className == L"DirectUIHWND") && pTaskDlgData->shouldErase())
			{
				RECT rcClient{};
				::GetClientRect(hWnd, &rcClient);
				::FillRect(reinterpret_cast<HDC>(wParam), &rcClient, pTaskDlgData->getBgBrush());
				pTaskDlgData->stopErase();
			}
			return TRUE;
		}

		case WM_CTLCOLORDLG:
		case WM_CTLCOLORSTATIC:
		{
			auto hdc = reinterpret_cast<HDC>(wParam);
			::SetTextColor(hdc, pTaskDlgData->getTextColor());
			::SetBkColor(hdc, pTaskDlgData->getBgColor());
			return reinterpret_cast<LRESULT>(pTaskDlgData->getBgBrush());
		}

		case WM_PRINTCLIENT:
		{
			return TRUE;
		}

		default:
		{
			break;
		}
	}
	return ::DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

/**
 * @brief Applies a subclass to task dialog to handle dark mode.
 *
 * @param[in] hWnd Handle to the task dialog.
 *
 * @see DarkMode::DarkTaskDlgSubclass()
 */
static void setDarkTaskDlgSubclass(HWND hWnd)
{
	dmlib_subclass::SetSubclass<TaskDlgData>(hWnd, DarkTaskDlgSubclass, dmlib_subclass::SubclassID::taskDlg);
}

/**
 * @brief Callback function used to enumerate and apply theming/subclassing to task dialog child controls.
 *
 * @param[in]   hWnd    Handle to the window being enumerated.
 * @param[in]   lParam  LPARAM data (unused).
 * @return `TRUE` to continue enumeration.
 */
static BOOL CALLBACK DarkTaskEnumChildProc(HWND hWnd, [[maybe_unused]] LPARAM lParam)
{
	const std::wstring className = dmlib_subclass::getWndClassName(hWnd);

	if (className == L"CtrlNotifySink")
	{
		setDarkTaskDlgSubclass(hWnd);
		return TRUE;
	}

	if (className == WC_BUTTON)
	{
		switch (::GetWindowLongPtr(hWnd, GWL_STYLE) & BS_TYPEMASK) // button style
		{
			case BS_RADIOBUTTON:
			case BS_AUTORADIOBUTTON:
			{
				DarkMode::setCheckboxOrRadioBtnCtrlSubclass(hWnd);
				break;
			}

			default:
			{
				break;
			}
		}

		DarkMode::setDarkExplorerTheme(hWnd);

		return TRUE;
	}

	if (className == WC_LINK)
	{
		DarkMode::enableSysLinkCtrlCtlColor(hWnd);
		setDarkTaskDlgSubclass(hWnd);
		return TRUE;
	}

	if (className == WC_SCROLLBAR)
	{
		DarkMode::setDarkScrollBar(hWnd);
		return TRUE;
	}

	if (className == PROGRESS_CLASS)
	{
		DarkMode::setProgressBarClassicTheme(hWnd);
		return TRUE;
	}

	if (className == L"DirectUIHWND")
	{
		::EnumChildWindows(hWnd, DarkTaskEnumChildProc, 0);
		setDarkTaskDlgSubclass(hWnd);
		DarkMode::setDarkExplorerTheme(hWnd);
		return TRUE;
	}

	return TRUE;
}

void dmlib_subclass::setTaskDlgChildCtrlsSubclassAndTheme(HWND hWnd)
{
	setDarkTaskDlgSubclass(hWnd);
	::EnumChildWindows(hWnd, DarkTaskEnumChildProc, 0);
}
