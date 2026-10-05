/* Copyright 2024 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// ng: orig's FindWindow.cpp - the floating variant of the find UI, chosen with
// SearchUIFloating (the pin button switches between the two). orig makes it an
// owned WS_POPUP | WS_CAPTION | WS_THICKFRAME tool window with a DropDown, a
// pages Edit and a VirtListBox; here it is one gpui card inside the frame,
// dragged by its header and remembered in gSettings->searchUIWindowPos. Same
// contents and order: find box, "n / m" status, previous, next, match case,
// match whole word, dock, close, the "Limit to pages 1-N:" row and the
// snippet results list with the matched term highlighted and the page label in
// a fixed right column.

#include "gui/GpuiBridge.h"
#include "base/UITask.h"
#if OS_WIN
#include "base/Win.h"
#endif

#include "gui/Dpi.h"
#include "gui/UIModels.h"

#include "Settings.h"
#include "AppSettings.h"
#include "DisplayMode.h"
#include "DocController.h"
#include "EngineBase.h"
#include "TextSelection.h"
#include "ProgressUpdateUI.h"
#include "TextSearch.h"
#include "DisplayModel.h"
#include "Commands.h"
#include "VirtKeys.h"
#include "ShortcutParse.h"
#include "Accelerators.h"
#include "FilterUtil.h"
#include "FilterHighlightDraw.h"
#include "Translations.h"
#include "Theme.h"
#include "SumatraPDF.h"
#include "MainWindow.h"
#include "WindowTab.h"
#include "gui/AppShell.h"
#include "gui/DialogWidgets.h"
#include "gui/ToolWindow.h"
#include "SearchAndDDE.h"
#include "SvgIcons.h"
#include "FindBar.h"
#include "FindWindow.h"

#include "SumatraLog.h"

// command ids for the window's toolbar buttons
constexpr int kFindWinPinCmdId = (int)CmdLast + 51;
constexpr int kFindWinCloseCmdId = (int)CmdLast + 52;

constexpr int kFindWinPadding = 8;
constexpr int kFindWinGap = 6;
constexpr int kFindWinDefaultDx = 460;
constexpr int kFindWinDefaultDy = 320;
constexpr int kFindWinMinDx = 260;
constexpr int kFindWinMinDy = 140;
constexpr int kFindWinPagesDx = 160;
constexpr int kFindWinRowDy = 20;
constexpr int kFindWinPageColDx = 40;

struct FindWindowView;

struct FindWindowUI {
    gp::Entity<FindWindowView> view;
    gp::Bounds listView;
    // orig's edit is a combo box: the list under it is the search history
    DialogSelect ddHistory;
    int historyLen = -1;
    Str historyFirst; // owned
};

FindWindowWnd::~FindWindowWnd() {
    str::Free(status);
    if (ui) {
        ui->ddHistory.Free();
        str::Free(ui->historyFirst);
    }
    delete ui;
}

static void FindWindowOpenToolWindow(MainWindow* win);

// ng: gp::Notify() wakes the view the listener belongs to, not the window
// that draws the find window (the frame, or a window of its own)
static void FindNotify(MainWindow* win, gp::Ctx* cx) {
    gp::Notify(cx);
    if (IsMainWindowValid(win)) {
        AppShellInvalidate(win);
    }
}

static FindWindowWnd* Wnd(MainWindow* win) {
    return win ? win->findWindow : nullptr;
}

// --- results ----------------------------------------------------------------

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

// list index of the match the document is currently on (so the selection can
// track the current match), or -1 if it isn't in the list
static int CurrentMatchIndex(MainWindow* win) {
    if (win->ctrl && win->ctrl->CanFindInPage()) {
        // tracked by the browser (chm / markdown) webview find
        return win->browserFindCurrent;
    }
    DisplayModel* dm = win->AsFixed();
    if (!dm || !dm->textSearch || dm->textSearch->result.len == 0) {
        return -1;
    }
    return FindMatchIndex(win, dm->textSearch->startPage, dm->textSearch->startGlyph);
}

// first match at/after the current page, wrapping to the start of the document
// if there is none: the match with the smallest forward page distance from the
// current page (the list itself is in document order)
static int FirstMatchFromCurrentPage(MainWindow* win) {
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

// ng: orig defers the navigation through uitask so the list can repaint first
// and coalesces rapid presses with an epoch; here the selection is drawn from
// the same frame's state, so the post is only needed to leave the event
struct DeferredGoToFindMatchData {
    MainWindow* win = nullptr;
    int startPage = 0;
    int startGlyph = 0;
    int endPage = 0;
    int endGlyph = 0;
};

static void DeferredGoToFindMatch(DeferredGoToFindMatchData* d) {
    MainWindow* win = d->win;
    if (IsMainWindowValidAndNotClosing(win) && IsFindWindowVisible(win)) {
        GoToFindMatch(win, d->startPage, d->startGlyph, d->endPage, d->endGlyph);
    }
    delete d;
}

static void OnResultSelected(MainWindow* win) {
    FindWindowWnd* w = Wnd(win);
    int idx = w ? w->sel : -1;
    if (idx < 0 || idx >= len(win->findMatches)) {
        return;
    }
    const FindMatch& fm = win->findMatches[idx];
    if (win->ctrl && win->ctrl->CanFindInPage() && idx == win->browserFindCurrent) {
        return; // already on this match
    }
    DisplayModel* dm = win->AsFixed();
    if (dm && dm->textSearch && dm->textSearch->result.len > 0 && dm->textSearch->startPage == fm.startPage &&
        dm->textSearch->startGlyph == fm.startGlyph) {
        return; // already on this match
    }
    auto* data = new DeferredGoToFindMatchData;
    data->win = win;
    data->startPage = fm.startPage;
    data->startGlyph = fm.startGlyph;
    data->endPage = fm.endPage;
    data->endGlyph = fm.endGlyph;
    uitask::Post(MkFunc0<DeferredGoToFindMatchData>(DeferredGoToFindMatch, data), "GoToFindMatch");
}

// remember which match the list is on, by identity rather than by row, so the
// next RefreshResults can restore it after the list is re-sorted or grows at
// the front. Called before win->findMatches is rebuilt
void FindWindowSaveSelectedMatch(MainWindow* win) {
    FindWindowWnd* w = Wnd(win);
    if (!w || !w->visible) {
        return;
    }
    w->savedSelPage = -1;
    w->savedSelGlyph = -1;
    int idx = w->sel;
    if (idx < 0 || idx >= len(win->findMatches)) {
        return;
    }
    const FindMatch& fm = win->findMatches[idx];
    w->savedSelPage = fm.startPage;
    w->savedSelGlyph = fm.startGlyph;
}

void FindWindowRefreshResults(MainWindow* win, bool allowNavigation) {
    FindWindowWnd* w = Wnd(win);
    if (!w || !w->visible) {
        return;
    }
    // keep a result selected so it's visible as you type and Next/Prev have a
    // sensible starting point.
    int sel = -1;
    if (w->savedSelPage > 0) {
        // the list was re-sorted (or grew at the front) under an existing
        // selection: stay on that match, not on that row number
        sel = FindMatchIndex(win, w->savedSelPage, w->savedSelGlyph);
        w->savedSelPage = -1;
        w->savedSelGlyph = -1;
    }
    if (sel < 0) {
        sel = CurrentMatchIndex(win);
    }
    if (sel >= 0) {
        // the document already sits on a match (find-as-you-type found it): just
        // mirror it in the list, no navigation
        w->sel = sel;
    } else if (len(win->findMatches) > 0) {
        // find-as-you-type gave up (it self-cancels for matches on far pages),
        // so the document isn't on a match. Drive selection + navigation off the
        // full count instead: go to the first match at/after the current page.
        w->sel = FirstMatchFromCurrentPage(win);
        // streamed partial updates must not navigate: OnResultSelected joins
        // the in-flight count worker, which would cancel the very scan that's
        // producing these results
        if (allowNavigation) {
            OnResultSelected(win);
        }
    } else {
        w->sel = -1;
    }
    AppShellInvalidate(win);
}

// move the results-list selection (keyboard arrows or the Next/Prev buttons)
// while focus stays in the search edit, navigating to the newly selected match.
// Returns false (not handled) when there are no results, so the caller can fall
// back to a normal document search.
static bool MoveResultSelection(MainWindow* win, int vkey) {
    FindWindowWnd* w = Wnd(win);
    if (!w) {
        return false;
    }
    int n = len(win->findMatches);
    if (n == 0) {
        return false;
    }
    constexpr int kPage = 10;
    int cur = w->sel;
    if (cur < 0) {
        cur = CurrentMatchIndex(win); // start from where the document already is
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
            // unlike the arrow keys, paging doesn't wrap around; it clamps to
            // the last match (issue #5742)
            idx = (cur < 0) ? 0 : std::min(cur + kPage, n - 1);
            break;
        case VK_PRIOR: // Page Up
            idx = (cur < 0) ? n - 1 : std::max(cur - kPage, 0);
            break;
        default:
            return false;
    }
    if (idx == cur) {
        return true; // e.g. a single match wrapping onto itself
    }
    w->sel = idx;
    OnResultSelected(win);
    AppShellInvalidate(win);
    return true;
}

// Enter / F3 / the Next/Prev buttons: walk the results list when it is for the
// current term. If the find box changed (paste, debounce already fired), start
// a new search instead of stepping stale matches (issue #893).
bool FindWindowNextOrPrev(MainWindow* win, bool forward) {
    if (!IsFindWindowVisible(win)) {
        return false;
    }
    if (FindFlushPendingSearch(win)) {
        return true;
    }
    int dir = forward ? VK_DOWN : VK_UP;
    if (FindTermDiffersFromLast(win) || !MoveResultSelection(win, dir)) {
        forward ? FindNext(win) : FindPrev(win);
    }
    return true;
}

// --- public API -------------------------------------------------------------

void DeleteFindWindow(MainWindow* win) {
    delete win->findWindow;
    win->findWindow = nullptr;
    delete win->findPagesEdit;
    win->findPagesEdit = nullptr;
}

// the gpui window the find window's fields are in: its own (Windows), or the
// frame's. Null while its own is being made
gpui::Window* FindWindowHostGpui(MainWindow* win) {
    FindWindowWnd* w = win ? win->findWindow : nullptr;
    if (w && w->visible && w->tw) {
        return ToolWindowGpui(w->tw);
    }
    return win ? win->gpuiWin : nullptr;
}

// in a window of its own the fields have the keyboard while it is the active one
bool FindWindowHasKeyboard(MainWindow* win) {
    FindWindowWnd* w = win ? win->findWindow : nullptr;
    if (!w || !w->visible || !w->tw) {
        return true;
    }
    return ToolWindowIsActive(w->tw);
}

bool FindWindowOnKeyDown(MainWindow* win, int vk, bool ctrl, bool shift, bool alt) {
    if (!IsFindWindowVisible(win)) {
        return false;
    }
    bool editFocused = IsFindEditFocused(win);
    FindWindowUI* ui = win->findWindow->ui;
    bool historyDropped = editFocused && ui && ui->ddHistory.comboOpen;
    switch (vk) {
        case VK_F3:
            if (ctrl || alt) {
                return false;
            }
            return FindWindowNextOrPrev(win, !shift);
        case VK_DOWN:
        case VK_UP:
        case VK_NEXT:
        case VK_PRIOR:
            // Only borrow navigation keys from the search edit. The page-range
            // edit, results list, and open history dropdown handle their own.
            if (!editFocused || historyDropped || ctrl || alt) {
                return false;
            }
            return MoveResultSelection(win, vk);
        case VK_HOME:
        case VK_END: {
            // Home/End in the page-range edit or results list remain native.
            if (!editFocused || alt) {
                return false;
            }
            // Ctrl+Home / Ctrl+End: always jump to first/last result (#5797)
            if (ctrl) {
                return MoveResultSelection(win, vk);
            }
            // Home / End: if the caret is already at the start/end of the search
            // text, move the results list; otherwise let the edit move the caret
            gp::InputState* edit = win->findEdit;
            int caret = gp::InputCursor(edit);
            int textLen = (int)gp::InputValue(edit).len;
            bool noSel = edit->selectedRange.start == edit->selectedRange.end;
            bool toEnd = vk == VK_END;
            bool caretAtBound = noSel && (toEnd ? caret == textLen : caret == 0);
            if (caretAtBound) {
                return MoveResultSelection(win, vk);
            }
            return false;
        }
    }
    return false;
}

bool IsFindWindowVisible(MainWindow* win) {
    return win && win->findWindow && win->findWindow->visible;
}

static gp::InputState* EnsurePagesEdit(MainWindow* win) {
    if (win->findPagesEdit) {
        return win->findPagesEdit;
    }
    auto* s = new gp::InputState();
    s->focus = gp::FocusHandleNew(win->gpuiWin ? win->gpuiWin->app : nullptr);
    gp::InputSetPlaceholder(s, ToGpui(StrL("e.g. 3,4-6,18-")));
    win->findPagesEdit = s;
    return s;
}

void ShowFindWindow(MainWindow* win) {
    // Capture before we point findEdit at this window's (possibly empty) edit.
    // CLI -search writes the term into the hidden compact bar; without this the
    // floating window opens with a blank box and empty result rows (#6055).
    TempStr term = CurrentFindTermTemp(win);
    if (!win->findWindow) {
        win->findWindow = new FindWindowWnd();
    }
    FindWindowWnd* w = win->findWindow;
    EnsureFindEdit(win);
    EnsurePagesEdit(win);
    if (len(term) > 0 && FindEditTextLen(win) == 0) {
        gp::InputSetValue(win->findEdit, ToGpui(term));
    }
    w->visible = true;
    w->wantFocus = true;
    w->wantSelectAll = true;
    FindWindowOpenToolWindow(win);
    AppShellInvalidate(win);
    // populate the results list: show what's cached, and (re)run the search for
    // the current term so snippets get built now that the window is visible
    FindWindowRefreshResults(win);
    if (FindEditTextLen(win) == 0 || win->findThread) {
        return;
    }
    // finish off a completed search's rows, but don't start a new search: the
    // term restored into the box is a starting point, not a request
    if (len(win->findMatches) > 0 && !win->findCountHasSnippets) {
        EnsureFindSnippets(win);
    }
}

void HideFindWindow(MainWindow* win) {
    FindWindowWnd* w = Wnd(win);
    if (!w || !w->visible) {
        return;
    }
    AbortFinding(win, true);
    gp::Window* host = FindWindowHostGpui(win);
    if (win->findEdit && host) {
        gp::InputBlur(win->findEdit, host->app, host);
    }
    w->visible = false;
    w->wantFocus = false;
    w->sel = -1;
    if (w->tw) {
        // orig's SavePos + ShowWindow(SW_HIDE) + HwndSetFocus(frame)
        Rect r = ToolWindowRect(w->tw);
        if (!r.IsEmpty()) {
            gSettings->searchUIWindowPos = r;
            ScheduleSaveSettings();
        }
        ToolWindowClose(w->tw);
        w->tw = nullptr;
        AppShellFocusFrame(win);
    }
    AppShellInvalidate(win);
}

void FindWindowSetStatus(MainWindow* win, Str s, int totalHits) {
    FindWindowWnd* w = Wnd(win);
    if (!w) {
        return;
    }
    Str text = s ? s : StrL("");
    w->statusCapped = str::EndsWith(text, StrL("+"));
    if (totalHits >= 0) {
        w->statusTotalHits = totalHits;
    }
    str::ReplaceWithCopy(&w->status, text);
    AppShellInvalidate(win);
}

// --- a window of its own (Windows) ------------------------------------------

static gp::El* FindWindowContentEl(MainWindow* win, gp::Ctx* cx, bool ownWindow);

static Str FindToolTitle() {
    return Tr("Find");
}

static gp::El* FindToolBuild(MainWindow* win, gp::Ctx* cx) {
    FindWindowWnd* w = Wnd(win);
    if (!w || !w->visible || !w->tw) {
        return nullptr;
    }
    return FindWindowContentEl(win, cx, true);
}

// orig's FindWindowWnd::OnKeyDown, the keys taken before the search edit
static bool FindToolOnCaptureKey(MainWindow* win, gp::Ctx*, const gp::KeyEvent* ev) {
    return FindWindowOnKeyDown(win, ev->vk, ev->ctrl, ev->shift, ev->alt);
}

// and the ones the edit leaves: Esc hides, Ctrl + F selects the term
static bool FindToolOnKey(MainWindow* win, gp::Ctx*, const gp::KeyEvent* ev) {
    FindWindowWnd* w = Wnd(win);
    if (!w) {
        return false;
    }
    if (ev->vk == VK_ESCAPE) {
        HideFindBar(win);
        return true;
    }
    if (ev->vk == 'F' && ev->ctrl && !ev->alt) {
        w->wantFocus = true;
        w->wantSelectAll = true;
        AppShellInvalidate(win);
        return true;
    }
    return false;
}

// the caption's close button hides the find UI, as orig's OnClose
static void FindToolOnClosed(MainWindow* win) {
    FindWindowWnd* w = Wnd(win);
    if (!w) {
        return;
    }
    w->tw = nullptr;
    HideFindBar(win);
}

// orig's SavePos (on WM_EXITSIZEMOVE and when hidden)
static void FindToolOnMoved(MainWindow* win, Rect outer) {
    FindWindowWnd* w = Wnd(win);
    if (!w || !w->visible || !w->tw) {
        return;
    }
    gSettings->searchUIWindowPos = outer;
    ScheduleSaveSettings();
}

#if OS_WIN
// orig's FindWindowPlacementRect: the saved position, or a default size near
// the top-right of the frame
static Rect FindWindowPlacementRect(MainWindow* win) {
    HWND hwndFrame = AppShellNativeHwnd(win);
    Rect r = gSettings->searchUIWindowPos;
    if (r.IsEmpty()) {
        Rect fr = HwndWindowRect(hwndFrame);
        int dpi = std::max(AppShellWindowDpi(win), 96);
        int dx = MulDiv(520, dpi, 96);
        int dy = MulDiv(360, dpi, 96);
        r = {fr.x + fr.dx - dx - MulDiv(40, dpi, 96), fr.y + MulDiv(80, dpi, 96), dx, dy};
    }
    return ShiftRectToWorkArea(r, hwndFrame, true);
}
#endif

// orig's FindWindowWnd::Create: WS_POPUP | WS_CAPTION | WS_SYSMENU |
// WS_THICKFRAME, WS_EX_TOOLWINDOW, owned by the frame
static void FindWindowOpenToolWindow(MainWindow* win) {
    FindWindowWnd* w = Wnd(win);
    if (!w || !ToolWindowsAvailable()) {
        return;
    }
    if (w->tw) {
        ToolWindowActivate(w->tw);
        return;
    }
#if OS_WIN
    ToolWindowDesc desc;
    desc.name = "find";
    desc.title = FindToolTitle;
    desc.frame = ToolWinFrame::Caption;
    desc.resize = ToolWinResize::Resizable;
    desc.owner = ToolWinOwner::Owned;
    desc.style = ToolWinStyle::Tool;
    desc.minClient = Size(kFindWinMinDx, kFindWinMinDy);
    desc.build = FindToolBuild;
    desc.onKey = FindToolOnKey;
    desc.onCaptureKey = FindToolOnCaptureKey;
    desc.onClosed = FindToolOnClosed;
    desc.onMoved = FindToolOnMoved;
    w->tw = ToolWindowOpen(desc, win, FindWindowPlacementRect(win));
#endif
}

// --- the gpui card ----------------------------------------------------------

struct FindWindowView {
    MainWindow* win = nullptr;

    static void OnInput(FindWindowView* self, gp::Ctx* cx, const gp::InputEvent* ev);
    static void OnPagesInput(FindWindowView* self, gp::Ctx* cx, const gp::InputEvent* ev);
    static void OnCmd(FindWindowView* self, gp::Ctx* cx, const gp::ClickEvent*, int64_t cmdId);
    static void OnRow(FindWindowView* self, gp::Ctx* cx, const gp::ClickEvent*, int64_t rowIdx);
    static void OnScroll(FindWindowView* self, gp::Ctx* cx, const gp::ScrollEvent* ev);
    static void OnHeaderDown(FindWindowView* self, gp::Ctx* cx, const gp::MouseDownEvent* ev);
    static void OnHeaderDrag(FindWindowView* self, gp::Ctx* cx, const gp::DragMoveEvent* ev);
    static void OnHeaderUp(FindWindowView* self, gp::Ctx* cx, const gp::MouseUpEvent*);
};

void FindWindowView::OnInput(FindWindowView* self, gp::Ctx* cx, const gp::InputEvent* ev) {
    MainWindow* win = self->win;
    if (!IsMainWindowValidAndNotClosing(win)) {
        return;
    }
    if (ev->kind == gp::InputEventKind::PressEnter) {
        FindWindowNextOrPrev(win, !ev->shift);
        FindNotify(self->win, cx);
        return;
    }
    if (ev->kind != gp::InputEventKind::Change) {
        return;
    }
    FindEditSetModified(win, true);
    OnFindBarTextChanged(win);
    FindNotify(self->win, cx);
}

void FindWindowView::OnPagesInput(FindWindowView* self, gp::Ctx* cx, const gp::InputEvent* ev) {
    MainWindow* win = self->win;
    if (!IsMainWindowValidAndNotClosing(win)) {
        return;
    }
    if (ev->kind != gp::InputEventKind::Change && ev->kind != gp::InputEventKind::PressEnter) {
        return;
    }
    // a changed range re-runs the search over the newly allowed pages
    OnFindBarTextChanged(win);
    FindNotify(self->win, cx);
}

void FindWindowView::OnCmd(FindWindowView* self, gp::Ctx* cx, const gp::ClickEvent*, int64_t cmdId) {
    MainWindow* win = self->win;
    if (!IsMainWindowValidAndNotClosing(win)) {
        return;
    }
    switch ((int)cmdId) {
        case CmdFindPrev:
            FindWindowNextOrPrev(win, false);
            break;
        case CmdFindNext:
            FindWindowNextOrPrev(win, true);
            break;
        case CmdFindToggleMatchCase:
            FindToggleMatchCase(win);
            break;
        case CmdFindToggleMatchWholeWord:
            FindToggleMatchWholeWord(win);
            break;
        case kFindWinPinCmdId:
            ToggleFloatingFindUI(win);
            break;
        case kFindWinCloseCmdId:
            HideFindBar(win);
            break;
        default:
            break;
    }
    FindNotify(self->win, cx);
}

void FindWindowView::OnRow(FindWindowView* self, gp::Ctx* cx, const gp::ClickEvent*, int64_t rowIdx) {
    MainWindow* win = self->win;
    FindWindowWnd* w = Wnd(win);
    if (!w || !IsMainWindowValidAndNotClosing(win)) {
        return;
    }
    w->sel = (int)rowIdx;
    OnResultSelected(win);
    FindNotify(self->win, cx);
}

void FindWindowView::OnScroll(FindWindowView* self, gp::Ctx* cx, const gp::ScrollEvent* ev) {
    FindWindowWnd* w = Wnd(self->win);
    if (w) {
        w->scrollY = ev->offsetY;
    }
    FindNotify(self->win, cx);
}

void FindWindowView::OnHeaderDown(FindWindowView* self, gp::Ctx* cx, const gp::MouseDownEvent* ev) {
    FindWindowWnd* w = Wnd(self->win);
    if (!w) {
        return;
    }
    w->dragging = true;
    w->dragStart = Point((int)ev->x, (int)ev->y);
    w->dragOrigin = Point(w->pos.x, w->pos.y);
    FindNotify(self->win, cx);
}

void FindWindowView::OnHeaderDrag(FindWindowView* self, gp::Ctx* cx, const gp::DragMoveEvent* ev) {
    MainWindow* win = self->win;
    FindWindowWnd* w = Wnd(win);
    if (!w || !w->dragging) {
        return;
    }
    int dx = (int)ev->event.x - w->dragStart.x;
    int dy = (int)ev->event.y - w->dragStart.y;
    w->pos.x = limitValue(w->dragOrigin.x + dx, 0, std::max(0, win->frameRc.dx - w->pos.dx));
    w->pos.y = limitValue(w->dragOrigin.y + dy, 0, std::max(0, win->frameRc.dy - w->pos.dy));
    FindNotify(self->win, cx);
}

void FindWindowView::OnHeaderUp(FindWindowView* self, gp::Ctx* cx, const gp::MouseUpEvent*) {
    FindWindowWnd* w = Wnd(self->win);
    if (!w || !w->dragging) {
        return;
    }
    w->dragging = false;
    gSettings->searchUIWindowPos = w->pos;
    ScheduleSaveSettings();
    FindNotify(self->win, cx);
}

static gp::El* WinButton(FindWindowWnd* w, gp::Ctx* cx, Str id, gp::IconName icon, Str label, Str tooltip, int cmdId,
                         bool selected, const char* svg = nullptr) {
    gpc::Button* b = gpc::Button::New(cx, GpuiDup(cx->a, id))
                         ->Ghost()
                         ->Compact()
                         ->WithSize(gp::UiSize::Small)
                         ->Selected(selected)
                         ->Tooltip(GpuiDup(cx->a, tooltip))
                         ->OnClick(gp::ListenTo(w->ui->view, &FindWindowView::OnCmd, (intptr_t)cmdId));
    if (svg) {
        b->Icon(gpc::ButtonIcon::New(cx, gpc::Icon::Empty(cx)->Data(ToGpui(Str(svg)))));
    } else if (icon != gp::IconName::None) {
        b->Icon(icon);
    } else {
        b->Label(GpuiDup(cx->a, label));
    }
    return b->IntoEl();
}

// the terms the snippets highlight: what the count scan ran for, or what is in
// the box while it is still running
static void ResultHighlightWords(MainWindow* win, StrVec& out) {
    Str term = win->findCountText;
    TempStr boxTerm;
    if (len(term) == 0) {
        boxTerm = FindEditTextTemp(win);
        term = boxTerm;
    }
    if (len(term) > 0) {
        out.Append(term);
    }
}

static gp::El* BuildResults(MainWindow* win, gp::Ctx* cx) {
    FindWindowWnd* w = Wnd(win);
    const gp::Theme& th = gp::ThemeNow(cx->app);
    float rowDy = (float)DpiScale(kFindWinRowDy);
    StrVec words;
    ResultHighlightWords(win, words);

    int n = len(win->findMatches);
    float viewH = w->ui->listView.h > 0 ? w->ui->listView.h : 200;
    int first = std::max(0, (int)(w->scrollY / rowDy) - 2);
    int end = std::min(n, (int)((w->scrollY + viewH) / rowDy) + 3);
    gp::El* list = gp::Div(cx->a)->FlexCol()->W(gp::kFill);
    if (first > 0) {
        list->Child(gp::Div(cx->a)->W(gp::kFill)->H((float)first * rowDy));
    }
    gp::Listener click = gp::ListenTo(w->ui->view, &FindWindowView::OnRow, 0);
    for (int i = first; i < end; i++) {
        const FindMatch& fm = win->findMatches[i];
        gp::El* row = gp::Div(cx->a)
                          ->FlexRow()
                          ->W(gp::kFill)
                          ->H(rowDy)
                          ->ItemsCenter()
                          ->PadX(6)
                          ->Gap(10)
                          ->Cursor(gp::CursorKind::Pointer)
                          ->PathClick(GpuiDup(cx->a, fmt("find-res-%d", i)))
                          ->OnClick(gp::ListenerArg(click, i));
        if (i == w->sel) {
            row->Bg(th.selection);
        } else {
            row->HoverBg(th.tokens.muted);
        }
        // snippet on the left with the matched term highlighted, the page label
        // in a fixed-width right column so it doesn't move as rows change
        row->Child(FilterHighlightText(cx, fm.snippet, words, th.foreground, 13)->Flex1()->MinW(0)->ClipX());
        // the page label sits in a fixed-width right column so the right edge
        // stays stable as the rows change (orig's pageColDx)
        TempStr pageStr = win->ctrl ? win->ctrl->GetPageLabeTemp(fm.startPage) : fmt("%d", fm.startPage);
        row->Child(gp::Div(cx->a)
                       ->FlexRow()
                       ->JustifyEnd()
                       ->W((float)DpiScale(kFindWinPageColDx))
                       ->Shrink0()
                       ->Child(gp::TextEl(cx->a, GpuiDup(cx->a, pageStr))->Font(12)->Fg(th.mutedFg)));
        list->Child(row);
    }
    if (end < n) {
        list->Child(gp::Div(cx->a)->W(gp::kFill)->H((float)(n - end) * rowDy));
    }
    return gp::Div(cx->a)
        ->FlexCol()
        ->Flex1()
        ->W(gp::kFill)
        ->MinH(0)
        ->ScrollY(w->scrollY)
        ->ScrollFromPath()
        ->OnScroll(gp::ListenTo(w->ui->view, &FindWindowView::OnScroll))
        ->BoundsOut(&w->ui->listView)
        ->Child(list);
}

gp::El* FindWindowBuild(MainWindow* win, gp::Ctx* cx) {
    FindWindowWnd* w = Wnd(win);
    if (!w || !w->visible || w->tw) {
        return nullptr;
    }
    return FindWindowContentEl(win, cx, false);
}

// the find window's content: a card floating in the frame, or what fills a
// window of its own
static gp::El* FindWindowContentEl(MainWindow* win, gp::Ctx* cx, bool ownWindow) {
    FindWindowWnd* w = Wnd(win);
    if (!w->ui) {
        w->ui = new FindWindowUI();
    }
    if (!w->ui->view.IsValid()) {
        w->ui->view = gp::EntityNewState<FindWindowView>(cx->app);
    }
    auto* view = (FindWindowView*)gp::EntityGet(cx->app, w->ui->view.id);
    view->win = win;

    gp::InputState* edit = EnsureFindEdit(win);
    edit->onChange = gp::ListenTo(w->ui->view, &FindWindowView::OnInput);
    gp::InputState* pages = EnsurePagesEdit(win);
    pages->onChange = gp::ListenTo(w->ui->view, &FindWindowView::OnPagesInput);

    // first show: orig's PositionFindWindow centers it on the frame unless the
    // settings remember a place for it
    if (!ownWindow && w->pos.dx <= 0) {
        Rect saved = gSettings->searchUIWindowPos;
        w->pos.dx = saved.dx > 0 ? saved.dx : DpiScale(kFindWinDefaultDx);
        w->pos.dy = saved.dy > 0 ? saved.dy : DpiScale(kFindWinDefaultDy);
        w->pos.dx = limitValue(w->pos.dx, DpiScale(kFindWinMinDx), std::max(win->frameRc.dx, DpiScale(kFindWinMinDx)));
        w->pos.dy = limitValue(w->pos.dy, DpiScale(kFindWinMinDy), std::max(win->frameRc.dy, DpiScale(kFindWinMinDy)));
        w->pos.x = saved.dx > 0 ? saved.x : (win->frameRc.dx - w->pos.dx) / 2;
        w->pos.y = saved.dy > 0 ? saved.y : (win->frameRc.dy - w->pos.dy) / 3;
    }
    if (!ownWindow) {
        w->pos.x = limitValue(w->pos.x, 0, std::max(0, win->frameRc.dx - w->pos.dx));
        w->pos.y = limitValue(w->pos.y, 0, std::max(0, win->frameRc.dy - w->pos.dy));
    }

    const gp::Theme& th = gp::ThemeNow(cx->app);
    float pad = (float)DpiScale(kFindWinPadding);
    float gap = (float)DpiScale(kFindWinGap);
    gp::El* card = gp::Div(cx->a)->FlexCol()->Gap(gap)->Pad(pad);
    gp::El* header = gp::Div(cx->a)->FlexRow()->ItemsCenter()->Gap(gap)->W(gp::kFill)->Shrink0();
    if (ownWindow) {
        card->W(gp::kFill)->Flex1()->MinH(0);
    } else {
        card->Absolute()
            ->Left((float)w->pos.x)
            ->Top((float)w->pos.y)
            ->W((float)w->pos.dx)
            ->H((float)w->pos.dy)
            ->Radius(8)
            ->Bg(th.tokens.popover)
            ->Border(1, th.border);
        // the header doubles as orig's caption: dragging it moves the card
        header->PathClick(GStrL("find-win-header"))
            ->OnDrag(GStrL("sumatra-find-win"))
            ->OnMouseDown(gp::ListenTo(w->ui->view, &FindWindowView::OnHeaderDown))
            ->OnDragMove(gp::ListenTo(w->ui->view, &FindWindowView::OnHeaderDrag))
            ->OnMouseUp(gp::ListenTo(w->ui->view, &FindWindowView::OnHeaderUp))
            ->OnMouseUpOut(gp::ListenTo(w->ui->view, &FindWindowView::OnHeaderUp));
    }
    // orig's ApplyFindHistory
    FindWindowUI* ui = w->ui;
    const StrVec& history = FindHistory();
    Str first = len(history) > 0 ? history[0] : Str{};
    if (!ui->ddHistory.app) {
        ui->ddHistory.Init(cx->app);
    }
    if (ui->historyLen != len(history) || !str::Eq(ui->historyFirst, first)) {
        ui->historyLen = len(history);
        str::ReplaceWithCopy(&ui->historyFirst, first);
        ui->ddHistory.SetItems(history, -1);
    }
    // orig's OnHistoryCommitted: picking an entry out of the open history list
    // is a search request; walking the list with the arrow keys only fills the
    // box, and waits for Enter
    if (ui->ddHistory.TakeComboListPick()) {
        FindEditSetModified(win, true);
        OnFindBarTextChanged(win);
        FindFlushPendingSearch(win);
    }
    ui->ddHistory.TakeComboPicked();
    header->Child(
        gp::Div(cx->a)->Flex1()->MinW(0)->Child(ui->ddHistory.BuildCombo(cx, StrL("find-win-edit"), edit, gp::kFill)));
    header->Child(gp::TextEl(cx->a, GpuiDup(cx->a, w->status))->Font(12)->Fg(th.mutedFg)->Shrink0());
    header->Child(
        WinButton(w, cx, StrL("find-win-prev"), gp::IconName::ChevronUp, {}, Tr("Find Previous"), CmdFindPrev, false));
    header->Child(
        WinButton(w, cx, StrL("find-win-next"), gp::IconName::ChevronDown, {}, Tr("Find Next"), CmdFindNext, false));
    header->Child(WinButton(w, cx, StrL("find-win-case"), gp::IconName::None, {}, Tr("Match Case"),
                            CmdFindToggleMatchCase, win->findMatchCase, gIconMatchCase));
    header->Child(WinButton(w, cx, StrL("find-win-word"), gp::IconName::None, {}, Tr("Match Whole Word"),
                            CmdFindToggleMatchWholeWord, win->findMatchWholeWord, gIconMatchWholeWord));
    header->Child(
        WinButton(w, cx, StrL("find-win-pin"), gp::IconName::Minimize, {}, Tr("Find"), kFindWinPinCmdId, true));
    if (!ownWindow) {
        // in a window of its own the caption has the close box
        header->Child(
            WinButton(w, cx, StrL("find-win-close"), gp::IconName::Close, {}, Tr("Close"), kFindWinCloseCmdId, false));
    }
    card->Child(header);

    int nPages = win->ctrl ? std::max(win->ctrl->PageCount(), 1) : 1;
    gp::El* pagesRow = gp::Div(cx->a)->FlexRow()->ItemsCenter()->Gap(gap)->W(gp::kFill)->Shrink0();
    pagesRow->Child(gp::TextEl(cx->a, GpuiDup(cx->a, fmt(Tr("Limit to pages 1-%d:").s, nPages)))
                        ->Font(13)
                        ->Fg(th.foreground)
                        ->Shrink0());
    pagesRow->Child(gpc::Input::New(cx, GStrL("find-win-pages"), pages)
                        ->WithSize(gp::UiSize::Small)
                        ->W((float)DpiScale(kFindWinPagesDx))
                        ->IntoEl());
    card->Child(pagesRow);

    card->Child(BuildResults(win, cx));

    if (w->wantFocus) {
        w->wantFocus = false;
        gp::InputFocus(edit, cx->app, cx->win);
        if (w->wantSelectAll) {
            w->wantSelectAll = false;
            gp::InputSelectAll(edit, cx->app, cx->win);
        }
    }
    return card;
}

// ng: what the scripted tests read back (orig's TestFindWindowContents)
TempStr FindWindowContentsResultTemp(int maxRows, int* exitCodeOut) {
    auto finish = [&](int code, TempStr s) -> TempStr {
        if (exitCodeOut) {
            *exitCodeOut = code;
        }
        return s;
    };
    MainWindow* win = len(gWindows) > 0 ? gWindows[0] : nullptr;
    if (!win) {
        return finish(2, str::DupTemp(StrL("NOTREADY no-window")));
    }
    FindWindowWnd* w = Wnd(win);
    str::Builder out;
    out.Append(fmt("OK visible=%d sel=%d n=%d status=%s\n", w && w->visible ? 1 : 0, w ? w->sel : -1,
                   len(win->findMatches), w ? Str(w->status) : StrL("")));
    int n = len(win->findMatches);
    if (maxRows > 0) {
        n = std::min(n, maxRows);
    }
    for (int i = 0; i < n; i++) {
        const FindMatch& fm = win->findMatches[i];
        out.Append(fmt("%d\t%d\t%s\n", i, fm.startPage, fm.snippet));
    }
    return finish(0, ToStrTemp(out));
}
