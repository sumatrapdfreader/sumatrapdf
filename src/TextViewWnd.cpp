/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "base/Dpi.h"
#include "base/UITask.h"
#include "base/Win.h"

#include "wingui/UIModels.h"
#include "wingui/Layout.h"
#include "wingui/WinGui.h"

#include "Theme.h"
#include "DarkModeSubclass.h"
#include "SumatraConfig.h"

struct TextViewWnd : Wnd {
    ~TextViewWnd() override;

    Edit* edit = nullptr;
    HFONT monoFont = nullptr;
    HWND* hwndPtr = nullptr;
    bool isDialog = false;

    bool Create(Str title, Str text);
    void LayoutToClient();
    void UpdateTheme();
    static Str FormatTextForEdit(Str text);
    LRESULT WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) override;
    void ScheduleDelete();
};

TextViewWnd::~TextViewWnd() {
    if (monoFont) {
        DeleteObject(monoFont);
        monoFont = nullptr;
    }
}

static void DeleteTextViewWndInstance(TextViewWnd* w) {
    delete w;
}

void TextViewWnd::ScheduleDelete() {
    auto fn = MkFunc0<TextViewWnd>(DeleteTextViewWndInstance, this);
    uitask::Post(fn, "SafeDeleteTextViewWnd");
}

void TextViewWnd::LayoutToClient() {
    if (!edit || !hwnd) {
        return;
    }
    Rect rc = ClientRect(hwnd);
    edit->SetBounds(rc);
}

void TextViewWnd::UpdateTheme() {
    COLORREF colBg = ThemeWindowControlBackgroundColor();
    COLORREF colTxt = ThemeWindowTextColor();
    SetColors(colTxt, colBg);
    if (edit) {
        edit->SetColors(colTxt, colBg);
    }
    if (UseDarkModeLib()) {
        DarkMode::setDarkWndSafe(hwnd);
        DarkMode::setWindowEraseBgSubclass(hwnd);
    }
    RedrawWindow(hwnd, nullptr, nullptr, RDW_ERASE | RDW_INVALIDATE | RDW_ALLCHILDREN);
}

Str TextViewWnd::FormatTextForEdit(Str text) {
    str::Builder crlfText;
    for (int i = 0; i < text.len; i++) {
        char c = text.s[i];
        if (c == '\n' && (i == 0 || text.s[i - 1] != '\r')) {
            crlfText.AppendChar('\r');
        }
        crlfText.AppendChar(c);
    }
    return ToStr(crlfText);
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

    HDC hdc = GetDC(hwnd);
    monoFont = CreateSimpleFont(hdc, "Consolas", 14);
    ReleaseDC(hwnd, hdc);
    if (monoFont) {
        edit->font = monoFont;
        SendMessageW(edit->hwnd, WM_SETFONT, (WPARAM)monoFont, TRUE);
    }

    // set tab stop to 4 spaces (16 dialog units; default is 32 = 8 spaces)
    DWORD tabStop = 16;
    SendMessageW(edit->hwnd, EM_SETTABSTOPS, 1, (LPARAM)&tabStop);

    edit->SetText(FormatTextForEdit(text));
    SendMessageW(edit->hwnd, EM_SETSEL, 0, 0);
    layout = edit;

    int winW = DpiScale(hwnd, 800);
    int winH = DpiScale(hwnd, 600);
    SetWindowPos(hwnd, nullptr, 0, 0, winW, winH, SWP_NOMOVE | SWP_NOZORDER);
    LayoutToClient();
    UpdateTheme();
    SetIsVisible(true);
    return true;
}

LRESULT TextViewWnd::WndProc(HWND hwndIn, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_SIZE) {
        LayoutToClient();
        return 0;
    }
    return WndProcDefault(hwndIn, msg, wp, lp);
}

static void OnTextViewDestroy(Wnd::DestroyEvent* ev) {
    TextViewWnd* w = (TextViewWnd*)WndListFindByHwnd(ev->e->hwnd);
    if (!w) {
        return;
    }
    if (w->hwndPtr) {
        *w->hwndPtr = nullptr;
    }
    if (w->isDialog) {
        PostQuitMessage(0);
    }
    w->ScheduleDelete();
}

HWND ShowTextInWindow(Str title, Str text, HWND* hwndPtr) {
    auto* wnd = new TextViewWnd();
    wnd->hwndPtr = hwndPtr;
    wnd->onDestroy = MkFunc1Void<Wnd::DestroyEvent*>(OnTextViewDestroy);
    if (!wnd->Create(title, text)) {
        delete wnd;
        return nullptr;
    }
    return wnd->hwnd;
}

void ShowTextInWindowDialog(Str title, Str text) {
    auto* wnd = new TextViewWnd();
    wnd->isDialog = true;
    wnd->onDestroy = MkFunc1Void<Wnd::DestroyEvent*>(OnTextViewDestroy);
    if (!wnd->Create(title, text)) {
        delete wnd;
        return;
    }
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}