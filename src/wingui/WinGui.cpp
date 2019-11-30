/* Copyright 2019 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"
#include "utils/WinUtil.h"

#include "wingui/WinGui.h"
#include "wingui/Layout.h"
#include "wingui/Window.h"

/* Return size of a text <txt> in a given <hwnd>, taking into account its font */
SIZE MeasureTextInHwnd(HWND hwnd, const WCHAR* txt, HFONT font) {
    SIZE sz{};
    size_t txtLen = str::Len(txt);
    HDC dc = GetWindowDC(hwnd);
    /* GetWindowDC() returns dc with default state, so we have to first set
       window's current font into dc */
    if (font == nullptr) {
        font = (HFONT)SendMessage(hwnd, WM_GETFONT, 0, 0);
    }
    HGDIOBJ prev = SelectObject(dc, font);

    RECT r{};
    UINT fmt = DT_CALCRECT | DT_LEFT | DT_NOCLIP | DT_EDITCONTROL;
    DrawTextExW(dc, (WCHAR*)txt, (int)txtLen, &r, fmt, nullptr);
    SelectObject(dc, prev);
    ReleaseDC(hwnd, dc);
    int dx = RectDx(r);
    int dy = RectDy(r);
    return {dx, dy};
}
