/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"
#include "utils/WinUtil.h"
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
#include "DisplayModel.h"
#include "MainWindow.h"
#include "Theme.h"
#include "WindowTab.h"
#include "SumatraConfig.h"
#include "Commands.h"
#include "CommandPalette.h"
#include "SumatraPDF.h"
#include "Tabs.h"
#include "ExternalViewers.h"
#include "Annotation.h"
#include "FileHistory.h"
#include "DarkModeSubclass.h"

#include "utils/Log.h"

constexpr const char* kInfoRegular = "↑ ↓ to navigate      Enter to select     Esc to close";
constexpr const char* kInfoSmartTab = "Ctrl+Tab to navigate         Release Ctrl to select    Space for sticky mode";

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
    CmdFavoriteToggle,
    CmdToggleFullscreen,
    CmdToggleMenuBar,
    CmdToggleToolbar,
    CmdShowLog,
    CmdClearHistory,
    CmdReopenLastClosedFile,
    CmdSelectNextTheme,
    CmdToggleFrequentlyRead,
    CmdDebugCrashMe,
    CmdDebugCorruptMemory,
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
    int ItemsCount() override {
        return strings.Size();
    }
    const char* Item(int i) override {
        return strings.At(i);
    }
    ItemDataCP* Data(int i) {
        return strings.AtData(i);
    }
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

    int currTabIdx = 0;
    bool smartTabMode = false;
    bool stickyMode = false;

    bool PreTranslateMessage(MSG&) override;
    LRESULT WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) override;

    void CollectStrings(MainWindow*);
    void FilterStringsForQuery(const char*, StrVecCP&);

    bool Create(MainWindow* win, const char* prefix, int smartTabAdvance);
    void QueryChanged();

    void ExecuteCurrentSelection();
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

    if (!ctx.annotationUnderCursor) {
        //        if ((cmdId == CmdSelectAnnotation) || (cmdId == CmdDeleteAnnotation)) {
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
    if ((cmdId == CmdToggleScrollbars) && !gGlobalPrefs->fixedPageUI.hideScrollbars) {
        return false;
    }
    return true;
}

static TempStr ConvertPathForDisplayTemp(const char* s) {
    TempStr name = path::GetBaseNameTemp(s);
    TempStr dir = path::GetDirTemp(s);
    TempStr res = str::JoinTemp(name, "  (", dir);
    res = str::JoinTemp(res, ")");
    return res;
}

static TempStr RemovePrefixFromString(const char* s) {
    return str::ReplaceTemp(s, "&", "");
}

void CommandPaletteWnd::CollectStrings(MainWindow* mainWin) {
    CommandPaletteBuildCtx ctx;
    ctx.isDocLoaded = mainWin->IsDocLoaded();
    WindowTab* currTab = mainWin->CurrentTab();
    ctx.filePath = currTab ? currTab->filePath : nullptr;
    ctx.hasSelection = ctx.isDocLoaded && currTab && mainWin->showSelection && currTab->selectionOnPage;
    ctx.canSendEmail = CanSendAsEmailAttachment(currTab);
    ctx.allowToggleMenuBar = !mainWin->tabsInTitlebar;

    int nTabs = mainWin->TabCount();
    int tabIdx = mainWin->GetTabIdx(currTab);
    ctx.canCloseTabsToRight = tabIdx < (nTabs - 1);
    ctx.canCloseTabsToLeft = false;
    int nFirstDocTab = 0;
    for (int i = 0; i < nTabs; i++) {
        WindowTab* t = mainWin->GetTab(i);
        if (t->IsAboutTab()) {
            ReportIf(i > 0);
            nFirstDocTab = 1;
            continue;
        }
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

    // append paths of opened files
    currTabIdx = 0;
    tabs.Reset();
    for (MainWindow* w : gWindows) {
        for (WindowTab* tab : w->Tabs()) {
            ItemDataCP data;
            data.tab = tab;
            if (tab->IsAboutTab()) {
                tabs.Append("Home", data);
                continue;
            }
            auto name = path::GetBaseNameTemp(tab->filePath);
            tabs.Append(name, data);
            if (tab == currTab) {
                currTabIdx = tabs.Size() - 1;
                logf("currTabIdx: %d\n", currTabIdx);
            }
        }
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
        if (AllowCommand(ctx, (i32)cmdId)) {
            ReportIf(str::Leni(name) == 0);
            ItemDataCP data;
            data.cmdId = (i32)cmdId;
            tempCommands.Append(name, data);
        }
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

static void SwitchToCommands(CommandPaletteWnd* wnd) {
    EditSetTextAndFocus(wnd->editQuery, kPalettePrefixCommands);
}

static void SwitchToTabs(CommandPaletteWnd* wnd) {
    EditSetTextAndFocus(wnd->editQuery, kPalettePrefixTabs);
}

static void SwitchToFileHistory(CommandPaletteWnd* wnd) {
    EditSetTextAndFocus(wnd->editQuery, kPalettePrefixFileHistory);
}

static CommandPaletteWnd* gCommandPaletteWnd = nullptr;
static HWND gHwndToActivateOnClose = nullptr;

void SafeDeleteCommandPaletteWnd() {
    if (!gCommandPaletteWnd) {
        return;
    }

    auto tmp = gCommandPaletteWnd;
    gCommandPaletteWnd = nullptr;
    delete tmp;
    if (gHwndToActivateOnClose) {
        SetActiveWindow(gHwndToActivateOnClose);
        gHwndToActivateOnClose = nullptr;
    }
}

static void ScheduleDelete() {
    if (!gCommandPaletteWnd) {
        return;
    }
    HighlightTab(gCommandPaletteWnd->win, nullptr);
    auto fn = MkFunc0Void(SafeDeleteCommandPaletteWnd);
    uitask::Post(fn, "SafeDeleteCommandPaletteWnd");
}

LRESULT CommandPaletteWnd::WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    switch (msg) {
        case WM_ACTIVATE:
            if (wparam == WA_INACTIVE) {
                ScheduleDelete();
                return 0;
            }
            break;
    }

    return WndProcDefault(hwnd, msg, wparam, lparam);
}

static void SelectionChange(CommandPaletteWnd* wnd) {
    int idx = wnd->listBox->GetCurrentSelection();
    // logf("Selection changed: %d\n", idx);
    if (!wnd->smartTabMode) {
        return;
    }
    auto m = (ListBoxModelCP*)wnd->listBox->model;
    ItemDataCP* data = m->strings.AtData(idx);
    HighlightTab(wnd->win, data->tab);
}

static void SetCurrentSelection(CommandPaletteWnd* wnd, int idx) {
    wnd->listBox->SetCurrentSelection(idx);
    SelectionChange(wnd);
}

bool CommandPaletteWnd::PreTranslateMessage(MSG& msg) {
    if (msg.message == WM_KEYDOWN) {
        int dir = 0;
        if (msg.wParam == VK_ESCAPE) {
            ScheduleDelete();
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

// filter is one or more words separated by whitespace
// filter matches if all words match, ignoring the case
static bool FilterMatches(const char* str, const char* filter) {
    // empty filter matches all
    if (str::IsEmptyOrWhiteSpace(filter)) {
        return true;
    }
    StrVec words;
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
    // all words must be present
    int nWords = words.Size();
    for (int i = 0; i < nWords; i++) {
        auto word = words.At(i);
        if (!str::ContainsI(str, word)) {
            return false;
        }
    }
    return true;
}

static void FilterStrings(StrVecCP& strs, const char* filter, StrVecCP& matchedOut) {
    int n = strs.Size();
    for (int i = 0; i < n; i++) {
        const char* s = strs.At(i);
        if (!FilterMatches(s, filter)) {
            continue;
        }
        matchedOut.AppendFrom(&strs, i);
    }
}

void CommandPaletteWnd::FilterStringsForQuery(const char* filter, StrVecCP& strings) {
    // for efficiency, reusing existing model
    strings.Reset();
    if (str::StartsWith(filter, kPalettePrefixAll)) {
        filter++;
        FilterStrings(tabs, filter, strings);
        FilterStrings(fileHistory, filter, strings);
        FilterStrings(commands, filter, strings);
        return;
    }

    if (str::StartsWith(filter, kPalettePrefixTabs)) {
        filter++;
        FilterStrings(tabs, filter, strings);
        return;
    }
    if (str::StartsWith(filter, kPalettePrefixFileHistory)) {
        filter++;
        FilterStrings(fileHistory, filter, strings);
        return;
    }
    if (str::StartsWith(filter, kPalettePrefixCommands)) {
        filter++;
    }
    FilterStrings(commands, filter, strings);
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

static void CommandPaletteQueryChanged(CommandPaletteWnd* self) {
    self->QueryChanged();
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
        HwndSendCommand(win->hwndFrame, cmdId);
        ScheduleDelete();
        return;
    }

    WindowTab* tab = data->tab;
    if (tab != nullptr) {
        MainWindow* mainWin = tab->win;
        if (mainWin->CurrentTab() != tab) {
            SelectTabInWindow(tab);
        }
        gHwndToActivateOnClose = mainWin->hwndFrame;
        ScheduleDelete();
        return;
    }
    auto filePath = data->filePath;
    if (filePath) {
        LoadArgs args(filePath, win);
        args.forceReuse = false; // open in a new tab
        StartLoadDocument(&args);
        ScheduleDelete();
        return;
    }
    logf("CommandPaletteWnd::ExecuteCurrentSelection: no match for selection '%s'\n", m->strings.At(idx));
    ReportIf(true);
    ScheduleDelete();
}

static void ListDoubleClick(CommandPaletteWnd* w) {
    w->ExecuteCurrentSelection();
}

void OnDestroy(Wnd::DestroyEvent*) {
    ScheduleDelete();
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

static Static* CreateStatic(HWND parent, HFONT font, const char* s) {
    Static::CreateArgs args;
    args.parent = parent;
    args.font = font;
    args.text = s;
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
        auto c = new Edit();
        c->SetColors(colTxt, colBg);
        c->maxDx = 150;
        HWND ok = c->Create(args);
        ReportIf(!ok);
        c->onTextChanged = MkFunc0(CommandPaletteQueryChanged, this);
        editQuery = c;
        vbox->AddChild(c);
    }

    if (!smartTabMode) {
        auto hbox = new HBox();
        hbox->alignMain = MainAxisAlign::MainCenter;
        hbox->alignCross = CrossAxisAlign::CrossCenter;
        auto pad = Insets{0, 8, 0, 8};
        {
            auto c = CreateStatic(hwnd, font, "# File History");
            c->SetColors(colTxt, colBg);
            c->onClick = MkFunc0(SwitchToFileHistory, this);
            auto p = new Padding(c, pad);
            hbox->AddChild(p);
        }
        {
            auto c = CreateStatic(hwnd, font, "> Commands");
            c->SetColors(colTxt, colBg);
            c->onClick = MkFunc0(SwitchToCommands, this);
            auto p = new Padding(c, pad);
            hbox->AddChild(p);
        }
        {
            auto c = CreateStatic(hwnd, font, "@ Tabs");
            c->SetColors(colTxt, colBg);
            c->onClick = MkFunc0(SwitchToTabs, this);
            auto p = new Padding(c, pad);
            hbox->AddChild(p);
        }
        {
            auto c = CreateStatic(hwnd, font, ": Everything");
            c->SetColors(colTxt, colBg);
            c->onClick = MkFunc0(SwitchToTabs, this);
            auto p = new Padding(c, pad);
            hbox->AddChild(p);
        }
        vbox->AddChild(hbox);
    }

    {
        ListBox::CreateArgs args;
        args.parent = hwnd;
        args.font = font;
        auto c = new ListBox();
        c->onDoubleClick = MkFunc0(ListDoubleClick, this);
        c->idealSizeLines = 32;
        c->SetInsetsPt(4, 0);
        c->Create(args);
        c->SetColors(colTxt, colBg);
        c->onSelectionChanged = MkFunc0(SelectionChange, this);
        auto m = new ListBoxModelCP();
        FilterStringsForQuery(prefix, m->strings);
        c->SetModel(m);
        listBox = c;
        if (gUseDarkModeLib) {
            DarkMode::setDarkScrollBar(listBox->hwnd);
        }
        vbox->AddChild(c, 1);
    }
    {
        auto c = CreateStatic(hwnd, this->font, smartTabMode ? kInfoSmartTab : kInfoRegular);
        c->SetColors(colTxt, colBg);
        staticInfo = c;
        vbox->AddChild(c);
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

void RunCommandPallette(MainWindow* win, const char* prefix, int smartTabAdvance) {
    ReportIf(gCommandPaletteWnd);

    auto wnd = new CommandPaletteWnd();
    auto fn = MkFunc1Void<Wnd::DestroyEvent*>(OnDestroy);
    wnd->onDestroy = fn;
    wnd->font = GetAppBiggerFont();
    wnd->win = win;
    bool ok = wnd->Create(win, prefix, smartTabAdvance);
    ReportIf(!ok);
    gCommandPaletteWnd = wnd;
    gHwndToActivateOnClose = win->hwndFrame;
}
