/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/ScopedWin.h"
#include "base/Win.h"
#include "base/Dpi.h"
#include "base/GdiPlusUtil.h"
#include "base/Pixmap.h"

#include "wingui/UIModels.h"

#include "Settings.h"
#include "DocController.h"
#include "EngineBase.h"
#include "GlobalPrefs.h"
#include "DisplayModel.h"
#include "TextSelection.h"
#include "SumatraPDF.h"
#include "MainWindow.h"
#include "WindowTab.h"
#include "Canvas.h"
#include "Selection.h"
#include "Commands.h"
#include "CommandAvailability.h"
#include "AppSettings.h"
#include "Toolbar.h"
#include "Translations.h"
#include "Theme.h"
#include "SelectionToolbar.h"

// A small floating toolbar shown under/over a finished text selection with
// the most common selection actions (copy, read aloud, highlight etc.).
// Ported from dengxibo/sumatrapdf-plus (db0b32b7a and follow-ups); button
// availability rewritten on top of CommandAvailability.

#define kSelectionToolbarClassName L"SumatraSelectionToolbar"

struct SelectionToolbarButton {
    int cmdId = 0;
    const char* label = nullptr; // English literal, translated via _TRA at layout/paint time
    // a selection handler's SelectToolbarNameOrSvg: user text shown as-is, or an
    // svg icon drawn instead of the text. Only one of them is set
    Str userLabel;
    Str svgIcon;
    Pixmap* icon = nullptr; // svgIcon rendered at the current size; not owned
    bool enabled = true;
    Rect rc; // position within the toolbar client area
};

// what to draw for a button that isn't an icon
static Str ButtonText(const SelectionToolbarButton& b) {
    if (b.userLabel) {
        return b.userLabel;
    }
    return _TRA(b.label);
}

struct SelectionToolbar {
    MainWindow* win = nullptr;
    WindowTab* tab = nullptr; // tab the current selection belongs to
    HWND hwnd = nullptr;
    HFONT font = nullptr;
    bool fontOwned = false;
    int hotIndex = -1;
    int pressedIndex = -1;
    bool trackingMouse = false;
    Size size;
    int iconSize = 0;   // size the cached icons were rendered at
    Rect lastPlaced;    // last screen rect we moved the window to (avoids redundant SetWindowPos)
    Rect lastSelBounds; // last canvas-space selection bounds used for placement
    DWORD lastPositionUpdateTick = 0;
    Vec<SelectionToolbarButton> buttons;
};

// candidate buttons; per-window visibility/enabled state comes from
// GetCommandVisibility (hidden buttons are dropped, disabled ones grayed)
static const SelectionToolbarButton gCandidateButtons[] = {
    {CmdCopySelection, "Copy"},
    {CmdTranslateSelection, "Translate"},
    {CmdReadAloudSelection, "Read Aloud"},
    {CmdCreateAnnotHighlight, "Highlight"},
    {CmdCreateAnnotUnderline, "Underline"},
    {CmdCreateAnnotSquiggly, "Squiggly"},
    {CmdCreateAnnotStrikeOut, "Strike Out"},
    {CmdCreateAnnotText, "Text"},
};

// selection handlers that asked for a button with SelectToolbarNameOrSvg
static void AppendSelectionHandlerButtons(SelectionToolbar* tb, const AppCommandCtx& ctx) {
    Vec<CustomCommand*> cmds;
    GetCommandsWithOrigId(cmds, CmdSelectionHandler);
    for (CustomCommand* cmd : cmds) {
        Str s = GetCommandStringArg(cmd, kCmdArgSelectToolbar, nullptr);
        if (str::IsEmptyOrWhiteSpace(s)) {
            continue;
        }
        CommandVisibility v = GetCommandVisibility(cmd->id, ctx, CommandSurface::Toolbar);
        if (CommandShouldRemove(v)) {
            continue;
        }
        SelectionToolbarButton b;
        b.cmdId = cmd->id;
        if (str::StartsWithI(s, StrL("<svg"))) {
            b.svgIcon = s;
        } else {
            b.userLabel = s;
        }
        b.enabled = !CommandShouldDisable(v);
        tb->buttons.Append(b);
    }
}

static void InitButtons(SelectionToolbar* tb, MainWindow* win) {
    AppCommandCtx ctx = NewAppCommandCtx(win);
    tb->buttons.Reset();
    for (const SelectionToolbarButton& cand : gCandidateButtons) {
        CommandVisibility v = GetCommandVisibility(cand.cmdId, ctx, CommandSurface::Toolbar);
        if (CommandShouldRemove(v)) {
            continue;
        }
        SelectionToolbarButton b = cand;
        b.enabled = !CommandShouldDisable(v);
        tb->buttons.Append(b);
    }
    AppendSelectionHandlerButtons(tb, ctx);
}

constexpr int kBtnPadX = 8; // horizontal padding inside a button
constexpr int kBtnPadY = 4; // vertical padding inside a button
constexpr int kMargin = 5;  // margin around the row of buttons
constexpr int kBtnGap = 2;  // gap between buttons
constexpr int kCornerRadius = 10;
constexpr int kButtonRadius = 6;
constexpr int kToolbarFontPct = 108;
// Throttle position moves during frequent canvas paints (plus 3229c8b2c).
constexpr DWORD kSelTbPositionUpdateMinMs = 32;

static bool IsActivelySelecting(MainWindow* win) {
    MouseAction ma = win->mouseAction;
    return ma == MouseAction::Selecting || ma == MouseAction::SelectingText;
}

static int SelectionBoundsSlack(HWND hwnd) {
    return DpiScale(hwnd, 3);
}

static bool SelectionBoundsChanged(Rect a, Rect b, int slack) {
    if (slack <= 0) {
        return a != b;
    }
    return abs(a.x - b.x) > slack || abs(a.y - b.y) > slack || abs(a.dx - b.dx) > slack || abs(a.dy - b.dy) > slack;
}

// theme-derived colors for the floating card; light mode tints the page
// render background so the card sits naturally over the document
static bool SelBarIsDark() {
    return !IsLightColor(ThemeWindowBackgroundColor());
}

static COLORREF SelBarBg() {
    if (SelBarIsDark()) {
        return ThemeWindowBackgroundColor();
    }
    COLORREF contentBg;
    ThemePageRenderColors(contentBg);
    return AccentColor(contentBg, 12);
}

static COLORREF SelBarBorderColor() {
    if (SelBarIsDark()) {
        return AccentColor(ThemeWindowControlBackgroundColor(), 35);
    }
    return AccentColor(SelBarBg(), 8);
}

static COLORREF SelBarTextColor() {
    if (SelBarIsDark()) {
        return ThemeWindowTextColor();
    }
    return RGB(27, 29, 33);
}

static COLORREF SelBarMutedTextColor() {
    if (SelBarIsDark()) {
        return ThemeWindowTextDisabledColor();
    }
    return RGB(92, 96, 104);
}

static COLORREF SelBarHoverBg(COLORREF bg) {
    if (SelBarIsDark()) {
        return AccentColor(ThemeWindowControlBackgroundColor(), 15);
    }
    return AccentColor(bg, 10);
}

static Gdiplus::Color GdipColor(COLORREF col) {
    return Gdiplus::Color(255, GetRValue(col), GetGValue(col), GetBValue(col));
}

static void AddRoundedRectPath(Gdiplus::GraphicsPath& path, const Rect& rc, int d) {
    path.AddArc(rc.x, rc.y, d, d, 180, 90);
    path.AddArc(rc.x + rc.dx - d - 1, rc.y, d, d, 270, 90);
    path.AddArc(rc.x + rc.dx - d - 1, rc.y + rc.dy - d - 1, d, d, 0, 90);
    path.AddArc(rc.x, rc.y + rc.dy - d - 1, d, d, 90, 90);
    path.CloseFigure();
}

static void FillRoundedRect(HDC hdc, const Rect& rc, int radius, COLORREF col) {
    Gdiplus::Graphics g(hdc);
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    Gdiplus::SolidBrush br(GdipColor(col));
    Gdiplus::GraphicsPath path;
    AddRoundedRectPath(path, rc, radius);
    g.FillPath(&br, &path);
}

static void StrokeRoundedRect(HDC hdc, const Rect& rc, int radius, COLORREF col) {
    Gdiplus::Graphics g(hdc);
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    Gdiplus::Pen pen(GdipColor(col), 1);
    Gdiplus::GraphicsPath path;
    AddRoundedRectPath(path, rc, radius);
    g.DrawPath(&pen, &path);
}

// Clip the popup to a rounded rect. Use the intended layout size — not
// HwndClientRect — because CreateWindow starts at 0x0 and client rect is still empty
// until after SetWindowPos (a 1x1 region left the toolbar invisible).
static void UpdateToolbarWindowRgn(HWND hwnd, int cornerRadius, int dx, int dy) {
    dx = std::max(dx, 1);
    dy = std::max(dy, 1);
    int radius = DpiScale(hwnd, cornerRadius);
    HRGN rgn = CreateRoundRectRgn(0, 0, dx + 1, dy + 1, radius, radius);
    if (!SetWindowRgn(hwnd, rgn, TRUE)) {
        DeleteObject(rgn);
    }
}

static HFONT CreateScaledFontFrom(HFONT base, int pct) {
    if (!base) {
        return nullptr;
    }
    LOGFONTW lf{};
    GetObjectW(base, sizeof(lf), &lf);
    lf.lfHeight = MulDiv(lf.lfHeight, pct, 100);
    return CreateFontIndirectW(&lf);
}

// Rendering an svg costs a mupdf context, and the bar re-lays out whenever the
// selection moves, so keep the rendered icons around. They only change when the
// size or the theme colors do
struct SelToolbarIcon {
    Str svg; // our own copy: a settings reload frees the string we were given
    int size = 0;
    COLORREF fgCol = 0;
    COLORREF bgCol = 0;
    Pixmap* pixmap = nullptr;
};

static Vec<SelToolbarIcon> gSelToolbarIcons;

static void FreeSelToolbarIcons() {
    for (SelToolbarIcon& i : gSelToolbarIcons) {
        str::Free(i.svg);
        FreePixmap(i.pixmap);
    }
    gSelToolbarIcons.Reset();
}

static Pixmap* GetSelToolbarIcon(Str svg, int size, COLORREF fgCol, COLORREF bgCol) {
    for (SelToolbarIcon& i : gSelToolbarIcons) {
        if (i.size == size && i.fgCol == fgCol && i.bgCol == bgCol && str::Eq(i.svg, svg)) {
            return i.pixmap;
        }
    }
    // a theme switch or a dpi change invalidates every entry; they're cheap to
    // re-render, so drop them all rather than track which are still wanted
    if (len(gSelToolbarIcons) >= 32) {
        FreeSelToolbarIcons();
    }
    SelToolbarIcon i;
    i.svg = str::Dup(svg);
    i.size = size;
    i.fgCol = fgCol;
    i.bgCol = bgCol;
    i.pixmap = RenderSvgIconToPixmap(svg, size, size, fgCol, bgCol);
    gSelToolbarIcons.Append(i);
    return i.pixmap;
}

static void UpdateButtonIcons(SelectionToolbar* tb, int size) {
    COLORREF fgCol = SelBarTextColor();
    COLORREF bgCol = SelBarBg();
    for (SelectionToolbarButton& b : tb->buttons) {
        if (!b.svgIcon) {
            continue;
        }
        b.icon = GetSelToolbarIcon(b.svgIcon, size, fgCol, bgCol);
    }
}

// Compute button layout and tb->size only (region applied after SetWindowPos).
static void LayoutToolbar(SelectionToolbar* tb) {
    HWND hwnd = tb->hwnd;
    int padX = DpiScale(hwnd, kBtnPadX);
    int padY = DpiScale(hwnd, kBtnPadY);
    int margin = DpiScale(hwnd, kMargin);
    int gap = DpiScale(hwnd, kBtnGap);

    int x = margin;
    int maxDy = 0;
    int n = len(tb->buttons);
    // an icon button is square, so its width isn't known until the row height
    // is; lay those out in a second pass
    int textDy = HwndMeasureText(hwnd, StrL("Mg"), tb->font).dy;
    for (int i = 0; i < n; i++) {
        SelectionToolbarButton& b = tb->buttons[i];
        int dy = textDy + (2 * padY);
        int dx = dy;
        if (!b.svgIcon) {
            dx = HwndMeasureText(hwnd, ButtonText(b), tb->font).dx + (2 * padX);
        }
        b.rc = Rect(x, margin, dx, dy);
        x += dx + gap;
        maxDy = std::max(dy, maxDy);
    }
    if (n > 0) {
        x -= gap;
    }
    int shift = 0;
    for (int i = 0; i < n; i++) {
        SelectionToolbarButton& b = tb->buttons[i];
        b.rc.x += shift;
        if (b.svgIcon && b.rc.dx != maxDy) {
            shift += maxDy - b.rc.dx;
            b.rc.dx = maxDy;
        }
        b.rc.dy = maxDy;
    }
    tb->size = Size(x + shift + margin, maxDy + (2 * margin));
    UpdateButtonIcons(tb, maxDy - (2 * padY));
}

static int ButtonFromPoint(SelectionToolbar* tb, int x, int y) {
    Point pt(x, y);
    for (int i = 0; i < len(tb->buttons); i++) {
        if (tb->buttons[i].rc.Contains(pt)) {
            return i;
        }
    }
    return -1;
}

// Sticky-note (Text) annots are placed at a canvas point; use the selection end.
// Ported from dengxibo/sumatrapdf-plus 89e4edfed.
static bool GetSelectionEndPoint(MainWindow* win, Point& out) {
    DisplayModel* dm = win->AsFixed();
    if (!dm || !dm->textSelection || dm->textSelection->result.len <= 0) {
        return false;
    }
    TextSel& result = dm->textSelection->result;
    int i = result.len - 1;
    Rect r = dm->CvtToScreen(result.pages[i], ToRectF(result.rects[i]));
    if (r.IsEmpty()) {
        return false;
    }
    out = Point(r.x + r.dx, r.y + (r.dy / 2));
    return true;
}

static void PaintToolbar(SelectionToolbar* tb, HDC hdc) {
    HWND hwnd = tb->hwnd;
    Rect rc = HwndClientRect(hwnd);
    COLORREF bgCol = SelBarBg();
    COLORREF hoverBg = SelBarHoverBg(bgCol);
    int cornerRadius = DpiScale(hwnd, kCornerRadius);
    int btnRadius = DpiScale(hwnd, kButtonRadius);

    FillRoundedRect(hdc, rc, cornerRadius, bgCol);
    StrokeRoundedRect(hdc, rc, cornerRadius, SelBarBorderColor());

    ScopedSelectObject selFont(hdc, tb->font);
    SetBkMode(hdc, TRANSPARENT);
    COLORREF textCol = SelBarTextColor();
    COLORREF mutedCol = SelBarMutedTextColor();
    for (int i = 0; i < len(tb->buttons); i++) {
        SelectionToolbarButton& b = tb->buttons[i];
        bool isHot = b.enabled && (i == tb->hotIndex);
        if (isHot) {
            FillRoundedRect(hdc, b.rc, btnRadius, hoverBg);
        }
        if (b.icon) {
            int x = b.rc.x + ((b.rc.dx - b.icon->width) / 2);
            int y = b.rc.y + ((b.rc.dy - b.icon->height) / 2);
            BlitPixmapAlpha(b.icon, hdc, {x, y, b.icon->width, b.icon->height});
            continue;
        }
        SetTextColor(hdc, b.enabled ? textCol : mutedCol);
        HdcDrawCenteredText(hdc, b.rc, ButtonText(b));
    }
}

static void TrackMouseLeave(SelectionToolbar* tb) {
    if (tb->trackingMouse) {
        return;
    }
    TRACKMOUSEEVENT tme{};
    tme.cbSize = sizeof(tme);
    tme.dwFlags = TME_LEAVE;
    tme.hwndTrack = tb->hwnd;
    TrackMouseEvent(&tme);
    tb->trackingMouse = true;
}

static LRESULT CALLBACK WndProcSelectionToolbar(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    SelectionToolbar* tb;
    if (msg == WM_NCCREATE) {
        LPCREATESTRUCT cs = reinterpret_cast<LPCREATESTRUCT>(lp);
        tb = reinterpret_cast<SelectionToolbar*>(cs->lpCreateParams);
        tb->hwnd = hwnd;
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(tb));
        return DefWindowProc(hwnd, msg, wp, lp);
    }
    tb = reinterpret_cast<SelectionToolbar*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    if (!tb) {
        return DefWindowProc(hwnd, msg, wp, lp);
    }

    switch (msg) {
        case WM_ERASEBKGND:
            return TRUE;

        // don't steal focus from the canvas, so keyboard shortcuts keep working
        case WM_MOUSEACTIVATE:
            return MA_NOACTIVATE;

        case WM_MOUSEMOVE: {
            int x = GET_X_LPARAM(lp);
            int y = GET_Y_LPARAM(lp);
            int idx = ButtonFromPoint(tb, x, y);
            if (idx >= 0 && !tb->buttons[idx].enabled) {
                idx = -1;
            }
            if (idx != tb->hotIndex) {
                tb->hotIndex = idx;
                HwndScheduleRepaint(hwnd);
            }
            TrackMouseLeave(tb);
            return 0;
        }

        case WM_MOUSELEAVE:
            tb->trackingMouse = false;
            if (tb->hotIndex != -1) {
                tb->hotIndex = -1;
                HwndScheduleRepaint(hwnd);
            }
            return 0;

        case WM_LBUTTONDOWN: {
            int x = GET_X_LPARAM(lp);
            int y = GET_Y_LPARAM(lp);
            int idx = ButtonFromPoint(tb, x, y);
            if (idx >= 0 && tb->buttons[idx].enabled) {
                tb->pressedIndex = idx;
            } else {
                tb->pressedIndex = -1;
            }
            return 0;
        }

        case WM_LBUTTONUP: {
            int x = GET_X_LPARAM(lp);
            int y = GET_Y_LPARAM(lp);
            int idx = ButtonFromPoint(tb, x, y);
            int pressed = tb->pressedIndex;
            tb->pressedIndex = -1;
            if (idx < 0 || idx != pressed || !tb->buttons[idx].enabled) {
                return 0;
            }
            int cmdId = tb->buttons[idx].cmdId;
            MainWindow* win = tb->win;
            LPARAM commandPoint = 0;
            if (cmdId == CmdCreateAnnotText) {
                Point selectionEnd;
                if (GetSelectionEndPoint(win, selectionEnd)) {
                    commandPoint = MAKELPARAM(selectionEnd.x, selectionEnd.y);
                }
                DeleteOldSelectionInfo(win, true);
            }
            HideSelectionToolbar(win);
            HwndPostCommand(win->hwndFrame, cmdId, commandPoint);
            return 0;
        }

        case WM_PAINT: {
            // Paint off-screen and blit once. Hovering a button repaints the
            // whole bar, and drawing straight to the window showed the
            // background fill wiping it before the buttons came back -- a
            // flash under the moving mouse.
            Rect rc = HwndClientRect(hwnd);
            DoubleBuffer buffer(hwnd, rc);
            PaintToolbar(tb, buffer.GetDC());
            PAINTSTRUCT ps;
            buffer.Flush(BeginPaint(hwnd, &ps));
            EndPaint(hwnd, &ps);
            return 0;
        }
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}

static void RegisterSelectionToolbarClass() {
    static bool registered = false;
    if (registered) {
        return;
    }
    WNDCLASSEX wcex{};
    FillWndClassEx(wcex, WStrL(kSelectionToolbarClassName), WndProcSelectionToolbar);
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    RegisterClassEx(&wcex);
    registered = true;
}

// union of the on-screen parts of the selection, in canvas coordinates;
// false if the selection is empty or fully scrolled out of view
static bool GetSelectionBounds(MainWindow* win, Rect& out) {
    DisplayModel* dm = win->AsFixed();
    if (!dm) {
        return false;
    }
    WindowTab* tab = win->CurrentTab();
    if (!tab || !tab->selectionOnPage) {
        return false;
    }
    Rect canvas = win->canvasRc;
    Rect bounds;
    bool first = true;
    for (SelectionOnPage& sel : *tab->selectionOnPage) {
        Rect r = sel.GetRect(dm).Intersect(canvas);
        if (r.IsEmpty()) {
            continue;
        }
        if (first) {
            bounds = r;
            first = false;
        } else {
            bounds = bounds.Union(r);
        }
    }
    if (first) {
        return false;
    }
    out = bounds;
    return true;
}

// Prefer above the selection, fall back to below; clamp to the canvas.
// Returns true if the window was moved/resized (caller may need a repaint).
static bool PositionToolbar(SelectionToolbar* tb, const Rect& sel) {
    MainWindow* win = tb->win;
    Rect canvas = win->canvasRc;
    int gap = DpiScale(tb->hwnd, 6);
    int w = tb->size.dx;
    int h = tb->size.dy;

    int x = sel.x + (sel.dx / 2) - (w / 2);
    int y = sel.y - gap - h;
    if (y < canvas.y) {
        y = sel.y + sel.dy + gap;
    }

    int maxX = canvas.x + canvas.dx - w;
    x = std::min(x, maxX);
    x = std::max(x, canvas.x);
    int maxY = canvas.y + canvas.dy - h;
    y = std::min(y, maxY);
    y = std::max(y, canvas.y);

    Point p = HwndClientToScreen(win->hwndCanvas, Point(x, y));
    Rect placed(p.x, p.y, w, h);
    if (placed == tb->lastPlaced) {
        return false;
    }
    tb->lastPlaced = placed;
    SetWindowPos(tb->hwnd, nullptr, p.x, p.y, w, h, SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_NOZORDER);
    // Region must match layout size after SetWindowPos (window may have been 0x0).
    UpdateToolbarWindowRgn(tb->hwnd, kCornerRadius, w, h);
    return true;
}

static SelectionToolbar* GetOrCreateToolbar(MainWindow* win) {
    if (win->selectionToolbar) {
        return win->selectionToolbar;
    }
    RegisterSelectionToolbarClass();
    auto* tb = new SelectionToolbar();
    tb->win = win;
    DWORD style = WS_POPUP;
    DWORD styleEx = WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE;
    HWND hwnd = CreateWindowEx(styleEx, kSelectionToolbarClassName, nullptr, style, 0, 0, 0, 0, win->hwndFrame, nullptr,
                               GetModuleHandle(nullptr), tb);
    if (!hwnd) {
        delete tb;
        return nullptr;
    }
    tb->font = CreateScaledFontFrom(GetAppFont(hwnd), kToolbarFontPct);
    tb->fontOwned = tb->font != nullptr;
    win->selectionToolbar = tb;
    return tb;
}

// Show the floating selection toolbar for the current text selection. Does
// nothing if the feature is disabled (Annotations.SelectionToolbar) or there
// is no on-screen text selection in a fixed-page document.
static void ShowSelectionToolbarNow(MainWindow* win) {
    if (!win || !gGlobalPrefs->selectionToolbar) {
        return;
    }
    // Do not check IsActivelySelecting here: OnSelectionStop schedules the show
    // while mouseAction is still SelectingText (cleared only after it returns);
    // by the time the debounce timer runs it is clear, and that is where a drag
    // that is still going gets filtered out. Hide-during-drag is handled in
    // UpdateSelectionToolbarPosition.
    DisplayModel* dm = win->AsFixed();
    if (!dm) {
        return;
    }
    if (dm->textSelection->result.len <= 0) {
        return;
    }
    Rect sel;
    if (!GetSelectionBounds(win, sel)) {
        return;
    }
    SelectionToolbar* tb = GetOrCreateToolbar(win);
    if (!tb) {
        return;
    }
    tb->tab = win->CurrentTab();
    tb->hotIndex = -1;
    tb->pressedIndex = -1;
    tb->lastPositionUpdateTick = GetTickCount();
    tb->lastSelBounds = sel;
    InitButtons(tb, win);
    if (len(tb->buttons) == 0) {
        return;
    }
    LayoutToolbar(tb);
    // Force SetWindowPos + region even if lastPlaced matched (e.g. after hide).
    tb->lastPlaced = Rect();
    PositionToolbar(tb, sel);
    ShowWindow(tb->hwnd, SW_SHOWNOACTIVATE);
    HwndScheduleRepaint(tb->hwnd);
}

// The toolbar pops up over the document, right where the user is reading, so
// showing it the instant a selection exists makes it flash in and out while
// selecting word by word or nudging the selection with the keyboard. Wait for
// the selection to settle first. Repeated requests during the wait keep the
// original deadline instead of pushing it back, so a stream of canvas repaints
// (UpdateSelectionToolbarPosition asks on every one) can't starve the timer.
void ShowSelectionToolbar(MainWindow* win) {
    if (!win || !win->hwndCanvas || !gGlobalPrefs->selectionToolbar) {
        return;
    }
    if (win->selectionToolbarShowPending) {
        return;
    }
    win->selectionToolbarShowPending = true;
    SetTimer(win->hwndCanvas, kSelectionToolbarShowTimerID, kSelectionToolbarShowDelayInMs, nullptr);
}

// fired by kSelectionToolbarShowTimerID
void SelectionToolbarOnShowTimer(MainWindow* win) {
    if (!win || !win->hwndCanvas) {
        return;
    }
    KillTimer(win->hwndCanvas, kSelectionToolbarShowTimerID);
    win->selectionToolbarShowPending = false;
    // the selection may be gone or still being dragged by now; both self-guard
    if (IsActivelySelecting(win)) {
        return;
    }
    ShowSelectionToolbarNow(win);
}

// cancel a pending debounced show (the selection went away or is being redone)
static void CancelPendingShow(MainWindow* win) {
    if (!win || !win->selectionToolbarShowPending) {
        return;
    }
    win->selectionToolbarShowPending = false;
    if (win->hwndCanvas) {
        KillTimer(win->hwndCanvas, kSelectionToolbarShowTimerID);
    }
}

// Reposition the toolbar so it keeps following the selection (called from the
// canvas paint routine). Hides it if the selection scrolled out of view or the
// current tab changed; re-shows it after e.g. a repaint restored the selection.
void UpdateSelectionToolbarPosition(MainWindow* win) {
    if (!win) {
        return;
    }
    // Hide during drag so the bar does not chase the rubber-band selection.
    if (IsActivelySelecting(win)) {
        SelectionToolbar* activeTb = win->selectionToolbar;
        if (activeTb && activeTb->hwnd && HwndIsVisible(activeTb->hwnd)) {
            HideSelectionToolbar(win);
        }
        return;
    }
    SelectionToolbar* tb = win->selectionToolbar;
    if (!tb || !tb->hwnd || !HwndIsVisible(tb->hwnd)) {
        if (win->showSelection) {
            ShowSelectionToolbar(win);
        }
        return;
    }
    if (win->CurrentTab() != tb->tab) {
        HideSelectionToolbar(win);
        if (win->showSelection) {
            ShowSelectionToolbar(win);
        }
        return;
    }
    Rect sel;
    if (!GetSelectionBounds(win, sel)) {
        HideSelectionToolbar(win);
        return;
    }
    // Canvas repaints often (e.g. read-aloud); skip work when the selection has
    // not moved, otherwise SetWindowRgn / ScheduleRepaint jitter the bar.
    int slack = SelectionBoundsSlack(win->hwndFrame);
    if (!SelectionBoundsChanged(sel, tb->lastSelBounds, slack)) {
        return;
    }
    DWORD now = GetTickCount();
    if (tb->lastPositionUpdateTick != 0 && now - tb->lastPositionUpdateTick < kSelTbPositionUpdateMinMs) {
        return;
    }
    tb->lastPositionUpdateTick = now;
    tb->lastSelBounds = sel;

    InitButtons(tb, win);
    LayoutToolbar(tb);
    if (PositionToolbar(tb, sel)) {
        HwndScheduleRepaint(tb->hwnd);
    }
}

// Hide the toolbar but keep the window around for reuse.
void HideSelectionToolbar(MainWindow* win) {
    CancelPendingShow(win);
    SelectionToolbar* tb = win ? win->selectionToolbar : nullptr;
    if (!tb || !tb->hwnd) {
        return;
    }
    if (HwndIsVisible(tb->hwnd)) {
        ShowWindow(tb->hwnd, SW_HIDE);
    }
    tb->hotIndex = -1;
    tb->pressedIndex = -1;
    tb->tab = nullptr;
    tb->lastPlaced = Rect();
    tb->lastSelBounds = Rect();
}

// Destroy the toolbar window and free its state.
void DeleteSelectionToolbar(MainWindow* win) {
    CancelPendingShow(win);
    SelectionToolbar* tb = win ? win->selectionToolbar : nullptr;
    if (!tb) {
        return;
    }
    if (tb->hwnd) {
        DestroyWindow(tb->hwnd);
    }
    if (tb->fontOwned && tb->font) {
        DeleteObject(tb->font);
    }
    delete tb;
    win->selectionToolbar = nullptr;
}
