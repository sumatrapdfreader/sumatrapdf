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
#include "CommandPalette.h" // DrawMaybeHighlightedText
#include "Translations.h"
#include "Theme.h"

#include "utils/Log.h"

// command ids for the window's toolbar buttons (handled in OnCommand)
constexpr int kFindWinPinCmdId = (int)CmdLast + 51;

// list model backed live by win->findMatches (the snippet for each match)
struct FindResultsModel : ListBoxModel {
    MainWindow* win = nullptr;
    explicit FindResultsModel(MainWindow* win) {
        this->win = win;
    }
    int ItemsCount() override {
        return (int)win->findMatches.size();
    }
    const char* Item(int i) override {
        const char* s = win->findMatches[i].snippet;
        return s ? s : "";
    }
};

struct FindWindowWnd : Wnd {
    MainWindow* win = nullptr;
    Edit* edit = nullptr;
    Static* status = nullptr;
    HWND hwndBtns = nullptr; // prev / next / match-case / unpin(dock)
    HIMAGELIST himl = nullptr;
    ListBox* results = nullptr;
    StrVec filterWords;  // search term(s) to highlight in snippets
    Vec<u8> hlScratch;   // reused highlight mask for DrawMaybeHighlightedText

    FindWindowWnd() = default;
    ~FindWindowWnd() override;

    bool Create(MainWindow* win);
    void Layout();
    void SavePos();
    void RefreshResults();

    void OnTextChanged();
    void DrawResultItem(ListBox::DrawItemEvent* ev);
    void OnResultSelected();
    bool MoveResultSelection(WPARAM vkey);
    int CurrentMatchIndex();        // list index of the document's current match, or -1
    int FirstMatchFromCurrentPage(); // list index of the first match at/after the current page

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
    delete results; // also deletes its FindResultsModel
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
        b[3].iBitmap = (int)TbIcon::ArrowsDiagonalMinimize;
        b[3].idCommand = kFindWinPinCmdId;
        b[3].fsState = TBSTATE_ENABLED;
        b[3].fsStyle = BTNS_BUTTON;
        SendMessageW(hwndBtns, TB_ADDBUTTONS, 4, (LPARAM)&b);
        SendMessageW(hwndBtns, TB_AUTOSIZE, 0, 0);
    }

    {
        ListBox::CreateArgs args;
        args.parent = hwnd;
        args.font = GetDefaultGuiFont();
        results = new ListBox();
        results->onDrawItem =
            MkMethod1<FindWindowWnd, ListBox::DrawItemEvent*, &FindWindowWnd::DrawResultItem>(this);
        results->onSelectionChanged = MkMethod0<FindWindowWnd, &FindWindowWnd::OnResultSelected>(this);
        results->onDoubleClick = MkMethod0<FindWindowWnd, &FindWindowWnd::OnResultSelected>(this);
        results->SetColors(colTxt, colBg);
        results->Create(args);
        results->SetModel(new FindResultsModel(win));
    }

    return true;
}

void FindWindowWnd::Layout() {
    // a WS_CAPTION/WS_THICKFRAME window gets WM_SIZE during CreateCustom, before
    // the child controls exist; ignore layout until they're created
    if (!edit || !status || !hwndBtns || !results) {
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

    // the results list fills the rest of the window below the search row
    int listTop = y + rowDy + pad;
    int listDy = std::max(0, rc.dy - listTop - pad);
    MoveWindow(results->hwnd, pad, listTop, std::max(0, rc.dx - 2 * pad), listDy, TRUE);
}

void FindWindowWnd::RefreshResults() {
    if (!results) {
        return;
    }
    // rebuild the highlight terms from the current search text
    filterWords.Reset();
    TempStr term = win->findCountText ? ToUtf8Temp(win->findCountText) : nullptr;
    if (str::IsEmpty(term)) {
        term = win->hwndFindEdit ? HwndGetTextTemp(win->hwndFindEdit) : nullptr;
    }
    if (!str::IsEmpty(term)) {
        filterWords.Append(term);
    }
    FillWithItems(results->hwnd, results->model);
    // keep a result selected so it's visible as you type and Next/Prev have a
    // sensible starting point.
    int sel = CurrentMatchIndex();
    if (sel >= 0) {
        // the document already sits on a match (find-as-you-type found it): just
        // mirror it in the list, no navigation
        results->SetCurrentSelection(sel);
    } else if (win->findMatches.size() > 0) {
        // find-as-you-type gave up (it self-cancels for matches on far pages),
        // so the document isn't on a match. Drive selection + navigation off the
        // full count instead: go to the first match at/after the current page,
        // like find-as-you-type would have.
        sel = FirstMatchFromCurrentPage();
        results->SetCurrentSelection(sel);
        OnResultSelected();
    }
}

void FindWindowWnd::DrawResultItem(ListBox::DrawItemEvent* ev) {
    ListBox* lb = ev->listBox;
    if (ev->itemIndex < 0 || ev->itemIndex >= (int)win->findMatches.size()) {
        return;
    }
    HDC hdc = ev->hdc;
    RECT rc = ev->itemRect;

    COLORREF colBg = IsSpecialColor(lb->bgColor) ? GetSysColor(COLOR_WINDOW) : lb->bgColor;
    COLORREF colText = IsSpecialColor(lb->textColor) ? GetSysColor(COLOR_WINDOWTEXT) : lb->textColor;
    if (ev->selected) {
        colBg = AccentColor(colBg, 30);
    }
    SetBkColor(hdc, colBg);
    ExtTextOutW(hdc, 0, 0, ETO_OPAQUE, &rc, nullptr, 0, nullptr);
    SetBkMode(hdc, TRANSPARENT);

    HFONT oldFont = lb->font ? SelectFont(hdc, lb->font) : nullptr;
    int pad = DpiScale(lb->hwnd, 6);
    RECT rcText = rc;
    rcText.left += pad;
    rcText.right -= pad;

    // page number, right-aligned and muted
    const FindMatch& fm = win->findMatches[ev->itemIndex];
    TempStr pageStr = str::FormatTemp("%s", win->ctrl->GetPageLabeTemp(fm.startPage));
    WCHAR* pageW = ToWStrTemp(pageStr);
    SIZE pSz{};
    GetTextExtentPoint32W(hdc, pageW, str::Leni(pageW), &pSz);
    RECT rcPage = rcText;
    rcPage.left = rcText.right - pSz.cx;
    SetTextColor(hdc, AccentColor(colText, 80));
    DrawTextW(hdc, pageW, -1, &rcPage, DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_RIGHT);

    // snippet on the left, with the matched term highlighted
    SetTextColor(hdc, colText);
    rcText.right = rcPage.left - DpiScale(lb->hwnd, 10);
    DrawMaybeHighlightedTextArgs args(filterWords, hlScratch);
    args.hdc = hdc;
    args.rc = rcText;
    args.text = fm.snippet ? fm.snippet : "";
    args.colBg = colBg;
    args.isRtl = false;
    args.drawFmt = DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_LEFT | DT_END_ELLIPSIS;
    DrawMaybeHighlightedText(args);

    if (oldFont) {
        SelectFont(hdc, oldFont);
    }
}

void FindWindowWnd::OnResultSelected() {
    int idx = results ? results->GetCurrentSelection() : -1;
    if (idx < 0 || idx >= (int)win->findMatches.size()) {
        return;
    }
    const FindMatch& fm = win->findMatches[idx];
    GoToFindMatch(win, fm.startPage, fm.startGlyph, fm.endPage, fm.endGlyph);
}

// list index of the match the document is currently on (so the selection can
// track the current match), or -1 if it isn't in the list
int FindWindowWnd::CurrentMatchIndex() {
    DisplayModel* dm = win->AsFixed();
    if (!dm || !dm->textSearch) {
        return -1;
    }
    int page = dm->textSearch->startPage;
    int glyph = dm->textSearch->startGlyph;
    int n = (int)win->findMatches.size();
    for (int i = 0; i < n; i++) {
        const FindMatch& fm = win->findMatches[i];
        if (fm.startPage == page && fm.startGlyph == glyph) {
            return i;
        }
    }
    return -1;
}

// first match at/after the current page (matches are in page order); wraps to
// the first match if none follow. Mirrors find-as-you-type's FindFirst(curPage).
int FindWindowWnd::FirstMatchFromCurrentPage() {
    int n = (int)win->findMatches.size();
    if (n == 0) {
        return -1;
    }
    int curPage = win->ctrl ? win->ctrl->CurrentPageNo() : 1;
    for (int i = 0; i < n; i++) {
        if (win->findMatches[i].startPage >= curPage) {
            return i;
        }
    }
    return 0;
}

// move the results-list selection (keyboard arrows or the Next/Prev buttons)
// while focus stays in the search edit, navigating to the newly selected match.
// Returns false (not handled) when there are no results, so the caller can fall
// back to a normal document search.
bool FindWindowWnd::MoveResultSelection(WPARAM vkey) {
    if (!results) {
        return false;
    }
    int n = (int)win->findMatches.size();
    if (n == 0) {
        return false;
    }
    constexpr int kPage = 10;
    int cur = results->GetCurrentSelection();
    if (cur < 0) {
        cur = CurrentMatchIndex(); // start from where the document already is
    }
    int idx;
    switch (vkey) {
        case VK_DOWN:
            idx = (cur < 0) ? 0 : cur + 1;
            break;
        case VK_UP:
            idx = (cur < 0) ? 0 : cur - 1;
            break;
        case VK_NEXT: // Page Down
            idx = (cur < 0 ? 0 : cur) + kPage;
            break;
        case VK_PRIOR: // Page Up
            idx = (cur < 0 ? 0 : cur) - kPage;
            break;
        default:
            return false;
    }
    idx = limitValue(idx, 0, n - 1);
    if (idx == cur) {
        return true; // already at the end/start; swallow the key
    }
    results->SetCurrentSelection(idx);
    OnResultSelected();
    return true;
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
        case VK_F3: {
            // step through the results list; fall back to a document search when
            // there's no list (e.g. count not ready)
            WPARAM dir = IsShiftPressed() ? VK_UP : VK_DOWN;
            if (!MoveResultSelection(dir)) {
                IsShiftPressed() ? FindPrev(win) : FindNext(win);
            }
            return true;
        }
        case VK_DOWN:
        case VK_UP:
        case VK_NEXT:
        case VK_PRIOR:
            // walk the results list from the search edit
            return MoveResultSelection(msg.wParam);
    }
    return false;
}

bool FindWindowWnd::OnCommand(WPARAM wparam, LPARAM) {
    int cmd = LOWORD(wparam);
    switch (cmd) {
        case CmdFindPrev:
            if (!MoveResultSelection(VK_UP)) {
                FindPrev(win);
            }
            return true;
        case CmdFindNext:
            if (!MoveResultSelection(VK_DOWN)) {
                FindNext(win);
            }
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
    // populate the results list: show what's cached, and (re)run the search for
    // the current term so snippets get built now that the window is visible
    w->RefreshResults();
    if (win->hwndFindEdit && HwndGetTextLen(win->hwndFindEdit) > 0) {
        OnFindBarTextChanged(win);
    }
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

void FindWindowRefreshResults(MainWindow* win) {
    if (IsFindWindowVisible(win)) {
        win->findWindow->RefreshResults();
    }
}
