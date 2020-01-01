/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"
#include "utils/WinUtil.h"

#include "wingui/WinGui.h"
#include "wingui/Layout.h"
#include "wingui/Window.h"
#include "wingui/TooltipCtrl.h"

// https://docs.microsoft.com/en-us/windows/win32/controls/tooltip-control-reference

Kind kindTooltip = "tooltip";

TooltipCtrl::TooltipCtrl(HWND p) : WindowBase(p) {
    kind = kindTooltip;
    dwExStyle = WS_EX_TOPMOST;
    dwStyle = WS_POPUP | TTS_NOPREFIX | TTS_ALWAYSTIP;
    winClass = TOOLTIPS_CLASS;
}

TooltipCtrl::~TooltipCtrl() {
    // this is a stand-alone window, so we need to destroy it ourselves
    DestroyWindow(hwnd);
}

bool TooltipCtrl::Create() {
    bool ok = WindowBase::Create();
    return ok;
}

SIZE TooltipCtrl::GetIdealSize() {
    return SIZE{100, 32}; // not used as this is top-level window
}

void TooltipCtrl::SetMaxWidth(int dx) {
    SendMessageW(hwnd, TTM_SETMAXTIPWIDTH, 0, dx);
}

static const int MULTILINE_INFOTIP_WIDTH_PX = 500;

static void SetMaxWidthForText(HWND hwnd, const WCHAR* text, bool multiline) {
    int dx = -1;
    if (multiline || str::FindChar(text, '\n')) {
        // TODO: dpi scale
        dx = MULTILINE_INFOTIP_WIDTH_PX;
    }
    SendMessageW(hwnd, TTM_SETMAXTIPWIDTH, 0, dx);
}

void TooltipCtrl::Show(std::string_view s, RectI& rc, bool multiline) {
    WCHAR* ws = strconv::Utf8ToWstr(s);
    Show(ws, rc, multiline);
    free(ws);
}

void TooltipCtrl::Show(const WCHAR* text, RectI& rc, bool multiline) {
    SetMaxWidthForText(hwnd, text, multiline);

    TOOLINFO ti = {0};
    ti.cbSize = sizeof(ti);
    ti.hwnd = parent;
    ti.uFlags = TTF_SUBCLASS;
    ti.lpszText = (WCHAR*)text;
    ti.rect = rc.ToRECT();
    UINT msg = isShowing ? TTM_NEWTOOLRECT : TTM_ADDTOOL;
    SendMessageW(hwnd, msg, 0, (LPARAM)&ti);

    isShowing = true;
}

void TooltipCtrl::Hide() {
    if (!isShowing) {
        return;
    }

    TOOLINFO ti = {0};
    ti.cbSize = sizeof(ti);
    ti.hwnd = parent;
    SendMessageW(hwnd, TTM_DELTOOL, 0, (LPARAM)&ti);
    isShowing = false;
}
