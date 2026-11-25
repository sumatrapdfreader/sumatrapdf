/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "base/Win.h"

#include "gui/PlatformWindow.h"

Rect PlatformWindowRect(NativeWnd native) {
    return HwndWindowRect(native);
}

Rect PlatformWindowWorkArea(NativeWnd native) {
    Rect r = HwndWindowRect(native);
    return GetWorkAreaRect(r, native);
}

bool PlatformWindowIsMaximized(NativeWnd native) {
    return IsZoomed(native);
}

void PlatformWindowActivateIfForeground(NativeWnd native) {
    if (!native) {
        return;
    }
    HWND fg = GetForegroundWindow();
    if (!fg || fg == native) {
        SetActiveWindow(native);
    }
}
