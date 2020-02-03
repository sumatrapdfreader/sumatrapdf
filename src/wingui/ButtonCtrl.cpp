/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

// https://docs.microsoft.com/en-us/windows/win32/controls/buttons

#include "utils/BaseUtil.h"
#include "utils/WinUtil.h"

#include "wingui/WinGui.h"
#include "wingui/Layout.h"
#include "wingui/Window.h"
#include "wingui/ButtonCtrl.h"

Kind kindButton = "button";

bool IsButton(Kind kind) {
    return kind == kindButton;
}

bool IsButton(ILayout* l) {
    return IsLayoutOfKind(l, kindButton);
}

ButtonCtrl::ButtonCtrl(HWND p) : WindowBase(p) {
    dwStyle = WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON;
    winClass = WC_BUTTONW;
    kind = kindButton;
}

ButtonCtrl::~ButtonCtrl() {
}

bool ButtonCtrl::Create() {
    bool ok = WindowBase::Create();
    if (!ok) {
        return false;
    }
    SubclassParent();
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

void ButtonCtrl::WndProcParent(WndProcArgs* args) {
    UINT msg = args->msg;
    WPARAM wp = args->wparam;

    args->result = 0;
    if (msg == WM_COMMAND) {
        auto code = HIWORD(wp);
        if (code == BN_CLICKED) {
            if (onClicked) {
                onClicked();
            }
        }
        args->didHandle = true;
    }
}

ILayout* NewButtonLayout(ButtonCtrl* b) {
    return new WindowBaseLayout(b, kindButton);
}

std::tuple<ILayout*, ButtonCtrl*> CreateButtonLayout(HWND parent, std::string_view s, const ClickedHandler& onClicked) {
    auto b = new ButtonCtrl(parent);
    b->onClicked = onClicked;
    b->SetText(s);
    b->Create();
    return {NewButtonLayout(b), b};
}
