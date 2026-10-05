/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// ng: the session snapshot and session restore. orig has the snapshot half in
// AppSettings.cpp and the restore half in SumatraPDF.cpp; both need MainWindow
// / WindowTab, which the `app` static library must not depend on (it links
// into the console tools too), so they live here, in the SumatraPDF target.
// The code is orig's.

#include "base/Base.h"
#include "base/File.h"
#include "base/GuessFileType.h"

#include "gui/UIModels.h"

#include "Settings.h"
#include "AppSettings.h"
#include "DisplayMode.h"
#include "DocController.h"
#include "EngineBase.h"
#include "EngineAll.h"
#include "DisplayModel.h"
#include "PagePosition.h"
#include "FileHistory.h"
#include "SumatraConfig.h"
#include "SumatraPDF.h"
#include "MainWindow.h"
#include "WindowTab.h"
#include "Tabs.h"
#include "TableOfContents.h"
#include "gui/Sidebar.h"
#include "SessionState.h"
#include "SumatraLog.h"

static TabState* CloneTabState(const TabState* src) {
    TabState* dst = (TabState*)AllocStruct<TabState>();
    str::ReplaceWithCopy(&dst->filePath, src->filePath);
    str::ReplaceWithCopy(&dst->displayMode, src->displayMode);
    str::ReplaceWithCopy(&dst->pageNo, src->pageNo);
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
        SessionData* sd = nullptr;
        if (gInitialSessionData && sdIdx < len(*gInitialSessionData)) {
            sd = (*gInitialSessionData)[sdIdx++];
        }
        int tsIdx = 0;
        for (WindowTab* tab : win->Tabs()) {
            if (len(tab->filePath) == 0) {
                continue;
            }
            TabState* ts = nullptr;
            if (sd && tsIdx < len(*sd->tabStates)) {
                ts = (*sd->tabStates)[tsIdx];
            }
            tsIdx++;
            if (!tab->ctrl && tab->tabState) {
                // null when the new snapshot has nothing to borrow: the old one
                // was just freed and must not be left dangling
                tab->tabState = ts;
            }
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
            if (len(tab->filePath) == 0) {
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
        RememberDefaultWindowPosition(win);
        windowState->windowState = gSettings->windowState;
        windowState->windowPos = gSettings->windowPos;
        windowState->sidebarDx = gSettings->sidebarDx;
        VecAppend(*sessionState, windowState);
    }
}

// ng: what orig's SaveSettings() does inline; installed as
// gRememberSessionStateFn so the `app` library does not need MainWindow
static void RememberSessionStateForSave() {
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
}

void InstallSessionStateHook() {
    gRememberSessionStateFn = RememberSessionStateForSave;
}

TabState* NewTabStateFromTab(WindowTab* tab) {
    if (!tab || !tab->ctrl) {
        return nullptr;
    }
    FileState* fs = NewFileState(tab->filePath);
    tab->ctrl->GetDisplayState(fs);
    fs->showToc = tab->showToc;
    str::ReplaceWithCopy(&fs->sidebarView, SidebarContentToStr(tab->sidebarContent));
    *fs->tocState = tab->tocState;

    TabState* state = NewTabState(fs);
    DeleteFileState(fs);
    return state;
}

void SetTabState(WindowTab* tab, TabState* state) {
    if (!tab || !tab->ctrl) {
        return;
    }

    auto* win = tab->win;
    DocController* ctrl = tab->ctrl;
    DisplayModel* dm = tab->AsFixed();

    // validate page number from session state
    if (ctrl->HasChapters()) {
        MigrateStoredPagePos(ctrl, &state->pageNo);
    }
    StoredPagePos storedPos = ParseStoredPagePos(state->pageNo);
    int pageNo = PageNoFromStoredPagePos(ctrl, state->pageNo);
    if (pageNo < 1) {
        pageNo = 1;
        state->scrollPos = {-1, -1};
    } else {
        // PageNoFromStoredPagePos() already synced dm (for a bookmark, via
        // LookupBookmark -> PageNoFromLocation), so this count is fresh
        int nPages = ctrl->PageCount();
        if (pageNo > nPages) {
            pageNo = nPages;
            state->scrollPos = {-1, -1};
        }
    }

    tab->tocState = *state->tocState;
    tab->sidebarContent = SidebarContentFromStr(state->sidebarView, SidebarContent::Bookmarks);
    SetSidebarVisibility(win, state->showToc, gSettings->showFavorites);

    DisplayMode displayMode = DisplayModeFromString(state->displayMode, DisplayMode::Automatic);
    if (displayMode != DisplayMode::Automatic) {
        SwitchToDisplayMode(win, displayMode);
    }

    // zoom first: Relayout keeps the current pixel Y, so doing it after
    // SetScrollState lands on the wrong page if the load used a different zoom
    float zoom = ZoomFromString(state->zoom, kInvalidZoom);
    if (zoom != kInvalidZoom) {
        if (dm) {
            dm->Relayout(zoom, state->rotation);
        } else {
            ctrl->SetZoomVirtual(zoom, nullptr);
        }
    }

    if (dm) {
        ScrollState scrollState = {pageNo, state->scrollPos.x, state->scrollPos.y};
        if (storedPos.bookmark) {
            // legacy plain int stays flat by design; only a bookmark restores by Location
            scrollState.loc = ctrl->LocationFromPageNo(pageNo);
        }
        dm->SetScrollState(scrollState);
    } else {
        ctrl->GoToPage(pageNo, true);
    }
}

static void RestoreMissingTabOnStartup(MainWindow* win, TabState* state) {
    logf("RestoreTabOnStartup: file not found '%s', creating placeholder tab\n", state->filePath);
    FileHistoryMarkFileInexistent(state->filePath, true);
    WindowTab* tab = new WindowTab(win);
    tab->SetFilePath(state->filePath);
    tab->tabState = state;
    AddTabToWindow(win, tab);
}

// while the session's tabs are being created: adding a tab selects it, which
// must not load a lazy one
static bool gRestoringSession = false;

// orig's LoadArgs::lazyLoad: with LazyLoading the tab is created without its
// document, which loads on the first switch to the tab
static void RestoreLazyTabOnStartup(MainWindow* win, TabState* state) {
    WindowTab* tab = new WindowTab(win);
    tab->SetFilePath(state->filePath);
    tab->tabState = state;
    AddTabToWindow(win, tab);
}

// orig's LoadModelIntoTab: "if (gSettings->lazyLoading && !tab->ctrl &&
// tab->loadState == LoadState::None) ReloadDocument(win, false)". ng: the
// state the tab was saved with (page, zoom, sidebar) is applied here, where
// orig passes it through LoadArgs::tabState.
void LoadLazyTabIfNeeded(WindowTab* tab) {
    if (gRestoringSession || !gSettings->lazyLoading || !tab || tab->ctrl || tab->IsNonDocumentTab()) {
        return;
    }
    if (tab->loadState != WindowTab::LoadState::None || !tab->tabState) {
        return;
    }
    MainWindow* win = tab->win;
    if (!IsMainWindowValidAndNotClosing(win) || win->CurrentTab() != tab) {
        return;
    }
    // a file that has gone away keeps its placeholder tab
    if (!DocumentPathExists(tab->filePath)) {
        return;
    }
    TabState* state = tab->tabState;
    logf("LoadLazyTabIfNeeded: '%s'\n", tab->filePath);
    tab->loadState = WindowTab::LoadState::Loading;
    ReloadDocument(win, false);
    WindowTab* curr = win->CurrentTab();
    if (curr && curr->ctrl) {
        SetTabState(curr, state);
    } else if (curr == tab) {
        tab->loadState = WindowTab::LoadState::Error;
    }
}

// The placeholder path below is orig's, for a file that has gone away.
static void RestoreTabOnStartup(MainWindow* win, TabState* state) {
    logf("RestoreTabOnStartup: state->filePath: '%s'\n", state->filePath);
    if (!DocumentPathExists(state->filePath)) {
        RestoreMissingTabOnStartup(win, state);
        return;
    }
    if (gSettings->lazyLoading && SettingsUseTabs()) {
        RestoreLazyTabOnStartup(win, state);
        return;
    }
    if (!LoadDocument(win, state->filePath, LoadPrefs::DontSave)) {
        RestoreMissingTabOnStartup(win, state);
        return;
    }
    SetTabState(win->CurrentTab(), state);
}

// TabIndex is 1-based among document tabs (the home tab is not in TabStates).
// Also accept legacy sessions that stored a UI index including home.
static void SelectRestoredTab(MainWindow* win, int want) {
    Vec<WindowTab*> tabs = win->Tabs();
    int nTabs = len(tabs);
    int selectIdx = 0;
    int docOrdinal = 0;
    int firstDocIdx = -1;
    int matchDocIdx = -1;
    for (int i = 0; i < nTabs; i++) {
        if (tabs[i]->IsAboutTab()) {
            continue;
        }
        if (firstDocIdx < 0) {
            firstDocIdx = i;
        }
        docOrdinal++;
        if (docOrdinal == want) {
            matchDocIdx = i;
        }
    }
    if (matchDocIdx >= 0) {
        selectIdx = matchDocIdx;
    } else if (want >= 1 && want <= nTabs && !tabs[want - 1]->IsAboutTab()) {
        selectIdx = want - 1;
    } else if (firstDocIdx >= 0) {
        selectIdx = firstDocIdx;
    }
    TabsSelect(win, selectIdx);
}

// orig's WinMain keeps the session it started with alive so lazily loaded tabs
// can still read their TabState; it also makes sure re-reading the settings
// file can't overwrite it
void TakeInitialSessionData() {
    gInitialSessionData = gSettings->sessionData;
    gSettings->sessionData = new Vec<SessionData*>();
}

bool SettingsRestoreSession() {
    return gSettings->restoreSession && !gForTesting;
}

// orig's RestoreSession: one window per SessionData. The first entry goes into
// the window that already exists, every other one opens a window of its own.
bool RestoreSession(MainWindow* firstWin) {
    if (!SettingsRestoreSession() || !gInitialSessionData || len(*gInitialSessionData) == 0) {
        return false;
    }
    int nRestored = 0;
    int nWindows = 0;
    for (SessionData* data : *gInitialSessionData) {
        MainWindow* win = nWindows == 0 ? firstWin : CreateAndShowMainWindow(data);
        if (!win) {
            continue;
        }
        if (win == firstWin) {
            // orig creates every window from its SessionData; the first one
            // exists already, so it is moved to where the session had it
            PlaceMainWindow(win, data, PlaceWindowWhen::Now);
        }
        if (data->sidebarDx > 0) {
            win->sidebarDx = data->sidebarDx;
        }
        int nInWindow = 0;
        gRestoringSession = true;
        for (TabState* state : *data->tabStates) {
            if (len(state->filePath) == 0) {
                logf("RestoreSession: skipping a TabState with an empty filePath\n");
                continue;
            }
            RestoreTabOnStartup(win, state);
            nInWindow++;
        }
        gRestoringSession = false;
        if (nInWindow == 0) {
            // an empty window is one the user never sees; only the first one
            // (which already existed) stays
            if (nWindows > 0) {
                CloseWindow(win, false, false);
            }
            continue;
        }
        SelectRestoredTab(win, data->tabIndex);
        if (gSettings->lazyLoading) {
            // trigger loading of the document
            LoadLazyTabIfNeeded(win->CurrentTab());
        }
        if (data->windowState == WIN_STATE_FULLSCREEN) {
            EnterFullScreen(win);
        }
        nRestored += nInWindow;
        nWindows++;
    }
    logf("RestoreSession: restored %d tabs in %d windows\n", nRestored, nWindows);
    return nRestored > 0;
}
