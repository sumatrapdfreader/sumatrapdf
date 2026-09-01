/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "base/Win.h"
#include "gui/Dpi.h"
#include "base/UITask.h"

#include "gui/UIModels.h"
#include "gui/Layout.h"
#include "gui/win/WinGui.h"
#include "gui/PlatformFont.h"
#include "gui/Gfx.h"
#include "gui/GuiColors.h"
#include "gui/VirtCtrl.h"

#include "Settings.h"
#include "AppSettings.h"
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
#include "Menu.h"
#include "Translations.h"
#include "CommandPaletteInternal.h"
#include "CommandPalette.h"

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

// UI language (and the debug RTL toggle), not the palette hwnd: that window
// stays LTR so virtual-control coords and clicks are not mirrored (#5956).
bool CommandPaletteUiRtl() {
    return IsUIRtl();
}

Str CommandPaletteSkipWS(Str s) {
    if (!s.s) {
        return {};
    }
    str::SkipWs(s);
    return s;
}

CommandPaletteWnd* gCommandPaletteWnd = nullptr;
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
    auto* tmp = gCommandPaletteWnd;
    gCommandPaletteWnd = nullptr;
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
        if (IsMainWindowValidAndNotClosing(tab->win) && tab->win->GetTabIdx(tab) >= 0) {
            SelectTabInWindow(tab);
        }
    }
    if (gCmdIdToExecOnClose != 0) {
        i32 cmdId = gCmdIdToExecOnClose;
        gCmdIdToExecOnClose = 0;
        if (IsMainWindowValidAndNotClosing(win)) {
            HwndPostCommand(win->hwndFrame, cmdId);
        }
    }
    if (gFavToGoToOnClose) {
        FileState* fs = gFavFsToGoToOnClose;
        Favorite* fav = gFavToGoToOnClose;
        gFavFsToGoToOnClose = nullptr;
        gFavToGoToOnClose = nullptr;
        if (IsMainWindowValidAndNotClosing(win)) {
            GoToFavorite(win, fs, fav);
        }
    }
}

void ScheduleDeleteAndExecCommand(i32 cmdId) {
    if (!gCommandPaletteWnd) {
        return;
    }
    gCmdIdToExecOnClose = cmdId;
    if (IsMainWindowValidAndNotClosing(gCommandPaletteWnd->win)) {
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
    EditSetCursorPosAtEnd(e);
    EditSetFocus(e);
}

void CommandPaletteWnd::SwitchToPrefix(Str prefix) {
    EditSetTextAndFocus(editQuery, prefix);
}

void CommandPaletteWnd::SwitchToCommands() {
    SwitchToPrefix(Str(kPalettePrefixCommands));
}

void CommandPaletteWnd::SwitchToTabs() {
    SwitchToPrefix(Str(kPalettePrefixTabs));
}

void CommandPaletteWnd::SwitchToEverything() {
    SwitchToPrefix(Str(kPalettePrefixEverything));
}

void CommandPaletteWnd::SwitchToFileHistory() {
    SwitchToPrefix(Str(kPalettePrefixFileHistory));
}

void CommandPaletteWnd::SwitchToTOC() {
    SwitchToPrefix(Str(kPalettePrefixTOC));
}

void CommandPaletteWnd::SwitchToFavorites() {
    SwitchToPrefix(Str(kPalettePrefixFavorites));
}

void CommandPaletteWnd::SwitchToBoolSettings() {
    SwitchToPrefix(Str(kPalettePrefixBoolSettings));
}

void CommandPaletteWnd::OnActivate(WindowBase::ActivateEvent* ev) {
    if (ev->state == WA_INACTIVE) {
        // -for-testing runs in the background, so this popup never stays
        // foreground. Closing on WA_INACTIVE would destroy it between
        // sequential WM_SETTEXT queries (image-only-palette-items).
        if (!gForTesting) {
            ScheduleDeleteAndExecCommand();
        }
        ev->didHandle = true;
    }
}

void CommandPaletteWnd::OnCommand(WindowBase::CommandEvent* ev) {
    int cmdId = LOWORD(ev->wparam);
    CustomCommand* cmd = FindCustomCommand(cmdId);
    if (cmd != nullptr) {
        cmdId = cmd->origId;
    }
    switch (cmdId) {
        case CmdNextTabSmart:
        case CmdPrevTabSmart: {
            int dir = cmdId == CmdNextTabSmart ? 1 : -1;
            AdvanceSelection(dir);
            ev->didHandle = true;
            return;
        }
        case CmdSelectAll:
            // Ctrl+A is an accelerator (CmdSelectAll) sent to this window;
            // select the query instead of the document (issue #5972).
            EditSelectAll(editQuery);
            ev->didHandle = true;
            return;
        case CmdCopySelection:
            // Ctrl+C is an accelerator (CmdCopySelection) sent to this window;
            // copy the query instead of the document (issue #5972).
            if (editQuery && editQuery->hwnd) {
                SendMessageW(editQuery->hwnd, WM_COPY, 0, 0);
            }
            ev->didHandle = true;
            return;
    }
}

void CommandPaletteWnd::OnSelectionChange() {
    int idx = listBox->GetCurrentSelection();
    if (!smartTabMode) {
        return;
    }
    auto* m = (ListBoxModelCP*)listBox->model;
    ItemDataCP* data = m->strings.AtData(idx);
    HighlightTab(win, data->tab);
}

bool CommandPaletteWnd::AdvanceSelection(int dir) {
    if (dir == 0) {
        return false;
    }
    int n = listBox->ItemsCount();
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

// Delete selected list item when it is removable: file-history entry, open tab,
// or favorite. Commands and TOC entries are not removable (caller should let
// the edit control handle Delete). After removal, refilter and keep selection
// on the same index (or the new last item if we deleted the last row).
// remove selected history / tab / favorite; keeps selection index stable
bool CommandPaletteWnd::RemoveSelectedItem() {
    if (!listBox || !listBox->model) {
        return false;
    }
    int currSel = listBox->GetCurrentSelection();
    if (currSel < 0) {
        return false;
    }
    auto* m = (ListBoxModelCP*)listBox->model;
    int n = m->ItemsCount();
    if (currSel >= n) {
        return false;
    }
    ItemDataCP* d = m->Data(currSel);
    if (!d) {
        return false;
    }

    // Commands and TOC: not removable from the palette
    if (d->cmdId != 0 || d->tocItem) {
        return false;
    }

    if (d->tab) {
        WindowTab* tab = d->tab;
        CloseTab(tab, false);
        if (!IsMainWindowValid(win)) {
            // closing the last tab closed the host window
            ScheduleDeleteAndExecCommand();
            return true;
        }
    } else if (d->fav && d->favFs) {
        // copy before DelFavorite frees the Favorite*
        Str path = d->favFs->filePath;
        int pageNo = d->fav->pageNo;
        DelFavorite(path, pageNo);
    } else if (d->filePath) {
        ForgetFileFromFrequentlyRead(win, d->filePath);
    } else {
        return false;
    }

    CollectStrings(win);
    Str filter = CommandPaletteSkipWS(Str(editQuery->GetTextTemp()));
    FilterStringsForQuery(filter, m->strings);
    listBox->SetModel(m);

    n = m->ItemsCount();
    if (n == 0) {
        listBox->SetCurrentSelection(-1);
        return true;
    }
    int sel = currSel;
    if (sel >= n) {
        sel = n - 1;
    }
    CommandPaletteSetCurrentSelection(this, sel);
    return true;
}

void CommandPaletteWnd::OnKeyDown(KeyEvent* ev) {
    if (ev->vkey == VK_ESCAPE) {
        ScheduleDeleteAndExecCommand();
        ev->didHandle = true;
        return;
    }

    if (ev->vkey == VK_RETURN) {
        ExecuteCurrentSelection();
        ev->didHandle = true;
        return;
    }

    if (ev->vkey == VK_DELETE) {
        if (RemoveSelectedItem()) {
            ev->didHandle = true;
        }
        // not a removable list item: let the edit control process Delete
        return;
    }

    if (ev->vkey == VK_TAB) {
        if (ev->isCtrl) {
            ev->didHandle = AdvanceSelection(ev->isShift ? -1 : 1);
        }
        return;
    }

    if (ev->vkey == VK_UP || ev->vkey == VK_DOWN || ev->vkey == VK_NEXT || ev->vkey == VK_PRIOR) {
        ev->didHandle = MoveSelection(ev->vkey);
        return;
    }

    if (ev->vkey == VK_HOME || ev->vkey == VK_END) {
        // Ctrl+Home / Ctrl+End: always first / last row
        if (ev->isCtrl) {
            ev->didHandle = MoveSelection(ev->vkey);
            return;
        }
        if (!editQuery || ev->hwnd != editQuery->hwnd) {
            ev->didHandle = MoveSelection(ev->vkey);
            return;
        }
        // Home / End: if the caret is already at the start/end of the query,
        // move the list; otherwise let the Edit control move the caret.
        int selStart = 0, selEnd = 0;
        EditGetSelection(editQuery, selStart, selEnd);
        int textLen = EditGetTextLen(editQuery);
        bool toEnd = (ev->vkey == VK_END);
        bool caretAtBound = (selStart == selEnd) && (toEnd ? selEnd == textLen : selStart == 0);
        if (caretAtBound) {
            ev->didHandle = MoveSelection(ev->vkey);
        }
    }
}

// Home / End / PageUp / PageDown move the list the same way as the Find
// window: Home/End go to the first/last row, PageUp/PageDown jump a page
// (no wrap). Up/Down still wrap via AdvanceSelection.
bool CommandPaletteWnd::MoveSelection(int vkey) {
    if (vkey == VK_UP) {
        return AdvanceSelection(-1);
    }
    if (vkey == VK_DOWN) {
        return AdvanceSelection(1);
    }
    if (!listBox) {
        return false;
    }
    int n = listBox->ItemsCount();
    if (n == 0) {
        return false;
    }
    int curr = listBox->GetCurrentSelection();
    int perPage = std::max(listBox->UsableDy() / listBox->GetItemHeight(), 1);
    int idx = curr;
    switch (vkey) {
        case VK_HOME:
            idx = 0;
            break;
        case VK_END:
            idx = n - 1;
            break;
        case VK_NEXT:
            if (curr < 0) {
                idx = 0;
            } else {
                idx = std::min(curr + perPage, n - 1);
            }
            break;
        case VK_PRIOR:
            if (curr < 0) {
                idx = n - 1;
            } else {
                idx = std::max(curr - perPage, 0);
            }
            break;
        default:
            return false;
    }
    if (idx == curr) {
        return true;
    }
    CommandPaletteSetCurrentSelection(this, idx);
    return true;
}

// smart-tab releases Ctrl after the palette is open; key-downs go via onKeyDown
void CommandPaletteWnd::PreTranslate(WindowBase::PreTranslateEvent* ev) {
    MSG& msg = *ev->msg;
    if (smartTabMode && msg.message == WM_KEYUP && msg.wParam == VK_CONTROL) {
        if (!stickyMode) {
            ExecuteCurrentSelection();
        }
        ev->didHandle = true;
    }
}

void CommandPaletteWnd::ExecuteCurrentSelection() {
    int idx = listBox->GetCurrentSelection();
    if (idx < 0) {
        return;
    }
    auto* m = (ListBoxModelCP*)listBox->model;
    ItemDataCP* data = m->strings.AtData(idx);
    i32 cmdId = data->cmdId;
    if (cmdId == CmdToggleBoolSetting) {
        SwitchToBoolSettings();
        return;
    }
    if (cmdId != 0) {
        bool noActivate = IsCmdInList(cmdId, gCommandsNoActivate);
        if (noActivate) {
            gHwndToActivateOnClose = nullptr;
        }
        ScheduleDeleteAndExecCommand(cmdId);
        return;
    }

    if (data->boolSetting) {
        ToggleSettingsBool(data->boolSetting);
        ScheduleDeleteAndExecCommand();
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

static void OnClose(WindowBase::CloseEvent* /*ev*/) {
    ScheduleDeleteAndExecCommand();
}

static void OnDestroy(WindowBase::DestroyEvent* /*ev*/) {
    ScheduleDeleteAndExecCommand();
}

// The help lines name keys, and a key reads better as a key-cap than as a word
// in the sentence. The strings are translated, so instead of putting markup in
// them - which would invalidate every existing translation - the key names are
// wrapped wherever they ended up in the sentence. Translators leave key names
// in English, so matching on them works in every language
static const char* kHelpKeys[] = {
    "Ctrl+Tab", "Ctrl", "Enter", "Space", "Del", "Esc", "PgUp", "PgDn", "\u2191", "\u2193",
};

// s[at..] is tok and isn't part of a longer word
static bool IsTokenAt(Str s, int at, Str tok) {
    int n = len(tok);
    if (at + n > len(s)) {
        return false;
    }
    if (!str::EqN(Str(s.s + at, n), tok, n)) {
        return false;
    }
    char before = (at > 0) ? s.s[at - 1] : ' ';
    char after = (at + n < len(s)) ? s.s[at + n] : ' ';
    bool okBefore = (before == ' ') || (before == '(');
    bool okAfter = (after == ' ') || (after == ',') || (after == ')') || (after == '.');
    return okBefore && okAfter;
}

static TempStr WithKbdMarkupTemp(Str s) {
    str::Builder out;
    int i = 0;
    while (i < len(s)) {
        Str match{};
        for (const char* k : kHelpKeys) {
            if (IsTokenAt(s, i, Str(k))) {
                match = Str(k);
                break;
            }
        }
        if (!match) {
            out.AppendChar(s.s[i]);
            i++;
            continue;
        }
        out.Append(StrL("(Kbd/"));
        out.Append(match);
        out.Append(StrL(")"));
        i += len(match);
    }
    return ToStrTemp(out);
}

// one of the "# File History" / "> Commands" switches in the top row; it
// carries the prefix it switches to so they can share one click handler
struct PaletteSwitch : VirtRichText {
    CommandPaletteWnd* wnd = nullptr;
    Str prefix;
};

static void OnPaletteSwitchClicked(VirtMouseEvent* ev) {
    auto* t = (PaletteSwitch*)ev->target;
    t->wnd->SwitchToPrefix(t->prefix);
}

// what the two help rows have in common
struct HelpStyle {
    HWND hwnd = nullptr;
    PlatformFont* font = nullptr;
    Color colTxt = kColorUnset;
    Color colBg = kColorUnset;
};

static void InitHelpText(const HelpStyle& st, VirtRichText* t, Str markup) {
    ParseTipInto(t, markup);
    t->font = st.font;
    // the help rows are not links, so they take the plain text color
    t->SetColor(kColRichText, st.colTxt);
    t->SetColor(kColRichLink, st.colTxt);
    t->SetColor(kColRichBg, st.colBg);
    int padX = DpiScale(8);
    t->padding = Insets{0, padX, 0, padX};
}

static VirtRichText* NewHelpText(const HelpStyle& st, Str markup) {
    auto* t = new VirtRichText();
    InitHelpText(st, t, markup);
    return t;
}

// a row of help items: virtual controls sitting in the palette's layout next
// to the real edit and list
static HBox* NewHelpRow(HBox* box) {
    box->alignMain = MainAxisAlign::MainCenter;
    box->alignCross = CrossAxisAlign::CrossCenter;
    return box;
}

bool CommandPaletteWnd::Create(MainWindow* win, Str prefix, int smartTabAdvance) {
    if (str::Eq(prefix, Str(kPalettePrefixTabs))) {
        smartTabMode = smartTabAdvance != 0;
    }
    tocMode = str::Eq(prefix, Str(kPalettePrefixTOC)) || str::Eq(prefix, Str(kPalettePrefixTOCLegacy));
    CollectStrings(win);
    {
        CreateCustomArgs args;
        args.visible = false;
        args.style = WS_POPUPWINDOW;
        args.font = GetFont();
        CreateCustom(args);
    }
    if (!hwnd) {
        return false;
    }

    auto colBg = ThemeWindowControlBackgroundColor();
    auto colTxt = ThemeWindowTextColor();
    SetColors(colTxt, colBg);

    auto* vbox = new VBox();
    vbox->alignMain = MainAxisAlign::MainStart;
    vbox->alignCross = CrossAxisAlign::Stretch;

    {
        Edit::CreateArgs args;
        args.parent = hwnd;
        args.isMultiLine = false;
        args.withBorder = true;
        args.cueText = StrL("enter search term");
        args.text = prefix;
        args.font = GetFont();
        args.isRtl = IsUIRtl();
        auto* c = new Edit();
        c->SetColors(colTxt, colBg);
        c->maxDx = 150;
        HWND ok = c->Create(args);
        ReportIf(!ok);
        c->onTextChanged = MkMethod0<CommandPaletteWnd, &CommandPaletteWnd::QueryChanged>(this);
        editQuery = c;
        vbox->AddChild(c);
    }

    if (!smartTabMode) {
        vbox->AddChild(new Spacer(0, DpiScale(4)));
        auto* box = new HBox();
        box->rtl = CommandPaletteUiRtl();
        // same smaller app font as the bottom hint row
        HelpStyle st{hwnd, GetAppFont(), colTxt, colBg};
        // in "# File History" and friends the first character is what you type
        // to get there, so it becomes a key-cap
        auto addSwitch = [this, box, &st](Str s, Str switchTo) {
            TempStr markup = str::JoinTemp(StrL("(Kbd/"), Str(s.s, 1), StrL(")"), Str(s.s + 1, len(s) - 1));
            auto* t = new PaletteSwitch();
            InitHelpText(st, t, markup);
            t->wnd = this;
            t->prefix = switchTo;
            t->onClick = MkFunc1Void(OnPaletteSwitchClicked);
            box->AddChild(t);
        };
        addSwitch(_TRA("# File History"), Str(kPalettePrefixFileHistory));
        addSwitch(_TRA("> Commands"), Str(kPalettePrefixCommands));
        addSwitch(_TRA("@ Tabs"), Str(kPalettePrefixTabs));
        addSwitch(_TRA(": Everything"), Str(kPalettePrefixEverything));
        if (len(toc) > 0) {
            addSwitch(_TRA("% TOC"), Str(kPalettePrefixTOC));
        }
        if (len(favorites) > 0) {
            addSwitch(_TRA("$ Favorites"), Str(kPalettePrefixFavorites));
        }
        addSwitch(_TRA("= Settings"), Str(kPalettePrefixBoolSettings));
        vbox->AddChild(NewHelpRow(box));
    }

    {
        auto* c = new VirtListBox();
        // the query edit owns the keyboard here (the palette turns the arrow
        // keys into selection changes itself), so the list doesn't take the
        // focus and doesn't show a focus ring
        c->SetFlag(vwfFocusable, false);
        c->dpi = GetDpi();
        c->font = font;
        c->SetColor(kColListText, colTxt);
        c->SetColor(kColListBg, colBg);
        c->padding = DpiScaledInsets(4, 0);
        c->onDoubleClick = MkMethod0<CommandPaletteWnd, &CommandPaletteWnd::OnListDoubleClick>(this);
        c->onDrawItem =
            MkMethod1<CommandPaletteWnd, VirtListBox::DrawItemEvent*, &CommandPaletteWnd::DrawListBoxItem>(this);
        c->onSelectionChanged = MkMethod0<CommandPaletteWnd, &CommandPaletteWnd::OnSelectionChange>(this);
        auto* m = new ListBoxModelCP();
        FilterStringsForQuery(prefix, m->strings);
        c->SetModel(m);
        listBox = c;
        vbox->AddChild(c, 1);
    }

    {
        Str strings[4];
        int nHelp = 0;
        bool boolSettingsMode = str::Eq(prefix, Str(kPalettePrefixBoolSettings));
        if (smartTabMode) {
            strings[nHelp++] = _TRA("Ctrl+Tab navigate");
            strings[nHelp++] = _TRA("Release Ctrl select");
            strings[nHelp++] = _TRA("Space for sticky mode");
            strings[nHelp++] = _TRA("Del remove item");
        } else if (boolSettingsMode) {
            strings[nHelp++] = _TRA("↑ ↓ PgUp PgDn navigate");
            strings[nHelp++] = _TRA("Enter change");
            strings[nHelp++] = _TRA("Esc close");
        } else {
            strings[nHelp++] = _TRA("↑ ↓ PgUp PgDn navigate");
            strings[nHelp++] = _TRA("Enter select");
            strings[nHelp++] = _TRA("Del remove item");
            strings[nHelp++] = _TRA("Esc close");
        }
        auto* box = new HBox();
        box->rtl = CommandPaletteUiRtl();
        // the hints are secondary information, so they use the regular (smaller)
        // app font, not the bigger font of the query / list
        HelpStyle st{hwnd, GetAppFont(), colTxt, colBg};
        for (int i = 0; i < nHelp; i++) {
            box->AddChild(NewHelpText(st, WithKbdMarkupTemp(strings[i])));
        }
        vbox->AddChild(NewHelpRow(box));
    }

    auto* padding = new Padding(vbox, DpiScaledInsets(4, 8));
    layout = padding;

    auto rc = HwndClientRect(win->hwndFrame);
    int dy = rc.dy - 72;
    dy = std::max(dy, 480);
    int dx = rc.dx - 256;
    dx = limitValue(dx, 640, 1024);
    if (smartTabMode) {
        // size the window to the number of tabs instead of using a fixed height
        int itemDy = listBox->GetItemHeight();
        int maxLines = 16;
        if (itemDy > 0) {
            maxLines = std::max((rc.dy - DpiScale(160)) / itemDy, 3);
        }
        listBox->idealSizeLines = std::min(listBox->model->ItemsCount(), maxLines);
        dy = 0;
    }
    LayoutAndSizeToContent(layout, dx, dy, hwnd);
    // the help rows are virtual controls: pick them up so we paint them and
    // they get their input
    DoLayout(HwndClientRect(hwnd).Size());
    PositionCommandPalette(hwnd, win->hwndFrame);

    EditSetCursorPosAtEnd(editQuery);
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
    EditSetFocus(editQuery);
    return true;
}

void RunCommandPalette(MainWindow* win, Str prefix, int smartTabAdvance) {
    if (gCommandPaletteWnd) {
        if (gCommandPaletteWnd->hwnd && IsWindow(gCommandPaletteWnd->hwnd)) {
            HwndSetFocus(gCommandPaletteWnd->hwnd);
            return;
        }
        ScheduleDeleteAndExecCommand();
    }

    auto* wnd = new CommandPaletteWnd();
    wnd->onClose = MkFunc1Void<WindowBase::CloseEvent*>(OnClose);
    wnd->onDestroy = MkFunc1Void<WindowBase::DestroyEvent*>(OnDestroy);
    wnd->onActivate = MkMethod1<CommandPaletteWnd, WindowBase::ActivateEvent*, &CommandPaletteWnd::OnActivate>(wnd);
    wnd->onCommand = MkMethod1<CommandPaletteWnd, WindowBase::CommandEvent*, &CommandPaletteWnd::OnCommand>(wnd);
    wnd->onKeyDown = MkMethod1<CommandPaletteWnd, KeyEvent*, &CommandPaletteWnd::OnKeyDown>(wnd);
    wnd->onPreTranslate =
        MkMethod1<CommandPaletteWnd, WindowBase::PreTranslateEvent*, &CommandPaletteWnd::PreTranslate>(wnd);
    wnd->SetFont(GetAppBiggerFont());
    wnd->win = win;
    bool ok = wnd->Create(win, prefix, smartTabAdvance);
    ReportIf(!ok);
    gCommandPaletteWnd = wnd;
    gHwndToActivateOnClose = win->hwndFrame;
}

HWND CommandPaletteHwndForAccelerator(HWND hwnd) {
    if (!gCommandPaletteWnd) {
        return nullptr;
    }
    auto* wnd = gCommandPaletteWnd;
    HWND wHwnd = wnd->hwnd;
    if (hwnd == wHwnd) {
        return wHwnd;
    }
    if (wnd->editQuery && wnd->editQuery->hwnd == hwnd) {
        return wHwnd;
    }
    return nullptr;
}

// Selected list row and query-edit selection, for -dbg-control tests.
TempStr CommandPaletteStateTemp(int* exitCodeOut) {
    str::Builder out;
    auto finish = [&](int code) -> TempStr {
        if (exitCodeOut) {
            *exitCodeOut = code;
        }
        return ToStrTemp(out);
    };
    if (!gCommandPaletteWnd || !gCommandPaletteWnd->hwnd) {
        out.Append(StrL("NOTREADY no-palette\n"));
        return finish(2);
    }
    auto* wnd = gCommandPaletteWnd;
    int sel = wnd->listBox ? wnd->listBox->GetCurrentSelection() : -1;
    int n = wnd->listBox ? wnd->listBox->ItemsCount() : 0;
    int selectedCmdId = 0;
    if (sel >= 0 && sel < n) {
        auto* model = (ListBoxModelCP*)wnd->listBox->model;
        ItemDataCP* data = model->Data(sel);
        selectedCmdId = data ? data->cmdId : 0;
    }
    int qStart = 0, qEnd = 0, qLen = 0;
    EditGetSelection(wnd->editQuery, qStart, qEnd);
    qLen = EditGetTextLen(wnd->editQuery);
    out.Append(fmt("OK sel=%d items=%d querySel=%d,%d queryLen=%d cmd=%d rtl=%d\n", sel, n, qStart, qEnd, qLen,
                   selectedCmdId, (int)CommandPaletteUiRtl()));
    return finish(0);
}
