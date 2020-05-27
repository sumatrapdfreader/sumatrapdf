/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
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

static void DispatchWM_COMMAND(void* user, WndEvent* ev) {
    auto w = (StaticCtrl*)user;
    w->HandleWM_COMMAND(ev);
}

bool StaticCtrl::Create() {
    bool ok = WindowBase::Create();
    if (!ok) {
        return false;
    }
    void* user = this;
    RegisterHandlerForMessage(hwnd, WM_COMMAND, DispatchWM_COMMAND, user);
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

void StaticCtrl::HandleWM_COMMAND(WndEvent* ev) {
    UINT msg = ev->msg;
    CrashIf(msg != WM_COMMAND);
    // TODO: support STN_CLICKED
}
