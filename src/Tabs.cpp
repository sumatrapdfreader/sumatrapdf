/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/Dpi.h"
#include "base/File.h"
#include "base/Win.h"

#include "wingui/UIModels.h"
#include "wingui/Layout.h"
#include "wingui/WinGui.h"

#include "Settings.h"
#include "AppSettings.h"
#include "DocController.h"
#include "EngineBase.h"
#include "EngineAll.h"
#include "DisplayModel.h"
#include "GlobalPrefs.h"
#include "SumatraPDF.h"
#include "SumatraProperties.h"
#include "Notifications.h"
#include "MainWindow.h"
#include "WindowTab.h"
#include "Commands.h"
#include "CommandAvailability.h"
#include "FindBar.h"
#include "Menu.h"
#include "TableOfContents.h"
#include "Tabs.h"
#include "SumatraDialogs.h"
#include "FileHistory.h"
#include "Theme.h"
#include "Translations.h"


static void UpdateTabTitle(WindowTab* tab) {
    if (!tab) {
        return;
    }
    MainWindow* win = tab->win;
    int idx = win->GetTabIdx(tab);
    Str title = tab->GetTabTitle();
    // respect FullPathInTitle for the tab tooltip too: when the user opted out
    // of showing the full path, don't reveal it on hover either (#3024)
    Str tooltip = tab->filePath;
    if (tooltip && !gGlobalPrefs->fullPathInTitle) {
        tooltip = path::GetBaseNameTemp(tooltip);
    }
    // append a human-readable file size, like the home page thumbnail tooltips
    if (tooltip) {
        i64 size = file::GetSize(tab->filePath);
        if (size >= 0) {
            tooltip = fmt("%s  %s", tooltip, str::FormatSizeShortTemp(size, nullptr));
        }
    }
    win->tabsCtrl->SetTextAndTooltip(idx, title, tooltip);
}

int GetTabbarHeight(HWND hwnd, float factor) {
    int tabDy = DpiScale(hwnd, kTabBarDy);
    HFONT hfont = GetAppFont();
    int fontDyWithPadding = FontDyPx(hwnd, hfont) + DpiScale(hwnd, 2);
    if (fontDyWithPadding > tabDy) {
        tabDy = fontDyWithPadding;
    }
    // guard against bad per-window DPI (e.g. under Wine)
    int minDy = DpiScale(HWND_DESKTOP, kTabBarDy);
    int minFontDy = FontDyPx(hwnd, hfont) + DpiScale(HWND_DESKTOP, 2);
    if (minFontDy > minDy) {
        minDy = minFontDy;
    }
    if (tabDy < minDy) {
        tabDy = minDy;
    }
    int res = (int)((float)tabDy * factor);
    if (IsRunningOnWine()) {
        int dpi = DpiGet(hwnd);
        int desktopDpi = DpiGet(HWND_DESKTOP);
        logf(
            "GetTabbarHeight: hwnd=%p factor=%g dpi=%d desktopDpi=%d tabDyScaled=%d fontDy=%d "
            "minDy=%d result=%d\n",
            hwnd, factor, dpi, desktopDpi, DpiScale(hwnd, kTabBarDy), fontDyWithPadding, minDy, res);
    }
    return res;
}

#if 0
static inline Size GetTabSize(HWND hwnd) {
    int dx = DpiScale(hwnd, std::max(gGlobalPrefs->tabWidth, kTabMinDx));
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
    RelayoutWindow(win);
}

void UpdateTabWidth(MainWindow* win) {
    int nTabs = (int)win->TabCount();
    bool showSingleTab = SettingsUseTabs() || win->tabsInTitlebar;
    bool showTabs = (nTabs > 1) || (showSingleTab && (nTabs > 0));
    int tabWidth = gGlobalPrefs->tabWidth;
    if (win->tabsCtrl) {
        win->tabsCtrl->tabDefaultDx = tabWidth;
    }
    if (!showTabs) {
        ShowTabBar(win, false);
        return;
    }
    ShowTabBar(win, true);
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
    if (newIdx > lastIdx) {
        newIdx = lastIdx;
    }
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

    // don't migrate if it's only one document tab and not
    // dragging over a window
    int nDocTabs = 0;
    for (auto& t : oldWin->Tabs()) {
        if (t->IsAboutTab()) continue;
        nDocTabs++;
    }
    if (nDocTabs == 1 && !newWin) return;

    auto engine = tab->GetEngine();
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
            int x = releasePt.x - DpiScale(oldWin->hwndFrame, 100);
            int y = releasePt.y - GetTabbarHeight(oldWin->hwndFrame) / 2;
            Rect rect = ShiftRectToWorkArea(Rect(x, y, dx, dy), oldWin->hwndFrame, true);
            newWin = CreateAndShowMainWindow(nullptr, false);
            if (!newWin) {
                return;
            }
            MoveWindow(newWin->hwndFrame, rect);
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

    bool isShowingPageInfo = (GetNotificationForGroup(win->hwndCanvas, kNotifPageInfo) != nullptr);

    // same work as in onSelectionChanging and onSelectionChanged
    SaveCurrentWindowTab(win);
    int prevIdx = tabsCtrl->SetSelected(tabIndex);
    if (prevIdx < 0) {
        return;
    }
    WindowTab* tab = tabs[tabIndex];
    LoadModelIntoTab(tab);
    if (isShowingPageInfo) {
        PostMessageW(win->hwndFrame, WM_COMMAND, CmdTogglePageInfo, 0);
    }
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
        CmdDiscardAnnotations,
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
        _TRN("Set Tab Color"),
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
static void TabsContextMenu(ContextMenuEvent* ev) {
    MainWindow* win = FindMainWindowByHwnd(ev->w->hwnd);
    TabsCtrl* tabsCtrl = (TabsCtrl*)ev->w;
    TabsCtrl::MouseState tabState = tabsCtrl->TabStateFromMousePosition(ev->mouseWindow);
    int tabIdx = tabState.tabIdx;
    if (tabIdx < 0) {
        return;
    }

    WindowTab* tabUnderMouse = win->Tabs()[tabIdx];
    if (tabUnderMouse->IsAboutTab()) {
        return;
    }
    POINT pt = ToPOINT(ev->mouseScreen);

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
    ctx->canCloseOtherTabs = !toCloseOther.IsEmpty();
    ctx->canCloseTabsToRight = !toCloseRight.IsEmpty();
    ctx->canCloseTabsToLeft = !toCloseLeft.IsEmpty();

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
        DeleteMenu(popup, CmdDiscardAnnotations, MF_BYCOMMAND);
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
        case CmdDiscardAnnotations: {
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

static void MainWindowTabSelectionChanged(MainWindow* win, TabsCtrl::SelectionChangedEvent* ev) {
    bool isShowingPageInfo = (GetNotificationForGroup(win->hwndCanvas, kNotifPageInfo) != nullptr);
    int currentIdx = win->tabsCtrl->GetSelected();
    WindowTab* tab = win->Tabs()[currentIdx];
    LoadModelIntoTab(tab);
    if (isShowingPageInfo) {
        PostMessageW(win->hwndFrame, WM_COMMAND, CmdTogglePageInfo, 0);
    }
}

static void MainWindowTabMigration(MainWindow* win, TabsCtrl::MigrationEvent* ev) {
    WindowTab* tab = win->GetTab(ev->tabIdx);
    MainWindow* releaseWnd = nullptr;
    POINT p;
    p.x = ev->releasePoint.x;
    p.y = ev->releasePoint.y;
    HWND hwnd = WindowFromPoint(p);
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
    TabsCtrl::CreateArgs args;
    args.parent = win->hwndFrame;
    args.withToolTips = true;
    args.font = GetAppFont();
    int tabWidth = gGlobalPrefs->tabWidth;
    args.tabDefaultDx = tabWidth;
    args.isRtl = false; // LTR hwnd; RTL tab order follows parent frame (see UpdateWindowRtlLayout)

    TabsCtrl* tabsCtrl = new TabsCtrl();
    tabsCtrl->onTabClosed = MkFunc1(MainWindowTabClosed, win);
    tabsCtrl->onSelectionChanging = MkFunc1(MainWindowTabSelectionChanging, win);
    tabsCtrl->onSelectionChanged = MkFunc1(MainWindowTabSelectionChanged, win);
    tabsCtrl->onContextMenu = MkFunc1Void(TabsContextMenu);
    tabsCtrl->onTabMigration = MkFunc1(MainWindowTabMigration, win);
    tabsCtrl->Create(args);
    win->tabsCtrl = tabsCtrl;
    win->tabSelectionHistory = new Vec<WindowTab*>();
}

// verifies that WindowTab state is consistent with MainWindow state
static NO_INLINE void VerifyWindowTab(MainWindow* win, WindowTab* tdata) {
    ReportIf(tdata->ctrl != win->ctrl);
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
    ReportDebugIf(win->tocVisible != expectedTocVisibility);
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

WindowTab* AddTabToWindow(MainWindow* win, WindowTab* tab) {
    ReportIf(!win);
    if (!win) {
        return nullptr;
    }
    if (!win->tabsCtrl) {
        return nullptr;
    }

    auto tabs = win->tabsCtrl;
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
        int insertedIdx = tabs->InsertTab(idx, newTab);
        ReportIf(insertedIdx != 0);
        idx++;
    }

    tab->canvasRc = win->canvasRc;
    TabInfo* newTab = new TabInfo();
    newTab->text = str::Dup(tab->GetTabTitle());
    newTab->tooltip = str::Dup(tab->filePath);
    newTab->userData = (UINT_PTR)tab;
    newTab->tabColor = tab->tabColor;

    int insertedIdx = tabs->InsertTab(idx, newTab);
    ReportIf(insertedIdx == -1);
    tabs->SetSelected(insertedIdx);
    UpdateTabWidth(win);
    return tab;
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
    auto tabs = win->Tabs();
    DeleteVecMembers(tabs);
    win->tabsCtrl->RemoveAllTabs();
    win->tabSelectionHistory->Reset();
    win->currentTabTemp = nullptr;
    win->ctrl = nullptr;
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
