/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/DirScan.h"
#include "gui/Dpi.h"
#include "base/File.h"
#include "base/GuessFileType.h"
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
#include "GlobalPrefs.h"
#include "FileHistory.h"
#include "SumatraConfig.h"
#include "AppSettings.h"
#include "MainWindow.h"
#include "WindowTab.h"
#include "SumatraPDF.h"
#include "Theme.h"
#include "Translations.h"
#include "DarkMode_win.h"
#include "NavFilesInFolder.h"

// A modeless directory browser listing sub-directories and files SumatraPDF
// can open (judged by extension). Enter / double-click replaces the document
// in the current tab or descends into a directory; Ctrl + Enter / Ctrl +
// double-click switches to the tab already showing the file or opens it in a
// new tab; ".." goes one directory up. The window stays open so you can open
// several files in succession; Esc or the close button dismisses it.

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
    Vec<NavFileEntry> entries;

    ~ListBoxModelNav() override {
        for (NavFileEntry& e : entries) {
            FreeNavEntry(e);
        }
    }
    int ItemsCount() override { return len(entries); }
    Str Item(int i) override { return entries[i].name; }
};

struct NavFilesInFolderWnd : WindowBase {
    ~NavFilesInFolderWnd() override;

    MainWindow* win = nullptr;
    // the label, the list and the hints are virtual controls; this window has
    // no HWND children at all
    VirtText* dirLabel = nullptr;
    VirtListBox* listBox = nullptr;
    Str currDir; // owned

    void OnKeyDown(KeyEvent* ev);
    void OnActivate(WindowBase::ActivateEvent* ev);
    void OnFocus(WindowBase::FocusEvent* ev);

    bool Create(MainWindow* win);
    void SetDir(Str dir, Str selectPath);
    TempStr SelectedPathTemp();
    void RefreshList();
    void ExecuteCurrentSelection(bool inNewTab = false);
    void DeleteCurrentSelection();
    void OnListDoubleClick();
    void GoUp();
    void DrawListBoxItem(VirtListBox::DrawItemEvent* ev);
    void UpdateDirLabel();
};

static NavFilesInFolderWnd* gNavFilesWnd = nullptr;
static HWND gHwndToActivateOnNavClose = nullptr;

NavFilesInFolderWnd::~NavFilesInFolderWnd() {
    str::Free(currDir);
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

// dirs first, then files, each sorted naturally by name
static void SortNavEntries(Vec<NavFileEntry>& entries, int firstIdx) {
    auto less = [](const NavFileEntry& a, const NavFileEntry& b) -> bool {
        if (a.isDir != b.isDir) {
            return a.isDir;
        }
        return str::CmpNatural(a.name, b.name) < 0;
    };
    for (int i = firstIdx + 1; i < len(entries); i++) {
        NavFileEntry value = entries[i];
        int j = i - 1;
        while (j >= firstIdx && less(value, entries[j])) {
            entries[j + 1] = entries[j];
            j--;
        }
        entries[j + 1] = value;
    }
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

static void FillEntriesForDir(ListBoxModelNav* m, Str dir) {
    for (NavFileEntry& e : m->entries) {
        FreeNavEntry(e);
    }
    m->entries.Reset();

    int firstIdx = 0;
    TempStr parent = path::GetDirTemp(dir);
    if (!path::IsSame(parent, dir)) {
        NavFileEntry e;
        e.name = str::Dup("..");
        e.isDir = true;
        m->entries.Append(e);
        firstIdx = 1; // keep ".." at the top when sorting
    }

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
        m->entries.Append(e);
    }

    SortNavEntries(m->entries, firstIdx);
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
    dirLabel->SetText(currDir);
}

void NavFilesInFolderWnd::SetDir(Str dir, Str selectPath) {
    str::ReplaceWithCopy(&currDir, dir);

    auto* m = (ListBoxModelNav*)listBox->model;
    if (!m) {
        m = new ListBoxModelNav();
    }
    FillEntriesForDir(m, currDir);
    listBox->SetModel(m);
    UpdateDirLabel();

    if (m->ItemsCount() > 0) {
        int selIdx = FindEntryIndex(this, m, selectPath);
        SelectAndEnsureVisible(listBox, selIdx);
    }
    listBox->Invalidate();
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
    if (!listBox || len(currDir) == 0) {
        return;
    }
    TempStr sel = SelectedPathTemp();
    TempStr dir = str::DupTemp(currDir);
    SetDir(dir, sel);
}

void NavFilesInFolderWnd::GoUp() {
    TempStr parent = path::GetDirTemp(currDir);
    if (path::IsSame(parent, currDir)) {
        return;
    }
    // select the directory we're coming from
    TempStr cameFrom = str::DupTemp(currDir);
    SetDir(str::DupTemp(parent), cameFrom);
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
        SetDir(path, Str{});
        return;
    }

    MainWindow* mainWin = win;
    if (!IsMainWindowValid(mainWin)) {
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
        MessageBoxWarning(hwnd, fmt(_TRA("Couldn't delete %s").s, path));
    }
    // re-list; keep the selection where the deleted entry was
    TempStr dir = str::DupTemp(currDir);
    SetDir(dir, Str{});
    SelectAndEnsureVisible(listBox, idx);
    SetFocusTo(listBox);
}

void NavFilesInFolderWnd::OnListDoubleClick() {
    ExecuteCurrentSelection(IsCtrlPressed());
}

void NavFilesInFolderWnd::OnKeyDown(KeyEvent* ev) {
    if (hwnd && ev->hwnd != hwnd && !IsChild(hwnd, ev->hwnd)) {
        return;
    }
    if (ev->vkey == VK_RETURN) {
        ExecuteCurrentSelection(ev->isCtrl);
        ev->didHandle = true;
        return;
    }
    // Alt + Up goes to the parent directory, like Explorer. It arrives as
    // WM_SYSKEYDOWN; swallowing it also avoids the system-menu beep
    if (ev->vkey == VK_UP && ev->isAlt) {
        GoUp();
        ev->didHandle = true;
        return;
    }
    if (ev->vkey == VK_DELETE) {
        DeleteCurrentSelection();
        ev->didHandle = true;
        return;
    }
    if (ev->vkey == VK_F5) {
        RefreshList();
        ev->didHandle = true;
    }
}

// after activate: refresh dir listing and put focus on the list (Alt-Tab)
void NavFilesInFolderWnd::OnActivate(WindowBase::ActivateEvent* ev) {
    if (ev->state != WA_INACTIVE) {
        RefreshList();
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
    TempStr sizeStr = nullptr;
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
        u32 drawFmt = gfxTextEllipsis | gfxTextVCenter;
        drawFmt |= isRtl ? (gfxTextRight | gfxTextRtl) : gfxTextLeft;
        gfx->DrawText(e.name, rcText, drawFmt, lb->font, colText);
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

bool NavFilesInFolderWnd::Create(MainWindow* mainWin) {
    win = mainWin;
    {
        CreateCustomArgs args;
        args.visible = false;
        // regular resizable window (not a popup that auto-dismisses)
        args.style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME;
        args.title = _TRA("Navigate Files in Folder");
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

    auto* vbox = new VBox();
    vbox->alignMain = MainAxisAlign::MainStart;
    vbox->alignCross = CrossAxisAlign::Stretch;

    {
        auto* c = NewVirtText({
            .font = font,
            .isRtl = IsUIRtl(),
            .ellipsis = true,
        });
        dirLabel = c;
        vbox->AddChild(new Padding(c, Insets{0, 4, 4, 4}));
    }

    {
        auto* c = new VirtListBox();
        c->dpi = GetDpi();
        c->font = font;
        c->padding = DpiScaledInsets(4, 0);
        c->onDoubleClick = MkMethod0<NavFilesInFolderWnd, &NavFilesInFolderWnd::OnListDoubleClick>(this);
        c->onDrawItem =
            MkMethod1<NavFilesInFolderWnd, VirtListBox::DrawItemEvent*, &NavFilesInFolderWnd::DrawListBoxItem>(this);
        listBox = c;
        vbox->AddChild(c, 1);
    }

    {
        Str strings[4] = {_TRA("Alt + Up: go to parent directory"), _TRA("Enter: open file in current tab"),
                          _TRA("Ctrl + Enter: open file in a new tab"), _TRA("Del: delete file")};
        auto* hbox = new VBox();
        hbox->alignMain = MainAxisAlign::MainCenter;
        hbox->alignCross = CrossAxisAlign::CrossCenter;
        auto pad = Insets{0, 8, 0, 8};
        for (Str s : strings) {
            auto* c = NewVirtText({.s = s, .font = font, .isRtl = IsUIRtl()});
            hbox->AddChild(new Padding(c, pad));
        }
        vbox->AddChild(hbox);
    }

    auto* padding = new Padding(vbox, DpiScaledInsets(4, 8));
    layout = padding;

    WindowTab* tab = mainWin->CurrentTab();
    Str filePath = (tab && !tab->IsAboutTab()) ? tab->filePath : Str{};
    TempStr dir = len(filePath) > 0 ? path::GetDirTemp(filePath) : NavStartDirNoDocTemp();
    SetDir(dir, filePath);
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

void ShowNavFilesInFolder(MainWindow* win, Str selectPath) {
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
                gNavFilesWnd->SetDir(path::GetDirTemp(filePath), filePath);
            } else {
                // on the home page with no selection keep whatever dir is open
                gNavFilesWnd->RefreshList();
            }
            ShowWindow(gNavFilesWnd->hwnd, SW_SHOW);
            SetForegroundWindow(gNavFilesWnd->hwnd);
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
    bool ok = wnd->Create(win);
    if (!ok) {
        gNavFilesWnd = nullptr;
        gHwndToActivateOnNavClose = nullptr;
        delete wnd;
        return;
    }
    // Create() picks the current tab / history start dir; override when the
    // caller asked for a specific file (home-page thumbnail context menu).
    if (len(selectPath) > 0) {
        TempStr dir = path::GetDirTemp(selectPath);
        if (len(dir) > 0 && dir::Exists(dir)) {
            wnd->SetDir(dir, selectPath);
            int selIdx = wnd->listBox ? wnd->listBox->GetCurrentSelection() : -1;
            if (selIdx >= 0) {
                SelectAndEnsureVisible(wnd->listBox, selIdx);
            }
        }
    }
}
