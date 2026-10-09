/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// ng: orig's AnnotFilterToolbar.cpp - the annotation list: a filter box, the
// annotations that match it and the Delete / Discard / Save / Save As buttons,
// with the filter-syntax cheat sheet under them. orig puts it in a resizable
// tool window; so does this where the platform can (gui/ToolWindow.h), and
// elsewhere it is a card centered in the frame, with the same rows, the same
// buttons and the same debounced "selecting a row selects the annotation on
// the page" behaviour. The list is orig's VirtListBox in
// multi-select mode: its selection functions are ported below under their
// names, and only the rows in view are built, in a gpui scroll container.

#include "gui/GpuiBridge.h"
#include "VirtKeys.h"
#include "base/File.h"
#include "base/UITask.h"
#if OS_WIN
#include "base/Win.h"
#endif

#include "gui/Dpi.h"
#include "gui/UIModels.h"

#include "Settings.h"
#include "AppSettings.h"
#include "DisplayMode.h"
#include "Annotation.h"
#include "AnnotSearch.h"
#include "FilterUtil.h"
#include "FilterHighlightDraw.h"
#include "DocController.h"
#include "EngineBase.h"
#include "base/GuessFileType.h"
#include "EngineAll.h"
#include "DisplayModel.h"
#include "Theme.h"
#include "Translations.h"
#include "Commands.h"
#include "SumatraPDF.h"
#include "MainWindow.h"
#include "WindowTab.h"
#include "Toolbar.h"
#include "gui/AppShell.h"
#include "gui/ToolWindow.h"
#include "AnnotEditToolbar.h"
#include "AnnotFilterToolbar.h"

#include "SumatraLog.h"

// orig's kSelectionDebounceMs: a row selected with the arrow keys only becomes
// the page's selection once the caret settles
constexpr int kSelectionDebounceMs = 300;
constexpr int kMaxListLines = 12;
// ng: the card gives up rows before it grows past a short frame
constexpr int kMinListLines = 3;
// everything in the card but the list: caption, filter, buttons, help, margins
constexpr int kCardChromeDy = 340;
constexpr int kFloatWinPadding = 8;
constexpr int kFloatWinGap = 6;
// orig's sizes: the app font (9 pt), a list row 4 taller than its text, an
// edit, a button 2 above and below its text, a line of the cheat sheet
constexpr int kRowDy = 19;
constexpr float kListFontSize = 12;
constexpr float kEditDy = 17;
constexpr float kBtnDy = 19;
constexpr float kHelpLineDy = 15;
constexpr int kHelpLines = 4;
constexpr float kListBorderDx = 1;
// orig's window: 360 x 540 beside the frame, never smaller than its content
constexpr int kFloatWinDx = 360;
constexpr int kFloatWinDy = 540;
constexpr int kFloatWinMinClientDx = 256;
constexpr int kFloatWinMinClientDy = 2 * kFloatWinPadding + 300;
// rows built above and below the visible ones
constexpr int kOverscanRows = 2;
constexpr int kScrollbarDx = 10;

// which part of the card a key was typed into
enum class AnnotListFocus {
    List,
    Filter
};

struct AnnotFilterToolbar {
    MainWindow* win = nullptr;
    bool visible = false;
    bool wantFocus = false;
    // orig's list is a focusable control; here it is drawn, not focused, so
    // the frame has the focus and this flag sends the keys to the list
    bool listFocused = false;
    // orig's window, where the platform can have one; null: a card in the frame
    ToolWindow* tw = nullptr;
    gp::InputState* edit = nullptr;
    Str filterText; // owned
    AnnotMatchOpts filter;
    StrVec filterWords;
    Vec<Annotation*> annotations;
    Vec<Annotation*> visibleAnnots;
    // orig's VirtListBox: the caret, the Shift anchor and one flag per row
    int sel = -1;
    int anchor = -1;
    Vec<u8> selected;
    float scrollY = 0;
    int viewRows = kMaxListLines;
    gp::Bounds listBounds{};
    gp::Bounds cardBounds{};
    int selectPendingMs = -1;

    ~AnnotFilterToolbar() { str::Free(filterText); }
};

static void OnListSelectionChanged(AnnotFilterToolbar*);

static AnnotFilterToolbar* GetOrCreate(MainWindow* win) {
    if (!win) {
        return nullptr;
    }
    if (!win->annotFilterToolbar) {
        auto* f = new AnnotFilterToolbar();
        f->win = win;
        win->annotFilterToolbar = f;
    }
    return win->annotFilterToolbar;
}

static WindowTab* FilterTab(AnnotFilterToolbar* f) {
    return f && f->win ? f->win->CurrentTab() : nullptr;
}

static void LoadAnnotations(AnnotFilterToolbar* f) {
    VecReset(f->annotations);
    WindowTab* tab = FilterTab(f);
    DisplayModel* dm = tab ? tab->AsFixed() : nullptr;
    EngineBase* engine = dm ? dm->GetEngine() : nullptr;
    if (!engine) {
        return;
    }
    EngineMupdfGetLoadedAnnotations(engine, f->annotations);
}

// Reparse the filter box. Bad syntax (a typo in a ":" condition) would match
// nothing, which reads as "the filter is broken"; treat it as plain text
// instead, the way it behaved before conditions existed.
static void SetFilter(AnnotFilterToolbar* f, Str text) {
    f->filter.Reset();
    f->filterWords.Reset();
    if (!ParseAnnotSearch(text, f->filter)) {
        f->filter.Reset();
        StrVec words;
        SplitFilterToWords(text, words);
        for (Str w : words) {
            AnnotSearchAddContentWord(f->filter, w);
        }
    }
    AnnotSearchContentWords(f->filter, f->filterWords);
}

static Annotation* VisibleAnnotAt(AnnotFilterToolbar* f, int idx) {
    if (!f || !VecIsValidIndex(f->visibleAnnots, idx)) {
        return nullptr;
    }
    return f->visibleAnnots[idx];
}

// the gpui window the list is drawn in: its own, or the frame's
static gp::Window* HostWindow(AnnotFilterToolbar* f) {
    if (!f || !f->win) {
        return nullptr;
    }
    return f->tw ? ToolWindowGpui(f->tw) : f->win->gpuiWin;
}

// the edit is the focused element of its window (which may not be the active
// window)
static bool FilterHasHostFocus(AnnotFilterToolbar* f) {
    gp::Window* host = HostWindow(f);
    if (!f || !f->edit || !f->visible || !host) {
        return false;
    }
    return gp::FocusHandleIsFocused(host, f->edit->focus);
}

// orig's edit->IsFocused(): it has the keyboard
static bool IsFilterFocused(AnnotFilterToolbar* f) {
    if (!FilterHasHostFocus(f)) {
        return false;
    }
    return !f->tw || ToolWindowIsActive(f->tw);
}

// orig's list takes the focus when pressed. ng: it is drawn, not focused, so
// the edit loses the focus and a flag sends the keys to the list
static void FocusList(AnnotFilterToolbar* f) {
    if (!f->tw) {
        if (!f->listFocused) {
            AppShellFocusFrame(f->win);
        }
        f->listFocused = true;
        return;
    }
    gp::Window* host = HostWindow(f);
    if (host && host->input) {
        gp::InputBlur(host->input, host->app, host);
    }
    f->listFocused = true;
}

// orig's HwndSetFocus(hwndCanvas): the document takes the keys
static void FocusDocument(AnnotFilterToolbar* f) {
    if (f->tw) {
        AppShellActivateWindow(f->win);
    }
    AppShellFocusFrame(f->win);
}

// --- the list: orig's VirtListBox with multiSelect ----------------------------

static int ItemsCount(AnnotFilterToolbar* f) {
    return len(f->visibleAnnots);
}

static float RowDy() {
    return (float)DpiScale(kRowDy);
}

// whole rows only, like orig's UsableDy()
static float ViewDy(AnnotFilterToolbar* f) {
    return (float)f->viewRows * RowDy();
}

static float MaxScrollY(AnnotFilterToolbar* f) {
    return std::max((float)ItemsCount(f) * RowDy() - ViewDy(f), 0.f);
}

static void ScrollTo(AnnotFilterToolbar* f, float y) {
    f->scrollY = std::max(0.f, std::min(y, MaxScrollY(f)));
}

static void EnsureVisible(AnnotFilterToolbar* f, int idx) {
    if (idx < 0 || idx >= ItemsCount(f)) {
        return;
    }
    float dy = RowDy();
    float top = (float)idx * dy;
    if (top < f->scrollY) {
        ScrollTo(f, top);
        return;
    }
    if (top + dy > f->scrollY + ViewDy(f)) {
        ScrollTo(f, top + dy - ViewDy(f));
    }
}

static void ClearSelected(AnnotFilterToolbar* f) {
    int n = ItemsCount(f);
    VecReset(f->selected);
    if (n <= 0) {
        return;
    }
    u8* p = VecAppendBlanks(f->selected, n);
    if (p) {
        memset(p, 0, (size_t)n);
    }
}

static void EnsureSelectedSize(AnnotFilterToolbar* f) {
    int n = ItemsCount(f);
    if (len(f->selected) == n) {
        return;
    }
    ClearSelected(f);
    if (f->sel >= 0 && f->sel < n) {
        f->selected[f->sel] = 1;
    }
}

static bool IsSelected(AnnotFilterToolbar* f, int idx) {
    if (idx < 0 || idx >= ItemsCount(f)) {
        return false;
    }
    EnsureSelectedSize(f);
    return f->selected[idx] != 0;
}

static int SelectedCount(AnnotFilterToolbar* f) {
    EnsureSelectedSize(f);
    int n = 0;
    for (u8 v : f->selected) {
        if (v) {
            n++;
        }
    }
    return n;
}

static void GetSelectedIndices(AnnotFilterToolbar* f, Vec<int>& out) {
    VecReset(out);
    EnsureSelectedSize(f);
    int n = ItemsCount(f);
    for (int i = 0; i < n; i++) {
        if (f->selected[i]) {
            VecAppend(out, i);
        }
    }
}

static void ToggleSelected(AnnotFilterToolbar* f, int idx) {
    if (idx < 0 || idx >= ItemsCount(f)) {
        return;
    }
    EnsureSelectedSize(f);
    f->selected[idx] = f->selected[idx] ? 0 : 1;
}

// Exclusive of other items; caret at `to`, anchor at `from`.
static void SelectRange(AnnotFilterToolbar* f, int from, int to) {
    int n = ItemsCount(f);
    if (n == 0) {
        return;
    }
    from = limitValue(from, 0, n - 1);
    to = limitValue(to, 0, n - 1);
    EnsureSelectedSize(f);
    int a = std::min(from, to);
    int b = std::max(from, to);
    for (int i = 0; i < n; i++) {
        f->selected[i] = (i >= a && i <= b) ? 1 : 0;
    }
    f->anchor = from;
    f->sel = to;
    EnsureVisible(f, to);
}

static void SelectAll(AnnotFilterToolbar* f) {
    int n = ItemsCount(f);
    if (n == 0) {
        return;
    }
    EnsureSelectedSize(f);
    for (int i = 0; i < n; i++) {
        f->selected[i] = 1;
    }
    if (f->sel < 0) {
        f->sel = 0;
    }
    if (f->anchor < 0) {
        f->anchor = f->sel;
    }
    OnListSelectionChanged(f);
}

// -1 clears the selection; doesn't call OnListSelectionChanged
static bool SetCurrentSelection(AnnotFilterToolbar* f, int idx) {
    if (idx < 0) {
        idx = -1;
    } else if (idx >= ItemsCount(f)) {
        return false;
    }
    f->sel = idx;
    f->anchor = idx;
    ClearSelected(f);
    if (idx >= 0) {
        f->selected[idx] = 1;
    }
    EnsureVisible(f, idx);
    return true;
}

static void ApplyClick(AnnotFilterToolbar* f, int idx, bool ctrl, bool shift) {
    if (shift) {
        if (f->anchor < 0) {
            f->anchor = (f->sel >= 0) ? f->sel : idx;
        }
        SelectRange(f, f->anchor, idx);
    } else if (ctrl) {
        ToggleSelected(f, idx);
        f->sel = idx;
        EnsureVisible(f, idx);
    } else {
        SetCurrentSelection(f, idx);
    }
    OnListSelectionChanged(f);
}

static void ApplyNav(AnnotFilterToolbar* f, int idx, bool ctrl, bool shift) {
    if (shift) {
        if (f->anchor < 0) {
            f->anchor = (f->sel >= 0) ? f->sel : idx;
        }
        SelectRange(f, f->anchor, idx);
    } else if (ctrl) {
        f->sel = idx;
        EnsureVisible(f, idx);
    } else {
        SetCurrentSelection(f, idx);
    }
    OnListSelectionChanged(f);
}

// orig's SetModel(): the rows are new
static void ResetListState(AnnotFilterToolbar* f) {
    f->sel = -1;
    f->anchor = -1;
    VecReset(f->selected);
    f->scrollY = 0;
}

static bool IsListNavKey(int vkey) {
    return vkey == VK_UP || vkey == VK_DOWN || vkey == VK_PRIOR || vkey == VK_NEXT || vkey == VK_HOME || vkey == VK_END;
}

// orig's VirtListBox::OnKeyDown
static bool ListOnKeyDown(AnnotFilterToolbar* f, int vkey, bool isCtrl, bool isShift, bool isAlt) {
    int n = ItemsCount(f);
    if (n == 0) {
        return false;
    }
    if (vkey == 'A' && isCtrl && !isAlt) {
        SelectAll(f);
        return true;
    }
    if (vkey == VK_SPACE && isCtrl) {
        if (f->sel >= 0) {
            ToggleSelected(f, f->sel);
            OnListSelectionChanged(f);
        }
        return true;
    }
    int perPage = std::max(f->viewRows, 1);
    int idx = f->sel;
    switch (vkey) {
        case VK_UP:
            idx = (idx < 0) ? 0 : idx - 1;
            break;
        case VK_DOWN:
            idx = (idx < 0) ? 0 : idx + 1;
            break;
        case VK_PRIOR:
            idx = (idx < 0) ? 0 : idx - perPage;
            break;
        case VK_NEXT:
            idx = (idx < 0) ? 0 : idx + perPage;
            break;
        case VK_HOME:
            idx = 0;
            break;
        case VK_END:
            idx = n - 1;
            break;
        default:
            return false;
    }
    idx = limitValue(idx, 0, n - 1);
    ApplyNav(f, idx, isCtrl, isShift);
    return true;
}

// --- the filter and what the list selects --------------------------------------

// orig's ApplyVisibleToList: the caret, the selected rows and the scroll
// position survive a rebuild when their annotations are still listed
static void RebuildList(AnnotFilterToolbar* f) {
    Annotation* caret = VisibleAnnotAt(f, f->sel);
    float prevScrollY = f->scrollY;
    Vec<Annotation*> keepSel;
    Vec<int> idxs;
    GetSelectedIndices(f, idxs);
    for (int i : idxs) {
        if (Annotation* a = VisibleAnnotAt(f, i)) {
            VecAppend(keepSel, a);
        }
    }
    VecReset(f->visibleAnnots);
    for (Annotation* annot : f->annotations) {
        if (AnnotMatches(annot, f->filter)) {
            VecAppend(f->visibleAnnots, annot);
        }
    }
    ResetListState(f);
    ScrollTo(f, prevScrollY);
    int caretIdx = caret ? VecFind(f->visibleAnnots, caret) : -1;
    if (caretIdx < 0) {
        Annotation* keep = FilterTab(f) ? FilterTab(f)->selectedAnnotation : nullptr;
        caretIdx = keep ? VecFind(f->visibleAnnots, keep) : -1;
    }
    if (len(keepSel) > 0) {
        if (caretIdx < 0) {
            caretIdx = VecFind(f->visibleAnnots, keepSel[0]);
        }
        if (caretIdx >= 0) {
            SetCurrentSelection(f, caretIdx);
        }
        for (Annotation* a : keepSel) {
            int i = VecFind(f->visibleAnnots, a);
            if (i >= 0 && !IsSelected(f, i)) {
                ToggleSelected(f, i);
            }
        }
        return;
    }
    if (caretIdx >= 0) {
        SetCurrentSelection(f, caretIdx);
    }
}

static void CancelPendingSelection(AnnotFilterToolbar* f) {
    f->selectPendingMs = -1;
}

static void ApplySelectionNow(AnnotFilterToolbar* f) {
    CancelPendingSelection(f);
    WindowTab* tab = FilterTab(f);
    if (!tab) {
        return;
    }
    Annotation* annot = VisibleAnnotAt(f, f->sel);
    if (!annot || annot == tab->selectedAnnotation) {
        return;
    }
    SetSelectedAnnotation(tab, annot);
}

static void ScheduleSelection(AnnotFilterToolbar* f) {
    WindowTab* tab = FilterTab(f);
    if (!tab) {
        return;
    }
    Annotation* annot = VisibleAnnotAt(f, f->sel);
    CancelPendingSelection(f);
    if (annot == tab->selectedAnnotation) {
        return;
    }
    f->selectPendingMs = kSelectionDebounceMs;
}

// the buttons are rebuilt from the selection every frame
static void OnListSelectionChanged(AnnotFilterToolbar* f) {
    ScheduleSelection(f);
    AppShellInvalidate(f->win);
}

static void OnFilterTextChanged(AnnotFilterToolbar* f) {
    WindowTab* tab = FilterTab(f);
    Annotation* keep = tab ? tab->selectedAnnotation : nullptr;
    Str text = FromGpui(gp::InputValue(f->edit));
    str::ReplaceWithCopy(&f->filterText, text);
    SetFilter(f, f->filterText);
    LoadAnnotations(f);
    RebuildList(f);
    AppShellInvalidate(f->win);
    int idx = keep ? VecFind(f->visibleAnnots, keep) : -1;
    if (idx >= 0) {
        SetCurrentSelection(f, idx);
        return;
    }
    if (len(f->visibleAnnots) > 0) {
        SetCurrentSelection(f, 0);
        ScheduleSelection(f);
        return;
    }
    if (tab) {
        SetSelectedAnnotation(tab, nullptr);
    }
}

// ng: orig waits out a WM_TIMER; the shell's tick counts it down instead
void AnnotFilterTick(MainWindow* win, int elapsedMs) {
    AnnotFilterToolbar* f = win ? win->annotFilterToolbar : nullptr;
    if (!f || f->selectPendingMs < 0) {
        return;
    }
    f->selectPendingMs -= elapsedMs;
    if (f->selectPendingMs <= 0) {
        ApplySelectionNow(f);
        AppShellInvalidate(win);
    }
}

bool IsFloatingAnnotListVisible(MainWindow* win) {
    AnnotFilterToolbar* f = win ? win->annotFilterToolbar : nullptr;
    return f && f->visible;
}

static void AnnotFilterOpenToolWindow(AnnotFilterToolbar* f);

static void ShowAnnotFilterWindow(MainWindow* win) {
    AnnotFilterToolbar* f = GetOrCreate(win);
    if (!f || !win->gpuiWin) {
        return;
    }
    if (!f->edit) {
        f->edit = new gp::InputState();
        f->edit->focus = gp::FocusHandleNew(win->gpuiWin->app);
    }
    StartLoadingAnnotationsForUi(win->CurrentTab());
    LoadAnnotations(f);
    RebuildList(f);
    f->visible = true;
    f->wantFocus = true;
    f->listFocused = false;
    logf("AnnotFilterList: %d annotations, %d shown\n", len(f->annotations), len(f->visibleAnnots));
    AnnotFilterOpenToolWindow(f);
    AppShellInvalidate(win);
}

static void HideAnnotFilterWindow(MainWindow* win) {
    AnnotFilterToolbar* f = win ? win->annotFilterToolbar : nullptr;
    if (!f || !f->visible) {
        return;
    }
    bool hadKeyboard = f->tw ? ToolWindowIsActive(f->tw) : (f->listFocused || IsFilterFocused(f));
    gp::Window* host = HostWindow(f);
    if (f->tw && host && host->input) {
        gp::InputBlur(host->input, host->app, host);
    }
    f->visible = false;
    f->listFocused = false;
    if (f->tw) {
        // orig hides its window; this one is made again where it was
        ToolWindowClose(f->tw);
        f->tw = nullptr;
    }
    if (hadKeyboard) {
        AppShellFocusFrame(win);
    }
    AppShellInvalidate(win);
}

void ToggleFloatingAnnotList(MainWindow* win) {
    if (!win) {
        return;
    }
    if (IsFloatingAnnotListVisible(win)) {
        HideAnnotFilterWindow(win);
        return;
    }
    ShowAnnotFilterWindow(win);
}

// orig's window has closeOnEsc = gSettings->escToExit; without it Esc is the
// frame's
bool AnnotFilterOnEscape(MainWindow* win) {
    if (!IsFloatingAnnotListVisible(win) || !gSettings->escToExit) {
        return false;
    }
    // a window of its own closes on its own Esc, not on the frame's
    if (win->annotFilterToolbar->tw) {
        return false;
    }
    HideAnnotFilterWindow(win);
    return true;
}

// orig's HandleEscape: Esc in the filter clears it, then gives the focus to
// the document
static void HandleEscape(AnnotFilterToolbar* f) {
    if (gp::InputValue(f->edit).len > 0) {
        gp::InputSetValue(f->edit, gp::Str{});
        // ng: orig's EN_CHANGE re-applies the (now empty) filter
        OnFilterTextChanged(f);
        return;
    }
    FocusDocument(f);
}

// Home / End move the list only when the caret has nowhere to go in the text
static bool FilterHomeEndMovesList(AnnotFilterToolbar* f, int vkey, bool isCtrl) {
    if (vkey != VK_HOME && vkey != VK_END) {
        return true;
    }
    if (isCtrl) {
        return true;
    }
    gp::Selection sel = f->edit->selectedRange;
    int textLen = (int)gp::InputValue(f->edit).len;
    bool toEnd = (vkey == VK_END);
    return sel.IsEmpty() && (toEnd ? sel.end == textLen : sel.start == 0);
}

static void DeleteListSelection(AnnotFilterToolbar* f);

// orig's OnFilterWndProc (the filter has the focus) and
// AnnotFilterWindow::OnKeyDown (the list has it), plus its closeOnCtrlW
static bool AnnotFilterKey(AnnotFilterToolbar* f, int vkey, bool isCtrl, bool isShift, bool isAlt,
                           AnnotListFocus focus) {
    if (vkey == 'W' && isCtrl && !isShift && !isAlt) {
        HideAnnotFilterWindow(f->win);
        return true;
    }
    if (focus == AnnotListFocus::Filter) {
        if (vkey == VK_ESCAPE) {
            // orig's closeOnEsc comes before the edit sees the key
            if (gSettings->escToExit) {
                HideAnnotFilterWindow(f->win);
                return true;
            }
            HandleEscape(f);
            return true;
        }
        if (vkey == VK_RETURN) {
            ApplySelectionNow(f);
            return true;
        }
        if (!IsListNavKey(vkey) || !FilterHomeEndMovesList(f, vkey, isCtrl)) {
            return false;
        }
        ListOnKeyDown(f, vkey, isCtrl, isShift, isAlt);
        return true;
    }
    // macOS Delete is Backspace. The filter box returned above; this is the list.
    bool macDelete = false;
#if OS_DARWIN
    macDelete = vkey == VK_BACK && !isCtrl && !isAlt;
#endif
    if (vkey == VK_DELETE || macDelete) {
        DeleteListSelection(f);
        return true;
    }
    if ((vkey == 'A' && isCtrl && !isAlt) || (vkey == VK_SPACE && isCtrl) || IsListNavKey(vkey)) {
        ListOnKeyDown(f, vkey, isCtrl, isShift, isAlt);
        return true;
    }
    // ng: orig's list is in a window of its own, so the plain arrows it does
    // not use never turn the document's pages
    return (vkey == VK_LEFT || vkey == VK_RIGHT) && !isCtrl && !isAlt;
}

// the keys the list takes while it has the keyboard; the filter gets its own
// in OnCaptureKey, before the edit does
bool AnnotFilterOnKeyDown(MainWindow* win, int vk, bool ctrl, bool shift, bool alt) {
    AnnotFilterToolbar* f = win ? win->annotFilterToolbar : nullptr;
    // a window of its own has its own keys (AnnotFilterToolOnKey)
    if (!f || !f->visible || !f->listFocused || f->tw) {
        return false;
    }
    gp::Window* gw = win->gpuiWin;
    if (gw && gw->input && gw->input->focused) {
        return false;
    }
    return AnnotFilterKey(f, vk, ctrl, shift, alt, AnnotListFocus::List);
}

// ng: orig's list takes the focus when it is pressed and keeps it while its
// window is the active one; a press in the filter or outside the card takes
// it away (the buttons do not: they are not focusable in orig either)
void AnnotFilterOnMouseDown(MainWindow* win, float x, float y) {
    AnnotFilterToolbar* f = win ? win->annotFilterToolbar : nullptr;
    if (!f || !f->visible || f->tw) {
        return;
    }
    gp::Point pt{x, y};
    if (!f->cardBounds.Contains(pt)) {
        f->listFocused = false;
        return;
    }
    if (!f->listBounds.Contains(pt)) {
        return;
    }
    if (!f->listFocused) {
        AppShellFocusFrame(win);
    }
    f->listFocused = true;
}

void UpdateAnnotFilterToolbar(MainWindow* win) {
    AnnotFilterToolbar* f = win ? win->annotFilterToolbar : nullptr;
    if (!f) {
        return;
    }
    if (!win->pdfAnnotationsToolbarEnabled) {
        HideAnnotFilterWindow(win);
        return;
    }
    if (!IsFilterFocused(f)) {
        WindowTab* tab = FilterTab(f);
        Annotation* keep = tab ? tab->selectedAnnotation : nullptr;
        int idx = keep ? VecFind(f->visibleAnnots, keep) : -1;
        if (idx >= 0 && f->sel != idx) {
            SetCurrentSelection(f, idx);
        }
    }
    if (f->visible) {
        AppShellInvalidate(win);
    }
}

static void PostedRefreshAnnots(MainWindow* win) {
    if (IsMainWindowValidAndNotClosing(win)) {
        RefreshAnnotFilterAnnotations(win);
    }
}

// The cached Annotation* belong to an engine the caller is about to destroy.
// Drop them now - a paint between here and the refresh below would otherwise
// read freed annotations - and re-read once the close has finished.
void ClearAnnotFilterAnnotations(MainWindow* win) {
    AnnotFilterToolbar* f = win ? win->annotFilterToolbar : nullptr;
    if (!f) {
        return;
    }
    CancelPendingSelection(f);
    VecReset(f->annotations);
    VecReset(f->visibleAnnots);
    ResetListState(f);
    uitask::Post(MkFunc0(PostedRefreshAnnots, win), "RefreshAnnotFilterAnnots");
}

void RefreshAnnotFilterAnnotations(MainWindow* win) {
    AnnotFilterToolbar* f = win ? win->annotFilterToolbar : nullptr;
    if (!f) {
        return;
    }
    LoadAnnotations(f);
    RebuildList(f);
    if (f->visible) {
        AppShellInvalidate(win);
    }
}

void DeleteAnnotFilterToolbar(MainWindow* win) {
    AnnotFilterToolbar* f = win ? win->annotFilterToolbar : nullptr;
    if (!f) {
        return;
    }
    win->annotFilterToolbar = nullptr;
    if (f->tw) {
        ToolWindowClose(f->tw);
        f->tw = nullptr;
    }
    delete f->edit;
    delete f;
}

void ApplyAnnotFilterText(MainWindow* win, Str text) {
    AnnotFilterToolbar* f = GetOrCreate(win);
    if (!f) {
        return;
    }
    str::ReplaceWithCopy(&f->filterText, text);
    SetFilter(f, text);
    LoadAnnotations(f);
    RebuildList(f);
    if (f->edit) {
        gp::InputSetValue(f->edit, ToGpui(text));
    }
    AppShellInvalidate(win);
}

// orig's nSel in UpdateFloatButtons: what the Delete button would delete
static int DeleteCount(AnnotFilterToolbar* f) {
    WindowTab* tab = FilterTab(f);
    int nSel = SelectedCount(f);
    if (nSel == 0 && f->sel >= 0) {
        nSel = 1;
    }
    if (nSel == 0 && tab && tab->selectedAnnotation) {
        nSel = 1;
    }
    return nSel;
}

// orig's DeleteFloatSelected: every selected row, else the caret's, else the
// page's selection
static void DeleteListSelection(AnnotFilterToolbar* f) {
    WindowTab* tab = FilterTab(f);
    if (!tab) {
        return;
    }
    Vec<int> idxs;
    GetSelectedIndices(f, idxs);
    if (len(idxs) == 0 && f->sel >= 0) {
        VecAppend(idxs, f->sel);
    }
    Vec<Annotation*> toDelete;
    for (int idx : idxs) {
        if (Annotation* a = VisibleAnnotAt(f, idx)) {
            VecAppend(toDelete, a);
        }
    }
    if (len(toDelete) == 0 && tab->selectedAnnotation) {
        VecAppend(toDelete, tab->selectedAnnotation);
    }
    if (len(toDelete) == 0) {
        return;
    }
    Annotation* keepSelected = tab->selectedAnnotation;
    if (VecContains(toDelete, keepSelected)) {
        keepSelected = nullptr;
    }
    MainWindow* win = f->win;
    for (Annotation* annot : toDelete) {
        DetachAnnotationFromUI(annot);
        DeleteAnnotation(annot);
    }
    RefreshAnnotationLists(tab);
    SetSelectedAnnotation(tab, keepSelected);
    if (IsMainWindowValidAndNotClosing(win)) {
        MainWindowRerender(win);
        ToolbarUpdateStateForWindow(win, true);
    }
}

// a press on a row; a double-click selects that row alone and applies it now
static void ListRowClick(AnnotFilterToolbar* f, int idx, bool ctrl, bool shift, int clickCount) {
    if (!VecIsValidIndex(f->visibleAnnots, idx)) {
        return;
    }
    if (clickCount < 2) {
        ApplyClick(f, idx, ctrl, shift);
        return;
    }
    SetCurrentSelection(f, idx);
    OnListSelectionChanged(f);
    ApplySelectionNow(f);
}

// --- the gpui card ----------------------------------------------------------

struct AnnotFilterView {
    MainWindow* win = nullptr;

    static void OnFilter(AnnotFilterView* self, gp::Ctx* cx, const gp::InputEvent* ev);
    static void OnRow(AnnotFilterView* self, gp::Ctx* cx, const gp::ClickEvent*, int64_t idx);
    static void OnScroll(AnnotFilterView* self, gp::Ctx* cx, const gp::ScrollEvent* ev);
    static void OnCaptureKey(AnnotFilterView* self, gp::Ctx* cx, const gp::KeyEvent* ev);
    static void OnCmd(AnnotFilterView* self, gp::Ctx* cx, const gp::ClickEvent*, int64_t cmdId);
    static void OnClose(AnnotFilterView* self, gp::Ctx* cx, const gp::ClickEvent*);
    static void OnListDown(AnnotFilterView* self, gp::Ctx* cx, const gp::MouseDownEvent*);
};

static gp::Entity<AnnotFilterView> gAnnotFilterView;

// ng: gp::Notify() wakes this view, not the shell that renders it
static void AnnotFilterNotify(AnnotFilterView* self, gp::Ctx* cx) {
    gp::Notify(cx);
    if (IsMainWindowValidAndNotClosing(self->win)) {
        AppShellInvalidate(self->win);
    }
}

void AnnotFilterView::OnFilter(AnnotFilterView* self, gp::Ctx* cx, const gp::InputEvent* ev) {
    AnnotFilterToolbar* f = self->win ? self->win->annotFilterToolbar : nullptr;
    if (!f || ev->kind != gp::InputEventKind::Change) {
        return;
    }
    OnFilterTextChanged(f);
    AnnotFilterNotify(self, cx);
}

void AnnotFilterView::OnRow(AnnotFilterView* self, gp::Ctx* cx, const gp::ClickEvent* ev, int64_t idx) {
    AnnotFilterToolbar* f = self->win ? self->win->annotFilterToolbar : nullptr;
    if (!f) {
        return;
    }
    ListRowClick(f, (int)idx, ev->modifiers.control, ev->modifiers.shift, ev->clickCount);
    AnnotFilterNotify(self, cx);
}

void AnnotFilterView::OnScroll(AnnotFilterView* self, gp::Ctx* cx, const gp::ScrollEvent* ev) {
    AnnotFilterToolbar* f = self->win ? self->win->annotFilterToolbar : nullptr;
    if (!f) {
        return;
    }
    f->scrollY = ev->offsetY;
    AnnotFilterNotify(self, cx);
}

// a press on the list of a window of its own (the frame's card hears of it
// through AnnotFilterOnMouseDown)
void AnnotFilterView::OnListDown(AnnotFilterView* self, gp::Ctx* cx, const gp::MouseDownEvent*) {
    AnnotFilterToolbar* f = self->win ? self->win->annotFilterToolbar : nullptr;
    if (!f || !f->tw) {
        return;
    }
    FocusList(f);
    AnnotFilterNotify(self, cx);
}

void AnnotFilterView::OnCaptureKey(AnnotFilterView* self, gp::Ctx* cx, const gp::KeyEvent* ev) {
    AnnotFilterToolbar* f = self->win ? self->win->annotFilterToolbar : nullptr;
    if (!f || !FilterHasHostFocus(f)) {
        return;
    }
    if (!AnnotFilterKey(f, ev->vk, ev->ctrl, ev->shift, ev->alt, AnnotListFocus::Filter)) {
        return;
    }
    const_cast<gp::KeyEvent*>(ev)->propagate = false;
    AnnotFilterNotify(self, cx);
}

void AnnotFilterView::OnCmd(AnnotFilterView* self, gp::Ctx* cx, const gp::ClickEvent*, int64_t cmdId) {
    MainWindow* win = self->win;
    AnnotFilterToolbar* f = win ? win->annotFilterToolbar : nullptr;
    if (!f) {
        return;
    }
    if (cmdId == CmdDeleteAnnotation) {
        DeleteListSelection(f);
    } else {
        ExecuteCmd(win, (int)cmdId);
    }
    AnnotFilterNotify(self, cx);
}

void AnnotFilterView::OnClose(AnnotFilterView* self, gp::Ctx* cx, const gp::ClickEvent*) {
    HideAnnotFilterWindow(self->win);
    gp::Notify(cx);
}

// the cheat sheet under the buttons; the syntax itself is not translated
static gp::El* BuildFilterHelp(gp::Ctx* cx, Color muted, float fontScale) {
    struct HelpRow {
        Str syntax;
        Str what;
    };
    HelpRow rows[] = {
        {StrL(":a=name  :a!=name"), Tr("author is / is not")},
        {StrL(":t=text  :t!=line"), Tr("type is / is not")},
        {StrL(":c+  :c-"), Tr("has / has no contents")},
    };
    float font = kListFontSize * fontScale;
    float gap = (float)kFloatWinGap;
    gp::El* box = gp::Div(cx->a)->FlexCol()->W(gp::kFill)->Shrink0();
    box->Child(gp::Div(cx->a)
                   ->FlexRow()
                   ->ItemsCenter()
                   ->H(kHelpLineDy + gap / 2)
                   ->PadB(gap / 2)
                   ->Child(gp::TextEl(cx->a, ToGpui(Tr("Search syntax:")))->Font(font)->Fg(ToGpui(muted))));
    for (const HelpRow& r : rows) {
        gp::El* row = gp::Div(cx->a)->FlexRow()->ItemsCenter()->H(kHelpLineDy)->Gap(2 * gap)->W(gp::kFill);
        row->Child(gp::TextEl(cx->a, GpuiDup(cx->a, r.syntax))->Font(font)->Fg(ToGpui(muted))->W(102)->Shrink0());
        row->Child(gp::TextEl(cx->a, GpuiDup(cx->a, r.what))->Font(font)->Fg(ToGpui(muted))->Flex1()->Truncate());
        box->Child(row);
    }
    return box;
}

// orig's DrawAnnotationListRow: type on the left, the contents in a muted
// color with the filter's matches highlighted, the page number on the right
static gp::El* AnnotRowEl(AnnotFilterToolbar* f, gp::Ctx* cx, int idx, Color colBg, Color colText, float fontScale) {
    float font = kListFontSize * fontScale;
    Annotation* annot = f->visibleAnnots[idx];
    if (IsSelected(f, idx)) {
        colBg = AccentColor(colBg, 30);
    }
    gp::El* row = gp::Div(cx->a)
                      ->FlexRow()
                      ->ItemsCenter()
                      ->W(gp::kFill)
                      ->H(RowDy())
                      ->Shrink0()
                      ->PadX(6)
                      ->Gap(8)
                      ->Bg(ToGpui(colBg))
                      ->PathClick(GpuiDup(cx->a, fmt("annot-row-%d", idx)))
                      ->OnClick(gp::ListenTo(gAnnotFilterView, &AnnotFilterView::OnRow, (intptr_t)idx));
    Str typeName = AnnotationReadableNameTemp(annot->type);
    row->Child(gp::TextEl(cx->a, GpuiDup(cx->a, typeName))->Font(font)->Fg(ToGpui(colText))->Shrink0());
    TempStr oneLine = str::NormalizeWSTemp(Contents(annot));
    if (len(oneLine) > 0) {
        Color colContents = EnsureContrast(ThemeWindowTextDisabledColor(), colBg);
        row->Child(
            FilterHighlightText(cx, oneLine, f->filterWords, ToGpui(colContents), font)->Flex1()->MinW(0)->ClipX());
    } else {
        row->Child(gp::Div(cx->a)->Flex1());
    }
    row->Child(gp::TextEl(cx->a, GpuiDup(cx->a, fmt("%d", annot->pageNo)))->Font(font)->Fg(ToGpui(colText))->Shrink0());
    return row;
}

static gp::El* AnnotFilterContentEl(MainWindow* win, gp::Ctx* cx, bool ownWindow);

gp::El* AnnotFilterListBuild(MainWindow* win, gp::Ctx* cx) {
    AnnotFilterToolbar* f = win ? win->annotFilterToolbar : nullptr;
    if (!f || !f->visible || f->tw) {
        return nullptr;
    }
    return AnnotFilterContentEl(win, cx, false);
}

// --- a window of its own (Windows) ------------------------------------------

static Str AnnotFilterToolTitle() {
    return Tr("Annotations");
}

static gp::El* AnnotFilterToolBuild(MainWindow* win, gp::Ctx* cx) {
    AnnotFilterToolbar* f = win ? win->annotFilterToolbar : nullptr;
    if (!f || !f->visible || !f->tw) {
        return nullptr;
    }
    return AnnotFilterContentEl(win, cx, true);
}

// orig's WindowBase::PreTranslateMessage for this window: the list's keys
// (AnnotFilterWindow::OnKeyDown), then closeOnEsc and closeOnCtrlW. The keys
// of the filter were taken before the edit saw them (OnCaptureKey)
static bool AnnotFilterToolOnKey(MainWindow* win, gp::Ctx* cx, const gp::KeyEvent* ev) {
    AnnotFilterToolbar* f = win ? win->annotFilterToolbar : nullptr;
    if (!f || !f->visible) {
        return false;
    }
    if (ev->vk == VK_ESCAPE) {
        // Esc only with EscToExit (issue #6124)
        if (gSettings->escToExit) {
            HideAnnotFilterWindow(win);
        }
        return true;
    }
    if (cx->win->input && cx->win->input->focused) {
        return false;
    }
    return AnnotFilterKey(f, ev->vk, ev->ctrl, ev->shift, ev->alt, AnnotListFocus::List);
}

// the caption's close box (orig hides the window) or the frame went away
static void AnnotFilterToolOnClosed(MainWindow* win) {
    AnnotFilterToolbar* f = win ? win->annotFilterToolbar : nullptr;
    if (!f) {
        return;
    }
    f->tw = nullptr;
    f->visible = false;
    f->listFocused = false;
}

// orig's SavePos: where the window is, for the next time it is shown
static void AnnotFilterToolOnMoved(MainWindow* win, Rect outer) {
    AnnotFilterToolbar* f = win ? win->annotFilterToolbar : nullptr;
    if (f && f->visible && f->tw) {
        win->annotListFloatPos = outer;
    }
}

// orig's WM_EXITSIZEMOVE: a place the user chose wins over the default one
static void AnnotFilterToolOnExitSizeMove(MainWindow* win, Rect outer) {
    AnnotFilterToolbar* f = win ? win->annotFilterToolbar : nullptr;
    if (f && f->visible && f->tw) {
        win->annotListFloatPos = outer;
        win->annotListFloatPosUserSet = true;
    }
}

static ToolWindowDesc AnnotFilterToolDesc() {
    // orig: WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME,
    // WS_EX_TOOLWINDOW, owned by the frame
    ToolWindowDesc desc;
    desc.name = "annotlist";
    desc.title = AnnotFilterToolTitle;
    desc.frame = ToolWinFrame::Caption;
    desc.resize = ToolWinResize::Resizable;
    desc.owner = ToolWinOwner::Owned;
    desc.style = ToolWinStyle::Tool;
    desc.minClient = Size(kFloatWinMinClientDx, kFloatWinMinClientDy);
    desc.build = AnnotFilterToolBuild;
    desc.onKey = AnnotFilterToolOnKey;
    desc.onClosed = AnnotFilterToolOnClosed;
    desc.onMoved = AnnotFilterToolOnMoved;
    desc.onExitSizeMove = AnnotFilterToolOnExitSizeMove;
    return desc;
}

static bool FrameIsMaxOrFullscreen(MainWindow* win) {
    bool zoomed = win->isMaximized || (win->gpuiWin && win->gpuiWin->maximized);
#if OS_WIN
    zoomed = zoomed || IsZoomed(AppShellNativeHwnd(win));
#endif
    return win->isFullScreen || win->presentation || zoomed;
}

// orig's AnnotFilterDefaultRect: beside the frame on the side with more room,
// at its top; at the right edge of the screen when the frame fills it
static Rect AnnotFilterDefaultRect(MainWindow* win, int dx, int dy) {
    Rect fr = AppShellWindowScreenRect(win);
    int gap = MulDiv(kFloatWinGap, std::max(AppShellWindowDpi(win), 96), 96);
    Rect area = (win->isFullScreen || win->presentation) ? AppShellMonitorRect(win) : AppShellWorkArea(win);
    dx = std::min(dx, std::max(area.dx, 1));
    dy = std::min(dy, std::max(area.dy, 1));

    if (FrameIsMaxOrFullscreen(win)) {
        int x = area.x + area.dx - dx;
        int y = area.y + ((area.dy - dy) / 2);
        return {x, y, dx, dy};
    }

    int spaceRight = area.x + area.dx - (fr.x + fr.dx);
    int spaceLeft = fr.x - area.x;
    int x = spaceRight >= spaceLeft ? fr.x + fr.dx + gap : fr.x - gap - dx;
    return AppShellShiftToWorkArea({x, fr.y, dx, dy}, nullptr, true);
}

// orig's AnnotFilterWindowPlacementRect
static Rect AnnotFilterToolRect(MainWindow* win) {
    int dpi = std::max(AppShellWindowDpi(win), 96);
    int dx = MulDiv(kFloatWinDx, dpi, 96);
    int dy = MulDiv(kFloatWinDy, dpi, 96);
    Rect saved = win->annotListFloatPos;
    if (saved.dx > 0) {
        dx = saved.dx;
    }
    if (saved.dy > 0) {
        dy = saved.dy;
    }
    if (win->annotListFloatPosUserSet && !saved.IsEmpty()) {
        return AppShellShiftToWorkArea(saved, nullptr, true);
    }
    if (FrameIsMaxOrFullscreen(win) || saved.IsEmpty()) {
        return AnnotFilterDefaultRect(win, dx, dy);
    }
    return AppShellShiftToWorkArea(saved, nullptr, true);
}

static void AnnotFilterOpenToolWindow(AnnotFilterToolbar* f) {
    if (f->tw || !ToolWindowsAvailable()) {
        return;
    }
    Rect r = AnnotFilterToolRect(f->win);
    f->win->annotListFloatPos = r;
    f->tw = ToolWindowOpen(AnnotFilterToolDesc(), f->win, r);
}

static gp::El* AnnotFilterContentEl(MainWindow* win, gp::Ctx* cx, bool ownWindow) {
    AnnotFilterToolbar* f = win->annotFilterToolbar;
    if (!gAnnotFilterView.IsValid()) {
        gAnnotFilterView = gp::EntityNewState<AnnotFilterView>(cx->app);
    }
    auto* view = (AnnotFilterView*)gp::EntityGet(cx->app, gAnnotFilterView.id);
    view->win = win;
    f->edit->onChange = gp::ListenTo(gAnnotFilterView, &AnnotFilterView::OnFilter);
    if (FilterHasHostFocus(f)) {
        f->listFocused = false;
    }
    // in its own window the edit and the buttons are in orig's 12 px
    float fontScale = ownWindow ? ToolWindowSetUiFontPx(cx, kListFontSize) : 1;
    // orig's UpdateCue
    gp::InputSetPlaceholder(f->edit, GpuiDup(cx->a, fmt(Tr("filter %d annotations").s, len(f->annotations))));

    const gp::Theme& th = gp::ThemeNow(cx->app);
    Color colBg = ThemeWindowControlBackgroundColor();
    Color colText = ThemeWindowTextColor();
    Color muted = EnsureContrast(ThemeWindowTextDisabledColor(), colBg);
    WindowTab* tab = FilterTab(f);
    bool dirty = tab && tab->AsFixed() && EngineHasUnsavedAnnotations(tab->AsFixed()->GetEngine());
    float pad = (float)DpiScale(kFloatWinPadding);
    float gap = (float)DpiScale(kFloatWinGap);
    gp::WinSize ws = gp::WindowSize(cx->win);

    gp::El* card = gp::Div(cx->a)
                       ->FlexCol()
                       ->Gap(gap)
                       ->Pad(pad)
                       ->Bg(ToGpui(colBg))
                       ->CaptureKeyDown(gp::ListenTo(gAnnotFilterView, &AnnotFilterView::OnCaptureKey));
    if (ownWindow) {
        card->W(gp::kFill)->Flex1()->MinH(0);
    } else {
        card->W((float)DpiScale(380))->Radius(th.radius)->Border(1, th.border)->BoundsOut(&f->cardBounds);
        // ng: orig's window caption and its close box
        gp::El* caption = gp::Div(cx->a)->FlexRow()->ItemsCenter()->Gap(gap)->W(gp::kFill);
        caption->Child(gp::TextEl(cx->a, ToGpui(Tr("Annotations")))
                           ->Font(kListFontSize)
                           ->Bold()
                           ->Fg(ToGpui(colText))
                           ->Flex1()
                           ->MinW(0)
                           ->Truncate());
        caption->Child(gpc::Button::New(cx, GStrL("annot-filter-close"))
                           ->Icon(gp::IconName::Close)
                           ->Ghost()
                           ->Compact()
                           ->WithSize(gp::UiSize::XSmall)
                           ->Tooltip(ToGpui(Tr("Close")))
                           ->OnClick(gp::ListenTo(gAnnotFilterView, &AnnotFilterView::OnClose))
                           ->IntoEl());
        card->Child(caption);
    }
    gp::El* editEl =
        gpc::Input::New(cx, GStrL("annot-filter"), f->edit)->WithSize(gp::UiSize::Small)->W(gp::kFill)->IntoEl();
    if (ownWindow) {
        editEl->H(kEditDy);
    }
    card->Child(gp::Div(cx->a)->W(gp::kFill)->Shrink0()->Child(editEl));

    // ng: only the rows in view are built, between two spacers that stand in
    // for the rest
    float rowDy = RowDy();
    int viewRows = limitValue((int)((ws.dipH - (float)kCardChromeDy) / rowDy), kMinListLines, kMaxListLines);
    if (ownWindow) {
        // the list takes what the window has left, in whole rows
        float helpDy = (float)kHelpLines * kHelpLineDy + gap / 2;
        float chromeDy = 2 * pad + kEditDy + 4 * kBtnDy + 6 * gap + helpDy + 2 * kListBorderDx;
        viewRows = std::max((int)((ws.dipH - chromeDy) / rowDy), 1);
    }
    if (viewRows != f->viewRows) {
        f->viewRows = viewRows;
        ScrollTo(f, f->scrollY);
    }
    int n = ItemsCount(f);
    int first = std::max((int)(f->scrollY / rowDy) - kOverscanRows, 0);
    int last = std::min(first + f->viewRows + 1 + 2 * kOverscanRows, n);
    gp::El* rows = gp::Div(cx->a)->FlexCol()->W(gp::kFill);
    // orig's ScrollbarDx(): the rows keep clear of the scrollbar's strip
    if (MaxScrollY(f) > 0) {
        rows->PadR((float)DpiScale(kScrollbarDx));
    }
    if (first > 0) {
        rows->Child(gp::Div(cx->a)->W(gp::kFill)->H((float)first * rowDy)->Shrink0());
    }
    for (int i = first; i < last; i++) {
        rows->Child(AnnotRowEl(f, cx, i, colBg, colText, fontScale));
    }
    if (last < n) {
        rows->Child(gp::Div(cx->a)->W(gp::kFill)->H((float)(n - last) * rowDy)->Shrink0());
    }
    // orig draws a dashed outline while the list has the keys
    gp::Rgba colRing = f->listFocused ? ToGpui(colText) : th.border;
    gp::El* list = gp::Div(cx->a)
                       ->Id(GStrL("annot-filter-list"))
                       ->FlexCol()
                       ->W(gp::kFill)
                       ->H(ViewDy(f) + 2 * kListBorderDx)
                       ->Shrink0()
                       ->Border(kListBorderDx, colRing)
                       ->ScrollY(f->scrollY)
                       ->ScrollFromPath()
                       ->OnScroll(gp::ListenTo(gAnnotFilterView, &AnnotFilterView::OnScroll))
                       ->BoundsOut(&f->listBounds)
                       ->Child(rows);
    if (ownWindow) {
        list->OnMouseDown(gp::ListenTo(gAnnotFilterView, &AnnotFilterView::OnListDown));
        // what the whole rows leave of the window is under the list
        card->Child(gp::Div(cx->a)->FlexCol()->W(gp::kFill)->Flex1()->MinH(0)->Child(list));
    } else {
        card->Child(list);
    }

    auto button = [&](Str id, Str label, int cmdId, bool enabled) {
        gpc::Button* b = gpc::Button::New(cx, GpuiDup(cx->a, id))
                             ->Label(GpuiDup(cx->a, label))
                             ->WithSize(gp::UiSize::Small)
                             ->Disabled(!enabled);
        if (enabled) {
            b->OnClick(gp::ListenTo(gAnnotFilterView, &AnnotFilterView::OnCmd, (intptr_t)cmdId));
        }
        gp::El* el = b->IntoEl()->W(gp::kFill);
        if (ownWindow) {
            el->H(kBtnDy)->Shrink0();
        }
        return el;
    };
    TempStr base = tab ? path::GetBaseNameTemp(tab->filePath) : TempStr{};
    Str saveLabel = Tr("Save changes to existing PDF");
    if (len(base) > 0) {
        saveLabel = fmt(Tr("Save changes to %s").s, base);
    }
    // orig's UpdateFloatButtons
    int nSel = DeleteCount(f);
    Str delLabel = Tr("Delete Annotation");
    if (nSel > 1) {
        delLabel = fmt(Tr("Delete %d annotations").s, nSel);
    }
    card->Child(button(StrL("annot-del"), delLabel, CmdDeleteAnnotation, nSel > 0));
    card->Child(button(StrL("annot-discard"), Tr("Discard changes"), CmdDiscardChanges, dirty));
    card->Child(button(StrL("annot-save"), saveLabel, CmdSaveAnnotations, dirty));
    card->Child(button(StrL("annot-save-new"), Tr("Save changes to a new PDF"), CmdSaveAnnotationsNewFile, dirty));
    card->Child(BuildFilterHelp(cx, muted, fontScale));

    if (f->wantFocus) {
        // orig's edit has selectAllOnFocus
        f->wantFocus = false;
        gp::InputFocus(f->edit, cx->app, cx->win);
        gp::InputSelectAll(f->edit, cx->app, cx->win);
    }
    if (ownWindow) {
        return card;
    }
    // centered in the frame, like the other floating cards in this port
    gp::El* overlay =
        gp::Div(cx->a)->Absolute()->Left(0)->Top(0)->W(ws.dipW)->H(ws.dipH)->FlexRow()->ItemsCenter()->JustifyCenter();
    overlay->Child(card);
    return overlay;
}

// --- -dbg-control ---------------------------------------------------------------

constexpr int kTestModCtrl = 1;
constexpr int kTestModShift = 2;
constexpr int kTestModAlt = 4;

// ng: not orig's: what a script cannot post as input. `click` / `dblclick`
// press row `arg`, `key` sends the virtual key `arg` to whichever of the
// filter and the list has the keyboard, `delete` is the Delete button and
// `scroll` sets the scroll position; mods is a kTestMod* mask
void AnnotFilterTestAction(MainWindow* win, Str action, int arg, int mods) {
    AnnotFilterToolbar* f = win ? win->annotFilterToolbar : nullptr;
    if (!f || !f->visible) {
        return;
    }
    bool ctrl = (mods & kTestModCtrl) != 0;
    bool shift = (mods & kTestModShift) != 0;
    bool alt = (mods & kTestModAlt) != 0;
    if (str::EqI(action, StrL("click")) || str::EqI(action, StrL("dblclick"))) {
        FocusList(f);
        ListRowClick(f, arg, ctrl, shift, str::EqI(action, StrL("click")) ? 1 : 2);
    } else if (str::EqI(action, StrL("key"))) {
        AnnotListFocus focus = FilterHasHostFocus(f) ? AnnotListFocus::Filter : AnnotListFocus::List;
        AnnotFilterKey(f, arg, ctrl, shift, alt, focus);
    } else if (str::EqI(action, StrL("delete"))) {
        DeleteListSelection(f);
    } else if (str::EqI(action, StrL("scroll"))) {
        ScrollTo(f, (float)arg);
    }
    AppShellInvalidate(win);
}

TempStr AnnotFilterToolbarStateTemp(MainWindow* win) {
    AnnotFilterToolbar* f = win ? win->annotFilterToolbar : nullptr;
    bool visible = f && f->visible;
    int nAll = f ? len(f->annotations) : 0;
    int nVisible = f ? len(f->visibleAnnots) : 0;
    int sel = f ? f->sel : -1;
    Rect wr;
    if (visible && f->tw) {
        wr = ToolWindowRect(f->tw);
    }
    str::Builder out;
    out.Append(fmt("annotFilter floatVisible=%d floatRect=%d,%d,%d,%d nAll=%d nVisible=%d sel=%d filter=%s\n",
                   visible ? 1 : 0, wr.x, wr.y, wr.dx, wr.dy, nAll, nVisible, sel, f ? f->filterText : Str{}));
    if (!f) {
        return ToStrTemp(out);
    }
    WindowTab* tab = FilterTab(f);
    int dirty = tab && tab->AsFixed() && EngineHasUnsavedAnnotations(tab->AsFixed()->GetEngine()) ? 1 : 0;
    int pageSel = tab && tab->selectedAnnotation ? VecFind(f->visibleAnnots, tab->selectedAnnotation) : -1;
    // ng: the first four are orig's; it also reports pixel rectangles
    out.Append(
        fmt("deleteEnabled=%d discardEnabled=%d saveEnabled=%d nSel=%d deleteCount=%d anchor=%d scrollY=%d "
            "viewRows=%d listFocused=%d filterFocused=%d pending=%d pageSel=%d\n",
            DeleteCount(f) > 0 ? 1 : 0, dirty, dirty, SelectedCount(f), DeleteCount(f), f->anchor, (int)f->scrollY,
            f->viewRows, f->listFocused ? 1 : 0, IsFilterFocused(f) ? 1 : 0, f->selectPendingMs >= 0 ? 1 : 0, pageSel));
    for (int i = 0; i < nVisible; i++) {
        out.Append(fmt("annot=%d sel=%d page=%d %s\n", i, IsSelected(f, i) ? 1 : 0, f->visibleAnnots[i]->pageNo,
                       AnnotationListRowTextTemp(f->visibleAnnots[i])));
    }
    return ToStrTemp(out);
}
