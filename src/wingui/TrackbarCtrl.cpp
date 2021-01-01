/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"
#include "utils/Log.h"
#include "utils/WinUtil.h"
#include "utils/Dpi.h"

#include "wingui/WinGui.h"
#include "wingui/Layout.h"
#include "wingui/Window.h"
#include "wingui/TrackbarCtrl.h"

// https://docs.microsoft.com/en-us/windows/win32/controls/trackbar-control-reference

Kind kindTrackbar = "trackbar";

TrackbarCtrl::TrackbarCtrl(HWND p) : WindowBase(p) {
    kind = kindTrackbar;
    dwStyle = WS_CHILD | WS_VISIBLE | WS_TABSTOP;
    dwStyle |= TBS_AUTOTICKS; // tick marks for each increment
    dwStyle |= TBS_TOOLTIPS;  // show current value when dragging in a tooltip
    dwExStyle = 0;
    winClass = TRACKBAR_CLASS;
    parent = p;
}

// https://docs.microsoft.com/en-us/windows/win32/controls/wm-hscroll--trackbar-
// https://docs.microsoft.com/en-us/windows/win32/controls/wm-vscroll--trackbar-
static void Handle_WM_VHSCROLL(void* user, WndEvent* ev) {
    uint msg = ev->msg;
    CrashIf(!((msg == WM_VSCROLL) || (msg == WM_HSCROLL)));

    TrackbarCtrl* w = (TrackbarCtrl*)user;
    if (!w->onPosChanging) {
        return;
    }
    ev->w = w;

    CrashIf(GetParent(w->hwnd) != (HWND)ev->hwnd);

    int pos = (int)HIWORD(ev->wp);
    int code = (int)LOWORD(ev->wp);
    switch (code) {
        case TB_THUMBPOSITION:
        case TB_THUMBTRACK:
            // pos is HIWORD so do nothing
            break;
        default:
            pos = w->GetValue();
    }

    TrackbarPosChangingEvent a{};
    CopyWndEvent cp(&a, ev);
    a.trackbarCtrl = w;
    a.pos = pos;
    w->onPosChanging(&a);
    // per https://docs.microsoft.com/en-us/windows/win32/controls/wm-vscroll--trackbar-
    // "if an application processes this message, it should return zero"
    if (a.didHandle) {
        ev->result = 0;
    } else {
        ev->result = 1; // what to return when not handled???
    }
}

// https://docs.microsoft.com/en-us/windows/win32/controls/wm-vscroll--trackbar-
bool TrackbarCtrl::Create() {
    if (isHorizontal) {
        dwStyle |= TBS_HORZ;
        idealSize.dx = 32;
        idealSize.dy = DpiScale(parent, 22);
    } else {
        dwStyle |= TBS_VERT;
        idealSize.dy = 32;
        idealSize.dx = DpiScale(parent, 22);
    }
    initialSize = idealSize;

    bool ok = WindowBase::Create();
    if (!ok) {
        return false;
    }
    SetRange(rangeMin, rangeMax);
    SetValue(rangeMin);

    void* user = this;
    if (isHorizontal) {
        RegisterHandlerForMessage(hwnd, WM_HSCROLL, Handle_WM_VHSCROLL, user);
    } else {
        RegisterHandlerForMessage(hwnd, WM_VSCROLL, Handle_WM_VHSCROLL, user);
    }
    return true;
}

TrackbarCtrl::~TrackbarCtrl() {
}

Size TrackbarCtrl::GetIdealSize() {
    return {idealSize.dx, idealSize.dy};
}

void TrackbarCtrl::SetRange(int min, int max) {
    rangeMin = min;
    rangeMax = max;
    WPARAM redraw = (WPARAM)TRUE;
    LPARAM range = (LPARAM)MAKELONG(min, max);
    SendMessageW(hwnd, TBM_SETRANGE, redraw, range);
}

void TrackbarCtrl::SetValue(int pos) {
    WPARAM redraw = (WPARAM)TRUE;
    LPARAM p = (LPARAM)pos;
    SendMessageW(hwnd, TBM_SETPOS, redraw, p);
}

int TrackbarCtrl::GetValue() {
    int res = (int)SendMessageW(hwnd, TBM_GETPOS, 0, 0);
    return res;
}
