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
#include "Tabs.h"

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
        tip = fmt("%s  %s", path, str::FormatSizeShortTemp(size, nullptr));
    } else {
        tip = path;
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
