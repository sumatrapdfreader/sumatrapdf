/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "base/Win.h"
#include "base/File.h"

#include "gui/UIModels.h"
#include "gui/Layout.h"
#include "gui/win/WinGui.h"
#include "gui/PlatformFont.h"
#include "gui/Gfx.h"
#include "gui/VirtCtrl.h"

#include "base/SettingsUtil.h"
#define INCLUDE_SETTINGSSTRUCTS_METADATA
#include "Settings.h"
#include "AppSettings.h"
#include "DisplayMode.h"
#include "DocController.h"
#include "EngineBase.h"
#include "DisplayModel.h"
#include "MainWindow.h"
#include "WindowTab.h"
#include "SumatraPDF.h"
#include "Canvas.h"
#include "Commands.h"
#include "Favorites.h"
#include "FileHistory.h"
#include "TableOfContents.h"
#include "Translations.h"
#include "Installer.h"
#include "RegistryPreview.h"
#include "RegistrySearchFilter.h"
#include "Notifications.h"
#include "PdfDarkMode.h"
#include "Theme.h"
#include "base/GuessFileType.h"
#include "EngineAll.h"
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
        case CmdToggleFullscreen: {
            isToggle = true;
            newIsOn = !(win->isFullScreen || win->presentation);
        } break;
        case CmdToggleToolbar: {
            isToggle = true;
            bool currentlyOn =
                win->isFullScreen ? FullscreenToolbarModeFromPrefs() != kToolbarHide : !ToolbarModeIsHidden();
            newIsOn = !currentlyOn;
        } break;
        case CmdToggleMenuBar: {
            isToggle = true;
            bool visible = SettingsUseTabs() ? gSettings->showMenubarWithTabs : gSettings->showMenubar;
            newIsOn = !visible;
        } break;
        case CmdToggleBookmarks:
        case CmdToggleTableOfContents: {
            isToggle = true;
            newIsOn = !win->uiState.tocVisible;
        } break;
        case CmdTogglePresentationMode: {
            isToggle = true;
            newIsOn = !win->presentation;
        } break;
        case CmdToggleLinks: {
            isToggle = true;
            newIsOn = !gSettings->showLinks;
        } break;
        case CmdToggleHighlightFormFields: {
            isToggle = true;
            newIsOn = !gSettings->highlightFormFields;
        } break;
        case CmdToggleDisableLinks: {
            isToggle = true;
            newIsOn = !gSettings->disableLinks;
        } break;
        case CmdToggleImages: {
            isToggle = true;
            newIsOn = !ShowImageOutlines();
        } break;
        case CmdToggleTransparencyGrid: {
            isToggle = true;
            newIsOn = !ShowTransparencyGrid();
        } break;
        case CmdTogglePageGrid: {
            isToggle = true;
            newIsOn = !ShowPageGrid();
        } break;
        case CmdToggleLaserPointer: {
            isToggle = true;
            newIsOn = !IsLaserPointerActive();
        } break;
        case CmdToggleHoverPreview: {
            isToggle = true;
            newIsOn = gSettings->citationHoverDelay < 0;
        } break;
        case CmdDebugShowFitContentArea: {
            isToggle = true;
            newIsOn = !ShowFitContentArea();
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
        case CmdToggleUniformPageWidth: {
            DisplayModel* dm = win->AsFixed();
            if (dm) {
                isToggle = true;
                newIsOn = !dm->GetUniformPageWidth();
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
            newIsOn = !gSettings->showFavorites;
        } break;
        case CmdTogglePageInfo: {
            isToggle = true;
            newIsOn = !win->pageInfoWanted;
        } break;
        case CmdTogglePageBoxes: {
            isToggle = true;
            newIsOn = !win->showPageBoxes;
        } break;
        case CmdTogglePreservePdfImages: {
            isToggle = true;
            newIsOn = !GetPreservePdfImagesInDarkMode();
        } break;
        case CmdDebugTogglePredictiveRender: {
            isToggle = true;
            newIsOn = !gPredictiveRender;
        } break;
        case CmdToggleEngineeringDrawingEnhance: {
            DisplayModel* dm = win->AsFixed();
            if (dm) {
                isToggle = true;
                newIsOn = !EngineMupdfCadEnhanceActive(dm->GetEngine());
            }
        } break;
    }

    if (isToggle) {
        return str::JoinTemp(s, newIsOn ? StrL(": set to true") : StrL(": set to false"));
    }

    // these two cycle through values rather than on and off, so they name what
    // comes next instead of saying set to true / false
    if (cmdId == CmdToggleZoom) {
        WindowTab* tab = win->CurrentTab();
        if (tab && tab->IsDocLoaded()) {
            Str zoomName;
            ZoomToString(&zoomName, tab->NextToggleZoom(), nullptr);
            TempStr res = str::JoinTemp(s, StrL(": switch to "), zoomName);
            str::Free(zoomName);
            return res;
        }
    }

    if (cmdId == CmdToggleCursorPosition) {
        Str unit = NextCursorPositionUnitName(win);
        if (unit) {
            return str::JoinTemp(s, StrL(": switch to "), unit);
        }
    }

    if (cmdId == CmdToggleLightDarkTheme) {
        // this toggle picks a theme, so name it instead of saying true / false
        Str target = ToggleLightDarkThemeTargetName();
        if (target) {
            return str::JoinTemp(s, StrL(": switch to "), target);
        }
    }

    if (cmdId == CmdToggleWindowsPreviewer) {
        if (IsPreviewInstalled()) {
            return _TRA("Unregister Windows Previewer");
        }
        return _TRA("Register Windows Previewer");
    }

    if (cmdId == CmdToggleWindowsSearchFilter) {
        if (IsSearchFilterInstalled()) {
            return _TRA("Unregister Windows Search Filter");
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
    if (cmdId == CmdAIChatWithAntiGravity) {
        return _TRA("AI Antigravity chat with document");
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

void CommandPaletteWnd::CollectTabsRegular(MainWindow* /*mainWin*/, WindowTab* currTab) {
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
    Str currFilePath = currTab ? currTab->filePath : Str();

    FileState* currFs = nullptr;
    if (currFilePath) {
        for (FileState* fs : *gSettings->fileStates) {
            if (str::Eq(fs->filePath, currFilePath)) {
                currFs = fs;
                break;
            }
        }
    }
    if (currFs) {
        AppendFavoritesForFile(favorites, currFs, true);
    }
    for (FileState* fs : *gSettings->fileStates) {
        if (fs == currFs) {
            continue;
        }
        AppendFavoritesForFile(favorites, fs, false);
    }
}

static void CollectBoolSettingsInStruct(StrVecCP& out, const StructInfo* info, u8* base, Str prefix) {
    if (!info || !base) {
        return;
    }
    const char* fieldName = info->fieldNames;
    for (u16 i = 0; i < info->fieldCount; i++) {
        const FieldInfo& field = info->fields[i];
        Str fname(fieldName);
        fieldName += len(fname) + 1;
        if (field.internal || field.type == SettingType::Comment || field.offset == (size_t)-1) {
            continue;
        }
        u8* fieldPtr = base + field.offset;
        TempStr path = len(prefix) > 0 ? fmt("%s.%s", prefix, fname) : str::DupTemp(fname);
        if (field.type == SettingType::Struct) {
            CollectBoolSettingsInStruct(out, (const StructInfo*)field.value, fieldPtr, path);
            continue;
        }
        if (field.type != SettingType::Bool || len(path) == 0) {
            continue;
        }
        ItemDataCP data;
        data.boolSetting = (bool*)fieldPtr;
        data.boolSettingDefault = field.value != 0;
        out.Append(path, data);
    }
}

void CommandPaletteWnd::CollectBoolSettings() {
    boolSettings.Reset();
    if (!gSettings) {
        return;
    }
    CollectBoolSettingsInStruct(boolSettings, &gSettingsInfo, (u8*)gSettings, {});
    SortNoCase(&boolSettings);

    // changed values first, then the rest; both groups stay alphabetical
    StrVecCP ordered;
    for (int pass = 0; pass < 2; pass++) {
        for (int i = 0; i < len(boolSettings); i++) {
            ItemDataCP* d = boolSettings.AtData(i);
            bool changed = *d->boolSetting != d->boolSettingDefault;
            if (changed == (pass == 0)) {
                ordered.AppendFrom(&boolSettings, i);
            }
        }
    }
    boolSettings = ordered;
}

void CommandPaletteWnd::CollectStrings(MainWindow* mainWin) {
    Point cursorPos = HwndGetCursorPos(mainWin->hwndCanvas);
    AppCommandCtx ctx = NewAppCommandCtx(mainWin, cursorPos);

    if (smartTabMode && gSettings->tabsMru) {
        CollectTabsMru(mainWin, ctx.tab);
    } else {
        CollectTabsRegular(mainWin, ctx.tab);
    }

    CollectToc(mainWin);
    CollectFavorites(mainWin);
    CollectBoolSettings();

    fileHistory.Reset();
    for (FileState* fs : *gSettings->fileStates) {
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
        // test against the English name: a translation may not carry the prefix
        data.isDebug = str::StartsWith(name, StrL("Debug: "));
        auto nameTranslated = trans::GetTranslation(name);
        auto nameUpdated = UpdateCommandNameTemp(mainWin, cmdId, nameTranslated);
        tempCommands.Append(nameUpdated, data);
        if (!SeqStrAdvance(gCommandDescriptions, off, &cmdId)) {
            break;
        }
    }

    auto* curr = gFirstCustomCommand;
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
    // dev-only commands go last instead of sitting in the middle of the list
    // under "D"; each group keeps its alphabetical order
    for (int pass = 0; pass < 2; pass++) {
        bool wantDebug = (pass == 1);
        for (int i = 0; i < n; i++) {
            if (tempCommands.AtData(i)->isDebug == wantDebug) {
                commands.AppendFrom(&tempCommands, i);
            }
        }
    }
}
