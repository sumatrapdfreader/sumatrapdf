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
#include "WindowTab.h"
#include "Tabs.h"
#include "gui/AppShell.h"
#include "gui/DocCanvas.h"
#include "gui/TabSwitcher.h"
#include "gui/DialogWidgets.h"
#include "gui/TabsUI.h"

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
    // the tab the context menu was opened on, and the model built for it
    WindowTab* menuTab = nullptr;
    MenuModel* menu = nullptr;
    WindowTab* menuFor = nullptr;
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
    static void OnBarWheel(TabsView* self, gp::Ctx* cx, const gp::ScrollWheelEvent* ev);
    static void OnFillerDrop(TabsView* self, gp::Ctx* cx, const gp::DropEvent* ev);
    static void OnTabHover(TabsView* self, gp::Ctx* cx, const gp::HoverEvent* ev, int64_t idx);
    static void OnTabDrop(TabsView* self, gp::Ctx* cx, const gp::DropEvent* ev, int64_t idx);
    static void OnTabDragMove(TabsView* self, gp::Ctx* cx, const gp::DragMoveEvent* ev);
    static void OnTabClose(TabsView* self, gp::Ctx* cx, const gp::ClickEvent*, int64_t idx);
    static void OnBarLeave(TabsView* self, gp::Ctx* cx, const gp::HoverEvent* ev);
    static void OnBarUpOut(TabsView* self, gp::Ctx* cx, const gp::MouseUpEvent* ev);
    static void OnMenuButton(TabsView* self, gp::Ctx* cx, const gp::ClickEvent*);
    static void OnMenuAction(TabsView* self, gp::Ctx* cx, const gp::ActionEvent* ev);
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
    ui->menuValid = false;
    AppShellInvalidate(win);
}

// orig: a pinned tab (the Home tab) is neither dragged nor a drop target
static bool IsPinnedTab(WindowTab* tab) {
    return tab->IsAboutTab();
}

// the text stays readable on a tab that carries a color of its own
static Color TabTextColorForBackground(Color text, Color tabBg) {
    if (abs((int)GetLightness(text) - (int)GetLightness(tabBg)) >= 80) {
        return text;
    }
    return IsLightColor(tabBg) ? kColBlack : kColWhite;
}

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

// orig's LayoutTabs: every tab is as wide as TabWidth allows, shrinking so all
// of them fit the bar
static int TabWidth(MainWindow* win, int barDx) {
    TabsUI* ui = Ui(win);
    int nTabs = win->TabCount();
    if (nTabs == 0) {
        return 0;
    }
    if (ui->widthFrozen && ui->frozenDx > 0) {
        return ui->frozenDx;
    }
    int maxDx = (barDx - 5) / nTabs;
    return std::min(gSettings->tabWidth, maxDx);
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
    if (ev->button != gp::MouseButton::Left) {
        // the context menu opens when the right button comes up (OnTabUp)
        return;
    }
    TabsSelect(win, (int)idx);
    gp::Notify(cx);
}

// orig's TabsContextMenu, from the right button going up over a tab; the tab
// is not selected
void TabsView::OnTabUp(TabsView* self, gp::Ctx* cx, const gp::MouseUpEvent* ev, int64_t idx) {
    if (ev->button != gp::MouseButton::Right) {
        return;
    }
    MainWindow* win = self->win;
    WindowTab* tab = win->GetTab((int)idx);
    if (!tab || tab->IsAboutTab()) {
        return;
    }
    TabsUI* ui = Ui(win);
    ui->menuTab = tab;
    ui->menuValid = false;
    OpenPopupMenuAt(cx, ui->menuPopup, ev->x - ui->barBounds.x, ev->y - ui->barBounds.y);
    AppShellInvalidate(win);
    gp::Notify(cx);
}

// ng: keeps a right press from gpui's ContextMenu, which would open from it
void TabsView::OnBarDown(TabsView*, gp::Ctx* cx, const gp::MouseDownEvent* ev) {
    if (ev->button == gp::MouseButton::Right) {
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
        TempStr tip = MakeTabTooltipTemp(tab->filePath, TabIsDirty(tab));
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
    if (dstIdx == from) {
        return;
    }
    WindowTab* dst = win->GetTab(dstIdx);
    if (dst && IsPinnedTab(dst)) {
        return;
    }
    logf("TabsUI: drag tab %d -> %d\n", from, dstIdx);
    TabsMoveTab(win, from, dstIdx);
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

// orig's MainWindowTabMigration: the window the tab was released over
static MainWindow* MainWindowUnderCursor() {
#if OS_WIN
    POINT pt{};
    if (!GetCursorPos(&pt)) {
        return nullptr;
    }
    HWND hwnd = WindowFromPoint(pt);
    if (hwnd) {
        hwnd = GetAncestor(hwnd, GA_ROOT);
    }
    for (MainWindow* w : gWindows) {
        if (hwnd && AppShellNativeHwnd(w) == hwnd) {
            return w;
        }
    }
#endif
    // ng: gpui does not say which window is under the pointer
    return nullptr;
}

// ng: orig's TabsCtrl::MigrationEvent - a tab released outside the strip.
// gpui's drag is confined to the window the press started in, so the window
// under the cursor is asked of the OS (Windows only; elsewhere the tab always
// gets a window of its own).
void TabsView::OnBarUpOut(TabsView* self, gp::Ctx* cx, const gp::MouseUpEvent*) {
    MainWindow* win = self->win;
    const gp::DragPayload* drag = gp::WindowActiveDrag(cx);
    if (!drag || !str::Eq(FromGpui(drag->kind), StrL("sumatra-tab"))) {
        return;
    }
    WindowTab* tab = win->GetTab(drag->ix);
    if (!tab) {
        return;
    }
    MainWindow* releaseWnd = MainWindowUnderCursor();
    if (releaseWnd == win) {
        // don't re-add to the same window
        releaseWnd = nullptr;
    }
    MaybeMigrateTab(tab, releaseWnd);
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

void TabsView::OnMenuButton(TabsView* self, gp::Ctx* cx, const gp::ClickEvent*) {
    AppShellShowMenuBarTemp(self->win);
    gp::Notify(cx);
}

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
    for (const MenuItemModel& it : model->items) {
        if (it.separator) {
            menu->Separator();
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
        ->IntoEl();
}

static gp::El* BuildTab(MainWindow* win, gp::Ctx* cx, int idx, int tabDx) {
    TabsUI* ui = Ui(win);
    const gp::Theme& th = gp::ThemeNow(cx->app);
    WindowTab* tab = win->GetTab(idx);
    // while the Ctrl+Tab list is open it previews the tab it points at, as
    // orig's TabsCtrl::SetHighlighted does
    WindowTab* preview = TabSwitcherHighlighted(win);
    bool isSelected = preview ? (tab == preview) : (tab == win->CurrentTab());
    bool isUnderMouse = ui->highlighted == idx;
    Color bgCol = TabBgColor(tab, isSelected, isUnderMouse);
    Color textCol = TabTextColorForBackground(ThemeWindowTextColor(), bgCol);
    if (tab->loadState == WindowTab::LoadState::Error) {
        // a tab whose document failed to load shows its title in red, shaded
        // to stay readable on light and dark tab backgrounds
        textCol = IsLightColor(bgCol) ? MkRgb(0xC4, 0x1E, 0x1E) : MkRgb(0xFF, 0x6A, 0x6A);
    }

    gp::El* el = gp::Div(cx->a)
                     ->FlexRow()
                     ->W((float)tabDx)
                     ->H(gp::kFill)
                     ->Shrink0()
                     ->ItemsCenter()
                     ->Gap(2)
                     ->PadL(8)
                     ->PadR(isSelected ? 2.f : 8.f)
                     ->Bg(ToGpui(bgCol))
                     ->PathClick(GpuiDup(cx->a, fmt("sumatra-tab-%d", idx)))
                     ->BoundsOut(&ui->tabBounds[idx])
                     ->OnMouseDown(gp::ListenTo(Ui(win)->view, &TabsView::OnTabDown, (intptr_t)idx))
                     ->OnMouseUp(gp::ListenTo(Ui(win)->view, &TabsView::OnTabUp, (intptr_t)idx))
                     ->OnHover(gp::ListenTo(Ui(win)->view, &TabsView::OnTabHover, (intptr_t)idx));
    if (!IsPinnedTab(tab)) {
        el->OnDrag(GStrL("sumatra-tab"), idx);
        el->OnDragMove(gp::ListenTo(Ui(win)->view, &TabsView::OnTabDragMove));
    }
    // the pinned tab is a target too: a drop on its right half lands after it
    // (OnTabDrop refuses the position a pinned tab holds)
    el->OnDrop(GStrL("sumatra-tab"), gp::ListenTo(Ui(win)->view, &TabsView::OnTabDrop, (intptr_t)idx));

    gp::El* title = gp::TextEl(cx->a, GpuiDup(cx->a, tab->GetTabTitle()))
                        ->Font(12)
                        ->Fg(ToGpui(textCol))
                        ->Flex1()
                        ->MinW(0)
                        ->Truncate();
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
    if (isSelected) {
        el->Child(CloseButton(win, cx, idx));
    }
    return el;
}

// orig's CB_MENU caption button. The strip puts it before its tabs; a window
// without tabs has no strip, so the caption asks for it alone. Null while the
// menu bar is showing
gp::El* TabsUIMenuButton(MainWindow* win, gp::Ctx* cx) {
    if (win->isMenuBarVisible || AppShellNativeMenu()) {
        return nullptr;
    }
    TabsUI* ui = Ui(win);
    if (!ui->view.IsValid()) {
        ui->view = gp::EntityNewState<TabsView>(cx->app);
    }
    ui->view.Get(cx)->win = win;
    return gpc::Button::New(cx, GStrL("sumatra-tab-menu"))
        ->Icon(gp::IconName::Menu)
        ->Ghost()
        ->Compact()
        ->WithSize(gp::UiSize::Small)
        ->OnClick(gp::ListenTo(ui->view, &TabsView::OnMenuButton))
        ->IntoEl();
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
    // hidden (CB_MENU); ng: it shows the bar until the menu mode ends, where
    // orig opens the whole menu as one popup
    if (gp::El* menuBtn = TabsUIMenuButton(win, cx)) {
        bar->Child(menuBtn);
    }
    for (int i = 0; i < nTabs; i++) {
        bar->Child(BuildTab(win, cx, i, tabDx));
    }
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

    if (!ui->menuValid || ui->menuFor != ui->menuTab) {
        DeleteMenuModel(ui->menu);
        ui->menu = BuildTabContextMenu(win, ui->menuTab);
        ui->menuFor = ui->menuTab;
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
