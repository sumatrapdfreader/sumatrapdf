/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/File.h"
#include "base/FileWatcher.h"
#include "base/SquareTreeParser.h"
#include "base/UITask.h"
#include "base/Win.h"
#include "gui/Dpi.h"
#include "gui/PlatformFont.h"
#include "base/Timer.h"

#include "gui/UIModels.h"

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
#include "GlobalPrefs.h"
#include "SumatraPDF.h"
#include "WindowTab.h"
#include "MainWindow.h"
#include "AppSettings.h"
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

// workaround for OnMenuExit
// if this flag is set, CloseWindow will not save prefs before closing the window.
bool gDontSaveSettings = false;

static bool ApplyReadAloudVoiceFromSettings() {
    if (!gGlobalPrefs) {
        return false;
    }

    float speed = gGlobalPrefs->readAloudSpeed;
    TtsSetSpeed(speed > 0 ? speed : 1.0f);

    Str voiceId = gGlobalPrefs->readAloudVoiceId;
    if (!voiceId) {
        TtsSetVoiceById("");
        return false;
    }

    if (!TtsSetVoiceById(voiceId)) {
        logf("ApplyReadAloudVoiceFromSettings: voice '%s' not available, using system default\n", voiceId);
        str::ReplaceWithCopy(&gGlobalPrefs->readAloudVoiceId, Str{});
        TtsSetVoiceById("");
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
        Color text = ParseColor(gGlobalPrefs->fixedPageUI.textColor.s, kColBlackDefault);
        Color bg = ParseColor(gGlobalPrefs->fixedPageUI.backgroundColor.s, kColWhiteDefault);
        SetColorText(gGlobalPrefs->fixedPageUI.textColor, SerializeColorTemp(bg));
        SetColorText(gGlobalPrefs->fixedPageUI.backgroundColor, SerializeColorTemp(text));
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
    gUiFontsAtDpi.Append(e);
    return &gUiFontsAtDpi[n];
}

// TODO: if font sizes change, would need to re-layout the app
static void ResetCachedFonts() {
    // Fonts are interned PlatformFonts, so just drop these per-DPI references;
    // old fonts stay valid for windows that still hold them.
    gUiFontsAtDpi.Reset();
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
    return str::DupTemp("SumatraPDF-settings.txt");
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

    for (auto& sh : *gGlobalPrefs->selectionHandlers) {
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
        CreateCustomCommand(definition, CmdSelectionHandler, args, sh->name, sh->key);
    }
}

static void CreateExternalViewersCommands() {
    for (ExternalViewer* ev : *gGlobalPrefs->externalViewers) {
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
        CreateCustomCommand("", CmdViewWithExternalViewer, args, ev->name, ev->key);
    }
}

static void CreateZoomCommands() {
    auto* prefs = gGlobalPrefs;
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
        auto* cmd = CreateCustomCommand("CmdZoomCustom", CmdZoomCustom, arg);
        cmdIds->InsertAt(i, cmd->id);
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
    for (Shortcut* shortcut : *gGlobalPrefs->shortcuts) {
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
    ReportIf(gGlobalPrefs);

    auto timeStart = TimeGet();

    GlobalPrefs* gprefs = nullptr;
    TempStr settingsPath = GetSettingsPathTemp();
    bool migratedDocumentColorsFollowTheme = false;
    {
        Str prefsData = file::ReadFile(settingsPath);

        gGlobalPrefs = NewGlobalPrefs(prefsData);
        ReportIf(!gGlobalPrefs);
        gprefs = gGlobalPrefs;
        migratedDocumentColorsFollowTheme = MigrateDocumentColorsFollowThemeSetting(prefsData);
        str::Free(prefsData);
    }
    if (MigrateRenamedThemeNames()) {
        // the file still named a theme we dropped; save so it stops doing that
        migratedDocumentColorsFollowTheme = true;
    }

    // takes effect for PDFs loaded after this (startup, and on settings reload)
    EngineMupdfSetDisableJavaScript(gGlobalPrefs->disableJavaScript);
    EngineMupdfSetAllowExternalImages(gGlobalPrefs->allowExternalImages);
    SetEngineeringDrawingEnhanceMode(gGlobalPrefs->engineeringDrawingEnhance);

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
        gprefs->zoomLevels->PopAt(0);
    }
    while (len(*gprefs->zoomLevels) > 0 && gprefs->zoomLevels->Last() > kZoomMaxAllowed) {
        gprefs->zoomLevels->Pop();
    }
    // the largest level the user listed is the largest zoom we allow (issue
    // #1195). Must come before any zoom is parsed, as it decides which are valid
    kZoomMax = kZoomMaxDefault;
    if (len(*gprefs->zoomLevels) > 0) {
        kZoomMax = std::max(kZoomMax, gprefs->zoomLevels->Last());
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
        str::ReplaceWithCopy(&gprefs->scrollbars, "windows");
    }

    // toolbar mode: if unset/invalid, derive from the legacy showToolbar bool
    // so existing settings (ShowToolbar = false) keep working
    if (SeqStrIndexIS(gToolbarModeNames, gprefs->toolbar) < 0) {
        str::ReplaceWithCopy(&gprefs->toolbar, gprefs->showToolbar ? "show" : "hide");
    } else {
        // keep the legacy bool consistent with the mode
        gprefs->showToolbar = !str::EqI(gprefs->toolbar, StrL("hide"));
    }

    // fullscreen toolbar mode: same migration from Fullscreen.ShowToolbar
    if (SeqStrIndexIS(gToolbarModeNames, gprefs->fullscreen.toolbar) < 0) {
        str::ReplaceWithCopy(&gprefs->fullscreen.toolbar, gprefs->fullscreen.showToolbar ? "show" : "hide");
    } else {
        gprefs->fullscreen.showToolbar = !str::EqI(gprefs->fullscreen.toolbar, StrL("hide"));
    }

    if (SeqStrIndexIS(gToolbarPositionNames, gprefs->toolbarPosition) < 0) {
        str::ReplaceWithCopy(&gprefs->toolbarPosition, "top");
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
                fileStates->RemoveAt(i);
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
        dst->tabStates->Append(CloneTabState(ts));
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
    for (SessionData* sd : *gGlobalPrefs->sessionData) {
        gInitialSessionData->Append(CloneSessionData(sd));
    }
    RefreshLazyTabStatePointers();
}

static void RememberSessionState() {
    Vec<SessionData*>* sessionState = gGlobalPrefs->sessionData;
    FreeSessionDataVec(sessionState);

    if (!SettingsRememberOpenedFiles()) {
        return;
    }

    for (auto* win : gWindows) {
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
                    windowState->tabStates->Append(CloneTabState(src));
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
            windowState->tabStates->Append(ts);
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
        // TODO: allow recording this state without changing gGlobalPrefs
        RememberDefaultWindowPosition(win);
        windowState->windowState = gGlobalPrefs->windowState;
        windowState->windowPos = gGlobalPrefs->windowPos;
        windowState->sidebarDx = gGlobalPrefs->sidebarDx;
        sessionState->Append(windowState);
    }
}

// called whenever global preferences change or a file is
// added or removed from the file history (in order to keep
// the list of recently opened documents in sync)
bool SaveSettings() {
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
    FileHistoryPurge(!gGlobalPrefs->rememberStatePerDocument);
    // update display mode and zoom fields from internal values.
    // "page aspect" is not a DisplayMode enum value — keep the string.
    if (!IsPageAspectDisplayMode(gGlobalPrefs->defaultDisplayMode)) {
        str::ReplaceWithCopy(&gGlobalPrefs->defaultDisplayMode,
                             DisplayModeToString(gGlobalPrefs->defaultDisplayModeEnum));
    }
    ZoomToString(&gGlobalPrefs->defaultZoom, gGlobalPrefs->defaultZoomFloat, nullptr);
    if (gGlobalPrefs->imageUI.defaultZoomFloat != 0) {
        ZoomToString(&gGlobalPrefs->imageUI.defaultZoom, gGlobalPrefs->imageUI.defaultZoomFloat, nullptr);
    }
    if (gGlobalPrefs->comicBookUI.defaultZoomFloat != 0) {
        ZoomToString(&gGlobalPrefs->comicBookUI.defaultZoom, gGlobalPrefs->comicBookUI.defaultZoomFloat, nullptr);
    }

    TempStr path = GetSettingsPathTemp();
    ReportIf(!path);
    if (!path) {
        return false;
    }
    TempStr prevPrefs = file::ReadFileWithArena(path, GetTempArena());
    Str prefs = SerializeGlobalPrefs(gGlobalPrefs, prevPrefs);
    AutoCall freePrefs((void (*)(Str))str::Free, prefs);
    ReportIf(len(prefs) == 0);
    if (len(prefs) == 0) {
        return false;
    }

    // only save if anything's changed at all
    if (prevPrefs.len == prefs.len && str::Eq(prefs, prevPrefs)) {
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
        SleepInMs(200);
        Str prefsData = file::ReadFile(settingsPath);
        if (prefsData.len > 0) {
            ok = true;
            str::Free(prefsData);
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

    TempStr uiLanguage = str::DupTemp(gGlobalPrefs->uiLanguage);
    bool showToolbar = gGlobalPrefs->showToolbar;

    // the home page layout cache points at FileState objects owned by
    // gGlobalPrefs; CleanUpSettings() frees them (crash 8c34d7eda)
    HomePageInvalidateLayoutCache();

    FileHistorySetStates(nullptr);
    CleanUpSettings();

    ok = LoadSettings();
    ReportIf(!ok || !gGlobalPrefs);

    // TODO: about window doesn't have to be at position 0
    if (len(gWindows) > 0 && gWindows[0]->IsCurrentTabAbout()) {
        MainWindow* win = gWindows[0];
        win->DeleteToolTip();
        HomePageDestroyChrome(win);
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

// reload settings from disk even if the file's timestamp matches
// gGlobalPrefs->lastPrefUpdate (e.g. right after we saved it ourselves)
void ForceReloadSettings() {
    gGlobalPrefs->lastPrefUpdate = {};
    ReloadSettings();
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
    if (gGlobalPrefs->uIFontSize >= kMinFontSize) {
        return gGlobalPrefs->uIFontSize;
    }
    NONCLIENTMETRICS ncm{};
    GetNonClientMetricsForDpiValue(dpi, &ncm);
    return std::abs(ncm.lfMenuFont.lfHeight);
}

int GetAppMenuFontSize() {
    return GetAppMenuFontSizeForDpi(DpiGet());
}

int GetAppFontSizeForDpi(int dpi) {
    auto fntSize = gGlobalPrefs->uIFontSize;
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
    fonts->appFont = GetUserGuiFont("auto", GetAppFontSizeForDpi(dpi));
    return fonts->appFont;
}

PlatformFont* GetAppFont() {
    return GetAppFontForDpi(DpiGet());
}

constexpr int kMinBiggerFontSize = 14;

// if user provided font size, we use that
// otherwise we return 1.2x of default font size but no smaller than 14
static int GetAppBiggerFontSizeForDpi(int dpi) {
    int fntSize = gGlobalPrefs->uIFontSize;
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
    int fntSize = gGlobalPrefs->treeFontSize;
    if (fntSize < kMinFontSize) {
        fntSize = gGlobalPrefs->uIFontSize;
    }
    if (fntSize < kMinFontSize) {
        fntSize = GetAppMenuFontSizeForDpi(dpi);
    }
    Str fntNameUser = gGlobalPrefs->treeFontName;
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
    fonts->sidebarLabelFont = GetUserGuiFontEx(nullptr, GetAppBiggerFontSizeForDpi(dpi), true, false);
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
    auto fntSize = gGlobalPrefs->uIFontSize;
    return fntSize < kMinFontSize;
}

bool IsAppFontSizeDefault() {
    auto fntSize = gGlobalPrefs->uIFontSize;
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
        return "-";
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
    out.Reset();
    auto* customZoomLevels = gGlobalPrefs->zoomLevels;
    int n = customZoomLevels ? len(*customZoomLevels) : 0;
    if (n > 0) {
        if (!forChm) {
            for (int i = 0; i < 4; i++) {
                out.Append(gZoomLevels[i]);
            }
        }
        float maxZoom = forChm ? 800 : kZoomMax;
        float minZoom = forChm ? 16 : kZoomMin;
        for (int i = 0; i < n; i++) {
            float zl = (*customZoomLevels)[n - i - 1]; // largest first
            if (zl >= minZoom && zl <= maxZoom) {
                out.Append(zl);
            }
        }
        return;
    }
    float* zoomLevels = forChm ? gZoomLevelsChm : gZoomLevels;
    n = forChm ? dimofi(gZoomLevelsChm) : dimofi(gZoomLevels);
    for (int i = 0; i < n; i++) {
        out.Append(zoomLevels[i]);
    }
}
