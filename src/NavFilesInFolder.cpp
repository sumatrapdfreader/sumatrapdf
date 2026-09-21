/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/DirScan.h"
#include "gui/Dpi.h"
#include "base/File.h"
#include "base/GuessFileType.h"
#include "base/Timer.h"
#include "base/UITask.h"
#include "base/Win.h"

#include "gui/UIModels.h"
#include "gui/Layout.h"
#include "gui/win/WinGui.h"
#include "gui/PlatformFont.h"
#include "gui/Gfx.h"
#include "gui/GuiColors.h"
#include "gui/VirtCtrl.h"

#include "Settings.h"
#include "DocController.h"
#include "EngineBase.h"
#include "EngineAll.h"
#include "FileHistory.h"
#include "SumatraConfig.h"
#include "AppSettings.h"
#include "MainWindow.h"
#include "WindowTab.h"
#include "SumatraPDF.h"
#include "Theme.h"
#include "Translations.h"
#include "DarkMode.h"
#include "SvgIcons.h"
#include "FilterUtil.h"
#include "FilterHighlightDraw.h"
#include "NavFilesInFolder.h"

// A modeless directory browser listing sub-directories and files SumatraPDF
// can open (judged by extension). Enter / double-click replaces the document
// in the current tab or descends into a directory; Ctrl + Enter / Ctrl +
// double-click switches to the tab already showing the file or opens it in a
// new tab; ".." goes one directory up. Back / Forward walk the folders visited
// in this session, Up goes to the parent, Home lists the drives and Explorer's
// Quick access. The window stays open so you can open several files in
// succession; Esc or the close button dismisses it.

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

// logical (pre-DPI) sizes for placement / sizing of the nav window
constexpr int kNavDockMinFreeDx = 320;  // free strip beside main must be wider than this to dock
constexpr int kNavDockMaxWidthDx = 480; // docked outer width = min(this, free strip)
constexpr int kNavMinClientDx = 200;    // floor after subtracting window chrome
constexpr int kNavMinClientDy = 200;
constexpr int kNavFallbackMinDy = 480; // centered (non-docked) client size
constexpr int kNavFallbackMinDx = 480;
constexpr int kNavFallbackMaxDx = 720;
constexpr int kNavFallbackMainDxMargin = 256; // main client dx minus this → preferred width
constexpr int kNavFallbackMainDyMargin = 72;
constexpr int kNavFallbackYOffset = 42; // top offset when centered over main

struct NavFileEntry {
    Str name; // owned; leaf display name (dirs end with "\\"); ".." for parent
    Str path; // owned; full path (empty for "..")
    bool isDir = false;
    i64 size = 0; // file size; 0 for dirs / unknown
};

static void FreeNavEntry(NavFileEntry& e) {
    str::Free(e.name);
    str::Free(e.path);
}

struct ListBoxModelNav : ListBoxModel {
    Vec<NavFileEntry> all;     // owned; the whole listing
    Vec<NavFileEntry> entries; // shown: the entries of `all` that pass the filter

    ~ListBoxModelNav() override {
        for (NavFileEntry& e : all) {
            FreeNavEntry(e);
        }
    }
    int ItemsCount() override { return len(entries); }
    Str Item(int i) override { return entries[i].name; }
};

struct NavFilesInFolderWnd : WindowBase {
    ~NavFilesInFolderWnd() override;

    MainWindow* win = nullptr;
    // the label, the buttons, the list and the hints are virtual controls
    VirtText* dirLabel = nullptr;
    Edit* dirEdit = nullptr; // HWND; shown over dirLabel while editing the path
    bool editingPath = false;
    VirtIconButton* navBtns[NavBtnCount]{};
    Edit* filterEdit = nullptr; // HWND
    StrVec filterWords;
    Vec<u8> highlighted; // scratch for DrawMaybeHighlightedText
    VirtListBox* listBox = nullptr;
    Str currDir; // owned; empty in the home view
    int scanGen = 0;
    bool scanInFlight = false;
    Str pendingSelectPath; // owned; file to select when a scan finishes
    int pendingSelectIdx = -1;
    bool skipHistory = false;

    void OnKeyDown(KeyEvent* ev);
    void OnActivate(WindowBase::ActivateEvent* ev);
    void OnFocus(WindowBase::FocusEvent* ev);

    bool Create(MainWindow* win, Str filePath);
    void CreateNavButtons(Color fg, Color bg);
    bool IsHome() const;
    void SetDir(Str dir, Str selectPath, int selectIdx = -1, NavListReset reset = NavListReset::Clear);
    void Navigate(Str dir, Str selectPath = {});
    TempStr SelectedPathTemp();
    void RefreshList();
    void ExecuteCurrentSelection(bool inNewTab = false);
    void DeleteCurrentSelection();
    void OnListDoubleClick();
    void GoUp();
    void GoBack();
    void GoForward();
    void GoHome();
    void OnFilterChanged();
    void ApplyFilter();
    void ClearFilter();
    void OnListChar(VirtCharEvent* ev);
    void OnDirLabelClick(VirtMouseEvent* ev);
    void OnDirEditKillFocus();
    void BeginEditPath();
    void EndEditPath(NavPathEditEnd how);
    Rect PathEditRect();
    void DrawListBoxItem(VirtListBox::DrawItemEvent* ev);
    void UpdateDirLabel();
    void UpdateNavButtons();
};

static NavFilesInFolderWnd* gNavFilesWnd = nullptr;
static HWND gHwndToActivateOnNavClose = nullptr;
// dirs visited ("" for home), kept for the whole session so a re-opened
// window can still go Back to where the previous one was
static StrVec gNavHistory;
static int gNavHistIdx = -1;

NavFilesInFolderWnd::~NavFilesInFolderWnd() {
    scanGen++; // in-flight scans must not apply to a destroyed window
    str::Free(currDir);
    str::Free(pendingSelectPath);
}

static void SafeDeleteNavFilesWnd() {
    if (!gNavFilesWnd) {
        return;
    }
    auto* tmp = gNavFilesWnd;
    gNavFilesWnd = nullptr;
    delete tmp;
    if (gHwndToActivateOnNavClose) {
        HWND fg = GetForegroundWindow();
        if (!fg || fg == gHwndToActivateOnNavClose) {
            SetActiveWindow(gHwndToActivateOnNavClose);
        }
        gHwndToActivateOnNavClose = nullptr;
    }
}

static void ScheduleDeleteNavFilesWnd() {
    if (!gNavFilesWnd) {
        return;
    }
    auto fn = MkFunc0Void(SafeDeleteNavFilesWnd);
    uitask::Post(fn, "SafeDeleteNavFilesWnd");
}

// Alt-Tab / activation finishes setting focus after WM_ACTIVATE, so set list
// focus after the message queue drains (and only while this window is still active).
static void FocusNavListBox() {
    NavFilesInFolderWnd* wnd = gNavFilesWnd;
    if (!wnd || !wnd->hwnd || !IsWindow(wnd->hwnd) || !wnd->listBox) {
        return;
    }
    if (GetForegroundWindow() != wnd->hwnd) {
        return;
    }
    wnd->SetFocusTo(wnd->listBox);
}

static void ScheduleFocusNavListBox() {
    if (!gNavFilesWnd) {
        return;
    }
    auto fn = MkFunc0Void(FocusNavListBox);
    uitask::Post(fn, "FocusNavListBox");
}

// skip GuessFileTypeFromName()'s IsDirectory() probe: name is relative to the
// listed dir, and callers already skipped directories.
static bool CanOpenFile(Str path) {
    FileType kind = GuessFileTypeFromName(path, true);
    return IsSupportedFileType(kind, true) || DocIsSupportedFileType(kind);
}

// dirs first, then files, each sorted naturally by name.
// qsort: the old insertion sort was O(n^2) and froze on huge folders.
static int CmpNavEntry(const NavFileEntry* a, const NavFileEntry* b) {
    if (a->isDir != b->isDir) {
        return a->isDir ? -1 : 1;
    }
    return str::CmpNatural(a->name, b->name);
}

static void SortNavEntries(Vec<NavFileEntry>& entries, int firstIdx) {
    int n = len(entries) - firstIdx;
    if (n <= 1) {
        return;
    }
    auto cmp = (int (*)(const void*, const void*))CmpNavEntry;
    qsort(entries.els + firstIdx, (size_t)n, sizeof(NavFileEntry), cmp);
}

static void FreeNavEntries(Vec<NavFileEntry>& entries) {
    for (NavFileEntry& e : entries) {
        FreeNavEntry(e);
    }
    VecReset(entries);
}

static void StealNavEntries(Vec<NavFileEntry>& dst, Vec<NavFileEntry>& src) {
    FreeNavEntries(dst);
    dst.els = src.els;
    dst.len = src.len;
    dst.cap = src.cap;
    src.els = nullptr;
    src.len = 0;
    src.cap = 0;
}

static Str NavEntryBaseName(const NavFileEntry& e);

// rebuild the shown entries: all of them without a filter, else those whose
// name has every filter word (the command palette's matching), without ".."
static void FilterNavEntries(ListBoxModelNav* m, const StrVec& words) {
    VecReset(m->entries);
    bool filtering = len(words) > 0;
    for (NavFileEntry& e : m->all) {
        if (filtering && str::Eq(e.name, StrL(".."))) {
            continue;
        }
        if (filtering && !FilterMatches(NavEntryBaseName(e), words)) {
            continue;
        }
        VecAppend(m->entries, e);
    }
}

static void ClearNavModel(ListBoxModelNav* m) {
    FreeNavEntries(m->all);
    VecReset(m->entries);
}

static bool SameNavEntries(const Vec<NavFileEntry>& a, const Vec<NavFileEntry>& b) {
    if (len(a) != len(b)) {
        return false;
    }
    for (int i = 0; i < len(a); i++) {
        const NavFileEntry& ea = a[i];
        const NavFileEntry& eb = b[i];
        if (ea.isDir != eb.isDir || ea.size != eb.size || !str::Eq(ea.name, eb.name)) {
            return false;
        }
    }
    return true;
}

static bool DirHasParent(Str dir) {
    if (len(dir) == 0) {
        return false; // home view
    }
    TempStr parent = path::GetDirTemp(dir);
    return !path::IsSame(parent, dir);
}

static void AppendParentEntry(Vec<NavFileEntry>& entries) {
    NavFileEntry e;
    e.name = str::Dup(StrL(".."));
    e.isDir = true;
    VecAppend(entries, e);
}

// leaf name for display: find-data names are usually basenames, but some network
// providers put a relative or full path in cFileName — always show the leaf only.
static TempStr NavLeafNameTemp(DirIterEntry* de) {
    TempStr leaf = path::GetBaseNameTemp(de->name);
    if (len(leaf) == 0) {
        leaf = path::GetBaseNameTemp(de->filePath);
    }
    return leaf;
}

// Explorer's Quick access, fetched once per process because the shell resolves
// every entry. F5 in the home view drops the cache.
static Mutex gQuickAccessMutex;
static bool gQuickAccessCached = false;
static StrVec gQuickAccessDirs;
static StrVec gQuickAccessFiles;

static void ResetQuickAccessCache() {
    gQuickAccessMutex.Lock();
    gQuickAccessCached = false;
    gQuickAccessMutex.Unlock();
}

static void GetQuickAccessCached(StrVec& dirsOut, StrVec& filesOut) {
    gQuickAccessMutex.Lock();
    if (!gQuickAccessCached) {
        gQuickAccessDirs.Reset();
        gQuickAccessFiles.Reset();
        auto t = TimeGet();
        ListShellQuickAccess(gQuickAccessDirs, gQuickAccessFiles);
        logf("NavDirScan: quick access %d dirs, %d files in %.1fms\n", len(gQuickAccessDirs), len(gQuickAccessFiles),
             TimeSinceInMs(t));
        gQuickAccessCached = true;
    }
    dirsOut = gQuickAccessDirs;
    filesOut = gQuickAccessFiles;
    gQuickAccessMutex.Unlock();
}

// home entries show the full path: Quick access has many same-named folders
static void AppendHomeDirEntry(Vec<NavFileEntry>& out, Str path) {
    NavFileEntry e;
    e.isDir = true;
    e.path = str::Dup(path);
    bool hasSep = path::IsSep(path.s[len(path) - 1]);
    e.name = hasSep ? str::Dup(path) : str::Join(path, StrL("\\"));
    VecAppend(out, e);
}

static void AppendHomeFileEntry(Vec<NavFileEntry>& out, Str path) {
    if (!CanOpenFile(path)) {
        return;
    }
    // Quick access keeps listing files after they are deleted
    WIN32_FILE_ATTRIBUTE_DATA fad{};
    if (!path::GetCachedAttributesEx(path, &fad)) {
        return;
    }
    NavFileEntry e;
    e.path = str::Dup(path);
    e.name = str::Dup(path);
    e.size = ((i64)fad.nFileSizeHigh << 32) | (i64)fad.nFileSizeLow;
    VecAppend(out, e);
}

// Home view: drive roots, then Explorer's Quick access folders and files
static void CollectHomeEntries(Vec<NavFileEntry>& out) {
    StrVec drives;
    ListDriveRoots(drives);
    for (int i = 0; i < len(drives); i++) {
        AppendHomeDirEntry(out, drives[i]);
    }

    StrVec dirs;
    StrVec files;
    GetQuickAccessCached(dirs, files);
    for (int i = 0; i < len(dirs); i++) {
        AppendHomeDirEntry(out, dirs[i]);
    }
    for (int i = 0; i < len(files); i++) {
        AppendHomeFileEntry(out, files[i]);
    }
}

// Built on a worker thread: listing + filtering + sorting a folder of tens of
// thousands of files must not freeze the UI (discussion #6014).
static void CollectNavEntriesForDir(Str dir, Vec<NavFileEntry>& out) {
    if (len(dir) == 0) {
        CollectHomeEntries(out);
        return;
    }
    // ".." also in a drive root, where it leads to the home view
    AppendParentEntry(out);
    int firstIdx = 1; // keep ".." at the top when sorting

    DirIter di{dir};
    di.includeFiles = true;
    di.includeDirs = true;
    for (DirIterEntry* de : di) {
        DWORD attrs = de->fd->dwFileAttributes;
        if (attrs & FILE_ATTRIBUTE_HIDDEN) {
            continue;
        }
        TempStr leaf = NavLeafNameTemp(de);
        if (len(leaf) == 0) {
            continue;
        }
        // own path/name before any further temp allocations
        Str fullPath = str::Dup(de->filePath);

        NavFileEntry e;
        e.path = fullPath;
        if (IsDirectory(de)) {
            e.isDir = true;
            e.name = str::Join(leaf, StrL("\\"));
        } else {
            if (!CanOpenFile(leaf)) {
                str::Free(fullPath);
                continue;
            }
            e.name = str::Dup(leaf);
            e.size = GetFileSize(de);
            // FindFirstFile size is sometimes 0 on network/cloud providers even
            // when the file has content; attributes are cached for network paths.
            if (e.size == 0) {
                WIN32_FILE_ATTRIBUTE_DATA fad{};
                if (path::GetCachedAttributesEx(fullPath, &fad) && !(fad.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                    e.size = ((i64)fad.nFileSizeHigh << 32) | (i64)fad.nFileSizeLow;
                }
            }
        }
        VecAppend(out, e);
    }

    SortNavEntries(out, firstIdx);
}

static int FindEntryIndex(NavFilesInFolderWnd* wnd, ListBoxModelNav* m, Str selectPath);
static void SelectAndEnsureVisible(VirtListBox* lb, int idx);

struct NavDirScanReq {
    NavFilesInFolderWnd* wnd = nullptr;
    int gen = 0;
    bool isRefresh = false; // re-read of the dir already shown
    Str dir;                // owned
    ~NavDirScanReq() { str::Free(dir); }
};

struct NavDirScanResult {
    NavFilesInFolderWnd* wnd = nullptr;
    int gen = 0;
    bool isRefresh = false;
    Str dir; // owned
    Vec<NavFileEntry> entries;
    double totalMs = 0;
    ~NavDirScanResult() {
        str::Free(dir);
        for (NavFileEntry& e : entries) {
            FreeNavEntry(e);
        }
    }
};

static void FinishNavDirScan(NavDirScanResult* r) {
    AutoDelete del(r);
    NavFilesInFolderWnd* wnd = gNavFilesWnd;
    if (!wnd || wnd != r->wnd || r->gen != wnd->scanGen || !wnd->listBox) {
        logf("NavDirScan: drop stale gen %d (current wnd gen=%d)\n", r->gen, wnd ? wnd->scanGen : -1);
        return;
    }
    logf("NavDirScan: apply %d entries for %s (%.1fms UI thread=%d)\n", len(r->entries), r->dir, r->totalMs,
         (int)uitask::IsMainUIThread());

    auto* m = (ListBoxModelNav*)wnd->listBox->model;
    if (!m) {
        m = new ListBoxModelNav();
    }
    wnd->scanInFlight = false;
    // a re-read that found nothing new leaves the list alone: no repaint at all
    if (r->isRefresh && SameNavEntries(m->all, r->entries)) {
        return;
    }
    int scrollY = wnd->listBox->scrollY;
    StealNavEntries(m->all, r->entries);
    FilterNavEntries(m, wnd->filterWords);
    wnd->listBox->SetModel(m);
    wnd->UpdateDirLabel();

    int selIdx = 0;
    if (len(wnd->pendingSelectPath) > 0) {
        selIdx = FindEntryIndex(wnd, m, wnd->pendingSelectPath);
    } else if (wnd->pendingSelectIdx >= 0) {
        selIdx = wnd->pendingSelectIdx;
    }
    if (m->ItemsCount() > 0 && r->isRefresh) {
        // the user is looking at this list: keep the viewport where it was
        selIdx = std::min(selIdx, m->ItemsCount() - 1);
        wnd->listBox->SetCurrentSelection(selIdx);
        wnd->listBox->ScrollTo(scrollY);
    } else if (m->ItemsCount() > 0) {
        SelectAndEnsureVisible(wnd->listBox, selIdx);
    }
    wnd->listBox->Invalidate();
}

static void NavDirScanThread(NavDirScanReq* req) {
    AutoDelete delReq(req);
    auto tAll = TimeGet();
    // the home view enumerates a shell folder, which needs COM on this thread
    bool comInited = false;
    if (len(req->dir) == 0) {
        comInited = SUCCEEDED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED));
    }
    auto* r = new NavDirScanResult;
    r->wnd = req->wnd;
    r->gen = req->gen;
    r->isRefresh = req->isRefresh;
    r->dir = str::Dup(req->dir);
    CollectNavEntriesForDir(req->dir, r->entries);
    if (comInited) {
        CoUninitialize();
    }
    r->totalMs = TimeSinceInMs(tAll);
    logf("NavDirScan: thread done dir=%s n=%d total=%.1fms UI thread=%d\n", r->dir, len(r->entries), r->totalMs,
         (int)uitask::IsMainUIThread());
    auto fn = MkFunc0(FinishNavDirScan, r);
    uitask::Post(fn, "FinishNavDirScan");
}

// full path for entry e under currDir
static TempStr NavEntryPathTemp(NavFilesInFolderWnd* wnd, NavFileEntry& e) {
    if (str::Eq(e.name, StrL(".."))) {
        return path::GetDirTemp(wnd->currDir);
    }
    if (e.path) {
        return e.path;
    }
    Str name = e.name;
    if (e.isDir) {
        name = Str(name.s, name.len - 1); // strip the trailing "\\"
    }
    return path::JoinTemp(wnd->currDir, name);
}

// entry display name without a trailing path separator (dirs use "name\\")
static Str NavEntryBaseName(const NavFileEntry& e) {
    Str name = e.name;
    if (e.isDir && name.len > 0 && path::IsSep(name.s[name.len - 1])) {
        return Str(name.s, name.len - 1);
    }
    return name;
}

// index of selectPath in the listing, or 0 if not found / empty
static int FindEntryIndex(NavFilesInFolderWnd* wnd, ListBoxModelNav* m, Str selectPath) {
    if (!m || len(selectPath) == 0) {
        return 0;
    }
    for (int i = 0; i < len(m->entries); i++) {
        TempStr path = NavEntryPathTemp(wnd, m->entries[i]);
        if (str::EqI(path, selectPath) || path::IsSame(path, selectPath)) {
            return i;
        }
    }
    // basename fallback (path form differences: long-path prefix, slash style, etc.)
    TempStr base = path::GetBaseNameTemp(selectPath);
    for (int i = 0; i < len(m->entries); i++) {
        if (str::EqI(NavEntryBaseName(m->entries[i]), base)) {
            return i;
        }
    }
    return 0;
}

// select idx and scroll so it is visible (centered when possible)
static void SelectAndEnsureVisible(VirtListBox* lb, int idx) {
    if (!lb || idx < 0) {
        return;
    }
    int n = lb->ItemsCount();
    if (n <= 0) {
        return;
    }
    if (idx >= n) {
        idx = n - 1;
    }
    lb->SetCurrentSelection(idx);

    int itemH = lb->GetItemHeight();
    int visible = std::max(lb->UsableDy() / itemH, 1);
    int top = Clamp(idx - (visible / 2), 0, std::max(n - visible, 0));
    lb->ScrollTo(top * itemH);
}

void NavFilesInFolderWnd::UpdateDirLabel() {
    dirLabel->SetText(IsHome() ? Tr("Home") : currDir);
    dirLabel->Invalidate(); // SetText() doesn't repaint
}

void NavFilesInFolderWnd::UpdateNavButtons() {
    bool enabled[NavBtnCount] = {gNavHistIdx > 0, gNavHistIdx + 1 < len(gNavHistory), !IsHome(), !IsHome()};
    for (int i = 0; i < NavBtnCount; i++) {
        if (navBtns[i]) {
            navBtns[i]->SetIsEnabled(enabled[i]);
        }
    }
}

bool NavFilesInFolderWnd::IsHome() const {
    return len(currDir) == 0;
}

void NavFilesInFolderWnd::OnFilterChanged() {
    TempStr s = filterEdit->GetTextTemp();
    filterWords.Reset();
    SplitFilterToWords(s, filterWords);
    ApplyFilter();
}

// re-filter the current listing; a filter selects its first match
void NavFilesInFolderWnd::ApplyFilter() {
    auto* m = (ListBoxModelNav*)listBox->model;
    if (!m) {
        return;
    }
    TempStr sel = SelectedPathTemp();
    FilterNavEntries(m, filterWords);
    listBox->SetModel(m);
    if (m->ItemsCount() == 0) {
        listBox->SetCurrentSelection(-1);
    } else {
        int idx = len(filterWords) > 0 ? 0 : FindEntryIndex(this, m, sel);
        SelectAndEnsureVisible(listBox, idx);
    }
    listBox->Invalidate();
}

void NavFilesInFolderWnd::ClearFilter() {
    if (!filterEdit || len(filterWords) == 0) {
        return;
    }
    filterWords.Reset();
    filterEdit->SetText(Str{}); // EN_CHANGE re-applies the (now empty) filter
}

void NavFilesInFolderWnd::OnDirLabelClick(VirtMouseEvent*) {
    BeginEditPath();
}

// clicking away restores the label, like Esc
void NavFilesInFolderWnd::OnDirEditKillFocus() {
    EndEditPath(NavPathEditEnd::Cancel);
}

// edit the path in place: the edit covers the label until Enter / Esc
void NavFilesInFolderWnd::BeginEditPath() {
    if (!dirEdit || editingPath) {
        return;
    }
    editingPath = true;
    dirEdit->SetText(currDir);
    dirEdit->SetBounds(PathEditRect());
    dirEdit->SetIsVisible(true);
    EditSetFocus(dirEdit);
    EditSelectAll(dirEdit);
}

// a little taller than the label so the border doesn't crowd the text
Rect NavFilesInFolderWnd::PathEditRect() {
    Rect r = dirLabel->VisibleRectInWindow();
    int pad = DpiScale(2);
    r.y -= pad;
    r.dy += 2 * pad;
    return r;
}

// Commit: a directory is navigated to, a file's directory with that file
// selected; anything else leaves the current dir (the label never changed)
void NavFilesInFolderWnd::EndEditPath(NavPathEditEnd how) {
    if (!editingPath) {
        return;
    }
    editingPath = false; // before hiding: that fires kill-focus
    TempStr path = dirEdit->GetTextTemp();
    dirEdit->SetIsVisible(false);
    // hiding the child doesn't repaint the label it covered
    HwndInvalidateRect(hwnd, PathEditRect(), true);
    SetFocusTo(listBox);
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

// typing while the list has focus goes into the search field
void NavFilesInFolderWnd::OnListChar(VirtCharEvent* ev) {
    if (ev->c < ' ' || IsCtrlPressed() || !filterEdit) {
        return;
    }
    EditSetFocus(filterEdit);
    SendMessageW(filterEdit->hwnd, WM_CHAR, (WPARAM)ev->c, 0);
    ev->didHandle = true;
}

// show dir and record it in the session history, dropping the forward entries
void NavFilesInFolderWnd::Navigate(Str dir, Str selectPath) {
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

void NavFilesInFolderWnd::SetDir(Str dir, Str selectPath, int selectIdx, NavListReset reset) {
    // the filter is per folder, like Explorer's search box
    if (!str::EqI(dir, currDir)) {
        ClearFilter();
    }
    str::ReplaceWithCopy(&currDir, dir);
    UpdateNavButtons();
    scanGen++;
    scanInFlight = true;
    str::ReplaceWithCopy(&pendingSelectPath, selectPath);
    pendingSelectIdx = selectIdx;

    auto* m = (ListBoxModelNav*)listBox->model;
    if (!m) {
        m = new ListBoxModelNav();
    }
    if (reset == NavListReset::Clear) {
        // show ".." immediately so the window is usable while the listing runs
        ClearNavModel(m);
        if (!IsHome()) {
            AppendParentEntry(m->all);
        }
        FilterNavEntries(m, filterWords);
        listBox->SetModel(m);
        UpdateDirLabel();
        if (m->ItemsCount() > 0) {
            SelectAndEnsureVisible(listBox, 0);
        }
        listBox->Invalidate();
    }

    auto* req = new NavDirScanReq;
    req->wnd = this;
    req->gen = scanGen;
    req->isRefresh = reset == NavListReset::Keep;
    req->dir = str::Dup(currDir);
    logf("NavDirScan: start %s gen=%d (UI thread=%d)\n", currDir, scanGen, (int)uitask::IsMainUIThread());
    auto fn = MkFunc0(NavDirScanThread, req);
    RunAsync(fn, StrL("NavDirScan"));
}

// full path of the selected entry, or empty. Owned copy: callers pass it back
// into SetDir(), which frees the entries the path would otherwise point into.
TempStr NavFilesInFolderWnd::SelectedPathTemp() {
    if (!listBox) {
        return {};
    }
    int idx = listBox->GetCurrentSelection();
    auto* m = (ListBoxModelNav*)listBox->model;
    if (!m || idx < 0 || idx >= m->ItemsCount()) {
        return {};
    }
    NavFileEntry& e = m->entries[idx];
    if (str::Eq(e.name, StrL(".."))) {
        return {};
    }
    return str::DupTemp(NavEntryPathTemp(this, e));
}

// re-read the directory, keeping the selection on the same file. The listing is
// a snapshot, so files renamed / added / removed after it was taken (by F2 in
// the main window, by another app, ...) would otherwise linger (issue #5878).
void NavFilesInFolderWnd::RefreshList() {
    // WM_ACTIVATE can arrive during CreateCustom(), before the list exists
    if (!listBox) {
        return;
    }
    TempStr sel = SelectedPathTemp();
    int selIdx = -1;
    // listing still in flight: keep the file we meant to select
    if (len(sel) == 0 && len(pendingSelectPath) > 0) {
        sel = str::DupTemp(pendingSelectPath);
    } else if (len(sel) == 0 && scanInFlight) {
        selIdx = pendingSelectIdx;
    }
    TempStr dir = str::DupTemp(currDir);
    SetDir(dir, sel, selIdx, NavListReset::Keep);
}

// from a root directory (C:\) Up goes to the home view
void NavFilesInFolderWnd::GoUp() {
    if (IsHome()) {
        return;
    }
    if (!DirHasParent(currDir)) {
        GoHome();
        return;
    }
    // select the directory we're coming from
    TempStr cameFrom = str::DupTemp(currDir);
    Navigate(path::GetDirTemp(currDir), cameFrom);
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

void NavFilesInFolderWnd::GoBack() {
    if (gNavHistIdx <= 0) {
        return;
    }
    TempStr cameFrom = str::DupTemp(currDir);
    gNavHistIdx--;
    Str dir = gNavHistory[gNavHistIdx];
    SetDir(dir, SelectIfChildOf(cameFrom, dir));
}

void NavFilesInFolderWnd::GoForward() {
    if (gNavHistIdx + 1 >= len(gNavHistory)) {
        return;
    }
    TempStr cameFrom = str::DupTemp(currDir);
    gNavHistIdx++;
    Str dir = gNavHistory[gNavHistIdx];
    SetDir(dir, SelectIfChildOf(cameFrom, dir));
}

void NavFilesInFolderWnd::GoHome() {
    if (IsHome()) {
        return;
    }
    Navigate(Str{});
}

// inNewTab: Ctrl+Enter / Ctrl+double-click. Switches to the tab already showing
// the file, or opens it in a new tab, instead of replacing the current document.
void NavFilesInFolderWnd::ExecuteCurrentSelection(bool inNewTab) {
    int idx = listBox->GetCurrentSelection();
    auto* m = (ListBoxModelNav*)listBox->model;
    if (!m || idx < 0 || idx >= m->ItemsCount()) {
        return;
    }
    NavFileEntry& e = m->entries[idx];
    if (str::Eq(e.name, StrL(".."))) {
        GoUp();
        return;
    }
    TempStr path = NavEntryPathTemp(this, e);
    if (e.isDir) {
        Navigate(path);
        return;
    }

    MainWindow* mainWin = win;
    if (!IsMainWindowValidAndNotClosing(mainWin)) {
        ScheduleDeleteNavFilesWnd();
        return;
    }

    if (inNewTab) {
        WindowTab* existing = FindTabByFilePath(path);
        if (existing) {
            SelectTabInWindow(existing);
            if (existing->win) {
                SetForegroundWindow(existing->win->hwndFrame);
            }
            return;
        }
        DismissNextFileScrollHint(mainWin);
        LoadArgs args(path, mainWin);
        args.skipHistory = skipHistory;
        // no forceReuse: opens in a new tab, leaving the current document alone
        StartLoadDocument(&args);
        return;
    }

    WindowTab* tab = mainWin->CurrentTab();
    if (tab && !MaybeSaveAnnotations(tab)) {
        return;
    }
    DismissNextFileScrollHint(mainWin);
    LoadArgs args(path, mainWin);
    args.skipHistory = skipHistory;
    // replace the document in the current tab; keep this window open
    args.forceReuse = true;
    StartLoadDocument(&args);
    // Hand keyboard control back to the document, the way Ctrl + Enter does
    // when it switches to a tab that already has the file (issue #5903).
    // Loading is async and LoadModelIntoTab focuses the frame when it lands,
    // but that only helps if the frame is the foreground window by then - this
    // window is still on top otherwise, and arrow keys keep driving the list.
    // The window stays open so browsing can continue; it just isn't focused.
    SetForegroundWindow(mainWin->hwndFrame);
    HwndSetFocus(mainWin->hwndFrame);
}

// Del on a file moves it to the recycle bin (issue #5877), without a
// confirmation prompt -- the recycle bin is the undo. Directories are left
// alone: recursively deleting a folder from a file picker is too easy to
// trigger by accident, and "Show in Folder" + Explorer covers it.
void NavFilesInFolderWnd::DeleteCurrentSelection() {
    if (!CanAccessDisk() || gPluginMode) {
        return;
    }
    int idx = listBox->GetCurrentSelection();
    auto* m = (ListBoxModelNav*)listBox->model;
    if (!m || idx < 0 || idx >= m->ItemsCount()) {
        return;
    }
    NavFileEntry& e = m->entries[idx];
    if (e.isDir || str::Eq(e.name, StrL(".."))) {
        return;
    }
    // own the path: deleting re-fills the model, which frees the entry
    TempStr path = str::DupTemp(NavEntryPathTemp(this, e));
    if (!file::Exists(path)) {
        return;
    }

    // no confirmation prompt: the file goes to the recycle bin, so it's undoable

    // a document open in a tab keeps the file mapped, so the delete would fail;
    // close that tab first, like CmdDeleteFile does for the current document
    WindowTab* tab = FindTabByFilePath(path);
    if (tab) {
        if (!MaybeSaveAnnotations(tab)) {
            return;
        }
        CloseTab(tab, false);
    }
    DeleteFileFromDiskAndHistory(path);

    if (file::Exists(path)) {
        MessageBoxWarning(hwnd, fmt(Tr("Couldn't delete %s").s, path));
    }
    // re-list; keep the selection where the deleted entry was
    if (IsHome()) {
        ResetQuickAccessCache();
    }
    TempStr dir = str::DupTemp(currDir);
    SetDir(dir, Str{}, idx);
    SetFocusTo(listBox);
}

void NavFilesInFolderWnd::OnListDoubleClick() {
    ExecuteCurrentSelection(IsCtrlPressed());
}

void NavFilesInFolderWnd::OnKeyDown(KeyEvent* ev) {
    if (hwnd && ev->hwnd != hwnd && !IsChild(hwnd, ev->hwnd)) {
        return;
    }
    if (dirEdit && ev->hwnd == dirEdit->hwnd) {
        if (ev->vkey == VK_RETURN) {
            EndEditPath(NavPathEditEnd::Commit);
            ev->didHandle = true;
        } else if (ev->vkey == VK_ESCAPE) {
            EndEditPath(NavPathEditEnd::Cancel);
            ev->didHandle = true;
        }
        return;
    }
    if (ev->vkey == VK_RETURN) {
        ExecuteCurrentSelection(ev->isCtrl);
        ev->didHandle = true;
        return;
    }
    if (ev->vkey == 'F' && ev->isCtrl && !ev->isAlt && filterEdit) {
        EditSetFocus(filterEdit);
        EditSelectAll(filterEdit);
        ev->didHandle = true;
        return;
    }
    bool editFocused = filterEdit && ev->hwnd == filterEdit->hwnd;
    if (editFocused && !ev->isAlt) {
        // Up / Down / PgUp / PgDn move the list selection while typing;
        // Esc clears the filter; the edit keeps its other keys (Backspace, Del)
        switch (ev->vkey) {
            case VK_UP:
            case VK_DOWN:
            case VK_PRIOR:
            case VK_NEXT: {
                VirtKeyEvent kev;
                kev.target = listBox;
                kev.vkey = ev->vkey;
                kev.isCtrl = ev->isCtrl;
                kev.isShift = ev->isShift;
                listBox->OnKeyDown(&kev);
                ev->didHandle = true;
                return;
            }
            case VK_ESCAPE:
                if (len(filterWords) > 0) {
                    ClearFilter();
                    ev->didHandle = true;
                }
                return;
            case VK_F5:
                break;
            default:
                return;
        }
    }
    // Alt + Up / Left / Right go up / back / forward, like Explorer. They
    // arrive as WM_SYSKEYDOWN; swallowing them also avoids the system-menu beep.
    // Backspace goes up, like the classic Explorer / file dialogs
    if ((ev->vkey == VK_UP && ev->isAlt) || ev->vkey == VK_BACK) {
        GoUp();
        ev->didHandle = true;
        return;
    }
    if (ev->vkey == VK_LEFT && ev->isAlt) {
        GoBack();
        ev->didHandle = true;
        return;
    }
    if (ev->vkey == VK_RIGHT && ev->isAlt) {
        GoForward();
        ev->didHandle = true;
        return;
    }
    if (ev->vkey == VK_DELETE) {
        DeleteCurrentSelection();
        ev->didHandle = true;
        return;
    }
    if (ev->vkey == VK_F5) {
        if (IsHome()) {
            ResetQuickAccessCache();
        }
        RefreshList();
        ev->didHandle = true;
    }
}

static void NavButtonClicked(NavFilesInFolderWnd* w, VirtMouseEvent* ev) {
    auto* btn = (VirtIconButton*)ev->target;
    switch (btn->id - 1) {
        case NavBtnBack:
            w->GoBack();
            break;
        case NavBtnForward:
            w->GoForward();
            break;
        case NavBtnUp:
            w->GoUp();
            break;
        case NavBtnHome:
            w->GoHome();
            break;
    }
    w->SetFocusTo(w->listBox);
}

void NavFilesInFolderWnd::CreateNavButtons(Color fg, Color bg) {
    static const char* icons[NavBtnCount] = {gIconNavigateBack, gIconNavigateForward, gIconArrowUp, gIconHome};
    Str tips[NavBtnCount] = {fmt("%s (Alt + Left)", Tr("Back")), fmt("%s (Alt + Right)", Tr("Forward")),
                             fmt("%s (Alt + Up, Backspace)", Tr("Up")), Tr("Home")};
    int isz = RoundUp(DpiScale(16), 4);
    int pad = DpiScale(4);
    Color dis = ThemeWindowTextDisabledColor();
    for (int i = 0; i < NavBtnCount; i++) {
        auto* b = new VirtIconButton();
        b->id = i + 1; // 0 is "no id"
        b->padding = Insets{pad, pad, pad, pad};
        b->SetTooltip(tips[i]);
        b->pixmap = GetCachedPixmapForSvg(Str(icons[i]), isz, isz, fg, bg);
        b->pixmapDisabled = GetCachedPixmapForSvg(Str(icons[i]), isz, isz, dis, bg);
        b->onClick = MkFunc1(NavButtonClicked, this);
        navBtns[i] = b;
    }
}

// after activate: refresh dir listing and put focus on the list (Alt-Tab)
void NavFilesInFolderWnd::OnActivate(WindowBase::ActivateEvent* ev) {
    if (ev->state != WA_INACTIVE) {
        // first show already started a listing; don't kick off a second one
        if (!scanInFlight) {
            RefreshList();
        }
        ScheduleFocusNavListBox();
    }
}

// top-level received focus (e.g. Alt-Tab); steer it to the list
void NavFilesInFolderWnd::OnFocus(WindowBase::FocusEvent*) {
    // the list is a virtual control: this window holds the win32 focus on
    // its behalf, so only the focus inside the tree moves
    if (vroot && listBox) {
        vroot->SetFocus(listBox);
    }
}

void NavFilesInFolderWnd::DrawListBoxItem(VirtListBox::DrawItemEvent* ev) {
    VirtListBox* lb = ev->listBox;
    auto* m = (ListBoxModelNav*)lb->model;
    if (ev->itemIndex < 0 || ev->itemIndex >= m->ItemsCount()) {
        return;
    }

    Gfx* gfx = ev->gfx;
    HWND hwndList = lb->GetHwnd();
    Rect rc = ev->itemRect;
    NavFileEntry& e = m->entries[ev->itemIndex];

    Color colBg = lb->GetColor(kColListBg);
    Color colText = lb->GetColor(kColListText);
    if (IsSpecialColor(colBg)) {
        colBg = GetSysColor(COLOR_WINDOW);
    }
    if (IsSpecialColor(colText)) {
        colText = GetSysColor(COLOR_WINDOWTEXT);
    }

    if (ev->selected) {
        colBg = AccentColor(colBg, 30);
    }

    gfx->FillRect(rc, colBg);

    // drawing text into a mirrored surface would mirror the glyphs; we lay the
    // row out right-to-left ourselves instead
    bool isRtl = HwndIsRtl(hwndList);
    bool prevMirrored = isRtl ? gfx->SetMirrored(false) : false;

    int padX = DpiScale(4);
    rc.x += padX;
    rc.dx -= 2 * padX;

    // human readable file size on the right (files only; include 0-byte files)
    Rect rcText = rc;
    TempStr sizeStr;
    int rightDx = 0;
    if (!e.isDir) {
        sizeStr = str::FormatSizeShortTemp(e.size);
        rightDx = gfx->MeasureText(sizeStr, lb->font).dx;
        int gap = DpiScale(8);
        if (isRtl) {
            rcText.x += rightDx + gap;
            rcText.dx -= rightDx + gap;
        } else {
            rcText.dx -= rightDx + gap;
        }
    }

    {
        // directories in bold, without the trailing "\"; filter matches highlighted
        u32 drawFmt = gfxTextEllipsis | gfxTextVCenter;
        drawFmt |= isRtl ? (gfxTextRight | gfxTextRtl) : gfxTextLeft;
        PlatformFont* font = e.isDir ? GetBoldPlatformFont(lb->font) : lb->font;
        DrawMaybeHighlightedText(gfx, rcText, NavEntryBaseName(e), filterWords, highlighted, colBg, isRtl, false,
                                 drawFmt, font, colText);
    }

    if (sizeStr) {
        Rect rcRight = rc;
        u32 drawFmt = gfxTextVCenter;
        if (isRtl) {
            rcRight.dx = rightDx;
            drawFmt |= gfxTextLeft | gfxTextRtl;
        } else {
            rcRight.x = rc.x + rc.dx - rightDx;
            rcRight.dx = rightDx;
            drawFmt |= gfxTextRight;
        }
        gfx->DrawText(sizeStr, rcRight, drawFmt, lb->font, AccentColor(colText, 80));
    }

    if (isRtl) {
        gfx->SetMirrored(prevMirrored);
    }
}

// non-client (frame) size for an outer width/height of the given client size
static Size NavFrameChrome(HWND hwnd) {
    DWORD style = (DWORD)GetWindowLongPtrW(hwnd, GWL_STYLE);
    DWORD exStyle = (DWORD)GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
    RECT r{0, 0, 0, 0};
    AdjustWindowRectEx(&r, style, FALSE, exStyle);
    return {r.right - r.left, r.bottom - r.top};
}

// free horizontal space on the work area to the left / right of the main frame
static void NavFreeSpaceBeside(HWND hwndMain, int* freeLeftOut, int* freeRightOut) {
    Rect main = HwndWindowRect(hwndMain);
    Rect work = GetWorkAreaRect(main, hwndMain);
    *freeLeftOut = main.x - work.x;
    *freeRightOut = (work.x + work.dx) - (main.x + main.dx);
    *freeLeftOut = std::max(*freeLeftOut, 0);
    *freeRightOut = std::max(*freeRightOut, 0);
}

// pick docked client size when there is enough free space beside the main window.
// returns true and fills clientDx/clientDy; false means fall back to centered placement.
static bool NavDockedClientSize(HWND hwnd, HWND hwndMain, int* clientDxOut, int* clientDyOut, bool* placeLeftOut) {
    int freeLeft = 0;
    int freeRight = 0;
    NavFreeSpaceBeside(hwndMain, &freeLeft, &freeRight);

    int minFree = DpiScale(kNavDockMinFreeDx);
    int free = 0;
    bool placeLeft = false;
    // NOLINTNEXTLINE(bugprone-branch-clone): left is preferred first, then falls back after right
    if (freeLeft > freeRight && freeLeft > minFree) {
        free = freeLeft;
        placeLeft = true;
    } else if (freeRight > minFree) {
        free = freeRight;
        placeLeft = false;
    } else if (freeLeft > minFree) {
        free = freeLeft;
        placeLeft = true;
    } else {
        return false;
    }

    // outer width fits the free strip but is capped at kNavDockMaxWidthDx
    int maxOuterDx = DpiScale(kNavDockMaxWidthDx);
    int outerDx = free < maxOuterDx ? free : maxOuterDx;

    Rect main = HwndWindowRect(hwndMain);
    int outerDy = main.dy;
    Size chrome = NavFrameChrome(hwnd);
    int clientDx = outerDx - chrome.dx;
    int clientDy = outerDy - chrome.dy;
    int minClientDx = DpiScale(kNavMinClientDx);
    int minClientDy = DpiScale(kNavMinClientDy);
    clientDx = std::max(clientDx, minClientDx);
    clientDy = std::max(clientDy, minClientDy);
    *clientDxOut = clientDx;
    *clientDyOut = clientDy;
    *placeLeftOut = placeLeft;
    return true;
}

// place hwnd next to the main frame (docked) or centered over it (fallback)
static void PositionNavFilesWnd(HWND hwnd, HWND hwndMain, bool docked, bool placeLeft) {
    Rect main = HwndWindowRect(hwndMain);
    Rect r = HwndWindowRect(hwnd);
    int x;
    int y;
    if (docked) {
        y = main.y;
        if (placeLeft) {
            x = main.x - r.dx;
        } else {
            x = main.x + main.dx;
        }
    } else {
        x = main.x + (main.dx / 2) - (r.dx / 2);
        y = main.y + DpiScale(kNavFallbackYOffset);
    }
    Rect r2 = ShiftRectToWorkArea({x, y, r.dx, r.dy}, hwndMain, true);
    SetWindowPos(hwnd, nullptr, r2.x, r2.y, 0, 0, SWP_NOZORDER | SWP_NOSIZE);
}

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
    TempStr docs = GetSpecialFolderTemp(CSIDL_PERSONAL);
    if (len(docs) > 0 && dir::Exists(docs)) {
        return docs;
    }
    return GetSelfExeDirTemp();
}

// filePath: the file to browse to and select; empty for the current tab's
// folder or, on the home page, the newest history entry's
bool NavFilesInFolderWnd::Create(MainWindow* mainWin, Str filePath) {
    win = mainWin;
    {
        CreateCustomArgs args;
        args.visible = false;
        // regular resizable window (not a popup that auto-dismisses)
        args.style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME;
        args.title = Tr("Navigate Files in Folder");
        args.font = GetFont();
        args.icon = LoadIconW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(GetAppIconID()));
        args.isRtl = IsUIRtl();
        CreateCustom(args);
    }
    if (!hwnd) {
        return false;
    }
    // top-level (no owner) so Alt-Tab switches between this and the main window
    DarkModeApplyToTitleBar(hwnd);

    auto colBg = ThemeWindowControlBackgroundColor();
    auto colTxt = ThemeWindowTextColor();
    SetColors(colTxt, colBg);
    CreateNavButtons(colTxt, colBg);

    auto* vbox = new VBox();
    vbox->alignMain = MainAxisAlign::MainStart;
    vbox->alignCross = CrossAxisAlign::Stretch;

    {
        // top row: Back / Forward / Up / Home buttons, then the current dir
        auto* row = new HBox();
        row->alignCross = CrossAxisAlign::CrossCenter;
        row->rtl = IsUIRtl();
        for (VirtIconButton* b : navBtns) {
            row->AddChild(b);
        }
        auto* c = NewVirtText({
            .font = font,
            .isRtl = IsUIRtl(),
            .ellipsis = true,
        });
        dirLabel = c;
        c->SetFlag(vwfNoHitTest, false); // plain text ignores the mouse
        c->cursor = CursorId::IBeam;
        c->SetTooltip(Tr("Click to edit the path"));
        c->onClick = MkMethod1<NavFilesInFolderWnd, VirtMouseEvent*, &NavFilesInFolderWnd::OnDirLabelClick>(this);
        row->AddChild(new Padding(c, Insets{0, 4, 0, 4}), 1);
        vbox->AddChild(new Padding(row, Insets{0, 0, 4, 0}));
    }

    {
        // path editor: not in the layout, placed over dirLabel while editing
        Edit::CreateArgs args;
        args.parent = hwnd;
        args.withBorder = true;
        args.font = font;
        args.isRtl = IsUIRtl();
        dirEdit = new Edit();
        dirEdit->SetColors(colTxt, colBg);
        dirEdit->Create(args);
        dirEdit->SetIsVisible(false);
        dirEdit->onKillFocus = MkMethod0<NavFilesInFolderWnd, &NavFilesInFolderWnd::OnDirEditKillFocus>(this);
    }

    {
        // second row: filters the list below as you type
        Edit::CreateArgs args;
        args.parent = hwnd;
        args.withBorder = true;
        args.cueText = Tr("Search");
        args.font = font;
        args.isRtl = IsUIRtl();
        filterEdit = new Edit();
        filterEdit->SetColors(colTxt, colBg);
        filterEdit->Create(args);
        filterEdit->onTextChanged = MkMethod0<NavFilesInFolderWnd, &NavFilesInFolderWnd::OnFilterChanged>(this);
        vbox->AddChild(new Padding(filterEdit, Insets{0, 0, 4, 0}));
    }

    {
        auto* c = new VirtListBox();
        c->dpi = GetDpi();
        c->font = font;
        c->padding = DpiScaledInsets(4, 0);
        c->onDoubleClick = MkMethod0<NavFilesInFolderWnd, &NavFilesInFolderWnd::OnListDoubleClick>(this);
        c->onChar = MkMethod1<NavFilesInFolderWnd, VirtCharEvent*, &NavFilesInFolderWnd::OnListChar>(this);
        c->onDrawItem =
            MkMethod1<NavFilesInFolderWnd, VirtListBox::DrawItemEvent*, &NavFilesInFolderWnd::DrawListBoxItem>(this);
        listBox = c;
        vbox->AddChild(c, 1);
    }

    {
        // one wrapping line of key-cap hints, like the command palette help
        // row; translators keep the key names in English
        TempStr hints = fmt("(Kbd/%s) %s (Kbd/%s) %s (Kbd/%s) %s", Tr("Enter"), Tr("open in current tab"),
                            Tr("Ctrl + Enter"), Tr("open in new tab"), Tr("Del"), Tr("delete file"));
        // the hints are secondary information, so they get a smaller font
        PlatformFont* helpFont = GetDefaultGuiFontOfSize(std::max(GetAppFontSize() - 2, 8));
        auto* k = new VirtRichText();
        ParseTipInto(k, hints);
        k->font = helpFont;
        k->SetColor(kColRichText, colTxt);
        k->SetColor(kColRichLink, colTxt);
        k->SetColor(kColRichBg, colBg);
        auto* center = new Align(k);
        center->HAlign = AlignCenter;
        vbox->AddChild(new Padding(center, Insets{DpiScale(4), 0, 0, 0}));
    }

    auto* padding = new Padding(vbox, DpiScaledInsets(4, 8));
    layout = padding;

    TempStr dir = len(filePath) > 0 ? path::GetDirTemp(filePath) : Str{};
    if (len(dir) == 0 || !dir::Exists(dir)) {
        dir = NavStartDirNoDocTemp();
    }
    Navigate(dir, filePath);
    // remember selection: layout below changes listbox size, so LB_SETCURSEL
    // during SetDir may not leave the item visible in the final viewport
    int selIdx = listBox->GetCurrentSelection();

    int dx = 0;
    int dy = 0;
    bool placeLeft = false;
    bool docked = NavDockedClientSize(hwnd, mainWin->hwndFrame, &dx, &dy, &placeLeft);
    if (!docked) {
        auto rc = HwndClientRect(mainWin->hwndFrame);
        dy = rc.dy - DpiScale(kNavFallbackMainDyMargin);
        dy = std::max(dy, DpiScale(kNavFallbackMinDy));
        dx = limitValue(rc.dx - DpiScale(kNavFallbackMainDxMargin), DpiScale(kNavFallbackMinDx),
                        DpiScale(kNavFallbackMaxDx));
    }
    LayoutAndSizeToContent(layout, dx, dy, hwnd);
    // pick up the virtual controls so we paint them and they get their input
    DoLayout(HwndClientRect(hwnd).Size());
    PositionNavFilesWnd(hwnd, mainWin->hwndFrame, docked, placeLeft);

    if (selIdx >= 0) {
        SelectAndEnsureVisible(listBox, selIdx);
    }

    SetIsVisible(true);
    SetFocusTo(listBox);
    return true;
}

void ShowNavFilesInFolder(MainWindow* win, Str selectPath, bool skipHistory) {
    // Prefer an explicit path (e.g. home-page thumbnail); else the current tab.
    Str filePath = selectPath;
    if (len(filePath) == 0) {
        WindowTab* tab = win->CurrentTab();
        if (tab && !tab->IsAboutTab()) {
            filePath = tab->filePath;
        }
    }

    if (gNavFilesWnd) {
        if (gNavFilesWnd->hwnd && IsWindow(gNavFilesWnd->hwnd)) {
            // re-sync to the target folder (and re-read: the file may have been
            // renamed since, #5878)
            if (len(filePath) > 0) {
                gNavFilesWnd->Navigate(path::GetDirTemp(filePath), filePath);
            } else {
                // on the home page with no selection keep whatever dir is open
                gNavFilesWnd->RefreshList();
            }
            ShowWindow(gNavFilesWnd->hwnd, SW_SHOW);
            SetForegroundWindow(gNavFilesWnd->hwnd);
            gNavFilesWnd->skipHistory = skipHistory;
            if (gNavFilesWnd->listBox) {
                gNavFilesWnd->SetFocusTo(gNavFilesWnd->listBox);
            } else {
                HwndSetFocus(gNavFilesWnd->hwnd);
            }
            return;
        }
        ScheduleDeleteNavFilesWnd();
    }
    auto* wnd = new NavFilesInFolderWnd();
    wnd->skipHistory = skipHistory;
    wnd->closeOnEsc = true;
    wnd->onClose = MkFunc0Void(ScheduleDeleteNavFilesWnd);
    wnd->onDestroy = MkFunc0Void(ScheduleDeleteNavFilesWnd);
    wnd->onActivate = MkMethod1<NavFilesInFolderWnd, WindowBase::ActivateEvent*, &NavFilesInFolderWnd::OnActivate>(wnd);
    wnd->onFocus = MkMethod1<NavFilesInFolderWnd, WindowBase::FocusEvent*, &NavFilesInFolderWnd::OnFocus>(wnd);
    wnd->onKeyDown = MkMethod1<NavFilesInFolderWnd, KeyEvent*, &NavFilesInFolderWnd::OnKeyDown>(wnd);
    wnd->SetFont(GetAppFont());
    // set before Create so Esc during Create can dismiss
    gNavFilesWnd = wnd;
    gHwndToActivateOnNavClose = win->hwndFrame;
    bool ok = wnd->Create(win, filePath);
    if (!ok) {
        gNavFilesWnd = nullptr;
        gHwndToActivateOnNavClose = nullptr;
        delete wnd;
    }
}

// State and actions used by the -dbg-control regression test.
TempStr NavFilesInFolderStateTemp(Str action, int idx, int* exitCodeOut) {
    if (exitCodeOut) {
        *exitCodeOut = 2;
    }
    NavFilesInFolderWnd* wnd = gNavFilesWnd;
    if (!wnd || !wnd->listBox || !wnd->listBox->model) {
        return str::DupTemp(StrL("NOTREADY no-window"));
    }
    if (str::Eq(action, StrL("select"))) {
        wnd->listBox->SetCurrentSelection(idx);
    } else if (str::Eq(action, StrL("delete-refresh"))) {
        wnd->DeleteCurrentSelection();
        wnd->RefreshList();
    } else if (str::Eq(action, StrL("up"))) {
        wnd->GoUp();
    } else if (str::Eq(action, StrL("back"))) {
        wnd->GoBack();
    } else if (str::Eq(action, StrL("forward"))) {
        wnd->GoForward();
    } else if (str::Eq(action, StrL("home"))) {
        wnd->GoHome();
    } else if (str::Eq(action, StrL("execute"))) {
        wnd->ExecuteCurrentSelection();
    } else if (str::Eq(action, StrL("refresh"))) {
        wnd->RefreshList();
    } else if (str::Eq(action, StrL("path-label-rect"))) {
        // where a test must click to start editing the path (client coords)
        Rect r = wnd->dirLabel->VisibleRectInWindow();
        if (exitCodeOut) {
            *exitCodeOut = 0;
        }
        return fmt("OK %d %d %d %d", r.x, r.y, r.dx, r.dy);
    } else if (str::Eq(action, StrL("edit-path")) || str::TrimPrefix(action, StrL("path-"))) {
        // path editing: edit-path, path-text:<text>, path-commit, path-cancel,
        // path-state (report only)
        if (str::Eq(action, StrL("edit-path"))) {
            wnd->BeginEditPath();
        } else if (str::Eq(action, StrL("commit"))) {
            wnd->EndEditPath(NavPathEditEnd::Commit);
        } else if (str::Eq(action, StrL("cancel"))) {
            wnd->EndEditPath(NavPathEditEnd::Cancel);
        } else if (str::TrimPrefix(action, StrL("text:"))) {
            wnd->dirEdit->SetText(action);
        }
        if (exitCodeOut) {
            *exitCodeOut = 0;
        }
        TempStr text = wnd->dirEdit->GetTextTemp();
        return fmt("OK editing=%d text=\"%s\"", (int)wnd->editingPath, text);
    } else if (str::TrimPrefix(action, StrL("filter:"))) {
        wnd->filterEdit->SetText(action);
        wnd->OnFilterChanged();
    } else if (str::Eq(action, StrL("close"))) {
        wnd->Close();
        if (exitCodeOut) {
            *exitCodeOut = 0;
        }
        return str::DupTemp(StrL("OK closed"));
    }

    auto* m = (ListBoxModelNav*)wnd->listBox->model;
    int sel = wnd->listBox->GetCurrentSelection();
    Str name;
    if (sel >= 0 && sel < m->ItemsCount()) {
        name = m->entries[sel].name;
    }
    if (exitCodeOut) {
        *exitCodeOut = 0;
    }
    int canBack = wnd->navBtns[NavBtnBack]->IsEnabled();
    int canFwd = wnd->navBtns[NavBtnForward]->IsEnabled();
    return fmt("OK scan=%d sel=%d items=%d back=%d fwd=%d dir=\"%s\" name=\"%s\"", (int)wnd->scanInFlight, sel,
               m->ItemsCount(), canBack, canFwd, wnd->currDir, name);
}
