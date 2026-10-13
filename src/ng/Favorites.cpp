/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// ng: the favorites model (the per-file list in FileHistory, sorting, add /
// remove, the readable name) and the tree model are orig's. What orig does
// with a win32 TreeView, custom draw and an HMENU is here: the tree model is
// handed to the gpui favorites pane (src/gui/Sidebar.cpp) and the Favorites
// menu fills a MenuModel. The full-window Favorites tab is step 10.

#include "base/Base.h"
#include "base/File.h"
#include "base/UITask.h"

#include "gui/UIModels.h"

#include "Settings.h"
#include "DisplayMode.h"
#include "DocController.h"
#include "EngineBase.h"
#include "DisplayModel.h"
#include "FileHistory.h"
#include "FilterUtil.h"
#include "SumatraPDF.h"
#include "MainWindow.h"
#include "WindowTab.h"
#include "Commands.h"
#include "AppSettings.h"
#include "Menu.h"
#include "SumatraDialogs.h"
#include "Translations.h"
#include "ShortcutParse.h"
#include "Accelerators.h"
#include "Tabs.h"
#include "Theme.h"
#include "PagePosition.h"
#include "SumatraLog.h"
#include "gui/AppShell.h"
#include "gui/Sidebar.h"
#include "Favorites.h"
#include "MenuDefs.h"

// For easy access, we try to show favorites in the menu, similar to a list of
// recently opened files.
// The first menu items are for currently opened file (up to kMaxFavMenus), based
// on the assumption that user is usually interested in navigating current file.
// Then we have a submenu for each file for which there are bookmarks (up to
// kMaxFavSubmenus), each having up to kMaxFavMenus menu items.
void AppendFavMenus(MenuRef m, Str currFilePath) {
    // To minimize mouse movement when navigating current file via favorites
    // menu, put favorites for current file first
    FileState* currFileFav = nullptr;
    if (currFilePath) {
        currFileFav = GetFavByFilePath(currFilePath);
    }

    // sort the files with favorites by base file name of file path
    StrVec filePathsSorted;
    if (CanAccessDisk()) {
        // only show favorites for other files, if we're allowed to open them
        GetSortedFilePaths(filePathsSorted, currFileFav);
    }
    if (currFileFav && len(*currFileFav->favorites) > 0) {
        filePathsSorted.InsertAt(0, currFileFav->filePath);
    }

    if (len(filePathsSorted) == 0) {
        return;
    }

    MenuAppendSeparator(m);

    int menusCount = len(filePathsSorted);
    // ng: orig caps this with kMaxFavMenus and never uses kMaxFavSubmenus; both
    // are 10, and /WX rejects the unused constant
    menusCount = std::min(menusCount, kMaxFavSubmenus);

    // collect the favorites that will be shown, so that a single pass over the
    // commands gives all of them their command id
    Vec<FavMenuEntry> favs;
    for (int i = 0; i < menusCount; i++) {
        Str filePath = filePathsSorted[i];
        FileState* f = GetFavByFilePath(filePath);
        if (!f) {
            continue;
        }
        for (int j = 0; j < len(*f->favorites) && j < kMaxFavMenus; j++) {
            Favorite* fn = (*f->favorites)[j];
            VecAppend(favs, FavMenuEntry{filePath, fn->pageNo, 0});
        }
    }
    SetFavCmdIds(favs);

    int favIdx = 0;
    for (int i = 0; i < menusCount; i++) {
        Str filePath = filePathsSorted[i];
        FileState* f = GetFavByFilePath(filePath);
        ReportIf(!f);
        if (!f) {
            continue;
        }
        bool combined = (len(*f->favorites) == 1);
        if (combined) {
            AppendFavMenuItems(m, f, favs, favIdx, true, f == currFileFav);
            continue;
        }
        Str s = Tr("Current file");
        if (f != currFileFav) {
            s = path::GetBaseNameTemp(filePath);
        }
        MenuModel* sub = MenuAppendSubmenu(m, s);
        AppendFavMenuItems(sub, f, favs, favIdx, false, f == currFileFav);
    }
}

// gpui's menu adds the shortcut itself
TempStr FavAddMenuTextTemp(Str addText) {
    return str::DupTemp(addText);
}

void GoToFavoritePage(MainWindow* win, Str pageNo, PointF scrollPos) {
    if (!IsMainWindowValidAndNotClosing(win)) {
        return;
    }
    ApplyFavoriteView(win, pageNo, scrollPos, true);
    // we might have been invoked by clicking on a tree view
    // switch focus so that keyboard navigation works, which enables
    // a fluid experience
    win->Focus();
    AppShellInvalidate(win);
}

void GoToFavForTreeItem(MainWindow* win, FavTreeItem* fti) {
    if (!fti) {
        return;
    }
    logf("GoToFavForTreeItem: '%s'\n", fti->text);
    Favorite* fn = fti->favorite;
    if (!fn) {
        // can happen for top-level node which is not associated with a favorite
        // but only serves a parent node for favorites for a given file
        return;
    }
    FileState* f = GetByFavorite(fn);
    GoToFavorite(win, f, fn);
}

static void ApplyFavFilter(MainWindow* win) {
    SidebarSetFavModel(win, BuildFavTreeModel(win, SidebarFavFilterTextTemp(win)));
}

void FavFilterChanged(MainWindow* win) {
    ApplyFavFilter(win);
    FavTreeModel* tm = SidebarFavModel(win);
    int n = tm ? tm->ChildCount(tm->Root()) : 0;
    logf("FavFilterChanged: '%s' -> %d rows\n", SidebarFavFilterTextTemp(win), n);
    AppShellInvalidate(win);
}

void PopulateFavTreeIfNeeded(MainWindow* win) {
    if (SidebarFavModel(win)) {
        return;
    }
    ApplyFavFilter(win);
}

void ToggleFavorites(MainWindow* win) {
    SidebarToggleFavorites(win);
}

// open/select the full-window Favorites tab (can be used with the sidebar's
// Favorites panel). Always switches to / creates the tab; it is closed with
// the tab's ✕. Requires tabs; falls back to the sidebar toggle when tabs are
// off.
void ToggleFavoritesTab(MainWindow* win) {
    if (!SettingsUseTabs()) {
        ToggleFavorites(win);
        return;
    }
    WindowTab* favTab = FindFavoritesTab(win);
    if (favTab) {
        int idx = win->GetTabIdx(favTab);
        if (idx >= 0 && !IsCurrentTab(win, favTab)) {
            TabsSelect(win, idx);
        }
        PopulateFavTreeIfNeeded(win);
        AppShellInvalidate(win);
        return;
    }
    PopulateFavTreeIfNeeded(win);
    SaveCurrentWindowTab(win);
    auto* tab = new WindowTab(win);
    tab->type = WindowTab::Type::Favorites;
    AddTabToWindow(win, tab);
    LoadModelIntoTab(tab);
}

void UpdateFavoritesTree(MainWindow* win) {
    // rebuild (honors current search filter if any)
    ApplyFavFilter(win);
    FavTreeModel* newModel = SidebarFavModel(win);

    // hide favorites UI if we've removed the last favorite
    bool hasAny = false;
    if (newModel) {
        hasAny = newModel->ChildCount(newModel->Root()) > 0;
    }
    if (!hasAny) {
        if (WindowTab* favTab = FindFavoritesTab(win)) {
            CloseTab(favTab, false);
        }
        if (gSettings->showFavorites) {
            SetSidebarVisibility(win, win->uiState.tocVisible, false);
        }
        return;
    }
    // refresh sidebar visibility only when the sidebar panel is supposed to be open
    if (gSettings->showFavorites) {
        SetSidebarVisibility(win, win->uiState.tocVisible, true);
    }
}

void RememberFavTreeExpansionState(MainWindow* win) {
    VecReset(win->expandedFavorites);
    FavTreeModel* tm = SidebarFavModel(win);
    if (!tm) {
        return;
    }
    TreeItem root = tm->Root();
    int n = tm->ChildCount(root);
    for (int i = 0; i < n; i++) {
        TreeItem ti = tm->ChildAt(root, i);
        auto* fti = (FavTreeItem*)ti;
        if (!fti->isExpanded) {
            continue;
        }
        FileState* f = GetByFavorite(fti->favorite);
        if (f) {
            VecAppend(win->expandedFavorites, f);
        }
    }
}

MenuModel* BuildFavContextMenu(MainWindow*, FavTreeItem* fti) {
    MenuModel* popup = BuildMenuFromDef(menuDefContextFav, nullptr);
    MenuSetChecked(popup, CmdToggleFavoritesSort, gSettings->sortFavoritesByName);
    if (!fti) {
        // Sort By Name works with no selection; Remove needs a favorite row.
        MenuRemove(popup, CmdFavoriteDel);
    }
    RemoveBadMenuSeparators(popup);
    return popup;
}

void FavContextMenuCommand(MainWindow*, FavTreeItem* fti, int cmd) {
    logf("FavContextMenuCommand: %d (%s) on '%s'\n", cmd, GetCommandName(cmd), fti ? fti->text : Str{});
    // TODO: it would be nice to have a system for undo-ing things, like in Gmail,
    // so that we can do destructive operations without asking for permission via
    // invasive model dialog boxes but also allow reverting them if were done
    // by mistake
    if (CmdToggleFavoritesSort == cmd) {
        ToggleSortFavoritesByName();
        return;
    }
    if (CmdFavoriteDel == cmd && fti) {
        Favorite* toDelete = fti->favorite;
        FileState* f = GetByFavorite(toDelete);
        if (!f) {
            return;
        }
        if (fti->parent) {
            DelFavorite(f, toDelete);
            return;
        }
        RememberFavTreeExpansionStateForAllWindows();
        RemoveAllFavForFile(f->filePath);
        UpdateFavoritesTreeForAllWindows();
        ScheduleSaveSettings();
    }
}
