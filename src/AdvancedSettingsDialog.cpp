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
#include "gui/Dpi.h"
#include "base/UITask.h"
#include "base/SettingsUtil.h"

#include "gui/UIModels.h"
#include "gui/Layout.h"
#include "gui/win/WinGui.h"
#include "gui/PlatformFont.h"
#include "gui/Gfx.h"
#include "gui/GuiColors.h"
#include "gui/VirtCtrl.h"

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
#include "DarkMode_win.h"
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
    "continuous", "continuous facing", "continuous book view", "page aspect", nullptr,
};
static const char* gEnumToolbar[] = {"show", "hide", "overlay", nullptr};
static const char* gEnumToolbarPosition[] = {"top", "bottom", nullptr};
static const char* gEnumScrollbars[] = {"windows", "smart", "overlay", "hidden", nullptr};
static const char* gEnumEngineeringDrawingEnhance[] = {"off", "auto", "on", nullptr};
static const char* gEnumDocumentColorsFollowTheme[] = {"off", "smart", "legacy", nullptr};
static const char* gEnumHomePageViewMode[] = {"thumbnails", "list", nullptr};
static const char* gEnumFilePicker[] = {"", "os", "sumatrapdf", nullptr};
static const char* gEnumPrintScale[] = {"shrink", "fit", "none", nullptr};
static const char* gEnumCollate[] = {"default", "collate", "nocollate", nullptr};
static const char* gEnumFreeTextAlignment[] = {"left", "center", "right", nullptr};

namespace {
struct EnumSettingDef {
    const char* name; // full path or leaf name (last dotted segment)
    const char** values;
};
} // namespace
static const char* gEnumFullscreenDisplayMode[] = {
    "", "automatic", "single page", "facing", "book view",
    "continuous", "continuous facing", "continuous book view", nullptr,
};

static const EnumSettingDef gEnumSettings[] = {
    {"DefaultDisplayMode", gEnumDisplayMode},
    {"Fullscreen.DisplayMode", gEnumFullscreenDisplayMode},
    {"Toolbar", gEnumToolbar},
    {"ToolbarPosition", gEnumToolbarPosition},
    {"Scrollbars", gEnumScrollbars},
    {"EngineeringDrawingEnhance", gEnumEngineeringDrawingEnhance},
    {"DocumentColorsFollowTheme", gEnumDocumentColorsFollowTheme},
    {"HomePageViewMode", gEnumHomePageViewMode},
    {"FilePicker", gEnumFilePicker},
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

// The doc comment of the selected setting, wrapped to the dialog's width.
// Its height is fixed to a number of lines so the rest of the dialog doesn't
// jump around as comments of different lengths come and go
struct CommentText : VirtRichText {
    int nLines = 6;

    int FixedDy() {
        int lineDy = PlatformFontMeasureText(font, "Ag").dy;
        return (lineDy * nLines) + padding.top + padding.bottom;
    }

    // the height is ours in all three, or the box would follow the text: it is
    // laid out (and sized) while empty, and every comment is a different length
    Size GetIdealSize() override {
        Size sz = VirtRichText::GetIdealSize();
        sz.dy = FixedDy();
        return sz;
    }

    int MinIntrinsicHeight(int width) override {
        VirtRichText::MinIntrinsicHeight(width); // wraps to `width`
        return FixedDy();
    }

    Size Layout(Constraints bc) override {
        Size sz = VirtRichText::Layout(bc);
        sz.dy = FixedDy();
        return bc.Constrain(sz);
    }

    // the box is a fixed number of lines, so a longer comment has to be cut off
    // rather than spill over the hints below it
    void Paint(VirtPaintCtx& ctx) override {
        Rect clip = ctx.bounds.Intersect(ctx.clip);
        if (clip.IsEmpty()) {
            return;
        }
        ctx.gfx->PushClip(clip);
        Color bg = GetColor(kColRichBg);
        if (!ColorSkipsPaint(bg)) {
            ctx.gfx->FillRect(ctx.bounds, bg);
        }
        VirtRichText::Paint(ctx);
        ctx.gfx->DrawRect(ctx.bounds, ThemeEdgeColor());
        ctx.gfx->PopClip();
    }

    // Reset() drops the word layout, so re-wrap to the width we already have
    void SetText(Str s) {
        Reset();
        AddPlainText(s);
        LayoutText(ContentRectInWindow().dx);
        Invalidate();
    }
};

struct AdvancedSettingsWnd : WindowBase {
    ~AdvancedSettingsWnd() override;

    PlatformFont* fontBold = nullptr;
    MainWindow* win = nullptr;

    // the list, the doc comment, the hints and the buttons are virtual
    // controls; the filter and the in-place editors are real HWNDs
    Edit* editFilter = nullptr;
    VirtListBox* listBox = nullptr;
    ListBoxModelSettings* model = nullptr; // owned by listBox
    CommentText* commentText = nullptr;    // shows the selected setting's doc comment
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
    VirtButton* btnSave = nullptr;
    VirtButton* btnCancel = nullptr;
    VirtButton* btnOpenSettingsFile = nullptr;
    VirtButton* btnHelp = nullptr;

    // filter tokens for list matching + match underlay paint (command-palette style)
    StrVec filterWords;
    Vec<u8> highlighted; // scratch for DrawMaybeHighlightedText

    Vec<SettingItem*> items;

    bool Create(MainWindow* win);
    void OnKeyDown(KeyEvent* ev);
    bool HandleEscapeKey(bool isEditingValue, bool isEditingEnum);
    bool HandleTabKey(bool isEditingValue, bool isEditingEnum);
    bool HandleEnterKey(bool isEditingValue, bool isEditingEnum);
    bool HandleUpDownKey(const KeyEvent& ev);
    void OnSize(WindowBase::SizeEvent* ev);

    void QueryChanged();
    void DrawListBoxItem(VirtListBox::DrawItemEvent* ev);
    void ActivateItem(int lbIdx);
    void OnItemDoubleClicked();
    void OnSelectionChanged();

    Rect ValueRectForItem(int idx); // in listBox client coords
    void BeginEditValue(int idx);
    void CommitEditValue();
    void CancelEditValue();

    void BeginEditEnum(int idx);
    void OnEnumSelectionChanged();
    void OnEnumDropDownClosed();
    void CheckDropDownClosed();
    void CloseEnumEdit(bool keepValue);
    void KeepFocus();

    void OnOpenSettingsFile(VirtMouseEvent* ev = nullptr);
    void OnHelp(VirtMouseEvent* ev = nullptr);
    void OnCancel(VirtMouseEvent* ev = nullptr);
    void OnSave(VirtMouseEvent* ev = nullptr);

    void ApplyChangesAndSave();
};

static AdvancedSettingsWnd* gAdvancedSettingsWnd = nullptr;

AdvancedSettingsWnd::~AdvancedSettingsWnd() {
    // editFilter, listBox and commentText are added to the layout; VBox owns
    // and deletes its children, and the base WindowBase::~WindowBase() deletes `layout`, so
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
}

static void ClearAdvancedSettingsWnd() {
    gAdvancedSettingsWnd = nullptr;
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

// Keep the metadata order within each group, but put customized settings first
// so the values users are most likely to review are immediately visible.
static void CollectFilteredSettings(Vec<SettingItem*>& items, const StrVec& words, Vec<int>& filtered) {
    filtered.Reset();
    for (int group = 0; group < 2; group++) {
        bool wantNonDefault = group == 0;
        int n = len(items);
        for (int i = 0; i < n; i++) {
            SettingItem* item = items[i];
            if (SettingDiffersFromDefault(item) == wantNonDefault && SettingNameMatchesFilter(item->name, words)) {
                filtered.Append(i);
            }
        }
    }
}

void AdvancedSettingsWnd::QueryChanged() {
    CancelEditValue();
    Str filter = editFilter->GetTextTemp();
    // split trims leading/trailing/internal runs of whitespace into words
    filterWords.Reset();
    SplitFilterToWords(filter, filterWords);
    CollectFilteredSettings(items, filterWords, model->filtered);
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
static void AdvSettingsItemColumns(const Rect& rc, Rect& rcName, Rect& rcVal) {
    int pad = DpiScale(4);
    int gap = DpiScale(10);
    int totalW = rc.dx - (2 * pad);
    if (totalW < 1) {
        rcName = rc;
        rcVal = rc;
        return;
    }
    // Value column ~45% (min 100px); name gets the rest. Both are clipped with
    // ellipsis when the dialog is narrow.
    int minVal = DpiScale(100);
    int valW = std::max(totalW * 45 / 100, minVal);
    valW = std::min(valW, totalW * 3 / 5);
    int nameW = totalW - valW - gap;
    if (nameW < DpiScale(72)) {
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

void AdvancedSettingsWnd::DrawListBoxItem(VirtListBox::DrawItemEvent* ev) {
    VirtListBox* lb = ev->listBox;
    auto* m = (ListBoxModelSettings*)lb->model;
    SettingItem* item = m->ItemAt(ev->itemIndex);
    if (!item) {
        return;
    }

    Gfx* gfx = ev->gfx;
    Rect rc = ev->itemRect;

    Color colBg = lb->GetColor(kColListBg);
    Color colText = lb->GetColor(kColListText);
    if (IsSpecialColor(colBg)) {
        colBg = GetSysColor(COLOR_WINDOW);
    }
    if (IsSpecialColor(colText)) {
        colText = GetSysColor(COLOR_WINDOWTEXT);
    }
    if (ev->selected) {
        colBg = AccentColor(colBg, 30);
    }

    gfx->FillRect(rc, colBg);

    PlatformFont* fontNormal = GetFont() ? GetFont() : GetAppFont();

    // bold name => changed this session; bold value => differs from default.
    // together they show both "not the default" and "edited since opening".
    PlatformFont* nameFont = (item->changed && fontBold) ? fontBold : fontNormal;
    PlatformFont* valFont = (SettingDiffersFromDefault(item) && fontBold) ? fontBold : fontNormal;

    Rect rcName{}, rcVal{};
    AdvSettingsItemColumns(rc, rcName, rcVal);

    bool isRtl = HwndIsRtl(lb->GetHwnd());
    // yellow/accent underlays on matched filter tokens (same as command palette)
    u32 nameFmt = gfxTextEllipsis | gfxTextVCenter;
    nameFmt |= isRtl ? (gfxTextRight | gfxTextRtl) : gfxTextLeft;
    DrawMaybeHighlightedText(gfx, rcName, item->name, filterWords, highlighted, colBg, isRtl, false, nameFmt, nameFont,
                             colText);

    TempStr val = FormatSettingValueTemp(item);
    u32 valFmt = gfxTextEllipsis | gfxTextVCenter | gfxTextRight;
    gfx->DrawText(val, rcVal, valFmt, valFont, colText);
}

// in-place editor sits in the value column (same split as DrawListBoxItem)
// in window (dialog client) coords, which is where the in-place editors go
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
    Rect rc = listBox->ItemRect(lbIdx);
    if (rc.IsEmpty()) {
        return {};
    }
    Rect rcName{}, rcVal{};
    AdvSettingsItemColumns(rc, rcName, rcVal);
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
    args.parent = hwnd;
    args.isMultiLine = false;
    args.withBorder = true;
    args.font = GetFont();
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
    SetFocusTo(listBox);
}

// show a drop-down with the allowed values over the item's value rect
void AdvancedSettingsWnd::BeginEditEnum(int idx) {
    CancelEditValue();
    SettingItem* item = items[idx];
    Rect r = ValueRectForItem(idx);
    if (r.IsEmpty()) {
        return;
    }
    DropDown::CreateArgs args;
    args.parent = hwnd;
    args.font = GetFont();
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
    c->onCloseUp = MkMethod0<AdvancedSettingsWnd, &AdvancedSettingsWnd::OnEnumDropDownClosed>(this);
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

// Previewing a value (or the combo list closing) can activate the main
// window. Put the user back in this dialog; if the list is still open,
// leave the caret on the combo.
void AdvancedSettingsWnd::KeepFocus() {
    if (gAdvancedSettingsWnd != this || !hwnd) {
        return;
    }
    HwndToForeground(hwnd);
    HWND focused = ::GetFocus();
    if (focused == hwnd || ::IsChild(hwnd, focused)) {
        return;
    }
    if (dropDownValue && dropDownValue->hwnd) {
        HwndSetFocus(dropDownValue->hwnd);
        return;
    }
    HwndSetFocus(hwnd);
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
        listBox->Invalidate();
    }
    KeepFocus();
    // selecting with the mouse closes the list: dispose of the control then.
    // can't do it here (we're inside its notification), so check afterwards;
    // during keyboard browsing the list stays open and the control stays up
    auto fn = MkMethod0<AdvancedSettingsWnd, &AdvancedSettingsWnd::CheckDropDownClosed>(this);
    uitask::Post(fn, "AdvSettingsCheckDropDownClosed");
}

void AdvancedSettingsWnd::OnEnumDropDownClosed() {
    // CBN_CLOSEUP is when Windows hands activation to whoever it thinks
    // owned the list (often the main window). Take it back now, not after
    // a posted task that runs too late.
    KeepFocus();
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
    listBox->Invalidate();
    KeepFocus();
    SetFocusTo(listBox);
    // destroying the combo can activate the main window after we return
    auto fn = MkMethod0<AdvancedSettingsWnd, &AdvancedSettingsWnd::KeepFocus>(this);
    uitask::Post(fn, "AdvSettingsKeepFocus");
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
    listBox->Invalidate();
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
        listBox->Invalidate();
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
        } else if (str::EqI(item->name, StrL("ComicBookUI.DefaultZoom"))) {
            gGlobalPrefs->comicBookUI.defaultZoomFloat = ZoomFromString(item->strVal, 0);
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

void AdvancedSettingsWnd::OnOpenSettingsFile(VirtMouseEvent*) {
    if (!CanAccessDisk()) {
        return;
    }
    TempStr path = GetSettingsPathTemp();
    LaunchFileIfExists(path);
}

void AdvancedSettingsWnd::OnHelp(VirtMouseEvent*) {
    SumatraLaunchBrowser(kSettingsDocsUrl);
}

void AdvancedSettingsWnd::OnCancel(VirtMouseEvent*) {
    ScheduleDelete();
}

void AdvancedSettingsWnd::OnSave(VirtMouseEvent*) {
    CommitEditValue();
    // queue the dialog teardown first: ApplyChangesAndSave() may post a tabs
    // transition that closes/recreates windows (including this dialog's owner),
    // and uitask runs FIFO, so the delete must be enqueued before it
    ScheduleDelete();
    ApplyChangesAndSave();
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
    // the ring is the layout order: filter → list → Save → Cancel → Open → Help
    TabNavigate(IsShiftPressed());
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
    // focused button: Enter = click (OnKeyDown would otherwise steal
    // Enter for list-item activation)
    LRESULT res = 0;
    if (vroot && vroot->focused && VirtTreeOnMessage(hwnd, vroot, WM_KEYDOWN, VK_RETURN, 0, res)) {
        return true;
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
bool AdvancedSettingsWnd::HandleUpDownKey(const KeyEvent& ev) {
    int dir = 0;
    if (ev.vkey == VK_UP) {
        dir = -1;
    } else if (ev.vkey == VK_DOWN) {
        dir = 1;
    }
    if (dir == 0 || ev.hwnd != editFilter->hwnd) {
        return false;
    }
    int n = listBox->ItemsCount();
    if (n > 0) {
        int sel = (listBox->GetCurrentSelection() + dir + n) % n;
        listBox->SetCurrentSelection(sel);
        OnSelectionChanged(); // programmatic selection: no LBN_SELCHANGE
    }
    return true;
}

void AdvancedSettingsWnd::OnKeyDown(KeyEvent* ev) {
    // a single click only selects; activation (toggle / edit) is on double-click
    // (OnItemDoubleClicked) or Enter
    bool isEditingValue = editValue && ev->hwnd == editValue->hwnd;
    bool isEditingEnum = dropDownValue && ev->hwnd == dropDownValue->hwnd;
    if (ev->vkey == VK_ESCAPE) {
        ev->didHandle = HandleEscapeKey(isEditingValue, isEditingEnum);
        return;
    }
    if (ev->vkey == VK_TAB) {
        ev->didHandle = HandleTabKey(isEditingValue, isEditingEnum);
        return;
    }
    if (ev->vkey == VK_RETURN) {
        ev->didHandle = HandleEnterKey(isEditingValue, isEditingEnum);
        return;
    }
    if (HandleUpDownKey(*ev)) {
        ev->didHandle = true;
    }
}

// clicking the window's close box sends WM_CLOSE. We must schedule our own
// teardown here: the framework's default WM_CLOSE handler calls WindowBase::Destroy(),
// which removes the WindowBase from the hwnd->WindowBase list *before* DestroyWindow(), so the
// following WM_DESTROY never reaches onDestroy - leaving gAdvancedSettingsWnd
// dangling and blocking reopen. CloseEvent::didHandle defaults to true, so
// returning from here skips that default Destroy().
static void OnClose(WindowBase::CloseEvent* /*ev*/) {
    if (gAdvancedSettingsWnd) {
        gAdvancedSettingsWnd->ScheduleDelete();
    }
}

static void OnDestroy(WindowBase::DestroyEvent* /*ev*/) {
    if (gAdvancedSettingsWnd) {
        gAdvancedSettingsWnd->ScheduleDelete();
    }
}

// last client size chosen by the user (or initial layout); reused when the
// dialog is reopened in the same process so a widened window sticks (#5804)
static int gAdvSettingsLastClientDx = 0;
static int gAdvSettingsLastClientDy = 0;

// re-layout the controls when the (resizable) window is resized
void AdvancedSettingsWnd::OnSize(WindowBase::SizeEvent* ev) {
    // a WS_CAPTION/WS_THICKFRAME window gets WM_SIZE during CreateCustom,
    // before the child controls exist; ignore layout until they're created
    if (!layout || !listBox) {
        return;
    }
    int dx = ev->size.dx;
    int dy = ev->size.dy;
    if (dx == 0 || dy == 0) {
        return;
    }
    gAdvSettingsLastClientDx = dx;
    gAdvSettingsLastClientDy = dy;
    // in-place editors are positioned over a specific item rect; that rect
    // moves on resize, so close them
    CancelEditValue();
    DoLayout({dx, dy});
    HwndInvalidate(hwnd);
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

// the buttons are virtual controls, so they are styled here rather than by the
// system: a filled box with a border, brighter on hover (like the other dialogs)
bool AdvancedSettingsWnd::Create(MainWindow* mainWin) {
    win = mainWin;
    // OnSize closes in-place editors before DoLayout; skip the generic path
    autoLayout = false;
    CollectSettings(items, &gGlobalPrefsInfo, (u8*)gGlobalPrefs, {});

    {
        CreateCustomArgs args;
        args.title = _TRA("Advanced Settings");
        args.visible = false;
        args.style = WS_POPUPWINDOW | WS_CAPTION | WS_THICKFRAME;
        args.font = GetFont();
        args.icon = LoadIconW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(GetAppIconID()));
        CreateCustom(args);
    }
    if (!hwnd) {
        return false;
    }
    fontBold = GetBoldPlatformFont(GetFont() ? GetFont() : GetAppFont());

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
        args.font = GetFont();
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
        auto* c = new VirtListBox();
        c->dpi = GetDpi();
        c->font = font;
        c->padding = DpiScaledInsets(4, 0);
        c->onDrawItem =
            MkMethod1<AdvancedSettingsWnd, VirtListBox::DrawItemEvent*, &AdvancedSettingsWnd::DrawListBoxItem>(this);
        listBox = c;
        model = new ListBoxModelSettings();
        model->items = &items;
        CollectFilteredSettings(items, filterWords, model->filtered);
        c->onSelectionChanged = MkMethod0<AdvancedSettingsWnd, &AdvancedSettingsWnd::OnSelectionChanged>(this);
        c->onDoubleClick = MkMethod0<AdvancedSettingsWnd, &AdvancedSettingsWnd::OnItemDoubleClicked>(this);
        c->SetModel(model);
        vbox->AddChild(c, 1);
    }

    // text area showing the selected setting's doc comment, between the list
    // and the buttons
    {
        auto* c = new CommentText();
        c->font = font;
        // don't let the doc text touch the edges of the dialog
        c->padding = DpiScaledInsets(4);
        commentText = c;
        vbox->AddChild(new Padding(c, DpiScaledInsets(4, 2)));
    }

    // centered hints, above the buttons: how to edit a setting and what the
    // bold text in the list means
    {
        Str hints[] = {
            _TRA("Enter or double-click to edit"),
            _TRA("Value bold? Value is different from default"),
            _TRA("Name bold? Value was changed but unsaved"),
        };
        for (const Str& hint : hints) {
            auto* hbox = new HBox();
            hbox->alignMain = MainAxisAlign::MainCenter;
            hbox->alignCross = CrossAxisAlign::CrossCenter;

            auto* c = NewVirtText({.s = hint, .font = font, .isRtl = isRtl});
            hbox->AddChild(new Padding(c, DpiScaledInsets(1, 8)));
            vbox->AddChild(hbox);
        }
    }

    {
        // left: Save, Cancel, Open Settings File — right: Help
        auto* hbox = new HBox();
        hbox->alignMain = MainAxisAlign::SpaceBetween;
        hbox->alignCross = CrossAxisAlign::CrossCenter;
        hbox->gap = font->averageCharWidth;
        auto pad = Insets{4, 0, 4, 0};

        auto* left = new HBox();
        left->alignMain = MainAxisAlign::MainStart;
        left->alignCross = CrossAxisAlign::CrossCenter;
        left->gap = font->averageCharWidth;
        btnSave = NewThemedButton(hwnd, _TRA("Save"), font, true);
        btnSave->onClick = MkMethod1<AdvancedSettingsWnd, VirtMouseEvent*, &AdvancedSettingsWnd::OnSave>(this);
        left->AddChild(new Padding(btnSave, pad));
        btnCancel = NewThemedButton(hwnd, _TRA("Cancel"), font, false);
        btnCancel->onClick = MkMethod1<AdvancedSettingsWnd, VirtMouseEvent*, &AdvancedSettingsWnd::OnCancel>(this);
        left->AddChild(new Padding(btnCancel, pad));
        btnOpenSettingsFile = NewThemedButton(hwnd, _TRA("Open Settings File"), font, false);
        btnOpenSettingsFile->onClick =
            MkMethod1<AdvancedSettingsWnd, VirtMouseEvent*, &AdvancedSettingsWnd::OnOpenSettingsFile>(this);
        left->AddChild(new Padding(btnOpenSettingsFile, pad));
        hbox->AddChild(left);

        btnHelp = NewThemedButton(hwnd, _TRA("Help"), font, false);
        btnHelp->onClick = MkMethod1<AdvancedSettingsWnd, VirtMouseEvent*, &AdvancedSettingsWnd::OnHelp>(this);
        hbox->AddChild(new Padding(btnHelp, pad));
        vbox->AddChild(hbox);
    }

    DarkModeApplyToPopupWindow(hwnd);

    auto* padding = new Padding(vbox, DpiScaledInsets(4, 8));
    layout = padding;

    auto rc = HwndClientRect(win->hwndFrame);
    // Default is wide enough that long setting names (e.g. InverseSearchCmdLine)
    // and long values don't crowd each other; reuse last size if the user
    // resized earlier this session (#5804).
    int dy = gAdvSettingsLastClientDy > 0 ? gAdvSettingsLastClientDy : limitValue(rc.dy - 72, 480, 900);
    int dx = gAdvSettingsLastClientDx > 0 ? gAdvSettingsLastClientDx : limitValue(rc.dx - 128, 760, 1100);
    LayoutAndSizeToContent(layout, dx, dy, hwnd);
    // pick up the virtual controls so we paint them and they get their input
    DoLayout(HwndClientRect(hwnd).Size());
    gAdvSettingsLastClientDx = HwndClientRect(hwnd).dx;
    gAdvSettingsLastClientDy = HwndClientRect(hwnd).dy;
    PositionDialog(hwnd, win->hwndFrame);

    SetIsVisible(true);
    HwndSetFocus(editFilter->hwnd);
    return true;
}

struct AdvSettingsRowRec {
    int idx;
    Rect rect;
};

// set only while the probe below is painting into its scratch bitmap
static Vec<AdvSettingsRowRec>* gAdvSettingsRowRec = nullptr;

static void RecordAdvSettingsRow(VirtListBox::DrawItemEvent* ev) {
    if (gAdvSettingsRowRec) {
        gAdvSettingsRowRec->Append({ev->itemIndex, ev->itemRect});
    }
}

// The rects the list hands its row drawer. Asking VirtListBox for them
// (ItemRect) would report what the layout intends, not what the paint does --
// and the bug this is here for was a paint that disagreed with the layout. So
// the real tree is painted into a scratch bitmap with the row drawer swapped
// for a recorder.
static void RecordAdvSettingsRows(AdvancedSettingsWnd* wnd, Vec<AdvSettingsRowRec>& out) {
    Rect client = HwndClientRect(wnd->hwnd);
    if (client.IsEmpty() || !wnd->vroot) {
        return;
    }
    HDC hdcWin = GetDC(wnd->hwnd);
    if (!hdcWin) {
        return;
    }
    HDC memDC = CreateCompatibleDC(hdcWin);
    HBITMAP bmp = CreateCompatibleBitmap(hdcWin, client.dx, client.dy);
    if (memDC && bmp) {
        HGDIOBJ oldBmp = SelectObject(memDC, bmp);
        auto prevDrawItem = wnd->listBox->onDrawItem;
        gAdvSettingsRowRec = &out;
        wnd->listBox->onDrawItem = MkFunc1Void<VirtListBox::DrawItemEvent*>(RecordAdvSettingsRow);
        {
            Gfx* gfx = GfxCreate(memDC);
            wnd->vroot->Paint(gfx, client);
            delete gfx;
        }
        wnd->listBox->onDrawItem = prevDrawItem;
        gAdvSettingsRowRec = nullptr;
        SelectObject(memDC, oldBmp);
    }
    if (bmp) {
        DeleteObject(bmp);
    }
    if (memDC) {
        DeleteDC(memDC);
    }
    ReleaseDC(wnd->hwnd, hdcWin);
}

// What the settings list actually draws: the list's content rect in dialog
// client coords, the row height, and the rect of every row the paint touched.
// `action` is "geom" to report, or "scroll" to first scroll by `arg` rows. Rows
// are always whole and always tile the usable viewport; a test can check that
// on the real pixels without hard-coding a layout that depends on window size,
// dpi and theme. Used by tests/issue-5882.ts.
TempStr AdvSettingsRowsResultTemp(Str action, int arg, int* exitCodeOut) {
    str::Builder out;
    auto finish = [&](int code) -> TempStr {
        if (exitCodeOut) {
            *exitCodeOut = code;
        }
        return ToStrTemp(out);
    };

    AdvancedSettingsWnd* wnd = gAdvancedSettingsWnd;
    if (!wnd || !wnd->hwnd || !wnd->listBox) {
        out.Append("NOTREADY no-dialog\n");
        return finish(2);
    }
    VirtListBox* lb = wnd->listBox;
    if (lb->bounds.IsEmpty()) {
        out.Append("NOTREADY no-layout\n");
        return finish(2);
    }
    int itemDy = lb->GetItemHeight();
    if (str::Eq(action, "scroll")) {
        lb->ScrollBy(arg * itemDy);
        HwndRepaintNow(wnd->hwnd);
    } else if (!str::Eq(action, "geom")) {
        out.Append(fmt("ERROR unknown-action action=%s\n", action));
        return finish(1);
    }

    // the content rect is where the rows live; UsableDy() rounds it down to
    // whole rows, so the strip below the last one is background and must stay
    // that way however far the list is scrolled
    Rect cr = lb->ContentRectInWindow();
    out.Append(fmt("OK content=%d,%d,%d,%d itemDy=%d usableDy=%d scrollY=%d maxScrollY=%d items=%d\n", cr.x, cr.y,
                   cr.dx, cr.dy, itemDy, lb->UsableDy(), lb->scrollY, lb->MaxScrollY(), lb->ItemsCount()));
    Vec<AdvSettingsRowRec> drawn;
    RecordAdvSettingsRows(wnd, drawn);
    for (const AdvSettingsRowRec& rec : drawn) {
        Rect r = rec.rect;
        out.Append(fmt("row=%d rect=%d,%d,%d,%d\n", rec.idx, r.x, r.y, r.dx, r.dy));
    }
    return finish(0);
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
    wnd->onBeforeDelete = MkFunc0Void(ClearAdvancedSettingsWnd);
    wnd->onClose = MkFunc1Void<WindowBase::CloseEvent*>(OnClose);
    wnd->onDestroy = MkFunc1Void<WindowBase::DestroyEvent*>(OnDestroy);
    wnd->onSize = MkMethod1<AdvancedSettingsWnd, WindowBase::SizeEvent*, &AdvancedSettingsWnd::OnSize>(wnd);
    wnd->onKeyDown = MkMethod1<AdvancedSettingsWnd, KeyEvent*, &AdvancedSettingsWnd::OnKeyDown>(wnd);
    wnd->SetFont(GetAppFont());
    bool ok = wnd->Create(win);
    if (!ok) {
        delete wnd;
        return;
    }
    gAdvancedSettingsWnd = wnd;
}
