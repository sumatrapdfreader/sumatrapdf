/* Copyright 2024 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/WinDynCalls.h"
#include "base/Win.h"
#include "gui/Dpi.h"

#include "base/Pixmap.h"

#include "gui/UIModels.h"
#include "gui/Layout.h"
#include "gui/win/WinGui.h"
#include "gui/PlatformFont.h"
#include "gui/Gfx.h"
#include "gui/VirtCtrl.h"

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
#include "Translations.h"
#include "Theme.h"
#include "DarkMode_win.h"

// command ids for the bar's toolbar buttons; must not collide with real commands
constexpr int kFindBarCloseCmdId = (int)CmdLast + 50;
constexpr int kFindBarPinCmdId = (int)CmdLast + 52;

namespace {

// min-width box: at least `dx`, wider if the child needs more
struct FindMinDx : ILayout {
    ILayout* child = nullptr;
    int dx = 0;

    FindMinDx(ILayout* c, int dxIn);
    ~FindMinDx() override;

    Size Layout(Constraints bc) override;
    int MinIntrinsicHeight(int width) override;
    int MinIntrinsicWidth(int height) override;
    void SetBounds(Rect) override;
    int LayoutChildCount() override;
    ILayout* LayoutChildAt(int) override;
};

FindMinDx::FindMinDx(ILayout* c, int dxIn) {
    child = c;
    dx = dxIn;
}

FindMinDx::~FindMinDx() {
    delete child;
}

int FindMinDx::LayoutChildCount() {
    return child ? 1 : 0;
}

ILayout* FindMinDx::LayoutChildAt(int) {
    return child;
}

int FindMinDx::MinIntrinsicWidth(int height) {
    int childDx = child ? child->MinIntrinsicWidth(height) : 0;
    return std::max(dx, childDx);
}

int FindMinDx::MinIntrinsicHeight(int width) {
    return child ? child->MinIntrinsicHeight(width) : 0;
}

Size FindMinDx::Layout(const Constraints bc) {
    int w = MinIntrinsicWidth(0);
    if (bc.min.dx > w) {
        w = bc.min.dx;
    }
    if (bc.HasBoundedWidth() && bc.max.dx < w) {
        w = bc.max.dx;
    }
    Size s = child ? child->Layout(bc.TightenWidth(w)) : Size{};
    return {w, s.dy};
}

void FindMinDx::SetBounds(Rect r) {
    lastBounds = r;
    if (child) {
        child->SetBounds(r);
    }
}

} // namespace

struct FindBarWnd : WindowBase {
    MainWindow* win = nullptr;
    // the status text and the buttons are virtual controls; the search field is
    // the only HWND child. Owned by `layout` once BuildLayout() runs
    Edit* edit = nullptr;
    VirtText* status = nullptr;
    // prev / next / match-case / match-whole-word / pop-out / close
    VirtIconButton* btns[6]{};

    int barDx = 0;
    int barDy = 0;
    // when set, programmatic edits to the text don't kick off a search
    // (used while restoring text during a theme-change recreate)
    bool suppressTextChanged = false;
    // set while Layout() runs so the WM_SIZE its own SetWindowPos generates
    // doesn't re-enter Layout()
    bool inLayout = false;

    FindBarWnd() = default;
    ~FindBarWnd() override;

    bool Create(MainWindow* win);
    void CreateButtons();
    void UpdateButtonIcons();
    void BuildLayout();
    // forceBarDx > 0: fit the bar into exactly that window width, giving the
    // slack to the edit box. 0: the default edit width.
    void Layout(int forceBarDx = 0);
    int MinBarDx() const;

    void OnTextChanged();

    void OnSize(WindowBase::SizeEvent* ev);
    void OnGetMinMaxInfo(WindowBase::GetMinMaxInfoEvent* ev);
    void OnNcHitTest(WindowBase::NcHitTestEvent* ev);
    void OnKeyDown(KeyEvent* ev);
    void OnCommand(WindowBase::CommandEvent* ev);
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
    // edit, status and buttons are owned by `layout` (deleted in ~WindowBase)
}

// the icons come from the shared cache, which renders them for the current
// theme and size
void FindBarWnd::UpdateButtonIcons() {
    static const char* icons[6] = {gIconChevronUp,      gIconChevronDown,    gIconMatchCase,
                                   gIconMatchWholeWord, gIconArrowsDiagonal, gIconClose};
    int isz = RoundUp(DpiScale(16), 4);
    for (int i = 0; i < 6; i++) {
        if (btns[i]) {
            btns[i]->pixmap = GetCachedPixmapForSvg(icons[i], isz, isz);
        }
    }
}

static void FindBarButtonClicked(FindBarWnd* bar, VirtMouseEvent* ev) {
    auto* btn = (VirtIconButton*)ev->target;
    WindowBase::CommandEvent ce;
    ce.w = bar;
    ce.wparam = (WPARAM)btn->id;
    bar->OnCommand(&ce);
}

void FindBarWnd::CreateButtons() {
    static const int cmds[6] = {
        CmdFindPrev,      CmdFindNext,       CmdFindToggleMatchCase, CmdFindToggleMatchWholeWord,
        kFindBarPinCmdId, kFindBarCloseCmdId};
    int pad = DpiScale(4);
    for (int i = 0; i < 6; i++) {
        auto* b = new VirtIconButton();
        b->id = cmds[i];
        b->padding = Insets{pad, pad, pad, pad};
        b->SetTooltip(FindBarButtonTooltip(cmds[i]));
        b->onClick = MkFunc1(FindBarButtonClicked, this);
        btns[i] = b;
    }
    UpdateButtonIcons();
}

bool FindBarWnd::Create(MainWindow* mainWin) {
    win = mainWin;
    // Layout() sizes the HWND to content; don't let WM_SIZE DoLayout first
    autoLayout = false;

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
        args.font = GetAppFont();
        args.isMultiLine = false;
        args.withBorder = true;
        args.cueText = _TRA("Find");
        args.isRtl = IsUIRtl();
        edit = new Edit();
        edit->SetColors(colTxt, colBg);
        edit->Create(args);
        edit->onTextChanged = MkMethod0<FindBarWnd, &FindBarWnd::OnTextChanged>(this);
        if (!win->findEdit) {
            // don't steal it from the floating find window: this can run while
            // that one is up (RecreateFindBar on a theme change)
            win->findEdit = edit;
        }
    }

    // ellipsis: single line, vertically centered, so it lines up with the
    // (taller, bordered) edit box's text instead of sitting at the top
    status = NewVirtText({
        .font = GetAppFont(),
        .isRtl = IsUIRtl(),
        .ellipsis = true,
    });

    CreateButtons();

    BuildLayout();

    DarkModeApplyToPopupWindow(hwnd);
    Layout();
    return true;
}

constexpr int kFindBarPadding = 6;
constexpr int kFindBarGap = 4;
constexpr int kFindBarDefaultEditDx = 220;
constexpr int kFindBarMinEditDx = 80;
// how wide the drag zone along the left edge is
constexpr int kFindBarResizeGripDx = 6;

void FindBarWnd::BuildLayout() {
    int p = DpiScale(kFindBarPadding);
    int gap = DpiScale(kFindBarGap);
    // cap preferred width at the min so HBox flex, not the typed text, sets the
    // edit's size (a long query would otherwise blow out the bar)
    int minEditDx = DpiScale(kFindBarMinEditDx);
    edit->idealDx = minEditDx;
    edit->maxDx = minEditDx;

    auto* row = new HBox();
    row->alignCross = CrossAxisAlign::CrossCenter;
    row->AddChild(edit, 1);
    row->AddChild(new Spacer(gap, 0));
    int statusMinDx = PlatformFontMeasureText(status->font, StrL("1 / 999")).dx;
    row->AddChild(new FindMinDx(status, statusMinDx));
    row->AddChild(new Spacer(gap, 0));
    for (VirtIconButton* b : btns) {
        row->AddChild(b);
    }
    layout = new Padding(row, Insets{p, p, p, p});
}

int FindBarWnd::MinBarDx() const {
    if (!layout) {
        return 0;
    }
    int client = layout->MinIntrinsicWidth(0);
    Rect wr = HwndWindowRect(hwnd);
    Rect cr = HwndClientRect(hwnd);
    return client + (wr.dx - cr.dx);
}

void FindBarWnd::Layout(int forceBarDx) {
    // WM_SIZE can arrive from CreateCustom, before the tree exists
    if (!layout) {
        return;
    }
    if (forceBarDx > 0) {
        DoLayout();
    } else {
        int extra = DpiScale(kFindBarDefaultEditDx - kFindBarMinEditDx);
        int minDx = layout->MinIntrinsicWidth(0) + extra;
        inLayout = true;
        LayoutAndSizeToContent(layout, minDx, 0, hwnd);
        DoLayout(HwndClientRect(hwnd).Size());
        inLayout = false;
    }
    Rect wr = HwndWindowRect(hwnd);
    barDx = wr.dx;
    barDy = wr.dy;
    HwndInvalidate(hwnd);
}

void FindBarWnd::OnTextChanged() {
    if (suppressTextChanged) {
        return;
    }
    OnFindBarTextChanged(win);
}

void FindBarWnd::OnSize(WindowBase::SizeEvent* ev) {
    if (ev->msg != WM_SIZE) {
        return;
    }
    // ignore the WM_SIZE our own SetWindowPos in Layout() generates
    if (!inLayout) {
        // window size (barDx), not client size from the size event
        Rect wr = HwndWindowRect(hwnd);
        Layout(wr.dx);
    }
}

void FindBarWnd::OnGetMinMaxInfo(WindowBase::GetMinMaxInfoEvent* ev) {
    if (barDy <= 0) {
        return;
    }
    // don't let the edit box be dragged away entirely, and keep the height
    // fixed: it comes from the font metrics, not from the user
    auto* mmi = ev->mmi;
    mmi->ptMinTrackSize.x = MinBarDx();
    mmi->ptMinTrackSize.y = barDy;
    mmi->ptMaxTrackSize.y = barDy;
}

// The bar is pinned to the right edge of the frame, so only its left edge
// can be dragged; report that edge as a sizing border and the default
// handling turns a drag there into a resize.
void FindBarWnd::OnNcHitTest(WindowBase::NcHitTestEvent* ev) {
    Rect wr = HwndWindowRect(hwnd);
    if (ev->screenPos.x < wr.x + DpiScale(kFindBarResizeGripDx)) {
        ev->result = HTLEFT;
        ev->didHandle = true;
    }
}

void FindBarWnd::OnKeyDown(KeyEvent* ev) {
    // the find edit lives in this owned popup, not as a child of the frame, so
    // the frame's edit accelerator table doesn't reach it; handle the find keys
    // here (Esc, Enter/Shift+Enter, F3/Shift+F3)
    switch (ev->vkey) {
        case 'F':
            if (ev->isCtrl && !ev->isAlt) {
                FocusFindEditSelectAll(win);
                ev->didHandle = true;
            }
            break;
        case VK_ESCAPE:
            HideFindBar(win);
            ev->didHandle = true;
            break;
        case VK_RETURN:
        case VK_F3:
            // Enter forces a pending debounced search to start now (find the
            // first match) instead of advancing to the next one (issue #4626)
            if (ev->vkey == VK_RETURN && FindFlushPendingSearch(win)) {
                ev->didHandle = true;
                break;
            }
            if (ev->isShift) {
                FindPrev(win);
            } else {
                FindNext(win);
            }
            ev->didHandle = true;
            break;
    }
}

void FindBarWnd::OnCommand(WindowBase::CommandEvent* ev) {
    int cmd = LOWORD(ev->wparam);
    switch (cmd) {
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
            ToggleFloatingFindUI(win); // pop out into the floating window
            break;
        case kFindBarCloseCmdId:
            HideFindBar(win);
            break;
        default:
            return;
    }
    ev->didHandle = true;
}

//--- public API

// Chrome-style floating search bar. Created hidden together with the toolbar;
// owns win->findEdit. Shown via Ctrl+F or the toolbar search icon.
FindBarWnd* CreateFindBar(MainWindow* win) {
    auto* bar = new FindBarWnd();
    bar->onCommand = MkMethod1<FindBarWnd, WindowBase::CommandEvent*, &FindBarWnd::OnCommand>(bar);
    bar->onSize = MkMethod1<FindBarWnd, WindowBase::SizeEvent*, &FindBarWnd::OnSize>(bar);
    bar->onGetMinMaxInfo = MkMethod1<FindBarWnd, WindowBase::GetMinMaxInfoEvent*, &FindBarWnd::OnGetMinMaxInfo>(bar);
    bar->onNcHitTest = MkMethod1<FindBarWnd, WindowBase::NcHitTestEvent*, &FindBarWnd::OnNcHitTest>(bar);
    bar->onKeyDown = MkMethod1<FindBarWnd, KeyEvent*, &FindBarWnd::OnKeyDown>(bar);
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
    // only if this bar is the active find UI; the floating find window's edit
    // must survive us (see ShowFindWindow)
    if (win->findEdit == win->findBar->edit) {
        win->findEdit = nullptr;
    }
    delete win->findBar;
    win->findBar = nullptr;
}

// rebuild the bar so it picks up new theme colors / icons (called on theme change)
void RecreateFindBar(MainWindow* win) {
    if (!win->findBar) {
        return;
    }
    // stop any in-flight find/count that captured the old bar's state
    AbortFinding(win, true);
    bool wasVisible = HwndIsVisible(win->findBar->hwnd);
    TempStr text = wasVisible && win->findEdit ? str::DupTemp(win->findEdit->GetTextTemp()) : nullptr;
    DeleteFindBar(win);
    win->findBar = CreateFindBar(win);
    if (win->findBar && wasVisible) {
        ShowFindBar(win);
        if (len(text) > 0 && win->findEdit) {
            // restore the text without re-running the search (the existing
            // document highlight is preserved across the recreate)
            win->findBar->suppressTextChanged = true;
            win->findEdit->SetText(text);
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
    Rect fr = HwndWindowRect(win->hwndFrame);
    // Align to the right edge of the client area, not the outer window rect:
    // HwndWindowRect includes the resize border (and sits off-screen when maximized),
    // which pushed the bar a few pixels too far right (#5762).
    Rect frClient = HwndMapLtrClientRectToScreen(win->hwndFrame, HwndClientRect(win->hwndFrame));
    int cx = frClient.x + frClient.dx - bar->barDx;
    int cy;
    if (btn.IsEmpty()) {
        cy = fr.y + bar->barDy;
    } else {
        cy = btn.y + (btn.dy / 2) - (bar->barDy / 2);
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
    win->findEdit = bar->edit;    // make this the active find edit
    win->findPagesEdit = nullptr; // page range is only on the floating window
    // reflect the current match-case / whole-word state on the toggle buttons
    FindBarSetMatchCaseChecked(win, win->findMatchCase);
    FindBarSetMatchWholeWordChecked(win, win->findMatchWholeWord);
    PositionFindBar(bar);
    ShowWindow(bar->hwnd, SW_SHOW);
    win->findEdit->SetFocus();
    win->findEdit->SelectAll();
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
    if (win->ctrl) {
        // remove in-page find highlights in a chm / markdown webview
        // (no-op for other document types and the IE backend)
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
    return win->findBar && HwndIsVisible(win->findBar->hwnd);
}

// true if either the compact bar or the floating find window is visible
bool IsFindUIVisible(MainWindow* win) {
    return IsFindBarVisible(win) || IsFindWindowVisible(win);
}

// focus the find edit and select all text (Ctrl+F when find UI is already open)
void FocusFindEditSelectAll(MainWindow* win) {
    if (!win->findEdit) {
        return;
    }
    win->findEdit->SetFocus();
    win->findEdit->SelectAll();
}

// switch the find UI between the compact toolbar overlay and the floating
// window (persists the choice in gGlobalPrefs->searchUIFloating)
void ToggleFloatingFindUI(MainWindow* win) {
    TempStr text = win->findEdit ? str::DupTemp(win->findEdit->GetTextTemp()) : nullptr;
    TempStr pages = win->findPagesEdit ? str::DupTemp(win->findPagesEdit->GetTextTemp()) : nullptr;
    int selStart = 0, selEnd = 0;
    if (win->findEdit) {
        win->findEdit->GetSelection(selStart, selEnd);
    }
    bool wasShowing = IsFindBarVisible(win) || IsFindWindowVisible(win);

    HideFindBar(win); // dispatches: hides whichever find UI is currently visible

    gGlobalPrefs->searchUIFloating = !gGlobalPrefs->searchUIFloating;
    SaveSettings();

    if (!wasShowing) {
        return; // just persist the preference; nothing was open
    }
    ShowFindBar(win); // shows the now-active UI and repoints win->findEdit
    if (len(text) > 0 && win->findEdit) {
        win->findEdit->SetText(text); // restore text (re-runs the search)
    }
    if (len(pages) > 0 && win->findPagesEdit) {
        win->findPagesEdit->SetText(pages);
    }
    if (win->findEdit) {
        win->findEdit->SetFocus();
        // restore the caret/selection last, after Show/SetText reset it
        win->findEdit->SetSelection(selStart, selEnd);
    }
}

// reposition over the search toolbar icon (no-op if not visible)
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

// show n/m or "No matches" style status in the bar
void FindBarSetStatus(MainWindow* win, Str s) {
    if (gGlobalPrefs->searchUIFloating) {
        FindWindowSetStatus(win, s);
        return;
    }
    if (win->findBar && win->findBar->status) {
        win->findBar->status->SetText(s ? s : StrL(""));
        // relayout so the status is the text width (a fixed slot left a large
        // empty gap after short counts like "1 / 999+")
        if (win->findBar->barDx > 0) {
            win->findBar->Layout(win->findBar->barDx);
        } else {
            win->findBar->Layout();
        }
    }
}

// idx into FindBarWnd::btns
constexpr int kBtnMatchCase = 2;
constexpr int kBtnMatchWholeWord = 3;

static void FindBarSetBtnChecked(MainWindow* win, int idx, bool checked) {
    if (!win->findBar) {
        return;
    }
    VirtIconButton* b = win->findBar->btns[idx];
    if (!b || b->isSelected == checked) {
        return;
    }
    b->isSelected = checked;
    b->Invalidate();
}

// reflect match-case toggle state on the bar's button
void FindBarSetMatchCaseChecked(MainWindow* win, bool checked) {
    if (gGlobalPrefs->searchUIFloating) {
        FindWindowSetMatchCaseChecked(win, checked);
        return;
    }
    FindBarSetBtnChecked(win, kBtnMatchCase, checked);
}

// reflect match-whole-word toggle state on the bar's button
void FindBarSetMatchWholeWordChecked(MainWindow* win, bool checked) {
    if (gGlobalPrefs->searchUIFloating) {
        FindWindowSetMatchWholeWordChecked(win, checked);
        return;
    }
    FindBarSetBtnChecked(win, kBtnMatchWholeWord, checked);
}
