/* Copyright 2024 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/WinDynCalls.h"
#include "base/Win.h"
#include "base/Dpi.h"

#include "base/Pixmap.h"

#include "wingui/UIModels.h"
#include "wingui/Layout.h"
#include "wingui/WinGui.h"
#include "wingui/PlatformFont.h"
#include "wingui/Gfx.h"
#include "wingui/VirtWnd.h"

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

// command ids for the bar's toolbar buttons; must not collide with real commands
constexpr int kFindBarCloseCmdId = (int)CmdLast + 50;
constexpr int kFindBarPinCmdId = (int)CmdLast + 52;

struct FindBarWnd : WindowBase {
    MainWindow* win = nullptr;
    // the status text and the buttons are virtual controls; the search field is
    // the only HWND child
    Edit* edit = nullptr;
    VirtText* status = nullptr;
    // prev / next / match-case / match-whole-word / pop-out / close
    VirtIconButton* btns[6]{};
    Tooltip* tooltip = nullptr;

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
    // one button's size, padding included
    Size ButtonSize() const;
    // the button under `pt` (window coords), or -1
    int ButtonIndexFromPoint(Point pt);
    // forceBarDx > 0: fit the bar into exactly that window width, giving the
    // slack to the edit box. 0: the default edit width.
    void Layout(int forceBarDx = 0);
    // width of everything in the bar except the edit box
    int FixedDx() const;
    int MinBarDx() const;

    void OnTextChanged();

    void WndProc(WindowBase::WndProcEvent* ev);
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
    delete edit;
    delete tooltip;
    delete status;
    for (VirtIconButton* b : btns) {
        delete b;
    }
}

// the icons come from the shared cache, which renders them for the current
// theme and size
void FindBarWnd::UpdateButtonIcons() {
    static const TbIcon icons[6] = {TbIcon::ChevronUp,      TbIcon::ChevronDown,    TbIcon::MatchCase,
                                    TbIcon::MatchWholeWord, TbIcon::ArrowsDiagonal, TbIcon::Close};
    int isz = RoundUp(DpiScale(hwnd, 16), 4);
    for (int i = 0; i < 6; i++) {
        if (btns[i]) {
            btns[i]->pixmap = GetPixmapForIcon(icons[i], isz, isz);
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
    COLORREF colBg = ThemeWindowControlBackgroundColor();
    int pad = DpiScale(hwnd, 4);
    for (int i = 0; i < 6; i++) {
        auto* b = new VirtIconButton();
        b->id = cmds[i];
        b->padding = Insets{pad, pad, pad, pad};
        b->bgColorHover = AccentColor(colBg, 20);
        b->bgColorSelected = AccentColor(colBg, 36);
        b->SetTooltip(FindBarButtonTooltip(cmds[i]));
        b->onClick = MkFunc1(FindBarButtonClicked, this);
        btns[i] = b;
    }
    UpdateButtonIcons();
}

Size FindBarWnd::ButtonSize() const {
    if (!btns[0]) {
        return {};
    }
    Size sz = btns[0]->GetIdealSize();
    sz.dx += btns[0]->padding.left + btns[0]->padding.right;
    sz.dy += btns[0]->padding.top + btns[0]->padding.bottom;
    return sz;
}

int FindBarWnd::ButtonIndexFromPoint(Point pt) {
    for (int i = 0; i < 6; i++) {
        if (btns[i] && btns[i]->BoundsInWindow().Contains(pt)) {
            return i;
        }
    }
    return -1;
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
        args.font = GetAppFont(hwnd);
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

    // ellipsis: single line, vertically centered, so it lines up with the
    // (taller, bordered) edit box's text instead of sitting at the top
    status = NewVirtText({
        .font = GetPlatformFont(GetAppFont(hwnd)),
        .textColor = colTxt,
        .isRtl = IsUIRtl(),
        .ellipsis = true,
    });

    CreateButtons();

    {
        Tooltip::CreateArgs targs;
        targs.parent = hwnd;
        tooltip = new Tooltip();
        tooltip->Create(targs);
    }

    // the virtual controls of this window; it positions them itself in Layout()
    vroot = new VirtRoot(hwnd);
    Vec<VirtWnd*> tops;
    tops.Append(status);
    for (VirtIconButton* b : btns) {
        tops.Append(b);
    }
    vroot->SetTops(tops);

    ApplyDarkModeToPopupWindow(hwnd);
    Layout();
    return true;
}

constexpr int kFindBarPadding = 6;
constexpr int kFindBarGap = 4;
constexpr int kFindBarStatusDx = 88;
constexpr int kFindBarDefaultEditDx = 220;
constexpr int kFindBarMinEditDx = 80;
// how wide the drag zone along the left edge is
constexpr int kFindBarResizeGripDx = 6;

// left padding + gap + status + gap + toolbar + right padding
int FindBarWnd::FixedDx() const {
    Size btnSz = ButtonSize();
    return (2 * DpiScale(hwnd, kFindBarPadding)) + (2 * DpiScale(hwnd, kFindBarGap)) +
           DpiScale(hwnd, kFindBarStatusDx) + (btnSz.dx * 6);
}

int FindBarWnd::MinBarDx() const {
    return DpiScale(hwnd, kFindBarMinEditDx) + FixedDx();
}

void FindBarWnd::Layout(int forceBarDx) {
    // WM_SIZE can arrive from CreateCustom, before the controls exist
    if (!edit || !status || !btns[0]) {
        return;
    }
    int p = DpiScale(hwnd, kFindBarPadding);
    int gap = DpiScale(hwnd, kFindBarGap);
    int statusDx = DpiScale(hwnd, kFindBarStatusDx);

    int editDy = edit->GetIdealSize().dy;

    Size btnSz = ButtonSize();
    Size tbSz = {btnSz.dx * 6, btnSz.dy};

    int innerDy = std::max(editDy, tbSz.dy);
    barDy = innerDy + (2 * p);

    int fixedDx = FixedDx();
    int editDx;
    if (forceBarDx > 0) {
        // resizing: the edit box absorbs the change
        editDx = std::max(forceBarDx - fixedDx, DpiScale(hwnd, kFindBarMinEditDx));
    } else {
        editDx = DpiScale(hwnd, kFindBarDefaultEditDx);
    }
    barDx = editDx + fixedDx;

    if (vroot) {
        vroot->SetBounds({0, 0, barDx, barDy});
    }
    int x = p;
    MoveWindow(edit->hwnd, x, (barDy - editDy) / 2, editDx, editDy, TRUE);
    x += editDx + gap;
    status->SetBounds({x, (barDy - editDy) / 2, statusDx, editDy});
    x += statusDx + gap;
    int btnY = (barDy - tbSz.dy) / 2;
    for (VirtIconButton* b : btns) {
        b->SetBounds({x, btnY, btnSz.dx, btnSz.dy});
        x += btnSz.dx;
    }
    HwndInvalidate(hwnd);

    inLayout = true;
    SetWindowPos(hwnd, nullptr, 0, 0, barDx, barDy, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    inLayout = false;
}

void FindBarWnd::OnTextChanged() {
    if (suppressTextChanged) {
        return;
    }
    OnFindBarTextChanged(win);
}

void FindBarWnd::WndProc(WindowBase::WndProcEvent* ev) {
    HWND h = ev->hwnd;
    UINT msg = ev->msg;
    LPARAM lp = ev->lparam;
    // The bar is pinned to the right edge of the frame, so only its left edge
    // can be dragged; report that edge as a sizing border and the default
    // handling turns a drag there into a resize.
    if (msg == WM_NCHITTEST) {
        Rect wr = HwndWindowRect(h);
        int x = GET_X_LPARAM(lp);
        if (x < wr.x + DpiScale(h, kFindBarResizeGripDx)) {
            ev->result = HTLEFT;
            ev->didHandle = true;
            return;
        }
    }
    if (msg == WM_SIZE) {
        // ignore the WM_SIZE our own SetWindowPos in Layout() generates
        if (!inLayout) {
            // GetWindowRect, not LOWORD(lp): lp is the client size, while barDx
            // is a window size
            Rect wr = HwndWindowRect(h);
            Layout(wr.dx);
        }
        ev->result = 0;
        ev->didHandle = true;
        return;
    }
    if (msg == WM_GETMINMAXINFO && barDy > 0) {
        // don't let the edit box be dragged away entirely, and keep the height
        // fixed: it comes from the font metrics, not from the user
        auto* mmi = (MINMAXINFO*)lp;
        mmi->ptMinTrackSize.x = MinBarDx();
        mmi->ptMinTrackSize.y = barDy;
        mmi->ptMaxTrackSize.y = barDy;
        ev->result = 0;
        ev->didHandle = true;
        return;
    }
    if (msg == WM_SETCURSOR) {
        // the buttons are virtual controls, so their tooltips are ours to show
        Point pt = HwndGetCursorPos(h);
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
// owns win->hwndFindEdit. Shown via Ctrl+F or the toolbar search icon.
FindBarWnd* CreateFindBar(MainWindow* win) {
    auto* bar = new FindBarWnd();
    bar->onCommand = MkMethod1<FindBarWnd, WindowBase::CommandEvent*, &FindBarWnd::OnCommand>(bar);
    bar->onWndProc = MkMethod1<FindBarWnd, WindowBase::WndProcEvent*, &FindBarWnd::WndProc>(bar);
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
    bool wasVisible = HwndIsVisible(win->findBar->hwnd);
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
    if (!win->hwndFindEdit) {
        return;
    }
    HwndSetFocus(win->hwndFindEdit);
    Edit_SetSel(win->hwndFindEdit, 0, -1);
}

// switch the find UI between the compact toolbar overlay and the floating
// window (persists the choice in gGlobalPrefs->searchUIFloating)
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
        win->findBar->status->Invalidate();
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
