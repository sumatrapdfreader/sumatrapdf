/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// ng: the window half of orig's NavFilesInFolder.cpp. Orig's
// NavFilesInFolderWnd is a resizable top-level window built out of virtual
// controls and docked beside the main window. On wasm the picker is a column
// at the right edge of the frame, like the AI chat panel, and "which window
// is active" is a flag: gNav.hasKeyboard.
// The functions keep orig's names (they are its methods there) and its
// comments; the listing itself is the model half in NavFilesInFolder.cpp.

#include "gui/GpuiBridge.h"
#include "VirtKeys.h"
#include "base/File.h"
#include "base/UITask.h"
#if OS_WIN
#include "base/Win.h"
#endif

#include "gui/Dpi.h"
#include "gui/UIModels.h"

#include "Settings.h"
#include "AppSettings.h"
#include "DisplayMode.h"
#include "DocController.h"
#include "EngineBase.h"
#include "base/GuessFileType.h"
#include "EngineAll.h"
#include "FileHistory.h"
#include "Commands.h"
#include "Translations.h"
#include "Theme.h"
#include "SumatraPDF.h"
#include "MainWindow.h"
#include "WindowTab.h"
#include "Tabs.h"
#include "SvgIcons.h"
#include "TipMarkup.h"
#include "FilterHighlightDraw.h"
#include "AIChatPanel.h"
#include "NavFilesInFolder.h"
#include "gui/AppShell.h"
#include "SumatraDialogs.h"
#include "gui/DialogWidgets.h"
#include "gui/ToolWindow.h"
#include "gui/NavFilesUI.h"

#include "SumatraLog.h"

enum NavBtn {
    NavBtnBack,
    NavBtnForward,
    NavBtnUp,
    NavBtnHome,
    NavBtnCount
};

constexpr int kNavHistoryMax = 100;

// Clear: show ".." alone until the listing arrives (new folder).
// Keep: leave the current listing on screen while it is re-read (F5,
// activation); the result replaces it only if something changed
enum class NavListReset {
    Clear,
    Keep
};

// how in-place editing of the path ends: Enter navigates, Esc restores
enum class NavPathEditEnd {
    Commit,
    Cancel
};

// which part of the picker a key was typed into
enum class NavFocus {
    List,
    Filter,
    PathEdit
};

// logical (pre-DPI) sizes for placement / sizing of the nav window. ng: the
// in-frame panel uses kNavDockMaxWidthDx and kNavMinClientDx only
constexpr int kNavDockMinFreeDx = 320;  // free strip beside main must be wider than this to dock
constexpr int kNavDockMaxWidthDx = 480; // docked outer width = min(this, free strip)
constexpr int kNavMinClientDx = 200;    // floor after subtracting window chrome
constexpr int kNavMinClientDy = 200;
constexpr int kNavFallbackMinDy = 480; // centered (non-docked) client size
constexpr int kNavFallbackMinDx = 480;
constexpr int kNavFallbackMaxDx = 720;
constexpr int kNavFallbackMainDxMargin = 256; // main client dx minus this -> preferred width
constexpr int kNavFallbackMainDyMargin = 72;
constexpr int kNavFallbackYOffset = 42; // top offset when centered over main

// orig's sizes: the app font (9 pt) and a list row 4 taller than its text,
// the hints 2 smaller, 24 for a button, 23 for an edit
constexpr float kNavRowDy = 19;
constexpr float kNavFontSize = 12;
constexpr float kNavHelpFontSize = 10;
constexpr float kNavBtnSize = 24;
constexpr float kNavEditDy = 23;
constexpr float kNavIconSize = 16;
constexpr float kNavListBorderDx = 1;
// orig's VirtListBox padding
constexpr float kNavListPadDy = 4;
// gpui's small input draws its text at 14
constexpr float kNavInputFontPx = 14;
// orig's key caps (VirtRichText)
constexpr float kNavCapPadX = 7;
constexpr float kNavCapPadY = 1;
constexpr float kNavCapRadius = 5;
// rows built above and below the visible ones
constexpr int kNavOverscanRows = 2;
// the path label's slot in tipBounds, after the buttons
constexpr int kNavTipDirLabel = NavBtnCount;

struct NavFilesView {
    static void OnRowClick(NavFilesView* self, gp::Ctx* cx, const gp::ClickEvent* ev, int64_t idx);
    static void OnListDown(NavFilesView* self, gp::Ctx* cx, const gp::MouseDownEvent* ev);
    static void OnScroll(NavFilesView* self, gp::Ctx* cx, const gp::ScrollEvent* ev);
    static void OnNavButton(NavFilesView* self, gp::Ctx* cx, const gp::ClickEvent* ev, int64_t btn);
    static void OnTipHover(NavFilesView* self, gp::Ctx* cx, const gp::HoverEvent* ev, int64_t idx);
    static void OnDirLabelClick(NavFilesView* self, gp::Ctx* cx, const gp::ClickEvent* ev);
    static void OnDirEdit(NavFilesView* self, gp::Ctx* cx, const gp::InputEvent* ev);
    static void OnFilter(NavFilesView* self, gp::Ctx* cx, const gp::InputEvent* ev);
    static void OnCaptureKey(NavFilesView* self, gp::Ctx* cx, const gp::KeyEvent* ev);
    static void OnClose(NavFilesView* self, gp::Ctx* cx, const gp::ClickEvent* ev);
    static void OnSplitterDrag(NavFilesView* self, gp::Ctx* cx, const gp::DragMoveEvent* ev);
};

// ng: orig's NavFilesInFolderWnd. One picker at a time, as orig has (it keeps
// a single gNavFilesWnd).
struct NavFilesUI {
    gp::Entity<NavFilesView> view;
    MainWindow* win = nullptr;
    bool visible = false;
    // orig's "this window is the foreground one": the list takes the keys
    bool hasKeyboard = false;
    int dx = 0; // set by dragging the splitter; 0 for the default width

    gp::InputState* dirEdit = nullptr; // shown instead of the label while editing the path
    bool editingPath = false;
    bool focusPathPending = false;
    gp::InputState* filterEdit = nullptr;
    StrVec filterWords;
    // the list's focus, in a window of its own
    gp::FocusHandle listFocus;

    Vec<NavFileEntry> all;     // owned; the whole listing
    Vec<NavFileEntry> entries; // shown: the entries of `all` that pass the filter
    int sel = -1;              // index into entries
    float scrollY = 0;
    gp::Bounds panelBounds{};
    gp::Bounds listBounds{};
    gp::Bounds tipBounds[NavBtnCount + 1]{};

    Str currDir; // owned; empty in the home view
    int scanGen = 0;
    bool scanInFlight = false;
    Str pendingSelectPath; // owned; file to select when a scan finishes
    int pendingSelectIdx = -1;
    bool skipHistory = false;
};

static NavFilesUI gNav;
// orig's window: where a second window can be placed and styled (Windows) the
// picker is one; null means it is a panel in gNav.win's frame
static ToolWindow* gNavTw = nullptr;
// orig's picker has no owner and lingers when its main window closes; the
// next file it is asked to open has nowhere to go and closes it. Set while
// the window it was opened from is gone (it is drawn for another one then)
static bool gNavOwnerGone = false;
// dirs visited ("" for home), kept for the whole session so a re-opened
// window can still go Back to where the previous one was
static StrVec gNavHistory;
static int gNavHistIdx = -1;

// ng: the picker outlives a file being opened, so the window it sits in can be
// gone by the time a scan or a save prompt comes back
static bool NavIsOpen() {
    return gNav.visible && IsMainWindowValidAndNotClosing(gNav.win);
}

// ng: orig's picker is a window of its own, so it is never part of a
// presentation. The panel steps aside for one (like the sidebar) and stays in
// fullscreen (like the sidebar and the AI chat panel)
static bool NavIsShownIn(MainWindow* win) {
    return gNav.visible && !gNavTw && gNav.win == win && !win->InPresentation();
}

// the gpui window the picker's elements are in; null while its own is being made
static gp::Window* NavHostGpui() {
    if (gNavTw) {
        return ToolWindowGpui(gNavTw);
    }
    return gNav.win ? gNav.win->gpuiWin : nullptr;
}

static void NavInvalidate() {
    if (NavIsOpen()) {
        AppShellInvalidate(gNav.win);
    }
}

// ng: gp::Notify() wakes this view, not the shell that renders it
static void NavNotify(gp::Ctx* cx) {
    gp::Notify(cx);
    NavInvalidate();
}

static void EnsureNavInputs() {
    if (gNav.filterEdit || !gNav.win || !gNav.win->gpuiWin) {
        return;
    }
    gp::App* app = gNav.win->gpuiWin->app;
    gNav.filterEdit = new gp::InputState();
    gNav.filterEdit->focus = gp::FocusHandleNew(app);
    gNav.dirEdit = new gp::InputState();
    gNav.dirEdit->focus = gp::FocusHandleNew(app);
    gNav.listFocus = gp::FocusHandleNew(app);
}

static bool IsNavInputFocused(gp::InputState* s) {
    gp::Window* gw = NavHostGpui();
    if (!s || !NavIsOpen() || !gw) {
        return false;
    }
    return gp::FocusHandleIsFocused(gw, s->focus);
}

// orig's SetFocusTo(listBox): the list is drawn, not focused, so the keyboard
// goes to the frame and the shell routes the keys here while hasKeyboard is set
static void NavFocusList() {
    if (!NavIsOpen()) {
        return;
    }
    gNav.hasKeyboard = true;
    if (!gNavTw) {
        AppShellFocusFrame(gNav.win);
        return;
    }
    // orig's SetForegroundWindow + SetFocusTo(listBox)
    gp::Window* gw = NavHostGpui();
    if (gw) {
        if (gw->input) {
            gp::InputBlur(gw->input, gw->app, gw);
        }
        gp::FocusHandleFocus(gw, gNav.listFocus);
    }
    ToolWindowActivate(gNavTw);
    ToolWindowInvalidate(gNavTw);
}

// orig's SetForegroundWindow(mainWin->hwndFrame): the document takes the keys
// and the picker stays open
static void NavGiveKeyboardToDoc(MainWindow* win) {
    gNav.hasKeyboard = false;
    if (!IsMainWindowValidAndNotClosing(win)) {
        return;
    }
    if (gNavTw) {
        AppShellActivateWindow(win);
    }
    AppShellFocusFrame(win);
}

void CloseNavFilesInFolder() {
    if (!gNav.visible) {
        return;
    }
    if (gNavTw) {
        // orig's gHwndToActivateOnNavClose: closing the foreground window
        // must not hand the activation to another program
        if (ToolWindowIsActive(gNavTw) && IsMainWindowValidAndNotClosing(gNav.win)) {
            AppShellActivateWindow(gNav.win);
        }
        ToolWindowClose(gNavTw);
        gNavTw = nullptr;
    }
    gNav.visible = false;
    gNavOwnerGone = false;
    gNav.hasKeyboard = false;
    gNav.editingPath = false;
    gNav.scanGen++; // in-flight scans must not apply to a closed picker
    gNav.scanInFlight = false;
    VecReset(gNav.entries);
    FreeNavEntries(gNav.all);
    gNav.sel = -1;
    gNav.scrollY = 0;
    gNav.filterWords.Reset();
    if (gNav.filterEdit) {
        gp::InputSetValue(gNav.filterEdit, gp::Str{});
    }
    str::Free(gNav.currDir);
    gNav.currDir = {};
    str::Free(gNav.pendingSelectPath);
    gNav.pendingSelectPath = {};
    gNav.pendingSelectIdx = -1;
    MainWindow* win = gNav.win;
    gNav.win = nullptr;
    if (IsMainWindowValidAndNotClosing(win)) {
        AppShellFocusFrame(win);
    }
}

bool IsNavFilesInFolderVisible() {
    return gNav.visible;
}

// ng: orig's picker is a window of its own and outlives the main one; so does
// this where it is a window (NavToolOnOwnerClosed). As a panel it goes with
// the window it sits in (the shell's tick calls this)
void NavFilesReapClosedWindow() {
    if (!gNav.visible || IsMainWindowValidAndNotClosing(gNav.win)) {
        return;
    }
    gNav.win = nullptr;
    CloseNavFilesInFolder();
}

static bool IsHome() {
    return len(gNav.currDir) == 0;
}

// --- the list -----------------------------------------------------------------

static float NavListViewDy() {
    if (gNav.listBounds.h > 0) {
        return gNav.listBounds.h;
    }
    // not laid out yet: the canvas height less the rows around the list
    float dy = gNav.win ? (float)gNav.win->canvasRc.dy - 130 : 0;
    if (gNavTw && gNav.win) {
        int dpi = std::max(AppShellWindowDpi(gNav.win), 96);
        dy = (float)(ToolWindowRect(gNavTw).dy * 96 / dpi) - 150;
    }
    return std::max(dy, kNavRowDy);
}

static int NavVisibleRows() {
    return std::max((int)(NavListViewDy() / kNavRowDy), 1);
}

static void NavScrollTo(float y) {
    float maxY = (float)len(gNav.entries) * kNavRowDy - NavListViewDy();
    gNav.scrollY = std::max(0.f, std::min(y, maxY));
}

// select idx and scroll so it is visible (centered when possible)
static void SelectAndEnsureVisible(int idx) {
    int n = len(gNav.entries);
    if (idx < 0 || n <= 0) {
        return;
    }
    if (idx >= n) {
        idx = n - 1;
    }
    gNav.sel = idx;

    int visible = NavVisibleRows();
    int top = limitValue(idx - (visible / 2), 0, std::max(n - visible, 0));
    NavScrollTo((float)top * kNavRowDy);
}

// ng: what orig's VirtListBox does for the arrow keys: scroll only as far as
// it takes to show the selection
static void NavMoveSel(int delta) {
    int n = len(gNav.entries);
    if (n == 0) {
        return;
    }
    gNav.sel = limitValue(gNav.sel + delta, 0, n - 1);
    float top = (float)gNav.sel * kNavRowDy;
    float viewDy = NavListViewDy();
    if (top < gNav.scrollY) {
        NavScrollTo(top);
    } else if (top + kNavRowDy > gNav.scrollY + viewDy) {
        NavScrollTo(top + kNavRowDy - viewDy);
    }
    NavInvalidate();
}

static bool NavListKey(int vk) {
    switch (vk) {
        case VK_UP:
            NavMoveSel(-1);
            return true;
        case VK_DOWN:
            NavMoveSel(1);
            return true;
        case VK_PRIOR:
            NavMoveSel(-NavVisibleRows());
            return true;
        case VK_NEXT:
            NavMoveSel(NavVisibleRows());
            return true;
        case VK_HOME:
            NavMoveSel(-len(gNav.entries));
            return true;
        case VK_END:
            NavMoveSel(len(gNav.entries));
            return true;
    }
    return false;
}

// rebuild the shown entries: all of them without a filter, else those whose
// name has every filter word (the command palette's matching), without ".."
static void FilterNavEntries() {
    VecReset(gNav.entries);
    bool filtering = len(gNav.filterWords) > 0;
    for (NavFileEntry& e : gNav.all) {
        if (filtering && str::Eq(e.name, StrL(".."))) {
            continue;
        }
        if (filtering && !FilterMatches(NavEntryBaseName(e), gNav.filterWords)) {
            continue;
        }
        VecAppend(gNav.entries, e);
    }
}

static void ClearNavModel() {
    VecReset(gNav.entries);
    FreeNavEntries(gNav.all);
}

// full path for entry e under currDir
static TempStr NavEntryPathTemp(NavFileEntry& e) {
    if (str::Eq(e.name, StrL(".."))) {
        return path::GetDirTemp(gNav.currDir);
    }
    if (e.path) {
        return e.path;
    }
    return path::JoinTemp(gNav.currDir, NavEntryBaseName(e));
}

// index of selectPath in the listing, or 0 if not found / empty
static int FindEntryIndex(Str selectPath) {
    if (len(selectPath) == 0) {
        return 0;
    }
    for (int i = 0; i < len(gNav.entries); i++) {
        TempStr path = NavEntryPathTemp(gNav.entries[i]);
        if (str::EqI(path, selectPath) || path::IsSame(path, selectPath)) {
            return i;
        }
    }
    // basename fallback (path form differences: long-path prefix, slash style, etc.)
    TempStr base = path::GetBaseNameTemp(selectPath);
    for (int i = 0; i < len(gNav.entries); i++) {
        if (str::EqI(NavEntryBaseName(gNav.entries[i]), base)) {
            return i;
        }
    }
    return 0;
}

// --- the background listing --------------------------------------------------

struct NavDirScanReq {
    int gen = 0;
    bool isRefresh = false; // re-read of the dir already shown
    Str dir;                // owned
    ~NavDirScanReq() { str::Free(dir); }
};

struct NavDirScanResult {
    int gen = 0;
    bool isRefresh = false;
    Str dir; // owned
    Vec<NavFileEntry> entries;
    ~NavDirScanResult() {
        str::Free(dir);
        FreeNavEntries(entries);
    }
};

static void FinishNavDirScan(NavDirScanResult* r) {
    AutoDelete del(r);
    if (!NavIsOpen() || r->gen != gNav.scanGen) {
        logf("NavDirScan: drop stale gen %d (current gen=%d)\n", r->gen, gNav.scanGen);
        return;
    }
    logf("NavDirScan: apply %d entries for %s\n", len(r->entries), r->dir);

    gNav.scanInFlight = false;
    // a re-read that found nothing new leaves the list alone: no repaint at all
    if (r->isRefresh && SameNavEntries(gNav.all, r->entries)) {
        return;
    }
    float scrollY = gNav.scrollY;
    VecReset(gNav.entries); // they point into `all`
    StealNavEntries(gNav.all, r->entries);
    FilterNavEntries();

    int selIdx = 0;
    if (len(gNav.pendingSelectPath) > 0) {
        selIdx = FindEntryIndex(gNav.pendingSelectPath);
    } else if (gNav.pendingSelectIdx >= 0) {
        selIdx = gNav.pendingSelectIdx;
    }
    int n = len(gNav.entries);
    if (n > 0 && r->isRefresh) {
        // the user is looking at this list: keep the viewport where it was
        gNav.sel = std::min(selIdx, n - 1);
        NavScrollTo(scrollY);
    } else if (n > 0) {
        SelectAndEnsureVisible(selIdx);
    } else {
        gNav.sel = -1;
        gNav.scrollY = 0;
    }
    NavInvalidate();
}

static void NavDirScanThread(NavDirScanReq* req) {
    AutoDelete delReq(req);
    // the home view enumerates a shell folder, which needs COM on this thread
#if OS_WIN
    bool comInited = false;
    if (len(req->dir) == 0) {
        comInited = SUCCEEDED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED));
    }
#endif
    auto* r = new NavDirScanResult;
    r->gen = req->gen;
    r->isRefresh = req->isRefresh;
    r->dir = str::Dup(req->dir);
    CollectNavEntriesForDir(req->dir, r->entries);
#if OS_WIN
    if (comInited) {
        CoUninitialize();
    }
#endif
    uitask::Post(MkFunc0<NavDirScanResult>(FinishNavDirScan, r), "FinishNavDirScan");
}

// --- filter --------------------------------------------------------------------

// full path of the selected entry, or empty. Owned copy: callers pass it back
// into SetDir(), which frees the entries the path would otherwise point into.
static TempStr SelectedPathTemp() {
    int idx = gNav.sel;
    if (idx < 0 || idx >= len(gNav.entries)) {
        return {};
    }
    NavFileEntry& e = gNav.entries[idx];
    if (str::Eq(e.name, StrL(".."))) {
        return {};
    }
    return str::DupTemp(NavEntryPathTemp(e));
}

// re-filter the current listing; a filter selects its first match
static void ApplyFilter() {
    TempStr sel = SelectedPathTemp();
    FilterNavEntries();
    if (len(gNav.entries) == 0) {
        gNav.sel = -1;
        gNav.scrollY = 0;
    } else {
        int idx = len(gNav.filterWords) > 0 ? 0 : FindEntryIndex(sel);
        SelectAndEnsureVisible(idx);
    }
    NavInvalidate();
}

static void OnFilterChanged() {
    TempStr s = str::DupTemp(FromGpui(gp::InputValue(gNav.filterEdit)));
    gNav.filterWords.Reset();
    SplitFilterToWords(s, gNav.filterWords);
    ApplyFilter();
}

static void ClearFilter() {
    if (!gNav.filterEdit || len(gNav.filterWords) == 0) {
        return;
    }
    gNav.filterWords.Reset();
    gp::InputSetValue(gNav.filterEdit, gp::Str{});
    // ng: orig's EN_CHANGE re-applies the (now empty) filter
    ApplyFilter();
}

static void FocusFilter() {
    EnsureNavInputs();
    if (!gNav.filterEdit || !NavIsOpen()) {
        return;
    }
    gp::Window* gw = NavHostGpui();
    if (!gw) {
        return;
    }
    gNav.hasKeyboard = true;
    gp::InputFocus(gNav.filterEdit, gw->app, gw);
}

// --- navigation ---------------------------------------------------------------

static void SetDir(Str dir, Str selectPath, int selectIdx = -1, NavListReset reset = NavListReset::Clear) {
    // the filter is per folder, like Explorer's search box
    if (!str::EqI(dir, gNav.currDir)) {
        ClearFilter();
    }
    str::ReplaceWithCopy(&gNav.currDir, dir);
    gNav.scanGen++;
    gNav.scanInFlight = true;
    str::ReplaceWithCopy(&gNav.pendingSelectPath, selectPath);
    gNav.pendingSelectIdx = selectIdx;

    if (reset == NavListReset::Clear) {
        // show ".." immediately so the window is usable while the listing runs
        ClearNavModel();
        if (!IsHome()) {
            AppendNavParentEntry(gNav.all);
        }
        FilterNavEntries();
        gNav.sel = -1;
        gNav.scrollY = 0;
        if (len(gNav.entries) > 0) {
            SelectAndEnsureVisible(0);
        }
    }

    auto* req = new NavDirScanReq;
    req->gen = gNav.scanGen;
    req->isRefresh = reset == NavListReset::Keep;
    req->dir = str::Dup(gNav.currDir);
    logf("NavDirScan: start %s gen=%d\n", gNav.currDir, gNav.scanGen);
    RunAsync(MkFunc0<NavDirScanReq>(NavDirScanThread, req), StrL("NavDirScan"));
    NavInvalidate();
}

// show dir and record it in the session history, dropping the forward entries
static void Navigate(Str dir, Str selectPath = {}) {
    bool same = gNavHistIdx >= 0 && str::EqI(gNavHistory[gNavHistIdx], dir);
    if (!same) {
        while (len(gNavHistory) > gNavHistIdx + 1) {
            gNavHistory.RemoveAt(len(gNavHistory) - 1);
        }
        if (len(gNavHistory) >= kNavHistoryMax) {
            gNavHistory.RemoveAt(0);
        }
        gNavHistory.Append(dir);
        gNavHistIdx = len(gNavHistory) - 1;
    }
    SetDir(dir, selectPath);
}

// re-read the directory, keeping the selection on the same file. The listing is
// a snapshot, so files renamed / added / removed after it was taken (by F2 in
// the main window, by another app, ...) would otherwise linger (issue #5878).
static void RefreshList() {
    TempStr sel = SelectedPathTemp();
    int selIdx = -1;
    // listing still in flight: keep the file we meant to select
    if (len(sel) == 0 && len(gNav.pendingSelectPath) > 0) {
        sel = str::DupTemp(gNav.pendingSelectPath);
    } else if (len(sel) == 0 && gNav.scanInFlight) {
        selIdx = gNav.pendingSelectIdx;
    }
    TempStr dir = str::DupTemp(gNav.currDir);
    SetDir(dir, sel, selIdx, NavListReset::Keep);
}

static void GoHome() {
    if (IsHome()) {
        return;
    }
    Navigate(Str{});
}

// from a root directory (C:\) Up goes to the home view
static void GoUp() {
    if (IsHome()) {
        return;
    }
    if (!NavDirHasParent(gNav.currDir)) {
        GoHome();
        return;
    }
    // select the directory we're coming from
    TempStr cameFrom = str::DupTemp(gNav.currDir);
    Navigate(path::GetDirTemp(gNav.currDir), cameFrom);
}

// cameFrom when it is a direct child of dir (so Back / Forward select it), else empty
static Str SelectIfChildOf(Str cameFrom, Str dir) {
    if (len(cameFrom) == 0 || len(dir) == 0) {
        return {};
    }
    if (!path::IsSame(path::GetDirTemp(cameFrom), dir)) {
        return {};
    }
    return cameFrom;
}

static void GoBack() {
    if (gNavHistIdx <= 0) {
        return;
    }
    TempStr cameFrom = str::DupTemp(gNav.currDir);
    gNavHistIdx--;
    TempStr dir = str::DupTemp(gNavHistory[gNavHistIdx]);
    SetDir(dir, SelectIfChildOf(cameFrom, dir));
}

static void GoForward() {
    if (gNavHistIdx + 1 >= len(gNavHistory)) {
        return;
    }
    TempStr cameFrom = str::DupTemp(gNav.currDir);
    gNavHistIdx++;
    TempStr dir = str::DupTemp(gNavHistory[gNavHistIdx]);
    SetDir(dir, SelectIfChildOf(cameFrom, dir));
}

// orig's UpdateNavButtons
static bool IsNavBtnEnabled(int btn) {
    switch (btn) {
        case NavBtnBack:
            return gNavHistIdx > 0;
        case NavBtnForward:
            return gNavHistIdx + 1 < len(gNavHistory);
    }
    return !IsHome();
}

// --- editing the path ----------------------------------------------------------

// edit the path in place: the edit covers the label until Enter / Esc
static void BeginEditPath() {
    EnsureNavInputs();
    if (!gNav.dirEdit || gNav.editingPath) {
        return;
    }
    gNav.editingPath = true;
    gNav.hasKeyboard = true;
    gp::InputSetValue(gNav.dirEdit, ToGpui(gNav.currDir));
    // ng: the input is only in the tree from the next frame on, which focuses it
    gNav.focusPathPending = true;
    NavInvalidate();
}

// Commit: a directory is navigated to, a file's directory with that file
// selected; anything else leaves the current dir (the label never changed)
static void EndEditPath(NavPathEditEnd how) {
    if (!gNav.editingPath) {
        return;
    }
    gNav.editingPath = false; // before moving the focus: that fires the blur
    gNav.focusPathPending = false;
    TempStr path = str::DupTemp(FromGpui(gp::InputValue(gNav.dirEdit)));
    NavFocusList();
    if (how == NavPathEditEnd::Cancel) {
        return;
    }
    // Explorer's "Copy as path" wraps the path in quotes
    str::TrimWSInPlace(path, str::TrimOpt::Both);
    if (len(path) >= 2 && path.s[0] == '"' && path.s[len(path) - 1] == '"') {
        path = Str(path.s + 1, len(path) - 2);
    }
    if (dir::Exists(path)) {
        Navigate(path);
    } else if (file::Exists(path)) {
        Navigate(path::GetDirTemp(path), path);
    }
}

// --- opening and deleting ------------------------------------------------------

struct NavOpenCtx {
    MainWindow* win = nullptr;
    Str path; // owned
    bool skipHistory = false;
    ~NavOpenCtx() { str::Free(path); }
};

static void NavMarkSkipHistory(MainWindow* win, bool skipHistory) {
    WindowTab* tab = win->CurrentTab();
    if (tab && skipHistory) {
        tab->skipHistory = true;
    }
}

static void NavOpenInCurrentTab(NavOpenCtx* c, bool ok) {
    AutoDelete del(c);
    MainWindow* win = c->win;
    if (!ok || !IsMainWindowValidAndNotClosing(win)) {
        return;
    }
    DismissNextFileScrollHint(win);
    // replace the document in the current tab; keep this window open
    LoadDocument(win, c->path, LoadPrefs::Save, LoadReuse::CurrentTab);
    NavMarkSkipHistory(win, c->skipHistory);
    // Hand keyboard control back to the document, the way Ctrl + Enter does
    // when it switches to a tab that already has the file (issue #5903).
    // The window stays open so browsing can continue; it just isn't focused.
    NavGiveKeyboardToDoc(win);
}

// inNewTab: Ctrl+Enter / Ctrl+double-click. Switches to the tab already showing
// the file, or opens it in a new tab, instead of replacing the current document.
static void ExecuteCurrentSelection(bool inNewTab = false) {
    int idx = gNav.sel;
    if (idx < 0 || idx >= len(gNav.entries)) {
        return;
    }
    NavFileEntry& e = gNav.entries[idx];
    if (str::Eq(e.name, StrL(".."))) {
        GoUp();
        return;
    }
    TempStr path = str::DupTemp(NavEntryPathTemp(e));
    if (e.isDir) {
        Navigate(path);
        return;
    }

    MainWindow* mainWin = gNav.win;
    if (gNavOwnerGone || !IsMainWindowValidAndNotClosing(mainWin)) {
        CloseNavFilesInFolder();
        return;
    }

    if (inNewTab) {
        WindowTab* existing = FindTabByFilePath(path);
        if (existing) {
            SelectTabInWindow(existing);
            AppShellActivateWindow(existing->win);
            NavGiveKeyboardToDoc(existing->win);
            return;
        }
        DismissNextFileScrollHint(mainWin);
        // no forceReuse: opens in a new tab, leaving the current document alone
        LoadDocument(mainWin, path);
        NavMarkSkipHistory(mainWin, gNav.skipHistory);
        return;
    }

    auto* c = new NavOpenCtx;
    c->win = mainWin;
    c->path = str::Dup(path);
    c->skipHistory = gNav.skipHistory;
    WindowTab* tab = mainWin->CurrentTab();
    if (!tab) {
        NavOpenInCurrentTab(c, true);
        return;
    }
    // ng: orig's MaybeSaveAnnotations blocks; here the answer comes back
    MaybeSaveAnnotations(tab, MkFunc1(NavOpenInCurrentTab, c));
}

struct NavDeleteCtx {
    Str path; // owned
    int idx = 0;
    ~NavDeleteCtx() { str::Free(path); }
};

static void NavDeleteFile(NavDeleteCtx* c, bool ok) {
    AutoDelete del(c);
    if (!ok) {
        return;
    }
    Str path = c->path;
    WindowTab* tab = FindTabByFilePath(path);
    if (tab) {
        CloseTab(tab, false);
    }
    DeleteFileFromDiskAndHistory(path);

    if (!NavIsOpen()) {
        return;
    }
    if (file::Exists(path)) {
        MessageBoxWarning(gNav.win, fmt(Tr("Couldn't delete %s").s, path));
    }
    // re-list; keep the selection where the deleted entry was
    if (IsHome()) {
        ResetQuickAccessCache();
    }
    TempStr dir = str::DupTemp(gNav.currDir);
    SetDir(dir, Str{}, c->idx);
    NavFocusList();
}

// Del on a file moves it to the recycle bin (issue #5877), without a
// confirmation prompt -- the recycle bin is the undo. Directories are left
// alone: recursively deleting a folder from a file picker is too easy to
// trigger by accident, and "Show in Folder" + Explorer covers it.
static void DeleteCurrentSelection() {
    if (!CanAccessDisk() || gPluginMode) {
        return;
    }
    int idx = gNav.sel;
    if (idx < 0 || idx >= len(gNav.entries)) {
        return;
    }
    NavFileEntry& e = gNav.entries[idx];
    if (e.isDir || str::Eq(e.name, StrL(".."))) {
        return;
    }
    // own the path: deleting re-fills the model, which frees the entry
    TempStr path = str::DupTemp(NavEntryPathTemp(e));
    if (!file::Exists(path)) {
        return;
    }

    // no confirmation prompt: the file goes to the recycle bin, so it's undoable

    auto* c = new NavDeleteCtx;
    c->path = str::Dup(path);
    c->idx = idx;
    // a document open in a tab keeps the file mapped, so the delete would fail;
    // close that tab first, like CmdDeleteFile does for the current document
    WindowTab* tab = FindTabByFilePath(path);
    if (!tab) {
        NavDeleteFile(c, true);
        return;
    }
    MaybeSaveAnnotations(tab, MkFunc1(NavDeleteFile, c));
}

// --- input --------------------------------------------------------------------

// orig's NavFilesInFolderWnd::OnKeyDown, plus what its VirtListBox and its
// closeOnEsc do with the keys it does not take
static bool NavOnKey(int vk, bool ctrl, bool alt, NavFocus focus) {
    if (focus == NavFocus::PathEdit) {
        if (vk == VK_RETURN) {
            EndEditPath(NavPathEditEnd::Commit);
            return true;
        }
        if (vk == VK_ESCAPE) {
            EndEditPath(NavPathEditEnd::Cancel);
            return true;
        }
        return false;
    }
    if (vk == VK_RETURN) {
        ExecuteCurrentSelection(ctrl);
        return true;
    }
    if (vk == 'F' && ctrl && !alt) {
        FocusFilter();
        gp::Window* gw = NavHostGpui();
        if (gNav.filterEdit && NavIsOpen() && gw) {
            gp::InputSelectAll(gNav.filterEdit, gw->app, gw);
        }
        return true;
    }
    bool editFocused = focus == NavFocus::Filter;
    if (editFocused && !alt) {
        // Up / Down / PgUp / PgDn move the list selection while typing;
        // Esc clears the filter; the edit keeps its other keys (Backspace, Del)
        switch (vk) {
            case VK_UP:
            case VK_DOWN:
            case VK_PRIOR:
            case VK_NEXT:
                return NavListKey(vk);
            case VK_ESCAPE:
                if (len(gNav.filterWords) > 0) {
                    ClearFilter();
                } else {
                    CloseNavFilesInFolder();
                }
                return true;
            case VK_F5:
                break;
            default:
                return false;
        }
    }
    // Alt + Up / Left / Right go up / back / forward, like Explorer.
    // Backspace goes up, like the classic Explorer / file dialogs
    if ((vk == VK_UP && alt) || vk == VK_BACK) {
        GoUp();
        return true;
    }
    if (vk == VK_LEFT && alt) {
        GoBack();
        return true;
    }
    if (vk == VK_RIGHT && alt) {
        GoForward();
        return true;
    }
    if (vk == VK_DELETE) {
        DeleteCurrentSelection();
        return true;
    }
    if (vk == VK_F5) {
        if (IsHome()) {
            ResetQuickAccessCache();
        }
        RefreshList();
        return true;
    }
    if (alt) {
        return false;
    }
    if (vk == VK_ESCAPE) {
        CloseNavFilesInFolder();
        return true;
    }
    return NavListKey(vk);
}

// a key that types a character (orig's OnListChar gets them as WM_CHAR)
static bool IsCharKey(int vk) {
    if (vk == VK_SPACE || (vk >= '0' && vk <= '9') || (vk >= 'A' && vk <= 'Z')) {
        return true;
    }
    if (vk >= VK_NUMPAD0 && vk <= VK_DIVIDE) {
        return true;
    }
    return vk >= VK_OEM_1 && vk <= VK_OEM_102;
}

// a key while the list has the keyboard, in the gpui window the picker is in
static NavKeyResult NavListOnKey(gp::Window* gw, int vk, bool ctrl, bool alt) {
    // a focused text field has the key; the picker's own two get theirs in
    // OnCaptureKey, before the field does
    if (gw && gw->input && gw->input->focused) {
        return NavKeyResult::NotHandled;
    }
    if (NavOnKey(vk, ctrl, alt, NavFocus::List)) {
        return NavKeyResult::Handled;
    }
    // ng: orig's list is in a window of its own, so the plain arrows it does
    // not use never turn the document's pages
    if ((vk == VK_LEFT || vk == VK_RIGHT) && !ctrl && !alt) {
        return NavKeyResult::Handled;
    }
    // typing while the list has focus goes into the search field
    if (ctrl || alt || !IsCharKey(vk)) {
        return NavKeyResult::NotHandled;
    }
    FocusFilter();
    return NavKeyResult::Typed;
}

NavKeyResult NavFilesOnKeyDown(MainWindow* win, int vk, bool ctrl, bool alt) {
    if (!NavIsOpen() || !NavIsShownIn(win) || !gNav.hasKeyboard) {
        return NavKeyResult::NotHandled;
    }
    return NavListOnKey(win->gpuiWin, vk, ctrl, alt);
}

void NavFilesView::OnCaptureKey(NavFilesView*, gp::Ctx* cx, const gp::KeyEvent* ev) {
    NavFocus focus = NavFocus::List;
    if (IsNavInputFocused(gNav.dirEdit)) {
        focus = NavFocus::PathEdit;
    } else if (IsNavInputFocused(gNav.filterEdit)) {
        focus = NavFocus::Filter;
    } else {
        return;
    }
    gNav.hasKeyboard = true;
    if (!NavOnKey(ev->vk, ev->ctrl, ev->alt, focus)) {
        return;
    }
    const_cast<gp::KeyEvent*>(ev)->propagate = false;
    NavNotify(cx);
}

// ng: orig's WM_ACTIVATE. A press inside the panel gives the picker the
// keyboard and re-reads the folder; one anywhere else in the frame hands the
// keyboard back to the document
void NavFilesOnMouseDown(MainWindow* win, float x, float y) {
    // in a window of its own the activation says who has the keyboard
    if (gNavTw || !NavIsOpen() || !NavIsShownIn(win)) {
        return;
    }
    bool inside = gNav.panelBounds.Contains(gp::Point{x, y});
    if (inside == gNav.hasKeyboard) {
        return;
    }
    gNav.hasKeyboard = inside;
    // first show already started a listing; don't kick off a second one
    if (inside && !gNav.scanInFlight) {
        RefreshList();
    }
}

void NavFilesView::OnRowClick(NavFilesView*, gp::Ctx* cx, const gp::ClickEvent* ev, int64_t idx) {
    if (idx < 0 || idx >= len(gNav.entries)) {
        return;
    }
    gNav.sel = (int)idx;
    if (ev->clickCount >= 2) {
        ExecuteCurrentSelection(ev->modifiers.control);
    }
    NavNotify(cx);
}

// a press on the list takes the focus out of the two edits, as orig's list does
void NavFilesView::OnListDown(NavFilesView*, gp::Ctx* cx, const gp::MouseDownEvent*) {
    NavFocusList();
    NavNotify(cx);
}

void NavFilesView::OnScroll(NavFilesView*, gp::Ctx* cx, const gp::ScrollEvent* ev) {
    gNav.scrollY = ev->offsetY;
    NavNotify(cx);
}

// orig's NavButtonClicked
void NavFilesView::OnNavButton(NavFilesView*, gp::Ctx* cx, const gp::ClickEvent*, int64_t btn) {
    switch (btn) {
        case NavBtnBack:
            GoBack();
            break;
        case NavBtnForward:
            GoForward();
            break;
        case NavBtnUp:
            GoUp();
            break;
        case NavBtnHome:
            GoHome();
            break;
    }
    NavFocusList();
    NavNotify(cx);
}

// the tooltips orig's CreateNavButtons and its path label set
static TempStr NavTipTemp(int idx) {
    switch (idx) {
        case NavBtnBack:
            return fmt("%s (Alt + Left)", Tr("Back"));
        case NavBtnForward:
            return fmt("%s (Alt + Right)", Tr("Forward"));
        case NavBtnUp:
            return fmt("%s (Alt + Up, Backspace)", Tr("Up"));
        case NavBtnHome:
            return str::DupTemp(Tr("Home"));
    }
    return str::DupTemp(Tr("Click to edit the path"));
}

void NavFilesView::OnTipHover(NavFilesView*, gp::Ctx* cx, const gp::HoverEvent* ev, int64_t idx) {
    if (!ev->hovered) {
        HoverTooltipHide(cx);
        return;
    }
    HoverTooltipShow(cx, NavTipTemp((int)idx), gNav.tipBounds[idx]);
}

void NavFilesView::OnDirLabelClick(NavFilesView*, gp::Ctx* cx, const gp::ClickEvent*) {
    HoverTooltipHide(cx);
    BeginEditPath();
    NavNotify(cx);
}

void NavFilesView::OnDirEdit(NavFilesView*, gp::Ctx* cx, const gp::InputEvent* ev) {
    // clicking away restores the label, like Esc. The focus is where the
    // click put it, so it is not moved to the list as EndEditPath would
    if (ev->kind != gp::InputEventKind::Blur || !gNav.editingPath || gNav.focusPathPending) {
        return;
    }
    gNav.editingPath = false;
    NavNotify(cx);
}

void NavFilesView::OnFilter(NavFilesView*, gp::Ctx* cx, const gp::InputEvent* ev) {
    if (ev->kind != gp::InputEventKind::Change) {
        return;
    }
    OnFilterChanged();
    NavNotify(cx);
}

void NavFilesView::OnClose(NavFilesView*, gp::Ctx* cx, const gp::ClickEvent*) {
    MainWindow* win = gNav.win;
    CloseNavFilesInFolder();
    gp::Notify(cx);
    if (IsMainWindowValidAndNotClosing(win)) {
        AppShellInvalidate(win);
    }
}

// what the panel may take without leaving the canvas less than kMinDocCanvasDx
static int NavMaxDx(MainWindow* win) {
    int sidebarDx = 0;
    if (win->uiState.tocVisible || win->uiState.favVisible) {
        sidebarDx = win->sidebarDx + kSplitterDx;
    }
    int avail = win->frameRc.dx - sidebarDx - AIChatPanelDx(win) - kMinDocCanvasDx - kSplitterDx;
    return std::max(avail, kNavMinClientDx);
}

// ng: orig's window is resizable; here its left edge is a splitter
void NavFilesView::OnSplitterDrag(NavFilesView*, gp::Ctx* cx, const gp::DragMoveEvent* ev) {
    if (!NavIsOpen()) {
        return;
    }
    MainWindow* win = gNav.win;
    int dx = win->frameRc.dx - (int)ev->event.x - kSplitterDx;
    dx = limitValue(dx, kNavMinClientDx, NavMaxDx(win));
    if (dx == gNav.dx) {
        return;
    }
    gNav.dx = dx;
    NavNotify(cx);
}

// the panel's width with its splitter, 0 when it is not in this window
int NavFilesPanelDx(MainWindow* win) {
    if (!win || !NavIsShownIn(win)) {
        return 0;
    }
    int dx = gNav.dx > 0 ? gNav.dx : kNavDockMaxWidthDx;
    dx = std::min(dx, NavMaxDx(win));
    return dx + kSplitterDx;
}

// --- a window of its own -------------------------------------------------------

static gp::El* NavContentEl(gp::Ctx* cx, bool ownWindow);

static Str NavToolTitle() {
    return Tr("Navigate Files in Folder");
}

static gp::El* NavToolBuild(MainWindow*, gp::Ctx* cx) {
    if (!gNav.visible || !gNavTw) {
        return nullptr;
    }
    return NavContentEl(cx, true);
}

// the keys the list takes: its window has no other key listener
static bool NavToolOnKey(MainWindow*, gp::Ctx* cx, const gp::KeyEvent* ev) {
    if (!NavIsOpen()) {
        return false;
    }
    NavKeyResult res = NavListOnKey(cx->win, ev->vk, ev->ctrl, ev->alt);
    NavInvalidate();
    // Typed: the filter box has the focus now and takes the character
    return res == NavKeyResult::Handled;
}

// the close box, Alt + F4, or the main window went away
static void NavToolOnClosed(MainWindow*) {
    gNavTw = nullptr;
    CloseNavFilesInFolder();
}

// the main window it was opened from closed and another one is left
static void NavToolOnOwnerClosed(MainWindow* newOwner) {
    gNav.win = newOwner;
    gNavOwnerGone = true;
}

// orig's OnActivate: re-read the folder, the list takes the keys
static void NavToolOnActivate(MainWindow*, bool active) {
    if (!NavIsOpen() || !gNavTw) {
        return;
    }
    gNav.hasKeyboard = active;
    // first show already started a listing; don't kick off a second one
    if (active && !gNav.scanInFlight) {
        RefreshList();
    }
    NavInvalidate();
}

// orig's NavDockedClientSize + PositionNavFilesWnd: beside the main window
// when the work area has a strip wide enough there (as tall as the main
// window), else centered over it. The window rectangle, in screen pixels
static Rect NavToolWindowRect(MainWindow* win, const ToolWindowDesc& desc) {
    int dpi = std::max(AppShellWindowDpi(win), 96);
    auto scale = [dpi](int v) { return MulDiv(v, dpi, 96); };
    Rect main = AppShellWindowScreenRect(win);
    Rect work = AppShellWorkArea(win);
    int freeLeft = std::max(main.x - work.x, 0);
    int freeRight = std::max((work.x + work.dx) - (main.x + main.dx), 0);
    Size chrome = ToolWindowOuterSize(desc, win, Size(0, 0));

    int minFree = scale(kNavDockMinFreeDx);
    int free = 0;
    bool placeLeft = false;
    bool docked = true;
    if (freeLeft > freeRight && freeLeft > minFree) {
        free = freeLeft;
        placeLeft = true;
    } else if (freeRight > minFree) {
        free = freeRight;
    } else if (freeLeft > minFree) {
        free = freeLeft;
        placeLeft = true;
    } else {
        docked = false;
    }

    int dx = 0;
    int dy = 0;
    if (docked) {
        // outer width fits the free strip but is capped at kNavDockMaxWidthDx
        int outerDx = std::min(free, scale(kNavDockMaxWidthDx));
        dx = std::max(outerDx - chrome.dx, scale(kNavMinClientDx));
        dy = std::max(main.dy - chrome.dy, scale(kNavMinClientDy));
    } else {
        gp::WinSize client = win->gpuiWin ? gp::WindowSize(win->gpuiWin) : gp::WinSize{};
        int rcDy = scale((int)client.dipH);
        int rcDx = scale((int)client.dipW);
        dy = std::max(rcDy - scale(kNavFallbackMainDyMargin), scale(kNavFallbackMinDy));
        dx = limitValue(rcDx - scale(kNavFallbackMainDxMargin), scale(kNavFallbackMinDx), scale(kNavFallbackMaxDx));
    }
    Rect r{0, 0, dx + chrome.dx, dy + chrome.dy};
    if (docked) {
        r.y = main.y;
        r.x = placeLeft ? main.x - r.dx : main.x + main.dx;
    } else {
        r.x = main.x + (main.dx / 2) - (r.dx / 2);
        r.y = main.y + scale(kNavFallbackYOffset);
    }
    return AppShellShiftToWorkArea(r, win, true);
}

// orig's NavFilesInFolderWnd::Create: a regular resizable top-level window
// (no owner, so Alt-Tab switches between it and the main window)
static void NavOpenToolWindow(MainWindow* win) {
    if (gNavTw || !ToolWindowsAvailable()) {
        return;
    }
    ToolWindowDesc desc;
    desc.name = "navfiles";
    desc.title = NavToolTitle;
    desc.frame = ToolWinFrame::Caption;
    desc.resize = ToolWinResize::Resizable;
    desc.owner = ToolWinOwner::TopLevel;
    desc.minClient = Size(kNavMinClientDx, kNavMinClientDy);
    desc.build = NavToolBuild;
    desc.onKey = NavToolOnKey;
    desc.onClosed = NavToolOnClosed;
    desc.onOwnerClosed = NavToolOnOwnerClosed;
    desc.onActivate = NavToolOnActivate;
    gNavTw = ToolWindowOpen(desc, win, NavToolWindowRect(win, desc));
}

// --- showing ------------------------------------------------------------------

// Start directory when no document is open (home page). The file-open dialog
// deliberately doesn't set an initial directory, letting the shell reopen the
// folder of the last file opened through it; the closest equivalent we can
// compute is the newest still-existing entry in our own file history.
static TempStr NavStartDirNoDocTemp() {
    for (int i = 0;; i++) {
        FileState* fs = FileHistoryGet(i);
        if (!fs) {
            break;
        }
        TempStr dir = path::GetDirTemp(fs->filePath);
        if (len(dir) > 0 && dir::Exists(dir)) {
            return dir;
        }
    }
#if OS_WIN
    TempStr docs = GetSpecialFolderTemp(CSIDL_PERSONAL);
#else
    const char* home = getenv("HOME");
    TempStr docs = str::DupTemp(Str((char*)(home ? home : "")));
#endif
    if (len(docs) > 0 && dir::Exists(docs)) {
        return docs;
    }
    return GetSelfExeDirTemp();
}

void ShowNavFilesInFolder(MainWindow* win, Str selectPath, bool skipHistory) {
    if (!win || !CanAccessDisk() || gPluginMode) {
        return;
    }
    // Prefer an explicit path (e.g. home-page thumbnail); else the current tab.
    TempStr filePath = str::DupTemp(selectPath);
    if (len(filePath) == 0) {
        WindowTab* tab = win->CurrentTab();
        if (tab && !tab->IsAboutTab()) {
            filePath = str::DupTemp(tab->filePath);
        }
    }

    NavFilesReapClosedWindow();
    if (gNav.visible) {
        gNavOwnerGone = false;
        // ng: one picker, as in orig; it moves to the window that asked for it
        if (gNav.win != win) {
            AppShellInvalidate(gNav.win);
            gNav.win = win;
            if (gNavTw) {
                ToolWindowSetOwner(gNavTw, win);
            }
        }
        // re-sync to the target folder (and re-read: the file may have been
        // renamed since, #5878)
        if (len(filePath) > 0) {
            Navigate(path::GetDirTemp(filePath), filePath);
        } else {
            // on the home page with no selection keep whatever dir is open
            RefreshList();
        }
        gNav.skipHistory = skipHistory;
        NavFocusList();
        return;
    }

    gNav.win = win;
    gNav.visible = true;
    gNav.skipHistory = skipHistory;
    EnsureNavInputs();

    TempStr dir = len(filePath) > 0 ? path::GetDirTemp(filePath) : Str{};
    if (len(dir) == 0 || !dir::Exists(dir)) {
        dir = NavStartDirNoDocTemp();
    }
    logf("ShowNavFilesInFolder: '%s'\n", dir);
    Navigate(dir, filePath);
    NavOpenToolWindow(win);
    NavFocusList();
}

// --- the panel ----------------------------------------------------------------

static gp::El* NavSvgIcon(gp::Ctx* cx, const char* svg, Color color) {
    return gpc::Icon::Empty(cx)->Data(ToGpui(Str(svg)))->Size(kNavIconSize)->Color(ToGpui(color))->IntoEl();
}

// orig's CreateNavButtons: Back / Forward / Up / Home
static gp::El* NavButtonEl(gp::Ctx* cx, int btn) {
    static const char* icons[NavBtnCount] = {gIconNavigateBack, gIconNavigateForward, gIconArrowUp, gIconHome};
    const gp::Theme& th = gp::ThemeNow(cx->app);
    bool enabled = IsNavBtnEnabled(btn);
    gp::El* el = gp::Div(cx->a)
                     ->W(kNavBtnSize)
                     ->H(kNavBtnSize)
                     ->Radius(4)
                     ->Shrink0()
                     ->ItemsCenter()
                     ->JustifyCenter()
                     ->AriaLabel(GpuiDup(cx->a, NavTipTemp(btn)))
                     ->BoundsOut(&gNav.tipBounds[btn])
                     ->OnHover(gp::ListenTo(gNav.view, &NavFilesView::OnTipHover, (intptr_t)btn));
    if (enabled) {
        el->HoverBg(th.tokens.muted)
            ->PathClick(GpuiDup(cx->a, fmt("nav-btn-%d", btn)))
            ->OnClick(gp::ListenTo(gNav.view, &NavFilesView::OnNavButton, (intptr_t)btn));
    }
    Color col = enabled ? ThemeWindowTextColor() : ThemeWindowTextDisabledColor();
    el->Child(NavSvgIcon(cx, icons[btn], col));
    return el;
}

// ng: gpui's inputs have one text size (14). In its own window the picker
// sets the window's rem so that the two edits come out in orig's 12, and
// every other font size is given in that rem
static float gNavFontScale = 1;

static float NavFontPx(float px) {
    return px * gNavFontScale;
}

// orig's DrawListBoxItem
static gp::El* NavRowEl(gp::Ctx* cx, int idx, Color colBg, Color colText) {
    NavFileEntry& e = gNav.entries[idx];
    gp::El* row = gp::Div(cx->a)
                      ->FlexRow()
                      ->W(gp::kFill)
                      ->H(kNavRowDy)
                      ->Shrink0()
                      ->ItemsCenter()
                      ->Gap(8)
                      ->PadX(4)
                      ->PathClick(GpuiDup(cx->a, fmt("nav-row-%d", idx)))
                      ->OnClick(gp::ListenTo(gNav.view, &NavFilesView::OnRowClick, (intptr_t)idx));
    if (idx == gNav.sel) {
        row->Bg(ToGpui(AccentColor(colBg, 30)));
    }
    // directories in bold, without the trailing "\"; filter matches highlighted
    Str name = NavEntryBaseName(e);
    int boldOffset = e.isDir ? 0 : -1;
    row->Child(
        FilterHighlightText(cx, name, gNav.filterWords, ToGpui(colText), NavFontPx(kNavFontSize), boldOffset, len(name))
            ->Flex1()
            ->MinW(0)
            ->ClipX());
    // human readable file size on the right (files only; include 0-byte files)
    if (!e.isDir) {
        row->Child(gp::TextEl(cx->a, GpuiDup(cx->a, str::FormatSizeShortTemp(e.size)))
                       ->Font(NavFontPx(kNavFontSize))
                       ->Fg(ToGpui(AccentColor(colText, 80)))
                       ->Shrink0());
    }
    return row;
}

// one wrapping line of key-cap hints, like the command palette help row;
// translators keep the key names in English
static gp::El* NavHintsEl(gp::Ctx* cx, Color colText) {
    const gp::Theme& th = gp::ThemeNow(cx->app);
    TempStr hints = fmt("(Kbd/%s) %s (Kbd/%s) %s (Kbd/%s) %s", Tr("Enter"), Tr("open in current tab"),
                        Tr("Ctrl + Enter"), Tr("open in new tab"), Tr("Del"), Tr("delete file"));
    Vec<TipSpan> spans;
    TipSpansParse(spans, hints);
    gp::El* row =
        gp::Div(cx->a)->FlexRow()->FlexWrap()->W(gp::kFill)->Shrink0()->ItemsCenter()->JustifyCenter()->Gap(3);
    // the hints are secondary information, so they get a smaller font
    float capDy = ceilf(kNavHelpFontSize * 1.33f) + 2 * kNavCapPadY;
    for (const TipSpan& sp : spans) {
        gp::El* el = gp::TextEl(cx->a, GpuiDup(cx->a, sp.text))->Font(NavFontPx(kNavHelpFontSize))->Fg(ToGpui(colText));
        if (sp.kind == TipSpanKind::Kbd) {
            // orig's key cap
            el = gp::Div(cx->a)
                     ->FlexRow()
                     ->ItemsCenter()
                     ->H(capDy)
                     ->PadX(kNavCapPadX)
                     ->Radius(kNavCapRadius)
                     ->Border(1, th.border)
                     ->Bg(th.tokens.muted)
                     ->Shrink0()
                     ->Child(el);
        }
        row->Child(el);
    }
    TipSpansFree(spans);
    return row;
}

// the presentation took the panel off screen: the document gets the keys, the
// way orig's frame is the foreground window while it presents
static void NavYieldToPresentation(MainWindow* win) {
    bool editFocused = IsNavInputFocused(gNav.filterEdit) || IsNavInputFocused(gNav.dirEdit);
    if (!gNav.hasKeyboard && !gNav.editingPath && !editFocused) {
        return;
    }
    gNav.editingPath = false;
    gNav.focusPathPending = false;
    NavGiveKeyboardToDoc(win);
}

// the picker's content: it fills a window of its own, or the panel in the frame
static gp::El* NavContentEl(gp::Ctx* cx, bool ownWindow) {
    if (!gNav.view.IsValid()) {
        gNav.view = gp::EntityNewState<NavFilesView>(cx->app);
    }
    EnsureNavInputs();
    const gp::Theme& th = gp::ThemeNow(cx->app);
    Color colBg = ThemeWindowControlBackgroundColor();
    Color colTxt = ThemeWindowTextColor();
    gNavFontScale = 1;
    if (ownWindow) {
        gNavFontScale = kNavInputFontPx / kNavFontSize;
        gp::WindowSetRemSize(cx->win, 16.f / gNavFontScale);
    }

    gp::El* col = gp::Div(cx->a)
                      ->FlexCol()
                      ->Flex1()
                      ->MinW(0)
                      ->H(gp::kFill)
                      ->PadX(8)
                      ->PadY(4)
                      ->Gap(4)
                      ->Bg(ToGpui(colBg))
                      ->CaptureKeyDown(gp::ListenTo(gNav.view, &NavFilesView::OnCaptureKey));

    if (ownWindow) {
        col->W(gp::kFill);
    } else {
        // ng: orig's window caption and its close box
        gp::El* caption = gp::Div(cx->a)->FlexRow()->W(gp::kFill)->Shrink0()->ItemsCenter()->Gap(4);
        caption->Child(gp::TextEl(cx->a, ToGpui(Tr("Navigate Files in Folder")))
                           ->Font(NavFontPx(kNavFontSize))
                           ->Bold()
                           ->Fg(ToGpui(colTxt))
                           ->Flex1()
                           ->MinW(0)
                           ->Truncate());
        caption->Child(gpc::Button::New(cx, GStrL("nav-close"))
                           ->Icon(gp::IconName::Close)
                           ->WithSize(gp::UiSize::XSmall)
                           ->Compact()
                           ->Ghost()
                           ->Tooltip(ToGpui(Tr("Close")))
                           ->OnClick(gp::ListenTo(gNav.view, &NavFilesView::OnClose))
                           ->IntoEl());
        col->Child(caption);
    }

    // top row: Back / Forward / Up / Home buttons, then the current dir
    gp::El* top = gp::Div(cx->a)->FlexRow()->W(gp::kFill)->Shrink0()->ItemsCenter();
    for (int i = 0; i < NavBtnCount; i++) {
        top->Child(NavButtonEl(cx, i));
    }
    if (gNav.editingPath) {
        gNav.dirEdit->onChange = gp::ListenTo(gNav.view, &NavFilesView::OnDirEdit);
        top->Child(
            gp::Div(cx->a)->Flex1()->MinW(0)->PadX(4)->Child(gpc::Input::New(cx, GStrL("nav-dir-edit"), gNav.dirEdit)
                                                                 ->WithSize(gp::UiSize::Small)
                                                                 ->W(gp::kFill)
                                                                 ->IntoEl()
                                                                 ->H(kNavEditDy)));
        if (gNav.focusPathPending) {
            gp::InputFocus(gNav.dirEdit, cx);
            gp::InputSelectAll(gNav.dirEdit, cx);
            gNav.focusPathPending = false;
        }
    } else {
        Str dir = IsHome() ? Tr("Home") : gNav.currDir;
        top->Child(gp::Div(cx->a)
                       ->FlexRow()
                       ->Flex1()
                       ->MinW(0)
                       ->H(kNavBtnSize)
                       ->ItemsCenter()
                       ->PadX(4)
                       ->Cursor(gp::CursorKind::IBeam)
                       ->BoundsOut(&gNav.tipBounds[kNavTipDirLabel])
                       ->OnHover(gp::ListenTo(gNav.view, &NavFilesView::OnTipHover, (intptr_t)kNavTipDirLabel))
                       ->PathClick(GStrL("nav-dir-label"))
                       ->OnClick(gp::ListenTo(gNav.view, &NavFilesView::OnDirLabelClick))
                       ->Child(gp::TextEl(cx->a, GpuiDup(cx->a, dir))
                                   ->Font(NavFontPx(kNavFontSize))
                                   ->Fg(ToGpui(colTxt))
                                   ->W(gp::kFill)
                                   ->Truncate()));
    }
    col->Child(top);

    // second row: filters the list below as you type
    gp::InputSetPlaceholder(gNav.filterEdit, ToGpui(Tr("Search")));
    gNav.filterEdit->onChange = gp::ListenTo(gNav.view, &NavFilesView::OnFilter);
    col->Child(gp::Div(cx->a)->W(gp::kFill)->Shrink0()->Child(gpc::Input::New(cx, GStrL("nav-filter"), gNav.filterEdit)
                                                                  ->WithSize(gp::UiSize::Small)
                                                                  ->W(gp::kFill)
                                                                  ->IntoEl()
                                                                  ->H(kNavEditDy)));

    // ng: only the rows in view are built, between two spacers that stand in
    // for the rest, so a folder of tens of thousands of files stays cheap
    int n = len(gNav.entries);
    int first = std::max((int)(gNav.scrollY / kNavRowDy) - kNavOverscanRows, 0);
    int last = std::min(first + NavVisibleRows() + 1 + 2 * kNavOverscanRows, n);
    gp::El* rows = gp::Div(cx->a)->FlexCol()->W(gp::kFill);
    if (first > 0) {
        rows->Child(gp::Div(cx->a)->W(gp::kFill)->H((float)first * kNavRowDy)->Shrink0());
    }
    for (int i = first; i < last; i++) {
        rows->Child(NavRowEl(cx, i, colBg, colTxt));
    }
    if (last < n) {
        rows->Child(gp::Div(cx->a)->W(gp::kFill)->H((float)(n - last) * kNavRowDy)->Shrink0());
    }
    gp::El* list = gp::Div(cx->a)
                       ->Id(GStrL("nav-files-list"))
                       ->FlexCol()
                       ->Flex1()
                       ->W(gp::kFill)
                       ->MinH(0)
                       ->ScrollY(gNav.scrollY)
                       ->ScrollFromPath()
                       ->OnScroll(gp::ListenTo(gNav.view, &NavFilesView::OnScroll))
                       ->OnMouseDown(gp::ListenTo(gNav.view, &NavFilesView::OnListDown))
                       ->BoundsOut(&gNav.listBounds)
                       ->Child(rows);
    if (ownWindow) {
        // the list has the keyboard unless one of the two fields does
        list->TrackFocus(gNav.listFocus)->TabStop(false);
        if (!gp::WindowFocused(cx->win).IsValid()) {
            gp::FocusHandleFocus(cx->win, gNav.listFocus);
        }
    }
    // orig's list has its rows 4 inside its edge. ng: and a focus rectangle
    // there; this is a border
    col->Child(gp::Div(cx->a)
                   ->FlexCol()
                   ->Flex1()
                   ->W(gp::kFill)
                   ->MinH(0)
                   ->Border(kNavListBorderDx, th.border)
                   ->PadY(kNavListPadDy - kNavListBorderDx)
                   ->Child(list));

    col->Child(NavHintsEl(cx, colTxt));
    return col;
}

gp::El* NavFilesUIBuild(MainWindow* win, gp::Ctx* cx) {
    if (!gNav.visible || gNavTw || gNav.win != win) {
        return nullptr;
    }
    if (win->InPresentation()) {
        NavYieldToPresentation(win);
        return nullptr;
    }
    const gp::Theme& th = gp::ThemeNow(cx->app);
    gp::El* col = NavContentEl(cx, false);

    gp::El* panel = gp::Div(cx->a)
                        ->FlexRow()
                        ->H(gp::kFill)
                        ->W((float)NavFilesPanelDx(win))
                        ->Shrink0()
                        ->BoundsOut(&gNav.panelBounds);
    panel->Child(gp::Div(cx->a)
                     ->W((float)kSplitterDx)
                     ->H(gp::kFill)
                     ->Shrink0()
                     ->Bg(th.border)
                     ->Cursor(gp::CursorKind::ColResize)
                     ->PathClick(GStrL("nav-splitter"))
                     ->OnDrag(GStrL("sumatra-nav-splitter"))
                     ->OnDragMove(gp::ListenTo(gNav.view, &NavFilesView::OnSplitterDrag)));
    panel->Child(col);
    return panel;
}

// --- -dbg-control ---------------------------------------------------------------

// State and actions used by the -dbg-control regression test.
TempStr NavFilesInFolderStateTemp(Str action, int idx, int* exitCodeOut) {
    if (exitCodeOut) {
        *exitCodeOut = 2;
    }
    if (!NavIsOpen()) {
        return str::DupTemp(StrL("NOTREADY no-window"));
    }
    EnsureNavInputs();
    if (str::Eq(action, StrL("select"))) {
        gNav.sel = idx;
    } else if (str::Eq(action, StrL("delete-refresh"))) {
        DeleteCurrentSelection();
        RefreshList();
    } else if (str::Eq(action, StrL("up"))) {
        GoUp();
    } else if (str::Eq(action, StrL("back"))) {
        GoBack();
    } else if (str::Eq(action, StrL("forward"))) {
        GoForward();
    } else if (str::Eq(action, StrL("home"))) {
        GoHome();
    } else if (str::Eq(action, StrL("execute"))) {
        ExecuteCurrentSelection();
    } else if (str::Eq(action, StrL("refresh"))) {
        RefreshList();
    } else if (str::Eq(action, StrL("path-label-rect"))) {
        // where a test must click to start editing the path (client coords)
        gp::Bounds r = gNav.tipBounds[kNavTipDirLabel];
        if (exitCodeOut) {
            *exitCodeOut = 0;
        }
        return fmt("OK %d %d %d %d", (int)r.x, (int)r.y, (int)r.w, (int)r.h);
    } else if (str::Eq(action, StrL("edit-path")) || str::TrimPrefix(action, StrL("path-"))) {
        // path editing: edit-path, path-text:<text>, path-commit, path-cancel,
        // path-state (report only)
        if (str::Eq(action, StrL("edit-path"))) {
            BeginEditPath();
        } else if (str::Eq(action, StrL("commit"))) {
            EndEditPath(NavPathEditEnd::Commit);
        } else if (str::Eq(action, StrL("cancel"))) {
            EndEditPath(NavPathEditEnd::Cancel);
        } else if (str::TrimPrefix(action, StrL("text:"))) {
            gp::InputSetValue(gNav.dirEdit, ToGpui(action));
        }
        if (exitCodeOut) {
            *exitCodeOut = 0;
        }
        TempStr text = str::DupTemp(FromGpui(gp::InputValue(gNav.dirEdit)));
        return fmt("OK editing=%d text=\"%s\"", (int)gNav.editingPath, text);
    } else if (str::TrimPrefix(action, StrL("filter:"))) {
        gp::InputSetValue(gNav.filterEdit, ToGpui(action));
        OnFilterChanged();
    } else if (str::Eq(action, StrL("key"))) {
        // ng: not one of orig's: the key `idx` as the shell hands it over
        if (gNavTw) {
            NavListOnKey(NavHostGpui(), idx, false, false);
        } else {
            NavFilesOnKeyDown(gNav.win, idx, false, false);
        }
        if (!NavIsOpen()) {
            if (exitCodeOut) {
                *exitCodeOut = 0;
            }
            return str::DupTemp(StrL("OK closed"));
        }
    } else if (str::Eq(action, StrL("close"))) {
        MainWindow* win = gNav.win;
        CloseNavFilesInFolder();
        AppShellInvalidate(win);
        if (exitCodeOut) {
            *exitCodeOut = 0;
        }
        return str::DupTemp(StrL("OK closed"));
    }
    NavInvalidate();

    int sel = gNav.sel;
    Str name;
    if (sel >= 0 && sel < len(gNav.entries)) {
        name = gNav.entries[sel].name;
    }
    if (exitCodeOut) {
        *exitCodeOut = 0;
    }
    int canBack = IsNavBtnEnabled(NavBtnBack);
    int canFwd = IsNavBtnEnabled(NavBtnForward);
    // ng: keyboard=, dx= and filter= are not in orig's line
    TempStr filter = str::DupTemp(FromGpui(gp::InputValue(gNav.filterEdit)));
    int filterFocused = IsNavInputFocused(gNav.filterEdit);
    return fmt(
        "OK scan=%d sel=%d items=%d back=%d fwd=%d dir=\"%s\" name=\"%s\" keyboard=%d dx=%d filter=\"%s\" "
        "filterFocused=%d",
        (int)gNav.scanInFlight, sel, len(gNav.entries), canBack, canFwd, gNav.currDir, name, (int)gNav.hasKeyboard,
        NavFilesPanelDx(gNav.win), filter, filterFocused);
}
