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
        if (str::EqI(state->filePath, path)) {
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
        DisplayMode mode = DisplayModeFromString(state->displayMode, DisplayMode::Continuous);
        view->SetContinuous(IsContinuous(mode));
        view->SetZoom(ZoomFromString(state->zoom, kZoomFitWidth));
        view->RotateBy(state->rotation - view->Rotation());
        view->GoToPage(state->pageNo);
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
