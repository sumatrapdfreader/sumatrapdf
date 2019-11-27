/* Copyright 2019 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"
#include "utils/WinUtil.h"
#include "utils/Dpi.h"

#include "wingui/WinGui.h"
#include "wingui/Layout.h"
#include "wingui/Window.h"
#include "wingui/ButtonCtrl.h"

// TODO: move to utilities or move to indow, cache DPI info
// on creation and handle WM_DPICHANGED
// https://docs.microsoft.com/en-us/windows/desktop/hidpi/wm-dpichanged
static void hwndDpiAdjust(HWND hwnd, float* x, float* y) {
    auto dpi = DpiGet(hwnd);

    if (x != nullptr) {
        float dpiFactor = (float)dpi->dpiX / 96.f;
        *x = *x * dpiFactor;
    }

    if (y != nullptr) {
        float dpiFactor = (float)dpi->dpiY / 96.f;
        *y = *y * dpiFactor;
    }
}

static SIZE ButtonGetIdealSize(HWND hwnd) {
    // adjust to real size and position to the right
    SIZE s{};
    Button_GetIdealSize(hwnd, &s);
    // add padding
    float xPadding = 8 * 2;
    float yPadding = 2 * 2;
    hwndDpiAdjust(hwnd, &xPadding, &yPadding);
    s.cx += (int)xPadding;
    s.cy += (int)yPadding;
    return s;
}

Kind kindButton = "button";

bool IsButton(Kind kind) {
    return kind == kindButton;
}

bool IsButton(ILayout* l) {
    return IsLayoutOfKind(l, kindButton);
}

ButtonCtrl::ButtonCtrl(HWND p) : WindowBase(p) {
    dwStyle = WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON;
    winClass = WC_BUTTON;
    kind = kindButton;
}

bool ButtonCtrl::Create() {
    bool ok = WindowBase::Create();
    if (!ok) {
        return false;
    }

    SubclassParent();
    return true;
}

ButtonCtrl::~ButtonCtrl() {
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

LRESULT ButtonCtrl::WndProcParent(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_COMMAND) {
        auto code = HIWORD(wp);
        if (code == BN_CLICKED) {
            if (this->OnClicked) {
                this->OnClicked();
            }
        }
    }
    return DefSubclassProc(hwnd, msg, wp, lp);
}

Kind kindCheckbox = "checkbox";

CheckboxCtrl::CheckboxCtrl(HWND parent) : WindowBase(parent) {
    dwStyle = WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX;
    winClass = WC_BUTTON;
    kind = kindCheckbox;
}

CheckboxCtrl::~CheckboxCtrl() {
}

SIZE CheckboxCtrl::GetIdealSize() {
    return ButtonGetIdealSize(hwnd);
}

void CheckboxCtrl::SetIsChecked(bool isChecked) {
    Button_SetCheck(this->hwnd, isChecked);
}

bool CheckboxCtrl::IsChecked() const {
    int isChecked = Button_GetCheck(this->hwnd);
    return !!isChecked;
}

WindowBaseLayout::WindowBaseLayout(WindowBase* b, Kind k) {
    wb = b;
    kind = k;
}

WindowBaseLayout::~WindowBaseLayout() {
    delete wb;
}

Size WindowBaseLayout::Layout(const Constraints bc) {
    i32 width = MinIntrinsicWidth(0);
    i32 height = MinIntrinsicHeight(0);
    return bc.Constrain(Size{width, height});
}

i32 WindowBaseLayout::MinIntrinsicHeight(i32) {
    SIZE s = wb->GetIdealSize();
    return (i32)s.cy;
}

i32 WindowBaseLayout::MinIntrinsicWidth(i32) {
    SIZE s = wb->GetIdealSize();
    return (i32)s.cx;
}

void WindowBaseLayout::SetBounds(const Rect bounds) {
    auto r = RectToRECT(bounds);
    ::MoveWindow(wb->hwnd, &r);
}

bool IsCheckbox(Kind kind) {
    return kind == kindCheckbox;
}

bool IsCheckbox(ILayout* l) {
    return IsLayoutOfKind(l, kindCheckbox);
}

ILayout* NewButtonLayout(ButtonCtrl* b) {
    return new WindowBaseLayout(b, kindButton);
}

ILayout* NewCheckboxLayout(CheckboxCtrl* b) {
    return new WindowBaseLayout(b, kindCheckbox);
}
