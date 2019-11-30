/* Copyright 2019 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"
#include "utils/WinUtil.h"

#include "wingui/WinGui.h"
#include "wingui/Layout.h"
#include "wingui/Window.h"
#include "wingui/ButtonCtrl.h"

Kind kindButton = "button";

ButtonCtrl::ButtonCtrl(HWND p) : WindowBase(p) {
    dwStyle = WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON;
    winClass = WC_BUTTONW;
    kind = kindButton;
}

ButtonCtrl::~ButtonCtrl() {
}

bool ButtonCtrl::Create() {
    bool ok = WindowBase::Create();
    if (ok) {
        SubclassParent();
    }
    auto size = GetIdealSize();
    RECT r{0, 0, size.cx, size.cy};
    SetBounds(r);
    return ok;
}

// TODO: cache
SIZE ButtonCtrl::GetIdealSize() {
    return ButtonGetIdealSize(hwnd);
}

#if 0
SIZE ButtonCtrl::SetTextAndResize(const WCHAR* s) {
    win::SetText(this->hwnd, s);
    SIZE size = this->GetIdealSize();
    UINT flags = SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED;
    SetWindowPos(this->hwnd, nullptr, 0, 0, size.cx, size.cy, flags);
    return size;
}
#endif

LRESULT ButtonCtrl::WndProcParent(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, bool& didHandle) {
    UNUSED(hwnd);
    UNUSED(lp);
    if (msg == WM_COMMAND) {
        auto code = HIWORD(wp);
        if (code == BN_CLICKED) {
            if (OnClicked) {
                OnClicked();
            }
        }
        didHandle = true;
        return 0;
    }
    return 0;
}

ILayout* NewButtonLayout(ButtonCtrl* b) {
    return new WindowBaseLayout(b, kindButton);
}

bool IsButton(Kind kind) {
    return kind == kindButton;
}

bool IsButton(ILayout* l) {
    return IsLayoutOfKind(l, kindButton);
}
