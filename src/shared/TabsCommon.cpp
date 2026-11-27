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
#include "AIChatTypes.h"
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
#include "FileThumbnails.h"
#include "Notifications.h"
#include "Translations.h"
#include "AppTools.h"
#include "Tabs.h"
#include "TabsCommon.h"
#include "StripGroups.h"

int gTabTooltipSizeLookups = 0;

// always full path (FullPathInTitle only affects tab/window title text).
// Append size when GetSize succeeds (may fail for offline network paths).
// Used by Tabs and Toolbar (toolbar was overwriting tooltips with path-only).
// full path + size (if available); optional dirty suffix for unsaved annotations
// The size is cached on the tab: a stat per hover blocks on an offline share.
TempStr MakeTabTooltipTemp(WindowTab* tab, bool dirty) {
    Str path = tab ? tab->filePath : Str{};
    if (len(path) == 0) {
        return Str{};
    }
    // saving annotations clears dirty and changes the size
    if (tab->tooltipFileSize == kTabFileSizeUnknown || tab->tooltipFileSizeDirty != dirty) {
        tab->tooltipFileSize = file::GetSize(path);
        tab->tooltipFileSizeDirty = dirty;
        gTabTooltipSizeLookups++;
    }
    i64 size = tab->tooltipFileSize;
    TempStr tip;
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

// a tab for a document of the restored session, its file not loaded yet; it
// shows the document's remembered tab color already
WindowTab* NewSessionTab(MainWindow* win, TabState* state) {
    WindowTab* tab = new WindowTab(win);
    tab->SetFilePath(state->filePath);
    tab->tabState = state;
    FileState* fs = FileHistoryFindByPath(state->filePath);
    ParsedColor* tabCol = fs ? GetPrefsColor(fs->tabCol) : nullptr;
    if (tabCol && tabCol->parsedOk) {
        tab->tabColor = tabCol->col;
    }
    return tab;
}

// A file in a temporary folder that is gone: an attachment Outlook extracted
// and deleted when it closed, a download in %TEMP%. It won't come back, unlike
// a file in a renamed folder or on an unplugged drive (#6618). Checking it
// costs little: temporary folders are local.
bool IsMissingTemporaryFile(Str path) {
    return IsTemporaryFile(path) && !DocumentPathExists(path);
}

// the warning about a restored session's missing files: one per window, which
// counts them, not one per file
static Kind kNotifMissingSessionFiles = "notifMissingSessionFiles";
static MainWindow* gMissingFilesWin = nullptr;
static int gMissingFilesCount = 0;

static void NotifyMissingSessionFile(MainWindow* win, Str path) {
    NotificationWnd* wnd = GetNotificationForGroup(win, kNotifMissingSessionFiles);
    if (!wnd || win != gMissingFilesWin) {
        gMissingFilesCount = 0;
    }
    gMissingFilesWin = win;
    gMissingFilesCount++;
    TempStr msg = gMissingFilesCount == 1 ? fmt(Tr("File %s not found").s, path)
                                          : fmt(Tr("%d files of the session were not found").s, gMissingFilesCount);
    if (wnd) {
        NotificationUpdateMessage(wnd, msg, kNotifDefaultTimeOut);
        return;
    }
    NotificationCreateArgs args;
    SetNotifWindow(args, win);
    args.msg = msg;
    args.timeoutMs = kNotifDefaultTimeOut;
    args.warning = true;
    args.plainText = true;
    args.groupId = kNotifMissingSessionFiles;
    ShowNotification(args);
}

// a missing temporary file has nothing worth keeping in the file history: its
// entry and thumbnail go, unless it has favorites
static void ForgetMissingTemporaryFile(Str path) {
    FileState* fs = FileHistoryFindByPath(path);
    if (fs && len(*fs->favorites) == 0) {
        FileHistoryRemove(fs);
        DeleteFileState(fs);
    } else if (fs) {
        FileHistoryDemote(path, true);
    }
    DeleteThumbnailForFile(path);
}

// A restored session's document that no longer exists keeps a tab that says so
// (#5585), unless KeepMissingSessionTabs = false (#6506): then a warning says
// it's gone. A temporary file's tab goes silently, whatever the setting
// (#6618). nullptr: no tab.
WindowTab* NewMissingSessionTab(MainWindow* win, TabState* state) {
    // a copy: forgetting the file can free the history entry state points into
    TempStr path = str::DupTemp(state->filePath);
    bool isTemp = IsTemporaryFile(path);
    logf("NewMissingSessionTab: file not found '%s', keep tab: %d, temporary: %d\n", path,
         (int)gSettings->keepMissingSessionTabs, (int)isTemp);
    if (isTemp) {
        ForgetMissingTemporaryFile(path);
        return nullptr;
    }
    FileHistoryDemote(path, true);
    if (!gSettings->keepMissingSessionTabs) {
        NotifyMissingSessionFile(win, path);
        return nullptr;
    }
    return NewSessionTab(win, state);
}

// closes win's tabs whose file no longer exists, e.g. deleted by another app (#6506)
void CloseMissingFileTabs(MainWindow* win) {
    Vec<WindowTab*> toClose;
    for (WindowTab* tab : win->Tabs()) {
        if (tab->IsAboutTab() || len(tab->filePath) == 0) {
            continue;
        }
        if (!DocumentPathExists(tab->filePath)) {
            VecAppend(toClose, tab);
        }
    }
    logf("CloseMissingFileTabs: closing %d tab(s)\n", len(toClose));
    CloseCollectedTabs(win, toClose);
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
    CloseWindow(win, true);
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
    bool hasOther = false;
    bool hasRight = false;
    bool hasLeft = false;
    bool seenCurrent = false;
    int nTabs = ctx.win->TabCount();
    for (int i = 0; i < nTabs; i++) {
        WindowTab* tab = ctx.win->GetTab(i);
        if (ctx.tab == tab) {
            seenCurrent = true;
            continue;
        }
        if (tab->IsPinnedHomeTab()) {
            continue;
        }
        hasOther = true;
        if (seenCurrent) {
            hasRight = true;
        } else {
            hasLeft = true;
        }
    }
    ctx.canCloseOtherTabs = hasOther;
    ctx.canCloseTabsToRight = hasRight;
    ctx.canCloseTabsToLeft = ctx.tab && hasLeft;
    ctx.hasDocTabs = HasOpenedDocuments(ctx.win);
    StripGroupSetFlags(ctx);
}

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
    // the next tab not hidden in a collapsed strip group
    int cur = win->GetTabIdx(win->CurrentTab());
    int step = reverse ? -1 : 1;
    int idx = cur;
    for (int i = 0; i < count; i++) {
        idx = (idx + step + count) % count;
        if (!StripGroupIsHidden(win, win->GetTab(idx))) {
            break;
        }
    }
    TabsSelect(win, idx);
}
