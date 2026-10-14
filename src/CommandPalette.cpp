/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"

#include "base/Pixmap.h"
#include "base/Win.h"
#include "base/File.h"
#include "base/GuessFileType.h"
#include "base/SettingsUtil.h"
#include "base/UITask.h"
#include "gui/Dpi.h"

#include "gui/UIModels.h"
#include "gui/Layout.h"
#include "gui/win/WinGui.h"
#include "gui/PlatformFont.h"
#include "gui/Gfx.h"
#include "gui/GuiColors.h"
#include "gui/VirtCtrl.h"

#include "Settings.h"
#include "AppSettings.h"
#include "DisplayMode.h"
#include "DocController.h"
#include "EngineBase.h"
#include "DisplayModel.h"
#include "MainWindow.h"
#include "Theme.h"
#include "WindowTab.h"
#include "SumatraConfig.h"
#include "Commands.h"
#include "SumatraPDF.h"
#include "Canvas.h"
#include "TableOfContents.h"
#include "SidebarPanel.h"
#include "Favorites.h"
#include "FileHistory.h"
#include "Menu.h"
#include "Translations.h"
#include "PagePosition.h"
#include "Installer.h"
#include "RegistryPreview.h"
#include "RegistrySearchFilter.h"
#include "Notifications.h"
#include "PdfDarkMode.h"
#include "EngineAll.h"
#include "CommandAvailability.h"
#include "Accelerators.h"
#include "FilterHighlightDraw.h"
#include "Annotation.h"
#include "AnnotSearch.h"
#include "AnnotEditToolbar.h"
#include "AnnotPlacement.h"
#include "PageThumbnails.h"
#include "CommandPalette.h"
#include "CommandPaletteCommon.h"

struct MainWindow;
struct WindowTab;
struct TocItem;
struct FileState;
struct Favorite;
struct Annotation;

enum class ThumbnailMode {
    Disabled,
    Enabled,
};

struct ListBoxModelCP : ListBoxModel {
    StrVecCP strings;

    ListBoxModelCP() = default;
    ~ListBoxModelCP() override = default;
    int ItemsCount() override { return len(strings); }
    Str Item(int i) override { return strings[i]; }
    ItemDataCP* Data(int i) { return strings.AtData(i); }
};

struct CommandPaletteWnd : WindowBase, PaletteSources {
    ~CommandPaletteWnd() override = default;
    MainWindow* win = nullptr;

    Edit* editQuery = nullptr;
    VirtListBox* listBox = nullptr;
    PageThumbnailsCtrl* thumbnailCtrl = nullptr;
    HBox* switchRow = nullptr;
    HBox* helpRow = nullptr;
    // "26 of 212" at the right end of the help row
    VirtText* countText = nullptr;
    int helpKind = -1;
    // the selected setting's doc comment, shown under the list in "= settings"
    ILayout* settingHelpBox = nullptr;
    VirtFixedLinesText* settingHelp = nullptr;

    Vec<u8> highlighted;

    int currTabIdx = 0;
    int currTocIdx = 0;
    bool tocMode = false;
    bool thumbnailMode = false;
    bool smartTabMode = false;
    bool stickyMode = false;

    void PreTranslate(WindowBase::PreTranslateEvent*);
    void OnKeyDown(KeyEvent*);
    void OnActivate(WindowBase::ActivateEvent*);
    void OnCommand(WindowBase::CommandEvent*);

    void CollectStrings(MainWindow*);
    void CollectTabsRegular(MainWindow*, WindowTab* currTab);
    void CollectTabsMru(MainWindow*, WindowTab* currTab);
    void CollectToc(MainWindow*);
    void CollectFavorites(MainWindow*);
    void CollectAnnotations(MainWindow*);
    void CollectSettings();
    void FillSwitchRow();

    bool Create(MainWindow* win, Str prefix, int smartTabAdvance);
    void QueryChanged();
    void UpdateHelpRow();
    void UpdateCount();
    void UpdateSettingHelp();
    void UpdateColors();

    void ExecuteCurrentSelection();
    bool AdvanceSelection(int dir);
    bool MoveSelection(int vkey);
    bool RemoveSelectedItem();
    void SwitchToPrefix(Str prefix);
    void SwitchToCommands();
    void SwitchToTabs();
    void SwitchToEverything();
    void SwitchToFileHistory();
    void SwitchToTOC();
    void SwitchToFavorites();
    void SwitchToSettings();
    void BeginEditSettingValue(Str path);
    void ReturnToSettings(Str selPath);
    bool IsEditingSettingValue();
    TempStr EditedSettingPathTemp();
    void SelectSetting(Str path);
    void SetThumbnailMode(ThumbnailMode mode);
    void OnSelectionChange();
    void OnListDoubleClick();
    void DrawListBoxItem(VirtListBox::DrawItemEvent* ev);
};

extern CommandPaletteWnd* gCommandPaletteWnd;

// the palette: a query line over a hairline, the mode switches, the rows on
// an inset with a rounded selection, and the key hints under a hairline
constexpr int kPaletteQueryFontPx = 15;
constexpr int kPaletteTextFontPx = 13;
constexpr int kPaletteMetaFontPx = 12;
constexpr int kPaletteHintFontPx = 11;
constexpr int kPaletteRowDy = 24;
constexpr int kPaletteInsetDx = 8;
constexpr int kPalettePadDx = 16;
constexpr int kPaletteQueryPadDy = 6;
constexpr int kPaletteSelectionRadius = 4;

static PlatformFont* PaletteFont(int px) {
    return GetUserGuiFont(StrL("Segoe UI"), DpiScale(px));
}

// a hairline across the palette
static void PaintPaletteRule(VirtCustom*, VirtPaintCtx* ctx) {
    ctx->gfx->FillRect(ctx->bounds, ThemeEdgeColor());
}

static VirtCustom* NewPaletteRule() {
    auto* rule = new VirtCustom();
    rule->idealSize = {1, 1};
    rule->onPaint = MkFunc1(PaintPaletteRule, rule);
    rule->SetFlag(vwfNoHitTest, true);
    return rule;
}

Str CommandPaletteSkipWS(Str s);
bool CommandPaletteUiRtl();
TempStr CommandPaletteShortcutTemp(i32 cmdId);
void CommandPaletteSetCurrentSelection(CommandPaletteWnd* wnd, int idx);
void ScheduleDeleteAndExecCommand(i32 cmdId = 0);
void SafeDeleteCommandPaletteWnd();
void PositionCommandPalette(HWND hwnd, HWND hwndRelative);

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
    CmdOpenFileNoHistory,
    CmdProperties,
    CmdNewWindow,
    CmdDuplicateInNewWindow,
    CmdPdfShowInfo,
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

// UI language (and the debug RTL toggle), not the palette hwnd: that window
// stays LTR so virtual-control coords and clicks are not mirrored (#5956).
bool CommandPaletteUiRtl() {
    return IsUIRtl();
}

CommandPaletteWnd* gCommandPaletteWnd = nullptr;
static int gPaletteOpDepth = 0;
static HWND gHwndToActivateOnClose = nullptr;
static WindowTab* gTabToSelectOnClose = nullptr;
static i32 gCmdIdToExecOnClose = 0;
// canvas mouse position when the palette was opened, as WM_COMMAND LPARAM
// (0 if the mouse was not over the canvas); the live cursor is over the palette
static LPARAM gCursorPosLParam = 0;
static FileState* gFavFsToGoToOnClose = nullptr;
static Favorite* gFavToGoToOnClose = nullptr;

// the palette's thumbnails went to a page: the palette's job is done
static void ClosePaletteAfterThumbnail() {
    ScheduleDeleteAndExecCommand();
}

void CommandPaletteWnd::SetThumbnailMode(ThumbnailMode mode) {
    bool enabled = mode == ThumbnailMode::Enabled;
    if (!thumbnailCtrl || thumbnailMode == enabled) {
        return;
    }
    if (enabled) {
        listBox->SetIsVisible(false);
        thumbnailCtrl->SetIsVisible(true);
        thumbnailMode = true;
        DoLayout(HwndClientRect(hwnd).Size());
        thumbnailCtrl->Activate();
    } else {
        thumbnailCtrl->Deactivate();
        thumbnailCtrl->SetIsVisible(false);
        listBox->SetIsVisible(true);
        thumbnailMode = false;
        DoLayout(HwndClientRect(hwnd).Size());
    }
    EditSetFocus(editQuery);
}

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
            LPARAM lp = CmdUsesCursorPos(cmdId) ? gCursorPosLParam : 0;
            HwndPostCommand(win->hwndFrame, cmdId, lp);
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

struct RemoveItemOp {
    CommandPaletteWnd* wnd = nullptr;
    WindowTab* tab = nullptr;
    Favorite* fav = nullptr;
    FileState* favFs = nullptr;
    Str filePath;
    int currSel = 0;
};

// CloseTab / DelFavorite can pump; this runs after the key handler returns.
static void ApplyRemoveItem(RemoveItemOp* op) {
    CommandPaletteWnd* wnd = op->wnd;
    WindowTab* tab = op->tab;
    Favorite* fav = op->fav;
    FileState* favFs = op->favFs;
    Str filePath = op->filePath;
    int currSel = op->currSel;

    AutoDelete opDel(op);
    defer {
        str::Free(filePath);
    };

    if (gCommandPaletteWnd != wnd) {
        return;
    }

    gPaletteOpDepth++;
    defer {
        gPaletteOpDepth--;
    };

    MainWindow* host = wnd->win;
    if (tab) {
        CloseTab(tab, false);
    } else if (fav && favFs) {
        DelFavorite(favFs, fav);
    } else if (len(filePath) > 0 && host) {
        ForgetFileFromFrequentlyRead(host, filePath);
    }

    if (gCommandPaletteWnd != wnd) {
        return;
    }
    if (!IsMainWindowValid(host)) {
        ScheduleDeleteAndExecCommand();
        return;
    }

    wnd->CollectStrings(host);
    if (gCommandPaletteWnd != wnd || !wnd->listBox || !wnd->listBox->model) {
        return;
    }
    auto* m = (ListBoxModelCP*)wnd->listBox->model;
    Str filter = CommandPaletteSkipWS(Str(wnd->editQuery->GetTextTemp()));
    FilterPaletteStrings(*wnd, filter, m->strings);
    wnd->listBox->SetModel(m);

    int n = m->ItemsCount();
    if (n == 0) {
        wnd->listBox->SetCurrentSelection(-1);
        return;
    }
    int sel = currSel;
    if (sel >= n) {
        sel = n - 1;
    }
    CommandPaletteSetCurrentSelection(wnd, sel);
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

void CommandPaletteWnd::SwitchToSettings() {
    SwitchToPrefix(Str(kPalettePrefixBoolSettings));
}

// Second stage of "= settings": the query becomes "=<path> = <value>" and the
// list offers values instead of settings. An enum's current value is left out
// so every choice shows; a free-form value is pre-filled so it can be edited.
void CommandPaletteWnd::BeginEditSettingValue(Str path) {
    TempStr value = {};
    if (!GetSettingsEnumValues(path)) {
        for (int i = 0; i < len(settings); i++) {
            if (str::Eq(settings[i], path)) {
                value = FormatSettingValueTemp(settings.AtData(i)->settingType, SettingRowPtr(settings.AtData(i)));
                break;
            }
        }
    }
    EditSetTextAndFocus(editQuery,
                        fmt("%s%s %s %s", Str(kPalettePrefixBoolSettings), path, Str(kPaletteSettingValueSep), value));
}

// Back to the setting-picking stage with selPath selected. Applying a value
// reloads gSettings, so the rows built from it (settings, file history,
// favorites) is rebuilt; selPath usually points into those rows, hence the copy.
void CommandPaletteWnd::ReturnToSettings(Str selPath) {
    TempStr path = str::DupTemp(selPath);
    CollectStrings(win);
    SwitchToSettings();
    SelectSetting(path);
}

// the full path of the setting named in the "=<path> = <value>" query
TempStr CommandPaletteWnd::EditedSettingPathTemp() {
    Str filter = CommandPaletteSkipWS(Str(editQuery->GetTextTemp()));
    str::TrimPrefix(filter, Str(kPalettePrefixBoolSettings));
    Str path, value, foundPath;
    if (!SplitSettingValueQuery(filter, path, value) || !FindPaletteSetting(*this, path, foundPath)) {
        return {};
    }
    return str::DupTemp(foundPath);
}

void CommandPaletteWnd::SelectSetting(Str path) {
    auto* m = (ListBoxModelCP*)listBox->model;
    for (int i = 0; i < m->ItemsCount(); i++) {
        if (str::Eq(m->strings[i], path)) {
            CommandPaletteSetCurrentSelection(this, i);
            return;
        }
    }
}

// true in the "=<path> = <value>" stage
bool CommandPaletteWnd::IsEditingSettingValue() {
    Str filter = CommandPaletteSkipWS(Str(editQuery->GetTextTemp()));
    if (!str::TrimPrefix(filter, Str(kPalettePrefixBoolSettings))) {
        return false;
    }
    Str path, value;
    return SplitSettingValueQuery(filter, path, value);
}

void CommandPaletteWnd::OnActivate(WindowBase::ActivateEvent* ev) {
    if (ev->state == WA_INACTIVE) {
        // -for-testing runs in the background, so this popup never stays
        // foreground. Closing on WA_INACTIVE would destroy it between
        // sequential WM_SETTEXT queries (image-only-palette-items).
        if (!gForTesting && gPaletteOpDepth == 0) {
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
    UpdateSettingHelp();
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

    // a setting goes back to its default
    if (IsSettingRow(d)) {
        TempStr path = RestorePaletteSettingDefault(d, m->strings[currSel]);
        if (len(path) == 0) {
            return false;
        }
        ReturnToSettings(path);
        return true;
    }

    WindowTab* tab = d->tab;
    Favorite* fav = d->fav;
    FileState* favFs = d->favFs;
    Str filePath = d->filePath;
    if (!tab && !(fav && favFs) && len(filePath) == 0) {
        return false;
    }

    auto* op = new RemoveItemOp;
    op->wnd = this;
    op->tab = tab;
    op->fav = fav;
    op->favFs = favFs;
    op->filePath = str::Dup(filePath);
    op->currSel = currSel;
    uitask::Post(MkFunc0<RemoveItemOp>(ApplyRemoveItem, op), "PaletteRemoveItem");
    return true;
}

void CommandPaletteWnd::OnKeyDown(KeyEvent* ev) {
    if (ev->vkey == VK_ESCAPE) {
        if (IsEditingSettingValue()) {
            ReturnToSettings(EditedSettingPathTemp());
            ev->didHandle = true;
            return;
        }
        ScheduleDeleteAndExecCommand();
        ev->didHandle = true;
        return;
    }

    if (ev->vkey == VK_RETURN) {
        if (thumbnailMode) {
            thumbnailCtrl->HandleKey(ev->vkey);
            ev->didHandle = true;
            return;
        }
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

    if (ev->vkey == VK_LEFT || ev->vkey == VK_RIGHT || ev->vkey == VK_UP || ev->vkey == VK_DOWN ||
        ev->vkey == VK_NEXT || ev->vkey == VK_PRIOR) {
        if (thumbnailMode) {
            thumbnailCtrl->HandleKey(ev->vkey);
            ev->didHandle = true;
            return;
        }
        ev->didHandle = MoveSelection(ev->vkey);
        return;
    }

    if (ev->vkey == VK_HOME || ev->vkey == VK_END) {
        if (thumbnailMode) {
            thumbnailCtrl->HandleKey(ev->vkey);
            ev->didHandle = true;
            return;
        }
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
        SwitchToSettings();
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

    if (IsSettingRow(data)) {
        Str itemText = m->strings[idx];
        if (len(data->settingPath) > 0) {
            // a value picked for a setting: the row text is the value
            SetSettingsValueFromStr(data->settingPath, itemText);
            ReturnToSettings(data->settingPath);
            return;
        }
        if (data->settingType == SettingType::Bool) {
            ToggleSettingsBool((bool*)SettingRowPtr(data));
            ReturnToSettings(itemText);
            return;
        }
        // anything else needs a value: stay open and ask for one
        BeginEditSettingValue(itemText);
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

    if (data->annot) {
        WindowTab* curr = win->CurrentTab();
        if (curr) {
            SetSelectedAnnotation(curr, data->annot);
        }
        gHwndToActivateOnClose = win->hwndFrame;
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

// one of the "# History" / "> Commands" switches in the top row; it
// carries the prefix it switches to so they can share one click handler
struct PaletteSwitchCtrl : VirtRichText {
    CommandPaletteWnd* wnd = nullptr;
    Str prefix;
};

static void OnPaletteSwitchClicked(VirtMouseEvent* ev) {
    auto* t = (PaletteSwitchCtrl*)ev->target;
    t->wnd->SwitchToPrefix(t->prefix);
}

// the mode switches: the one the query is in takes the text color, the others
// are muted
void CommandPaletteWnd::FillSwitchRow() {
    if (!switchRow) {
        return;
    }
    for (auto& c : switchRow->children) {
        delete c.layout;
    }
    VecReset(switchRow->children);

    Str filter{};
    if (editQuery) {
        filter = CommandPaletteSkipWS(Str(editQuery->GetTextTemp()));
    }
    Color colBg = ThemeWindowControlBackgroundColor();
    Color colTxt = ThemeWindowTextColor();
    Color colMuted = ThemeWindowTextDisabledColor();
    PaletteSwitch switches[kPaletteMaxSwitches];
    int n = PaletteSwitches(*this, win && win->AsFixed(), switches);
    for (int i = 0; i < n; i++) {
        PaletteSwitch& sw = switches[i];
        bool active = PaletteSwitchIsActive(filter, sw.prefix);
        auto* t = new PaletteSwitchCtrl();
        InitKeyHint(t, PaletteFont(kPaletteMetaFontPx), Str(sw.label.s, 1), Str(sw.label.s + 2, len(sw.label) - 2),
                    active ? colTxt : colMuted, colBg);
        t->wnd = this;
        t->prefix = sw.prefix;
        t->onClick = MkFunc1Void(OnPaletteSwitchClicked);
        switchRow->AddChild(t);
    }
    if (layout) {
        DoLayout();
    }
}

// a row of help items: virtual controls sitting in the palette's layout next
// to the real edit and list
static HBox* NewHelpRow(HBox* box, MainAxisAlign align) {
    box->alignMain = align;
    box->alignCross = CrossAxisAlign::CrossCenter;
    return box;
}

void CommandPaletteWnd::UpdateHelpRow() {
    if (!helpRow) {
        return;
    }
    Str filter{};
    if (editQuery) {
        filter = CommandPaletteSkipWS(Str(editQuery->GetTextTemp()));
    }
    int kind = PaletteHelpKind(filter, smartTabMode);
    if (helpRow->ChildrenCount() > 0 && kind == helpKind) {
        UpdateCount();
        return;
    }
    helpKind = kind;

    for (auto& c : helpRow->children) {
        delete c.layout;
    }
    VecReset(helpRow->children);
    countText = nullptr;

    Color colBg = ThemeWindowControlBackgroundColor();
    Color colMuted = ThemeWindowTextDisabledColor();
    PaletteHint hints[kPaletteMaxHints];
    int nHelp = PaletteHelpHints(kind, hints);
    for (int i = 0; i < nHelp; i++) {
        helpRow->AddChild(NewKeyHint(PaletteFont(kPaletteHintFontPx), hints[i].key, hints[i].desc, colMuted, colBg));
    }
    // the count at the right end
    helpRow->AddChild(new Spacer(0, 0), 1);
    countText = new VirtText({});
    countText->font = PaletteFont(kPaletteMetaFontPx);
    countText->SetColor(kColText, colMuted);
    helpRow->AddChild(countText);
    UpdateCount();

    if (settingHelpBox) {
        bool show = kind == kHelpSettings || kind == kHelpSettingValue;
        settingHelpBox->SetVisibility(show ? Visibility::Visible : Visibility::Collapse);
        // a collapsed box is not laid out, but its virtual child still paints
        settingHelp->SetIsVisible(show);
    }
    // the mode changed with the kind: the active switch moves
    FillSwitchRow();
    if (layout) {
        DoLayout();
    }
    UpdateSettingHelp();
}

// "26 of 212": the rows shown out of the mode's rows
void CommandPaletteWnd::UpdateCount() {
    if (!countText || !listBox) {
        return;
    }
    Str filter{};
    if (editQuery) {
        filter = CommandPaletteSkipWS(Str(editQuery->GetTextTemp()));
    }
    int shown = listBox->model ? listBox->model->ItemsCount() : 0;
    Str prev = countText->s;
    TempStr count = PaletteCountTemp(shown, PaletteSourceCount(*this, filter));
    if (str::Eq(prev, count)) {
        return;
    }
    countText->SetText(count);
    countText->sz = {};
    if (layout) {
        DoLayout();
    }
    HwndRepaintNow(hwnd);
}

// the doc comment of the selected setting, like the advanced settings dialog
void CommandPaletteWnd::UpdateSettingHelp() {
    if (!settingHelpBox || IsCollapsed(settingHelpBox)) {
        return;
    }
    Str comment;
    int idx = listBox->GetCurrentSelection();
    auto* m = (ListBoxModelCP*)listBox->model;
    if (idx >= 0 && idx < m->ItemsCount()) {
        comment = m->Data(idx)->settingComment;
    }
    settingHelp->SetText(comment);
}

// the theme changed while the palette is open: the colors were set at creation
void CommandPaletteWnd::UpdateColors() {
    auto colBg = ThemeWindowControlBackgroundColor();
    auto colTxt = ThemeWindowTextColor();
    // recolors the window and its native children (the query edit)
    UpdateTheme();

    listBox->SetColor(kColListText, colTxt);
    listBox->SetColor(kColListBg, colBg);
    if (thumbnailCtrl) {
        thumbnailCtrl->SetColor(kColListText, colTxt);
        thumbnailCtrl->SetColor(kColListBg, colBg);
    }
    settingHelp->SetColor(kColRichText, colTxt);
    settingHelp->SetColor(kColRichLink, colTxt);
    settingHelp->SetColor(kColRichBg, colBg);
    settingHelp->borderCol = ThemeEdgeColor();

    // the help rows bake the colors into their items, so rebuild them
    FillSwitchRow();
    helpKind = kHelpNone;
    UpdateHelpRow();
    RedrawWindow(hwnd, nullptr, nullptr, RDW_ERASE | RDW_INVALIDATE | RDW_ALLCHILDREN);
}

bool CommandPaletteWnd::Create(MainWindow* win, Str prefix, int smartTabAdvance) {
    if (str::Eq(prefix, Str(kPalettePrefixTabs))) {
        smartTabMode = smartTabAdvance != 0;
    }
    tocMode = str::Eq(prefix, Str(kPalettePrefixTOC));
    CollectStrings(win);
    {
        CreateCustomArgs args;
        args.visible = false;
        args.style = WS_POPUPWINDOW;
        // owned tool window: stays above the frame, off the taskbar (#6126)
        args.owner = win->hwndFrame;
        args.exStyle = WS_EX_TOOLWINDOW;
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

    // the query line: a bare edit over a hairline
    {
        Edit::CreateArgs args;
        args.parent = hwnd;
        args.isMultiLine = false;
        args.withBorder = false;
        args.cueText = StrL("Type a command");
        args.text = prefix;
        args.font = PaletteFont(kPaletteQueryFontPx);
        args.isRtl = IsUIRtl();
        auto* c = new Edit();
        c->SetColors(colTxt, colBg);
        c->maxDx = 150;
        HWND ok = c->Create(args);
        ReportIf(!ok);
        c->onTextChanged = MkMethod0<CommandPaletteWnd, &CommandPaletteWnd::QueryChanged>(this);
        editQuery = c;
        vbox->AddChild(new Padding(c, DpiScaledInsets(kPaletteQueryPadDy, kPalettePadDx)));
        vbox->AddChild(NewPaletteRule());
    }

    if (!smartTabMode) {
        auto* box = new HBox();
        box->rtl = CommandPaletteUiRtl();
        switchRow = NewHelpRow(box, MainAxisAlign::MainCenter);
        FillSwitchRow();
        vbox->AddChild(new Padding(switchRow, DpiScaledInsets(6, kPaletteInsetDx, 4, kPaletteInsetDx)));
    }

    {
        auto* overlay = new Overlay();
        auto* c = new VirtListBox();
        // the query edit owns the keyboard here (the palette turns the arrow
        // keys into selection changes itself), so the list doesn't take the
        // focus and doesn't show a focus ring
        c->SetFlag(vwfFocusable, false);
        c->dpi = GetDpi();
        c->font = PaletteFont(kPaletteTextFontPx);
        c->itemDy = DpiScale(kPaletteRowDy);
        c->SetColor(kColListText, colTxt);
        c->SetColor(kColListBg, colBg);
        c->padding = DpiScaledInsets(0, kPaletteInsetDx);
        c->onDoubleClick = MkMethod0<CommandPaletteWnd, &CommandPaletteWnd::OnListDoubleClick>(this);
        c->onDrawItem =
            MkMethod1<CommandPaletteWnd, VirtListBox::DrawItemEvent*, &CommandPaletteWnd::DrawListBoxItem>(this);
        c->onSelectionChanged = MkMethod0<CommandPaletteWnd, &CommandPaletteWnd::OnSelectionChange>(this);
        auto* m = new ListBoxModelCP();
        FilterPaletteStrings(*this, prefix, m->strings);
        c->SetModel(m);
        listBox = c;
        overlay->AddChild(c);

        if (win->AsFixed()) {
            thumbnailCtrl = new PageThumbnailsCtrl(win, font, GetDpi(), ThumbnailsHost::Palette);
            thumbnailCtrl->onPageOpened = MkFunc0Void(ClosePaletteAfterThumbnail);
            thumbnailCtrl->SetIsVisible(false);
            overlay->AddChild(thumbnailCtrl);
        }
        vbox->AddChild(overlay, 1);
    }

    {
        auto* c = new VirtFixedLinesText();
        c->font = font;
        c->SetColor(kColRichText, colTxt);
        c->SetColor(kColRichLink, colTxt);
        c->SetColor(kColRichBg, colBg);
        c->borderCol = ThemeEdgeColor();
        c->padding = DpiScaledInsets(4);
        settingHelp = c;
        auto* box = new Padding(c, DpiScaledInsets(4, 0));
        box->SetVisibility(Visibility::Collapse);
        settingHelpBox = box;
        vbox->AddChild(box);
    }

    // the footer: the key hints and the count under a hairline
    {
        vbox->AddChild(NewPaletteRule());
        auto* box = new HBox();
        box->rtl = CommandPaletteUiRtl();
        helpRow = NewHelpRow(box, MainAxisAlign::MainStart);
        vbox->AddChild(new Padding(helpRow, DpiScaledInsets(5, kPaletteInsetDx)));
        UpdateHelpRow();
    }

    layout = vbox;

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
    if (str::StartsWith(prefix, Str(kPalettePrefixThumbnails))) {
        SetThumbnailMode(ThumbnailMode::Enabled);
    }
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

    gCursorPosLParam = 0;
    if (HwndIsCursorOverWindow(win->hwndCanvas)) {
        Point pt = HwndGetCursorPos(win->hwndCanvas);
        gCursorPosLParam = MAKELPARAM(pt.x, pt.y);
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
    gCommandPaletteWnd = wnd;
    bool ok = wnd->Create(win, prefix, smartTabAdvance);
    ReportIf(!ok);
    gHwndToActivateOnClose = win->hwndFrame;
}

void CommandPaletteUpdateTheme() {
    CommandPaletteWnd* wnd = gCommandPaletteWnd;
    if (!wnd || !wnd->hwnd) {
        return;
    }
    wnd->UpdateColors();
}

void CommandPaletteOnAnnotationsChanged() {
    CommandPaletteWnd* wnd = gCommandPaletteWnd;
    if (!wnd || !wnd->hwnd || !wnd->win) {
        return;
    }
    if (!IsMainWindowValidAndNotClosing(wnd->win)) {
        return;
    }
    int nPrev = len(wnd->annotations);
    wnd->CollectAnnotations(wnd->win);
    if ((nPrev == 0) != (len(wnd->annotations) == 0)) {
        wnd->FillSwitchRow();
    }
    if (!wnd->editQuery || !wnd->listBox) {
        return;
    }
    Str filter = CommandPaletteSkipWS(Str(wnd->editQuery->GetTextTemp()));
    if (!str::StartsWith(filter, Str(kPalettePrefixAnnotations))) {
        return;
    }
    auto* m = (ListBoxModelCP*)wnd->listBox->model;
    if (!m) {
        return;
    }
    Annotation* keep = nullptr;
    int sel = wnd->listBox->GetCurrentSelection();
    if (sel >= 0 && sel < m->ItemsCount()) {
        ItemDataCP* data = m->Data(sel);
        keep = data ? data->annot : nullptr;
    }
    FilterPaletteStrings(*wnd, filter, m->strings);
    wnd->listBox->SetModel(m);
    wnd->UpdateHelpRow();
    int n = m->ItemsCount();
    if (n == 0) {
        return;
    }
    int idx = 0;
    if (keep) {
        for (int i = 0; i < n; i++) {
            ItemDataCP* data = m->Data(i);
            if (data && data->annot == keep) {
                idx = i;
                break;
            }
        }
    }
    CommandPaletteSetCurrentSelection(wnd, idx);
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
    int annotPage = 0;
    Str selText;
    Str selValue; // a setting row: its current value
    if (sel >= 0 && sel < n) {
        auto* model = (ListBoxModelCP*)wnd->listBox->model;
        selText = model->Item(sel);
        ItemDataCP* data = model->Data(sel);
        selectedCmdId = data ? data->cmdId : 0;
        if (data && data->annot) {
            annotPage = data->annot->pageNo;
        }
        if (data && IsSettingRow(data) && len(data->settingPath) == 0) {
            selValue = FormatSettingValueTemp(data->settingType, SettingRowPtr(data));
        }
    }
    int qStart = 0, qEnd = 0, qLen = 0;
    EditGetSelection(wnd->editQuery, qStart, qEnd);
    qLen = EditGetTextLen(wnd->editQuery);
    int thumbPage = wnd->thumbnailCtrl ? wnd->thumbnailCtrl->selectedPage : 0;
    int rendered = wnd->thumbnailCtrl ? wnd->thumbnailCtrl->RenderedCount() : 0;
    int nAnnots = len(wnd->annotations);
    EngineBase* engine = wnd->win && wnd->win->CurrentTab() ? wnd->win->CurrentTab()->GetEngine() : nullptr;
    int annotsDone = EngineMupdfAnnotsLoadDone(engine) ? 1 : 0;
    // the setting help paints when it is a visible top of the window's virtual tree
    bool helpShown = false;
    if (wnd->vroot && wnd->settingHelp && wnd->settingHelp->IsVisible()) {
        for (VirtCtrl* w : wnd->vroot->tops) {
            helpShown |= w == wnd->settingHelp;
        }
    }
    out.Append(
        fmt("OK sel=%d items=%d querySel=%d,%d queryLen=%d cmd=%d rtl=%d thumb=%d page=%d rendered=%d annots=%d "
            "annotPage=%d annotsDone=%d ",
            sel, n, qStart, qEnd, qLen, selectedCmdId, (int)CommandPaletteUiRtl(), (int)wnd->thumbnailMode, thumbPage,
            rendered, nAnnots, annotPage, annotsDone));
    out.Append(fmt("settingHelp=%d selValue=%s selText=%s\n", (int)helpShown, selValue, selText));
    return finish(0);
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
            newIsOn = !IsSidebarViewShown(win, SidebarView::Bookmarks);
        } break;
        case CmdToggleThumbnails: {
            isToggle = true;
            newIsOn = !IsSidebarViewShown(win, SidebarView::Thumbnails);
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
        case CmdToggleTrimEmptyMargins: {
            DisplayModel* dm = win->AsFixed();
            if (dm) {
                isToggle = true;
                newIsOn = !dm->GetTrimEmptyMargins();
            }
        } break;
        case CmdToggleFreePan: {
            DisplayModel* dm = win->AsFixed();
            if (dm) {
                isToggle = true;
                newIsOn = !dm->GetFreePan();
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
            return Tr("Unregister Windows Previewer");
        }
        return Tr("Register Windows Previewer");
    }

    if (cmdId == CmdToggleWindowsSearchFilter) {
        if (IsSearchFilterInstalled()) {
            return Tr("Unregister Windows Search Filter");
        }
        return Tr("Register Windows Search Filter");
    }

    if (cmdId == CmdAIChatWithClaudeCode) {
        return Tr("AI Claude chat with document");
    }
    if (cmdId == CmdAIChatWithGrokBuild) {
        return Tr("AI Grok chat with document");
    }
    if (cmdId == CmdAIChatWithOpenAICodex) {
        return Tr("AI Codex chat with document");
    }
    if (cmdId == CmdAIChatWithAntiGravity) {
        return Tr("AI Antigravity chat with document");
    }

    return s;
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

void CommandPaletteWnd::CollectAnnotations(MainWindow* mainWin) {
    annotations.Reset();
    WindowTab* tab = mainWin ? mainWin->CurrentTab() : nullptr;
    if (!tab) {
        return;
    }
    EngineBase* engine = tab->GetEngine();
    if (!EngineSupportsAnnotations(engine)) {
        return;
    }
    Vec<Annotation*> annots;
    EngineMupdfGetLoadedAnnotations(engine, annots);
    for (Annotation* a : annots) {
        if (!a) {
            continue;
        }
        ItemDataCP data;
        data.annot = a;
        annotations.Append(AnnotationReadableNameTemp(a->type), data);
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

void CommandPaletteWnd::CollectSettings() {
    settings.Reset();
    CollectSettingRows(settings);
    SortNoCase(&settings);

    // changed values first, then the rest; both groups stay alphabetical
    StrVecCP ordered;
    for (int pass = 0; pass < 2; pass++) {
        for (int i = 0; i < len(settings); i++) {
            bool changed = SettingDiffersFromDefault(settings.AtData(i));
            if (changed == (pass == 0)) {
                ordered.AppendFrom(&settings, i);
            }
        }
    }
    settings = ordered;
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
    WindowTab* tab = mainWin->CurrentTab();
    if (tab && EngineSupportsAnnotations(tab->GetEngine())) {
        StartLoadingAnnotationsForUi(tab);
    }
    CollectAnnotations(mainWin);
    CollectSettings();

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
    int cmdId = 0;
    for (int i = 0; i < gCommandsCount; i++) {
        Str name = gCommands[i].description;
        cmdId = gCommands[i].id;
        if (!AllowCommand(ctx, (i32)cmdId)) {
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
    }

    // the same command under another wording a user may search for
    for (int i = 0; i < gCommandAltDescsCount; i++) {
        Str name = gCommandAltDescs[i].description;
        cmdId = gCommandAltDescs[i].id;
        if (!AllowCommand(ctx, (i32)cmdId)) {
            continue;
        }
        ItemDataCP data;
        data.cmdId = (i32)cmdId;
        auto nameTranslated = trans::GetTranslation(name);
        auto nameUpdated = UpdateCommandNameTemp(mainWin, cmdId, nameTranslated);
        tempCommands.Append(nameUpdated, data);
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

void PositionCommandPalette(HWND hwnd, HWND hwndRelative) {
    Rect rRelative = HwndWindowRect(hwndRelative);
    Rect r = HwndWindowRect(hwnd);
    int x = rRelative.x + (rRelative.dx / 2) - (r.dx / 2);
    int y = rRelative.y + (rRelative.dy / 2) - (r.dy / 2);
    r = {x, y, r.dx, r.dy};
    Rect r2 = ShiftRectToWorkArea(r, hwndRelative, true);
    r2.y = rRelative.y + 42;
    SetWindowPos(hwnd, nullptr, r2.x, r2.y, 0, 0, SWP_NOZORDER | SWP_NOSIZE);
}

void CommandPaletteWnd::DrawListBoxItem(VirtListBox::DrawItemEvent* ev) {
    VirtListBox* lb = ev->listBox;
    auto* m = (ListBoxModelCP*)lb->model;
    if (ev->itemIndex < 0 || ev->itemIndex >= m->ItemsCount()) {
        return;
    }

    Gfx* gfx = ev->gfx;
    HWND hwndList = lb->GetHwnd();
    Rect rc = ev->itemRect;
    ItemDataCP* data = m->Data(ev->itemIndex);
    if (!data) {
        return;
    }
    if (data->annot) {
        DrawAnnotationListRow(gfx, lb->font, rc, data->annot, filterWords, highlighted, lb->GetColor(kColListBg),
                              lb->GetColor(kColListText), ev->selected);
        return;
    }

    Color colBg = lb->GetColor(kColListBg);
    Color colText = lb->GetColor(kColListText);
    if (IsSpecialColor(colBg)) {
        colBg = GetSysColor(COLOR_WINDOW);
    }
    if (IsSpecialColor(colText)) {
        colText = GetSysColor(COLOR_WINDOWTEXT);
    }

    gfx->FillRect(rc, colBg);
    if (ev->selected) {
        colBg = AccentColor(colBg, 28);
        gfx->FillRoundedRect(rc, DpiScale(kPaletteSelectionRadius), colBg);
    }

    // Gfx (Direct2D / GDI+) does not pick up WS_EX_LAYOUTRTL, so we lay the
    // row out right-to-left ourselves. Do not use HwndIsRtl(): the palette
    // hwnd stays LTR so mouse hit-testing and virtual-control coords match
    // (issue #5956). If the DC is still mirrored (nested RTL hwnd), turn it
    // off so Hebrew glyphs are not reversed.
    bool isRtl = CommandPaletteUiRtl();
    bool hwndRtl = HwndIsRtl(hwndList);
    bool prevMirrored = hwndRtl ? gfx->SetMirrored(false) : false;

    Str itemText = m->Item(ev->itemIndex);

    TempStr rightStr = PaletteRightColumnTemp(data);
    PlatformFont* rightFont = PaletteFont(kPaletteMetaFontPx);
    Color rightCol = ThemeWindowTextDisabledColor();
    // a setting's value reads as part of the row; one off its default is bold
    if (IsSettingRow(data) && len(data->settingPath) == 0) {
        rightCol = colText;
        if (SettingDiffersFromDefault(data)) {
            PlatformFont* bold = GetBoldPlatformFont(rightFont);
            if (bold) {
                rightFont = bold;
            }
        }
    }

    int padX = DpiScale(kPaletteInsetDx);
    rc.x += padX;
    rc.dx -= 2 * padX;

    if (data->indent > 0) {
        int indentW = data->indent * DpiScale(16);
        if (isRtl) {
            rc.dx -= indentW;
        } else {
            rc.x += indentW;
            rc.dx -= indentW;
        }
    }

    // reserve space on the right for rightStr (accel key, dir, or "p34") so it
    // is always visible; the item text gets the remaining space and is
    // ellipsized when too long. File history: the filename takes precedence
    // over a long directory (issue #6104).
    Rect rcText = rc;
    bool hasRight = rightStr && rightStr.s[0];
    int rightW = 0;
    if (hasRight) {
        int gap = DpiScale(8);
        rightW = gfx->MeasureText(rightStr, rightFont).dx;
        if (data->filePath) {
            int nameW = gfx->MeasureText(itemText, lb->font).dx;
            int minDir = DpiScale(80);
            int maxRight = rc.dx - nameW - gap;
            if (maxRight < minDir) {
                hasRight = false;
                rightW = 0;
            } else if (rightW > maxRight) {
                rightW = maxRight;
            }
        }
        if (hasRight) {
            if (isRtl) {
                rcText.x += rightW + gap;
                rcText.dx -= rightW + gap;
            } else {
                rcText.dx -= rightW + gap;
            }
        }
    }

    {
        u32 drawFmt = gfxTextEllipsis | gfxTextVCenter;
        drawFmt |= isRtl ? (gfxTextRight | gfxTextRtl) : gfxTextLeft;
        DrawMaybeHighlightedText(gfx, rcText, itemText, filterWords, highlighted, colBg, isRtl, false, drawFmt,
                                 lb->font, colText);
    }

    if (hasRight) {
        Rect rcRight = rc;
        u32 rightFmt = gfxTextVCenter;
        if (isRtl) {
            rcRight.dx = rightW;
            rightFmt |= gfxTextLeft;
        } else {
            rcRight.x += rcRight.dx - rightW;
            rcRight.dx = rightW;
            rightFmt |= gfxTextRight;
        }
        if (data->cmdId != 0) {
            DrawMaybeHighlightedText(gfx, rcRight, rightStr, filterWords, highlighted, colBg, false, false, rightFmt,
                                     rightFont, rightCol);
        } else {
            if (data->filePath) {
                rightFmt |= gfxTextPathEllipsis;
            }
            gfx->DrawText(rightStr, rcRight, rightFmt, rightFont, rightCol);
        }
    }

    if (hwndRtl) {
        gfx->SetMirrored(prevMirrored);
    }
}

void CommandPaletteWnd::QueryChanged() {
    Str filter = CommandPaletteSkipWS(Str(editQuery->GetTextTemp()));
    if (win->AsFixed() && str::StartsWith(filter, Str(kPalettePrefixThumbnails))) {
        SetThumbnailMode(ThumbnailMode::Enabled);
        UpdateHelpRow();
        return;
    }
    SetThumbnailMode(ThumbnailMode::Disabled);
    int currSelIdx = 0;
    auto* m = (ListBoxModelCP*)listBox->model;
    int nItemsPrev = m->ItemsCount();
    if (smartTabMode) {
        if (!stickyMode) {
            if (len(filter) > 1) {
                stickyMode = true;
                currSelIdx = listBox->GetCurrentSelection();
            }
        }
    }
    FilterPaletteStrings(*this, filter, m->strings);
    listBox->SetModel(m);
    UpdateHelpRow();
    UpdateCount();
    int nItems = m->ItemsCount();
    if (nItems == 0) {
        return;
    }
    if (stickyMode && nItemsPrev == nItems) {
        CommandPaletteSetCurrentSelection(this, currSelIdx);
        return;
    }
    if (str::StartsWith(filter, Str(kPalettePrefixTOC)) && len(filterWords) == 0) {
        int idx = (currTocIdx >= 0 && currTocIdx < nItems) ? currTocIdx : 0;
        CommandPaletteSetCurrentSelection(this, idx);
        return;
    }
    CommandPaletteSetCurrentSelection(this, 0);
}
