/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "utils/BaseUtil.h"
#include "utils/WinUtil.h"

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
#include "FileHistory.h"
#include "ProgressUpdateUI.h"
#include "Notifications.h"
#include "WindowInfo.h"
#include "TabInfo.h"
#include "resource.h"
#include "Tabs.h"
#include "Version.h"
#include "SumatraConfig.h"
#include "Commands.h"
#include "CommandPalette.h"

#include "utils/Log.h"

using namespace wg;

struct CommandPaletteWnd : Wnd {
    ~CommandPaletteWnd() override {
        delete mainLayout;
    }
    Button* btn = nullptr;
    Edit* editQuery = nullptr;

    VecStr allStrings;
    ListBox* listBox = nullptr;

    LayoutBase* mainLayout = nullptr;

    void OnDestroy() override;
    bool PreTranslateMessage(MSG& msg) override;

    bool Create(WindowInfo* win);
    void QueryChanged();
    void ListDoubleClick();
    void ButtonClicked();
};

static CommandPaletteWnd* gCommandPaletteWnd = nullptr;

static bool IsCmdInList(i32 cmdId, int n, i32* list) {
    for (int i = 0; i < n; i++) {
        if (list[i] == cmdId) {
            return true;
        }
    }
    return false;
}

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

static void AddOpenedFiles(VecStr& strings, WindowInfo* win) {
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

static void CollectPaletteStrings(VecStr& strings, WindowInfo* win) {
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
    const char* strs = gCommandDescriptions;
    while (strs) {
        if (AllowCommand(ctx, cmdId)) {
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
    VecStr words;
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

static void FilterStrings(const VecStr& allStrings, const char* filter, VecStr& matchedOut) {
    matchedOut.Reset();
    int n = allStrings.Size();
    for (int i = 0; i < n; i++) {
        auto s = allStrings.at(i);
        if (FilterMatches(s.data(), filter)) {
            matchedOut.Append(s);
        }
    }
}

bool CommandPaletteWnd::PreTranslateMessage(MSG& msg) {
    if (msg.message == WM_KEYDOWN) {
        int dir = 0;
        if (msg.wParam == VK_ESCAPE) {
            Close();
            return true;
        }

        if (msg.wParam == VK_RETURN) {
            ListDoubleClick();
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
    logf("query changed\n");
    auto filter = editQuery->GetText();
    // TODO: would be more efficient to use existing model
    // and tell listbox to re-render
    auto m = new ListBoxModelStrings();
    FilterStrings(allStrings, filter.Get(), m->strings);
    listBox->SetModel(m);
    if (m->ItemsCount() > 0) {
        listBox->SetCurrentSelection(0);
    }
}

void CommandPaletteWnd::ListDoubleClick() {
    int sel = listBox->GetCurrentSelection();
    if (sel >= 0) {
        logf("selected an item %d\n", sel);
        Close();
    }
}

void CommandPaletteWnd::ButtonClicked() {
    Close();
}

void CommandPaletteWnd::OnDestroy() {
    gCommandPaletteWnd = nullptr;
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
    auto wnd = gCommandPaletteWnd;
    if (wnd) {
        HWND hwnd = wnd->hwnd;
        BringWindowToTop(hwnd);
        return;
    }

    wnd = new CommandPaletteWnd();
    bool ok = wnd->Create(win);
    CrashIf(!ok);
}
