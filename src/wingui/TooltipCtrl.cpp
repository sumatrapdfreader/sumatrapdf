/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"
#include "utils/ScopedWin.h"
#include "utils/WinUtil.h"

#include "wingui/WinGui.h"
#include "wingui/Layout.h"
#include "wingui/Window.h"
#include "wingui/TooltipCtrl.h"

#include "utils/Log.h"

// https://docs.microsoft.com/en-us/windows/win32/controls/tooltip-control-reference

Kind kindTooltip = "tooltip";

TooltipCtrl::TooltipCtrl(HWND p) : WindowBase(p) {
    kind = kindTooltip;
    dwExStyle = WS_EX_TOPMOST;
    dwStyle = WS_POPUP | TTS_NOPREFIX | TTS_ALWAYSTIP;
    winClass = TOOLTIPS_CLASS;
    // CreateWindow() for tooltip fails if this is not 0
    ctrlID = 0;
}

TooltipCtrl::~TooltipCtrl() {
    // this is a stand-alone window, so we need to destroy it ourselves
    DestroyWindow(hwnd);
}

bool TooltipCtrl::Create() {
    bool ok = WindowBase::Create();
    SetDelayTime(TTDT_AUTOPOP, 32767);
    return ok;
}

Size TooltipCtrl::GetIdealSize() {
    return {100, 32}; // not used as this is top-level window
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

void TooltipCtrl::Show(std::string_view s, Rect& rc, bool multiline) {
    auto ws = ToWstrTemp(s);
    Show(ws, rc, multiline);
}

void TooltipCtrl::Show(const WCHAR* txt, Rect& rc, bool multiline) {
    auto currText = text.Get();
    // to change the text we need to delete and add tooltip
    // TODO: probably there's a simpler way
    if (!str::Eq(txt, currText)) {
        Hide();
    }

    SetMaxWidthForText(hwnd, txt, multiline);

    TOOLINFO ti = {0};
    ti.cbSize = sizeof(ti);
    ti.hwnd = parent;
    ti.uFlags = TTF_SUBCLASS;
    ti.rect = ToRECT(rc);

    bool isNew = text.IsEmpty();
    uint msg = TTM_NEWTOOLRECT;
    if (isNew) {
        ti.lpszText = (WCHAR*)txt;
        msg = TTM_ADDTOOL;
    }
    SendMessageW(hwnd, msg, 0, (LPARAM)&ti);
    logf(L"TooltipCtrl::Show() (%d, %d) - (%d, %d), '%s', msg: %s\n", rc.x, rc.y, rc.dx, rc.dy, txt, isNew ? L"TTM_ADDTOOL" : L"TTM_NEWTOOLRECT");
    text.Set(txt);
}

void TooltipCtrl::Hide() {
    if (text.IsEmpty()) {
        return;
    }

    TOOLINFO ti = {0};
    ti.cbSize = sizeof(ti);
    ti.hwnd = parent;
    SendMessageW(hwnd, TTM_DELTOOL, 0, (LPARAM)&ti);
    text.Reset();
}

// https://docs.microsoft.com/en-us/windows/win32/controls/ttm-setdelaytime
// type is: TTDT_AUTOPOP, TTDT_INITIAL, TTDT_RESHOW, TTDT_AUTOMATIC
// timeInMs is max 32767 (~32 secs)
void TooltipCtrl::SetDelayTime(int type, int timeInMs) {
    CrashIf(!IsValidDelayType(type));
    CrashIf(timeInMs < 0);
    CrashIf(timeInMs > 32767); // TODO: or is it 65535?
    SendMessageW(hwnd, TTM_SETDELAYTIME, type, (LPARAM)timeInMs);
}
