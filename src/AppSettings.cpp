/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "utils/BaseUtil.h"
#include "utils/FileUtil.h"
#include "utils/FileWatcher.h"
#include "utils/UITask.h"
#include "utils/ScopedWin.h"
#include "utils/WinUtil.h"
#include "utils/Timer.h"

#include "wingui/UIModels.h"

#include "Settings.h"
#include "Commands.h"
#include "DisplayMode.h"
#include "DocController.h"
#include "EngineBase.h"
#include "EngineAll.h"
#include "SumatraConfig.h"
#include "DisplayModel.h"
#include "FileHistory.h"
#include "GlobalPrefs.h"
#include "ProgressUpdateUI.h"
#include "SumatraPDF.h"
#include "WindowTab.h"
#include "ExternalViewers.h"
#include "Flags.h"
#include "MainWindow.h"
#include "AppSettings.h"
#include "AppTools.h"
#include "Favorites.h"
#include "Toolbar.h"
#include "Translations.h"
#include "Accelerators.h"
#include "Theme.h"

#include "utils/Log.h"

// workaround for OnMenuExit
// if this flag is set, CloseWindow will not save prefs before closing the window.
bool gDontSaveSettings = false;

// SumatraPDF.cpp
extern void RememberDefaultWindowPosition(MainWindow* win);

static WatchedFile* gWatchedSettingsFile = nullptr;

static HFONT gAppFont = nullptr;
static HFONT gBiggerAppFont = nullptr;
static HFONT gAppMenuFont = nullptr;
static HFONT gTreeFont = nullptr;

// TODO: if font sizes change, would need to re-layout the app
static void ResetCachedFonts() {
    gAppFont = nullptr;
    gBiggerAppFont = nullptr;
    gAppMenuFont = nullptr;
    gTreeFont = nullptr;
}

// number of weeks past since 2011-01-01
static int GetWeekCount() {
    SYSTEMTIME date20110101{};
    date20110101.wYear = 2011;
    date20110101.wMonth = 1;
    date20110101.wDay = 1;
    FILETIME origTime, currTime;
    BOOL ok = SystemTimeToFileTime(&date20110101, &origTime);
    ReportIf(!ok);
    GetSystemTimeAsFileTime(&currTime);
    return (currTime.dwHighDateTime - origTime.dwHighDateTime) / 1408;
    // 1408 == (10 * 1000 * 1000 * 60 * 60 * 24 * 7) / (1 << 32)
}

static int cmpFloat(const void* a, const void* b) {
    return *(float*)a < *(float*)b ? -1 : *(float*)a > *(float*)b ? 1 : 0;
}

TempStr GetSettingsFileNameTemp() {
    return str::DupTemp("SumatraPDF-settings.txt");
}

// this could be virtual path when running in app store
TempStr GetSettingsPathTemp() {
    return GetPathInAppDataDirTemp(GetSettingsFileNameTemp());
}

static void setMin(int& i, int minVal) {
    if (i < minVal) {
        i = minVal;
    }
}

static void setMinMax(int& i, int minVal, int maxVal) {
    if (i < minVal) {
        i = minVal;
    }
    if (i > maxVal) {
        i = maxVal;
    }
}

static void SetCommandNameAndShortcut(CustomCommand* cmd, const char* name, const char* key) {
    if (!cmd) {
        return;
    }
    cmd->name = str::IsEmptyOrWhiteSpace(name) ? nullptr : str::Dup(name);
    if (str::IsEmptyOrWhiteSpace(key)) {
        return;
    }
    if (!IsValidShortcutString(key)) {
        logf("SetCommandNameAndShortcut: '%s' is not a valid shortcut for '%s'\n", key, cmd->definition);
        return;
    }
    cmd->key = str::Dup(key);
}

/* for every selection handler defined by user in advanced settings, create
    a command that will be inserted into a menu item */
static void CreateSelectionHandlerCommands() {
    if (!HasPermission(Perm::InternetAccess) || !HasPermission(Perm::CopySelection)) {
        // TODO: when we add exe handlers, only filter the URL ones
        return;
    }

    for (auto& sh : *gGlobalPrefs->selectionHandlers) {
        if (!sh || !sh->url || !sh->name) {
            // can happen for bad selection handler definition
            continue;
        }
        if (str::IsEmptyOrWhiteSpace(sh->url) || str::IsEmptyOrWhiteSpace(sh->name)) {
            continue;
        }

        CommandArg* args = NewStringArg(kCmdArgURL, sh->url);
        auto cmd = CreateCustomCommand(sh->url, CmdSelectionHandler, args);
        SetCommandNameAndShortcut(cmd, sh->name, sh->key);
    }
}

static void CreateExternalViewersCommands() {
    for (ExternalViewer* ev : *gGlobalPrefs->externalViewers) {
        if (!ev || str::IsEmptyOrWhiteSpace(ev->commandLine)) {
            continue;
        }
        CommandArg* args = NewStringArg(kCmdArgCommandLine, ev->commandLine);
        if (!str::IsEmptyOrWhiteSpace(ev->filter)) {
            auto arg = NewStringArg(kCmdArgFilter, ev->filter);
            InsertArg(&args, arg);
        }
        auto cmd = CreateCustomCommand("", CmdViewWithExternalViewer, args);
        SetCommandNameAndShortcut(cmd, ev->name, ev->key);
    }
}

static void CreateZoomCommands() {
    auto prefs = gGlobalPrefs;
    delete prefs->zoomLevelsCmdIds;
    int n = prefs->zoomLevels->Size();
    if (n <= 0) {
        return;
    }
    Vec<int>* cmdIds = new Vec<int>(n);
    prefs->zoomLevelsCmdIds = cmdIds;
    for (int i = 0; i < n; i++) {
        float zoomLevel = prefs->zoomLevels->At(i);
        CommandArg* arg = NewFloatArg(kCmdArgLevel, zoomLevel);
        auto cmd = CreateCustomCommand("CmdZoomCustom", CmdZoomCustom, arg);
        cmdIds->InsertAt(i, cmd->id);
    }
}

static void CreateCustomShortcuts() {
    for (Shortcut* shortcut : *gGlobalPrefs->shortcuts) {
        auto cmd = CreateCommandFromDefinition(shortcut->cmd);
        if (!cmd) {
            continue;
        }
        shortcut->cmdId = cmd->id;
        SetCommandNameAndShortcut(cmd, shortcut->name, shortcut->key);
    }
}

/* Caller needs to CleanUpSettings() */
bool LoadSettings() {
    ReportIf(gGlobalPrefs);

    auto timeStart = TimeGet();

    GlobalPrefs* gprefs = nullptr;
    TempStr settingsPath = GetSettingsPathTemp();
    {
        ByteSlice prefsData = file::ReadFile(settingsPath);

        gGlobalPrefs = NewGlobalPrefs(prefsData);
        ReportIf(!gGlobalPrefs);
        gprefs = gGlobalPrefs;
        prefsData.Free();
    }

    if (!gprefs->uiLanguage || !trans::ValidateLangCode(gprefs->uiLanguage)) {
        // guess the ui language on first start
        str::ReplaceWithCopy(&gprefs->uiLanguage, trans::DetectUserLang());
    }
    gprefs->lastPrefUpdate = file::GetModificationTime(settingsPath);
    gprefs->defaultDisplayModeEnum = DisplayModeFromString(gprefs->defaultDisplayMode, DisplayMode::Automatic);
    gprefs->defaultZoomFloat = ZoomFromString(gprefs->defaultZoom, kZoomActualSize);
    ReportIf(!IsValidZoom(gprefs->defaultZoomFloat));

    int weekDiff = GetWeekCount() - gprefs->openCountWeek;
    gprefs->openCountWeek = GetWeekCount();
    if (weekDiff > 0) {
        // "age" openCount statistics (cut in in half after every week)
        for (FileState* fs : *gprefs->fileStates) {
            fs->openCount >>= weekDiff;
        }
    }

    // make sure that zoom levels are in the order expected by DisplayModel
    gprefs->zoomLevels->Sort(cmpFloat);
    while (gprefs->zoomLevels->size() > 0 && gprefs->zoomLevels->at(0) < kZoomMin) {
        gprefs->zoomLevels->PopAt(0);
    }
    while (gprefs->zoomLevels->size() > 0 && gprefs->zoomLevels->Last() > kZoomMax) {
        gprefs->zoomLevels->Pop();
    }

    // sanitize WindowMargin and PageSpacing values
    // https://github.com/sumatrapdfreader/sumatrapdf/issues/1899
    {
        auto&& m = gprefs->fixedPageUI.windowMargin;
        setMin(m.bottom, 0);
        setMin(m.top, 0);
        setMin(m.left, 0);
        setMin(m.right, 0);
    }
    {
        auto&& m = gprefs->comicBookUI.windowMargin;
        setMin(m.bottom, 0);
        setMin(m.top, 0);
        setMin(m.left, 0);
        setMin(m.right, 0);
    }
    {
        auto&& s = gprefs->fixedPageUI.pageSpacing;
        setMin(s.dx, 0);
        setMin(s.dy, 0);
    }
    {
        auto&& s = gprefs->comicBookUI.pageSpacing;
        setMin(s.dx, 0);
        setMin(s.dy, 0);
    }
    setMin(gprefs->tabWidth, 60);
    setMin(gprefs->sidebarDx, 0);
    setMin(gprefs->tocDy, 0);
    setMin(gprefs->treeFontSize, 0);
    if (gprefs->toolbarSize == 0) {
        gprefs->toolbarSize = 18; // same as kDefaultIconSize in Toolbar.cpp
    }
    setMinMax(gprefs->toolbarSize, 8, 64);

    if (!gprefs->treeFontName) {
        gprefs->treeFontName = const_cast<char*>("automatic");
    }

    // TODO: verify that all states have a non-nullptr file path?
    gFileHistory.UpdateStatesSource(gprefs->fileStates);
    //    auto fontName = ToWStrTemp(gprefs->fixedPageUI.ebookFontName);
    //    SetDefaultEbookFont(fontName.Get(), gprefs->fixedPageUI.ebookFontSize);

    ResetCachedFonts();

    // re-create commands
    FreeCustomCommands();
    // Note: some are also created in ReCreateSumatraAcceleratorTable()
    CreateZoomCommands();
    CreateThemeCommands();
    CreateExternalViewersCommands();
    CreateSelectionHandlerCommands();
    CreateCustomShortcuts();

    // re-create accelerators
    FreeAcceleratorTables();
    CreateSumatraAcceleratorTable();

    ReCreateToolbars();

    SetCurrentThemeFromSettings();
    if (!file::Exists(settingsPath)) {
        SaveSettings();
    }

    logf("LoadSettings('%s') took %.2f ms\n", settingsPath, TimeSinceInMs(timeStart));
    return true;
}

static void RememberSessionState() {
    Vec<SessionData*>* sessionData = gGlobalPrefs->sessionData;
    ResetSessionState(sessionData);

    if (!gGlobalPrefs->rememberOpenedFiles) {
        return;
    }

    if (gWindows.size() == 0) {
        return;
    }

    for (auto* win : gWindows) {
        SessionData* data = NewSessionData();
        for (WindowTab* tab : win->Tabs()) {
            if (tab->IsAboutTab()) {
                continue;
            }
            const char* fp = tab->filePath;
            FileState* fs = NewDisplayState(fp);
            if (tab->ctrl) {
                tab->ctrl->GetDisplayState(fs);
            }
            fs->showToc = tab->showToc;
            *fs->tocState = tab->tocState;
            TabState* ts = NewTabState(fs);
            data->tabStates->Append(ts);
            DeleteDisplayState(fs);
        }
        if (data->tabStates->Size() == 0) {
            continue;
        }
        data->tabIndex = win->GetTabIdx(win->CurrentTab()) + 1;
        if (data->tabIndex < 0) {
            data->tabIndex = 0;
        }
        // TODO: allow recording this state without changing gGlobalPrefs
        RememberDefaultWindowPosition(win);
        data->windowState = gGlobalPrefs->windowState;
        data->windowPos = gGlobalPrefs->windowPos;
        data->sidebarDx = gGlobalPrefs->sidebarDx;
        sessionData->Append(data);
    }
}

// called whenever global preferences change or a file is
// added or removed from gFileHistory (in order to keep
// the list of recently opened documents in sync)
bool SaveSettings() {
    if (!gDontSaveSettings) {
        // if we are exiting the application by File->Exit,
        // OnMenuExit will have called SaveSettings() already
        // and we skip the call here to avoid saving incomplete session info
        // (because some windows might have been closed already)
    }

    // don't save preferences without the proper permission
    if (!HasPermission(Perm::SavePreferences)) {
        return false;
    }
    logf("SaveSettings\n");
    // update display states for all tabs
    for (MainWindow* win : gWindows) {
        for (WindowTab* tab : win->Tabs()) {
            UpdateTabFileDisplayStateForTab(tab);
        }
    }
    RememberSessionState();

    // remove entries which should (no longer) be remembered
    gFileHistory.Purge(!gGlobalPrefs->rememberStatePerDocument);
    // update display mode and zoom fields from internal values
    str::ReplaceWithCopy(&gGlobalPrefs->defaultDisplayMode, DisplayModeToString(gGlobalPrefs->defaultDisplayModeEnum));
    ZoomToString(&gGlobalPrefs->defaultZoom, gGlobalPrefs->defaultZoomFloat, nullptr);

    TempStr path = GetSettingsPathTemp();
    ReportIf(!path);
    if (!path) {
        return false;
    }
    ByteSlice prevPrefs = file::ReadFile(path);
    const char* prevPrefsData = (char*)prevPrefs.data();
    ByteSlice prefs = SerializeGlobalPrefs(gGlobalPrefs, prevPrefsData);
    defer {
        str::Free(prevPrefs.data());
        str::Free(prefs.data());
    };
    ReportIf(prefs.empty());
    if (prefs.empty()) {
        return false;
    }

    // only save if anything's changed at all
    if (prevPrefs.size() == prefs.size() && str::Eq(prefs, prevPrefs)) {
        return true;
    }

    WatchedFileSetIgnore(gWatchedSettingsFile, true);
    bool ok = file::WriteFile(path, prefs);
    if (ok) {
        gGlobalPrefs->lastPrefUpdate = file::GetModificationTime(path);
    }
    WatchedFileSetIgnore(gWatchedSettingsFile, false);
    return ok;
}

// refresh the preferences when a different SumatraPDF process saves them
// or if they are edited by the user using a text editor
static void ReloadSettings() {
    TempStr settingsPath = GetSettingsPathTemp();
    if (!file::Exists(settingsPath)) {
        return;
    }

    // make sure that the settings file is readable - else wait
    // a short while to prevent accidental data loss
    // this is triggered when e.g. saving the file with VS Code
    bool ok = false;
    for (int i = 0; !ok && i < 5; i++) {
        Sleep(200);
        ByteSlice prefsData = file::ReadFile(settingsPath);
        if (prefsData.size() > 0) {
            ok = true;
            prefsData.Free();
        } else {
            logf("ReloadSettings: failed to load '%s', i=%d\n", settingsPath, i);
        }
    }
    if (!ok) {
        return;
    }

    FILETIME time = file::GetModificationTime(settingsPath);
    if (FileTimeEq(time, gGlobalPrefs->lastPrefUpdate)) {
        return;
    }

    const char* uiLanguage = str::DupTemp(gGlobalPrefs->uiLanguage);
    bool showToolbar = gGlobalPrefs->showToolbar;

    gFileHistory.UpdateStatesSource(nullptr);
    CleanUpSettings();

    ok = LoadSettings();
    ReportIf(!ok || !gGlobalPrefs);

    // TODO: about window doesn't have to be at position 0
    if (gWindows.size() > 0 && gWindows.at(0)->IsCurrentTabAbout()) {
        MainWindow* win = gWindows.at(0);
        win->DeleteToolTip();
        DeleteVecMembers(win->staticLinks);
        win->RedrawAll(true);
    }

    if (!str::Eq(uiLanguage, gGlobalPrefs->uiLanguage)) {
        SetCurrentLanguageAndRefreshUI(gGlobalPrefs->uiLanguage);
    }

    for (MainWindow* win : gWindows) {
        if (gGlobalPrefs->showToolbar != showToolbar) {
            ShowOrHideToolbar(win);
        }
        UpdateFavoritesTree(win);
        UpdateControlsColors(win);
    }

    UpdateDocumentColors();
    UpdateFixedPageScrollbarsVisibility();
}

void CleanUpSettings() {
    DeleteGlobalPrefs(gGlobalPrefs);
    gGlobalPrefs = nullptr;
}

static void SchedulePrefsReload() {
    auto fn = MkFunc0Void(ReloadSettings);
    uitask::Post(fn, "TaskReloadSettings");
}

void RegisterSettingsForFileChanges() {
    if (!HasPermission(Perm::SavePreferences)) {
        return;
    }

    ReportIf(gWatchedSettingsFile); // only call me once
    TempStr path = GetSettingsPathTemp();
    auto fn = MkFunc0Void(SchedulePrefsReload);
    gWatchedSettingsFile = FileWatcherSubscribe(path, fn);
}

void UnregisterSettingsForFileChanges() {
    FileWatcherUnsubscribe(gWatchedSettingsFile);
    // TODO: memleak of gWatchedSettingsFile
}

constexpr int kMinFontSize = 9;

int GetAppFontSize() {
    auto fntSize = gGlobalPrefs->uIFontSize;
    if (fntSize < kMinFontSize) {
        fntSize = GetSizeOfDefaultGuiFont();
    }
    return fntSize;
}

HFONT GetAppFont() {
    if (gAppFont) {
        return gAppFont;
    }
    auto fntSize = GetAppFontSize();
    gAppFont = GetUserGuiFont("auto", fntSize);
    return gAppFont;
}

constexpr int kMinBiggerFontSize = 14;

// if user provided font size, we use that
// otherwise we return 1.4x of default font size but no smaller than 16
// on my laptop on high dpi default font size is 12
HFONT GetAppBiggerFont() {
    if (gBiggerAppFont) {
        return gBiggerAppFont;
    }
    int fntSize = gGlobalPrefs->uIFontSize;
    if (fntSize < kMinFontSize) {
        fntSize = GetSizeOfDefaultGuiFont();
        fntSize = (fntSize * 12) / 10;
        if (fntSize < kMinBiggerFontSize) {
            fntSize = kMinBiggerFontSize;
        }
    }
    gBiggerAppFont = GetDefaultGuiFontOfSize(fntSize);
    return gBiggerAppFont;
}

HFONT GetAppTreeFont() {
    if (gTreeFont) {
        return gTreeFont;
    }
    int fntSize = gGlobalPrefs->treeFontSize;
    if (fntSize < kMinFontSize) {
        fntSize = gGlobalPrefs->uIFontSize;
    }
    if (fntSize < kMinFontSize) {
        fntSize = GetSizeOfDefaultGuiFont();
    }
    char* fntNameUser = gGlobalPrefs->treeFontName;
    gTreeFont = GetUserGuiFont(fntNameUser, fntSize);
    return gTreeFont;
}

int GetAppMenuFontSize() {
    NONCLIENTMETRICS ncm{};
    ncm.cbSize = sizeof(ncm);
    SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0);
    int fntSize = std::abs(ncm.lfMenuFont.lfHeight);
    if (gGlobalPrefs->uIFontSize >= kMinFontSize) {
        fntSize = gGlobalPrefs->uIFontSize;
    }
    return fntSize;
}

HFONT GetAppMenuFont() {
    if (gAppMenuFont) {
        return gAppMenuFont;
    }
    NONCLIENTMETRICS ncm{};
    ncm.cbSize = sizeof(ncm);
    SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0);
    int fntSize = GetAppMenuFontSize();
    ncm.lfMenuFont.lfHeight = -fntSize;
    gAppMenuFont = CreateFontIndirectW(&ncm.lfMenuFont);
    return gAppMenuFont;
}

bool IsMenuFontSizeDefault() {
    auto fntSize = gGlobalPrefs->uIFontSize;
    return fntSize < kMinFontSize;
}

bool IsAppFontSizeDefault() {
    auto fntSize = gGlobalPrefs->uIFontSize;
    return fntSize < kMinFontSize;
}
