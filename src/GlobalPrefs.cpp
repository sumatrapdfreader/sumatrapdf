/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "utils/BaseUtil.h"
#include "utils/ScopedWin.h"
#include "utils/FileUtil.h"
#include "utils/SettingsUtil.h"
#include "utils/Log.h"

#include "wingui/TreeModel.h"

#include "EngineBase.h"
#include "DisplayMode.h"
#define INCLUDE_SETTINGSSTRUCTS_METADATA
#include "SettingsStructs.h"
#include "GlobalPrefs.h"

GlobalPrefs* gGlobalPrefs = nullptr;

FileState* NewDisplayState(const WCHAR* filePath) {
    FileState* ds = (FileState*)DeserializeStruct(&gFileStateInfo, nullptr);
    str::ReplaceWithCopy(&ds->filePath, filePath);
    return ds;
}

void DeleteDisplayState(FileState* ds) {
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

// prevData is used to preserve fields that exists in prevField but not in GlobalPrefs
std::span<u8> SerializeGlobalPrefs(GlobalPrefs* prefs, const char* prevData) {
    if (!prefs->rememberStatePerDocument || !prefs->rememberOpenedFiles) {
        for (FileState* ds : *prefs->fileStates) {
            ds->useDefaultState = true;
        }
        // prevent unnecessary settings from being written out
        u16 fieldCount = 0;
        while (++fieldCount <= dimof(gFileStateFields)) {
            // count the number of fields up to and including useDefaultState
            if (gFileStateFields[fieldCount - 1].offset == offsetof(FileState, useDefaultState)) {
                break;
            }
        }
        // restore the correct fieldCount ASAP after serialization
        gFileStateInfo.fieldCount = fieldCount;
    }

    std::span<u8> serialized = SerializeStruct(&gGlobalPrefsInfo, prefs, prevData);

    if (!prefs->rememberStatePerDocument || !prefs->rememberOpenedFiles) {
        gFileStateInfo.fieldCount = dimof(gFileStateFields);
    }

    return serialized;
}

void DeleteGlobalPrefs(GlobalPrefs* gp) {
    if (!gp) {
        return;
    }

    for (FileState* ds : *gp->fileStates) {
        delete ds->thumbnail;
    }
    FreeStruct(&gGlobalPrefsInfo, gp);
}

SessionData* NewSessionData() {
    return (SessionData*)DeserializeStruct(&gSessionDataInfo, nullptr);
}

TabState* NewTabState(FileState* ds) {
    TabState* state = (TabState*)DeserializeStruct(&gTabStateInfo, nullptr);
    auto dsFilePathA = ToUtf8Temp(ds->filePath);
    str::ReplaceWithCopy(&state->filePath, dsFilePathA);
    str::ReplaceWithCopy(&state->displayMode, ds->displayMode);
    state->pageNo = ds->pageNo;
    str::ReplaceWithCopy(&state->zoom, ds->zoom);
    state->rotation = ds->rotation;
    state->scrollPos = ds->scrollPos;
    state->showToc = ds->showToc;
    *state->tocState = *ds->tocState;
    return state;
}

void ResetSessionState(Vec<SessionData*>* sessionData) {
    CrashIf(!sessionData);
    if (!sessionData) {
        return;
    }
    for (SessionData* data : *sessionData) {
        FreeStruct(&gSessionDataInfo, data);
    }
    sessionData->Reset();
}

ParsedColor* GetParsedColor(const char* s, ParsedColor& parsed) {
    if (parsed.wasParsed) {
        return &parsed;
    }
    ParseColor(parsed, s);
    return &parsed;
}
