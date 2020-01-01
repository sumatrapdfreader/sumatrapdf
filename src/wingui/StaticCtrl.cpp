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

bool IsStatic(Kind kind) {
    return kind == kindStatic;
}

bool IsStatic(ILayout* l) {
    return IsLayoutOfKind(l, kindStatic);
}

StaticCtrl::StaticCtrl(HWND p) : WindowBase(p) {
    dwStyle = WS_CHILD | WS_VISIBLE;
    winClass = WC_STATICW;
    kind = kindStatic;
}

StaticCtrl::~StaticCtrl() {
}

bool StaticCtrl::Create() {
    bool ok = WindowBase::Create();
    if (ok) {
        SubclassParent();
    }
    auto size = GetIdealSize();
    RECT r{0, 0, size.cx, size.cy};
    SetBounds(r);
    return ok;
}

SIZE StaticCtrl::GetIdealSize() {
    WCHAR* txt = win::GetText(hwnd);
    SIZE s = MeasureTextInHwnd(hwnd, txt, hfont);
    free(txt);
    return s;
}

void StaticCtrl::WndProcParent(WndProcArgs* args) {
    UINT msg = args->msg;
    if (msg == WM_COMMAND) {
        // TODO: support STN_CLICKED
        return;
    }
}

ILayout* NewStaticLayout(StaticCtrl* b) {
    return new WindowBaseLayout(b, kindStatic);
}
