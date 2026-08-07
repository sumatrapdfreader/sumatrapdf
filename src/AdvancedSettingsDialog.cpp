/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// A dialog for editing advanced settings, driven by the settings metadata
// (gGlobalPrefsInfo). Shows a filterable list of settings; clicking a bool
// toggles it, clicking an enum cycles its allowed values, clicking a string /
// color / number setting edits it in-place (Enter confirms, Esc cancels).
// Save writes the settings file and reloads it (so all derived state is
// re-computed); Cancel abandons the changes.

#include "base/Base.h"
#include "base/Win.h"
#include "base/Dpi.h"
#include "base/UITask.h"
#include "base/SettingsUtil.h"

#include "wingui/UIModels.h"
#include "wingui/Layout.h"
#include "wingui/WinGui.h"

#define INCLUDE_SETTINGSSTRUCTS_METADATA
#include "Settings.h"
#include "AppSettings.h"
#include "AppTools.h"
#include "GlobalPrefs.h"
#include "DisplayMode.h"
#include "DocController.h"
#include "EngineBase.h"
#include "MainWindow.h"
#include "PdfDarkMode.h"
#include "Theme.h"
#include "SumatraConfig.h"
#include "SumatraPDF.h"
#include "Translations.h"
#include "FilterHighlightDraw.h"
#include "AdvancedSettingsDialog.h"

constexpr const char* kSettingsDocsUrl = "https://www.sumatrapdfreader.org/settings/settings3-7.html";

// enum settings: string settings restricted to a fixed set of values.
// Fixed string values for the in-place enum drop-down. Matched by full path
// or by the last path segment so nested settings reuse the same list
// (e.g. Fullscreen.Toolbar → Toolbar).
// clang-format off
static const char* gEnumDisplayMode[] = {
    "automatic", "single page", "facing", "book view",
    "continuous", "continuous facing", "continuous book view", nullptr,
};
static const char* gEnumToolbar[] = {"show", "hide", "overlay", nullptr};
static const char* gEnumToolbarPosition[] = {"top", "bottom", nullptr};
static const char* gEnumScrollbars[] = {"windows", "smart", "overlay", "hidden", nullptr};
static const char* gEnumEngineeringDrawingEnhance[] = {"off", "auto", "on", nullptr};
static const char* gEnumDocumentColorsFollowTheme[] = {"off", "smart", "legacy", nullptr};
static const char* gEnumHomePageViewMode[] = {"thumbnails", "list", nullptr};
static const char* gEnumPrintScale[] = {"shrink", "fit", "none", nullptr};
static const char* gEnumCollate[] = {"default", "collate", "nocollate", nullptr};
static const char* gEnumFreeTextAlignment[] = {"left", "center", "right", nullptr};

namespace {
struct EnumSettingDef {
    const char* name; // full path or leaf name (last dotted segment)
    const char** values;
};
} // namespace
static const EnumSettingDef gEnumSettings[] = {
    {"DefaultDisplayMode", gEnumDisplayMode},
    {"Toolbar", gEnumToolbar},
    {"ToolbarPosition", gEnumToolbarPosition},
    {"Scrollbars", gEnumScrollbars},
    {"EngineeringDrawingEnhance", gEnumEngineeringDrawingEnhance},
    {"DocumentColorsFollowTheme", gEnumDocumentColorsFollowTheme},
    {"HomePageViewMode", gEnumHomePageViewMode},
    {"PrintScale", gEnumPrintScale},
    {"Collate", gEnumCollate},
    {"FreeTextAlignment", gEnumFreeTextAlignment},
};
// clang-format on

// Leaf name of a dotted path: "Fullscreen.Toolbar" → "Toolbar"
static Str SettingPathLeaf(Str name) {
    if (!name || name.len == 0) {
        return name;
    }
    const char* s = name.s;
    const char* last = s;
    for (int i = 0; i < name.len; i++) {
        if (s[i] == '.') {
            last = s + i + 1;
        }
    }
    return Str(last, name.len - (int)(last - s));
}

static const char** GetEnumValuesForSetting(Str name) {
    Str leaf = SettingPathLeaf(name);
    for (const auto& def : gEnumSettings) {
        if (str::EqI(name, def.name) || str::EqI(leaf, def.name)) {
            return def.values;
        }
    }
    return nullptr;
}

// a single editable setting; fieldPtr points into gGlobalPrefs, the pending
// (possibly edited) value is kept here and only written back on Save
namespace {
struct SettingItem {
    Str name;    // dotted path, e.g. "FixedPageUI.TextColor", owned
    Str comment; // doc comment describing the setting, owned
    SettingType type = SettingType::Bool;
    u8* fieldPtr = nullptr;
    const char** enumValues = nullptr; // non-null for enum (string) settings

    // pending value; strVal (owned) is used for String and Color
    bool boolVal = false;
    int intVal = 0;
    float floatVal = 0;
    Str strVal;

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

// value of the setting formatted for display; the result is temp-allocated
static TempStr FormatSettingValueTemp(SettingItem* item) {
    switch (item->type) {
        case SettingType::Bool:
            return str::DupTemp(item->boolVal ? "true" : "false");
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
// The value is previewed without touching gGlobalPrefs, so Cancel (or closing
// the dialog) puts the saved value back - see EndPreviewSettingChange().
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
    u8* p = item->fieldPtr;
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
        default: {
            Str curr = *(Str*)p;
            bool bothEmpty = len(item->strVal) == 0 && len(curr) == 0;
            item->changed = !bothEmpty && !str::Eq(item->strVal, curr);
            break;
        }
    }
}

// collect editable leaf settings from the metadata, recursing into
// sub-structs with a dotted path prefix
static void CollectSettings(Vec<SettingItem*>& items, const StructInfo* info, u8* base, Str prefix) {
    const char* fieldName = info->fieldNames;
    const char* fieldComment = info->fieldComments; // parallel to fieldNames
    for (size_t i = 0; i < info->fieldCount; i++) {
        const FieldInfo& field = info->fields[i];
        Str name(fieldName);
        fieldName += len(name) + 1;
        Str comment;
        if (fieldComment) {
            comment = Str(fieldComment);
            fieldComment += len(comment) + 1;
        }
        // internal settings (WindowState, OpenCountWeek, deprecated keys ...) are
        // app-managed and not shown to the user. The generated metadata marks
        // them, so no comment-string matching is needed here.
        if (field.internal) {
            continue;
        }
        if (field.type == SettingType::Comment) {
            continue;
        }
        u8* fieldPtr = base + field.offset;
        TempStr path = len(prefix) > 0 ? fmt("%s.%s", prefix, name) : str::DupTemp(name);
        switch (field.type) {
            case SettingType::Struct: {
                const auto* sub = (const StructInfo*)field.value;
                CollectSettings(items, sub, fieldPtr, path);
                break;
            }
            case SettingType::Bool:
            case SettingType::Int:
            case SettingType::Float:
            case SettingType::String:
            case SettingType::Color: {
                auto* item = new SettingItem();
                item->name = str::Dup(path);
                item->comment = str::Dup(comment);
                item->type = field.type;
                item->fieldPtr = fieldPtr;
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
                            item->enumValues = GetEnumValuesForSetting(path);
                        }
                        break;
                }
                items.Append(item);
                break;
            }
            default:
                // arrays, compact structs etc. can't be edited yet; the
                // "Open Settings File" button covers those
                break;
        }
    }
}

struct ListBoxModelSettings : ListBoxModel {
    Vec<SettingItem*>* items = nullptr; // not owned
    Vec<int> filtered;                  // indexes into items

    ~ListBoxModelSettings() override = default;
    int ItemsCount() override { return len(filtered); }
    Str Item(int i) override { return (*items)[filtered[i]]->name; }
    SettingItem* ItemAt(int i) {
        if (i < 0 || i >= len(filtered)) {
            return nullptr;
        }
        return (*items)[filtered[i]];
    }
};

struct AdvancedSettingsWnd : Wnd {
    ~AdvancedSettingsWnd() override;

    HFONT font = nullptr;
    HFONT fontBold = nullptr; // for changed settings, owned
    MainWindow* win = nullptr;

    Edit* editFilter = nullptr;
    ListBox* listBox = nullptr;
    ListBoxModelSettings* model = nullptr; // owned by listBox
    Edit* commentText = nullptr;           // read-only, shows the selected setting's doc comment
    Edit* editValue = nullptr;             // in-place value editor, created on demand
    DropDown* dropDownValue = nullptr;     // in-place enum editor, created on demand
    Str dropDownOrigVal;                   // value before the drop-down opened (owned), for Esc
    int editItemIdx = -1;                  // index into items of the setting being edited
    // Create/show/focus of the in-place editor can re-enter CancelEditValue
    // (filter EN_CHANGE, selection change) while the local editor pointer is
    // still in use — that was a heap UAF (CloseEnumEdit delete). While set,
    // CancelEditValue / CloseEnumEdit only mark pendingCancelEdit and return.
    bool editCreating = false;
    bool pendingCancelEdit = false;

    // bottom buttons (layout-owned; Tab order Save → Cancel → Open → Help)
    Button* btnSave = nullptr;
    Button* btnCancel = nullptr;
    Button* btnOpenSettingsFile = nullptr;
    Button* btnHelp = nullptr;

    // filter tokens for list matching + match underlay paint (command-palette style)
    StrVec filterWords;
    Vec<u8> highlighted; // scratch for DrawMaybeHighlightedText

    Vec<SettingItem*> items;

    bool Create(MainWindow* win);
    bool PreTranslateMessage(MSG& msg) override;
    bool HandleEscapeKey(bool isEditingValue, bool isEditingEnum);
    bool HandleTabKey(bool isEditingValue, bool isEditingEnum);
    bool HandleEnterKey(bool isEditingValue, bool isEditingEnum);
    bool HandleUpDownKey(const MSG& msg);
    void OnSize(UINT msg, UINT type, Size size) override;
    void AdvanceFocus(bool forward);

    void QueryChanged();
    void DrawListBoxItem(ListBox::DrawItemEvent* ev);
    void ActivateItem(int lbIdx);
    void OnItemDoubleClicked();
    void OnSelectionChanged();

    Rect ValueRectForItem(int idx); // in listBox client coords
    void BeginEditValue(int idx);
    void CommitEditValue();
    void CancelEditValue();

    void BeginEditEnum(int idx);
    void OnEnumSelectionChanged();
    void CheckDropDownClosed();
    void CloseEnumEdit(bool keepValue);

    void OnOpenSettingsFile();
    void OnHelp();
    void OnCancel();
    void OnSave();

    void ApplyChangesAndSave();
    void ScheduleDelete();
};

static AdvancedSettingsWnd* gAdvancedSettingsWnd = nullptr;

AdvancedSettingsWnd::~AdvancedSettingsWnd() {
    // editFilter, listBox and commentText are added to the layout; VBox owns
    // and deletes its children, and the base Wnd::~Wnd() deletes `layout`, so
    // they must not be deleted here (doing so is a double-free). editValue and
    // dropDownValue are created on demand and are not part of the layout, so
    // they are freed explicitly.
    // covers every way the dialog goes away (Save, Cancel, Esc, the close box).
    // On Save the value is in gGlobalPrefs by now, so dropping the preview
    // renders the same thing; on every other path it undoes the preview.
    EndPreviewSettingChange();
    delete editValue;
    delete dropDownValue;
    DeleteVecMembers(items);
    str::Free(dropDownOrigVal);
    if (fontBold) {
        DeleteObject(fontBold);
    }
}

void SafeDeleteAdvancedSettingsDialog() {
    if (!gAdvancedSettingsWnd) {
        return;
    }
    auto* tmp = gAdvancedSettingsWnd;
    gAdvancedSettingsWnd = nullptr;
    delete tmp;
}

void AdvancedSettingsWnd::ScheduleDelete() {
    if (gAdvancedSettingsWnd != this) {
        return;
    }
    auto fn = MkFunc0Void(SafeDeleteAdvancedSettingsDialog);
    uitask::Post(fn, "SafeDeleteAdvancedSettingsDialog");
}

// Match setting name against a pre-split filter so "use tabs" hits "UseTabs".
// words is empty when the filter is empty or only whitespace (match all).
// SplitFilterToWords already trims; do not ContainsI against the raw edit text
// ("use " would miss "UseTabs" because of the trailing space).
static bool SettingNameMatchesFilter(Str name, const StrVec& words) {
    if (len(words) == 0) {
        return true;
    }
    // Single token: continuous case-insensitive substring ("use" / "use " → UseTabs)
    if (len(words) == 1) {
        return str::ContainsI(name, words[0]);
    }
    // Multi-word: every word must appear (camelCase-friendly; order-independent)
    return FilterMatches(name, words);
}

void AdvancedSettingsWnd::QueryChanged() {
    CancelEditValue();
    Str filter = editFilter->GetTextTemp();
    // split trims leading/trailing/internal runs of whitespace into words
    filterWords.Reset();
    SplitFilterToWords(filter, filterWords);
    model->filtered.Reset();
    int n = len(items);
    for (int i = 0; i < n; i++) {
        if (SettingNameMatchesFilter(items[i]->name, filterWords)) {
            model->filtered.Append(i);
        }
    }
    listBox->SetModel(model); // resets selection to -1
    OnSelectionChanged();
}

// show the selected setting's doc comment in the text area below the list
void AdvancedSettingsWnd::OnSelectionChanged() {
    int lbSel = listBox->GetCurrentSelection();
    int selItemIdx = (lbSel >= 0 && lbSel < model->ItemsCount()) ? model->filtered[lbSel] : -1;
    // an in-place editor stays keyed to editItemIdx; when the selection moves to
    // a different setting, dismiss the editor (keeping the value). Guarding on
    // the index (rather than dismissing unconditionally) is important: creating
    // an editor can synchronously re-enter here (focus change / CB_SHOWDROPDOWN)
    // while the selection is still on the item being edited, and dismissing then
    // would free the editor mid-construction.
    if (editItemIdx >= 0 && editItemIdx != selItemIdx) {
        CommitEditValue();
        CloseEnumEdit(true);
    }
    if (!commentText) {
        return;
    }
    SettingItem* item = model->ItemAt(lbSel);
    commentText->SetText(item ? item->comment : Str(""));
}

// Split a list-item row into non-overlapping name (left) and value (right)
// columns so long names and long values (e.g. InverseSearchCmdLine) don't
// draw on top of each other (#5804).
static void AdvSettingsItemColumns(HWND hwnd, const Rect& rc, Rect& rcName, Rect& rcVal) {
    int pad = DpiScale(hwnd, 4);
    int gap = DpiScale(hwnd, 10);
    int totalW = rc.dx - (2 * pad);
    if (totalW < 1) {
        rcName = rc;
        rcVal = rc;
        return;
    }
    // Value column ~45% (min 100px); name gets the rest. Both are clipped with
    // ellipsis when the dialog is narrow.
    int minVal = DpiScale(hwnd, 100);
    int valW = std::max(totalW * 45 / 100, minVal);
    valW = std::min(valW, totalW * 3 / 5);
    int nameW = totalW - valW - gap;
    if (nameW < DpiScale(hwnd, 72)) {
        nameW = (totalW / 2) - (gap / 2);
        valW = totalW - nameW - gap;
    }
    nameW = std::max(nameW, 1);
    valW = std::max(valW, 1);

    rcName = rc;
    rcName.x += pad;
    rcName.dx = nameW;

    rcVal = rc;
    rcVal.x += rcVal.dx - pad - valW;
    rcVal.dx = valW;
}

void AdvancedSettingsWnd::DrawListBoxItem(ListBox::DrawItemEvent* ev) {
    ListBox* lb = ev->listBox;
    auto* m = (ListBoxModelSettings*)lb->model;
    SettingItem* item = m->ItemAt(ev->itemIndex);
    if (!item) {
        return;
    }

    HDC hdc = ev->hdc;
    Rect rc = ev->itemRect;

    COLORREF colBg = IsSpecialColor(lb->bgColor) ? GetSysColor(COLOR_WINDOW) : lb->bgColor;
    COLORREF colText = IsSpecialColor(lb->textColor) ? GetSysColor(COLOR_WINDOWTEXT) : lb->textColor;
    if (ev->selected) {
        colBg = AccentColor(colBg, 30);
    }

    SetBkColor(hdc, colBg);
    HdcFillRectWithBkColor(hdc, rc);

    // With LBS_NOINTEGRALHEIGHT the last row can be cut in half by the bottom of
    // the list; half a line of text there reads as a rendering glitch, so leave
    // it as plain background (same as the Find and Navigate Files lists, #5796)
    if (ev->clippedAtBottom && rc.y > 0) {
        return;
    }

    HFONT fontNormal = font ? font : GetAppFont(hwnd);

    // bold name => changed this session; bold value => differs from default.
    // together they show both "not the default" and "edited since opening".
    HFONT nameFont = (item->changed && fontBold) ? fontBold : fontNormal;
    HFONT valFont = (SettingDiffersFromDefault(item) && fontBold) ? fontBold : fontNormal;

    SetTextColor(hdc, colText);
    SetBkMode(hdc, TRANSPARENT);

    Rect rcName{}, rcVal{};
    AdvSettingsItemColumns(hwnd, rc, rcName, rcVal);

    bool isRtl = HwndIsRtl(lb->hwnd);
    HGDIOBJ prevFont = SelectObject(hdc, nameFont);
    // yellow/accent underlays on matched filter tokens (same as command palette)
    uint nameFmt = DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX | DT_END_ELLIPSIS;
    if (isRtl) {
        nameFmt = DT_RIGHT | DT_RTLREADING | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX | DT_END_ELLIPSIS;
    }
    DrawMaybeHighlightedText(hdc, rcName, item->name, filterWords, highlighted, colBg, isRtl, false, nameFmt);

    TempStr val = FormatSettingValueTemp(item);
    SelectObject(hdc, valFont);
    TempWStr ws = ToWStrTemp(val);
    HdcDrawText(hdc, ws, rcVal, DT_RIGHT | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX | DT_END_ELLIPSIS);

    SelectObject(hdc, prevFont);
}

// in-place editor sits in the value column (same split as DrawListBoxItem)
Rect AdvancedSettingsWnd::ValueRectForItem(int idx) {
    int lbIdx = -1;
    int n = model->ItemsCount();
    for (int i = 0; i < n; i++) {
        if (model->filtered[i] == idx) {
            lbIdx = i;
            break;
        }
    }
    if (lbIdx < 0) {
        return {};
    }
    Rect rc = LbGetItemRect(listBox->hwnd, lbIdx);
    if (rc.IsEmpty()) {
        return {};
    }
    Rect rcName{}, rcVal{};
    AdvSettingsItemColumns(hwnd, rc, rcName, rcVal);
    return rcVal;
}

void AdvancedSettingsWnd::BeginEditValue(int idx) {
    CancelEditValue();
    SettingItem* item = items[idx];
    Rect r = ValueRectForItem(idx);
    if (r.IsEmpty()) {
        return;
    }

    Edit::CreateArgs args;
    args.parent = listBox->hwnd;
    args.isMultiLine = false;
    args.withBorder = true;
    args.font = font;
    args.text = FormatSettingValueTemp(item);
    auto* c = new Edit();
    c->SetColors(ThemeWindowTextColor(), ThemeWindowControlBackgroundColor());
    // Suppress re-entrant CancelEditValue while Create/show/focus pump messages.
    editCreating = true;
    pendingCancelEdit = false;
    HWND ok = c->Create(args);
    if (!ok) {
        editCreating = false;
        delete c;
        if (pendingCancelEdit) {
            pendingCancelEdit = false;
            CancelEditValue();
        }
        return;
    }
    editValue = c;
    editItemIdx = idx;
    SetWindowPos(c->hwnd, HWND_TOP, r.x, r.y, r.dx, r.dy, SWP_SHOWWINDOW);
    c->SelectAll();
    HwndSetFocus(c->hwnd);
    editCreating = false;
    if (pendingCancelEdit) {
        pendingCancelEdit = false;
        CancelEditValue();
    }
}

void AdvancedSettingsWnd::CancelEditValue() {
    if (editCreating) {
        // Called re-entrantly from Create/show/focus of BeginEdit*; finish
        // construction first, then cancel (avoids UAF on the editor being built).
        pendingCancelEdit = true;
        return;
    }
    CloseEnumEdit(true);
    if (!editValue) {
        return;
    }
    auto* tmp = editValue;
    editValue = nullptr;
    editItemIdx = -1;
    delete tmp;
    HwndSetFocus(listBox->hwnd);
}

// show a drop-down with the allowed values over the item's value rect
void AdvancedSettingsWnd::BeginEditEnum(int idx) {
    CancelEditValue();
    SettingItem* item = items[idx];
    Rect r = ValueRectForItem(idx);
    if (r.IsEmpty()) {
        return;
    }
    // ValueRectForItem is in listBox client coords; the drop-down is parented
    // to the dialog (see below), so map the rect into dialog client coords
    r = HwndMapRectToWindow(r, listBox->hwnd, hwnd);

    DropDown::CreateArgs args;
    // parent to the dialog, not the listBox: a subclassed control (the listBox)
    // does not reflect WM_COMMAND to child controls, so CBN_SELCHANGE would
    // never reach DropDown::OnCommand and the selection would be lost
    args.parent = hwnd;
    args.font = font;
    auto* c = new DropDown();
    // Suppress re-entrant CancelEditValue / CloseEnumEdit while Create and
    // CB_SHOWDROPDOWN pump messages (filter EN_CHANGE was freeing c mid-flight).
    editCreating = true;
    pendingCancelEdit = false;
    HWND ok = c->Create(args);
    if (!ok) {
        editCreating = false;
        delete c;
        if (pendingCancelEdit) {
            pendingCancelEdit = false;
            CancelEditValue();
        }
        return;
    }
    StrVec vals;
    int currSel = 0;
    for (int i = 0; item->enumValues[i]; i++) {
        vals.Append(item->enumValues[i]);
        if (str::EqI(item->strVal, item->enumValues[i])) {
            currSel = i;
        }
    }
    c->SetItems(vals);
    c->SetCurrentSelection(currSel);
    c->onSelectionChanged = MkMethod0<AdvancedSettingsWnd, &AdvancedSettingsWnd::OnEnumSelectionChanged>(this);
    dropDownValue = c;
    editItemIdx = idx;
    str::ReplaceWithCopy(&dropDownOrigVal, item->strVal);
    SetWindowPos(c->hwnd, HWND_TOP, r.x, r.y, r.dx, r.dy, SWP_SHOWWINDOW);
    HwndSetFocus(c->hwnd);
    SendMessageW(c->hwnd, CB_SHOWDROPDOWN, TRUE, 0);
    editCreating = false;
    if (pendingCancelEdit) {
        pendingCancelEdit = false;
        CancelEditValue();
    }
}

void AdvancedSettingsWnd::OnEnumSelectionChanged() {
    if (!dropDownValue) {
        return;
    }
    SettingItem* item = items[editItemIdx];
    int sel = dropDownValue->GetCurrentSelection();
    if (sel >= 0) {
        str::ReplaceWithCopy(&item->strVal, item->enumValues[sel]);
        SetItemChanged(item);
        // browsing the list with the arrow keys previews each value in turn
        PreviewSettingChange(item);
        HwndInvalidate(listBox->hwnd, true);
    }
    // selecting with the mouse closes the list: dispose of the control then.
    // can't do it here (we're inside its notification), so check afterwards;
    // during keyboard browsing the list stays open and the control stays up
    auto fn = MkMethod0<AdvancedSettingsWnd, &AdvancedSettingsWnd::CheckDropDownClosed>(this);
    uitask::Post(fn, "AdvSettingsCheckDropDownClosed");
}

void AdvancedSettingsWnd::CheckDropDownClosed() {
    if (gAdvancedSettingsWnd != this || !dropDownValue) {
        return;
    }
    bool droppedDown = SendMessageW(dropDownValue->hwnd, CB_GETDROPPEDSTATE, 0, 0) != 0;
    if (!droppedDown) {
        CloseEnumEdit(true);
    }
}

void AdvancedSettingsWnd::CloseEnumEdit(bool keepValue) {
    if (editCreating) {
        pendingCancelEdit = true;
        return;
    }
    if (!dropDownValue) {
        return;
    }
    auto* tmp = dropDownValue;
    dropDownValue = nullptr;
    if (!keepValue) {
        SettingItem* item = items[editItemIdx];
        str::ReplaceWithCopy(&item->strVal, dropDownOrigVal);
        SetItemChanged(item);
        PreviewSettingChange(item);
    }
    editItemIdx = -1;
    delete tmp;
    HwndInvalidate(listBox->hwnd, true);
    HwndSetFocus(listBox->hwnd);
}

void AdvancedSettingsWnd::CommitEditValue() {
    if (!editValue) {
        return;
    }
    SettingItem* item = items[editItemIdx];
    Str s = editValue->GetTextTemp();
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
    HwndInvalidate(listBox->hwnd, true);
}

// activate a setting: toggle a bool, or begin editing an enum / value. A single
// click only selects (so a stray click doesn't change anything); activation
// happens on double-click or Enter.
void AdvancedSettingsWnd::ActivateItem(int lbIdx) {
    SettingItem* item = model->ItemAt(lbIdx);
    if (!item) {
        return;
    }
    if (item->type == SettingType::Bool) {
        item->boolVal = !item->boolVal;
        SetItemChanged(item);
        HwndInvalidate(listBox->hwnd, true);
        return;
    }
    int idx = model->filtered[lbIdx];
    if (item->enumValues) {
        BeginEditEnum(idx);
        return;
    }
    BeginEditValue(idx);
}

void AdvancedSettingsWnd::OnItemDoubleClicked() {
    ActivateItem(listBox->GetCurrentSelection());
}

void AdvancedSettingsWnd::ApplyChangesAndSave() {
    // snapshot settings that need explicit apply (tabs, menu bar ...) before we
    // overwrite them, so we can act on what actually changed after the reload
    SettingsApplyState before = GetSettingsApplyState();
    bool didChange = false;
    for (SettingItem* item : items) {
        if (!item->changed) {
            continue;
        }
        didChange = true;
        u8* p = item->fieldPtr;
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
            default:
                str::ReplaceWithCopy((Str*)p, item->strVal);
                break;
        }
        // SaveSettings() re-generates these strings from their parsed
        // representations, which would clobber the edit unless the parsed
        // representation is updated as well
        if (str::EqI(item->name, StrL("DefaultDisplayMode"))) {
            gGlobalPrefs->defaultDisplayModeEnum = DisplayModeFromString(item->strVal, DisplayMode::Automatic);
        } else if (str::EqI(item->name, StrL("DefaultZoom"))) {
            gGlobalPrefs->defaultZoomFloat = ZoomFromString(item->strVal, kZoomActualSize);
        } else if (str::EqI(item->name, StrL("ImageUI.DefaultZoom"))) {
            gGlobalPrefs->imageUI.defaultZoomFloat = ZoomFromString(item->strVal, 0);
        }
    }
    if (!didChange) {
        return;
    }
    SaveSettings();
    // reload so that all state derived from settings (theme, fonts, parsed
    // colors, custom commands, accelerators ...) is re-computed and applied
    ForceReloadSettings();
    // apply changes that a reload doesn't pick up on its own (menu bar, tabs,
    // anti-alias ...) and re-layout the open windows
    ApplyChangedSettingsAndRelayout(before);
}

void AdvancedSettingsWnd::OnOpenSettingsFile() {
    if (!CanAccessDisk()) {
        return;
    }
    TempStr path = GetSettingsPathTemp();
    LaunchFileIfExists(path);
}

void AdvancedSettingsWnd::OnHelp() {
    SumatraLaunchBrowser(kSettingsDocsUrl);
}

void AdvancedSettingsWnd::OnCancel() {
    ScheduleDelete();
}

void AdvancedSettingsWnd::OnSave() {
    CommitEditValue();
    // queue the dialog teardown first: ApplyChangesAndSave() may post a tabs
    // transition that closes/recreates windows (including this dialog's owner),
    // and uitask runs FIFO, so the delete must be enqueued before it
    ScheduleDelete();
    ApplyChangesAndSave();
}

// Tab / Shift+Tab: filter → list → Save → Cancel → Open Settings File → Help
void AdvancedSettingsWnd::AdvanceFocus(bool forward) {
    HWND controls[] = {
        editFilter ? editFilter->hwnd : nullptr,
        listBox ? listBox->hwnd : nullptr,
        btnSave ? btnSave->hwnd : nullptr,
        btnCancel ? btnCancel->hwnd : nullptr,
        btnOpenSettingsFile ? btnOpenSettingsFile->hwnd : nullptr,
        btnHelp ? btnHelp->hwnd : nullptr,
    };
    HWND order[6]{};
    int n = 0;
    for (HWND h : controls) {
        if (h && IsWindow(h)) {
            order[n++] = h;
        }
    }
    if (n == 0) {
        return;
    }

    HWND focused = ::GetFocus();
    int idx = -1;
    for (int i = 0; i < n; i++) {
        if (order[i] == focused || ::IsChild(order[i], focused)) {
            idx = i;
            break;
        }
    }
    // in-place editors sit over the list; treat them as the list slot
    if (idx < 0 && editValue && editValue->hwnd &&
        (focused == editValue->hwnd || ::IsChild(editValue->hwnd, focused))) {
        idx = 1; // list
    }
    if (idx < 0 && dropDownValue && dropDownValue->hwnd &&
        (focused == dropDownValue->hwnd || ::IsChild(dropDownValue->hwnd, focused))) {
        idx = 1; // list
    }

    int next;
    if (forward) {
        next = (idx + 1) % n;
    } else {
        next = (idx <= 0) ? n - 1 : idx - 1;
    }
    HwndSetFocus(order[next]);
}

bool AdvancedSettingsWnd::HandleEscapeKey(bool isEditingValue, bool isEditingEnum) {
    if (isEditingValue) {
        CancelEditValue();
    } else if (isEditingEnum) {
        CloseEnumEdit(false);
    } else {
        ScheduleDelete();
    }
    return true;
}

bool AdvancedSettingsWnd::HandleTabKey(bool isEditingValue, bool isEditingEnum) {
    // leave in-place editors before moving focus (Enter would commit too)
    if (isEditingValue) {
        CommitEditValue();
    } else if (isEditingEnum) {
        CloseEnumEdit(true);
    }
    AdvanceFocus(!IsShiftPressed());
    return true;
}

bool AdvancedSettingsWnd::HandleEnterKey(bool isEditingValue, bool isEditingEnum) {
    if (isEditingValue) {
        CommitEditValue();
        return true;
    }
    if (isEditingEnum) {
        CloseEnumEdit(true);
        return true;
    }
    // focused button: Enter = click (PreTranslate would otherwise steal
    // Enter for list-item activation)
    HWND focused = ::GetFocus();
    Button* buttons[] = {btnSave, btnCancel, btnOpenSettingsFile, btnHelp};
    for (Button* b : buttons) {
        if (b && b->hwnd && b->hwnd == focused) {
            SendMessageW(b->hwnd, BM_CLICK, 0, 0);
            return true;
        }
    }
    // Activate the selected row (bool: toggle true↔false; else edit).
    // Do not require list focus: after filter/arrow navigation the
    // selection can be valid while focus is still on the filter.
    int lbIdx = listBox->GetCurrentSelection();
    if (lbIdx < 0) {
        return false;
    }
    ActivateItem(lbIdx);
    return true;
}

// up/down in the filter edit moves the list selection (wrapping)
bool AdvancedSettingsWnd::HandleUpDownKey(const MSG& msg) {
    int dir = 0;
    if (msg.wParam == VK_UP) {
        dir = -1;
    } else if (msg.wParam == VK_DOWN) {
        dir = 1;
    }
    if (dir == 0 || msg.hwnd != editFilter->hwnd) {
        return false;
    }
    int n = listBox->GetCount();
    if (n > 0) {
        int sel = (listBox->GetCurrentSelection() + dir + n) % n;
        listBox->SetCurrentSelection(sel);
        OnSelectionChanged(); // programmatic selection: no LBN_SELCHANGE
    }
    return true;
}

bool AdvancedSettingsWnd::PreTranslateMessage(MSG& msg) {
    // a single click only selects; activation (toggle / edit) is on double-click
    // (OnItemDoubleClicked) or Enter, so there's no WM_LBUTTONUP handling here
    if (msg.message != WM_KEYDOWN) {
        return false;
    }
    bool isEditingValue = editValue && msg.hwnd == editValue->hwnd;
    bool isEditingEnum = dropDownValue && msg.hwnd == dropDownValue->hwnd;
    if (msg.wParam == VK_ESCAPE) {
        return HandleEscapeKey(isEditingValue, isEditingEnum);
    }
    if (msg.wParam == VK_TAB) {
        return HandleTabKey(isEditingValue, isEditingEnum);
    }
    if (msg.wParam == VK_RETURN) {
        return HandleEnterKey(isEditingValue, isEditingEnum);
    }
    return HandleUpDownKey(msg);
}

// clicking the window's close box sends WM_CLOSE. We must schedule our own
// teardown here: the framework's default WM_CLOSE handler calls Wnd::Destroy(),
// which removes the Wnd from the hwnd->Wnd list *before* DestroyWindow(), so the
// following WM_DESTROY never reaches onDestroy - leaving gAdvancedSettingsWnd
// dangling and blocking reopen. CloseEvent::didHandle defaults to true, so
// returning from here skips that default Destroy().
static void OnClose(Wnd::CloseEvent* /*ev*/) {
    if (gAdvancedSettingsWnd) {
        gAdvancedSettingsWnd->ScheduleDelete();
    }
}

static void OnDestroy(Wnd::DestroyEvent* /*ev*/) {
    if (gAdvancedSettingsWnd) {
        gAdvancedSettingsWnd->ScheduleDelete();
    }
}

// last client size chosen by the user (or initial layout); reused when the
// dialog is reopened in the same process so a widened window sticks (#5804)
static int gAdvSettingsLastClientDx = 0;
static int gAdvSettingsLastClientDy = 0;

// re-layout the controls when the (resizable) window is resized
void AdvancedSettingsWnd::OnSize(UINT /*msg*/, UINT /*type*/, Size size) {
    // a WS_CAPTION/WS_THICKFRAME window gets WM_SIZE during CreateCustom,
    // before the child controls exist; ignore layout until they're created
    if (!layout || !listBox) {
        return;
    }
    int dx = size.dx;
    int dy = size.dy;
    if (dx == 0 || dy == 0) {
        return;
    }
    gAdvSettingsLastClientDx = dx;
    gAdvSettingsLastClientDy = dy;
    // in-place editors are positioned over a specific item rect; that rect
    // moves on resize, so close them
    CancelEditValue();
    LayoutToSize(layout, {dx, dy});
    HwndInvalidate(hwnd);
}

// a bold variant of the given font, for drawing changed settings
static HFONT CreateBoldFont(HFONT font) {
    LOGFONTW lf{};
    if (0 == GetObjectW(font, sizeof(lf), &lf)) {
        return nullptr;
    }
    lf.lfWeight = FW_BOLD;
    return CreateFontIndirectW(&lf);
}

// center the dialog over the main window frame
static void PositionDialog(HWND hwnd, HWND hwndRelative) {
    Rect rRelative = HwndWindowRect(hwndRelative);
    Rect r = HwndWindowRect(hwnd);
    int x = rRelative.x + (rRelative.dx / 2) - (r.dx / 2);
    int y = rRelative.y + (rRelative.dy / 2) - (r.dy / 2);
    r = {x, y, r.dx, r.dy};
    Rect r2 = ShiftRectToWorkArea(r, hwndRelative, true);
    SetWindowPos(hwnd, nullptr, r2.x, r2.y, 0, 0, SWP_NOZORDER | SWP_NOSIZE);
}

bool AdvancedSettingsWnd::Create(MainWindow* mainWin) {
    win = mainWin;
    CollectSettings(items, &gGlobalPrefsInfo, (u8*)gGlobalPrefs, {});

    {
        CreateCustomArgs args;
        args.title = _TRA("Advanced Settings");
        args.visible = false;
        args.style = WS_POPUPWINDOW | WS_CAPTION | WS_THICKFRAME;
        args.font = font;
        args.icon = LoadIconW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(GetAppIconID()));
        CreateCustom(args);
    }
    if (!hwnd) {
        return false;
    }
    fontBold = CreateBoldFont(font ? font : GetAppFont(hwnd));

    auto colBg = ThemeWindowControlBackgroundColor();
    auto colTxt = ThemeWindowTextColor();
    SetColors(colTxt, colBg);
    bool isRtl = IsUIRtl();

    auto* vbox = new VBox();
    vbox->alignMain = MainAxisAlign::MainStart;
    vbox->alignCross = CrossAxisAlign::Stretch;

    {
        Edit::CreateArgs args;
        args.parent = hwnd;
        args.isMultiLine = false;
        args.withBorder = false;
        // underline so the filter field reads clearly against the dialog bg
        args.withBottomBorder = true;
        args.cueText = _TRA("enter search term to filter settings");
        args.font = font;
        args.isRtl = isRtl;
        auto* c = new Edit();
        c->SetColors(colTxt, colBg);
        c->maxDx = 150;
        HWND ok = c->Create(args);
        ReportIf(!ok);
        c->onTextChanged = MkMethod0<AdvancedSettingsWnd, &AdvancedSettingsWnd::QueryChanged>(this);
        editFilter = c;
        vbox->AddChild(c);
    }

    {
        ListBox::CreateArgs args;
        args.parent = hwnd;
        args.font = font;
        args.isRtl = isRtl;
        auto* c = new ListBox();
        c->onDrawItem =
            MkMethod1<AdvancedSettingsWnd, ListBox::DrawItemEvent*, &AdvancedSettingsWnd::DrawListBoxItem>(this);
        c->SetInsetsPt(4, 0);
        c->Create(args);
        c->SetColors(colTxt, colBg);
        listBox = c;
        model = new ListBoxModelSettings();
        model->items = &items;
        int n = len(items);
        for (int i = 0; i < n; i++) {
            model->filtered.Append(i);
        }
        c->onSelectionChanged = MkMethod0<AdvancedSettingsWnd, &AdvancedSettingsWnd::OnSelectionChanged>(this);
        c->onDoubleClick = MkMethod0<AdvancedSettingsWnd, &AdvancedSettingsWnd::OnItemDoubleClicked>(this);
        c->SetModel(model);
        vbox->AddChild(c, 1);
    }

    // read-only text area showing the selected setting's doc comment, between
    // the list and the buttons
    {
        Edit::CreateArgs args;
        args.parent = hwnd;
        args.isMultiLine = true;
        args.withBorder = false;
        args.idealSizeLines = 5;
        // don't let the doc text touch the edges of the control
        args.textPadding = 4;
        args.font = font;
        args.isRtl = isRtl;
        auto* c = new Edit();
        c->SetColors(colTxt, colBg);
        HWND ok = c->Create(args);
        ReportIf(!ok);
        SendMessageW(c->hwnd, EM_SETREADONLY, TRUE, 0);
        commentText = c;
        vbox->AddChild(new Padding(c, DpiScaledInsets(hwnd, 4, 2)));
    }

    // centered hints, above the buttons: how to edit a setting and what the
    // bold text in the list means
    {
        Str hints[] = {
            _TRA("double-click or Enter to edit"),
            _TRA("value bold? different from default"),
            _TRA("name bold? value changed and unsaved"),
        };
        for (const Str& hint : hints) {
            auto* hbox = new HBox();
            hbox->alignMain = MainAxisAlign::MainCenter;
            hbox->alignCross = CrossAxisAlign::CrossCenter;

            Static::CreateArgs args;
            args.parent = hwnd;
            args.font = font;
            args.text = hint;
            args.isRtl = isRtl;
            auto* c = new Static();
            c->SetColors(colTxt, colBg);
            c->Create(args);
            hbox->AddChild(new Padding(c, DpiScaledInsets(hwnd, 1, 8)));
            vbox->AddChild(hbox);
        }
    }

    {
        // left: Save, Cancel, Open Settings File — right: Help
        auto* hbox = new HBox();
        hbox->alignMain = MainAxisAlign::SpaceBetween;
        hbox->alignCross = CrossAxisAlign::CrossCenter;
        auto pad = Insets{4, 8, 4, 8};

        auto* left = new HBox();
        left->alignMain = MainAxisAlign::MainStart;
        left->alignCross = CrossAxisAlign::CrossCenter;
        btnSave =
            CreateButton(hwnd, _TRA("Save"), MkMethod0<AdvancedSettingsWnd, &AdvancedSettingsWnd::OnSave>(this), isRtl);
        left->AddChild(new Padding(btnSave, pad));
        btnCancel = CreateButton(hwnd, _TRA("Cancel"),
                                 MkMethod0<AdvancedSettingsWnd, &AdvancedSettingsWnd::OnCancel>(this), isRtl);
        left->AddChild(new Padding(btnCancel, pad));
        btnOpenSettingsFile =
            CreateButton(hwnd, _TRA("Open Settings File"),
                         MkMethod0<AdvancedSettingsWnd, &AdvancedSettingsWnd::OnOpenSettingsFile>(this), isRtl);
        left->AddChild(new Padding(btnOpenSettingsFile, pad));
        hbox->AddChild(left);

        btnHelp =
            CreateButton(hwnd, _TRA("Help"), MkMethod0<AdvancedSettingsWnd, &AdvancedSettingsWnd::OnHelp>(this), isRtl);
        hbox->AddChild(new Padding(btnHelp, pad));
        vbox->AddChild(hbox);
    }

    ApplyDarkModeToPopupWindow(hwnd);

    auto* padding = new Padding(vbox, DpiScaledInsets(hwnd, 4, 8));
    layout = padding;

    auto rc = HwndClientRect(win->hwndFrame);
    // Default is wide enough that long setting names (e.g. InverseSearchCmdLine)
    // and long values don't crowd each other; reuse last size if the user
    // resized earlier this session (#5804).
    int dy = gAdvSettingsLastClientDy > 0 ? gAdvSettingsLastClientDy : limitValue(rc.dy - 72, 480, 900);
    int dx = gAdvSettingsLastClientDx > 0 ? gAdvSettingsLastClientDx : limitValue(rc.dx - 128, 760, 1100);
    LayoutAndSizeToContent(layout, dx, dy, hwnd);
    gAdvSettingsLastClientDx = HwndClientRect(hwnd).dx;
    gAdvSettingsLastClientDy = HwndClientRect(hwnd).dy;
    PositionDialog(hwnd, win->hwndFrame);

    SetIsVisible(true);
    HwndSetFocus(editFilter->hwnd);
    return true;
}

void ShowAdvancedSettingsDialog(MainWindow* win) {
    if (!HasPermission(Perm::SavePreferences)) {
        return;
    }
    if (gAdvancedSettingsWnd) {
        HwndSetFocus(gAdvancedSettingsWnd->hwnd);
        return;
    }
    auto* wnd = new AdvancedSettingsWnd();
    wnd->onClose = MkFunc1Void<Wnd::CloseEvent*>(OnClose);
    wnd->onDestroy = MkFunc1Void<Wnd::DestroyEvent*>(OnDestroy);
    wnd->font = GetAppFont(win->hwndFrame);
    bool ok = wnd->Create(win);
    if (!ok) {
        delete wnd;
        return;
    }
    gAdvancedSettingsWnd = wnd;
}
