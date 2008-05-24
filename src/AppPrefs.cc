/* Copyright Krzysztof Kowalczyk 2006-2007
   License: GPLv2 */

#include "base_util.h"
#include "benc_util.h"
#include "str_util.h"
#include "file_util.h"
#include "dstring.h"

#include "AppPrefs.h"
#include "DisplayState.h"
#include "FileHistory.h"

extern bool CurrLangNameSet(const char* langName);
extern const char* CurrLangNameGet();

#define DEFAULT_WINDOW_X     40
#define DEFAULT_WINDOW_Y     20
#define DEFAULT_WINDOW_DX    640
#define DEFAULT_WINDOW_DY    480

static bool ParseDisplayMode(const char *txt, DisplayMode *resOut)
{
    assert(txt);
    if (!txt) return false;
    return DisplayModeEnumFromName(txt, resOut);
}

#define GLOBAL_PREFS_STR "gp"

#define DICT_NEW(boname) \
    benc_dict* boname = benc_dict_new(); \
    if (!boname) \
        goto Error;

#define DICT_ADD_INT64(doname,name,val) \
    ok = benc_dict_insert_int64(doname,name,(int64_t)val); \
    if (!ok) \
        goto Error;

#define DICT_ADD_STR(doname,name,val) \
        ok = benc_dict_insert_str(doname,name,val); \
        if (!ok) \
            goto Error;

#define DICT_ADD_DICT(doname,name,val) \
        ok = benc_dict_insert2(doname,name,(benc_obj*)val); \
        if (!ok) \
            goto Error;

benc_dict* Prefs_SerializeGlobal(void)
{
    BOOL       ok;
    const char * txt;

    DICT_NEW(prefs);
    DICT_ADD_INT64(prefs, SHOW_TOOLBAR_STR, gGlobalPrefs.m_showToolbar);
    DICT_ADD_INT64(prefs, USE_FITZ_STR, gGlobalPrefs.m_useFitz);
    DICT_ADD_INT64(prefs, PDF_ASSOCIATE_DONT_ASK_STR, gGlobalPrefs.m_pdfAssociateDontAskAgain);
    DICT_ADD_INT64(prefs, PDF_ASSOCIATE_ASSOCIATE_STR, gGlobalPrefs.m_pdfAssociateShouldAssociate);

    DICT_ADD_INT64(prefs, BG_COLOR_STR, gGlobalPrefs.m_bgColor);
    DICT_ADD_INT64(prefs, ESC_TO_EXIT_STR, gGlobalPrefs.m_escToExit);
    DICT_ADD_INT64(prefs, FULLSCREEN_STR, gGlobalPrefs.m_fullScreen);

    txt = DisplayModeNameFromEnum(gGlobalPrefs.m_defaultDisplayMode);
    if (txt)
        DICT_ADD_STR(prefs, DISPLAY_MODE_STR, txt);

    txt = str_printf("%.4f", gGlobalPrefs.m_defaultZoom);
    if (txt) {
        DICT_ADD_STR(prefs, ZOOM_VIRTUAL_STR, txt);
        free((void*)txt);
    }
    DICT_ADD_INT64(prefs, WINDOW_STATE_STR, gGlobalPrefs.m_windowState);
    DICT_ADD_INT64(prefs, WINDOW_X_STR, gGlobalPrefs.m_windowPosX);
    DICT_ADD_INT64(prefs, WINDOW_Y_STR, gGlobalPrefs.m_windowPosY);
    DICT_ADD_INT64(prefs, WINDOW_DX_STR, gGlobalPrefs.m_windowDx);
    DICT_ADD_INT64(prefs, WINDOW_DY_STR, gGlobalPrefs.m_windowDy);
    DICT_ADD_STR(prefs, INVERSE_SEARCH_COMMANDLINE, gGlobalPrefs.m_inversesearch_cmdline);
    DICT_ADD_STR(prefs, UI_LANGUAGE_STR, CurrLangNameGet());
    return prefs;
Error:
    benc_obj_delete((benc_obj*)prefs);
    return NULL;
}

/* TODO: move to benc_util.c ? */
static BOOL dict_get_str_dup(benc_dict* dict, const char* key, const char** valOut)
{
    const char *str = dict_get_str(dict, key);
    if (!str) return FALSE;
    *valOut = str_dup(str);
    return true;
}

static bool DisplayState_Deserialize(benc_dict* dict, DisplayState *ds)
{
    DisplayState_Init(ds);

    dict_get_str_dup(dict, FILE_STR, &ds->filePath);
    const char* txt = dict_get_str(dict, DISPLAY_MODE_STR);
    if (txt)
        DisplayModeEnumFromName(txt, &ds->displayMode);
    dict_get_bool(dict, VISIBLE_STR, &ds->visible);
    dict_get_int(dict, PAGE_NO_STR, &ds->pageNo);
    dict_get_int(dict, ROTATION_STR, &ds->rotation);
    dict_get_bool(dict, FULLSCREEN_STR, &ds->fullScreen);
    dict_get_int(dict, SCROLL_X_STR, &ds->scrollX);
    dict_get_int(dict, SCROLL_Y_STR, &ds->scrollY);
    dict_get_int(dict, WINDOW_X_STR, &ds->windowX);
    dict_get_int(dict, WINDOW_Y_STR, &ds->windowY);
    dict_get_int(dict, WINDOW_DX_STR, &ds->windowDx);
    dict_get_int(dict, WINDOW_DY_STR, &ds->windowDy);
    dict_get_bool(dict, SHOW_TOC_STR, &ds->showToc);
    dict_get_double_from_str(dict, ZOOM_VIRTUAL_STR, &ds->zoomVirtual);
    return true;
}

static benc_dict* DisplayState_Serialize(DisplayState *ds)
{
    BOOL  ok;
    const char * txt;
    
    DICT_NEW(prefs);
    DICT_ADD_STR(prefs, FILE_STR, ds->filePath);
    txt = DisplayModeNameFromEnum(ds->displayMode);
    if (txt)
        DICT_ADD_STR(prefs, DISPLAY_MODE_STR, txt);
    DICT_ADD_INT64(prefs, VISIBLE_STR, ds->visible);
    DICT_ADD_INT64(prefs, PAGE_NO_STR, ds->pageNo);
    DICT_ADD_INT64(prefs, ROTATION_STR, ds->rotation);
    DICT_ADD_INT64(prefs, FULLSCREEN_STR, ds->fullScreen);
    DICT_ADD_INT64(prefs, SCROLL_X_STR, ds->scrollX);
    DICT_ADD_INT64(prefs, SCROLL_Y_STR, ds->scrollY);
    DICT_ADD_INT64(prefs, WINDOW_X_STR, ds->windowX);
    DICT_ADD_INT64(prefs, WINDOW_Y_STR, ds->windowY);
    DICT_ADD_INT64(prefs, WINDOW_DX_STR, ds->windowDx);
    DICT_ADD_INT64(prefs, WINDOW_DY_STR, ds->windowDy);
    DICT_ADD_INT64(prefs, SHOW_TOC_STR, ds->showToc);

    txt = str_printf("%.4f", ds->zoomVirtual);
    if (txt) {
        DICT_ADD_STR(prefs, ZOOM_VIRTUAL_STR, txt);
        free((void*)txt);
    }

    return prefs;
Error:
    benc_obj_delete((benc_obj*)prefs);
    return NULL;
}

static benc_dict* FileHistoryList_Node_Serialize2(FileHistoryList *node)
{
    assert(node);
    if (!node) return NULL;

    return DisplayState_Serialize(&(node->state));
}

benc_array* FileHistoryList_Serialize(FileHistoryList **root)
{
    BOOL ok;
    assert(root);
    if (!root) return NULL;

    benc_array* arr = benc_array_new();
    if (!arr)
        goto Error;

    FileHistoryList *curr = *root;
    while (curr) {
        benc_obj* bobj = (benc_obj*) FileHistoryList_Node_Serialize2(curr);
        if (!bobj)
            goto Error;
        ok = benc_array_append(arr, bobj);
        if (!ok)
            goto Error;
        curr = curr->next;
    }
    return arr;
Error:
    if (arr)
        benc_array_delete(arr);
    return NULL;      
}

const char *Prefs_Serialize(FileHistoryList **root, size_t* lenOut)
{
    BOOL        ok;
    char *      data = NULL;

    DICT_NEW(prefs);

    benc_dict* global = Prefs_SerializeGlobal();
    if (!global)
        goto Error;
    DICT_ADD_DICT(prefs, GLOBAL_PREFS_STR, global);
    benc_array *fileHistory = FileHistoryList_Serialize(root);
    if (!fileHistory)
        goto Error;
    DICT_ADD_DICT(prefs, FILE_HISTORY_STR, fileHistory);

    data = benc_obj_to_data((benc_obj*)prefs, lenOut);
Error:
    benc_obj_delete((benc_obj*)prefs);
    return (const char*)data;
}

static BOOL ParseInt(const char *txt, int *resOut)
{
    assert(txt);
    if (!txt) return FALSE;
    *resOut = atoi(txt);
    return TRUE;
}

static BOOL ParseBool(const char *txt, BOOL *resOut)
{
    assert(txt);
    if (!txt) return FALSE;
    int val = atoi(txt);
    if (val)
        *resOut = TRUE;
    else
        *resOut = FALSE;
    return TRUE;
}

enum PrefsParsingState { PPS_START, PPS_IN_FILE_HISTORY };

/* Return TRUE if 'str' is a comment line in preferences file.
   Comment lines start with '#'. */
static int Prefs_LineIsComment(const char *str)
{
    if (!str)
        return FALSE;
    if ('#' == *str)
        return TRUE;
    return FALSE;
}

static int Prefs_LineIsStructKey(const char *str)
{
    if (strlen(str) <= 3)
        return FALSE;
    if ((' ' == str[0]) && (' ' == str[1]))
        return TRUE;
    return FALSE;
}

static void ParseKeyValue(char *key, char *value, DisplayState *dsOut)
{
    BOOL    fOk;

    assert(key);
    assert(value);
    assert(dsOut);
    if (!key || !value || !dsOut)
        return;

    if (str_eq(FILE_STR, key)) {
        assert(value);
        if (!value) return;
        assert(!dsOut->filePath);
        free((void*)dsOut->filePath);
        dsOut->filePath = str_dup(value);
        return;
    }

    if (str_eq(DISPLAY_MODE_STR, key)) {
        dsOut->displayMode = DM_SINGLE_PAGE;
        fOk = ParseDisplayMode(value, &dsOut->displayMode);
        assert(fOk);
        return;
    }

    if (str_eq(PAGE_NO_STR, key)) {
        fOk = ParseInt(value, &dsOut->pageNo);
        assert(fOk);
        if (!fOk || (dsOut->pageNo < 1))
            dsOut->pageNo = 1;
        return;
    }

    if (str_eq(ZOOM_VIRTUAL_STR, key)) {
        fOk = str_to_double(value, &dsOut->zoomVirtual);
        assert(fOk);
        if (!fOk || !ValidZoomVirtual(dsOut->zoomVirtual))
            dsOut->zoomVirtual = 100.0;
        return;
    }

    if (str_eq(ROTATION_STR, key)) {
        fOk = ParseInt(value, &dsOut->rotation);
        assert(fOk);
        if (!fOk || !validRotation(dsOut->rotation))
            dsOut->rotation = 0;
        return;
    }

    if (str_eq(VISIBLE_STR, key)) {
        dsOut->visible= FALSE;
        fOk = ParseBool(value, &dsOut->visible);
        assert(fOk);
        return;
    }

    if (str_eq(FULLSCREEN_STR, key)) {
        dsOut->fullScreen = FALSE;
        fOk = ParseBool(value, &dsOut->fullScreen);
        assert(fOk);
        return;
    }

    if (str_eq(SCROLL_X_STR, key)) {
        dsOut->scrollX = 0;
        fOk = ParseInt(value, &dsOut->scrollX);
        assert(fOk);
        return;
    }

    if (str_eq(SCROLL_Y_STR, key)) {
        dsOut->scrollY = 0;
        fOk = ParseInt(value, &dsOut->scrollY);
        assert(fOk);
        return;
    }

    if (str_eq(WINDOW_X_STR, key)) {
        dsOut->windowX = DEFAULT_WINDOW_X;
        fOk = ParseInt(value, &dsOut->windowX);
        assert(fOk);
        return;
    } 

    if (str_eq(WINDOW_Y_STR, key)) {
        dsOut->windowY = DEFAULT_WINDOW_Y;
        fOk = ParseInt(value, &dsOut->windowY);
        assert(fOk);
        return;
    }

    if (str_eq(WINDOW_DX_STR, key)) {
        dsOut->windowDx = DEFAULT_WINDOW_DX;
        fOk = ParseInt(value, &dsOut->windowDx);
        assert(fOk);
        return;
    }

    if (str_eq(WINDOW_DY_STR, key)) {
        dsOut->windowDy = DEFAULT_WINDOW_DY;
        fOk = ParseInt(value, &dsOut->windowDy);
        assert(fOk);
        return;
    }

    assert(0);
}

void FileHistory_Add(FileHistoryList **fileHistoryRoot, DisplayState *state)
{
    FileHistoryList *   fileHistoryNode = NULL;
    if (!file_exists(state->filePath)) {
        DBG_OUT("FileHistory_Add() file '%s' doesn't exist anymore\n", state->filePath);
        return;
    }

    fileHistoryNode = FileHistoryList_Node_Create();
    fileHistoryNode->state = *state;
    FileHistoryList_Node_Append(fileHistoryRoot, fileHistoryNode);
    fileHistoryNode = NULL;
}

bool Prefs_Deserialize(const char *prefsTxt, size_t prefsTxtLen, FileHistoryList **fileHistoryRoot)
{
    benc_obj * bobj;
    benc_str * bstr;
    bobj = benc_obj_from_data(prefsTxt, prefsTxtLen);
    if (!bobj)
        return false;
    benc_dict* prefs = benc_obj_as_dict(bobj);
    if (!prefs)
        goto Error;

    benc_dict* global = benc_obj_as_dict(benc_dict_find2(prefs, GLOBAL_PREFS_STR));
    if (!global)
        goto Error;

    dict_get_bool(global, SHOW_TOOLBAR_STR, &gGlobalPrefs.m_showToolbar);
    dict_get_bool(global, USE_FITZ_STR, &gGlobalPrefs.m_useFitz);
    dict_get_bool(global, PDF_ASSOCIATE_DONT_ASK_STR, &gGlobalPrefs.m_pdfAssociateDontAskAgain);
    dict_get_bool(global, PDF_ASSOCIATE_ASSOCIATE_STR, &gGlobalPrefs.m_pdfAssociateShouldAssociate);
    dict_get_bool(global, ESC_TO_EXIT_STR, &gGlobalPrefs.m_escToExit);
    dict_get_bool(global, FULLSCREEN_STR, &gGlobalPrefs.m_fullScreen);
    dict_get_int(global, BG_COLOR_STR, &gGlobalPrefs.m_bgColor);
    const char* txt = dict_get_str(global, DISPLAY_MODE_STR);
    if (txt)
        DisplayModeEnumFromName(txt, &gGlobalPrefs.m_defaultDisplayMode);
    dict_get_double_from_str(global, ZOOM_VIRTUAL_STR, &gGlobalPrefs.m_defaultZoom);
    dict_get_int(global, WINDOW_STATE_STR, &gGlobalPrefs.m_windowState);
    dict_get_int(global, WINDOW_X_STR, &gGlobalPrefs.m_windowPosX);
    dict_get_int(global, WINDOW_Y_STR, &gGlobalPrefs.m_windowPosY);
    dict_get_int(global, WINDOW_DX_STR, &gGlobalPrefs.m_windowDx);
    dict_get_int(global, WINDOW_DY_STR, &gGlobalPrefs.m_windowDy);
    const char* invsearch = dict_get_str(global, INVERSE_SEARCH_COMMANDLINE);
    if (invsearch) {
        str_copy(gGlobalPrefs.m_inversesearch_cmdline,
            dimof(gGlobalPrefs.m_inversesearch_cmdline), invsearch);
    }

    bstr = benc_obj_as_str(benc_dict_find2(global, UI_LANGUAGE_STR));
    if (bstr)
        CurrLangNameSet(bstr->m_str);

    benc_array* fileHistory = benc_obj_as_array(benc_dict_find2(prefs, FILE_HISTORY_STR));
    if (!fileHistory)
        goto Error;
    size_t dlen = benc_array_len(fileHistory);
    for (size_t i = 0; i < dlen; i++) {
        DisplayState state;
        benc_dict *dict = benc_obj_as_dict(benc_array_get(fileHistory, i));
        assert(dict);
        if (!dict) continue;
        DisplayState_Deserialize(dict, &state);
        if (state.filePath) {
            if (file_exists(state.filePath))
                FileHistory_Add(fileHistoryRoot, &state);
        }
    }
    benc_obj_delete(bobj);
    return true;
Error:
    benc_obj_delete(bobj);
    return false;
}

/* Deserialize preferences from text. Put state into 'dsOut' and add all history
   items to file history list 'root'.
   Return FALSE if there was an error.
   An ode to a state machine. */
bool Prefs_DeserializeOld(const char *prefsTxt, FileHistoryList **fileHistoryRoot)
{
    PrefsParsingState   state = PPS_START;
    char *              prefsTxtNormalized = NULL;
    char *              strTmp = NULL;
    char *              line;
    char *              key, *value, *keyToFree = NULL;
    int                 isStructVal;
    DisplayState        currState;

    DisplayState_Init(&currState);

    prefsTxtNormalized = str_normalize_newline(prefsTxt, UNIX_NEWLINE);
    if (!prefsTxtNormalized)
        goto Exit;

    strTmp = prefsTxtNormalized;
    for (;;) {
        line = str_split_iter(&strTmp, UNIX_NEWLINE_C);
        if (!line)
            break;
        str_strip_ws_right(line);

        /* skip empty and comment lines*/
        if (str_empty(line))
            goto Next;
        if (Prefs_LineIsComment(line))
            goto Next;

        /* each line is key/value pair formatted as: "key: value"
           value is optional. If value exists, there must
           be a space after ':' */
        value = line;
        keyToFree = str_split_iter(&value, ':');
        key = keyToFree;
        assert(key);
        if (!key)
            goto Next;
        if (str_empty(value)) {
            value = NULL; /* there was no value */
        } else {
            assert(' ' == *value);
            if (' ' != *value)
                goto Next;
            value += 1;
        }
        isStructVal = Prefs_LineIsStructKey(key);
        if (isStructVal)
            key += 2;

StartOver:
        switch (state) {
            case PPS_START:
                if (str_eq(SHOW_TOOLBAR_STR, key)) {
                    gGlobalPrefs.m_showToolbar = TRUE;
                    ParseBool(value, &gGlobalPrefs.m_showToolbar);
                    break;
                }
                if (str_eq(USE_FITZ_STR, key)) {
                    gGlobalPrefs.m_useFitz = TRUE;
                    ParseBool(value, &gGlobalPrefs.m_useFitz);
                    break;
                }
                if (str_eq(PDF_ASSOCIATE_DONT_ASK_STR, key)) {
                    gGlobalPrefs.m_pdfAssociateDontAskAgain = FALSE;
                    ParseBool(value, &gGlobalPrefs.m_pdfAssociateDontAskAgain);
                    break;
                }
                if (str_eq(PDF_ASSOCIATE_ASSOCIATE_STR, key)) {
                    gGlobalPrefs.m_pdfAssociateShouldAssociate = TRUE;
                    ParseBool(value, &gGlobalPrefs.m_pdfAssociateShouldAssociate);
                    break;
                }
                if (str_eq(UI_LANGUAGE_STR, key)) {
                    CurrLangNameSet(value);
                    break;
                }
                if (str_eq(FILE_HISTORY_STR, key)) {
                    assert(!isStructVal);
                    state = PPS_IN_FILE_HISTORY;
                    assert(!value);
                } else {
                    if (line)
                        DBG_OUT("  in state PPS_START, line='%s'  \n\n", line);
                    else
                        DBG_OUT("  in state PPS_START, line is NULL\n\n");
                    assert(0);
                }
                break;

            case PPS_IN_FILE_HISTORY:
                if (isStructVal) {
                    ParseKeyValue(key, value, &currState);
                } else {
                    if (currState.filePath) {
                        FileHistory_Add(fileHistoryRoot, &currState);
                        DisplayState_Init(&currState);
                    }
                    state = PPS_START;
                    goto StartOver;
                }
                break;

        }
Next:
        free((void*)keyToFree);
        keyToFree = NULL;
        free((void*)line);
        line = NULL;
    }

    if (PPS_IN_FILE_HISTORY == state) {
        if (currState.filePath) {
            if (file_exists(currState.filePath))
                FileHistory_Add(fileHistoryRoot, &currState);
        }
    }
Exit:
    free((void*)prefsTxtNormalized);
    return TRUE;
}

