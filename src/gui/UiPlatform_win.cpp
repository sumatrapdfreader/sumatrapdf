/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

// The handful of things the portable UI layer asks the platform for (declared
// at the end of VirtHost.h). They live apart from VirtHost_win.cpp because
// VirtCtrl.cpp calls UiSetCursor() and is built by tools that have no host
// window at all -- logview links VirtCtrl but not VirtHost, and pulling the
// whole host in for one cursor call would drag the window class with it.

#include "base/Base.h"
#include "base/Win.h"
#include "gui/Dpi.h"

#include "gui/UIModels.h"
#include "gui/Layout.h"
#include "gui/Gfx.h"
#include "gui/VirtCtrl.h"
#include "gui/PlatformFont.h"
#include "gui/VirtHost.h"

Point UiCursorScreenPos() {
    return GetCursorPosition();
}

int UiHScrollbarDy() {
    return DpiGetSystemMetrics(SM_CYHSCROLL);
}

int UiEdgeDx() {
    return DpiGetSystemMetrics(SM_CXEDGE);
}

static LPWSTR ToWin32Cursor(CursorId id) {
    switch (id) {
        case CursorId::Arrow:
            return IDC_ARROW;
        case CursorId::IBeam:
            return IDC_IBEAM;
        case CursorId::Hand:
            return IDC_HAND;
        case CursorId::Cross:
            return IDC_CROSS;
        case CursorId::Move:
            return IDC_SIZEALL;
        case CursorId::SizeNS:
            return IDC_SIZENS;
        case CursorId::SizeWE:
            return IDC_SIZEWE;
        case CursorId::No:
            return IDC_NO;
        case CursorId::None:
            return nullptr;
    }
    return nullptr;
}

void UiSetCursor(CursorId id) {
    LPWSTR win32Id = ToWin32Cursor(id);
    if (win32Id) {
        SetCursorCached(win32Id);
    }
}
