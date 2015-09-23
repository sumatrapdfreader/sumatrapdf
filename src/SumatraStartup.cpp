/* Copyright 2015 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// utils
#include "BaseUtil.h"
#include "WinDynCalls.h"
#include "CmdLineParser.h"
#include "DbgHelpDyn.h"
#include "Dpi.h"
#include "FileUtil.h"
#include "FileWatcher.h"
#include "HtmlParserLookup.h"
#include "LabelWithCloseWnd.h"
#include "Mui.h"
#include "SplitterWnd.h"
#include "SquareTreeParser.h"
#include "ThreadUtil.h"
#include "UITask.h"
#include "WinUtil.h"
// rendering engines
#include "BaseEngine.h"
#include "EngineManager.h"
// layout controllers
#include "SettingsStructs.h"
#include "Controller.h"
#include "DisplayModel.h"
#include "FileHistory.h"
#include "GlobalPrefs.h"
#include "PdfSync.h"
#include "RenderCache.h"
#include "TextSelection.h"
#include "TextSearch.h"
// ui
#include "SumatraPDF.h"
#include "WindowInfo.h"
#include "TabInfo.h"
#include "resource.h"
#include "AppPrefs.h"
#include "AppTools.h"
#include "Canvas.h"
#include "Caption.h"
#include "CrashHandler.h"
#include "FileThumbnails.h"
#include "Notifications.h"
#include "ParseCommandLine.h"
#include "Print.h"
#include "Search.h"
#include "Selection.h"
#include "SumatraDialogs.h"
#include "SumatraProperties.h"
#include "Tabs.h"
#include "Translations.h"
#include "uia/Provider.h"
#include "StressTesting.h"
#include "Version.h"

// "SumatraPDF yellow" similar to the one use for icon and installer
#define ABOUT_BG_LOGO_COLOR     RGB(0xFF, 0xF2, 0x00)

// it's very light gray but not white so that there's contrast between
// background and thumbnail, which often have white background because
// most PDFs have white background
#define ABOUT_BG_GRAY_COLOR     RGB(0xF2, 0xF2, 0xF2)

#define CRASH_DUMP_FILE_NAME         L"sumatrapdfcrash.dmp"

#ifdef DEBUG
static bool TryLoadMemTrace()
{
    ScopedMem<WCHAR> dllPath(path::GetAppPath(L"memtrace.dll"));
    if (!LoadLibrary(dllPath))
        return false;
    return true;
}
#endif

// gFileExistenceChecker is initialized at startup and should
// terminate and delete itself asynchronously while the UI is
// being set up
class FileExistenceChecker : public ThreadBase
{
    WStrVec paths;

    void GetFilePathsToCheck();
    void HideMissingFiles();
    void Terminate();

public:
    FileExistenceChecker() { GetFilePathsToCheck(); }
    virtual void Run() override;
};

static FileExistenceChecker *gFileExistenceChecker = nullptr;

void FileExistenceChecker::GetFilePathsToCheck()
{
    DisplayState *state;
    for (size_t i = 0; i < 2 * FILE_HISTORY_MAX_RECENT && (state = gFileHistory.Get(i)) != nullptr; i++) {
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

void FileExistenceChecker::HideMissingFiles()
{
    for (const WCHAR *path : paths) {
        gFileHistory.MarkFileInexistent(path, true);
    }
    // update the Frequently Read page in case it's been displayed already
    if (paths.Count() > 0 && gWindows.Count() > 0 && gWindows.At(0)->IsAboutWindow()) {
        gWindows.At(0)->RedrawAll(true);
    }
}

void FileExistenceChecker::Terminate()
{
    gFileExistenceChecker = nullptr;
    Join(); // just to be safe
    delete this;
}

void FileExistenceChecker::Run()
{
    // filters all file paths on network drives, removable drives and
    // all paths which still exist from the list (remaining paths will
    // be marked as inexistent in gFileHistory)
    for (size_t i = 0; i < paths.Count(); i++) {
        const WCHAR *path = paths.At(i);
        if (!path || !path::IsOnFixedDrive(path) || DocumentPathExists(path)) {
            free(paths.PopAt(i--));
        }
    }

    uitask::Post([=] {
        CrashIf(WasCancelRequested());
        HideMissingFiles();
        Terminate();
    });
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

static bool RegisterWinClass()
{
    WNDCLASSEX  wcex;
    ATOM        atom;

    FillWndClassEx(wcex, FRAME_CLASS_NAME, WndProcFrame);
    wcex.hIcon  = LoadIcon(GetModuleHandle(nullptr), MAKEINTRESOURCE(IDI_SUMATRAPDF));
    CrashIf(!wcex.hIcon);
    // For the extended translucent frame to be visible, we need black background.
    wcex.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    atom = RegisterClassEx(&wcex);
    CrashIf(!atom);

    FillWndClassEx(wcex, CANVAS_CLASS_NAME, WndProcCanvas);
    wcex.style |= CS_DBLCLKS;
    atom = RegisterClassEx(&wcex);
    CrashIf(!atom);

    FillWndClassEx(wcex, PROPERTIES_CLASS_NAME, WndProcProperties);
    wcex.hIcon = LoadIcon(GetModuleHandle(nullptr), MAKEINTRESOURCE(IDI_SUMATRAPDF));
    CrashIf(!wcex.hIcon);
    atom = RegisterClassEx(&wcex);
    CrashIf(!atom);

    RegisterNotificationsWndClass();
    RegisterSplitterWndClass();
    RegisterLabelWithCloseWnd();
    RegisterCaptionWndClass();
    return true;
}

// returns the background color for the "SumatraPDF" logo in start page and About window
COLORREF GetLogoBgColor()
{
#ifdef ABOUT_USE_LESS_COLORS
    return ABOUT_BG_LOGO_COLOR;
#else
    return GetAboutBgColor();
#endif
}

// returns the background color for start page, About window and Properties dialog
COLORREF GetAboutBgColor()
{
    COLORREF bgColor = ABOUT_BG_GRAY_COLOR;
    if (ABOUT_BG_COLOR_DEFAULT != gGlobalPrefs->mainWindowBackground)
        bgColor = gGlobalPrefs->mainWindowBackground;
    return bgColor;
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

static bool InstanceInit()
{
    gCursorDrag     = LoadCursor(GetModuleHandle(nullptr), MAKEINTRESOURCE(IDC_CURSORDRAG));
    CrashIf(!gCursorDrag);

    gBitmapReloadingCue = LoadBitmap(GetModuleHandle(nullptr), MAKEINTRESOURCE(IDB_RELOADING_CUE));
    CrashIf(!gBitmapReloadingCue);
    return true;
}

static void OpenUsingDde(HWND targetWnd, const WCHAR *filePath, CommandLineInfo& i, bool isFirstWin)
{
    // delegate file opening to a previously running instance by sending a DDE message
    WCHAR fullpath[MAX_PATH];
    GetFullPathName(filePath, dimof(fullpath), fullpath, nullptr);

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
        LRESULT res = SendMessage(targetWnd, WM_COPYDATA, 0, (LPARAM)&cds);
        if (res)
            return;
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

    if (i.enterPresentation || i.enterFullScreen) {
        if (i.enterPresentation && win->isFullScreen || i.enterFullScreen && win->presentation)
            ExitFullScreen(win);
        EnterFullScreen(win, i.enterPresentation);
    }
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

static void RestoreTabOnStartup(WindowInfo *win, TabState *state)
{
    LoadArgs args(state->filePath, win);
    if (!LoadDocument(args))
        return;
    TabInfo *tab = win->currentTab;
    if (!tab || !tab->ctrl)
        return;

    tab->tocState = *state->tocState;
    SetSidebarVisibility(win, state->showToc, gGlobalPrefs->showFavorites);

    DisplayMode displayMode = prefs::conv::ToDisplayMode(state->displayMode, DM_AUTOMATIC);
    if (displayMode != DM_AUTOMATIC)
        SwitchToDisplayMode(win, displayMode);
    // TODO: make EbookController::GoToPage not crash
    if (!tab->AsEbook())
        tab->ctrl->GoToPage(state->pageNo, true);
    float zoom = prefs::conv::ToZoom(state->zoom, INVALID_ZOOM);
    if (zoom != INVALID_ZOOM) {
        if (tab->AsFixed())
            tab->AsFixed()->Relayout(zoom, state->rotation);
        else
            tab->ctrl->SetZoomVirtual(zoom);
    }
    if (tab->AsFixed())
        tab->AsFixed()->SetScrollState(ScrollState(state->pageNo, state->scrollPos.x, state->scrollPos.y));

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
    RestrictPolicies(Perm_SavePreferences | Perm_FullscreenAccess);

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
                i.destName.Set(str::Dup(part + 10));
            else if (!str::FindChar(part, '=') && part[0])
                i.destName.Set(str::Dup(part));
        }
    }

    return true;
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
    HANDLE hMap = CreateFileMapping(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, sizeof(DWORD), mapId);
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
        return nullptr;
    }

    // if the mapping already exists, find one window belonging to the original process
    DWORD prevProcId = *procId;
    UnmapViewOfFile(procId);
    CloseHandle(hMap);
    HWND hwnd = nullptr;
    while ((hwnd = FindWindowEx(HWND_DESKTOP, hwnd, FRAME_CLASS_NAME, nullptr)) != nullptr) {
        DWORD wndProcId;
        GetWindowThreadProcessId(hwnd, &wndProcId);
        if (wndProcId == prevProcId) {
            AllowSetForegroundWindow(prevProcId);
            return hwnd;
        }
    }

    // fall through
Error:
    if (--retriesLeft < 0)
        return nullptr;
    Sleep(100);
    goto Retry;
}

extern "C" void fz_redirect_dll_io_to_console();

// Registering happens either through the Installer or the Options dialog;
// here we just make sure that we're still registered
static bool RegisterForPdfExtentions(HWND hwnd)
{
    if (IsRunningInPortableMode() || !HasPermission(Perm_RegistryAccess) || gPluginMode)
        return false;

    if (IsExeAssociatedWithPdfExtension())
        return true;

    /* Ask user for permission, unless he previously said he doesn't want to
       see this dialog */
    if (!gGlobalPrefs->associateSilently) {
        INT_PTR result = Dialog_PdfAssociate(hwnd, &gGlobalPrefs->associateSilently);
        str::ReplacePtr(&gGlobalPrefs->associatedExtensions, IDYES == result ? L".pdf" : nullptr);
    }
    // for now, .pdf is the only choice
    if (!str::EqI(gGlobalPrefs->associatedExtensions, L".pdf"))
        return false;

    AssociateExeWithPdfExtension();
    return true;
}

static int RunMessageLoop()
{
    HACCEL accTable = LoadAccelerators(GetModuleHandle(nullptr), MAKEINTRESOURCE(IDC_SUMATRAPDF));
    MSG msg = { 0 };

    while (GetMessage(&msg, nullptr, 0, 0)) {
        // dispatch the accelerator to the correct window
        WindowInfo *win = FindWindowInfoByHwnd(msg.hwnd);
        HWND accHwnd = win ? win->hwndFrame : msg.hwnd;
        if (TranslateAccelerator(accHwnd, accTable, &msg))
            continue;

        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return (int)msg.wParam;
}

#if defined(SUPPORTS_AUTO_UPDATE) || defined(DEBUG)
static bool RetryIO(const std::function<bool ()>& func, int tries=10)
{
    while (tries-- > 0) {
        if (func())
            return true;
        Sleep(200);
    }
    return false;
}

static bool AutoUpdateMain()
{
    WStrVec argList;
    ParseCmdLine(GetCommandLine(), argList, 4);
    if (argList.Count() != 3 || !str::Eq(argList.At(1), L"-autoupdate")) {
        // the argument was misinterpreted, let SumatraPDF start as usual
        return false;
    }
    if (str::Eq(argList.At(2), L"replace")) {
        // older 2.6 prerelease versions used implicit paths
        ScopedMem<WCHAR> exePath(GetExePath());
        CrashIf(!str::EndsWith(exePath, L".exe-updater.exe"));
        exePath[str::Len(exePath) - 12] = '\0';
        free(argList.At(2));
        argList.At(2) = str::Format(L"replace:%s", exePath);
    }
    const WCHAR *otherExe = nullptr;
    if (str::StartsWith(argList.At(2), L"replace:"))
        otherExe = argList.At(2) + 8;
    else if (str::StartsWith(argList.At(2), L"cleanup:"))
        otherExe = argList.At(2) + 8;
    if (!str::EndsWithI(otherExe, L".exe") || !file::Exists(otherExe)) {
        // continue startup
        return false;
    }
    RetryIO([&] { return file::Delete(otherExe); });
    if (str::StartsWith(argList.At(2), L"cleanup:")) {
        // continue startup, restoring the previous session
        return false;
    }
    ScopedMem<WCHAR> thisExe(GetExePath());
    RetryIO([&] { return CopyFile(thisExe, otherExe, FALSE) != 0; });
    // TODO: somehow indicate success or failure
    ScopedMem<WCHAR> cleanupArgs(str::Format(L"-autoupdate cleanup:\"%s\"", thisExe));
    RetryIO([&] { return LaunchFile(otherExe, cleanupArgs); });
    return true;
}
#endif

int APIENTRY WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR cmdLine, _In_ int nCmdShow)
{
    UNUSED(hPrevInstance); UNUSED(cmdLine); UNUSED(nCmdShow);
    int retCode = 1;    // by default it's error

    CrashIf(hInstance != GetInstance());

#ifdef DEBUG
    // Memory leak detection (only enable _CRTDBG_LEAK_CHECK_DF for
    // regular termination so that leaks aren't checked on exceptions,
    // aborts, etc. where some clean-up might not take place)
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF);
    //_CrtSetBreakAlloc(421);
    TryLoadMemTrace();
#endif

    InitDynCalls();

    DisableDataExecution();
    // ensure that C functions behave consistently under all OS locales
    // (use Win32 functions where localized input or output is desired)
    setlocale(LC_ALL, "C");
    // don't show system-provided dialog boxes when accessing files on drives
    // that are not mounted (e.g. a: drive without floppy or cd rom drive
    // without a cd).
    SetErrorMode(SEM_NOOPENFILEERRORBOX | SEM_FAILCRITICALERRORS);

#if defined(DEBUG) || defined(SVN_PRE_RELEASE_VER)
    if (str::StartsWith(cmdLine, "/tester")) {
        extern int TesterMain(); // in Tester.cpp
        return TesterMain();
    }

    if (str::StartsWith(cmdLine, "/regress")) {
        extern int RegressMain(); // in Regress.cpp
        return RegressMain();
    }
#endif
#if defined(SUPPORTS_AUTO_UPDATE) || defined(DEBUG)
    if (str::StartsWith(cmdLine, "-autoupdate")) {
        bool quit = AutoUpdateMain();
        if (quit)
            return 0;
    }
#endif

    srand((unsigned int)time(nullptr));

#ifdef DEBUG
    dbghelp::RememberCallstackLogs();
#endif

    SetupCrashHandler();

    ScopedOle ole;
    InitAllCommonControls();
    ScopedGdiPlus gdiPlus(true);
    mui::Initialize();
    uitask::Initialize();

    CommandLineInfo i;
    i.ParseCommandLine(GetCommandLine());
    InitializePolicies(i.restrictedUse);
    if (i.appdataDir)
        SetAppDataPath(i.appdataDir);

    prefs::Load();
    i.UpdateGlobalPrefs();
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
        fz_redirect_dll_io_to_console();
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

    GetFixedPageUiColors(gRenderCache.textColor, gRenderCache.backgroundColor);

    if (!RegisterWinClass())
        goto Exit;

    CrashIf(hInstance != GetModuleHandle(nullptr));
    if (!InstanceInit())
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

    HANDLE hMutex = nullptr;
    HWND hPrevWnd = nullptr;
    if (i.printDialog || i.stressTestPath || gPluginMode) {
        // TODO: pass print request through to previous instance?
    }
    else if (i.reuseDdeInstance) {
        hPrevWnd = FindWindow(FRAME_CLASS_NAME, nullptr);
    }
    else if (gGlobalPrefs->reuseInstance || gGlobalPrefs->useTabs) {
        hPrevWnd = FindPrevInstWindow(&hMutex);
    }
    if (hPrevWnd) {
        for (size_t n = 0; n < i.fileNames.Count(); n++) {
            OpenUsingDde(hPrevWnd, i.fileNames.At(n), i, 0 == n);
        }
        if (0 == i.fileNames.Count()) {
            win::ToForeground(hPrevWnd);
        }
        goto Exit;
    }

    bool restoreSession = false;
    if (gGlobalPrefs->sessionData->Count() > 0 && !gPluginURL) {
        restoreSession = str::EqI(gGlobalPrefs->restoreSession, "yes") ||
                         str::EqI(gGlobalPrefs->restoreSession, "true") ||
#ifndef DISABLE_SESSION_RESTORE
                         str::EqI(gGlobalPrefs->restoreSession, "auto");
#else
                         false;
#endif
    }
    if (gGlobalPrefs->reopenOnce->Count() > 0 && !gPluginURL) {
        if (gGlobalPrefs->reopenOnce->Count() == 1 && str::EqI(gGlobalPrefs->reopenOnce->At(0), L"SessionData")) {
            gGlobalPrefs->reopenOnce->FreeMembers();
            restoreSession = true;
        }
        while (gGlobalPrefs->reopenOnce->Count() > 0) {
            i.fileNames.Append(gGlobalPrefs->reopenOnce->Pop());
        }
    }

    bool showStartPage = !restoreSession && i.fileNames.Count() == 0 &&
                         gGlobalPrefs->rememberOpenedFiles && gGlobalPrefs->showStartPage;
    if (showStartPage) {
        // make the shell prepare the image list, so that it's ready when the first window's loaded
        SHFILEINFO sfi;
        SHGetFileInfo(L".pdf", 0, &sfi, sizeof(sfi), SHGFI_SYSICONINDEX | SHGFI_SMALLICON | SHGFI_USEFILEATTRIBUTES);
    }

    WindowInfo *win = nullptr;
    if (restoreSession) {
        for (SessionData *data : *gGlobalPrefs->sessionData) {
            win = CreateAndShowWindowInfo(data);
            for (TabState *state : *data->tabStates) {
                RestoreTabOnStartup(win, state);
            }
            TabsSelect(win, data->tabIndex - 1);
        }
    }
    ResetSessionState(gGlobalPrefs->sessionData);

    for (const WCHAR *filePath : i.fileNames) {
        if (restoreSession && FindWindowInfoByFile(filePath, true))
            continue;
        win = LoadOnStartup(filePath, i, !win);
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

    // Make sure that we're still registered as default,
    // if the user has explicitly told us to be
    if (gGlobalPrefs->associatedExtensions)
        RegisterForPdfExtentions(win->hwndFrame);

    if (i.stressTestPath) {
        // don't save file history and preference changes
        RestrictPolicies(Perm_SavePreferences);
        RebuildMenuBarForWindow(win);
        StartStressTest(&i, win);
    }

    if (gGlobalPrefs->checkForUpdates)
        UpdateCheckAsync(win, true);

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

    DeleteCachedCursors();
#ifndef DEBUG

    // leave all the remaining clean-up to the OS
    // (as recommended for a quick exit)
    ExitProcess(retCode);

#else

    DeleteObject(GetDefaultGuiFont());
    DeleteBitmap(gBitmapReloadingCue);
    DeleteSplitterBrush();

    // wait for FileExistenceChecker to terminate
    // (which should be necessary only very rarely)
    while (gFileExistenceChecker) {
        Sleep(10);
        uitask::DrainQueue();
    }

    mui::Destroy();
    uitask::Destroy();
    trans::Destroy();
    DpiRemoveAll();

    FileWatcherWaitForShutdown();

    SaveCallstackLogs();
    dbghelp::FreeCallstackLogs();

    // must be after uitask::Destroy() because we might have queued prefs::Reload()
    // which crashes if gGlobalPrefs is freed
    gFileHistory.UpdateStatesSource(nullptr);
    prefs::CleanUp();

    // it's still possible to crash after this (destructors of static classes,
    // atexit() code etc.) point, but it's very unlikely
    UninstallCrashHandler();

    // output leaks after all destructors of static objects have run
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);

    return retCode;
#endif
}
