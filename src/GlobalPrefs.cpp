/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "utils/BaseUtil.h"
#include "utils/ScopedWin.h"
#include "utils/FileUtil.h"
#include "utils/SettingsUtil.h"
#include "utils/Log.h"

#include "wingui/TreeModel.h"
#include "EngineBase.h"
#define INCLUDE_SETTINGSSTRUCTS_METADATA
#include "SettingsStructs.h"
#include "GlobalPrefs.h"

GlobalPrefs* gGlobalPrefs = nullptr;

DisplayState* NewDisplayState(const WCHAR* filePath) {
    DisplayState* ds = (DisplayState*)DeserializeStruct(&gFileStateInfo, nullptr);
    str::ReplacePtr(&ds->filePath, filePath);
    return ds;
}

void DeleteDisplayState(DisplayState* ds) {
    delete ds->thumbnail;
    FreeStruct(&gFileStateInfo, ds);
}

Favorite* NewFavorite(int pageNo, const WCHAR* name, const WCHAR* pageLabel) {
    Favorite* fav = (Favorite*)DeserializeStruct(&gFavoriteInfo, nullptr);
    fav->pageNo = pageNo;
    fav->name = str::Dup(name);
    fav->pageLabel = str::Dup(pageLabel);
    return fav;
}

void DeleteFavorite(Favorite* fav) {
    FreeStruct(&gFavoriteInfo, fav);
}

GlobalPrefs* NewGlobalPrefs(const char* data) {
    return (GlobalPrefs*)DeserializeStruct(&gGlobalPrefsInfo, data);
}

// TODO: return std::string_view
char* SerializeGlobalPrefs(GlobalPrefs* gp, const char* prevData, size_t* sizeOut) {
    if (!gp->rememberStatePerDocument || !gp->rememberOpenedFiles) {
        for (DisplayState* ds : *gp->fileStates) {
            ds->useDefaultState = true;
        }
        // prevent unnecessary settings from being written out
        uint16_t fieldCount = 0;
        while (++fieldCount <= dimof(gFileStateFields)) {
            // count the number of fields up to and including useDefaultState
            if (gFileStateFields[fieldCount - 1].offset == offsetof(FileState, useDefaultState)) {
                break;
            }
        }
        // restore the correct fieldCount ASAP after serialization
        gFileStateInfo.fieldCount = fieldCount;
    }

    char* serialized = SerializeStruct(&gGlobalPrefsInfo, gp, prevData, sizeOut);

    if (!gp->rememberStatePerDocument || !gp->rememberOpenedFiles) {
        gFileStateInfo.fieldCount = dimof(gFileStateFields);
    }

    return serialized;
}

void DeleteGlobalPrefs(GlobalPrefs* gp) {
    if (!gp) {
        return;
    }

    for (DisplayState* ds : *gp->fileStates) {
        delete ds->thumbnail;
    }
    FreeStruct(&gGlobalPrefsInfo, gp);
}

SessionData* NewSessionData() {
    return (SessionData*)DeserializeStruct(&gSessionDataInfo, nullptr);
}

TabState* NewTabState(DisplayState* ds) {
    TabState* state = (TabState*)DeserializeStruct(&gTabStateInfo, nullptr);
    str::ReplacePtr(&state->filePath, ds->filePath);
    str::ReplacePtr(&state->displayMode, ds->displayMode);
    state->pageNo = ds->pageNo;
    str::ReplacePtr(&state->zoom, ds->zoom);
    state->rotation = ds->rotation;
    state->scrollPos = ds->scrollPos;
    state->showToc = ds->showToc;
    *state->tocState = *ds->tocState;
    return state;
}

void ResetSessionState(Vec<SessionData*>* sessionData) {
    CrashIf(!sessionData);
    for (SessionData* data : *sessionData) {
        FreeStruct(&gSessionDataInfo, data);
    }
    sessionData->Reset();
}
