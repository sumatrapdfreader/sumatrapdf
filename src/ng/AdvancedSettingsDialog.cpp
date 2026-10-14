/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// A dialog for editing advanced settings, driven by the settings metadata
// (gSettingsInfo). Shows a filterable list of settings; clicking a bool
// toggles it, clicking an enum picks from its allowed values, clicking a
// string / color / number setting edits it in-place (Enter confirms, Esc
// cancels). Save writes the settings file and reloads it (so all derived state
// is re-computed); Cancel abandons the changes.
//
// ng: orig's is a resizable WS_POPUPWINDOW with a VirtListBox it custom-draws
// and in-place HWND editors placed over the value column. Here it is a window
// of its own where the platform has one, else a gpui Dialog in the frame; the
// list is an element per row, and the in-place editor is an Input (or a
// Select for an enum) rendered inside the row it belongs to.

#include "gui/GpuiBridge.h"
#include "VirtKeys.h"
#if OS_WIN
#include "base/Win.h"
#endif

#include "base/SettingsUtil.h"
#include "base/File.h"
#include "base/UITask.h"
#include "gui/UIModels.h"

#define INCLUDE_SETTINGSSTRUCTS_METADATA
#include "Settings.h"
#include "AppSettings.h"
#include "AppTools.h"
#include "DisplayMode.h"
#include "DocController.h"
#include "EngineBase.h"
#include "MainWindow.h"
#include "PdfDarkMode.h"
#include "Theme.h"
#include "SumatraConfig.h"
#include "SumatraPDF.h"
#include "Translations.h"
#include "FilterUtil.h"
#include "FilterHighlightDraw.h"
#include "gui/AppShell.h"
#include "gui/DialogWidgets.h"
#include "gui/ToolWindow.h"
#include "SumatraDialogs.h"
#include "RestrictedUseConfig.h"
#include "AdvancedSettingsDialogCommon.h"

#include "SumatraLog.h"

// the dialog is drawn like the command palette: a bare filter line, 24 px
// rows on an 8 px inset with a rounded selection, 16 px side padding
constexpr float kAdvQueryFontPx = 15;
constexpr float kAdvTextFontPx = 13;
constexpr float kAdvMetaFontPx = 12;
constexpr float kAdvRowDy = 24;
constexpr float kAdvQueryDy = 40;
constexpr float kAdvInsetDx = 8;
constexpr float kAdvPadDx = 16;
constexpr float kAdvGapDx = 8;
constexpr float kAdvHintGapDx = 18;
constexpr float kAdvListPadDy = 4;
constexpr float kAdvHelpDy = 104;
constexpr float kAdvHelpPadDy = 8;
constexpr float kAdvFootDy = 44;
constexpr float kAdvRadius = 4;
constexpr int kAdvSelShift = 28;
constexpr int kAdvFaintShift = 25;
constexpr float kAdvHeaderPadB = 4;
constexpr float kAdvSwatchDx = 12;
constexpr float kAdvDotDx = 6;
constexpr float kAdvValueDx = 240;
constexpr float kAdvListDy = 360; // the list in a frame dialog
constexpr int kAdvOverscanRows = 2;

struct AdvancedSettingsDlg {
    MainWindow* win = nullptr;
    bool visible = false;
    Vec<SettingItem*> items;
    Vec<SettingSection*> sections;
    Vec<SettingRow> rows;
    StrVec filterWords;
    int sel = -1; // index into rows
    float scrollY = 0;
    int editIdx = -1; // index into items of the setting being edited
    bool editIsEnum = false;
    gpui::InputState* editFilter = nullptr;
    gpui::InputState* editValue = nullptr;
    DialogSelect ddValue;
    bool wantFocus = false;
    bool wantEditFocus = false;
    // orig's window, where the platform can have one (gui/ToolWindow.h);
    // null: a dialog in the frame
    ToolWindow* tw = nullptr;
    // the list as laid out last frame in that window: it takes what is left
    gpui::Bounds listBounds{};
};

static AdvancedSettingsDlg gAdv;

// orig's gAdvSettingsLastClientDx / Dy: the size the window was dragged to
// earlier this session (dips)
static Size gAdvLastClient;
constexpr int kAdvSettingsMinClientDy = 320;
// what is in the window besides the list, until a frame has measured it
constexpr float kAdvWinChromeGuessDy = kAdvQueryDy + kAdvHelpDy + kAdvFootDy + 2 * kAdvListPadDy + 3;

static void AdvOpenToolWindow(MainWindow* win);

// orig's AdvSettingsItemColumns: the value column is about 45% of the row
constexpr float kAdvColGap = 10;
constexpr float kAdvMinValDx = 100;
constexpr float kAdvMinNameDx = 72;

// the gpui window the dialog is drawn in
static gp::Window* AdvHostWindow() {
    if (gAdv.tw) {
        return ToolWindowGpui(gAdv.tw);
    }
    return gAdv.win ? gAdv.win->gpuiWin : nullptr;
}

struct AdvancedSettingsView {
    static void OnSave(AdvancedSettingsView* self, gp::Ctx* cx, const gp::ClickEvent*);
    static void OnCancel(AdvancedSettingsView* self, gp::Ctx* cx, const gp::ClickEvent*);
    static void OnOpenFile(AdvancedSettingsView* self, gp::Ctx* cx, const gp::ClickEvent*);
    static void OnHelp(AdvancedSettingsView* self, gp::Ctx* cx, const gp::ClickEvent*);
    static void OnFilter(AdvancedSettingsView* self, gp::Ctx* cx, const gp::InputEvent* ev);
    static void OnRowClick(AdvancedSettingsView* self, gp::Ctx* cx, const gp::ClickEvent* ev, int64_t idx);
    static void OnEditInput(AdvancedSettingsView* self, gp::Ctx* cx, const gp::InputEvent* ev);
    static void OnScroll(AdvancedSettingsView* self, gp::Ctx* cx, const gp::ScrollEvent* ev);
};

static gp::Entity<AdvancedSettingsView> gAdvView;

static int CountChangedSettings() {
    int n = 0;
    for (SettingItem* item : gAdv.items) {
        if (item->changed) {
            n++;
        }
    }
    return n;
}

// the setting of a row; null for a header or no row
static SettingItem* RowItem(int row) {
    if (row < 0 || row >= len(gAdv.rows) || gAdv.rows[row].itemIdx < 0) {
        return nullptr;
    }
    return gAdv.items[gAdv.rows[row].itemIdx];
}

// the dialog in the frame; a window of its own is not the frame's business
bool IsAdvancedSettingsDialogVisible() {
    return gAdv.visible && !gAdv.tw;
}

// orig's CancelEditValue: the in-place editor goes away
static void CancelEditValue() {
    gAdv.editIdx = -1;
    gAdv.editIsEnum = false;
    gAdv.ddValue.Free();
}

// orig's CommitEditValue
static void CommitEditValue() {
    if (gAdv.editIdx < 0 || gAdv.editIsEnum) {
        CancelEditValue();
        return;
    }
    SettingItem* item = gAdv.items[gAdv.editIdx];
    TempStr s = str::DupTemp(FromGpui(gp::InputValue(gAdv.editValue)));
    switch (item->type) {
        case SettingType::Int:
            item->intVal = ParseInt(s);
            break;
        case SettingType::Float: {
            float f = item->floatVal;
            str::Parse(s, "%f", &f);
            item->floatVal = f;
            break;
        }
        default:
            str::ReplaceWithCopy(&item->strVal, s);
            break;
    }
    SetItemChanged(item);
    PreviewSettingChange(item);
    CancelEditValue();
}

static void ActivateItem(int lbIdx);
static void ApplyChangesAndSave();

// text editor is up (orig's editValue hwnd), not the enum dropdown
static bool AdvEditingValue() {
    return gAdv.editIdx >= 0 && !gAdv.editIsEnum;
}

// first row orig's "edit" action would open
static int FirstEditableRow() {
    int n = len(gAdv.rows);
    for (int i = 0; i < n; i++) {
        SettingItem* item = RowItem(i);
        if (item && !item->enumValues && item->type != SettingType::Bool) {
            return i;
        }
    }
    return -1;
}

static bool AdvRestoreDefault();

// "names", "nondefault", "changed", "toggle", "restore", "edit", "state",
// "killfocus", "resize", "save" and "esc" are what the settings tests ask for.
TempStr AdvSettingsRowsResultTemp(Str action, int arg, int* exitCodeOut) {
    str::Builder out;
    auto finish = [&](int code) -> TempStr {
        if (exitCodeOut) {
            *exitCodeOut = code;
        }
        return ToStrTemp(out);
    };
    if (!gAdv.visible) {
        out.Append(StrL("NOTREADY no-dialog\n"));
        return finish(2);
    }
    if (str::Eq(action, StrL("names"))) {
        for (SettingItem* item : gAdv.items) {
            out.Append(item->name);
            out.AppendChar('\n');
        }
        return finish(0);
    }
    if (str::Eq(action, StrL("nondefault"))) {
        int n = 0;
        for (SettingItem* item : gAdv.items) {
            if (!SettingDiffersFromDefault(item)) {
                continue;
            }
            n++;
            out.Append(
                fmt("%s=%s default=%s\n", item->name, FormatSettingValueTemp(item), FormatSettingDefaultTemp(item)));
        }
        out.Append(fmt("count=%d\n", n));
        return finish(0);
    }
    if (str::Eq(action, StrL("changed"))) {
        int n = CountChangedSettings();
        out.Append(fmt("changed=%d banner=%d\n", n, n > 0 ? 1 : 0));
        return finish(0);
    }
    if (str::Eq(action, StrL("toggle"))) {
        int nBool = 0;
        SettingItem* target = nullptr;
        for (SettingItem* item : gAdv.items) {
            if (item->type != SettingType::Bool) {
                continue;
            }
            if (nBool == arg) {
                target = item;
                break;
            }
            nBool++;
        }
        if (!target) {
            out.Append(StrL("ERROR no-bool\n"));
            return finish(1);
        }
        target->boolVal = !target->boolVal;
        SetItemChanged(target);
        AppShellInvalidate(gAdv.win);
        int n = CountChangedSettings();
        out.Append(fmt("toggled=%s changed=%d banner=%d\n", target->name, n, n > 0 ? 1 : 0));
        return finish(0);
    }
    // select the row of the arg-th bool setting (as "toggle" counts them) and
    // press Delete on it
    if (str::Eq(action, StrL("restore"))) {
        int nBool = 0;
        int row = -1;
        for (int i = 0; i < len(gAdv.items) && row < 0; i++) {
            if (gAdv.items[i]->type != SettingType::Bool || nBool++ != arg) {
                continue;
            }
            row = FindSettingRow(gAdv.rows, i);
        }
        if (row < 0) {
            out.Append(StrL("ERROR no-bool\n"));
            return finish(1);
        }
        gAdv.sel = row;
        bool restored = AdvRestoreDefault();
        out.Append(fmt("restored=%s changed=%d\n", restored ? RowItem(row)->name : StrL(""), CountChangedSettings()));
        return finish(0);
    }
    if (str::Eq(action, StrL("save"))) {
        CommitEditValue();
        ApplyChangesAndSave();
        CloseAdvancedSettingsDialog();
        out.Append(StrL("saved=1\n"));
        return finish(0);
    }
    if (str::Eq(action, StrL("esc"))) {
        AdvancedSettingsOnEscape();
        out.Append(fmt("closed=%d\n", gAdv.visible ? 0 : 1));
        return finish(0);
    }
    if (str::Eq(action, StrL("state"))) {
        out.Append(fmt("editing=%d\n", AdvEditingValue() ? 1 : 0));
        return finish(0);
    }
    if (str::Eq(action, StrL("edit"))) {
        int row = FirstEditableRow();
        if (row < 0) {
            out.Append(StrL("ERROR no-edit-item\n"));
            return finish(1);
        }
        gAdv.sel = row;
        ActivateItem(row);
        AppShellInvalidate(gAdv.win);
        out.Append(fmt("editing=%d\n", AdvEditingValue() ? 1 : 0));
        return finish(0);
    }
    if (str::Eq(action, StrL("killfocus"))) {
        // OSK's WM_KILLFOCUS names no new window. That must not commit.
        out.Append(fmt("editing=%d\n", AdvEditingValue() ? 1 : 0));
        return finish(0);
    }
    if (str::Eq(action, StrL("resize"))) {
        // the docked keyboard's WM_SIZE re-lays the editor out; it stays open
        out.Append(fmt("editing=%d\n", AdvEditingValue() ? 1 : 0));
        return finish(0);
    }
    out.Append(fmt("ERROR unknown-action action=%s\n", action));
    return finish(1);
}

void CloseAdvancedSettingsDialog() {
    if (!gAdv.visible) {
        return;
    }
    gAdv.visible = false;
    // covers every way the dialog goes away: on Save the value is in gSettings
    // by now, so dropping the preview renders the same thing
    EndPreviewSettingChange();
    CancelEditValue();
    MainWindow* win = gAdv.win;
    gp::Window* host = AdvHostWindow();
    gp::InputState* edits[] = {gAdv.editFilter, gAdv.editValue};
    for (gp::InputState* e : edits) {
        if (e && host) {
            gp::InputBlur(e, host->app, host);
        }
        delete e;
    }
    if (gAdv.tw) {
        ToolWindowClose(gAdv.tw);
        gAdv.tw = nullptr;
    }
    gAdv.editFilter = nullptr;
    gAdv.editValue = nullptr;
    DeleteVecMembers(gAdv.items);
    VecReset(gAdv.items);
    DeleteVecMembers(gAdv.sections);
    VecReset(gAdv.sections);
    VecReset(gAdv.rows);
    gAdv.filterWords.Reset();
    gAdv.sel = -1;
    AppShellInvalidate(win);
}

void ShowAdvancedSettingsDialog(MainWindow* win) {
    if (!HasPermission(Perm::SavePreferences)) {
        return;
    }
    if (!IsMainWindowValidAndNotClosing(win)) {
        return;
    }
    if (gAdv.visible) {
        // orig: HwndSetFocus() on the one that is open
        ToolWindowActivate(gAdv.tw);
        return;
    }
    gAdv.win = win;
    gAdv.listBounds = {};
    gp::App* app = win->gpuiWin ? win->gpuiWin->app : nullptr;
    gAdv.editFilter = new gp::InputState();
    gAdv.editFilter->focus = gp::FocusHandleNew(app);
    gp::InputSetPlaceholder(gAdv.editFilter, ToGpui(Tr("Filter settings")));
    gAdv.editValue = new gp::InputState();
    gAdv.editValue->focus = gp::FocusHandleNew(app);
    CollectSettings(gAdv.items, gAdv.sections);
    CollectFilteredSettings(gAdv.items, gAdv.filterWords, gAdv.rows);
    gAdv.sel = NextSettingRow(gAdv.rows, -1, 1);
    gAdv.scrollY = 0;
    gAdv.visible = true;
    gAdv.wantFocus = true;
    AdvOpenToolWindow(win);
    if (gAdv.tw) {
        // orig's list starts without a selection
        gAdv.sel = -1;
    }
    AppShellInvalidate(win);
}

// orig's ApplyChangesAndSave
static void ApplyChangesAndSave() {
    // snapshot settings that need explicit apply (tabs, menu bar ...) before we
    // overwrite them, so we can act on what actually changed after the reload
    SettingsApplyState before = GetSettingsApplyState();
    bool didChange = false;
    for (SettingItem* item : gAdv.items) {
        if (!item->changed) {
            continue;
        }
        didChange = true;
        u8* p = SettingFieldPtr(item->fieldOffset);
        switch (item->type) {
            case SettingType::Bool:
                *(bool*)p = item->boolVal;
                break;
            case SettingType::Int:
                *(int*)p = item->intVal;
                break;
            case SettingType::Float:
                *(float*)p = item->floatVal;
                break;
            case SettingType::Compact:
                ApplyCompactInts(item->compactInfo, p, item->strVal);
                break;
            default:
                str::ReplaceWithCopy((Str*)p, item->strVal);
                break;
        }
        logf("AdvancedSettings: %s = %s\n", item->name, FormatSettingValueTemp(item));
        // SaveSettings() re-generates these strings from their parsed
        // representations, which would clobber the edit unless the parsed
        // representation is updated as well
        if (str::EqI(item->name, StrL("DefaultDisplayMode"))) {
            gSettings->defaultDisplayModeEnum = DisplayModeFromString(item->strVal, DisplayMode::Automatic);
        } else if (str::EqI(item->name, StrL("DefaultZoom"))) {
            gSettings->defaultZoomFloat = ZoomFromString(item->strVal, kZoomActualSize);
        } else if (str::EqI(item->name, StrL("ImageUI.DefaultZoom"))) {
            gSettings->imageUI.defaultZoomFloat = ZoomFromString(item->strVal, 0);
        } else if (str::EqI(item->name, StrL("ComicBookUI.DefaultZoom"))) {
            gSettings->comicBookUI.defaultZoomFloat = ZoomFromString(item->strVal, 0);
        }
    }
    if (!didChange) {
        str::Free(before.ebookLayout);
        return;
    }
    ScheduleSaveSettings();
    // reload so that all state derived from settings (theme, fonts, parsed
    // colors, custom commands, accelerators ...) is re-computed and applied
    ForceReloadSettings();
    ApplyChangedSettingsAndRelayout(before);
}

// orig's ActivateItem: toggle a bool, or begin editing a value
static void ActivateItem(int lbIdx) {
    SettingItem* item = RowItem(lbIdx);
    if (!item) {
        return;
    }
    int idx = gAdv.rows[lbIdx].itemIdx;
    if (item->type == SettingType::Bool) {
        item->boolVal = !item->boolVal;
        SetItemChanged(item);
        return;
    }
    CommitEditValue();
    gAdv.editIdx = idx;
    if (item->enumValues) {
        gAdv.editIsEnum = true;
        StrVec vals;
        int currSel = 0;
        for (int i = 0; item->enumValues[i]; i++) {
            vals.Append(Str(item->enumValues[i]));
            if (str::EqI(item->strVal, Str(item->enumValues[i]))) {
                currSel = i;
            }
        }
        gAdv.ddValue.Init(gAdv.win && gAdv.win->gpuiWin ? gAdv.win->gpuiWin->app : nullptr);
        gAdv.ddValue.SetItems(vals, currSel);
        return;
    }
    gAdv.editIsEnum = false;
    gp::InputSetValue(gAdv.editValue, ToGpui(FormatSettingValueTemp(item)));
    gAdv.wantEditFocus = true;
}

static float AdvListViewDy() {
    if (!gAdv.tw) {
        return kAdvListDy;
    }
    // the list takes what the window has left
    float dy = gAdv.listBounds.h;
    gp::Window* host = AdvHostWindow();
    if (dy <= 0 && host) {
        dy = gp::WindowSize(host).dipH - kAdvWinChromeGuessDy;
    }
    return std::max(dy, kAdvRowDy);
}

// orig's VirtListBox::EnsureVisible, on whole rows
static void AdvEnsureVisible() {
    float viewDy = (float)(int)(AdvListViewDy() / kAdvRowDy) * kAdvRowDy;
    float maxY = std::max(0.f, (float)len(gAdv.rows) * kAdvRowDy - viewDy);
    if (gAdv.sel >= 0) {
        float top = (float)gAdv.sel * kAdvRowDy;
        if (top < gAdv.scrollY) {
            gAdv.scrollY = top;
        } else if (top + kAdvRowDy > gAdv.scrollY + viewDy) {
            gAdv.scrollY = top + kAdvRowDy - viewDy;
        }
    }
    gAdv.scrollY = std::max(0.f, std::min(gAdv.scrollY, maxY));
}

static bool AdvMoveSelection(int dir);
static bool AdvOnEnter();

// orig's HandleEscapeKey: Esc cancels an in-place edit; it only closes the
// dialog when nothing is unsaved
void AdvancedSettingsOnEscape() {
    if (!gAdv.visible) {
        return;
    }
    if (gAdv.editIdx >= 0) {
        CancelEditValue();
        AppShellInvalidate(gAdv.win);
        return;
    }
    if (CountChangedSettings() == 0) {
        CloseAdvancedSettingsDialog();
    }
}

// the frame's Up / Down
bool AdvancedSettingsMoveSelection(int dir) {
    return !gAdv.tw && AdvMoveSelection(dir);
}

// orig's HandleUpDownKey: Up / Down move the list selection (wrapping),
// stepping over the section headers
static bool AdvMoveSelection(int dir) {
    if (!gAdv.visible) {
        return false;
    }
    int n = len(gAdv.rows);
    if (n == 0 || CountSettingRows(gAdv.rows) == 0) {
        return false;
    }
    CommitEditValue();
    int sel = gAdv.sel;
    do {
        sel = (sel + dir + n) % n;
    } while (!RowItem(sel));
    gAdv.sel = sel;
    AdvEnsureVisible();
    AppShellInvalidate(gAdv.win);
    return true;
}

// the frame's Enter
bool AdvancedSettingsOnEnter() {
    return !gAdv.tw && AdvOnEnter();
}

// orig's HandleEnterKey outside an editor: activate the selected row
static bool AdvOnEnter() {
    if (!gAdv.visible) {
        return false;
    }
    ActivateItem(gAdv.sel);
    AppShellInvalidate(gAdv.win);
    return true;
}

void AdvancedSettingsView::OnCancel(AdvancedSettingsView*, gp::Ctx* cx, const gp::ClickEvent*) {
    CloseAdvancedSettingsDialog();
    gp::Notify(cx);
}

void AdvancedSettingsView::OnSave(AdvancedSettingsView*, gp::Ctx* cx, const gp::ClickEvent*) {
    CommitEditValue();
    // queue the dialog teardown first: ApplyChangesAndSave() relayouts every
    // window, which must not run against a dialog that is going away
    ApplyChangesAndSave();
    CloseAdvancedSettingsDialog();
    gp::Notify(cx);
}

void AdvancedSettingsView::OnOpenFile(AdvancedSettingsView*, gp::Ctx* cx, const gp::ClickEvent*) {
    OpenSettingsFile();
    gp::Notify(cx);
}

void AdvancedSettingsView::OnHelp(AdvancedSettingsView*, gp::Ctx* cx, const gp::ClickEvent*) {
    SumatraLaunchBrowser(Str(kSettingsDocsUrl));
    gp::Notify(cx);
}

// orig's QueryChanged
void AdvancedSettingsView::OnFilter(AdvancedSettingsView*, gp::Ctx* cx, const gp::InputEvent* ev) {
    // orig's HandleEnterKey: Enter activates the selected row even while the
    // focus is still in the filter box
    if (ev->kind == gp::InputEventKind::PressEnter) {
        if (gAdv.tw && gAdv.sel < 0) {
            // nothing selected: Enter is the default button's
            OnSave(nullptr, cx, nullptr);
            return;
        }
        ActivateItem(gAdv.sel);
        gp::Notify(cx);
        AppShellInvalidate(gAdv.win);
        return;
    }
    if (ev->kind != gp::InputEventKind::Change) {
        return;
    }
    CancelEditValue();
    TempStr filter = str::DupTemp(FromGpui(gp::InputValue(gAdv.editFilter)));
    gAdv.filterWords.Reset();
    SplitFilterToWords(filter, gAdv.filterWords);
    CollectFilteredSettings(gAdv.items, gAdv.filterWords, gAdv.rows);
    gAdv.sel = NextSettingRow(gAdv.rows, -1, 1);
    if (gAdv.tw) {
        // orig's SetModel() resets the selection
        gAdv.sel = -1;
    }
    gAdv.scrollY = 0;
    gp::Notify(cx);
}

void AdvancedSettingsView::OnScroll(AdvancedSettingsView*, gp::Ctx* cx, const gp::ScrollEvent* ev) {
    gAdv.scrollY = ev->offsetY;
    gp::Notify(cx);
    AppShellInvalidate(gAdv.win);
}

void AdvancedSettingsView::OnRowClick(AdvancedSettingsView*, gp::Ctx* cx, const gp::ClickEvent* ev, int64_t idx) {
    // a single click only selects; activation is on double-click or Enter.
    // A header row takes no click
    if (!RowItem((int)idx)) {
        return;
    }
    if (gAdv.sel != (int)idx) {
        CommitEditValue();
    }
    gAdv.sel = (int)idx;
    if (ev->clickCount >= 2) {
        ActivateItem(gAdv.sel);
    }
    gp::Notify(cx);
    AppShellInvalidate(gAdv.win);
}

void AdvancedSettingsView::OnEditInput(AdvancedSettingsView*, gp::Ctx* cx, const gp::InputEvent* ev) {
    if (ev->kind != gp::InputEventKind::PressEnter) {
        return;
    }
    CommitEditValue();
    gp::Notify(cx);
    AppShellInvalidate(gAdv.win);
}

static gp::El* AdvContentEl(gp::Ctx* cx);

gp::El* AdvancedSettingsDialogBuild(MainWindow* win, gp::Ctx* cx) {
    if (!gAdv.visible || gAdv.tw || gAdv.win != win) {
        return nullptr;
    }
    return AdvContentEl(cx);
}

// --- the dialog's elements -------------------------------------------------

// orig's AdvSettingsItemColumns: the value column is about 45% of the row
static float AdvValueDx(float rowDx) {
    float totalW = rowDx - 2 * kAdvInsetDx;
    float valW = std::max(totalW * 45 / 100, kAdvMinValDx);
    valW = std::min(valW, totalW * 3 / 5);
    float nameW = totalW - valW - kAdvColGap;
    if (nameW < kAdvMinNameDx) {
        nameW = (totalW / 2) - (kAdvColGap / 2);
        valW = totalW - nameW - kAdvColGap;
    }
    return std::max(valW, 1.f);
}

// the state the footer's hints are for
static int AdvEditState() {
    if (gAdv.editIdx < 0) {
        return kAdvEditNone;
    }
    return gAdv.editIsEnum ? kAdvEditEnum : kAdvEditValue;
}

// a header row: the section's name with its comment fainter after it, set
// low in the row so it reads as the title of the rows under it
static gp::El* AdvSectionRow(gp::Ctx* cx, SettingSection* sec, Color colMuted) {
    gp::El* row = gp::Div(cx->a)
                      ->FlexRow()
                      ->W(gp::kFill)
                      ->H(kAdvRowDy)
                      ->Shrink0()
                      ->ItemsEnd()
                      ->PadX(kAdvInsetDx)
                      ->PadB(kAdvHeaderPadB)
                      ->Gap(kAdvGapDx)
                      ->ClipX();
    row->Child(gp::TextEl(cx->a, GpuiDup(cx->a, sec->name))->Font(kAdvMetaFontPx)->Fg(ToGpui(colMuted))->Shrink0());
    if (len(sec->comment) > 0) {
        row->Child(gp::TextEl(cx->a, GpuiDup(cx->a, sec->comment))
                       ->Font(kAdvMetaFontPx)
                       ->Fg(ToGpui(AccentColor(colMuted, kAdvFaintShift)))
                       ->Truncate()
                       ->MinW(0));
    }
    return row;
}

// a setting's row: the name, and the value on the right. The value is muted
// when it is the default, the text color when not; a color setting gets a
// swatch and an unsaved edit a yellow dot before it, or the in-place editor
// takes its place
static gp::El* AdvSettingRow(gp::Ctx* cx, int i, float valDx, Color colBg, Color colText, Color colMuted) {
    const SettingRow& r = gAdv.rows[i];
    SettingItem* item = gAdv.items[r.itemIdx];
    bool editing = gAdv.editIdx == r.itemIdx;
    gp::El* row = gp::Div(cx->a)
                      ->FlexRow()
                      ->W(gp::kFill)
                      ->H(kAdvRowDy)
                      ->Shrink0()
                      ->ItemsCenter()
                      ->PadX(kAdvInsetDx)
                      ->Gap(kAdvColGap)
                      ->Radius(kAdvRadius)
                      ->PathClick(GpuiDup(cx->a, fmt("adv-row-%d", i)))
                      ->OnClick(gp::ListenTo(gAdvView, &AdvancedSettingsView::OnRowClick, (intptr_t)i));
    if (i == gAdv.sel) {
        row->Bg(ToGpui(AccentColor(colBg, kAdvSelShift)));
    }
    Str name = SettingRowName(gAdv.items, r);
    row->Child(
        FilterHighlightText(cx, name, gAdv.filterWords, ToGpui(colText), kAdvTextFontPx)->Flex1()->MinW(0)->ClipX());
    if (editing && gAdv.editIsEnum) {
        row->Child(gAdv.ddValue.Build(cx, StrL("adv-enum"), valDx));
        return row;
    }
    if (editing) {
        row->Child(
            DlgUnderlineInput(cx, GStrL("adv-value"), gAdv.editValue, kAdvTextFontPx, colText)->W(valDx)->Shrink0());
        return row;
    }
    gp::El* val = gp::Div(cx->a)->FlexRow()->ItemsCenter()->JustifyEnd()->Gap(kAdvGapDx)->W(valDx)->Shrink0();
    if (item->changed) {
        val->Child(gp::Div(cx->a)
                       ->W(kAdvDotDx)
                       ->H(kAdvDotDx)
                       ->Shrink0()
                       ->Radius(kAdvDotDx / 2)
                       ->Bg(ToGpui(ThemeAccentColor())));
    }
    Color swatch;
    if (item->type == SettingType::Color && ParseColor(&swatch, item->strVal)) {
        val->Child(gp::Div(cx->a)
                       ->W(kAdvSwatchDx)
                       ->H(kAdvSwatchDx)
                       ->Shrink0()
                       ->Bg(ToGpui(swatch))
                       ->Border(1, ToGpui(ThemeEdgeColor())));
    }
    Color valCol = SettingDiffersFromDefault(item) ? colText : colMuted;
    val->Child(gp::TextEl(cx->a, GpuiDup(cx->a, FormatSettingValueTemp(item)))
                   ->Font(kAdvTextFontPx)
                   ->Fg(ToGpui(valCol))
                   ->Truncate()
                   ->MinW(0));
    row->Child(val);
    return row;
}

// the list: only the rows in view are built, between two spacers that stand
// in for the rest
static gp::El* AdvList(gp::Ctx* cx, float viewDy, float rowDx) {
    Color colBg = ThemeWindowControlBackgroundColor();
    Color colText = ThemeWindowTextColor();
    Color colMuted = ThemeWindowTextDisabledColor();
    int n = len(gAdv.rows);
    gAdv.scrollY = std::max(0.f, std::min(gAdv.scrollY, std::max(0.f, (float)n * kAdvRowDy - viewDy)));
    int first = std::max((int)(gAdv.scrollY / kAdvRowDy) - kAdvOverscanRows, 0);
    int last = std::min(first + (int)(viewDy / kAdvRowDy) + 1 + 2 * kAdvOverscanRows, n);
    float valDx = AdvValueDx(rowDx - 2 * kAdvInsetDx);
    gp::El* rows = gp::Div(cx->a)->FlexCol()->W(gp::kFill)->PadX(kAdvInsetDx);
    if (first > 0) {
        rows->Child(gp::Div(cx->a)->W(gp::kFill)->H((float)first * kAdvRowDy)->Shrink0());
    }
    for (int i = first; i < last; i++) {
        const SettingRow& r = gAdv.rows[i];
        if (r.itemIdx < 0) {
            rows->Child(AdvSectionRow(cx, gAdv.sections[r.sectionIdx], colMuted));
        } else {
            rows->Child(AdvSettingRow(cx, i, valDx, colBg, colText, colMuted));
        }
    }
    if (last < n) {
        rows->Child(gp::Div(cx->a)->W(gp::kFill)->H((float)(n - last) * kAdvRowDy)->Shrink0());
    }
    return gp::Div(cx->a)
        ->Id(GStrL("adv-list"))
        ->FlexCol()
        ->W(gp::kFill)
        ->ScrollY(gAdv.scrollY)
        ->ScrollFromPath()
        ->OnScroll(gp::ListenTo(gAdvView, &AdvancedSettingsView::OnScroll))
        ->Child(rows);
}

// the filter line: a bare input with the count at its right end
static gp::El* AdvQueryLine(gp::Ctx* cx) {
    Color colMuted = ThemeWindowTextDisabledColor();
    TempStr count = AdvSettingsCountTemp(CountSettingRows(gAdv.rows), len(gAdv.items));
    return gp::Div(cx->a)
        ->FlexRow()
        ->W(gp::kFill)
        ->H(kAdvQueryDy)
        ->Shrink0()
        ->ItemsCenter()
        ->Gap(kAdvGapDx)
        ->PadL(kAdvPadDx - kAdvInsetDx)
        ->PadR(kAdvPadDx)
        ->Child(gpc::Input::New(cx, GStrL("adv-filter"), gAdv.editFilter)
                    ->Appearance(false)
                    ->W(gp::kFill)
                    ->IntoEl()
                    ->Font(kAdvQueryFontPx))
        ->Child(gp::TextEl(cx->a, GpuiDup(cx->a, count))->Font(kAdvMetaFontPx)->Fg(ToGpui(colMuted))->Shrink0());
}

// the help strip: the selected setting's name and default over its doc comment
static gp::El* AdvHelpStrip(gp::Ctx* cx) {
    Color colText = ThemeWindowTextColor();
    Color colMuted = ThemeWindowTextDisabledColor();
    SettingItem* item = RowItem(gAdv.sel);
    gp::El* head = gp::Div(cx->a)->FlexRow()->W(gp::kFill)->ItemsCenter()->Gap(kAdvPadDx)->Shrink0();
    head->Child(gp::TextEl(cx->a, GpuiDup(cx->a, item ? item->name : Str{}))
                    ->Font(kAdvTextFontPx)
                    ->Bold()
                    ->Fg(ToGpui(colText))
                    ->Truncate()
                    ->Flex1()
                    ->MinW(0));
    head->Child(gp::TextEl(cx->a, GpuiDup(cx->a, item ? Str(AdvSettingsDefaultTemp(item)) : Str{}))
                    ->Font(kAdvMetaFontPx)
                    ->Fg(ToGpui(colMuted))
                    ->Shrink0());
    return gp::Div(cx->a)
        ->FlexCol()
        ->W(gp::kFill)
        ->H(kAdvHelpDy)
        ->Shrink0()
        ->PadX(kAdvPadDx)
        ->PadY(kAdvHelpPadDy)
        ->Gap(3)
        ->ClipY()
        ->Child(head)
        ->Child(gp::TextEl(cx->a, GpuiDup(cx->a, item ? item->comment : Str{}))
                    ->Font(kAdvTextFontPx)
                    ->Fg(ToGpui(colMuted))
                    ->Wrap()
                    ->W(gp::kFill));
}

// the footer: the key hints and the quiet actions on the left, the unsaved
// count, Cancel and Save on the right
static gp::El* AdvFooter(gp::Ctx* cx) {
    Color colText = ThemeWindowTextColor();
    Color colMuted = ThemeWindowTextDisabledColor();
    gp::El* row = gp::Div(cx->a)
                      ->FlexRow()
                      ->W(gp::kFill)
                      ->H(kAdvFootDy)
                      ->Shrink0()
                      ->ItemsCenter()
                      ->PadX(kAdvPadDx)
                      ->Gap(kAdvGapDx);
    AdvHint hints[kAdvMaxHints];
    int nHints = AdvSettingsHints(AdvEditState(), hints);
    gp::El* hintRow = gp::Div(cx->a)->FlexRow()->ItemsCenter()->Gap(kAdvHintGapDx)->Shrink0();
    for (int i = 0; i < nHints; i++) {
        gp::El* hint = gp::Div(cx->a)->FlexRow()->ItemsCenter()->Gap(6);
        hint->Child(DlgKeyCap(cx, hints[i].key));
        hint->Child(gp::TextEl(cx->a, GpuiDup(cx->a, hints[i].desc))->Font(kAdvMetaFontPx)->Fg(ToGpui(colMuted)));
        hintRow->Child(hint);
    }
    row->Child(hintRow);
    row->Child(gpc::Button::New(cx, GStrL("adv-openfile"))
                   ->Label(ToGpui(Tr("Open Settings File")))
                   ->Ghost()
                   ->WithSize(gp::UiSize::Small)
                   ->OnClick(gp::ListenTo(gAdvView, &AdvancedSettingsView::OnOpenFile))
                   ->IntoEl());
    row->Child(gpc::Button::New(cx, GStrL("adv-help"))
                   ->Label(ToGpui(Tr("Help")))
                   ->Ghost()
                   ->WithSize(gp::UiSize::Small)
                   ->OnClick(gp::ListenTo(gAdvView, &AdvancedSettingsView::OnHelp))
                   ->IntoEl());
    row->Child(gp::Div(cx->a)->Flex1());
    int nChanged = CountChangedSettings();
    if (nChanged > 0) {
        row->Child(gp::Div(cx->a)
                       ->FlexRow()
                       ->ItemsCenter()
                       ->Gap(kAdvGapDx)
                       ->PadX(kAdvGapDx)
                       ->Shrink0()
                       ->Child(gp::Div(cx->a)
                                   ->W(kAdvDotDx)
                                   ->H(kAdvDotDx)
                                   ->Shrink0()
                                   ->Radius(kAdvDotDx / 2)
                                   ->Bg(ToGpui(ThemeAccentColor())))
                       ->Child(gp::TextEl(cx->a, GpuiDup(cx->a, fmt(Tr("Unsaved changes: %d").s, nChanged)))
                                   ->Font(kAdvMetaFontPx)
                                   ->Fg(ToGpui(colText))));
    }
    row->Child(gpc::Button::New(cx, GStrL("adv-cancel"))
                   ->Label(ToGpui(Tr("Cancel")))
                   ->Outline()
                   ->WithSize(gp::UiSize::Small)
                   ->OnClick(gp::ListenTo(gAdvView, &AdvancedSettingsView::OnCancel))
                   ->IntoEl());
    row->Child(
        DlgFilledButton(cx, GStrL("adv-save"), Tr("Save"), gp::ListenTo(gAdvView, &AdvancedSettingsView::OnSave)));
    return row;
}

// what both hosts share: the per-frame state, the focus, the enum's pick
static void AdvBeginBuild(gp::Ctx* cx) {
    if (!gAdvView.IsValid()) {
        gAdvView = gp::EntityNewState<AdvancedSettingsView>(cx->app);
    }
    if (gAdv.editIsEnum && gAdv.editIdx >= 0 && gAdv.ddValue.PollChanged(cx->app)) {
        SettingItem* item = gAdv.items[gAdv.editIdx];
        str::ReplaceWithCopy(&item->strVal, gAdv.ddValue.SelText());
        SetItemChanged(item);
        // browsing the list previews each value in turn, as orig does
        PreviewSettingChange(item);
    }
    gAdv.editFilter->onChange = gp::ListenTo(gAdvView, &AdvancedSettingsView::OnFilter);
    gAdv.editValue->onChange = gp::ListenTo(gAdvView, &AdvancedSettingsView::OnEditInput);
}

static void AdvEndBuild(gp::Ctx* cx) {
    if (gAdv.wantEditFocus && gAdv.editIdx >= 0 && !gAdv.editIsEnum) {
        gp::InputFocus(gAdv.editValue, cx->app, cx->win);
        gp::InputSelectAll(gAdv.editValue, cx->app, cx->win);
        gAdv.wantEditFocus = cx->win->input != gAdv.editValue;
    } else if (gAdv.wantFocus) {
        gp::InputFocus(gAdv.editFilter, cx->app, cx->win);
        gAdv.wantFocus = cx->win->input != gAdv.editFilter;
    }
}

// the dialog's column: the filter line, the list, the help strip and the
// footer, hairlines between them. `list` is sized by the host
static gp::El* AdvColumn(gp::Ctx* cx, gp::El* list) {
    gp::El* col = gp::Div(cx->a)->FlexCol()->W(gp::kFill)->Flex1()->MinH(0);
    col->Child(AdvQueryLine(cx));
    col->Child(DlgHairline(cx));
    col->Child(list);
    col->Child(DlgHairline(cx));
    col->Child(AdvHelpStrip(cx));
    col->Child(DlgHairline(cx));
    col->Child(AdvFooter(cx));
    return col;
}

// the dialog in the frame
static gp::El* AdvContentEl(gp::Ctx* cx) {
    AdvBeginBuild(cx);
    gp::WinSize ws = gp::WindowSize(cx->win);
    float dx = std::min(820.f, ws.dipW - 32);
    gp::El* list = AdvList(cx, kAdvListDy, dx)->H(kAdvListDy)->Shrink0();
    gp::El* body = AdvColumn(cx, list);
    DlgSetDefault(cx, gp::ListenTo(gAdvView, &AdvancedSettingsView::OnSave), true);
    gp::El* dlg = gpc::Dialog::New(cx)
                      ->Open(true)
                      ->Title(ToGpui(Tr("Advanced Settings")))
                      ->Body(body)
                      ->W(dx)
                      ->OnClose(gp::ListenTo(gAdvView, &AdvancedSettingsView::OnCancel))
                      ->IntoEl(ws);
    AdvEndBuild(cx);
    return dlg;
}

struct RestrictedUseDlg {
    MainWindow* win = nullptr;
    RestrictedUseConfig config;
    bool visible = false;
    int sel = 0;
    ToolWindow* tw = nullptr;
};

static RestrictedUseDlg gRestrictedUse;

struct RestrictedUseView {
    static void OnRow(RestrictedUseView*, gp::Ctx* cx, const gp::ClickEvent*, int64_t idx);
    static void OnToggle(RestrictedUseView*, gp::Ctx* cx, const gp::ClickEvent*, int64_t idx);
    static void OnSave(RestrictedUseView*, gp::Ctx* cx, const gp::ClickEvent*);
    static void OnDelete(RestrictedUseView*, gp::Ctx* cx, const gp::ClickEvent*);
    static void OnCancel(RestrictedUseView*, gp::Ctx* cx, const gp::ClickEvent*);
    static void OnHelp(RestrictedUseView*, gp::Ctx* cx, const gp::ClickEvent*);
};

static gp::Entity<RestrictedUseView> gRestrictedUseView;

static Str RestrictedUseTitle() {
    return Tr("Configure Restricted Use");
}

bool IsConfigureRestrictedUseDialogVisible() {
    return gRestrictedUse.visible && !gRestrictedUse.tw;
}

void CloseConfigureRestrictedUseDialog() {
    if (!gRestrictedUse.visible) {
        return;
    }
    gRestrictedUse.visible = false;
    DlgWindowClose(&gRestrictedUse.tw);
    AppShellInvalidate(gRestrictedUse.win);
}

static void ToggleRestrictedUseItem(int idx) {
    if (idx < 0 || idx >= RestrictedUseFeatureCount()) {
        return;
    }
    gRestrictedUse.sel = idx;
    gRestrictedUse.config.items[idx].enabled = !gRestrictedUse.config.items[idx].enabled;
    UpdateRestrictedUseChanged(gRestrictedUse.config);
    AppShellInvalidate(gRestrictedUse.win);
}

bool ConfigureRestrictedUseMoveSelection(int dir) {
    if (!gRestrictedUse.visible) {
        return false;
    }
    int n = RestrictedUseFeatureCount();
    if (dir < 0) {
        gRestrictedUse.sel = gRestrictedUse.sel <= 0 ? n - 1 : gRestrictedUse.sel - 1;
    } else {
        gRestrictedUse.sel = gRestrictedUse.sel >= n - 1 ? 0 : gRestrictedUse.sel + 1;
    }
    AppShellInvalidate(gRestrictedUse.win);
    return true;
}

bool ConfigureRestrictedUseOnEnter() {
    if (!gRestrictedUse.visible) {
        return false;
    }
    ToggleRestrictedUseItem(gRestrictedUse.sel);
    return true;
}

static void RestrictedUseEnter() {
    ConfigureRestrictedUseOnEnter();
}

static void OnRestrictedUseElevate(int result) {
#if OS_WIN
    if (result == MbRetYes) {
        LaunchElevated(GetSelfExePathTemp(), StrL("-configure-restricted-use"));
    }
#else
    (void)result;
#endif
}

void ShowConfigureRestrictedUseDialog(MainWindow* win) {
    if (!IsMainWindowValidAndNotClosing(win)) {
        return;
    }
    if (!CanWriteRestrictedUseConfig()) {
#if OS_WIN
        if (!IsProcessRunningElevated()) {
            MsgBox(win, Tr("Administrator access is required. Restart elevated?"), RestrictedUseTitle(),
                   MbYesNo | MbIconError, MkFunc1Void(OnRestrictedUseElevate));
            return;
        }
#endif
        MsgBox(win, Tr("SumatraPDF cannot create files in its installation folder."), RestrictedUseTitle(),
               MbOk | MbIconError);
        return;
    }

    CloseConfigureRestrictedUseDialog();
    TempStr path = GetPathInExeDirTemp(Str(kRestrictionsFileName));
    bool exists = file::Exists(path);
    LoadRestrictedUseConfig(gRestrictedUse.config, exists ? file::ReadFile(path) : Str(),
                            exists ? RestrictedUseFile::Exists : RestrictedUseFile::Missing);
    gRestrictedUse.win = win;
    gRestrictedUse.sel = 0;
    gRestrictedUse.visible = true;
    DlgWindowSpec spec;
    spec.name = "restricteduse";
    spec.title = RestrictedUseTitle;
    spec.build = ConfigureRestrictedUseDialogBuild;
    spec.close = CloseConfigureRestrictedUseDialog;
    spec.onArrow = [](int dir, bool) { return ConfigureRestrictedUseMoveSelection(dir); };
    spec.onEnter = RestrictedUseEnter;
    spec.clientDx = 520;
    gRestrictedUse.tw = DlgWindowOpen(spec, win);
    AppShellInvalidate(win);
}

void RestrictedUseView::OnRow(RestrictedUseView*, gp::Ctx* cx, const gp::ClickEvent*, int64_t idx) {
    gRestrictedUse.sel = (int)idx;
    gp::Notify(cx);
}

void RestrictedUseView::OnToggle(RestrictedUseView*, gp::Ctx* cx, const gp::ClickEvent*, int64_t idx) {
    ToggleRestrictedUseItem((int)idx);
    gp::Notify(cx);
}

void RestrictedUseView::OnSave(RestrictedUseView*, gp::Ctx* cx, const gp::ClickEvent*) {
    if (!SaveRestrictedUseConfig(gRestrictedUse.config)) {
        MsgBox(gRestrictedUse.win, Tr("Failed to save sumatrapdfrestrict.ini."), Tr("Error"), MbOk | MbIconError);
        return;
    }
    CloseConfigureRestrictedUseDialog();
    gp::Notify(cx);
}

static void OnRestrictedUseDelete(int result) {
    if (result != MbRetYes) {
        return;
    }
    if (!DeleteRestrictedUseConfig()) {
        MsgBox(gRestrictedUse.win, Tr("Failed to delete sumatrapdfrestrict.ini."), Tr("Error"), MbOk | MbIconError);
        return;
    }
    CloseConfigureRestrictedUseDialog();
}

void RestrictedUseView::OnDelete(RestrictedUseView*, gp::Ctx*, const gp::ClickEvent*) {
    MsgBox(gRestrictedUse.win, Tr("Delete sumatrapdfrestrict.ini?"), RestrictedUseTitle(), MbYesNo | MbIconQuestion,
           MkFunc1Void(OnRestrictedUseDelete));
}

void RestrictedUseView::OnCancel(RestrictedUseView*, gp::Ctx* cx, const gp::ClickEvent*) {
    CloseConfigureRestrictedUseDialog();
    gp::Notify(cx);
}

void RestrictedUseView::OnHelp(RestrictedUseView*, gp::Ctx*, const gp::ClickEvent*) {
    SumatraLaunchBrowser(Str(kRestrictedUseDocsUrl));
}

gp::El* ConfigureRestrictedUseDialogBuild(MainWindow* win, gp::Ctx* cx) {
    if (!gRestrictedUse.visible || gRestrictedUse.win != win) {
        return nullptr;
    }
    if (gRestrictedUse.tw && !DlgWindowIsHost(cx)) {
        return nullptr;
    }
    if (!gRestrictedUseView.IsValid()) {
        gRestrictedUseView = gp::EntityNewState<RestrictedUseView>(cx->app);
    }
    const gp::Theme& th = gp::ThemeNow(cx->app);
    gp::El* body = gp::Div(cx->a)->FlexCol()->Gap(6)->W(gp::kFill);
    gp::El* list = gp::Div(cx->a)->FlexCol()->W(gp::kFill)->Border(1, th.border);
    for (int i = 0; i < RestrictedUseFeatureCount(); i++) {
        gp::Listener onToggle = gp::ListenTo(gRestrictedUseView, &RestrictedUseView::OnToggle, (intptr_t)i);
        gp::El* label =
            gp::TextEl(cx->a, GpuiDup(cx->a, RestrictedUseFeatureName(i)))->Font(13)->Fg(th.foreground)->Flex1();
        if (gRestrictedUse.config.items[i].changed) {
            label->Bold();
        }
        gp::El* row = gp::Div(cx->a)
                          ->FlexRow()
                          ->W(gp::kFill)
                          ->H(28)
                          ->Shrink0()
                          ->ItemsCenter()
                          ->PadX(6)
                          ->PathClick(GpuiDup(cx->a, fmt("restricted-row-%d", i)))
                          ->OnClick(gp::ListenTo(gRestrictedUseView, &RestrictedUseView::OnRow, (intptr_t)i));
        if (i == gRestrictedUse.sel) {
            row->Bg(th.selection);
        }
        row->Child(label);
        row->Child(gpc::Checkbox::New(cx, GpuiDup(cx->a, fmt("restricted-check-%d", i)))
                       ->Checked(gRestrictedUse.config.items[i].enabled)
                       ->OnClick(onToggle)
                       ->IntoEl());
        list->Child(row);
    }
    body->Child(list);
    body->Child(gp::Div(cx->a)
                    ->W(gp::kFill)
                    ->H(72)
                    ->Shrink0()
                    ->Pad(6)
                    ->Border(1, th.border)
                    ->Child(gp::TextEl(cx->a, GpuiDup(cx->a, RestrictedUseFeatureDescription(gRestrictedUse.sel)))
                                ->Font(12)
                                ->Fg(th.mutedFg)
                                ->Wrap()));

    gp::El* footer = gp::Div(cx->a)->FlexRow()->W(gp::kFill)->ItemsCenter()->JustifyBetween()->Gap(8);
    gp::El* left = gp::Div(cx->a)->FlexRow()->ItemsCenter()->Gap(8);
    if (RestrictedUseConfigChanged(gRestrictedUse.config)) {
        left->Child(gpc::Button::New(cx, GStrL("restricted-save"))
                        ->Label(ToGpui(Tr("Save")))
                        ->Primary()
                        ->WithSize(gp::UiSize::Small)
                        ->OnClick(gp::ListenTo(gRestrictedUseView, &RestrictedUseView::OnSave))
                        ->IntoEl());
    }
    if (gRestrictedUse.config.fileExists) {
        left->Child(gpc::Button::New(cx, GStrL("restricted-delete"))
                        ->Label(ToGpui(Tr("Delete File")))
                        ->WithSize(gp::UiSize::Small)
                        ->OnClick(gp::ListenTo(gRestrictedUseView, &RestrictedUseView::OnDelete))
                        ->IntoEl());
    }
    left->Child(gpc::Button::New(cx, GStrL("restricted-cancel"))
                    ->Label(ToGpui(Tr("Cancel")))
                    ->WithSize(gp::UiSize::Small)
                    ->OnClick(gp::ListenTo(gRestrictedUseView, &RestrictedUseView::OnCancel))
                    ->IntoEl());
    footer->Child(left);
    footer->Child(gpc::Button::New(cx, GStrL("restricted-help"))
                      ->Label(ToGpui(Tr("Help")))
                      ->WithSize(gp::UiSize::Small)
                      ->OnClick(gp::ListenTo(gRestrictedUseView, &RestrictedUseView::OnHelp))
                      ->IntoEl());

    return DlgIntoEl(cx, gpc::Dialog::New(cx)
                             ->Open(true)
                             ->Title(ToGpui(RestrictedUseTitle()))
                             ->Body(body)
                             ->Footer(footer)
                             ->W(520)
                             ->OnClose(gp::ListenTo(gRestrictedUseView, &RestrictedUseView::OnCancel)));
}

// --- a window of its own (Windows) ------------------------------------------

static Str AdvToolTitle() {
    return Tr("Advanced Settings");
}

// the window's content: the column, with the list taking what is left
static gp::El* AdvToolBuild(MainWindow*, gp::Ctx* cx) {
    if (!gAdv.visible || !gAdv.tw) {
        return nullptr;
    }
    AdvBeginBuild(cx);
    gp::WinSize ws = gp::WindowSize(cx->win);
    float viewDy = AdvListViewDy();
    float rowDx = gAdv.listBounds.w > 0 ? gAdv.listBounds.w : ws.dipW;
    float prevDy = gAdv.listBounds.h;
    gp::El* list = AdvList(cx, viewDy, rowDx)->Flex1()->MinH(kAdvRowDy)->BoundsOut(&gAdv.listBounds);
    if (prevDy <= 0) {
        // the list's height is read back from the first frame
        uitask::Post(MkFunc0(AppShellInvalidate, gAdv.win), "AdvSettingsListDy");
    }
    gp::El* col =
        AdvColumn(cx, gp::Div(cx->a)->FlexCol()->W(gp::kFill)->Flex1()->MinH(0)->PadY(kAdvListPadDy)->Child(list));
    AdvEndBuild(cx);
    return col;
}

// orig's AdvancedSettingsWnd::OnKeyDown: Up / Down move the list from the
// filter too, before an edit sees them
// Delete (Cmd+Backspace on macOS) puts the selected setting's default back
static bool AdvIsRestoreDefaultKey(const gp::KeyEvent* ev) {
#if OS_DARWIN
    if (ev->vk == VK_BACK && ev->platform && !ev->ctrl && !ev->alt && !ev->shift) {
        return true;
    }
#endif
    return ev->vk == VK_DELETE && !ev->ctrl && !ev->alt && !ev->shift && !ev->platform;
}

static bool AdvRestoreDefault() {
    SettingItem* item = RowItem(gAdv.sel);
    if (!item) {
        return false;
    }
    // typing a value: Delete edits that text
    if (gAdv.editIdx >= 0 && !gAdv.editIsEnum) {
        return false;
    }
    CancelEditValue();
    RestoreSettingDefault(item);
    AppShellInvalidate(gAdv.win);
    return true;
}

static bool AdvToolOnCaptureKey(MainWindow*, gp::Ctx*, const gp::KeyEvent* ev) {
    if (!gAdv.visible) {
        return false;
    }
    if (AdvIsRestoreDefaultKey(ev)) {
        // the filter keeps the focus; a forward Delete edits its text when there is some
        bool filterHasText = gAdv.editFilter && len(gp::InputValue(gAdv.editFilter)) > 0;
        if (ev->vk == VK_DELETE && filterHasText) {
            return false;
        }
        return AdvRestoreDefault();
    }
    if (ev->ctrl || ev->alt || ev->shift) {
        return false;
    }
    if (ev->vk != VK_UP && ev->vk != VK_DOWN) {
        return false;
    }
    // the open drop-down of an enum has the arrows
    if (gAdv.editIdx >= 0 && gAdv.editIsEnum) {
        return false;
    }
    return AdvMoveSelection(ev->vk == VK_DOWN ? 1 : -1);
}

// and what the edits leave: Esc (HandleEscapeKey), Enter (HandleEnterKey)
static bool AdvToolOnKey(MainWindow*, gp::Ctx*, const gp::KeyEvent* ev) {
    if (!gAdv.visible) {
        return false;
    }
    if (ev->vk == VK_ESCAPE) {
        AdvancedSettingsOnEscape();
        return true;
    }
    if (ev->vk == VK_RETURN) {
        if (gAdv.sel < 0 && gAdv.editIdx < 0) {
            // orig's HandleEnterKey without a selection: the default button
            CommitEditValue();
            ApplyChangesAndSave();
            CloseAdvancedSettingsDialog();
            return true;
        }
        return AdvOnEnter();
    }
    return false;
}

// orig's window has no owner: it stays when the main window it was opened
// from closes
static void AdvToolOnOwnerClosed(MainWindow* newOwner) {
    gAdv.win = newOwner;
}

// the caption's close box: orig's OnClose cancels
static void AdvToolOnClosed(MainWindow*) {
    gAdv.tw = nullptr;
    CloseAdvancedSettingsDialog();
}

// orig remembers the client size the window was dragged to (#5804)
static void AdvToolOnMoved(MainWindow*, Rect) {
    gp::Window* gw = ToolWindowGpui(gAdv.tw);
    if (!gAdv.visible || !gw) {
        return;
    }
    gp::WinSize ws = gp::WindowSize(gw);
    gAdvLastClient = Size((int)ws.dipW, (int)ws.dipH);
}

// orig's Create: as wide as the frame's client area less 128 (760..1100) and
// as tall as it less 72 (480..900), or the size it had earlier this session;
// centered over the frame (PositionDialog)
static void AdvOpenToolWindow(MainWindow* win) {
    if (gAdv.tw || !ToolWindowsAvailable()) {
        return;
    }
    // orig: WS_POPUPWINDOW | WS_CAPTION | WS_THICKFRAME, no owner
    ToolWindowDesc desc;
    desc.name = "advsettings";
    desc.title = AdvToolTitle;
    desc.frame = ToolWinFrame::Caption;
    desc.resize = ToolWinResize::Resizable;
    desc.owner = ToolWinOwner::TopLevel;
    desc.minClient = Size(kAdvSettingsMinClientDx, kAdvSettingsMinClientDy);
    desc.build = AdvToolBuild;
    desc.onKey = AdvToolOnKey;
    desc.onCaptureKey = AdvToolOnCaptureKey;
    desc.onClosed = AdvToolOnClosed;
    desc.onOwnerClosed = AdvToolOnOwnerClosed;
    desc.onMoved = AdvToolOnMoved;

    gp::WinSize client = win->gpuiWin ? gp::WindowSize(win->gpuiWin) : gp::WinSize{};
    int dy = gAdvLastClient.dy > 0 ? gAdvLastClient.dy : limitValue((int)client.dipH - 72, 480, 900);
    int dx = gAdvLastClient.dx > 0 ? gAdvLastClient.dx : limitValue((int)client.dipW - 128, 760, 1100);
    dx = std::max(dx, kAdvSettingsMinClientDx);
    Size size = ToolWindowOuterSize(desc, win, Size(dx, dy));
    Rect frame = AppShellWindowScreenRect(win);
    Rect r{frame.x + (frame.dx / 2) - (size.dx / 2), frame.y + (frame.dy / 2) - (size.dy / 2), size.dx, size.dy};
    gAdv.tw = ToolWindowOpen(desc, win, AppShellShiftToWorkArea(r, win, true));
}
