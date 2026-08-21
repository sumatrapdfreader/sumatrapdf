/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/Pixmap.h"
#include "base/SettingsUtil.h"

#define INCLUDE_SETTINGSSTRUCTS_METADATA
#include "Settings.h"

#include "GlobalPrefs.h"

GlobalPrefs* gGlobalPrefs = nullptr;

// Walk setting metadata for a Bool field matching name (case-insensitive leaf
// or full dotted path). Returns a pointer into gGlobalPrefs, or nullptr.
static bool* FindBoolSettingInStruct(const StructInfo* info, u8* base, Str pathPrefix, Str name) {
    if (!info || !base || len(name) == 0) {
        return nullptr;
    }
    const char* fieldName = info->fieldNames;
    for (u16 i = 0; i < info->fieldCount; i++) {
        const FieldInfo& field = info->fields[i];
        Str fname(fieldName);
        fieldName += len(fname) + 1;
        if (field.type == SettingType::Comment || field.offset == (size_t)-1) {
            continue;
        }
        u8* fieldPtr = base + field.offset;
        TempStr path = len(pathPrefix) > 0 ? fmt("%s.%s", pathPrefix, fname) : str::DupTemp(fname);
        if (field.type == SettingType::Struct) {
            const auto* sub = (const StructInfo*)field.value;
            bool* found = FindBoolSettingInStruct(sub, fieldPtr, path, name);
            if (found) {
                return found;
            }
            continue;
        }
        if (field.type != SettingType::Bool) {
            continue;
        }
        if (str::EqI(fname, name) || str::EqI(path, name)) {
            return (bool*)fieldPtr;
        }
    }
    return nullptr;
}

// Case-insensitive leaf or dotted path (e.g. "SelectionToolbar", "Fullscreen.ShowMenubar").
bool* FindGlobalPrefsBoolSetting(Str name) {
    if (!gGlobalPrefs || len(name) == 0) {
        return nullptr;
    }
    return FindBoolSettingInStruct(&gGlobalPrefsInfo, (u8*)gGlobalPrefs, {}, name);
}

FileState* NewFileState(Str filePath) {
    FileState* fs = (FileState*)DeserializeStruct(&gFileStateInfo, nullptr);
    SetFileStatePath(fs, filePath);
    return fs;
}

FileEBookUI* NewFileEBookUI() {
    return (FileEBookUI*)DeserializeStruct(&gFileEBookUIInfo, nullptr);
}

FileEBookUI* CopyFileEBookUI(const FileEBookUI* src) {
    if (!src) {
        return nullptr;
    }
    auto* res = NewFileEBookUI();
    str::ReplaceWithCopy(&res->fontName, src->fontName);
    res->fontSize = src->fontSize;
    res->lineSpacing = src->lineSpacing;
    res->layoutDx = src->layoutDx;
    res->layoutDy = src->layoutDy;
    str::ReplaceWithCopy(&res->ignoreDocumentCSS, src->ignoreDocumentCSS);
    str::ReplaceWithCopy(&res->customCSS, src->customCSS);
    return res;
}

void DeleteFileEBookUI(FileEBookUI* v) {
    if (v) {
        FreeStruct(&gFileEBookUIInfo, v);
    }
}

void DeleteFileState(FileState* fs) {
    FreePixmap(fs->thumbnail);
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
    if (!fs) {
        return false;
    }
    // per-document ebook settings are configuration the user typed, not history
    if (fs->eBookUI) {
        return true;
    }
    if (!fs->favorites) {
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
        FreePixmap(ds->thumbnail);
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
