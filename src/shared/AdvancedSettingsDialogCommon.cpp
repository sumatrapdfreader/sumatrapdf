/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/UITask.h"
#include "base/SettingsUtil.h"
#include "gui/UIModels.h"
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

// one dialog item per editable setting, seeded with the current value
void CollectSettings(Vec<SettingItem*>& items) {
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
void CollectFilteredSettings(Vec<SettingItem*>& items, const StrVec& words, Vec<int>& filtered) {
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
