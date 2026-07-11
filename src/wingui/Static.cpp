/* Copyright 2024 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "base/Win.h"

#include "wingui/UIModels.h"

#include "wingui/Layout.h"
#include "wingui/WinGui.h"

//- Static

// https://docs.microsoft.com/en-us/windows/win32/controls/static-controls

Kind kindStatic = "static";

Static::Static() {
    kind = kindStatic;
}

HWND Static::Create(const CreateArgs& args) {
    CreateControlArgs cargs;
    cargs.className = WC_STATICW;
    cargs.parent = args.parent;
    cargs.font = args.font;
    cargs.style = WS_CHILD | WS_VISIBLE | SS_NOTIFY;
    if (args.pathEllipsis) {
        cargs.style |= SS_PATHELLIPSIS;
    }
    cargs.text = args.text;
    cargs.isRtl = args.isRtl;

    Wnd::CreateControl(cargs);
    SizeToIdealSize(this);

    return hwnd;
}

Size Static::GetIdealSize() {
    ReportIf(!hwnd);
    TempStr txt = HwndGetTextTemp(hwnd);
    HFONT hfont = GetWindowFont(hwnd);
    return HwndMeasureText(hwnd, txt, hfont);
}

bool Static::OnCommand(WPARAM wparam, LPARAM lparam) {
    auto code = HIWORD(wparam);
    if (code == STN_CLICKED && onClick.IsValid()) {
        onClick.Call();
        return true;
    }
    return false;
}

LRESULT Static::OnMessageReflect(UINT msg, WPARAM wp, LPARAM lparam) {
    if (msg == WM_CTLCOLORSTATIC) {
        HDC hdc = (HDC)wp;
        if (!IsSpecialColor(textColor)) {
            SetTextColor(hdc, textColor);
        }
        if (!IsSpecialColor(bgColor)) {
            SetBkColor(hdc, bgColor);
        }
        auto br = BackgroundBrush();
        return (LRESULT)br;
    }
    return 0;
}
