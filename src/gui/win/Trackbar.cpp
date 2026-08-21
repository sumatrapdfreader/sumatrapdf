/* Copyright 2024 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "gui/Dpi.h"

#include "gui/UIModels.h"

#include "gui/Layout.h"
#include "gui/win/WinGui.h"

//- Trackbar

// https://docs.microsoft.com/en-us/windows/win32/controls/trackbar-control-reference

static Kind kindTrackbar = "trackbar";

Trackbar::Trackbar() {
    kind = kindTrackbar;
}

// https://docs.microsoft.com/en-us/windows/win32/controls/wm-vscroll--trackbar-
HWND Trackbar::Create(const CreateArgs& args) {
    onMessageReflect = MkMethod1<Trackbar, ControlBase::MessageReflectEvent*, &Trackbar::OnMessageReflect>(this);
    CreateControlArgs cargs;
    cargs.className = TRACKBAR_CLASS;
    cargs.parent = args.parent;
    cargs.font = args.font;
    cargs.isRtl = args.isRtl;

    DWORD dwStyle = WS_CHILD | WS_VISIBLE | WS_TABSTOP;
    dwStyle |= TBS_AUTOTICKS; // tick marks for each increment
    dwStyle |= TBS_TOOLTIPS;  // show current value when dragging in a tooltip
    if (args.isHorizontal) {
        dwStyle |= TBS_HORZ;
        idealSize.dx = 32;
        idealSize.dy = DpiScale(22);
    } else {
        dwStyle |= TBS_VERT;
        idealSize.dy = 32;
        idealSize.dx = DpiScale(22);
    }
    // TODO: add initial size to CreateControlArgs
    // initialSize = idealSize;
    cargs.style = dwStyle;
    // args.style |= WS_BORDER;
    ControlBase::CreateControl(cargs);
    SizeToIdealSize(this);

    if (hwnd) {
        SetRange(args.rangeMin, args.rangeMax);
        SetValue(args.rangeMin);
    }
    return hwnd;
}

// https://docs.microsoft.com/en-us/windows/win32/controls/wm-hscroll--trackbar-
// https://docs.microsoft.com/en-us/windows/win32/controls/wm-vscroll--trackbar-
void Trackbar::OnMessageReflect(ControlBase::MessageReflectEvent* ev) {
    if (!onPositionChanging.IsValid()) {
        return;
    }
    switch (ev->msg) {
        case WM_VSCROLL:
        case WM_HSCROLL: {
            int pos = (int)HIWORD(ev->wparam);
            int code = (int)LOWORD(ev->wparam);
            switch (code) {
                case TB_THUMBPOSITION:
                case TB_THUMBTRACK:
                    // pos is HIWORD so do nothing
                    break;
                default:
                    pos = GetValue();
            }
            Trackbar::PositionChangingEvent a{};
            a.trackbar = this;
            a.pos = pos;
            onPositionChanging.Call(&a);
            // per https://docs.microsoft.com/en-us/windows/win32/controls/wm-vscroll--trackbar-
            // "if an application processes this message, it should return zero"
            ev->result = 0;
        }
    }
}

Size Trackbar::GetIdealSize() {
    return {idealSize.dx, idealSize.dy};
}

void Trackbar::SetRange(int min, int max) {
    WPARAM redraw = (WPARAM)TRUE;
    LPARAM range = (LPARAM)MAKELONG(min, max);
    SendMessageW(hwnd, TBM_SETRANGE, redraw, range);
}

int Trackbar::GetRangeMin() {
    int res = (int)SendMessageW(hwnd, TBM_GETRANGEMIN, 0, 0);
    return res;
}

int Trackbar::getRangeMax() {
    int res = (int)SendMessageW(hwnd, TBM_GETRANGEMAX, 0, 0);
    return res;
}

void Trackbar::SetValue(int pos) {
    WPARAM redraw = (WPARAM)TRUE;
    LPARAM p = (LPARAM)pos;
    SendMessageW(hwnd, TBM_SETPOS, redraw, p);
}

int Trackbar::GetValue() {
    int res = (int)SendMessageW(hwnd, TBM_GETPOS, 0, 0);
    return res;
}
