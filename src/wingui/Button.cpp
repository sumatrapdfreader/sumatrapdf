/* Copyright 2024 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"
#include "utils/ScopedWin.h"
#include "utils/BitManip.h"
#include "utils/WinUtil.h"
#include "utils/Dpi.h"
#include "utils/WinDynCalls.h"

#include "wingui/UIModels.h"

#include "wingui/Layout.h"
#include "wingui/WinGui.h"

#include "Theme.h"

#include "utils/Log.h"

//--- Button

// https://docs.microsoft.com/en-us/windows/win32/controls/buttons

Kind kindButton = "button";

Button::Button() {
    kind = kindButton;
}

bool Button::OnCommand(WPARAM wparam, LPARAM lparam) {
    auto code = HIWORD(wparam);
    if (code == BN_CLICKED) {
        if (onClick.IsValid()) {
            onClick.Call();
            return true;
        }
    }
    return false;
}

LRESULT Button::OnMessageReflect(UINT msg, WPARAM wparam, LPARAM lparam) {
    if (msg == WM_CTLCOLORBTN) {
        // TODO: implement me
        return 0;
    }
    return 0;
}

HWND Button::Create(const CreateArgs& args) {
    CreateControlArgs cargs;
    cargs.className = WC_BUTTONW;
    cargs.parent = args.parent;
    cargs.font = args.font;
    cargs.style = WS_CHILD | WS_VISIBLE | WS_TABSTOP;
    if (isDefault) {
        cargs.style |= BS_DEFPUSHBUTTON;
    } else {
        cargs.style |= BS_PUSHBUTTON;
    }
    cargs.text = args.text;

    Wnd::CreateControl(cargs);
    SizeToIdealSize(this);

    return hwnd;
}

Size Button::GetIdealSize() {
    return ButtonGetIdealSize(hwnd);
}

#if 0
Size Button::SetTextAndResize(const WCHAR* s) {
    HwndSetText(this->hwnd, s);
    Size size = this->GetIdealSize();
    uint flags = SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED;
    SetWindowPos(this->hwnd, nullptr, 0, 0, size.dx, size.dy, flags);
    return size;
}
#endif

Button* CreateButton(HWND parent, const char* s, const Func0& onClick) {
    Button::CreateArgs args;
    args.parent = parent;
    args.text = s;

    auto b = new Button();
    b->onClick = onClick;
    b->Create(args);
    return b;
}

#define kButtonMargin 8

Button* CreateDefaultButton(HWND parent, const char* s) {
    Button::CreateArgs args;
    args.parent = parent;
    args.text = s;

    auto* b = new Button();
    b->Create(args);

    RECT r = ClientRECT(parent);
    Size size = b->GetIdealSize();
    int margin = DpiScale(parent, kButtonMargin);
    int x = RectDx(r) - size.dx - margin;
    int y = RectDy(r) - size.dy - margin;
    r.left = x;
    r.right = x + size.dx;
    r.top = y;
    r.bottom = y + size.dy;
    b->SetPos(&r);
    return b;
}
