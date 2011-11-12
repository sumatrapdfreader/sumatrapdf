/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "BencUtil.h"
#include "StrUtil.h"
#include "FileUtil.h"
#include "Transactions.h"

#include "AppPrefs.h"
#include "DisplayState.h"
#include "FileHistory.h"
#include "Favorites.h"
#include "translations.h"

#include "SumatraPDF.h"
#include "WindowInfo.h"
#include "AppTools.h"

#define PREFS_FILE_NAME         _T("sumatrapdfprefs.dat")

#define MAX_REMEMBERED_FILES 1000

#define GLOBAL_PREFS_STR            "gp"
#define FILE_HISTORY_STR            "File History"
#define FAVS_STR                    "Favorites"

#define FILE_STR                    "File"
#define DISPLAY_MODE_STR            "Display Mode"
#define PAGE_NO_STR                 "Page"
#define ZOOM_VIRTUAL_STR            "ZoomVirtual"
#define ROTATION_STR                "Rotation"
#define SCROLL_X_STR                "Scroll X2"
#define SCROLL_Y_STR                "Scroll Y2"
#define WINDOW_STATE_STR            "Window State"
#define WINDOW_X_STR                "Window X"
#define WINDOW_Y_STR                "Window Y"
#define WINDOW_DX_STR               "Window DX"
#define WINDOW_DY_STR               "Window DY"
// for backwards compatibility the string si "ShowToolbar" and not
// (more appropriate now) "ToolbarVisible"
#define TOOLBAR_VISIBLE_STR         "ShowToolbar"
#define PDF_ASSOCIATE_DONT_ASK_STR  "PdfAssociateDontAskAgain"
#define PDF_ASSOCIATE_ASSOCIATE_STR "PdfAssociateShouldAssociate"
#define UI_LANGUAGE_STR             "UILanguage"
#define FAV_VISIBLE_STR             "FavVisible"
// for backwards compatibility the string is "ShowToc" and not 
// (more appropriate now) "TocVisible"
#define TOC_VISIBLE_STR             "ShowToc"
// for backwards compatibility, the serialized name is "Toc DX" and not
// (more apropriate now) "Sidebar DX".
#define SIDEBAR_DX_STR              "Toc DX"
#define TOC_DY_STR                  "Toc Dy"
#define TOC_STATE_STR               "TocToggles"
#define BG_COLOR_STR                "BgColor"
#define ESC_TO_EXIT_STR             "EscToExit"
#define INVERSE_SEARCH_COMMANDLINE  "InverseSearchCommandLine"
#define ENABLE_TEX_ENHANCEMENTS_STR "ExposeInverseSearch"
#define VERSION_TO_SKIP_STR         "VersionToSkip"
#define LAST_UPDATE_STR             "LastUpdate"
#define ENABLE_AUTO_UPDATE_STR      "EnableAutoUpdate"
#define REMEMBER_OPENED_FILES_STR   "RememberOpenedFiles"
#define PRINT_COMMANDLINE           "PrintCommandLine"
#define GLOBAL_PREFS_ONLY_STR       "GlobalPrefsOnly"
#define USE_GLOBAL_VALUES_STR       "UseGlobalValues"
#define DECRYPTION_KEY_STR          "Decryption Key"
#define SHOW_RECENT_FILES_STR       "ShowStartPage"
#define OPEN_COUNT_STR              "OpenCount"
#define IS_PINNED_STR               "Pinned"
#define OPEN_COUNT_WEEK_STR         "OpenCountWeek"
#define FWDSEARCH_OFFSET            "ForwardSearch_HighlightOffset"
#define FWDSEARCH_COLOR             "ForwardSearch_HighlightColor"
#define FWDSEARCH_WIDTH             "ForwardSearch_HighlightWidth"
#define FWDSEARCH_PERMANENT         "ForwardSearch_HighlightPermanent"
#ifdef BUILD_RIBBON
#define USE_RIBBON_STR              "UseRibbon"
#define RIBBON_STATE_STR            "RibbonState"
#endif

#define DM_AUTOMATIC_STR            "automatic"
#define DM_SINGLE_PAGE_STR          "single page"
#define DM_FACING_STR               "facing"
#define DM_BOOK_VIEW_STR            "book view"
#define DM_CONTINUOUS_STR           "continuous"
#define DM_CONTINUOUS_FACING_STR    "continuous facing"
#define DM_CONTINUOUS_BOOK_VIEW_STR "continuous book view"

/* default UI settings */
#define DEFAULT_DISPLAY_MODE    DM_AUTOMATIC
#define DEFAULT_ZOOM            ZOOM_FIT_PAGE
#define DEFAULT_LANGUAGE        "en"
#define COL_FWDSEARCH_BG        RGB(0x65, 0x81, 0xff)

SerializableGlobalPrefs gGlobalPrefs = {
    false, // bool globalPrefsOnly
    DEFAULT_LANGUAGE, // const char *currentLanguage
    true, // bool toolbarVisible
    false, // bool favVisible
    false, // bool pdfAssociateDontAskAgain
    false, // bool pdfAssociateShouldAssociate
    true, // bool enableAutoUpdate
    true, // bool rememberOpenedFiles
    ABOUT_BG_COLOR_DEFAULT, // int bgColor
    false, // bool escToExit
    NULL, // TCHAR *inverseSearchCmdLine
    false, // bool enableTeXEnhancements
    NULL, // TCHAR *versionToSkip
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
#ifdef BUILD_RIBBON
    true, // bool useRibbon
    NULL, // char *ribbonState
#endif
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
    BencDict *prefs = new BencDict();
    if (!prefs)
        return NULL;

    prefs->Add(TOOLBAR_VISIBLE_STR, globalPrefs.toolbarVisible);
    prefs->Add(TOC_VISIBLE_STR, globalPrefs.tocVisible);
    prefs->Add(FAV_VISIBLE_STR, globalPrefs.favVisible);

    prefs->Add(SIDEBAR_DX_STR, globalPrefs.sidebarDx);
    prefs->Add(TOC_DY_STR, globalPrefs.tocDy);
    prefs->Add(PDF_ASSOCIATE_DONT_ASK_STR, globalPrefs.pdfAssociateDontAskAgain);
    prefs->Add(PDF_ASSOCIATE_ASSOCIATE_STR, globalPrefs.pdfAssociateShouldAssociate);

    prefs->Add(BG_COLOR_STR, globalPrefs.bgColor);
    prefs->Add(ESC_TO_EXIT_STR, globalPrefs.escToExit);
    prefs->Add(ENABLE_AUTO_UPDATE_STR, globalPrefs.enableAutoUpdate);
    prefs->Add(REMEMBER_OPENED_FILES_STR, globalPrefs.rememberOpenedFiles);
    prefs->Add(GLOBAL_PREFS_ONLY_STR, globalPrefs.globalPrefsOnly);
    prefs->Add(SHOW_RECENT_FILES_STR, globalPrefs.showStartPage);

    const TCHAR *mode = DisplayModeConv::NameFromEnum(globalPrefs.defaultDisplayMode);
    prefs->Add(DISPLAY_MODE_STR, mode);

    ScopedMem<char> zoom(str::Format("%.4f", globalPrefs.defaultZoom));
    prefs->AddRaw(ZOOM_VIRTUAL_STR, zoom);
    prefs->Add(WINDOW_STATE_STR, globalPrefs.windowState);
    prefs->Add(WINDOW_X_STR, globalPrefs.windowPos.x);
    prefs->Add(WINDOW_Y_STR, globalPrefs.windowPos.y);
    prefs->Add(WINDOW_DX_STR, globalPrefs.windowPos.dx);
    prefs->Add(WINDOW_DY_STR, globalPrefs.windowPos.dy);

    if (globalPrefs.inverseSearchCmdLine)
        prefs->Add(INVERSE_SEARCH_COMMANDLINE, globalPrefs.inverseSearchCmdLine);
    prefs->Add(ENABLE_TEX_ENHANCEMENTS_STR, globalPrefs.enableTeXEnhancements);
    if (globalPrefs.versionToSkip)
        prefs->Add(VERSION_TO_SKIP_STR, globalPrefs.versionToSkip);
    if (globalPrefs.lastUpdateTime)
        prefs->AddRaw(LAST_UPDATE_STR, globalPrefs.lastUpdateTime);
    prefs->AddRaw(UI_LANGUAGE_STR, globalPrefs.currentLanguage);

    if (!globalPrefs.openCountWeek)
        globalPrefs.openCountWeek = GetWeekCount();
    prefs->Add(OPEN_COUNT_WEEK_STR, globalPrefs.openCountWeek);

    prefs->Add(FWDSEARCH_OFFSET, globalPrefs.fwdSearch.offset);
    prefs->Add(FWDSEARCH_COLOR, globalPrefs.fwdSearch.color);
    prefs->Add(FWDSEARCH_WIDTH, globalPrefs.fwdSearch.width);
    prefs->Add(FWDSEARCH_PERMANENT, globalPrefs.fwdSearch.permanent);

#ifdef BUILD_RIBBON
    prefs->Add(USE_RIBBON_STR, globalPrefs.useRibbon);
    if (globalPrefs.ribbonState)
        prefs->AddRaw(RIBBON_STATE_STR, globalPrefs.ribbonState);
#endif

    return prefs;
}

static BencDict *DisplayState_Serialize(DisplayState *ds, bool globalPrefsOnly)
{
    BencDict *prefs = new BencDict();
    if (!prefs)
        return NULL;

    prefs->Add(FILE_STR, ds->filePath);
    if (ds->decryptionKey)
        prefs->AddRaw(DECRYPTION_KEY_STR, ds->decryptionKey);

    prefs->Add(OPEN_COUNT_STR, ds->openCount);
    prefs->Add(IS_PINNED_STR, ds->isPinned);
    if (globalPrefsOnly || ds->useGlobalValues) {
        prefs->Add(USE_GLOBAL_VALUES_STR, TRUE);
        return prefs;
    }

    const TCHAR *mode = DisplayModeConv::NameFromEnum(ds->displayMode);
    prefs->Add(DISPLAY_MODE_STR, mode);
    prefs->Add(PAGE_NO_STR, ds->pageNo);
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

    ScopedMem<char> zoom(str::Format("%.4f", ds->zoomVirtual));
    prefs->AddRaw(ZOOM_VIRTUAL_STR, zoom);

    if (ds->tocState && ds->tocState->Count() > 0) {
        BencArray *tocState = new BencArray();
        if (tocState) {
            for (size_t i = 0; i < ds->tocState->Count(); i++)
                tocState->Add(ds->tocState->At(i));
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
        Vec<DisplayState *> *frequencyList = fileHistory.GetFrequencyOrder();
        if (frequencyList->Count() > FILE_HISTORY_MAX_RECENT)
            minOpenCount = frequencyList->At(FILE_HISTORY_MAX_FREQUENT)->openCount / 2;
        delete frequencyList;
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
        if (!obj)
            goto Error;
        arr->Add(obj);
    }
    return arr;

Error:
    delete arr;
    return NULL;      
}

static inline const TCHAR *NullToEmpty(const TCHAR *s)
{
    return s == NULL ? _T("") : s;
}

static inline const TCHAR *EmptyToNull(const TCHAR *s)
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

static const char *SerializePrefs(SerializableGlobalPrefs& globalPrefs, FileHistory& root, Favorites *favs, size_t* lenOut)
{
    const char *data = NULL;

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
    const char *string = GetRawString(dict, key);
    if (string) {
        char *str = str::Dup(string);
        if (str) {
            free(value);
            value = str;
        }
    }
}

static void Retrieve(BencDict *dict, const char *key, TCHAR *& value)
{
    BencString *string = dict->GetString(key);
    if (string) {
        TCHAR *str = string->Value();
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
        ScopedMem<TCHAR> mode(string->Value());
        if (mode)
            DisplayModeConv::EnumFromName(mode, &value);
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
    if (globalPrefsOnly) {
        ds->useGlobalValues = TRUE;
        return ds;
    }

    Retrieve(dict, DISPLAY_MODE_STR, ds->displayMode);
    Retrieve(dict, PAGE_NO_STR, ds->pageNo);
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

    BencArray *tocState = dict->GetArray(TOC_STATE_STR);
    if (tocState) {
        size_t len = tocState->Length();
        ds->tocState = new Vec<int>(len);
        if (ds->tocState) {
            for (size_t i = 0; i < len; i++) {
                BencInt *intObj = tocState->GetInt(i);
                if (intObj)
                    ds->tocState->Append((int)intObj->Value());
            }
        }
    }

    return ds;
}

static void DeserializePrefs(const char *prefsTxt, SerializableGlobalPrefs& globalPrefs, 
    FileHistory& fh, Favorites **favsOut)
{
    BencObj *obj = BencObj::Decode(prefsTxt);
    if (!obj || obj->Type() != BT_DICT)
        goto Exit;
    BencDict *prefs = static_cast<BencDict *>(obj);
    BencDict *global = prefs->GetDict(GLOBAL_PREFS_STR);
    if (!global)
        goto Exit;

    Retrieve(global, TOOLBAR_VISIBLE_STR, globalPrefs.toolbarVisible);
    Retrieve(global, TOC_VISIBLE_STR, globalPrefs.tocVisible);
    Retrieve(global, FAV_VISIBLE_STR, globalPrefs.favVisible);

    Retrieve(global, SIDEBAR_DX_STR, globalPrefs.sidebarDx);
    Retrieve(global, TOC_DY_STR, globalPrefs.tocDy);
    Retrieve(global, PDF_ASSOCIATE_DONT_ASK_STR, globalPrefs.pdfAssociateDontAskAgain);
    Retrieve(global, PDF_ASSOCIATE_ASSOCIATE_STR, globalPrefs.pdfAssociateShouldAssociate);
    Retrieve(global, ESC_TO_EXIT_STR, globalPrefs.escToExit);
    Retrieve(global, BG_COLOR_STR, globalPrefs.bgColor);
    Retrieve(global, ENABLE_AUTO_UPDATE_STR, globalPrefs.enableAutoUpdate);
    Retrieve(global, REMEMBER_OPENED_FILES_STR, globalPrefs.rememberOpenedFiles);
    Retrieve(global, GLOBAL_PREFS_ONLY_STR, globalPrefs.globalPrefsOnly);
    Retrieve(global, SHOW_RECENT_FILES_STR, globalPrefs.showStartPage);

    Retrieve(global, DISPLAY_MODE_STR, globalPrefs.defaultDisplayMode);
    Retrieve(global, ZOOM_VIRTUAL_STR, globalPrefs.defaultZoom);
    Retrieve(global, WINDOW_STATE_STR, globalPrefs.windowState);

    Retrieve(global, WINDOW_X_STR, globalPrefs.windowPos.x);
    Retrieve(global, WINDOW_Y_STR, globalPrefs.windowPos.y);
    Retrieve(global, WINDOW_DX_STR, globalPrefs.windowPos.dx);
    Retrieve(global, WINDOW_DY_STR, globalPrefs.windowPos.dy);

    Retrieve(global, INVERSE_SEARCH_COMMANDLINE, globalPrefs.inverseSearchCmdLine);
    Retrieve(global, ENABLE_TEX_ENHANCEMENTS_STR, globalPrefs.enableTeXEnhancements);
    Retrieve(global, VERSION_TO_SKIP_STR, globalPrefs.versionToSkip);
    RetrieveRaw(global, LAST_UPDATE_STR, globalPrefs.lastUpdateTime);

    const char *lang = GetRawString(global, UI_LANGUAGE_STR);
    const char *langCode = Trans::ConfirmLanguage(lang);
    if (langCode)
        globalPrefs.currentLanguage = langCode;

    Retrieve(global, FWDSEARCH_OFFSET, globalPrefs.fwdSearch.offset);
    Retrieve(global, FWDSEARCH_COLOR, globalPrefs.fwdSearch.color);
    Retrieve(global, FWDSEARCH_WIDTH, globalPrefs.fwdSearch.width);
    Retrieve(global, FWDSEARCH_PERMANENT, globalPrefs.fwdSearch.permanent);

    Retrieve(global, OPEN_COUNT_WEEK_STR, globalPrefs.openCountWeek);
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

    Favorites *favs = new Favorites();
    *favsOut = favs;

    BencArray *favsArr = prefs->GetArray(FAVS_STR);
    if (!favsArr)
        goto Exit;
    for (size_t i = 0; i < favsArr->Length(); i += 2) {
        BencString *filePathBenc = favsArr->GetString(i);
        BencArray *favData = favsArr->GetArray(i+1);
        if (!filePathBenc || !favData)
            break;
        ScopedMem<TCHAR> filePath(filePathBenc->Value());
        for (size_t j = 0; j < favData->Length(); j += 2) {
            // we're lenient about errors
            BencInt *pageNoBenc = favData->GetInt(j);
            BencString *nameBenc = favData->GetString(j + 1);
            if (!pageNoBenc || !nameBenc)
                break;
            int pageNo = (int)pageNoBenc->Value();
            ScopedMem<TCHAR> name(nameBenc->Value());
            favs->AddOrReplace(filePath, pageNo, EmptyToNull(name));
        }
    }

#ifdef BUILD_RIBBON
    Retrieve(global, USE_RIBBON_STR, globalPrefs.useRibbon);
    RetrieveRaw(global, RIBBON_STATE_STR, globalPrefs.ribbonState);
#endif

Exit:
    delete obj;
}

namespace Prefs {

/* Load preferences from the preferences file. */
void Load(TCHAR *filepath, SerializableGlobalPrefs& globalPrefs,
          FileHistory& fileHistory, Favorites **favs)
{
    size_t prefsFileLen;
    ScopedMem<char> prefsTxt(file::ReadAll(filepath, &prefsFileLen));
    if (!str::IsEmpty(prefsTxt.Get())) {
        DeserializePrefs(prefsTxt, globalPrefs, fileHistory, favs);
        globalPrefs.lastPrefUpdate = file::GetModificationTime(filepath);
    }

    if (!*favs)
        *favs = new Favorites();

    // TODO: add a check if a file exists, to filter out deleted files
    // but only if a file is on a non-network drive (because
    // accessing network drives can be slow and unnecessarily spin
    // the drives).
#if 0
    for (int index = 0; fileHistory.Get(index); index++) {
        DisplayState *state = fileHistory.Get(index);
        if (!file::Exists(state->filePath)) {
            DBG_OUT("Prefs_Load() file '%s' doesn't exist anymore\n", state->filePath);
            fileHistory.Remove(state);
            delete state;
        }
    }
#endif
}

bool Save(TCHAR *filepath, SerializableGlobalPrefs& globalPrefs, FileHistory& fileHistory, Favorites* favs)
{
    assert(filepath);
    if (!filepath)
        return false;

    size_t dataLen;
    ScopedMem<const char> data(SerializePrefs(globalPrefs, fileHistory, favs, &dataLen));
    if (!data)
        return false;

    assert(dataLen > 0);
    FileTransaction trans;
    bool ok = trans.WriteAll(filepath, (void *)data.Get(), dataLen) && trans.Commit();
    if (ok)
        globalPrefs.lastPrefUpdate = file::GetModificationTime(filepath);
    return ok;
}

}

#define IS_STR_ENUM(enumName) \
    if (str::EqIS(txt, _T(enumName##_STR))) { \
        *mode = enumName; \
        return true; \
    }

// -view [continuous][singlepage|facing|bookview]
bool ParseViewMode(DisplayMode *mode, const TCHAR *txt)
{
    IS_STR_ENUM(DM_SINGLE_PAGE);
    IS_STR_ENUM(DM_CONTINUOUS);
    IS_STR_ENUM(DM_FACING);
    IS_STR_ENUM(DM_CONTINUOUS_FACING);
    IS_STR_ENUM(DM_BOOK_VIEW);
    IS_STR_ENUM(DM_CONTINUOUS_BOOK_VIEW);
    if (str::EqIS(txt, _T("continuous single page"))) {
        *mode = DM_CONTINUOUS;
    }
    return true;
}

namespace DisplayModeConv {

#define STR_FROM_ENUM(val) \
    if (val == var) \
        return _T(val##_STR);

const TCHAR *NameFromEnum(DisplayMode var)
{
    STR_FROM_ENUM(DM_AUTOMATIC)
    STR_FROM_ENUM(DM_SINGLE_PAGE)
    STR_FROM_ENUM(DM_FACING)
    STR_FROM_ENUM(DM_BOOK_VIEW)
    STR_FROM_ENUM(DM_CONTINUOUS)
    STR_FROM_ENUM(DM_CONTINUOUS_FACING)
    STR_FROM_ENUM(DM_CONTINUOUS_BOOK_VIEW)
    return _T("unknown display mode!?");
}

#undef STR_FROM_ENUM

bool EnumFromName(const TCHAR *txt, DisplayMode *mode)
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
TCHAR *GetPrefsFileName()
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

    /* mark currently shown files as visible */
    for (size_t i = 0; i < gWindows.Count(); i++)
        UpdateCurrentFileDisplayStateForWin(gWindows.At(i));

    ScopedMem<TCHAR> path(GetPrefsFileName());
    bool ok = Prefs::Save(path, gGlobalPrefs, gFileHistory, gFavorites);
    if (ok) {
        // notify all SumatraPDF instances about the updated prefs file
        HWND hwnd = NULL;
        while ((hwnd = FindWindowEx(HWND_DESKTOP, hwnd, FRAME_CLASS_NAME, NULL)))
            PostMessage(hwnd, UWM_PREFS_FILE_UPDATED, 0, 0);
    }
    return ok;
}

// refresh the preferences when a different SumatraPDF process saves them
bool ReloadPrefs()
{
    ScopedMem<TCHAR> path(GetPrefsFileName());

    FILETIME time = file::GetModificationTime(path);
    if (time.dwLowDateTime == gGlobalPrefs.lastPrefUpdate.dwLowDateTime &&
        time.dwHighDateTime == gGlobalPrefs.lastPrefUpdate.dwHighDateTime) {
        return true;
    }

    const char *currLang = gGlobalPrefs.currentLanguage;
    bool toolbarVisible = gGlobalPrefs.toolbarVisible;

    FileHistory fileHistory;
    Favorites *favs = NULL;
    Prefs::Load(path, gGlobalPrefs, fileHistory, &favs);

    gFileHistory.Clear();
    gFileHistory.ExtendWith(fileHistory);
    delete gFavorites;
    gFavorites = favs;

    if (gWindows.Count() > 0 && gWindows.At(0)->IsAboutWindow()) {
        gWindows.At(0)->DeleteInfotip();
        gWindows.At(0)->RedrawAll(true);
    }

    if (!str::Eq(currLang, gGlobalPrefs.currentLanguage))
        ChangeLanguage(gGlobalPrefs.currentLanguage);

    if (gGlobalPrefs.toolbarVisible != toolbarVisible)
        ShowOrHideToolbarGlobally();
    UpdateFavoritesTreeForAllWindows();

    return true;
}

