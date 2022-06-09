/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
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
#include "wingui/VirtWnd.h"

#include "utils/Log.h"

Kind kindVirtWnd = "kindVirtWnd";

VirtWnd::VirtWnd() {
    kind = kindVirtWnd;
}

Kind VirtWnd::GetKind() {
    return kind;
}

void VirtWnd::SetVisibility(Visibility v) {
    visibility = v;
}

Visibility VirtWnd::GetVisibility() {
    return visibility;
}

int VirtWnd::MinIntrinsicHeight(int) {
    return 0;
}

int VirtWnd::MinIntrinsicWidth(int) {
    return 0;
}

Size VirtWnd::Layout(Constraints) {
    return {};
}

void VirtWnd::SetBounds(Rect b) {
    bounds = b;
}

void VirtWnd::Paint(HDC) {
}
