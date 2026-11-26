/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "gui/UIModels.h"

#include "Settings.h"
#include "AppSettings.h"
#include "DisplayMode.h"
#include "DocController.h"
#include "EngineBase.h"
#include "DisplayModel.h"
#include "ProgressUpdateUI.h"
#include "SumatraPDF.h"
#include "MainWindow.h"
#include "AIChatTypes.h"
#include "WindowTab.h"
#include "AppHelpersCommon.h"
#include "PagePosition.h"
#include "StripGroups.h"
#if OS_WIN
#include "VirtualDesktop.h"
#endif
#if defined(SUMATRA_NG)
#include "gui/Sidebar.h"
#endif
#include "SessionStateCommon.h"

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

TabState* NewTabStateFromTab(WindowTab* tab) {
    if (!tab || !tab->ctrl || len(tab->filePath) == 0) {
        return nullptr;
    }

    FileState* fs = NewFileState(tab->filePath);
    tab->ctrl->GetDisplayState(fs);
    fs->showToc = tab->showToc;
#if defined(SUMATRA_NG)
    Str sidebarView = SidebarContentToStr(tab->sidebarContent);
#else
    Str sidebarView = SidebarViewToStr(tab->sidebarView);
#endif
    str::ReplaceWithCopy(&fs->sidebarView, sidebarView);
    *fs->tocState = tab->tocState;

    TabState* state = NewTabState(fs);
    DeleteFileState(fs);
    return state;
}

static SessionData* CloneSessionData(const SessionData* src) {
    SessionData* dst = NewSessionData();
    dst->tabIndex = src->tabIndex;
    dst->windowState = src->windowState;
    dst->windowPos = src->windowPos;
    dst->sidebarDx = src->sidebarDx;
    str::ReplaceWithCopy(&dst->virtualDesktopId, src->virtualDesktopId);
    for (TabState* ts : *src->tabStates) {
        VecAppend(*dst->tabStates, CloneTabState(ts));
    }
    return dst;
}

struct SavedLazyTab {
    WindowTab* tab;
    int windowIndex;
    int tabIndex;
};

// Lazy tabs borrow from the startup snapshot. Rebind using the same rows
// that were serialized, even when the same file is open in several windows.
static void SyncInitialSessionData(const Vec<SavedLazyTab>& lazyTabs) {
    if (!gInitialSessionData) {
        return;
    }
    for (MainWindow* win : gWindows) {
        for (WindowTab* tab : win->Tabs()) {
            if (!tab->ctrl) {
                tab->tabState = nullptr;
            }
        }
    }
    FreeSessionDataVec(gInitialSessionData);
    gInitialSessionDataGen++;
    for (SessionData* sd : *gSettings->sessionData) {
        VecAppend(*gInitialSessionData, CloneSessionData(sd));
    }
    for (const SavedLazyTab& lazy : lazyTabs) {
        if (!IsWindowTabValid(lazy.tab) || !VecIsValidIndex(*gInitialSessionData, lazy.windowIndex)) {
            continue;
        }
        SessionData* session = (*gInitialSessionData)[lazy.windowIndex];
        if (!VecIsValidIndex(*session->tabStates, lazy.tabIndex)) {
            continue;
        }
        TabState* state = (*session->tabStates)[lazy.tabIndex];
        if (str::EqI(state->filePath, lazy.tab->filePath)) {
            lazy.tab->tabState = state;
        }
    }
}

#if OS_WIN
static HWND SessionWindowHwnd(MainWindow* win) {
#if defined(SUMATRA_NG)
    return MainWindowHwnd(win);
#else
    return win->hwndFrame;
#endif
}
#endif

static void RememberSessionState(Vec<SavedLazyTab>& lazyTabs) {
#if defined(SUMATRA_NG)
    // The last window has already saved the session.
    if (len(gWindows) == 0) {
        return;
    }
#endif
    if (!SettingsRememberOpenedFiles()) {
        FreeSessionDataVec(gSettings->sessionData);
        return;
    }

#if OS_WIN
    // which virtual desktop each window is on, so restoring puts it back (#6271).
    // Asked first: the shell's COM calls can pump messages, and a window closed
    // (deleted) or a nested SaveSettings() in there must not happen while we
    // walk gWindows and the session state below.
    Vec<HWND> desktopHwnds;
    for (auto* win : gWindows) {
        VecAppend(desktopHwnds, SessionWindowHwnd(win));
    }
    StrVec desktopIds;
    ISumatraVirtualDesktopManager* vdm = CreateVirtualDesktopManager();
    for (HWND hwnd : desktopHwnds) {
        desktopIds.Append(GetWindowDesktopIdTemp(vdm, hwnd));
    }
    if (vdm) {
        vdm->Release();
    }
#endif

    Vec<SessionData*>* sessionState = gSettings->sessionData;
    FreeSessionDataVec(sessionState);
    for (auto* win : gWindows) {
        if (win->isQuickLook) {
            continue;
        }
        SessionData* windowState = NewSessionData();
        for (WindowTab* tab : win->Tabs()) {
            if (len(tab->filePath) == 0 || tab->IsHexTab()) {
                // home page or hex view tab
                continue;
            }
            TabState* ts = nullptr;
            if (tab->ctrl) {
                ts = NewTabStateFromTab(tab);
            } else {
                // The tab's state distinguishes the same file opened in different windows.
                TabState* src = tab->tabState ? tab->tabState : FindSessionTabState(tab->filePath);
                if (src) {
                    ts = CloneTabState(src);
                }
            }
            if (ts) {
                if (!tab->ctrl && tab->tabState) {
                    VecAppend(lazyTabs, SavedLazyTab{tab, len(*sessionState), len(*windowState->tabStates)});
                }
                StripGroupSaveTab(win, tab, ts);
                VecAppend(*windowState->tabStates, ts);
            }
        }
        if (len(*windowState->tabStates) == 0) {
            FreeSessionData(windowState);
            continue;
        }
#if OS_WIN
        int desktopIdx = VecFind(desktopHwnds, SessionWindowHwnd(win));
        if (desktopIdx >= 0) {
            str::ReplaceWithCopy(&windowState->virtualDesktopId, desktopIds.At(desktopIdx));
        }
#endif
        // 1-based index among document tabs only (home / about tab is omitted
        // from TabStates above). Using the UI tab index would mis-restore when
        // the home tab was closed at save time but recreated on the next start.
        int docOrdinal = 0;
        int selectedDocOrdinal = 1;
        WindowTab* cur = win->CurrentTab();
        for (WindowTab* tab : win->Tabs()) {
            if (tab->IsAboutTab() || tab->IsHexTab() || len(tab->filePath) == 0) {
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

void SaveWindowSessionState() {
    // update display states for all tabs
    // we snapshot the list because SaveSettings() can be called re-entrantly
    // (e.g. from LoadDocumentFinish while other documents are still loading/closing)
    for (MainWindow* win : gWindows) {
        Vec<WindowTab*> tabs = win->Tabs();
        for (WindowTab* tab : tabs) {
            UpdateTabFileDisplayStateForTab(tab);
        }
    }
    Vec<SavedLazyTab> lazyTabs;
    RememberSessionState(lazyTabs);
    SyncInitialSessionData(lazyTabs);
    SyncTabGroups();
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
#if defined(SUMATRA_NG)
    tab->sidebarContent = SidebarContentFromStr(state->sidebarView, SidebarContent::Bookmarks);
#else
    tab->sidebarView = SidebarViewFromStr(state->sidebarView, SidebarView::Bookmarks);
#endif
    SetTocVisibility(win, state->showToc);

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
#if defined(SUMATRA_NG)
        if (dm->pendingRelayout) {
            dm->pendingScroll = scrollState;
            dm->hasPendingScroll = true;
            return;
        }
#endif
        dm->SetScrollState(scrollState);
    } else {
        ctrl->GoToPage(pageNo, true);
    }
}
