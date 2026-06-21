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
#include "GlobalPrefs.h"
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
#include "FindWindow.h"
#include "Translations.h"
#include "Theme.h"

#include "utils/Log.h"

// command ids for the window's toolbar buttons (handled in OnCommand)
constexpr int kFindWinPinCmdId = (int)CmdLast + 51;

struct FindWindowWnd : Wnd {
    MainWindow* win = nullptr;
    Edit* edit = nullptr;
    Static* status = nullptr;
    HWND hwndBtns = nullptr; // prev / next / match-case / unpin(dock)
    HIMAGELIST himl = nullptr;

    FindWindowWnd() = default;
    ~FindWindowWnd() override;

    bool Create(MainWindow* win);
    void Layout();
    void SavePos();

    void OnTextChanged();

    LRESULT WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) override;
    LRESULT OnNotify(int controlId, NMHDR* nmh) override;
    bool PreTranslateMessage(MSG& msg) override;
    bool OnCommand(WPARAM wparam, LPARAM lparam) override;
};

static const char* FindWindowButtonTooltip(int cmd) {
    switch (cmd) {
        case CmdFindPrev:
            return _TRA("Find Previous");
        case CmdFindNext:
            return _TRA("Find Next");
        case CmdFindToggleMatchCase:
            return _TRA("Match Case");
        case kFindWinPinCmdId:
            return _TRA("Dock to toolbar");
    }
    return nullptr;
}

FindWindowWnd::~FindWindowWnd() {
    delete edit;
    delete status;
    HwndDestroyWindowSafe(&hwndBtns);
    if (himl) {
        ImageList_Destroy(himl);
    }
}

bool FindWindowWnd::Create(MainWindow* mainWin) {
    win = mainWin;

    auto colBg = ThemeWindowControlBackgroundColor();
    auto colTxt = ThemeWindowTextColor();

    {
        CreateCustomArgs args;
        args.visible = false;
        args.title = _TRA("Find");
        args.style = WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME;
        args.exStyle = WS_EX_TOOLWINDOW; // small caption, off the taskbar
        args.isRtl = IsUIRtl();
        CreateCustom(args);
    }
    if (!hwnd) {
        return false;
    }
    // owned by the frame so it groups/minimizes with it but isn't a child
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
        edit->maxDx = DpiScale(hwnd, 1000);
        edit->SetColors(colTxt, colBg);
        edit->Create(args);
        edit->onTextChanged = MkMethod0<FindWindowWnd, &FindWindowWnd::OnTextChanged>(this);
    }

    {
        Static::CreateArgs args;
        args.parent = hwnd;
        args.text = "";
        args.isRtl = IsUIRtl();
        status = new Static();
        status->SetColors(colTxt, colBg);
        status->Create(args);
        SetWindowStyle(status->hwnd, SS_CENTERIMAGE, true);
    }

    {
        DWORD style = WS_CHILD | WS_VISIBLE | TBSTYLE_FLAT | TBSTYLE_LIST | TBSTYLE_TOOLTIPS | CCS_NODIVIDER |
                      CCS_NORESIZE | CCS_NOPARENTALIGN;
        DWORD exStyle = IsUIRtl() ? WS_EX_LAYOUTRTL : 0;
        HINSTANCE hinst = GetModuleHandleW(nullptr);
        hwndBtns = CreateWindowExW(exStyle, TOOLBARCLASSNAMEW, nullptr, style, 0, 0, 0, 0, hwnd, (HMENU) nullptr, hinst,
                                   nullptr);
        SendMessageW(hwndBtns, TB_BUTTONSTRUCTSIZE, (WPARAM)sizeof(TBBUTTON), 0);

        int isz = RoundUp(DpiScale(hwnd, 16), 4);
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
        b[3].iBitmap = (int)TbIcon::Pin;
        b[3].idCommand = kFindWinPinCmdId;
        b[3].fsState = TBSTATE_ENABLED;
        b[3].fsStyle = BTNS_BUTTON;
        SendMessageW(hwndBtns, TB_ADDBUTTONS, 4, (LPARAM)&b);
        SendMessageW(hwndBtns, TB_AUTOSIZE, 0, 0);
    }

    return true;
}

void FindWindowWnd::Layout() {
    // a WS_CAPTION/WS_THICKFRAME window gets WM_SIZE during CreateCustom, before
    // the child controls exist; ignore layout until they're created
    if (!edit || !status || !hwndBtns) {
        return;
    }
    Rect rc = ClientRect(hwnd);
    int pad = DpiScale(hwnd, 8);
    int gap = DpiScale(hwnd, 6);
    int statusDx = DpiScale(hwnd, 90);

    int editDy = edit->GetIdealSize().dy;
    SIZE tbSz{};
    SendMessageW(hwndBtns, TB_GETMAXSIZE, 0, (LPARAM)&tbSz);
    int rowDy = std::max(editDy, (int)tbSz.cy);
    int y = pad;

    int tbX = rc.dx - pad - (int)tbSz.cx;
    MoveWindow(hwndBtns, tbX, y + (rowDy - (int)tbSz.cy) / 2, (int)tbSz.cx, (int)tbSz.cy, TRUE);
    int statusX = tbX - gap - statusDx;
    MoveWindow(status->hwnd, statusX, y + (rowDy - editDy) / 2, statusDx, editDy, TRUE);
    int editX = pad;
    int editDx = statusX - gap - editX;
    if (editDx < DpiScale(hwnd, 40)) {
        editDx = DpiScale(hwnd, 40);
    }
    MoveWindow(edit->hwnd, editX, y + (rowDy - editDy) / 2, editDx, editDy, TRUE);
    // the area below (rc from y+rowDy+pad to bottom) is reserved for the
    // results list, added in a later phase
}

void FindWindowWnd::SavePos() {
    if (!IsWindowVisible(hwnd)) {
        return;
    }
    Rect r = WindowRect(hwnd);
    gGlobalPrefs->searchUIWindowPos = r;
}

void FindWindowWnd::OnTextChanged() {
    OnFindBarTextChanged(win);
}

LRESULT FindWindowWnd::WndProc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_SIZE:
            Layout();
            break;
        case WM_EXITSIZEMOVE:
            SavePos();
            break;
        case WM_GETMINMAXINFO: {
            auto mmi = (MINMAXINFO*)lp;
            mmi->ptMinTrackSize.x = DpiScale(h, 280);
            mmi->ptMinTrackSize.y = DpiScale(h, 120);
            return 0;
        }
        case WM_CLOSE:
            // the caption close button hides the bar instead of destroying it
            HideFindWindow(win);
            return 0;
    }
    return WndProcDefault(h, msg, wp, lp);
}

LRESULT FindWindowWnd::OnNotify(int, NMHDR* nmh) {
    if (nmh->code == TTN_GETDISPINFOW) {
        auto di = (NMTTDISPINFOW*)nmh;
        const char* s = FindWindowButtonTooltip((int)nmh->idFrom);
        if (s) {
            lstrcpynW(di->szText, ToWStrTemp(s), dimof(di->szText));
            di->lpszText = di->szText;
        }
    }
    return 0;
}

bool FindWindowWnd::PreTranslateMessage(MSG& msg) {
    if (msg.message != WM_KEYDOWN) {
        return false;
    }
    switch (msg.wParam) {
        case VK_ESCAPE:
            HideFindWindow(win);
            return true;
        case VK_RETURN:
        case VK_F3:
            if (IsShiftPressed()) {
                FindPrev(win);
            } else {
                FindNext(win);
            }
            return true;
    }
    return false;
}

bool FindWindowWnd::OnCommand(WPARAM wparam, LPARAM) {
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
        case kFindWinPinCmdId:
            ToggleFloatingFindUI(win); // dock back to the compact toolbar bar
            return true;
    }
    return false;
}

//--- public API

FindWindowWnd* CreateFindWindow(MainWindow* win) {
    auto w = new FindWindowWnd();
    if (!w->Create(win)) {
        delete w;
        return nullptr;
    }
    return w;
}

void DeleteFindWindow(MainWindow* win) {
    if (!win->findWindow) {
        return;
    }
    delete win->findWindow;
    win->findWindow = nullptr;
}

static void PositionFindWindow(FindWindowWnd* w) {
    MainWindow* win = w->win;
    Rect r = gGlobalPrefs->searchUIWindowPos;
    if (r.IsEmpty()) {
        // default: a reasonable size near the top-right of the frame
        Rect fr = WindowRect(win->hwndFrame);
        int dx = DpiScale(w->hwnd, 520);
        int dy = DpiScale(w->hwnd, 360);
        r = {fr.x + fr.dx - dx - DpiScale(w->hwnd, 40), fr.y + DpiScale(w->hwnd, 80), dx, dy};
    }
    r = ShiftRectToWorkArea(r, win->hwndFrame, true);
    SetWindowPos(w->hwnd, HWND_TOP, r.x, r.y, r.dx, r.dy, SWP_NOACTIVATE);
}

void ShowFindWindow(MainWindow* win) {
    if (!win->findWindow) {
        win->findWindow = CreateFindWindow(win);
    }
    if (!win->findWindow) {
        return;
    }
    FindWindowWnd* w = win->findWindow;
    win->hwndFindEdit = w->edit->hwnd; // make this the active find edit
    FindWindowSetMatchCaseChecked(win, win->findMatchCase);
    PositionFindWindow(w);
    w->Layout();
    ShowWindow(w->hwnd, SW_SHOW);
    HwndSetFocus(win->hwndFindEdit);
    Edit_SetSel(win->hwndFindEdit, 0, -1);
}

void HideFindWindow(MainWindow* win) {
    if (!win->findWindow) {
        return;
    }
    win->findWindow->SavePos();
    AbortFinding(win, true);
    ShowWindow(win->findWindow->hwnd, SW_HIDE);
    HwndSetFocus(win->hwndFrame);
}

bool IsFindWindowVisible(MainWindow* win) {
    return win->findWindow && IsWindowVisible(win->findWindow->hwnd);
}

void FindWindowSetStatus(MainWindow* win, const char* s) {
    if (win->findWindow && win->findWindow->status) {
        HwndSetText(win->findWindow->status->hwnd, s ? s : "");
    }
}

void FindWindowSetMatchCaseChecked(MainWindow* win, bool checked) {
    if (win->findWindow && win->findWindow->hwndBtns) {
        SendMessageW(win->findWindow->hwndBtns, TB_CHECKBUTTON, CmdFindToggleMatchCase, MAKELONG(checked ? 1 : 0, 0));
    }
}
