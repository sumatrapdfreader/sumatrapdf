/* Copyright 2015 the SumatraPDF project authors (see AUTHORS file).
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
// layout controllers
#include "SettingsStructs.h"
#include "FileHistory.h"
#include "GlobalPrefs.h"
// ui
#include "SumatraPDF.h"
#include "ParseCommandLine.h"
#include "WindowInfo.h"
#include "AppPrefs.h"
#include "AppTools.h"
#include "Favorites.h"
#include "Toolbar.h"
#include "Translations.h"

#define PREFS_FILE_NAME     L"SumatraPDF-settings.txt"

static WatchedFile * gWatchedSettingsFile = nullptr;

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

    std::unique_ptr<WCHAR> path(GetSettingsPath());
    std::unique_ptr<char> prefsData(file::ReadAll(path.get(), nullptr));
    gGlobalPrefs = NewGlobalPrefs(prefsData.get());
    CrashAlwaysIf(!gGlobalPrefs);

    // in pre-release builds between 3.1.10079 and 3.1.10377,
    // RestoreSession was a string with the additional option "auto"
    // TODO: remove this after 3.2 has been released
#if defined(DEBUG) || defined(SVN_PRE_RELEASE_VER)
    if (!gGlobalPrefs->restoreSession && prefsData && str::Find(prefsData.get(), "\nRestoreSession = auto"))
        gGlobalPrefs->restoreSession = true;
#endif

#ifdef DISABLE_EBOOK_UI
    if (!prefsData || !str::Find(prefsData, "UseFixedPageUI ="))
        gGlobalPrefs->ebookUI.useFixedPageUI = gGlobalPrefs->chmUI.useFixedPageUI = true;
#endif
#ifdef DISABLE_TABS
    if (!prefsData || !str::Find(prefsData, "UseTabs ="))
        gGlobalPrefs->useTabs = false;
#endif

    if (!gGlobalPrefs->uiLanguage || !trans::ValidateLangCode(gGlobalPrefs->uiLanguage)) {
        // guess the ui language on first start
        str::ReplacePtr(&gGlobalPrefs->uiLanguage, trans::DetectUserLang());
    }
    gGlobalPrefs->lastPrefUpdate = file::GetModificationTime(path.get());
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

    // TODO: verify that all states have a non-nullptr file path?
    gFileHistory.UpdateStatesSource(gGlobalPrefs->fileStates);
    SetDefaultEbookFont(gGlobalPrefs->ebookUI.fontName, gGlobalPrefs->ebookUI.fontSize);

    if (!file::Exists(path.get()))
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

    std::unique_ptr<WCHAR> path(GetSettingsPath());
    CrashIfDebugOnly(!path);
    if (!path)
        return false;
    size_t prevPrefsDataSize = 0;
    std::unique_ptr<char> prevPrefsData(file::ReadAll(path.get(), &prevPrefsDataSize));
    size_t prefsDataSize = 0;
    std::unique_ptr<char> prefsData(SerializeGlobalPrefs(gGlobalPrefs, prevPrefsData.get(), &prefsDataSize));

    CrashIf(!prefsData || 0 == prefsDataSize);
    if (!prefsData || 0 == prefsDataSize)
        return false;

    // only save if anything's changed at all
    if (prevPrefsDataSize == prefsDataSize &&
        str::Eq(prefsData.get(), prevPrefsData.get())) {
        return true;
    }

    bool ok = file::WriteAll(path.get(), prefsData.get(), prefsDataSize);
    if (!ok)
        return false;
    gGlobalPrefs->lastPrefUpdate = file::GetModificationTime(path.get());
    return true;
}

// refresh the preferences when a different SumatraPDF process saves them
// or if they are edited by the user using a text editor
bool Reload()
{
    std::unique_ptr<WCHAR> path(GetSettingsPath());
    if (!file::Exists(path.get()))
        return false;

    // make sure that the settings file is readable - else wait
    // a short while to prevent accidental dataloss
    int tryAgainCount = 5;
    HANDLE h = file::OpenReadOnly(path.get());
    while (INVALID_HANDLE_VALUE == h && tryAgainCount-- > 0) {
        Sleep(200);
        h = file::OpenReadOnly(path.get());
    }
    if (INVALID_HANDLE_VALUE == h) {
        // prefer not reloading to resetting all settings
        return false;
    }

    ScopedHandle hScope(h);

    FILETIME time = file::GetModificationTime(path.get());
    if (FileTimeEq(time, gGlobalPrefs->lastPrefUpdate))
        return true;

    std::unique_ptr<char> uiLanguage(str::Dup(gGlobalPrefs->uiLanguage));
    bool showToolbar = gGlobalPrefs->showToolbar;
    bool invertColors = gGlobalPrefs->fixedPageUI.invertColors;

    gFileHistory.UpdateStatesSource(nullptr);
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

    if (!str::Eq(uiLanguage.get(), gGlobalPrefs->uiLanguage))
        SetCurrentLanguageAndRefreshUI(gGlobalPrefs->uiLanguage);

    for (WindowInfo *win : gWindows) {
        if (gGlobalPrefs->showToolbar != showToolbar)
            ShowOrHideToolbar(win);
        UpdateFavoritesTree(win);
    }

    UpdateDocumentColors();

    return true;
}

void UpdateGlobalPrefs(const CommandLineInfo& i) {
    if (i.inverseSearchCmdLine) {
        str::ReplacePtr(&gGlobalPrefs->inverseSearchCmdLine, i.inverseSearchCmdLine);
        gGlobalPrefs->enableTeXEnhancements = true;
    }
    gGlobalPrefs->fixedPageUI.invertColors = i.invertColors;

    for (size_t n = 0; n <i.globalPrefArgs.Count(); n++) {
        if (str::EqI(i.globalPrefArgs.At(n), L"-esc-to-exit")) {
            gGlobalPrefs->escToExit = true;
        } else if (str::EqI(i.globalPrefArgs.At(n), L"-bgcolor") ||
                   str::EqI(i.globalPrefArgs.At(n), L"-bg-color")) {
            // -bgcolor is for backwards compat (was used pre-1.3)
            // -bg-color is for consistency
            ParseColor(&gGlobalPrefs->mainWindowBackground, i.globalPrefArgs.At(++n));
        } else if (str::EqI(i.globalPrefArgs.At(n), L"-set-color-range")) {
            ParseColor(&gGlobalPrefs->fixedPageUI.textColor, i.globalPrefArgs.At(++n));
            ParseColor(&gGlobalPrefs->fixedPageUI.backgroundColor, i.globalPrefArgs.At(++n));
        } else if (str::EqI(i.globalPrefArgs.At(n), L"-fwdsearch-offset")) {
            gGlobalPrefs->forwardSearch.highlightOffset = _wtoi(i.globalPrefArgs.At(++n));
            gGlobalPrefs->enableTeXEnhancements = true;
        } else if (str::EqI(i.globalPrefArgs.At(n), L"-fwdsearch-width")) {
            gGlobalPrefs->forwardSearch.highlightWidth = _wtoi(i.globalPrefArgs.At(++n));
            gGlobalPrefs->enableTeXEnhancements = true;
        } else if (str::EqI(i.globalPrefArgs.At(n), L"-fwdsearch-color")) {
            ParseColor(&gGlobalPrefs->forwardSearch.highlightColor, i.globalPrefArgs.At(++n));
            gGlobalPrefs->enableTeXEnhancements = true;
        } else if (str::EqI(i.globalPrefArgs.At(n), L"-fwdsearch-permanent")) {
            gGlobalPrefs->forwardSearch.highlightPermanent = _wtoi(i.globalPrefArgs.At(++n));
            gGlobalPrefs->enableTeXEnhancements = true;
        } else if (str::EqI(i.globalPrefArgs.At(n), L"-manga-mode")) {
            const WCHAR *s = i.globalPrefArgs.At(++n);
            gGlobalPrefs->comicBookUI.cbxMangaMode = str::EqI(L"true", s) || str::Eq(L"1", s);
        }
    }
}

void CleanUp()
{
    DeleteGlobalPrefs(gGlobalPrefs);
    gGlobalPrefs = nullptr;
}

void schedulePrefsReload() {
    uitask::Post(prefs::Reload);
}

void RegisterForFileChanges()
{
    if (!HasPermission(Perm_SavePreferences))
        return;

    CrashIf(gWatchedSettingsFile); // only call me once
    std::unique_ptr<WCHAR> path(GetSettingsPath());
    gWatchedSettingsFile = FileWatcherSubscribe(path.get(), schedulePrefsReload);
}

void UnregisterForFileChanges()
{
    FileWatcherUnsubscribe(gWatchedSettingsFile);
}

}; // namespace prefs
