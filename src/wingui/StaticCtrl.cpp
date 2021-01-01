/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"
#include "utils/WinUtil.h"

#include "wingui/WinGui.h"
#include "wingui/Layout.h"
#include "wingui/Window.h"
#include "wingui/StaticCtrl.h"

// https://docs.microsoft.com/en-us/windows/win32/controls/static-controls

// TODO: add OnClicked handler, use SS_NOTIFY to get notified about STN_CLICKED

Kind kindStatic = "static";

StaticCtrl::StaticCtrl(HWND p) : WindowBase(p) {
    dwStyle = WS_CHILD | WS_VISIBLE;
    winClass = WC_STATICW;
    kind = kindStatic;
}

StaticCtrl::~StaticCtrl() {
}

static void Handle_WM_COMMAND([[maybe_unused]] void* user, WndEvent* ev) {
    // auto w = (StaticCtrl*)user;
    CrashIf(ev->msg != WM_COMMAND);
    // TODO: implement me
}

// static
void Handle_WM_CTLCOLORSTATIC(void* user, WndEvent* ev) {
    auto w = (StaticCtrl*)user;
    uint msg = ev->msg;
    CrashIf(msg != WM_CTLCOLORSTATIC);
    HDC hdc = (HDC)ev->wp;
    if (w->textColor != ColorUnset) {
        SetTextColor(hdc, w->textColor);
    }
    // the brush we return is the background color for the whole
    // area of static control
    // SetBkColor() is just for the part where the text is
    // SetBkMode(hdc, TRANSPARENT) sets the part of the text to transparent
    // (but the whole background is still controlled by the bruhs
    auto bgBrush = w->backgroundColorBrush;
    if (bgBrush != nullptr) {
        SetBkColor(hdc, w->backgroundColor);
        ev->result = (LRESULT)bgBrush;
    } else {
        SetBkMode(hdc, TRANSPARENT);
    }
    ev->didHandle = true;
}

bool StaticCtrl::Create() {
    bool ok = WindowBase::Create();
    if (!ok) {
        return false;
    }
    void* user = this;
    RegisterHandlerForMessage(hwnd, WM_COMMAND, Handle_WM_COMMAND, user);
    // RegisterHandlerForMessage(hwnd, WM_CTLCOLORSTATIC, Handle_WM_CTLCOLORSTATIC, user);
    auto size = GetIdealSize();
    RECT r{0, 0, size.dx, size.dy};
    SetBounds(r);
    return true;
}

Size StaticCtrl::GetIdealSize() {
    WCHAR* txt = win::GetText(hwnd);
    Size s = HwndMeasureText(hwnd, txt, hfont);
    free(txt);
    return s;
}
