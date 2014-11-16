/* Copyright 2014 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "BaseUtil.h"
#include "BaseEngine.h"
#define INCLUDE_SETTINGSSTRUCTS_METADATA
#include "SettingsStructs.h"
#include "DisplayState.h"
#include "AppPrefs.h"
#include "AppTools.h"
#include "DebugLog.h"
#include "EbookEngine.h"
#include "Favorites.h"
#include "FileUtil.h"
#include "FileHistory.h"
#include "FileTransactions.h"
#include "FileWatcher.h"
#include "SumatraPDF.h"
#include "Translations.h"
#include "UITask.h"
#include "Controller.h"
#include "EngineManager.h"
#include "WindowInfo.h"

#define PREFS_FILE_NAME     L"SumatraPDF-settings.txt"

GlobalPrefs *        gGlobalPrefs = NULL;

static WatchedFile * gWatchedSettingsFile = NULL;

Favorite *NewFavorite(int pageNo, const WCHAR *name, const WCHAR *pageLabel)
{
    Favorite *fav = (Favorite *)DeserializeStruct(&gFavoriteInfo, NULL);
    fav->pageNo = pageNo;
    fav->name = str::Dup(name);
    fav->pageLabel = str::Dup(pageLabel);
    return fav;
}

void DeleteFavorite(Favorite *fav)
{
    FreeStruct(&gFavoriteInfo, fav);
}

DisplayState *NewDisplayState(const WCHAR *filePath)
{
    DisplayState *ds = (DisplayState *)DeserializeStruct(&gFileStateInfo, NULL);
    str::ReplacePtr(&ds->filePath, filePath);
    return ds;
}

void DeleteDisplayState(DisplayState *ds)
{
    delete ds->thumbnail;
    FreeStruct(&gFileStateInfo, ds);
}

void DeleteGlobalPrefs(GlobalPrefs *globalPrefs)
{
    if (!globalPrefs)
        return;
    for (size_t i = 0; i < globalPrefs->fileStates->Count(); i++) {
        delete globalPrefs->fileStates->At(i)->thumbnail;
    }
    FreeStruct(&gGlobalPrefsInfo, globalPrefs);
}

// number of weeks past since 2011-01-01
static int GetWeekCount()
{
    SYSTEMTIME date20110101 = { 0 };
    date20110101.wYear = 2011; date20110101.wMonth = 1; date20110101.wDay = 1;
    FILETIME origTime, currTime;
    SystemTimeToFileTime(&date20110101, &origTime);
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

/* Caller needs to DeleteGlobalPrefs(gGlobalPrefs) */
bool Load()
{
    CrashIf(gGlobalPrefs);

    ScopedMem<WCHAR> path(GetSettingsPath());
    ScopedMem<char> prefsData(file::ReadAll(path, NULL));
    gGlobalPrefs = (GlobalPrefs *)DeserializeStruct(&gGlobalPrefsInfo, prefsData);
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
        for (DisplayState **ds = gGlobalPrefs->fileStates->IterStart(); ds; ds = gGlobalPrefs->fileStates->IterNext()) {
            (*ds)->openCount >>= weekDiff;
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

    /* mark currently shown files as visible */
    for (size_t i = 0; i < gWindows.Count(); i++) {
        UpdateCurrentFileDisplayStateForWin(gWindows.At(i));
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

    if (!gGlobalPrefs->rememberStatePerDocument || !gGlobalPrefs->rememberOpenedFiles) {
        for (DisplayState **ds = gGlobalPrefs->fileStates->IterStart(); ds; ds = gGlobalPrefs->fileStates->IterNext()) {
            (*ds)->useDefaultState = true;
        }
        // prevent unnecessary settings from being written out
        uint16_t fieldCount = 0;
        while (++fieldCount <= dimof(gFileStateFields)) {
            // count the number of fields up to and including useDefaultState
            if (gFileStateFields[fieldCount - 1].offset == offsetof(FileState, useDefaultState))
                break;
        }
        // restore the correct fieldCount ASAP after serialization
        gFileStateInfo.fieldCount = fieldCount;
    }

    size_t prefsDataSize;
    ScopedMem<char> prefsData(SerializeStruct(&gGlobalPrefsInfo, gGlobalPrefs, prevPrefsData, &prefsDataSize));

    if (!gGlobalPrefs->rememberStatePerDocument || !gGlobalPrefs->rememberOpenedFiles)
        gFileStateInfo.fieldCount = dimof(gFileStateFields);

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
    DeleteGlobalPrefs(gGlobalPrefs);
    gGlobalPrefs = NULL;

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

class SettingsFileObserver : public FileChangeObserver, public UITask {
public:
    SettingsFileObserver() { }

    virtual void OnFileChanged() {
        // don't Reload directly so as to prevent potential race conditions
        uitask::Post(new SettingsFileObserver());
    }

    virtual void Execute() {
        prefs::Reload();
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

namespace conv {

#define DM_AUTOMATIC_STR            "automatic"
#define DM_SINGLE_PAGE_STR          "single page"
#define DM_FACING_STR               "facing"
#define DM_BOOK_VIEW_STR            "book view"
#define DM_CONTINUOUS_STR           "continuous"
#define DM_CONTINUOUS_FACING_STR    "continuous facing"
#define DM_CONTINUOUS_BOOK_VIEW_STR "continuous book view"

#define STR_FROM_ENUM(val) \
    if (val == mode) \
        return TEXT(val##_STR); \
    else NoOp()

const WCHAR *FromDisplayMode(DisplayMode mode)
{
    STR_FROM_ENUM(DM_AUTOMATIC);
    STR_FROM_ENUM(DM_SINGLE_PAGE);
    STR_FROM_ENUM(DM_FACING);
    STR_FROM_ENUM(DM_BOOK_VIEW);
    STR_FROM_ENUM(DM_CONTINUOUS);
    STR_FROM_ENUM(DM_CONTINUOUS_FACING);
    STR_FROM_ENUM(DM_CONTINUOUS_BOOK_VIEW);
    CrashIf(true);
    return L"unknown display mode!?";
}

#undef STR_FROM_ENUM

#define IS_STR_ENUM(enumName) \
    if (str::EqIS(s, TEXT(enumName##_STR))) \
        return enumName; \
    else NoOp()

DisplayMode ToDisplayMode(const WCHAR *s, DisplayMode defVal)
{
    IS_STR_ENUM(DM_AUTOMATIC);
    IS_STR_ENUM(DM_SINGLE_PAGE);
    IS_STR_ENUM(DM_FACING);
    IS_STR_ENUM(DM_BOOK_VIEW);
    IS_STR_ENUM(DM_CONTINUOUS);
    IS_STR_ENUM(DM_CONTINUOUS_FACING);
    IS_STR_ENUM(DM_CONTINUOUS_BOOK_VIEW);
    // for consistency ("continuous" is used instead in the settings instead for brevity)
    if (str::EqIS(s, L"continuous single page"))
        return DM_CONTINUOUS;
    return defVal;
}

#undef IS_STR_ENUM

void FromZoom(char **dst, float zoom, DisplayState *stateForIssue2140)
{
    float prevZoom = *dst ? ToZoom(*dst, INVALID_ZOOM) : INVALID_ZOOM;
    if (prevZoom == zoom)
        return;
    if (!IsValidZoom(zoom) && stateForIssue2140) {
        // TODO: does issue 2140 still occur?
        dbglog::CrashLogF("Invalid ds->zoom: %g", zoom);
        const WCHAR *ext = path::GetExt(stateForIssue2140->filePath);
        if (!str::IsEmpty(ext)) {
            ScopedMem<char> extA(str::conv::ToUtf8(ext));
            dbglog::CrashLogF("File type: %s", extA.Get());
        }
        dbglog::CrashLogF("DisplayMode: %S", stateForIssue2140->displayMode);
        dbglog::CrashLogF("PageNo: %d", stateForIssue2140->pageNo);
    }
    CrashIf(!IsValidZoom(zoom));
    free(*dst);
    if (ZOOM_FIT_PAGE == zoom)
        *dst = str::Dup("fit page");
    else if (ZOOM_FIT_WIDTH == zoom)
        *dst = str::Dup("fit width");
    else if (ZOOM_FIT_CONTENT == zoom)
        *dst = str::Dup("fit content");
    else
        *dst = str::Format("%g", zoom);
}

float ToZoom(const char *s, float defVal)
{
    if (str::EqIS(s, "fit page"))
        return ZOOM_FIT_PAGE;
    if (str::EqIS(s, "fit width"))
        return ZOOM_FIT_WIDTH;
    if (str::EqIS(s, "fit content"))
        return ZOOM_FIT_CONTENT;
    float zoom;
    if (str::Parse(s, "%f", &zoom) && IsValidZoom(zoom))
        return zoom;
    return defVal;
}

}; // namespace conv

}; // namespace prefs
