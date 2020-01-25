/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "utils/BaseUtil.h"
#include "utils/FileUtil.h"
#include "utils/FileWatcher.h"
#include "utils/UITask.h"
#include "utils/ScopedWin.h"

#include "wingui/TreeModel.h"
#include "EngineBase.h"
#include "EngineEbook.h"

#include "SumatraConfig.h"
#include "SettingsStructs.h"
#include "FileHistory.h"
#include "GlobalPrefs.h"
#include "ProgressUpdateUI.h"
#include "Notifications.h"
#include "SumatraPDF.h"
#include "Flags.h"
#include "WindowInfo.h"
#include "AppPrefs.h"
#include "AppTools.h"
#include "Favorites.h"
#include "Toolbar.h"
#include "Translations.h"

static WatchedFile* gWatchedSettingsFile = nullptr;

// number of weeks past since 2011-01-01
static int GetWeekCount() {
    SYSTEMTIME date20110101 = {0};
    date20110101.wYear = 2011;
    date20110101.wMonth = 1;
    date20110101.wDay = 1;
    FILETIME origTime, currTime;
    BOOL ok = SystemTimeToFileTime(&date20110101, &origTime);
    CrashIf(!ok);
    GetSystemTimeAsFileTime(&currTime);
    return (currTime.dwHighDateTime - origTime.dwHighDateTime) / 1408;
    // 1408 == (10 * 1000 * 1000 * 60 * 60 * 24 * 7) / (1 << 32)
}

static int cmpFloat(const void* a, const void* b) {
    return *(float*)a < *(float*)b ? -1 : *(float*)a > *(float*)b ? 1 : 0;
}

namespace prefs {

WCHAR* GetSettingsFileNameNoFree() {
    if (gIsRaMicroBuild) {
        return L"RAMicroPDF-settings.txt";
    }
    return L"SumatraPDF-settings.txt";
}

WCHAR* GetSettingsPath() {
    return AppGenDataFilename(GetSettingsFileNameNoFree());
}

/* Caller needs to prefs::CleanUp() */
bool Load() {
    CrashIf(gGlobalPrefs);

    AutoFreeWstr path = GetSettingsPath();
    AutoFree prefsData = file::ReadFile(path.get());

    gGlobalPrefs = NewGlobalPrefs(prefsData.data);
    CrashAlwaysIf(!gGlobalPrefs);
    auto* gprefs = gGlobalPrefs;

    // in pre-release builds between 3.1.10079 and 3.1.10377,
    // RestoreSession was a string with the additional option "auto"
    // TODO: remove this after 3.2 has been released
#if defined(DEBUG) || defined(PRE_RELEASE_VER)
    if (!gprefs->restoreSession && prefsData.data && str::Find(prefsData.data, "\nRestoreSession = auto")) {
        gprefs->restoreSession = true;
    }
#endif

#ifdef DISABLE_EBOOK_UI
    if (!prefsData || !str::Find(prefsData, "UseFixedPageUI =")) {
        gprefs->ebookUI.useFixedPageUI = gprefs->chmUI.useFixedPageUI = true;
    }
#endif
#ifdef DISABLE_TABS
    if (!prefsData || !str::Find(prefsData, "UseTabs =")) {
        gprefs->useTabs = false;
    }
#endif

    if (!gprefs->uiLanguage || !trans::ValidateLangCode(gprefs->uiLanguage)) {
        // guess the ui language on first start
        str::ReplacePtr(&gprefs->uiLanguage, trans::DetectUserLang());
    }
    gprefs->lastPrefUpdate = file::GetModificationTime(path.get());
    gprefs->defaultDisplayModeEnum = conv::ToDisplayMode(gprefs->defaultDisplayMode, DM_AUTOMATIC);
    gprefs->defaultZoomFloat = conv::ToZoom(gprefs->defaultZoom, ZOOM_ACTUAL_SIZE);
    CrashIf(!IsValidZoom(gprefs->defaultZoomFloat));

    int weekDiff = GetWeekCount() - gprefs->openCountWeek;
    gprefs->openCountWeek = GetWeekCount();
    if (weekDiff > 0) {
        // "age" openCount statistics (cut in in half after every week)
        for (DisplayState* ds : *gprefs->fileStates) {
            ds->openCount >>= weekDiff;
        }
    }

    // make sure that zoom levels are in the order expected by DisplayModel
    gprefs->zoomLevels->Sort(cmpFloat);
    while (gprefs->zoomLevels->size() > 0 && gprefs->zoomLevels->at(0) < ZOOM_MIN) {
        gprefs->zoomLevels->PopAt(0);
    }
    while (gprefs->zoomLevels->size() > 0 && gprefs->zoomLevels->Last() > ZOOM_MAX) {
        gprefs->zoomLevels->Pop();
    }

    // TODO: verify that all states have a non-nullptr file path?
    gFileHistory.UpdateStatesSource(gprefs->fileStates);
    SetDefaultEbookFont(gprefs->ebookUI.fontName, gprefs->ebookUI.fontSize);

    if (!file::Exists(path.get())) {
        Save();
    }
    return true;
}

// called whenever global preferences change or a file is
// added or removed from gFileHistory (in order to keep
// the list of recently opened documents in sync)
bool Save() {
    // don't save preferences without the proper permission
    if (!HasPermission(Perm_SavePreferences)) {
        return false;
    }

    // update display states for all tabs
    for (WindowInfo* win : gWindows) {
        for (TabInfo* tab : win->tabs) {
            UpdateTabFileDisplayStateForWin(win, tab);
        }
    }

    // remove entries which should (no longer) be remembered
    gFileHistory.Purge(!gGlobalPrefs->rememberStatePerDocument);
    // update display mode and zoom fields from internal values
    str::ReplacePtr(&gGlobalPrefs->defaultDisplayMode, conv::FromDisplayMode(gGlobalPrefs->defaultDisplayModeEnum));
    conv::FromZoom(&gGlobalPrefs->defaultZoom, gGlobalPrefs->defaultZoomFloat);

    AutoFreeWstr path = GetSettingsPath();
    DebugCrashIf(!path.data);
    if (!path.data) {
        return false;
    }
    AutoFree prevPrefsData = file::ReadFile(path.data);
    size_t prefsDataSize = 0;
    AutoFree prefsData = SerializeGlobalPrefs(gGlobalPrefs, prevPrefsData.data, &prefsDataSize);

    CrashIf(!prefsData.data || 0 == prefsDataSize);
    if (!prefsData.data || 0 == prefsDataSize) {
        return false;
    }

    // only save if anything's changed at all
    if (prevPrefsData.size() == prefsDataSize && str::Eq(prefsData.get(), prevPrefsData.data)) {
        return true;
    }

    bool ok = file::WriteFile(path.get(), prefsData.as_view());
    if (!ok) {
        return false;
    }
    gGlobalPrefs->lastPrefUpdate = file::GetModificationTime(path.get());
    return true;
}

// refresh the preferences when a different SumatraPDF process saves them
// or if they are edited by the user using a text editor
bool Reload() {
    AutoFreeWstr path = GetSettingsPath();
    if (!file::Exists(path)) {
        return false;
    }

    // make sure that the settings file is readable - else wait
    // a short while to prevent accidental dataloss
    int tryAgainCount = 5;
    HANDLE h = file::OpenReadOnly(path);
    while (INVALID_HANDLE_VALUE == h && tryAgainCount-- > 0) {
        Sleep(200);
        h = file::OpenReadOnly(path);
    }
    if (INVALID_HANDLE_VALUE == h) {
        // prefer not reloading to resetting all settings
        return false;
    }

    AutoCloseHandle hScope(h);

    FILETIME time = file::GetModificationTime(path);
    if (FileTimeEq(time, gGlobalPrefs->lastPrefUpdate)) {
        return true;
    }

    AutoFree uiLanguage = str::Dup(gGlobalPrefs->uiLanguage);
    bool showToolbar = gGlobalPrefs->showToolbar;
    bool invertColors = gGlobalPrefs->fixedPageUI.invertColors;

    gFileHistory.UpdateStatesSource(nullptr);
    CleanUp();

    bool ok = Load();
    CrashAlwaysIf(!ok || !gGlobalPrefs);

    gGlobalPrefs->fixedPageUI.invertColors = invertColors;

    // TODO: about window doesn't have to be at position 0
    if (gWindows.size() > 0 && gWindows.at(0)->IsAboutWindow()) {
        gWindows.at(0)->HideInfoTip();
        gWindows.at(0)->staticLinks.Reset();
        gWindows.at(0)->RedrawAll(true);
    }

    if (!str::Eq(uiLanguage.get(), gGlobalPrefs->uiLanguage)) {
        SetCurrentLanguageAndRefreshUI(gGlobalPrefs->uiLanguage);
    }

    for (WindowInfo* win : gWindows) {
        if (gGlobalPrefs->showToolbar != showToolbar) {
            ShowOrHideToolbar(win);
        }
        UpdateFavoritesTree(win);
        UpdateTreeCtrlColors(win);
    }

    UpdateDocumentColors();
    return true;
}

void CleanUp() {
    DeleteGlobalPrefs(gGlobalPrefs);
    gGlobalPrefs = nullptr;
}

void schedulePrefsReload() {
    uitask::Post(prefs::Reload);
}

void RegisterForFileChanges() {
    if (!HasPermission(Perm_SavePreferences))
        return;

    CrashIf(gWatchedSettingsFile); // only call me once
    AutoFreeWstr path = GetSettingsPath();
    gWatchedSettingsFile = FileWatcherSubscribe(path, schedulePrefsReload);
}

void UnregisterForFileChanges() {
    FileWatcherUnsubscribe(gWatchedSettingsFile);
}

}; // namespace prefs
