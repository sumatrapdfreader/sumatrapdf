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
    dwStyle = WS_CHILD | WS_VISIBLE | WS_TABSTOP;
    winClass = WC_BUTTONW;
    kind = kindButton;
}

ButtonCtrl::~ButtonCtrl() {
}

static void DispatchWM_COMMAND(void* user, WndEvent* ev) {
    auto w = (ButtonCtrl*)user;
    w->HandleWM_COMMAND(ev);
}

bool ButtonCtrl::Create() {
    if (isDefault) {
        dwStyle |= BS_DEFPUSHBUTTON;
    } else {
        dwStyle |= BS_PUSHBUTTON;
    }
    bool ok = WindowBase::Create();
    if (!ok) {
        return false;
    }
    void* user = this;
    RegisterHandlerForMessage(hwnd, WM_COMMAND, DispatchWM_COMMAND, user);
    auto size = GetIdealSize();
    RECT r{0, 0, size.dx, size.dy};
    SetBounds(r);
    return ok;
}

SizeI ButtonCtrl::GetIdealSize() {
    return ButtonGetIdealSize(hwnd);
}

#if 0
SizeI ButtonCtrl::SetTextAndResize(const WCHAR* s) {
    win::SetText(this->hwnd, s);
    SizeI size = this->GetIdealSize();
    UINT flags = SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED;
    SetWindowPos(this->hwnd, nullptr, 0, 0, size.dx, size.dy, flags);
    return size;
}
#endif

void ButtonCtrl::HandleWM_COMMAND(WndEvent* ev) {
    UINT msg = ev->msg;
    CrashIf(msg != WM_COMMAND);
    WPARAM wp = ev->wparam;

    ev->result = 0;
    auto code = HIWORD(wp);
    if (code == BN_CLICKED) {
        if (onClicked) {
            onClicked();
        }
    }
    ev->didHandle = true;
}

ILayout* NewButtonLayout(ButtonCtrl* w) {
    return new WindowBaseLayout(w, kindButton);
}

std::tuple<ILayout*, ButtonCtrl*> CreateButtonLayout(HWND parent, std::string_view s, const ClickedHandler& onClicked) {
    auto b = new ButtonCtrl(parent);
    b->onClicked = onClicked;
    b->SetText(s);
    b->Create();
    return {NewButtonLayout(b), b};
}
