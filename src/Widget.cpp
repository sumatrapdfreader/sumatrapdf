/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"
#include "utils/ScopedWin.h"
#include "utils/BitManip.h"
#include "utils/WinUtil.h"
#include "utils/Dpi.h"
#include "utils/WinDynCalls.h"

#include "wingui/UIModels.h"
#include "wingui/Layout.h"

#include "Widget.h"

Kind kindHwndWidgetText = "hwndWidgetText";

HwndWidgetText::HwndWidgetText(const char* s, HWND hwnd, HFONT font) : s(s), hwnd(hwnd), font(font) {
    kind = kindHwndWidgetText;
}

Size HwndWidgetText::Layout(const Constraints bc) {
    Measure();
    return bc.Constrain({sz.dx, sz.dy});
}

Size HwndWidgetText::Measure(bool onlyIfEmpty) {
    if (onlyIfEmpty && !sz.IsEmpty()) {
        return sz;
    }
    sz = HwndMeasureText(hwnd, s, font);
    return sz;
}

int HwndWidgetText::MinIntrinsicHeight(int width) {
    Measure(true);
    return sz.dy;
}

int HwndWidgetText::MinIntrinsicWidth(int height) {
    Measure(true);
    return sz.dx;
}

Size HwndWidgetText::MinIntrinsicSize(int width, int height) {
    int dx = MinIntrinsicWidth(height);
    int dy = MinIntrinsicHeight(width);
    return {dx, dy};
}

void HwndWidgetText::Draw(HDC hdc) {
    CrashIf(lastBounds.IsEmpty());
    UINT fmt = DT_NOCLIP | DT_NOPREFIX | (isRtl ? DT_RTLREADING : DT_LEFT);
    RECT dr = ToRECT(lastBounds);
    HdcDrawText(hdc, s, &dr, fmt, font);
    if (withUnderline) {
        auto& r = lastBounds;
        Rect lineRect = {r.x, r.y + sz.dy, sz.dx, 0};
        DrawLine(hdc, lineRect);
    }
}
