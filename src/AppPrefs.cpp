/* Copyright 2014 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// utils
#include "BaseUtil.h"
#include "FileTransactions.h"
#include "FileUtil.h"
#include "FileWatcher.h"
#include "UITask.h"
// rendering engines
#include "BaseEngine.h"
#include "EbookEngine.h"
#include "EngineManager.h"
// layout controllers
#include "SettingsStructs.h"
#include "Controller.h"
#include "FileHistory.h"
#include "GlobalPrefs.h"
// ui
#include "SumatraPDF.h"
#include "WindowInfo.h"
#include "AppPrefs.h"
#include "AppTools.h"
#include "Favorites.h"
#include "Translations.h"

#define PREFS_FILE_NAME     L"SumatraPDF-settings.txt"

static WatchedFile * gWatchedSettingsFile = NULL;

// number of weeks past since 2011-01-01
static int GetWeekCount()
{
    SYSTEMTIME date20110101 = { 0 };
    date20110101.wYear = 2011; date20110101.wMonth = 1; date20110101.wDay = 1;
    FILETIME origTime, currTime;
    BOOL ok = SystemTimeToFileTime(&date20110101, &origTime);
    CrashIf(!ok);
    GetSystemTimeAsFileTime(&currTime);
    return (currTime.dwHighDateTime - origTime.dwHighDateTime) / 1408;
    // 1408 == (10 * 1000 * 1000 * 60 * 60 * 24 * 7) / (1 << 32)
}

static int cmpFloat(const void *a, const void *b)
{
    return *(float *)a < *(float *)b ? -1 : *(float *)a > *(float *)b ? 1 : 0;
}

namespace prefs {

WCHAR *GetSettingsPath()
{
    return AppGenDataFilename(PREFS_FILE_NAME);
}

/* Caller needs to prefs::CleanUp() */
bool Load()
{
    CrashIf(gGlobalPrefs);

    ScopedMem<WCHAR> path(GetSettingsPath());
    ScopedMem<char> prefsData(file::ReadAll(path, NULL));
    gGlobalPrefs = NewGlobalPrefs(prefsData);
    CrashAlwaysIf(!gGlobalPrefs);

#ifdef DISABLE_EBOOK_UI
    if (!file::Exists(path))
        gGlobalPrefs->ebookUI.useFixedPageUI = gGlobalPrefs->chmUI.useFixedPageUI = true;
#endif
#ifdef DISABLE_TABS
    if (!file::Exists(path))
        gGlobalPrefs->useTabs = false;
#endif

    if (!gGlobalPrefs->uiLanguage || !trans::ValidateLangCode(gGlobalPrefs->uiLanguage)) {
        // guess the ui language on first start
        str::ReplacePtr(&gGlobalPrefs->uiLanguage, trans::DetectUserLang());
    }
    gGlobalPrefs->lastPrefUpdate = file::GetModificationTime(path);
    gGlobalPrefs->defaultDisplayModeEnum = conv::ToDisplayMode(gGlobalPrefs->defaultDisplayMode, DM_AUTOMATIC);
    gGlobalPrefs->defaultZoomFloat = conv::ToZoom(gGlobalPrefs->defaultZoom, ZOOM_ACTUAL_SIZE);
    CrashIf(!IsValidZoom(gGlobalPrefs->defaultZoomFloat));

    int weekDiff = GetWeekCount() - gGlobalPrefs->openCountWeek;
    gGlobalPrefs->openCountWeek = GetWeekCount();
    if (weekDiff > 0) {
        // "age" openCount statistics (cut in in half after every week)
        for (DisplayState *ds : *gGlobalPrefs->fileStates) {
            ds->openCount >>= weekDiff;
        }
    }

    // make sure that zoom levels are in the order expected by DisplayModel
    gGlobalPrefs->zoomLevels->Sort(cmpFloat);
    while (gGlobalPrefs->zoomLevels->Count() > 0 &&
           gGlobalPrefs->zoomLevels->At(0) < ZOOM_MIN) {
        gGlobalPrefs->zoomLevels->PopAt(0);
    }
    while (gGlobalPrefs->zoomLevels->Count() > 0 &&
           gGlobalPrefs->zoomLevels->Last() > ZOOM_MAX) {
        gGlobalPrefs->zoomLevels->Pop();
    }

    // TODO: verify that all states have a non-NULL file path?
    gFileHistory.UpdateStatesSource(gGlobalPrefs->fileStates);
    SetDefaultEbookFont(gGlobalPrefs->ebookUI.fontName, gGlobalPrefs->ebookUI.fontSize);

    if (!file::Exists(path))
        Save();
    return true;
}

// called whenever global preferences change or a file is
// added or removed from gFileHistory (in order to keep
// the list of recently opened documents in sync)
bool Save()
{
    // don't save preferences without the proper permission
    if (!HasPermission(Perm_SavePreferences))
        return false;

    // update display states for all tabs
    for (WindowInfo *win : gWindows) {
        for (TabInfo *tab : win->tabs) {
            UpdateTabFileDisplayStateForWin(win, tab);
        }
    }

    // remove entries which should (no longer) be remembered
    gFileHistory.Purge(!gGlobalPrefs->rememberStatePerDocument);
    // update display mode and zoom fields from internal values
    str::ReplacePtr(&gGlobalPrefs->defaultDisplayMode, conv::FromDisplayMode(gGlobalPrefs->defaultDisplayModeEnum));
    conv::FromZoom(&gGlobalPrefs->defaultZoom, gGlobalPrefs->defaultZoomFloat);

    ScopedMem<WCHAR> path(GetSettingsPath());
    CrashIf(!path);
    if (!path)
        return false;
    size_t prevPrefsDataSize;
    ScopedMem<char> prevPrefsData(file::ReadAll(path, &prevPrefsDataSize));
    size_t prefsDataSize;
    ScopedMem<char> prefsData(SerializeGlobalPrefs(gGlobalPrefs, prevPrefsData, &prefsDataSize));

    CrashIf(!prefsData || 0 == prefsDataSize);
    if (!prefsData || 0 == prefsDataSize)
        return false;

    // only save if anything's changed at all
    if (prevPrefsDataSize == prefsDataSize && str::Eq(prefsData, prevPrefsData))
        return true;

    FileTransaction trans;
    bool ok = trans.WriteAll(path, prefsData, prefsDataSize) && trans.Commit();
    if (!ok)
        return false;
    gGlobalPrefs->lastPrefUpdate = file::GetModificationTime(path);
    return true;
}

// refresh the preferences when a different SumatraPDF process saves them
// or if they are edited by the user using a text editor
bool Reload()
{
    ScopedMem<WCHAR> path(GetSettingsPath());
    if (!file::Exists(path))
        return false;

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

    ScopedHandle hScope(h);

    FILETIME time = file::GetModificationTime(path);
    if (FileTimeEq(time, gGlobalPrefs->lastPrefUpdate))
        return true;

    ScopedMem<char> uiLanguage(str::Dup(gGlobalPrefs->uiLanguage));
    bool showToolbar = gGlobalPrefs->showToolbar;
    bool invertColors = gGlobalPrefs->fixedPageUI.invertColors;

    gFileHistory.UpdateStatesSource(NULL);
    CleanUp();

    bool ok = Load();
    CrashAlwaysIf(!ok || !gGlobalPrefs);

    gGlobalPrefs->fixedPageUI.invertColors = invertColors;

    // TODO: about window doesn't have to be at position 0
    if (gWindows.Count() > 0 && gWindows.At(0)->IsAboutWindow()) {
        gWindows.At(0)->DeleteInfotip();
        gWindows.At(0)->staticLinks.Reset();
        gWindows.At(0)->RedrawAll(true);
    }

    if (!str::Eq(uiLanguage, gGlobalPrefs->uiLanguage))
        SetCurrentLanguageAndRefreshUi(gGlobalPrefs->uiLanguage);

    if (gGlobalPrefs->showToolbar != showToolbar)
        ShowOrHideToolbarGlobally();

    UpdateDocumentColors();
    UpdateFavoritesTreeForAllWindows();

    return true;
}

void CleanUp()
{
    DeleteGlobalPrefs(gGlobalPrefs);
    gGlobalPrefs = NULL;
}

class SettingsFileObserver : public FileChangeObserver {
public:
    SettingsFileObserver() { }

    virtual void OnFileChanged() {
        // don't Reload directly so as to prevent potential race conditions
        uitask::Post([] { prefs::Reload(); });
    }
};

void RegisterForFileChanges()
{
    if (!HasPermission(Perm_SavePreferences))
        return;

    CrashIf(gWatchedSettingsFile); // only call me once
    ScopedMem<WCHAR> path(GetSettingsPath());
    gWatchedSettingsFile = FileWatcherSubscribe(path, new SettingsFileObserver());
}

void UnregisterForFileChanges()
{
    FileWatcherUnsubscribe(gWatchedSettingsFile);
}

}; // namespace prefs
