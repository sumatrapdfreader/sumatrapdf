/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "base/Win.h"
#include "base/Dpi.h"
#include "base/UITask.h"

#include "wingui/UIModels.h"
#include "wingui/Layout.h"
#include "wingui/WinGui.h"

#include "Settings.h"
#include "AppSettings.h"
#include "GlobalPrefs.h"
#include "DocController.h"
#include "EngineBase.h"
#include "MainWindow.h"
#include "Theme.h"
#include "WindowTab.h"
#include "SumatraConfig.h"
#include "Commands.h"
#include "SumatraPDF.h"
#include "TableOfContents.h"
#include "Favorites.h"
#include "FileHistory.h"
#include "DarkModeSubclass.h"
#include "Translations.h"
#include "CommandPalette.h"
#include "CommandPaletteInternal.h"

// clang-format off
static i32 gCommandsNoActivate[] = {
    CmdOptions,
    CmdSetInverseSearch,
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
    CmdConvertImageToPdf,
    CmdTabGroupSave,
    CmdTabGroupRestore,
    0,
};
// clang-format on

static bool IsCmdInList(i32 cmdId, i32* ids) {
    while (*ids) {
        if (cmdId == *ids) {
            return true;
        }
        ids++;
    }
    return false;
}

Str CommandPaletteSkipWS(Str s) {
    if (!s.s) {
        return Str();
    }
    int off = 0;
    while (off < s.len && str::IsWs(s.s[off])) {
        off++;
    }
    return Str(s.s + off, s.len - off);
}

CommandPaletteWnd* gCommandPaletteWnd = nullptr;
HWND gCommandPaletteHwnd = nullptr;
static HWND gHwndToActivateOnClose = nullptr;
static WindowTab* gTabToSelectOnClose = nullptr;
static i32 gCmdIdToExecOnClose = 0;
static FileState* gFavFsToGoToOnClose = nullptr;
static Favorite* gFavToGoToOnClose = nullptr;

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
    if (gFavToGoToOnClose) {
        FileState* fs = gFavFsToGoToOnClose;
        Favorite* fav = gFavToGoToOnClose;
        gFavFsToGoToOnClose = nullptr;
        gFavToGoToOnClose = nullptr;
        if (win && IsMainWindowValid(win)) {
            GoToFavorite(win, fs, fav);
        }
    }
}

void ScheduleDeleteAndExecCommand(i32 cmdId) {
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

void CommandPaletteSetCurrentSelection(CommandPaletteWnd* wnd, int idx) {
    wnd->listBox->SetCurrentSelection(idx);
    wnd->OnSelectionChange();
}

static void EditSetTextAndFocus(Edit* e, Str s) {
    e->SetText(s);
    e->SetCursorPositionAtEnd();
    HwndSetFocus(e->hwnd);
}

void CommandPaletteWnd::SwitchToPrefix(Str prefix) {
    EditSetTextAndFocus(editQuery, prefix);
}

void CommandPaletteWnd::SwitchToCommands() {
    SwitchToPrefix(kPalettePrefixCommands);
}

void CommandPaletteWnd::SwitchToTabs() {
    SwitchToPrefix(kPalettePrefixTabs);
}

void CommandPaletteWnd::SwitchToEverything() {
    SwitchToPrefix(kPalettePrefixEverything);
}

void CommandPaletteWnd::SwitchToFileHistory() {
    SwitchToPrefix(kPalettePrefixFileHistory);
}

void CommandPaletteWnd::SwitchToTOC() {
    SwitchToPrefix(kPalettePrefixTOC);
}

void CommandPaletteWnd::SwitchToFavorites() {
    SwitchToPrefix(kPalettePrefixFavorites);
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
    if (!smartTabMode) {
        return;
    }
    auto m = (ListBoxModelCP*)listBox->model;
    ItemDataCP* data = m->strings.AtData(idx);
    HighlightTab(win, data->tab);
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
    CommandPaletteSetCurrentSelection(this, sel);
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
            Str filter = CommandPaletteSkipWS(Str(editQuery->GetTextTemp()));
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
            // not in file-history mode: let the edit control process Delete
            // normally (delete the character to the right of the cursor)
            return false;
        }

        if (msg.wParam == VK_UP) {
            dir = -1;
        } else if (msg.wParam == VK_DOWN) {
            dir = 1;
        }

        if (msg.wParam == VK_TAB) {
            if (IsCtrlPressed()) {
                dir = IsShiftPressed() ? -1 : 1;
            }
        }
        return AdvanceSelection(dir);
    }

    if (smartTabMode) {
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
        MainWindow* mainWin = FindMainWindowByTab(tab);
        if (!mainWin) {
            ScheduleDeleteAndExecCommand();
            return;
        }
        gTabToSelectOnClose = tab;
        gHwndToActivateOnClose = mainWin->hwndFrame;
        ScheduleDeleteAndExecCommand();
        return;
    }

    if (data->tocItem) {
        gHwndToActivateOnClose = win->hwndFrame;
        GoToTocItem(win, data->tocItem);
        ScheduleDeleteAndExecCommand();
        return;
    }

    if (data->fav) {
        gHwndToActivateOnClose = win->hwndFrame;
        gFavFsToGoToOnClose = data->favFs;
        gFavToGoToOnClose = data->fav;
        ScheduleDeleteAndExecCommand();
        return;
    }
    auto filePath = data->filePath;
    if (filePath) {
        LoadArgs args(filePath, win);
        args.activateExisting = true;
        args.activateExistingInWindow = true;
        args.forceReuse = false;
        StartLoadDocument(&args);
        ScheduleDeleteAndExecCommand();
        return;
    }
    logf("CommandPaletteWnd::ExecuteCurrentSelection: no match for selection '%s'\n", m->strings[idx]);
    ReportIf(true);
    ScheduleDeleteAndExecCommand();
}

void CommandPaletteWnd::OnListDoubleClick() {
    ExecuteCurrentSelection();
}

static void OnDestroy(Wnd::DestroyEvent*) {
    ScheduleDeleteAndExecCommand();
}

static Static* CreateStatic(HWND parent, HFONT font, Str s) {
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

bool CommandPaletteWnd::Create(MainWindow* win, Str prefix, int smartTabAdvance) {
    if (str::Eq(prefix, kPalettePrefixTabs)) {
        smartTabMode = smartTabAdvance != 0;
    }
    tocMode = str::Eq(prefix, kPalettePrefixTOC);
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
            hbox->AddChild(new Padding(c, pad));
        }
        {
            auto c = CreateStatic(hwnd, font, _TRA("> Commands"));
            c->SetColors(colTxt, colBg);
            c->onClick = MkMethod0<CommandPaletteWnd, &CommandPaletteWnd::SwitchToCommands>(this);
            hbox->AddChild(new Padding(c, pad));
        }
        {
            auto c = CreateStatic(hwnd, font, _TRA("@ Tabs"));
            c->SetColors(colTxt, colBg);
            c->onClick = MkMethod0<CommandPaletteWnd, &CommandPaletteWnd::SwitchToTabs>(this);
            hbox->AddChild(new Padding(c, pad));
        }
        {
            auto c = CreateStatic(hwnd, font, _TRA(": Everything"));
            c->SetColors(colTxt, colBg);
            c->onClick = MkMethod0<CommandPaletteWnd, &CommandPaletteWnd::SwitchToEverything>(this);
            hbox->AddChild(new Padding(c, pad));
        }
        if (len(toc) > 0) {
            auto c = CreateStatic(hwnd, font, _TRA("* TOC"));
            c->SetColors(colTxt, colBg);
            c->onClick = MkMethod0<CommandPaletteWnd, &CommandPaletteWnd::SwitchToTOC>(this);
            hbox->AddChild(new Padding(c, pad));
        }
        if (len(favorites) > 0) {
            auto c = CreateStatic(hwnd, font, _TRA("$ Favorites"));
            c->SetColors(colTxt, colBg);
            c->onClick = MkMethod0<CommandPaletteWnd, &CommandPaletteWnd::SwitchToFavorites>(this);
            hbox->AddChild(new Padding(c, pad));
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
        Str strings[3];
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
    LayoutAndSizeToContent(layout, dx, dy, hwnd);
    PositionCommandPalette(hwnd, win->hwndFrame);

    editQuery->SetCursorPositionAtEnd();
    if (smartTabMode) {
        int nItems = listBox->model->ItemsCount();
        int tabToSelect = (currTabIdx + nItems + smartTabAdvance) % nItems;
        CommandPaletteSetCurrentSelection(this, tabToSelect);
    } else if (tocMode) {
        int nItems = listBox->model->ItemsCount();
        if (currTocIdx >= 0 && currTocIdx < nItems) {
            CommandPaletteSetCurrentSelection(this, currTocIdx);
        }
    }

    SetIsVisible(true);
    HwndSetFocus(editQuery->hwnd);
    return true;
}

void RunCommandPalette(MainWindow* win, Str prefix, int smartTabAdvance) {
    if (gCommandPaletteWnd) {
        HwndSetFocus(gCommandPaletteHwnd);
        return;
    }

    auto wnd = new CommandPaletteWnd();
    auto fn = MkFunc1Void<Wnd::DestroyEvent*>(OnDestroy);
    wnd->onDestroy = fn;
    wnd->font = GetAppBiggerFont(win->hwndFrame);
    wnd->win = win;
    bool ok = wnd->Create(win, prefix, smartTabAdvance);
    ReportIf(!ok);
    gCommandPaletteWnd = wnd;
    gCommandPaletteHwnd = wnd->hwnd;
    gHwndToActivateOnClose = win->hwndFrame;
}

HWND CommandPaletteHwndForAccelerator(HWND hwnd) {
    if (!gCommandPaletteWnd) {
        return nullptr;
    }
    auto wnd = gCommandPaletteWnd;
    HWND wHwnd = wnd->hwnd;
    if (hwnd == wHwnd) {
        return wHwnd;
    }
    if (wnd->editQuery && wnd->editQuery->hwnd == hwnd) {
        return wHwnd;
    }
    if (!wnd->listBox) {
        return nullptr;
    }
    if (hwnd == wnd->listBox->hwnd) {
        return wHwnd;
    }
    return nullptr;
}