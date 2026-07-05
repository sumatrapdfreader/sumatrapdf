/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "base/Win.h"
#include "base/File.h"

#include "wingui/UIModels.h"
#include "wingui/Layout.h"
#include "wingui/WinGui.h"

#include "Settings.h"
#include "AppSettings.h"
#include "GlobalPrefs.h"
#include "DisplayMode.h"
#include "DocController.h"
#include "EngineBase.h"
#include "DisplayModel.h"
#include "MainWindow.h"
#include "WindowTab.h"
#include "SumatraPDF.h"
#include "Commands.h"
#include "Favorites.h"
#include "FileHistory.h"
#include "TableOfContents.h"
#include "Translations.h"
#include "Installer.h"
#include "RegistryPreview.h"
#include "RegistrySearchFilter.h"
#include "Notifications.h"
#include "CommandAvailability.h"
#include "CommandPalette.h"
#include "CommandPaletteInternal.h"


static bool AllowCommand(const AppCommandCtx& ctx, i32 cmdId) {
    return CommandShouldShow(GetCommandVisibility(cmdId, ctx, CommandSurface::Palette));
}

static TempStr ConvertPathForDisplayTemp(Str s) {
    return path::GetBaseNameTemp(s);
}

static TempStr RemovePrefixFromString(Str s) {
    return str::ReplaceTemp(s, StrL("&"), StrL(""));
}

static TempStr UpdateCommandNameTemp(MainWindow* win, int cmdId, Str s) {
    bool isToggle = false;
    bool newIsOn = false;
    switch (cmdId) {
        case CmdToggleInverseSearch: {
            extern bool gDisableInteractiveInverseSearch;
            isToggle = true;
            newIsOn = !gDisableInteractiveInverseSearch;
        } break;
        case CmdToggleFrequentlyRead: {
            isToggle = true;
            newIsOn = !gGlobalPrefs->showStartPage;
        } break;
        case CmdToggleFullscreen: {
            isToggle = true;
            newIsOn = !(win->isFullScreen || win->presentation);
        } break;
        case CmdToggleToolbar: {
            isToggle = true;
            newIsOn = !gGlobalPrefs->showToolbar;
        } break;
        case CmdToggleMenuBar: {
            isToggle = true;
            bool visible = SettingsUseTabs() ? gGlobalPrefs->showMenubarWithTabs : gGlobalPrefs->showMenubar;
            newIsOn = !visible;
        } break;
        case CmdToggleBookmarks:
        case CmdToggleTableOfContents: {
            isToggle = true;
            newIsOn = !win->tocVisible;
        } break;
        case CmdTogglePresentationMode: {
            isToggle = true;
            newIsOn = !win->presentation;
        } break;
        case CmdToggleLinks: {
            isToggle = true;
            newIsOn = !gGlobalPrefs->showLinks;
        } break;
        case CmdToggleShowAnnotations: {
            WindowTab* tab = win->CurrentTab();
            if (tab) {
                isToggle = true;
                newIsOn = tab->hideAnnotations;
            }
        } break;
        case CmdToggleContinuousView: {
            if (win->ctrl) {
                isToggle = true;
                newIsOn = !IsContinuous(win->ctrl->GetDisplayMode());
            }
        } break;
        case CmdToggleMangaMode: {
            DisplayModel* dm = win->AsFixed();
            if (dm) {
                isToggle = true;
                newIsOn = !dm->GetDisplayR2L();
            }
        } break;
        case CmdFindToggleMatchCase: {
            isToggle = true;
            newIsOn = !win->findMatchCase;
        } break;
        case CmdFindToggleMatchWholeWord: {
            isToggle = true;
            newIsOn = !win->findMatchWholeWord;
        } break;
        case CmdFavoriteToggle: {
            isToggle = true;
            newIsOn = !gGlobalPrefs->showFavorites;
        } break;
        case CmdToggleAntiAlias: {
            isToggle = true;
            newIsOn = gGlobalPrefs->disableAntiAlias;
        } break;
        case CmdToggleSmoothScroll: {
            isToggle = true;
            newIsOn = !gGlobalPrefs->smoothScroll;
        } break;
        case CmdToggleScrollbarInSinglePage: {
            isToggle = true;
            newIsOn = !gGlobalPrefs->scrollbarInSinglePage;
        } break;
        case CmdToggleLazyLoading: {
            isToggle = true;
            newIsOn = !gGlobalPrefs->lazyLoading;
        } break;
        case CmdToggleEscToExit: {
            isToggle = true;
            newIsOn = !gGlobalPrefs->escToExit;
        } break;
        case CmdToggleUseTabs: {
            isToggle = true;
            newIsOn = !gGlobalPrefs->useTabs;
        } break;
        case CmdToggleTabsMru: {
            isToggle = true;
            newIsOn = !gGlobalPrefs->tabsMru;
        } break;
        case CmdToggleZoom: {
            // TODO: this toggles via different values
        } break;
        case CmdToggleCursorPosition: {
            // TODO: this toggles 3 states
        } break;
        case CmdTogglePageInfo: {
            auto wnd = GetNotificationForGroup(win->hwndCanvas, kNotifPageInfo);
            isToggle = true;
            newIsOn = !wnd;
        } break;
        case CmdToggleTips: {
            isToggle = true;
            newIsOn = !gGlobalPrefs->showTips;
        } break;
        case CmdToggleReuseInstance: {
            isToggle = true;
            newIsOn = !gGlobalPrefs->reuseInstance;
        } break;
        case CmdToggleHoverPreview: {
            isToggle = true;
            newIsOn = gGlobalPrefs->citationHoverDelay < 0;
        } break;
    }

    if (isToggle) {
        return str::JoinTemp(s, newIsOn ? StrL(": set to true") : StrL(": set to false"));
    }

    if (cmdId == CmdToggleChmUI) {
        if (gGlobalPrefs->chmUI.useFixedPageUI) {
            return str::JoinTemp(s, StrL(": browser"));
        }
        return str::JoinTemp(s, StrL(": fixed"));
    }

    if (cmdId == CmdToggleToolbarPosition) {
        Str next = ToolbarAtBottom() ? StrL("top") : StrL("bottom");
        return str::JoinTemp(s, StrL(": set to "), next);
    }

    if (cmdId == CmdToggleWindowsPreviewer) {
        if (IsPreviewInstalled()) {
            return _TRA("Un-register Windows Previewer");
        }
        return _TRA("Register Windows Previewer");
    }

    if (cmdId == CmdToggleWindowsSearchFilter) {
        if (IsSearchFilterInstalled()) {
            return _TRA("Un-register Windows Search Filter");
        }
        return _TRA("Register Windows Search Filter");
    }

    if (cmdId == CmdAIChatWithClaudeCode) {
        return _TRA("AI Claude chat with document");
    }
    if (cmdId == CmdAIChatWithGrokBuild) {
        return _TRA("AI Grok chat with document");
    }
    if (cmdId == CmdAIChatWithOpenAICodex) {
        return _TRA("AI Codex chat with document");
    }

    return s;
}

static void AppendTab(StrVecCP& tabs, WindowTab* tab, WindowTab* currTab, int& currTabIdx) {
    ItemDataCP data;
    data.tab = tab;
    if (tab->IsAboutTab()) {
        tabs.Append(_TRA("Home"), data);
    } else {
        auto name = path::GetBaseNameTemp(tab->filePath);
        if (len(name) == 0) {
            return;
        }
        tabs.Append(name, data);
    }
    if (tab == currTab) {
        currTabIdx = len(tabs) - 1;
        logf("currTabIdx: %d\n", currTabIdx);
    }
}

void CommandPaletteWnd::CollectTabsRegular(MainWindow* mainWin, WindowTab* currTab) {
    currTabIdx = 0;
    tabs.Reset();
    for (MainWindow* w : gWindows) {
        for (WindowTab* tab : w->Tabs()) {
            AppendTab(tabs, tab, currTab, currTabIdx);
        }
    }
}

void CommandPaletteWnd::CollectTabsMru(MainWindow* mainWin, WindowTab* currTab) {
    currTabIdx = 0;
    tabs.Reset();
    if (currTab) {
        AppendTab(tabs, currTab, currTab, currTabIdx);
    }
    Vec<WindowTab*>* history = mainWin->tabSelectionHistory;
    if (history) {
        for (int i = len(*history) - 1; i >= 0; i--) {
            WindowTab* tab = (*history)[i];
            if (tab == currTab) {
                continue;
            }
            AppendTab(tabs, tab, currTab, currTabIdx);
        }
    }
    for (MainWindow* w : gWindows) {
        for (WindowTab* tab : w->Tabs()) {
            bool alreadyAdded = false;
            for (int i = 0; i < len(tabs); i++) {
                if (tabs.AtData(i)->tab == tab) {
                    alreadyAdded = true;
                    break;
                }
            }
            if (!alreadyAdded) {
                AppendTab(tabs, tab, currTab, currTabIdx);
            }
        }
    }
}

static void CollectTocRec(StrVecCP& toc, TocItem* ti, int indent, int currPageNo, int& bestIdx, int& bestPageNo) {
    while (ti) {
        Str title = ti->title ? ti->title : StrL("");
        ItemDataCP data;
        data.tocItem = ti;
        data.indent = indent;
        data.pageNo = ti->pageNo;
        if (len(title) > 0) {
            toc.Append(title, data);
        }
        int pageNo = ti->pageNo;
        if (len(title) > 0 && pageNo > 0 && pageNo <= currPageNo && pageNo > bestPageNo) {
            bestPageNo = pageNo;
            bestIdx = len(toc) - 1;
        }
        if (ti->child) {
            CollectTocRec(toc, ti->child, indent + 1, currPageNo, bestIdx, bestPageNo);
        }
        ti = ti->next;
    }
}

void CommandPaletteWnd::CollectToc(MainWindow* mainWin) {
    toc.Reset();
    currTocIdx = 0;
    if (!mainWin->ctrl) {
        return;
    }
    TocTree* tree = mainWin->ctrl->GetToc();
    if (!tree || !tree->root) {
        return;
    }
    int currPageNo = mainWin->ctrl->CurrentPageNo();
    int bestIdx = 0;
    int bestPageNo = 0;
    CollectTocRec(toc, tree->root->child, 0, currPageNo, bestIdx, bestPageNo);
    currTocIdx = bestIdx;
}

static void AppendFavoritesForFile(StrVecCP& favorites, FileState* fs, bool isCurrent) {
    if (!fs || !fs->favorites) {
        return;
    }
    for (Favorite* fav : *fs->favorites) {
        TempStr rn = FavReadableNameTemp(fav);
        TempStr disp;
        if (isCurrent) {
            disp = rn;
        } else {
            TempStr base = path::GetBaseNameTemp(fs->filePath);
            disp = fmt("%s : %s", base, rn);
        }
        if (len(disp) == 0) {
            continue;
        }
        ItemDataCP data;
        data.favFs = fs;
        data.fav = fav;
        favorites.Append(disp, data);
    }
}

void CommandPaletteWnd::CollectFavorites(MainWindow* mainWin) {
    favorites.Reset();
    WindowTab* currTab = mainWin->CurrentTab();
    Str currFilePath = currTab ? currTab->filePath : nullptr;

    FileState* currFs = nullptr;
    if (currFilePath) {
        for (FileState* fs : *gGlobalPrefs->fileStates) {
            if (str::Eq(fs->filePath, currFilePath)) {
                currFs = fs;
                break;
            }
        }
    }
    if (currFs) {
        AppendFavoritesForFile(favorites, currFs, true);
    }
    for (FileState* fs : *gGlobalPrefs->fileStates) {
        if (fs == currFs) {
            continue;
        }
        AppendFavoritesForFile(favorites, fs, false);
    }
}

void CommandPaletteWnd::CollectStrings(MainWindow* mainWin) {
    Point cursorPos = HwndGetCursorPos(mainWin->hwndCanvas);
    AppCommandCtx ctx = NewAppCommandCtx(mainWin, cursorPos);

    if (smartTabMode && gGlobalPrefs->tabsMru) {
        CollectTabsMru(mainWin, ctx.tab);
    } else {
        CollectTabsRegular(mainWin, ctx.tab);
    }

    CollectToc(mainWin);
    CollectFavorites(mainWin);

    fileHistory.Reset();
    for (FileState* fs : *gGlobalPrefs->fileStates) {
        TempStr s = ConvertPathForDisplayTemp(fs->filePath);
        if (len(s) == 0) {
            continue;
        }
        ItemDataCP data;
        data.filePath = fs->filePath;
        fileHistory.Append(s, data);
    }

    StrVecCP tempCommands;
    int cmdId = (int)CmdFirst + 1;
    for (int off = 0; SeqStrAt(gCommandDescriptions, off);) {
        Str name = SeqStrAt(gCommandDescriptions, off);
        if (!AllowCommand(ctx, (i32)cmdId)) {
            if (!SeqStrAdvance(gCommandDescriptions, off, &cmdId)) {
                break;
            }
            continue;
        }
        ReportIf(len(name) == 0);
        ItemDataCP data;
        data.cmdId = (i32)cmdId;
        auto nameTranslated = trans::GetTranslation(name);
        auto nameUpdated = UpdateCommandNameTemp(mainWin, cmdId, nameTranslated);
        tempCommands.Append(nameUpdated, data);
        if (!SeqStrAdvance(gCommandDescriptions, off, &cmdId)) {
            break;
        }
    }

    auto curr = gFirstCustomCommand;
    while (curr) {
        TempStr name = curr->name;
        cmdId = curr->id;
        if (cmdId > 0 && !str::IsEmptyOrWhiteSpace(name)) {
            if (AllowCommand(ctx, cmdId)) {
                ItemDataCP data;
                data.cmdId = cmdId;
                name = RemovePrefixFromString(name);
                tempCommands.Append(name, data);
            }
        }
        curr = curr->next;
    }

    SortNoCase(&tempCommands);
    int n = len(tempCommands);
    commands.Reset();
    for (int i = 0; i < n; i++) {
        commands.AppendFrom(&tempCommands, i);
    }
}
