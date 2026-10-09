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
// and in-place HWND editors placed over the value column. Here it is a gpui
// Dialog whose list is an element per row, and the in-place editor is an Input
// (or a Select for an enum) rendered inside the row it belongs to.

#include "gui/GpuiBridge.h"
#include "VirtKeys.h"
#if OS_WIN
#include "base/Win.h"
#endif

#include "base/SettingsUtil.h"
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

#include "SumatraLog.h"

constexpr const char* kSettingsDocsUrl = "https://www.sumatrapdfreader.org/settings/settings3-7.html";

// a single editable setting; the pending (possibly edited) value is kept
// here and only written back on Save. The field is addressed by its offset
// into gSettings, not a pointer: a settings reload (file watcher, Save)
// frees and re-creates gSettings while the dialog stays open
namespace {
struct SettingItem {
    Str name;    // dotted path, e.g. "FixedPageUI.TextColor", owned
    Str comment; // doc comment describing the setting, owned
    SettingType type = SettingType::Bool;
    int fieldOffset = 0;
    const char** enumValues = nullptr; // non-null for enum (string) settings

    // pending value; strVal (owned) is used for String, Color and Compact
    bool boolVal = false;
    int intVal = 0;
    float floatVal = 0;
    Str strVal;
    const StructInfo* compactInfo = nullptr; // Compact: WindowMargin, PageSpacing, …

    // default value from the settings metadata; defStr (owned) for String/Color
    bool defBool = false;
    int defInt = 0;
    float defFloat = 0;
    Str defStr;

    bool changed = false; // pending value differs from what was loaded (this session)

    ~SettingItem() {
        str::Free(name);
        str::Free(comment);
        str::Free(strVal);
        str::Free(defStr);
    }
};
} // namespace

static bool CompactIsAllInts(const StructInfo* info) {
    if (!info || info->fieldCount == 0) {
        return false;
    }
    for (size_t i = 0; i < info->fieldCount; i++) {
        if (info->fields[i].type != SettingType::Int) {
            return false;
        }
    }
    return true;
}

static TempStr FormatCompactIntsTemp(const StructInfo* info, const u8* base, bool useDefault) {
    str::Builder b;
    for (size_t i = 0; i < info->fieldCount; i++) {
        const FieldInfo& f = info->fields[i];
        if (i > 0) {
            b.AppendChar(' ');
        }
        int val = useDefault ? (int)f.value : *(const int*)(base + f.offset);
        b.Append(fmt("%d", val));
    }
    return ToStrTemp(b);
}

static void ApplyCompactInts(const StructInfo* info, u8* base, Str s) {
    int off = 0;
    for (size_t i = 0; i < info->fieldCount; i++) {
        const FieldInfo& f = info->fields[i];
        if (f.type != SettingType::Int) {
            continue;
        }
        Str rest = Str(s.s + off, len(s) - off);
        off += str::TrimWs(rest);
        int val = (int)f.value;
        if (len(rest) > 0) {
            val = ParseInt(rest);
            off += str::TrimNonWs(rest);
        }
        *(int*)(base + f.offset) = val;
    }
}

static bool StrValsEq(Str a, Str b) {
    if (len(a) == 0 && len(b) == 0) {
        return true;
    }
    return str::Eq(a, b);
}

// true if the pending value differs from the setting's default value
static bool SettingDiffersFromDefault(SettingItem* item) {
    switch (item->type) {
        case SettingType::Bool:
            return item->boolVal != item->defBool;
        case SettingType::Int:
            return item->intVal != item->defInt;
        case SettingType::Float:
            return item->floatVal != item->defFloat;
        default:
            return !StrValsEq(item->strVal, item->defStr);
    }
}

static TempStr FormatSettingDefaultTemp(SettingItem* item) {
    switch (item->type) {
        case SettingType::Bool:
            return str::DupTemp(item->defBool ? StrL("true") : StrL("false"));
        case SettingType::Int:
            return fmt("%d", item->defInt);
        case SettingType::Float:
            return fmt("%g", item->defFloat);
        default:
            return str::DupTemp(item->defStr);
    }
}

// value of the setting formatted for display; the result is temp-allocated
static TempStr FormatSettingValueTemp(SettingItem* item) {
    switch (item->type) {
        case SettingType::Bool:
            return str::DupTemp(item->boolVal ? StrL("true") : StrL("false"));
        case SettingType::Int:
            return fmt("%d", item->intVal);
        case SettingType::Float:
            return fmt("%g", item->floatVal);
        default:
            return str::DupTemp(item->strVal);
    }
}

// Show what a setting does before it's saved: some settings change how pages
// are rendered, and picking a value from a list is guesswork without seeing it.
// The value is previewed without touching gSettings, so Cancel (or closing the
// dialog) puts the saved value back - see EndPreviewSettingChange().
static void PreviewSettingChange(SettingItem* item) {
    if (!str::EqI(item->name, StrL("DocumentColorsFollowTheme"))) {
        return;
    }
    SetDocumentColorsFollowThemePreview(DocumentColorsFollowThemeFromString(item->strVal));
    UpdateDocumentColors();
}

// drop every preview and render with the saved settings again
static void EndPreviewSettingChange() {
    ClearDocumentColorsFollowThemePreview();
    UpdateDocumentColors();
}

static void SetItemChanged(SettingItem* item) {
    u8* p = SettingFieldPtr(item->fieldOffset);
    switch (item->type) {
        case SettingType::Bool:
            item->changed = item->boolVal != *(bool*)p;
            break;
        case SettingType::Int:
            item->changed = item->intVal != *(int*)p;
            break;
        case SettingType::Float:
            item->changed = item->floatVal != *(float*)p;
            break;
        case SettingType::Compact:
            item->changed = !StrValsEq(item->strVal, FormatCompactIntsTemp(item->compactInfo, p, false));
            break;
        default: {
            Str curr = *(Str*)p;
            bool bothEmpty = len(item->strVal) == 0 && len(curr) == 0;
            item->changed = !bothEmpty && !str::Eq(item->strVal, curr);
            break;
        }
    }
}

// one dialog item per editable setting, seeded with the current value
static void CollectSettings(Vec<SettingItem*>& items) {
    Vec<SettingField> fields;
    CollectSettingFields(fields);
    for (const SettingField& sf : fields) {
        const FieldInfo& field = *sf.field;
        u8* fieldPtr = SettingFieldPtr(sf.offset);
        if (field.type == SettingType::Compact) {
            // only all-int compact structs; the "Open Settings File" button
            // covers the rest
            const auto* sub = (const StructInfo*)field.value;
            if (!CompactIsAllInts(sub)) {
                continue;
            }
            auto* item = new SettingItem();
            item->name = str::Dup(sf.path);
            item->comment = str::Dup(sf.comment);
            item->type = field.type;
            item->fieldOffset = sf.offset;
            item->compactInfo = sub;
            item->strVal = str::Dup(FormatCompactIntsTemp(sub, fieldPtr, false));
            item->defStr = str::Dup(FormatCompactIntsTemp(sub, fieldPtr, true));
            VecAppend(items, item);
            continue;
        }

        auto* item = new SettingItem();
        item->name = str::Dup(sf.path);
        item->comment = str::Dup(sf.comment);
        item->type = field.type;
        item->fieldOffset = sf.offset;
        // field.value holds the default: the value itself for Bool/Int,
        // a string pointer for Float/String/Color (null == empty). It's
        // NOT a valid pointer for Bool/Int, so only deref it for the
        // string-backed types.
        switch (field.type) {
            case SettingType::Bool:
                item->boolVal = *(bool*)fieldPtr;
                item->defBool = field.value != 0;
                break;
            case SettingType::Int:
                item->intVal = *(int*)fieldPtr;
                item->defInt = (int)field.value;
                break;
            case SettingType::Float:
                item->floatVal = *(float*)fieldPtr;
                str::Parse(Str((const char*)field.value), "%f", &item->defFloat);
                break;
            default:
                item->strVal = str::Dup(*(Str*)fieldPtr);
                item->defStr = str::Dup(Str((const char*)field.value));
                if (field.type == SettingType::String) {
                    item->enumValues = GetSettingsEnumValues(sf.path);
                }
                break;
        }
        VecAppend(items, item);
    }
}

constexpr float kAdvRowDy = 24;
constexpr float kAdvListDy = 360;
constexpr float kAdvListBorderDx = 1;
constexpr int kAdvOverscanRows = 2;

struct AdvancedSettingsDlg {
    MainWindow* win = nullptr;
    bool visible = false;
    Vec<SettingItem*> items;
    Vec<int> filtered; // indexes into items
    StrVec filterWords;
    int sel = -1; // index into filtered
    float scrollY = 0;
    // the comment box's offset, and the row it belongs to
    float commentScrollY = 0;
    int commentSel = -1;
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
// orig's kAdvSettingsMinClientDx
constexpr int kAdvSettingsMinClientDx = 480;
constexpr int kAdvSettingsMinClientDy = 320;
// what is in the window besides the list, until a frame has measured it
constexpr float kAdvWinChromeGuessDy = 230;

static void AdvOpenToolWindow(MainWindow* win);

// orig's sizes for the window, at 96 dpi: 19 high rows in the 12 px app font,
// 4 / 8 around, a 23 high filter, 4 above and below the rows, a comment box
// of 6 lines, 25 high buttons 7 apart
constexpr float kAdvWinRowDy = 19;
constexpr float kAdvWinFontPx = 12;
constexpr float kAdvWinPadX = 8;
constexpr float kAdvWinPadY = 4;
constexpr float kAdvWinFilterDy = 23;
constexpr float kAdvWinListPadY = 4;
constexpr float kAdvWinLineDy = 15;
constexpr int kAdvWinCommentLines = 6;
constexpr float kAdvWinBtnDy = 25;
constexpr float kAdvWinBtnPadDx = 12;
constexpr float kAdvWinBtnGap = 7;
// AdvSettingsItemColumns
constexpr float kAdvWinColPad = 4;
constexpr float kAdvWinColGap = 10;
constexpr float kAdvWinMinValDx = 100;
constexpr float kAdvWinMinNameDx = 72;
constexpr float kAdvWinScrollbarDx = 10;

static float AdvRowDy();

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
    static void OnCommentScroll(AdvancedSettingsView* self, gp::Ctx* cx, const gp::ScrollEvent* ev);
};

static gp::Entity<AdvancedSettingsView> gAdvView;

// Match setting name against a pre-split filter so "use tabs" hits "UseTabs".
// orig's SettingNameMatchesFilter
static bool SettingNameMatchesFilter(Str name, const StrVec& words) {
    if (len(words) == 0) {
        return true;
    }
    if (len(words) == 1) {
        return str::ContainsI(name, words[0]);
    }
    return FilterMatches(name, words);
}

// Keep the metadata order within each group, but put customized settings first
static void CollectFilteredSettings(Vec<SettingItem*>& items, const StrVec& words, Vec<int>& filtered) {
    VecReset(filtered);
    for (int group = 0; group < 2; group++) {
        bool wantNonDefault = group == 0;
        int n = len(items);
        for (int i = 0; i < n; i++) {
            SettingItem* item = items[i];
            if (SettingDiffersFromDefault(item) == wantNonDefault && SettingNameMatchesFilter(item->name, words)) {
                VecAppend(filtered, i);
            }
        }
    }
}

static int CountChangedSettings() {
    int n = 0;
    for (SettingItem* item : gAdv.items) {
        if (item->changed) {
            n++;
        }
    }
    return n;
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

// text editor is up (orig's editValue hwnd), not the enum dropdown
static bool AdvEditingValue() {
    return gAdv.editIdx >= 0 && !gAdv.editIsEnum;
}

// first filtered row orig's "edit" action would open
static int FirstEditableRow() {
    int n = len(gAdv.filtered);
    for (int i = 0; i < n; i++) {
        SettingItem* item = gAdv.items[gAdv.filtered[i]];
        if (item && !item->enumValues && item->type != SettingType::Bool) {
            return i;
        }
    }
    return -1;
}

// "names", "nondefault", "changed", "toggle", "edit", "state", "killfocus",
// "resize" and "esc" are what the settings tests ask for.
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
    VecReset(gAdv.filtered);
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
    gp::InputSetPlaceholder(gAdv.editFilter, ToGpui(Tr("enter search term to filter settings")));
    gAdv.editValue = new gp::InputState();
    gAdv.editValue->focus = gp::FocusHandleNew(app);
    CollectSettings(gAdv.items);
    CollectFilteredSettings(gAdv.items, gAdv.filterWords, gAdv.filtered);
    gAdv.sel = len(gAdv.filtered) > 0 ? 0 : -1;
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
    if (lbIdx < 0 || lbIdx >= len(gAdv.filtered)) {
        return;
    }
    int idx = gAdv.filtered[lbIdx];
    SettingItem* item = gAdv.items[idx];
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

static float AdvRowDy() {
    return gAdv.tw ? kAdvWinRowDy : kAdvRowDy;
}

static float AdvListViewDy() {
    if (!gAdv.tw) {
        return kAdvListDy - 2 * kAdvListBorderDx;
    }
    // the list takes what the window has left
    float dy = gAdv.listBounds.h;
    gp::Window* host = AdvHostWindow();
    if (dy <= 0 && host) {
        dy = gp::WindowSize(host).dipH - kAdvWinChromeGuessDy;
    }
    return std::max(dy, kAdvWinRowDy);
}

// orig's VirtListBox::EnsureVisible, on whole rows
static void AdvEnsureVisible() {
    float rowDy = AdvRowDy();
    float viewDy = (float)(int)(AdvListViewDy() / rowDy) * rowDy;
    float maxY = std::max(0.f, (float)len(gAdv.filtered) * rowDy - viewDy);
    if (gAdv.sel >= 0) {
        float top = (float)gAdv.sel * rowDy;
        if (top < gAdv.scrollY) {
            gAdv.scrollY = top;
        } else if (top + rowDy > gAdv.scrollY + viewDy) {
            gAdv.scrollY = top + rowDy - viewDy;
        }
    }
    gAdv.scrollY = std::max(0.f, std::min(gAdv.scrollY, maxY));
}

static bool AdvMoveSelection(int dir);
static bool AdvOnEnter();

// orig's HandleUpDownKey: Up / Down move the list selection (wrapping)
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
    int nChanged = 0;
    for (SettingItem* item : gAdv.items) {
        if (item->changed) {
            nChanged++;
        }
    }
    if (nChanged == 0) {
        CloseAdvancedSettingsDialog();
    }
}

// the frame's Up / Down
bool AdvancedSettingsMoveSelection(int dir) {
    return !gAdv.tw && AdvMoveSelection(dir);
}

static bool AdvMoveSelection(int dir) {
    if (!gAdv.visible) {
        return false;
    }
    int n = len(gAdv.filtered);
    if (n == 0) {
        return false;
    }
    CommitEditValue();
    gAdv.sel = (gAdv.sel + dir + n) % n;
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
    SumatraLaunchBrowser(StrL("https://www.sumatrapdfreader.org/settings/settings3-7.html"));
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
    CollectFilteredSettings(gAdv.items, gAdv.filterWords, gAdv.filtered);
    gAdv.sel = len(gAdv.filtered) > 0 ? 0 : -1;
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

void AdvancedSettingsView::OnCommentScroll(AdvancedSettingsView*, gp::Ctx* cx, const gp::ScrollEvent* ev) {
    gAdv.commentScrollY = ev->offsetY;
    gp::Notify(cx);
    AppShellInvalidate(gAdv.win);
}

void AdvancedSettingsView::OnRowClick(AdvancedSettingsView*, gp::Ctx* cx, const gp::ClickEvent* ev, int64_t idx) {
    // a single click only selects; activation is on double-click or Enter
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

// --- a window of its own (Windows) ------------------------------------------

static Str AdvToolTitle() {
    return Tr("Advanced Settings");
}

// orig's AdvSettingsItemColumns: the value column is about 45% of the row
static float AdvWinValueDx(float rowDx) {
    float totalW = rowDx - 2 * kAdvWinColPad;
    float valW = std::max(totalW * 45 / 100, kAdvWinMinValDx);
    valW = std::min(valW, totalW * 3 / 5);
    float nameW = totalW - valW - kAdvWinColGap;
    if (nameW < kAdvWinMinNameDx) {
        nameW = (totalW / 2) - (kAdvWinColGap / 2);
        valW = totalW - nameW - kAdvWinColGap;
    }
    return std::max(valW, 1.f);
}

// the window's content, in orig's layout
static gp::El* AdvToolBuild(MainWindow*, gp::Ctx* cx) {
    if (!gAdv.visible || !gAdv.tw) {
        return nullptr;
    }
    if (!gAdvView.IsValid()) {
        gAdvView = gp::EntityNewState<AdvancedSettingsView>(cx->app);
    }
    if (gAdv.editIsEnum && gAdv.editIdx >= 0 && gAdv.ddValue.PollChanged(cx->app)) {
        SettingItem* item = gAdv.items[gAdv.editIdx];
        str::ReplaceWithCopy(&item->strVal, gAdv.ddValue.SelText());
        SetItemChanged(item);
        PreviewSettingChange(item);
    }
    const gp::Theme& th = gp::ThemeNow(cx->app);
    gAdv.editFilter->onChange = gp::ListenTo(gAdvView, &AdvancedSettingsView::OnFilter);
    gAdv.editValue->onChange = gp::ListenTo(gAdvView, &AdvancedSettingsView::OnEditInput);
    float font = kAdvWinFontPx * ToolWindowSetUiFontPx(cx, kAdvWinFontPx);
    gp::WinSize ws = gp::WindowSize(cx->win);

    gp::El* col = gp::Div(cx->a)->FlexCol()->W(gp::kFill)->Flex1()->MinH(0)->PadX(kAdvWinPadX)->PadY(kAdvWinPadY);
    col->Child(gp::Div(cx->a)
                   ->FlexRow()
                   ->ItemsCenter()
                   ->W(gp::kFill)
                   ->H(kAdvWinFilterDy)
                   ->Shrink0()
                   ->Child(gpc::Input::New(cx, GStrL("adv-filter"), gAdv.editFilter)
                               ->WithSize(gp::UiSize::Small)
                               ->W(gp::kFill)
                               ->IntoEl()));

    int n = len(gAdv.filtered);
    float viewDy = AdvListViewDy();
    gAdv.scrollY = std::max(0.f, std::min(gAdv.scrollY, std::max(0.f, (float)n * kAdvWinRowDy - viewDy)));
    int first = std::max((int)(gAdv.scrollY / kAdvWinRowDy) - kAdvOverscanRows, 0);
    int last = std::min(first + (int)(viewDy / kAdvWinRowDy) + 1 + 2 * kAdvOverscanRows, n);
    float rowDx = gAdv.listBounds.w > 0 ? gAdv.listBounds.w : ws.dipW - 2 * kAdvWinPadX;
    // orig's list keeps its scrollbar's width free; gpui draws one over the rows
    float scrollbarDx = (float)n * kAdvWinRowDy > viewDy ? kAdvWinScrollbarDx : 0;
    rowDx -= scrollbarDx;
    float valDx = AdvWinValueDx(rowDx);
    gp::Rgba selBg = ToGpui(AccentColor(ThemeWindowControlBackgroundColor(), 30));
    gp::El* rows = gp::Div(cx->a)->FlexCol()->W(gp::kFill);
    if (first > 0) {
        rows->Child(gp::Div(cx->a)->W(gp::kFill)->H((float)first * kAdvWinRowDy)->Shrink0());
    }
    for (int i = first; i < last; i++) {
        SettingItem* item = gAdv.items[gAdv.filtered[i]];
        bool editing = gAdv.editIdx == gAdv.filtered[i];
        gp::El* row = gp::Div(cx->a)
                          ->FlexRow()
                          ->W(gp::kFill)
                          ->H(kAdvWinRowDy)
                          ->Shrink0()
                          ->ItemsCenter()
                          ->PadL(kAdvWinColPad)
                          ->PadR(kAdvWinColPad + scrollbarDx)
                          ->Gap(kAdvWinColGap)
                          ->PathClick(GpuiDup(cx->a, fmt("adv-row-%d", i)))
                          ->OnClick(gp::ListenTo(gAdvView, &AdvancedSettingsView::OnRowClick, (intptr_t)i));
        if (i == gAdv.sel) {
            row->Bg(selBg);
        }
        // bold name => changed this session; bold value => differs from default
        int boldLen = item->changed ? len(item->name) : 0;
        gp::El* name =
            FilterHighlightText(cx, item->name, gAdv.filterWords, th.foreground, font, 0, boldLen)->Flex1()->MinW(0);
        row->Child(name->ClipX());
        if (editing && gAdv.editIsEnum) {
            row->Child(gAdv.ddValue.Build(cx, StrL("adv-enum"), valDx));
        } else if (editing) {
            row->Child(gpc::Input::New(cx, GStrL("adv-value"), gAdv.editValue)
                           ->WithSize(gp::UiSize::Small)
                           ->W(valDx)
                           ->IntoEl());
        } else {
            gp::El* val =
                gp::TextEl(cx->a, GpuiDup(cx->a, FormatSettingValueTemp(item)))->Font(font)->Fg(th.foreground);
            if (SettingDiffersFromDefault(item)) {
                val->Bold();
            }
            row->Child(gp::Div(cx->a)->FlexRow()->JustifyEnd()->W(valDx)->Shrink0()->ClipX()->Child(val->Truncate()));
        }
        rows->Child(row);
    }
    if (last < n) {
        rows->Child(gp::Div(cx->a)->W(gp::kFill)->H((float)(n - last) * kAdvWinRowDy)->Shrink0());
    }
    float prevDy = gAdv.listBounds.h;
    gp::El* list = gp::Div(cx->a)
                       ->Id(GStrL("adv-list"))
                       ->FlexCol()
                       ->W(gp::kFill)
                       ->Flex1()
                       ->MinH(kAdvWinRowDy)
                       ->BoundsOut(&gAdv.listBounds)
                       ->ScrollY(gAdv.scrollY)
                       ->ScrollFromPath()
                       ->OnScroll(gp::ListenTo(gAdvView, &AdvancedSettingsView::OnScroll))
                       ->Child(rows);
    if (prevDy <= 0) {
        // the list's height is read back from the first frame
        uitask::Post(MkFunc0(AppShellInvalidate, gAdv.win), "AdvSettingsListDy");
    }
    col->Child(gp::Div(cx->a)->FlexCol()->W(gp::kFill)->Flex1()->MinH(0)->PadY(kAdvWinListPadY)->Child(list));

    int nChanged = CountChangedSettings();
    if (nChanged > 0) {
        col->Child(gp::Div(cx->a)
                       ->FlexRow()
                       ->W(gp::kFill)
                       ->H(kAdvWinLineDy + 8)
                       ->Shrink0()
                       ->ItemsCenter()
                       ->JustifyCenter()
                       ->Bg(ToGpui(kColYellow))
                       ->Child(gp::TextEl(cx->a, GpuiDup(cx->a, fmt(Tr("Unsaved changes: %d").s, nChanged)))
                                   ->Font(font)
                                   ->Bold()
                                   ->Fg(ToGpui(kColRed))));
    }

    // the selected setting's doc comment
    SettingItem* selItem = nullptr;
    if (gAdv.sel >= 0 && gAdv.sel < len(gAdv.filtered)) {
        selItem = gAdv.items[gAdv.filtered[gAdv.sel]];
    }
    int commentSel = selItem ? gAdv.filtered[gAdv.sel] : -1;
    if (commentSel != gAdv.commentSel) {
        gAdv.commentSel = commentSel;
        gAdv.commentScrollY = 0;
    }
    float commentDy = (float)kAdvWinCommentLines * kAdvWinLineDy + 8;
    col->Child(gp::Div(cx->a)->W(gp::kFill)->Shrink0()->PadX(2)->PadY(4)->Child(
        gp::Div(cx->a)
            ->Id(GStrL("adv-comment"))
            ->W(gp::kFill)
            ->H(commentDy)
            ->Pad(4)
            ->Border(1, ToGpui(ThemeEdgeColor()))
            ->ScrollY(gAdv.commentScrollY)
            ->ScrollFromPath()
            ->OnScroll(gp::ListenTo(gAdvView, &AdvancedSettingsView::OnCommentScroll))
            ->Child(gp::TextEl(cx->a, GpuiDup(cx->a, selItem ? selItem->comment : Str{}))
                        ->Font(font)
                        ->Fg(th.foreground)
                        ->Wrap()
                        ->W(gp::kFill))));
    col->Child(gp::Div(cx->a)
                   ->FlexRow()
                   ->W(gp::kFill)
                   ->H(kAdvWinLineDy + 2)
                   ->Shrink0()
                   ->ItemsCenter()
                   ->JustifyCenter()
                   ->Child(gp::TextEl(cx->a, ToGpui(Tr("Bold: differs from default")))->Font(font)->Fg(th.foreground)));

    // left: Save, Cancel, Open Settings File; right: Help
    auto button = [&](gp::Str id, Str label, gp::Listener onClick, bool isDefault) {
        gpc::Button* b = gpc::Button::New(cx, id)->Label(ToGpui(label))->OnClick(onClick);
        if (isDefault) {
            b->Primary();
        }
        return b->IntoEl()->H(kAdvWinBtnDy)->PadX(kAdvWinBtnPadDx)->Shrink0();
    };
    gp::El* buttons =
        gp::Div(cx->a)->FlexRow()->ItemsCenter()->W(gp::kFill)->H(kAdvWinBtnDy + 8)->Gap(kAdvWinBtnGap)->Shrink0();
    buttons->Child(button(GStrL("adv-save"), Tr("Save"), gp::ListenTo(gAdvView, &AdvancedSettingsView::OnSave), true));
    buttons->Child(
        button(GStrL("adv-cancel"), Tr("Cancel"), gp::ListenTo(gAdvView, &AdvancedSettingsView::OnCancel), false));
    buttons->Child(button(GStrL("adv-openfile"), Tr("Open Settings File"),
                          gp::ListenTo(gAdvView, &AdvancedSettingsView::OnOpenFile), false));
    buttons->Child(gp::Div(cx->a)->Flex1());
    buttons->Child(button(GStrL("adv-help"), Tr("Help"), gp::ListenTo(gAdvView, &AdvancedSettingsView::OnHelp), false));
    col->Child(buttons);

    if (gAdv.wantEditFocus && gAdv.editIdx >= 0 && !gAdv.editIsEnum) {
        gp::InputFocus(gAdv.editValue, cx->app, cx->win);
        gp::InputSelectAll(gAdv.editValue, cx->app, cx->win);
        gAdv.wantEditFocus = cx->win->input != gAdv.editValue;
    } else if (gAdv.wantFocus) {
        gp::InputFocus(gAdv.editFilter, cx->app, cx->win);
        gAdv.wantFocus = cx->win->input != gAdv.editFilter;
    }
    return col;
}

// orig's AdvancedSettingsWnd::OnKeyDown: Up / Down move the list from the
// filter too, before an edit sees them
static bool AdvToolOnCaptureKey(MainWindow*, gp::Ctx*, const gp::KeyEvent* ev) {
    if (!gAdv.visible || ev->ctrl || ev->alt || ev->shift) {
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

static gp::El* AdvContentEl(gp::Ctx* cx) {
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
    const gp::Theme& th = gp::ThemeNow(cx->app);
    gAdv.editFilter->onChange = gp::ListenTo(gAdvView, &AdvancedSettingsView::OnFilter);
    gAdv.editValue->onChange = gp::ListenTo(gAdvView, &AdvancedSettingsView::OnEditInput);

    gp::El* body = gp::Div(cx->a)->FlexCol()->Gap(6);
    body->Child(gp::Div(cx->a)->W(gp::kFill)->Shrink0()->Child(gpc::Input::New(cx, GStrL("adv-filter"), gAdv.editFilter)
                                                                   ->WithSize(gp::UiSize::Small)
                                                                   ->W(gp::kFill)
                                                                   ->IntoEl()));

    // ng: only the rows in view are built, between two spacers that stand in
    // for the rest
    int n = len(gAdv.filtered);
    float viewDy = AdvListViewDy();
    gAdv.scrollY = std::max(0.f, std::min(gAdv.scrollY, std::max(0.f, (float)n * kAdvRowDy - viewDy)));
    int first = std::max((int)(gAdv.scrollY / kAdvRowDy) - kAdvOverscanRows, 0);
    int last = std::min(first + (int)(viewDy / kAdvRowDy) + 1 + 2 * kAdvOverscanRows, n);
    gp::El* rows = gp::Div(cx->a)->FlexCol()->W(gp::kFill);
    if (first > 0) {
        rows->Child(gp::Div(cx->a)->W(gp::kFill)->H((float)first * kAdvRowDy)->Shrink0());
    }
    for (int i = first; i < last; i++) {
        SettingItem* item = gAdv.items[gAdv.filtered[i]];
        bool editing = gAdv.editIdx == gAdv.filtered[i];
        gp::El* row = gp::Div(cx->a)
                          ->FlexRow()
                          ->W(gp::kFill)
                          ->H(kAdvRowDy)
                          ->Shrink0()
                          ->ItemsCenter()
                          ->PadX(6)
                          ->Gap(10)
                          ->PathClick(GpuiDup(cx->a, fmt("adv-row-%d", i)))
                          ->OnClick(gp::ListenTo(gAdvView, &AdvancedSettingsView::OnRowClick, (intptr_t)i));
        if (i == gAdv.sel) {
            row->Bg(th.selection);
        }
        // bold name => changed this session; bold value => differs from default
        int boldLen = item->changed ? len(item->name) : 0;
        gp::El* name =
            FilterHighlightText(cx, item->name, gAdv.filterWords, th.foreground, 13, 0, boldLen)->Flex1()->MinW(0);
        row->Child(name->ClipX());
        if (editing && gAdv.editIsEnum) {
            row->Child(gAdv.ddValue.Build(cx, StrL("adv-enum"), 240));
        } else if (editing) {
            row->Child(
                gpc::Input::New(cx, GStrL("adv-value"), gAdv.editValue)->WithSize(gp::UiSize::Small)->W(240)->IntoEl());
        } else {
            gp::El* val = gp::TextEl(cx->a, GpuiDup(cx->a, FormatSettingValueTemp(item)))
                              ->Font(13)
                              ->Fg(th.mutedFg)
                              ->W(240)
                              ->Shrink0()
                              ->Truncate();
            if (SettingDiffersFromDefault(item)) {
                val->Bold()->Fg(th.foreground);
            }
            row->Child(val);
        }
        rows->Child(row);
    }
    if (last < n) {
        rows->Child(gp::Div(cx->a)->W(gp::kFill)->H((float)(n - last) * kAdvRowDy)->Shrink0());
    }
    gp::El* list = gp::Div(cx->a)
                       ->Id(GStrL("adv-list"))
                       ->FlexCol()
                       ->W(gp::kFill)
                       ->Border(kAdvListBorderDx, th.border)
                       ->ScrollY(gAdv.scrollY)
                       ->ScrollFromPath()
                       ->OnScroll(gp::ListenTo(gAdvView, &AdvancedSettingsView::OnScroll))
                       ->Child(rows);
    list->H(kAdvListDy);
    body->Child(list);

    int nChanged = CountChangedSettings();
    if (nChanged > 0) {
        body->Child(gp::Div(cx->a)
                        ->W(gp::kFill)
                        ->ItemsCenter()
                        ->JustifyCenter()
                        ->PadY(2)
                        ->Bg(th.yellowLight)
                        ->Child(gp::TextEl(cx->a, GpuiDup(cx->a, fmt(Tr("Unsaved changes: %d").s, nChanged)))
                                    ->Font(13)
                                    ->Bold()
                                    ->Fg(th.red)));
    }

    // the selected setting's doc comment
    SettingItem* selItem = nullptr;
    if (gAdv.sel >= 0 && gAdv.sel < len(gAdv.filtered)) {
        selItem = gAdv.items[gAdv.filtered[gAdv.sel]];
    }
    // another setting's comment starts at its top
    int commentSel = selItem ? gAdv.filtered[gAdv.sel] : -1;
    if (commentSel != gAdv.commentSel) {
        gAdv.commentSel = commentSel;
        gAdv.commentScrollY = 0;
    }
    body->Child(gp::Div(cx->a)
                    ->Id(GStrL("adv-comment"))
                    ->W(gp::kFill)
                    ->H(84)
                    ->Shrink0()
                    ->Pad(6)
                    ->Border(1, th.border)
                    ->ScrollY(gAdv.commentScrollY)
                    ->ScrollFromPath()
                    ->OnScroll(gp::ListenTo(gAdvView, &AdvancedSettingsView::OnCommentScroll))
                    ->Child(gp::TextEl(cx->a, GpuiDup(cx->a, selItem ? selItem->comment : Str{}))
                                ->Font(12)
                                ->Fg(th.mutedFg)
                                ->Wrap()
                                ->W(gp::kFill)));
    body->Child(gp::Div(cx->a)->W(gp::kFill)->ItemsCenter()->JustifyCenter()->Child(
        gp::TextEl(cx->a, ToGpui(Tr("Bold: differs from default")))->Font(12)->Fg(th.mutedFg)));

    gp::El* extra = gp::Div(cx->a)->FlexRow()->ItemsCenter()->Gap(8);
    extra->Child(gpc::Button::New(cx, GStrL("adv-openfile"))
                     ->Label(ToGpui(Tr("Open Settings File")))
                     ->WithSize(gp::UiSize::Small)
                     ->OnClick(gp::ListenTo(gAdvView, &AdvancedSettingsView::OnOpenFile))
                     ->IntoEl());
    extra->Child(gpc::Button::New(cx, GStrL("adv-help"))
                     ->Label(ToGpui(Tr("Help")))
                     ->WithSize(gp::UiSize::Small)
                     ->OnClick(gp::ListenTo(gAdvView, &AdvancedSettingsView::OnHelp))
                     ->IntoEl());
    gp::El* footer = DialogFooter(cx, extra, gAdvView, Tr("Save"), Tr("Cancel"), &AdvancedSettingsView::OnSave,
                                  &AdvancedSettingsView::OnCancel);

    gp::El* dlg = gpc::Dialog::New(cx)
                      ->Open(true)
                      ->Title(ToGpui(Tr("Advanced Settings")))
                      ->Body(body)
                      ->Footer(footer)
                      ->W(820)
                      ->OnClose(gp::ListenTo(gAdvView, &AdvancedSettingsView::OnCancel))
                      ->IntoEl(gp::WindowSize(cx->win));

    if (gAdv.wantEditFocus && gAdv.editIdx >= 0 && !gAdv.editIsEnum) {
        gp::InputFocus(gAdv.editValue, cx->app, cx->win);
        gp::InputSelectAll(gAdv.editValue, cx->app, cx->win);
        gAdv.wantEditFocus = cx->win->input != gAdv.editValue;
    } else if (gAdv.wantFocus) {
        gp::InputFocus(gAdv.editFilter, cx->app, cx->win);
        gAdv.wantFocus = cx->win->input != gAdv.editFilter;
    }
    return dlg;
}
