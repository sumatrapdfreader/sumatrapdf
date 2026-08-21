/* Copyright 2024 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "base/Win.h"
#include "gui/Dpi.h"

#include "gui/UIModels.h"

#include "gui/Layout.h"
#include "gui/PlatformFont.h"
#include "gui/win/WinGui.h"

//--- Button

// https://docs.microsoft.com/en-us/windows/win32/controls/buttons

static Kind kindButton = "button";

Button::Button() {
    kind = kindButton;
}

void Button::OnCommand(ControlBase::CommandEvent* ev) {
    auto code = HIWORD(ev->wparam);
    if (code == BN_CLICKED) {
        if (onClick.IsValid()) {
            onClick.Call();
            ev->didHandle = true;
        }
    }
}

HWND Button::Create(const CreateArgs& args) {
    onCommand = MkMethod1<Button, ControlBase::CommandEvent*, &Button::OnCommand>(this);
    CreateControlArgs cargs;
    cargs.className = WC_BUTTONW;
    cargs.parent = args.parent;
    cargs.font = args.font;
    cargs.isRtl = args.isRtl;
    cargs.style = WS_CHILD | WS_VISIBLE | WS_TABSTOP;
    if (isDefault) {
        cargs.style |= BS_DEFPUSHBUTTON;
    } else {
        cargs.style |= BS_PUSHBUTTON;
    }
    cargs.text = args.text;

    ControlBase::CreateControl(cargs);
    SizeToIdealSize(this);

    return hwnd;
}

Size Button::GetIdealSize() {
    // BCM_GETIDEALSIZE returns the text size without any button padding, so
    // measure the text and add our own
    TempStr s = HwndGetTextTemp(hwnd);
    Size sz = PlatformFontMeasureText(font, s);
    int dx = sz.dx + DpiScale(2 * 12);
    int dy = sz.dy + DpiScale(2 * 5);
    int minDx = DpiScale(70);
    dx = std::max(dx, minDx);
    return {dx, dy};
}

#define kButtonMargin 8

// Only the installer and uninstaller use this, and neither loads settings, so
// there is no theme to draw from - let Windows draw its own themed button.
Button* CreateDefaultButton(HWND parent, Str s, bool isRtl) {
    Button::CreateArgs args;
    args.parent = parent;
    args.text = s;
    args.isRtl = isRtl;

    auto* b = new Button();
    b->Create(args);

    Rect rc = HwndClientRect(parent);
    Size size = b->GetIdealSize();
    int margin = DpiScale(kButtonMargin);
    int x = rc.dx - size.dx - margin;
    int y = rc.dy - size.dy - margin;
    Rect r = {x, y, size.dx, size.dy};
    b->SetPos(&r);
    return b;
}
