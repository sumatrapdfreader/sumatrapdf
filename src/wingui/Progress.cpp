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

//- Progress

// https://docs.microsoft.com/en-us/windows/win32/controls/progress-bar-control-reference

Kind kindProgress = "progress";

Progress::Progress() {
    kind = kindProgress;
}

HWND Progress::Create(const CreateArgs& args) {
    CreateControlArgs cargs;
    cargs.parent = args.parent;
    cargs.style = WS_CHILD | WS_VISIBLE;
    cargs.className = PROGRESS_CLASSW;

    Wnd::CreateControl(cargs);
    SizeToIdealSize(this);
    if (hwnd && args.initialMax != 0) {
        SetMax(args.initialMax);
    }
    return hwnd;
}

Size Progress::GetIdealSize() {
    return {idealDx, idealDy};
}

void Progress::SetMax(int newMax) {
    int min = 0;
    SendMessageW(hwnd, PBM_SETRANGE32, min, newMax);
}

void Progress::SetCurrent(int newCurrent) {
    SendMessageW(hwnd, PBM_SETPOS, newCurrent, 0);
}

int Progress::GetMax() {
    auto max = (int)SendMessageW(hwnd, PBM_GETRANGE, FALSE /* get high limit */, 0);
    return max;
}

int Progress::GetCurrent() {
    auto current = (int)SendMessageW(hwnd, PBM_GETPOS, 0, 0);
    return current;
}
