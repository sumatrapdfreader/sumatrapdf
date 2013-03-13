/* Copyright 2013 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "BaseUtil.h"
#include "AppPrefs.h"

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

#define PREFS_FILE_NAME         L"sumatrapdfprefs.dat"

#define MAX_REMEMBERED_FILES 1000

/* default UI settings */
#define DEFAULT_DISPLAY_MODE    DM_AUTOMATIC
#define DEFAULT_ZOOM            ZOOM_FIT_PAGE
#define DEFAULT_LANGUAGE        "en"
#define COL_FWDSEARCH_BG        RGB(0x65, 0x81, 0xff)

enum PrefType { Pref_Bool, Pref_Int, Pref_Str, Pref_WStr, Pref_Custom };

struct PrefInfo {
    const char *name;
    PrefType type;
    size_t offset;
    bool (*FromBenc)(BencString *obj, void *valOut);
    BencString *(*ToBenc)(void *valIn);
};

/* converters for custom types */
static bool DisplayModeFromBenc(BencString *obj, void *valOut)
{
    ScopedMem<WCHAR> mode(obj->Value());
    return mode && DisplayModeConv::EnumFromName(mode, (DisplayMode *)valOut);
}
static BencString *DisplayModeToBenc(void *valIn)
{
    return new BencString(DisplayModeConv::NameFromEnum(*(DisplayMode *)valIn));
}

static bool UILangFromBenc(BencString *obj, void *valOut)
{
    // ensure language code is valid
    const char *langCode = trans::ValidateLangCode(obj->RawValue());
    *(const char **)valOut = langCode ? langCode : DEFAULT_LANGUAGE;
    return true;
}
static BencString *UILangToBenc(void *valIn)
{
    return new BencString(*(const char **)valIn, (size_t)-1);
}

static bool FloatFromBenc(BencString *obj, void *valOut)
{
    return str::Parse(obj->RawValue(), "%f%$", (float *)valOut) != NULL;
}
static BencString *FloatToBenc(void *valIn)
{
    ScopedMem<char> zoom(str::Format("%.4f", *(float *)valIn));
    return new BencString(zoom, (size_t)-1);
}

// this list is in alphabetical order as Benc expects it
PrefInfo gGlobalPrefInfo[] = {
#define sgpOffset(x) offsetof(SerializableGlobalPrefs, x)
    { "BgColor", Pref_Int, sgpOffset(bgColor) },
    { "CBX_Right2Left", Pref_Bool, sgpOffset(cbxR2L) },
    { "Display Mode", Pref_Custom, sgpOffset(defaultDisplayMode), &DisplayModeFromBenc, &DisplayModeToBenc },
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
    { "UILanguage", Pref_Custom, sgpOffset(currLangCode), &UILangFromBenc, &UILangToBenc },
    { "UseSysColors", Pref_Bool, sgpOffset(useSysColors) },
    { "VersionToSkip", Pref_WStr, sgpOffset(versionToSkip) },
    { "Window DX", Pref_Int, sgpOffset(windowPos.dx) },
    { "Window DY", Pref_Int, sgpOffset(windowPos.dy) },
    { "Window State", Pref_Int, sgpOffset(windowState) },
    { "Window X", Pref_Int, sgpOffset(windowPos.x) },
    { "Window Y", Pref_Int, sgpOffset(windowPos.y) },
    { "ZoomVirtual", Pref_Custom, sgpOffset(defaultZoom), &FloatFromBenc, &FloatToBenc },
#undef sgpOffset
};

#define GLOBAL_PREFS_STR            "gp"
#define FILE_HISTORY_STR            "File History"
#define FAVS_STR                    "Favorites"

#define FILE_STR                    "File"
#define DISPLAY_MODE_STR            "Display Mode"
#define PAGE_NO_STR                 "Page"
#define REPARSE_IDX_STR             "ReparseIdx"
#define ZOOM_VIRTUAL_STR            "ZoomVirtual"
#define ROTATION_STR                "Rotation"
#define SCROLL_X_STR                "Scroll X2"
#define SCROLL_Y_STR                "Scroll Y2"
#define WINDOW_STATE_STR            "Window State"
#define WINDOW_X_STR                "Window X"
#define WINDOW_Y_STR                "Window Y"
#define WINDOW_DX_STR               "Window DX"
#define WINDOW_DY_STR               "Window DY"
// for backwards compatibility the string is "ShowToc" and not
// (more appropriate now) "TocVisible"
#define TOC_VISIBLE_STR             "ShowToc"
// for backwards compatibility, the serialized name is "Toc DX" and not
// (more apropriate now) "Sidebar DX".
#define SIDEBAR_DX_STR              "Toc DX"
#define TOC_STATE_STR               "TocToggles"
#define USE_GLOBAL_VALUES_STR       "UseGlobalValues"
#define DECRYPTION_KEY_STR          "Decryption Key"
#define OPEN_COUNT_STR              "OpenCount"
#define IS_PINNED_STR               "Pinned"
#define IS_MISSING_STR              "Missing"

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

    BencDict *prefs = new BencDict();
    char *structBase = (char *)&globalPrefs;
    for (size_t i = 0; i < dimof(gGlobalPrefInfo); i++) {
        PrefInfo& meta = gGlobalPrefInfo[i];
        switch (meta.type) {
        case Pref_Bool:
            prefs->Add(meta.name, (int64_t)*(bool *)(structBase + meta.offset));
            break;
        case Pref_Int:
            prefs->Add(meta.name, (int64_t)*(int *)(structBase + meta.offset));
            break;
        case Pref_Str:
            if (*(const char **)(structBase + meta.offset))
                prefs->AddRaw(meta.name, *(const char **)(structBase + meta.offset));
            break;
        case Pref_WStr:
            if (*(const WCHAR **)(structBase + meta.offset))
                prefs->Add(meta.name, *(const WCHAR **)(structBase + meta.offset));
            break;
        case Pref_Custom:
            prefs->Add(meta.name, meta.ToBenc(structBase + meta.offset));
            break;
        }
    }
    return prefs;
}

static BencDict *DisplayState_Serialize(DisplayState *ds, bool globalPrefsOnly)
{
    if (ds->isMissing && (globalPrefsOnly || ds->useGlobalValues) &&
        !ds->decryptionKey && !ds->isPinned) {
        // forget about missing documents without valuable state
        return NULL;
    }

    BencDict *prefs = new BencDict();

    prefs->Add(FILE_STR, ds->filePath);
    if (ds->decryptionKey)
        prefs->AddRaw(DECRYPTION_KEY_STR, ds->decryptionKey);

    if (ds->openCount > 0)
        prefs->Add(OPEN_COUNT_STR, ds->openCount);
    if (ds->isPinned)
        prefs->Add(IS_PINNED_STR, ds->isPinned);
    if (ds->isMissing)
        prefs->Add(IS_MISSING_STR, ds->isMissing);
    if (globalPrefsOnly || ds->useGlobalValues) {
        prefs->Add(USE_GLOBAL_VALUES_STR, TRUE);
        return prefs;
    }

    const WCHAR *mode = DisplayModeConv::NameFromEnum(ds->displayMode);
    prefs->Add(DISPLAY_MODE_STR, mode);
    prefs->Add(PAGE_NO_STR, ds->pageNo);
    prefs->Add(REPARSE_IDX_STR, ds->reparseIdx);
    prefs->Add(ROTATION_STR, ds->rotation);
    prefs->Add(SCROLL_X_STR, ds->scrollPos.x);
    prefs->Add(SCROLL_Y_STR, ds->scrollPos.y);
    prefs->Add(WINDOW_STATE_STR, ds->windowState);
    prefs->Add(WINDOW_X_STR, ds->windowPos.x);
    prefs->Add(WINDOW_Y_STR, ds->windowPos.y);
    prefs->Add(WINDOW_DX_STR, ds->windowPos.dx);
    prefs->Add(WINDOW_DY_STR, ds->windowPos.dy);

    prefs->Add(TOC_VISIBLE_STR, ds->tocVisible);
    prefs->Add(SIDEBAR_DX_STR, ds->sidebarDx);

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

    ScopedMem<char> zoom(str::Format("%.4f", ds->zoomVirtual));
    prefs->AddRaw(ZOOM_VIRTUAL_STR, zoom);

    if (ds->tocState && ds->tocState->Count() > 0) {
        BencArray *tocState = new BencArray();
        if (tocState) {
            for (size_t i = 0; i < ds->tocState->Count(); i++) {
                tocState->Add(ds->tocState->At(i));
            }
            prefs->Add(TOC_STATE_STR, tocState);
        }
    }

    return prefs;
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

static void Retrieve(BencDict *dict, const char *key, int& value)
{
    BencInt *intObj = dict->GetInt(key);
    if (intObj)
        value = (int)intObj->Value();
}

static void Retrieve(BencDict *dict, const char *key, bool& value)
{
    BencInt *intObj = dict->GetInt(key);
    if (intObj)
        value = intObj->Value() != 0;
}

static const char *GetRawString(BencDict *dict, const char *key)
{
    BencString *string = dict->GetString(key);
    if (string)
        return string->RawValue();
    return NULL;
}

static void RetrieveRaw(BencDict *dict, const char *key, char *& value)
{
    char *str = str::Dup(GetRawString(dict, key));
    if (str) {
        free(value);
        value = str;
    }
}

static void Retrieve(BencDict *dict, const char *key, WCHAR *& value)
{
    BencString *string = dict->GetString(key);
    if (string) {
        WCHAR *str = string->Value();
        if (str) {
            free(value);
            value = str;
        }
    }
}

static void Retrieve(BencDict *dict, const char *key, float& value)
{
    const char *string = GetRawString(dict, key);
    if (string) {
        // note: this might round the value for files produced with versions
        //       prior to 1.6 and on a system where the decimal mark isn't a '.'
        //       (the difference should be hardly notable, though)
        value = (float)atof(string);
    }
}

static void Retrieve(BencDict *dict, const char *key, DisplayMode& value)
{
    BencString *string = dict->GetString(key);
    if (string) {
        ScopedMem<WCHAR> mode(string->Value());
        if (mode)
            DisplayModeConv::EnumFromName(mode, &value);
    }
}

static void DeserializeToc(BencDict *dict, DisplayState *ds)
{
    BencArray *tocState = dict->GetArray(TOC_STATE_STR);
    if (!tocState)
        return;
    size_t len = tocState->Length();
    ds->tocState = new Vec<int>(len);
    if (!ds->tocState)
        return;

    for (size_t i = 0; i < len; i++) {
        BencInt *intObj = tocState->GetInt(i);
        if (intObj)
            ds->tocState->Append((int)intObj->Value());
    }
}

static DisplayState * DeserializeDisplayState(BencDict *dict, bool globalPrefsOnly)
{
    DisplayState *ds = new DisplayState();
    if (!ds)
        return NULL;

    Retrieve(dict, FILE_STR, ds->filePath);
    if (!ds->filePath) {
        delete ds;
        return NULL;
    }

    RetrieveRaw(dict, DECRYPTION_KEY_STR, ds->decryptionKey);
    Retrieve(dict, OPEN_COUNT_STR, ds->openCount);
    Retrieve(dict, IS_PINNED_STR, ds->isPinned);
    Retrieve(dict, IS_MISSING_STR, ds->isMissing);
    if (globalPrefsOnly) {
        ds->useGlobalValues = TRUE;
        return ds;
    }

    Retrieve(dict, DISPLAY_MODE_STR, ds->displayMode);
    Retrieve(dict, PAGE_NO_STR, ds->pageNo);
    Retrieve(dict, REPARSE_IDX_STR, ds->reparseIdx);
    Retrieve(dict, ROTATION_STR, ds->rotation);
    Retrieve(dict, SCROLL_X_STR, ds->scrollPos.x);
    Retrieve(dict, SCROLL_Y_STR, ds->scrollPos.y);
    Retrieve(dict, WINDOW_STATE_STR, ds->windowState);
    Retrieve(dict, WINDOW_X_STR, ds->windowPos.x);
    Retrieve(dict, WINDOW_Y_STR, ds->windowPos.y);
    Retrieve(dict, WINDOW_DX_STR, ds->windowPos.dx);
    Retrieve(dict, WINDOW_DY_STR, ds->windowPos.dy);
    Retrieve(dict, TOC_VISIBLE_STR, ds->tocVisible);
    Retrieve(dict, SIDEBAR_DX_STR, ds->sidebarDx);
    Retrieve(dict, ZOOM_VIRTUAL_STR, ds->zoomVirtual);
    Retrieve(dict, USE_GLOBAL_VALUES_STR, ds->useGlobalValues);

    // work-around https://code.google.com/p/sumatrapdf/issues/detail?id=2140
    if (!IsValidZoom(ds->zoomVirtual))
        ds->zoomVirtual = 100.f;

    DeserializeToc(dict, ds);

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

    char *structBase = (char *)&globalPrefs;
    BencInt *intObj;
    BencString *strObj;
    for (size_t i = 0; i < dimof(gGlobalPrefInfo); i++) {
        PrefInfo& meta = gGlobalPrefInfo[i];
        switch (meta.type) {
        case Pref_Bool:
            if ((intObj = global->GetInt(meta.name)))
                *(bool *)(structBase + meta.offset) = intObj->Value() != 0;
            break;
        case Pref_Int:
            if ((intObj = global->GetInt(meta.name)))
                *(int *)(structBase + meta.offset) = (int)intObj->Value();
            break;
        case Pref_Str:
            if ((strObj = global->GetString(meta.name))) {
                const char *str = strObj->RawValue();
                if (str)
                    str::ReplacePtr((char **)(structBase + meta.offset), str);
            }
            break;
        case Pref_WStr:
            if ((strObj = global->GetString(meta.name))) {
                ScopedMem<WCHAR> str(strObj->Value());
                if (str)
                    str::ReplacePtr((WCHAR **)(structBase + meta.offset), str);
            }
            break;
        case Pref_Custom:
            if ((strObj = global->GetString(meta.name)))
                meta.FromBenc(strObj, structBase + meta.offset);
            break;
        }
    }

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
WCHAR *GetPrefsFileName()
{
    return AppGenDataFilename(PREFS_FILE_NAME);
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

    Prefs::Load(path, gGlobalPrefs, gFileHistory, gFavorites);

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
