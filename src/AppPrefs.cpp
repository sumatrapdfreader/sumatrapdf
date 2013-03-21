/* Copyright 2013 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "BaseUtil.h"
#define INCLUDE_APPPREFS2_METADATA
#include "AppPrefs.h"
#include "DisplayState.h"

#include "AppTools.h"
#include "BencUtil.h"
#include "DebugLog.h"
#include "Favorites.h"
#include "FileHistory.h"
#include "FileTransactions.h"
#include "FileUtil.h"
#include "IniParser.h"
#include "SumatraPDF.h"
#include "Translations.h"
#include "WindowInfo.h"
#include "WinUtil.h"

#define PREFS_FILE_NAME         L"sumatrapdfprefs.dat"
#define NEW_PREFS_FILE_NAME     L"SumatraPDF.ini"
#define USER_PREFS_FILE_NAME    L"SumatraPDF-user.ini"

#define MAX_REMEMBERED_FILES 1000

SerializableGlobalPrefs gGlobalPrefs;
AdvancedSettings gUserPrefs;

static BencDict *SerializeStructBenc(SettingInfo *info, size_t count, const void *structBase, BencDict *prefs=NULL, uint32_t bitmask=-1)
{
    if (!prefs)
        prefs = new BencDict();
    const char *base = (const char *)structBase;
    for (size_t i = 0; i < count; i++) {
        SettingInfo& meta = info[i];
        if ((meta.flags & bitmask) != meta.flags)
            continue;
        switch (meta.type) {
        case Type_Bool:
            // TODO: always persist all values?
            if (!(meta.flags & 1) || *(bool *)(base + meta.offset))
                prefs->Add(meta.name, (int64_t)*(bool *)(base + meta.offset));
            else
                delete prefs->Remove(meta.name);
            break;
        case Type_Color:
            prefs->Add(meta.name, (int64_t)*(COLORREF *)(base + meta.offset));
            break;
        case Type_FileTime:
            prefs->AddRaw(meta.name, ScopedMem<char>(_MemToHex((FILETIME *)(base + meta.offset))));
            break;
        case Type_Float:
            prefs->AddRaw(meta.name, ScopedMem<char>(str::Format("%.4f", *(float *)(base + meta.offset))));
            break;
        case Type_Int:
            prefs->Add(meta.name, (int64_t)*(int *)(base + meta.offset));
            break;
        case Type_String:
            if (((ScopedMem<WCHAR> *)(base + meta.offset))->Get())
                prefs->Add(meta.name, ((ScopedMem<WCHAR> *)(base + meta.offset))->Get());
            else
                delete prefs->Remove(meta.name);
            break;
        case Type_Utf8String:
            if (((ScopedMem<char> *)(base + meta.offset))->Get())
                prefs->AddRaw(meta.name, ((ScopedMem<char> *)(base + meta.offset))->Get());
            else
                delete prefs->Remove(meta.name);
            break;
        case Type_Custom:
            if (str::Eq(meta.name, "Display Mode"))
                prefs->Add(meta.name, DisplayModeConv::NameFromEnum(*(DisplayMode *)(base + meta.offset)));
            else if (str::Eq(meta.name, "UILanguage"))
                prefs->AddRaw(meta.name, *(const char **)(base + meta.offset));
            else if (str::Eq(meta.name, "TocToggles")) {
                Vec<int> *intVec = *(Vec<int> **)(base + meta.offset);
                if (intVec) {
                    BencArray *array = new BencArray();
                    for (size_t idx = 0; idx < intVec->Count(); idx++) {
                        array->Add(intVec->At(idx));
                    }
                    prefs->Add(meta.name, array);
                }
                else
                    delete prefs->Remove(meta.name);
            }
            break;
        default:
            CrashIf(true);
        }
    }
    return prefs;
}

static void DeserializeStructBenc(SettingInfo *info, size_t count, void *structBase, BencDict *prefs)
{
    char *base = (char *)structBase;
    BencInt *intObj;
    BencString *strObj;
    BencArray *arrObj;

    for (size_t i = 0; i < count; i++) {
        SettingInfo& meta = info[i];
        switch (meta.type) {
        case Type_Bool:
            if ((intObj = prefs->GetInt(meta.name)))
                *(bool *)(base + meta.offset) = intObj->Value() != 0;
            break;
        case Type_Color:
            if ((intObj = prefs->GetInt(meta.name)))
                *(COLORREF *)(base + meta.offset) = (COLORREF)intObj->Value();
            break;
        case Type_FileTime:
            if ((strObj = prefs->GetString(meta.name))) {
                FILETIME ft;
                if (_HexToMem(strObj->RawValue(), &ft))
                    *(FILETIME *)(base + meta.offset) = ft;
            }
            break;
        case Type_Float:
            if ((strObj = prefs->GetString(meta.name))) {
                // note: this might round the value for files produced with versions
                //       prior to 1.6 and on a system where the decimal mark isn't a '.'
                //       (the difference should be hardly notable, though)
                *(float *)(base + meta.offset) = (float)atof(strObj->RawValue());
            }
            break;
        case Type_Int:
            if ((intObj = prefs->GetInt(meta.name)))
                *(int *)(base + meta.offset) = (int)intObj->Value();
            break;
        case Type_String:
            if ((strObj = prefs->GetString(meta.name))) {
                ScopedMem<WCHAR> str(strObj->Value());
                if (str)
                    ((ScopedMem<WCHAR> *)(base + meta.offset))->Set(str.StealData());
            }
            break;
        case Type_Utf8String:
            if ((strObj = prefs->GetString(meta.name)))
                ((ScopedMem<char> *)(base + meta.offset))->Set(str::Dup(strObj->RawValue()));
            break;
        case Type_Custom:
            if (str::Eq(meta.name, "Display Mode") && (strObj = prefs->GetString(meta.name))) {
                ScopedMem<WCHAR> mode(strObj->Value());
                if (mode)
                    DisplayModeConv::EnumFromName(mode, (DisplayMode *)(base + meta.offset));
            }
            else if (str::Eq(meta.name, "UILanguage") && (strObj = prefs->GetString(meta.name))) {
                // ensure language code is valid
                const char *langCode = trans::ValidateLangCode(strObj->RawValue());
                if (langCode)
                    *(const char **)(base + meta.offset) = langCode;
            }
            else if (str::Eq(meta.name, "TocToggles") && (arrObj = prefs->GetArray(meta.name))) {
                size_t len = arrObj->Length();
                Vec<int> *intVec = new Vec<int>(len);
                if (intVec) {
                    for (size_t idx = 0; idx < len; idx++) {
                        if ((intObj = arrObj->GetInt(idx)))
                            intVec->Append((int)intObj->Value());
                    }
                    delete *(Vec<int> **)(base + meta.offset);
                    *(Vec<int> **)(base + meta.offset) = intVec;
                }
            }
            break;
        default:
            CrashIf(true);
        }
    }
}

#define GLOBAL_PREFS_STR            "gp"
#define FILE_HISTORY_STR            "File History"
#define FAVS_STR                    "Favorites"

// number of weeks past since 2011-01-01
static int GetWeekCount()
{
    SYSTEMTIME date20110101 = { 0 };
    date20110101.wYear = 2011; date20110101.wMonth = 1; date20110101.wDay = 1;
    FILETIME origTime, currTime;
    SystemTimeToFileTime(&date20110101, &origTime);
    GetSystemTimeAsFileTime(&currTime);
    return (currTime.dwHighDateTime - origTime.dwHighDateTime) / 1408;
    // 1408 == (10 * 1000 * 1000 * 60 * 60 * 24 * 7) / (1 << 32)
}

static BencDict* SerializeGlobalPrefs(SerializableGlobalPrefs& globalPrefs)
{
    CrashIf(!IsValidZoom(globalPrefs.defaultZoom));
    if (!globalPrefs.openCountWeek)
        globalPrefs.openCountWeek = GetWeekCount();

    BencDict *prevDict = NULL;
    if (globalPrefs.prevSerialization)
        prevDict = BencDict::Decode(globalPrefs.prevSerialization, NULL);
    return SerializeStructBenc(gSerializableGlobalPrefsInfo, dimof(gSerializableGlobalPrefsInfo), &globalPrefs, prevDict);
}

static BencDict *DisplayState_Serialize(DisplayState *ds, bool globalPrefsOnly)
{
    if (ds->isMissing && (globalPrefsOnly || ds->useGlobalValues) &&
        !ds->decryptionKey && !ds->isPinned) {
        // forget about missing documents without valuable state
        return NULL;
    }

    // BUG: 2140
    if (!IsValidZoom(ds->zoomVirtual)) {
        dbglog::CrashLogF("Invalid ds->zoomVirtual: %.4f", ds->zoomVirtual);
        const WCHAR *ext = path::GetExt(ds->filePath);
        if (!str::IsEmpty(ext)) {
            ScopedMem<char> extA(str::conv::ToUtf8(ext));
            dbglog::CrashLogF("File type: %s", extA.Get());
        }
        dbglog::CrashLogF("DisplayMode: %d", ds->displayMode);
        dbglog::CrashLogF("PageNo: %d", ds->pageNo);
        CrashIf(true);
    }

    // don't include common values in order to keep the preference file size down
    uint32_t bitmask = globalPrefsOnly || ds->useGlobalValues ? 1 : 3;
    return SerializeStructBenc(gDisplayStateInfo, dimof(gDisplayStateInfo), ds, NULL, bitmask);
}

static BencArray *SerializeFileHistory(FileHistory& fileHistory, bool globalPrefsOnly)
{
    BencArray *arr = new BencArray();

    // Don't save more file entries than will be useful
    int minOpenCount = 0;
    if (globalPrefsOnly) {
        Vec<DisplayState *> frequencyList;
        fileHistory.GetFrequencyOrder(frequencyList);
        if (frequencyList.Count() > FILE_HISTORY_MAX_RECENT)
            minOpenCount = frequencyList.At(FILE_HISTORY_MAX_FREQUENT)->openCount / 2;
    }

    DisplayState *state;
    for (int index = 0; (state = fileHistory.Get(index)); index++) {
        // never forget pinned documents and documents we've remembered a password for
        bool forceSave = state->isPinned || state->decryptionKey != NULL;
        if (index >= MAX_REMEMBERED_FILES && !forceSave)
            continue;
        if (state->openCount < minOpenCount && index > FILE_HISTORY_MAX_RECENT && !forceSave)
            continue;
        BencDict *obj = DisplayState_Serialize(state, globalPrefsOnly);
        if (obj)
            arr->Add(obj);
    }

    return arr;
}

static inline const WCHAR *NullToEmpty(const WCHAR *s)
{
    return s == NULL ? L"" : s;
}

static inline const WCHAR *EmptyToNull(const WCHAR *s)
{
    return str::IsEmpty(s) ? NULL : s;
}

static BencArray *SerializeFavData(FileFavs *fav)
{
    BencArray *res = new BencArray();
    for (size_t i = 0; i < fav->favNames.Count(); i++) {
        FavName *fn = fav->favNames.At(i);
        res->Add(fn->pageNo);
        res->Add(NullToEmpty(fn->name));
    }
    return res;
}

// for simplicity, favorites are serialized as an array. Element 2*i is
// a name of the file, for favorite i, element 2*i+1 is an array of
// page number integer/name string pairs
// TODO: rework this serialization so that FavName::pageLabel can also be persisted
static BencArray *SerializeFavorites(Favorites *favs)
{
    BencArray *res = new BencArray();
    for (size_t i = 0; i < favs->favs.Count(); i++) {
        FileFavs *fav = favs->favs.At(i);
        res->Add(fav->filePath);
        res->Add(SerializeFavData(fav));
    }
    return res;
}

static char *SerializePrefs(SerializableGlobalPrefs& globalPrefs, FileHistory& root, Favorites *favs, size_t* lenOut)
{
    char *data = NULL;

    BencDict *prefs = new BencDict();
    if (!prefs)
        goto Error;

    BencDict* global = SerializeGlobalPrefs(globalPrefs);
    if (!global)
        goto Error;
    prefs->Add(GLOBAL_PREFS_STR, global);

    BencArray *fileHistory = SerializeFileHistory(root, globalPrefs.globalPrefsOnly);
    if (!fileHistory)
        goto Error;
    prefs->Add(FILE_HISTORY_STR, fileHistory);

    BencArray *favsArr = SerializeFavorites(favs);
    if (!favsArr)
        goto Error;
    prefs->Add(FAVS_STR, favsArr);

    data = prefs->Encode();
    *lenOut = str::Len(data);

Error:
    delete prefs;
    return data;
}

static DisplayState * DeserializeDisplayState(BencDict *dict, bool globalPrefsOnly)
{
    DisplayState *ds = new DisplayState();
    if (!ds)
        return NULL;

    DeserializeStructBenc(gDisplayStateInfo, dimof(gDisplayStateInfo), ds, dict);
    if (!ds->filePath) {
        delete ds;
        return NULL;
    }

    // work-around https://code.google.com/p/sumatrapdf/issues/detail?id=2140
    if (!IsValidZoom(ds->zoomVirtual))
        ds->zoomVirtual = 100.f;

    return ds;
}

static void DeserializePrefs(const char *prefsTxt, SerializableGlobalPrefs& globalPrefs,
    FileHistory& fh, Favorites *favs)
{
    BencObj *obj = BencObj::Decode(prefsTxt);
    if (!obj || obj->Type() != BT_DICT)
        goto Exit;
    BencDict *prefs = static_cast<BencDict *>(obj);
    BencDict *global = prefs->GetDict(GLOBAL_PREFS_STR);
    if (!global)
        goto Exit;

    DeserializeStructBenc(gSerializableGlobalPrefsInfo, dimof(gSerializableGlobalPrefsInfo), &globalPrefs, global);

    globalPrefs.prevSerialization.Set(global->Encode());

    int weekDiff = GetWeekCount() - globalPrefs.openCountWeek;
    globalPrefs.openCountWeek = GetWeekCount();

    BencArray *fileHistory = prefs->GetArray(FILE_HISTORY_STR);
    if (!fileHistory)
        goto Exit;
    size_t dlen = fileHistory->Length();
    for (size_t i = 0; i < dlen; i++) {
        BencDict *dict = fileHistory->GetDict(i);
        assert(dict);
        if (!dict) continue;
        DisplayState *state = DeserializeDisplayState(dict, globalPrefs.globalPrefsOnly);
        if (state) {
            // "age" openCount statistics (cut in in half after every week)
            state->openCount >>= weekDiff;
            fh.Append(state);
        }
    }

    BencArray *favsArr = prefs->GetArray(FAVS_STR);
    if (!favsArr)
        goto Exit;
    for (size_t i = 0; i < favsArr->Length(); i += 2) {
        BencString *filePathBenc = favsArr->GetString(i);
        BencArray *favData = favsArr->GetArray(i+1);
        if (!filePathBenc || !favData)
            break;
        ScopedMem<WCHAR> filePath(filePathBenc->Value());
        for (size_t j = 0; j < favData->Length(); j += 2) {
            // we're lenient about errors
            BencInt *pageNoBenc = favData->GetInt(j);
            BencString *nameBenc = favData->GetString(j + 1);
            if (!pageNoBenc || !nameBenc)
                break;
            int pageNo = (int)pageNoBenc->Value();
            ScopedMem<WCHAR> name(nameBenc->Value());
            favs->AddOrReplace(filePath, pageNo, EmptyToNull(name));
        }
    }

Exit:
    delete obj;
}

static void DeserializeStructIni(const WCHAR *filepath, SettingInfo *info, size_t count, void *structBase)
{
    IniFile ini(filepath);
    char *base = (char *)structBase;
    IniSection *section = NULL;
    IniLine *line;

    for (size_t i = 0; i < count; i++) {
        SettingInfo& meta = info[i];
        if (!section && meta.type != Type_Section && meta.type != Type_SectionVec) {
            // skip missing section
            while (++i < count && info[i].type != Type_Section && info[i].type != Type_SectionVec);
            i--;
            continue;
        }
        switch (meta.type) {
        case Type_Section:
            section = ini.FindSection(meta.name);
            break;
        case Type_Bool:
            if ((line = section->FindLine(meta.name)))
                *(bool *)(base + meta.offset) = atoi(line->value) != 0;
            break;
        case Type_Color:
            if ((line = section->FindLine(meta.name))) {
                int color;
                if (str::Parse(line->value, "#%6x", &color))
                    *(COLORREF *)(base + meta.offset) = (COLORREF)color;
            }
            break;
        case Type_FileTime:
            if ((line = section->FindLine(meta.name))) {
                FILETIME ft;
                if (_HexToMem(line->value, &ft))
                    *(FILETIME *)(base + meta.offset) = ft;
            }
            break;
        case Type_Float:
            if ((line = section->FindLine(meta.name))) {
                float value;
                if (str::Parse(line->value, "%f", &value))
                    *(float *)(base + meta.offset) = value;
            }
            break;
        case Type_Int:
            if ((line = section->FindLine(meta.name)))
                *(int *)(base + meta.offset) = atoi(line->value);
            break;
        case Type_String:
            if ((line = section->FindLine(meta.name)))
                ((ScopedMem<WCHAR> *)(base + meta.offset))->Set(str::conv::FromUtf8(line->value));
            break;
        case Type_Utf8String:
            if ((line = section->FindLine(meta.name)))
                ((ScopedMem<char> *)(base + meta.offset))->Set(str::Dup(line->value));
            break;
        case Type_Custom:
            CrashIf(true);
            break;
        default:
            CrashIf(true);
        case Type_SectionVec:
            size_t i2 = i;
            while (++i < count && info[i].type != Type_Section && info[i].type != Type_SectionVec);
            for (size_t ix = 0; (section = ini.FindSection(meta.name, ix)); ix++) {
                for (size_t j = i2 + 1; j < i; j++) {
                    SettingInfo& meta2 = info[j];
                    switch (meta2.type) {
#if 0
                    // currently only Strings are used in array sections
                    case Type_Int:
                        ((Vec<int,1> *)(base + meta2.offset))->AppendBlanks(1);
                        if ((line = section->FindLine(meta2.name)))
                            ((Vec<int,1> *)(base + meta2.offset))->Last() = atoi(line->value);
                        break;
                    // etc.
#endif
                    case Type_String:
                        ((WStrVec *)(base + meta2.offset))->AppendBlanks(1);
                        if ((line = section->FindLine(meta2.name)))
                            ((WStrVec *)(base + meta2.offset))->Last() = str::conv::FromUtf8(line->value);
                        break;
                    default:
                        CrashIf(true);
                    }
                }
            }
            i--;
            break;
        }
    }
}

namespace Prefs {

/* Load preferences from the preferences file. */
bool Load(const WCHAR *filepath, SerializableGlobalPrefs& globalPrefs,
          FileHistory& fileHistory, Favorites *favs)
{
    CrashIf(!filepath);
    if (!filepath) return false;

    size_t prefsFileLen;
    ScopedMem<char> prefsTxt(file::ReadAll(filepath, &prefsFileLen));
    if (str::IsEmpty(prefsTxt.Get()))
        return false;

    DeserializePrefs(prefsTxt, globalPrefs, fileHistory, favs);
    globalPrefs.lastPrefUpdate = file::GetModificationTime(filepath);
    return true;
}

bool Save(const WCHAR *filepath, SerializableGlobalPrefs& globalPrefs,
          FileHistory& fileHistory, Favorites* favs)
{
    CrashIf(!filepath);
    if (!filepath) return false;

    size_t dataLen;
    ScopedMem<char> data(SerializePrefs(globalPrefs, fileHistory, favs, &dataLen));
    if (!data)
        return false;

    assert(dataLen > 0);
    FileTransaction trans;
    bool ok = trans.WriteAll(filepath, data, dataLen) && trans.Commit();
    if (ok)
        globalPrefs.lastPrefUpdate = file::GetModificationTime(filepath);
    return ok;
}

}

#define DM_AUTOMATIC_STR            "automatic"
#define DM_SINGLE_PAGE_STR          "single page"
#define DM_FACING_STR               "facing"
#define DM_BOOK_VIEW_STR            "book view"
#define DM_CONTINUOUS_STR           "continuous"
#define DM_CONTINUOUS_FACING_STR    "continuous facing"
#define DM_CONTINUOUS_BOOK_VIEW_STR "continuous book view"

#define IS_STR_ENUM(enumName) \
    if (str::EqIS(txt, TEXT(enumName##_STR))) { \
        *mode = enumName; \
        return true; \
    }

// -view [continuous][singlepage|facing|bookview]
bool ParseViewMode(DisplayMode *mode, const WCHAR *txt)
{
    IS_STR_ENUM(DM_SINGLE_PAGE);
    IS_STR_ENUM(DM_CONTINUOUS);
    IS_STR_ENUM(DM_FACING);
    IS_STR_ENUM(DM_CONTINUOUS_FACING);
    IS_STR_ENUM(DM_BOOK_VIEW);
    IS_STR_ENUM(DM_CONTINUOUS_BOOK_VIEW);
    if (str::EqIS(txt, L"continuous single page")) {
        *mode = DM_CONTINUOUS;
    }
    return true;
}

namespace DisplayModeConv {

#define STR_FROM_ENUM(val) \
    if (val == var) \
        return TEXT(val##_STR);

const WCHAR *NameFromEnum(DisplayMode var)
{
    STR_FROM_ENUM(DM_AUTOMATIC)
    STR_FROM_ENUM(DM_SINGLE_PAGE)
    STR_FROM_ENUM(DM_FACING)
    STR_FROM_ENUM(DM_BOOK_VIEW)
    STR_FROM_ENUM(DM_CONTINUOUS)
    STR_FROM_ENUM(DM_CONTINUOUS_FACING)
    STR_FROM_ENUM(DM_CONTINUOUS_BOOK_VIEW)
    return L"unknown display mode!?";
}

#undef STR_FROM_ENUM

bool EnumFromName(const WCHAR *txt, DisplayMode *mode)
{
    IS_STR_ENUM(DM_AUTOMATIC)
    IS_STR_ENUM(DM_SINGLE_PAGE)
    IS_STR_ENUM(DM_FACING)
    IS_STR_ENUM(DM_BOOK_VIEW)
    IS_STR_ENUM(DM_CONTINUOUS)
    IS_STR_ENUM(DM_CONTINUOUS_FACING)
    IS_STR_ENUM(DM_CONTINUOUS_BOOK_VIEW)
    return false;
}

#undef IS_STR_ENUM

}

/* Caller needs to free() the result. */
static WCHAR *GetAdvancedPrefsPath(bool createMissing=false)
{
    ScopedMem<WCHAR> path(AppGenDataFilename(USER_PREFS_FILE_NAME));
    if (file::Exists(path))
        return path.StealData();
    if (!IsRunningInPortableMode()) {
        path.Set(GetExePath());
        path.Set(path::GetDir(path));
        path.Set(path::Join(path, USER_PREFS_FILE_NAME));
        if (file::Exists(path))
            return path.StealData();
    }
    if (createMissing) {
        // TODO: create from template with link to documentation?
        return AppGenDataFilename(USER_PREFS_FILE_NAME);
    }
    return NULL;
}

static bool LoadAdvancedPrefs(AdvancedSettings *advancedPrefs)
{
#ifdef DISABLE_EBOOK_UI
    advancedPrefs->traditionalEbookUI = true;
#endif
    ScopedMem<WCHAR> path(GetAdvancedPrefsPath());
    if (!path)
        return false;
    DeserializeStructIni(path, gAdvancedSettingsInfo, dimof(gAdvancedSettingsInfo), advancedPrefs);
    return true;
}

/* Caller needs to free() the result. */
static inline WCHAR *GetPrefsFileName()
{
    return AppGenDataFilename(PREFS_FILE_NAME);
}

bool LoadPrefs()
{
    delete gFavorites;
    gFavorites = new Favorites();

    LoadAdvancedPrefs(&gUserPrefs);

    ScopedMem<WCHAR> path(GetPrefsFileName());
    if (!file::Exists(path)) {
        // guess the ui language on first start
        gGlobalPrefs.currLangCode = trans::DetectUserLang();
        return true;
    }
    return Prefs::Load(path, gGlobalPrefs, gFileHistory, gFavorites);
}

// called whenever global preferences change or a file is
// added or removed from gFileHistory (in order to keep
// the list of recently opened documents in sync)
bool SavePrefs()
{
    // don't save preferences for plugin windows
    if (gPluginMode)
        return false;

    // don't save preferences without the proper permission
    if (!HasPermission(Perm_SavePreferences))
        return false;

    /* mark currently shown files as visible */
    for (size_t i = 0; i < gWindows.Count(); i++) {
        UpdateCurrentFileDisplayStateForWin(SumatraWindow::Make(gWindows.At(i)));
    }

    for (size_t i = 0; i < gEbookWindows.Count(); i++) {
        UpdateCurrentFileDisplayStateForWin(SumatraWindow::Make(gEbookWindows.At(i)));
    }

    ScopedMem<WCHAR> path(GetPrefsFileName());
    bool ok = Prefs::Save(path, gGlobalPrefs, gFileHistory, gFavorites);
    if (!ok)
        return false;

    // notify all SumatraPDF instances about the updated prefs file
    HWND hwnd = NULL;
    while ((hwnd = FindWindowEx(HWND_DESKTOP, hwnd, FRAME_CLASS_NAME, NULL))) {
        PostMessage(hwnd, UWM_PREFS_FILE_UPDATED, 0, 0);
    }
    return true;
}

// refresh the preferences when a different SumatraPDF process saves them
bool ReloadPrefs()
{
    ScopedMem<WCHAR> path(GetPrefsFileName());

    FILETIME time = file::GetModificationTime(path);
    if (time.dwLowDateTime == gGlobalPrefs.lastPrefUpdate.dwLowDateTime &&
        time.dwHighDateTime == gGlobalPrefs.lastPrefUpdate.dwHighDateTime) {
        return true;
    }

    const char *currLangCode = gGlobalPrefs.currLangCode;
    bool toolbarVisible = gGlobalPrefs.toolbarVisible;
    bool useSysColors = gGlobalPrefs.useSysColors;

    gFileHistory.Clear();
    delete gFavorites;
    gFavorites = new Favorites();

    bool ok = Prefs::Load(path, gGlobalPrefs, gFileHistory, gFavorites);
    if (!ok)
        return false;

    if (gWindows.Count() > 0 && gWindows.At(0)->IsAboutWindow()) {
        gWindows.At(0)->DeleteInfotip();
        gWindows.At(0)->RedrawAll(true);
    }

    if (!str::Eq(currLangCode, gGlobalPrefs.currLangCode)) {
        SetCurrentLanguageAndRefreshUi(gGlobalPrefs.currLangCode);
    }

    if (gGlobalPrefs.toolbarVisible != toolbarVisible)
        ShowOrHideToolbarGlobally();
    if (gGlobalPrefs.useSysColors != useSysColors)
        UpdateDocumentColors();
    UpdateFavoritesTreeForAllWindows();

    return true;
}
