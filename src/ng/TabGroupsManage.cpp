/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// ng: orig's TabGroupsManage.cpp is an overlapped window with an Edit, a
// VirtListBox it custom-draws and three buttons. Where the platform can have
// such a window (gui/ToolWindow.h) it is that, in orig's sizes; elsewhere a
// gpui Dialog with the same rows. The model (Settings.TabGroups, save /
// restore / delete) is orig's.

#include "gui/GpuiBridge.h"
#include "VirtKeys.h"
#if OS_WIN
#include "base/Win.h"
#endif

#include "gui/Dpi.h"
#include "gui/UIModels.h"

#include "Settings.h"
#include "AppSettings.h"
#include "DisplayMode.h"
#include "DocController.h"
#include "EngineBase.h"
#include "base/GuessFileType.h"
#include "EngineAll.h"
#include "SumatraPDF.h"
#include "MainWindow.h"
#include "WindowTab.h"
#include "Tabs.h"
#include "Translations.h"
#include "Theme.h"
#include "gui/AppShell.h"
#include "gui/DialogWidgets.h"
#include "gui/ToolWindow.h"
#include "TabGroupsManage.h"

#include "SumatraLog.h"

enum class TabGroupDialogMode {
    Save,
    Open,
};

struct TabGroupsDlg {
    MainWindow* win = nullptr;
    bool visible = false;
    TabGroupDialogMode mode = TabGroupDialogMode::Save;
    int sel = -1;
    gpui::InputState* editName = nullptr;
    bool wantFocus = false;
    float scrollY = 0;
    // orig's window, where the platform can have one; null: a dialog in the
    // frame
    ToolWindow* tw = nullptr;
    // the list's view height in that window, known once it was laid out
    float listViewDy = 0;
    // its main window closed and it was handed to `win`: not the window that
    // main window is asked for (orig's keeps the closed window's pointer)
    bool inherited = false;
};

// orig's gTabGroupsWnds: a window per main window and mode, so Save and
// Restore can be up together, and for each main window. ng: a closed one
// stays in the list (not visible) and is used again
static Vec<TabGroupsDlg*> gTabGroupsDlgs;
static TabGroupsDlg gNoTabGroupsDlg;
// the one the code below works on; every entry point sets it
static TabGroupsDlg* gTg = &gNoTabGroupsDlg;

// the dialog in the frame: there is room for one
static TabGroupsDlg* FrameDlg() {
    for (TabGroupsDlg* d : gTabGroupsDlgs) {
        if (d->visible && !d->tw) {
            return d;
        }
    }
    return nullptr;
}

// makes the dialog an event of `cx` belongs to the current one
static void UseDlgOf(gp::Ctx* cx) {
    for (TabGroupsDlg* d : gTabGroupsDlgs) {
        if (d->visible && d->tw && cx && ToolWindowGpui(d->tw) == cx->win) {
            gTg = d;
            return;
        }
    }
    if (TabGroupsDlg* d = FrameDlg()) {
        gTg = d;
    }
}

// orig's lookup in ShowTabGroupsDialog: the window of `win` in `mode`
static TabGroupsDlg* WindowDlgOf(MainWindow* win, TabGroupDialogMode mode) {
    for (TabGroupsDlg* d : gTabGroupsDlgs) {
        if (d->visible && d->tw && !d->inherited && d->win == win && d->mode == mode) {
            return d;
        }
    }
    return nullptr;
}

static TabGroupsDlg* UnusedDlg() {
    for (TabGroupsDlg* d : gTabGroupsDlgs) {
        if (!d->visible && !d->tw) {
            return d;
        }
    }
    auto* d = new TabGroupsDlg();
    VecAppend(gTabGroupsDlgs, d);
    return d;
}

struct TabGroupsView {
    static void OnOk(TabGroupsView* self, gp::Ctx* cx, const gp::ClickEvent*);
    static void OnCancel(TabGroupsView* self, gp::Ctx* cx, const gp::ClickEvent*);
    static void OnRowClick(TabGroupsView* self, gp::Ctx* cx, const gp::ClickEvent* ev, int64_t idx);
    static void OnDelete(TabGroupsView* self, gp::Ctx* cx, const gp::ClickEvent*);
    static void OnInput(TabGroupsView* self, gp::Ctx* cx, const gp::InputEvent* ev);
    static void OnScroll(TabGroupsView* self, gp::Ctx* cx, const gp::ScrollEvent* ev);
};

static gp::Entity<TabGroupsView> gTabGroupsView;

static Vec<TabGroup*>* Groups() {
    if (!gSettings->tabGroups) {
        gSettings->tabGroups = new Vec<TabGroup*>();
    }
    return gSettings->tabGroups;
}

static int GroupTabCount(TabGroup* g) {
    return g->tabFiles ? len(*g->tabFiles) : 0;
}

// the dialog in the frame; a window of its own is not the frame's business
bool IsTabGroupsDialogVisible() {
    return FrameDlg() != nullptr;
}

static void TabGroupsOpenToolWindow(MainWindow* win);

// closes the current one
static void CloseCurrentDlg() {
    if (!gTg->visible) {
        return;
    }
    gTg->visible = false;
    MainWindow* win = gTg->win;
    if (gTg->tw) {
        ToolWindowClose(gTg->tw);
        gTg->tw = nullptr;
    } else if (win && win->gpuiWin && gTg->editName) {
        gp::InputBlur(gTg->editName, win->gpuiWin->app, win->gpuiWin);
    }
    delete gTg->editName;
    gTg->editName = nullptr;
    AppShellInvalidate(win);
}

// the frame's
void CloseTabGroupsDialog() {
    if (TabGroupsDlg* d = FrameDlg()) {
        gTg = d;
        CloseCurrentDlg();
    }
}

static void ShowTabGroupsDialog(MainWindow* win, TabGroupDialogMode mode) {
    if (!IsMainWindowValidAndNotClosing(win)) {
        return;
    }
    if (TabGroupsDlg* open = WindowDlgOf(win, mode)) {
        // orig: BringWindowToTop() on the one that is open
        gTg = open;
        ToolWindowActivate(gTg->tw);
        return;
    }
    // the frame shows one dialog
    CloseTabGroupsDialog();
    gTg = UnusedDlg();
    gTg->inherited = false;
    gTg->win = win;
    gTg->mode = mode;
    gTg->sel = -1;
    gTg->scrollY = 0;
    gTg->visible = true;
    if (mode == TabGroupDialogMode::Save) {
        auto* s = new gp::InputState();
        s->focus = gp::FocusHandleNew(win->gpuiWin ? win->gpuiWin->app : nullptr);
        int groupNum = len(*Groups()) + 1;
        gp::InputSetValue(s, ToGpui(fmt("group #%d", groupNum)));
        gTg->editName = s;
        gTg->wantFocus = true;
    }
    TabGroupsOpenToolWindow(win);
    AppShellInvalidate(win);
}

void ShowSaveTabGroupDialog(MainWindow* win) {
    ShowTabGroupsDialog(win, TabGroupDialogMode::Save);
}

void ShowOpenTabGroupDialog(MainWindow* win) {
    ShowTabGroupsDialog(win, TabGroupDialogMode::Open);
}

// the windows of every main window list the same groups
static void InvalidateTabGroupWindows() {
    for (TabGroupsDlg* d : gTabGroupsDlgs) {
        if (d->visible && d->tw) {
            ToolWindowInvalidate(d->tw);
        }
    }
}

static void SaveTabGroup(MainWindow* win) {
    if (!gTg->editName) {
        return;
    }
    TempStr name = str::DupTemp(FromGpui(gp::InputValue(gTg->editName)));
    if (str::IsEmptyOrWhiteSpace(name)) {
        return;
    }

    auto* group = AllocStruct<TabGroup>();
    group->name = str::Dup(name);
    group->tabFiles = new Vec<TabFile*>();

    for (WindowTab* tab : win->Tabs()) {
        if (tab->IsAboutTab()) {
            continue;
        }
        if (len(tab->filePath) == 0) {
            continue;
        }
        auto* tf = AllocStruct<TabFile>();
        str::ReplaceWithCopy(&tf->path, tab->filePath);
        VecAppend(*group->tabFiles, tf);
    }
    logf("SaveTabGroup: '%s', %d tabs\n", name, GroupTabCount(group));
    VecAppend(*Groups(), group);
    ScheduleSaveSettings();
    CloseCurrentDlg();
    InvalidateTabGroupWindows();
}

// orig's TabGroupsWnd::OpenTabGroup: the group opens in this window when it
// has no documents (only the home tab, or nothing), otherwise in a new one
static void OpenTabGroup(MainWindow* win) {
    int sel = gTg->sel;
    Vec<TabGroup*>* groups = Groups();
    if (sel < 0 || sel >= len(*groups)) {
        return;
    }
    TabGroup* group = (*groups)[sel];
    if (!group->tabFiles || len(*group->tabFiles) == 0) {
        return;
    }
    bool hasFiles = false;
    for (WindowTab* tab : win->Tabs()) {
        if (!tab->IsAboutTab()) {
            hasFiles = true;
            break;
        }
    }
    logf("OpenTabGroup: '%s', %d tabs, new window %d\n", group->name, GroupTabCount(group), hasFiles ? 1 : 0);
    CloseCurrentDlg();
    MainWindow* targetWin = hasFiles ? CreateAndShowMainWindow(nullptr) : win;
    if (!targetWin) {
        return;
    }
    for (TabFile* tf : *group->tabFiles) {
        if (len(tf->path) == 0) {
            continue;
        }
        LoadDocument(targetWin, tf->path);
    }
}

static void FreeTabGroup(TabGroup* group) {
    if (!group) {
        return;
    }
    str::Free(group->name);
    if (group->tabFiles) {
        for (auto* tf : *group->tabFiles) {
            str::Free(tf->path);
            free(tf);
        }
        delete group->tabFiles;
    }
    free(group);
}

static void DeleteTabGroup() {
    int sel = gTg->sel;
    Vec<TabGroup*>* groups = Groups();
    if (sel < 0 || sel >= len(*groups)) {
        return;
    }
    TabGroup* group = (*groups)[sel];
    VecRemove(*groups, group);
    FreeTabGroup(group);
    // the other dialogs list the same groups
    for (TabGroupsDlg* d : gTabGroupsDlgs) {
        d->sel = -1;
    }
    InvalidateTabGroupWindows();
    ScheduleSaveSettings();
}

void TabGroupsView::OnOk(TabGroupsView*, gp::Ctx* cx, const gp::ClickEvent*) {
    UseDlgOf(cx);
    MainWindow* win = gTg->win;
    if (gTg->mode == TabGroupDialogMode::Save) {
        SaveTabGroup(win);
    } else {
        OpenTabGroup(win);
    }
    gp::Notify(cx);
}

void TabGroupsView::OnCancel(TabGroupsView*, gp::Ctx* cx, const gp::ClickEvent*) {
    UseDlgOf(cx);
    CloseCurrentDlg();
    gp::Notify(cx);
}

void TabGroupsView::OnScroll(TabGroupsView*, gp::Ctx* cx, const gp::ScrollEvent* ev) {
    UseDlgOf(cx);
    gTg->scrollY = ev->offsetY;
    gp::Notify(cx);
    AppShellInvalidate(gTg->win);
}

void TabGroupsView::OnDelete(TabGroupsView*, gp::Ctx* cx, const gp::ClickEvent*) {
    UseDlgOf(cx);
    DeleteTabGroup();
    AppShellInvalidate(gTg->win);
    gp::Notify(cx);
}

void TabGroupsView::OnRowClick(TabGroupsView*, gp::Ctx* cx, const gp::ClickEvent* ev, int64_t idx) {
    UseDlgOf(cx);
    gTg->sel = (int)idx;
    if (ev->clickCount >= 2) {
        // orig's double click: restore right away, or copy the name into the
        // edit box when saving
        if (gTg->mode == TabGroupDialogMode::Open) {
            OpenTabGroup(gTg->win);
        } else if (gTg->editName) {
            Vec<TabGroup*>* groups = Groups();
            if (idx < len(*groups)) {
                gp::InputSetValue(gTg->editName, ToGpui((*groups)[(int)idx]->name));
                // orig: EditSelectAll() and EditSetFocus()
                gTg->wantFocus = true;
                AppShellInvalidate(gTg->win);
            }
        }
    }
    gp::Notify(cx);
}

void TabGroupsView::OnInput(TabGroupsView*, gp::Ctx* cx, const gp::InputEvent* ev) {
    if (ev->kind != gp::InputEventKind::PressEnter) {
        return;
    }
    UseDlgOf(cx);
    SaveTabGroup(gTg->win);
    gp::Notify(cx);
}

// --- a window of its own (Windows) ------------------------------------------

// orig's sizes at 96 dpi: a 400 x 350 window; 8 around and between; a 23 high
// edit, 19 high rows in the 12 px app font, 25 high buttons 7 apart
constexpr int kTgWinDx = 400;
constexpr int kTgWinDy = 350;
constexpr float kTgPad = 8;
constexpr float kTgEditDy = 23;
constexpr float kTgRowDy = 19;
constexpr float kTgBtnDy = 25;
constexpr float kTgBtnPadDx = 12;
constexpr float kTgBtnGap = 7;
constexpr float kTgFontPx = 12;
constexpr float kTgScrollbarDx = 16;

static Str TabGroupsSaveTitle() {
    return Tr("Save Tab Group");
}

static Str TabGroupsRestoreTitle() {
    return Tr("Restore Tab Group");
}

static void TabGroupsSelect(int sel) {
    int n = len(*Groups());
    if (n == 0) {
        return;
    }
    gTg->sel = std::clamp(sel, 0, n - 1);
    if (gTg->listViewDy > 0) {
        gTg->scrollY = DialogScrollToRow(gTg->scrollY, gTg->sel, n, kTgRowDy, gTg->listViewDy);
    }
    AppShellInvalidate(gTg->win);
}

static gp::El* TabGroupsToolBuild(MainWindow*, gp::Ctx* cx) {
    UseDlgOf(cx);
    if (!gTg->visible || !gTg->tw || ToolWindowGpui(gTg->tw) != cx->win) {
        return nullptr;
    }
    if (!gTabGroupsView.IsValid()) {
        gTabGroupsView = gp::EntityNewState<TabGroupsView>(cx->app);
    }
    const gp::Theme& th = gp::ThemeNow(cx->app);
    bool isSave = gTg->mode == TabGroupDialogMode::Save;
    float font = kTgFontPx * ToolWindowSetUiFontPx(cx, kTgFontPx);
    gp::WinSize ws = gp::WindowSize(cx->win);

    gp::El* col = gp::Div(cx->a)->FlexCol()->W(gp::kFill)->Flex1()->MinH(0)->Pad(kTgPad);
    float listDy = ws.dipH - 2 * kTgPad - kTgPad - kTgBtnDy;
    if (isSave && gTg->editName) {
        gTg->editName->onChange = gp::ListenTo(gTabGroupsView, &TabGroupsView::OnInput);
        col->Child(gp::Div(cx->a)
                       ->FlexRow()
                       ->ItemsCenter()
                       ->W(gp::kFill)
                       ->H(kTgEditDy + kTgPad)
                       ->PadB(kTgPad)
                       ->Shrink0()
                       ->Child(gpc::Input::New(cx, GStrL("tabgroup-name"), gTg->editName)
                                   ->WithSize(gp::UiSize::Small)
                                   ->W(gp::kFill)
                                   ->IntoEl()));
        listDy -= kTgEditDy + kTgPad;
    }
    listDy = std::max(listDy, kTgRowDy);
    gTg->listViewDy = listDy;

    Vec<TabGroup*>* groups = Groups();
    gp::El* list = gp::Div(cx->a)
                       ->Id(GStrL("tabgroup-list"))
                       ->FlexCol()
                       ->W(gp::kFill)
                       ->H(listDy)
                       ->Shrink0()
                       ->ScrollY(gTg->scrollY)
                       ->ScrollFromPath()
                       ->OnScroll(gp::ListenTo(gTabGroupsView, &TabGroupsView::OnScroll));
    gp::Rgba countFg = ToGpui(AccentColor(ThemeWindowTextColor(), 80));
    // gpui's scrollbar is drawn over the rows' right end
    bool scrolls = (float)len(*groups) * kTgRowDy > listDy;
    for (int i = 0; i < len(*groups); i++) {
        TabGroup* g = (*groups)[i];
        gp::El* row = gp::Div(cx->a)
                          ->FlexRow()
                          ->W(gp::kFill)
                          ->H(kTgRowDy)
                          ->Shrink0()
                          ->ItemsCenter()
                          ->PadL(4)
                          ->PadR(scrolls ? kTgScrollbarDx : 4)
                          ->PathClick(GpuiDup(cx->a, fmt("tabgroup-row-%d", i)))
                          ->OnClick(gp::ListenTo(gTabGroupsView, &TabGroupsView::OnRowClick, (intptr_t)i));
        if (i == gTg->sel) {
            row->Bg(ToGpui(AccentColor(ThemeWindowControlBackgroundColor(), 30)));
        }
        row->Child(
            gp::TextEl(cx->a, GpuiDup(cx->a, g->name))->Font(font)->Fg(th.foreground)->Flex1()->MinW(0)->Truncate());
        TempStr count = fmt("%d tabs", GroupTabCount(g));
        row->Child(gp::TextEl(cx->a, GpuiDup(cx->a, count))->Font(font)->Fg(countFg)->Shrink0());
        list->Child(row);
    }
    col->Child(list);

    auto button = [&](gp::Str id, Str label, gp::Listener onClick, bool isDefault, bool disabled) {
        gpc::Button* b = gpc::Button::New(cx, id)->Label(ToGpui(label))->Disabled(disabled)->OnClick(onClick);
        if (isDefault) {
            b->Primary();
        }
        return b->IntoEl()->H(kTgBtnDy)->PadX(kTgBtnPadDx)->Shrink0();
    };
    gp::El* buttons = gp::Div(cx->a)
                          ->FlexRow()
                          ->JustifyEnd()
                          ->ItemsCenter()
                          ->W(gp::kFill)
                          ->H(kTgPad + kTgBtnDy)
                          ->PadT(kTgPad)
                          ->Gap(kTgBtnGap)
                          ->Shrink0();
    buttons->Child(button(GStrL("tabgroup-cancel"), Tr("Cancel"),
                          gp::ListenTo(gTabGroupsView, &TabGroupsView::OnCancel), false, false));
    buttons->Child(button(GStrL("tabgroup-delete"), Tr("Delete"),
                          gp::ListenTo(gTabGroupsView, &TabGroupsView::OnDelete), false, gTg->sel < 0));
    buttons->Child(button(GStrL("tabgroup-ok"), isSave ? Tr("Save") : Tr("Restore"),
                          gp::ListenTo(gTabGroupsView, &TabGroupsView::OnOk), true, false));
    col->Child(buttons);

    if (gTg->wantFocus && gTg->editName) {
        gp::InputFocus(gTg->editName, cx->app, cx->win);
        gp::InputSelectAll(gTg->editName, cx->app, cx->win);
        gTg->wantFocus = cx->win->input != gTg->editName;
    }
    return col;
}

// orig's closeOnEsc, Enter for the default button, and the list's keys while
// the edit does not have them
static bool TabGroupsToolOnKey(MainWindow*, gp::Ctx* cx, const gp::KeyEvent* ev) {
    UseDlgOf(cx);
    if (!gTg->visible) {
        return false;
    }
    if (ev->vk == VK_ESCAPE) {
        CloseCurrentDlg();
        return true;
    }
    if (ev->ctrl || ev->alt) {
        return false;
    }
    if (ev->vk == VK_RETURN) {
        MainWindow* win = gTg->win;
        if (gTg->mode == TabGroupDialogMode::Save) {
            SaveTabGroup(win);
        } else {
            OpenTabGroup(win);
        }
        return true;
    }
    bool editFocused = cx->win->input && cx->win->input->focused;
    if (editFocused) {
        return false;
    }
    int n = len(*Groups());
    int page = std::max((int)(gTg->listViewDy / kTgRowDy) - 1, 1);
    int sel = gTg->sel;
    switch (ev->vk) {
        case VK_UP:
            TabGroupsSelect(sel < 0 ? 0 : sel - 1);
            return true;
        case VK_DOWN:
            TabGroupsSelect(sel + 1);
            return true;
        case VK_PRIOR:
            TabGroupsSelect(std::max(sel, 0) - page);
            return true;
        case VK_NEXT:
            TabGroupsSelect(std::max(sel, 0) + page);
            return true;
        case VK_HOME:
            TabGroupsSelect(0);
            return true;
        case VK_END:
            TabGroupsSelect(n - 1);
            return true;
    }
    return false;
}

// the close box, or the main window going with no other left
static void TabGroupsToolOnClosed(MainWindow*) {
    for (TabGroupsDlg* d : gTabGroupsDlgs) {
        if (!d->tw || ToolWindowIsLive(d->tw)) {
            continue;
        }
        d->tw = nullptr;
        if (!IsMainWindowValid(d->win)) {
            d->win = nullptr;
        }
        gTg = d;
        CloseCurrentDlg();
    }
}

// orig's window has no owner and stays when its main window closes (holding
// that window). Here it belongs to the main window that is left: Save saves
// that window's tabs
static void TabGroupsToolOnOwnerClosed(MainWindow* newOwner) {
    for (TabGroupsDlg* d : gTabGroupsDlgs) {
        if (d->visible && d->tw && d->win != newOwner && ToolWindowOwner(d->tw) == newOwner) {
            d->win = newOwner;
            d->inherited = true;
        }
    }
}

static void TabGroupsOpenToolWindow(MainWindow* win) {
    if (gTg->tw || !ToolWindowsAvailable()) {
        return;
    }
    // orig: WS_OVERLAPPEDWINDOW, no owner, 400 x 350, centered on the frame
    ToolWindowDesc desc;
    bool isSave = gTg->mode == TabGroupDialogMode::Save;
    desc.name = isSave ? "tabgroupsave" : "tabgrouprestore";
    desc.title = isSave ? TabGroupsSaveTitle : TabGroupsRestoreTitle;
    desc.onOwnerClosed = TabGroupsToolOnOwnerClosed;
    desc.frame = ToolWinFrame::Overlapped;
    desc.resize = ToolWinResize::Resizable;
    desc.owner = ToolWinOwner::TopLevel;
    desc.build = TabGroupsToolBuild;
    desc.onKey = TabGroupsToolOnKey;
    desc.onClosed = TabGroupsToolOnClosed;
    int dpi = std::max(AppShellWindowDpi(win), 96);
    Size outer{MulDiv(kTgWinDx, dpi, 96), MulDiv(kTgWinDy, dpi, 96)};
    gTg->listViewDy = 0;
    gTg->tw = ToolWindowOpen(desc, win, ToolWindowCenteredOuter(win, outer));
}

gp::El* TabGroupsDialogBuild(MainWindow* win, gp::Ctx* cx) {
    TabGroupsDlg* frameDlg = FrameDlg();
    if (!frameDlg || frameDlg->win != win) {
        return nullptr;
    }
    gTg = frameDlg;
    if (!gTabGroupsView.IsValid()) {
        gTabGroupsView = gp::EntityNewState<TabGroupsView>(cx->app);
    }
    const gp::Theme& th = gp::ThemeNow(cx->app);
    bool isSave = gTg->mode == TabGroupDialogMode::Save;

    gp::El* body = gp::Div(cx->a)->FlexCol()->Gap(8);
    if (isSave && gTg->editName) {
        gTg->editName->onChange = gp::ListenTo(gTabGroupsView, &TabGroupsView::OnInput);
        body->Child(gpc::Input::New(cx, GStrL("tabgroup-name"), gTg->editName)
                        ->WithSize(gp::UiSize::Small)
                        ->W(gp::kFill)
                        ->IntoEl());
    }

    Vec<TabGroup*>* groups = Groups();
    gp::El* list = gp::Div(cx->a)
                       ->Id(GStrL("tabgroup-list"))
                       ->FlexCol()
                       ->W(gp::kFill)
                       ->MinH(120)
                       ->MaxH(240)
                       ->ScrollY(gTg->scrollY)
                       ->ScrollFromPath()
                       ->OnScroll(gp::ListenTo(gTabGroupsView, &TabGroupsView::OnScroll))
                       ->Border(1, th.border);
    for (int i = 0; i < len(*groups); i++) {
        TabGroup* g = (*groups)[i];
        gp::El* row = gp::Div(cx->a)
                          ->FlexRow()
                          ->W(gp::kFill)
                          ->H(24)
                          ->Shrink0()
                          ->ItemsCenter()
                          ->PadX(4)
                          ->PathClick(GpuiDup(cx->a, fmt("tabgroup-row-%d", i)))
                          ->OnClick(gp::ListenTo(gTabGroupsView, &TabGroupsView::OnRowClick, (intptr_t)i));
        if (i == gTg->sel) {
            row->Bg(th.selection);
        }
        row->Child(
            gp::TextEl(cx->a, GpuiDup(cx->a, g->name))->Font(13)->Fg(th.foreground)->Flex1()->MinW(0)->Truncate());
        TempStr count = fmt("%d tabs", GroupTabCount(g));
        row->Child(gp::TextEl(cx->a, GpuiDup(cx->a, count))->Font(11)->Fg(th.mutedFg)->Shrink0());
        list->Child(row);
    }
    body->Child(list);
    body->Child(gpc::Button::New(cx, GStrL("tabgroup-delete"))
                    ->Label(ToGpui(Tr("Delete")))
                    ->WithSize(gp::UiSize::Small)
                    ->Disabled(gTg->sel < 0)
                    ->OnClick(gp::ListenTo(gTabGroupsView, &TabGroupsView::OnDelete))
                    ->IntoEl());

    Str title = isSave ? Tr("Save Tab Group") : Tr("Restore Tab Group");
    Str okText = isSave ? Tr("Save") : Tr("Restore");
    DlgSetDefault(cx, gp::ListenTo(gTabGroupsView, &TabGroupsView::OnOk));
    gp::El* dlg = gpc::Dialog::New(cx)
                      ->Open(true)
                      ->Title(ToGpui(title))
                      ->Body(body)
                      ->W(400)
                      ->OkText(ToGpui(okText))
                      ->CancelText(ToGpui(Tr("Cancel")))
                      ->ShowCancel(true)
                      ->OnOk(gp::ListenTo(gTabGroupsView, &TabGroupsView::OnOk))
                      ->OnCancel(gp::ListenTo(gTabGroupsView, &TabGroupsView::OnCancel))
                      ->OnClose(gp::ListenTo(gTabGroupsView, &TabGroupsView::OnCancel))
                      ->IntoEl(gp::WindowSize(cx->win));

    // ng: stays armed until the field really is the window's focused input -
    // the popup the command came from can take the focus back on a later frame
    if (gTg->wantFocus && gTg->editName) {
        gp::InputFocus(gTg->editName, cx->app, cx->win);
        gp::InputSelectAll(gTg->editName, cx->app, cx->win);
        gTg->wantFocus = cx->win->input != gTg->editName;
    }
    return dlg;
}
