/* Copyright 2024 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/WinDynCalls.h"
#include "base/Win.h"
#include "base/Dpi.h"

#include "wingui/UIModels.h"
#include "wingui/Layout.h"
#include "wingui/WinGui.h"

#include "Settings.h"
#include "GlobalPrefs.h"
#include "AppSettings.h"
#include "DocController.h"
#include "EngineBase.h"
#include "MarkdownModel.h"
#include "ProgressUpdateUI.h"
#include "TextSelection.h"
#include "TextSearch.h"
#include "SumatraPDF.h"
#include "MainWindow.h"
#include "Commands.h"
#include "Accelerators.h"
#include "SvgIcons.h"
#include "Toolbar.h"
#include "SearchAndDDE.h"
#include "FindBar.h"
#include "FindWindow.h"
#include "Translations.h"
#include "Theme.h"

// command ids for the bar's toolbar buttons; must not collide with real commands
constexpr int kFindBarCloseCmdId = (int)CmdLast + 50;
constexpr int kFindBarPinCmdId = (int)CmdLast + 52;

struct FindBarWnd : Wnd {
    MainWindow* win = nullptr;
    Edit* edit = nullptr;
    Static* status = nullptr;
    HWND hwndBtns = nullptr; // small toolbar: prev / next / match-case / close
    HIMAGELIST himl = nullptr;

    int barDx = 0;
    int barDy = 0;
    // when set, programmatic edits to the text don't kick off a search
    // (used while restoring text during a theme-change recreate)
    bool suppressTextChanged = false;

    FindBarWnd() = default;
    ~FindBarWnd() override;

    bool Create(MainWindow* win);
    void Layout();

    void OnTextChanged();

    LRESULT WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) override;
    LRESULT OnNotify(int controlId, NMHDR* nmh) override;
    bool PreTranslateMessage(MSG& msg) override;
    bool OnCommand(WPARAM wparam, LPARAM lparam) override;
};

// tooltip text for the bar's toolbar buttons
// append a command's keyboard shortcut to its tooltip, e.g. "Find Next (F3)"
static TempStr AppendCmdAccel(Str base, int cmd) {
    TempStr accel = AppendAccelKeyToMenuStringTemp(nullptr, cmd);
    if (!accel) {
        return base;
    }
    return str::JoinTemp(base, fmt(" (%s)", Str(accel.s + 1, accel.len - 1))); // +1 skips the leading \t
}

static TempStr FindBarButtonTooltip(int cmd) {
    switch (cmd) {
        case CmdFindPrev:
            return AppendCmdAccel(_TRA("Find Previous"), cmd);
        case CmdFindNext:
            return AppendCmdAccel(_TRA("Find Next"), cmd);
        case CmdFindToggleMatchCase:
            return AppendCmdAccel(_TRA("Match Case"), cmd);
        case CmdFindToggleMatchWholeWord:
            return AppendCmdAccel(_TRA("Match Whole Word"), cmd);
        case kFindBarPinCmdId:
            return _TRA("Open in a window");
        case kFindBarCloseCmdId:
            return _TRA("Close");
    }
    return {};
}

FindBarWnd::~FindBarWnd() {
    delete edit;
    delete status;
    HwndDestroyWindowSafe(&hwndBtns);
    if (himl) {
        ImageList_Destroy(himl);
    }
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
        args.isRtl = IsUIRtl();
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
        // vertically center the single line of text so it lines up with the
        // (taller, bordered) edit box's text instead of sitting at the top
        SetWindowStyle(status->hwnd, SS_CENTERIMAGE, true);
    }

    {
        DWORD style = WS_CHILD | WS_VISIBLE | TBSTYLE_FLAT | TBSTYLE_LIST | TBSTYLE_TOOLTIPS | CCS_NODIVIDER |
                      CCS_NORESIZE | CCS_NOPARENTALIGN;
        DWORD exStyle = IsUIRtl() ? WS_EX_LAYOUTRTL : 0;
        HINSTANCE hinst = GetModuleHandleW(nullptr);
        hwndBtns = CreateWindowExW(exStyle, TOOLBARCLASSNAMEW, nullptr, style, 0, 0, 0, 0, hwnd, (HMENU) nullptr, hinst,
                                   nullptr);
        // drop the visual-style button background so the flat toolbar shows the
        // bar's themed background instead of a light box in dark themes (the
        // background is painted from NM_CUSTOMDRAW in WndProc)
        SetWindowTheme(hwndBtns, L"", L"");
        SendMessageW(hwndBtns, TB_BUTTONSTRUCTSIZE, (WPARAM)sizeof(TBBUTTON), 0);

        int isz = RoundUp(DpiScale(hwnd, 16), 4);
        himl = BuildStdToolbarImageList(isz);
        SendMessageW(hwndBtns, TB_SETIMAGELIST, 0, (LPARAM)himl);
        SendMessageW(hwndBtns, TB_SETBUTTONSIZE, 0, MAKELONG(isz, isz));

        TBBUTTON b[6]{};
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
        b[3].iBitmap = (int)TbIcon::MatchWholeWord;
        b[3].idCommand = CmdFindToggleMatchWholeWord;
        b[3].fsState = TBSTATE_ENABLED;
        b[3].fsStyle = BTNS_CHECK;
        b[4].iBitmap = (int)TbIcon::ArrowsDiagonal;
        b[4].idCommand = kFindBarPinCmdId;
        b[4].fsState = TBSTATE_ENABLED;
        b[4].fsStyle = BTNS_BUTTON;
        b[5].iBitmap = (int)TbIcon::Close;
        b[5].idCommand = kFindBarCloseCmdId;
        b[5].fsState = TBSTATE_ENABLED;
        b[5].fsStyle = BTNS_BUTTON;
        SendMessageW(hwndBtns, TB_ADDBUTTONS, 6, (LPARAM)&b);
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
    if (suppressTextChanged) {
        return;
    }
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
    if (msg == WM_NOTIFY) {
        // the embedded toolbar paints a light button background in dark themes;
        // repaint it with the bar's theme background so the icons sit on the
        // same color as the rest of the bar
        auto nmh = (NMHDR*)lp;
        if (nmh->hwndFrom == hwndBtns && nmh->code == NM_CUSTOMDRAW) {
            auto cd = (NMTBCUSTOMDRAW*)nmh;
            auto stage = cd->nmcd.dwDrawStage;
            if (stage == CDDS_PREPAINT || stage == CDDS_ITEMPREPAINT) {
                // reuse the bar's cached background brush (rebuilt on theme change
                // via SetColors) instead of allocating one per paint
                FillRect(cd->nmcd.hdc, &cd->nmcd.rc, BackgroundBrush());
                return stage == CDDS_PREPAINT ? CDRF_NOTIFYITEMDRAW : CDRF_DODEFAULT;
            }
        }
    }
    return WndProcDefault(h, msg, wp, lp);
}

LRESULT FindBarWnd::OnNotify(int, NMHDR* nmh) {
    if (nmh->code == TTN_GETDISPINFOW) {
        auto di = (NMTTDISPINFOW*)nmh;
        TempStr s = FindBarButtonTooltip((int)nmh->idFrom);
        if (s) {
            lstrcpynW(di->szText, CWStrTemp(s), dimof(di->szText));
            di->lpszText = di->szText;
        }
    }
    return 0;
}

bool FindBarWnd::PreTranslateMessage(MSG& msg) {
    if (msg.message != WM_KEYDOWN) {
        return false;
    }
    // the find edit lives in this owned popup, not as a child of the frame, so
    // the frame's edit accelerator table doesn't reach it; handle the find keys
    // here (Esc, Enter/Shift+Enter, F3/Shift+F3)
    switch (msg.wParam) {
        case 'F':
            if (IsCtrlPressed() && !IsAltPressed()) {
                FocusFindEditSelectAll(win);
                return true;
            }
            break;
        case VK_ESCAPE:
            HideFindBar(win);
            return true;
        case VK_RETURN:
        case VK_F3:
            // Enter forces a pending debounced search to start now (find the
            // first match) instead of advancing to the next one (issue #4626)
            if (msg.wParam == VK_RETURN && FindFlushPendingSearch(win)) {
                return true;
            }
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
        case CmdFindToggleMatchWholeWord:
            FindToggleMatchWholeWord(win);
            return true;
        case kFindBarPinCmdId:
            ToggleFloatingFindUI(win); // pop out into the floating window
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

// rebuild the bar so it picks up new theme colors / icons (called on theme change)
void RecreateFindBar(MainWindow* win) {
    if (!win->findBar) {
        return;
    }
    // stop any in-flight find/count that captured the old bar's state
    AbortFinding(win, true);
    bool wasVisible = IsWindowVisible(win->findBar->hwnd);
    TempStr text = wasVisible ? str::DupTemp(HwndGetTextTemp(win->hwndFindEdit)) : nullptr;
    DeleteFindBar(win);
    win->findBar = CreateFindBar(win);
    if (win->findBar && wasVisible) {
        ShowFindBar(win);
        if (len(text) > 0) {
            // restore the text without re-running the search (the existing
            // document highlight is preserved across the recreate)
            win->findBar->suppressTextChanged = true;
            HwndSetText(win->hwndFindEdit, text);
            win->findBar->suppressTextChanged = false;
        }
    }
}

// Position the bar at the right edge of the window so it doesn't cover the
// toolbar (issue #5739). The x is always the same as if the toolbar were hidden;
// only the y follows the toolbar: centered on the search icon when the toolbar
// is shown, else just below the frame top.
static void PositionFindBar(FindBarWnd* bar) {
    MainWindow* win = bar->win;
    Rect btn = GetToolbarButtonScreenRect(win, CmdFindFirst);
    Rect fr = WindowRect(win->hwndFrame);
    // Align to the right edge of the client area, not the outer window rect:
    // WindowRect includes the resize border (and sits off-screen when maximized),
    // which pushed the bar a few pixels too far right (#5762).
    Rect frClient = MapLtrClientRectToScreen(win->hwndFrame, ClientRect(win->hwndFrame));
    int cx = frClient.x + frClient.dx - bar->barDx;
    int cy;
    if (btn.IsEmpty()) {
        cy = fr.y + bar->barDy;
    } else {
        cy = btn.y + btn.dy / 2 - bar->barDy / 2;
    }
    Rect r{cx, cy, bar->barDx, bar->barDy};
    r = ShiftRectToWorkArea(r, win->hwndFrame, true);
    SetWindowPos(bar->hwnd, HWND_TOP, r.x, r.y, r.dx, r.dy, SWP_NOACTIVATE);
}

static void ShowCompactBar(MainWindow* win) {
    if (!win->findBar) {
        win->findBar = CreateFindBar(win);
    }
    if (!win->findBar) {
        return;
    }
    FindBarWnd* bar = win->findBar;
    win->hwndFindEdit = bar->edit->hwnd; // make this the active find edit
    // reflect the current match-case / whole-word state on the toggle buttons
    FindBarSetMatchCaseChecked(win, win->findMatchCase);
    FindBarSetMatchWholeWordChecked(win, win->findMatchWholeWord);
    PositionFindBar(bar);
    ShowWindow(bar->hwnd, SW_SHOW);
    HwndSetFocus(win->hwndFindEdit);
    Edit_SetSel(win->hwndFindEdit, 0, -1);
}

// "ShowFindBar" is the entry point used by FindFirst/Ctrl+F; it shows whichever
// find UI the user has chosen (compact overlay or floating window)
void ShowFindBar(MainWindow* win) {
    if (gGlobalPrefs->searchUIFloating) {
        ShowFindWindow(win);
        return;
    }
    ShowCompactBar(win);
}

void HideFindBar(MainWindow* win) {
    // drop the cached results: they belong to this search/document and must not
    // be shown or navigated into after the find UI is reopened (e.g. on another
    // tab, which would carry the previous document's page/glyph coordinates)
    ClearFindMatches(win);
    if (win->AsMarkdown()) {
        // remove in-page find highlights in the webview (no-op for IE backend)
        win->AsMarkdown()->FindClear();
    }
    if (IsFindWindowVisible(win)) {
        HideFindWindow(win);
        return;
    }
    if (!win->findBar) {
        return;
    }
    AbortFinding(win, true);
    ShowWindow(win->findBar->hwnd, SW_HIDE);
    HwndSetFocus(win->hwndFrame);
    ScheduleRepaint(win, 0);
}

// note: the floating window is not anchored to the search icon, so "visible"
// here means specifically the compact bar (used to reposition it on move)
bool IsFindBarVisible(MainWindow* win) {
    return win->findBar && IsWindowVisible(win->findBar->hwnd);
}

bool IsFindUIVisible(MainWindow* win) {
    return IsFindBarVisible(win) || IsFindWindowVisible(win);
}

void FocusFindEditSelectAll(MainWindow* win) {
    if (!win->hwndFindEdit) {
        return;
    }
    HwndSetFocus(win->hwndFindEdit);
    Edit_SetSel(win->hwndFindEdit, 0, -1);
}

void ToggleFloatingFindUI(MainWindow* win) {
    TempStr text = win->hwndFindEdit ? str::DupTemp(HwndGetTextTemp(win->hwndFindEdit)) : nullptr;
    // remember the caret/selection (LOWORD start, HIWORD end) so it survives the switch
    DWORD sel = win->hwndFindEdit ? (DWORD)Edit_GetSel(win->hwndFindEdit) : 0;
    bool wasShowing = IsFindBarVisible(win) || IsFindWindowVisible(win);

    HideFindBar(win); // dispatches: hides whichever find UI is currently visible

    gGlobalPrefs->searchUIFloating = !gGlobalPrefs->searchUIFloating;
    SaveSettings();

    if (!wasShowing) {
        return; // just persist the preference; nothing was open
    }
    ShowFindBar(win); // shows the now-active UI and repoints win->hwndFindEdit
    if (len(text) > 0) {
        HwndSetText(win->hwndFindEdit, text); // restore text (re-runs the search)
    }
    HwndSetFocus(win->hwndFindEdit);
    // restore the caret/selection last, after Show/SetText reset it
    Edit_SetSel(win->hwndFindEdit, LOWORD(sel), HIWORD(sel));
}

void FindBarReposition(MainWindow* win) {
    if (!IsFindBarVisible(win)) {
        return;
    }
    // the current document may not support find (e.g. switched to an
    // image-only doc / CHM); don't leave an orphaned, inert bar floating
    if (!NeedsFindUI(win)) {
        HideFindBar(win);
        return;
    }
    PositionFindBar(win->findBar);
}

void FindBarSetStatus(MainWindow* win, Str s) {
    if (gGlobalPrefs->searchUIFloating) {
        FindWindowSetStatus(win, s);
        return;
    }
    if (win->findBar && win->findBar->status) {
        HwndSetText(win->findBar->status->hwnd, s ? s : StrL(""));
    }
}

void FindBarSetMatchCaseChecked(MainWindow* win, bool checked) {
    if (gGlobalPrefs->searchUIFloating) {
        FindWindowSetMatchCaseChecked(win, checked);
        return;
    }
    if (win->findBar && win->findBar->hwndBtns) {
        SendMessageW(win->findBar->hwndBtns, TB_CHECKBUTTON, CmdFindToggleMatchCase, MAKELONG(checked ? 1 : 0, 0));
    }
}

void FindBarSetMatchWholeWordChecked(MainWindow* win, bool checked) {
    if (gGlobalPrefs->searchUIFloating) {
        FindWindowSetMatchWholeWordChecked(win, checked);
        return;
    }
    if (win->findBar && win->findBar->hwndBtns) {
        SendMessageW(win->findBar->hwndBtns, TB_CHECKBUTTON, CmdFindToggleMatchWholeWord, MAKELONG(checked ? 1 : 0, 0));
    }
}
