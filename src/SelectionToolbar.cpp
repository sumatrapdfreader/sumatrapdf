/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/ScopedWin.h"
#include "base/Win.h"
#include "gui/Dpi.h"
#include "base/Pixmap.h"

#include "gui/UIModels.h"
#include "gui/Layout.h"
#include "gui/win/WinGui.h"
#include "gui/PlatformFont.h"
#include "gui/Gfx.h"
#include "gui/GuiColors.h"
#include "gui/VirtCtrl.h"
#include "gui/VirtHost.h"

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
#include "Translations.h"
#include "Theme.h"
#include "SvgIcons.h"
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
    // the popup window; owns the row of buttons and the virtual controls
    VirtHost* host = nullptr;
    PlatformFont* font = nullptr;
    Size size;
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

static int SelectionBoundsSlack() {
    return DpiScale(3);
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

static Color SelBarBg() {
    if (SelBarIsDark()) {
        return ThemeWindowBackgroundColor();
    }
    Color contentBg;
    ThemePageRenderColors(contentBg);
    return AccentColor(contentBg, 12);
}

static Color SelBarBorderColor() {
    if (SelBarIsDark()) {
        return AccentColor(ThemeWindowControlBackgroundColor(), 35);
    }
    return AccentColor(SelBarBg(), 8);
}

static Color SelBarTextColor() {
    if (SelBarIsDark()) {
        return ThemeWindowTextColor();
    }
    return MkRgb(27, 29, 33);
}

static Color SelBarMutedTextColor() {
    if (SelBarIsDark()) {
        return ThemeWindowTextDisabledColor();
    }
    return MkRgb(92, 96, 104);
}

static Color SelBarHoverBg(Color bg) {
    if (SelBarIsDark()) {
        return AccentColor(ThemeWindowControlBackgroundColor(), 15);
    }
    return AccentColor(bg, 10);
}

static void UpdateButtonIcons(SelectionToolbar* tb, int size) {
    Color fgCol = SelBarTextColor();
    Color bgCol = SelBarBg();
    for (SelectionToolbarButton& b : tb->buttons) {
        if (!b.svgIcon) {
            continue;
        }
        b.icon = GetCachedPixmapForSvg(b.svgIcon, size, size, fgCol, bgCol);
    }
}

// The buttons are pills, not rectangles: they draw their hover background with
// the same rounded corners as the card they sit on, so they get their own Paint
// the hover highlight is a rounded rect this draws itself, so the button's own
// box and border stay unpainted
struct SelToolbarTextButton : VirtButton {
    SelToolbarTextButton(Str s, PlatformFont* f) : VirtButton(s, f) {
        SetColor(kColBtnBg, kColorTransparent);
        SetColor(kColBtnBgHover, kColorTransparent);
        SetColor(kColBtnBorder, kColorTransparent);
    }
    void Paint(VirtPaintCtx& ctx) override {
        if (IsEnabled() && HasFlag(vwfHovered) && hoverBg != kColorUnset) {
            int radius = DpiScale(kButtonRadius);
            ctx.gfx->FillRoundedRect(ctx.bounds, radius, hoverBg);
        }
        VirtButton::Paint(ctx);
    }
    Color hoverBg = kColorUnset;
};

struct SelToolbarIconButton : VirtIconButton {
    SelToolbarIconButton() {
        SetColor(kColIconBtnBgHover, kColorTransparent);
        SetColor(kColIconBtnBgSelected, kColorTransparent);
    }
    void Paint(VirtPaintCtx& ctx) override {
        if (IsEnabled() && HasFlag(vwfHovered) && hoverBg != kColorUnset) {
            int radius = DpiScale(kButtonRadius);
            ctx.gfx->FillRoundedRect(ctx.bounds, radius, hoverBg);
        }
        VirtIconButton::Paint(ctx);
    }
    // square, as tall as the row: the icon is centered in it
    Size GetIdealSize() override { return {sideLen, sideLen}; }
    Color hoverBg = kColorUnset;
    int sideLen = 0;
};

static bool GetSelectionEndPoint(MainWindow* win, Point& out);

static void OnSelToolbarButtonClicked(SelectionToolbar* tb, VirtMouseEvent* ev) {
    int cmdId = ev->target ? ev->target->id : 0;
    if (!cmdId) {
        return;
    }
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
}

// Build the layout tree for the current buttons and measure it into tb->size
// (the window region is applied after SetWindowPos).
static void LayoutToolbar(SelectionToolbar* tb) {
    int padX = DpiScale(kBtnPadX);
    int padY = DpiScale(kBtnPadY);
    int margin = DpiScale(kMargin);
    int gap = DpiScale(kBtnGap);

    Color bgCol = SelBarBg();
    Color hoverBg = SelBarHoverBg(bgCol);
    Color textCol = SelBarTextColor();
    Color mutedCol = SelBarMutedTextColor();

    int textDy = PlatformFontMeasureText(tb->font, StrL("Mg")).dy;
    int rowDy = textDy + (2 * padY);
    UpdateButtonIcons(tb, textDy);

    auto* box = new HBox();
    box->alignCross = CrossAxisAlign::Stretch;
    bool isFirst = true;
    for (SelectionToolbarButton& b : tb->buttons) {
        VirtCtrl* w;
        if (b.svgIcon) {
            auto* ib = new SelToolbarIconButton();
            ib->pixmap = b.icon;
            ib->sideLen = rowDy;
            ib->hoverBg = hoverBg;
            ib->onClick = MkFunc1(OnSelToolbarButtonClicked, tb);
            w = ib;
        } else {
            auto* tbtn = new SelToolbarTextButton(ButtonText(b), tb->font);
            tbtn->textPadding = {padY, padX, padY, padX};
            tbtn->SetColor(kColBtnText, textCol);
            tbtn->SetColor(kColBtnTextDisabled, mutedCol);
            tbtn->align = VirtTextAlign::Center;
            tbtn->hoverBg = hoverBg;
            tbtn->onClick = MkFunc1(OnSelToolbarButtonClicked, tb);
            w = tbtn;
        }
        w->id = b.cmdId;
        w->SetIsEnabled(b.enabled);
        ILayout* child = w;
        if (!isFirst) {
            child = new Padding(w, Insets{0, 0, 0, gap});
        }
        isFirst = false;
        box->AddChild(child);
    }
    auto* content = new Padding(box, Insets{margin, margin, margin, margin});
    tb->size = tb->host->SetLayoutSizedToContent(content);
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

// the card itself; the buttons on it are virtual controls painted on top
static void PaintToolbar(SelectionToolbar*, VirtHostPaintEvent* ev) {
    int cornerRadius = DpiScale(kCornerRadius);
    ev->gfx->FillRoundedRect(ev->clientRect, cornerRadius, SelBarBg(), SelBarBorderColor());
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
    int gap = DpiScale(6);
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
    tb->host->SetBounds(placed);
    // the region must match the layout size after the move (the window may
    // have been 0x0, and a 1x1 region left the toolbar invisible)
    tb->host->ClipToRoundedRect(kCornerRadius, {w, h});
    return true;
}

static SelectionToolbar* GetOrCreateToolbar(MainWindow* win) {
    if (win->selectionToolbar) {
        return win->selectionToolbar;
    }
    auto* tb = new SelectionToolbar();
    tb->win = win;

    VirtHost::CreateArgs args;
    args.parent = win->hwndFrame;
    args.className = WStrL(kSelectionToolbarClassName);
    args.isPopup = true;
    args.visible = false;
    // don't steal the focus from the canvas, so keyboard shortcuts keep working
    args.noActivate = true;
    args.userData = tb;

    tb->host = VirtHost::Create(args);
    if (!tb->host) {
        delete tb;
        return nullptr;
    }
    tb->host->onPaintBackground = MkFunc1(PaintToolbar, tb);
    tb->font = GetScaledPlatformFont(GetAppFont(), kToolbarFontPct);
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
    tb->host->Show(true);
    tb->host->Invalidate(false);
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
        if (activeTb && activeTb->host && activeTb->host->IsVisible()) {
            HideSelectionToolbar(win);
        }
        return;
    }
    SelectionToolbar* tb = win->selectionToolbar;
    if (!tb || !tb->host || !tb->host->IsVisible()) {
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
    int slack = SelectionBoundsSlack();
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
        tb->host->Invalidate(false);
    }
}

void RefreshSelectionToolbarIcons(MainWindow* win) {
    SelectionToolbar* tb = win ? win->selectionToolbar : nullptr;
    if (!tb || !tb->host || !tb->host->IsVisible()) {
        return;
    }
    LayoutToolbar(tb);
    tb->host->Invalidate(false);
}

// Hide the toolbar but keep the window around for reuse.
void HideSelectionToolbar(MainWindow* win) {
    CancelPendingShow(win);
    SelectionToolbar* tb = win ? win->selectionToolbar : nullptr;
    if (!tb || !tb->host) {
        return;
    }
    if (tb->host->IsVisible()) {
        tb->host->Show(false);
    }
    if (tb->host->vroot) {
        // the mouse can't leave a hidden window, so drop the hover ourselves
        tb->host->vroot->ClearHover();
        tb->host->vroot->ClearPressed();
    }
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
    // ~VirtHost deletes the layout first: the buttons report their
    // destruction to the root
    delete tb->host;
    delete tb;
    win->selectionToolbar = nullptr;
}
