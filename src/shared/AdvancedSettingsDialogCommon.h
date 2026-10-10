/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// --- shared by AdvancedSettingsDialogCommon.cpp and each app's AdvancedSettingsDialog.cpp ---

// a single editable setting; the pending (possibly edited) value is kept
// here and only written back on Save. The field is addressed by its offset
// into gSettings, not a pointer: a settings reload (file watcher, Save)
// frees and re-creates gSettings while the dialog stays open
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

void ApplyCompactInts(const StructInfo* info, u8* base, Str s);
bool SettingDiffersFromDefault(SettingItem* item);
TempStr FormatSettingValueTemp(SettingItem* item);
TempStr FormatSettingDefaultTemp(SettingItem* item);
void PreviewSettingChange(SettingItem* item);
void EndPreviewSettingChange();
void SetItemChanged(SettingItem* item);
void CollectSettings(Vec<SettingItem*>& items);
void CollectFilteredSettings(Vec<SettingItem*>& items, const StrVec& words, Vec<int>& filtered);
