/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

// https://docs.microsoft.com/en-us/windows/win32/controls/buttons

#include "utils/BaseUtil.h"
#include "utils/ScopedWin.h"
#include "utils/Dpi.h"
#include "utils/WinUtil.h"

#include "wingui/Layout.h"
#include "wingui/Window.h"
#include "wingui/ButtonCtrl.h"

Kind kindButton = "button";

ButtonCtrl::ButtonCtrl() {
    dwStyle = WS_CHILD | WS_VISIBLE | WS_TABSTOP;
    winClass = WC_BUTTONW;
    kind = kindButton;
}

ButtonCtrl::~ButtonCtrl() = default;

static void Handle_WM_COMMAND(void* user, WndEvent* ev) {
    auto w = (ButtonCtrl*)user;
    uint msg = ev->msg;
    CrashIf(msg != WM_COMMAND);
    WPARAM wp = ev->wp;

    ev->result = 0;
    auto code = HIWORD(wp);
    if (code == BN_CLICKED) {
        if (w->onClicked) {
            w->onClicked();
            ev->didHandle = true;
        }
    }
}

bool ButtonCtrl::Create(HWND parent) {
    if (isDefault) {
        dwStyle |= BS_DEFPUSHBUTTON;
    } else {
        dwStyle |= BS_PUSHBUTTON;
    }
    bool ok = WindowBase::Create(parent);
    if (!ok) {
        return false;
    }
    void* user = this;
    RegisterHandlerForMessage(hwnd, WM_COMMAND, Handle_WM_COMMAND, user);
    auto size = GetIdealSize();
    RECT r{0, 0, size.dx, size.dy};
    SetBounds(r);
    return ok;
}

Size ButtonCtrl::GetIdealSize() {
    return ButtonGetIdealSize(hwnd);
}

#if 0
Size ButtonCtrl::SetTextAndResize(const WCHAR* s) {
    win::SetText(this->hwnd, s);
    Size size = this->GetIdealSize();
    uint flags = SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED;
    SetWindowPos(this->hwnd, nullptr, 0, 0, size.dx, size.dy, flags);
    return size;
}
#endif

ButtonCtrl* CreateButton(HWND parent, std::string_view s, const ClickedHandler& onClicked) {
    auto b = new ButtonCtrl();
    b->onClicked = onClicked;
    b->SetText(s);
    b->Create(parent);
    return b;
}

#define kButtonMargin DpiScale(8)

ButtonCtrl* CreateDefaultButtonCtrl(HWND hwndParent, const WCHAR* s) {
    auto* b = new ButtonCtrl();
    b->SetText(s);
    b->Create(hwndParent);

    RECT r;
    GetClientRect(hwndParent, &r);
    Size size = b->GetIdealSize();
    int x = RectDx(r) - size.dx - kButtonMargin;
    int y = RectDy(r) - size.dy - kButtonMargin;
    r.left = x;
    r.right = x + size.dx;
    r.top = y;
    r.bottom = y + size.dy;
    b->SetPos(&r);
    return b;
}
