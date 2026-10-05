/* Copyright 2024 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// ng: orig's FindBar.cpp - the Chrome-style compact find bar and the
// dispatcher that picks between it and the floating window (FindWindow.cpp)
// on SearchUIFloating. orig builds the bar as a WS_POPUP window owned by the
// frame, laid out with HBox / Padding and drawn with virtual controls; here it
// is one gpui card, absolutely positioned at the right edge of the frame the
// way orig's PositionFindBar places the popup. Same contents and order: find
// box, "n / m" status, previous, next, match case, match whole word, pop out,
// close.

#include "gui/GpuiBridge.h"

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
#include "ShortcutParse.h"
#include "Accelerators.h"
#include "Translations.h"
#include "SumatraPDF.h"
#include "MainWindow.h"
#include "WindowTab.h"
#include "gui/AppShell.h"
#include "gui/DialogWidgets.h"
#include "SearchAndDDE.h"
#include "Toolbar.h"
#include "SvgIcons.h"
#include "FindBar.h"
#include "FindWindow.h"

#include "SumatraLog.h"

struct FindBarView;

// ng: one view entity per window (step 10b)
struct FindBarUI {
    gp::Entity<FindBarView> view;
    // orig's search field is an editable drop-down with the find history
    DialogSelect ddHistory;
    int historyLen = -1;
    Str historyFirst; // owned
    gp::Bounds cardBounds{};
    // where in the grip the drag started, so the edge does not jump to the cursor
    float gripGrabDx = 0;
};

FindBar::~FindBar() {
    str::Free(status);
    if (ui) {
        ui->ddHistory.Free();
        str::Free(ui->historyFirst);
    }
    delete ui;
}

// --- the find box -----------------------------------------------------------

static gp::App* WinApp(MainWindow* win) {
    return win && win->gpuiWin ? win->gpuiWin->app : nullptr;
}

gp::InputState* EnsureFindEdit(MainWindow* win) {
    if (win->findEdit) {
        return win->findEdit;
    }
    auto* s = new gp::InputState();
    s->focus = gp::FocusHandleNew(WinApp(win));
    gp::InputSetPlaceholder(s, ToGpui(Tr("Find")));
    win->findEdit = s;
    return s;
}

TempStr FindEditTextTemp(MainWindow* win) {
    if (!win || !win->findEdit) {
        return {};
    }
    return str::DupTemp(FromGpui(gp::InputValue(win->findEdit)));
}

int FindEditTextLen(MainWindow* win) {
    if (!win || !win->findEdit) {
        return 0;
    }
    return gp::InputValue(win->findEdit).len;
}

void FindEditSetText(MainWindow* win, Str s) {
    gp::InputState* edit = EnsureFindEdit(win);
    if (str::Eq(FromGpui(gp::InputValue(edit)), s)) {
        return;
    }
    gp::InputSetValue(edit, ToGpui(s));
    // win32's WM_SETTEXT clears the edit's modify flag, and InputSetValue()
    // emits no change event, so the notification orig gets is sent here
    FindEditSetModified(win, false);
    OnFindBarTextChanged(win);
}

bool FindEditIsModified(MainWindow* win) {
    return win && win->findBar && win->findBar->editModified;
}

void FindEditSetModified(MainWindow* win, bool modified) {
    if (win && win->findBar) {
        win->findBar->editModified = modified;
    }
}

bool IsFindEditFocused(MainWindow* win) {
    gp::Window* host = FindWindowHostGpui(win);
    if (!win || !win->findEdit || !host || !FindWindowHasKeyboard(win)) {
        return false;
    }
    return gp::FocusHandleIsFocused(host, win->findEdit->focus);
}

// focus the find edit and select all text (Ctrl+F when find UI is already open)
void FocusFindEditSelectAll(MainWindow* win) {
    if (!win || !win->findBar) {
        return;
    }
    win->findBar->wantFocus = true;
    win->findBar->wantSelectAll = true;
    AppShellInvalidate(win);
}

// --- public API -------------------------------------------------------------

void DeleteFindBar(MainWindow* win) {
    delete win->findBar;
    win->findBar = nullptr;
    delete win->findEdit;
    win->findEdit = nullptr;
}

// the compact overlay half of ShowFindBar
static void ShowCompactBar(MainWindow* win) {
    TempStr term = CurrentFindTermTemp(win);
    if (!win->findBar) {
        win->findBar = new FindBar();
    }
    EnsureFindEdit(win);
    if (len(term) > 0 && FindEditTextLen(win) == 0) {
        // the restored term is only a starting point, not a search request:
        // hitting Ctrl+F must not re-run the last search behind the user's back
        gp::InputSetValue(win->findEdit, ToGpui(term));
    }
    win->findBar->visible = true;
    win->findBar->wantFocus = true;
    win->findBar->wantSelectAll = true;
    AppShellInvalidate(win);
}

// "ShowFindBar" is the entry point used by FindFirst / Ctrl+F; it shows
// whichever find UI the user has chosen (compact overlay or floating window)
void ShowFindBar(MainWindow* win) {
    if (gSettings->searchUIFloating) {
        ShowFindWindow(win);
        return;
    }
    ShowCompactBar(win);
}

void HideFindBar(MainWindow* win) {
    win->searchStartMarked = false;
    // drop the cached results: they belong to this search/document and must not
    // be shown or navigated into after the find UI is reopened (e.g. on another
    // tab, which would carry the previous document's page/glyph coordinates)
    ClearFindMatches(win);
    if (win->ctrl) {
        // remove in-page find highlights in a chm / markdown webview
        win->ctrl->FindClear();
    }
    // drop the active TextSearch hit so closing find clears the highlight;
    // F3 still works (FindNext re-searches) and paints the new hit (#5802)
    if (DisplayModel* dm = win->AsFixed()) {
        if (dm->textSearch) {
            dm->textSearch->Reset();
        }
    }
    if (IsFindWindowVisible(win)) {
        HideFindWindow(win);
        return;
    }
    if (!win->findBar || !win->findBar->visible) {
        return;
    }
    AbortFinding(win, true);
    win->findBar->visible = false;
    win->findBar->wantFocus = false;
    if (win->findEdit && win->gpuiWin) {
        gp::InputBlur(win->findEdit, win->gpuiWin->app, win->gpuiWin);
    }
    AppShellInvalidate(win);
}

bool IsFindBarVisible(MainWindow* win) {
    return win && win->findBar && win->findBar->visible;
}

// true if either the compact bar or the floating find window is visible
bool IsFindUIVisible(MainWindow* win) {
    return IsFindBarVisible(win) || IsFindWindowVisible(win);
}

// switch the find UI between the compact overlay and the floating window
// (persists the choice in gSettings->searchUIFloating). orig carries the text,
// the selection and the page range across for every window that has find open.
void ToggleFloatingFindUI(MainWindow* win) {
    struct FindUiSwitchState {
        MainWindow* win = nullptr;
        Str text;
        Str pages;
        bool hasText = false;
    };
    Vec<FindUiSwitchState> states;
    for (MainWindow* w : gWindows) {
        if (!IsFindUIVisible(w)) {
            continue;
        }
        FindUiSwitchState state;
        state.win = w;
        if (w->findEdit) {
            state.hasText = true;
            state.text = str::Dup(FindEditTextTemp(w));
        }
        if (w->findPagesEdit) {
            state.pages = str::Dup(FromGpui(gp::InputValue(w->findPagesEdit)));
        }
        VecAppend(states, state);
    }

    for (FindUiSwitchState& state : states) {
        HideFindBar(state.win); // dispatches: hides whichever find UI is up
    }

    gSettings->searchUIFloating = !gSettings->searchUIFloating;
    ScheduleSaveSettings();

    auto restore = [](FindUiSwitchState& state) {
        MainWindow* w = state.win;
        ShowFindBar(w); // shows the now-active UI
        if (state.hasText && w->findEdit) {
            FindEditSetText(w, state.text); // restores the text, re-runs the search
        }
        if (len(state.pages) > 0 && w->findPagesEdit) {
            gp::InputSetValue(w->findPagesEdit, ToGpui(state.pages));
        }
    };
    // Restore the initiating window last so switching another window does not
    // steal focus from it.
    for (FindUiSwitchState& state : states) {
        if (state.win != win) {
            restore(state);
        }
    }
    for (FindUiSwitchState& state : states) {
        if (state.win == win) {
            restore(state);
        }
        str::Free(state.text);
        str::Free(state.pages);
    }
}

// the current document may not support find (e.g. switched to an image-only
// doc); don't leave an orphaned, inert bar floating
void FindBarReposition(MainWindow* win) {
    if (!IsFindBarVisible(win)) {
        return;
    }
    if (!NeedsFindUI(win)) {
        HideFindBar(win);
    }
}

// show n/m or "No matches" style status in the bar
void FindBarSetStatus(MainWindow* win, Str s, int totalHits) {
    if (IsFindWindowVisible(win)) {
        FindWindowSetStatus(win, s, totalHits);
        return;
    }
    FindBar* bar = win->findBar;
    if (!bar) {
        return;
    }
    Str text = s ? s : StrL("");
    bar->statusCapped = str::EndsWith(text, StrL("+"));
    if (totalHits >= 0) {
        bar->statusTotalHits = totalHits;
    }
    str::ReplaceWithCopy(&bar->status, text);
    AppShellInvalidate(win);
}

// --- the gpui card ----------------------------------------------------------

constexpr int kFindBarPadding = 6;
constexpr int kFindBarGap = 4;
constexpr int kFindBarDefaultEditDx = 220;
constexpr int kFindBarMinEditDx = 80;
// how wide the drag zone along the left edge is
constexpr int kFindBarResizeGripDx = 6;

struct FindBarView {
    MainWindow* win = nullptr;

    static void OnInput(FindBarView* self, gp::Ctx* cx, const gp::InputEvent* ev);
    static void OnCmd(FindBarView* self, gp::Ctx* cx, const gp::ClickEvent*, int64_t cmdId);
    static void OnCmdUp(FindBarView* self, gp::Ctx* cx, const gp::MouseUpEvent* ev, int64_t cmdId);
    static void OnGripDown(FindBarView* self, gp::Ctx* cx, const gp::MouseDownEvent* ev);
    static void OnGripDrag(FindBarView* self, gp::Ctx* cx, const gp::DragMoveEvent* ev);
};

static int FindBarEditDx(FindBar* bar) {
    return bar->editDx > 0 ? bar->editDx : DpiScale(kFindBarDefaultEditDx);
}

void FindBarView::OnGripDown(FindBarView* self, gp::Ctx*, const gp::MouseDownEvent* ev) {
    MainWindow* win = self->win;
    if (!IsMainWindowValidAndNotClosing(win) || !win->findBar || !win->findBar->ui) {
        return;
    }
    FindBarUI* ui = win->findBar->ui;
    ui->gripGrabDx = ev->x - ui->cardBounds.x;
}

// orig's OnNcHitTest / OnGetMinMaxInfo: the bar is pinned to the right edge of
// the frame, so dragging its left edge resizes the search field, never under
// kFindBarMinEditDx and never past the frame's left edge
void FindBarView::OnGripDrag(FindBarView* self, gp::Ctx* cx, const gp::DragMoveEvent* ev) {
    MainWindow* win = self->win;
    if (!IsMainWindowValidAndNotClosing(win) || !win->findBar || !win->findBar->ui) {
        return;
    }
    FindBar* bar = win->findBar;
    FindBarUI* ui = bar->ui;
    if (ui->cardBounds.w <= 0) {
        return;
    }
    int currDx = FindBarEditDx(bar);
    float left = ev->event.x - ui->gripGrabDx;
    int dx = currDx + (int)(ui->cardBounds.x - left);
    int maxDx = currDx + (int)ui->cardBounds.x;
    dx = limitValue(dx, DpiScale(kFindBarMinEditDx), std::max(DpiScale(kFindBarMinEditDx), maxDx));
    if (dx == currDx) {
        return;
    }
    bar->editDx = dx;
    AppShellInvalidate(win);
    gp::Notify(cx);
}

// the bar's toolbar buttons are the same commands orig's buttons send
constexpr int kFindBarCloseCmdId = (int)CmdLast + 50;
// the pop-out button: switches to the floating find window
constexpr int kFindBarPinCmdId = (int)CmdLast + 51;

void FindBarView::OnInput(FindBarView* self, gp::Ctx* cx, const gp::InputEvent* ev) {
    MainWindow* win = self->win;
    if (!IsMainWindowValidAndNotClosing(win)) {
        return;
    }
    if (ev->kind == gp::InputEventKind::PressEnter) {
        // Enter forces a pending debounced search to start now (find the first
        // match) instead of advancing to the next one (issue #4626)
        if (!FindFlushPendingSearch(win)) {
            if (ev->shift) {
                FindPrev(win);
            } else {
                FindNext(win);
            }
        }
        gp::Notify(cx);
        return;
    }
    if (ev->kind != gp::InputEventKind::Change) {
        return;
    }
    FindEditSetModified(win, true);
    OnFindBarTextChanged(win);
    gp::Notify(cx);
}

void FindBarView::OnCmdUp(FindBarView* self, gp::Ctx* cx, const gp::MouseUpEvent* ev, int64_t cmdId) {
    if (ev->button != gp::MouseButton::Right && ev->button != gp::MouseButton::Middle) {
        return;
    }
    gp::ClickEvent click;
    click.button = ev->button;
    OnCmd(self, cx, &click, cmdId);
}

void FindBarView::OnCmd(FindBarView* self, gp::Ctx* cx, const gp::ClickEvent*, int64_t cmdId) {
    MainWindow* win = self->win;
    if (!IsMainWindowValidAndNotClosing(win)) {
        return;
    }
    switch ((int)cmdId) {
        case CmdFindPrev:
            FindPrev(win);
            break;
        case CmdFindNext:
            FindNext(win);
            break;
        case CmdFindToggleMatchCase:
            FindToggleMatchCase(win);
            break;
        case CmdFindToggleMatchWholeWord:
            FindToggleMatchWholeWord(win);
            break;
        case kFindBarPinCmdId:
            ToggleFloatingFindUI(win);
            break;
        case kFindBarCloseCmdId:
            HideFindBar(win);
            break;
        default:
            break;
    }
    gp::Notify(cx);
}

static int DecimalDigits(int n) {
    int digits = 1;
    while (n >= 10) {
        n /= 10;
        digits++;
    }
    return digits;
}

// width of the "n / m" status slot, wide enough for the largest count it will
// show, so the text field next to it doesn't move on every count update
static float FindStatusDx(int totalHits, bool capped) {
    int digits = DecimalDigits(std::max(totalHits, 0));
    int nChars = (2 * digits) + 3; // N, " / ", M
    if (capped) {
        nChars++; // the trailing '+' in e.g. "999 / 999+"
    }
    return (float)nChars * 7.f;
}

// tooltip text for the bar's buttons: "Find Next (F3)"
static TempStr AppendCmdAccel(Str base, int cmd) {
    TempStr accel = AppendAccelKeyToMenuStringTemp({}, cmd);
    if (len(accel) == 0) {
        return str::DupTemp(base);
    }
    return str::JoinTemp(base, fmt(" (%s)", Str(accel.s + 1, len(accel) - 1))); // +1 skips the leading \t
}

static gp::El* BarButton(FindBar* bar, gp::Ctx* cx, Str id, gp::IconName icon, Str label, Str tooltip, int cmdId,
                         bool selected, const char* svg = nullptr) {
    gpc::Button* b = gpc::Button::New(cx, GpuiDup(cx->a, id))
                         ->Ghost()
                         ->Compact()
                         ->WithSize(gp::UiSize::Small)
                         ->Selected(selected)
                         ->Tooltip(GpuiDup(cx->a, tooltip))
                         ->OnClick(gp::ListenTo(bar->ui->view, &FindBarView::OnCmd, (intptr_t)cmdId));
    if (svg) {
        b->Icon(gpc::ButtonIcon::New(cx, gpc::Icon::Empty(cx)->Data(ToGpui(Str(svg)))));
    } else if (icon != gp::IconName::None) {
        b->Icon(icon);
    } else {
        b->Label(GpuiDup(cx->a, label));
    }
    // orig's VirtButton fires for any mouse button
    return b->IntoEl()->OnMouseUp(gp::ListenTo(bar->ui->view, &FindBarView::OnCmdUp, (intptr_t)cmdId));
}

gp::El* FindBarBuild(MainWindow* win, gp::Ctx* cx) {
    FindBar* bar = win->findBar;
    if (!bar || !bar->visible) {
        return nullptr;
    }
    if (!bar->ui) {
        bar->ui = new FindBarUI();
    }
    if (!bar->ui->view.IsValid()) {
        bar->ui->view = gp::EntityNewState<FindBarView>(cx->app);
    }
    auto* view = (FindBarView*)gp::EntityGet(cx->app, bar->ui->view.id);
    view->win = win;

    gp::InputState* edit = EnsureFindEdit(win);
    edit->onChange = gp::ListenTo(bar->ui->view, &FindBarView::OnInput);

    const gp::Theme& th = gp::ThemeNow(cx->app);
    float pad = (float)DpiScale(kFindBarPadding);
    float gap = (float)DpiScale(kFindBarGap);
    gp::El* card =
        gp::Div(cx->a)->FlexRow()->ItemsCenter()->Gap(gap)->Pad(pad)->Bg(th.tokens.popover)->Border(1, th.border);

    // orig's ApplyFindHistory
    FindBarUI* ui = bar->ui;
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
    // picking an entry out of the open history list is a search request;
    // walking the list with the arrow keys only fills the box, and waits for
    // Enter
    if (ui->ddHistory.TakeComboListPick()) {
        FindEditSetModified(win, true);
        OnFindBarTextChanged(win);
        FindFlushPendingSearch(win);
    }
    ui->ddHistory.TakeComboPicked();
    card->Child(ui->ddHistory.BuildCombo(cx, StrL("find-edit"), edit, (float)FindBarEditDx(bar)));

    float statusDx = FindStatusDx(bar->statusTotalHits, bar->statusCapped);
    card->Child(gp::Div(cx->a)->MinW(statusDx)->Child(
        gp::TextEl(cx->a, GpuiDup(cx->a, bar->status))->Font(12)->Fg(th.mutedFg)));

    card->Child(BarButton(bar, cx, StrL("find-prev"), gp::IconName::ChevronUp, {},
                          AppendCmdAccel(Tr("Find Previous"), CmdFindPrev), CmdFindPrev, false));
    card->Child(BarButton(bar, cx, StrL("find-next"), gp::IconName::ChevronDown, {},
                          AppendCmdAccel(Tr("Find Next"), CmdFindNext), CmdFindNext, false));
    card->Child(BarButton(bar, cx, StrL("find-case"), gp::IconName::None, {},
                          AppendCmdAccel(Tr("Match Case"), CmdFindToggleMatchCase), CmdFindToggleMatchCase,
                          win->findMatchCase, gIconMatchCase));
    card->Child(BarButton(bar, cx, StrL("find-word"), gp::IconName::None, {},
                          AppendCmdAccel(Tr("Match Whole Word"), CmdFindToggleMatchWholeWord),
                          CmdFindToggleMatchWholeWord, win->findMatchWholeWord, gIconMatchWholeWord));
    card->Child(BarButton(bar, cx, StrL("find-pin"), gp::IconName::ExternalLink, {}, Tr("Open in a window"),
                          kFindBarPinCmdId, false));
    card->Child(
        BarButton(bar, cx, StrL("find-close"), gp::IconName::Close, {}, Tr("Close"), kFindBarCloseCmdId, false));

    // orig's PositionFindBar: the right edge of the frame's client area, and
    // vertically centered on the toolbar's search button when there is one
    float top = (float)win->canvasRc.y + pad;
    Rect btn = GetToolbarButtonRect(win, CmdFindFirst);
    if (!btn.IsEmpty()) {
        // the bar is one row of small controls plus its padding
        float barDy = (float)DpiScale(24) + (2 * pad);
        top = (float)btn.y + ((float)btn.dy / 2) - (barDy / 2);
    }
    card->Absolute()->Right(pad)->Top(top)->BoundsOut(&bar->ui->cardBounds);

    // orig's HTLEFT strip: the left edge is a sizing border
    gp::El* grip = gp::Div(cx->a)
                       ->W((float)DpiScale(kFindBarResizeGripDx))
                       ->H(gp::kFill)
                       ->Cursor(gp::CursorKind::ResizeLeftRight)
                       ->PathClick(GStrL("find-grip"))
                       ->OnMouseDown(gp::ListenTo(bar->ui->view, &FindBarView::OnGripDown))
                       ->OnDrag(GStrL("sumatra-find-grip"))
                       ->OnDragMove(gp::ListenTo(bar->ui->view, &FindBarView::OnGripDrag));
    grip->Absolute()->Left(0)->Top(0);
    card->Child(grip);

    if (bar->wantFocus) {
        bar->wantFocus = false;
        gp::InputFocus(edit, cx->app, cx->win);
        if (bar->wantSelectAll) {
            bar->wantSelectAll = false;
            gp::InputSelectAll(edit, cx->app, cx->win);
        }
    }
    return card;
}
