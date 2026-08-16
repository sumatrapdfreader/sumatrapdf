/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/File.h"

#include "Settings.h"
#include "DisplayMode.h"
#include "GlobalPrefs.h"
#include "gui/DocumentView.h"
#include "linux/LinuxPrefs.h"

static Str settingsPath;
static Str previousSettings;

static FileState* FindFileState(Str path) {
    if (!gGlobalPrefs || !gGlobalPrefs->fileStates) {
        return nullptr;
    }
    for (FileState* state : *gGlobalPrefs->fileStates) {
        if (str::Eq(state->filePath, path)) {
            return state;
        }
    }
    return nullptr;
}

static void MoveFileStateToFront(FileState* state) {
    Vec<FileState*>* states = gGlobalPrefs->fileStates;
    states->Remove(state);
    states->InsertAt(0, state);
}

void LinuxPrefsInit() {
    const char* xdgConfig = getenv("XDG_CONFIG_HOME");
    const char* home = getenv("HOME");
    TempStr configDir;
    if (xdgConfig && xdgConfig[0]) {
        configDir = str::DupTemp(xdgConfig);
    } else if (home && home[0]) {
        configDir = path::JoinTemp(Str(home), StrL(".config"));
    } else {
        configDir = str::DupTemp(".");
    }
    settingsPath = path::Join(nullptr, configDir, StrL("sumatrapdf/SumatraPDF-settings.txt"));
    previousSettings = file::ReadFile(settingsPath);
    gGlobalPrefs = NewGlobalPrefs(previousSettings);
}

static void SaveSettings() {
    if (!gGlobalPrefs || !settingsPath) {
        return;
    }
    Str serialized = SerializeGlobalPrefs(gGlobalPrefs, previousSettings);
    if (dir::CreateForFile(settingsPath) && file::WriteFile(settingsPath, serialized)) {
        str::Free(previousSettings);
        previousSettings = str::Dup(serialized);
    }
    str::Free(serialized);
}

static void ApplyViewState(DocumentView* view, Str displayMode, Str zoom, int rotation, int pageNo) {
    if (!view) {
        return;
    }
    DisplayMode mode = DisplayModeFromString(displayMode, DisplayMode::Continuous);
    view->SetContinuous(IsContinuous(mode));
    view->SetZoom(ZoomFromString(zoom, kZoomFitWidth));
    view->RotateBy(rotation - view->Rotation());
    view->GoToPage(pageNo);
}

void LinuxPrefsShutdown() {
    SaveSettings();
    DeleteGlobalPrefs(gGlobalPrefs);
    gGlobalPrefs = nullptr;
    str::Free(previousSettings);
    str::Free(settingsPath);
}

void LinuxPrefsOpenView(DocumentView* view, Str path) {
    if (!view || !path || !gGlobalPrefs) {
        return;
    }
    FileState* state = FindFileState(path);
    if (state && !state->useDefaultState) {
        ApplyViewState(view, state->displayMode, state->zoom, state->rotation, state->pageNo);
    }
    if (!state) {
        state = NewFileState(path);
    }
    MoveFileStateToFront(state);
    state->isMissing = false;
    state->openCount++;
}

void LinuxPrefsSaveView(DocumentView* view, Str path) {
    if (!view || !path || !gGlobalPrefs) {
        return;
    }
    FileState* state = FindFileState(path);
    if (!state) {
        state = NewFileState(path);
        MoveFileStateToFront(state);
    }
    state->useDefaultState = false;
    DisplayMode mode = view->IsContinuous() ? DisplayMode::Continuous : DisplayMode::SinglePage;
    str::ReplaceWithCopy(&state->displayMode, DisplayModeToString(mode));
    state->pageNo = view->CurrentPageNo();
    ZoomToString(&state->zoom, view->Zoom(), state);
    state->rotation = view->Rotation();
}

void LinuxPrefsSaveSession(const Vec<DocumentView*>& views, const StrVec& paths, int activeTab) {
    if (!gGlobalPrefs || !gGlobalPrefs->sessionData) {
        return;
    }
    FreeSessionDataVec(gGlobalPrefs->sessionData);
    if (!gGlobalPrefs->rememberOpenedFiles || len(views) != len(paths)) {
        return;
    }

    SessionData* session = NewSessionData();
    for (int i = 0; i < len(views); i++) {
        DocumentView* view = views[i];
        Str path = paths[i];
        if (!view || !path) {
            continue;
        }
        LinuxPrefsSaveView(view, path);
        FileState* state = FindFileState(path);
        if (state) {
            session->tabStates->Append(NewTabState(state));
        }
    }
    if (len(*session->tabStates) == 0) {
        FreeSessionData(session);
        return;
    }
    session->tabIndex = limitValue(activeTab + 1, 1, len(*session->tabStates));
    gGlobalPrefs->sessionData->Append(session);
}

static SessionData* SavedSession() {
    if (!gGlobalPrefs || !gGlobalPrefs->restoreSession || !gGlobalPrefs->rememberOpenedFiles ||
        !gGlobalPrefs->sessionData || len(*gGlobalPrefs->sessionData) == 0) {
        return nullptr;
    }
    return (*gGlobalPrefs->sessionData)[0];
}

int LinuxPrefsSessionTabCount() {
    SessionData* session = SavedSession();
    return session && session->tabStates ? len(*session->tabStates) : 0;
}

Str LinuxPrefsSessionPath(int index) {
    SessionData* session = SavedSession();
    if (!session || !session->tabStates || index < 0 || index >= len(*session->tabStates)) {
        return {};
    }
    return (*session->tabStates)[index]->filePath;
}

void LinuxPrefsRestoreSessionView(DocumentView* view, int index) {
    SessionData* session = SavedSession();
    if (!view || !session || !session->tabStates || index < 0 || index >= len(*session->tabStates)) {
        return;
    }
    TabState* state = (*session->tabStates)[index];
    ApplyViewState(view, state->displayMode, state->zoom, state->rotation, state->pageNo);
}

int LinuxPrefsSessionActiveTab() {
    SessionData* session = SavedSession();
    return session ? session->tabIndex - 1 : 0;
}

int LinuxPrefsRecentCount() {
    if (!gGlobalPrefs || !gGlobalPrefs->fileStates) {
        return 0;
    }
    int count = 0;
    for (FileState* state : *gGlobalPrefs->fileStates) {
        if (!state->isMissing && state->filePath) {
            count++;
            if (count == 10) {
                break;
            }
        }
    }
    return count;
}

Str LinuxPrefsRecentPath(int index) {
    if (index < 0 || index >= LinuxPrefsRecentCount()) {
        return {};
    }
    for (FileState* state : *gGlobalPrefs->fileStates) {
        if (state->isMissing || !state->filePath) {
            continue;
        }
        if (index == 0) {
            return state->filePath;
        }
        index--;
    }
    return {};
}

static Favorite* FindFavorite(FileState* state, int pageNo) {
    if (!state || !state->favorites) {
        return nullptr;
    }
    for (Favorite* favorite : *state->favorites) {
        if (!favorite->isTemporary && favorite->pageNo == pageNo) {
            return favorite;
        }
    }
    return nullptr;
}

bool LinuxPrefsAddFavorite(Str path, int pageNo) {
    if (!gGlobalPrefs || !path || pageNo < 1) {
        return false;
    }
    FileState* state = FindFileState(path);
    if (!state) {
        state = NewFileState(path);
        MoveFileStateToFront(state);
    }
    if (FindFavorite(state, pageNo)) {
        return false;
    }
    state->favorites->Append(NewFavorite(pageNo, {}, {}));
    return true;
}

bool LinuxPrefsRemoveFavorite(Str path, int pageNo) {
    FileState* state = FindFileState(path);
    Favorite* favorite = FindFavorite(state, pageNo);
    if (!favorite) {
        return false;
    }
    state->favorites->Remove(favorite);
    DeleteFavorite(favorite);
    return true;
}

static Favorite* GetFavoriteAt(int index, FileState** stateOut = nullptr) {
    if (!gGlobalPrefs || !gGlobalPrefs->fileStates || index < 0) {
        return nullptr;
    }
    for (FileState* state : *gGlobalPrefs->fileStates) {
        if (!state->favorites) {
            continue;
        }
        for (Favorite* favorite : *state->favorites) {
            if (favorite->isTemporary) {
                continue;
            }
            if (index == 0) {
                if (stateOut) {
                    *stateOut = state;
                }
                return favorite;
            }
            index--;
        }
    }
    return nullptr;
}

int LinuxPrefsFavoriteCount() {
    int count = 0;
    if (!gGlobalPrefs || !gGlobalPrefs->fileStates) {
        return count;
    }
    for (FileState* state : *gGlobalPrefs->fileStates) {
        if (!state->favorites) {
            continue;
        }
        for (Favorite* favorite : *state->favorites) {
            count += !favorite->isTemporary;
        }
    }
    return count;
}

Str LinuxPrefsFavoritePath(int index) {
    FileState* state = nullptr;
    return GetFavoriteAt(index, &state) && state ? state->filePath : Str{};
}

Str LinuxPrefsFavoriteLabel(int index) {
    Favorite* favorite = GetFavoriteAt(index);
    if (!favorite) {
        return {};
    }
    return favorite->name ? favorite->name : favorite->pageLabel;
}

int LinuxPrefsFavoritePageNo(int index) {
    Favorite* favorite = GetFavoriteAt(index);
    return favorite ? favorite->pageNo : 0;
}
