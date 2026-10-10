/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// ng: orig's Tabs.cpp drives a win32 TabsCtrl (gui/win/TabsCtrl.cpp), which
// owns a TabInfo per tab and paints it. Here the gpui strip
// (src/gui/TabsUI.cpp) paints from the WindowTab itself, so the TabInfo mirror
// and the functions that kept it in sync (SetTabInfoColor, UpdateTabIsError,
// SetTextAndTooltip) are gone. Everything else is orig's.

#include "base/Base.h"
#include "base/File.h"
#include "base/Win.h"

#include "gui/UIModels.h"

#include "Settings.h"
#include "AppSettings.h"
#include "DisplayMode.h"
#include "DocController.h"
#include "EngineBase.h"
#include "base/GuessFileType.h"
#include "EngineAll.h"
#include "TextSelection.h"
#include "ProgressUpdateUI.h"
#include "TextSearch.h"
#include "DisplayModel.h"
#include "PagePosition.h"
#include "FileHistory.h"
#include "Commands.h"
#include "CommandAvailability.h"
#include "Menu.h"
#include "SumatraPDF.h"
#include "MainWindow.h"
#include "ReadingBar.h"
#include "ReadingAutoScroll.h"
#include "WindowTab.h"
#include "Notifications.h"
#include "Translations.h"
#include "SearchAndDDE.h"
#include "FindBar.h"
#include "SelectionToolbar.h"
#include "TableOfContents.h"
#include "DocumentProperties.h"
#include "SumatraDialogs.h"
#include "gui/AppShell.h"
#include "gui/TabsUI.h"

#include "SessionState.h"
#include "AIChatCommon.h"
#include "AIChatPanel.h"
#include "Tabs.h"

#include "SumatraLog.h"

// ng: orig pushes the text into the TabsCtrl; the gpui strip reads it back on
// every frame, so this only has to ask for a repaint
void UpdateTabPageText(WindowTab* tab) {
    if (tab && tab->win) {
        AppShellInvalidate(tab->win);
    }
}

bool TabIsDirty(WindowTab* tab) {
    if (!tab || !tab->AsFixed()) {
        return false;
    }
    return EngineHasUnsavedAnnotations(tab->AsFixed()->GetEngine());
}

// Hide a lone Home tab. Favorites is a closable tab, so it must remain visible
// even when it is the only tab left.
bool TabsAreVisible(MainWindow* win) {
    if (win->isQuickLook) {
        return false;
    }
    // orig's showTabsBar: fullscreen and presentation hide the strip
    if (win->isFullScreen || win->InPresentation()) {
        return false;
    }
    int nTabs = win->TabCount();
    if (nTabs == 0) {
        return false;
    }
    bool onlyHomeTab = nTabs == 1 && win->GetTab(0)->IsAboutTab();
    bool showSingleTab = SettingsUseTabs();
    return !onlyHomeTab && ((nTabs > 1) || (showSingleTab && (nTabs > 0)));
}

// verifies that WindowTab state is consistent with MainWindow state
static NO_INLINE void VerifyWindowTab(MainWindow* win, WindowTab* tdata) {
    ReportIf(tdata->ctrl != win->ctrl);
    // Home / Favorites tabs have no document controller
    if (tdata->IsNonDocumentTab()) {
        return;
    }
    bool expectedTocVisibility = tdata->showToc; // if not in presentation mode
    if (PM_DISABLED != win->presentation) {
        expectedTocVisibility = false; // PM_BLACK_SCREEN, PM_WHITE_SCREEN
        if (PM_ENABLED == win->presentation) {
            expectedTocVisibility = tdata->showTocPresentation;
        }
    }
    // Heading TOC is generated after the document is shown. Until that finishes
    // the sidebar stays hidden (uiState.tocVisible) but the tab keeps the
    // caller's showToc preference so we can open it when headings arrive.
    if (win->uiState.tocVisible != expectedTocVisibility) {
        bool headingPending = EngineMupdfHeadingTocPending(tdata->GetEngine());
        bool okPendingHide = headingPending && expectedTocVisibility && !win->uiState.tocVisible;
        ReportDebugIf(!okPendingHide);
    }
}

// Must be called when the active tab is losing selection.
// This happens when a new document is loaded or when another tab is selected.
void SaveCurrentWindowTab(MainWindow* win) {
    if (!win) {
        return;
    }
    // the find UI belongs to the previous tab's search; close it when leaving
    // the tab (HideFindBar also drops the cached results so the next tab can't
    // show or navigate into the old document's matches)
    HideFindBar(win);
    HideSelectionToolbar(win);
    ReadingAutoScrollHideBar(win);
    ReadingBarCancelDrag(win);

    WindowTab* tab = win->CurrentTab();
    if (!tab) {
        return;
    }
    if (win->tocLoaded && tab->ctrl) {
        TocTree* tocTree = tab->ctrl->GetToc();
        UpdateTocExpansionState(tab->tocState, win, tocTree);
    }
    VerifyWindowTab(win, tab);

    // update the selection history
    VecRemove(*win->tabSelectionHistory, tab);
    VecAppend(*win->tabSelectionHistory, tab);
}

// make `tab` the window's current tab: its controller becomes the window's
void LoadModelIntoTab(WindowTab* tab) {
    if (!tab) {
        return;
    }
    MainWindow* win = tab->win;
    win->currentTabTemp = tab;
    win->ctrl = tab->ctrl;
    // this canvas size was already applied to the previous document
    win->lastViewPortSize = Size{};
    win->showSelection = tab->selectionOnPage != nullptr;
    // the bookmarks tree belongs to the tab we are leaving
    ClearTocBox(win);
    win->currPageNo = tab->ctrl ? tab->ctrl->CurrentPageNo() : 0;
    // find matches / count cache are for the previous tab's document; keeps the
    // find box text (#5308) and rebuilds the highlights if find is still open
    InvalidateFindForDocumentChange(win);
    FindBarReposition(win);
    // orig's LoadModelIntoTab: presentation mode has a sidebar state of its own
    bool showToc = win->InPresentation() ? tab->showTocPresentation : tab->showToc;
    SetSidebarVisibility(win, showToc, gSettings->showFavorites);
    UpdateWindowTitle(win);
    RebuildMenuBar(win);
    ReadingAutoScrollSyncToTab(tab);
    AIChatSyncPanelsToCurrentTab(win);
    OnAIChatTabChanged(win);
    RemoveNotificationsForGroup(win, kNotifZoomOrView);
    // keep / restore the page-info tip after a tab switch (issue #4454)
    ShowPageInfoIfWanted(win);
    win->RedrawAll();
    LoadLazyTabIfNeeded(tab);
    StartPendingSearch(win);
}

// Refresh the tab's title
void TabsOnChangedDoc(MainWindow* win) {
    WindowTab* tab = win->CurrentTab();
    if (!tab) {
        return;
    }
    VerifyWindowTab(win, tab);
    UpdateWindowTitle(win);
    RebuildMenuBar(win);
    win->RedrawAll();
}

WindowTab* AddTabToWindow(MainWindow* win, WindowTab* tab, bool deferUpdate) {
    ReportIf(!win);
    if (!win) {
        return nullptr;
    }
    int idx = win->TabCount();
    bool useTabs = SettingsUseTabs();
    bool noHomeTab = gSettings->noHomeTab;
    bool createHomeTab = useTabs && !noHomeTab && (idx == 0);
    if (createHomeTab) {
        auto* homeTab = new WindowTab(win);
        homeTab->type = WindowTab::Type::About;
        homeTab->canvasRc = win->canvasRc;
        VecAppend(win->tabs, homeTab);
        idx++;
    }

    tab->canvasRc = win->canvasRc;
    VecAppend(win->tabs, tab);
    if (!deferUpdate) {
        win->currentTabTemp = tab;
        win->ctrl = tab->ctrl;
    }
    return tab;
}

// Selects the given tab (0-based index)
// tabIndex can come from settings file so must be sanitized
void TabsSelect(MainWindow* win, int tabIndex) {
    int nTabs = win->TabCount();
    logf("TabsSelect: tabIndex: %d, nTabs: %d\n", tabIndex, nTabs);
    if (nTabs == 0) {
        return;
    }
    if (tabIndex < 0 || tabIndex >= nTabs) {
        tabIndex = 0;
    }
    WindowTab* tab = win->GetTab(tabIndex);
    if (tab == win->CurrentTab()) {
        return;
    }
    SaveCurrentWindowTab(win);
    LoadModelIntoTab(tab);
}

// Select the Home tab, creating it when NoHomeTab left the window without one.
void GoToHomeTab(MainWindow* win) {
    if (!win || !SettingsUseTabs()) {
        return;
    }
    for (int i = 0; i < win->TabCount(); i++) {
        if (win->GetTab(i)->IsAboutTab()) {
            TabsSelect(win, i);
            return;
        }
    }

    SaveCurrentWindowTab(win);
    auto* homeTab = new WindowTab(win);
    homeTab->type = WindowTab::Type::About;
    homeTab->canvasRc = win->canvasRc;
    VecInsertAt(win->tabs, 0, homeTab);
    TabsUIOnTabsChanged(win);
    LoadModelIntoTab(homeTab);
}

void RemoveTab(WindowTab* tab) {
    if (!tab) {
        return;
    }
    MainWindow* win = tab->win;
    if (!win) {
        return;
    }
    int idx = win->GetTabIdx(tab);
    if (idx < 0) {
        // a nested close already took this tab out of the strip
        return;
    }
    UpdateTabFileDisplayStateForTab(tab);
    RemoveNotificationsForTab(tab);
    VecRemove(*win->tabSelectionHistory, tab);
    bool closedCurrentTab = (tab == win->CurrentTab());
    VecRemoveAt(win->tabs, idx);
    if (closedCurrentTab) {
        win->ctrl = nullptr;
        win->currentTabTemp = nullptr;
    }
    TabsUIOnTabsChanged(win);

    int nTabs = win->TabCount();
    if (nTabs < 1 || !closedCurrentTab) {
        UpdateWindowTitle(win);
        RebuildMenuBar(win);
        return;
    }
    // select tab to the right or to the left if nothing to the right
    int newIdx = std::min(idx, nTabs - 1);
    WindowTab* newTab = win->GetTab(newIdx);
    if (newTab && newTab->type != WindowTab::Type::None) {
        LoadModelIntoTab(newTab);
    }
}

// orig's CloseWindowIfNoDocuments: a window whose last document tab was
// dragged out goes away
static void CloseWindowIfNoDocuments(MainWindow* win) {
    for (WindowTab* tab : win->Tabs()) {
        if (!tab->IsAboutTab()) {
            return;
        }
    }
    CloseWindow(win, true, false);
}

// orig's MaybeMigrateTab: a tab dropped outside its strip moves to `newWin`,
// or to a window of its own when there is none.
// ng: gpui captures the mouse for the window the drag started in, so a drop on
// another window's strip never arrives; `newWin` is always null here (see
// "gpui gaps"). Everything else is orig's.
void MaybeMigrateTab(WindowTab* tab, MainWindow* newWin) {
    MainWindow* oldWin = tab->win;
    if (tab->IsNonDocumentTab()) {
        return;
    }
    // don't migrate a lone document tab unless it is dropped on another window
    int nDocTabs = 0;
    for (WindowTab* t : oldWin->Tabs()) {
        if (!t->IsNonDocumentTab()) {
            nDocTabs++;
        }
    }
    if (nDocTabs == 1 && !newWin) {
        return;
    }
    if (EngineHasUnsavedAnnotations(tab->GetEngine())) {
        return;
    }
    TempStr path = str::DupTemp(tab->filePath);
    TempStr displayName = str::DupTemp(tab->displayName);
    TabState* state = NewTabStateFromTab(tab);
    RemoveTab(tab);
    delete tab;

    if (!newWin) {
#if OS_WIN
        // dragging a tab out of a maximized window: like Chrome, create a
        // normal (non-maximized) window with the size the source window
        // would have when restored, positioned at the cursor so it lands
        // on the new window's tab strip
        HWND oldHwnd = AppShellNativeHwnd(oldWin);
        Rect normal;
        bool wasZoomed = oldHwnd && IsZoomed(oldHwnd) && AppShellNormalWindowRect(oldWin, &normal);
        Rect rect;
        if (wasZoomed) {
            int dpi = AppShellWindowDpi(oldWin);
            POINT releasePt{};
            GetCursorPos(&releasePt);
            int x = releasePt.x - MulDiv(100, dpi, 96);
            int y = releasePt.y - (MulDiv(kTabBarDy, dpi, 96) / 2);
            rect = ShiftRectToWorkArea(Rect(x, y, normal.dx, normal.dy), oldHwnd, true);
        }
#endif
        newWin = CreateAndShowMainWindow(nullptr);
#if OS_WIN
        if (newWin && wasZoomed) {
            // after the placement CreateAndShowMainWindow posted, which may
            // maximize it (the remembered state is the old window's)
            PlaceMainWindowLater(newWin, rect, false);
        }
#endif
    }
    if (!newWin) {
        DeleteTabState(state);
        return;
    }
    logf("MaybeMigrateTab: '%s' -> window 0x%p\n", path, newWin);
    // orig re-opens the document in the new window rather than sliding the
    // WindowTab across; the same comment applies here
    LoadDocument(newWin, path);
    WindowTab* newTab = newWin->CurrentTab();
    if (newTab && len(displayName) > 0) {
        newTab->SetDisplayName(displayName);
    }
    if (state) {
        SetTabState(newTab, state);
        DeleteTabState(state);
    }
    CloseWindowIfNoDocuments(oldWin);
}

// create a new window if win==nullptr
void CollectTabsToClose(MainWindow* win, WindowTab* currTab, Vec<WindowTab*>& toCloseOther,
                        Vec<WindowTab*>& toCloseRight, Vec<WindowTab*>& toCloseLeft) {
    int nTabs = win->TabCount();
    bool seenCurrent = false;
    for (int i = 0; i < nTabs; i++) {
        WindowTab* tab = win->GetTab(i);
        if (tab->IsAboutTab()) {
            continue;
        }
        if (currTab == tab) {
            seenCurrent = true;
            continue;
        }
        VecAppend(toCloseOther, tab);
        if (seenCurrent) {
            VecAppend(toCloseRight, tab);
        } else {
            VecAppend(toCloseLeft, tab);
        }
    }
}

void CloseAllTabs(MainWindow* win) {
    if (!win || win->isBeingClosed) {
        return;
    }
    // can't close while iterating over the tabs so collect them first
    Vec<WindowTab*> toClose;
    int nTabs = win->TabCount();
    for (int i = 0; i < nTabs; i++) {
        WindowTab* t = win->GetTab(i);
        if (t->IsAboutTab()) {
            continue;
        }
        VecAppend(toClose, t);
    }
    CloseCollectedTabs(win, toClose);
}

// Called when we're closing an entire window (quitting)
void TabsOnCloseWindow(MainWindow* win) {
    // Clear these BEFORE destroying tabs: deleting a tab frees its
    // DisplayModel, and anything that still asks the window for its controller
    // in between would read a freed one
    win->ctrl = nullptr;
    win->currentTabTemp = nullptr;
    Vec<WindowTab*> tabs = win->tabs;
    VecReset(win->tabs);
    DeleteVecMembers(tabs);
    VecReset(*win->tabSelectionHistory);
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
    int idx = win->GetTabIdx(win->CurrentTab()) + 1;
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
    int idx = win->GetTabIdx(win->CurrentTab());
    int newIdx = idx + dir;
    if (idx < 0 || newIdx < 0 || newIdx >= nTabs) {
        return;
    }
    WindowTab* tmp = win->tabs[idx];
    win->tabs[idx] = win->tabs[newIdx];
    win->tabs[newIdx] = tmp;
    TabsUIOnTabsChanged(win);
    win->RedrawAll();
}

// orig's UpdateAfterDrag: `from` is pulled out of the strip and re-inserted so
// it ends up in front of what used to be at `to`
void TabsMoveTab(MainWindow* win, int from, int to) {
    int nTabs = win->TabCount();
    bool badState = (from == to) || (from < 0) || (to < 0) || (from >= nTabs) || (to > nTabs);
    if (badState) {
        return;
    }
    WindowTab* moved = win->tabs[from];
    VecRemoveAt(win->tabs, from);
    if (from < to) {
        // we moved from left to right e.g. from 1 to 3
        // after removing 1 we insert not at 3 but 2
        to -= 1;
    }
    VecInsertAt(win->tabs, to, moved);
    TabsUIOnTabsChanged(win);
    TabsSelect(win, to);
    win->RedrawAll();
}

// --- the tab context menu ---------------------------------------------------

// clang-format off
static MenuDef menuDefContextTab[] = {
    // these top items are removed unless the document has unsaved changes;
    // text matches the "Unsaved changes" close dialog
    {
        TrN("&Save changes to existing PDF"),
        CmdSaveAnnotations,
    },
    {
        TrN("Save changes to &new PDF"),
        CmdSaveAnnotationsNewFile,
    },
    {
        TrN("&Discard changes"),
        CmdDiscardChanges,
    },
    {
        StrL(kMenuSeparator),
        0,
    },
    {
        TrN("Properties..."),
        CmdProperties,
    },
    {
        TrN("Show in folder"),
        CmdShowInFolder,
    },
    {
        TrN("Copy File Path"),
        CmdCopyFilePath,
    },
    {
        TrN("Open In New Window"),
        CmdDuplicateInNewWindow,
    },
    {
        TrN("Change Tab Color"),
        CmdSetTabColor,
    },
    {
        StrL(kMenuSeparator),
        0,
    },
    {
        TrN("Close"),
        CmdClose,
    },
    {
        TrN("Close Other Tabs"),
        CmdCloseOtherTabs,
    },
    {
        TrN("Close Tabs To The Right"),
        CmdCloseTabsToTheRight,
    },
    {
        TrN("Close Tabs To The Left"),
        CmdCloseTabsToTheLeft,
    },
    {
        TrN("Close All Tabs"),
        CmdCloseAllTabs,
    },
    {
        StrL(kMenuSeparator),
        0,
    },
    {
        TrN("Save Tab Group"),
        CmdTabGroupSave,
    },
    {
        TrN("Restore Tab Group"),
        CmdTabGroupRestore,
    },
    {
        {},
        0,
    },
};
// clang-format on

MenuModel* BuildTabContextMenu(MainWindow* win, WindowTab* tabUnderMouse) {
    if (!win || !tabUnderMouse || tabUnderMouse->IsAboutTab()) {
        return nullptr;
    }
    Vec<WindowTab*> toCloseOther;
    Vec<WindowTab*> toCloseRight;
    Vec<WindowTab*> toCloseLeft;
    CollectTabsToClose(win, tabUnderMouse, toCloseOther, toCloseRight, toCloseLeft);

    DisplayModel* dmTab = tabUnderMouse->AsFixed();
    EngineBase* tabEngine = dmTab ? dmTab->GetEngine() : nullptr;

    // Build the command context for the tab under the mouse, which may differ
    // from the current tab that NewAppCommandCtx() keys off. Without a context
    // command availability is evaluated against an empty (no-document) state,
    // which removes almost every item.
    BuildMenuCtx* ctx = NewBuildMenuCtx(tabUnderMouse, Point{0, 0});
    ctx->tab = tabUnderMouse;
    ctx->isDocLoaded = true; // tabUnderMouse is a real (non-about) document tab
    ctx->filePath = tabUnderMouse->filePath;
    ctx->supportsAnnots = EngineSupportsAnnotations(tabEngine);
    ctx->hasUnsavedAnnotations = EngineHasUnsavedAnnotations(tabEngine);
    ctx->canCloseOtherTabs = len(toCloseOther) > 0;
    ctx->canCloseTabsToRight = len(toCloseRight) > 0;
    ctx->canCloseTabsToLeft = len(toCloseLeft) > 0;

    MenuModel* menu = BuildMenuFromDef(menuDefContextTab, ctx);
    DeleteBuildMenuCtx(ctx);

    if (!tabUnderMouse->ctrl) {
        MenuSetEnabled(menu, CmdSetTabColor, false);
    }

    // the save/discard items only make sense when the document has unsaved
    // changes; otherwise remove them, then clean up the separator they leave
    if (!EngineHasUnsavedAnnotations(tabEngine)) {
        MenuRemove(menu, CmdSaveAnnotations);
        MenuRemove(menu, CmdSaveAnnotationsNewFile);
        MenuRemove(menu, CmdDiscardChanges);
        RemoveBadMenuSeparators(menu);
    }
    return menu;
}

void TabContextMenuCommand(MainWindow* win, WindowTab* tabUnderMouse, int cmdId) {
    if (!win || !tabUnderMouse || win->GetTabIdx(tabUnderMouse) < 0) {
        return;
    }
    Vec<WindowTab*> toCloseOther;
    Vec<WindowTab*> toCloseRight;
    Vec<WindowTab*> toCloseLeft;
    CollectTabsToClose(win, tabUnderMouse, toCloseOther, toCloseRight, toCloseLeft);

    switch (cmdId) {
        case CmdClose:
            CloseTab(tabUnderMouse, false);
            return;
        case CmdCloseAllTabs:
            CloseAllTabs(win);
            return;
        case CmdCloseOtherTabs:
            CloseCollectedTabs(win, toCloseOther);
            return;
        case CmdCloseTabsToTheRight:
            CloseCollectedTabs(win, toCloseRight);
            return;
        case CmdCloseTabsToTheLeft:
            CloseCollectedTabs(win, toCloseLeft);
            return;
        case CmdShowInFolder:
            ShowFileInFolder(win, tabUnderMouse->filePath);
            return;
        case CmdCopyFilePath:
            CopyFilePath(tabUnderMouse);
            return;
        case CmdProperties:
            ShowProperties(win, tabUnderMouse->ctrl);
            return;
        case CmdDuplicateInNewWindow:
            DuplicateTabInNewWindow(tabUnderMouse);
            return;
        case CmdSetTabColor:
            ShowSetTabColorDialog(win, tabUnderMouse);
            return;
        case CmdSaveAnnotations:
            SaveAnnotationsToExistingFile(tabUnderMouse);
            return;
        case CmdSaveAnnotationsNewFile:
            SaveAnnotationsToMaybeNewPdfFile(tabUnderMouse);
            return;
        case CmdDiscardChanges:
            // revert to the on-disk version, discarding unsaved changes
            TabsSelect(win, win->GetTabIdx(tabUnderMouse));
            ReloadDocument(win, false);
            return;
    }
    // everything we forward to main window
    ExecuteCmd(win, cmdId);
}
