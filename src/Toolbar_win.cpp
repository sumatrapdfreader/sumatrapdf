/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// The toolbar's window: its window class and message loop, its creation and
// destruction, the native edit control for the page number, and everything that
// manipulates window handles. The toolbar's content and rules (which buttons
// exist, when they are enabled, what they look like) are in Toolbar.cpp.

#include "base/Base.h"
#include "gui/Dpi.h"
#include "base/Win.h"
#include "base/BitManip.h"
#include "base/Pixmap.h"

#include "gui/UIModels.h"

#include "Accelerators.h"
#include "Settings.h"
#include "AppSettings.h"
#include "DocController.h"
#include "EngineBase.h"
#include "base/GuessFileType.h"
#include "EngineAll.h"
#include "DisplayModel.h"
#include "GlobalPrefs.h"
#include "ProgressUpdateUI.h"
#include "TextSelection.h"
#include "TextSearch.h"
#include "SumatraPDF.h"
#include "MainWindow.h"
#include "WindowTab.h"
#include "Commands.h"
#include "AppTools.h"
#include "CommandAvailability.h"
#include "Menu.h"
#include "SearchAndDDE.h"
#include "Toolbar.h"
#include "ToolbarInternal.h"
#include "Tabs.h"
#include "gui/Layout.h"
#include "gui/win/WinGui.h"
#include "gui/PlatformFont.h"
#include "gui/Gfx.h"
#include "gui/VirtCtrl.h"
#include "gui/win/TabsCtrl.h"
#include "FindBar.h"
#include "Translations.h"
#include "SvgIcons.h"
#include "Theme.h"
#include "TextToSpeech.h"

static const WStr kVirtToolbarClass = WStrL(L"SUMATRA_VIRT_TOOLBAR");

void RelayoutToolbar(MainWindow* win) {
    ToolbarVirt* tb = win->toolbarVirt;
    if (!tb || !tb->layout || !win->hwndToolbar) {
        return;
    }
    Rect rc = HwndClientRect(win->hwndToolbar);
    if (rc.dx <= 0 || rc.dy <= 0) {
        return;
    }
    LayoutTreeToSize(win->hwndToolbar, tb->layout, rc.Size(), &tb->vroot);
}

// natural width of the toolbar content (buttons + page box); the find bar
// floats separately so the page-total label is the rightmost element
static int ToolbarNaturalWidth(MainWindow* win) {
    ToolbarVirt* tb = win->toolbarVirt;
    if (!tb || !tb->layout) {
        return 0;
    }
    int dx = tb->layout->MinIntrinsicWidth(tb->rowDy);
    if (dx <= 0) {
        dx = tb->rowDy * 8;
    }
    return dx + DpiScale(12);
}

// canvas rectangle in frame-client coordinates
static Rect CanvasRectInFrame(MainWindow* win) {
    Rect rc = HwndWindowRect(win->hwndCanvas);
    Point tl = HwndScreenToClient(win->hwndFrame, rc.TL());
    return {tl, rc.Size()};
}

// when the overlay toolbar sits at the bottom, lift it above the horizontal
// scrollbar so it doesn't cover it. The height is reserved even when the
// scrollbar isn't currently visible, so the toolbar's position is stable.
static int OverlayToolbarBottomScrollbarOffset() {
    if (ScrollbarsAreHidden()) {
        return 0;
    }
    if (ScrollbarsUseOverlay()) {
        // smart/overlay: the thick overlay scrollbar height (see OverlayScrollbarCreate)
        return DpiScale(16);
    }
    // windows native horizontal scrollbar
    return DpiGetSystemMetrics(SM_CYHSCROLL);
}

// rectangle (frame-client coords) the overlay toolbar occupies when shown
static Rect OverlayToolbarRect(MainWindow* win) {
    Rect canvas = CanvasRectInFrame(win);
    int natW = ToolbarNaturalWidth(win);
    if (natW <= 0 || natW > canvas.dx) {
        natW = canvas.dx;
    }
    int h = HwndWindowRect(win->hwndToolbar).dy;
    int x = canvas.x + ((canvas.dx - natW) / 2);
    int y = canvas.y;
    if (ToolbarAtBottom()) {
        y = canvas.y + canvas.dy - h - OverlayToolbarBottomScrollbarOffset();
    }
    return {x, y, natW, h};
}

// position/show the floating overlay toolbar; called on relayout and mouse move
void PositionOverlayToolbar(MainWindow* win) {
    if (!win->isToolbarOverlay || !win->hwndToolbar) {
        return;
    }
    Rect r = OverlayToolbarRect(win);
    UINT flags = SWP_NOACTIVATE;
    flags |= win->toolbarOverlayShown ? SWP_SHOWWINDOW : SWP_HIDEWINDOW;
    SetWindowPos(win->hwndToolbar, HWND_TOP, r.x, r.y, r.dx, r.dy, flags);
    if (!win->toolbarOverlayShown) {
        // repaint the canvas area the toolbar was covering
        HwndInvalidate(win->hwndCanvas);
        HwndInvalidateRect(win->hwndFrame, r, false);
    }
}

// whether the cursor is currently in the reveal band or over the toolbar
static bool OverlayToolbarShouldShowForCursor(MainWindow* win) {
    Point pt = GetCursorPosition();
    Point ptFrame = HwndScreenToClient(win->hwndFrame, pt);

    Rect tb = OverlayToolbarRect(win);
    // reveal band: spans the full canvas width so the toolbar also appears when
    // the mouse is to the left or right of it, and extends a bit past the
    // toolbar (toward the page) so it shows before the cursor reaches it
    Rect canvas = CanvasRectInFrame(win);
    int my = DpiScale(16);
    int bandY = ToolbarAtBottom() ? (tb.y - my) : tb.y;
    Rect band(canvas.x, bandY, canvas.dx, tb.dy + my);
    bool inBand = band.Contains(Point(ptFrame.x, ptFrame.y));

    // also keep shown while the cursor is over the toolbar window itself
    HWND hwndUnder = HwndWindowFromPoint(pt);
    bool overToolbar = hwndUnder && (hwndUnder == win->hwndToolbar || IsChild(win->hwndToolbar, hwndUnder));
    return inBand || overToolbar;
}

// the overlay toolbar must not vanish while it owns the keyboard focus (e.g.
// the user is typing a page number into the page box after Ctrl+G)
static bool OverlayToolbarHasFocus(MainWindow* win) {
    if (!win->hwndToolbar) {
        return false;
    }
    HWND focus = GetFocus();
    if (!focus) {
        return false;
    }
    return focus == win->hwndToolbar || IsChild(win->hwndToolbar, focus);
}

static void CancelOverlayHide(MainWindow* win) {
    if (win->toolbarOverlayHidePending) {
        KillTimer(win->hwndFrame, kHideOverlayToolbarTimerId);
        win->toolbarOverlayHidePending = false;
    }
}

static void ScheduleOverlayHide(MainWindow* win) {
    if (win->toolbarOverlayHidePending) {
        return; // already scheduled; don't keep pushing it out on every move
    }
    win->toolbarOverlayHidePending = true;
    SetTimer(win->hwndFrame, kHideOverlayToolbarTimerId, kDelayToolbarHide, nullptr);
}

static void SetOverlayShown(MainWindow* win, bool shown) {
    if (shown == win->toolbarOverlayShown) {
        return;
    }
    win->toolbarOverlayShown = shown;
    PositionOverlayToolbar(win);
}

// re-evaluate overlay toolbar visibility based on the cursor's screen position
void UpdateOverlayToolbarForMouse(MainWindow* win) {
    if (!win->isToolbarOverlay || !win->hwndToolbar) {
        return;
    }
    bool show = OverlayToolbarShouldShowForCursor(win) || OverlayToolbarHasFocus(win);
    if (show) {
        CancelOverlayHide(win);
        SetOverlayShown(win, true);
    } else if (win->toolbarOverlayShown) {
        // don't hide immediately; give the user kDelayToolbarHide to come back
        ScheduleOverlayHide(win);
    }
}

// reveal the overlay toolbar right now, without waiting for the cursor to enter
// the reveal band. Used by commands that drive the toolbar from the keyboard
// (Ctrl+G): the toolbar stays up while it has the focus and auto-hides once the
// focus and the cursor are away from it.
void RevealOverlayToolbar(MainWindow* win) {
    if (!win->isToolbarOverlay || !win->hwndToolbar) {
        return;
    }
    CancelOverlayHide(win);
    SetOverlayShown(win, true);
}

// handle the delayed-hide timer firing (kHideOverlayToolbarTimerId)
void OverlayToolbarHideTimerFired(MainWindow* win) {
    win->toolbarOverlayHidePending = false;
    KillTimer(win->hwndFrame, kHideOverlayToolbarTimerId);
    if (!win->isToolbarOverlay) {
        return;
    }
    // if the cursor came back near the top while the timer was pending, keep
    // the toolbar shown; otherwise hide it now
    if (OverlayToolbarShouldShowForCursor(win) || OverlayToolbarHasFocus(win)) {
        SetOverlayShown(win, true);
    } else {
        SetOverlayShown(win, false);
    }
}

void ShowOrHideToolbar(MainWindow* win) {
    bool show = ShouldShowToolbar(win);
    bool overlay = ShouldOverlayToolbar(win);
    if (show == win->isToolbarVisible && overlay == win->isToolbarOverlay) {
        return;
    }
    bool enteredOverlay = overlay && !win->isToolbarOverlay;
    win->isToolbarVisible = show;
    win->isToolbarOverlay = overlay;
    if (!overlay) {
        CancelOverlayHide(win);
        win->toolbarOverlayShown = false;
    }
    if (enteredOverlay) {
        // reveal immediately on entering overlay mode (e.g. via F8) so the
        // change is visible; it auto-hides after kDelayToolbarHide
        win->toolbarOverlayShown = true;
    }
    if (!show && !overlay) {
        // Move the focus out of the toolbar
        if ((win->findEdit && win->findEdit->IsFocused()) || (win->pageEdit && win->pageEdit->IsFocused())) {
            HwndSetFocus(win->hwndFrame);
        }
    }
    ScheduleUiUpdate(win);
    if (enteredOverlay) {
        ScheduleOverlayHide(win);
    }
}

void UpdateFindbox(MainWindow* win) {
    HwndInvalidate(win->hwndToolbar, true);
    if (HwndIsVisible(win->hwndFrame)) {
        UpdateWindow(win->hwndToolbar);
    }

    // no document: the find edit does nothing, so don't offer a text cursor
    LPWSTR cursorId = win->IsDocLoaded() ? nullptr : IDC_ARROW;
    if (win->findEdit) {
        win->findEdit->SetCursorId(cursorId);
    }
}

static void OnPageEditChar(MainWindow* win, Edit::CharEvent* ev) {
    if (!win || !win->IsDocLoaded() || !win->pageEdit) {
        return;
    }
    switch (ev->c) {
        case VK_RETURN: {
            TempStr s = win->pageEdit->GetTextTemp();
            int newPageNo = win->ctrl->GetPageByLabel(s);
            if (win->ctrl->ValidPageNo(newPageNo)) {
                win->ctrl->GoToPage(newPageNo, true);
                HwndSetFocus(win->hwndFrame);
                // the overlay toolbar was kept up by the focus; now that
                // it's gone, let it hide again
                UpdateOverlayToolbarForMouse(win);
            }
            ev->didHandle = true;
            return;
        }
        case VK_ESCAPE:
            HwndSetFocus(win->hwndFrame);
            UpdateOverlayToolbarForMouse(win);
            ev->didHandle = true;
            return;
        case VK_TAB:
            AdvanceFocus(win);
            ev->didHandle = true;
            return;
    }
}

static int PageEditPadL() {
    return DpiGetSystemMetrics(SM_CXEDGE);
}

static int PageEditPadR() {
    return PageEditPadL() + DpiScale(4);
}

Edit* ToolbarCreatePageEdit(MainWindow* win, PlatformFont* font, int iconDy) {
    Edit::CreateArgs args;
    args.parent = win->hwndToolbar;
    args.font = font;
    args.isRtl = IsUIRtl();
    // no WS_EX_CLIENTEDGE: a themed edit draws a blue bottom accent (Win11)
    args.withFrame = true;
    args.noTheme = true;
    args.numbersOnly = true;
    args.alignRight = true;
    args.selectAllOnFocus = true;
    // the box is as tall as the icons, so without this the digits would sit at
    // its top instead of on the same line as "Page:" and "/ N"
    args.centerTextVert = true;
    args.text = StrL("0");
    args.marginLeft = PageEditPadL();
    args.marginRight = PageEditPadR();
    auto* e = new Edit();
    e->SetColors(TbTextColor(), ThemeWindowControlBackgroundColor());
    e->Create(args);
    e->SetIdealWidthFromText(StrL("999999"), DpiScale(24));
    e->idealDy = iconDy;
    e->onChar = MkFunc1(OnPageEditChar, win);
    return e;
}

// screen-coordinates rect of a toolbar button, used to position the FindBar.
// returns an empty rect when the toolbar isn't visible (e.g. fullscreen /
// presentation) so the caller can fall back to a different anchor.
Rect GetToolbarButtonScreenRect(MainWindow* win, int cmdId) {
    if (!win->hwndToolbar || !HwndIsVisible(win->hwndToolbar) || !win->toolbarVirt) {
        return {};
    }
    for (VirtCtrl* w : win->toolbarVirt->items) {
        if (w && w->id == cmdId && w->GetVisibility() == Visibility::Visible) {
            Rect r = w->BoundsInWindow();
            return HwndMapRectToWindow(r, win->hwndToolbar, HWND_DESKTOP);
        }
    }
    return {};
}

static bool PeekRemoveClickOnRect(HWND hwnd, UINT msgId, Rect btn) {
    MSG msg{};
    if (!PeekMessageW(&msg, hwnd, msgId, msgId, PM_NOREMOVE)) {
        return false;
    }
    Point pt = {GET_X_LPARAM(msg.lParam), GET_Y_LPARAM(msg.lParam)};
    if (!btn.Contains(pt)) {
        return false;
    }
    PeekMessageW(&msg, hwnd, msgId, msgId, PM_REMOVE);
    return true;
}

// After the popup returns, drop a pending click that landed on this button.
void ToolbarEatMenuDismissClick(MainWindow* win, int cmdId) {
    ToolbarNoteDropdownClosed();
    if (!win || !win->hwndToolbar || !win->toolbarVirt) {
        return;
    }
    Rect btn{};
    for (VirtCtrl* w : win->toolbarVirt->items) {
        if (w && w->id == cmdId && w->GetVisibility() == Visibility::Visible) {
            btn = w->BoundsInWindow();
            break;
        }
    }
    if (btn.IsEmpty()) {
        return;
    }
    HWND hwnd = win->hwndToolbar;
    while (PeekRemoveClickOnRect(hwnd, WM_LBUTTONDOWN, btn) || PeekRemoveClickOnRect(hwnd, WM_LBUTTONDBLCLK, btn)) {
        MSG up{};
        PeekMessageW(&up, hwnd, WM_LBUTTONUP, WM_LBUTTONUP, PM_REMOVE);
    }
}

static void FreeToolbarVirt(MainWindow* win) {
    ToolbarVirt* tb = win->toolbarVirt;
    if (!tb) {
        return;
    }
    delete tb->layout;
    delete tb->vroot;
    delete tb;
    win->toolbarVirt = nullptr;
}

static LRESULT CALLBACK WndProcVirtToolbar(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    MainWindow* win = FindMainWindowByHwnd(hwnd);
    ToolbarVirt* tb = win ? win->toolbarVirt : nullptr;
    Color bg = TbBgColor();

    if (msg == WM_MOUSEACTIVATE) {
        // the old Win32 toolbar did not take keyboard focus; a generic child
        // would, and then accelerators (Ctrl+W, …) never reached the frame
        HWND frame = GetAncestor(hwnd, GA_ROOT);
        if (frame && GetForegroundWindow() == frame) {
            return MA_NOACTIVATE;
        }
    }
    if (msg == WM_SIZE) {
        if (tb) {
            RelayoutToolbar(win);
        }
        return 0;
    }
    if (msg == WM_ERASEBKGND) {
        return 1;
    }
    if (msg == WM_PAINT) {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        Rect rc = HwndClientRect(hwnd);
        DoubleBuffer buffer(hwnd, rc);
        HDC memDC = buffer.GetDC();
        HdcFillRect(memDC, rc, bg);
        if (tb && tb->vroot) {
            SetBkMode(memDC, TRANSPARENT);
            GfxHdc gfx(memDC);
            tb->vroot->Paint(&gfx, rc);
        }
        if (IsCurrentThemeDefault() && !ThemeColorizeControls()) {
            HdcFillRect(memDC, {rc.x, rc.Bottom() - 1, rc.dx, 1}, TbEdgeColor());
        }
        buffer.Flush(hdc);
        EndPaint(hwnd, &ps);
        return 0;
    }
    if (msg == WM_COMMAND || msg == WM_CTLCOLOREDIT || msg == WM_CTLCOLORSTATIC) {
        LRESULT reflected = TryReflectMessages(hwnd, msg, wp, lp);
        if (reflected) {
            return reflected;
        }
    }
    if ((msg == WM_CTLCOLOREDIT || msg == WM_CTLCOLORSTATIC) && win) {
        HDC hdc = (HDC)wp;
        SetTextColor(hdc, TbTextColor());
        SetBkColor(hdc, ThemeWindowControlBackgroundColor());
        if (IsCurrentThemeDefault() && !ThemeColorizeControls() && !ThemeUsesHighContrastColors()) {
            return (LRESULT)GetStockObject(WHITE_BRUSH);
        }
        return (LRESULT)win->brControlBgColor;
    }
    if (msg == WM_LBUTTONDOWN || msg == WM_LBUTTONDBLCLK) {
        if (win && win->tabsInTitlebar) {
            Point pt = {GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
            HWND childAtPoint = ChildWindowFromPoint(hwnd, ToPOINT(pt));
            bool overChild = childAtPoint && childAtPoint != hwnd;
            VirtCtrl* hit = ToolbarItemFromPoint(win, pt);
            if (!overChild && (!hit || hit->id == 0 || hit->id == PageInfoId)) {
                HWND hwndFrame = GetAncestor(hwnd, GA_ROOT);
                if (msg == WM_LBUTTONDBLCLK) {
                    WPARAM cmd = IsZoomed(hwndFrame) ? SC_RESTORE : SC_MAXIMIZE;
                    PostMessageW(hwndFrame, WM_SYSCOMMAND, cmd, 0);
                } else {
                    ReleaseCapture();
                    SendMessageW(hwndFrame, WM_NCLBUTTONDOWN, HTCAPTION, 0);
                }
                return 0;
            }
        }
    }
    if (msg == WM_MOUSEMOVE || msg == WM_MOUSELEAVE) {
        if (win && win->isToolbarOverlay) {
            if (msg == WM_MOUSEMOVE) {
                TRACKMOUSEEVENT tme{sizeof(TRACKMOUSEEVENT), TME_LEAVE, hwnd, 0};
                TrackMouseEvent(&tme);
            }
            UpdateOverlayToolbarForMouse(win);
        }
    }
    if (tb && tb->vroot) {
        LRESULT res = 0;
        if (VirtTreeOnMessage(hwnd, tb->vroot, msg, wp, lp, res)) {
            return res;
        }
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

static void RegisterVirtToolbarClass() {
    static ATOM atom = 0;
    if (atom) {
        return;
    }
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_DBLCLKS;
    wc.lpfnWndProc = WndProcVirtToolbar;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.hCursor = GetCachedCursor(IDC_ARROW);
    wc.lpszClassName = kVirtToolbarClass.s;
    atom = RegisterClassExW(&wc);
}

void CreateToolbar(MainWindow* win) {
    bool isRtl = IsUIRtl();
    RegisterVirtToolbarClass();

    HINSTANCE hinst = GetModuleHandle(nullptr);
    HWND hwndParent = win->hwndFrame;

    // WS_CLIPSIBLINGS so that in overlay mode the canvas (a lower-Z sibling)
    // doesn't paint over the floating toolbar
    DWORD style = WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | WS_VISIBLE;
    DWORD exStyle = WS_EX_TOOLWINDOW;
    if (isRtl) {
        exStyle |= WS_EX_LAYOUTRTL;
    }

    int iconSize = ToolbarIconSize();
    int yPad = DpiScale(2);
    int rowDy = ToolbarRowDy(iconSize);

    HWND hwnd = CreateWindowExW(exStyle, kVirtToolbarClass.s, nullptr, style, 0, 0, 100, rowDy, hwndParent, nullptr,
                                hinst, nullptr);
    win->hwndToolbar = hwnd;

    auto* tb = new ToolbarVirt();
    tb->iconSize = iconSize;
    int defFontSize = GetAppFontSize();
    int newSize = defFontSize;
    int maxFontSize = iconSize - (yPad * 2) - 2;
    if (newSize > maxFontSize) {
        newSize = maxFontSize;
    }
    tb->platformFont = GetDefaultGuiFontOfSize(newSize);
    win->toolbarVirt = tb;
    HwndSetFont(hwnd, tb->platformFont->GetHFont());

    BuildToolbarLayout(win);

    DocController* ctrl = win->ctrl;
    UpdateToolbarPageText(win, ctrl ? ctrl->PageCount() : -1);
    if (ctrl && win->pageEdit) {
        TempStr label = ctrl->GetPageLabeTemp(ctrl->CurrentPageNo());
        win->pageEdit->SetText(label);
        win->pageEdit->SetNumbersOnly(!ctrl->HasPageLabels());
    }
    UpdateToolbarFindText(win);
    ToolbarUpdateStateForWindow(win, true);
}

void DestroyToolbar(MainWindow* win) {
    if (!win->hwndToolbar && !win->toolbarVirt) {
        return;
    }
    win->pageEdit = nullptr;
    FreeToolbarVirt(win);
    HwndDestroyWindowSafe(&win->hwndToolbar);
    win->hwndToolbar = nullptr;
}

void ReCreateToolbar(MainWindow* win) {
    DestroyToolbar(win);
    CreateToolbar(win);
}
