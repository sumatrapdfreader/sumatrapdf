/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/DirScan.h"
#include "base/Dpi.h"
#include "base/File.h"
#include "base/GuessFileType.h"
#include "base/UITask.h"
#include "base/Win.h"

#include "wingui/UIModels.h"
#include "wingui/Layout.h"
#include "wingui/WinGui.h"

#include "Settings.h"
#include "DocController.h"
#include "EngineBase.h"
#include "EngineAll.h"
#include "GlobalPrefs.h"
#include "SumatraConfig.h"
#include "AppSettings.h"
#include "MainWindow.h"
#include "WindowTab.h"
#include "SumatraPDF.h"
#include "Theme.h"
#include "Translations.h"
#include "DarkModeSubclass.h"
#include "NavFilesInFolder.h"

// A floating window with a directory listing of all sub-directories and
// files SumatraPDF can open (judged by extension). Enter / double-click
// opens a file or descends into a directory, the ".." entry at the top
// goes one directory up, Esc closes the window.

struct NavFileEntry {
    Str name; // owned; display name, "..\\" for the parent dir
    bool isDir = false;
    i64 size = 0; // file size, 0 for dirs
};

struct ListBoxModelNav : ListBoxModel {
    Vec<NavFileEntry> entries;

    ~ListBoxModelNav() override {
        for (NavFileEntry& e : entries) {
            str::Free(e.name);
        }
    }
    int ItemsCount() override { return len(entries); }
    Str Item(int i) override { return entries[i].name; }
};

struct NavFilesInFolderWnd : Wnd {
    ~NavFilesInFolderWnd() override;

    HFONT font = nullptr;
    MainWindow* win = nullptr;
    Static* dirLabel = nullptr;
    ListBox* listBox = nullptr;
    Str currDir; // owned

    // "n/m" shown after the path: n entries in currDir, m in the whole tree
    // below it. m needs a full traversal, so it arrives from a background
    // thread; countGen discards results for a directory we've navigated away
    // from. countTotalPartial means the traversal hit kMaxTreeCount.
    int countDirect = 0;
    int countTotal = -1; // -1: still counting
    bool countTotalPartial = false;
    i64 countGen = 0; // from gNavCountGen, so it can't collide with a previous window's

    bool PreTranslateMessage(MSG&) override;
    LRESULT WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) override;

    bool Create(MainWindow* win);
    void SetDir(Str dir, Str selectPath);
    void ExecuteCurrentSelection();
    void OnListDoubleClick();
    void GoUp();
    void DrawListBoxItem(ListBox::DrawItemEvent* ev);
    void UpdateDirLabel();
};

static NavFilesInFolderWnd* gNavFilesWnd = nullptr;
static HWND gHwndToActivateOnNavClose = nullptr;
// never reset, so an in-flight count from a closed window can't be mistaken
// for one belonging to a window opened afterwards
static i64 gNavCountGen = 0;

NavFilesInFolderWnd::~NavFilesInFolderWnd() {
    str::Free(currDir);
}

static void SafeDeleteNavFilesWnd() {
    if (!gNavFilesWnd) {
        return;
    }
    auto tmp = gNavFilesWnd;
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

// only called for entries we already know are files (both callers check
// IsDirectory(de) first), so skip GuessFileTypeFromName()'s IsDirectory() probe:
// it's a per-entry round trip on a network drive, and this runs over a whole
// tree. The name is also relative to the listed dir, so the probe was answering
// about a path relative to the cwd anyway.
static bool CanOpenFile(Str path) {
    FileType kind = GuessFileTypeFromName(path, true);
    return IsSupportedFileType(kind, true) || DocIsSupportedFileType(kind);
}

// Traversing a big tree (or a network drive) can take a long time and the user
// can browse anywhere, including a drive root, so stop counting after this many
// entries and show the total as "n+".
constexpr int kMaxTreeCount = 50 * 1000;

// true for entries the listing shows: sub-directories and openable files,
// skipping hidden ones. Keep in sync with FillEntriesForDir().
static bool IsCountedEntry(DirIterEntry* de, bool* isDirOut) {
    *isDirOut = false;
    if (de->fd->dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) {
        return false;
    }
    if (IsDirectory(de)) {
        *isDirOut = true;
        return true;
    }
    return CanOpenFile(de->name);
}

// number of entries under dir, including all sub-directories. Uses an explicit
// worklist rather than DirIter's recurse so hidden directories are pruned the
// same way the listing prunes them.
static int CountEntriesInTree(Str dir, bool* partialOut) {
    *partialOut = false;
    int n = 0;
    StrVec dirs;
    dirs.Append(dir);
    // dirs grows while we iterate: index-based so appends can't invalidate it
    for (int i = 0; i < len(dirs); i++) {
        DirIter di{dirs[i]};
        di.includeFiles = true;
        di.includeDirs = true;
        for (DirIterEntry* de : di) {
            bool isDir = false;
            if (!IsCountedEntry(de, &isDir)) {
                continue;
            }
            n++;
            if (n >= kMaxTreeCount) {
                *partialOut = true;
                return n;
            }
            if (isDir) {
                dirs.Append(de->filePath);
            }
        }
    }
    return n;
}

struct CountTreeData {
    Str dir; // owned
    i64 gen = 0;
    int total = 0;
    bool partial = false;
};

static void CountTreeFinished(CountTreeData* d) {
    NavFilesInFolderWnd* wnd = gNavFilesWnd;
    // ignore if the window is gone or the user already navigated elsewhere
    if (wnd && wnd->countGen == d->gen) {
        wnd->countTotal = d->total;
        wnd->countTotalPartial = d->partial;
        wnd->UpdateDirLabel();
    }
    str::Free(d->dir);
    delete d;
}

static void CountTreeThread(CountTreeData* d) {
    d->total = CountEntriesInTree(d->dir, &d->partial);
    auto fn = MkFunc0<CountTreeData>(CountTreeFinished, d);
    uitask::Post(fn, "CountTreeFinished");
    DestroyTempArena();
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

static void FillEntriesForDir(ListBoxModelNav* m, Str dir) {
    for (NavFileEntry& e : m->entries) {
        str::Free(e.name);
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
        NavFileEntry e;
        if (IsDirectory(de)) {
            e.isDir = true;
            e.name = str::Join(de->name, StrL("\\"));
        } else {
            if (!CanOpenFile(de->name)) {
                continue;
            }
            e.name = str::Dup(de->name);
            e.size = GetFileSize(de);
        }
        m->entries.Append(e);
    }

    SortNavEntries(m->entries, firstIdx);
}

// display name for entry i resolved to a full path in currDir
static TempStr NavEntryPathTemp(NavFilesInFolderWnd* wnd, NavFileEntry& e) {
    if (str::Eq(e.name, StrL(".."))) {
        return path::GetDirTemp(wnd->currDir);
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
static void SelectAndEnsureVisible(ListBox* lb, int idx) {
    if (!lb || !lb->hwnd || idx < 0) {
        return;
    }
    int n = lb->GetCount();
    if (n <= 0) {
        return;
    }
    if (idx >= n) {
        idx = n - 1;
    }
    lb->SetCurrentSelection(idx);

    int itemH = lb->GetItemHeight(0);
    if (itemH <= 0) {
        LbSetTopIndex(lb->hwnd, idx);
        return;
    }
    Rect client = HwndClientRect(lb->hwnd);
    int visible = client.dy / itemH;
    if (visible < 1) {
        visible = 1;
    }
    int top = idx - visible / 2;
    if (top < 0) {
        top = 0;
    }
    int maxTop = n - visible;
    if (maxTop < 0) {
        maxTop = 0;
    }
    if (top > maxTop) {
        top = maxTop;
    }
    LbSetTopIndex(lb->hwnd, top);
}

// "<path>  n/m": n entries here, m in this directory and its sub-directories
void NavFilesInFolderWnd::UpdateDirLabel() {
    TempStr total;
    if (countTotal < 0) {
        total = str::DupTemp("…"); // still counting the tree
    } else if (countTotalPartial) {
        total = fmt("%d+", countTotal);
    } else {
        total = fmt("%d", countTotal);
    }
    dirLabel->SetText(fmt("%s  %d/%s", currDir, countDirect, total));
}

void NavFilesInFolderWnd::SetDir(Str dir, Str selectPath) {
    str::ReplaceWithCopy(&currDir, dir);

    auto m = (ListBoxModelNav*)listBox->model;
    if (!m) {
        m = new ListBoxModelNav();
    }
    FillEntriesForDir(m, currDir);
    listBox->SetModel(m);

    // ".." is navigation, not an entry of this directory
    countDirect = m->ItemsCount();
    if (countDirect > 0 && str::Eq(m->entries[0].name, StrL(".."))) {
        countDirect--;
    }
    countTotal = -1;
    countTotalPartial = false;
    countGen = ++gNavCountGen;
    UpdateDirLabel();
    {
        auto d = new CountTreeData;
        d->dir = str::Dup(currDir);
        d->gen = countGen;
        auto fn = MkFunc0<CountTreeData>(CountTreeThread, d);
        RunAsync(fn, "CountTreeThread");
    }

    if (m->ItemsCount() > 0) {
        int selIdx = FindEntryIndex(this, m, selectPath);
        SelectAndEnsureVisible(listBox, selIdx);
    }
    HwndScheduleRepaint(listBox->hwnd);
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

void NavFilesInFolderWnd::ExecuteCurrentSelection() {
    int idx = listBox->GetCurrentSelection();
    auto m = (ListBoxModelNav*)listBox->model;
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
    WindowTab* tab = mainWin->CurrentTab();
    if (tab && !MaybeSaveAnnotations(tab)) {
        return;
    }
    DismissNextFileScrollHint(mainWin);
    LoadArgs args(path, mainWin);
    args.forceReuse = true;
    StartLoadDocument(&args);
    ScheduleDeleteNavFilesWnd();
}

void NavFilesInFolderWnd::OnListDoubleClick() {
    ExecuteCurrentSelection();
}

bool NavFilesInFolderWnd::PreTranslateMessage(MSG& msg) {
    // only keys aimed at this window or its children
    if (hwnd && msg.hwnd != hwnd && !IsChild(hwnd, msg.hwnd)) {
        return false;
    }
    bool isKey = (msg.message == WM_KEYDOWN || msg.message == WM_SYSKEYDOWN);
    bool isEscChar = (msg.message == WM_CHAR && msg.wParam == VK_ESCAPE);
    if (!isKey && !isEscChar) {
        return false;
    }
    if (msg.wParam == VK_ESCAPE || isEscChar) {
        ScheduleDeleteNavFilesWnd();
        return true;
    }
    if (!isKey) {
        return false;
    }
    if (msg.wParam == VK_RETURN) {
        ExecuteCurrentSelection();
        return true;
    }
    if (msg.wParam == VK_BACK) {
        GoUp();
        return true;
    }
    return false;
}

LRESULT NavFilesInFolderWnd::WndProc(HWND hwndIn, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_ACTIVATE && wp == WA_INACTIVE) {
        ScheduleDeleteNavFilesWnd();
        return 0;
    }
    // Esc when this popup (not a child) has focus
    if (msg == WM_KEYDOWN && wp == VK_ESCAPE) {
        ScheduleDeleteNavFilesWnd();
        return 0;
    }
    return WndProcDefault(hwndIn, msg, wp, lp);
}

void NavFilesInFolderWnd::DrawListBoxItem(ListBox::DrawItemEvent* ev) {
    ListBox* lb = ev->listBox;
    auto m = (ListBoxModelNav*)lb->model;
    if (ev->itemIndex < 0 || ev->itemIndex >= m->ItemsCount()) {
        return;
    }

    HDC hdc = ev->hdc;
    Rect rc = ev->itemRect;
    NavFileEntry& e = m->entries[ev->itemIndex];

    COLORREF colBg = IsSpecialColor(lb->bgColor) ? GetSysColor(COLOR_WINDOW) : lb->bgColor;
    COLORREF colText = IsSpecialColor(lb->textColor) ? GetSysColor(COLOR_WINDOWTEXT) : lb->textColor;
    if (ev->selected) {
        colBg = AccentColor(colBg, 30);
    }

    SetBkColor(hdc, colBg);
    HdcFillRectWithBkColor(hdc, rc);

    bool isRtl = HwndIsRtl(lb->hwnd);
    if (isRtl) {
        SetLayout(hdc, 0);
    }

    SetTextColor(hdc, colText);
    SetBkMode(hdc, TRANSPARENT);

    HFONT oldFont = nullptr;
    if (lb->font) {
        oldFont = SelectFont(hdc, lb->font);
    }

    int padX = DpiScale(lb->hwnd, 4);
    rc.x += padX;
    rc.dx -= 2 * padX;

    // human readable file size on the right (files only)
    Rect rcText = rc;
    TempWStr rightW = nullptr;
    int rightDx = 0;
    if (!e.isDir && e.size > 0) {
        TempStr sizeStr = str::FormatSizeShortTemp(e.size);
        rightW = ToWStrTemp(sizeStr);
        rightDx = HdcGetTextExtentPoint32(hdc, sizeStr).dx;
        int gap = DpiScale(lb->hwnd, 8);
        if (isRtl) {
            rcText.x += rightDx + gap;
            rcText.dx -= rightDx + gap;
        } else {
            rcText.dx -= rightDx + gap;
        }
    }

    {
        uint drawFmt = DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS;
        drawFmt |= isRtl ? (DT_RIGHT | DT_RTLREADING) : DT_LEFT;
        TempWStr nameW = ToWStrTemp(e.name);
        HdcDrawText(hdc, nameW, rcText, drawFmt);
    }

    if (rightW) {
        Rect rcRight = rc;
        uint drawFmt = DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX;
        if (isRtl) {
            rcRight.dx = rightDx;
            drawFmt |= DT_LEFT | DT_RTLREADING;
        } else {
            rcRight.x = rc.x + rc.dx - rightDx;
            rcRight.dx = rightDx;
            drawFmt |= DT_RIGHT;
        }
        SetTextColor(hdc, AccentColor(colText, 80));
        HdcDrawText(hdc, rightW, rcRight, drawFmt);
        SetTextColor(hdc, colText);
    }

    if (oldFont) {
        SelectFont(hdc, oldFont);
    }
}

static void PositionNavFilesWnd(HWND hwnd, HWND hwndRelative) {
    Rect rRelative = HwndWindowRect(hwndRelative);
    Rect r = HwndWindowRect(hwnd);
    int x = rRelative.x + (rRelative.dx / 2) - (r.dx / 2);
    int y = rRelative.y + (rRelative.dy / 2) - (r.dy / 2);
    Rect r2 = ShiftRectToWorkArea({x, y, r.dx, r.dy}, hwndRelative, true);
    r2.y = rRelative.y + 42;
    SetWindowPos(hwnd, nullptr, r2.x, r2.y, 0, 0, SWP_NOZORDER | SWP_NOSIZE);
}

static void OnNavFilesWndClose(Wnd::CloseEvent*) {
    ScheduleDeleteNavFilesWnd();
}

static void OnNavFilesWndDestroy(Wnd::DestroyEvent*) {
    ScheduleDeleteNavFilesWnd();
}

bool NavFilesInFolderWnd::Create(MainWindow* mainWin) {
    win = mainWin;
    {
        CreateCustomArgs args;
        args.visible = false;
        args.style = WS_POPUPWINDOW;
        args.font = font;
        CreateCustom(args);
    }
    if (!hwnd) {
        return false;
    }

    auto colBg = ThemeWindowControlBackgroundColor();
    auto colTxt = ThemeWindowTextColor();
    SetColors(colTxt, colBg);

    auto vbox = new VBox();
    vbox->alignMain = MainAxisAlign::MainStart;
    vbox->alignCross = CrossAxisAlign::Stretch;

    {
        Static::CreateArgs args;
        args.parent = hwnd;
        args.font = font;
        args.isRtl = IsUIRtl();
        auto c = new Static();
        HWND ok = c->Create(args);
        ReportIf(!ok);
        c->SetColors(colTxt, colBg);
        dirLabel = c;
        vbox->AddChild(new Padding(c, Insets{0, 4, 4, 4}));
    }

    {
        ListBox::CreateArgs args;
        args.parent = hwnd;
        args.font = font;
        args.isRtl = IsUIRtl();
        auto c = new ListBox();
        c->onDoubleClick = MkMethod0<NavFilesInFolderWnd, &NavFilesInFolderWnd::OnListDoubleClick>(this);
        c->onDrawItem =
            MkMethod1<NavFilesInFolderWnd, ListBox::DrawItemEvent*, &NavFilesInFolderWnd::DrawListBoxItem>(this);
        c->SetInsetsPt(4, 0);
        c->Create(args);
        c->SetColors(colTxt, colBg);
        listBox = c;
        if (UseDarkModeLib()) {
            DarkMode::setDarkScrollBar(listBox->hwnd);
        }
        vbox->AddChild(c, 1);
    }

    {
        Str strings[3] = {_TRA("↑ ↓ to navigate"), _TRA("Enter to select"), _TRA("Esc to close")};
        auto hbox = new HBox();
        hbox->alignMain = MainAxisAlign::MainCenter;
        hbox->alignCross = CrossAxisAlign::CrossCenter;
        auto pad = Insets{0, 8, 0, 8};
        for (Str s : strings) {
            Static::CreateArgs args;
            args.parent = hwnd;
            args.font = font;
            args.text = s;
            args.isRtl = IsUIRtl();
            auto c = new Static();
            HWND ok = c->Create(args);
            ReportIf(!ok);
            c->SetColors(colTxt, colBg);
            hbox->AddChild(new Padding(c, pad));
        }
        vbox->AddChild(hbox);
    }

    auto padding = new Padding(vbox, DpiScaledInsets(hwnd, 4, 8));
    layout = padding;

    WindowTab* tab = mainWin->CurrentTab();
    Str filePath = tab ? tab->filePath : Str{};
    TempStr dir = path::GetDirTemp(filePath);
    SetDir(dir, filePath);
    // remember selection: layout below changes listbox size, so LB_SETCURSEL
    // during SetDir may not leave the item visible in the final viewport
    int selIdx = listBox->GetCurrentSelection();

    auto rc = HwndClientRect(mainWin->hwndFrame);
    int dy = rc.dy - 72;
    if (dy < 480) {
        dy = 480;
    }
    int dx = limitValue(rc.dx - 256, 480, 720);
    LayoutAndSizeToContent(layout, dx, dy, hwnd);
    PositionNavFilesWnd(hwnd, mainWin->hwndFrame);

    if (selIdx >= 0) {
        SelectAndEnsureVisible(listBox, selIdx);
    }

    SetIsVisible(true);
    HwndSetFocus(listBox->hwnd);
    return true;
}

void ShowNavFilesInFolder(MainWindow* win) {
    if (gNavFilesWnd) {
        if (gNavFilesWnd->hwnd && IsWindow(gNavFilesWnd->hwnd)) {
            if (gNavFilesWnd->listBox) {
                HwndSetFocus(gNavFilesWnd->listBox->hwnd);
            } else {
                HwndSetFocus(gNavFilesWnd->hwnd);
            }
            return;
        }
        ScheduleDeleteNavFilesWnd();
    }
    WindowTab* tab = win->CurrentTab();
    if (!tab || len(tab->filePath) == 0) {
        return;
    }

    auto wnd = new NavFilesInFolderWnd();
    wnd->onClose = MkFunc1Void<Wnd::CloseEvent*>(OnNavFilesWndClose);
    wnd->onDestroy = MkFunc1Void<Wnd::DestroyEvent*>(OnNavFilesWndDestroy);
    wnd->font = GetAppBiggerFont(win->hwndFrame);
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
}
