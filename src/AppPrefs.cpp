/* Copyright 2013 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "BaseUtil.h"
#include "AppPrefs.h"
#define INCLUDE_APPPREFS2_METADATA
#include "AppPrefs2.h"

#include "AppTools.h"
#include "BencUtil.h"
#include "DebugLog.h"
#include "DisplayState.h"
#include "Favorites.h"
#include "FileHistory.h"
#include "FileTransactions.h"
#include "FileUtil.h"
#include "SumatraPDF.h"
#include "Translations2.h"
#include "WindowInfo.h"
#include "WinUtil.h"

#define PREFS_FILE_NAME         L"sumatrapdfprefs.dat"
#define ADV_PREFS_FILE_NAME     L"sumatrapdfconfig.ini"

#define MAX_REMEMBERED_FILES 1000

enum PrefType {
    Pref_Bool, Pref_Int, Pref_Str, Pref_WStr,
    // custom types which could be implemented through callbacks if need be
    Pref_DisplayMode, Pref_Float, Pref_IntVec, Pref_UILang,
};

struct PrefInfo {
    const char *name;
    PrefType type;
    size_t offset;
    uint32_t bitfield;
};

static BencDict *SerializeStruct(PrefInfo *info, size_t count, const void *structBase, BencDict *prefs=NULL, uint32_t bitmask=-1)
{
    if (!prefs)
        prefs = new BencDict();
    const char *base = (const char *)structBase;
    for (size_t i = 0; i < count; i++) {
        PrefInfo& meta = info[i];
        if ((meta.bitfield & bitmask) != meta.bitfield)
            continue;
        switch (meta.type) {
        case Pref_Bool:
            prefs->Add(meta.name, (int64_t)*(bool *)(base + meta.offset));
            break;
        case Pref_Int:
            prefs->Add(meta.name, (int64_t)*(int *)(base + meta.offset));
            break;
        case Pref_Str:
        case Pref_UILang:
            if (*(const char **)(base + meta.offset))
                prefs->AddRaw(meta.name, *(const char **)(base + meta.offset));
            else
                delete prefs->Remove(meta.name);
            break;
        case Pref_WStr:
            if (*(const WCHAR **)(base + meta.offset))
                prefs->Add(meta.name, *(const WCHAR **)(base + meta.offset));
            else
                delete prefs->Remove(meta.name);
            break;
        case Pref_DisplayMode:
            prefs->Add(meta.name, DisplayModeConv::NameFromEnum(*(DisplayMode *)(base + meta.offset)));
            break;
        case Pref_Float:
            prefs->AddRaw(meta.name, ScopedMem<char>(str::Format("%.4f", *(float *)(base + meta.offset))));
            break;
        case Pref_IntVec:
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
            break;
        }
    }
    return prefs;
}

static void DeserializeStruct(PrefInfo *info, size_t count, void *structBase, BencDict *prefs)
{
    char *base = (char *)structBase;
    BencInt *intObj;
    BencString *strObj;
    BencArray *arrObj;

    for (size_t i = 0; i < count; i++) {
        PrefInfo& meta = info[i];
        switch (meta.type) {
        case Pref_Bool:
            if ((intObj = prefs->GetInt(meta.name)))
                *(bool *)(base + meta.offset) = intObj->Value() != 0;
            break;
        case Pref_Int:
            if ((intObj = prefs->GetInt(meta.name)))
                *(int *)(base + meta.offset) = (int)intObj->Value();
            break;
        case Pref_Str:
            if ((strObj = prefs->GetString(meta.name)))
                str::ReplacePtr((char **)(base + meta.offset), strObj->RawValue());
            break;
        case Pref_WStr:
            if ((strObj = prefs->GetString(meta.name))) {
                ScopedMem<WCHAR> str(strObj->Value());
                if (str)
                    str::ReplacePtr((WCHAR **)(base + meta.offset), str);
            }
            break;
        case Pref_DisplayMode:
            if ((strObj = prefs->GetString(meta.name))) {
                ScopedMem<WCHAR> mode(strObj->Value());
                if (mode)
                    DisplayModeConv::EnumFromName(mode, (DisplayMode *)(base + meta.offset));
            }
            break;
        case Pref_Float:
            if ((strObj = prefs->GetString(meta.name))) {
                // note: this might round the value for files produced with versions
                //       prior to 1.6 and on a system where the decimal mark isn't a '.'
                //       (the difference should be hardly notable, though)
                *(float *)(base + meta.offset) = (float)atof(strObj->RawValue());
            }
            break;
        case Pref_IntVec:
            if ((arrObj = prefs->GetArray(meta.name))) {
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
        case Pref_UILang:
            if ((strObj = prefs->GetString(meta.name))) {
                // ensure language code is valid
                const char *langCode = trans::ValidateLangCode(strObj->RawValue());
                if (langCode)
                    *(const char **)(base + meta.offset) = langCode;
            }
            break;
        }
    }
}

// this list is in alphabetical order as Benc expects it
PrefInfo gGlobalPrefInfo[] = {
#define sgpOffset(x) offsetof(SerializableGlobalPrefs, x)
    { "BgColor", Pref_Int, sgpOffset(bgColor) },
    { "CBX_Right2Left", Pref_Bool, sgpOffset(cbxR2L) },
    { "Display Mode", Pref_DisplayMode, sgpOffset(defaultDisplayMode) },
    { "EnableAutoUpdate", Pref_Bool, sgpOffset(enableAutoUpdate) },
    { "EscToExit", Pref_Bool, sgpOffset(escToExit) },
    { "ExposeInverseSearch", Pref_Bool, sgpOffset(enableTeXEnhancements) },
    { "FavVisible", Pref_Bool, sgpOffset(favVisible) },
    { "ForwardSearch_HighlightColor", Pref_Int, sgpOffset(fwdSearch.color) },
    { "ForwardSearch_HighlightOffset", Pref_Int, sgpOffset(fwdSearch.offset) },
    { "ForwardSearch_HighlightPermanent", Pref_Bool, sgpOffset(fwdSearch.permanent) },
    { "ForwardSearch_HighlightWidth", Pref_Int, sgpOffset(fwdSearch.width) },
    { "GlobalPrefsOnly", Pref_Bool, sgpOffset(globalPrefsOnly) },
    { "InverseSearchCommandLine", Pref_WStr, sgpOffset(inverseSearchCmdLine) },
    { "LastUpdate", Pref_Str, sgpOffset(lastUpdateTime) },
    { "OpenCountWeek", Pref_Int, sgpOffset(openCountWeek) },
    { "PdfAssociateDontAskAgain", Pref_Bool, sgpOffset(pdfAssociateDontAskAgain) },
    { "PdfAssociateShouldAssociate", Pref_Bool, sgpOffset(pdfAssociateShouldAssociate) },
    { "RememberOpenedFiles", Pref_Bool, sgpOffset(rememberOpenedFiles) },
    { "ShowStartPage", Pref_Bool, sgpOffset(showStartPage) },
// for backwards compatibility the string is "ShowToc" and not
// (more appropriate now) "TocVisible"
    { "ShowToc", Pref_Bool, sgpOffset(tocVisible) },
// for backwards compatibility the string is "ShowToolbar" and not
// (more appropriate now) "ToolbarVisible"
    { "ShowToolbar", Pref_Bool, sgpOffset(toolbarVisible) },
// for backwards compatibility, the serialized name is "Toc DX" and not
// (more apropriate now) "Sidebar DX".
    { "Toc DX", Pref_Int, sgpOffset(sidebarDx) },
    { "Toc Dy", Pref_Int, sgpOffset(tocDy) },
    { "UILanguage", Pref_UILang, sgpOffset(currLangCode) },
    { "UseSysColors", Pref_Bool, sgpOffset(useSysColors) },
    { "VersionToSkip", Pref_WStr, sgpOffset(versionToSkip) },
    { "Window DX", Pref_Int, sgpOffset(windowPos.dx) },
    { "Window DY", Pref_Int, sgpOffset(windowPos.dy) },
    { "Window State", Pref_Int, sgpOffset(windowState) },
    { "Window X", Pref_Int, sgpOffset(windowPos.x) },
    { "Window Y", Pref_Int, sgpOffset(windowPos.y) },
    { "ZoomVirtual", Pref_Float, sgpOffset(defaultZoom) },
#undef sgpOffset
};

enum DsIncludeRestrictions {
    Ds_Always = 0,
    Ds_NotGlobal = (1 << 0),
    Ds_OnlyGlobal = (1 << 1),
    Ds_IsRecent = (1 << 2),
    Ds_IsPinned = (1 << 3),
    Ds_IsMissing = (1 << 4),
    Ds_HasTocState = (1 << 5),
};

// this list is in alphabetical order as Benc expects it
PrefInfo gFilePrefInfo[] = {
#define dsOffset(x) offsetof(DisplayState, x)
    { "Decryption Key", Pref_Str, dsOffset(decryptionKey), Ds_Always },
    { "Display Mode", Pref_DisplayMode, dsOffset(displayMode), Ds_NotGlobal },
    { "File", Pref_WStr, dsOffset(filePath), Ds_Always },
    { "Missing", Pref_Bool, dsOffset(isMissing), Ds_IsMissing },
    { "OpenCount", Pref_Int, dsOffset(openCount), Ds_IsRecent },
    { "Page", Pref_Int, dsOffset(pageNo), Ds_NotGlobal },
    { "Pinned", Pref_Bool, dsOffset(isPinned), Ds_IsPinned },
    { "ReparseIdx", Pref_Int, dsOffset(reparseIdx), Ds_NotGlobal },
    { "Rotation", Pref_Int, dsOffset(rotation), Ds_NotGlobal },
    { "Scroll X2", Pref_Int, dsOffset(scrollPos.x), Ds_NotGlobal },
    { "Scroll Y2", Pref_Int, dsOffset(scrollPos.y), Ds_NotGlobal },
// for backwards compatibility the string is "ShowToc" and not
// (more appropriate now) "TocVisible"
    { "ShowToc", Pref_Bool, dsOffset(tocVisible), Ds_NotGlobal },
// for backwards compatibility, the serialized name is "Toc DX" and not
// (more apropriate now) "Sidebar DX".
    { "Toc DX", Pref_Int, dsOffset(sidebarDx), Ds_NotGlobal },
    { "TocToggles", Pref_IntVec, dsOffset(tocState), Ds_NotGlobal | Ds_HasTocState },
    { "UseGlobalValues", Pref_Bool, dsOffset(useGlobalValues), Ds_OnlyGlobal },
    { "Window DX", Pref_Int, dsOffset(windowPos.dx), Ds_NotGlobal },
    { "Window DY", Pref_Int, dsOffset(windowPos.dy), Ds_NotGlobal },
    { "Window State", Pref_Int, dsOffset(windowState), Ds_NotGlobal },
    { "Window X", Pref_Int, dsOffset(windowPos.x), Ds_NotGlobal },
    { "Window Y", Pref_Int, dsOffset(windowPos.y), Ds_NotGlobal },
    { "ZoomVirtual", Pref_Float, dsOffset(zoomVirtual), Ds_NotGlobal },
#undef dsOffset
};

#define GLOBAL_PREFS_STR            "gp"
#define FILE_HISTORY_STR            "File History"
#define FAVS_STR                    "Favorites"

/* default UI settings */
#define DEFAULT_DISPLAY_MODE    DM_AUTOMATIC
#define DEFAULT_ZOOM            ZOOM_FIT_PAGE
#define DEFAULT_LANGUAGE        "en"
#define COL_FWDSEARCH_BG        RGB(0x65, 0x81, 0xff)

SerializableGlobalPrefs gGlobalPrefs = {
    false, // bool globalPrefsOnly
    DEFAULT_LANGUAGE, // const char *currLangCode
    true, // bool toolbarVisible
    false, // bool favVisible
    false, // bool pdfAssociateDontAskAgain
    false, // bool pdfAssociateShouldAssociate
    true, // bool enableAutoUpdate
    true, // bool rememberOpenedFiles
    ABOUT_BG_COLOR_DEFAULT, // int bgColor
    false, // bool escToExit
    false, // bool useSysColors
    NULL, // WCHAR *inverseSearchCmdLine
    false, // bool enableTeXEnhancements
    NULL, // WCHAR *versionToSkip
    NULL, // char *lastUpdateTime
    DEFAULT_DISPLAY_MODE, // DisplayMode defaultDisplayMode
    DEFAULT_ZOOM, // float defaultZoom
    WIN_STATE_NORMAL, // int  windowState
    RectI(), // RectI windowPos
    true, // bool tocVisible
    0, // int sidebarDx
    0, // int tocDy
    {
        0, // int  fwdSearch.offset
        COL_FWDSEARCH_BG, // int  fwdSearch.color
        15, // int  fwdSearch.width
        false, // bool fwdSearch.permanent
    },
    true, // bool showStartPage
    0, // int openCountWeek
    { 0, 0 }, // FILETIME lastPrefUpdate
    false, // bool cbxR2L
    NULL, // char *prevSerialization
};

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
    return SerializeStruct(gGlobalPrefInfo, dimof(gGlobalPrefInfo), &globalPrefs, prevDict);
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
    uint32_t bitmask = (globalPrefsOnly || ds->useGlobalValues ? Ds_OnlyGlobal : Ds_NotGlobal) |
                       (ds->openCount > 0 ? Ds_IsRecent : 0) |
                       (ds->isPinned ? Ds_IsPinned : 0) |
                       (ds->isMissing ? Ds_IsMissing : 0) |
                       (ds->tocState && ds->tocState->Count() > 0 ? Ds_HasTocState : 0);
    return SerializeStruct(gFilePrefInfo, dimof(gFilePrefInfo), ds, NULL, bitmask);
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

    DeserializeStruct(gFilePrefInfo, dimof(gFilePrefInfo), ds, dict);
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

    DeserializeStruct(gGlobalPrefInfo, dimof(gGlobalPrefInfo), &globalPrefs, global);

    free(globalPrefs.prevSerialization);
    globalPrefs.prevSerialization = global->Encode();

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

static void DeserializeAdvancedSettings(const WCHAR *filepath, SettingInfo *info, size_t count, void *structBase)
{
    char *base = (char *)structBase;
    const WCHAR *section = NULL;
    INT intValue, intValueDef;
    ScopedMem<WCHAR> strValue;

    for (size_t i = 0; i < count; i++) {
        SettingInfo& meta = info[i];
        CrashIf(meta.type != SType_Section && !section);
        switch (meta.type) {
        case SType_Section:
            section = meta.name;
            break;
        case SType_Bool:
            intValueDef = *(bool *)(base + meta.offset) ? 1 : 0;
            intValue = GetPrivateProfileInt(section, meta.name, intValueDef, filepath);
            *(bool *)(base + meta.offset) = intValue != 0;
            break;
        case SType_Color:
            strValue.Set(ReadIniString(filepath, section, meta.name));
            if (str::Parse(strValue, L"#%6x", &intValue))
                *(COLORREF *)(base + meta.offset) = (COLORREF)intValue;
            break;
        case SType_Int:
            intValueDef = *(int *)(base + meta.offset);
            intValue = GetPrivateProfileInt(section, meta.name, intValueDef, filepath);
            *(int *)(base + meta.offset) = intValue;
            break;
        case SType_String:
            strValue.Set(ReadIniString(filepath, section, meta.name, L"\n"));
            if (!str::Eq(strValue, L"\n"))
                ((ScopedMem<WCHAR> *)(base + meta.offset))->Set(strValue.StealData());
            break;
        case SType_BoolVec:
        case SType_ColorVec:
        case SType_IntVec:
        case SType_StringVec:
            ScopedMem<WCHAR> sections(ReadIniString(filepath, section, NULL));
            for (const WCHAR *name = sections; *name; name += str::Len(name) + 1) {
                UINT idx;
                if (str::Eq(str::Parse(name, L"%u.", &idx), meta.name)) {
                    if (SType_BoolVec == meta.type) {
                        bool value = GetPrivateProfileInt(section, name, 0, filepath) != 0;
                        ((Vec<bool> *)(base + meta.offset))->InsertAt(idx, value);
                    }
                    else if (SType_ColorVec == meta.type) {
                        strValue.Set(ReadIniString(filepath, section, name));
                        if (str::Parse(strValue, L"#%6x", &intValue)) {
                            COLORREF value = (COLORREF)intValue;
                            ((Vec<COLORREF> *)(base + meta.offset))->InsertAt(idx, value);
                        }
                    }
                    else if (SType_IntVec == meta.type) {
                        intValue = GetPrivateProfileInt(section, name, 0, filepath);
                        ((Vec<int> *)(base + meta.offset))->InsertAt(idx, intValue);
                    }
                    else {
                        strValue.Set(ReadIniString(filepath, section, name));
                        WStrVec *strVec = (WStrVec *)(base + meta.offset);
                        // TODO: shouldn't InsertAt free the previous string?
                        if (idx < strVec->Count())
                            free(strVec->At(idx));
                        strVec->InsertAt(idx, strValue.StealData());
                    }
                }
            }
            break;
        }
    }
}

static void SerializeAdvancedSettings(const WCHAR *filepath, SettingInfo *info, size_t count, void *structBase)
{
    char *base = (char *)structBase;
    const WCHAR *section = NULL;
    bool skipSection = false;
    ScopedMem<WCHAR> strValue;

    for (size_t i = 0; i < count; i++) {
        SettingInfo& meta = info[i];
        CrashIf(meta.type != SType_Section && !section);
        if (meta.type != SType_Section && skipSection)
            continue;
        switch (meta.type) {
        case SType_Section:
            section = meta.name;
            skipSection = !meta.offset;
            break;
        case SType_Bool:
            strValue.Set(str::Format(L"%d", *(bool *)(base + meta.offset) ? 1 : 0));
            WritePrivateProfileString(section, meta.name, strValue, filepath);
            break;
        case SType_Color:
            strValue.Set(str::Format(L"#%06X", (int)*(COLORREF *)(base + meta.offset)));
            WritePrivateProfileString(section, meta.name, strValue, filepath);
            break;
        case SType_Int:
            strValue.Set(str::Format(L"%d", *(int *)(base + meta.offset)));
            WritePrivateProfileString(section, meta.name, strValue, filepath);
            break;
        case SType_String:
            WritePrivateProfileString(section, meta.name, ((ScopedMem<WCHAR> *)(base + meta.offset))->Get(), filepath);
            break;
        case SType_BoolVec:
            {
                Vec<bool> *vec = (Vec<bool> *)(base + meta.offset);
                for (size_t i = 1; i < vec->Count(); i++) {
                    ScopedMem<WCHAR> key(str::Format(L"%u.%s", i, meta.name));
                    strValue.Set(str::Format(L"%d", vec->At(i) ? 1 : 0));
                    WritePrivateProfileString(section, key, strValue, filepath);
                }
            }
            break;
        case SType_ColorVec:
            {
                Vec<COLORREF> *vec = (Vec<COLORREF> *)(base + meta.offset);
                for (size_t i = 1; i < vec->Count(); i++) {
                    ScopedMem<WCHAR> key(str::Format(L"%u.%s", i, meta.name));
                    strValue.Set(str::Format(L"#%06X", (int)vec->At(i)));
                    WritePrivateProfileString(section, key, strValue, filepath);
                }
            }
            break;
        case SType_IntVec:
            {
                Vec<int> *vec = (Vec<int> *)(base + meta.offset);
                for (size_t i = 1; i < vec->Count(); i++) {
                    ScopedMem<WCHAR> key(str::Format(L"%u.%s", i, meta.name));
                    strValue.Set(str::Format(L"%d", vec->At(i)));
                    WritePrivateProfileString(section, key, strValue, filepath);
                }
            }
            break;
        case SType_StringVec:
            {
                WStrVec *vec = (WStrVec *)(base + meta.offset);
                for (size_t i = 1; i < vec->Count(); i++) {
                    ScopedMem<WCHAR> key(str::Format(L"%u.%s", i, meta.name));
                    WritePrivateProfileString(section, key, vec->At(i), filepath);
                }
            }
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
static inline WCHAR *GetPrefsFileName()
{
    return AppGenDataFilename(PREFS_FILE_NAME);
}

bool LoadPrefs()
{
    delete gFavorites;
    gFavorites = new Favorites();

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

static WCHAR *GetAdvancedPrefsPath(bool createMissing=false)
{
    ScopedMem<WCHAR> path(AppGenDataFilename(ADV_PREFS_FILE_NAME));
    if (file::Exists(path))
        return path.StealData();
    if (!IsRunningInPortableMode()) {
        path.Set(GetExePath());
        path.Set(path::GetDir(path));
        path.Set(path::Join(path, ADV_PREFS_FILE_NAME));
        if (file::Exists(path))
            return path.StealData();
    }
    if (createMissing) {
        // TODO: create from template with link to documentation?
        return AppGenDataFilename(ADV_PREFS_FILE_NAME);
    }
    return NULL;
}

bool LoadAdvancedPrefs(AdvancedSettings *advancedPrefs)
{
    ScopedMem<WCHAR> path(GetAdvancedPrefsPath());
    if (!path)
        return false;
    DeserializeAdvancedSettings(path, gAdvancedSettingsInfo, dimof(gAdvancedSettingsInfo), advancedPrefs);
    return true;
}

bool SaveAdvancedPrefs(AdvancedSettings *advancedPrefs)
{
    ScopedMem<WCHAR> path(GetAdvancedPrefsPath(true));
    if (!path)
        return false;
    SerializeAdvancedSettings(path, gAdvancedSettingsInfo, dimof(gAdvancedSettingsInfo), advancedPrefs);
    return true;
}
