/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/File.h"
#include "base/Win.h"
#include "gui/UIModels.h"
#include "Settings.h"
#include "AppSettings.h"
#include "DocController.h"
#include "EngineBase.h"
#include "base/GuessFileType.h"
#include "EngineAll.h"
#include "DisplayModel.h"
#include "SumatraPDF.h"
#include "DocumentProperties.h"
#include "MainWindow.h"
#include "WindowTab.h"
#include "Commands.h"
#include "CommandAvailability.h"
#include "FindBar.h"
#include "PagePosition.h"
#include "ReadingAutoScroll.h"
#include "ReadingBar.h"
#include "SelectionToolbar.h"
#include "Menu.h"
#include "TableOfContents.h"
#include "FileHistory.h"
#include "Translations.h"
#include "AppTools.h"
#include "Tabs.h"
#include "TabsCommon.h"

// always full path (FullPathInTitle only affects tab/window title text).
// Append size when GetSize succeeds (may fail for offline network paths).
// Used by Tabs and Toolbar (toolbar was overwriting tooltips with path-only).
// full path + size (if available); optional dirty suffix for unsaved annotations
TempStr MakeTabTooltipTemp(Str path, bool dirty) {
    if (len(path) == 0) {
        return Str{};
    }
    TempStr tip;
    i64 size = file::GetSize(path);
    if (size >= 0) {
        tip = fmt("%s  %s", PathForDisplayTemp(path), str::FormatSizeShortTemp(size, nullptr));
    } else {
        tip = PathForDisplayTemp(path);
    }
    if (dirty) {
        tip = str::JoinTemp(tip, StrL(" "), Tr("(unsaved annotations)"));
    }
    return tip;
}

TempStr TabPageSuffixTemp(WindowTab* tab) {
    if (!gSettings || !gSettings->showPageNumberInTabs) {
        return {};
    }
    if (!tab || !tab->IsDocLoaded() || !tab->ctrl) {
        return {};
    }
    int curr = tab->ctrl->CurrentPageNo();
    int count = tab->ctrl->PageCount();
    if (count <= 0 || curr < 1) {
        return {};
    }
    if (ShowChapterUi(tab->ctrl)) {
        Location loc = tab->ctrl->CurrentLocation();
        int chapterPages = tab->ctrl->ChapterPageCount(loc.chapter);
        return fmt(" %d/%d · %d/%d", loc.chapter, tab->ctrl->ChapterCount(), loc.page, chapterPages);
    }
    return fmt(" %d/%d", curr, count);
}

void CloseCollectedTabs(MainWindow* win, const Vec<WindowTab*>& toClose) {
    // CloseTab can pump (DDE, SaveSettings, dialogs). A nested close may have
    // already freed some of these pointers; GetTabIdx is pointer identity and
    // does not dereference a freed WindowTab.
    if (!win) {
        return;
    }
    for (WindowTab* t : toClose) {
        if (!IsMainWindowValid(win) || win->isBeingClosed) {
            return;
        }
        if (win->GetTabIdx(t) < 0) {
            continue;
        }
        CloseTab(t, false);
    }
}

// orig's CloseWindowIfNoDocuments: a window whose last document tab was
// dragged out goes away
void CloseWindowIfNoDocuments(MainWindow* win) {
    if (HasOpenedDocuments(win)) {
        return;
    }
    CloseWindow(win, true, false);
}

WindowTab* NewHomeTab(MainWindow* win, HomeTabKind kind) {
    auto* tab = new WindowTab(win);
    tab->type = WindowTab::Type::About;
    tab->isExtraHome = kind == HomeTabKind::Extra;
    tab->canvasRc = win->canvasRc;
    return tab;
}

WindowTab* FindHomeTab(MainWindow* win) {
    for (WindowTab* tab : win->Tabs()) {
        if (tab->IsAboutTab()) {
            return tab;
        }
    }
    return nullptr;
}

// the current tab if a file should open in it instead of in a new tab: an extra
// Home tab is replaced, like a browser's new tab. The pinned Home tab stays
WindowTab* HomeTabToReuse(MainWindow* win) {
    WindowTab* tab = win->CurrentTab();
    return (tab && tab->IsAboutTab() && tab->isExtraHome) ? tab : nullptr;
}

// every tab "close all" takes: all but the pinned Home tab
void CollectClosableTabs(MainWindow* win, Vec<WindowTab*>& out) {
    for (WindowTab* tab : win->Tabs()) {
        if (!tab->IsPinnedHomeTab()) {
            VecAppend(out, tab);
        }
    }
}

// what the close commands can act on, relative to ctx.tab
void SetTabCloseFlags(AppCommandCtx& ctx) {
    Vec<WindowTab*> other;
    Vec<WindowTab*> right;
    Vec<WindowTab*> left;
    CollectTabsToClose(ctx.win, ctx.tab, other, right, left);
    ctx.canCloseOtherTabs = len(other) > 0;
    ctx.canCloseTabsToRight = len(right) > 0;
    ctx.canCloseTabsToLeft = ctx.tab && len(left) > 0;
    ctx.hasDocTabs = HasOpenedDocuments(ctx.win);
}

// create a new window if win==nullptr
void CollectTabsToClose(MainWindow* win, WindowTab* currTab, Vec<WindowTab*>& toCloseOther,
                        Vec<WindowTab*>& toCloseRight, Vec<WindowTab*>& toCloseLeft) {
    int nTabs = win->TabCount();
    bool seenCurrent = false;
    for (int i = 0; i < nTabs; i++) {
        WindowTab* tab = win->GetTab(i);
        if (currTab == tab) {
            seenCurrent = true;
            continue;
        }
        if (tab->IsPinnedHomeTab()) {
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
