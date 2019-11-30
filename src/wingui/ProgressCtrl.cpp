/* Copyright 2019 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"
#include "utils/WinUtil.h"

#include "wingui/WinGui.h"
#include "wingui/Layout.h"
#include "wingui/Window.h"
#include "wingui/ProgressCtrl.h"

// TODO: add OnClicked handler, use SS_NOTIFY to get notified about STN_CLICKED

Kind kindProgress = "progress";

bool IsProgress(Kind kind) {
    return kind == kindProgress;
}

bool IsProgress(ILayout* l) {
    return IsLayoutOfKind(l, kindProgress);
}

ProgressCtrl::ProgressCtrl(HWND p) : WindowBase(p) {
    dwStyle = WS_CHILD | WS_VISIBLE;
    winClass = WC_STATICW;
    kind = kindProgress;
}

ProgressCtrl::~ProgressCtrl() {
}

bool ProgressCtrl::Create() {
    bool ok = WindowBase::Create();
    return ok;
}

SIZE ProgressCtrl::GetIdealSize() {
    WCHAR* txt = win::GetText(hwnd);
    SIZE s = MeasureTextInHwnd(hwnd, txt, hfont);
    free(txt);
    return s;
}

// https://docs.microsoft.com/en-us/windows/win32/controls/static-controls
LRESULT ProgressCtrl::WndProcParent(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, bool& didHandle) {
    UNUSED(hwnd);
    UNUSED(lp);
    UNUSED(didHandle);
    UNUSED(wp);

    if (msg == WM_COMMAND) {
        // TODO: support STN_CLICKED
        return 0;
    }
    return 0;
}

ILayout* NewProgressLayout(ProgressCtrl* b) {
    return new WindowBaseLayout(b, kindProgress);
}
