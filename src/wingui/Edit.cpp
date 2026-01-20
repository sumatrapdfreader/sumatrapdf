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

//--- Edit

// https://docs.microsoft.com/en-us/windows/win32/controls/edit-controls

// TODO:
// - expose EN_UPDATE
// https://docs.microsoft.com/en-us/windows/win32/controls/en-update
// - add border and possibly other decorations by handling WM_NCCALCSIZE, WM_NCPAINT and
// WM_NCHITTEST
//   etc., http://www.catch22.net/tuts/insert-buttons-edit-control
// - include value we remember in WM_NCCALCSIZE in GetIdealSize()

Kind kindEdit = "edit";

static bool EditSetCueText(HWND hwnd, const char* s) {
    if (!hwnd) {
        return false;
    }
    TempWStr ws = ToWStrTemp(s);
    bool ok = Edit_SetCueBannerText(hwnd, ws) == TRUE;
    return ok;
}

Edit::Edit() {
    kind = kindEdit;
}

Edit::~Edit() {
    // DeleteObject(bgBrush);
}

void Edit::SetSelection(int start, int end) {
    Edit_SetSel(hwnd, start, end);
}

void Edit::SetCursorPosition(int pos) {
    SetSelection(pos, pos);
}

void Edit::SetCursorPositionAtEnd() {
    WCHAR* s = HwndGetTextWTemp(hwnd);
    int pos = str::Len(s);
    SetCursorPosition(pos);
}

HWND Edit::Create(const CreateArgs& editArgs) {
    // https://docs.microsoft.com/en-us/windows/win32/controls/edit-control-styles
    CreateControlArgs args;
    args.className = WC_EDITW;
    args.parent = editArgs.parent;
    args.font = editArgs.font;
    args.style = WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_LEFT;
    if (editArgs.withBorder) {
        args.exStyle = WS_EX_CLIENTEDGE;
        // Note: when using WS_BORDER, we would need to remember
        // we have border and use it in Edit::HasBorder
        // args.style |= WS_BORDER;
    }
    if (editArgs.isMultiLine) {
        args.style |= ES_MULTILINE | WS_VSCROLL | ES_WANTRETURN;
    } else {
        // ES_AUTOHSCROLL disable wrapping in multi-line setup
        args.style |= ES_AUTOHSCROLL;
    }
    idealSizeLines = editArgs.idealSizeLines;
    if (idealSizeLines < 1) {
        idealSizeLines = 1;
    }
    Wnd::CreateControl(args);
    if (!hwnd) {
        return nullptr;
    }
    SizeToIdealSize(this);

    if (editArgs.cueText) {
        EditSetCueText(hwnd, editArgs.cueText);
    }
    if (editArgs.text) {
        SetText(editArgs.text);
    }
    return hwnd;
}

LRESULT Edit::WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_KEYDOWN: {
            bool isCtrlBack = (VK_BACK == wp) && IsCtrlPressed() && !IsShiftPressed();
            if (isCtrlBack) {
                PostMessageW(hwnd, UWM_DELAYED_CTRL_BACK, 0, 0);
                return true;
            }
            break;
        }

        case UWM_DELAYED_CTRL_BACK: {
            EditImplementCtrlBack(hwnd);
            return true;
        }
    }
    return WndProcDefault(hwnd, msg, wp, lp);
    // return FinalWindowProc(msg, wp, lp);
}

bool Edit::HasBorder() {
    DWORD exStyle = GetWindowExStyle(hwnd);
    bool res = bit::IsMaskSet<DWORD>(exStyle, WS_EX_CLIENTEDGE);
    return res;
}

Size Edit::GetIdealSize() {
    HFONT hfont = HwndGetFont(hwnd);
    Size s1 = HwndMeasureText(hwnd, "Minimal", hfont);
    // logf("Edit::GetIdealSize: s1.dx=%d, s2.dy=%d\n", (int)s1.cx, (int)s1.cy);
    char* txt = HwndGetTextTemp(hwnd);
    Size s2 = HwndMeasureText(hwnd, txt, hfont);
    // logf("Edit::GetIdealSize: s2.dx=%d, s2.dy=%d\n", (int)s2.cx, (int)s2.cy);

    int dx = std::max(s1.dx, s2.dx);
    if (maxDx > 0 && dx > maxDx) {
        dx = maxDx;
    }
    // for multi-line text, this measures multiple line.
    // TODO: maybe figure out better protocol
    int dy = std::min(s1.dy, s2.dy);
    if (dy == 0) {
        dy = std::max(s1.dy, s2.dy);
    }
    dy = dy * idealSizeLines;
    // logf("Edit::GetIdealSize: dx=%d, dy=%d\n", (int)dx, (int)dy);

    LRESULT margins = SendMessageW(hwnd, EM_GETMARGINS, 0, 0);
    int lm = (int)LOWORD(margins);
    int rm = (int)HIWORD(margins);
    dx += lm + rm;

    if (HasBorder()) {
        dx += DpiScale(hwnd, 4);
        dy += DpiScale(hwnd, 8);
    }
    // logf("Edit::GetIdealSize(): dx=%d, dy=%d\n", int(res.cx), int(res.cy));
    return {dx, dy};
}

// https://docs.microsoft.com/en-us/windows/win32/controls/en-change
bool Edit::OnCommand(WPARAM wparam, LPARAM lparam) {
    auto code = HIWORD(wparam);
    if (code == EN_CHANGE && onTextChanged.IsValid()) {
        onTextChanged.Call();
        return true;
    }
    return false;
}

LRESULT Edit::OnMessageReflect(UINT msg, WPARAM wp, LPARAM lparam) {
    if (msg == WM_CTLCOLOREDIT) {
        HDC hdc = (HDC)wp;
        if (!IsSpecialColor(textColor)) {
            SetTextColor(hdc, textColor);
        }
        if (!IsSpecialColor(bgColor)) {
            SetBkColor(hdc, bgColor);
            SetBkMode(hdc, TRANSPARENT);
        }
        auto br = BackgroundBrush();
        return (LRESULT)br;
        return 0;
    }
    return 0;
}
