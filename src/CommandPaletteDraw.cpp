/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "base/Dpi.h"
#include "base/File.h"
#include "base/Win.h"

#include "wingui/UIModels.h"
#include "wingui/Layout.h"
#include "wingui/WinGui.h"

#include "Theme.h"
#include "Accelerators.h"
#include "FilterHighlightDraw.h"
#include "CommandPaletteInternal.h"

void PositionCommandPalette(HWND hwnd, HWND hwndRelative) {
    Rect rRelative = HwndWindowRect(hwndRelative);
    Rect r = HwndWindowRect(hwnd);
    int x = rRelative.x + (rRelative.dx / 2) - (r.dx / 2);
    int y = rRelative.y + (rRelative.dy / 2) - (r.dy / 2);
    r = {x, y, r.dx, r.dy};
    Rect r2 = ShiftRectToWorkArea(r, hwndRelative, true);
    r2.y = rRelative.y + 42;
    SetWindowPos(hwnd, nullptr, r2.x, r2.y, 0, 0, SWP_NOZORDER | SWP_NOSIZE);
}

void CommandPaletteWnd::DrawListBoxItem(ListBox::DrawItemEvent* ev) {
    ListBox* lb = ev->listBox;
    auto m = (ListBoxModelCP*)lb->model;
    if (ev->itemIndex < 0 || ev->itemIndex >= m->ItemsCount()) {
        return;
    }

    HDC hdc = ev->hdc;
    Rect rc = ev->itemRect;

    COLORREF colBg = IsSpecialColor(lb->bgColor) ? GetSysColor(COLOR_WINDOW) : lb->bgColor;
    COLORREF colText = IsSpecialColor(lb->textColor) ? GetSysColor(COLOR_WINDOWTEXT) : lb->textColor;
    if (ev->selected) {
        colBg = AccentColor(colBg, 30);
    }

    SetBkColor(hdc, colBg);
    HdcFillRectWithBkColor(hdc, rc);

    bool isRtl = HwndIsRtl(lb->hwnd);
    if (isRtl) {
        SetLayout(hdc, 0);
    }

    Str itemText = m->Item(ev->itemIndex);
    ItemDataCP* data = m->Data(ev->itemIndex);

    TempStr rightStr = nullptr;
    if (data->cmdId != 0) {
        TempStr withAccel = AppendAccelKeyToMenuStringTemp("", data->cmdId);
        if (withAccel && withAccel.s[0] == '\t') {
            rightStr = Str(withAccel.s + 1);
        }
    } else if (data->pageNo > 0) {
        // toc entry: show the destination page number on the right, e.g. "p33"
        rightStr = fmt("p%d", data->pageNo);
    } else if (data->filePath) {
        rightStr = path::GetDirTemp(data->filePath);
    }

    SetTextColor(hdc, colText);
    SetBkMode(hdc, TRANSPARENT);

    HFONT oldFont = nullptr;
    if (lb->font) {
        oldFont = SelectFont(hdc, lb->font);
    }

    int padX = DpiScale(lb->hwnd, 4);
    rc.x += padX;
    rc.dx -= 2 * padX;

    if (data->indent > 0) {
        int indentW = data->indent * DpiScale(lb->hwnd, 16);
        if (isRtl) {
            rc.dx -= indentW;
        } else {
            rc.x += indentW;
            rc.dx -= indentW;
        }
    }

    // reserve space on the right for rightStr (accel key, dir, or "p34") so it
    // is always visible; the item text gets the remaining space and is
    // ellipsized when too long.
    Rect rcText = rc;
    TempWStr rightStrW = nullptr;
    int rightW = 0;
    if (rightStr && rightStr.s[0]) {
        rightStrW = ToWStrTemp(rightStr);
        int gap = DpiScale(lb->hwnd, 8);
        rightW = HdcGetTextExtentPoint32(hdc, rightStr).dx;
        if (isRtl) {
            rcText.x += rightW + gap;
            rcText.dx -= rightW + gap;
        } else {
            rcText.dx -= rightW + gap;
        }
    }

    {
        uint drawFmt = DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS;
        drawFmt |= isRtl ? (DT_RIGHT | DT_RTLREADING) : DT_LEFT;
        DrawMaybeHighlightedText(hdc, ToRECT(rcText), itemText, filterWords, highlighted, colBg, isRtl, false, drawFmt);
    }

    if (rightStrW) {
        Rect rcRight = rc;
        uint fmt = DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX;
        if (isRtl) {
            rcRight.dx = rightW;
            fmt |= DT_LEFT | DT_RTLREADING;
        } else {
            rcRight.x += rcRight.dx - rightW;
            rcRight.dx = rightW;
            fmt |= DT_RIGHT;
        }
        COLORREF rightCol = AccentColor(colText, 80);
        SetTextColor(hdc, rightCol);
        HdcDrawText(hdc, rightStrW, rcRight, fmt);
        SetTextColor(hdc, colText);
    }

    if (oldFont) {
        SelectFont(hdc, oldFont);
    }
}
