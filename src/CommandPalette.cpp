/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"
#include "utils/WinUtil.h"
#include "utils/UITask.h"

#include "wingui/WinGui.h"
#include "wingui/Layout.h"
#include "wingui/Window.h"
#include "wingui/TreeModel.h"
#include "wingui/ListBoxCtrl.h"
#include "wingui/wingui2.h"

#include "DisplayMode.h"
#include "Controller.h"
#include "EngineBase.h"
#include "EngineAll.h"
#include "SettingsStructs.h"
#include "GlobalPrefs.h"
#include "DisplayModel.h"
#include "WindowInfo.h"
#include "TabInfo.h"
#include "SumatraConfig.h"
#include "Commands.h"
#include "CommandPalette.h"
#include "SumatraPDF.h"
#include "Tabs.h"

#include "utils/Log.h"

using namespace wg;

// clang-format off
static i32 gBlacklistCommandsFromPalette[] = {
    CmdNone,
    CmdOpenWithFirst,
    CmdOpenWithLast,
    CmdCommandPalette,
    CmdExitFullScreen, // ?

    // managing frequently list in home tab
    CmdOpenSelectedDocument,
    CmdPinSelectedDocument,
    CmdForgetSelectedDocument,

    CmdExpandAll,   // TODO: figure the proper context for it
    CmdCollapseAll, // TODO: figure the proper context for it
    CmdMoveFrameFocus,

    CmdFavoriteAdd,
    CmdFavoriteDel,
    CmdFavoriteToggle,

    CmdPresentationWhiteBackground,
    CmdPresentationBlackBackground,
};

// most commands is not valid when document is not opened
// it's shorter to list the remaining commands
static i32 gDocumentNotOpenWhitelist[] = {
    CmdOpenFile,
    CmdOpenFolder,
    CmdExit,
    CmdNewWindow,
    CmdContributeTranslation,
    CmdOptions,
    CmdAdvancedOptions,
    CmdChangeLanguage,
    CmdCheckUpdate,
    CmdHelpOpenManualInBrowser,
    CmdHelpVisitWebsite,
    CmdHelpAbout,
    CmdFavoriteShow,
    CmdFavoriteHide,
    CmdToggleFullscreen,
};
// clang-format on

static bool IsCmdInList(i32 cmdId, int n, i32* list) {
    for (int i = 0; i < n; i++) {
        if (list[i] == cmdId) {
            return true;
        }
    }
    return false;
}

struct CommandPaletteWnd : Wnd {
    ~CommandPaletteWnd() override {
        delete mainLayout;
    }
    WindowInfo* win = nullptr;
    Button* btn = nullptr;
    Edit* editQuery = nullptr;

    StrVec allStrings;
    ListBox* listBox = nullptr;

    LayoutBase* mainLayout = nullptr;

    void OnDestroy() override;
    bool PreTranslateMessage(MSG& msg) override;
    LRESULT WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) override;

    void ScheduleDelete();

    bool Create(WindowInfo* win);
    void QueryChanged();
    void ListDoubleClick();
    void ButtonClicked();

    void ExecuteSelection();
};

struct CommandPaletteBuildCtx {
    bool isDocLoaded = false;
    bool supportsAnnots = false;
};

static bool AllowCommand(const CommandPaletteBuildCtx& ctx, i32 cmdId) {
    int n = (int)dimof(gBlacklistCommandsFromPalette);
    if (IsCmdInList(cmdId, n, gBlacklistCommandsFromPalette)) {
        return false;
    }
    if (!ctx.isDocLoaded) {
        n = (int)dimof(gDocumentNotOpenWhitelist);
        if (!IsCmdInList(cmdId, n, gDocumentNotOpenWhitelist)) {
            return false;
        }
    }

    switch (cmdId) {
        case CmdDebugShowLinks:
        case CmdDebugAnnotations:
        case CmdDebugDownloadSymbols:
        case CmdDebugTestApp:
        case CmdDebugShowNotif:
        case CmdDebugCrashMe: {
            return gIsDebugBuild || gIsPreReleaseBuild;
        }
    }
    return true;
}

static void AddOpenedFiles(StrVec& strings, WindowInfo* win) {
    for (TabInfo* tab : win->tabs) {
        if (!tab->IsDocLoaded()) {
            continue;
        }
        auto path = tab->filePath.Get();
        auto s = ToUtf8Temp(path);
        // avoid adding the same file opened in multiple window
        strings.AppendIfNotExists(s);
    }
}

static TabInfo* FindOpenedFile(std::string_view sv) {
    for (WindowInfo* win : gWindows) {
        for (TabInfo* tab : win->tabs) {
            if (!tab->IsDocLoaded()) {
                continue;
            }
            auto path = tab->filePath.Get();
            auto s = ToUtf8Temp(path);
            if (str::Eq(s.Get(), sv.data())) {
                return tab;
            }
        }
    }
    return nullptr;
}

static void CollectPaletteStrings(StrVec& strings, WindowInfo* win) {
    CommandPaletteBuildCtx ctx;
    ctx.isDocLoaded = win->IsDocLoaded();
    DisplayModel* dm = win->AsFixed();
    if (dm) {
        auto engine = dm->GetEngine();
        ctx.supportsAnnots = EngineSupportsAnnotations(engine);
    }

    // append paths of opened files
    for (WindowInfo* w : gWindows) {
        AddOpenedFiles(strings, w);
    }
    // append paths of files from history, excluding
    // already appended (from opened files)
    for (FileState* fs : *gGlobalPrefs->fileStates) {
        strings.AppendIfNotExists(fs->filePath);
    }

    i32 cmdId = CmdFirst + 1;
    SeqStrings strs = gCommandDescriptions;
    while (strs) {
        if (AllowCommand(ctx, cmdId)) {
            CrashIf(str::Len(strs) == 0);
            strings.Append(strs);
        }
        seqstrings::Next(strs);
        cmdId++;
    }
}

// filter is one or more words separated by whitespace
// filter matches if all words match, ignoring the case
static bool FilterMatches(const char* str, const char* filter) {
    // empty filter matches all
    if (!filter || str::EmptyOrWhiteSpaceOnly(filter)) {
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
        if (!str::ContainsI(str, word.data())) {
            return false;
        }
    }
    return true;
}

static void FilterStrings(const StrVec& strs, const char* filter, StrVec& matchedOut) {
    matchedOut.Reset();
    int n = strs.Size();
    for (int i = 0; i < n; i++) {
        auto s = strs.at(i);
        if (FilterMatches(s.data(), filter)) {
            matchedOut.Append(s.data());
        }
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
            ExecuteSelection();
            return true;
        }

        if (msg.wParam == VK_UP) {
            dir = -1;
        } else if (msg.wParam == VK_DOWN) {
            dir = 1;
        }
        if (!dir) {
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
    }
    return false;
}

void CommandPaletteWnd::QueryChanged() {
    auto filter = editQuery->GetText();
    // for efficiency, reusing existing model
    auto m = (ListBoxModelStrings*)listBox->model;
    // auto m = new ListBoxModelStrings();
    FilterStrings(allStrings, filter.Get(), m->strings);
    listBox->SetModel(m);
    if (m->ItemsCount() > 0) {
        listBox->SetCurrentSelection(0);
    }
}

static CommandPaletteWnd* gCommandPaletteWnd = nullptr;

void SafeDeleteCommandPaletteWnd() {
    if (!gCommandPaletteWnd) {
        return;
    }

    auto tmp = gCommandPaletteWnd;
    gCommandPaletteWnd = nullptr;
    delete tmp;
}

void CommandPaletteWnd::ScheduleDelete() {
    uitask::Post(&SafeDeleteCommandPaletteWnd);
}

void CommandPaletteWnd::ExecuteSelection() {
    int sel = listBox->GetCurrentSelection();
    if (sel < 0) {
        return;
    }
    auto s = listBox->model->Item(sel);
    int cmdId = GetCommandIdByDesc(s.data());
    // logf("selection: %s, id: %d\n", s.data(), cmdId);
    if (cmdId >= 0) {
        HwndSendCommand(win->hwndFrame, cmdId);
        SetActiveWindow(win->hwndFrame);
        ScheduleDelete();
        return;
    }
    TabInfo* tab = FindOpenedFile(s);
    if (tab != nullptr) {
        if (tab->win->currentTab != tab) {
            SelectTabInWindow(tab);
        }
        SetActiveWindow(tab->win->hwndFrame);
        ScheduleDelete();
        return;
    }

    LoadArgs args(s.data(), win);
    args.forceReuse = false; // open in a new tab
    LoadDocument(args);
    ScheduleDelete();
    SetActiveWindow(win->hwndFrame);
}

void CommandPaletteWnd::ListDoubleClick() {
    ExecuteSelection();
}

void CommandPaletteWnd::ButtonClicked() {
    ScheduleDelete();
}

void CommandPaletteWnd::OnDestroy() {
    ScheduleDelete();
}

bool CommandPaletteWnd::Create(WindowInfo* win) {
    CollectPaletteStrings(allStrings, win);
    {
        CreateCustomArgs args;
        // args.title = L"Command Palette";
        args.visible = false;
        args.style = WS_POPUPWINDOW;
        CreateCustom(args);
    }
    if (!hwnd) {
        return false;
    }

    auto vbox = new VBox();
    vbox->alignMain = MainAxisAlign::MainStart;
    vbox->alignCross = CrossAxisAlign::Stretch;

    {
        auto c = new Edit();
        EditCreateArgs args;
        args.parent = hwnd;
        args.isMultiLine = false;
        args.withBorder = true;
        args.cueText = "a cue text";
        HWND ok = c->Create(args);
        CrashIf(!ok);
        c->maxDx = 150;
        c->onTextChanged = std::bind(&CommandPaletteWnd::QueryChanged, this);
        editQuery = c;
        vbox->AddChild(c);
    }

    {
        auto c = new ListBox();
        c->idealSizeLines = 32;
        c->SetInsetsPt(4, 0);
        auto wnd = c->Create(hwnd);
        CrashIf(!wnd);

        auto m = new ListBoxModelStrings();
        FilterStrings(allStrings, nullptr, m->strings);
        c->SetModel(m);
        c->onDoubleClick = std::bind(&CommandPaletteWnd::ListDoubleClick, this);
        listBox = c;
        vbox->AddChild(c, 1);
    }

    {
        auto c = new Button();
        auto wnd = c->Create(hwnd);
        CrashIf(!wnd);
        c->SetText(L"Close");
        c->onClicked = std::bind(&CommandPaletteWnd::ButtonClicked, this);
        btn = c;
        vbox->AddChild(c);
    }

    auto padding = new Padding(vbox, DpiScaledInsets(hwnd, 4, 8));
    mainLayout = padding;

    auto rc = ClientRect(win->hwndFrame);
    int dy = rc.dy - 72;
    if (dy < 480) {
        dy = 480;
    }
    if (dy > 640) {
        dy = 640;
    }
    LayoutAndSizeToContent(mainLayout, 520, dy, hwnd);
    HwndPositionInCenterOf(hwnd, win->hwndFrame);

    SetIsVisible(true);
    ::SetFocus(editQuery->hwnd);
    return true;
}

void RunCommandPallette(WindowInfo* win) {
    CrashIf(gCommandPaletteWnd);
    auto wnd = new CommandPaletteWnd();
    wnd->win = win;
    bool ok = wnd->Create(win);
    CrashIf(!ok);
    gCommandPaletteWnd = wnd;
}
