/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "gui/Dpi.h"
#include "base/File.h"
#include "base/Win.h"

#include "gui/UIModels.h"
#include "gui/Layout.h"
#include "gui/win/WinGui.h"
#include "gui/PlatformFont.h"
#include "gui/Gfx.h"
#include "gui/VirtCtrl.h"
#include "gui/win/TabsCtrl.h"

#include "Settings.h"
#include "AppSettings.h"
#include "DocController.h"
#include "EngineBase.h"
#include "base/GuessFileType.h"
#include "EngineAll.h"
#include "DisplayModel.h"
#include "GlobalPrefs.h"
#include "SumatraPDF.h"
#include "SumatraProperties.h"
#include "MainWindow.h"
#include "WindowTab.h"
#include "Commands.h"
#include "CommandAvailability.h"
#include "FindBar.h"
#include "SelectionToolbar.h"
#include "Menu.h"
#include "TableOfContents.h"
#include "Tabs.h"
#include "FileHistory.h"
#include "Theme.h"
#include "Translations.h"

// always full path (FullPathInTitle only affects tab/window title text).
// Append size when GetSize succeeds (may fail for offline network paths).
// Used by Tabs and Toolbar (toolbar was overwriting tooltips with path-only).
// full path + size (if available); optional dirty suffix for unsaved annotations
TempStr MakeTabTooltipTemp(Str path, bool dirty) {
    if (!path) {
        return Str{};
    }
    TempStr tip;
    i64 size = file::GetSize(path);
    if (size >= 0) {
        tip = fmt("%s  %s", path, str::FormatSizeShortTemp(size, nullptr));
    } else {
        tip = path;
    }
    if (dirty) {
        tip = str::JoinTemp(tip, StrL(" "), _TRA("(unsaved annotations)"));
    }
    return tip;
}

static void UpdateTabTitle(WindowTab* tab) {
    if (!tab) {
        return;
    }
    MainWindow* win = tab->win;
    int idx = win->GetTabIdx(tab);
    Str title = tab->GetTabTitle();
    bool dirty = false;
    if (tab->AsFixed()) {
        dirty = EngineHasUnsavedAnnotations(tab->AsFixed()->GetEngine());
    }
    TempStr tooltip = MakeTabTooltipTemp(tab->filePath, dirty);
    win->tabsCtrl->SetTextAndTooltip(idx, title, tooltip);
}

int GetTabbarHeight(HWND hwnd, float factor) {
    DpiSetFromHwnd(hwnd);
    PlatformFont* font = GetAppFont();
    int tabDy = DpiScale(kTabBarDy);
    int fontDyWithPadding = PlatformFontLineHeight(font) + DpiScale(2);
    tabDy = std::max(fontDyWithPadding, tabDy);
    // Guard against the bad per-window DPI Wine reports (93e5b4e47: the tab bar
    // and caption came out tiny). Wine only, deliberately: we are PerMonitorV2,
    // so DpiScale(HWND_DESKTOP) is the *system* (primary monitor) DPI, which
    // doesn't follow the window between monitors. Using it as a floor made the
    // tab bar too tall on any monitor scaled lower than the primary
    // (discussion #4831).
    if (IsRunningOnWine()) {
        int minDy = DpiScaleByDpi(DpiGetForHwnd(HWND_DESKTOP), kTabBarDy);
        int minFontDy = PlatformFontLineHeight(font) + DpiScaleByDpi(DpiGetForHwnd(HWND_DESKTOP), 2);
        minDy = std::max(minFontDy, minDy);
        tabDy = std::max(tabDy, minDy);
        int res = (int)((float)tabDy * factor);
        logf(
            "GetTabbarHeight: hwnd=%p factor=%g dpi=%d desktopDpi=%d tabDyScaled=%d fontDy=%d "
            "minDy=%d result=%d\n",
            hwnd, factor, DpiGetForHwnd(hwnd), DpiGetForHwnd(HWND_DESKTOP), DpiScale(kTabBarDy), fontDyWithPadding,
            minDy, res);
        return res;
    }
    return (int)((float)tabDy * factor);
}

#if 0
static inline Size GetTabSize(HWND hwnd) {
    int dx = DpiScale(std::max(gGlobalPrefs->tabWidth, kTabMinDx));
    int dy = GetTabbarHeight(hwnd);
    return Size(dx, dy);
}
#endif

static void ShowTabBar(MainWindow* win, bool show) {
    if (show == win->tabsVisible) {
        return;
    }
    win->tabsVisible = show;
    if (win->tabsCtrl) {
        win->tabsCtrl->SetIsVisible(show);
    }
    ScheduleUiUpdate(win);
}

// also shows/hides the tabbar when necessary
void UpdateTabWidth(MainWindow* win) {
    int nTabs = win->TabCount();
    // Count real documents only: Home (and Favorites) alone should not keep the
    // tab bar open. Showing Home as a single tab after the last document closed
    // fought RelayoutCaption (which hides tabs with no file tabs) and caused a
    // continuous TabsCtrl repaint storm (issue #5861).
    int nDocTabs = 0;
    for (int i = 0; i < nTabs; i++) {
        WindowTab* t = win->GetTab(i);
        if (t && !t->IsNonDocumentTab()) {
            nDocTabs++;
        }
    }
    bool showSingleTab = SettingsUseTabs() || win->tabsInTitlebar;
    bool showTabs = (nDocTabs > 1) || (showSingleTab && (nDocTabs > 0));
    // TabWidth is stored in logical (96-DPI) units, same as other layout
    // settings; convert to physical pixels so HiDPI monitors honor the value
    // (issue #3850). Height already uses DpiScale via GetTabbarHeight.
    if (win->tabsCtrl) {
        HWND hwnd = win->tabsCtrl->hwnd ? win->tabsCtrl->hwnd : win->hwndFrame;
        win->tabsCtrl->tabDefaultDx = DpiScale(gGlobalPrefs->tabWidth);
    }
    // Lay out only when the bar stays visible. Hiding it right after
    // TabCtrl_SetItemSize invalidated the control leaves a pending WM_PAINT for
    // a window we're about to hide (issue #5861). It's laid out when shown again.
    if (!showTabs) {
        ShowTabBar(win, false);
        return;
    }
    ShowTabBar(win, true);
    if (win->tabsCtrl) {
        win->tabsCtrl->LayoutTabs();
    }
}

void RemoveTab(WindowTab* tab) {
    UpdateTabFileDisplayStateForTab(tab);
    MainWindow* win = tab->win;
    win->tabSelectionHistory->Remove(tab);
    int idx = win->GetTabIdx(tab);
    WindowTab* tab2 = win->tabsCtrl->RemoveTab<WindowTab*>(idx);
    ReportIf(tab != tab2);
    bool closedCurrentTab = (tab == win->CurrentTab());
    if (closedCurrentTab) {
        win->ctrl = nullptr;
        win->currentTabTemp = nullptr;
    }
    // Hide the bar before selecting Home so we don't flash Home's close button
    // when the last document closes (formerly a special case; now UpdateTabWidth
    // itself treats Home-only as "no tabs").
    UpdateTabWidth(win);

    int nTabs = win->TabCount();
    if (nTabs < 1) {
        return;
    }
    if (!closedCurrentTab) {
        return;
    }
    // if the removed tab was the current one, select another
#if 0
    WindowTab* curr = win->CurrentTab();
    WindowTab* newCurrent = curr;
    if (!curr || newCurrent == tab) {
        // TODO(tabs): why do I need win->tabSelectionHistory.Size() > 0
        if (win->tabSelectionHistory->Size() > 0) {
            newCurrent = win->tabSelectionHistory->Pop();
        } else {
            newCurrent = win->GetTab(0);
        }
    }
    int newIdx = win->GetTabIdx(newCurrent);
    win->tabsCtrl->SetSelected(newIdx);
    tab = win->CurrentTab();
    LoadModelIntoTab(tab);
#else
    // select tab to the right or to the left if nothing to the right
    int newIdx = idx;
    int lastIdx = nTabs - 1;
    newIdx = std::min(newIdx, lastIdx);
    win->tabsCtrl->SetSelected(newIdx);
    tab = win->CurrentTab();
    // seen in crash report that tab was WindowTab::Type::None
    // TODO: don't know how it could have happened
    if (tab && tab->type != WindowTab::Type::None) {
        LoadModelIntoTab(tab);
    }
#endif
}

static void CloseWindowIfNoDocuments(MainWindow* win) {
    for (auto& tab : win->Tabs()) {
        if (!tab->IsAboutTab()) {
            return;
        }
    }
    // no tabs or only about tab
    CloseWindow(win, true, false);
}

static void MaybeMigrateTab(WindowTab* tab, MainWindow* newWin, Point releasePt) {
    MainWindow* oldWin = tab->win;

    // Home / Favorites tabs stay in their window
    if (tab->IsNonDocumentTab()) {
        return;
    }

    // don't migrate if it's only one document tab and not
    // dragging over a window
    int nDocTabs = 0;
    for (auto& t : oldWin->Tabs()) {
        if (t->IsNonDocumentTab()) continue;
        nDocTabs++;
    }
    if (nDocTabs == 1 && !newWin) return;

    auto* engine = tab->GetEngine();
    if (EngineHasUnsavedAnnotations(engine)) {
        return;
    }

    RemoveTab(tab);

    if (!newWin) {
        if (IsZoomed(oldWin->hwndFrame)) {
            // dragging a tab out of a maximized window: like Chrome, create a
            // normal (non-maximized) window with the size the source window
            // would have when restored, positioned at the cursor so it lands
            // on the new window's tab strip
            WINDOWPLACEMENT wp{};
            wp.length = sizeof(wp);
            GetWindowPlacement(oldWin->hwndFrame, &wp);
            int dx = wp.rcNormalPosition.right - wp.rcNormalPosition.left;
            int dy = wp.rcNormalPosition.bottom - wp.rcNormalPosition.top;
            int x = releasePt.x - DpiScale(100);
            int y = releasePt.y - (GetTabbarHeight(oldWin->hwndFrame) / 2);
            Rect rect = ShiftRectToWorkArea(Rect(x, y, dx, dy), oldWin->hwndFrame, true);
            newWin = CreateAndShowMainWindow(nullptr, false);
            if (!newWin) {
                return;
            }
            HwndMoveWindow(newWin->hwndFrame, &rect);
            ShowMainWindow(newWin, WIN_STATE_NORMAL);
        } else {
            newWin = CreateAndShowMainWindow(nullptr);
        }
        if (!newWin) {
            return;
        }
    }

    // TODO: we should be able to just slide the existing tab
    // into the new window, preserving its controller and
    // the entire state, but this crashes/renders badly etc.
    // More work needed. The code that should work but doesn't:
    // tab->win = newWin;
    // newWin->currentTabTemp = AddTabToWindow(newWin, tab);
    // newWin->ctrl = tab->ctrl;
    // UpdateUiForCurrentTab(newWin);
    // newWin->showSelection = tab->selectionOnPage != nullptr;
    // HwndSetFocus(newWin->hwndFrame);
    // newWin->RedrawAll(true);
    // TabsOnChangedDoc(newWin);
    WindowTab* newTab = new WindowTab(newWin);
    newTab->SetFilePath(tab->filePath);
    newTab->SetDisplayName(tab->displayName);
    newWin->currentTabTemp = AddTabToWindow(newWin, newTab);
    newWin->ctrl = nullptr;
    LoadArgs args(tab->filePath, newWin);
    args.SetDisplayName(tab->displayName);
    args.forceReuse = true;
    args.noSavePrefs = true;
    LoadDocument(&args);
    delete tab;

    CloseWindowIfNoDocuments(oldWin);
}

// Selects the given tab (0-based index)
// tabIndex can come from settings file so must be sanitized
void TabsSelect(MainWindow* win, int tabIndex) {
    auto tabs = win->Tabs();
    int nTabs = len(tabs);
    logf("TabsSelect: tabIndex: %d, nTabs: %d\n", tabIndex, nTabs);
    if (nTabs == 0) {
        logf("TabsSelect: skipping because nTabs = %d\n", nTabs);
        return;
    }
    if (tabIndex < 0 || tabIndex >= nTabs) {
        tabIndex = 0;
        logf("TabsSelect: fixing tabIndex to 0\n");
    }
    TabsCtrl* tabsCtrl = win->tabsCtrl;
    int currIdx = tabsCtrl->GetSelected();
    if (tabIndex == currIdx) {
        return;
    }

    // same work as in onSelectionChanging and onSelectionChanged
    SaveCurrentWindowTab(win);
    int prevIdx = tabsCtrl->SetSelected(tabIndex);
    if (prevIdx < 0) {
        return;
    }
    WindowTab* tab = tabs[tabIndex];
    // page-info tip is restored via MainWindow::pageInfoWanted in LoadModelIntoTab
    LoadModelIntoTab(tab);
}

// clang-format off
extern bool SaveAnnotationsToExistingFile(WindowTab*);
extern bool SaveAnnotationsToMaybeNewPdfFile(WindowTab*);

static MenuDef menuDefContextTab[] = {
    // these top items are removed unless the document has unsaved changes;
    // text matches the "Unsaved changes" close dialog
    {
        _TRN("&Save changes to existing PDF"),
        CmdSaveAnnotations,
    },
    {
        _TRN("Save changes to &new PDF"),
        CmdSaveAnnotationsNewFile,
    },
    {
        _TRN("&Discard changes"),
        CmdDiscardChanges,
    },
    {
        kMenuSeparator,
        0,
    },
    {
        _TRN("Properties..."),
        CmdProperties,
    },
    {
        _TRN("Show in folder"),
        CmdShowInFolder,
    },
    {
        _TRN("Copy File Path"),
        CmdCopyFilePath,
    },
    {
        _TRN("Open In New Window"),
        CmdDuplicateInNewWindow,
    },
    {
        _TRN("Change Tab Color"),
        CmdSetTabColor,
    },
    {
        kMenuSeparator,
        0,
    },
    {
        _TRN("Close"),
        CmdClose,
    },
    {
        _TRN("Close Other Tabs"),
        CmdCloseOtherTabs,
    },
    {
        _TRN("Close Tabs To The Right"),
        CmdCloseTabsToTheRight,
    },
    {
        _TRN("Close Tabs To The Left"),
        CmdCloseTabsToTheLeft,
    },
    {
        _TRN("Close All Tabs"),
        CmdCloseAllTabs,
    },
    {
        kMenuSeparator,
        0,
    },
    {
        _TRN("Save Tab Group"),
        CmdTabGroupSave,
    },
    {
        _TRN("Restore Tab Group"),
        CmdTabGroupRestore,
    },
    {
        nullptr,
        0,
    },
};
// clang-format on

// create a new window if win==nullptr
void CollectTabsToClose(MainWindow* win, WindowTab* currTab, Vec<WindowTab*>& toCloseOther,
                        Vec<WindowTab*>& toCloseRight, Vec<WindowTab*>& toCloseLeft) {
    int nTabs = win->TabCount();
    bool seenCurrent = false;
    for (int i = 0; i < nTabs; i++) {
        WindowTab* tab = win->Tabs()[i];
        if (tab->IsAboutTab()) {
            continue;
        }
        if (currTab == tab) {
            seenCurrent = true;
            continue;
        }
        toCloseOther.Append(tab);
        if (seenCurrent) {
            toCloseRight.Append(tab);
        } else {
            toCloseLeft.Append(tab);
        }
    }
}

void CloseAllTabs(MainWindow* win) {
    // can't close while iterating over the tabs so collect them first
    Vec<WindowTab*> toClose;
    int nTabs = win->TabCount();
    for (int i = 0; i < nTabs; i++) {
        WindowTab* t = win->GetTab(i);
        if (t->IsAboutTab()) {
            continue;
        }
        toClose.Append(t);
    }
    for (WindowTab* t : toClose) {
        CloseTab(t, false);
    }
}

// TODO: add "Move to another window" sub-menu
static void TabsContextMenu(TabsCtrl* tabsCtrl, VirtMouseEvent* ev) {
    MainWindow* win = FindMainWindowByHwnd(tabsCtrl->hwnd);
    if (!win) {
        return;
    }
    TabsCtrl::MouseState tabState = tabsCtrl->TabStateFromMousePosition(ev->ptWindow);
    int tabIdx = tabState.tabIdx;
    if (tabIdx < 0) {
        return;
    }

    WindowTab* tabUnderMouse = win->Tabs()[tabIdx];
    if (tabUnderMouse->IsAboutTab()) {
        return;
    }
    Point pt = HwndClientToScreen(tabsCtrl->hwnd, ev->ptWindow);

    Vec<WindowTab*> toCloseOther;
    Vec<WindowTab*> toCloseRight;
    Vec<WindowTab*> toCloseLeft;
    CollectTabsToClose(win, tabUnderMouse, toCloseOther, toCloseRight, toCloseLeft);

    DisplayModel* dmTab = tabUnderMouse->AsFixed();
    EngineBase* tabEngine = dmTab ? dmTab->GetEngine() : nullptr;

    // Build the command context for the tab under the mouse, which may differ
    // from the current tab that NewAppCommandCtx() keys off. Without a context
    // command availability is evaluated against an empty (no-document) state,
    // which removes almost every item (leaving only "Restore Tab Group").
    BuildMenuCtx* ctx = NewBuildMenuCtx(tabUnderMouse, Point{0, 0});
    ctx->tab = tabUnderMouse;
    ctx->isDocLoaded = true; // tabUnderMouse is a real (non-about) document tab
    ctx->filePath = tabUnderMouse->filePath;
    ctx->supportsAnnots = EngineSupportsAnnotations(tabEngine) && !win->isFullScreen;
    ctx->hasUnsavedAnnotations = EngineHasUnsavedAnnotations(tabEngine);
    ctx->canCloseOtherTabs = len(toCloseOther) > 0;
    ctx->canCloseTabsToRight = len(toCloseRight) > 0;
    ctx->canCloseTabsToLeft = len(toCloseLeft) > 0;

    HMENU popup = BuildMenuFromDef(menuDefContextTab, CreatePopupMenu(), ctx);
    DeleteBuildMenuCtx(ctx);

    if (!tabUnderMouse->ctrl) {
        MenuSetEnabled(popup, CmdSetTabColor, false);
    }
    // the save/discard items only make sense when the document has unsaved
    // changes (e.g. filled form fields, added annotations); otherwise remove
    // them, then clean up the separator they leave behind
    if (!EngineHasUnsavedAnnotations(tabEngine)) {
        DeleteMenu(popup, CmdSaveAnnotations, MF_BYCOMMAND);
        DeleteMenu(popup, CmdSaveAnnotationsNewFile, MF_BYCOMMAND);
        DeleteMenu(popup, CmdDiscardChanges, MF_BYCOMMAND);
        RemoveBadMenuSeparators(popup);
    }
    MarkMenuOwnerDraw(popup);
    uint flags = TPM_RETURNCMD | TPM_RIGHTBUTTON;
    int cmdId = TrackPopupMenu(popup, flags, pt.x, pt.y, 0, win->hwndFrame, nullptr);
    FreeMenuOwnerDrawInfoData(popup);
    DestroyMenu(popup);
    switch (cmdId) {
        case CmdClose: {
            CloseTab(tabUnderMouse, false);
            return;
        }

        case CmdCloseAllTabs: {
            CloseAllTabs(win);
            return;
        }
        case CmdCloseOtherTabs: {
            for (WindowTab* t : toCloseOther) {
                CloseTab(t, false);
            }
            return;
        }
        case CmdCloseTabsToTheRight: {
            for (WindowTab* t : toCloseRight) {
                CloseTab(t, false);
            }
            return;
        }
        case CmdCloseTabsToTheLeft: {
            for (WindowTab* t : toCloseLeft) {
                CloseTab(t, false);
            }
            return;
        }
        case CmdShowInFolder: {
            SumatraOpenPathInDefaultFileManager(tabUnderMouse->filePath);
            return;
        }
        case CmdCopyFilePath: {
            CopyFilePath(tabUnderMouse);
            return;
        }
        case CmdDuplicateInNewWindow: {
            DuplicateTabInNewWindow(tabUnderMouse);
            return;
        }
        case CmdProperties: {
            ShowProperties(win->hwndFrame, tabUnderMouse->ctrl);
            return;
        }
        case CmdSetTabColor: {
            // handled in FrameOnCommand; forward the tab under the mouse encoded
            // in lp (the command palette invokes it on the current tab, lp == 0)
            HwndSendCommand(win->hwndFrame, CmdSetTabColor, (LPARAM)tabUnderMouse);
            return;
        }
        case CmdSaveAnnotations: {
            SaveAnnotationsToExistingFile(tabUnderMouse);
            return;
        }
        case CmdSaveAnnotationsNewFile: {
            SaveAnnotationsToMaybeNewPdfFile(tabUnderMouse);
            return;
        }
        case CmdDiscardChanges: {
            // revert to the on-disk version, discarding unsaved changes
            TabsSelect(win, tabIdx);
            ReloadDocument(win, false);
            return;
        }
    }
    // everything we forward to main window
    HwndSendCommand(win->hwndFrame, cmdId);
}

static void MainWindowTabClosed(MainWindow* win, TabsCtrl::ClosedEvent* ev) {
    int closedTabIdx = ev->tabIdx;
    WindowTab* tab = win->GetTab(closedTabIdx);
    CloseTab(tab, false);
}

static void MainWindowTabSelectionChanging(MainWindow* win, TabsCtrl::SelectionChangingEvent* ev) {
    // TODO: Should we allow the switch of the tab if we are in process of printing?
    SaveCurrentWindowTab(win);
    ev->preventChanging = false;
}

static void MainWindowTabSelectionChanged(MainWindow* win, TabsCtrl::SelectionChangedEvent* /*ev*/) {
    int currentIdx = win->tabsCtrl->GetSelected();
    WindowTab* tab = win->Tabs()[currentIdx];
    // page-info tip is restored via MainWindow::pageInfoWanted in LoadModelIntoTab
    LoadModelIntoTab(tab);
}

static void MainWindowTabMigration(MainWindow* win, TabsCtrl::MigrationEvent* ev) {
    WindowTab* tab = win->GetTab(ev->tabIdx);
    MainWindow* releaseWnd = nullptr;
    HWND hwnd = HwndWindowFromPoint(ev->releasePoint);
    if (hwnd != nullptr) {
        releaseWnd = FindMainWindowByHwnd(hwnd);
    }
    if (releaseWnd == win) {
        // don't re-add to the same window
        releaseWnd = nullptr;
    }
    MaybeMigrateTab(tab, releaseWnd, ev->releasePoint);
}

void CreateTabbar(MainWindow* win) {
    if (win->frameDpi > 0) {
        DpiSet(win->frameDpi, win->frameDpi);
    }
    TabsCtrl::CreateArgs args;
    args.parent = win->hwndFrame;
    args.withToolTips = true;
    args.font = GetAppFont();
    // logical TabWidth → physical (see UpdateTabWidth / issue #3850)
    args.tabDefaultDx = DpiScale(gGlobalPrefs->tabWidth);
    args.isRtl = false; // LTR hwnd; RTL tab order follows parent frame (see UpdateWindowRtlLayout)

    TabsCtrl* tabsCtrl = new TabsCtrl();
    tabsCtrl->onTabClosed = MkFunc1(MainWindowTabClosed, win);
    tabsCtrl->onSelectionChanging = MkFunc1(MainWindowTabSelectionChanging, win);
    tabsCtrl->onSelectionChanged = MkFunc1(MainWindowTabSelectionChanged, win);
    tabsCtrl->onContextMenu = MkFunc1(TabsContextMenu, tabsCtrl);
    tabsCtrl->onTabMigration = MkFunc1(MainWindowTabMigration, win);
    tabsCtrl->Create(args);
    win->tabsCtrl = tabsCtrl;
    win->tabSelectionHistory = new Vec<WindowTab*>();
}

// verifies that WindowTab state is consistent with MainWindow state
static NO_INLINE void VerifyWindowTab(MainWindow* win, WindowTab* tdata) {
    ReportIf(tdata->ctrl != win->ctrl);
    // Home / Favorites tabs have no document controller. Favorites layout
    // intentionally leaves canvas geometry alone (hidden), so canvasRc is not
    // kept in lockstep with win->canvasRc the way document tabs are.
    if (tdata->IsNonDocumentTab()) {
        return;
    }
#if 0
    // disabling this check. best I can tell, external apps can change window
    // title and trigger this
    auto winTitle = win::GetTextTemp(win->hwndFrame);
    if (!str::Eq(winTitle.Get(), tdata->frameTitle.Get())) {
        logf(L"VerifyWindowTab: winTitle: '%s', tdata->frameTitle: '%s'\n", winTitle.Get(), tdata->frameTitle.Get());
        ReportIf(!str::Eq(winTitle.Get(), tdata->frameTitle));
    }
#endif
    bool expectedTocVisibility = tdata->showToc; // if not in presentation mode
    if (PM_DISABLED != win->presentation) {
        expectedTocVisibility = false; // PM_BLACK_SCREEN, PM_WHITE_SCREEN
        if (PM_ENABLED == win->presentation) {
            expectedTocVisibility = tdata->showTocPresentation;
        }
    }
    ReportDebugIf(win->uiState.tocVisible != expectedTocVisibility);
    ReportIf(tdata->canvasRc != win->canvasRc);
}

// Must be called when the active tab is losing selection.
// This happens when a new document is loaded or when another tab is selected.
void SaveCurrentWindowTab(MainWindow* win) {
    if (!win) {
        return;
    }
    if (!win->tabsCtrl) {
        return;
    }
    // the find UI (compact bar or floating window) belongs to the previous tab's
    // search; close it when leaving the tab (HideFindBar also drops the cached
    // results so the next tab can't show or navigate into the old document's
    // matches)
    HideFindBar(win);
    HideSelectionToolbar(win);

    int current = win->tabsCtrl->GetSelected();
    if (-1 == current) {
        return;
    }
    if (win->CurrentTab() != win->Tabs()[current]) {
        return; // TODO: restore ReportIf() ?
    }

    WindowTab* tab = win->CurrentTab();
    if (win->tocLoaded && tab->ctrl) {
        TocTree* tocTree = tab->ctrl->GetToc();
        UpdateTocExpansionState(tab->tocState, win->tocTreeView, tocTree);
    }
    VerifyWindowTab(win, tab);

    // update the selection history
    win->tabSelectionHistory->Remove(tab);
    win->tabSelectionHistory->Append(tab);
}

WindowTab* AddTabToWindow(MainWindow* win, WindowTab* tab, bool deferUpdate) {
    ReportIf(!win);
    if (!win) {
        return nullptr;
    }
    if (!win->tabsCtrl) {
        return nullptr;
    }

    auto* tabs = win->tabsCtrl;
    int idx = win->TabCount();
    bool useTabs = SettingsUseTabs();
    bool noHomeTab = gGlobalPrefs->noHomeTab;
    bool createHomeTab = useTabs && !noHomeTab && (idx == 0);
    if (createHomeTab) {
        WindowTab* homeTab = new WindowTab(win);
        homeTab->type = WindowTab::Type::About;
        homeTab->canvasRc = win->canvasRc;
        TabInfo* newTab = new TabInfo();
        newTab->text = str::Dup(StrL("Home"));
        newTab->tooltip = nullptr;
        newTab->isPinned = true;
        newTab->canClose = true;
        newTab->userData = (UINT_PTR)homeTab;
        int insertedIdx = tabs->InsertTab(idx, newTab, !deferUpdate);
        ReportIf(insertedIdx != 0);
        idx++;
    }

    tab->canvasRc = win->canvasRc;
    TabInfo* newTab = new TabInfo();
    newTab->text = str::Dup(tab->GetTabTitle());
    // same full path + size as UpdateTabTitle (was path-only, so size missing until
    // TabsOnChangedDoc for the active tab)
    newTab->tooltip = str::Dup(MakeTabTooltipTemp(tab->filePath, false));
    newTab->userData = (UINT_PTR)tab;
    newTab->tabColor = tab->tabColor;
    // a failed load can arrive in a tab created for it (ShowLoadErrorInTab)
    newTab->isError = tab->loadState == WindowTab::LoadState::Error;

    int insertedIdx = tabs->InsertTab(idx, newTab, !deferUpdate);
    ReportIf(insertedIdx == -1);
    if (!deferUpdate) {
        tabs->SetSelected(insertedIdx);
        UpdateTabWidth(win);
    }
    return tab;
}

// The tab control paints from TabInfo::tabColor, so WindowTab::tabColor has to
// be pushed into it whenever it changes. AddTabToWindow() does it at insert
// time, but a document's saved color is only known once it has loaded, well
// after that (issue #5884).
// copy tab->tabColor into the tab control's TabInfo (what it paints from)
void SetTabInfoColor(WindowTab* tab) {
    if (!tab || !tab->win || !tab->win->tabsCtrl) {
        return;
    }
    MainWindow* win = tab->win;
    TabInfo* ti = win->tabsCtrl->GetTab(win->GetTabIdx(tab));
    if (!ti || ti->tabColor == tab->tabColor) {
        return;
    }
    ti->tabColor = tab->tabColor;
    win->tabsCtrl->ScheduleRepaint();
}

// The tab control paints from TabInfo::isError, so it has to be re-synced from
// WindowTab::loadState when a load fails or a reload succeeds
void UpdateTabIsError(WindowTab* tab) {
    if (!tab || !tab->win || !tab->win->tabsCtrl) {
        return;
    }
    MainWindow* win = tab->win;
    TabInfo* ti = win->tabsCtrl->GetTab(win->GetTabIdx(tab));
    bool isError = tab->loadState == WindowTab::LoadState::Error;
    if (!ti || ti->isError == isError) {
        return;
    }
    ti->isError = isError;
    win->tabsCtrl->ScheduleRepaint();
}

// Refresh the tab's title
void TabsOnChangedDoc(MainWindow* win) {
    WindowTab* tab = win->CurrentTab();
    ReportIf(!tab != !win->TabCount());
    if (!tab) {
        return;
    }

    int tabIdx = win->GetTabIdx(tab);
    int selectedIdx = win->tabsCtrl->GetSelected();
    if (tabIdx != selectedIdx) {
        logf("TabsonChangeDoc: tabIdx (%d) != selectedIdx (%d)\n", tabIdx, selectedIdx);
        ReportDebugIf(tabIdx != selectedIdx);
    }
    VerifyWindowTab(win, tab);
    UpdateTabTitle(tab);
}

// Called when we're closing an entire window (quitting)
void TabsOnCloseWindow(MainWindow* win) {
    // TODO: I've seen a crash here where it seems like we've deleted the only main window
    // but somehow we still process Esc and we get here. this might not be enough
    if (!win->tabsCtrl) {
        return;
    }
    // Clear these BEFORE destroying tabs. Deleting a Markdown/CHM tab destroys
    // its WebView2, which pumps messages; canvas WndProc then called AsFixed()
    // on win->ctrl after another tab's DisplayModel had already been freed.
    win->ctrl = nullptr;
    win->currentTabTemp = nullptr;
    auto tabs = win->Tabs();
    DeleteVecMembers(tabs);
    win->tabsCtrl->RemoveAllTabs();
    win->tabSelectionHistory->Reset();
}

void SetTabsInTitlebar(MainWindow* win, bool inTitleBar) {
    if (inTitleBar == win->tabsInTitlebar) {
        return;
    }
    win->tabsInTitlebar = inTitleBar;
    win->tabsCtrl->inTitleBar = inTitleBar;
    if (inTitleBar) {
        RelayoutCaption(win);
    }
    uint flags = SWP_FRAMECHANGED | SWP_NOZORDER | SWP_NOSIZE | SWP_NOMOVE;
    SetWindowPos(win->hwndFrame, nullptr, 0, 0, 0, 0, flags);
}

// Selects the next (or previous) tab.
void TabsOnCtrlTab(MainWindow* win, bool reverse) {
    if (!win) {
        return;
    }
    int count = win->TabCount();
    if (count < 2) {
        return;
    }
    int idx = win->tabsCtrl->GetSelected() + 1;
    if (reverse) {
        idx -= 2;
    }
    idx += count; // ensure > 0
    idx = idx % count;
    TabsSelect(win, idx);
}

void MoveTab(MainWindow* win, int dir) {
    if (!win) {
        return;
    }
    int nTabs = win->TabCount();
    int idx = win->tabsCtrl->GetSelected();
    int newIdx = idx + dir;
    if (newIdx < 0) {
        return;
    }
    if (newIdx >= nTabs) {
        return;
    }
    win->tabsCtrl->SwapTabs(idx, newIdx);
    win->tabsCtrl->SetSelected(newIdx);
    win->tabsCtrl->LayoutTabs();
}
