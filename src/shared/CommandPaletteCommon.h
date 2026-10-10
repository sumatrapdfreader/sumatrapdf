/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// --- shared by CommandPaletteCommon.cpp and each app's CommandPalette.cpp ---

// which help text the palette shows under the list
enum {
    kHelpNone = -1,
    kHelpSmartTab,
    kHelpCommands,
    kHelpHistory,
    kHelpTabs,
    kHelpFavorites,
    kHelpAnnotations,
    kHelpSettings,
    kHelpSettingValue,
    kHelpToc,
    kHelpEverything,
    kHelpThumbnails,
};

// separates a setting from the value being typed for it in the "= settings"
// query, e.g. "=ZoomIncrement = 25". A setting name never contains one
constexpr const char* kPaletteSettingValueSep = "=";
struct ItemDataCP {
    i32 cmdId = 0;
    // a "Debug: ..." command; those are listed after all the others
    bool isDebug = false;
    WindowTab* tab = nullptr;
    Str filePath;
    TocItem* tocItem = nullptr;
    int indent = 0;
    int pageNo = 0; // toc entry destination page (0 if none), shown in the list
    FileState* favFs = nullptr;
    Favorite* fav = nullptr;
    Annotation* annot = nullptr;
    // a "= settings" row. In the setting-picking stage the row text is the
    // setting's dotted path; in the value-picking stage it is a candidate value
    // and settingPath names the setting it belongs to.
    SettingType settingType = SettingType::Comment; // Comment: not a setting row
    int settingOffset = 0;                          // into gSettings, see SettingFieldPtr()
    intptr_t settingDefault = 0;                    // FieldInfo::value, decoded per type
    Str settingPath;
    Str settingComment; // its doc comment, from the settings metadata
};
using StrVecCP = StrVecWithData<ItemDataCP>;
bool IsSettingRow(const ItemDataCP* d);
const u8* SettingRowPtr(const ItemDataCP* d);
bool IsCmdInList(i32 cmdId, i32* ids);
Str CommandPaletteSkipWS(Str s);
int PaletteHelpKind(Str filter, bool smartTab);
bool AllowCommand(const AppCommandCtx& ctx, i32 cmdId);
TempStr ConvertPathForDisplayTemp(Str s);
TempStr RemovePrefixFromString(Str s);
void CollectTocRec(StrVecCP& toc, TocItem* ti, int indent, int currPageNo, int& bestIdx, int& bestPageNo);
void AppendFavoritesForFile(StrVecCP& favorites, FileState* fs, bool isCurrent);
TempStr FormatSettingValueTemp(SettingType type, const u8* p);
bool SettingDiffersFromDefault(const ItemDataCP* d);
void CollectSettingRows(StrVecCP& out);
TempStr CommandPaletteShortcutTemp(i32 cmdId);
void FilterStrings(StrVecCP& strs, const StrVec& words, StrVecCP& matchedOut);
bool SplitSettingValueQuery(Str query, Str& path, Str& value);
