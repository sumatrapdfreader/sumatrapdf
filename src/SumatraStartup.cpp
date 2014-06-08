/* Copyright 2014 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// TODO: for the moment it needs to be included from SumatraPDF.cpp
// and not compiled as stand-alone

#include "DbgHelpDyn.h"

#ifdef DEBUG
static bool TryLoadMemTrace()
{
    ScopedMem<WCHAR> exePath(GetExePath());
    ScopedMem<WCHAR> exeDir(path::GetDir(exePath));
    ScopedMem<WCHAR> dllPath(path::Join(exeDir, L"memtrace.dll"));
    if (!LoadLibrary(dllPath))
        return false;
    return true;
}
#endif

// gFileExistenceChecker is initialized at startup and should
// terminate and delete itself asynchronously while the UI is
// being set up
class FileExistenceChecker;
static FileExistenceChecker *gFileExistenceChecker = NULL;

class FileExistenceChecker : public ThreadBase, public UITask
{
    WStrVec paths;

public:
    FileExistenceChecker() {
        DisplayState *state;
        for (size_t i = 0; i < 2 * FILE_HISTORY_MAX_RECENT && (state = gFileHistory.Get(i)) != NULL; i++) {
            if (!state->isMissing)
                paths.Append(str::Dup(state->filePath));
        }
        // add missing paths from the list of most frequently opened documents
        Vec<DisplayState *> frequencyList;
        gFileHistory.GetFrequencyOrder(frequencyList);
        for (size_t i = 0; i < 2 * FILE_HISTORY_MAX_FREQUENT && i < frequencyList.Count(); i++) {
            state = frequencyList.At(i);
            if (!paths.Contains(state->filePath))
                paths.Append(str::Dup(state->filePath));
        }
    }

    virtual void Run() {
        // filters all file paths on network drives, removable drives and
        // all paths which still exist from the list (remaining paths will
        // be marked as inexistent in gFileHistory)
        for (size_t i = 0; i < paths.Count() && !WasCancelRequested(); i++) {
            WCHAR *path = paths.At(i);
            if (!path || !path::IsOnFixedDrive(path) || DocumentPathExists(path)) {
                paths.RemoveAt(i--);
                free(path);
            }
        }
        if (!WasCancelRequested())
            uitask::Post(this);
    }

    virtual void Execute() {
        for (size_t i = 0; i < paths.Count(); i++) {
            gFileHistory.MarkFileInexistent(paths.At(i), true);
        }
        // update the Frequently Read page in case it's been displayed already
        if (paths.Count() > 0 && gWindows.Count() > 0 && gWindows.At(0)->IsAboutWindow())
            gWindows.At(0)->RedrawAll(true);
        // prepare for clean-up (Join() just to be safe)
        gFileExistenceChecker = NULL;
        Join();
    }
};

static void MakePluginWindow(WindowInfo& win, HWND hwndParent)
{
    assert(IsWindow(hwndParent));
    assert(gPluginMode);

    long ws = GetWindowLong(win.hwndFrame, GWL_STYLE);
    ws &= ~(WS_POPUP | WS_BORDER | WS_CAPTION | WS_THICKFRAME);
    ws |= WS_CHILD;
    SetWindowLong(win.hwndFrame, GWL_STYLE, ws);

    SetParent(win.hwndFrame, hwndParent);
    MoveWindow(win.hwndFrame, ClientRect(hwndParent));
    ShowWindow(win.hwndFrame, SW_SHOW);
    UpdateWindow(win.hwndFrame);

    // from here on, we depend on the plugin's host to resize us
    SetFocus(win.hwndFrame);
}

static bool RegisterWinClass(HINSTANCE hinst)
{
    WNDCLASSEX  wcex;
    ATOM        atom;

    FillWndClassEx(wcex, hinst, FRAME_CLASS_NAME, WndProcFrame);
    wcex.hIcon  = LoadIcon(hinst, MAKEINTRESOURCE(IDI_SUMATRAPDF));
    atom = RegisterClassEx(&wcex);
    CrashIf(!atom);

    FillWndClassEx(wcex, hinst, CANVAS_CLASS_NAME, WndProcCanvas);
    wcex.style |= CS_DBLCLKS;
    atom = RegisterClassEx(&wcex);
    CrashIf(!atom);

    FillWndClassEx(wcex, hinst, PROPERTIES_CLASS_NAME, WndProcProperties);
    wcex.hIcon = LoadIcon(hinst, MAKEINTRESOURCE(IDI_SUMATRAPDF));
    atom = RegisterClassEx(&wcex);
    CrashIf(!atom);

    FillWndClassEx(wcex, hinst, SIDEBAR_SPLITTER_CLASS_NAME, WndProcSidebarSplitter);
    wcex.hCursor        = LoadCursor(NULL, IDC_SIZEWE);
    wcex.hbrBackground  = (HBRUSH)(COLOR_BTNFACE + 1);
    atom = RegisterClassEx(&wcex);
    CrashIf(!atom);

    FillWndClassEx(wcex, hinst, FAV_SPLITTER_CLASS_NAME, WndProcFavSplitter);
    wcex.hCursor        = LoadCursor(NULL, IDC_SIZENS);
    wcex.hbrBackground  = (HBRUSH)(COLOR_BTNFACE + 1);
    atom = RegisterClassEx(&wcex);
    CrashIf(!atom);

    RegisterNotificationsWndClass(hinst);

    return true;
}

COLORREF GetLogoBgColor()
{
    COLORREF bgColor = ABOUT_BG_LOGO_COLOR;
    if (ABOUT_BG_COLOR_DEFAULT != gGlobalPrefs->mainWindowBackground)
        bgColor = gGlobalPrefs->mainWindowBackground;
    return bgColor;
}

COLORREF GetAboutBgColor()
{
#ifdef ABOUT_USE_LESS_COLORS
    return ABOUT_BG_GRAY_COLOR;
#else
    return GetLogoBgColor();
#endif
}

COLORREF GetNoDocBgColor()
{
    // use the system background color if the user has non-default
    // colors for text (not black-on-white) and also wants to use them
    bool useSysColor = gGlobalPrefs->useSysColors &&
                       (GetSysColor(COLOR_WINDOWTEXT) != WIN_COL_BLACK ||
                        GetSysColor(COLOR_WINDOW) != WIN_COL_WHITE);
    if (useSysColor)
        return GetSysColor(COLOR_BTNFACE);

    return COL_WINDOW_BG;
}

static bool InstanceInit(HINSTANCE hInstance, int nCmdShow)
{
    ghinst = hInstance;

    gCursorArrow = LoadCursor(NULL, IDC_ARROW);
    gCursorIBeam = LoadCursor(NULL, IDC_IBEAM);
    gCursorHand  = LoadCursor(NULL, IDC_HAND);
    if (!gCursorHand) // IDC_HAND isn't available if WINVER < 0x0500
        gCursorHand = LoadCursor(ghinst, MAKEINTRESOURCE(IDC_CURSORDRAG));

    gCursorScroll   = LoadCursor(NULL, IDC_SIZEALL);
    gCursorDrag     = LoadCursor(ghinst, MAKEINTRESOURCE(IDC_CURSORDRAG));
    gCursorSizeWE   = LoadCursor(NULL, IDC_SIZEWE);
    gCursorSizeNS   = LoadCursor(NULL, IDC_SIZENS);
    gCursorNo       = LoadCursor(NULL, IDC_NO);

    NONCLIENTMETRICS ncm = { 0 };
    ncm.cbSize = sizeof(ncm);
    SystemParametersInfo(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0);
    gDefaultGuiFont = CreateFontIndirect(&ncm.lfMessageFont);
    gBitmapReloadingCue = LoadBitmap(ghinst, MAKEINTRESOURCE(IDB_RELOADING_CUE));

    return true;
}

static void OpenUsingDde(HWND targetWnd, const WCHAR *filePath, CommandLineInfo& i, bool isFirstWin)
{
    // delegate file opening to a previously running instance by sending a DDE message
    WCHAR fullpath[MAX_PATH];
    GetFullPathName(filePath, dimof(fullpath), fullpath, NULL);

    str::Str<WCHAR> cmd;
    cmd.AppendFmt(L"[" DDECOMMAND_OPEN L"(\"%s\", 0, 1, 0)]", fullpath);
    if (i.destName && isFirstWin) {
        cmd.AppendFmt(L"[" DDECOMMAND_GOTO L"(\"%s\", \"%s\")]", fullpath, i.destName);
    }
    else if (i.pageNumber > 0 && isFirstWin) {
        cmd.AppendFmt(L"[" DDECOMMAND_PAGE L"(\"%s\", %d)]", fullpath, i.pageNumber);
    }
    if ((i.startView != DM_AUTOMATIC || i.startZoom != INVALID_ZOOM ||
            i.startScroll.x != -1 && i.startScroll.y != -1) && isFirstWin) {
        const WCHAR *viewMode = prefs::conv::FromDisplayMode(i.startView);
        cmd.AppendFmt(L"[" DDECOMMAND_SETVIEW L"(\"%s\", \"%s\", %.2f, %d, %d)]",
                      fullpath, viewMode, i.startZoom, i.startScroll.x, i.startScroll.y);
    }
    if (i.forwardSearchOrigin && i.forwardSearchLine) {
        ScopedMem<WCHAR> sourcePath(path::Normalize(i.forwardSearchOrigin));
        cmd.AppendFmt(L"[" DDECOMMAND_SYNC L"(\"%s\", \"%s\", %d, 0, 0, 1)]",
                      fullpath, sourcePath, i.forwardSearchLine);
    }

    if (!i.reuseDdeInstance) {
        // try WM_COPYDATA first, as that allows targetting a specific window
        COPYDATASTRUCT cds = { 0x44646557 /* DdeW */, (DWORD)(cmd.Size() + 1) * sizeof(WCHAR), cmd.Get() };
        LRESULT res = SendMessage(targetWnd, WM_COPYDATA, NULL, (LPARAM)&cds);
        if (res) {
            SetForegroundWindow(targetWnd);
            return;
        }
    }
    DDEExecute(PDFSYNC_DDE_SERVICE, PDFSYNC_DDE_TOPIC, cmd.Get());
}

static WindowInfo *LoadOnStartup(const WCHAR *filePath, CommandLineInfo& i, bool isFirstWin)
{
    LoadArgs args(filePath);
    args.showWin = !(i.printDialog && i.exitWhenDone) && !gPluginMode;
    WindowInfo *win = LoadDocument(args);
    if (!win)
        return win;

    if (win->IsDocLoaded() && i.destName && isFirstWin) {
        win->linkHandler->GotoNamedDest(i.destName);
    } else if (win->IsDocLoaded() && i.pageNumber > 0 && isFirstWin) {
        if (win->ctrl->ValidPageNo(i.pageNumber))
            win->ctrl->GoToPage(i.pageNumber, false);
    }
    if (i.hwndPluginParent)
        MakePluginWindow(*win, i.hwndPluginParent);
    if (!win->IsDocLoaded() || !isFirstWin)
        return win;

    if (i.enterPresentation || i.enterFullScreen)
        EnterFullScreen(*win, i.enterPresentation);
    if (i.startView != DM_AUTOMATIC)
        SwitchToDisplayMode(win, i.startView);
    if (i.startZoom != INVALID_ZOOM)
        ZoomToSelection(win, i.startZoom);
    if ((i.startScroll.x != -1 || i.startScroll.y != -1) && win->AsFixed()) {
        DisplayModel *dm = win->AsFixed();
        ScrollState ss = dm->GetScrollState();
        ss.x = i.startScroll.x;
        ss.y = i.startScroll.y;
        dm->SetScrollState(ss);
    }
    if (i.forwardSearchOrigin && i.forwardSearchLine && win->AsFixed() && win->AsFixed()->pdfSync) {
        UINT page;
        Vec<RectI> rects;
            ScopedMem<WCHAR> sourcePath(path::Normalize(i.forwardSearchOrigin));
        int ret = win->AsFixed()->pdfSync->SourceToDoc(sourcePath, i.forwardSearchLine, 0, &page, rects);
        ShowForwardSearchResult(win, sourcePath, i.forwardSearchLine, 0, ret, page, rects);
    }
    return win;
}

static bool SetupPluginMode(CommandLineInfo& i)
{
    if (!IsWindow(i.hwndPluginParent) || i.fileNames.Count() == 0)
        return false;

    gPluginURL = i.pluginURL;
    if (!gPluginURL)
        gPluginURL = i.fileNames.At(0);

    assert(i.fileNames.Count() == 1);
    while (i.fileNames.Count() > 1) {
        free(i.fileNames.Pop());
    }

    // don't save preferences for plugin windows (and don't allow fullscreen mode)
    // TODO: Perm_DiskAccess is required for saving viewed files and printing and
    //       Perm_InternetAccess is required for crash reports
    // (they can still be disabled through sumatrapdfrestrict.ini or -restrict)
    gPolicyRestrictions = (gPolicyRestrictions | Perm_RestrictedUse) & ~(Perm_SavePreferences | Perm_FullscreenAccess);

    i.reuseDdeInstance = i.exitWhenDone = false;
    gGlobalPrefs->reuseInstance = false;
    // don't allow tabbed navigation
    gGlobalPrefs->useTabs = false;
    // always display the toolbar when embedded (as there's no menubar in that case)
    gGlobalPrefs->showToolbar = true;
    // never allow esc as a shortcut to quit
    gGlobalPrefs->escToExit = false;
    // never show the sidebar by default
    gGlobalPrefs->showToc = false;
    if (DM_AUTOMATIC == gGlobalPrefs->defaultDisplayModeEnum) {
        // if the user hasn't changed the default display mode,
        // display documents as single page/continuous/fit width
        // (similar to Adobe Reader, Google Chrome and how browsers display HTML)
        gGlobalPrefs->defaultDisplayModeEnum = DM_CONTINUOUS;
        gGlobalPrefs->defaultZoomFloat = ZOOM_FIT_WIDTH;
    }
    // use fixed page UI for all document types (so that the context menu always
    // contains all plugin specific entries and the main window is never closed)
    gGlobalPrefs->ebookUI.useFixedPageUI = gGlobalPrefs->chmUI.useFixedPageUI = true;

    // extract some command line arguments from the URL's hash fragment where available
    // see http://www.adobe.com/devnet/acrobat/pdfs/pdf_open_parameters.pdf#nameddest=G4.1501531
    if (i.pluginURL && str::FindChar(i.pluginURL, '#')) {
        ScopedMem<WCHAR> args(str::Dup(str::FindChar(i.pluginURL, '#') + 1));
        str::TransChars(args, L"#", L"&");
        WStrVec parts;
        parts.Split(args, L"&", true);
        for (size_t k = 0; k < parts.Count(); k++) {
            WCHAR *part = parts.At(k);
            int pageNo;
            if (str::StartsWithI(part, L"page=") && str::Parse(part + 4, L"=%d%$", &pageNo))
                i.pageNumber = pageNo;
            else if (str::StartsWithI(part, L"nameddest=") && part[10])
                str::ReplacePtr(&i.destName, part + 10);
            else if (!str::FindChar(part, '=') && part[0])
                str::ReplacePtr(&i.destName, part);
        }
    }

    return true;
}

static void GetCommandLineInfo(CommandLineInfo& i)
{
    i.bgColor = gGlobalPrefs->mainWindowBackground;
    i.forwardSearch = gGlobalPrefs->forwardSearch;
    i.escToExit = gGlobalPrefs->escToExit;
    i.cbxMangaMode = gGlobalPrefs->comicBookUI.cbxMangaMode;
    i.textColor = GetFixedPageUiTextColor();
    i.backgroundColor = GetFixedPageUiBgColor();
    i.ParseCommandLine(GetCommandLine());
}

static void SetupCrashHandler()
{
    ScopedMem<WCHAR> symDir;
    ScopedMem<WCHAR> tmpDir(path::GetTempPath());
    if (tmpDir)
        symDir.Set(path::Join(tmpDir, L"SumatraPDF-symbols"));
    else
        symDir.Set(AppGenDataFilename(L"SumatraPDF-symbols"));
    ScopedMem<WCHAR> crashDumpPath(AppGenDataFilename(CRASH_DUMP_FILE_NAME));
    InstallCrashHandler(crashDumpPath, symDir);
}

static HWND FindPrevInstWindow(HANDLE *hMutex)
{
    // create a unique identifier for this executable
    // (allows independent side-by-side installations)
    ScopedMem<WCHAR> exePath(GetExePath());
    str::ToLower(exePath);
    uint32_t hash = MurmurHash2(exePath.Get(), str::Len(exePath) * sizeof(WCHAR));
    ScopedMem<WCHAR> mapId(str::Format(L"SumatraPDF-%08x", hash));

    int retriesLeft = 3;
Retry:
    // use a memory mapping containing a process id as mutex
    HANDLE hMap = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, sizeof(DWORD), mapId);
    if (!hMap)
        goto Error;
    bool hasPrevInst = GetLastError() == ERROR_ALREADY_EXISTS;
    DWORD *procId = (DWORD *)MapViewOfFile(hMap, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(DWORD));
    if (!procId) {
        CloseHandle(hMap);
        goto Error;
    }
    if (!hasPrevInst) {
        *procId = GetCurrentProcessId();
        UnmapViewOfFile(procId);
        *hMutex = hMap;
        return NULL;
    }

    // if the mapping already exists, find one window belonging to the original process
    DWORD prevProcId = *procId;
    UnmapViewOfFile(procId);
    CloseHandle(hMap);
    HWND hwnd = NULL;
    while ((hwnd = FindWindowEx(HWND_DESKTOP, hwnd, FRAME_CLASS_NAME, NULL)) != NULL) {
        DWORD wndProcId;
        GetWindowThreadProcessId(hwnd, &wndProcId);
        if (wndProcId == prevProcId)
            return hwnd;
    }

    // fall through
Error:
    if (--retriesLeft < 0)
        return NULL;
    Sleep(100);
    goto Retry;
}

extern void RedirectDllIOToConsole();

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    int retCode = 1;    // by default it's error

#ifdef DEBUG
    // Memory leak detection (only enable _CRTDBG_LEAK_CHECK_DF for
    // regular termination so that leaks aren't checked on exceptions,
    // aborts, etc. where some clean-up might not take place)
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF);
    //_CrtSetBreakAlloc(421);
    TryLoadMemTrace();
#endif

    DisableDataExecution();
    // ensure that C functions behave consistently under all OS locales
    // (use Win32 functions where localized input or output is desired)
    setlocale(LC_ALL, "C");
    // don't show system-provided dialog boxes when accessing files on drives
    // that are not mounted (e.g. a: drive without floppy or cd rom drive
    // without a cd).
    SetErrorMode(SEM_NOOPENFILEERRORBOX | SEM_FAILCRITICALERRORS);

#if defined(DEBUG) || defined(SVN_PRE_RELEASE_VER)
    if (str::StartsWith(lpCmdLine, "/tester")) {
        extern int TesterMain(); // in Tester.cpp
        return TesterMain();
    }

    if (str::StartsWith(lpCmdLine, "/regress")) {
        extern int RegressMain(); // in Regress.cpp
        return RegressMain();
    }
#endif
#ifdef SUPPORTS_AUTO_UPDATE
    if (str::StartsWith(lpCmdLine, "-autoupdate")) {
        bool quit = AutoUpdateMain();
        if (quit)
            return 0;
    }
#endif

    srand((unsigned int)time(NULL));

    // load uiautomationcore.dll before installing crash handler (i.e. initializing
    // dbghelp.dll), so that we get function names/offsets in GetCallstack()
    uia::Initialize();
#ifdef DEBUG
    dbghelp::RememberCallstackLogs();
#endif

    SetupCrashHandler();

    ScopedOle ole;
    InitAllCommonControls();
    ScopedGdiPlus gdiPlus(true);
    mui::Initialize();
    uitask::Initialize();

    prefs::Load();

    CommandLineInfo i;
    GetCommandLineInfo(i);

    SetCurrentLang(i.lang ? i.lang : gGlobalPrefs->uiLanguage);

    // This allows ad-hoc comparison of gdi, gdi+ and gdi+ quick when used
    // in layout
#if 0
    RedirectIOToConsole();
    BenchEbookLayout(L"C:\\kjk\\downloads\\pg12.mobi");
    system("pause");
    goto Exit;
#endif

    if (i.showConsole) {
        RedirectIOToConsole();
        RedirectDllIOToConsole();
    }
    if (i.makeDefault)
        AssociateExeWithPdfExtension();
    if (i.pathsToBenchmark.Count() > 0) {
        BenchFileOrDir(i.pathsToBenchmark);
        if (i.showConsole)
            system("pause");
    }
    if (i.exitImmediately)
        goto Exit;
    gCrashOnOpen = i.crashOnOpen;

    gGlobalPrefs->mainWindowBackground = i.bgColor;
    if (gGlobalPrefs->forwardSearch.highlightColor != i.forwardSearch.highlightColor ||
        gGlobalPrefs->forwardSearch.highlightOffset != i.forwardSearch.highlightOffset ||
        gGlobalPrefs->forwardSearch.highlightPermanent != i.forwardSearch.highlightPermanent ||
        gGlobalPrefs->forwardSearch.highlightWidth != i.forwardSearch.highlightWidth) {
        gGlobalPrefs->enableTeXEnhancements = true;
    }
    gGlobalPrefs->forwardSearch = i.forwardSearch;
    gGlobalPrefs->escToExit = i.escToExit;
    gGlobalPrefs->comicBookUI.cbxMangaMode = i.cbxMangaMode;
    gPolicyRestrictions = GetPolicies(i.restrictedUse);
    gRenderCache.textColor = i.textColor;
    gRenderCache.backgroundColor = i.backgroundColor;
    DebugGdiPlusDevice(gUseGdiRenderer);

    if (i.inverseSearchCmdLine) {
        str::ReplacePtr(&gGlobalPrefs->inverseSearchCmdLine, i.inverseSearchCmdLine);
        gGlobalPrefs->enableTeXEnhancements = true;
    }

    if (!RegisterWinClass(hInstance))
        goto Exit;
    if (!InstanceInit(hInstance, nCmdShow))
        goto Exit;

    if (i.hwndPluginParent) {
        if (!SetupPluginMode(i))
            goto Exit;
    }

    if (i.printerName) {
        // note: this prints all PDF files. Another option would be to
        // print only the first one
        for (size_t n = 0; n < i.fileNames.Count(); n++) {
            bool ok = PrintFile(i.fileNames.At(n), i.printerName, !i.silent, i.printSettings);
            if (!ok)
                retCode++;
        }
        --retCode; // was 1 if no print failures, turn 1 into 0
        goto Exit;
    }

    bool showStartPage = i.fileNames.Count() == 0 && gGlobalPrefs->rememberOpenedFiles && gGlobalPrefs->showStartPage;
    if (showStartPage) {
        // make the shell prepare the image list, so that it's ready when the first window's loaded
        SHFILEINFO sfi;
        SHGetFileInfo(L".pdf", 0, &sfi, sizeof(sfi), SHGFI_SYSICONINDEX | SHGFI_SMALLICON | SHGFI_USEFILEATTRIBUTES);
    }

    if (gGlobalPrefs->reopenOnce) {
        WStrVec moreFileNames;
        ParseCmdLine(gGlobalPrefs->reopenOnce, moreFileNames);
        moreFileNames.Reverse();
        for (WCHAR **fileName = moreFileNames.IterStart(); fileName; fileName = moreFileNames.IterNext()) {
            i.fileNames.Append(*fileName);
        }
        moreFileNames.RemoveAt(0, moreFileNames.Count());
        str::ReplacePtr(&gGlobalPrefs->reopenOnce, NULL);
    }

    HANDLE hMutex = NULL;
    HWND hPrevWnd = NULL;
    if (i.printDialog || i.stressTestPath || gPluginMode) {
        // TODO: pass print request through to previous instance?
    }
    else if (i.reuseDdeInstance) {
        hPrevWnd = FindWindow(FRAME_CLASS_NAME, NULL);
    }
    else if (gGlobalPrefs->reuseInstance || gGlobalPrefs->useTabs) {
        hPrevWnd = FindPrevInstWindow(&hMutex);
    }
    if (hPrevWnd) {
        for (size_t n = 0; n < i.fileNames.Count(); n++) {
            OpenUsingDde(hPrevWnd, i.fileNames.At(n), i, 0 == n);
        }
        goto Exit;
    }

    WindowInfo *win = NULL;
    for (size_t n = 0; n < i.fileNames.Count(); n++) {
        win = LoadOnStartup(i.fileNames.At(n), i, !win);
        if (!win) {
            retCode++;
            continue;
        }
        if (i.printDialog)
            OnMenuPrint(win, i.exitWhenDone);
    }
    if (i.fileNames.Count() > 0 && !win) {
        // failed to create any window, even though there
        // were files to load (or show a failure message for)
        goto Exit;
    }
    if (i.printDialog && i.exitWhenDone)
        goto Exit;

    if (!win) {
        win = CreateAndShowWindowInfo();
        if (!win)
            goto Exit;
    }

    UpdateUITextForLanguage(); // needed for RTL languages
    if (win->IsAboutWindow()) {
        // TODO: shouldn't CreateAndShowWindowInfo take care of this?
        UpdateToolbarAndScrollbarState(*win);
    }

    // Make sure that we're still registered as default,
    // if the user has explicitly told us to be
    if (gGlobalPrefs->associatedExtensions)
        RegisterForPdfExtentions(win->hwndFrame);

    if (i.stressTestPath) {
        // don't save file history and preference changes
        gPolicyRestrictions = (gPolicyRestrictions | Perm_RestrictedUse) & ~Perm_SavePreferences;
        RebuildMenuBarForWindow(win);
        StartStressTest(&i, win, &gRenderCache);
    }

    if (gGlobalPrefs->checkForUpdates)
        AutoUpdateCheckAsync(win->hwndFrame, true);

    // only hide newly missing files when showing the start page on startup
    if (showStartPage && gFileHistory.Get(0)) {
        gFileExistenceChecker = new FileExistenceChecker();
        gFileExistenceChecker->Start();
    }
    // call this once it's clear whether Perm_SavePreferences has been granted
    prefs::RegisterForFileChanges();

    retCode = RunMessageLoop();

    SafeCloseHandle(&hMutex);
    CleanUpThumbnailCache(gFileHistory);

Exit:
    prefs::UnregisterForFileChanges();

    while (gWindows.Count() > 0) {
        DeleteWindowInfo(gWindows.At(0));
    }

#ifndef DEBUG

    // leave all the remaining clean-up to the OS
    // (as recommended for a quick exit)
    ExitProcess(retCode);

#else

    DeleteObject(gDefaultGuiFont);
    DeleteBitmap(gBitmapReloadingCue);

    // wait for FileExistenceChecker to terminate
    // (which should be necessary only very rarely)
    while (gFileExistenceChecker) {
        Sleep(10);
        uitask::DrainQueue();
    }

    mui::Destroy();
    uitask::Destroy();
    trans::Destroy();

    SaveCallstackLogs();
    dbghelp::FreeCallstackLogs();

    // must be after uitask::Destroy() because we might have queued prefs::Reload()
    // which crashes if gGlobalPrefs is freed
    gFileHistory.UpdateStatesSource(NULL);
    DeleteGlobalPrefs(gGlobalPrefs);

    // it's still possible to crash after this (destructors of static classes,
    // atexit() code etc.) point, but it's very unlikely
    UninstallCrashHandler();

    // output leaks after all destructors of static objects have run
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);

    return retCode;
#endif
}
