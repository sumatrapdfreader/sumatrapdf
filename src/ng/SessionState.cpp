/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// Restore saved session data in the ng window shell.

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
#include "AIChatTypes.h"
#include "WindowTab.h"
#include "Tabs.h"
#include "TableOfContents.h"
#include "gui/Sidebar.h"
#include "gui/TabsUI.h"
#include "VirtualDesktop.h"
#include "SessionStateCommon.h"
#include "gui/AppShell.h"
#include "SessionState.h"
#include "StripGroups.h"
#include "SumatraLog.h"

void InstallSessionStateHook() {
    gRememberSessionStateFn = SaveWindowSessionState;
}

static void RestoreMissingTabOnStartup(MainWindow* win, TabState* state) {
    if (WindowTab* tab = NewMissingSessionTab(win, state)) {
        AddTabToWindow(win, tab);
    }
}

// while the session's tabs are being created: adding a tab selects it, which
// must not load a lazy one
static bool gRestoringSession = false;

// orig's LoadArgs::lazyLoad: with LazyLoading the tab is created without its
// document, which loads on the first switch to the tab
static void RestoreLazyTabOnStartup(MainWindow* win, TabState* state) {
    AddTabToWindow(win, NewSessionTab(win, state));
}

// orig's LoadModelIntoTab: "if (gSettings->lazyLoading && !tab->ctrl &&
// tab->loadState == LoadState::None) ReloadDocument(win, false)". ng: the
// state the tab was saved with (page, zoom, sidebar) is applied here, where
// orig passes it through LoadArgs::tabState.
void LoadLazyTabIfNeeded(WindowTab* tab) {
    if (gRestoringSession || !gSettings->lazyLoading || !tab || tab->ctrl || tab->IsNonDocumentTab()) {
        return;
    }
    if (tab->loadState != WindowTab::LoadState::None || len(tab->filePath) == 0) {
        return;
    }
    MainWindow* win = tab->win;
    if (!IsMainWindowValidAndNotClosing(win) || !IsCurrentTab(win, tab)) {
        return;
    }
    logf("LoadLazyTabIfNeeded: '%s'\n", tab->filePath);
    StartLazyTabLoad(tab);
}

// The UI-thread half of a lazy tab's load. engine is what the background load
// created; without one (browser view, password, failure) the regular load
// runs, quickly now that the drive is awake.
void FinishLazyTabLoad(WindowTab* tab, EngineBase* engine) {
    MainWindow* win = tab->win;
    // a file that has gone away keeps its tab, marked as failed, as orig's
    if (!engine && !DocumentPathExists(tab->filePath)) {
        logf("LoadLazyTabIfNeeded: file not found '%s'\n", tab->filePath);
        FileHistoryDemote(tab->filePath, true);
        tab->loadState = WindowTab::LoadState::Error;
        TabsUIOnTabsChanged(win);
        return;
    }
    // RememberOpenedFiles = false frees the borrowed TabState on save.
    // The file still has to load; there is just no remembered view.
    TabState* state = tab->tabState;
    if (engine) {
        // a copy: loading replaces tab->filePath, then uses the path again
        LoadDocumentWithEngine(win, str::DupTemp(tab->filePath), engine);
    } else {
        ReloadDocument(win, false);
    }
    WindowTab* curr = win->CurrentTab();
    if (curr && curr->ctrl) {
        if (state) {
            SetTabState(curr, state);
        }
        // the borrowed state is applied; gInitialSessionData that owns it can go
        // away on the next save, as orig's LoadDocumentFinish does
        curr->tabState = nullptr;
        tab->tabState = nullptr;
    } else if (curr == tab) {
        tab->loadState = WindowTab::LoadState::Error;
    }
}

// The placeholder path below is orig's, for a file that has gone away.
static void RestoreTabOnStartup(MainWindow* win, TabState* state) {
    logf("RestoreTabOnStartup: state->filePath: '%s'\n", state->filePath);
    // a lazy tab touches its file only when selected: checking every file here
    // woke each sleeping drive of the session
    if (gSettings->lazyLoading && SettingsUseTabs()) {
        RestoreLazyTabOnStartup(win, state);
        return;
    }
    if (!DocumentPathExists(state->filePath)) {
        RestoreMissingTabOnStartup(win, state);
        return;
    }
    // a failed load already shows its error tab (or asks for a password)
    if (!LoadDocument(win, state->filePath, LoadPrefs::DontSave)) {
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
// startWinOut: the window for the foreground and the command line, one on the
// launch desktop; null to use the first window.
bool RestoreSession(MainWindow* firstWin, MainWindow** startWinOut) {
    *startWinOut = nullptr;
    if (!SettingsRestoreSession() || !gInitialSessionData || len(*gInitialSessionData) == 0) {
        return false;
    }
    int nRestored = 0;
    int nWindows = 0;
#if OS_WIN
    // each window goes back to the virtual desktop it was on (#6271)
    ISumatraVirtualDesktopManager* vdm = CreateVirtualDesktopManager();
    // before any window moves: focus changes can switch the desktop
    Str startDesktopId = str::Dup(GetCurrentDesktopIdTemp(vdm));
    MainWindow* winOnCurrentDesktop = nullptr;
#endif
    // a settings save rebuilds gInitialSessionData, freeing its SessionData
    int sessionGen = gInitialSessionDataGen;
    // closing a window saves the settings, so empty ones are closed after the loop
    Vec<MainWindow*> emptyWindows;
    for (int i = 0; i < len(*gInitialSessionData); i++) {
        if (sessionGen != gInitialSessionDataGen) {
            logf("RestoreSession: session re-saved while restoring, stopping\n");
            break;
        }
        SessionData* data = (*gInitialSessionData)[i];
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
        StripGroupRestore groupsRestore;
        for (TabState* state : *data->tabStates) {
            if (len(state->filePath) == 0) {
                logf("RestoreSession: skipping a TabState with an empty filePath\n");
                continue;
            }
            int nTabsBefore = win->TabCount();
            RestoreTabOnStartup(win, state);
            if (win->TabCount() > nTabsBefore) {
                StripGroupRestoreTab(win, win->GetTab(win->TabCount() - 1), state, groupsRestore);
            }
            nInWindow++;
        }
        gRestoringSession = false;
        if (nInWindow == 0) {
            // an empty window is one the user never sees; only the first one
            // (which already existed) stays
            if (nWindows > 0) {
                VecAppend(emptyWindows, win);
            }
            continue;
        }
        SelectRestoredTab(win, data->tabIndex);
        StripGroupRestoreDone(win, groupsRestore);
        if (gSettings->lazyLoading) {
            // trigger loading of the document
            LoadLazyTabIfNeeded(win->CurrentTab());
        }
        if (data->windowState == WIN_STATE_FULLSCREEN) {
            EnterFullScreen(win);
        }
        nRestored += nInWindow;
        nWindows++;
#if OS_WIN
        // COM calls to the shell dispatch incoming sent messages: a DDE
        // [CmdExit] can close win and re-save (free) the session data
        TempStr desktopId = str::DupTemp(data->virtualDesktopId);
        MoveWindowToVirtualDesktop(vdm, MainWindowHwnd(win), desktopId);
        bool onCurrentDesktop = IsWindowOnCurrentDesktop(vdm, MainWindowHwnd(win));
        if (!IsMainWindowValid(win) || sessionGen != gInitialSessionDataGen) {
            logf("RestoreSession: window closed or session re-saved while restoring, stopping\n");
            break;
        }
        if (onCurrentDesktop && !winOnCurrentDesktop) {
            winOnCurrentDesktop = win;
        }
#endif
    }
    for (MainWindow* win : emptyWindows) {
        if (IsMainWindowValid(win)) {
            CloseWindow(win, false);
        }
    }
#if OS_WIN
    // orig: all windows went to other desktops: keep the user on the launch
    // desktop with an empty window instead of following the last one
    if (nWindows > 0 && !winOnCurrentDesktop) {
        winOnCurrentDesktop = CreateAndShowMainWindow(nullptr);
        if (winOnCurrentDesktop) {
            MoveWindowToVirtualDesktop(vdm, MainWindowHwnd(winOnCurrentDesktop), startDesktopId);
        }
    }
    // the first window on the launch desktop is what happens without this
    if (winOnCurrentDesktop && winOnCurrentDesktop != firstWin && IsMainWindowValid(winOnCurrentDesktop)) {
        AppShellActivateWindow(winOnCurrentDesktop);
        *startWinOut = winOnCurrentDesktop;
    }
    str::Free(startDesktopId);
    if (vdm) {
        vdm->Release();
    }
#endif
    logf("RestoreSession: restored %d tabs in %d windows\n", nRestored, nWindows);
    return nRestored > 0;
}
