/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "BencUtil.h"
#include "StrUtil.h"
#include "FileUtil.h"

#include "AppPrefs.h"
#include "DisplayState.h"
#include "FileHistory.h"
#include "translations.h"

static bool ParseDisplayMode(const char *txt, DisplayMode *resOut)
{
    assert(txt);
    if (!txt) return false;
    return DisplayModeEnumFromName(txt, resOut);
}

#define GLOBAL_PREFS_STR "gp"

static BencDict* SerializeGlobalPrefs(SerializableGlobalPrefs *globalPrefs)
{
    BencDict *prefs = new BencDict();
    if (!prefs)
        return NULL;

    prefs->Add(SHOW_TOOLBAR_STR, globalPrefs->m_showToolbar);
    prefs->Add(SHOW_TOC_STR, globalPrefs->m_showToc);
    prefs->Add(TOC_DX_STR, globalPrefs->m_tocDx);
    prefs->Add(PDF_ASSOCIATE_DONT_ASK_STR, globalPrefs->m_pdfAssociateDontAskAgain);
    prefs->Add(PDF_ASSOCIATE_ASSOCIATE_STR, globalPrefs->m_pdfAssociateShouldAssociate);

    prefs->Add(BG_COLOR_STR, globalPrefs->m_bgColor);
    prefs->Add(ESC_TO_EXIT_STR, globalPrefs->m_escToExit);
    prefs->Add(ENABLE_AUTO_UPDATE_STR, globalPrefs->m_enableAutoUpdate);
    prefs->Add(REMEMBER_OPENED_FILES_STR, globalPrefs->m_rememberOpenedFiles);
    prefs->Add(GLOBAL_PREFS_ONLY_STR, globalPrefs->m_globalPrefsOnly);
    prefs->Add(SHOW_RECENT_FILES_STR, globalPrefs->m_showStartPage);

    const char *txt = DisplayModeNameFromEnum(globalPrefs->m_defaultDisplayMode);
    prefs->Add(DISPLAY_MODE_STR, new BencRawString(txt));

    txt = Str::Format("%.4f", globalPrefs->m_defaultZoom);
    if (txt) {
        prefs->Add(ZOOM_VIRTUAL_STR, new BencRawString(txt));
        free((void*)txt);
    }
    prefs->Add(WINDOW_STATE_STR, globalPrefs->m_windowState);
    prefs->Add(WINDOW_X_STR, globalPrefs->m_windowPos.x);
    prefs->Add(WINDOW_Y_STR, globalPrefs->m_windowPos.y);
    prefs->Add(WINDOW_DX_STR, globalPrefs->m_windowPos.dx);
    prefs->Add(WINDOW_DY_STR, globalPrefs->m_windowPos.dy);

    if (globalPrefs->m_inverseSearchCmdLine)
        prefs->Add(INVERSE_SEARCH_COMMANDLINE, globalPrefs->m_inverseSearchCmdLine);
    prefs->Add(ENABLE_TEX_ENHANCEMENTS_STR, globalPrefs->m_enableTeXEnhancements);
    if (globalPrefs->m_versionToSkip)
        prefs->Add(VERSION_TO_SKIP_STR, globalPrefs->m_versionToSkip);
    if (globalPrefs->m_lastUpdateTime)
        prefs->Add(LAST_UPDATE_STR, new BencRawString(globalPrefs->m_lastUpdateTime));
    prefs->Add(UI_LANGUAGE_STR, new BencRawString(globalPrefs->m_currentLanguage));
    prefs->Add(FWDSEARCH_OFFSET, globalPrefs->m_fwdsearchOffset);
    prefs->Add(FWDSEARCH_COLOR, globalPrefs->m_fwdsearchColor);
    prefs->Add(FWDSEARCH_WIDTH, globalPrefs->m_fwdsearchWidth);
    prefs->Add(FWDSEARCH_PERMANENT, globalPrefs->m_fwdsearchPermanent);

    return prefs;
}

static BencDict *DisplayState_Serialize(DisplayState *ds, bool globalPrefsOnly)
{
    BencDict *prefs = new BencDict();
    if (!prefs)
        return NULL;

    prefs->Add(FILE_STR, ds->filePath);
    if (ds->decryptionKey)
        prefs->Add(DECRYPTION_KEY_STR, new BencRawString(ds->decryptionKey));

    prefs->Add(OPEN_COUNT_STR, ds->openCount);
    if (globalPrefsOnly || ds->useGlobalValues) {
        prefs->Add(USE_GLOBAL_VALUES_STR, TRUE);
        return prefs;
    }

    const char *txt = DisplayModeNameFromEnum(ds->displayMode);
    if (txt)
        prefs->Add(DISPLAY_MODE_STR, new BencRawString(txt));
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

    txt = Str::Format("%.4f", ds->zoomVirtual);
    if (txt) {
        prefs->Add(ZOOM_VIRTUAL_STR, new BencRawString(txt));
        free((void*)txt);
    }

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

static BencArray *SerializeFileHistory(FileHistory *fileHistory, bool globalPrefsOnly)
{
    assert(fileHistory);
    if (!fileHistory) return NULL;

    BencArray *arr = new BencArray();
    if (!arr)
        goto Error;

    // Don't save more file entries than will be useful
    int maxRememberdItems = globalPrefsOnly ? MAX_RECENT_FILES_IN_MENU : INT_MAX;
    for (int index = 0; index < maxRememberdItems; index++) {
        DisplayState *state = fileHistory->Get(index);
        if (!state)
            break;
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

static const char *SerializePrefs(SerializableGlobalPrefs *globalPrefs, FileHistory *root, size_t* lenOut)
{
    const char *data = NULL;

    BencDict *prefs = new BencDict();
    if (!prefs)
        goto Error;
    BencDict* global = SerializeGlobalPrefs(globalPrefs);
    if (!global)
        goto Error;
    prefs->Add(GLOBAL_PREFS_STR, global);
    BencArray *fileHistory = SerializeFileHistory(root, globalPrefs->m_globalPrefsOnly);
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
    if (globalPrefsOnly) {
        ds->useGlobalValues = TRUE;
        return ds;
    }

    const char* txt = GetRawString(dict, DISPLAY_MODE_STR);
    if (txt)
        DisplayModeEnumFromName(txt, &ds->displayMode);
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
    txt = GetRawString(dict, ZOOM_VIRTUAL_STR);
    if (txt)
        ds->zoomVirtual = (float)atof(txt);
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

static bool DeserializePrefs(const char *prefsTxt, SerializableGlobalPrefs *globalPrefs, FileHistory *fh)
{
    BencObj *obj = BencObj::Decode(prefsTxt);
    if (!obj || obj->Type() != BT_DICT)
        goto Error;
    BencDict *prefs = static_cast<BencDict *>(obj);
    BencDict *global = prefs->GetDict(GLOBAL_PREFS_STR);
    if (!global)
        goto Error;

    Retrieve(global, SHOW_TOOLBAR_STR, globalPrefs->m_showToolbar);
    Retrieve(global, SHOW_TOC_STR, globalPrefs->m_showToc);
    Retrieve(global, TOC_DX_STR, globalPrefs->m_tocDx);
    Retrieve(global, PDF_ASSOCIATE_DONT_ASK_STR, globalPrefs->m_pdfAssociateDontAskAgain);
    Retrieve(global, PDF_ASSOCIATE_ASSOCIATE_STR, globalPrefs->m_pdfAssociateShouldAssociate);
    Retrieve(global, ESC_TO_EXIT_STR, globalPrefs->m_escToExit);
    Retrieve(global, BG_COLOR_STR, globalPrefs->m_bgColor);
    Retrieve(global, ENABLE_AUTO_UPDATE_STR, globalPrefs->m_enableAutoUpdate);
    Retrieve(global, REMEMBER_OPENED_FILES_STR, globalPrefs->m_rememberOpenedFiles);
    Retrieve(global, GLOBAL_PREFS_ONLY_STR, globalPrefs->m_globalPrefsOnly);
    Retrieve(global, SHOW_RECENT_FILES_STR, globalPrefs->m_showStartPage);

    const char* txt = GetRawString(global, DISPLAY_MODE_STR);
    if (txt)
        DisplayModeEnumFromName(txt, &globalPrefs->m_defaultDisplayMode);
    txt = GetRawString(global, ZOOM_VIRTUAL_STR);
    if (txt)
        globalPrefs->m_defaultZoom = (float)atof(txt);
    Retrieve(global, WINDOW_STATE_STR, globalPrefs->m_windowState);

    Retrieve(global, WINDOW_X_STR, globalPrefs->m_windowPos.x);
    Retrieve(global, WINDOW_Y_STR, globalPrefs->m_windowPos.y);
    Retrieve(global, WINDOW_DX_STR, globalPrefs->m_windowPos.dx);
    Retrieve(global, WINDOW_DY_STR, globalPrefs->m_windowPos.dy);

    Retrieve(global, INVERSE_SEARCH_COMMANDLINE, globalPrefs->m_inverseSearchCmdLine);
    Retrieve(global, ENABLE_TEX_ENHANCEMENTS_STR, globalPrefs->m_enableTeXEnhancements);
    Retrieve(global, VERSION_TO_SKIP_STR, globalPrefs->m_versionToSkip);
    RetrieveRaw(global, LAST_UPDATE_STR, globalPrefs->m_lastUpdateTime);

    txt = GetRawString(global, UI_LANGUAGE_STR);
    const char *langCode = Trans::ConfirmLanguage(txt);
    if (langCode)
        globalPrefs->m_currentLanguage = langCode;

    Retrieve(global, FWDSEARCH_OFFSET, globalPrefs->m_fwdsearchOffset);
    Retrieve(global, FWDSEARCH_COLOR, globalPrefs->m_fwdsearchColor);
    Retrieve(global, FWDSEARCH_WIDTH, globalPrefs->m_fwdsearchWidth);
    Retrieve(global, FWDSEARCH_PERMANENT, globalPrefs->m_fwdsearchPermanent);

    BencArray *fileHistory = prefs->GetArray(FILE_HISTORY_STR);
    if (!fileHistory)
        goto Error;
    size_t dlen = fileHistory->Length();
    for (size_t i = 0; i < dlen; i++) {
        BencDict *dict = fileHistory->GetDict(i);
        assert(dict);
        if (!dict) continue;
        DisplayState *state = DisplayState_Deserialize(dict, globalPrefs->m_globalPrefsOnly);
        if (state)
            fh->Append(state);
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
bool Load(TCHAR *filepath, SerializableGlobalPrefs *globalPrefs, FileHistory *fileHistory)
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
        globalPrefs->m_lastPrefUpdate = File::GetModificationTime(filepath);
    }

    // TODO: add a check if a file exists, to filter out deleted files
    // but only if a file is on a non-network drive (because
    // accessing network drives can be slow and unnecessarily spin
    // the drives).
#if 0
    for (int index = 0; fileHistory->Get(index); index++) {
        DisplayState *state = fileHistory->Get(index);
        if (!File::Exists(state->filePath)) {
            DBG_OUT("Prefs_Load() file '%s' doesn't exist anymore\n", state->filePath);
            fileHistory->Remove(state);
            delete state;
        }
    }
#endif

    return ok;
}

bool Save(TCHAR *filepath, SerializableGlobalPrefs *globalPrefs, FileHistory *fileHistory)
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
        globalPrefs->m_lastPrefUpdate = File::GetModificationTime(filepath);
    return ok;
}

}
