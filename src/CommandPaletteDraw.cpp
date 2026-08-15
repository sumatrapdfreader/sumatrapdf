/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "gui/Dpi.h"
#include "base/File.h"
#include "base/Win.h"

#include "gui/UIModels.h"
#include "gui/Layout.h"
#include "gui/win/WinGui.h"
#include "gui/PlatformFont.h"
#include "gui/Gfx.h"
#include "gui/GuiColors.h"
#include "gui/VirtCtrl.h"

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

void CommandPaletteWnd::DrawListBoxItem(VirtListBox::DrawItemEvent* ev) {
    VirtListBox* lb = ev->listBox;
    auto* m = (ListBoxModelCP*)lb->model;
    if (ev->itemIndex < 0 || ev->itemIndex >= m->ItemsCount()) {
        return;
    }

    Gfx* gfx = ev->gfx;
    HWND hwndList = lb->GetHwnd();
    Rect rc = ev->itemRect;

    Color colBg = lb->GetColor(kColListBg);
    Color colText = lb->GetColor(kColListText);
    if (IsSpecialColor(colBg)) {
        colBg = GetSysColor(COLOR_WINDOW);
    }
    if (IsSpecialColor(colText)) {
        colText = GetSysColor(COLOR_WINDOWTEXT);
    }
    if (ev->selected) {
        colBg = AccentColor(colBg, 30);
    }

    gfx->FillRect(rc, colBg);

    // drawing text into a mirrored surface would mirror the glyphs; we lay the
    // row out right-to-left ourselves instead
    bool isRtl = HwndIsRtl(hwndList);
    bool prevMirrored = isRtl ? gfx->SetMirrored(false) : false;

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

    int padX = DpiScale(4);
    rc.x += padX;
    rc.dx -= 2 * padX;

    if (data->indent > 0) {
        int indentW = data->indent * DpiScale(16);
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
    bool hasRight = rightStr && rightStr.s[0];
    int rightW = 0;
    if (hasRight) {
        int gap = DpiScale(8);
        rightW = gfx->MeasureText(rightStr, lb->font).dx;
        if (isRtl) {
            rcText.x += rightW + gap;
            rcText.dx -= rightW + gap;
        } else {
            rcText.dx -= rightW + gap;
        }
    }

    {
        u32 drawFmt = gfxTextEllipsis | gfxTextVCenter;
        drawFmt |= isRtl ? (gfxTextRight | gfxTextRtl) : gfxTextLeft;
        DrawMaybeHighlightedText(gfx, rcText, itemText, filterWords, highlighted, colBg, isRtl, false, drawFmt,
                                 lb->font, colText);
    }

    if (hasRight) {
        Rect rcRight = rc;
        u32 rightFmt = gfxTextVCenter;
        if (isRtl) {
            rcRight.dx = rightW;
            rightFmt |= gfxTextLeft | gfxTextRtl;
        } else {
            rcRight.x += rcRight.dx - rightW;
            rcRight.dx = rightW;
            rightFmt |= gfxTextRight;
        }
        gfx->DrawText(rightStr, rcRight, rightFmt, lb->font, AccentColor(colText, 80));
    }

    if (isRtl) {
        gfx->SetMirrored(prevMirrored);
    }
}
