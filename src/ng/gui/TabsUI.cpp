/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// ng: orig's tab bar is gui/win/TabsCtrl.cpp: a host HWND with one TabCtrl per
// tab, painted through Gfx. Here it is a row of gpui elements over the
// WindowTab list. What is ported is the behaviour: the tab width rule and the
// width freeze after a close, the selected / inactive / hovered / custom tab
// colors, the red title of a failed load, the dirty dot, the page-number
// suffix, the ✕ on the selected tab, middle-click close, drag reorder with
// orig's "right half means after" drop rule, and the tab context menu.

#include "gui/GpuiBridge.h"
#include "base/UITask.h"

#include "gui/Dpi.h"
#include "gui/PlatformFont.h"
#include "gui/UIModels.h"

#include "Settings.h"
#include "AppSettings.h"
#include "DisplayMode.h"
#include "DocController.h"
#include "EngineBase.h"
#include "base/GuessFileType.h"
#include "EngineAll.h"
#include "DisplayModel.h"
#include "Commands.h"
#include "Translations.h"
#include "Theme.h"
#include "Menu.h"
#include "SumatraPDF.h"
#include "MainWindow.h"
#include "AIChatTypes.h"
#include "WindowTab.h"
#include "Tabs.h"
#include "StripGroups.h"
#include "gui/AppShell.h"
#include "gui/DocCanvas.h"
#include "gui/TabSwitcher.h"
#include "gui/DialogWidgets.h"
#include "gui/TabColors.h"
#include "gui/TabsUI.h"
#include "SvgIcons.h"

#include "SumatraLog.h"

struct TabsView;

// the drag payload kind is "sumatra-tab"; a drop on another tab reorders
struct TabsUI {
    // ng: one view entity per window (step 10b)
    gp::Entity<TabsView> view;
    // which tab the pointer is over, -1 for none
    int highlighted = -1;
    // Chrome-like close: keep tab widths frozen after closing with the ✕ so
    // the next tab's ✕ lands under the cursor
    bool widthFrozen = false;
    int frozenDx = 0;
    // the tab the context menu was opened on, and the model built for it. A
    // strip group's chip opens the group's menu: menuGroupId, and menuTab is
    // the group's first tab
    WindowTab* menuTab = nullptr;
    int menuGroupId = 0;
    MenuModel* menu = nullptr;
    WindowTab* menuFor = nullptr;
    int menuForGroupId = 0;
    bool menuValid = false;
    // bounds of every tab as gpui laid it out last frame (for the tooltip)
    Vec<gpui::Bounds> tabBounds;
    // the strip's height: kTabBarDy, more in the caption
    int barDy = kTabBarDy;
    // the popup of the tab context menu and the strip it is placed in
    gp::Entity<gp::PopupMenuState> menuPopup;
    gpui::Bounds barBounds{};
};

struct TabsView {
    MainWindow* win = nullptr;

    static void OnTabDown(TabsView* self, gp::Ctx* cx, const gp::MouseDownEvent* ev, int64_t idx);
    static void OnTabUp(TabsView* self, gp::Ctx* cx, const gp::MouseUpEvent* ev, int64_t idx);
    static void OnBarDown(TabsView* self, gp::Ctx* cx, const gp::MouseDownEvent* ev);
    static void OnPlusClick(TabsView* self, gp::Ctx* cx, const gp::ClickEvent*);
#if OS_WASM
    static void OnHomeClick(TabsView* self, gp::Ctx* cx, const gp::ClickEvent*);
#endif
    static void OnBarWheel(TabsView* self, gp::Ctx* cx, const gp::ScrollWheelEvent* ev);
    static void OnFillerDrop(TabsView* self, gp::Ctx* cx, const gp::DropEvent* ev);
    static void OnTabHover(TabsView* self, gp::Ctx* cx, const gp::HoverEvent* ev, int64_t idx);
    static void OnTabDrop(TabsView* self, gp::Ctx* cx, const gp::DropEvent* ev, int64_t idx);
    static void OnTabDragMove(TabsView* self, gp::Ctx* cx, const gp::DragMoveEvent* ev);
    static void OnTabClose(TabsView* self, gp::Ctx* cx, const gp::ClickEvent*, int64_t idx);
    static void OnBarLeave(TabsView* self, gp::Ctx* cx, const gp::HoverEvent* ev);
    static void OnBarUpOut(TabsView* self, gp::Ctx* cx, const gp::MouseUpEvent* ev);
    static void OnMenuAction(TabsView* self, gp::Ctx* cx, const gp::ActionEvent* ev);
    static void OnChipClick(TabsView* self, gp::Ctx* cx, const gp::ClickEvent*, int64_t groupId);
    static void OnChipUp(TabsView* self, gp::Ctx* cx, const gp::MouseUpEvent* ev, int64_t groupId);
    static void OnChipDrop(TabsView* self, gp::Ctx* cx, const gp::DropEvent* ev, int64_t groupId);
};

static uint32_t ActTabMenu() {
    static uint32_t id = gp::ActionOf(GStrL("sumatra::TabMenu"));
    return id;
}

static TabsUI* Ui(MainWindow* win) {
    if (!win->tabsUI) {
        win->tabsUI = new TabsUI();
    }
    return win->tabsUI;
}

void TabsUIDelete(MainWindow* win) {
    TabsUI* ui = win->tabsUI;
    if (!ui) {
        return;
    }
    DeleteMenuModel(ui->menu);
    delete ui;
    win->tabsUI = nullptr;
}

void TabsUIOnTabsChanged(MainWindow* win) {
    TabsUI* ui = Ui(win);
    ui->highlighted = -1;
    ui->menuTab = nullptr;
    ui->menuGroupId = 0;
    ui->menuValid = false;
    AppShellInvalidate(win);
}

// orig: a pinned tab (the Home tab) is neither dragged nor a drop target
static bool IsPinnedTab(WindowTab* tab) {
    return tab->IsPinnedHomeTab();
}

// orig: every Home tab is as wide as its label; a "+" one keeps room for its ✕ (#6469)
static bool IsLabelSizedTab(WindowTab* tab) {
    return tab->IsAboutTab();
}

// the ✕ of a tab, and the room a label-sized tab keeps for it when it's hidden
constexpr float kTabCloseDx = 20;

static Color TabBgColor(WindowTab* tab, bool isSelected, bool isUnderMouse) {
    Color selected = ThemeActiveTabBackgroundColor();
    Color inactive = ThemeInactiveTabBackgroundColor();
    // a tab with a color of its own keeps it, shaded when it isn't selected
    if (!IsSpecialColor(tab->tabColor)) {
        if (isSelected) {
            return tab->tabColor;
        }
        return AccentColor(tab->tabColor, isUnderMouse ? 35 : 25);
    }
    if (isSelected) {
        return selected;
    }
    return isUnderMouse ? AccentColor(inactive, 10) : inactive;
}

// the tab label's size; Home's margins are 1rem of it
constexpr float kTabFontSize = 12;

struct TabTextDx {
    Str text;
    Str family;
    int fontPx;
    int dx;
};

static Vec<TabTextDx> gTabTextDx;
// titles, page suffixes and chip labels; cleared when full
constexpr int kTabTextDxMax = 128;
int gTabTextMeasureCount = 0;

static void FreeTabTextDx() {
    for (TabTextDx& e : gTabTextDx) {
        str::Free(e.text);
        str::Free(e.family);
    }
    VecReset(gTabTextDx);
}

// the tab bar is laid out every frame, so widths are remembered by text, font
// and size. gpui's MeasureText is what the labels are painted with.
static int MeasureTabTextDx(gp::PaintCtx* paint, Str family, float fontPx, Str s) {
    int key = (int)fontPx;
    for (const TabTextDx& e : gTabTextDx) {
        if (e.fontPx == key && str::Eq(e.text, s) && str::Eq(e.family, family)) {
            return e.dx;
        }
    }
    if (len(gTabTextDx) >= kTabTextDxMax) {
        FreeTabTextDx();
    }
    int dx = 0;
    if (paint && paint->pa && len(s) > 0) {
        dx = (int)(gp::MeasureText(paint, ToGpui(s), fontPx, 0).w + 0.5f);
    }
    if (dx <= 0 && len(s) > 0) {
        float dpi = paint && paint->dpi > 0 ? paint->dpi : 96.f;
        PlatformFont* font = GetUserGuiFont(family, (int)(fontPx * dpi / 96.f + 0.5f));
        dx = (int)((float)PlatformFontMeasureText(font, s).dx * 96.f / dpi + 0.5f);
    }
    VecAppend(gTabTextDx, TabTextDx{str::Dup(s), str::Dup(family), key, dx});
    gTabTextMeasureCount++;
    return dx;
}

// TabsEqualWidth = false: what a tab sized to its name needs, in dips, as
// BuildTab lays it out: margins, page suffix, dirty dot and room for the ✕
static int FitToNameDx(MainWindow* win, WindowTab* tab) {
    gp::PaintCtx* paint = win->gpuiWin ? &win->gpuiWin->paint : nullptr;
    Str family = FromGpui(gp::PaintSystemUIFontMappedFamily());
    auto measure = [&](Str s, float fontSize) { return MeasureTabTextDx(paint, family, fontSize, s); };
    int dx = 8 + measure(tab->GetTabTitle(), kTabFontSize);
    TempStr pageText = TabPageSuffixTemp(tab);
    if (len(pageText) > 0) {
        dx += 2 + measure(pageText, 10);
    }
    if (TabIsDirty(tab)) {
        dx += 2 + 6;
    }
    return dx + 2 + (int)kTabCloseDx + 2 + 1;
}

// a strip group's chip: as wide as its label, within limits; without one a dot
static int ChipDx(MainWindow* win, StripGroup* g) {
    constexpr int kMinDx = 20;
    constexpr int kMaxDx = 160;
    TempStr label = StripGroupLabelTemp(win, g);
    if (len(label) == 0) {
        return kMinDx;
    }
    gp::PaintCtx* paint = win->gpuiWin ? &win->gpuiWin->paint : nullptr;
    Str family = FromGpui(gp::PaintSystemUIFontMappedFamily());
    int dx = MeasureTabTextDx(paint, family, kTabFontSize, label) + 18;
    return limitValue(dx, kMinDx, kMaxDx);
}

// for -dbg-control: each tab's laid-out width, one "dx=<n> text=<title>" line per tab
TempStr TabsUIWidthsTemp(MainWindow* win) {
    TabsUI* ui = Ui(win);
    str::Builder out;
    for (int i = 0; i < win->TabCount() && i < len(ui->tabBounds); i++) {
        const gpui::Bounds& b = ui->tabBounds[i];
        out.Append(fmt("dx=%d text=%s\n", (int)b.w, win->GetTab(i)->GetTabTitle()));
        // where to click it, in frame coordinates
        out.Append(fmt("  at x=%d y=%d dy=%d\n", (int)b.x, (int)b.y, (int)b.h));
    }
    return ToStrTemp(out);
}

// orig's LayoutTabs: every document tab is as wide as TabWidth allows, shrinking
// so all of them fit the bar. Home is sized by its label, so the others get what
// it took in the last frame: its width is only known after layout
static int TabWidth(MainWindow* win, int barDx) {
    TabsUI* ui = Ui(win);
    int nTabs = win->TabCount();
    if (nTabs == 0) {
        return 0;
    }
    if (ui->widthFrozen && ui->frozenDx > 0) {
        return ui->frozenDx;
    }
    // the "+" is square and always there; strip group chips and the tabs of
    // collapsed groups aren't tabs
    int restDx = barDx - 5 - ui->barDy;
    for (StripGroup* g : win->stripGroups) {
        restDx -= ChipDx(win, g);
    }
    int nRest = nTabs;
    for (int i = 0; i < nTabs; i++) {
        if (StripGroupIsHidden(win, win->GetTab(i))) {
            nRest--;
            continue;
        }
        if (!IsLabelSizedTab(win->GetTab(i))) {
            continue;
        }
        nRest--;
        if (i < len(ui->tabBounds)) {
            restDx -= (int)ui->tabBounds[i].w;
        }
    }
    if (gSettings->tabsEqualWidth) {
        return std::min(gSettings->tabWidth, restDx / std::max(nRest, 1));
    }
    // as wide as the name, shrunk like equal-width tabs when they don't fit,
    // but never wider than the name needs (BuildTab caps each at this)
    Vec<int> nameDx;
    for (int i = 0; i < nTabs; i++) {
        WindowTab* tab = win->GetTab(i);
        if (!IsLabelSizedTab(tab) && !StripGroupIsHidden(win, tab)) {
            VecAppend(nameDx, std::min(FitToNameDx(win, tab), gSettings->tabWidth));
        }
    }
    return FairShareCap(nameDx, restDx);
}

// --- input ------------------------------------------------------------------

void TabsView::OnTabDown(TabsView* self, gp::Ctx* cx, const gp::MouseDownEvent* ev, int64_t idx) {
    MainWindow* win = self->win;
    WindowTab* tab = win->GetTab((int)idx);
    if (!tab) {
        return;
    }
    if (ev->button == gp::MouseButton::Middle) {
        // middle-clicking unconditionally closes the tab
        TabsUI* ui = Ui(win);
        ui->frozenDx = TabWidth(win, win->frameRc.dx);
        ui->widthFrozen = true;
        CloseTab(tab, false);
        gp::Notify(cx);
        return;
    }
    if (IsContextClick(ev->button, ev->modifiers) || ev->button != gp::MouseButton::Left) {
        // the context menu opens when the button comes up (OnTabUp)
        return;
    }
    TabsSelect(win, (int)idx);
    gp::Notify(cx);
}

// orig's TabsContextMenu, from the right button going up over a tab; the tab
// is not selected
void TabsView::OnTabUp(TabsView* self, gp::Ctx* cx, const gp::MouseUpEvent* ev, int64_t idx) {
    if (!IsContextClick(ev->button, ev->modifiers)) {
        return;
    }
    MainWindow* win = self->win;
    WindowTab* tab = win->GetTab((int)idx);
    if (!tab || tab->IsAboutTab()) {
        return;
    }
    TabsUI* ui = Ui(win);
    ui->menuTab = tab;
    ui->menuGroupId = 0;
    ui->menuValid = false;
    OpenPopupMenuAt(cx, ui->menuPopup, ev->x - ui->barBounds.x, ev->y - ui->barBounds.y);
    AppShellInvalidate(win);
    gp::Notify(cx);
}

// ng: keeps a right press from gpui's ContextMenu, which would open from it
void TabsView::OnBarDown(TabsView*, gp::Ctx* cx, const gp::MouseDownEvent* ev) {
    // a press hides a tab's tooltip, as a Win32 tooltip does
    HoverTooltipHide(cx);
    if (IsContextClick(ev->button, ev->modifiers)) {
        gp::WindowStopPropagation(cx);
    }
}

void TabsView::OnBarWheel(TabsView* self, gp::Ctx* cx, const gp::ScrollWheelEvent* ev) {
    DocCanvasWheelFromFrame(self->win, cx, ev);
}

// orig's WM_LBUTTONUP with no tab under the mouse: TriggerTabMigration, which
// for the tab's own window means a window of its own
void TabsView::OnFillerDrop(TabsView* self, gp::Ctx* cx, const gp::DropEvent* ev) {
    WindowTab* tab = self->win->GetTab(ev->drag.ix);
    if (!tab) {
        return;
    }
    MaybeMigrateTab(tab, nullptr);
    gp::Notify(cx);
}

void TabsView::OnTabHover(TabsView* self, gp::Ctx* cx, const gp::HoverEvent* ev, int64_t idx) {
    MainWindow* win = self->win;
    TabsUI* ui = Ui(win);
    int want = ev->hovered ? (int)idx : -1;
    if (!ev->hovered && ui->highlighted != (int)idx) {
        return;
    }
    if (ui->highlighted == want) {
        return;
    }
    ui->highlighted = want;
    WindowTab* tab = win->GetTab((int)idx);
    if (ev->hovered && tab && idx < ui->tabBounds.len) {
        TempStr tip = MakeTabTooltipTemp(tab, TabIsDirty(tab));
        if (len(tip) > 0) {
            HoverTooltipShow(cx, tip, ui->tabBounds[(int)idx]);
        }
    } else {
        HoverTooltipHide(cx);
    }
    gp::Notify(cx);
}

// orig unfreezes the tab widths when the pointer leaves the bar
void TabsView::OnBarLeave(TabsView* self, gp::Ctx* cx, const gp::HoverEvent* ev) {
    if (ev->hovered) {
        return;
    }
    TabsUI* ui = Ui(self->win);
    if (!ui->widthFrozen && ui->highlighted < 0) {
        return;
    }
    ui->widthFrozen = false;
    ui->highlighted = -1;
    gp::Notify(cx);
}

// orig's WM_LBUTTONUP drag branch: the tab lands before the tab it was dropped
// on, or after it when the drop was in its right half
void TabsView::OnTabDrop(TabsView* self, gp::Ctx* cx, const gp::DropEvent* ev, int64_t idx) {
    MainWindow* win = self->win;
    int from = ev->drag.ix;
    int dstIdx = (int)idx;
    bool inRightHalf = (ev->x - ev->el.x) > (ev->el.w / 2);
    if (inRightHalf) {
        dstIdx++;
    }
    WindowTab* dst = win->GetTab(dstIdx);
    if (dst && IsPinnedTab(dst)) {
        return;
    }
    // it joins the strip group of the tab it was dropped on
    WindowTab* on = win->GetTab((int)idx);
    int groupId = on ? on->stripGroupId : 0;
    logf("TabsUI: drag tab %d -> %d (group %d)\n", from, dstIdx, groupId);
    StripGroupDropTab(win, from, dstIdx, groupId);
    gp::Notify(cx);
}

// the drag image is drawn by the shell, so it has to render for every move
void TabsView::OnTabDragMove(TabsView* self, gp::Ctx* cx, const gp::DragMoveEvent*) {
    if (cx->win->pressedMoved) {
        AppShellInvalidate(self->win);
    }
}

// orig's TabsCtrl::RenderForDragging: once the pointer is past the drag
// threshold the tab follows it as an image of the tab as it looks while
// selected. ng: an element of the window, so it stays inside the frame where
// orig's ImageList drag image moves over the whole screen
static gp::El* BuildTabDragImage(MainWindow* win, gp::Ctx* cx, int tabDx) {
    const gp::DragPayload* drag = gp::WindowActiveDrag(cx);
    if (!drag || !cx->win->pressedMoved || !str::Eq(FromGpui(drag->kind), StrL("sumatra-tab"))) {
        return nullptr;
    }
    WindowTab* tab = win->GetTab(drag->ix);
    if (!tab) {
        return nullptr;
    }
    Color bgCol = ThemeActiveTabBackgroundColor();
    Color textCol = TabTextColorForBackground(ThemeWindowTextColor(), bgCol);
    gp::Point off = gp::WindowDragOffset(cx);
    gp::El* el = gp::Div(cx->a)
                     ->FlexRow()
                     ->W((float)tabDx)
                     ->H((float)kTabBarDy)
                     ->ItemsCenter()
                     ->Gap(2)
                     ->PadX(8)
                     ->Bg(ToGpui(bgCol));
    el->Child(gp::TextEl(cx->a, GpuiDup(cx->a, tab->GetTabTitle()))
                  ->Font(12)
                  ->Fg(ToGpui(textCol))
                  ->Flex1()
                  ->MinW(0)
                  ->Truncate());
    TempStr pageText = TabPageSuffixTemp(tab);
    if (len(pageText) > 0) {
        Color pageCol = AccentColor(textCol, 40);
        el->Child(gp::TextEl(cx->a, GpuiDup(cx->a, pageText))->Font(10)->Fg(ToGpui(pageCol))->Shrink0());
    }
    return el->Fixed()->Left(cx->win->mouseX - off.x)->Top(cx->win->mouseY - off.y)->Deferred();
}

// orig's MainWindowTabMigration: the main window the tab was released over.
// The release arrives at the window the drag started in, in its coordinates.
static MainWindow* MainWindowAtRelease(gp::Window* from, const gp::MouseUpEvent* ev) {
    gp::Window* at = gp::WindowAtPoint(from, gp::Point{ev->x, ev->y}, nullptr);
    for (MainWindow* w : gWindows) {
        if (at && w->gpuiWin == at) {
            return w;
        }
    }
    return nullptr;
}

// ng: orig's TabsCtrl::MigrationEvent - a tab released outside the strip
void TabsView::OnBarUpOut(TabsView* self, gp::Ctx* cx, const gp::MouseUpEvent* ev) {
    MainWindow* win = self->win;
    const gp::DragPayload* drag = gp::WindowActiveDrag(cx);
    if (!drag || !str::Eq(FromGpui(drag->kind), StrL("sumatra-tab"))) {
        return;
    }
    WindowTab* tab = win->GetTab(drag->ix);
    if (!tab) {
        return;
    }
    MainWindow* releaseWnd = MainWindowAtRelease(cx->win, ev);
    if (releaseWnd == win) {
        // don't re-add to the same window
        releaseWnd = nullptr;
    }
    MaybeMigrateTab(tab, releaseWnd);
    gp::Notify(cx);
}

// a click on a strip group's chip collapses or expands the group
void TabsView::OnChipClick(TabsView* self, gp::Ctx* cx, const gp::ClickEvent*, int64_t groupId) {
    MainWindow* win = self->win;
    StripGroup* g = StripGroupFind(win, (int)groupId);
    if (g) {
        StripGroupSetCollapsed(win, g->id, !g->collapsed);
    }
    gp::Notify(cx);
}

void TabsView::OnChipUp(TabsView* self, gp::Ctx* cx, const gp::MouseUpEvent* ev, int64_t groupId) {
    if (!IsContextClick(ev->button, ev->modifiers)) {
        return;
    }
    MainWindow* win = self->win;
    WindowTab* tab = StripGroupFirstTab(win, (int)groupId);
    if (!tab) {
        return;
    }
    TabsUI* ui = Ui(win);
    ui->menuTab = tab;
    ui->menuGroupId = (int)groupId;
    ui->menuValid = false;
    OpenPopupMenuAt(cx, ui->menuPopup, ev->x - ui->barBounds.x, ev->y - ui->barBounds.y);
    AppShellInvalidate(win);
    gp::Notify(cx);
}

// dropped on a chip: the tab joins the group, at its front (the end of a
// collapsed one)
void TabsView::OnChipDrop(TabsView* self, gp::Ctx* cx, const gp::DropEvent* ev, int64_t groupId) {
    MainWindow* win = self->win;
    StripGroup* g = StripGroupFind(win, (int)groupId);
    WindowTab* first = StripGroupFirstTab(win, (int)groupId);
    if (!g || !first) {
        return;
    }
    int dstIdx = win->GetTabIdx(first);
    if (g->collapsed) {
        dstIdx += StripGroupTabCount(win, g->id);
    }
    StripGroupDropTab(win, ev->drag.ix, dstIdx, g->id);
    gp::Notify(cx);
}

void TabsView::OnTabClose(TabsView* self, gp::Ctx* cx, const gp::ClickEvent*, int64_t idx) {
    MainWindow* win = self->win;
    WindowTab* tab = win->GetTab((int)idx);
    if (!tab) {
        return;
    }
    // freeze tab widths so the next close button stays under the cursor
    TabsUI* ui = Ui(win);
    ui->frozenDx = TabWidth(win, win->frameRc.dx);
    ui->widthFrozen = true;
    CloseTab(tab, false);
    gp::Notify(cx);
}

void TabsView::OnPlusClick(TabsView* self, gp::Ctx* cx, const gp::ClickEvent*) {
    AddExtraHomeTab(self->win);
    gp::Notify(cx);
}

#if OS_WASM
static void RunExitCmd(MainWindow* win) {
    if (IsMainWindowValidAndNotClosing(win)) {
        ExecuteCmd(win, CmdExit);
    }
}

// Exit saves the state, then the host page goes to its home page (sumatra-web
// navigates to "/", docs/wasm-host-api.md). Posted: Exit deletes the window
// this click is still being handled in
void TabsView::OnHomeClick(TabsView* self, gp::Ctx* cx, const gp::ClickEvent*) {
    uitask::Post(MkFunc0<MainWindow>(RunExitCmd, self->win), "HomeExit");
    gp::Notify(cx);
}

// SumatraPDF Web: back to the web app's home page, before the menu button
static gp::El* TabsUIHomeButton(MainWindow* win, gp::Ctx* cx) {
    gpc::Icon* icon = gpc::Icon::Empty(cx)->Data(ToGpui(Str(gIconHome)));
    gp::El* btn = gpc::Button::New(cx, GStrL("sumatra-web-home"))
                      ->Icon(gpc::ButtonIcon::New(cx, icon)->WithSize(gp::UiSize::Small))
                      ->Ghost()
                      ->Compact()
                      ->WithSize(gp::UiSize::Small)
                      ->Tooltip(GpuiDup(cx->a, Tr("Home")))
                      ->OnClick(gp::ListenTo(Ui(win)->view, &TabsView::OnHomeClick))
                      ->IntoEl();
    btn->H(gp::kFill)->Shrink0();
    return btn;
}
#endif

// ng: orig's TrackPopupMenu returns the command only after the menu is gone.
// gpui's popup is still up when the action fires, and it holds the keyboard and
// the mouse, so a command that opens a dialog would open it behind a layer that
// swallows every event. Running the command from the ui task queue gives gpui
// the frame it needs to close the popup.
struct TabMenuCmd {
    MainWindow* win;
    WindowTab* tab;
    int cmdId;
};

static void RunTabMenuCmd(TabMenuCmd* c) {
    if (IsMainWindowValidAndNotClosing(c->win)) {
        TabContextMenuCommand(c->win, c->tab, c->cmdId);
    }
    delete c;
}

void TabsView::OnMenuAction(TabsView* self, gp::Ctx* cx, const gp::ActionEvent* ev) {
    MainWindow* win = self->win;
    auto* c = new TabMenuCmd{win, Ui(win)->menuTab, (int)ev->arg};
    uitask::Post(MkFunc0<TabMenuCmd>(RunTabMenuCmd, c), "TabMenuCmd");
    gp::Notify(cx);
}

// --- building ---------------------------------------------------------------

static gpc::PopupMenu* PopupFromModel(gp::Ctx* cx, MenuModel* model, Str id) {
    gpc::PopupMenu* menu = gpc::PopupMenu::New(cx, GpuiDup(cx->a, id))->MinW(200);
    if (!model) {
        return menu;
    }
    int i = -1;
    for (const MenuItemModel& it : model->items) {
        i++;
        if (it.separator) {
            menu->Separator();
            continue;
        }
        if (it.submenu) {
            TempStr subId = fmt("%s-%d", id, i);
            menu->Submenu(GpuiDup(cx->a, it.title), PopupFromModel(cx, it.submenu, subId));
            continue;
        }
        menu->MenuWithAction(GpuiDup(cx->a, it.title), ActTabMenu(), (intptr_t)it.cmdId);
        menu->Disabled(it.disabled);
        menu->Checked(it.checked);
    }
    return menu;
}

static gp::El* CloseButton(MainWindow* win, gp::Ctx* cx, int idx) {
    TempStr id = fmt("sumatra-tab-close-%d", idx);
    return gpc::Button::New(cx, GpuiDup(cx->a, id))
        ->Icon(gp::IconName::Close)
        ->Ghost()
        ->Compact()
        ->WithSize(gp::UiSize::XSmall)
        ->OnClick(gp::ListenTo(Ui(win)->view, &TabsView::OnTabClose, (intptr_t)idx))
        ->IntoEl()
        ->W(kTabCloseDx)
        ->Shrink0();
}

// the colored chip in front of a strip group's tabs, with its name (and the
// count of hidden tabs when collapsed)
static gp::El* BuildChip(MainWindow* win, gp::Ctx* cx, StripGroup* g) {
    TempStr label = StripGroupLabelTemp(win, g);
    Color fg = IsLightColor(g->color) ? MkRgb(0x20, 0x20, 0x20) : MkRgb(0xff, 0xff, 0xff);
    gp::El* inner =
        gp::Div(cx->a)->FlexRow()->ItemsCenter()->JustifyCenter()->H(gp::kFill)->W(gp::kFill)->PadX(6)->Radius(3)->Bg(
            ToGpui(g->color));
    if (len(label) > 0) {
        inner->Child(gp::TextEl(cx->a, GpuiDup(cx->a, label))->Font(kTabFontSize)->Fg(ToGpui(fg))->Truncate());
    }
    TabsUI* ui = Ui(win);
    return gp::Div(cx->a)
        ->H(gp::kFill)
        ->W((float)ChipDx(win, g))
        ->Shrink0()
        ->PadX(2)
        ->PadY(4)
        ->Role(gp::AccessibilityRole::Button)
        ->AriaLabel(GpuiDup(cx->a, label))
        ->PathClick(GpuiDup(cx->a, fmt("sumatra-tab-group-%d", g->id)))
        ->OnClick(gp::ListenTo(ui->view, &TabsView::OnChipClick, (intptr_t)g->id))
        ->OnMouseUp(gp::ListenTo(ui->view, &TabsView::OnChipUp, (intptr_t)g->id))
        ->OnDrop(GStrL("sumatra-tab"), gp::ListenTo(ui->view, &TabsView::OnChipDrop, (intptr_t)g->id))
        ->Child(inner);
}

static gp::El* BuildTab(MainWindow* win, gp::Ctx* cx, int idx, int tabDx) {
    TabsUI* ui = Ui(win);
    WindowTab* tab = win->GetTab(idx);
    // while the Ctrl+Tab list is open it previews the tab it points at, as
    // orig's TabsCtrl::SetHighlighted does
    WindowTab* preview = TabSwitcherHighlighted(win);
    bool isSelected = preview ? (tab == preview) : IsCurrentTab(win, tab);
    bool isUnderMouse = ui->highlighted == idx;
    Color bgCol = TabBgColor(tab, isSelected, isUnderMouse);
    Color textCol = TabTextColorForBackground(ThemeWindowTextColor(), bgCol);
    if (tab->loadState == WindowTab::LoadState::Error) {
        // a tab whose document failed to load shows its title in red, shaded
        // to stay readable on light and dark tab backgrounds
        textCol = IsLightColor(bgCol) ? MkRgb(0xC4, 0x1E, 0x1E) : MkRgb(0xFF, 0x6A, 0x6A);
    }

    // orig: Home is as wide as its label plus margins. The pinned one has no room
    // for a close, a "+" one keeps it
    bool isHome = IsLabelSizedTab(tab);
    bool isPinnedHome = IsPinnedTab(tab);
    // a tab sized by its label keeps the ✕'s room, so selecting it doesn't resize it
    bool keepsCloseRoom = isHome || !gSettings->tabsEqualWidth;
    float padR = isSelected ? 2.f : 8.f;
    if (isPinnedHome) {
        padR = kTabFontSize;
    } else if (keepsCloseRoom) {
        padR = 2.f;
    }
    gp::El* el = gp::Div(cx->a)
                     ->FlexRow()
                     ->H(gp::kFill)
                     ->Shrink0()
                     ->ItemsCenter()
                     ->Gap(2)
                     ->PadL(isHome ? kTabFontSize : 8.f)
                     ->PadR(padR)
                     ->Bg(ToGpui(bgCol))
                     ->Role(gp::AccessibilityRole::Tab)
                     ->AriaLabel(GpuiDup(cx->a, tab->GetTabTitle()))
                     ->PathClick(GpuiDup(cx->a, fmt("sumatra-tab-%d", idx)))
                     ->BoundsOut(&ui->tabBounds[idx])
                     ->OnMouseDown(gp::ListenTo(Ui(win)->view, &TabsView::OnTabDown, (intptr_t)idx))
                     ->OnMouseUp(gp::ListenTo(Ui(win)->view, &TabsView::OnTabUp, (intptr_t)idx))
                     ->OnHover(gp::ListenTo(Ui(win)->view, &TabsView::OnTabHover, (intptr_t)idx));
    if (StripGroup* g = StripGroupOf(win, tab)) {
        el->BorderB(3, ToGpui(g->color));
    }
    if (!IsPinnedTab(tab)) {
        el->OnDrag(GStrL("sumatra-tab"), idx);
        el->OnDragMove(gp::ListenTo(Ui(win)->view, &TabsView::OnTabDragMove));
    }
    // the pinned tab is a target too: a drop on its right half lands after it
    // (OnTabDrop refuses the position a pinned tab holds)
    el->OnDrop(GStrL("sumatra-tab"), gp::ListenTo(Ui(win)->view, &TabsView::OnTabDrop, (intptr_t)idx));

    gp::El* title = gp::TextEl(cx->a, GpuiDup(cx->a, tab->GetTabTitle()))->Font(kTabFontSize)->Fg(ToGpui(textCol));
    if (!isHome && gSettings->tabsEqualWidth) {
        el->W((float)tabDx);
        title->Flex1()->MinW(0)->Truncate();
    } else if (!isHome) {
        el->MaxW((float)tabDx);
        title->Shrink(1)->MinW(0)->Truncate();
    }
    el->Child(title);

    TempStr pageText = TabPageSuffixTemp(tab);
    if (len(pageText) > 0) {
        Color pageCol = AccentColor(textCol, 40);
        el->Child(gp::TextEl(cx->a, GpuiDup(cx->a, pageText))->Font(10)->Fg(ToGpui(pageCol))->Shrink0());
    }
    if (TabIsDirty(tab)) {
        el->Child(gp::Div(cx->a)->W(6)->H(6)->Radius(3)->Shrink0()->Bg(ToGpui(MkRgb(0xEE, 0x22, 0x22))));
    }
    // like Chrome: only the selected tab shows its ✕, so a click on another
    // tab always selects it and can't accidentally close it
    if (isSelected && !isPinnedHome) {
        el->Child(CloseButton(win, cx, idx));
    } else if (keepsCloseRoom && !isPinnedHome) {
        // the label stays where it is when the tab is selected
        el->Child(gp::Div(cx->a)->W(kTabCloseDx)->Shrink0());
    }
    return el;
}

// orig's CB_MENU caption button. The strip puts it before its tabs; a window
// without tabs has no strip, so the caption asks for it alone. Null while the
// menu bar is showing. A click opens the bar's menus as one drop-down.
gp::El* TabsUIMenuButton(MainWindow* win, gp::Ctx* cx) {
    if (win->isMenuBarVisible || AppShellNativeMenu() || !win->menu) {
        return nullptr;
    }
    gpc::PopupMenu* popup = AppShellMenuPopup(cx, win->menu, StrL("sumatra-menu-btn"), MenuCuesAlways());
    // an open menu shows what the last command changed, even mid-burst
    gp::PopupMenuState* popupState = popup->state.Get(cx);
    if (win->menuDirty && popupState && popupState->open) {
        RebuildMenuBar(win);
        popup = AppShellMenuPopup(cx, win->menu, StrL("sumatra-menu-btn"), MenuCuesAlways());
    }
    popup->MinW(160);
    TrackPopup(cx, popup);

    gp::El* btn = gpc::Button::New(cx, GStrL("sumatra-tab-menu"))
                      ->Icon(gp::IconName::Menu)
                      ->Ghost()
                      ->Compact()
                      ->WithSize(gp::UiSize::Small)
                      ->IntoEl();
    // the cell is the whole tab-row height, so the menu opens under the row
    btn->H(gp::kFill);
    auto* dd = gpc::DropdownMenu::New(cx, GStrL("sumatra-menu-btn-dd"));
    // flush under the button, as orig's TrackCaptionPopupMenu
    dd->gap = 0;
    gp::El* el = dd->Trigger(btn)->Menu(popup)->IntoEl();
    el->H(gp::kFill);
    return el;
}

gp::El* TabsUIBuild(MainWindow* win, gp::Ctx* cx, int barDy) {
    TabsUI* tabsUi = Ui(win);
    tabsUi->barDy = barDy;
    if (!tabsUi->view.IsValid()) {
        tabsUi->view = gp::EntityNewState<TabsView>(cx->app);
    }
    tabsUi->view.Get(cx)->win = win;

    if (!TabsAreVisible(win)) {
        return nullptr;
    }
    TabsUI* ui = Ui(win);
    int nTabs = win->TabCount();
    // gpui writes each tab's laid-out rect back through BoundsOut, so the
    // slots have to exist (and keep their address) before the tabs are built
    while (len(ui->tabBounds) < nTabs) {
        VecAppend(ui->tabBounds, gpui::Bounds{});
    }

    // a little air at the end of the strip
    int barDx = std::max(100, (win->tabsAvailDx > 0 ? win->tabsAvailDx : win->frameRc.dx) - 40);
    int tabDx = TabWidth(win, barDx);

    gp::El* bar = gp::Div(cx->a)
                      ->FlexRow()
                      ->W(gp::kFill)
                      ->H((float)ui->barDy)
                      ->Shrink0()
                      ->ItemsStretch()
                      // orig fills the bar's background with the selected tab's
                      // color, so the inactive tabs are the ones that stand out
                      ->Bg(ToGpui(ThemeActiveTabBackgroundColor()))
                      ->OnHover(gp::ListenTo(Ui(win)->view, &TabsView::OnBarLeave))
                      ->OnMouseDown(gp::ListenTo(Ui(win)->view, &TabsView::OnBarDown))
                      ->OnScrollWheel(gp::ListenTo(Ui(win)->view, &TabsView::OnBarWheel))
                      ->BoundsOut(&ui->barBounds)
                      ->OnMouseUpOut(gp::ListenTo(Ui(win)->view, &TabsView::OnBarUpOut));
    // orig's caption has a menu button left of the tabs while the menu bar is
    // hidden (CB_MENU); a click opens the bar's menus as one drop-down
#if OS_WASM
    bar->Child(TabsUIHomeButton(win, cx));
#endif
    if (gp::El* menuBtn = TabsUIMenuButton(win, cx)) {
        bar->Child(menuBtn);
    }
    for (int i = 0; i < nTabs; i++) {
        WindowTab* tab = win->GetTab(i);
        StripGroup* g = StripGroupOf(win, tab);
        bool startsGroup = g && (i == 0 || win->GetTab(i - 1)->stripGroupId != g->id);
        if (startsGroup) {
            bar->Child(BuildChip(win, cx, g));
        }
        if (!StripGroupIsHidden(win, tab)) {
            bar->Child(BuildTab(win, cx, i, tabDx));
        }
    }
    bar->Child(gpc::Button::New(cx, GStrL("sumatra-tabs-plus"))
                   ->Icon(gp::IconName::Plus)
                   ->Ghost()
                   ->Compact()
                   ->WithSize(gp::UiSize::XSmall)
                   ->OnClick(gp::ListenTo(Ui(win)->view, &TabsView::OnPlusClick))
                   ->IntoEl()
                   ->W((float)ui->barDy)
                   ->H(gp::kFill)
                   ->Shrink0());
    // orig answers WM_NCHITTEST with HTCAPTION for the strip's empty area: a
    // drag there moves the window, a double-click maximizes it and a right
    // click shows the system menu. A tab dropped there leaves the window
    bar->Child(gp::Div(cx->a)
                   ->Flex1()
                   ->H(gp::kFill)
                   ->Click(gp::ClickWinCaption)
                   ->OnDrop(GStrL("sumatra-tab"), gp::ListenTo(Ui(win)->view, &TabsView::OnFillerDrop)));
    gp::El* dragImage = BuildTabDragImage(win, cx, tabDx);
    if (dragImage) {
        bar->Child(dragImage);
    }

    if (!ui->menuValid || ui->menuFor != ui->menuTab || ui->menuForGroupId != ui->menuGroupId) {
        DeleteMenuModel(ui->menu);
        if (ui->menuGroupId != 0) {
            ui->menu = BuildStripGroupMenu(win, ui->menuGroupId);
        } else {
            ui->menu = BuildTabContextMenu(win, ui->menuTab);
        }
        ui->menuFor = ui->menuTab;
        ui->menuForGroupId = ui->menuGroupId;
        ui->menuValid = true;
    }
    // ng: the ContextMenu wraps a box around the strip, so the strip's own
    // right-press listener can keep the press from it
    gpc::PopupMenu* popup = TrackPopup(cx, PopupFromModel(cx, ui->menu, StrL("tab-ctx-menu")));
    ui->menuPopup = popup->state;
    gp::El* barBox = gp::Div(cx->a)->W(gp::kFill)->Shrink0()->Child(bar);
    gp::El* body = gpc::ContextMenu::New(cx, GStrL("tab-ctx"))->Child(barBox)->Menu(popup)->IntoEl();
    body->W(gp::kFill)->Shrink0()->Bg(ToGpui(ThemeActiveTabBackgroundColor()));
    body->OnAction(ActTabMenu(), gp::ListenTo(Ui(win)->view, &TabsView::OnMenuAction));
    return body;
}
