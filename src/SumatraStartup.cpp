// TODO: for the moment it needs to be included from SumatraPDF.cpp
// and not compiled as stand-alone

static bool TryLoadMemTrace()
{
    ScopedMem<TCHAR> exePath(GetExePath());
    ScopedMem<TCHAR> exeDir(path::GetDir(exePath));
    ScopedMem<TCHAR> dllPath(path::Join(exeDir, _T("memtrace.dll")));
    if (!LoadLibrary(dllPath))
        return false;
    return true;
}

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

    FillWndClassEx(wcex, hinst);
    wcex.lpfnWndProc    = WndProcFrame;
    wcex.lpszClassName  = FRAME_CLASS_NAME;
    wcex.hIcon          = LoadIcon(hinst, MAKEINTRESOURCE(IDI_SUMATRAPDF));
    wcex.hIconSm        = LoadIcon(hinst, MAKEINTRESOURCE(IDI_SMALL));
    atom = RegisterClassEx(&wcex);
    if (!atom)
        return false;

    FillWndClassEx(wcex, hinst);
    wcex.style          = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    wcex.lpfnWndProc    = WndProcCanvas;
    wcex.hCursor        = LoadCursor(NULL, IDC_ARROW);
    wcex.lpszClassName  = CANVAS_CLASS_NAME;
    atom = RegisterClassEx(&wcex);
    if (!atom)
        return false;

    FillWndClassEx(wcex, hinst);
    wcex.lpfnWndProc    = WndProcAbout;
    wcex.hIcon          = LoadIcon(hinst, MAKEINTRESOURCE(IDI_SUMATRAPDF));
    wcex.lpszClassName  = ABOUT_CLASS_NAME;
    atom = RegisterClassEx(&wcex);
    if (!atom)
        return false;

    FillWndClassEx(wcex, hinst);
    wcex.lpfnWndProc    = WndProcProperties;
    wcex.hIcon          = LoadIcon(hinst, MAKEINTRESOURCE(IDI_SUMATRAPDF));
    wcex.lpszClassName  = PROPERTIES_CLASS_NAME;
    atom = RegisterClassEx(&wcex);
    if (!atom)
        return false;

    FillWndClassEx(wcex, hinst);
    wcex.lpfnWndProc    = WndProcSidebarSplitter;
    wcex.hCursor        = LoadCursor(NULL, IDC_SIZEWE);
    wcex.hbrBackground  = (HBRUSH)(COLOR_BTNFACE + 1);
    wcex.lpszClassName  = SIDEBAR_SPLITTER_CLASS_NAME;
    atom = RegisterClassEx(&wcex);
    if (!atom)
        return false;

    FillWndClassEx(wcex, hinst);
    wcex.lpfnWndProc    = WndProcFavSplitter;
    wcex.hCursor        = LoadCursor(NULL, IDC_SIZENS);
    wcex.hbrBackground  = (HBRUSH)(COLOR_BTNFACE + 1);
    wcex.lpszClassName  = FAV_SPLITTER_CLASS_NAME;
    atom = RegisterClassEx(&wcex);
    if (!atom)
        return false;

    if (!RegisterNotificationsWndClass(hinst))
        return false;

    if (!RegisterMobiWinClass(hinst))
        return false;

    return true;
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
    // use the system background color if the user has non-default
    // colors for text (not black-on-white) and also wants to use them
    bool useSysColor = gGlobalPrefs.useSysColors &&
                       (GetSysColor(COLOR_WINDOWTEXT) != WIN_COL_BLACK ||
                        GetSysColor(COLOR_WINDOW) != WIN_COL_WHITE);
    if (useSysColor) {
        // not using GetSysColorBrush so that gBrushNoDocBg can be deleted
        gBrushNoDocBg = CreateSolidBrush(GetSysColor(COLOR_BTNFACE));
    }
    else
        gBrushNoDocBg = CreateSolidBrush(COL_WINDOW_BG);
    if (ABOUT_BG_COLOR_DEFAULT != gGlobalPrefs.bgColor)
        gBrushAboutBg = CreateSolidBrush(gGlobalPrefs.bgColor);
    else
        gBrushAboutBg = CreateSolidBrush(ABOUT_BG_COLOR);

    NONCLIENTMETRICS ncm = { 0 };
    ncm.cbSize = sizeof(ncm);
    SystemParametersInfo(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0);
    gDefaultGuiFont = CreateFontIndirect(&ncm.lfMessageFont);
    gBitmapReloadingCue = LoadBitmap(ghinst, MAKEINTRESOURCE(IDB_RELOADING_CUE));

    return true;
}

static void OpenUsingDde(CommandLineInfo& i, int n, bool firstIsDocLoaded)
{
    // delegate file opening to a previously running instance by sending a DDE message
    TCHAR fullpath[MAX_PATH];
    GetFullPathName(i.fileNames.At(n), dimof(fullpath), fullpath, NULL);

    ScopedMem<TCHAR> cmd(str::Format(_T("[") DDECOMMAND_OPEN _T("(\"%s\", 0, 1, 0)]"), fullpath));
    DDEExecute(PDFSYNC_DDE_SERVICE, PDFSYNC_DDE_TOPIC, cmd);
    if (i.destName && !firstIsDocLoaded) {
        cmd.Set(str::Format(_T("[") DDECOMMAND_GOTO _T("(\"%s\", \"%s\")]"), fullpath, i.destName));
        DDEExecute(PDFSYNC_DDE_SERVICE, PDFSYNC_DDE_TOPIC, cmd);
    }
    else if (i.pageNumber > 0 && !firstIsDocLoaded) {
        cmd.Set(str::Format(_T("[") DDECOMMAND_PAGE _T("(\"%s\", %d)]"), fullpath, i.pageNumber));
        DDEExecute(PDFSYNC_DDE_SERVICE, PDFSYNC_DDE_TOPIC, cmd);
    }
    if ((i.startView != DM_AUTOMATIC || i.startZoom != INVALID_ZOOM ||
            i.startScroll.x != -1 && i.startScroll.y != -1) && !firstIsDocLoaded) {
        const TCHAR *viewMode = DisplayModeConv::NameFromEnum(i.startView);
        cmd.Set(str::Format(_T("[") DDECOMMAND_SETVIEW _T("(\"%s\", \"%s\", %.2f, %d, %d)]"),
                                    fullpath, viewMode, i.startZoom, i.startScroll.x, i.startScroll.y));
        DDEExecute(PDFSYNC_DDE_SERVICE, PDFSYNC_DDE_TOPIC, cmd);
    }
    if (i.forwardSearchOrigin && i.forwardSearchLine) {
        cmd.Set(str::Format(_T("[") DDECOMMAND_SYNC _T("(\"%s\", \"%s\", %d, 0, 0, 1)]"),
                                    i.fileNames.At(n), i.forwardSearchOrigin, i.forwardSearchLine));
        DDEExecute(PDFSYNC_DDE_SERVICE, PDFSYNC_DDE_TOPIC, cmd);
    }
}

static bool LoadOnStartup(CommandLineInfo& i, int n, bool firstIsDocLoaded)
{
    bool showWin = !(i.printDialog && i.exitOnPrint) && !gPluginMode;
    WindowInfo *win = LoadDocument(i.fileNames.At(n), NULL, showWin);
    if (!win || !win->IsDocLoaded())
        return false;

    if (win->IsDocLoaded() && i.destName && !firstIsDocLoaded) {
        win->linkHandler->GotoNamedDest(i.destName);
    } else if (win->IsDocLoaded() && i.pageNumber > 0 && !firstIsDocLoaded) {
        if (win->dm->ValidPageNo(i.pageNumber))
            win->dm->GoToPage(i.pageNumber, 0);
    }
    if (i.hwndPluginParent)
        MakePluginWindow(*win, i.hwndPluginParent);
    if (!(win->IsDocLoaded() && !firstIsDocLoaded))
        return true;

    if (i.enterPresentation || i.enterFullscreen)
        EnterFullscreen(*win, i.enterPresentation);
    if (i.startView != DM_AUTOMATIC)
        SwitchToDisplayMode(win, i.startView);
    if (i.startZoom != INVALID_ZOOM)
        ZoomToSelection(win, i.startZoom);
    if (i.startScroll.x != -1 || i.startScroll.y != -1) {
        ScrollState ss = win->dm->GetScrollState();
        ss.x = i.startScroll.x;
        ss.y = i.startScroll.y;
        win->dm->SetScrollState(ss);
    }
    if (i.forwardSearchOrigin && i.forwardSearchLine && win->pdfsync) {
        UINT page;
        Vec<RectI> rects;
        int ret = win->pdfsync->SourceToDoc(i.forwardSearchOrigin, i.forwardSearchLine, 0, &page, rects);
        ShowForwardSearchResult(win, i.forwardSearchOrigin, i.forwardSearchLine, 0, ret, page, rects);
    }
    return true;
}

static void SetupPluginMode(CommandLineInfo& i)
{
    gPluginURL = i.pluginURL;
    if (!gPluginURL)
        gPluginURL = i.fileNames.At(0);

    assert(i.fileNames.Count() == 1);
    while (i.fileNames.Count() > 1) {
        free(i.fileNames.Pop());
    }
    i.reuseInstance = i.exitOnPrint = false;
    // always display the toolbar when embedded (as there's no menubar in that case)
    gGlobalPrefs.toolbarVisible = true;
    // never allow esc as a shortcut to quit
    gGlobalPrefs.escToExit = false;
    // never show the sidebar by default
    gGlobalPrefs.tocVisible = false;
    if (DM_AUTOMATIC == gGlobalPrefs.defaultDisplayMode) {
        // if the user hasn't changed the default display mode,
        // display documents as single page/continuous/fit width
        // (similar to Adobe Reader, Google Chrome and how browsers display HTML)
        gGlobalPrefs.defaultDisplayMode = DM_CONTINUOUS;
        gGlobalPrefs.defaultZoom = ZOOM_FIT_WIDTH;
    }
}

static void RunUnitTests()
{
#ifdef DEBUG
    extern void BaseUtils_UnitTests();
    BaseUtils_UnitTests();
    extern void HtmlPullParser_UnitTests();
    HtmlPullParser_UnitTests();
    extern void TrivialHtmlParser_UnitTests();
    TrivialHtmlParser_UnitTests();
    extern void SumatraPDF_UnitTests();
    SumatraPDF_UnitTests();
#endif
}

static void GetCommandLineInfo(CommandLineInfo& i)
{
    i.bgColor = gGlobalPrefs.bgColor;
    i.fwdSearch.offset = gGlobalPrefs.fwdSearch.offset;
    i.fwdSearch.width = gGlobalPrefs.fwdSearch.width;
    i.fwdSearch.color = gGlobalPrefs.fwdSearch.color;
    i.fwdSearch.permanent = gGlobalPrefs.fwdSearch.permanent;
    i.escToExit = gGlobalPrefs.escToExit;
    if (gGlobalPrefs.useSysColors) {
        i.colorRange[0] = GetSysColor(COLOR_WINDOWTEXT);
        i.colorRange[1] = GetSysColor(COLOR_WINDOW);
    }
    i.ParseCommandLine(GetCommandLine());
}

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

    EnableNx();
    // ensure that C functions behave consistently under all OS locales
    // (use Win32 functions where localized input or output is desired)
    setlocale(LC_ALL, "C");

    RunUnitTests();

    // don't show system-provided dialog boxes when accessing files on drives
    // that are not mounted (e.g. a: drive without floppy or cd rom drive
    // without a cd).
    SetErrorMode(SEM_NOOPENFILEERRORBOX | SEM_FAILCRITICALERRORS);
    srand((unsigned int)time(NULL));

    ScopedMem<TCHAR> crashDumpPath(AppGenDataFilename(CRASH_DUMP_FILE_NAME));
    InstallCrashHandler(crashDumpPath);

    ScopedOle ole;
    InitAllCommonControls();
    ScopedGdiPlus gdiPlus(true);
    mui::Initialize();
    uimsg::Initialize();

    ScopedMem<TCHAR> prefsFilename(GetPrefsFileName());
    if (!file::Exists(prefsFilename)) {
        // guess the ui language on first start
        CurrLangNameSet(Trans::GuessLanguage());
        gFavorites = new Favorites();
    } else {
        assert(gFavorites == NULL);
        Prefs::Load(prefsFilename, gGlobalPrefs, gFileHistory, &gFavorites);
        CurrLangNameSet(gGlobalPrefs.currentLanguage);
    }
    prefsFilename.Set(NULL);

    CommandLineInfo i;
    GetCommandLineInfo(i);

    if (i.showConsole)
        RedirectIOToConsole();
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

    gGlobalPrefs.bgColor = i.bgColor;
    gGlobalPrefs.fwdSearch.offset = i.fwdSearch.offset;
    gGlobalPrefs.fwdSearch.width = i.fwdSearch.width;
    gGlobalPrefs.fwdSearch.color = i.fwdSearch.color;
    gGlobalPrefs.fwdSearch.permanent = i.fwdSearch.permanent;
    gGlobalPrefs.escToExit = i.escToExit;
    gPolicyRestrictions = GetPolicies(i.restrictedUse);
    gRenderCache.colorRange[0] = i.colorRange[0];
    gRenderCache.colorRange[1] = i.colorRange[1];
    DebugGdiPlusDevice(gUseGdiRenderer);

    if (i.inverseSearchCmdLine) {
        str::ReplacePtr(&gGlobalPrefs.inverseSearchCmdLine, i.inverseSearchCmdLine);
        gGlobalPrefs.enableTeXEnhancements = true;
    }
    CurrLangNameSet(i.lang);

    if (!RegisterWinClass(hInstance))
        goto Exit;
    if (!InstanceInit(hInstance, nCmdShow))
        goto Exit;

    if (i.hwndPluginParent) {
        if (!IsWindow(i.hwndPluginParent) || i.fileNames.Count() == 0)
            goto Exit;
        SetupPluginMode(i);
    }

    WindowInfo *win = NULL;
    bool firstIsDocLoaded = false;
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

    if (i.fileNames.Count() == 0 && gGlobalPrefs.rememberOpenedFiles && gGlobalPrefs.showStartPage) {
        // make the shell prepare the image list, so that it's ready when the first window's loaded
        SHFILEINFO sfi;
        SHGetFileInfo(_T(".pdf"), 0, &sfi, sizeof(sfi), SHGFI_SYSICONINDEX | SHGFI_SMALLICON | SHGFI_USEFILEATTRIBUTES);
    }

    for (size_t n = 0; n < i.fileNames.Count(); n++) {
        if (i.reuseInstance && !i.printDialog) {
            OpenUsingDde(i, n, firstIsDocLoaded);
        } else {
            if (!LoadOnStartup(i, n, firstIsDocLoaded))
                goto Exit;
        }

        if (i.printDialog)
            OnMenuPrint(win, i.exitOnPrint);
        firstIsDocLoaded = true;
    }

    if (i.reuseInstance || i.printDialog && i.exitOnPrint)
        goto Exit;

    if (!firstIsDocLoaded) {
        win = CreateAndShowWindowInfo();
        if (!win)
            goto Exit;
    }

    UpdateUITextForLanguage(); // needed for RTL languages
    if (!firstIsDocLoaded)
        UpdateToolbarAndScrollbarState(*win);

    // Make sure that we're still registered as default,
    // if the user has explicitly told us to be
    if (gGlobalPrefs.pdfAssociateShouldAssociate && win)
        RegisterForPdfExtentions(win->hwndFrame);

    if (gGlobalPrefs.enableAutoUpdate && gWindows.Count() > 0)
        AutoUpdateCheckAsync(gWindows.At(0)->hwndFrame, true);

#ifndef THREAD_BASED_FILEWATCH
    const UINT_PTR timerID = SetTimer(NULL, -1, FILEWATCH_DELAY_IN_MS, NULL);
#endif

    if (i.stressTestPath) {
        gIsStressTesting = true;
        StartStressTest(win, i.stressTestPath, i.stressTestFilter,
                        i.stressTestRanges, i.stressTestCycles, &gRenderCache);
    }

    retCode = RunMessageLoop();

#ifndef THREAD_BASED_FILEWATCH
    KillTimer(NULL, timerID);
#endif

    CleanUpThumbnailCache(gFileHistory);

Exit:
    while (gWindows.Count() > 0) {
        DeleteWindowInfo(gWindows.At(0));
    }
    while (gMobiWindows.Count() > 0) {
        DeleteMobiWindow(gMobiWindows.At(0), true);
    }

#ifndef DEBUG
    // leave all the remaining clean-up to the OS
    // (as recommended for a quick exit)
    ExitProcess(retCode);
#endif

    DeleteObject(gBrushNoDocBg);
    DeleteObject(gBrushAboutBg);
    DeleteObject(gDefaultGuiFont);
    DeleteBitmap(gBitmapReloadingCue);

    delete gFavorites;

    mui::Destroy();

    DrainUiMsgQueue();
    uimsg::Destroy();

    // it's still possible to crash after this (destructors of static classes,
    // atexit() code etc.) point, but it's very unlikely
    UninstallCrashHandler();

#ifdef DEBUG
    // output leaks after all destructors of static objects have run
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

    return retCode;
}
