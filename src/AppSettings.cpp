/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/File.h"
#include "base/Pixmap.h"
#include "base/SettingsUtil.h"
#include "base/FileWatcher.h"
#include "base/SquareTreeParser.h"
#include "base/UITask.h"
#include "base/Win.h"
#include "gui/Dpi.h"
#include "gui/PlatformFont.h"
#include "base/Timer.h"

#include "gui/UIModels.h"

#define INCLUDE_SETTINGSSTRUCTS_METADATA
#include "Settings.h"
#include "Commands.h"
#include "DisplayMode.h"
#include "DocController.h"
#include "EngineBase.h"
#include "base/GuessFileType.h"
#include "EngineAll.h"
#include "PdfCadDetect.h"
#include "SumatraConfig.h"
#include "FileHistory.h"
#include "SumatraPDF.h"
#include "WindowTab.h"
#include "MainWindow.h"
#include "DisplayModel.h"
#include "AppTools.h"
#include "Favorites.h"
#include "HomePage.h"
#include "Toolbar.h"
#include "Translations.h"
#include "Accelerators.h"
#include "Theme.h"
#include "PdfDarkMode.h"
#include "TextToSpeech.h"
#include "Notifications.h"
#include "ExplorerQuickLook.h"
#include "AppSettings.h"

// workaround for OnMenuExit
// if this flag is set, CloseWindow will not save prefs before closing the window.
bool gDontSaveSettings = false;

// coalesces ScheduleSaveSettings() onto one uitask; a sync SaveSettings()
// clears it so a pending post becomes a no-op
static bool gSaveSettingsPending = false;

// last bytes we wrote (or loaded). Watcher reloads and SaveSettings() writes
// are skipped when the file / serialized prefs still match this.
static Str gLastSavedPrefs;

static bool IsLastSavedPrefs(Str s) {
    return len(gLastSavedPrefs) == len(s) && str::Eq(gLastSavedPrefs, s);
}

static void RememberLastSavedPrefs(Str s) {
    str::ReplaceWithCopy(&gLastSavedPrefs, s);
}

static bool ApplyReadAloudVoiceFromSettings() {
    if (!gSettings) {
        return false;
    }

    float speed = gSettings->readAloudSpeed;
    TtsSetSpeed(speed > 0 ? speed : 1.0f);

    Str voiceId = gSettings->readAloudVoiceId;
    if (!voiceId) {
        TtsSetVoiceById(StrL(""));
        return false;
    }

    if (!TtsSetVoiceById(voiceId)) {
        logf("ApplyReadAloudVoiceFromSettings: voice '%s' not available, using system default\n", voiceId);
        str::ReplaceWithCopy(&gSettings->readAloudVoiceId, Str{});
        TtsSetVoiceById(StrL(""));
        return true;
    }
    return false;
}

// SumatraPDF.cpp
extern void RememberDefaultWindowPosition(MainWindow* win);

static WatchedFile* gWatchedSettingsFile = nullptr;

static DocumentColorsFollowTheme MapLegacyDocumentColorMode(Str v) {
    if (str::EqI(v, StrL("auto"))) {
        return DocumentColorsFollowTheme::Smart;
    }
    if (str::EqI(v, StrL("black"))) {
        return DocumentColorsFollowTheme::Legacy;
    }
    return DocumentColorsFollowTheme::Off;
}

// the black-on-white a document renders as when FixedPageUI says nothing
constexpr Color kColBlackDefault = 0x000000;
constexpr Color kColWhiteDefault = 0xFFFFFF;

// Migrate FixedPageUI.InvertColors and DocumentColorMode to DocumentColorsFollowTheme
static bool MigrateDocumentColorsFollowThemeSetting(Str prefsData) {
    if (!prefsData) {
        return false;
    }
    SquareTreeNode* root = ParseSquareTree(prefsData);
    if (!root) {
        return false;
    }

    Str newSetting = root->GetValue(StrL("DocumentColorsFollowTheme"));
    if (!str::IsNull(newSetting)) {
        delete root;
        DocumentColorsFollowTheme mode = GetDocumentColorsFollowTheme();
        SetDocumentColorsFollowTheme(mode);
        return false;
    }

    Str oldSetting = root->GetValue(StrL("DocumentColorMode"));
    bool hadOldSetting = !str::IsNull(oldSetting);

    bool hadInvertColors = false;
    SquareTreeNode* fixedPageUI = root->GetChild(StrL("FixedPageUI"));
    if (fixedPageUI) {
        Str invertColors = fixedPageUI->GetValue(StrL("InvertColors"));
        hadInvertColors = str::EqI(invertColors, StrL("true"));
    }

    delete root;

    if (hadOldSetting) {
        SetDocumentColorsFollowTheme(MapLegacyDocumentColorMode(oldSetting));
        return true;
    }
    if (hadInvertColors) {
        // InvertColors meant "swap FixedPageUI TextColor and BackgroundColor",
        // so swap them: that's still what those two settings do, it doesn't
        // depend on the theme, and it's what the user was looking at in 3.6.
        //
        // This used to map to DocumentColorsFollowTheme::Smart, which inverted
        // back when the mapping was written. 37f920ff0 then redefined smart as
        // "match the UI theme, don't swap black/white", which quietly turned
        // this migration into "light pages" for anyone on a light theme.
        Color text = ParseColor(gSettings->fixedPageUI.textColor.s, kColBlackDefault);
        Color bg = ParseColor(gSettings->fixedPageUI.backgroundColor.s, kColWhiteDefault);
        SetColorText(gSettings->fixedPageUI.textColor, SerializeColorTemp(bg));
        SetColorText(gSettings->fixedPageUI.backgroundColor, SerializeColorTemp(text));
        SetDocumentColorsFollowTheme(DocumentColorsFollowTheme::Off);
        return true;
    }
    return false;
}

// UI fonts are cached per DPI so windows on monitors with different scale
// factors get correctly sized fonts. User-set sizes (UIFontSize, TreeFontSize)
// are pixel sizes and used as-is at every DPI.
struct UiFontsAtDpi {
    int dpi = 0;
    PlatformFont* appFont = nullptr;
    PlatformFont* biggerAppFont = nullptr;
    PlatformFont* appMenuFont = nullptr;
    PlatformFont* sidebarLabelFont = nullptr;
    PlatformFont* treeFontEx[4] = {nullptr, nullptr, nullptr, nullptr};
};

static Vec<UiFontsAtDpi> gUiFontsAtDpi;

// the returned pointer is only valid until the next call (Vec can reallocate)
static UiFontsAtDpi* GetUiFontsAtDpi(int dpi) {
    int n = len(gUiFontsAtDpi);
    for (int i = 0; i < n; i++) {
        if (gUiFontsAtDpi[i].dpi == dpi) {
            return &gUiFontsAtDpi[i];
        }
    }
    UiFontsAtDpi e;
    e.dpi = dpi;
    VecAppend(gUiFontsAtDpi, e);
    return &gUiFontsAtDpi[n];
}

// TODO: if font sizes change, would need to re-layout the app
static void ResetCachedFonts() {
    // Fonts are interned PlatformFonts, so just drop these per-DPI references;
    // old fonts stay valid for windows that still hold them.
    VecReset(gUiFontsAtDpi);
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
    return (int)(currTime.dwHighDateTime - origTime.dwHighDateTime) / 1408;
    // 1408 == (10 * 1000 * 1000 * 60 * 60 * 24 * 7) / (1 << 32)
}

static int cmpFloat(const float* a, const float* b) {
    if (*a < *b) {
        return -1;
    }
    if (*a > *b) {
        return 1;
    }
    return 0;
}

TempStr GetSettingsFileNameTemp() {
    return str::DupTemp(StrL("SumatraPDF-settings.txt"));
}

// this could be virtual path when running in app store
TempStr GetSettingsPathTemp() {
    return GetPathInAppDataDirTemp(GetSettingsFileNameTemp());
}

static void setMin(int& i, int minVal) {
    i = std::max(i, minVal);
}

/* for every selection handler defined by user in advanced settings, create
    a command that will be inserted into a menu item */
static void CreateSelectionHandlerCommands() {
    // every handler reads the selection; only the ones that talk to a web
    // service need network access, so an Exe handler still works without it
    if (!HasPermission(Perm::CopySelection)) {
        return;
    }
    bool canUseInternet = HasPermission(Perm::InternetAccess);

    for (auto& sh : *gSettings->selectionHandlers) {
        if (!sh || !sh->name || str::IsEmptyOrWhiteSpace(sh->name)) {
            // can happen for bad selection handler definition
            continue;
        }
        bool hasExe = !str::IsEmptyOrWhiteSpace(sh->exe);
        bool hasUrl = !str::IsEmptyOrWhiteSpace(sh->url);
        if (!hasExe && !hasUrl) {
            continue;
        }
        if (!hasExe && !canUseInternet) {
            continue;
        }

        // args are a linked list; only attach the optional ones that are set so
        // a handler with just URL/Name/Key behaves exactly as it did before
        Str definition = hasExe ? sh->exe : sh->url;
        CommandArg* args = hasExe ? NewStringArg(kCmdArgExe, sh->exe) : NewStringArg(kCmdArgURL, sh->url);
        auto addArg = [&args](Str name, Str val) {
            if (str::IsEmptyOrWhiteSpace(val)) {
                return;
            }
            CommandArg* a = NewStringArg(name, val);
            a->next = args;
            args = a;
        };
        if (!hasExe) {
            addArg(kCmdArgMethod, sh->method);
            addArg(kCmdArgBody, sh->body);
            addArg(kCmdArgContentType, sh->contentType);
            addArg(kCmdArgHeaders, sh->headers);
        }
        addArg(kCmdArgSelectToolbar, sh->selectToolbarNameOrSvg);
        addArg(kCmdArgToolbarText, sh->toolbarText);
        addArg(kCmdArgToolbarSvgIcon, sh->toolbarSvgIcon);
        CreateCustomCommand(definition, CmdSelectionHandler, args, sh->name, sh->key);
    }
}

static void CreateExternalViewersCommands() {
    for (ExternalViewer* ev : *gSettings->externalViewers) {
        if (!ev || str::IsEmptyOrWhiteSpace(ev->commandLine)) {
            continue;
        }
        CommandArg* args = NewStringArg(kCmdArgCommandLine, ev->commandLine);
        if (!str::IsEmptyOrWhiteSpace(ev->filter)) {
            auto* arg = NewStringArg(kCmdArgFilter, ev->filter);
            InsertArg(&args, arg);
        }
        if (!str::IsEmptyOrWhiteSpace(ev->toolbarText)) {
            auto* arg = NewStringArg(kCmdArgToolbarText, ev->toolbarText);
            InsertArg(&args, arg);
        }
        if (!str::IsEmptyOrWhiteSpace(ev->toolbarSvgIcon)) {
            auto* arg = NewStringArg(kCmdArgToolbarSvgIcon, ev->toolbarSvgIcon);
            InsertArg(&args, arg);
        }
        CreateCustomCommand(StrL(""), CmdViewWithExternalViewer, args, ev->name, ev->key);
    }
}

static void CreateZoomCommands() {
    auto* prefs = gSettings;
    delete prefs->zoomLevelsCmdIds;
    int n = len(*prefs->zoomLevels);
    if (n <= 0) {
        return;
    }
    Vec<int>* cmdIds = new Vec<int>();
    VecReserve(*cmdIds, n);
    prefs->zoomLevelsCmdIds = cmdIds;
    for (int i = 0; i < n; i++) {
        float zoomLevel = (*prefs->zoomLevels)[i];
        CommandArg* arg = NewFloatArg(kCmdArgLevel, zoomLevel);
        auto* cmd = CreateCustomCommand(StrL("CmdZoomCustom"), CmdZoomCustom, arg);
        VecInsertAt(*cmdIds, i, cmd->id);
    }
}

// Every entry in the Shortcuts section is its own thing: it has its own Name,
// its own Key and possibly its own toolbar button. Two entries must therefore
// never share a CustomCommand or command id: the toolbar identifies a button
// (and its tooltip) by command id, so with duplicate ids all but one of the
// buttons ends up without a working tooltip (#5869).
//
// CreateCommandFromDefinition caches by definition string and may return a
// command that keeps its original id (no args). Always CloneCustomCommand so
// each shortcut gets a unique id with its name/key packed into the allocation.
static void CreateCustomShortcuts() {
    for (Shortcut* shortcut : *gSettings->shortcuts) {
        auto* base = CreateCommandFromDefinition(shortcut->cmd);
        if (!base) {
            continue;
        }
        auto* cmd = CloneCustomCommand(base, shortcut->name, shortcut->key);
        shortcut->cmdId = cmd->id;
    }
}

/* Caller needs to CleanUpSettings() */
void ApplySettingsToOpenWindows() {
    for (MainWindow* win : gWindows) {
        // WindowMargin / PageSpacing are copied into DisplayModel at SetUiDpi;
        // pick up the reloaded prefs before the relayout below (issue #6018)
        if (DisplayModel* dm = win->AsFixed()) {
            int dpi = win->frameDpi > 0 ? win->frameDpi : DpiGetForHwnd(win->hwndFrame);
            dm->SetUiDpi(dpi);
        }
        // LoadSettings re-creates custom commands (themes, external viewers,
        // selection handlers, shortcuts) with fresh command ids. Menus still
        // hold the old ids unless rebuilt — without this, e.g. "Set theme '…'"
        // does nothing until restart (issue #5822).
        RebuildMenuBarForWindow(win);
        ReCreateToolbar(win);
        ToolbarUpdateStateForWindow(win, true);
        UpdateFindbox(win);
        // force the relayout: toolbar size/font are not part of the layout
        // state snapshot (see issue #5136), and repaint the toolbar after it
        ScheduleUiUpdate(win, kUiForceRelayout | kUiToolbarDirty);
        win->RedrawAll(true);
    }
}

bool LoadSettings() {
    ReportIf(gSettings);

    auto timeStart = TimeGet();

    Settings* gprefs = nullptr;
    TempStr settingsPath = GetSettingsPathTemp();
    bool migratedDocumentColorsFollowTheme = false;
    {
        Str prefsData = file::ReadFile(settingsPath);

        gSettings = NewSettings(prefsData);
        ReportIf(!gSettings);
        gprefs = gSettings;
        migratedDocumentColorsFollowTheme = MigrateDocumentColorsFollowThemeSetting(prefsData);
        RememberLastSavedPrefs(prefsData);
        str::Free(prefsData);
    }
    if (MigrateRenamedThemeNames()) {
        // the file still named a theme we dropped; save so it stops doing that
        migratedDocumentColorsFollowTheme = true;
    }

    // takes effect for PDFs loaded after this (startup, and on settings reload)
    EngineMupdfSetDisableJavaScript(gSettings->disableJavaScript);
    EngineMupdfSetAllowExternalImages(gSettings->allowExternalImages);
    SetEngineeringDrawingEnhanceMode(gSettings->engineeringDrawingEnhance);
    ExplorerQuickLookApplyFromSettings();

    if (trans::ValidateLangCode(gprefs->uiLanguage)) {
        SetCurrentLang(gprefs->uiLanguage);
    } else {
        // guess the ui language on first start
        str::ReplaceWithCopy(&gprefs->uiLanguage, trans::DetectUserLang());
    }

    gprefs->lastPrefUpdate = file::GetModificationTime(settingsPath);
    // make sure that zoom levels are in the order expected by DisplayModel
    VecSort(*gprefs->zoomLevels, cmpFloat);
    while (len(*gprefs->zoomLevels) > 0 && (*gprefs->zoomLevels)[0] < kZoomMin) {
        VecPopAt(*gprefs->zoomLevels, 0);
    }
    while (len(*gprefs->zoomLevels) > 0 && VecLast(*gprefs->zoomLevels) > kZoomMaxAllowed) {
        VecPop(*gprefs->zoomLevels);
    }
    // the largest level the user listed is the largest zoom we allow (issue
    // #1195). Must come before any zoom is parsed, as it decides which are valid
    kZoomMax = kZoomMaxDefault;
    if (len(*gprefs->zoomLevels) > 0) {
        kZoomMax = std::max(kZoomMax, VecLast(*gprefs->zoomLevels));
    }

    gprefs->defaultDisplayModeEnum = DisplayModeFromString(gprefs->defaultDisplayMode, DisplayMode::Automatic);
    gprefs->defaultZoomFloat = ZoomFromString(gprefs->defaultZoom, kZoomActualSize);
    ReportIf(!IsValidZoom(gprefs->defaultZoomFloat));
    if (gprefs->imageUI.defaultZoom) {
        gprefs->imageUI.defaultZoomFloat = ZoomFromString(gprefs->imageUI.defaultZoom, 0);
    }
    if (gprefs->comicBookUI.defaultZoom) {
        gprefs->comicBookUI.defaultZoomFloat = ZoomFromString(gprefs->comicBookUI.defaultZoom, 0);
    }

    int weekDiff = GetWeekCount() - gprefs->openCountWeek;
    gprefs->openCountWeek = GetWeekCount();
    if (weekDiff > 0) {
        // "age" openCount statistics (cut in in half after every week)
        for (FileState* fs : *gprefs->fileStates) {
            fs->openCount >>= weekDiff;
        }
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
    // 0 means "not set, use system DPI"; users have been seen setting -1,
    // which would propagate as a negative DPI and break zoom calculations
    setMin(gprefs->customScreenDPI, 0);
    setMin(gprefs->tabWidth, 60);
    setMin(gprefs->sidebarDx, 0);
    setMin(gprefs->tocDy, 0);
    setMin(gprefs->treeFontSize, 0);
    if (gprefs->toolbarSize == 0) {
        gprefs->toolbarSize = 18; // same as the ToolbarSize default in gen-settings.ts
    }
    setMinMax(gprefs->toolbarSize, 8, 64);
    setMinMax(gprefs->annotations.freeTextOpacity, 0, 100);

    if (SeqStrIndexIS(gScrollbarModeNames, gprefs->scrollbars) < 0) {
        str::ReplaceWithCopy(&gprefs->scrollbars, StrL("windows"));
    }

    // toolbar mode: if unset/invalid, derive from the legacy showToolbar bool
    // so existing settings (ShowToolbar = false) keep working
    if (SeqStrIndexIS(gToolbarModeNames, gprefs->toolbar) < 0) {
        str::ReplaceWithCopy(&gprefs->toolbar, gprefs->showToolbar ? StrL("show") : StrL("hide"));
    } else {
        // keep the legacy bool consistent with the mode
        gprefs->showToolbar = !str::EqI(gprefs->toolbar, StrL("hide"));
    }

    // fullscreen toolbar mode: same migration from Fullscreen.ShowToolbar
    if (SeqStrIndexIS(gToolbarModeNames, gprefs->fullscreen.toolbar) < 0) {
        str::ReplaceWithCopy(&gprefs->fullscreen.toolbar, gprefs->fullscreen.showToolbar ? StrL("show") : StrL("hide"));
    } else {
        gprefs->fullscreen.showToolbar = !str::EqI(gprefs->fullscreen.toolbar, StrL("hide"));
    }

    if (SeqStrIndexIS(gToolbarPositionNames, gprefs->toolbarPosition) < 0) {
        str::ReplaceWithCopy(&gprefs->toolbarPosition, StrL("top"));
    }

    if (!gprefs->treeFontName) {
        gprefs->treeFontName = StrL("automatic");
    }

    // drop file states without a path: they can't be opened, found by path
    // or shown as a thumbnail, so they're useless and would render as blank
    // thumbnails on the home page
    {
        Vec<FileState*>* fileStates = gprefs->fileStates;
        for (int i = len(*fileStates) - 1; i >= 0; i--) {
            FileState* fs = (*fileStates)[i];
            if (len(fs->filePath) == 0) {
                VecRemoveAt(*fileStates, i);
                DeleteFileState(fs);
            }
        }
    }
    FileHistorySetStates(gprefs->fileStates);
    {
        Str fontName = EbookFontNameFromSetting(gprefs->eBookUI.fontName);
        if (!fontName) {
            fontName = StrL("Georgia");
        }
        float fontSize = gprefs->eBookUI.fontSize;
        if (fontSize <= 0) {
            fontSize = 8.f;
        }
        SetDefaultEbookFont(fontName, fontSize);
        SetDefaultChmFont(EbookFontNameFromSetting(gprefs->chmUI.fontName));
    }

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

    SetCurrentThemeFromSettings();
    ApplySettingsToOpenWindows();
    bool readAloudVoiceCleared = ApplyReadAloudVoiceFromSettings();

    bool needsSave = !file::Exists(settingsPath) || readAloudVoiceCleared || migratedDocumentColorsFollowTheme;
    if (needsSave) {
        SaveSettings();
    }

    logf("LoadSettings('%s') took %.2f ms\n", settingsPath, TimeSinceInMs(timeStart));
    return true;
}

static TabState* CloneTabState(const TabState* src) {
    TabState* dst = (TabState*)AllocStruct<TabState>();
    str::ReplaceWithCopy(&dst->filePath, src->filePath);
    str::ReplaceWithCopy(&dst->displayMode, src->displayMode);
    dst->pageNo = src->pageNo;
    str::ReplaceWithCopy(&dst->zoom, src->zoom);
    dst->rotation = src->rotation;
    dst->scrollPos = src->scrollPos;
    dst->showToc = src->showToc;
    dst->tocState = new Vec<int>(*src->tocState);
    return dst;
}

static SessionData* CloneSessionData(const SessionData* src) {
    SessionData* dst = NewSessionData();
    dst->tabIndex = src->tabIndex;
    dst->windowState = src->windowState;
    dst->windowPos = src->windowPos;
    dst->sidebarDx = src->sidebarDx;
    for (TabState* ts : *src->tabStates) {
        VecAppend(*dst->tabStates, CloneTabState(ts));
    }
    return dst;
}

// session snapshot loaded at startup. Also the source of state for re-saving
// not-yet-loaded (lazy) tabs, kept mirroring the live session by
// SyncInitialSessionData() so it never carries closed-window entries.
Vec<SessionData*>* gInitialSessionData = nullptr;

// find the saved state for a lazy tab by file path. Because gInitialSessionData
// is kept in sync with the live session, this never matches a closed window;
// per-tab disambiguation (e.g. same file in two windows) comes from the more
// reliable tab->tabState, which RememberSessionState prefers.
static TabState* FindSessionTabState(Str fp) {
    if (!gInitialSessionData) {
        return nullptr;
    }
    for (SessionData* psd : *gInitialSessionData) {
        for (TabState* pts : *psd->tabStates) {
            if (str::Eq(pts->filePath, fp)) {
                return pts;
            }
        }
    }
    return nullptr;
}

// lazy tabs borrow tab->tabState from gInitialSessionData. After we replace that
// snapshot, repoint those pointers so the next SaveSettings() does not clone freed
// TabState objects
static void RefreshLazyTabStatePointers() {
    if (!gInitialSessionData) {
        return;
    }
    int sdIdx = 0;
    for (MainWindow* win : gWindows) {
        bool hasFileTab = false;
        for (WindowTab* tab : win->Tabs()) {
            if (tab->filePath) {
                hasFileTab = true;
                break;
            }
        }
        if (!hasFileTab) {
            continue;
        }
        if (sdIdx >= len(*gInitialSessionData)) {
            break;
        }
        SessionData* sd = (*gInitialSessionData)[sdIdx++];
        int tsIdx = 0;
        for (WindowTab* tab : win->Tabs()) {
            if (!tab->filePath) {
                continue;
            }
            if (tsIdx >= len(*sd->tabStates)) {
                break;
            }
            if (!tab->ctrl && tab->tabState) {
                tab->tabState = (*sd->tabStates)[tsIdx];
            }
            tsIdx++;
        }
    }
}

// keep gInitialSessionData mirroring the just-saved live session, so re-saving
// not-yet-loaded tabs never feeds stale state from a closed window back into the
// saved session (fixes #5668). Call after RememberSessionState().
static void SyncInitialSessionData() {
    if (!gInitialSessionData) {
        return;
    }
    FreeSessionDataVec(gInitialSessionData);
    for (SessionData* sd : *gSettings->sessionData) {
        VecAppend(*gInitialSessionData, CloneSessionData(sd));
    }
    RefreshLazyTabStatePointers();
}

static void RememberSessionState() {
    Vec<SessionData*>* sessionState = gSettings->sessionData;
    FreeSessionDataVec(sessionState);

    if (!SettingsRememberOpenedFiles()) {
        return;
    }

    for (auto* win : gWindows) {
        if (win->isQuickLook) {
            continue;
        }
        SessionData* windowState = NewSessionData();
        for (WindowTab* tab : win->Tabs()) {
            if (!tab->filePath) {
                // home page tab
                continue;
            }
            Str fp = tab->filePath;
            if (!tab->ctrl) {
                // file not loaded into a tab (lazy loading, or a placeholder for
                // a missing file). Prefer the tab's own remembered state -- it's
                // authoritative and disambiguates the same file open in multiple
                // windows -- and only fall back to the (in-sync) startup snapshot.
                TabState* src = tab->tabState;
                if (!src) {
                    src = FindSessionTabState(fp);
                }
                if (src) {
                    VecAppend(*windowState->tabStates, CloneTabState(src));
                } else {
                    logf("RememberSessionState: didn't find state for file '%s'\n", fp ? fp : StrL("(none)"));
                }
                continue;
            }
            FileState* fs = NewFileState(fp);
            tab->ctrl->GetDisplayState(fs);
            fs->showToc = tab->showToc;
            *fs->tocState = tab->tocState;
            TabState* ts = NewTabState(fs);
            VecAppend(*windowState->tabStates, ts);
            DeleteFileState(fs);
        }
        if (len(*windowState->tabStates) == 0) {
            FreeSessionData(windowState);
            continue;
        }
        // 1-based index among document tabs only (home / about tab is omitted
        // from TabStates above). Using the UI tab index would mis-restore when
        // the home tab was closed at save time but recreated on the next start.
        int docOrdinal = 0;
        int selectedDocOrdinal = 1;
        WindowTab* cur = win->CurrentTab();
        for (WindowTab* tab : win->Tabs()) {
            if (tab->IsAboutTab() || len(tab->filePath) == 0) {
                continue;
            }
            docOrdinal++;
            if (tab == cur) {
                selectedDocOrdinal = docOrdinal;
            }
        }
        windowState->tabIndex = selectedDocOrdinal;
        // TODO: allow recording this state without changing gSettings
        RememberDefaultWindowPosition(win);
        windowState->windowState = gSettings->windowState;
        windowState->windowPos = gSettings->windowPos;
        windowState->sidebarDx = gSettings->sidebarDx;
        VecAppend(*sessionState, windowState);
    }
}

static void SaveSettingsPosted() {
    if (!gSaveSettingsPending) {
        return;
    }
    gSaveSettingsPending = false;
    SaveSettings();
}

void ScheduleSaveSettings() {
    if (gSaveSettingsPending || gForTesting || gDontSaveSettings) {
        return;
    }
    if (!HasPermission(Perm::SavePreferences)) {
        return;
    }
    gSaveSettingsPending = true;
    auto fn = MkFunc0Void(SaveSettingsPosted);
    uitask::Post(fn, "SaveSettings");
}

// Last-window close and process exit cannot wait for the uitask:
// ShowWindow(SW_HIDE) can tear the window down first, and a fast
// ExitProcess never drains the queue.
void FlushScheduledSaveSettings() {
    if (!gSaveSettingsPending) {
        return;
    }
    SaveSettings();
}

// called whenever global preferences change or a file is
// added or removed from the file history (in order to keep
// the list of recently opened documents in sync)
bool SaveSettings() {
    gSaveSettingsPending = false;
    if (gForTesting) {
        // started with -for-testing for ad-hoc testing: don't modify
        // the settings of the tester
        return true;
    }
    if (gDontSaveSettings) {
        // if we are exiting the application by File->Exit,
        // OnMenuExit will have called SaveSettings() already
        // and we skip the call here to avoid saving incomplete session info
        // (because some windows might have been closed already)
        return true;
    }

    // don't save preferences without the proper permission
    if (!HasPermission(Perm::SavePreferences)) {
        return false;
    }
    logf("SaveSettings\n");
    // update display states for all tabs
    // we snapshot the list because SaveSettings() can be called re-entrantly
    // (e.g. from LoadDocumentFinish while other documents are still loading/closing)
    for (MainWindow* win : gWindows) {
        Vec<WindowTab*> tabs = win->Tabs();
        for (WindowTab* tab : tabs) {
            UpdateTabFileDisplayStateForTab(tab);
        }
    }
    RememberSessionState();
    SyncInitialSessionData();

    // remove entries which should (no longer) be remembered
    FileHistoryPurge(!gSettings->rememberStatePerDocument);
    // update display mode and zoom fields from internal values.
    // "page aspect" is not a DisplayMode enum value — keep the string.
    if (!IsPageAspectDisplayMode(gSettings->defaultDisplayMode)) {
        str::ReplaceWithCopy(&gSettings->defaultDisplayMode, DisplayModeToString(gSettings->defaultDisplayModeEnum));
    }
    ZoomToString(&gSettings->defaultZoom, gSettings->defaultZoomFloat, nullptr);
    if (gSettings->imageUI.defaultZoomFloat != 0) {
        ZoomToString(&gSettings->imageUI.defaultZoom, gSettings->imageUI.defaultZoomFloat, nullptr);
    }
    if (gSettings->comicBookUI.defaultZoomFloat != 0) {
        ZoomToString(&gSettings->comicBookUI.defaultZoom, gSettings->comicBookUI.defaultZoomFloat, nullptr);
    }

    TempStr path = GetSettingsPathTemp();
    ReportIf(!path);
    if (!path) {
        return false;
    }
    TempStr prevPrefs = file::ReadFileWithArena(path, GetTempArena());
    Str prefs = SerializeSettings(gSettings, prevPrefs);
    AutoCall freePrefs((void (*)(Str))str::Free, prefs);
    ReportIf(len(prefs) == 0);
    if (len(prefs) == 0) {
        return false;
    }

    if (IsLastSavedPrefs(prefs) || (prevPrefs.len == prefs.len && str::Eq(prefs, prevPrefs))) {
        RememberLastSavedPrefs(prefs);
        return true;
    }

    WatchedFileSetIgnore(gWatchedSettingsFile, true);
    bool ok = file::WriteFile(path, prefs);
    if (ok) {
        RememberLastSavedPrefs(prefs);
        gSettings->lastPrefUpdate = file::GetModificationTime(path);
    }
    WatchedFileSetIgnore(gWatchedSettingsFile, false);
    return ok;
}

// refresh the preferences when a different SumatraPDF process saves them
// or if they are edited by the user using a text editor
static void ReloadSettings(bool force = false) {
    TempStr settingsPath = GetSettingsPathTemp();
    if (!file::Exists(settingsPath)) {
        return;
    }

    // make sure that the settings file is readable - else wait
    // a short while to prevent accidental data loss
    // this is triggered when e.g. saving the file with VS Code
    bool ok = false;
    Str prefsData{};
    for (int i = 0; !ok && i < 5; i++) {
        str::Free(prefsData);
        prefsData = file::ReadFile(settingsPath);
        if (prefsData.len > 0) {
            ok = true;
            break;
        }
        logf("ReloadSettings: failed to load '%s', i=%d\n", settingsPath, i);
        SleepInMs(200);
    }
    if (!ok) {
        str::Free(prefsData);
        return;
    }

    if (!force && IsLastSavedPrefs(prefsData)) {
        if (gSettings) {
            gSettings->lastPrefUpdate = file::GetModificationTime(settingsPath);
        }
        str::Free(prefsData);
        return;
    }

    FILETIME time = file::GetModificationTime(settingsPath);
    if (!force && gSettings && FileTimeEq(time, gSettings->lastPrefUpdate)) {
        str::Free(prefsData);
        return;
    }
    str::Free(prefsData);

    TempStr uiLanguage = str::DupTemp(gSettings->uiLanguage);
    bool showToolbar = gSettings->showToolbar;

    // the home page layout cache points at FileState objects owned by
    // gSettings; CleanUpSettings() frees them (crash 8c34d7eda)
    HomePageInvalidateLayoutCache();

    FileHistorySetStates(nullptr);
    CleanUpSettings();

    ok = LoadSettings();
    ReportIf(!ok || !gSettings);

    // TODO: about window doesn't have to be at position 0
    if (len(gWindows) > 0 && gWindows[0]->IsCurrentTabAbout()) {
        MainWindow* win = gWindows[0];
        win->DeleteToolTip();
        HomePageDestroyChrome(win);
        win->RedrawAll(true);
    }

    if (!str::Eq(uiLanguage, gSettings->uiLanguage)) {
        SetCurrentLanguageAndRefreshUI(gSettings->uiLanguage);
    }

    for (MainWindow* win : gWindows) {
        if (gSettings->showToolbar != showToolbar) {
            ShowOrHideToolbar(win);
        }
        UpdateFavoritesTree(win);
        UpdateControlsColors(win);
        if (DisplayModel* dm = win->AsFixed()) {
            int dpi = win->frameDpi > 0 ? win->frameDpi : DpiGetForHwnd(win->hwndFrame);
            dm->SetUiDpi(dpi);
        }
        ScheduleUiUpdate(win, kUiForceRelayout | kUiToolbarDirty);
    }

    UpdateDocumentColors();
    UpdateFixedPageScrollbarsVisibility();
}

void CleanUpSettings() {
    DeleteSettings(gSettings);
    gSettings = nullptr;
}

void ForceReloadSettings() {
    ReloadSettings(true);
}

static void ReloadSettingsFromWatcher() {
    ReloadSettings(false);
}

static void SchedulePrefsReload() {
    auto fn = MkFunc0Void(ReloadSettingsFromWatcher);
    uitask::Post(fn, "TaskReloadSettings");
}

void RegisterSettingsForFileChanges() {
    if (!HasPermission(Perm::SavePreferences)) {
        return;
    }

    ReportIf(gWatchedSettingsFile); // only call me once
    TempStr path = GetSettingsPathTemp();
    auto fn = MkFunc0Void(SchedulePrefsReload);
    gWatchedSettingsFile = FileWatcherSubscribe(path, fn, true);
}

void UnregisterSettingsForFileChanges() {
    FileWatcherUnsubscribe(gWatchedSettingsFile);
    gWatchedSettingsFile = nullptr;
}

constexpr int kMinFontSize = 9;

// metrics for an explicit DPI (system dpi when GetNonClientMetricsForDpi fails)
static void GetNonClientMetricsForDpiValue(int dpi, NONCLIENTMETRICS* ncm) {
    if (dpi <= 0) {
        dpi = 96;
    }
    if (!GetNonClientMetricsForDpi(dpi, ncm)) {
        ncm->cbSize = sizeof(*ncm);
        SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(*ncm), ncm, 0);
    }
}

// Font size follows the current layout DPI (dpiX/dpiY), so UI text scales
// when a window is moved to a monitor with a different scale factor.
// A user-set UIFontSize is used as-is at every dpi.
int GetAppMenuFontSizeForDpi(int dpi) {
    if (gSettings->uIFontSize >= kMinFontSize) {
        return gSettings->uIFontSize;
    }
    NONCLIENTMETRICS ncm{};
    GetNonClientMetricsForDpiValue(dpi, &ncm);
    return std::abs(ncm.lfMenuFont.lfHeight);
}

int GetAppMenuFontSize() {
    return GetAppMenuFontSizeForDpi(DpiGet());
}

int GetAppFontSizeForDpi(int dpi) {
    auto fntSize = gSettings->uIFontSize;
    if (fntSize < kMinFontSize) {
        // match the menu font so tabs/toolbar text scale like native menus
        fntSize = GetAppMenuFontSizeForDpi(dpi);
    }
    return fntSize;
}

int GetAppFontSize() {
    return GetAppFontSizeForDpi(DpiGet());
}

PlatformFont* GetAppFontForDpi(int dpi) {
    UiFontsAtDpi* fonts = GetUiFontsAtDpi(dpi);
    if (fonts->appFont) {
        return fonts->appFont;
    }
    fonts->appFont = GetUserGuiFont(StrL("auto"), GetAppFontSizeForDpi(dpi));
    return fonts->appFont;
}

PlatformFont* GetAppFont() {
    return GetAppFontForDpi(DpiGet());
}

constexpr int kMinBiggerFontSize = 14;

// if user provided font size, we use that
// otherwise we return 1.2x of default font size but no smaller than 14
static int GetAppBiggerFontSizeForDpi(int dpi) {
    int fntSize = gSettings->uIFontSize;
    if (fntSize < kMinFontSize) {
        fntSize = GetAppMenuFontSizeForDpi(dpi);
        fntSize = (fntSize * 12) / 10;
        fntSize = std::max(fntSize, kMinBiggerFontSize);
    }
    return fntSize;
}

PlatformFont* GetAppBiggerFontForDpi(int dpi) {
    UiFontsAtDpi* fonts = GetUiFontsAtDpi(dpi);
    if (fonts->biggerAppFont) {
        return fonts->biggerAppFont;
    }
    fonts->biggerAppFont = GetDefaultGuiFontOfSize(GetAppBiggerFontSizeForDpi(dpi));
    return fonts->biggerAppFont;
}

PlatformFont* GetAppBiggerFont() {
    return GetAppBiggerFontForDpi(DpiGet());
}

PlatformFont* GetAppTreeFontExForDpi(int dpi, bool bold, bool italic) {
    int idx = (bold ? 1 : 0) | (italic ? 2 : 0);
    UiFontsAtDpi* fonts = GetUiFontsAtDpi(dpi);
    if (fonts->treeFontEx[idx]) {
        return fonts->treeFontEx[idx];
    }
    int fntSize = gSettings->treeFontSize;
    if (fntSize < kMinFontSize) {
        fntSize = gSettings->uIFontSize;
    }
    if (fntSize < kMinFontSize) {
        fntSize = GetAppMenuFontSizeForDpi(dpi);
    }
    Str fntNameUser = gSettings->treeFontName;
    fonts->treeFontEx[idx] = GetUserGuiFontEx(fntNameUser, fntSize, bold, italic);
    return fonts->treeFontEx[idx];
}

PlatformFont* GetAppTreeFontForDpi(int dpi) {
    return GetAppTreeFontExForDpi(dpi, false, false);
}

PlatformFont* GetAppTreeFont() {
    return GetAppTreeFontEx(false, false);
}

PlatformFont* GetAppTreeFontEx(bool bold, bool italic) {
    return GetAppTreeFontExForDpi(DpiGet(), bold, italic);
}

PlatformFont* GetAppSidebarLabelFontForDpi(int dpi) {
    UiFontsAtDpi* fonts = GetUiFontsAtDpi(dpi);
    if (fonts->sidebarLabelFont) {
        return fonts->sidebarLabelFont;
    }
    fonts->sidebarLabelFont = GetUserGuiFontEx({}, GetAppBiggerFontSizeForDpi(dpi), true, false);
    return fonts->sidebarLabelFont;
}

PlatformFont* GetAppSidebarLabelFont() {
    return GetAppSidebarLabelFontForDpi(DpiGet());
}

PlatformFont* GetAppMenuFontForDpi(int dpi) {
    UiFontsAtDpi* fonts = GetUiFontsAtDpi(dpi);
    if (fonts->appMenuFont) {
        return fonts->appMenuFont;
    }
    NONCLIENTMETRICS ncm{};
    GetNonClientMetricsForDpiValue(dpi, &ncm);
    int fntSize = GetAppMenuFontSizeForDpi(dpi);
    ncm.lfMenuFont.lfHeight = -fntSize;
    fonts->appMenuFont = GetPlatformFont(CreateFontIndirectW(&ncm.lfMenuFont));
    return fonts->appMenuFont;
}

PlatformFont* GetAppMenuFont() {
    return GetAppMenuFontForDpi(DpiGet());
}

bool IsMenuFontSizeDefault() {
    auto fntSize = gSettings->uIFontSize;
    return fntSize < kMinFontSize;
}

bool IsAppFontSizeDefault() {
    auto fntSize = gSettings->uIFontSize;
    return fntSize < kMinFontSize;
}

TempStr ZoomLevelStr(float zoom) {
    if (zoom == kZoomFitPage) {
        return _TRA("Fit Page");
    }
    if (zoom == kZoomFitWidth) {
        return _TRA("Fit Width");
    }
    if (zoom == kZoomFitHeight) {
        return _TRA("Fit Height");
    }
    if (zoom == kZoomFitContent) {
        return _TRA("Fit Content");
    }
    if (zoom == kZoomShrinkToFit) {
        return _TRA("Shrink To Fit");
    }
    if (zoom == kZoomFitByOrientation) {
        return _TRA("Fit by Orientation");
    }
    if (zoom == 0) {
        return StrL("-");
    }
    return fmt("%.f%%", zoom);
}

// clang-format off
static float gZoomLevels[] = {
    kZoomFitPage,
    kZoomFitWidth,
    kZoomFitHeight,
    kZoomFitByOrientation,
    kZoomFitContent,
    kZoomShrinkToFit,
    0,
    6400.0,
    3200.0,
    1600.0,
    800.0,
    400.0,
    200.0,
    150.0,
    125.0,
    100.0,
    50.0,
    25.0,
    12.5,
    8.33f
};
static float gZoomLevelsChm[] = {
    800.0,
    400.0,
    200.0,
    150.0,
    125.0,
    100.0,
    50.0,
    25.0,
};
// clang-format on

// Fit/preset zoom values for the zoom combo (Settings) and Custom Zoom dialog.
void CollectZoomLevels(Vec<float>& out, bool forChm) {
    VecReset(out);
    auto* customZoomLevels = gSettings->zoomLevels;
    int n = customZoomLevels ? len(*customZoomLevels) : 0;
    if (n > 0) {
        if (!forChm) {
            for (int i = 0; i < 4; i++) {
                VecAppend(out, gZoomLevels[i]);
            }
        }
        float maxZoom = forChm ? 800 : kZoomMax;
        float minZoom = forChm ? 16 : kZoomMin;
        for (int i = 0; i < n; i++) {
            float zl = (*customZoomLevels)[n - i - 1]; // largest first
            if (zl >= minZoom && zl <= maxZoom) {
                VecAppend(out, zl);
            }
        }
        return;
    }
    float* zoomLevels = forChm ? gZoomLevelsChm : gZoomLevels;
    n = forChm ? dimofi(gZoomLevelsChm) : dimofi(gZoomLevels);
    for (int i = 0; i < n; i++) {
        VecAppend(out, zoomLevels[i]);
    }
}

Settings* gSettings = nullptr;

// Walk setting metadata for a Bool field matching name (case-insensitive leaf
// or full dotted path). Returns a pointer into gSettings, or nullptr.
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
bool* FindSettingsBoolSetting(Str name) {
    if (!gSettings || len(name) == 0) {
        return nullptr;
    }
    return FindBoolSettingInStruct(&gSettingsInfo, (u8*)gSettings, {}, name);
}

FileState* NewFileState(Str filePath) {
    FileState* fs = (FileState*)DeserializeStruct(&gFileStateInfo, {});
    SetFileStatePath(fs, filePath);
    return fs;
}

FileEBookUI* NewFileEBookUI() {
    return (FileEBookUI*)DeserializeStruct(&gFileEBookUIInfo, {});
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
    Favorite* fav = (Favorite*)DeserializeStruct(&gFavoriteInfo, {});
    fav->pageNo = pageNo;
    str::ReplaceWithCopy(&fav->name, name);
    str::ReplaceWithCopy(&fav->pageLabel, pageLabel);
    return fav;
}

void DeleteFavorite(Favorite* fav) {
    FreeStruct(&gFavoriteInfo, fav);
}

Settings* NewSettings(Str data) {
    return (Settings*)DeserializeStruct(&gSettingsInfo, data);
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

// prevData is used to preserve fields that exists in prevField but not in Settings
// caller has to free()
Str SerializeSettings(Settings* prefs, Str prevData) {
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
                VecAppend(withFavorites, fs);
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

    Str serialized = SerializeStruct(&gSettingsInfo, prefs, prevData);

    if (dropPerDocState) {
        gFileStateInfo.fieldCount = dimof(gFileStateFields);
        prefs->fileStates = allFileStates;
    }

    return serialized;
}

void DeleteSettings(Settings* gp) {
    if (!gp) {
        return;
    }

    for (FileState* ds : *gp->fileStates) {
        FreePixmap(ds->thumbnail);
    }
    FreeStruct(&gSettingsInfo, gp);
}

SessionData* NewSessionData() {
    return (SessionData*)DeserializeStruct(&gSessionDataInfo, {});
}

TabState* NewTabState(FileState* fs) {
    TabState* state = (TabState*)DeserializeStruct(&gTabStateInfo, {});
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
    VecReset(*sessionData);
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
