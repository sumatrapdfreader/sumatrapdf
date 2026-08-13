/* Copyright 2024 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/WinDynCalls.h"
#include "base/UITask.h"
#include "base/Win.h"
#include "base/Dpi.h"

#include "base/Pixmap.h"

#include "wingui/UIModels.h"
#include "wingui/Layout.h"
#include "wingui/WinGui.h"
#include "wingui/PlatformFont.h"
#include "wingui/Gfx.h"
#include "wingui/VirtCtrl.h"

#include "Settings.h"
#include "GlobalPrefs.h"
#include "AppSettings.h"
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

// list index of the match starting at (page, glyph), or -1 if there is none
static int FindMatchIndex(MainWindow* win, int page, int glyph) {
    int n = len(win->findMatches);
    for (int i = 0; i < n; i++) {
        const FindMatch& fm = win->findMatches[i];
        if (fm.startPage == page && fm.startGlyph == glyph) {
            return i;
        }
    }
    return -1;
}

struct FindWindowWnd : WindowBase {
    MainWindow* win = nullptr;
    Edit* edit = nullptr;
    // the status text, the buttons and the results list are virtual controls;
    // the search field is the only HWND child
    VirtText* status = nullptr;
    // prev / next / match-case / match-whole-word / unpin(dock)
    VirtIconButton* btns[5]{};
    Tooltip* tooltip = nullptr;
    VirtListBox* results = nullptr;
    StrVec filterWords; // search term(s) to highlight in snippets
    Vec<u8> hlScratch;  // reused highlight mask for DrawMaybeHighlightedText
    // coalesce rapid list selections: only the latest deferred navigation runs
    AtomicInt pendingNavEpoch = 0;
    // the match the list was selected on, saved before win->findMatches is
    // rebuilt. The list is sorted by page while the scan wraps around, so a
    // later batch inserts rows *above* the selection; restoring by match
    // identity keeps the selection on the same result instead of the same row.
    // One-shot: RefreshResults consumes and clears it. <= 0: nothing saved
    int savedSelPage = -1;
    int savedSelGlyph = -1;
    // in an interactive size/move loop (between WM_ENTERSIZEMOVE/EXITSIZEMOVE)
    bool inSizeMove = false;
    // list redraw is paused only while interactively *resizing* (a WM_SIZE
    // arrived during the size/move loop), not while merely moving the window
    bool listRedrawPaused = false;

    FindWindowWnd() = default;
    ~FindWindowWnd() override;

    bool Create(MainWindow* win);
    void CreateButtons();
    void UpdateButtonIcons();
    // the button under `pt` (window coords), or -1
    int ButtonIndexFromPoint(Point pt);
    void Layout();
    void SavePos();
    void RefreshResults(bool allowNavigation = true);
    void UpdateTheme();

    void OnTextChanged();
    void DrawResultItem(VirtListBox::DrawItemEvent* ev);
    void OnResultSelected();
    void SaveSelectedMatch();
    bool MoveResultSelection(WPARAM vkey);
    int CurrentMatchIndex();         // list index of the document's current match, or -1
    int FirstMatchFromCurrentPage(); // list index of the first match at/after the current page

    void OnSize(WindowBase::SizeEvent* ev);
    void OnGetMinMaxInfo(WindowBase::GetMinMaxInfoEvent* ev);
    void OnClose(WindowBase::CloseEvent* ev);
    void OnSetCursor(WindowBase::SetCursorEvent* ev);
    void OnKeyDown(KeyEvent* ev);
    void OnCommand(WindowBase::CommandEvent* ev);
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
    delete tooltip;
    delete status;
    delete results; // also deletes its FindResultsModel
    for (VirtIconButton* b : btns) {
        delete b;
    }
}

// the pixmaps belong to the icon cache, which re-renders them for the current
// theme and size
void FindWindowWnd::UpdateButtonIcons() {
    static const TbIcon icons[5] = {TbIcon::ChevronUp, TbIcon::ChevronDown, TbIcon::MatchCase, TbIcon::MatchWholeWord,
                                    TbIcon::ArrowsDiagonalMinimize};
    int isz = RoundUp(DpiScale(hwnd, 16), 4);
    for (int i = 0; i < 5; i++) {
        if (btns[i]) {
            btns[i]->pixmap = GetPixmapForIcon(icons[i], isz, isz);
        }
    }
}

static void FindWindowButtonClicked(FindWindowWnd* w, VirtMouseEvent* ev) {
    auto* btn = (VirtIconButton*)ev->target;
    WindowBase::CommandEvent ce;
    ce.w = w;
    ce.wparam = (WPARAM)btn->id;
    w->OnCommand(&ce);
}

void FindWindowWnd::CreateButtons() {
    static const int cmds[5] = {CmdFindPrev, CmdFindNext, CmdFindToggleMatchCase, CmdFindToggleMatchWholeWord,
                                kFindWinPinCmdId};
    COLORREF colBg = ThemeWindowControlBackgroundColor();
    int pad = DpiScale(hwnd, 4);
    for (int i = 0; i < 5; i++) {
        auto* b = new VirtIconButton();
        b->id = cmds[i];
        b->padding = Insets{pad, pad, pad, pad};
        b->bgColorHover = AccentColor(colBg, 20);
        b->bgColorSelected = AccentColor(colBg, 36);
        b->SetTooltip(FindWindowButtonTooltip(cmds[i]));
        b->onClick = MkFunc1(FindWindowButtonClicked, this);
        btns[i] = b;
    }
    UpdateButtonIcons();
}

int FindWindowWnd::ButtonIndexFromPoint(Point pt) {
    for (int i = 0; i < 5; i++) {
        if (btns[i] && btns[i]->BoundsInWindow().Contains(pt)) {
            return i;
        }
    }
    return -1;
}

bool FindWindowWnd::Create(MainWindow* mainWin) {
    win = mainWin;

    auto colBg = ThemeWindowControlBackgroundColor();
    auto colTxt = ThemeWindowTextColor();

    {
        CreateCustomArgs args;
        args.visible = false;
        args.title = _TRA("Find");
        // WS_CLIPCHILDREN neutralizes CS_PARENTDC of the standard controls
        // (their DCs get clipped to the control, not to this window), so e.g.
        // the results listbox can't paint its partially visible bottom row
        // below itself onto this window
        args.style = WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_CLIPCHILDREN;
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

    PlatformFont* platformFont = GetPlatformFont(GetAppFont(hwnd));

    status = NewVirtText({
        .font = platformFont,
        .textColor = colTxt,
        .isRtl = IsUIRtl(),
        // single line, vertically centered (what SS_CENTERIMAGE used to do)
        .ellipsis = true,
    });

    CreateButtons();

    {
        auto* c = new VirtListBox();
        c->dpi = GetDpi();
        c->font = platformFont;
        c->textColor = colTxt;
        c->bgColor = colBg;
        c->onDrawItem = MkMethod1<FindWindowWnd, VirtListBox::DrawItemEvent*, &FindWindowWnd::DrawResultItem>(this);
        c->onSelectionChanged = MkMethod0<FindWindowWnd, &FindWindowWnd::OnResultSelected>(this);
        c->onDoubleClick = MkMethod0<FindWindowWnd, &FindWindowWnd::OnResultSelected>(this);
        c->SetModel(new FindResultsModel(win));
        results = c;
    }

    {
        Tooltip::CreateArgs args;
        args.parent = hwnd;
        tooltip = new Tooltip();
        tooltip->Create(args);
    }

    // the virtual controls of this window; it positions them itself in Layout()
    vroot = new VirtRoot(hwnd);
    Vec<VirtCtrl*> tops;
    tops.Append(status);
    for (VirtIconButton* b : btns) {
        tops.Append(b);
    }
    tops.Append(results);
    vroot->SetTops(tops);

    ApplyDarkModeToPopupWindow(hwnd);
    return true;
}

void FindWindowWnd::Layout() {
    // a WS_CAPTION/WS_THICKFRAME window gets WM_SIZE during CreateCustom, before
    // the child controls exist; ignore layout until they're created
    if (!edit || !status || !btns[0] || !results) {
        return;
    }
    Rect rc = HwndClientRect(hwnd);
    if (vroot) {
        vroot->SetBounds({0, 0, rc.dx, rc.dy});
    }
    int pad = DpiScale(hwnd, 8);
    int gap = DpiScale(hwnd, 6);
    int statusDx = DpiScale(hwnd, 90);
    int minEditDx = DpiScale(hwnd, 48);

    int editDy = edit->GetIdealSize().dy;
    Size btnSz = btns[0]->GetIdealSize();
    btnSz.dx += btns[0]->padding.left + btns[0]->padding.right;
    btnSz.dy += btns[0]->padding.top + btns[0]->padding.bottom;
    int tbW = btnSz.dx * 5;
    int tbH = btnSz.dy;
    // lays the five buttons out left to right inside the strip they were given
    auto layoutBtns = [&](int x, int y) {
        for (VirtIconButton* b : btns) {
            b->SetBounds({x, y, btnSz.dx, btnSz.dy});
            x += btnSz.dx;
        }
    };

    int contentDx = std::max(0, rc.dx - (2 * pad));
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
        layoutBtns(tbX, y + ((headerDy - tbH) / 2));
        status->SetBounds({statusX, y + ((headerDy - editDy) / 2), statusDx, editDy});
        MoveWindow(edit->hwnd, pad, y + ((headerDy - editDy) / 2), editDx, editDy, TRUE);
    } else {
        // narrow: full-width edit, then [n/m][toolbar] (issue #5692)
        MoveWindow(edit->hwnd, pad, y, contentDx, editDy, TRUE);
        y += editDy + gap;
        headerDy = editDy + gap + std::max(editDy, tbH);
        int row2Dy = std::max(editDy, tbH);
        int statusW = std::max(0, contentDx - gap - tbW);
        status->SetBounds({pad, y + ((row2Dy - editDy) / 2), statusW, editDy});
        int tbX = pad + contentDx - tbW;
        layoutBtns(tbX, y + ((row2Dy - tbH) / 2));
    }

    // the results list fills the rest of the window below the header
    int listTop = pad + headerDy + pad;
    int listDy = std::max(0, rc.dy - listTop - pad);
    results->SetBounds({pad, listTop, contentDx, listDy});

    // Erase margins (and any area the list just vacated when shrinking) so
    // snippet/page-number pixels don't ghost at the bottom/side of the window
    // when the dialog is resized narrower than the previous text (#5796).
    HwndInvalidate(hwnd, true);
}

void FindWindowWnd::RefreshResults(bool allowNavigation) {
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
    results->SetModel(results->model); // the model is live; re-read it
    // keep a result selected so it's visible as you type and Next/Prev have a
    // sensible starting point.
    int sel = -1;
    if (savedSelPage > 0) {
        // the list was re-sorted (or grew at the front) under an existing
        // selection: stay on that match, not on that row number
        sel = FindMatchIndex(win, savedSelPage, savedSelGlyph);
        savedSelPage = -1;
        savedSelGlyph = -1;
    }
    if (sel < 0) {
        sel = CurrentMatchIndex();
    }
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
        // streamed partial updates must not navigate: OnResultSelected joins
        // the in-flight count worker (GoToFindMatch), which would cancel the
        // very scan that's producing these results
        if (allowNavigation) {
            OnResultSelected();
        }
    }
}

void FindWindowWnd::DrawResultItem(VirtListBox::DrawItemEvent* ev) {
    VirtListBox* lb = ev->listBox;
    if (ev->itemIndex < 0 || ev->itemIndex >= len(win->findMatches)) {
        return;
    }
    Gfx* gfx = ev->gfx;
    HWND hwndList = lb->GetHwnd();
    Rect rc = ev->itemRect;

    // clip the whole row so a partially visible last item (LBS_NOINTEGRALHEIGHT)
    // and highlight fill cannot paint outside the item / list client (#5796)
    gfx->PushClip(rc);

    COLORREF colBg = IsSpecialColor(lb->bgColor) ? GetSysColor(COLOR_WINDOW) : lb->bgColor;
    COLORREF colText = IsSpecialColor(lb->textColor) ? GetSysColor(COLOR_WINDOWTEXT) : lb->textColor;

    if (ev->selected) {
        colBg = AccentColor(colBg, 30);
    }
    gfx->FillRect(rc, colBg);

    int pad = DpiScale(hwndList, 6);
    Rect rcText = rc;
    rcText.x += pad;
    rcText.dx -= 2 * pad;

    // Fixed-width page column (room for multi-digit labels) so the right edge
    // stays stable while the window is resized; long snippets ellipsize into it
    // instead of fighting a per-row measured width (#5692 / #5796).
    const FindMatch& fm = win->findMatches[ev->itemIndex];
    TempStr pageStr = fmt("%s", win->ctrl->GetPageLabeTemp(fm.startPage));
    int pageGap = DpiScale(hwndList, 10);
    int pageColDx = DpiScale(hwndList, 40);
    Size pageSize = gfx->MeasureText(pageStr, lb->font);
    pageColDx = std::max(pageSize.dx + DpiScale(hwndList, 4), pageColDx);
    Rect rcPage = rcText;
    rcPage.x = std::max(rcText.x, rcText.x + rcText.dx - pageColDx);
    rcPage.dx = rcText.x + rcText.dx - rcPage.x;

    // snippet on the left, with the matched term highlighted
    Rect rcSnippet = rcText;
    rcSnippet.dx = std::max(0, rcPage.x - pageGap - rcSnippet.x);
    if (rcSnippet.dx > 0) {
        u32 drawFmt = gfxTextEllipsis | gfxTextVCenter | gfxTextLeft;
        // clip snippet drawing so match highlights cannot bleed into the page
        // number column when the floating window is narrow (issue #5736); it
        // nests, so the outer row clip stays in effect afterwards
        gfx->PushClip(rcSnippet);
        DrawMaybeHighlightedText(gfx, rcSnippet, fm.snippet, filterWords, hlScratch, colBg, false,
                                 win->findMatchWholeWord, drawFmt, lb->font, colText);
        gfx->PopClip();
    }

    // repaint the page column on top in case a prior draw left stray pixels
    gfx->FillRect(rcPage, colBg);
    u32 pageFmt = gfxTextEllipsis | gfxTextVCenter | gfxTextRight;
    gfx->DrawText(pageStr, rcPage, pageFmt, lb->font, AccentColor(colText, 80));

    gfx->PopClip();
}

void FindWindowWnd::OnResultSelected() {
    int idx = results ? results->GetCurrentSelection() : -1;
    if (idx < 0 || idx >= len(win->findMatches)) {
        return;
    }
    const FindMatch& fm = win->findMatches[idx];
    if (win->ctrl && win->ctrl->CanFindInPage() && idx == win->browserFindCurrent) {
        return; // already on this match
    }
    DisplayModel* dm = win->AsFixed();
    if (dm && dm->textSearch && dm->textSearch->startPage == fm.startPage &&
        dm->textSearch->startGlyph == fm.startGlyph) {
        return; // already on this match
    }
    // defer document navigation so the results list can scroll/repaint first
    // (issue #5692). Coalesce rapid F3 / arrow presses to the latest selection.
    auto* data = new DeferredGoToFindMatchData;
    data->win = win;
    data->findWindow = this;
    data->startPage = fm.startPage;
    data->startGlyph = fm.startGlyph;
    data->endPage = fm.endPage;
    data->endGlyph = fm.endGlyph;
    data->epoch = AtomicIntInc(&pendingNavEpoch);
    uitask::Post(MkFunc0<DeferredGoToFindMatchData>(DeferredGoToFindMatch, data), "GoToFindMatch");
}

// remember which match the list is on, by identity rather than by row, so the
// next RefreshResults can restore it after the list is re-sorted or grows at
// the front. Called before win->findMatches is rebuilt
void FindWindowWnd::SaveSelectedMatch() {
    savedSelPage = -1;
    savedSelGlyph = -1;
    int idx = results ? results->GetCurrentSelection() : -1;
    if (idx < 0 || idx >= len(win->findMatches)) {
        return;
    }
    const FindMatch& fm = win->findMatches[idx];
    savedSelPage = fm.startPage;
    savedSelGlyph = fm.startGlyph;
}

// list index of the match the document is currently on (so the selection can
// track the current match), or -1 if it isn't in the list
int FindWindowWnd::CurrentMatchIndex() {
    if (win->ctrl && win->ctrl->CanFindInPage()) {
        // tracked by the browser (chm / markdown) webview find (see
        // SearchAndDDE.cpp BrowserFind*)
        return win->browserFindCurrent;
    }
    DisplayModel* dm = win->AsFixed();
    if (!dm || !dm->textSearch) {
        return -1;
    }
    return FindMatchIndex(win, dm->textSearch->startPage, dm->textSearch->startGlyph);
}

// first match at/after the current page, wrapping to the start of the document
// if there is none: the match with the smallest forward page distance from the
// current page (the list itself is in document order)
int FindWindowWnd::FirstMatchFromCurrentPage() {
    int n = len(win->findMatches);
    if (n == 0) {
        return -1;
    }
    int curPage = win->ctrl ? win->ctrl->CurrentPageNo() : 1;
    int nPages = win->ctrl ? win->ctrl->PageCount() : 1;
    int best = 0;
    int bestDist = INT_MAX;
    for (int i = 0; i < n; i++) {
        int dist = win->findMatches[i].startPage - curPage;
        if (dist < 0) {
            dist += nPages;
        }
        if (dist < bestDist) {
            bestDist = dist;
            best = i;
            if (dist == 0) {
                break; // first match on the current page
            }
        }
    }
    return best;
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
        case VK_HOME:
            idx = 0;
            break;
        case VK_END:
            idx = n - 1;
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
                idx = std::max(idx, 0);
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
    if (!HwndIsVisible(hwnd)) {
        return;
    }
    Rect r = HwndWindowRect(hwnd);
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
    if (results) {
        results->textColor = colTxt;
        results->bgColor = colBg;
    }
    if (status) {
        status->textColor = colTxt;
    }
    for (VirtIconButton* b : btns) {
        if (b) {
            b->bgColorHover = AccentColor(colBg, 20);
            b->bgColorSelected = AccentColor(colBg, 36);
        }
    }
    // the icons are drawn in the theme's text color, so re-render them
    UpdateButtonIcons();
    ApplyTitleBarTheme(hwnd);
    RedrawWindow(hwnd, nullptr, nullptr, RDW_ERASE | RDW_INVALIDATE | RDW_ALLCHILDREN);
}

void FindWindowWnd::OnTextChanged() {
    OnFindBarTextChanged(win);
}

void FindWindowWnd::OnSize(WindowBase::SizeEvent* ev) {
    if (ev->msg == WM_ENTERSIZEMOVE) {
        inSizeMove = true;
        return;
    }
    if (ev->msg == WM_SIZE) {
        Layout();
        return;
    }
    if (ev->msg == WM_EXITSIZEMOVE) {
        inSizeMove = false;
        HwndInvalidate(hwnd, true);
        SavePos();
    }
}

void FindWindowWnd::OnGetMinMaxInfo(WindowBase::GetMinMaxInfoEvent* ev) {
    auto* mmi = ev->mmi;
    int pad = DpiScale(hwnd, 8);
    int gap = DpiScale(hwnd, 6);
    int editDy = edit ? edit->GetIdealSize().dy : DpiScale(hwnd, 22);
    int tbH = DpiScale(hwnd, 24);
    int tbW = DpiScale(hwnd, 120);
    if (btns[0]) {
        Size btnSz = btns[0]->GetIdealSize();
        tbW = (btnSz.dx + btns[0]->padding.left + btns[0]->padding.right) * 5;
        tbH = btnSz.dy + btns[0]->padding.top + btns[0]->padding.bottom;
    }
    int row2Dy = std::max(editDy, tbH);
    // narrow two-row header: edit, then status+toolbar
    mmi->ptMinTrackSize.x = (2 * pad) + std::max(tbW, DpiScale(hwnd, 160));
    mmi->ptMinTrackSize.y = (2 * pad) + editDy + gap + row2Dy + pad + DpiScale(hwnd, 48);
}

void FindWindowWnd::OnClose(WindowBase::CloseEvent* ev) {
    // the caption close button hides the bar instead of destroying it
    HideFindWindow(win);
    // WmEvent.didHandle defaults true -> skip WindowBase::Destroy()
}

// virtual-control tooltips
void FindWindowWnd::OnSetCursor(WindowBase::SetCursorEvent*) {
    Point pt = HwndGetCursorPos(hwnd);
    int idx = ButtonIndexFromPoint(pt);
    if (idx >= 0 && tooltip) {
        TempStr tip = btns[idx]->GetTooltipTemp({});
        if (tip) {
            tooltip->SetSingle(tip, btns[idx]->BoundsInWindow(), false);
        }
    } else if (tooltip) {
        tooltip->Delete();
    }
}

void FindWindowWnd::OnKeyDown(KeyEvent* ev) {
    switch (ev->vkey) {
        case 'F':
            if (ev->isCtrl && !ev->isAlt) {
                FocusFindEditSelectAll(win);
                ev->didHandle = true;
            }
            break;
        case VK_ESCAPE:
            HideFindWindow(win);
            ev->didHandle = true;
            break;
        case VK_RETURN:
        case VK_F3: {
            // Enter forces a pending debounced search to start now (find the
            // first match) instead of stepping the (stale) results list (#4626)
            if (ev->vkey == VK_RETURN && FindFlushPendingSearch(win)) {
                ev->didHandle = true;
                break;
            }
            // step through the results list; fall back to a document search when
            // there's no list (e.g. count not ready)
            WPARAM dir = ev->isShift ? VK_UP : VK_DOWN;
            if (!MoveResultSelection(dir)) {
                ev->isShift ? FindPrev(win) : FindNext(win);
            }
            ev->didHandle = true;
            break;
        }
        case VK_DOWN:
        case VK_UP:
        case VK_NEXT:
        case VK_PRIOR:
            // walk the results list from the search edit
            ev->didHandle = MoveResultSelection(ev->vkey);
            break;
        case VK_HOME:
        case VK_END: {
            // Ctrl+Home / Ctrl+End: always jump to first/last result (#5797)
            if (ev->isCtrl) {
                ev->didHandle = MoveResultSelection(ev->vkey);
                break;
            }
            // Home / End: if the caret is already at the start/end of the search
            // text, move the results list; otherwise let the Edit control move
            // the caret (same idea as the two-press pattern in the request).
            if (!edit || ev->hwnd != edit->hwnd) {
                // focus is on the list itself: Home/End jump first/last
                ev->didHandle = MoveResultSelection(ev->vkey);
                break;
            }
            DWORD selStart = 0, selEnd = 0;
            SendMessageW(edit->hwnd, EM_GETSEL, (WPARAM)&selStart, (LPARAM)&selEnd);
            int textLen = Edit_GetTextLength(edit->hwnd);
            bool toEnd = (ev->vkey == VK_END);
            bool caretAtBound = (selStart == selEnd) && (toEnd ? (int)selEnd == textLen : (int)selStart == 0);
            if (caretAtBound) {
                ev->didHandle = MoveResultSelection(ev->vkey);
            }
            // else leave didHandle false: Edit moves the caret
            break;
        }
    }
}

void FindWindowWnd::OnCommand(WindowBase::CommandEvent* ev) {
    int cmd = LOWORD(ev->wparam);
    switch (cmd) {
        case CmdFindPrev:
            if (!MoveResultSelection(VK_UP)) {
                FindPrev(win);
            }
            break;
        case CmdFindNext:
            if (!MoveResultSelection(VK_DOWN)) {
                FindNext(win);
            }
            break;
        case CmdFindToggleMatchCase:
            FindToggleMatchCase(win);
            break;
        case CmdFindToggleMatchWholeWord:
            FindToggleMatchWholeWord(win);
            break;
        case kFindWinPinCmdId:
            ToggleFloatingFindUI(win); // dock back to the compact toolbar bar
            break;
        default:
            return;
    }
    ev->didHandle = true;
}

//--- public API

// The floating, movable/resizable variant of the find UI (see SearchUIFloating).
// Phase 1: search controls only; a results list is added in a later phase.
FindWindowWnd* CreateFindWindow(MainWindow* win) {
    auto* w = new FindWindowWnd();
    w->onCommand = MkMethod1<FindWindowWnd, WindowBase::CommandEvent*, &FindWindowWnd::OnCommand>(w);
    w->onSize = MkMethod1<FindWindowWnd, WindowBase::SizeEvent*, &FindWindowWnd::OnSize>(w);
    w->onGetMinMaxInfo = MkMethod1<FindWindowWnd, WindowBase::GetMinMaxInfoEvent*, &FindWindowWnd::OnGetMinMaxInfo>(w);
    w->onClose = MkMethod1<FindWindowWnd, WindowBase::CloseEvent*, &FindWindowWnd::OnClose>(w);
    w->onSetCursor = MkMethod1<FindWindowWnd, WindowBase::SetCursorEvent*, &FindWindowWnd::OnSetCursor>(w);
    w->onKeyDown = MkMethod1<FindWindowWnd, KeyEvent*, &FindWindowWnd::OnKeyDown>(w);
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
        Rect fr = HwndWindowRect(win->hwndFrame);
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
    // Cancel any deferred GoToFindMatch so it cannot run after the document/tab
    // that owned these matches is gone (issue #5807).
    AtomicIntInc(&win->findWindow->pendingNavEpoch);
    ClearFindMatches(win);
    // drop the active TextSearch hit so closing find clears the highlight;
    // F3 still works (FindNext re-searches) and paints the new hit (#5802)
    if (DisplayModel* dm = win->AsFixed()) {
        if (dm->textSearch) {
            dm->textSearch->Reset();
        }
    }
    AbortFinding(win, true);
    ShowWindow(win->findWindow->hwnd, SW_HIDE);
    HwndSetFocus(win->hwndFrame);
    ScheduleRepaint(win, 0);
}

bool IsFindWindowVisible(MainWindow* win) {
    return win->findWindow && HwndIsVisible(win->findWindow->hwnd);
}

void FindWindowSetStatus(MainWindow* win, Str s) {
    if (win->findWindow && win->findWindow->status) {
        win->findWindow->status->SetText(s ? s : StrL(""));
        win->findWindow->status->Invalidate();
    }
}

// idx into FindWindowWnd::btns
constexpr int kBtnMatchCase = 2;
constexpr int kBtnMatchWholeWord = 3;

static void FindWindowSetBtnChecked(MainWindow* win, int idx, bool checked) {
    if (!win->findWindow) {
        return;
    }
    VirtIconButton* b = win->findWindow->btns[idx];
    if (!b || b->isSelected == checked) {
        return;
    }
    b->isSelected = checked;
    b->Invalidate();
}

void FindWindowSetMatchCaseChecked(MainWindow* win, bool checked) {
    FindWindowSetBtnChecked(win, kBtnMatchCase, checked);
}

void FindWindowSetMatchWholeWordChecked(MainWindow* win, bool checked) {
    FindWindowSetBtnChecked(win, kBtnMatchWholeWord, checked);
}

// repopulate the results list from win->findMatches (no-op if not visible).
// allowNavigation=false for streamed partial updates: don't navigate the
// document (navigation would cancel the in-flight count scan)
void FindWindowRefreshResults(MainWindow* win, bool allowNavigation) {
    if (IsFindWindowVisible(win)) {
        win->findWindow->RefreshResults(allowNavigation);
    }
}

// remember the selected result by match identity (page + glyph) so the next
// FindWindowRefreshResults can restore it even though the list was re-sorted
// or grew at the front. Call before changing win->findMatches
void FindWindowSaveSelectedMatch(MainWindow* win) {
    if (IsFindWindowVisible(win)) {
        win->findWindow->SaveSelectedMatch();
    }
}

// re-apply theme colors/icons to the floating window after a theme change
void UpdateFindWindowTheme(MainWindow* win) {
    if (win->findWindow) {
        win->findWindow->UpdateTheme();
    }
}

// term the pending / finished FindResultsOrderResultTemp scan was started for
static Str gFindOrderTerm;

// Report the order of the floating results list, to verify results are always
// listed in document order. The count scan starts at the page that is current
// when it begins and wraps around, so it finds matches out of order (e.g. 89,
// 104, 47 when starting on page 89) -- they get re-sorted before being
// installed. The scan is async, so the first call starts it and every call
// reports NOTREADY until it finishes; the test polls.
// Test hook: run a search from startPage and report the results list order.
TempStr FindResultsOrderResultTemp(Str term, int startPage, int* exitCodeOut) {
    str::Builder out;
    auto fail = [&](Str msg) -> Str {
        out.Append(msg);
        out.AppendChar('\n');
        if (exitCodeOut) {
            *exitCodeOut = 1;
        }
        return ToStrTemp(out);
    };

    if (str::IsEmptyOrWhiteSpace(term)) {
        return fail("ERROR missing term");
    }
    if (len(gWindows) == 0) {
        return fail("NOTREADY no-window");
    }
    MainWindow* win = gWindows[0];
    if (!win || !win->AsFixed()) {
        return fail("NOTREADY no-doc");
    }
    if (!str::Eq(gFindOrderTerm, term)) {
        str::ReplaceWithCopy(&gFindOrderTerm, term);
        // start the search the way find-as-you-type does, from `startPage`
        gGlobalPrefs->searchUIFloating = true;
        if (startPage > 0) {
            win->ctrl->GoToPage(startPage, false);
        }
        ShowFindWindow(win);
        HwndSetText(win->hwndFindEdit, term);
        OnFindBarTextChanged(win);
        FindFlushPendingSearch(win); // run it now instead of waiting out the debounce
        return fail("NOTREADY scan-started");
    }
    if (!win->findCountValid) {
        return fail("NOTREADY scanning");
    }
    FindWindowWnd* fw = win->findWindow;
    if (!fw || !fw->results) {
        return fail("ERROR no-find-window");
    }

    int n = len(win->findMatches);
    out.Append(fmt("OK n=%d sel=%d pages=", n, fw->results->GetCurrentSelection()));
    for (int i = 0; i < n; i++) {
        if (i > 0) {
            out.AppendChar(',');
        }
        out.Append(fmt("%d", win->findMatches[i].startPage));
    }
    out.AppendChar('\n');
    if (exitCodeOut) {
        *exitCodeOut = 0;
    }
    return ToStrTemp(out);
}

// Headless draw test for issue #5736: match highlights must not bleed into the page column.
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

    if (len(gWindows) == 0) {
        return fail("NOTREADY no-window");
    }
    MainWindow* win = gWindows[0];
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
        if (hdcMem) {
            DeleteDC(hdcMem);
        }
        if (hbmp) {
            DeleteObject(hbmp);
        }
        ReleaseDC(nullptr, hdcScreen);
        return fail("ERROR no-mem-dc");
    }
    HGDIOBJ oldBmp = SelectObject(hdcMem, hbmp);

    GfxHdc gfx(hdcMem);
    VirtListBox::DrawItemEvent ev;
    ev.listBox = fw->results;
    ev.gfx = &gfx;
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
