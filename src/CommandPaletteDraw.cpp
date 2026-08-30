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

    // Gfx (Direct2D / GDI+) does not pick up WS_EX_LAYOUTRTL, so we lay the
    // row out right-to-left ourselves. Do not use HwndIsRtl(): the palette
    // hwnd stays LTR so mouse hit-testing and virtual-control coords match
    // (issue #5956). If the DC is still mirrored (nested RTL hwnd), turn it
    // off so Hebrew glyphs are not reversed.
    bool isRtl = CommandPaletteUiRtl();
    bool hwndRtl = HwndIsRtl(hwndList);
    bool prevMirrored = hwndRtl ? gfx->SetMirrored(false) : false;

    Str itemText = m->Item(ev->itemIndex);
    ItemDataCP* data = m->Data(ev->itemIndex);

    TempStr rightStr;
    if (data->cmdId != 0) {
        rightStr = CommandPaletteShortcutTemp(data->cmdId);
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
    // ellipsized when too long. File history: the filename takes precedence
    // over a long directory (issue #6104).
    Rect rcText = rc;
    bool hasRight = rightStr && rightStr.s[0];
    int rightW = 0;
    if (hasRight) {
        int gap = DpiScale(8);
        rightW = gfx->MeasureText(rightStr, lb->font).dx;
        if (data->filePath) {
            int nameW = gfx->MeasureText(itemText, lb->font).dx;
            int minDir = DpiScale(80);
            int maxRight = rc.dx - nameW - gap;
            if (maxRight < minDir) {
                hasRight = false;
                rightW = 0;
            } else if (rightW > maxRight) {
                rightW = maxRight;
            }
        }
        if (hasRight) {
            if (isRtl) {
                rcText.x += rightW + gap;
                rcText.dx -= rightW + gap;
            } else {
                rcText.dx -= rightW + gap;
            }
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
            rightFmt |= gfxTextLeft;
        } else {
            rcRight.x += rcRight.dx - rightW;
            rcRight.dx = rightW;
            rightFmt |= gfxTextRight;
        }
        Color rightCol = AccentColor(colText, 80);
        if (data->cmdId != 0) {
            DrawMaybeHighlightedText(gfx, rcRight, rightStr, filterWords, highlighted, colBg, false, false, rightFmt,
                                     lb->font, rightCol);
        } else {
            if (data->filePath) {
                rightFmt |= gfxTextPathEllipsis;
            }
            gfx->DrawText(rightStr, rcRight, rightFmt, lb->font, rightCol);
        }
    }

    if (hwndRtl) {
        gfx->SetMirrored(prevMirrored);
    }
}
