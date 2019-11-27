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

Kind buttonKind = "button";

bool IsButton(Kind kind) {
    return kind == buttonKind;
}

bool IsButton(ILayout* l) {
    return IsLayoutOfKind(l, buttonKind);
}

ButtonCtrl::ButtonCtrl(HWND parent, int menuId, RECT* initialPosition) {
    dwStyle = WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON;
    dwExStyle = 0;

    winClass = WC_BUTTON;
    kind = buttonKind;
    this->parent = parent;
    this->menuId = menuId;
    if (initialPosition) {
        this->initialPos = *initialPosition;
    } else {
        SetRect(&this->initialPos, 0, 0, 100, 20);
    }
}

bool ButtonCtrl::Create() {
    bool ok = WindowBase::Create();
    if (!ok) {
        return false;
    }
    // this->SetTextAndResize(s);
    return true;
}

ButtonCtrl::~ButtonCtrl() {
}

// TODO: cache
SIZE ButtonCtrl::GetIdealSize() {
    // adjust to real size and position to the right
    SIZE s{};
    Button_GetIdealSize(this->hwnd, &s);
    // add padding
    float xPadding = 8 * 2;
    float yPadding = 2 * 2;
    hwndDpiAdjust(this->hwnd, &xPadding, &yPadding);
    s.cx += (int)xPadding;
    s.cy += (int)yPadding;
    return s;
}

SIZE ButtonCtrl::SetTextAndResize(const WCHAR* s) {
    win::SetText(this->hwnd, s);
    SIZE size = this->GetIdealSize();
    UINT flags = SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED;
    SetWindowPos(this->hwnd, nullptr, 0, 0, size.cx, size.cy, flags);
    return size;
}

ButtonLayout::ButtonLayout(ButtonCtrl* wb) {
    kind = buttonKind;
    button = wb;
}

ButtonLayout::~ButtonLayout() {
    delete button;
}

Size ButtonLayout::Layout(const Constraints bc) {
    i32 width = this->MinIntrinsicWidth(0);
    i32 height = this->MinIntrinsicHeight(0);
    return bc.Constrain(Size{width, height});
}

i32 ButtonLayout::MinIntrinsicHeight(i32) {
    SIZE s = button->GetIdealSize();
    return (i32)s.cy;
}

i32 ButtonLayout::MinIntrinsicWidth(i32) {
    SIZE s = button->GetIdealSize();
    return (i32)s.cx;
}

void ButtonLayout::SetBounds(const Rect bounds) {
    auto r = RectToRECT(bounds);
    ::MoveWindow(button->hwnd, &r);
}

Kind checkboxKind = "checkbox";

bool IsCheckbox(Kind kind) {
    return kind == checkboxKind;
}

bool IsCheckbox(ILayout* l) {
    return IsLayoutOfKind(l, checkboxKind);
}

CheckboxCtrl::CheckboxCtrl(HWND parent, int menuId, RECT* initialPosition) {
    this->parent = parent;
    this->menuId = menuId;
    if (initialPosition) {
        this->initialPos = *initialPosition;
    } else {
        SetRect(&this->initialPos, 0, 0, 100, 20);
    }
}

bool CheckboxCtrl::Create() {
    RECT rc = this->initialPos;
    auto h = GetModuleHandle(nullptr);
    int x = rc.left;
    int y = rc.top;
    int dx = RectDx(rc);
    int dy = RectDy(rc);
    HMENU idMenu = (HMENU)(UINT_PTR)this->menuId;
    this->hwnd =
        CreateWindowExW(this->dwExStyle, WC_BUTTON, L"", this->dwStyle, x, y, dx, dy, this->parent, idMenu, h, nullptr);

    if (!this->hwnd) {
        return false;
    }
    if (hfont == nullptr) {
        hfont = GetDefaultGuiFont();
    }
    SetFont(hfont);
    HwndSetText(hwnd, text.AsView());
    // this->SetTextAndResize(s);
    return true;
}

CheckboxCtrl::~CheckboxCtrl() {
}

// caller must free() the result
WCHAR* CheckboxCtrl::GetTextW() {
    return win::GetText(this->hwnd);
}

void CheckboxCtrl::SetPos(RECT* r) {
    MoveWindow(this->hwnd, r);
}

void CheckboxCtrl::SetText(std::string_view sv) {
    text.Set(sv.data());
    HwndSetText(hwnd, text.AsView());
}

SIZE CheckboxCtrl::GetIdealSize() {
    // adjust to real size and position to the right
    SIZE s;
    Button_GetIdealSize(this->hwnd, &s);
    // add padding
    float xPadding = 8 * 2;
    float yPadding = 2 * 2;
    hwndDpiAdjust(this->hwnd, &xPadding, &yPadding);
    s.cx += (int)xPadding;
    s.cy += (int)yPadding;
    return s;
}

SIZE CheckboxCtrl::SetTextAndResize(const WCHAR* s) {
    win::SetText(this->hwnd, s);
    SIZE size = this->GetIdealSize();
    UINT flags = SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED;
    SetWindowPos(this->hwnd, nullptr, 0, 0, size.cx, size.cy, flags);
    return size;
}

void CheckboxCtrl::SetFont(HFONT f) {
    SetWindowFont(this->hwnd, f, TRUE);
}

void CheckboxCtrl::SetIsChecked(bool isChecked) {
    Button_SetCheck(this->hwnd, isChecked);
}

bool CheckboxCtrl::IsChecked() const {
    int isChecked = Button_GetCheck(this->hwnd);
    return !!isChecked;
}

Size CheckboxCtrl::Layout(const Constraints bc) {
    i32 width = this->MinIntrinsicWidth(0);
    i32 height = this->MinIntrinsicHeight(0);
    return bc.Constrain(Size{width, height});
}

i32 CheckboxCtrl::MinIntrinsicHeight(i32) {
    SIZE s = this->GetIdealSize();
    return (i32)s.cy;
}

i32 CheckboxCtrl::MinIntrinsicWidth(i32) {
    SIZE s = this->GetIdealSize();
    return (i32)s.cx;
}

void CheckboxCtrl::SetBounds(const Rect bounds) {
    auto r = RectToRECT(bounds);
    ::MoveWindow(this->hwnd, &r);
}
