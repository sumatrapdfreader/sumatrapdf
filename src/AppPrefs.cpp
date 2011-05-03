/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "BencUtil.h"
#include "StrUtil.h"
#include "FileUtil.h"

#include "AppPrefs.h"
#include "DisplayState.h"
#include "FileHistory.h"
#include "translations.h"

#define MAX_REMEMBERED_FILES 1000

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

    prefs->Add(SHOW_TOOLBAR_STR, globalPrefs.m_showToolbar);
    prefs->Add(SHOW_TOC_STR, globalPrefs.m_showToc);
    prefs->Add(TOC_DX_STR, globalPrefs.m_tocDx);
    prefs->Add(PDF_ASSOCIATE_DONT_ASK_STR, globalPrefs.m_pdfAssociateDontAskAgain);
    prefs->Add(PDF_ASSOCIATE_ASSOCIATE_STR, globalPrefs.m_pdfAssociateShouldAssociate);

    prefs->Add(BG_COLOR_STR, globalPrefs.m_bgColor);
    prefs->Add(ESC_TO_EXIT_STR, globalPrefs.m_escToExit);
    prefs->Add(ENABLE_AUTO_UPDATE_STR, globalPrefs.m_enableAutoUpdate);
    prefs->Add(REMEMBER_OPENED_FILES_STR, globalPrefs.m_rememberOpenedFiles);
    prefs->Add(GLOBAL_PREFS_ONLY_STR, globalPrefs.m_globalPrefsOnly);
    prefs->Add(SHOW_RECENT_FILES_STR, globalPrefs.m_showStartPage);

    const TCHAR *mode = DisplayModeConv::NameFromEnum(globalPrefs.m_defaultDisplayMode);
    prefs->Add(DISPLAY_MODE_STR, mode);

    ScopedMem<char> zoom(Str::Format("%.4f", globalPrefs.m_defaultZoom));
    prefs->AddRaw(ZOOM_VIRTUAL_STR, zoom);
    prefs->Add(WINDOW_STATE_STR, globalPrefs.m_windowState);
    prefs->Add(WINDOW_X_STR, globalPrefs.m_windowPos.x);
    prefs->Add(WINDOW_Y_STR, globalPrefs.m_windowPos.y);
    prefs->Add(WINDOW_DX_STR, globalPrefs.m_windowPos.dx);
    prefs->Add(WINDOW_DY_STR, globalPrefs.m_windowPos.dy);

    if (globalPrefs.m_inverseSearchCmdLine)
        prefs->Add(INVERSE_SEARCH_COMMANDLINE, globalPrefs.m_inverseSearchCmdLine);
    prefs->Add(ENABLE_TEX_ENHANCEMENTS_STR, globalPrefs.m_enableTeXEnhancements);
    if (globalPrefs.m_versionToSkip)
        prefs->Add(VERSION_TO_SKIP_STR, globalPrefs.m_versionToSkip);
    if (globalPrefs.m_lastUpdateTime)
        prefs->AddRaw(LAST_UPDATE_STR, globalPrefs.m_lastUpdateTime);
    prefs->AddRaw(UI_LANGUAGE_STR, globalPrefs.m_currentLanguage);

    if (!globalPrefs.m_openCountWeek)
        globalPrefs.m_openCountWeek = GetWeekCount();
    prefs->Add(OPEN_COUNT_WEEK_STR, globalPrefs.m_openCountWeek);

    prefs->Add(FWDSEARCH_OFFSET, globalPrefs.m_fwdsearchOffset);
    prefs->Add(FWDSEARCH_COLOR, globalPrefs.m_fwdsearchColor);
    prefs->Add(FWDSEARCH_WIDTH, globalPrefs.m_fwdsearchWidth);
    prefs->Add(FWDSEARCH_PERMANENT, globalPrefs.m_fwdsearchPermanent);

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

    prefs->Add(SHOW_TOC_STR, ds->showToc);
    prefs->Add(TOC_DX_STR, ds->tocDx);

    ScopedMem<char> zoom(Str::Format("%.4f", ds->zoomVirtual));
    prefs->AddRaw(ZOOM_VIRTUAL_STR, zoom);

    if (ds->tocState && ds->tocState[0] > 0) {
        BencArray *tocState = new BencArray();
        if (tocState) {
            for (int i = 1; i <= ds->tocState[0]; i++)
                tocState->Add(ds->tocState[i]);
            prefs->Add(TOC_STATE_STR, tocState);
        }
    }

    return prefs;
}

static BencArray *SerializeFileHistory(FileHistory& fileHistory, bool globalPrefsOnly)
{
    BencArray *arr = new BencArray();
    if (!arr)
        goto Error;

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
        if (index >= MAX_REMEMBERED_FILES && !state->isPinned)
            continue;
        if (state->openCount < minOpenCount && index > FILE_HISTORY_MAX_RECENT && !state->isPinned)
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

static const char *SerializePrefs(SerializableGlobalPrefs& globalPrefs, FileHistory& root, size_t* lenOut)
{
    const char *data = NULL;

    BencDict *prefs = new BencDict();
    if (!prefs)
        goto Error;
    BencDict* global = SerializeGlobalPrefs(globalPrefs);
    if (!global)
        goto Error;
    prefs->Add(GLOBAL_PREFS_STR, global);
    BencArray *fileHistory = SerializeFileHistory(root, globalPrefs.m_globalPrefsOnly);
    if (!fileHistory)
        goto Error;
    prefs->Add(FILE_HISTORY_STR, fileHistory);

    data = prefs->Encode();
    *lenOut = Str::Len(data);

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
        char *str = Str::Dup(string);
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
    if (string)
        // note: this might round the value for files produced with versions
        //       prior to 1.6 and on a system where the decimal mark isn't a '.'
        //       (the difference should be hardly notable, though)
        value = (float)atof(string);
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

static DisplayState * DisplayState_Deserialize(BencDict *dict, bool globalPrefsOnly)
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
    Retrieve(dict, SHOW_TOC_STR, ds->showToc);
    Retrieve(dict, TOC_DX_STR, ds->tocDx);
    Retrieve(dict, ZOOM_VIRTUAL_STR, ds->zoomVirtual);
    Retrieve(dict, USE_GLOBAL_VALUES_STR, ds->useGlobalValues);

    BencArray *tocState = dict->GetArray(TOC_STATE_STR);
    if (tocState) {
        size_t len = tocState->Length();
        ds->tocState = SAZA(int, len + 1);
        if (ds->tocState) {
            ds->tocState[0] = (int)len;
            for (size_t i = 0; i < len; i++) {
                BencInt *intObj = tocState->GetInt(i);
                if (intObj)
                    ds->tocState[i + 1] = (int)intObj->Value();
            }
        }
    }

    return ds;
}

static bool DeserializePrefs(const char *prefsTxt, SerializableGlobalPrefs& globalPrefs, FileHistory& fh)
{
    BencObj *obj = BencObj::Decode(prefsTxt);
    if (!obj || obj->Type() != BT_DICT)
        goto Error;
    BencDict *prefs = static_cast<BencDict *>(obj);
    BencDict *global = prefs->GetDict(GLOBAL_PREFS_STR);
    if (!global)
        goto Error;

    Retrieve(global, SHOW_TOOLBAR_STR, globalPrefs.m_showToolbar);
    Retrieve(global, SHOW_TOC_STR, globalPrefs.m_showToc);
    Retrieve(global, TOC_DX_STR, globalPrefs.m_tocDx);
    Retrieve(global, PDF_ASSOCIATE_DONT_ASK_STR, globalPrefs.m_pdfAssociateDontAskAgain);
    Retrieve(global, PDF_ASSOCIATE_ASSOCIATE_STR, globalPrefs.m_pdfAssociateShouldAssociate);
    Retrieve(global, ESC_TO_EXIT_STR, globalPrefs.m_escToExit);
    Retrieve(global, BG_COLOR_STR, globalPrefs.m_bgColor);
    Retrieve(global, ENABLE_AUTO_UPDATE_STR, globalPrefs.m_enableAutoUpdate);
    Retrieve(global, REMEMBER_OPENED_FILES_STR, globalPrefs.m_rememberOpenedFiles);
    Retrieve(global, GLOBAL_PREFS_ONLY_STR, globalPrefs.m_globalPrefsOnly);
    Retrieve(global, SHOW_RECENT_FILES_STR, globalPrefs.m_showStartPage);

    Retrieve(global, DISPLAY_MODE_STR, globalPrefs.m_defaultDisplayMode);
    Retrieve(global, ZOOM_VIRTUAL_STR, globalPrefs.m_defaultZoom);
    Retrieve(global, WINDOW_STATE_STR, globalPrefs.m_windowState);

    Retrieve(global, WINDOW_X_STR, globalPrefs.m_windowPos.x);
    Retrieve(global, WINDOW_Y_STR, globalPrefs.m_windowPos.y);
    Retrieve(global, WINDOW_DX_STR, globalPrefs.m_windowPos.dx);
    Retrieve(global, WINDOW_DY_STR, globalPrefs.m_windowPos.dy);

    Retrieve(global, INVERSE_SEARCH_COMMANDLINE, globalPrefs.m_inverseSearchCmdLine);
    Retrieve(global, ENABLE_TEX_ENHANCEMENTS_STR, globalPrefs.m_enableTeXEnhancements);
    Retrieve(global, VERSION_TO_SKIP_STR, globalPrefs.m_versionToSkip);
    RetrieveRaw(global, LAST_UPDATE_STR, globalPrefs.m_lastUpdateTime);

    const char *lang = GetRawString(global, UI_LANGUAGE_STR);
    const char *langCode = Trans::ConfirmLanguage(lang);
    if (langCode)
        globalPrefs.m_currentLanguage = langCode;

    Retrieve(global, FWDSEARCH_OFFSET, globalPrefs.m_fwdsearchOffset);
    Retrieve(global, FWDSEARCH_COLOR, globalPrefs.m_fwdsearchColor);
    Retrieve(global, FWDSEARCH_WIDTH, globalPrefs.m_fwdsearchWidth);
    Retrieve(global, FWDSEARCH_PERMANENT, globalPrefs.m_fwdsearchPermanent);

    Retrieve(global, OPEN_COUNT_WEEK_STR, globalPrefs.m_openCountWeek);
    int weekDiff = GetWeekCount() - globalPrefs.m_openCountWeek;
    globalPrefs.m_openCountWeek = GetWeekCount();

    BencArray *fileHistory = prefs->GetArray(FILE_HISTORY_STR);
    if (!fileHistory)
        goto Error;
    size_t dlen = fileHistory->Length();
    for (size_t i = 0; i < dlen; i++) {
        BencDict *dict = fileHistory->GetDict(i);
        assert(dict);
        if (!dict) continue;
        DisplayState *state = DisplayState_Deserialize(dict, globalPrefs.m_globalPrefsOnly);
        if (state) {
            // "age" openCount statistics (cut in in half after every week)
            state->openCount >>= weekDiff;
            fh.Append(state);
        }
    }
    delete obj;
    return true;

Error:
    delete obj;
    return false;
}

namespace Prefs {

/* Load preferences from the preferences file.
   Returns true if preferences file was loaded, false if there was an error.
*/
bool Load(TCHAR *filepath, SerializableGlobalPrefs& globalPrefs, FileHistory& fileHistory)
{
    bool            ok = false;

    assert(filepath);
    if (!filepath)
        return false;

    size_t prefsFileLen;
    ScopedMem<char> prefsTxt(File::ReadAll(filepath, &prefsFileLen));
    if (!Str::IsEmpty(prefsTxt.Get())) {
        ok = DeserializePrefs(prefsTxt, globalPrefs, fileHistory);
        assert(ok);
        globalPrefs.m_lastPrefUpdate = File::GetModificationTime(filepath);
    }

    // TODO: add a check if a file exists, to filter out deleted files
    // but only if a file is on a non-network drive (because
    // accessing network drives can be slow and unnecessarily spin
    // the drives).
#if 0
    for (int index = 0; fileHistory.Get(index); index++) {
        DisplayState *state = fileHistory.Get(index);
        if (!File::Exists(state->filePath)) {
            DBG_OUT("Prefs_Load() file '%s' doesn't exist anymore\n", state->filePath);
            fileHistory.Remove(state);
            delete state;
        }
    }
#endif

    return ok;
}

bool Save(TCHAR *filepath, SerializableGlobalPrefs& globalPrefs, FileHistory& fileHistory)
{
    assert(filepath);
    if (!filepath)
        return false;

    size_t dataLen;
    ScopedMem<const char> data(SerializePrefs(globalPrefs, fileHistory, &dataLen));
    if (!data)
        return false;

    assert(dataLen > 0);
    /* TODO: consider 2-step process:
        * write to a temp file
        * rename temp file to final file */
    bool ok = File::WriteAll(filepath, (void*)data.Get(), dataLen);
    if (ok)
        globalPrefs.m_lastPrefUpdate = File::GetModificationTime(filepath);
    return ok;
}

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

#define IS_STR_ENUM(enumName) \
    if (Str::Eq(txt, _T(enumName##_STR))) { \
        *resOut = enumName; \
        return true; \
    }

bool EnumFromName(const TCHAR *txt, DisplayMode *resOut)
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
