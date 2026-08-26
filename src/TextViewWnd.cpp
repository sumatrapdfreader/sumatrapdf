/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "gui/Dpi.h"

#include "gui/UIModels.h"
#include "gui/Layout.h"
#include "gui/PlatformFont.h"
#include "gui/win/WinGui.h"

#include "Theme.h"
#include "DarkMode_win.h"
#include "SumatraConfig.h"
#include "base/Win.h"

struct TextViewWnd : WindowBase {
    Edit* edit = nullptr;
    PlatformFont* monoFont = nullptr;
    HWND* hwndPtr = nullptr;

    bool Create(Str title, Str text);
    void UpdateTheme() override;
    void ApplyDarkMode() override;
    static Str FormatTextForEdit(Str text);
};

void TextViewWnd::ApplyDarkMode() {
    DarkModeApplyToWindowAndEraseBg(hwnd);
}

void TextViewWnd::UpdateTheme() {
    WindowBase::UpdateTheme();
    // Re-apply monospaced font after darkmode child theming (may reset font).
    if (edit && monoFont) {
        edit->SetFont(monoFont);
    }
}

// Returns a temp-arena string: ToStr() would be a view into the local Builder
// and dangle as soon as this function returns (crash in HwndSetText/CWStrTemp).
Str TextViewWnd::FormatTextForEdit(Str text) {
    str::Builder crlfText;
    for (int i = 0; i < text.len; i++) {
        char c = text.s[i];
        if (c == '\n' && (i == 0 || text.s[i - 1] != '\r')) {
            crlfText.AppendChar('\r');
        }
        crlfText.AppendChar(c);
    }
    return ToStrTemp(crlfText);
}

bool TextViewWnd::Create(Str title, Str text) {
    {
        CreateCustomArgs args;
        args.title = title;
        args.visible = false;
        args.style = WS_OVERLAPPEDWINDOW;
        args.icon = LoadIconW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(GetAppIconID()));
        CreateCustom(args);
    }
    if (!hwnd) {
        return false;
    }

    Edit::CreateArgs args;
    args.parent = hwnd;
    args.isMultiLine = true;
    args.withBorder = true;
    edit = new Edit();
    edit->Create(args);
    SendMessageW(edit->hwnd, EM_SETREADONLY, TRUE, 0);
    SendMessageW(edit->hwnd, EM_SETLIMITTEXT, 0, 0);

    HDC hdc = GetDC(hwnd);
    monoFont = HdcCreateSimpleFont(hdc, StrL("Consolas"), 14);
    ReleaseDC(hwnd, hdc);
    if (monoFont) {
        edit->SetFont(monoFont);
    }

    // set tab stop to 4 spaces (16 dialog units; default is 32 = 8 spaces)
    DWORD tabStop = 16;
    SendMessageW(edit->hwnd, EM_SETTABSTOPS, 1, (LPARAM)&tabStop);

    edit->SetText(FormatTextForEdit(text));
    SendMessageW(edit->hwnd, EM_SETSEL, 0, 0);
    layout = edit;

    int winW = DpiScale(800);
    int winH = DpiScale(600);
    SetWindowPos(hwnd, nullptr, 0, 0, winW, winH, SWP_NOMOVE | SWP_NOZORDER);
    DoLayout();
    UpdateTheme();
    SetIsVisible(true);
    return true;
}

static void TeardownTextViewWnd(TextViewWnd* w) {
    if (!w) {
        return;
    }
    if (w->hwndPtr) {
        *w->hwndPtr = nullptr;
    }
    w->ScheduleDelete();
}

static void OnTextViewClose(WindowBase::CloseEvent* ev) {
    TeardownTextViewWnd((TextViewWnd*)ev->e->self);
}

static void OnTextViewDestroy(WindowBase::DestroyEvent* ev) {
    TeardownTextViewWnd((TextViewWnd*)ev->e->self);
}

HWND ShowTextInWindow(Str title, Str text, HWND* hwndPtr) {
    auto* wnd = new TextViewWnd();
    wnd->hwndPtr = hwndPtr;
    wnd->closeOnEsc = true;
    wnd->onClose = MkFunc1Void<WindowBase::CloseEvent*>(OnTextViewClose);
    wnd->onDestroy = MkFunc1Void<WindowBase::DestroyEvent*>(OnTextViewDestroy);
    if (!wnd->Create(title, text)) {
        delete wnd;
        return nullptr;
    }
    return wnd->hwnd;
}

void ShowTextInWindowDialog(Str title, Str text) {
    auto* wnd = new TextViewWnd();
    wnd->closeOnEsc = true;
    wnd->onClose = MkFunc1Void<WindowBase::CloseEvent*>(OnTextViewClose);
    wnd->onDestroy = MkFunc1Void<WindowBase::DestroyEvent*>(OnTextViewDestroy);
    if (!wnd->Create(title, text)) {
        delete wnd;
        return;
    }
    // returns once the scheduled delete has run and destroyed the window
    RunModalWindow(wnd->hwnd, nullptr);
}
