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
#include "EngineBase.h"
#include "EngineAll.h"
#include "DisplayModel.h"
#include "GlobalPrefs.h"
#include "SumatraPDF.h"
#include "MainWindow.h"
#include "WindowTab.h"
#include "resource.h"
#include "Commands.h"
#include "Caption.h"
#include "Menu.h"
#include "TableOfContents.h"
#include "Tabs.h"

#include "utils/Log.h"

static void UpdateTabTitle(WindowTab* tab) {
    if (!tab) {
        return;
    }
    MainWindow* win = tab->win;
    int idx = win->tabs.Find(tab);
    const char* title = tab->GetTabTitle();
    const char* tooltip = tab->filePath.Get();
    win->tabsCtrl->SetTextAndTooltip(idx, title, tooltip);
}

static void NO_INLINE SwapTabs(MainWindow* win, int tab1, int tab2) {
    if (tab1 == tab2 || tab1 < 0 || tab2 < 0) {
        return;
    }

    auto&& tabs = win->tabs;
    std::swap(tabs.at(tab1), tabs.at(tab2));
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
    int count = (int)win->tabs.size();
    bool showSingleTab = gGlobalPrefs->useTabs || win->tabsInTitlebar;
    bool showTabs = (count > 1) || (showSingleTab && (count > 0));
    if (!showTabs) {
        ShowTabBar(win, false);
        return;
    }
    ShowTabBar(win, true);
    Rect rect = ClientRect(win->tabsCtrl->hwnd);
    Size tabSize = GetTabSize(win->hwndFrame);
    auto maxDx = (rect.dx - 3) / count;
    tabSize.dx = std::min(tabSize.dx, maxDx);
    win->tabsCtrl->SetTabSize(tabSize);
}

static void RemoveTab(MainWindow* win, int idx) {
    WindowTab* tab = win->tabs.at(idx);
    UpdateTabFileDisplayStateForTab(tab);
    win->tabSelectionHistory->Remove(tab);
    win->tabs.Remove(tab);
    if (tab == win->CurrentTab()) {
        win->ctrl = nullptr;
        win->currentTabTemp = nullptr;
    }
    WindowTab* tab2 = win->tabsCtrl->RemoveTab<WindowTab>(idx);
    CrashIf(tab2 != tab);
    delete tab;
    UpdateTabWidth(win);
}

static void WinTabClosedHandler(MainWindow* win, TabsCtrl* tabs, int closedTabIdx) {
    int current = win->tabsCtrl->GetSelected();
    if (closedTabIdx == current) {
        CloseCurrentTab(win);
    } else {
        RemoveTab(win, closedTabIdx);
    }
}

// Selects the given tab (0-based index)
// TODO: this shouldn't go through the same notifications, just do it
void TabsSelect(MainWindow* win, int tabIndex) {
    auto& tabs = win->tabs;
    int count = tabs.Size();
    if (count < 2 || tabIndex < 0 || tabIndex >= count) {
        return;
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

void CreateTabbar(MainWindow* win) {
    TabsCtrl* tabsCtrl = new TabsCtrl();
    tabsCtrl->onTabClosed = [win](TabClosedEvent* ev) { WinTabClosedHandler(win, ev->tabs, ev->tabIdx); };
    tabsCtrl->onSelectionChanging = [win](TabsSelectionChangingEvent* ev) -> bool {
        // TODO: Should we allow the switch of the tab if we are in process of printing?
        SaveCurrentWindowTab(win);
        return false;
    };

    tabsCtrl->onSelectionChanged = [win](TabsSelectionChangedEvent* ev) {
        int currentIdx = win->tabsCtrl->GetSelected();
        WindowTab* tab = win->tabs[currentIdx];
        LoadModelIntoTab(tab);
    };
    tabsCtrl->onTabDragged = [win](TabDraggedEvent* ev) {
        int tab1 = ev->tab1;
        int tab2 = ev->tab2;
        SwapTabs(win, tab1, tab2);
    };

    TabsCreateArgs args;
    args.parent = win->hwndFrame;
    args.ctrlID = IDC_TABBAR;
    args.createToolTipsHwnd = true;
    tabsCtrl->Create(args);

    Size tabSize = GetTabSize(win->hwndFrame);
    tabsCtrl->SetTabSize(tabSize);
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
    if (win->CurrentTab() != win->tabs.at(current)) {
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

#include "AppColors.h"

void TabPainterSetColors(TabPainter* p) {
    p->tabBackgroundBg = GetAppColor(AppColor::TabBackgroundBg);
    p->tabBackgroundText = GetAppColor(AppColor::TabBackgroundText);
    p->tabBackgroundCloseX = GetAppColor(AppColor::TabBackgroundCloseX);
    p->tabBackgroundCloseCircle = GetAppColor(AppColor::TabBackgroundCloseCircle);
    p->tabSelectedBg = GetAppColor(AppColor::TabSelectedBg);
    p->tabSelectedText = GetAppColor(AppColor::TabSelectedText);
    p->tabSelectedCloseX = GetAppColor(AppColor::TabSelectedCloseX);
    p->tabSelectedCloseCircle = GetAppColor(AppColor::TabSelectedCloseCircle);
    p->tabHighlightedBg = GetAppColor(AppColor::TabHighlightedBg);
    p->tabHighlightedText = GetAppColor(AppColor::TabHighlightedText);
    p->tabHighlightedCloseX = GetAppColor(AppColor::TabHighlightedCloseX);
    p->tabHighlightedCloseCircle = GetAppColor(AppColor::TabHighlightedCloseCircle);
    p->tabHoveredCloseX = GetAppColor(AppColor::TabHoveredCloseX);
    p->tabHoveredCloseCircle = GetAppColor(AppColor::TabHoveredCloseCircle);
    p->tabClickedCloseX = GetAppColor(AppColor::TabClickedCloseX);
    p->tabClickedCloseCircle = GetAppColor(AppColor::TabClickedCloseCircle);
}

void UpdateCurrentTabBgColor(MainWindow* win) {
    TabPainter* tab = win->tabsCtrl->painter;
    // TODO: match either the toolbar (if shown) or background
    tab->currBgCol = kTabDefaultBgCol;
    TabPainterSetColors(tab);
    RepaintNow(win->tabsCtrl->hwnd);
}

// On load of a new document we insert a new tab item in the tab bar.
WindowTab* CreateNewTab(MainWindow* win, const char* filePath) {
    CrashIf(!win);
    if (!win) {
        return nullptr;
    }

    WindowTab* tab = new WindowTab(win, filePath);
    win->tabs.Append(tab);
    tab->canvasRc = win->canvasRc;

    int idx = (int)win->tabs.size() - 1;
    auto tabs = win->tabsCtrl;
    TabInfo* newTab = new TabInfo();
    newTab->text = str::Dup(tab->GetTabTitle());
    newTab->tooltip = str::Dup(tab->filePath.Get());
    newTab->userData = (UINT_PTR)tab;

    int insertedIdx = tabs->InsertTab(idx, newTab);
    CrashIf(insertedIdx == -1);
    tabs->SetSelected(idx);
    UpdateTabWidth(win);
    return tab;
}

// Refresh the tab's title
void TabsOnChangedDoc(MainWindow* win) {
    WindowTab* tab = win->CurrentTab();
    CrashIf(!tab != !win->tabs.size());
    if (!tab) {
        return;
    }

    CrashIf(win->tabs.Find(tab) != win->tabsCtrl->GetSelected());
    VerifyWindowTab(win, tab);
    UpdateTabTitle(tab);
}

// Called when we're closing a document
void TabsOnCloseDoc(MainWindow* win) {
    if (win->tabs.size() == 0) {
        return;
    }

    /*
    DisplayModel* dm = win->AsFixed();
    if (dm) {
        EngineBase* engine = dm->GetEngine();
        if (EngineHasUnsavedAnnotations(engine)) {
            // TODO: warn about unsaved annotations
            logf("File has unsaved annotations\n");
        }
    }
    */

    int current = win->tabsCtrl->GetSelected();
    RemoveTab(win, current);

    if (win->tabs.size() > 0) {
        WindowTab* tab = win->tabSelectionHistory->Pop();
        int idx = win->tabs.Find(tab);
        win->tabsCtrl->SetSelected(idx);
        LoadModelIntoTab(tab);
    }
}

// Called when we're closing an entire window (quitting)
void TabsOnCloseWindow(MainWindow* win) {
    win->tabsCtrl->RemoveAllTabs();
    win->tabSelectionHistory->Reset();
    win->currentTabTemp = nullptr;
    win->ctrl = nullptr;
    DeleteVecMembers(win->tabs);
}

void SetTabsInTitlebar(MainWindow* win, bool inTitleBar) {
    if (inTitleBar == win->tabsInTitlebar) {
        return;
    }
    win->tabsInTitlebar = inTitleBar;
    win->tabsCtrl->painter->inTitleBar = inTitleBar;
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
    int count = (int)win->tabs.size();
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
