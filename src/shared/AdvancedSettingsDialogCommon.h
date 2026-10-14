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
    int sectionIdx = -1;               // the struct the setting is in ("FixedPageUI"), -1 at the top level
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

// the settings of one struct ("FixedPageUI.*"), listed under a header row
struct SettingSection {
    Str name;    // the struct's dotted path, owned
    Str comment; // its doc comment, owned

    ~SettingSection() {
        str::Free(name);
        str::Free(comment);
    }
};

// a row of the filtered list: a setting, or the header of the section the
// rows under it belong to
struct SettingRow {
    int itemIdx = -1;      // index into the items; -1 for a header
    int sectionIdx = -1;   // header: the section
    bool leafName = false; // a setting under its header is shown by its leaf name
};

// what the footer's key hints are for
enum {
    kAdvEditNone,
    kAdvEditValue,
    kAdvEditEnum,
};

// one "Enter change" hint in the footer, like the palette's
struct AdvHint {
    Str key;
    Str desc;
};
constexpr int kAdvMaxHints = 2;

void ApplyCompactInts(const StructInfo* info, u8* base, Str s);
bool SettingDiffersFromDefault(SettingItem* item);
TempStr FormatSettingValueTemp(SettingItem* item);
TempStr FormatSettingDefaultTemp(SettingItem* item);
void PreviewSettingChange(SettingItem* item);
void EndPreviewSettingChange();
void SetItemChanged(SettingItem* item);
void RestoreSettingDefault(SettingItem* item);
void CollectSettings(Vec<SettingItem*>& items, Vec<SettingSection*>& sections);
void CollectFilteredSettings(Vec<SettingItem*>& items, const StrVec& words, Vec<SettingRow>& rows);
int CountSettingRows(const Vec<SettingRow>& rows);
int FindSettingRow(const Vec<SettingRow>& rows, int itemIdx);
int NextSettingRow(const Vec<SettingRow>& rows, int row, int dir);
Str SettingLeafName(Str path);
Str SettingRowName(Vec<SettingItem*>& items, const SettingRow& row);
TempStr AdvSettingsCountTemp(int shown, int total);
TempStr AdvSettingsDefaultTemp(SettingItem* item);
int AdvSettingsHints(int editState, AdvHint* out);

constexpr const char* kSettingsDocsUrl = "https://www.sumatrapdfreader.org/settings/settings3-7.html";

// smallest client width the user can drag the dialog to (96 dpi pixels)
constexpr int kAdvSettingsMinClientDx = 480;

// the key that restores a setting's default, for the help line
#if OS_DARWIN
#define kRestoreDefaultKeyName "Cmd+Backspace"
#else
#define kRestoreDefaultKeyName "Del"
#endif
