/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/Win.h"
#include "base/SettingsUtil.h"

#define INCLUDE_SETTINGSSTRUCTS_METADATA
#include "Settings.h"

#include "GlobalPrefs.h"

GlobalPrefs* gGlobalPrefs = nullptr;

FileState* NewFileState(Str filePath) {
    FileState* fs = (FileState*)DeserializeStruct(&gFileStateInfo, nullptr);
    SetFileStatePath(fs, filePath);
    return fs;
}

void DeleteFileState(FileState* fs) {
    delete fs->thumbnail;
    FreeStruct(&gFileStateInfo, fs);
}

void DeleteFileStates(Vec<FileState*>* a) {
    for (auto* fs : *a) {
        DeleteFileState(fs);
    }
    delete a;
}

Favorite* NewFavorite(int pageNo, Str name, Str pageLabel) {
    Favorite* fav = (Favorite*)DeserializeStruct(&gFavoriteInfo, nullptr);
    fav->pageNo = pageNo;
    str::ReplaceWithCopy(&fav->name, name);
    str::ReplaceWithCopy(&fav->pageLabel, pageLabel);
    return fav;
}

void DeleteFavorite(Favorite* fav) {
    FreeStruct(&gFavoriteInfo, fav);
}

GlobalPrefs* NewGlobalPrefs(Str data) {
    return (GlobalPrefs*)DeserializeStruct(&gGlobalPrefsInfo, data);
}

// With file history turned off the only thing worth keeping about a file is a
// favorite the user added on purpose. Everything else in a FileState is history
// by definition, and some entries have no user content at all: searching
// creates one just to hang the session-only "jump back here" favorite on
// (SetSearchStartFavorite), which left a bare FilePath in the settings file of
// someone who asked us not to remember opened files (issue #5899).
static bool FileStateWorthKeepingWithoutHistory(FileState* fs) {
    if (!fs || !fs->favorites) {
        return false;
    }
    for (Favorite* fav : *fs->favorites) {
        if (!fav->isTemporary) {
            return true;
        }
    }
    return false;
}

// prevData is used to preserve fields that exists in prevField but not in GlobalPrefs
// caller has to free()
Str SerializeGlobalPrefs(GlobalPrefs* prefs, Str prevData) {
    Vec<FileState*>* allFileStates = prefs->fileStates;
    Vec<FileState*> withFavorites;

    // The two settings mean different things and must not be conflated (#5907):
    // RememberStatePerDocument = false only drops the per-document state (page,
    // zoom, scroll, window placement) - the list of files stays, because that is
    // what the home page shows. RememberOpenedFiles = false drops the list
    // itself, keeping only files the user hung a favorite on (#5899).
    bool dropPerDocState = !prefs->rememberStatePerDocument || !prefs->rememberOpenedFiles;
    bool dropHistory = !prefs->rememberOpenedFiles;

    if (dropPerDocState) {
        for (FileState* fs : *prefs->fileStates) {
            fs->useDefaultState = true;
            if (FileStateWorthKeepingWithoutHistory(fs)) {
                withFavorites.Append(fs);
            }
        }
        if (dropHistory) {
            // serialize the filtered list, then put the real one back below -
            // the in-memory history is still needed for this session
            prefs->fileStates = &withFavorites;
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

    Str serialized = SerializeStruct(&gGlobalPrefsInfo, prefs, prevData);

    if (dropPerDocState) {
        gFileStateInfo.fieldCount = dimof(gFileStateFields);
        prefs->fileStates = allFileStates;
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

TabState* NewTabState(FileState* fs) {
    TabState* state = (TabState*)DeserializeStruct(&gTabStateInfo, nullptr);
    str::ReplaceWithCopy(&state->filePath, fs->filePath);
    str::ReplaceWithCopy(&state->displayMode, fs->displayMode);
    state->pageNo = fs->pageNo;
    str::ReplaceWithCopy(&state->zoom, fs->zoom);
    state->rotation = fs->rotation;
    state->scrollPos = fs->scrollPos;
    state->showToc = fs->showToc;
    *state->tocState = *fs->tocState;
    return state;
}

void DeleteTabState(TabState* state) {
    FreeStruct(&gTabStateInfo, state);
}

void FreeSessionData(SessionData* data) {
    FreeStruct(&gSessionDataInfo, data);
}

void FreeSessionDataVec(Vec<SessionData*>* sessionData) {
    ReportIf(!sessionData);
    if (!sessionData) {
        return;
    }
    for (SessionData* data : *sessionData) {
        FreeSessionData(data);
    }
    sessionData->Reset();
}

ParsedColor* GetParsedColor(Str s, ParsedColor& parsed) {
    if (parsed.wasParsed) {
        return &parsed;
    }
    ParseColor(parsed, s);
    return &parsed;
}

COLORREF GetParsedCOLORREF(Str s, ParsedColor& parsed, COLORREF def) {
    if (parsed.wasParsed && parsed.parsedOk) {
        return parsed.col;
    }
    ParseColor(parsed, s);
    if (parsed.parsedOk) {
        return parsed.col;
    }
    return def;
}

void SetFileStatePath(FileState* fs, Str path) {
    if (fs->filePath && str::EqI(fs->filePath, path)) {
        return;
    }
    str::ReplaceWithCopy(&fs->filePath, path);
}

void SetFileStatePath(FileState* fs, WStr path) {
    SetFileStatePath(fs, ToUtf8Temp(path));
}

Themes* ParseThemes(Str data) {
    return (Themes*)DeserializeStruct(&gThemesInfo, data);
}

void FreeParsedThemes(Themes* themes) {
    if (!themes) {
        return;
    }
    FreeStruct(&gThemesInfo, themes);
}
