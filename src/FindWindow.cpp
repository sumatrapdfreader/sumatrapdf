/* Copyright 2024 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/WinDynCalls.h"
#include "base/UITask.h"
#include "base/Win.h"
#include "base/Dpi.h"

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
#include "Accelerators.h"
#include "SvgIcons.h"
#include "Toolbar.h"
#include "SearchAndDDE.h"
#include "FindBar.h"
#include "FindWindow.h"
#include "FilterHighlightDraw.h"
#include "Translations.h"
#include "Theme.h"
#include "DarkModeSubclass.h"

// match the frame's title bar to the current theme (dark caption in dark mode)
static void ApplyTitleBarTheme(HWND hwnd) {
    if (UseDarkModeLib()) {
        DarkMode::setDarkTitleBarEx(hwnd, true);
    }
}

// command ids for the window's toolbar buttons (handled in OnCommand)
constexpr int kFindWinPinCmdId = (int)CmdLast + 51;

struct FindWindowWnd;

struct DeferredGoToFindMatchData {
    MainWindow* win = nullptr;
    FindWindowWnd* findWindow = nullptr;
    int startPage = 0;
    int startGlyph = 0;
    int endPage = 0;
    int endGlyph = 0;
    LONG epoch = 0;
};

// list model backed live by win->findMatches (the snippet for each match)
struct FindResultsModel : ListBoxModel {
    MainWindow* win = nullptr;
    explicit FindResultsModel(MainWindow* win) { this->win = win; }
    int ItemsCount() override { return len(win->findMatches); }
    Str Item(int i) override { return win->findMatches[i].snippet; }
};

struct FindWindowWnd : Wnd {
    MainWindow* win = nullptr;
    Edit* edit = nullptr;
    Static* status = nullptr;
    HWND hwndBtns = nullptr; // prev / next / match-case / unpin(dock)
    HIMAGELIST himl = nullptr;
    ListBox* results = nullptr;
    StrVec filterWords; // search term(s) to highlight in snippets
    Vec<u8> hlScratch;  // reused highlight mask for DrawMaybeHighlightedText
    // coalesce rapid list selections: only the latest deferred navigation runs
    LONG pendingNavEpoch = 0;
    // in an interactive size/move loop (between WM_ENTERSIZEMOVE/EXITSIZEMOVE)
    bool inSizeMove = false;
    // list redraw is paused only while interactively *resizing* (a WM_SIZE
    // arrived during the size/move loop), not while merely moving the window
    bool listRedrawPaused = false;

    FindWindowWnd() = default;
    ~FindWindowWnd() override;

    bool Create(MainWindow* win);
    void Layout();
    void SavePos();
    void RefreshResults();
    void UpdateTheme();

    void OnTextChanged();
    void DrawResultItem(ListBox::DrawItemEvent* ev);
    void OnResultSelected();
    bool MoveResultSelection(WPARAM vkey);
    int CurrentMatchIndex();         // list index of the document's current match, or -1
    int FirstMatchFromCurrentPage(); // list index of the first match at/after the current page

    LRESULT WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) override;
    LRESULT OnNotify(int controlId, NMHDR* nmh) override;
    bool PreTranslateMessage(MSG& msg) override;
    bool OnCommand(WPARAM wparam, LPARAM lparam) override;
};

static void DeferredGoToFindMatch(DeferredGoToFindMatchData* d) {
    AutoDelete del(d);
    if (!IsMainWindowValid(d->win) || !d->findWindow) {
        return;
    }
    if (d->epoch != d->findWindow->pendingNavEpoch) {
        return;
    }
    GoToFindMatch(d->win, d->startPage, d->startGlyph, d->endPage, d->endGlyph);
}

// append a command's keyboard shortcut to its tooltip, e.g. "Find Next (F3)"
static TempStr AppendCmdAccel(Str base, int cmd) {
    TempStr accel = AppendAccelKeyToMenuStringTemp(nullptr, cmd);
    if (!accel) {
        return base;
    }
    return str::JoinTemp(base, fmt(" (%s)", Str(accel.s + 1, accel.len - 1))); // +1 skips the leading \t
}

static TempStr FindWindowButtonTooltip(int cmd) {
    switch (cmd) {
        case CmdFindPrev:
            return AppendCmdAccel(_TRA("Find Previous"), cmd);
        case CmdFindNext:
            return AppendCmdAccel(_TRA("Find Next"), cmd);
        case CmdFindToggleMatchCase:
            return AppendCmdAccel(_TRA("Match Case"), cmd);
        case CmdFindToggleMatchWholeWord:
            return AppendCmdAccel(_TRA("Match Whole Word"), cmd);
        case kFindWinPinCmdId:
            return _TRA("Dock to toolbar");
    }
    return {};
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
    ApplyTitleBarTheme(hwnd);

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
        // drop the visual-style button background so the flat toolbar shows the
        // window's themed background instead of a light box in dark themes
        SetWindowTheme(hwndBtns, L"", L"");
        SendMessageW(hwndBtns, TB_BUTTONSTRUCTSIZE, (WPARAM)sizeof(TBBUTTON), 0);

        int isz = RoundUp(DpiScale(hwnd, 16), 4);
        himl = BuildStdToolbarImageList(isz);
        SendMessageW(hwndBtns, TB_SETIMAGELIST, 0, (LPARAM)himl);
        SendMessageW(hwndBtns, TB_SETBUTTONSIZE, 0, MAKELONG(isz, isz));

        TBBUTTON b[5]{};
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
        b[4].iBitmap = (int)TbIcon::ArrowsDiagonalMinimize;
        b[4].idCommand = kFindWinPinCmdId;
        b[4].fsState = TBSTATE_ENABLED;
        b[4].fsStyle = BTNS_BUTTON;
        SendMessageW(hwndBtns, TB_ADDBUTTONS, 5, (LPARAM)&b);
        SendMessageW(hwndBtns, TB_AUTOSIZE, 0, 0);
    }

    {
        ListBox::CreateArgs args;
        args.parent = hwnd;
        args.font = GetDefaultGuiFont();
        results = new ListBox();
        results->onDrawItem = MkMethod1<FindWindowWnd, ListBox::DrawItemEvent*, &FindWindowWnd::DrawResultItem>(this);
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
    int minEditDx = DpiScale(hwnd, 48);

    int editDy = edit->GetIdealSize().dy;
    SIZE tbSz{};
    SendMessageW(hwndBtns, TB_GETMAXSIZE, 0, (LPARAM)&tbSz);
    int tbW = (int)tbSz.cx;
    int tbH = (int)tbSz.cy;

    int contentDx = std::max(0, rc.dx - 2 * pad);
    // minimum width for [edit][status][toolbar] on one row without overlap
    int singleRowDx = minEditDx + gap + statusDx + gap + tbW;

    int y = pad;
    int headerDy;
    if (contentDx >= singleRowDx) {
        // wide: [edit][n/m][toolbar]
        headerDy = std::max(editDy, tbH);
        int tbX = pad + contentDx - tbW;
        int statusX = tbX - gap - statusDx;
        int editDx = statusX - gap - pad;
        MoveWindow(hwndBtns, tbX, y + (headerDy - tbH) / 2, tbW, tbH, TRUE);
        MoveWindow(status->hwnd, statusX, y + (headerDy - editDy) / 2, statusDx, editDy, TRUE);
        MoveWindow(edit->hwnd, pad, y + (headerDy - editDy) / 2, editDx, editDy, TRUE);
    } else {
        // narrow: full-width edit, then [n/m][toolbar] (issue #5692)
        MoveWindow(edit->hwnd, pad, y, contentDx, editDy, TRUE);
        y += editDy + gap;
        headerDy = editDy + gap + std::max(editDy, tbH);
        int row2Dy = std::max(editDy, tbH);
        int statusW = std::max(0, contentDx - gap - tbW);
        MoveWindow(status->hwnd, pad, y + (row2Dy - editDy) / 2, statusW, editDy, TRUE);
        int tbX = pad + contentDx - tbW;
        MoveWindow(hwndBtns, tbX, y + (row2Dy - tbH) / 2, tbW, tbH, TRUE);
    }

    // the results list fills the rest of the window below the header
    int listTop = pad + headerDy + pad;
    int listDy = std::max(0, rc.dy - listTop - pad);
    MoveWindow(results->hwnd, pad, listTop, contentDx, listDy, TRUE);
}

void FindWindowWnd::RefreshResults() {
    if (!results) {
        return;
    }
    // rebuild the highlight terms from the current search text
    filterWords.Reset();
    Str term = win->findCountText;
    if (len(term) == 0) {
        term = win->hwndFindEdit ? HwndGetTextTemp(win->hwndFindEdit) : nullptr;
    }
    if (len(term) > 0) {
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
    } else if (len(win->findMatches) > 0) {
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
    if (ev->itemIndex < 0 || ev->itemIndex >= len(win->findMatches)) {
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

    // page number in a fixed right column so it can't overlap the snippet while
    // the window is being resized (issue #5692)
    const FindMatch& fm = win->findMatches[ev->itemIndex];
    TempStr pageStr = fmt("%s", win->ctrl->GetPageLabeTemp(fm.startPage));
    int pageCch;
    WCHAR* pageW = CWStrTemp(pageStr, pageCch);
    SIZE pSz{};
    GetTextExtentPoint32W(hdc, pageW, pageCch, &pSz);
    int pageGap = DpiScale(lb->hwnd, 10);
    int pageColDx = std::max((int)pSz.cx, DpiScale(lb->hwnd, 32));
    RECT rcPage = rcText;
    rcPage.left = std::max(rcText.left, (LONG)(rcText.right - pageColDx));

    // snippet on the left, with the matched term highlighted
    RECT rcSnippet = rcText;
    rcSnippet.right = std::max(rcSnippet.left, rcPage.left - pageGap);
    if (rcSnippet.right > rcSnippet.left) {
        SetTextColor(hdc, colText);
        uint drawFmt = DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_LEFT | DT_END_ELLIPSIS;
        // clip snippet drawing so match highlights cannot bleed into the page
        // number column when the floating window is narrow (issue #5736)
        HRGN clipRgn = CreateRectRgnIndirect(&rcSnippet);
        if (clipRgn) {
            SelectClipRgn(hdc, clipRgn);
            DrawMaybeHighlightedText(hdc, rcSnippet, fm.snippet, filterWords, hlScratch, colBg, false,
                                     win->findMatchWholeWord, drawFmt);
            SelectClipRgn(hdc, nullptr);
            DeleteObject(clipRgn);
        } else {
            DrawMaybeHighlightedText(hdc, rcSnippet, fm.snippet, filterWords, hlScratch, colBg, false,
                                     win->findMatchWholeWord, drawFmt);
        }
    }

    // repaint the page column on top in case a prior draw left stray pixels
    SetBkColor(hdc, colBg);
    ExtTextOutW(hdc, 0, 0, ETO_OPAQUE, &rcPage, nullptr, 0, nullptr);
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, AccentColor(colText, 80));
    DrawTextW(hdc, pageW, -1, &rcPage, DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_RIGHT | DT_END_ELLIPSIS);

    if (oldFont) {
        SelectFont(hdc, oldFont);
    }
}

void FindWindowWnd::OnResultSelected() {
    int idx = results ? results->GetCurrentSelection() : -1;
    if (idx < 0 || idx >= len(win->findMatches)) {
        return;
    }
    const FindMatch& fm = win->findMatches[idx];
    DisplayModel* dm = win->AsFixed();
    if (dm && dm->textSearch && dm->textSearch->startPage == fm.startPage &&
        dm->textSearch->startGlyph == fm.startGlyph) {
        return; // already on this match
    }
    // defer document navigation so the results list can scroll/repaint first
    // (issue #5692). Coalesce rapid F3 / arrow presses to the latest selection.
    auto data = new DeferredGoToFindMatchData;
    data->win = win;
    data->findWindow = this;
    data->startPage = fm.startPage;
    data->startGlyph = fm.startGlyph;
    data->endPage = fm.endPage;
    data->endGlyph = fm.endGlyph;
    data->epoch = InterlockedIncrement(&pendingNavEpoch);
    uitask::Post(MkFunc0<DeferredGoToFindMatchData>(DeferredGoToFindMatch, data), "GoToFindMatch");
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
    int n = len(win->findMatches);
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
    int n = len(win->findMatches);
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
    int n = len(win->findMatches);
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
            // wrap like the compact bar's Find Next (issue #5692)
            idx = (cur < 0) ? 0 : (cur + 1) % n;
            break;
        case VK_UP:
            idx = (cur < 0) ? n - 1 : (cur - 1 + n) % n;
            break;
        case VK_NEXT: // Page Down
            // unlike the arrow keys, paging doesn't wrap around; it clamps to the
            // last match (issue #5742)
            if (cur < 0) {
                idx = 0;
            } else {
                idx = cur + kPage;
                if (idx >= n) {
                    idx = n - 1;
                }
            }
            break;
        case VK_PRIOR: // Page Up
            // clamp to the first match instead of wrapping (issue #5742)
            if (cur < 0) {
                idx = n - 1;
            } else {
                idx = cur - kPage;
                if (idx < 0) {
                    idx = 0;
                }
            }
            break;
        default:
            return false;
    }
    if (idx == cur) {
        return true; // e.g. a single match wrapping onto itself
    }
    results->SetCurrentSelection(idx);
    // ListBox_SetCurSel does not send LBN_SELCHANGE; navigate explicitly
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

// re-apply theme colors after the user switches themes. The toolbar icons are
// baked into an image list at the current text color, so rebuild it; the
// controls and caption also need recoloring.
void FindWindowWnd::UpdateTheme() {
    auto colBg = ThemeWindowControlBackgroundColor();
    auto colTxt = ThemeWindowTextColor();
    SetColors(colTxt, colBg);
    if (edit) {
        edit->SetColors(colTxt, colBg);
    }
    if (status) {
        status->SetColors(colTxt, colBg);
    }
    if (results) {
        results->SetColors(colTxt, colBg);
    }
    if (hwndBtns) {
        int isz = RoundUp(DpiScale(hwnd, 16), 4);
        HIMAGELIST oldHiml = himl;
        himl = BuildStdToolbarImageList(isz);
        SendMessageW(hwndBtns, TB_SETIMAGELIST, 0, (LPARAM)himl);
        if (oldHiml) {
            ImageList_Destroy(oldHiml);
        }
    }
    ApplyTitleBarTheme(hwnd);
    RedrawWindow(hwnd, nullptr, nullptr, RDW_ERASE | RDW_INVALIDATE | RDW_ALLCHILDREN);
}

void FindWindowWnd::OnTextChanged() {
    OnFindBarTextChanged(win);
}

LRESULT FindWindowWnd::WndProc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_ENTERSIZEMOVE:
            inSizeMove = true;
            break;
        case WM_SIZE:
            Layout();
            // Pause list redraws only on an actual resize (a WM_SIZE arrived
            // during the interactive size/move loop), to avoid the page-number
            // glitch (#5692). Don't pause for a plain move -- doing so left the
            // results list blank/white while dragging the window (#5737 follow-up).
            if (inSizeMove && results && !listRedrawPaused && wp != SIZE_MINIMIZED) {
                SendMessageW(results->hwnd, WM_SETREDRAW, FALSE, 0);
                listRedrawPaused = true;
            }
            break;
        case WM_EXITSIZEMOVE:
            inSizeMove = false;
            if (results && listRedrawPaused) {
                SendMessageW(results->hwnd, WM_SETREDRAW, TRUE, 0);
                InvalidateRect(results->hwnd, nullptr, TRUE);
                listRedrawPaused = false;
            }
            SavePos();
            break;
        case WM_GETMINMAXINFO: {
            auto mmi = (MINMAXINFO*)lp;
            int pad = DpiScale(h, 8);
            int gap = DpiScale(h, 6);
            int editDy = edit ? edit->GetIdealSize().dy : DpiScale(h, 22);
            int tbH = DpiScale(h, 24);
            int tbW = DpiScale(h, 120);
            if (hwndBtns) {
                SIZE tbSz{};
                SendMessageW(hwndBtns, TB_GETMAXSIZE, 0, (LPARAM)&tbSz);
                tbW = (int)tbSz.cx;
                tbH = (int)tbSz.cy;
            }
            int row2Dy = std::max(editDy, tbH);
            // narrow two-row header: edit, then status+toolbar
            mmi->ptMinTrackSize.x = 2 * pad + std::max(tbW, DpiScale(h, 160));
            mmi->ptMinTrackSize.y = 2 * pad + editDy + gap + row2Dy + pad + DpiScale(h, 48);
            return 0;
        }
        case WM_CLOSE:
            // the caption close button hides the bar instead of destroying it
            HideFindWindow(win);
            return 0;
        case WM_NOTIFY: {
            // the embedded toolbar paints a light button background in dark
            // themes; repaint it with the window's theme background so the icons
            // sit on the same color as the rest of the window
            auto nmh = (NMHDR*)lp;
            if (nmh->hwndFrom == hwndBtns && nmh->code == NM_CUSTOMDRAW) {
                auto cd = (NMTBCUSTOMDRAW*)nmh;
                auto stage = cd->nmcd.dwDrawStage;
                if (stage == CDDS_PREPAINT || stage == CDDS_ITEMPREPAINT) {
                    // reuse the window's cached background brush (rebuilt on theme
                    // change via SetColors) instead of allocating one per paint
                    FillRect(cd->nmcd.hdc, &cd->nmcd.rc, BackgroundBrush());
                    return stage == CDDS_PREPAINT ? CDRF_NOTIFYITEMDRAW : CDRF_DODEFAULT;
                }
            }
            break;
        }
    }
    return WndProcDefault(h, msg, wp, lp);
}

LRESULT FindWindowWnd::OnNotify(int, NMHDR* nmh) {
    if (nmh->code == TTN_GETDISPINFOW) {
        auto di = (NMTTDISPINFOW*)nmh;
        TempStr s = FindWindowButtonTooltip((int)nmh->idFrom);
        if (s) {
            lstrcpynW(di->szText, CWStrTemp(s), dimof(di->szText));
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
        case 'F':
            if (IsCtrlPressed() && !IsAltPressed()) {
                FocusFindEditSelectAll(win);
                return true;
            }
            break;
        case VK_ESCAPE:
            HideFindWindow(win);
            return true;
        case VK_RETURN:
        case VK_F3: {
            // Enter forces a pending debounced search to start now (find the
            // first match) instead of stepping the (stale) results list (#4626)
            if (msg.wParam == VK_RETURN && FindFlushPendingSearch(win)) {
                return true;
            }
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
        case CmdFindToggleMatchWholeWord:
            FindToggleMatchWholeWord(win);
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
    FindWindowSetMatchWholeWordChecked(win, win->findMatchWholeWord);
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
    ClearFindMatches(win);
    AbortFinding(win, true);
    ShowWindow(win->findWindow->hwnd, SW_HIDE);
    HwndSetFocus(win->hwndFrame);
    ScheduleRepaint(win, 0);
}

bool IsFindWindowVisible(MainWindow* win) {
    return win->findWindow && IsWindowVisible(win->findWindow->hwnd);
}

void FindWindowSetStatus(MainWindow* win, Str s) {
    if (win->findWindow && win->findWindow->status) {
        HwndSetText(win->findWindow->status->hwnd, s ? s : StrL(""));
    }
}

void FindWindowSetMatchCaseChecked(MainWindow* win, bool checked) {
    if (win->findWindow && win->findWindow->hwndBtns) {
        SendMessageW(win->findWindow->hwndBtns, TB_CHECKBUTTON, CmdFindToggleMatchCase, MAKELONG(checked ? 1 : 0, 0));
    }
}

void FindWindowSetMatchWholeWordChecked(MainWindow* win, bool checked) {
    if (win->findWindow && win->findWindow->hwndBtns) {
        SendMessageW(win->findWindow->hwndBtns, TB_CHECKBUTTON, CmdFindToggleMatchWholeWord,
                     MAKELONG(checked ? 1 : 0, 0));
    }
}

void FindWindowRefreshResults(MainWindow* win) {
    if (IsFindWindowVisible(win)) {
        win->findWindow->RefreshResults();
    }
}

void UpdateFindWindowTheme(MainWindow* win) {
    if (win->findWindow) {
        win->findWindow->UpdateTheme();
    }
}

TempStr FindResultPageColumnClipResultTemp(int* exitCodeOut) {
    str::Builder out;
    auto fail = [&](Str msg) -> Str {
        out.Append(msg);
        out.AppendChar('\n');
        if (exitCodeOut) {
            *exitCodeOut = 1;
        }
        return ToStrTemp(out);
    };

    if (gWindows.IsEmpty()) {
        return fail("NOTREADY no-window");
    }
    MainWindow* win = gWindows.at(0);
    if (!win || !win->ctrl) {
        return fail("NOTREADY no-doc");
    }
    if (!win->findWindow) {
        win->findWindow = CreateFindWindow(win);
    }
    FindWindowWnd* fw = win->findWindow;
    if (!fw || !fw->results) {
        return fail("ERROR no-find-window");
    }

    ClearFindMatches(win);
    FindMatch fm;
    fm.startPage = 1;
    str::ReplaceWithCopy(&fm.snippet, "longprefix testword suffix");
    win->findMatches.Append(fm);
    fw->filterWords.Reset();
    fw->filterWords.Append("testword");

    HDC hdcScreen = GetDC(nullptr);
    if (!hdcScreen) {
        return fail("ERROR no-screen-dc");
    }
    const int w = 110;
    const int h = DpiScale(fw->hwnd, 20);
    HDC hdcMem = CreateCompatibleDC(hdcScreen);
    HBITMAP hbmp = CreateCompatibleBitmap(hdcScreen, w, h);
    if (!hdcMem || !hbmp) {
        ReleaseDC(nullptr, hdcScreen);
        DeleteDC(hdcMem);
        DeleteObject(hbmp);
        return fail("ERROR no-mem-dc");
    }
    HGDIOBJ oldBmp = SelectObject(hdcMem, hbmp);

    ListBox::DrawItemEvent ev;
    ev.listBox = fw->results;
    ev.hdc = hdcMem;
    ev.itemRect = {0, 0, w, h};
    ev.itemIndex = 0;
    ev.selected = false;
    fw->DrawResultItem(&ev);

    COLORREF px = GetPixel(hdcMem, w - 3, h / 2);
    SelectObject(hdcMem, oldBmp);
    DeleteObject(hbmp);
    DeleteDC(hdcMem);
    ReleaseDC(nullptr, hdcScreen);
    ClearFindMatches(win);

    bool isYellow = GetRValue(px) > 200 && GetGValue(px) > 200 && GetBValue(px) < 100;
    if (isYellow) {
        out.Append(fmt("FAIL pixel=0x%06x\n", (unsigned)px));
        if (exitCodeOut) {
            *exitCodeOut = 1;
        }
        return ToStrTemp(out);
    }
    out.Append(fmt("OK pixel=0x%06x\n", (unsigned)px));
    if (exitCodeOut) {
        *exitCodeOut = 0;
    }
    return ToStrTemp(out);
}
