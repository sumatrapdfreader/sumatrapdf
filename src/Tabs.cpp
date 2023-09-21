/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "utils/BaseUtil.h"
#include "utils/ScopedWin.h"
#include "utils/WinDynCalls.h"
#include "utils/Dpi.h"
#include "utils/FileUtil.h"
#include "utils/GdiPlusUtil.h"
#include "utils/UITask.h"
#include "utils/WinUtil.h"

#include "wingui/UIModels.h"
#include "wingui/Layout.h"
#include "wingui/WinGui.h"

#include "Settings.h"
#include "DocController.h"
#include "AppColors.h"
#include "EngineBase.h"
#include "EngineAll.h"
#include "DisplayModel.h"
#include "GlobalPrefs.h"
#include "SumatraPDF.h"
#include "SumatraProperties.h"
#include "MainWindow.h"
#include "WindowTab.h"
#include "resource.h"
#include "Commands.h"
#include "Caption.h"
#include "Menu.h"
#include "TableOfContents.h"
#include "Tabs.h"
#include "Translations.h"

#include "utils/Log.h"

static void UpdateTabTitle(WindowTab* tab) {
    if (!tab) {
        return;
    }
    MainWindow* win = tab->win;
    int idx = win->GetTabIdx(tab);
    const char* title = tab->GetTabTitle();
    const char* tooltip = tab->filePath.Get();
    win->tabsCtrl->SetTextAndTooltip(idx, title, tooltip);
}

int GetTabbarHeight(HWND hwnd, float factor) {
    int dy = DpiScale(hwnd, kTabBarDy);
    return (int)(dy * factor);
}

static inline Size GetTabSize(HWND hwnd) {
    int dx = DpiScale(hwnd, std::max(gGlobalPrefs->tabWidth, kTabMinDx));
    int dy = DpiScale(hwnd, kTabBarDy);
    return Size(dx, dy);
}

static void ShowTabBar(MainWindow* win, bool show) {
    if (show == win->tabsVisible) {
        return;
    }
    win->tabsVisible = show;
    win->tabsCtrl->SetIsVisible(show);
    RelayoutWindow(win);
}

void UpdateTabWidth(MainWindow* win) {
    int nTabs = (int)win->TabCount();
    bool showSingleTab = gGlobalPrefs->useTabs || win->tabsInTitlebar;
    bool showTabs = (nTabs > 1) || (showSingleTab && (nTabs > 0));
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
    CrashIf(tab != tab2);
    if (tab == win->CurrentTab()) {
        win->ctrl = nullptr;
        win->currentTabTemp = nullptr;
    }
    UpdateTabWidth(win);

    // if the removed tab was the current one, select another
    if (win->TabCount() < 1) {
        return;
    }
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
}

static void CloseWindowIfNoDocuments(MainWindow* win) {
    for (auto& tab : win->Tabs()) {
        if (!tab->IsAboutTab()) {
            return;
        }
    }
    // no tabs or only about tab
    CloseWindow(win, true, true);
}

static void MigrateTab(WindowTab* tab, MainWindow* newWin) {
    MainWindow* oldWin = tab->win;
    RemoveTab(tab);

    if (!newWin) {
        newWin = CreateAndShowMainWindow(nullptr);
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
    // SetFocus(newWin->hwndFrame);
    // newWin->RedrawAll(true);
    // TabsOnChangedDoc(newWin);
    WindowTab* newTab = new WindowTab(newWin);
    newTab->SetFilePath(tab->filePath);
    newWin->currentTabTemp = AddTabToWindow(newWin, newTab);
    LoadArgs args(tab->filePath, newWin);
    args.forceReuse = true;
    args.noSavePrefs = true;
    LoadDocument(&args, false, false);
    delete tab;

    CloseWindowIfNoDocuments(oldWin);
}

// Selects the given tab (0-based index)
// tabIndex can come from settings file so must be sanitized
void TabsSelect(MainWindow* win, int tabIndex) {
    auto tabs = win->Tabs();
    int nTabs = tabs.Size();
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
    LoadModelIntoTab(tab);
}

// clang-format off
static MenuDef menuDefContextTab[] = {
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
        nullptr,
        0,
    },
};
// clang-format on

static void ShowFileInFolder(WindowTab* tab) {
    if (!HasPermission(Perm::DiskAccess)) {
        return;
    }
    const char* path = tab->GetPath();
    if (!path) {
        return;
    }

    const char* process = "explorer.exe";
    AutoFreeStr args = str::Format("/select,\"%s\"", path);
    CreateProcessHelper(process, args);
}

void CollectTabsToClose(MainWindow* win, WindowTab* currTab, Vec<WindowTab*>& toCloseOther,
                        Vec<WindowTab*>& toCloseRight) {
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
        }
    }
}

// TODO: add "Move to another window" sub-menu
static void TabsContextMenu(ContextMenuEvent* ev) {
    MainWindow* win = FindMainWindowByHwnd(ev->w->hwnd);
    TabsCtrl* tabsCtrl = (TabsCtrl*)ev->w;
    TabMouseState tabState = tabsCtrl->TabStateFromMousePosition(ev->mouseWindow);
    int tabIdx = tabState.tabIdx;
    if (tabIdx < 0) {
        return;
    }

    int nTabs = tabsCtrl->TabCount();
    WindowTab* tabUnderMouse = win->Tabs()[tabIdx];
    if (tabUnderMouse->IsAboutTab()) {
        return;
    }
    POINT pt = ToPOINT(ev->mouseScreen);
    HMENU popup = BuildMenuFromMenuDef(menuDefContextTab, CreatePopupMenu(), nullptr);

    Vec<WindowTab*> toCloseOther;
    Vec<WindowTab*> toCloseRight;
    CollectTabsToClose(win, tabUnderMouse, toCloseOther, toCloseRight);

    if (toCloseOther.IsEmpty()) {
        MenuSetEnabled(popup, CmdCloseOtherTabs, false);
    }
    if (toCloseRight.IsEmpty()) {
        MenuSetEnabled(popup, CmdCloseTabsToTheRight, false);
    }
    MarkMenuOwnerDraw(popup);
    uint flags = TPM_RETURNCMD | TPM_RIGHTBUTTON;
    int cmd = TrackPopupMenu(popup, flags, pt.x, pt.y, 0, win->hwndFrame, nullptr);
    FreeMenuOwnerDrawInfoData(popup);
    DestroyMenu(popup);
    switch (cmd) {
        case CmdClose:
            CloseTab(tabUnderMouse, false);
            break;

        case CmdCloseOtherTabs: {
            for (WindowTab* t : toCloseOther) {
                CloseTab(t, false);
            }
            break;
        }
        case CmdCloseTabsToTheRight: {
            for (WindowTab* t : toCloseRight) {
                CloseTab(t, false);
            }
            break;
        }
        case CmdShowInFolder: {
            ShowFileInFolder(tabUnderMouse);
            break;
        }
        case CmdCopyFilePath: {
            CopyFilePath(tabUnderMouse);
            break;
        }
        case CmdDuplicateInNewWindow: {
            DuplicateTabInNewWindow(tabUnderMouse);
            break;
        }
        case CmdProperties: {
            bool extended = false;
            ShowProperties(win->hwndFrame, tabUnderMouse->ctrl, extended);
            break;
        }
    }
}

void CreateTabbar(MainWindow* win) {
    TabsCtrl* tabsCtrl = new TabsCtrl();

    tabsCtrl->onTabClosed = [win](TabClosedEvent* ev) {
        int closedTabIdx = ev->tabIdx;
        WindowTab* tab = win->GetTab(closedTabIdx);
        CloseTab(tab, false);
    };

    tabsCtrl->onSelectionChanging = [win](TabsSelectionChangingEvent* ev) -> bool {
        // TODO: Should we allow the switch of the tab if we are in process of printing?
        SaveCurrentWindowTab(win);
        return false;
    };

    tabsCtrl->onSelectionChanged = [win](TabsSelectionChangedEvent* ev) {
        int currentIdx = win->tabsCtrl->GetSelected();
        WindowTab* tab = win->Tabs()[currentIdx];
        LoadModelIntoTab(tab);
    };
    tabsCtrl->onContextMenu = TabsContextMenu;

    tabsCtrl->onTabMigration = [win](TabMigrationEvent* ev) {
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
        MigrateTab(tab, releaseWnd);
    };

    TabsCreateArgs args;
    args.parent = win->hwndFrame;
    args.withToolTips = true;
    tabsCtrl->Create(args);
    win->tabsCtrl = tabsCtrl;
    win->tabSelectionHistory = new Vec<WindowTab*>();
}

// verifies that WindowTab state is consistent with MainWindow state
static NO_INLINE void VerifyWindowTab(MainWindow* win, WindowTab* tdata) {
    CrashIf(tdata->ctrl != win->ctrl);
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
    ReportIf(win->tocVisible != expectedTocVisibility);
    ReportIf(tdata->canvasRc != win->canvasRc);
}

// Must be called when the active tab is losing selection.
// This happens when a new document is loaded or when another tab is selected.
void SaveCurrentWindowTab(MainWindow* win) {
    if (!win) {
        return;
    }

    int current = win->tabsCtrl->GetSelected();
    if (-1 == current) {
        return;
    }
    if (win->CurrentTab() != win->Tabs().at(current)) {
        return; // TODO: restore CrashIf() ?
    }

    WindowTab* tab = win->CurrentTab();
    if (win->tocLoaded) {
        TocTree* tocTree = tab->ctrl->GetToc();
        UpdateTocExpansionState(tab->tocState, win->tocTreeView, tocTree);
    }
    VerifyWindowTab(win, tab);

    // update the selection history
    win->tabSelectionHistory->Remove(tab);
    win->tabSelectionHistory->Append(tab);
}

WindowTab* AddTabToWindow(MainWindow* win, WindowTab* tab) {
    CrashIf(!win);
    if (!win) {
        return nullptr;
    }

    auto tabs = win->tabsCtrl;
    int idx = win->TabCount();
    bool useTabs = gGlobalPrefs->useTabs;
    bool noHomeTab = gGlobalPrefs->noHomeTab;
    bool createHomeTab = useTabs && !noHomeTab && (idx == 0);
    if (createHomeTab) {
        WindowTab* homeTab = new WindowTab(win);
        homeTab->type = WindowTab::Type::About;
        homeTab->canvasRc = win->canvasRc;
        TabInfo* newTab = new TabInfo();
        newTab->text = str::Dup("Home");
        newTab->tooltip = nullptr;
        newTab->isPinned = true;
        newTab->canClose = true;
        newTab->userData = (UINT_PTR)homeTab;
        int insertedIdx = tabs->InsertTab(idx, newTab);
        CrashIf(insertedIdx != 0);
        idx++;
    }

    tab->canvasRc = win->canvasRc;
    TabInfo* newTab = new TabInfo();
    newTab->text = str::Dup(tab->GetTabTitle());
    newTab->tooltip = str::Dup(tab->filePath.Get());
    newTab->userData = (UINT_PTR)tab;

    int insertedIdx = tabs->InsertTab(idx, newTab);
    CrashIf(insertedIdx == -1);
    tabs->SetSelected(insertedIdx);
    UpdateTabWidth(win);
    return tab;
}

// Refresh the tab's title
void TabsOnChangedDoc(MainWindow* win) {
    WindowTab* tab = win->CurrentTab();
    CrashIf(!tab != !win->TabCount());
    if (!tab) {
        return;
    }

    CrashIf(win->GetTabIdx(tab) != win->tabsCtrl->GetSelected());
    VerifyWindowTab(win, tab);
    UpdateTabTitle(tab);
}

// Called when we're closing an entire window (quitting)
void TabsOnCloseWindow(MainWindow* win) {
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
    SetParent(win->tabsCtrl->hwnd, inTitleBar ? win->hwndCaption : win->hwndFrame);
    ShowWindow(win->hwndCaption, inTitleBar ? SW_SHOW : SW_HIDE);
    if (inTitleBar != win->isMenuHidden) {
        ToggleMenuBar(win);
    }
    if (inTitleBar) {
        CaptionUpdateUI(win, win->caption);
        RelayoutCaption(win);
    } else if (dwm::IsCompositionEnabled()) {
        // remove the extended frame
        MARGINS margins{};
        dwm::ExtendFrameIntoClientArea(win->hwndFrame, &margins);
        win->extendedFrameHeight = 0;
    }
    uint flags = SWP_FRAMECHANGED | SWP_NOZORDER | SWP_NOSIZE | SWP_NOMOVE;
    SetWindowPos(win->hwndFrame, nullptr, 0, 0, 0, 0, flags);
}

// Selects the next (or previous) tab.
void TabsOnCtrlTab(MainWindow* win, bool reverse) {
    if (!win) {
        return;
    }
    int count = (int)win->TabCount();
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
