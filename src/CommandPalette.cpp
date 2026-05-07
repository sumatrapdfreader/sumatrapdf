/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"
#include "utils/WinUtil.h"
#include "utils/Dpi.h"
#include "utils/UITask.h"
#include "utils/FileUtil.h"

#include "wingui/UIModels.h"
#include "wingui/Layout.h"
#include "wingui/WinGui.h"

#include "Settings.h"
#include "AppSettings.h"
#include "DocController.h"
#include "EngineBase.h"
#include "EngineAll.h"
#include "GlobalPrefs.h"
#include "DisplayMode.h"
#include "DisplayModel.h"
#include "MainWindow.h"
#include "Theme.h"
#include "WindowTab.h"
#include "SumatraConfig.h"
#include "Commands.h"
#include "CommandPalette.h"
#include "Accelerators.h"
#include "SumatraPDF.h"
#include "Tabs.h"
#include "ExternalViewers.h"
#include "Annotation.h"
#include "FileHistory.h"
#include "DarkModeSubclass.h"
#include "Notifications.h"
#include "Translations.h"
#include "Installer.h"
#include "RegistryPreview.h"
#include "RegistrySearchFilter.h"

#include "utils/Log.h"

// clang-format off
// those commands never show up in command palette
static i32 gBlacklistCommandsFromPalette[] = {
    CmdNone,
    CmdOpenWithKnownExternalViewerFirst,
    CmdOpenWithKnownExternalViewerLast,
    CmdCommandPalette,
    CmdNextTabSmart,
    CmdPrevTabSmart,
    CmdSetTheme,

    // managing frequently list in home tab
    CmdOpenSelectedDocument,
    CmdPinSelectedDocument,
    CmdForgetSelectedDocument,

    CmdExpandAll,   // TODO: figure proper context for it
    CmdCollapseAll, // TODO: figure proper context for it
    CmdMoveFrameFocus,

    //CmdFavoriteAdd,
    CmdFavoriteDel,

    CmdPresentationWhiteBackground,
    CmdPresentationBlackBackground,

    CmdSaveEmbeddedFile, // TODO: figure proper context for it
    CmdOpenEmbeddedPDF,
    CmdSaveAttachment,
    CmdOpenAttachment,

    CmdCreateShortcutToFile, // not sure I want this at all
    0,
};

// most commands are not valid when document is not opened
// it's shorter to list the remaining commands
static i32 gDocumentNotOpenWhitelist[] = {
    CmdOpenFile,
    CmdExit,
    CmdNewWindow,
    CmdContributeTranslation,
    CmdOptions,
    CmdAdvancedOptions,
    CmdAdvancedSettings,
    CmdChangeLanguage,
    CmdCheckUpdate,
    CmdHelpOpenManual,
    CmdHelpOpenManualOnWebsite,
    CmdHelpOpenKeyboardShortcuts,
    CmdHelpVisitWebsite,
    CmdHelpAbout,
    CmdDebugDownloadSymbols,
    CmdDebugShowNotif,
    CmdDebugStartStressTest,
    CmdDebugTestApp,
    CmdDebugTogglePredictiveRender,
    CmdDebugToggleRtl,
    CmdChangeScrollbar,
    CmdToggleAntiAlias,
    CmdToggleSmoothScroll,
    CmdToggleScrollbarInSinglePage,
    CmdToggleLazyLoading,
    CmdToggleFullscreen,
    CmdToggleMenuBar,
    CmdToggleToolbar,
    CmdToggleUseTabs,
    CmdToggleTips,
    CmdToggleFrequentlyRead,
    CmdToggleChmUI,
    CmdToggleReuseInstance,
    CmdFavoriteToggle,
    CmdShowLog,
    CmdClearHistory,
    CmdReopenLastClosedFile,
    CmdSelectNextTheme,
    CmdListPrinters,
    CmdDebugCrashMe,
    CmdDebugCorruptMemory,
    CmdScreenshot,
    CmdTabGroupRestore,
    CmdSetScreenshotHotkey,
    0,
};

// for those commands do not activate main window
// for example those that show dialogs (because the main window takes
// focus away from them)
static i32 gCommandsNoActivate[] = {
    CmdOptions,
    CmdChangeLanguage,
    CmdHelpAbout,
    CmdHelpOpenManual,
    CmdHelpOpenManualOnWebsite,
    CmdHelpOpenKeyboardShortcuts,
    CmdHelpVisitWebsite,
    CmdOpenFile,
    CmdProperties,
    CmdNewWindow,
    CmdDuplicateInNewWindow,
    CmdPdShowInfo,
    CmdDocumentShowOutline,
    CmdListPrinters,
    CmdCropImage,
    CmdResizeImage,
    CmdTabGroupSave,
    CmdTabGroupRestore,
    // TOOD: probably more
    0,
};

static i32 gCommandsDebugOnly[] = {
    CmdDebugCorruptMemory,
    CmdDebugCrashMe,
    CmdDebugDownloadSymbols,
    CmdDebugTestApp,
    CmdDebugShowNotif,
    CmdDebugStartStressTest,
    0,
};

// clang-format on

// those are shared with Menu.cpp
extern UINT_PTR removeIfAnnotsNotSupported[];
extern UINT_PTR disableIfNoSelection[];

extern UINT_PTR removeIfNoInternetPerms[];
extern UINT_PTR removeIfNoFullscreenPerms[];
extern UINT_PTR removeIfNoPrefsPerms[];
extern UINT_PTR removeIfNoDiskAccessPerm[];
extern UINT_PTR removeIfNoCopyPerms[];
extern UINT_PTR removeIfChm[];

static bool IsCmdInList(i32 cmdId, i32* ids) {
    while (*ids) {
        if (cmdId == *ids) {
            return true;
        }
        ids++;
    }
    return false;
}

// a must end with sentinel value of 0
static bool IsCmdInMenuList(i32 cmdId, UINT_PTR* a) {
    UINT_PTR id = (UINT_PTR)cmdId;
    for (int i = 0; a[i]; i++) {
        if (a[i] == id) {
            return true;
        }
    }
    return false;
}

struct ItemDataCP {
    i32 cmdId = 0;
    WindowTab* tab = nullptr;
    const char* filePath = nullptr;
};

using StrVecCP = StrVecWithData<ItemDataCP>;

struct ListBoxModelCP : ListBoxModel {
    StrVecCP strings;

    ListBoxModelCP() = default;
    ~ListBoxModelCP() override = default;
    int ItemsCount() override { return strings.Size(); }
    const char* Item(int i) override { return strings.At(i); }
    ItemDataCP* Data(int i) { return strings.AtData(i); }
};

struct CommandPaletteWnd : Wnd {
    ~CommandPaletteWnd() override = default;
    HFONT font = nullptr;
    MainWindow* win = nullptr;

    Edit* editQuery = nullptr;
    StrVecCP tabs;
    StrVecCP fileHistory;
    StrVecCP commands;
    ListBox* listBox = nullptr;
    Static* staticInfo = nullptr;

    StrVec filterWords;
    Vec<u8> highlighted; // reused across DrawListBoxItem calls

    int currTabIdx = 0;
    bool smartTabMode = false;
    bool stickyMode = false;

    bool PreTranslateMessage(MSG&) override;
    LRESULT WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) override;

    void CollectStrings(MainWindow*);
    void CollectTabsRegular(MainWindow*, WindowTab* currTab);
    void CollectTabsMru(MainWindow*, WindowTab* currTab);
    void FilterStringsForQuery(const char*, StrVecCP&);

    bool Create(MainWindow* win, const char* prefix, int smartTabAdvance);
    void QueryChanged();

    void ExecuteCurrentSelection();
    bool AdvanceSelection(int dir);
    void SwitchToCommands();
    void SwitchToTabs();
    void SwitchToEverything();
    void SwitchToFileHistory();
    void OnSelectionChange();
    void OnListDoubleClick();
    void DrawListBoxItem(ListBox::DrawItemEvent* ev);
};

struct CommandPaletteBuildCtx {
    const char* filePath = nullptr;
    bool isDocLoaded = false;
    bool supportsAnnots = false;
    bool hasSelection = false;
    bool isChm = false;
    bool canSendEmail = false;
    Annotation* annotationUnderCursor = nullptr;
    bool hasUnsavedAnnotations = false;
    bool isCursorOnPage = false;
    bool cursorOnLinkTarget = false;
    bool cursorOnComment = false;
    bool cursorOnImage = false;
    bool hasToc = false;
    bool allowToggleMenuBar = false;
    bool canCloseOtherTabs = false;
    bool canCloseTabsToRight = false;
    bool canCloseTabsToLeft = false;
    bool isPdf = false;
    bool isPdfEncrypted = false;
    int pageCount = 0;
    bool isSinglePage = false;
    bool hasDocTabs = false;
    Kind engineKind = nullptr;

    ~CommandPaletteBuildCtx() = default;
};

static const char* SkipWS(const char* s) {
    while (str::IsWs(*s)) {
        s++;
    }
    return s;
}

static bool AllowCommand(const CommandPaletteBuildCtx& ctx, i32 cmdId) {
    if (cmdId <= CmdFirst) {
        return false;
    }
    CustomCommand* cmd = FindCustomCommand(cmdId);
    int origCmdId = cmd ? cmd->origId : 0;
    if (origCmdId == CmdSetTheme) {
        return true;
    }

    if (IsCmdInList(cmdId, gCommandsDebugOnly)) {
        return gIsDebugBuild;
    }

    if (IsCmdInList(cmdId, gBlacklistCommandsFromPalette)) {
        return false;
    }

    if (CmdCloseOtherTabs == cmdId) {
        return ctx.canCloseOtherTabs;
    }
    if (CmdCloseTabsToTheRight == cmdId) {
        return ctx.canCloseTabsToRight;
    }
    if (CmdCloseTabsToTheLeft == cmdId) {
        return ctx.canCloseTabsToLeft;
    }

    if (CmdReopenLastClosedFile == cmdId) {
        return RecentlyCloseDocumentsCount() > 0;
    }

    if (CmdTabGroupSave == cmdId) {
        return ctx.hasDocTabs;
    }

    // must check before ctx.isDocLoaded
    if (cmdId == CmdToggleWindowsPreviewer || cmdId == CmdToggleWindowsSearchFilter) {
        return IsOurExeInstalled();
    }

    // when document is not loaded, most commands are not available
    // except those white-listed
    if (IsCmdInList(cmdId, gDocumentNotOpenWhitelist)) {
        return true;
    }

    if (!ctx.isDocLoaded) {
        return false;
    }

    bool isKnownEV = (cmdId >= CmdOpenWithKnownExternalViewerFirst) && (cmdId <= CmdOpenWithKnownExternalViewerLast);
    if (origCmdId == CmdViewWithExternalViewer || isKnownEV) {
        if (!ctx.isDocLoaded) {
            return false;
        }
        if (isKnownEV) {
            // TODO: match file name
            return HasKnownExternalViewerForCmd(cmdId);
        }
        const char* filter = GetCommandStringArg(cmd, kCmdArgFilter, nullptr);
        return PathMatchFilter(ctx.filePath, filter);
    }

    if ((origCmdId == CmdSelectionHandler) || IsCmdInMenuList(cmdId, disableIfNoSelection)) {
        return ctx.hasSelection;
    }

    // we only want to show this in home page
    if (cmdId == CmdToggleFrequentlyRead) {
        return !ctx.isDocLoaded;
    }

    if (cmdId == CmdToggleMenuBar) {
        return ctx.allowToggleMenuBar;
    }

    if (!ctx.supportsAnnots) {
        if ((cmdId >= (i32)CmdCreateAnnotFirst) && (cmdId <= (i32)CmdCreateAnnotLast)) {
            return false;
        }
        if (IsCmdInMenuList(cmdId, removeIfAnnotsNotSupported)) {
            return false;
        }
    }

    if (ctx.isChm && IsCmdInMenuList(cmdId, removeIfChm)) {
        return false;
    }

    if (!ctx.canSendEmail && (cmdId == CmdSendByEmail)) {
        return false;
    }

    if (!ctx.isPdf && cmdId == CmdPdShowInfo) {
        return false;
    }

    if (!ctx.hasToc && cmdId == CmdDocumentShowOutline) {
        return false;
    }

    if (!ctx.isPdf && cmdId == CmdPdfBake) {
        return false;
    }

    if (!ctx.isPdf && cmdId == CmdPdfCompress) {
        return false;
    }

    if (!ctx.isPdf && cmdId == CmdPdfDecompress) {
        return false;
    }

    if (cmdId == CmdPdfDeletePages && (!ctx.isPdf || ctx.pageCount < 2)) {
        return false;
    }

    if (cmdId == CmdPdfExtractPages && (!ctx.isPdf || ctx.pageCount < 2)) {
        return false;
    }

    if (cmdId == CmdPdfEncrypt && (!ctx.isPdf || ctx.isPdfEncrypted)) {
        return false;
    }

    if (cmdId == CmdPdfDecrypt && (!ctx.isPdf || !ctx.isPdfEncrypted)) {
        return false;
    }

    if (cmdId == CmdDocumentExtractText) {
        bool canExtract = ctx.engineKind == kindEngineMupdf || ctx.engineKind == kindEngineDjVu;
        if (!canExtract) {
            return false;
        }
    }

    if (ctx.isSinglePage && cmdId == CmdToggleMangaMode) {
        return false;
    }

    if (!ctx.annotationUnderCursor) {
        if (cmdId == CmdDeleteAnnotation) {
            return false;
        }
    }

    if ((cmdId == CmdSaveAnnotations) || (cmdId == CmdSaveAnnotationsNewFile)) {
        return ctx.hasUnsavedAnnotations;
    }

    if ((cmdId == CmdCheckUpdate) && gIsStoreBuild) {
        return false;
    }

    bool remove = false;
    if (!HasPermission(Perm::InternetAccess)) {
        remove |= IsCmdInMenuList(cmdId, removeIfNoInternetPerms);
    }
    if (!HasPermission(Perm::FullscreenAccess)) {
        remove |= IsCmdInMenuList(cmdId, removeIfNoFullscreenPerms);
    }
    if (!HasPermission(Perm::SavePreferences)) {
        remove |= IsCmdInMenuList(cmdId, removeIfNoPrefsPerms);
    }
    if (!HasPermission(Perm::PrinterAccess)) {
        remove |= (cmdId == CmdPrint);
    }
    if (!CanAccessDisk()) {
        remove |= IsCmdInMenuList(cmdId, removeIfNoDiskAccessPerm);
    }
    if (!HasPermission(Perm::CopySelection)) {
        remove |= IsCmdInMenuList(cmdId, removeIfNoCopyPerms);
    }
    if (remove) {
        return false;
    }

    if (!ctx.cursorOnLinkTarget && (cmdId == CmdCopyLinkTarget)) {
        return false;
    }
    if (!ctx.cursorOnComment && (cmdId == CmdCopyComment)) {
        return false;
    }
    if (!ctx.cursorOnImage && (cmdId == CmdCopyImage)) {
        return false;
    }
    if ((cmdId == CmdToggleBookmarks) || (cmdId == CmdToggleTableOfContents)) {
        return ctx.hasToc;
    }
    return true;
}

static TempStr ConvertPathForDisplayTemp(const char* s) {
    return path::GetBaseNameTemp(s);
}

static TempStr RemovePrefixFromString(const char* s) {
    return str::ReplaceTemp(s, "&", "");
}

static const char* UpdateCommandNameTemp(MainWindow* win, int cmdId, const char* s) {
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
            //    isToggle = true;
            //    auto notif = GetNotificationForGroup(win->hwndCanvas, kNotifCursorPos);
            //    newIsOn = !notif;
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
    }

    if (isToggle) {
        s = (const char*)str::JoinTemp(s, newIsOn ? ": on" : ": off");
        return s;
    }

    if (cmdId == CmdToggleChmUI) {
        if (gGlobalPrefs->chmUI.useFixedPageUI) {
            s = (const char*)str::JoinTemp(s, ": browser");
        } else {
            s = (const char*)str::JoinTemp(s, ": fixed");
        }
        return s;
    }

    if (cmdId == CmdToggleWindowsPreviewer) {
        if (IsPreviewInstalled()) {
            s = _TRA("Un-register Windows Previewer");
        } else {
            s = _TRA("Register Windows Previewer");
        }
        return s;
    }

    if (cmdId == CmdToggleWindowsSearchFilter) {
        if (IsSearchFilterInstalled()) {
            s = _TRA("Un-register Windows Search Filter");
        } else {
            s = _TRA("Register Windows Search Filter");
        }
        return s;
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
        tabs.Append(name, data);
    }
    if (tab == currTab) {
        currTabIdx = tabs.Size() - 1;
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
    // current tab is by definition the most recently used, so add it first
    if (currTab) {
        AppendTab(tabs, currTab, currTab, currTabIdx);
    }
    // then add remaining tabs in MRU order from selection history
    Vec<WindowTab*>* history = mainWin->tabSelectionHistory;
    if (history) {
        // history is oldest-first, iterate in reverse for MRU order
        for (int i = history->Size() - 1; i >= 0; i--) {
            WindowTab* tab = history->At(i);
            if (tab == currTab) {
                continue;
            }
            AppendTab(tabs, tab, currTab, currTabIdx);
        }
    }
    // add any tabs not in the history (e.g. tabs from other windows)
    for (MainWindow* w : gWindows) {
        for (WindowTab* tab : w->Tabs()) {
            bool alreadyAdded = false;
            for (int i = 0; i < tabs.Size(); i++) {
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

void CommandPaletteWnd::CollectStrings(MainWindow* mainWin) {
    CommandPaletteBuildCtx ctx;
    ctx.isDocLoaded = mainWin->IsDocLoaded();
    WindowTab* currTab = mainWin->CurrentTab();
    ctx.filePath = currTab ? currTab->filePath : nullptr;
    ctx.hasSelection = ctx.isDocLoaded && currTab && mainWin->showSelection && currTab->selectionOnPage;
    ctx.canSendEmail = CanSendAsEmailAttachment(currTab);
    ctx.isPdf = ctx.isDocLoaded && CouldBePDFDoc(currTab);
    if (ctx.isPdf && currTab) {
        ctx.isPdfEncrypted = EngineMupdfIsEncrypted(currTab->GetEngine());
    }
    if (ctx.isDocLoaded && currTab) {
        ctx.engineKind = currTab->GetEngineType();
    }
    if (ctx.isDocLoaded && mainWin->ctrl) {
        ctx.isSinglePage = IsSingle(mainWin->ctrl->GetDisplayMode());
        ctx.pageCount = mainWin->ctrl->PageCount();
    }
    ctx.allowToggleMenuBar = true;

    int nTabs = mainWin->TabCount();
    int tabIdx = mainWin->GetTabIdx(currTab);
    ctx.canCloseTabsToRight = tabIdx < (nTabs - 1);
    ctx.canCloseTabsToLeft = false;
    int nFirstDocTab = 0;
    for (int i = 0; i < nTabs; i++) {
        WindowTab* t = mainWin->GetTab(i);
        if (t->IsAboutTab()) {
            if (i > 0) {
                logf("CommandPaletteWnd::CollectStrings: unexpected about tab at idx: %d out of %d\n", i, nTabs);
                for (int j = 0; j < nTabs; j++) {
                    if (!t->IsAboutTab()) {
                        logf("i: %d path: %s\n", j, t->filePath ? t->filePath : "");
                    }
                }
                ReportIf(i > 0);
            }
            nFirstDocTab = 1;
            continue;
        }
        ctx.hasDocTabs = true;
        if (t == currTab) {
            if (i > nFirstDocTab) {
                ctx.canCloseTabsToLeft = true;
            }
            continue;
        }
        ctx.canCloseOtherTabs = true;
    }

    Point cursorPos = HwndGetCursorPos(mainWin->hwndCanvas);

    DisplayModel* dm = mainWin->AsFixed();
    if (dm) {
        auto engine = dm->GetEngine();
        ctx.supportsAnnots = EngineSupportsAnnotations(engine);
        ctx.hasUnsavedAnnotations = EngineHasUnsavedAnnotations(engine);
        int pageNoUnderCursor = dm->GetPageNoByPoint(cursorPos);
        if (pageNoUnderCursor > 0) {
            ctx.isCursorOnPage = true;
        }
        ctx.annotationUnderCursor = dm->GetAnnotationAtPos(cursorPos, nullptr);

        // PointF ptOnPage = dm->CvtFromScreen(cursorPos, pageNoUnderCursor);
        //  TODO: should this be point on page?
        IPageElement* pageEl = dm->GetElementAtPos(cursorPos, nullptr);
        if (pageEl) {
            char* value = pageEl->GetValue();
            ctx.cursorOnLinkTarget = value && pageEl->Is(kindPageElementDest);
            ctx.cursorOnComment = value && pageEl->Is(kindPageElementComment);
            ctx.cursorOnImage = pageEl->Is(kindPageElementImage);
        }
    }

    if (!CanAccessDisk()) {
        ctx.supportsAnnots = false;
        ctx.hasUnsavedAnnotations = false;
    }

    ctx.hasToc = mainWin->ctrl && mainWin->ctrl->HasToc();

    if (smartTabMode && gGlobalPrefs->tabsMru) {
        CollectTabsMru(mainWin, currTab);
    } else {
        CollectTabsRegular(mainWin, currTab);
    }

    // append paths of files from history, excluding
    // already appended (from opened files)
    fileHistory.Reset();
    for (FileState* fs : *gGlobalPrefs->fileStates) {
        char* s = fs->filePath;
        s = ConvertPathForDisplayTemp(s);
        ItemDataCP data;
        data.filePath = fs->filePath;
        fileHistory.Append(s, data);
    }

    StrVecCP tempCommands;
    int cmdId = (int)CmdFirst + 1;
    for (SeqStrings name = gCommandDescriptions; name; seqstrings::Next(name, &cmdId)) {
        if (!AllowCommand(ctx, (i32)cmdId)) {
            continue;
        }
        ReportIf(str::Leni(name) == 0);
        ItemDataCP data;
        data.cmdId = (i32)cmdId;
        auto nameTranslated = trans::GetTranslation(name);
        auto nameUpdated = UpdateCommandNameTemp(mainWin, cmdId, (TempStr)nameTranslated);
        tempCommands.Append(nameUpdated, data);
    }

    // includes externalViewers, selectionHandlers and keyboardShortcuts
    auto curr = gFirstCustomCommand;
    while (curr) {
        TempStr name = (TempStr)curr->name;
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

    // we want the commands sorted
    SortNoCase(&tempCommands);
    int n = tempCommands.Size();
    commands.Reset();
    for (int i = 0; i < n; i++) {
        commands.AppendFrom(&tempCommands, i);
    }
}

static void EditSetTextAndFocus(Edit* e, const char* s) {
    e->SetText(s);
    e->SetCursorPositionAtEnd();
    HwndSetFocus(e->hwnd);
}

void CommandPaletteWnd::SwitchToCommands() {
    EditSetTextAndFocus(editQuery, kPalettePrefixCommands);
}

void CommandPaletteWnd::SwitchToTabs() {
    EditSetTextAndFocus(editQuery, kPalettePrefixTabs);
}

void CommandPaletteWnd::SwitchToEverything() {
    EditSetTextAndFocus(editQuery, kPalettePrefixEverything);
}

void CommandPaletteWnd::SwitchToFileHistory() {
    EditSetTextAndFocus(editQuery, kPalettePrefixFileHistory);
}

CommandPaletteWnd* gCommandPaletteWnd = nullptr;
HWND gCommandPaletteHwnd = nullptr;
static HWND gHwndToActivateOnClose = nullptr;
static WindowTab* gTabToSelectOnClose = nullptr;
static i32 gCmdIdToExecOnClose = 0;

void SafeDeleteCommandPaletteWnd() {
    if (!gCommandPaletteWnd) {
        return;
    }

    MainWindow* win = gCommandPaletteWnd->win;
    auto tmp = gCommandPaletteWnd;
    gCommandPaletteWnd = nullptr;
    gCommandPaletteHwnd = nullptr;
    delete tmp;
    if (gHwndToActivateOnClose) {
        HWND fg = GetForegroundWindow();
        // Only restore main window if no other window already took foreground.
        // This avoids stealing focus from e.g. the screenshot overlay which
        // may have been shown while the command palette was closing.
        if (!fg || fg == gHwndToActivateOnClose) {
            SetActiveWindow(gHwndToActivateOnClose);
        }
        gHwndToActivateOnClose = nullptr;
    }
    if (gTabToSelectOnClose) {
        WindowTab* tab = gTabToSelectOnClose;
        gTabToSelectOnClose = nullptr;
        if (tab->win && IsMainWindowValid(tab->win) && tab->win->GetTabIdx(tab) >= 0) {
            SelectTabInWindow(tab);
        }
    }
    if (gCmdIdToExecOnClose != 0) {
        i32 cmdId = gCmdIdToExecOnClose;
        gCmdIdToExecOnClose = 0;
        if (win && IsMainWindowValid(win)) {
            HwndPostCommand(win->hwndFrame, cmdId);
        }
    }
}

static void ScheduleDeleteAndExecCommand(i32 cmdId = 0) {
    if (!gCommandPaletteWnd) {
        return;
    }
    gCmdIdToExecOnClose = cmdId;
    if (IsMainWindowValid(gCommandPaletteWnd->win)) {
        HighlightTab(gCommandPaletteWnd->win, nullptr);
    }
    auto fn = MkFunc0Void(SafeDeleteCommandPaletteWnd);
    uitask::Post(fn, "SafeDeleteCommandPaletteWnd");
}

LRESULT CommandPaletteWnd::WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_ACTIVATE:
            if (wp == WA_INACTIVE) {
                ScheduleDeleteAndExecCommand();
                return 0;
            }
            break;
        case WM_COMMAND: {
            int cmdId = LOWORD(wp);
            CustomCommand* cmd = FindCustomCommand(cmdId);
            if (cmd != nullptr) {
                cmdId = cmd->origId;
            }
            switch (cmdId) {
                case CmdNextTabSmart:
                case CmdPrevTabSmart: {
                    int dir = cmdId == CmdNextTabSmart ? 1 : -1;
                    return AdvanceSelection(dir);
                }
            }
        }
    }

    return WndProcDefault(hwnd, msg, wp, lp);
}

void CommandPaletteWnd::OnSelectionChange() {
    int idx = listBox->GetCurrentSelection();
    // logf("Selection changed: %d\n", idx);
    if (!smartTabMode) {
        return;
    }
    auto m = (ListBoxModelCP*)listBox->model;
    ItemDataCP* data = m->strings.AtData(idx);
    HighlightTab(win, data->tab);
}

static void SetCurrentSelection(CommandPaletteWnd* wnd, int idx) {
    wnd->listBox->SetCurrentSelection(idx);
    wnd->OnSelectionChange();
}

bool CommandPaletteWnd::AdvanceSelection(int dir) {
    if (dir == 0) {
        return false;
    }
    int n = listBox->GetCount();
    if (n == 0) {
        return false;
    }
    int currSel = listBox->GetCurrentSelection();
    int sel = currSel + dir;
    if (sel < 0) {
        sel = n - 1;
    }
    if (sel >= n) {
        sel = 0;
    }
    SetCurrentSelection(this, sel);
    return true;
}

bool CommandPaletteWnd::PreTranslateMessage(MSG& msg) {
    if (msg.message == WM_KEYDOWN) {
        int dir = 0;
        if (msg.wParam == VK_ESCAPE) {
            ScheduleDeleteAndExecCommand();
            return true;
        }

        if (msg.wParam == VK_RETURN) {
            ExecuteCurrentSelection();
            return true;
        }

        if (msg.wParam == VK_DELETE) {
            const char* filter = editQuery->GetTextTemp();
            filter = SkipWS(filter);
            if (str::StartsWith(filter, kPalettePrefixFileHistory)) {
                int n = listBox->GetCount();
                if (n == 0) {
                    return false;
                }
                int currSel = listBox->GetCurrentSelection();
                auto m = (ListBoxModelCP*)listBox->model;
                auto d = m->Data(currSel);
                FileState* fs = gFileHistory.FindByPath(d->filePath);
                if (!fs) {
                    return true;
                }
                gFileHistory.Remove(fs);
                CollectStrings(this->win);
                this->QueryChanged();

                // restore selection for fluid use
                n = listBox->GetCount();
                if (n == 0) {
                    return true;
                }
                int lastIdx = n - 1;
                if (currSel > lastIdx) {
                    currSel = lastIdx;
                }
                listBox->SetCurrentSelection(currSel);
                return true;
            }
            return true;
        }

        if (msg.wParam == VK_UP) {
            dir = -1;
        } else if (msg.wParam == VK_DOWN) {
            dir = 1;
        }

        // ctrl+tab, ctrl+shift+tab is like up / down
        if (msg.wParam == VK_TAB) {
            if (IsCtrlPressed()) {
                dir = IsShiftPressed() ? -1 : 1;
            }
        }
        return AdvanceSelection(dir);
    }

    if (smartTabMode) {
        // in smart tab mode releasing ctrl + tab selects a tab
        if (msg.message == WM_KEYUP) {
            if (msg.wParam == VK_CONTROL) {
                if (!stickyMode) {
                    ExecuteCurrentSelection();
                }
                return true;
            }
        }
    }
    return false;
}

// all words must be present in str, ignoring the case
bool FilterMatches(const char* str, const StrVec& words) {
    int nWords = words.Size();
    for (int i = 0; i < nWords; i++) {
        auto word = words.At(i);
        if (!str::ContainsI(str, word)) {
            return false;
        }
    }
    return true;
}

void SplitFilterToWords(const char* filter, StrVec& words) {
    char* s = str::DupTemp(filter);
    char* wordStart = s;
    bool wasWs = false;
    while (*s) {
        if (str::IsWs(*s)) {
            *s = 0;
            if (!wasWs) {
                AppendIfNotExists(&words, wordStart);
                wasWs = true;
            }
            wordStart = s + 1;
        }
        s++;
    }
    if (str::Leni(wordStart) > 0) {
        AppendIfNotExists(&words, wordStart);
    }
}

static void FilterStrings(StrVecCP& strs, const StrVec& words, StrVecCP& matchedOut) {
    int n = strs.Size();
    for (int i = 0; i < n; i++) {
        const char* s = strs.At(i);
        if (!FilterMatches(s, words)) {
            continue;
        }
        matchedOut.AppendFrom(&strs, i);
    }
}

void CommandPaletteWnd::FilterStringsForQuery(const char* filter, StrVecCP& strings) {
    // for efficiency, reusing existing model
    strings.Reset();
    if (!filter) {
        filter = "";
    }

    // strip prefix and remember which lists to search
    bool searchTabs = false, searchHistory = false, searchCommands = false;
    if (str::StartsWith(filter, kPalettePrefixEverything)) {
        filter++;
        searchTabs = searchHistory = searchCommands = true;
    } else if (str::StartsWith(filter, kPalettePrefixTabs)) {
        filter++;
        searchTabs = true;
    } else if (str::StartsWith(filter, kPalettePrefixFileHistory)) {
        filter++;
        searchHistory = true;
    } else {
        if (str::StartsWith(filter, kPalettePrefixCommands)) {
            filter++;
        }
        searchCommands = true;
    }

    // split filter into words once
    filterWords.Reset();
    SplitFilterToWords(filter, filterWords);

    if (searchTabs) {
        FilterStrings(tabs, filterWords, strings);
    }
    if (searchHistory) {
        FilterStrings(fileHistory, filterWords, strings);
    }
    if (searchCommands) {
        FilterStrings(commands, filterWords, strings);
    }
}

void CommandPaletteWnd::QueryChanged() {
    const char* filter = editQuery->GetTextTemp();
    filter = SkipWS(filter);
    int currSelIdx = 0;
    auto m = (ListBoxModelCP*)listBox->model;
    int nItemsPrev = m->ItemsCount();
    if (smartTabMode) {
        if (!stickyMode) {
            if (str::Len(filter) > 1) {
                // we only advertise this for 'space' but any change to query
                // enables sticky mode (i.e. no auto-selection
                stickyMode = true;
                currSelIdx = listBox->GetCurrentSelection();
            }
        }
    }
    FilterStringsForQuery(filter, m->strings);
    listBox->SetModel(m);
    int nItems = m->ItemsCount();
    if (nItems == 0) {
        return;
    }
    if (stickyMode && nItemsPrev == nItems) {
        SetCurrentSelection(this, currSelIdx);
        return;
    }
    SetCurrentSelection(this, 0);
}

void CommandPaletteWnd::ExecuteCurrentSelection() {
    int idx = listBox->GetCurrentSelection();
    if (idx < 0) {
        return;
    }
    auto m = (ListBoxModelCP*)listBox->model;
    ItemDataCP* data = m->strings.AtData(idx);
    i32 cmdId = data->cmdId;
    if (cmdId != 0) {
        bool noActivate = IsCmdInList(cmdId, gCommandsNoActivate);
        if (noActivate) {
            gHwndToActivateOnClose = nullptr;
        }
        ScheduleDeleteAndExecCommand(cmdId);
        return;
    }

    WindowTab* tab = data->tab;
    if (tab != nullptr) {
        MainWindow* mainWin = tab->win;
        gTabToSelectOnClose = tab;
        gHwndToActivateOnClose = mainWin->hwndFrame;
        ScheduleDeleteAndExecCommand();
        return;
    }
    auto filePath = data->filePath;
    if (filePath) {
        LoadArgs args(filePath, win);
        args.forceReuse = false; // open in a new tab
        StartLoadDocument(&args);
        ScheduleDeleteAndExecCommand();
        return;
    }
    logf("CommandPaletteWnd::ExecuteCurrentSelection: no match for selection '%s'\n", m->strings.At(idx));
    ReportIf(true);
    ScheduleDeleteAndExecCommand();
}

void CommandPaletteWnd::OnListDoubleClick() {
    ExecuteCurrentSelection();
}

void OnDestroy(Wnd::DestroyEvent*) {
    ScheduleDeleteAndExecCommand();
}

// almost like HwndPositionInCenterOf but y is near top of hwndRelative
static void PositionCommandPalette(HWND hwnd, HWND hwndRelative) {
    Rect rRelative = WindowRect(hwndRelative);
    Rect r = WindowRect(hwnd);
    int x = rRelative.x + (rRelative.dx / 2) - (r.dx / 2);
    int y = rRelative.y + (rRelative.dy / 2) - (r.dy / 2);
    r = {x, y, r.dx, r.dy};
    Rect r2 = ShiftRectToWorkArea(r, hwndRelative, true);
    r2.y = rRelative.y + 42;
    SetWindowPos(hwnd, nullptr, r2.x, r2.y, 0, 0, SWP_NOZORDER | SWP_NOSIZE);
}

void DrawMaybeHighlightedText(DrawMaybeHighlightedTextArgs& args) {
    HDC hdc = args.hdc;
    RECT rc = args.rc;
    const char* text = args.text;
    const StrVec& filterWords = args.filterWords;
    COLORREF colBg = args.colBg;
    bool isRtl = args.isRtl;
    uint drawFmt = args.drawFmt;

    int nWords = filterWords.Size();
    if (nWords == 0) {
        WCHAR* textW = ToWStrTemp(text);
        DrawTextW(hdc, textW, -1, &rc, drawFmt);
        return;
    }

    // find all match ranges in text
    int textLen = str::Leni(text);
    u8* hl = args.highlighted.EnsureCap((size_t)textLen);
    memset(hl, 0, textLen);
    for (int w = 0; w < nWords; w++) {
        const char* word = filterWords.At(w);
        int wordLen = str::Leni(word);
        if (wordLen == 0) {
            continue;
        }
        const char* p = text;
        while ((p = str::FindI(p, word)) != nullptr) {
            int off = (int)(p - text);
            for (int k = 0; k < wordLen && off + k < textLen; k++) {
                hl[off + k] = 1;
            }
            p += wordLen;
        }
    }

    // collect contiguous highlighted ranges (up to 16)
    struct ByteRange {
        int start;
        int end;
    };
    ByteRange byteRanges[16];
    int nRanges = 0;
    {
        int pos = 0;
        while (pos < textLen && nRanges < 16) {
            if (hl[pos]) {
                int start = pos;
                while (pos < textLen && hl[pos]) {
                    pos++;
                }
                byteRanges[nRanges++] = {start, pos};
            } else {
                pos++;
            }
        }
    }

    WCHAR* textW = ToWStrTemp(text);
    int textWLen = str::Leni(textW);

    // measure total string width for RTL positioning
    int strOriginX = rc.left;
    if (isRtl) {
        SIZE szTotal;
        GetTextExtentPoint32W(hdc, textW, textWLen, &szTotal);
        strOriginX = rc.right - szTotal.cx;
    }

    // compute pixel rectangles for each highlighted range
    RECT highlightRects[16];
    for (int i = 0; i < nRanges; i++) {
        WCHAR* prefixToStart = ToWStrTemp(text, (size_t)byteRanges[i].start);
        int wStart = str::Leni(prefixToStart);
        WCHAR* prefixToEnd = ToWStrTemp(text, (size_t)byteRanges[i].end);
        int wEnd = str::Leni(prefixToEnd);

        SIZE szStart, szEnd;
        GetTextExtentPoint32W(hdc, textW, wStart, &szStart);
        GetTextExtentPoint32W(hdc, textW, wEnd, &szEnd);

        highlightRects[i].top = rc.top;
        highlightRects[i].bottom = rc.bottom;
        highlightRects[i].left = strOriginX + szStart.cx;
        highlightRects[i].right = strOriginX + szEnd.cx;
    }

    // draw highlight background rectangles for matches
    {
        COLORREF highlightCol;
        if (IsCurrentThemeDefault()) {
            highlightCol = RGB(255, 255, 0); // yellow for default theme
        } else {
            highlightCol = AccentColor(colBg, 40);
        }
        HBRUSH hbrHighlight = CreateSolidBrush(highlightCol);
        for (int i = 0; i < nRanges; i++) {
            FillRect(hdc, &highlightRects[i], hbrHighlight);
        }
        DeleteObject(hbrHighlight);
    }

    // draw the whole string at once over the highlights
    DrawTextW(hdc, textW, -1, &rc, drawFmt);
}

void CommandPaletteWnd::DrawListBoxItem(ListBox::DrawItemEvent* ev) {
    ListBox* lb = ev->listBox;
    auto m = (ListBoxModelCP*)lb->model;
    if (ev->itemIndex < 0 || ev->itemIndex >= m->ItemsCount()) {
        return;
    }

    HDC hdc = ev->hdc;
    RECT rc = ev->itemRect;

    // set colors based on selection state
    COLORREF colBg = IsSpecialColor(lb->bgColor) ? GetSysColor(COLOR_WINDOW) : lb->bgColor;
    COLORREF colText = IsSpecialColor(lb->textColor) ? GetSysColor(COLOR_WINDOWTEXT) : lb->textColor;
    if (ev->selected) {
        if (false && IsCurrentThemeDefault()) {
            colBg = GetSysColor(COLOR_HIGHLIGHT);
            colText = GetSysColor(COLOR_HIGHLIGHTTEXT);
        } else {
            colBg = AccentColor(colBg, 30);
        }
    }

    // fill background
    SetBkColor(hdc, colBg);
    ExtTextOutW(hdc, 0, 0, ETO_OPAQUE, &rc, nullptr, 0, nullptr);

    // For RTL: remove LAYOUT_RTL from DC so we can position text manually.
    // The item rect spans full width so coordinates are the same mirrored or not.
    bool isRtl = HwndIsRtl(lb->hwnd);
    if (isRtl) {
        SetLayout(hdc, 0); // LAYOUT_LTR
    }

    // get item text and data
    const char* itemText = m->Item(ev->itemIndex);
    ItemDataCP* data = m->Data(ev->itemIndex);

    // get right-side string: accelerator for commands, directory for file history
    TempStr rightStr = nullptr;
    if (data->cmdId != 0) {
        // AppendAccelKeyToMenuStringTemp returns "\tCtrl + X" or original string if no accel
        TempStr withAccel = AppendAccelKeyToMenuStringTemp((TempStr) "", data->cmdId);
        if (withAccel && withAccel[0] == '\t') {
            rightStr = withAccel + 1; // skip the tab character
        }
    } else if (data->filePath) {
        rightStr = path::GetDirTemp(data->filePath);
    }

    // set text color and background mode
    SetTextColor(hdc, colText);
    SetBkMode(hdc, TRANSPARENT);

    // select font
    HFONT oldFont = nullptr;
    if (lb->font) {
        oldFont = SelectFont(hdc, lb->font);
    }

    // add some padding
    int padX = DpiScale(lb->hwnd, 4);
    rc.left += padX;
    rc.right -= padX;

    // draw command name on the left, highlighting matched words
    {
        DrawMaybeHighlightedTextArgs args(filterWords, highlighted);
        args.hdc = hdc;
        args.rc = rc;
        args.text = itemText;
        args.colBg = colBg;
        args.isRtl = isRtl;
        args.drawFmt = DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX;
        args.drawFmt |= isRtl ? (DT_RIGHT | DT_RTLREADING) : DT_LEFT;
        DrawMaybeHighlightedText(args);
    }

    // draw right-side text (shortcut or directory), truncated to avoid overlapping left text
    if (rightStr && rightStr[0]) {
        WCHAR* rightStrW = ToWStrTemp(rightStr);
        int gap = DpiScale(lb->hwnd, 8);

        // measure left text width
        WCHAR* itemTextW2 = ToWStrTemp(itemText);
        SIZE szLeft{};
        GetTextExtentPoint32W(hdc, itemTextW2, str::Leni(itemText), &szLeft);
        int leftEnd = rc.left + szLeft.cx + gap;

        RECT rcRight = rc;
        uint fmt = DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS;
        if (isRtl) {
            rcRight.right = rc.right - szLeft.cx - gap;
            fmt |= DT_LEFT | DT_RTLREADING;
        } else {
            rcRight.left = leftEnd;
            rcRight.right -= gap;
            fmt |= DT_RIGHT;
        }
        if (rcRight.left < rcRight.right) {
            COLORREF rightCol = AccentColor(colText, 80);
            SetTextColor(hdc, rightCol);
            DrawTextW(hdc, rightStrW, -1, &rcRight, fmt);
            SetTextColor(hdc, colText);
        }
    }

    if (oldFont) {
        SelectFont(hdc, oldFont);
    }
}

static Static* CreateStatic(HWND parent, HFONT font, const char* s) {
    Static::CreateArgs args;
    args.parent = parent;
    args.font = font;
    args.text = s;
    args.isRtl = IsUIRtl();
    auto c = new Static();
    auto wnd = c->Create(args);
    ReportIf(!wnd);
    return c;
}

bool CommandPaletteWnd::Create(MainWindow* win, const char* prefix, int smartTabAdvance) {
    if (str::Eq(prefix, kPalettePrefixTabs)) {
        smartTabMode = smartTabAdvance != 0;
    }
    CollectStrings(win);
    {
        CreateCustomArgs args;
        args.visible = false;
        args.style = WS_POPUPWINDOW;
        args.font = font;
        CreateCustom(args);
    }
    if (!hwnd) {
        return false;
    }

    auto colBg = ThemeWindowControlBackgroundColor();
    auto colTxt = ThemeWindowTextColor();
    SetColors(colTxt, colBg);

    auto vbox = new VBox();
    vbox->alignMain = MainAxisAlign::MainStart;
    vbox->alignCross = CrossAxisAlign::Stretch;

    {
        Edit::CreateArgs args;
        args.parent = hwnd;
        args.isMultiLine = false;
        args.withBorder = false;
        args.cueText = "enter search term";
        args.text = prefix;
        args.font = font;
        args.isRtl = IsUIRtl();
        auto c = new Edit();
        c->SetColors(colTxt, colBg);
        c->maxDx = 150;
        HWND ok = c->Create(args);
        ReportIf(!ok);
        c->onTextChanged = MkMethod0<CommandPaletteWnd, &CommandPaletteWnd::QueryChanged>(this);
        editQuery = c;
        vbox->AddChild(c);
    }

    if (!smartTabMode) {
        auto hbox = new HBox();
        hbox->alignMain = MainAxisAlign::MainCenter;
        hbox->alignCross = CrossAxisAlign::CrossCenter;
        auto pad = Insets{0, 8, 0, 8};
        {
            auto c = CreateStatic(hwnd, font, _TRA("# File History"));
            c->SetColors(colTxt, colBg);
            c->onClick = MkMethod0<CommandPaletteWnd, &CommandPaletteWnd::SwitchToFileHistory>(this);
            auto p = new Padding(c, pad);
            hbox->AddChild(p);
        }
        {
            auto c = CreateStatic(hwnd, font, _TRA("> Commands"));
            c->SetColors(colTxt, colBg);
            c->onClick = MkMethod0<CommandPaletteWnd, &CommandPaletteWnd::SwitchToCommands>(this);
            auto p = new Padding(c, pad);
            hbox->AddChild(p);
        }
        {
            auto c = CreateStatic(hwnd, font, _TRA("@ Tabs"));
            c->SetColors(colTxt, colBg);
            c->onClick = MkMethod0<CommandPaletteWnd, &CommandPaletteWnd::SwitchToTabs>(this);
            auto p = new Padding(c, pad);
            hbox->AddChild(p);
        }
        {
            auto c = CreateStatic(hwnd, font, _TRA(": Everything"));
            c->SetColors(colTxt, colBg);
            c->onClick = MkMethod0<CommandPaletteWnd, &CommandPaletteWnd::SwitchToEverything>(this);
            auto p = new Padding(c, pad);
            hbox->AddChild(p);
        }
        vbox->AddChild(hbox);
    }

    {
        ListBox::CreateArgs args;
        args.parent = hwnd;
        args.font = font;
        args.isRtl = IsUIRtl();
        auto c = new ListBox();
        c->onDoubleClick = MkMethod0<CommandPaletteWnd, &CommandPaletteWnd::OnListDoubleClick>(this);
        c->onDrawItem =
            MkMethod1<CommandPaletteWnd, ListBox::DrawItemEvent*, &CommandPaletteWnd::DrawListBoxItem>(this);
        c->idealSizeLines = 32;
        c->SetInsetsPt(4, 0);
        c->Create(args);
        c->SetColors(colTxt, colBg);
        c->onSelectionChanged = MkMethod0<CommandPaletteWnd, &CommandPaletteWnd::OnSelectionChange>(this);
        auto m = new ListBoxModelCP();
        FilterStringsForQuery(prefix, m->strings);
        c->SetModel(m);
        listBox = c;
        if (UseDarkModeLib()) {
            DarkMode::setDarkScrollBar(listBox->hwnd);
        }
        vbox->AddChild(c, 1);
    }

    {
        char const* strings[3];
        if (smartTabMode) {
            strings[0] = _TRA("Ctrl+Tab to navigate");
            strings[1] = _TRA("Release Ctrl to select");
            strings[2] = _TRA("Space for sticky mode");
        } else {
            strings[0] = _TRA("↑ ↓ to navigate");
            strings[1] = _TRA("Enter to select");
            strings[2] = _TRA("Esc to close");
        }
        auto hbox = new HBox();
        hbox->alignMain = MainAxisAlign::MainCenter;
        hbox->alignCross = CrossAxisAlign::CrossCenter;
        auto pad = Insets{0, 8, 0, 8};
        for (int i = 0; i < 3; i++) {
            auto c = CreateStatic(hwnd, font, strings[i]);
            c->SetColors(colTxt, colBg);
            auto p = new Padding(c, pad);
            hbox->AddChild(p);
        }
        vbox->AddChild(hbox);
    }

    auto padding = new Padding(vbox, DpiScaledInsets(hwnd, 4, 8));
    layout = padding;

    auto rc = ClientRect(win->hwndFrame);
    int dy = rc.dy - 72;
    if (dy < 480) {
        dy = 480;
    }
    int dx = rc.dx - 256;
    dx = limitValue(dx, 640, 1024);
    limitValue(dx, 640, 1024);
    LayoutAndSizeToContent(layout, dx, dy, hwnd);
    PositionCommandPalette(hwnd, win->hwndFrame);

    editQuery->SetCursorPositionAtEnd();
    if (smartTabMode) {
        int nItems = listBox->model->ItemsCount();
        int tabToSelect = (currTabIdx + nItems + smartTabAdvance) % nItems;
        SetCurrentSelection(this, tabToSelect);
    }

    SetIsVisible(true);
    HwndSetFocus(editQuery->hwnd);
    return true;
}

void RunCommandPalette(MainWindow* win, const char* prefix, int smartTabAdvance) {
    if (gCommandPaletteWnd) {
        // already open — just focus it
        HwndSetFocus(gCommandPaletteHwnd);
        return;
    }

    auto wnd = new CommandPaletteWnd();
    auto fn = MkFunc1Void<Wnd::DestroyEvent*>(OnDestroy);
    wnd->onDestroy = fn;
    wnd->font = GetAppBiggerFont();
    wnd->win = win;
    bool ok = wnd->Create(win, prefix, smartTabAdvance);
    ReportIf(!ok);
    gCommandPaletteWnd = wnd;
    gCommandPaletteHwnd = wnd->hwnd;
    gHwndToActivateOnClose = win->hwndFrame;
}

HWND CommandPaletteHwndForAccelerator(HWND hwnd) {
    if (!gCommandPaletteWnd) return nullptr;
    auto wnd = gCommandPaletteWnd;
    HWND wHwnd = wnd->hwnd;
    if (hwnd == wHwnd) return wHwnd;
    if (wnd->editQuery && wnd->editQuery->hwnd == hwnd) {
        return wHwnd;
    }
    if (!wnd->listBox) return nullptr;
    if (hwnd == wnd->listBox->hwnd) return wHwnd;
    return nullptr;
}
