/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/UITask.h"
#include "base/SettingsUtil.h"
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
#include "FilterHighlightDraw.h"
#include "SumatraDialogs.h"
#include "AdvancedSettingsDialogCommon.h"

// "Enter change" => "Enter" and "change": the key is the first word
static AdvHint SplitAdvHint(Str s) {
    int keyLen = 0;
    while (keyLen < len(s) && s.s[keyLen] != ' ') {
        keyLen++;
    }
    AdvHint h;
    h.key = Str(s.s, keyLen);
    h.desc = keyLen < len(s) ? Str(s.s + keyLen + 1, len(s) - keyLen - 1) : Str();
    return h;
}

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

void ApplyCompactInts(const StructInfo* info, u8* base, Str s) {
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
bool SettingDiffersFromDefault(SettingItem* item) {
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
TempStr FormatSettingValueTemp(SettingItem* item) {
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

TempStr FormatSettingDefaultTemp(SettingItem* item) {
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

// Show what a setting does before it's saved: some settings change how pages
// are rendered, and picking a value from a list is guesswork without seeing it.
// The value is previewed without touching gSettings, so Cancel (or closing
// the dialog) puts the saved value back - see EndPreviewSettingChange().
void SetItemChanged(SettingItem* item);

void PreviewSettingChange(SettingItem* item) {
    if (!str::EqI(item->name, StrL("DocumentColorsFollowTheme"))) {
        return;
    }
    SetDocumentColorsFollowThemePreview(DocumentColorsFollowThemeFromString(item->strVal));
    UpdateDocumentColors();
}

// drop every preview and render with the saved settings again
void EndPreviewSettingChange() {
    ClearDocumentColorsFollowThemePreview();
    UpdateDocumentColors();
}

// the pending value goes back to the default from the settings metadata
void RestoreSettingDefault(SettingItem* item) {
    switch (item->type) {
        case SettingType::Bool:
            item->boolVal = item->defBool;
            break;
        case SettingType::Int:
            item->intVal = item->defInt;
            break;
        case SettingType::Float:
            item->floatVal = item->defFloat;
            break;
        default:
            str::ReplaceWithCopy(&item->strVal, FormatSettingDefaultTemp(item));
            break;
    }
    SetItemChanged(item);
    PreviewSettingChange(item);
}

void SetItemChanged(SettingItem* item) {
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

// the doc comment of the struct at `path` ("FixedPageUI.PageGrid"), from the
// metadata of the struct that holds it
static Str StructCommentAtPath(const StructInfo* info, Str prefix, Str path) {
    const char* fieldName = info->fieldNames;
    const char* fieldComment = info->fieldComments;
    for (u16 i = 0; i < info->fieldCount; i++) {
        const FieldInfo& field = info->fields[i];
        Str fname(fieldName);
        fieldName += len(fname) + 1;
        Str comment;
        if (fieldComment) {
            comment = Str(fieldComment);
            fieldComment += len(comment) + 1;
        }
        if (field.type != SettingType::Struct) {
            continue;
        }
        TempStr fpath = len(prefix) > 0 ? fmt("%s.%s", prefix, fname) : str::DupTemp(fname);
        if (str::Eq(fpath, path)) {
            return comment;
        }
        if (str::StartsWith(path, fpath) && path.s[len(fpath)] == '.') {
            return StructCommentAtPath((const StructInfo*)field.value, fpath, path);
        }
    }
    return {};
}

// the index of the last '.' in a dotted path, -1 at the top level
static int LastDotIdx(Str path) {
    for (int i = len(path) - 1; i >= 0; i--) {
        if (path.s[i] == '.') {
            return i;
        }
    }
    return -1;
}

// "FixedPageUI.TextColor" => "TextColor"
Str SettingLeafName(Str path) {
    int n = LastDotIdx(path) + 1;
    return Str(path.s + n, len(path) - n);
}

// the section a dotted path is in, added to `sections` on first sight
static int SectionIdxForPath(Vec<SettingSection*>& sections, Str path) {
    int dot = LastDotIdx(path);
    if (dot < 0) {
        return -1;
    }
    Str name(path.s, dot);
    for (int i = 0; i < len(sections); i++) {
        if (str::Eq(sections[i]->name, name)) {
            return i;
        }
    }
    auto* sec = new SettingSection();
    sec->name = str::Dup(name);
    sec->comment = str::Dup(StructCommentAtPath(&gSettingsInfo, {}, name));
    VecAppend(sections, sec);
    return len(sections) - 1;
}

// one dialog item per editable setting, seeded with the current value
void CollectSettings(Vec<SettingItem*>& items, Vec<SettingSection*>& sections) {
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
            item->sectionIdx = SectionIdxForPath(sections, sf.path);
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
        item->sectionIdx = SectionIdxForPath(sections, sf.path);
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
                item->enumValues = field.enumValues;
                break;
        }
        VecAppend(items, item);
    }
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

static void AppendSettingRow(Vec<SettingRow>& rows, int itemIdx, int sectionIdx, bool leafName) {
    SettingRow row;
    row.itemIdx = itemIdx;
    row.sectionIdx = sectionIdx;
    row.leafName = leafName;
    VecAppend(rows, row);
}

// Customized settings come first, by their full names, so the values a user
// is most likely to review are at the top. The rest keep the metadata order
// within each section: the top-level settings, then each struct's under a
// header row.
void CollectFilteredSettings(Vec<SettingItem*>& items, const StrVec& words, Vec<SettingRow>& rows) {
    VecReset(rows);
    int n = len(items);
    auto matches = [&](int i, bool nonDefault, int sectionIdx) {
        SettingItem* item = items[i];
        if (SettingDiffersFromDefault(item) != nonDefault || item->sectionIdx != sectionIdx) {
            return false;
        }
        return SettingNameMatchesFilter(item->name, words);
    };
    int nSections = 0;
    for (int i = 0; i < n; i++) {
        nSections = std::max(nSections, items[i]->sectionIdx + 1);
    }
    for (int sec = -1; sec < nSections; sec++) {
        for (int i = 0; i < n; i++) {
            if (matches(i, true, sec)) {
                AppendSettingRow(rows, i, sec, false);
            }
        }
    }
    for (int sec = -1; sec < nSections; sec++) {
        bool hasHeader = sec < 0;
        for (int i = 0; i < n; i++) {
            if (!matches(i, false, sec)) {
                continue;
            }
            if (!hasHeader) {
                AppendSettingRow(rows, -1, sec, false);
                hasHeader = true;
            }
            AppendSettingRow(rows, i, sec, true);
        }
    }
}

// the settings among the rows, without the headers
int CountSettingRows(const Vec<SettingRow>& rows) {
    int n = 0;
    for (const SettingRow& row : rows) {
        n += row.itemIdx >= 0;
    }
    return n;
}

int FindSettingRow(const Vec<SettingRow>& rows, int itemIdx) {
    for (int i = 0; i < len(rows); i++) {
        if (rows[i].itemIdx == itemIdx) {
            return i;
        }
    }
    return -1;
}

// the nearest setting row from `row` in direction `dir`, skipping headers;
// -1 when there is none that way
int NextSettingRow(const Vec<SettingRow>& rows, int row, int dir) {
    int n = len(rows);
    for (int i = row + dir; i >= 0 && i < n; i += dir) {
        if (rows[i].itemIdx >= 0) {
            return i;
        }
    }
    return -1;
}

Str SettingRowName(Vec<SettingItem*>& items, const SettingRow& row) {
    Str name = items[row.itemIdx]->name;
    return row.leafName ? SettingLeafName(name) : name;
}

// "212 settings", or "14 of 212" while a filter is typed
TempStr AdvSettingsCountTemp(int shown, int total) {
    if (shown >= total) {
        return fmt(Tr("%d settings").s, total);
    }
    return fmt(Tr("%d of %d").s, shown, total);
}

// "default: fit page" in the help strip
TempStr AdvSettingsDefaultTemp(SettingItem* item) {
    TempStr def = FormatSettingDefaultTemp(item);
    if (len(def) == 0) {
        def = str::DupTemp(Tr("empty"));
    }
    return fmt(Tr("default: %s").s, def);
}

// the footer's key hints for what the keys do now
int AdvSettingsHints(int editState, AdvHint* out) {
    int n = 0;
    switch (editState) {
        case kAdvEditValue:
            out[n++] = SplitAdvHint(Tr("Enter confirm"));
            out[n++] = SplitAdvHint(Tr("Esc cancel"));
            break;
        case kAdvEditEnum:
            out[n++] = SplitAdvHint(Tr("Enter select"));
            out[n++] = SplitAdvHint(Tr("Esc cancel"));
            break;
        default:
            out[n++] = SplitAdvHint(Tr("Enter change"));
            out[n++] = SplitAdvHint(fmt(Tr("%s restore default").s, StrL(kRestoreDefaultKeyName)));
            break;
    }
    return n;
}
