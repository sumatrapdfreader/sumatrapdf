/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/DirIter.h"
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

    bool PreTranslateMessage(MSG&) override;
    LRESULT WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) override;

    bool Create(MainWindow* win);
    void SetDir(Str dir, Str selectPath);
    void ExecuteCurrentSelection();
    void OnListDoubleClick();
    void GoUp();
    void DrawListBoxItem(ListBox::DrawItemEvent* ev);
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

static bool CanOpenFile(Str path) {
    FileType kind = GuessFileTypeFromName(path);
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
    if (str::Eq(e.name, "..")) {
        return path::GetDirTemp(wnd->currDir);
    }
    Str name = e.name;
    if (e.isDir) {
        name = Str(name.s, name.len - 1); // strip the trailing "\\"
    }
    return path::JoinTemp(wnd->currDir, name);
}

void NavFilesInFolderWnd::SetDir(Str dir, Str selectPath) {
    str::ReplaceWithCopy(&currDir, dir);
    dirLabel->SetText(currDir);

    auto m = (ListBoxModelNav*)listBox->model;
    if (!m) {
        m = new ListBoxModelNav();
    }
    FillEntriesForDir(m, currDir);
    listBox->SetModel(m);

    int selIdx = 0;
    if (len(selectPath) > 0) {
        for (int i = 0; i < len(m->entries); i++) {
            TempStr path = NavEntryPathTemp(this, m->entries[i]);
            if (path::IsSame(path, selectPath)) {
                selIdx = i;
                break;
            }
        }
    }
    if (m->ItemsCount() > 0) {
        listBox->SetCurrentSelection(selIdx);
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
    if (str::Eq(e.name, "..")) {
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
    LoadArgs args(path, mainWin);
    args.forceReuse = true;
    StartLoadDocument(&args);
    ScheduleDeleteNavFilesWnd();
}

void NavFilesInFolderWnd::OnListDoubleClick() {
    ExecuteCurrentSelection();
}

bool NavFilesInFolderWnd::PreTranslateMessage(MSG& msg) {
    if (msg.message != WM_KEYDOWN) {
        return false;
    }
    if (msg.wParam == VK_ESCAPE) {
        ScheduleDeleteNavFilesWnd();
        return true;
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
    return WndProcDefault(hwndIn, msg, wp, lp);
}

void NavFilesInFolderWnd::DrawListBoxItem(ListBox::DrawItemEvent* ev) {
    ListBox* lb = ev->listBox;
    auto m = (ListBoxModelNav*)lb->model;
    if (ev->itemIndex < 0 || ev->itemIndex >= m->ItemsCount()) {
        return;
    }

    HDC hdc = ev->hdc;
    RECT rc = ev->itemRect;
    NavFileEntry& e = m->entries[ev->itemIndex];

    COLORREF colBg = IsSpecialColor(lb->bgColor) ? GetSysColor(COLOR_WINDOW) : lb->bgColor;
    COLORREF colText = IsSpecialColor(lb->textColor) ? GetSysColor(COLOR_WINDOWTEXT) : lb->textColor;
    if (ev->selected) {
        colBg = AccentColor(colBg, 30);
    }

    SetBkColor(hdc, colBg);
    ExtTextOutW(hdc, 0, 0, ETO_OPAQUE, &rc, nullptr, 0, nullptr);

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
    rc.left += padX;
    rc.right -= padX;

    // human readable file size on the right (files only)
    RECT rcText = rc;
    TempWStr rightW = nullptr;
    int rightDx = 0;
    if (!e.isDir && e.size > 0) {
        TempStr sizeStr = str::FormatSizeShortTemp(e.size);
        rightW = ToWStrTemp(sizeStr);
        SIZE szRight{};
        GetTextExtentPoint32W(hdc, rightW.s, len(rightW), &szRight);
        rightDx = szRight.cx;
        int gap = DpiScale(lb->hwnd, 8);
        if (isRtl) {
            rcText.left += rightDx + gap;
        } else {
            rcText.right -= rightDx + gap;
        }
    }

    {
        uint drawFmt = DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS;
        drawFmt |= isRtl ? (DT_RIGHT | DT_RTLREADING) : DT_LEFT;
        TempWStr nameW = ToWStrTemp(e.name);
        DrawTextW(hdc, nameW.s, -1, &rcText, drawFmt);
    }

    if (rightW) {
        RECT rcRight = rc;
        uint drawFmt = DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX;
        if (isRtl) {
            rcRight.right = rc.left + rightDx;
            drawFmt |= DT_LEFT | DT_RTLREADING;
        } else {
            rcRight.left = rc.right - rightDx;
            drawFmt |= DT_RIGHT;
        }
        SetTextColor(hdc, AccentColor(colText, 80));
        DrawTextW(hdc, rightW.s, -1, &rcRight, drawFmt);
        SetTextColor(hdc, colText);
    }

    if (oldFont) {
        SelectFont(hdc, oldFont);
    }
}

static void PositionNavFilesWnd(HWND hwnd, HWND hwndRelative) {
    Rect rRelative = WindowRect(hwndRelative);
    Rect r = WindowRect(hwnd);
    int x = rRelative.x + (rRelative.dx / 2) - (r.dx / 2);
    int y = rRelative.y + (rRelative.dy / 2) - (r.dy / 2);
    Rect r2 = ShiftRectToWorkArea({x, y, r.dx, r.dy}, hwndRelative, true);
    r2.y = rRelative.y + 42;
    SetWindowPos(hwnd, nullptr, r2.x, r2.y, 0, 0, SWP_NOZORDER | SWP_NOSIZE);
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

    auto rc = ClientRect(mainWin->hwndFrame);
    int dy = rc.dy - 72;
    if (dy < 480) {
        dy = 480;
    }
    int dx = limitValue(rc.dx - 256, 480, 720);
    LayoutAndSizeToContent(layout, dx, dy, hwnd);
    PositionNavFilesWnd(hwnd, mainWin->hwndFrame);

    SetIsVisible(true);
    HwndSetFocus(listBox->hwnd);
    return true;
}

void ShowNavFilesInFolder(MainWindow* win) {
    if (gNavFilesWnd) {
        HwndSetFocus(gNavFilesWnd->hwnd);
        return;
    }
    WindowTab* tab = win->CurrentTab();
    if (!tab || len(tab->filePath) == 0) {
        return;
    }

    auto wnd = new NavFilesInFolderWnd();
    wnd->onDestroy = MkFunc1Void<Wnd::DestroyEvent*>(OnNavFilesWndDestroy);
    wnd->font = GetAppBiggerFont(win->hwndFrame);
    bool ok = wnd->Create(win);
    if (!ok) {
        delete wnd;
        return;
    }
    gNavFilesWnd = wnd;
    gHwndToActivateOnNavClose = win->hwndFrame;
}
