/* Copyright 2024 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "utils/BaseUtil.h"
#include "utils/ScopedWin.h"
#include "utils/WinUtil.h"
#include "utils/Dpi.h"

#include "wingui/UIModels.h"
#include "wingui/Layout.h"
#include "wingui/WinGui.h"

#include "Settings.h"
#include "DocController.h"
#include "EngineBase.h"
#include "ProgressUpdateUI.h"
#include "TextSelection.h"
#include "TextSearch.h"
#include "DisplayModel.h"
#include "SumatraPDF.h"
#include "MainWindow.h"
#include "Commands.h"
#include "SvgIcons.h"
#include "Toolbar.h"
#include "SearchAndDDE.h"
#include "FindBar.h"
#include "Translations.h"
#include "Theme.h"

#include "utils/Log.h"

// command id for the bar's close (x) button; must not collide with real commands
constexpr int kFindBarCloseCmdId = (int)CmdLast + 50;

struct FindBarWnd : Wnd {
    MainWindow* win = nullptr;
    Edit* edit = nullptr;
    Static* status = nullptr;
    HWND hwndBtns = nullptr; // small toolbar: prev / next / match-case / close
    HIMAGELIST himl = nullptr;

    int barDx = 0;
    int barDy = 0;

    FindBarWnd() = default;
    ~FindBarWnd() override;

    bool Create(MainWindow* win);
    void Layout();

    void OnTextChanged();

    LRESULT WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) override;
    bool PreTranslateMessage(MSG& msg) override;
    bool OnCommand(WPARAM wparam, LPARAM lparam) override;
};

FindBarWnd::~FindBarWnd() {
    delete edit;
    delete status;
    HwndDestroyWindowSafe(&hwndBtns);
    if (himl) {
        ImageList_Destroy(himl);
    }
}

static int RoundUp4(int n) {
    return (n + 3) & ~3;
}

bool FindBarWnd::Create(MainWindow* mainWin) {
    win = mainWin;

    auto colBg = ThemeWindowControlBackgroundColor();
    auto colTxt = ThemeWindowTextColor();

    {
        CreateCustomArgs args;
        args.visible = false;
        args.style = WS_POPUP | WS_BORDER;
        // WS_EX_TOOLWINDOW keeps it off the taskbar. Not topmost: we make the
        // frame our owner instead (below) so the bar floats above the frame but
        // not above other apps.
        args.exStyle = WS_EX_TOOLWINDOW;
        CreateCustom(args);
    }
    if (!hwnd) {
        return false;
    }
    // make the frame our owner: an owned window always renders above its owner
    // (so it stays visible when the user clicks into the document) yet drops
    // behind when another application is activated.
    SetWindowLongPtrW(hwnd, GWLP_HWNDPARENT, (LONG_PTR)win->hwndFrame);
    SetColors(colTxt, colBg);

    {
        Edit::CreateArgs args;
        args.parent = hwnd;
        args.isMultiLine = false;
        args.withBorder = true;
        args.cueText = _TRA("Find");
        args.isRtl = IsUIRtl();
        edit = new Edit();
        edit->maxDx = DpiScale(hwnd, 240);
        edit->SetColors(colTxt, colBg);
        edit->Create(args);
        edit->onTextChanged = MkMethod0<FindBarWnd, &FindBarWnd::OnTextChanged>(this);
        win->hwndFindEdit = edit->hwnd;
    }

    {
        Static::CreateArgs args;
        args.parent = hwnd;
        args.text = "";
        args.isRtl = IsUIRtl();
        status = new Static();
        status->SetColors(colTxt, colBg);
        status->Create(args);
    }

    {
        DWORD style = WS_CHILD | WS_VISIBLE | TBSTYLE_FLAT | TBSTYLE_LIST | CCS_NODIVIDER | CCS_NORESIZE |
                      CCS_NOPARENTALIGN;
        HINSTANCE hinst = GetModuleHandleW(nullptr);
        hwndBtns = CreateWindowExW(0, TOOLBARCLASSNAMEW, nullptr, style, 0, 0, 0, 0, hwnd, (HMENU) nullptr, hinst,
                                   nullptr);
        SendMessageW(hwndBtns, TB_BUTTONSTRUCTSIZE, (WPARAM)sizeof(TBBUTTON), 0);

        int isz = RoundUp4(DpiScale(hwnd, 16));
        himl = BuildStdToolbarImageList(isz);
        SendMessageW(hwndBtns, TB_SETIMAGELIST, 0, (LPARAM)himl);
        SendMessageW(hwndBtns, TB_SETBUTTONSIZE, 0, MAKELONG(isz, isz));

        TBBUTTON b[4]{};
        b[0].iBitmap = (int)TbIcon::ChevronUp;
        b[0].idCommand = CmdFindPrev;
        b[0].fsState = TBSTATE_ENABLED;
        b[0].fsStyle = BTNS_BUTTON;
        b[1].iBitmap = (int)TbIcon::ChevronDown;
        b[1].idCommand = CmdFindNext;
        b[1].fsState = TBSTATE_ENABLED;
        b[1].fsStyle = BTNS_BUTTON;
        b[2].iBitmap = (int)TbIcon::MatchCase;
        b[2].idCommand = CmdFindToggleMatchCase;
        b[2].fsState = TBSTATE_ENABLED;
        b[2].fsStyle = BTNS_CHECK;
        b[3].iBitmap = (int)TbIcon::Close;
        b[3].idCommand = kFindBarCloseCmdId;
        b[3].fsState = TBSTATE_ENABLED;
        b[3].fsStyle = BTNS_BUTTON;
        SendMessageW(hwndBtns, TB_ADDBUTTONS, 4, (LPARAM)&b);
        SendMessageW(hwndBtns, TB_AUTOSIZE, 0, 0);
    }

    Layout();
    return true;
}

void FindBarWnd::Layout() {
    int p = DpiScale(hwnd, 6);
    int gap = DpiScale(hwnd, 4);
    int editDx = DpiScale(hwnd, 220);
    int statusDx = DpiScale(hwnd, 88);

    int editDy = edit->GetIdealSize().dy;

    SIZE tbSz{};
    SendMessageW(hwndBtns, TB_GETMAXSIZE, 0, (LPARAM)&tbSz);

    int innerDy = std::max(editDy, (int)tbSz.cy);
    barDy = innerDy + 2 * p;
    barDx = p + editDx + gap + statusDx + gap + (int)tbSz.cx + p;

    int x = p;
    MoveWindow(edit->hwnd, x, (barDy - editDy) / 2, editDx, editDy, TRUE);
    x += editDx + gap;
    MoveWindow(status->hwnd, x, (barDy - editDy) / 2, statusDx, editDy, TRUE);
    x += statusDx + gap;
    MoveWindow(hwndBtns, x, (barDy - (int)tbSz.cy) / 2, (int)tbSz.cx, (int)tbSz.cy, TRUE);

    SetWindowPos(hwnd, nullptr, 0, 0, barDx, barDy, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
}

void FindBarWnd::OnTextChanged() {
    OnFindBarTextChanged(win);
}

LRESULT FindBarWnd::WndProc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_ERASEBKGND) {
        HBRUSH br = BackgroundBrush();
        if (br) {
            HDC hdc = (HDC)wp;
            RECT rc;
            GetClientRect(h, &rc);
            FillRect(hdc, &rc, br);
            return 1;
        }
    }
    return WndProcDefault(h, msg, wp, lp);
}

bool FindBarWnd::PreTranslateMessage(MSG& msg) {
    if (msg.message != WM_KEYDOWN) {
        return false;
    }
    if (msg.wParam == VK_ESCAPE) {
        HideFindBar(win);
        return true;
    }
    if (msg.wParam == VK_RETURN) {
        if (IsShiftPressed()) {
            FindPrev(win);
        } else {
            FindNext(win);
        }
        return true;
    }
    return false;
}

bool FindBarWnd::OnCommand(WPARAM wparam, LPARAM) {
    int cmd = LOWORD(wparam);
    switch (cmd) {
        case CmdFindPrev:
            FindPrev(win);
            return true;
        case CmdFindNext:
            FindNext(win);
            return true;
        case CmdFindToggleMatchCase:
            FindToggleMatchCase(win);
            return true;
        case kFindBarCloseCmdId:
            HideFindBar(win);
            return true;
    }
    return false;
}

//--- public API

FindBarWnd* CreateFindBar(MainWindow* win) {
    auto bar = new FindBarWnd();
    if (!bar->Create(win)) {
        delete bar;
        return nullptr;
    }
    return bar;
}

void DeleteFindBar(MainWindow* win) {
    if (!win->findBar) {
        return;
    }
    delete win->findBar;
    win->findBar = nullptr;
    win->hwndFindEdit = nullptr;
}

// center the bar over the search toolbar icon, both horizontally and vertically
static void PositionFindBar(FindBarWnd* bar) {
    MainWindow* win = bar->win;
    Rect btn = GetToolbarButtonScreenRect(win, CmdFindFirst);
    int cx, cy;
    if (btn.IsEmpty()) {
        Rect fr = WindowRect(win->hwndFrame);
        cx = fr.x + fr.dx - bar->barDx;
        cy = fr.y + bar->barDy;
    } else {
        cx = btn.x + btn.dx / 2 - bar->barDx / 2;
        cy = btn.y + btn.dy / 2 - bar->barDy / 2;
    }
    Rect r{cx, cy, bar->barDx, bar->barDy};
    r = ShiftRectToWorkArea(r, win->hwndFrame, true);
    SetWindowPos(bar->hwnd, HWND_TOP, r.x, r.y, r.dx, r.dy, SWP_NOACTIVATE);
}

void ShowFindBar(MainWindow* win) {
    if (!win->findBar) {
        win->findBar = CreateFindBar(win);
    }
    if (!win->findBar) {
        return;
    }
    FindBarWnd* bar = win->findBar;
    PositionFindBar(bar);
    ShowWindow(bar->hwnd, SW_SHOW);
    HwndSetFocus(win->hwndFindEdit);
    Edit_SetSel(win->hwndFindEdit, 0, -1);
}

void HideFindBar(MainWindow* win) {
    if (!win->findBar) {
        return;
    }
    AbortFinding(win, true);
    ShowWindow(win->findBar->hwnd, SW_HIDE);
    HwndSetFocus(win->hwndFrame);
}

bool IsFindBarVisible(MainWindow* win) {
    return win->findBar && IsWindowVisible(win->findBar->hwnd);
}

void FindBarReposition(MainWindow* win) {
    if (IsFindBarVisible(win)) {
        PositionFindBar(win->findBar);
    }
}

void FindBarSetStatus(MainWindow* win, const char* s) {
    if (win->findBar && win->findBar->status) {
        HwndSetText(win->findBar->status->hwnd, s ? s : "");
    }
}

void FindBarSetMatchCaseChecked(MainWindow* win, bool checked) {
    if (win->findBar && win->findBar->hwndBtns) {
        SendMessageW(win->findBar->hwndBtns, TB_CHECKBUTTON, CmdFindToggleMatchCase, MAKELONG(checked ? 1 : 0, 0));
    }
}
