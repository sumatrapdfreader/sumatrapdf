/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"
#include "utils/WinUtil.h"

#include "wingui/WinGui.h"
#include "wingui/Layout.h"
#include "wingui/Window.h"
#include "wingui/ProgressCtrl.h"

// https://docs.microsoft.com/en-us/windows/win32/controls/progress-bar-control-reference

Kind kindProgress = "progress";

bool IsProgress(Kind kind) {
    return kind == kindProgress;
}

bool IsProgress(ILayout* l) {
    return IsLayoutOfKind(l, kindProgress);
}

ProgressCtrl::ProgressCtrl(HWND p, int initialMax) : WindowBase(p) {
    dwStyle = WS_CHILD | WS_VISIBLE;
    winClass = PROGRESS_CLASSW;
    kind = kindProgress;
    max = initialMax;
}

ProgressCtrl::~ProgressCtrl() {
}

bool ProgressCtrl::Create() {
    bool ok = WindowBase::Create();
    if (max != 0) {
        SetMax(max);
    }
    return ok;
}

SIZE ProgressCtrl::GetIdealSize() {
    return {idealDx, idealDy};
}

void ProgressCtrl::SetMax(int newMax) {
    max = newMax;
    int min = 0;
    SendMessageW(hwnd, PBM_SETRANGE32, min, max);
}

void ProgressCtrl::SetCurrent(int newCurrent) {
    current = newCurrent;
    SendMessageW(hwnd, PBM_SETPOS, current, 0);
}

int ProgressCtrl::GetMax() {
    max = (int)SendMessageW(hwnd, PBM_GETRANGE, FALSE /* get high limit */, 0);
    return max;
}

int ProgressCtrl::GetCurrent() {
    current = (int)SendMessageW(hwnd, PBM_GETPOS, 0, 0);
    return current;
}

ILayout* NewProgressLayout(ProgressCtrl* b) {
    return new WindowBaseLayout(b, kindProgress);
}
