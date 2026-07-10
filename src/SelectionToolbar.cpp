/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/ScopedWin.h"
#include "base/Win.h"
#include "base/Dpi.h"
#include "base/GdiPlus.h"

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
#include "Selection.h"
#include "Commands.h"
#include "CommandAvailability.h"
#include "AppSettings.h"
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
    bool enabled = true;
    Rect rc; // position within the toolbar client area
};

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
    Rect lastPlaced;    // last screen rect we moved the window to (avoids redundant SetWindowPos)
    Rect lastSelBounds; // last canvas-space selection bounds used for placement
    SelectionToolbarButton buttons[8];
    int nButtons = 0;
};

// candidate buttons; per-window visibility/enabled state comes from
// GetCommandVisibility (hidden buttons are dropped, disabled ones grayed)
static const SelectionToolbarButton gCandidateButtons[] = {
    {CmdCopySelection, "Copy"},
    {CmdReadAloudSelection, "Read Aloud"},
    {CmdCreateAnnotHighlight, "Highlight"},
    {CmdCreateAnnotUnderline, "Underline"},
    {CmdCreateAnnotSquiggly, "Squiggly"},
    {CmdCreateAnnotStrikeOut, "Strike Out"},
};

static void InitButtons(SelectionToolbar* tb, MainWindow* win) {
    AppCommandCtx ctx = NewAppCommandCtx(win);
    int i = 0;
    for (const SelectionToolbarButton& cand : gCandidateButtons) {
        CommandVisibility v = GetCommandVisibility(cand.cmdId, ctx, CommandSurface::Toolbar);
        if (CommandShouldRemove(v)) {
            continue;
        }
        tb->buttons[i] = cand;
        tb->buttons[i].enabled = !CommandShouldDisable(v);
        i++;
    }
    tb->nButtons = i;
}

constexpr int kBtnPadX = 8; // horizontal padding inside a button
constexpr int kBtnPadY = 4; // vertical padding inside a button
constexpr int kMargin = 5;  // margin around the row of buttons
constexpr int kBtnGap = 2;  // gap between buttons
constexpr int kCornerRadius = 10;
constexpr int kButtonRadius = 6;
constexpr int kToolbarFontPct = 108;

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

// clip the popup window to a rounded rect so the corners don't show as
// opaque squares over the document
static void UpdateToolbarWindowRgn(HWND hwnd, int cornerRadius) {
    Rect card = ClientRect(hwnd);
    if (card.dx < 1) {
        card.dx = 1;
    }
    if (card.dy < 1) {
        card.dy = 1;
    }
    int radius = DpiScale(hwnd, cornerRadius);
    HRGN rgn = CreateRoundRectRgn(card.x, card.y, card.x + card.dx + 1, card.y + card.dy + 1, radius, radius);
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

static void LayoutToolbar(SelectionToolbar* tb) {
    HWND hwnd = tb->hwnd;
    int padX = DpiScale(hwnd, kBtnPadX);
    int padY = DpiScale(hwnd, kBtnPadY);
    int margin = DpiScale(hwnd, kMargin);
    int gap = DpiScale(hwnd, kBtnGap);

    int x = margin;
    int maxDy = 0;
    int n = tb->nButtons;
    for (int i = 0; i < n; i++) {
        SelectionToolbarButton& b = tb->buttons[i];
        Size s = HwndMeasureText(hwnd, _TRA(b.label), tb->font);
        int dx = s.dx + 2 * padX;
        int dy = s.dy + 2 * padY;
        b.rc = Rect(x, margin, dx, dy);
        x += dx + gap;
        if (dy > maxDy) {
            maxDy = dy;
        }
    }
    if (n > 0) {
        x -= gap;
    }
    for (int i = 0; i < n; i++) {
        tb->buttons[i].rc.dy = maxDy;
    }
    tb->size = Size(x + margin, maxDy + 2 * margin);
    UpdateToolbarWindowRgn(hwnd, kCornerRadius);
}

static int ButtonFromPoint(SelectionToolbar* tb, int x, int y) {
    Point pt(x, y);
    for (int i = 0; i < tb->nButtons; i++) {
        if (tb->buttons[i].rc.Contains(pt)) {
            return i;
        }
    }
    return -1;
}

static void PaintToolbar(SelectionToolbar* tb, HDC hdc) {
    HWND hwnd = tb->hwnd;
    Rect rc = ClientRect(hwnd);
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
    for (int i = 0; i < tb->nButtons; i++) {
        SelectionToolbarButton& b = tb->buttons[i];
        bool isHot = b.enabled && (i == tb->hotIndex);
        if (isHot) {
            FillRoundedRect(hdc, b.rc, btnRadius, hoverBg);
        }
        SetTextColor(hdc, b.enabled ? textCol : mutedCol);
        DrawCenteredText(hdc, b.rc, _TRA(b.label));
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
            if (idx >= 0 && idx == pressed && tb->buttons[idx].enabled) {
                int cmdId = tb->buttons[idx].cmdId;
                MainWindow* win = tb->win;
                HideSelectionToolbar(win);
                HwndPostCommand(win->hwndFrame, cmdId);
            }
            return 0;
        }

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            PaintToolbar(tb, hdc);
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

// prefer above the selection, fall back to below; always clamp to the canvas
static void PositionToolbar(SelectionToolbar* tb, const Rect& sel) {
    MainWindow* win = tb->win;
    Rect canvas = win->canvasRc;
    int gap = DpiScale(tb->hwnd, 6);
    int w = tb->size.dx;
    int h = tb->size.dy;

    int x = sel.x + sel.dx / 2 - w / 2;
    int y = sel.y - gap - h;
    if (y < canvas.y) {
        y = sel.y + sel.dy + gap;
    }

    int maxX = canvas.x + canvas.dx - w;
    if (x > maxX) {
        x = maxX;
    }
    if (x < canvas.x) {
        x = canvas.x;
    }
    int maxY = canvas.y + canvas.dy - h;
    if (y > maxY) {
        y = maxY;
    }
    if (y < canvas.y) {
        y = canvas.y;
    }

    POINT p{x, y};
    ClientToScreen(win->hwndCanvas, &p);
    Rect placed(p.x, p.y, w, h);
    if (placed == tb->lastPlaced) {
        return;
    }
    tb->lastPlaced = placed;
    SetWindowPos(tb->hwnd, nullptr, p.x, p.y, w, h, SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_NOZORDER);
}

static SelectionToolbar* GetOrCreateToolbar(MainWindow* win) {
    if (win->selectionToolbar) {
        return win->selectionToolbar;
    }
    RegisterSelectionToolbarClass();
    auto tb = new SelectionToolbar();
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
void ShowSelectionToolbar(MainWindow* win) {
    if (!win || !gGlobalPrefs->annotations.selectionToolbar) {
        return;
    }
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
    tb->lastSelBounds = sel;
    InitButtons(tb, win);
    if (tb->nButtons == 0) {
        return;
    }
    LayoutToolbar(tb);
    PositionToolbar(tb, sel);
    ShowWindow(tb->hwnd, SW_SHOWNOACTIVATE);
    HwndScheduleRepaint(tb->hwnd);
}

// Reposition the toolbar so it keeps following the selection (called from the
// canvas paint routine). Hides it if the selection scrolled out of view or the
// current tab changed; re-shows it after e.g. a repaint restored the selection.
void UpdateSelectionToolbarPosition(MainWindow* win) {
    if (!win) {
        return;
    }
    SelectionToolbar* tb = win->selectionToolbar;
    if (!tb || !tb->hwnd || !IsWindowVisible(tb->hwnd)) {
        if (win->showSelection) {
            ShowSelectionToolbar(win);
        }
        return;
    }
    if (win->CurrentTab() != tb->tab) {
        HideSelectionToolbar(win);
        return;
    }
    Rect sel;
    if (!GetSelectionBounds(win, sel)) {
        HideSelectionToolbar(win);
        return;
    }
    // the canvas repaints often (e.g. read-aloud highlight updates); skip all
    // toolbar work when the selection has not moved, otherwise SetWindowRgn /
    // ScheduleRepaint makes the floating bar jitter
    if (sel == tb->lastSelBounds) {
        return;
    }
    tb->lastSelBounds = sel;

    Rect prevPlaced = tb->lastPlaced;
    InitButtons(tb, win);
    LayoutToolbar(tb);
    PositionToolbar(tb, sel);
    if (tb->lastPlaced != prevPlaced) {
        HwndScheduleRepaint(tb->hwnd);
    }
}

// Hide the toolbar but keep the window around for reuse.
void HideSelectionToolbar(MainWindow* win) {
    SelectionToolbar* tb = win ? win->selectionToolbar : nullptr;
    if (!tb || !tb->hwnd) {
        return;
    }
    if (IsWindowVisible(tb->hwnd)) {
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
