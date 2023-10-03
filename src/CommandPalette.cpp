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
#include "DocController.h"
#include "EngineBase.h"
#include "EngineAll.h"
#include "GlobalPrefs.h"
#include "DisplayModel.h"
#include "MainWindow.h"
#include "WindowTab.h"
#include "SumatraConfig.h"
#include "Commands.h"
#include "CommandPalette.h"
#include "SumatraPDF.h"
#include "Tabs.h"
#include "ExternalViewers.h"
#include "Annotation.h"
#include "FileHistory.h"

#include "utils/Log.h"

static HFONT gCommandPaletteFont = nullptr;

// clang-format off
// those commands never show up in command palette
static i32 gBlacklistCommandsFromPalette[] = {
    CmdNone,
    CmdOpenWithFirst,
    CmdOpenWithLast,
    CmdCommandPalette,
    CmdCommandPaletteNoFiles,
    CmdCommandPaletteOnlyTabs,

    // managing frequently list in home tab
    CmdOpenSelectedDocument,
    CmdPinSelectedDocument,
    CmdForgetSelectedDocument,

    CmdExpandAll,   // TODO: figure proper context for it
    CmdCollapseAll, // TODO: figure proper context for it
    CmdMoveFrameFocus,

    CmdFavoriteAdd,
    CmdFavoriteDel,

    CmdPresentationWhiteBackground,
    CmdPresentationBlackBackground,

    CmdSaveEmbeddedFile, // TODO: figure proper context for it
    CmdOpenEmbeddedPDF,
    CmdSaveAttachment,
    CmdOpenAttachment,

    CmdCreateShortcutToFile, // not sure I want this at all

};

// most commands are not valid when document is not opened
// it's shorter to list the remaining commands
static i32 gDocumentNotOpenWhitelist[] = {
    CmdOpenFile,
    CmdOpenFolder,
    CmdExit,
    CmdNewWindow,
    CmdContributeTranslation,
    CmdOptions,
    CmdAdvancedOptions,
    CmdAdvancedSettings,
    CmdChangeLanguage,
    CmdCheckUpdate,
    CmdHelpOpenManualInBrowser,
    CmdHelpVisitWebsite,
    CmdHelpAbout,
    CmdDebugDownloadSymbols,
    CmdDebugShowNotif,
    CmdDebugStartStressTest,
    CmdDebugTestApp,
    CmdFavoriteToggle,
    CmdToggleFullscreen,
    CmdToggleMenuBar,
    CmdToggleToolbar,
    CmdShowLog,
    CmdClearHistory,
    CmdReopenLastClosedFile,
    CmdSelectNextTheme,
#if defined(DEBUG)
    CmdDebugCrashMe,
    CmdDebugCorruptMemory,
#endif
};

// for those commands do not activate main window
// for example those that show dialogs (because the main window takes
// focus away from them)
static i32 gCommandsNoActivate[] = {
    CmdOptions,
    CmdChangeLanguage,
    CmdHelpAbout,
    CmdHelpOpenManualInBrowser,
    CmdHelpVisitWebsite,
    CmdOpenFile,
    CmdOpenFolder,
    // TOOD: probably more
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

static bool __cmdInList(i32 cmdId, i32* ids, int nIds) {
    for (int i = 0; i < nIds; i++) {
        if (ids[i] == cmdId) {
            return true;
        }
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

#define IsCmdInList(name) __cmdInList(cmdId, name, dimof(name))

struct CommandPaletteWnd : Wnd {
    ~CommandPaletteWnd() override = default;
    MainWindow* win = nullptr;

    Edit* editQuery = nullptr;
    StrVec filesInTabs;
    StrVec filesInHistory;
    StrVec commands;
    ListBox* listBox = nullptr;
    Static* staticHelp = nullptr;

    int currTabPos = 0;

    void OnDestroy() override;
    bool PreTranslateMessage(MSG&) override;
    LRESULT WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) override;

    void ScheduleDelete();
    void CollectStrings(MainWindow*);
    void FilterStringsForQuery(const char*, StrVec&);

    bool Create(MainWindow* win, const char* prefix);
    void QueryChanged();
    void ListDoubleClick();

    void ExecuteCurrentSelection();
};

struct CommandPaletteBuildCtx {
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
    bool canCloseTabsToRight = true;

    ~CommandPaletteBuildCtx();
};
CommandPaletteBuildCtx::~CommandPaletteBuildCtx() {
}

static bool IsOpenExternalViewerCommand(i32 cmdId) {
    return ((cmdId >= CmdOpenWithExternalFirst) && (cmdId <= CmdOpenWithExternalLast)) ||
           ((cmdId >= CmdOpenWithFirst) && (cmdId <= CmdOpenWithLast));
}

static bool AllowCommand(const CommandPaletteBuildCtx& ctx, i32 cmdId) {
    if (IsCmdInList(gBlacklistCommandsFromPalette)) {
        return false;
    }

    if (CmdCloseOtherTabs == cmdId) {
        return ctx.canCloseOtherTabs;
    }
    if (CmdCloseTabsToTheRight == cmdId) {
        return ctx.canCloseTabsToRight;
    }

    if (CmdReopenLastClosedFile == cmdId) {
        return RecentlyCloseDocumentsCount() > 0;
    }

    if (IsOpenExternalViewerCommand(cmdId)) {
        return HasExternalViewerForCmd(cmdId);
    }

    if (!ctx.isDocLoaded) {
        if (!IsCmdInList(gDocumentNotOpenWhitelist)) {
            return false;
        }
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

    if (!ctx.hasSelection && IsCmdInMenuList(cmdId, disableIfNoSelection)) {
        return false;
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
    if (!HasPermission(Perm::DiskAccess)) {
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

    switch (cmdId) {
        case CmdDebugShowLinks:
            return gIsDebugBuild || gIsPreReleaseBuild;
        case CmdDebugTestApp:
        case CmdDebugShowNotif:
        case CmdDebugStartStressTest:
        case CmdDebugCorruptMemory:
        case CmdDebugCrashMe: {
            return gIsDebugBuild;
        }
    }
    return true;
}

static char* ConvertPathForDisplayTemp(const char* s) {
    const char* name = path::GetBaseNameTemp(s);
    char* dir = path::GetDirTemp(s);
    char* res = str::JoinTemp(name, "  (", dir);
    res = str::JoinTemp(res, ")");
    return res;
}

void CommandPaletteWnd::CollectStrings(MainWindow* mainWin) {
    CommandPaletteBuildCtx ctx;
    ctx.isDocLoaded = mainWin->IsDocLoaded();
    WindowTab* tab = mainWin->CurrentTab();
    ctx.hasSelection = ctx.isDocLoaded && tab && mainWin->showSelection && tab->selectionOnPage;
    ctx.canSendEmail = CanSendAsEmailAttachment(tab);
    ctx.allowToggleMenuBar = !mainWin->tabsInTitlebar;

    int nTabs = mainWin->TabCount();
    int currTabIdx = mainWin->GetTabIdx(tab);
    ctx.canCloseTabsToRight = currTabIdx < (nTabs - 1);
    for (int i = 0; i < nTabs; i++) {
        WindowTab* t = mainWin->GetTab(i);
        if (t->IsAboutTab()) {
            continue;
        }
        if (t == tab) {
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

    if (!HasPermission(Perm::DiskAccess)) {
        ctx.supportsAnnots = false;
        ctx.hasUnsavedAnnotations = false;
    }

    ctx.hasToc = mainWin->ctrl && mainWin->ctrl->HasToc();

    // append paths of opened files
    int tabPos = 0;
    for (MainWindow* mw : gWindows) {
        if (mw == mainWin) {
            for (WindowTab* tab2 : mainWin->Tabs()) {
                if (tab2->IsAboutTab()) {
                    continue;
                }
                const char* name = tab2->filePath.Get();
                name = path::GetBaseNameTemp(name);
                filesInTabs.AppendIfNotExists(name);
                // find current tab index
                if (tab2 == mainWin->CurrentTab()) {
                    currTabPos = tabPos;
                }
                tabPos++;
            }
        }
    }

    // append paths of files from history, excluding
    // already appended (from opened files)
    for (FileState* fs : *gGlobalPrefs->fileStates) {
        char* s = fs->filePath;
        s = ConvertPathForDisplayTemp(s);
        filesInHistory.Append(s);
    }

    // we want the commands sorted
    StrVec tempStrings;
    int cmdId = (int)CmdFirst + 1;
    for (SeqStrings strs = gCommandDescriptions; strs; seqstrings::Next(strs, cmdId)) {
        if (AllowCommand(ctx, (i32)cmdId)) {
            CrashIf(str::Len(strs) == 0);
            tempStrings.Append(strs);
        }
    }
    tempStrings.SortNoCase();
    for (char* s : tempStrings) {
        commands.Append(s);
    }
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

        if (msg.wParam == VK_UP) {
            dir = -1;
        } else if (msg.wParam == VK_DOWN) {
            dir = 1;
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
        listBox->SetCurrentSelection(sel);
        return true;
    }
    return false;
}

// filter is one or more words separated by whitespace
// filter matches if all words match, ignoring the case
static bool FilterMatches(const char* str, const char* filter) {
    // empty filter matches all
    if (str::EmptyOrWhiteSpaceOnly(filter)) {
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
                words.AppendIfNotExists(wordStart);
                wasWs = true;
            }
            wordStart = s + 1;
        }
        s++;
    }
    if (str::Len(wordStart) > 0) {
        words.AppendIfNotExists(wordStart);
    }
    // all words must be present
    int nWords = words.Size();
    for (int i = 0; i < nWords; i++) {
        auto word = words.at(i);
        if (!str::ContainsI(str, word)) {
            return false;
        }
    }
    return true;
}

static void FilterStrings(StrVec& strs, const char* filter, StrVec& matchedOut) {
    for (char* s : strs) {
        if (!FilterMatches(s, filter)) {
            continue;
        }
        matchedOut.Append(s);
    }
}

const char* SkipWS(const char* s) {
    while (str::IsWs(*s)) {
        s++;
    }
    return s;
}

void CommandPaletteWnd::FilterStringsForQuery(const char* filter, StrVec& strings) {
    filter = SkipWS(filter);
    bool skipFiles = (filter[0] == '>');
    bool onlyTabs = (filter[0] == '@');
    if (skipFiles || onlyTabs) {
        ++filter;
        filter = SkipWS(filter);
    }
    // for efficiency, reusing existing model
    strings.Reset();
    if (onlyTabs) {
        FilterStrings(filesInTabs, filter, strings);
        return;
    }
    if (!skipFiles) {
        FilterStrings(filesInTabs, filter, strings);
        FilterStrings(filesInHistory, filter, strings);
    }
    FilterStrings(commands, filter, strings);
}

void CommandPaletteWnd::QueryChanged() {
    char* filter = editQuery->GetTextTemp();
    auto m = (ListBoxModelStrings*)listBox->model;
    FilterStringsForQuery(filter, m->strings);
    listBox->SetModel(m);
    if (m->ItemsCount() > 0) {
        if (str::Eq(filter, "@")) {
            listBox->SetCurrentSelection(currTabPos);
        } else {
            listBox->SetCurrentSelection(0);
        }
    }
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

void CommandPaletteWnd::ScheduleDelete() {
    uitask::Post(&SafeDeleteCommandPaletteWnd);
}

static WindowTab* FindOpenedFile(const char* s) {
    for (MainWindow* win : gWindows) {
        for (WindowTab* tab : win->Tabs()) {
            if (tab->IsAboutTab()) {
                continue;
            }
            const char* name = tab->filePath.Get();
            name = path::GetBaseNameTemp(name);
            if (str::Eq(name, s)) {
                return tab;
            }
        }
    }
    return nullptr;
}

void CommandPaletteWnd::ExecuteCurrentSelection() {
    int sel = listBox->GetCurrentSelection();
    if (sel < 0) {
        return;
    }
    auto m = (ListBoxModelStrings*)listBox->model;
    const char* s = m->Item(sel);
    int cmdId = GetCommandIdByDesc(s);
    if (cmdId >= 0) {
        bool noActivate = IsCmdInList(gCommandsNoActivate);
        if (noActivate) {
            gHwndToActivateOnClose = nullptr;
        }
        HwndSendCommand(win->hwndFrame, cmdId);
        ScheduleDelete();
        return;
    }

    bool isFromTab = filesInTabs.Contains(s);
    if (isFromTab) {
        WindowTab* tab = nullptr;
        // First find opened file in current window
        for (WindowTab* winTab : win->Tabs()) {
            if (winTab->IsAboutTab()) {
                continue;
            }
            const char* name = winTab->filePath.Get();
            name = path::GetBaseNameTemp(name);
            if (str::Eq(name, s)) {
                tab = winTab;
            }
        }

        // If not found, find it in other windows
        if (tab == nullptr) {
            tab = FindOpenedFile(s);
        }

        if (tab) {
            if (tab->win->CurrentTab() != tab) {
                SelectTabInWindow(tab);
            }
            gHwndToActivateOnClose = tab->win->hwndFrame;
            ScheduleDelete();
            return;
        }
    }

    for (FileState* fs : *gGlobalPrefs->fileStates) {
        char* path = fs->filePath;
        char* converted = ConvertPathForDisplayTemp(path);
        if (str::Eq(s, converted)) {
            LoadArgs args(path, win);
            args.forceReuse = false; // open in a new tab
            LoadDocument(&args, false, false);
            ScheduleDelete();
            return;
        }
    }
    logf("CommandPaletteWnd::ExecuteCurrentSelection: no match for selection '%s'\n", s);
    ReportIf(true);
    ScheduleDelete();
}

void CommandPaletteWnd::ListDoubleClick() {
    ExecuteCurrentSelection();
}

void CommandPaletteWnd::OnDestroy() {
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

bool CommandPaletteWnd::Create(MainWindow* win, const char* prefix) {
    CollectStrings(win);
    {
        CreateCustomArgs args;
        args.visible = false;
        args.style = WS_POPUPWINDOW;
        args.font = gCommandPaletteFont;
        CreateCustom(args);
    }
    if (!hwnd) {
        return false;
    }

    auto vbox = new VBox();
    vbox->alignMain = MainAxisAlign::MainStart;
    vbox->alignCross = CrossAxisAlign::Stretch;

    {
        EditCreateArgs args;
        args.parent = hwnd;
        args.isMultiLine = false;
        args.withBorder = true;
        args.cueText = "enter search term";
        args.font = gCommandPaletteFont;
        auto c = new Edit();
        c->maxDx = 150;
        c->onTextChanged = std::bind(&CommandPaletteWnd::QueryChanged, this);
        HWND ok = c->Create(args);
        CrashIf(!ok);
        editQuery = c;
        vbox->AddChild(c);
    }

    {
        ListBoxCreateArgs args;
        args.parent = hwnd;
        args.font = gCommandPaletteFont;
        auto c = new ListBox();
        c->onDoubleClick = std::bind(&CommandPaletteWnd::ListDoubleClick, this);
        c->idealSizeLines = 32;
        c->SetInsetsPt(4, 0);
        auto wnd = c->Create(args);
        CrashIf(!wnd);
        auto m = new ListBoxModelStrings();
        FilterStringsForQuery("", m->strings);
        c->SetModel(m);
        listBox = c;
        vbox->AddChild(c, 1);
    }

    {
        StaticCreateArgs args;
        args.parent = hwnd;
        args.font = gCommandPaletteFont;
        args.text = "↑ ↓ to navigate      Enter to select     Esc to close";

        auto c = new Static();
        auto wnd = c->Create(args);
        CrashIf(!wnd);
        staticHelp = c;
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

    if (!str::IsEmpty(prefix)) {
        // this will trigger filtering
        editQuery->SetText(prefix);
        editQuery->SetSelection(1, 1);
    }

    SetIsVisible(true);
    ::SetFocus(editQuery->hwnd);
    return true;
}

void RunCommandPallette(MainWindow* win, const char* prefix) {
    CrashIf(gCommandPaletteWnd);

    if (!gCommandPaletteFont) {
        // make min font size 16 (I get 12)
        int fontSize = GetSizeOfDefaultGuiFont();
        // make font 1.4x bigger than system font
        fontSize = (fontSize * 14) / 10;
        if (fontSize < 16) {
            fontSize = 16;
        }
        // TODO: leaking font
        gCommandPaletteFont = GetDefaultGuiFontOfSize(fontSize);
    }

    auto wnd = new CommandPaletteWnd();
    wnd->win = win;
    bool ok = wnd->Create(win, prefix);
    CrashIf(!ok);
    gCommandPaletteWnd = wnd;
    gHwndToActivateOnClose = win->hwndFrame;
}
