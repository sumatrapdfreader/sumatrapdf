/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "utils/BaseUtil.h"
#include "utils/ScopedWin.h"
#include "utils/WinDynCalls.h"
#include "utils/CmdLineParser.h"
#include "utils/DbgHelpDyn.h"
#include "utils/Dpi.h"
#include "utils/FileUtil.h"
#include "utils/FileWatcher.h"
#include "utils/HtmlParserLookup.h"
#include "mui/Mui.h"
#include "utils/SquareTreeParser.h"
#include "utils/ThreadUtil.h"
#include "utils/UITask.h"
#include "utils/WinUtil.h"
#include "utils/Archive.h"
#include "utils/LzmaSimpleArchive.h"
#include "utils/Log.h"

#include "SumatraConfig.h"

#include "wingui/WinGui.h"
#include "wingui/Layout.h"
#include "wingui/Window.h"
//#include "wingui/TooltipCtrl.h"
#include "wingui/SplitterWnd.h"
#include "wingui/LabelWithCloseWnd.h"

#include "wingui/TreeModel.h"
#include "EngineBase.h"
#include "EngineManager.h"
#include "SettingsStructs.h"
#include "Controller.h"
#include "DisplayModel.h"
#include "FileHistory.h"
#include "GlobalPrefs.h"
#include "PdfSync.h"
#include "RenderCache.h"
#include "ProgressUpdateUI.h"
#include "TextSelection.h"
#include "TextSearch.h"
#include "Notifications.h"
#include "SumatraPDF.h"
#include "WindowInfo.h"
#include "TabInfo.h"
#include "resource.h"
#include "Flags.h"
#include "AppPrefs.h"
#include "AppTools.h"
#include "Canvas.h"
#include "Caption.h"
#include "CrashHandler.h"
#include "FileThumbnails.h"
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
#include "Tests.h"
#include "Menu.h"
#include "AppTools.h"
#include "Installer.h"
#include "SumatraConfig.h"

static bool TryLoadMemTrace() {
    AutoFreeWstr dllPath(path::GetPathOfFileInAppDir(L"memtrace.dll"));
    if (!LoadLibrary(dllPath))
        return false;
    return true;
}

// gFileExistenceChecker is initialized at startup and should
// terminate and delete itself asynchronously while the UI is
// being set up
class FileExistenceChecker : public ThreadBase {
    WStrVec paths;

    void GetFilePathsToCheck();
    void HideMissingFiles();
    void Terminate();

  public:
    FileExistenceChecker() {
        GetFilePathsToCheck();
    }
    void Run() override;
};

static FileExistenceChecker* gFileExistenceChecker = nullptr;

void FileExistenceChecker::GetFilePathsToCheck() {
    DisplayState* state;
    for (size_t i = 0; i < 2 * FILE_HISTORY_MAX_RECENT && (state = gFileHistory.Get(i)) != nullptr; i++) {
        if (!state->isMissing)
            paths.Append(str::Dup(state->filePath));
    }
    // add missing paths from the list of most frequently opened documents
    Vec<DisplayState*> frequencyList;
    gFileHistory.GetFrequencyOrder(frequencyList);
    size_t iMax = std::min<size_t>(2 * FILE_HISTORY_MAX_FREQUENT, frequencyList.size());
    for (size_t i = 0; i < iMax; i++) {
        state = frequencyList.at(i);
        if (!paths.Contains(state->filePath))
            paths.Append(str::Dup(state->filePath));
    }
}

void FileExistenceChecker::HideMissingFiles() {
    for (const WCHAR* path : paths) {
        gFileHistory.MarkFileInexistent(path, true);
    }
    // update the Frequently Read page in case it's been displayed already
    if (paths.size() > 0 && gWindows.size() > 0 && gWindows.at(0)->IsAboutWindow()) {
        gWindows.at(0)->RedrawAll(true);
    }
}

void FileExistenceChecker::Terminate() {
    gFileExistenceChecker = nullptr;
    Join(); // just to be safe
    delete this;
}

void FileExistenceChecker::Run() {
    // filters all file paths on network drives, removable drives and
    // all paths which still exist from the list (remaining paths will
    // be marked as inexistent in gFileHistory)
    for (size_t i = 0; i < paths.size(); i++) {
        const WCHAR* path = paths.at(i);
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

static void MakePluginWindow(WindowInfo* win, HWND hwndParent) {
    CrashIf(!IsWindow(hwndParent));
    CrashIf(!gPluginMode);

    auto hwndFrame = win->hwndFrame;
    long ws = GetWindowLong(hwndFrame, GWL_STYLE);
    ws &= ~(WS_POPUP | WS_BORDER | WS_CAPTION | WS_THICKFRAME);
    ws |= WS_CHILD;
    SetWindowLong(hwndFrame, GWL_STYLE, ws);

    SetParent(hwndFrame, hwndParent);
    MoveWindow(hwndFrame, ClientRect(hwndParent));
    ShowWindow(hwndFrame, SW_SHOW);
    UpdateWindow(hwndFrame);

    // from here on, we depend on the plugin's host to resize us
    SetFocus(hwndFrame);
}

static bool RegisterWinClass() {
    WNDCLASSEX wcex;
    ATOM atom;

    HMODULE h = GetModuleHandleW(nullptr);
    LPCWSTR iconName = MAKEINTRESOURCEW(getAppIconID());
    FillWndClassEx(wcex, FRAME_CLASS_NAME, WndProcFrame);
    wcex.hIcon = LoadIconW(h, iconName);
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
    wcex.hIcon = LoadIconW(h, iconName);
    CrashIf(!wcex.hIcon);
    atom = RegisterClassEx(&wcex);
    CrashIf(!atom);

    RegisterCaptionWndClass();
    return true;
}

static bool InstanceInit() {
    gCursorDrag = LoadCursor(GetModuleHandle(nullptr), MAKEINTRESOURCE(IDC_CURSORDRAG));
    CrashIf(!gCursorDrag);

    gBitmapReloadingCue = LoadBitmap(GetModuleHandle(nullptr), MAKEINTRESOURCE(IDB_RELOADING_CUE));
    CrashIf(!gBitmapReloadingCue);
    return true;
}

static void OpenUsingDde(HWND targetWnd, const WCHAR* filePath, Flags& i, bool isFirstWin) {
    // delegate file opening to a previously running instance by sending a DDE message
    WCHAR fullpath[MAX_PATH];
    GetFullPathName(filePath, dimof(fullpath), fullpath, nullptr);

    str::WStr cmd;
    cmd.AppendFmt(L"[Open(\"%s\", 0, 1, 0)]", fullpath);
    if (i.destName && isFirstWin) {
        cmd.AppendFmt(L"[GotoNamedDest(\"%s\", \"%s\")]", fullpath, i.destName);
    } else if (i.pageNumber > 0 && isFirstWin) {
        cmd.AppendFmt(L"[GotoPage(\"%s\", %d)]", fullpath, i.pageNumber);
    }
    if ((i.startView != DM_AUTOMATIC || i.startZoom != INVALID_ZOOM ||
         i.startScroll.x != -1 && i.startScroll.y != -1) &&
        isFirstWin) {
        const WCHAR* viewMode = prefs::conv::FromDisplayMode(i.startView);
        cmd.AppendFmt(L"[" DDECOMMAND_SETVIEW L"(\"%s\", \"%s\", %.2f, %d, %d)]", fullpath, viewMode, i.startZoom,
                      i.startScroll.x, i.startScroll.y);
    }
    if (i.forwardSearchOrigin && i.forwardSearchLine) {
        AutoFreeWstr sourcePath(path::Normalize(i.forwardSearchOrigin));
        cmd.AppendFmt(L"[" DDECOMMAND_SYNC L"(\"%s\", \"%s\", %d, 0, 0, 1)]", fullpath, sourcePath.get(),
                      i.forwardSearchLine);
    }

    if (!i.reuseDdeInstance) {
        // try WM_COPYDATA first, as that allows targetting a specific window
        COPYDATASTRUCT cds = {0x44646557 /* DdeW */, (DWORD)(cmd.size() + 1) * sizeof(WCHAR), cmd.Get()};
        LRESULT res = SendMessage(targetWnd, WM_COPYDATA, 0, (LPARAM)&cds);
        if (res) {
            return;
        }
    }
    DDEExecute(PDFSYNC_DDE_SERVICE, PDFSYNC_DDE_TOPIC, cmd.Get());
}

static WindowInfo* LoadOnStartup(const WCHAR* filePath, const Flags& i, bool isFirstWin) {
    LoadArgs args(filePath, nullptr);
    args.showWin = !(i.printDialog && i.exitWhenDone) && !gPluginMode;
    WindowInfo* win = LoadDocument(args);
    if (!win) {
        return win;
    }

    if (win->IsDocLoaded() && i.destName && isFirstWin) {
        win->linkHandler->GotoNamedDest(i.destName);
    } else if (win->IsDocLoaded() && i.pageNumber > 0 && isFirstWin) {
        if (win->ctrl->ValidPageNo(i.pageNumber)) {
            win->ctrl->GoToPage(i.pageNumber, false);
        }
    }
    if (i.hwndPluginParent) {
        MakePluginWindow(win, i.hwndPluginParent);
    }
    if (!win->IsDocLoaded() || !isFirstWin) {
        return win;
    }

    if (i.enterPresentation || i.enterFullScreen) {
        if (i.enterPresentation && win->isFullScreen || i.enterFullScreen && win->presentation) {
            ExitFullScreen(win);
        }
        EnterFullScreen(win, i.enterPresentation);
    }
    if (i.startView != DM_AUTOMATIC) {
        SwitchToDisplayMode(win, i.startView);
    }
    if (i.startZoom != INVALID_ZOOM) {
        ZoomToSelection(win, i.startZoom);
    }
    if ((i.startScroll.x != -1 || i.startScroll.y != -1) && win->AsFixed()) {
        DisplayModel* dm = win->AsFixed();
        ScrollState ss = dm->GetScrollState();
        ss.x = i.startScroll.x;
        ss.y = i.startScroll.y;
        dm->SetScrollState(ss);
    }
    if (i.forwardSearchOrigin && i.forwardSearchLine && win->AsFixed() && win->AsFixed()->pdfSync) {
        UINT page;
        Vec<RectI> rects;
        AutoFreeWstr sourcePath(path::Normalize(i.forwardSearchOrigin));
        int ret = win->AsFixed()->pdfSync->SourceToDoc(sourcePath, i.forwardSearchLine, 0, &page, rects);
        ShowForwardSearchResult(win, sourcePath, i.forwardSearchLine, 0, ret, page, rects);
    }
    return win;
}

static void RestoreTabOnStartup(WindowInfo* win, TabState* state) {
    LoadArgs args(state->filePath, win);
    if (!LoadDocument(args)) {
        return;
    }
    TabInfo* tab = win->currentTab;
    if (!tab || !tab->ctrl) {
        return;
    }

    tab->tocState = *state->tocState;
    SetSidebarVisibility(win, state->showToc, gGlobalPrefs->showFavorites);

    DisplayMode displayMode = prefs::conv::ToDisplayMode(state->displayMode, DM_AUTOMATIC);
    if (displayMode != DM_AUTOMATIC) {
        SwitchToDisplayMode(win, displayMode);
    }
    // TODO: make EbookController::GoToPage not crash
    if (!tab->AsEbook()) {
        tab->ctrl->GoToPage(state->pageNo, true);
    }
    float zoom = prefs::conv::ToZoom(state->zoom, INVALID_ZOOM);
    if (zoom != INVALID_ZOOM) {
        if (tab->AsFixed()) {
            tab->AsFixed()->Relayout(zoom, state->rotation);
        } else {
            tab->ctrl->SetZoomVirtual(zoom, nullptr);
        }
    }
    if (tab->AsFixed()) {
        tab->AsFixed()->SetScrollState(ScrollState(state->pageNo, state->scrollPos.x, state->scrollPos.y));
    }
}

static bool SetupPluginMode(Flags& i) {
    if (!IsWindow(i.hwndPluginParent) || i.fileNames.size() == 0) {
        return false;
    }

    gPluginURL = i.pluginURL;
    if (!gPluginURL) {
        gPluginURL = i.fileNames.at(0);
    }

    AssertCrash(i.fileNames.size() == 1);
    while (i.fileNames.size() > 1) {
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
        AutoFreeWstr args(str::Dup(str::FindChar(i.pluginURL, '#') + 1));
        str::TransChars(args, L"#", L"&");
        WStrVec parts;
        parts.Split(args, L"&", true);
        for (size_t k = 0; k < parts.size(); k++) {
            WCHAR* part = parts.at(k);
            int pageNo;
            if (str::StartsWithI(part, L"page=") && str::Parse(part + 4, L"=%d%$", &pageNo))
                i.pageNumber = pageNo;
            else if (str::StartsWithI(part, L"nameddest=") && part[10])
                i.destName = str::Dup(part + 10);
            else if (!str::FindChar(part, '=') && part[0])
                i.destName = str::Dup(part);
        }
    }
    return true;
}

static void SetupCrashHandler() {
    WCHAR* symDir = AppGenDataFilename(L"crashinfo");
    WCHAR* crashDumpPath = path::Join(symDir, L"sumatrapdfcrash.dmp");
    WCHAR* crashFilePath = path::Join(symDir, L"sumatrapdfcrash.txt");
    InstallCrashHandler(crashDumpPath, crashFilePath, symDir);
    free(crashFilePath);
    free(crashDumpPath);
    free(symDir);
}

static HWND FindPrevInstWindow(HANDLE* hMutex) {
    // create a unique identifier for this executable
    // (allows independent side-by-side installations)
    AutoFreeWstr exePath = GetExePath();
    str::ToLowerInPlace(exePath);
    uint32_t hash = MurmurHash2(exePath, str::Len(exePath) * sizeof(WCHAR));
    AutoFreeWstr mapId = str::Format(L"SumatraPDF-%08x", hash);

    int retriesLeft = 3;
    HANDLE hMap = nullptr;
Retry:
    // use a memory mapping containing a process id as mutex
    hMap = CreateFileMapping(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, sizeof(DWORD), mapId);
    if (!hMap) {
        goto Error;
    }
    DWORD lastErr = GetLastError();
    bool hasPrevInst = (lastErr == ERROR_ALREADY_EXISTS);
    DWORD* procId = (DWORD*)MapViewOfFile(hMap, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(DWORD));
    if (!procId) {
        CloseHandle(hMap);
        hMap = nullptr;
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
    if (--retriesLeft < 0) {
        return nullptr;
    }
    Sleep(100);
    goto Retry;
}

// TODO(port)
// extern "C" void fz_redirect_dll_io_to_console();

// Registering happens either through the Installer or the Options dialog;
// here we just make sure that we're still registered
static bool RegisterForPdfExtentions(HWND hwnd) {
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

static int RunMessageLoop() {
    HACCEL accTable = LoadAccelerators(GetModuleHandle(nullptr), MAKEINTRESOURCE(IDC_SUMATRAPDF));
    MSG msg = {0};

    while (GetMessage(&msg, nullptr, 0, 0)) {
        // dispatch the accelerator to the correct window
        WindowInfo* win = FindWindowInfoByHwnd(msg.hwnd);
        HWND accHwnd = win ? win->hwndFrame : msg.hwnd;
        if (TranslateAccelerator(accHwnd, accTable, &msg)) {
            continue;
        }

        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return (int)msg.wParam;
}

#if defined(SUPPORTS_AUTO_UPDATE) || defined(DEBUG)
static bool RetryIO(const std::function<bool()>& func, int tries = 10) {
    while (tries-- > 0) {
        if (func())
            return true;
        Sleep(200);
    }
    return false;
}

static bool AutoUpdateMain() {
    WStrVec argList;
    ParseCmdLine(GetCommandLine(), argList, 4);
    if (argList.size() != 3 || !str::Eq(argList.at(1), L"-autoupdate")) {
        // the argument was misinterpreted, let SumatraPDF start as usual
        return false;
    }
    if (str::Eq(argList.at(2), L"replace")) {
        // older 2.6 prerelease versions used implicit paths
        AutoFreeWstr exePath(GetExePath());
        CrashIf(!str::EndsWith(exePath, L".exe-updater.exe"));
        exePath[str::Len(exePath) - 12] = '\0';
        free(argList.at(2));
        argList.at(2) = str::Format(L"replace:%s", exePath.get());
    }
    const WCHAR* otherExe = nullptr;
    if (str::StartsWith(argList.at(2), L"replace:"))
        otherExe = argList.at(2) + 8;
    else if (str::StartsWith(argList.at(2), L"cleanup:"))
        otherExe = argList.at(2) + 8;
    if (!str::EndsWithI(otherExe, L".exe") || !file::Exists(otherExe)) {
        // continue startup
        return false;
    }
    RetryIO([&] { return file::Delete(otherExe); });
    if (str::StartsWith(argList.at(2), L"cleanup:")) {
        // continue startup, restoring the previous session
        return false;
    }
    AutoFreeWstr thisExe(GetExePath());
    RetryIO([&] { return CopyFile(thisExe, otherExe, FALSE) != 0; });
    // TODO: somehow indicate success or failure
    AutoFreeWstr cleanupArgs(str::Format(L"-autoupdate cleanup:\"%s\"", thisExe.get()));
    RetryIO([&] { return LaunchFile(otherExe, cleanupArgs); });
    return true;
}
#endif

static void ShutdownCommon() {
    mui::Destroy();
    uitask::Destroy();
    UninstallCrashHandler();
    dbghelp::FreeCallstackLogs();
    // output leaks after all destructors of static objects have run
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
}

static void UpdateGlobalPrefs(const Flags& i) {
    if (i.inverseSearchCmdLine) {
        str::ReplacePtr(&gGlobalPrefs->inverseSearchCmdLine, i.inverseSearchCmdLine);
        gGlobalPrefs->enableTeXEnhancements = true;
    }
    gGlobalPrefs->fixedPageUI.invertColors = i.invertColors;

    for (size_t n = 0; n < i.globalPrefArgs.size(); n++) {
        if (str::EqI(i.globalPrefArgs.at(n), L"-esc-to-exit")) {
            gGlobalPrefs->escToExit = true;
        } else if (str::EqI(i.globalPrefArgs.at(n), L"-bgcolor") || str::EqI(i.globalPrefArgs.at(n), L"-bg-color")) {
            // -bgcolor is for backwards compat (was used pre-1.3)
            // -bg-color is for consistency
            ParseColor(&gGlobalPrefs->mainWindowBackground, i.globalPrefArgs.at(++n));
        } else if (str::EqI(i.globalPrefArgs.at(n), L"-set-color-range")) {
            ParseColor(&gGlobalPrefs->fixedPageUI.textColor, i.globalPrefArgs.at(++n));
            ParseColor(&gGlobalPrefs->fixedPageUI.backgroundColor, i.globalPrefArgs.at(++n));
        } else if (str::EqI(i.globalPrefArgs.at(n), L"-fwdsearch-offset")) {
            gGlobalPrefs->forwardSearch.highlightOffset = _wtoi(i.globalPrefArgs.at(++n));
            gGlobalPrefs->enableTeXEnhancements = true;
        } else if (str::EqI(i.globalPrefArgs.at(n), L"-fwdsearch-width")) {
            gGlobalPrefs->forwardSearch.highlightWidth = _wtoi(i.globalPrefArgs.at(++n));
            gGlobalPrefs->enableTeXEnhancements = true;
        } else if (str::EqI(i.globalPrefArgs.at(n), L"-fwdsearch-color")) {
            ParseColor(&gGlobalPrefs->forwardSearch.highlightColor, i.globalPrefArgs.at(++n));
            gGlobalPrefs->enableTeXEnhancements = true;
        } else if (str::EqI(i.globalPrefArgs.at(n), L"-fwdsearch-permanent")) {
            gGlobalPrefs->forwardSearch.highlightPermanent = _wtoi(i.globalPrefArgs.at(++n));
            gGlobalPrefs->enableTeXEnhancements = true;
        } else if (str::EqI(i.globalPrefArgs.at(n), L"-manga-mode")) {
            const WCHAR* s = i.globalPrefArgs.at(++n);
            gGlobalPrefs->comicBookUI.cbxMangaMode = str::EqI(L"true", s) || str::Eq(L"1", s);
        }
    }
}

static bool ExeHasNameOfRaMicro() {
    AutoFreeWstr exePath = GetExePath();
    const WCHAR* exeName = path::GetBaseNameNoFree(exePath);
    return str::FindI(exeName, L"ramicro");
}

// we're in installer mode if the name of the executable
// has "install" string in it e.g. SumatraPDF-installer.exe
static bool ExeHasNameOfInstaller() {
    AutoFreeWstr exePath = GetExePath();
    const WCHAR* exeName = path::GetBaseNameNoFree(exePath);
    return str::FindI(exeName, L"install");
}

static bool ExeHasInstallerResources() {
    HRSRC resSrc = FindResource(GetModuleHandle(nullptr), MAKEINTRESOURCEW(1), RT_RCDATA);
    return resSrc != nullptr;
}

static bool IsInstallerAndNamedAsSuch() {
    if (!ExeHasInstallerResources()) {
        return false;
    }
    return ExeHasNameOfInstaller();
}

// if we can load "libmupdf.dll" this is likely an installed executable
static bool SeemsInstalled() {
    HMODULE h = LoadLibraryW(L"libmupdf.dll");
    return h != nullptr;
}

static bool IsInstallerButNotInstalled() {
    if (!ExeHasInstallerResources()) {
        return false;
    }
    if (ExeHasNameOfInstaller()) {
        return true;
    }
    if (SeemsInstalled()) {
        return false;
    }
    return true;
}

// I see people trying to use installer as a portable
// version. This crashes because it can't load
// libmupdf.dll. We try to detect that case and show an error message instead.
// TODO: I still see crashes due to delay loading of libmupdf.dll
static void EnsureNotInstaller() {
    if (IsInstallerButNotInstalled()) {
        MessageBoxA(nullptr,
                    "This is a SumatraPDF installer.\nEither install it with -install option or use portable "
                    "version.\nDownload portable from https://www.sumatrapdfreader.org\n",
                    "Error", MB_OK);
        ::ExitProcess(1);
    }
}

// TODO: autoupdate, was it ever working?
// [/autoupdate]
//     /autoupdate\tperforms an update with visible UI and minimal user interaction.

static void ShowInstallerHelp() {
    // Note: translation services aren't initialized at this point, so English only
    const WCHAR* appName = getAppName();

    AutoFreeWstr msg = str::Format(
        L"%s installer options:\n\
    [/s][/d <path>][/with-filter][/with-preview][/x]\n\
    \n\
    /s\tinstalls %s silently (without user interaction).\n\
    /d\tchanges the directory where %s will be installed.\n\
    /with-filter\tinstall search filter\n\
    /with-preview\tinstall shell preview\n\
    /x\tjust extracts the files contained within the installer.\n\
",
        appName, appName, appName);
    AutoFreeWstr caption = str::Join(appName, L" Installer Usage");
    MessageBoxW(nullptr, msg, caption, MB_OK);
}

// in Installer.cpp
extern int RunInstaller(Flags*);
extern void ShowInstallerHelp();
// in Uninstaller.cpp
extern int RunUninstaller(Flags*);

// In release builds, we want to do fast exit and leave cleaning up (freeing memory) to the os.
// In debug and in release asan builds, we want to cleanup ourselves in order to see leaks.
// Note: detect_leaks ASAN flag is not (yet?) supported with msvc 16.4
#if defined(DEBUG) || defined(ASAN_BUILD)
static bool fastExit = false;
#else
static bool fastExit = true;
#endif

static void stdNewHandler() {
    // do nothing
    // this suppresses throw std::bad_alloc done by default hanlder
}

// even though we compile without exceptions, new throws std::bad_alloc and we don't want that
static void supressThrowFromNew() {
    std::set_new_handler(stdNewHandler);
}

static void ShowNotValidInstallerError() {
    MessageBoxW(nullptr, L"Not a valid installer", L"Error", MB_OK | MB_ICONERROR);
}

// in Installer.cpp
extern bool ExtractFiles(lzma::SimpleArchive* archive, const WCHAR* destDir);
extern bool CopySelfToDir(const WCHAR* destDir);

static void ExtractInstallerFiles() {
    auto [data, size, res] = LockDataResource(1);
    if (data == nullptr) {
        ShowNotValidInstallerError();
        return;
    }

    lzma::SimpleArchive archive = {};
    bool ok = lzma::ParseSimpleArchive(data, size, &archive);
    if (!ok) {
        ShowNotValidInstallerError();
        return;
    }
    AutoFreeWstr dir = GetExeDir();
    if (ExtractFiles(&archive, dir.data)) {
        CopySelfToDir(dir);
    }
}

#if 0
static void LogDpiAwareness() {
    if (!DynGetThreadDpiAwarenessContext) {
        return;
    }
    auto awc = DynGetThreadDpiAwarenessContext();
    auto aw = DynGetAwarenessFromDpiAwarenessContext(awc);

    char* aws = "unknown";
    if (aw == DPI_AWARENESS_INVALID) {
        aws = "DPI_AWARENESS_INVALID";
    } else if (aw == DPI_AWARENESS_UNAWARE) {
        aws = "DPI_AWARENESS_UNAWARE";
    } else if (aw == DPI_AWARENESS_SYSTEM_AWARE) {
        aws = "DPI_AWARENESS_SYSTEM_AWARE";
    } else if (aw == DPI_AWARENESS_PER_MONITOR_AWARE) {
        aws = "DPI_AWARENESS_PER_MONITOR_AWARE";
    }

    logf("aw: %d %s\n", (int)aw, aws);
}
#endif

#if 0
static void testLogf() {
    const char* fileName = path::GetBaseNameNoFree(__FILE__);
    WCHAR* gswin32c = L"this is a path";
    WCHAR* tmpFile = L"c:\foo\bar.txt";
    AutoFree gswin = strconv::WstrToUtf8(gswin32c);
    AutoFree tmpFileName = strconv::WstrToUtf8(path::GetBaseNameNoFree(tmpFile));
    logf("- %s:%d: using '%s' for creating '%%TEMP%%\\%s'\n", fileName, __LINE__, gswin.get(), tmpFileName.get());
}
#endif

int APIENTRY WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR cmdLine,
                     _In_ int nCmdShow) {
    UNUSED(hPrevInstance);
    UNUSED(cmdLine);
    UNUSED(nCmdShow);
    int retCode = 1; // by default it's error

    CrashIf(hInstance != GetInstance());

    if (gIsDebugBuild) {
        // Memory leak detection (only enable _CRTDBG_LEAK_CHECK_DF for
        // regular termination so that leaks aren't checked on exceptions,
        // aborts, etc. where some clean-up might not take place)
        _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF);
        //_CrtSetDbgFlag(_CRTDBG_CHECK_ALWAYS_DF);
        //_CrtSetBreakAlloc(421);
        TryLoadMemTrace();
    }

    supressThrowFromNew();

    InitDynCalls();
    NoDllHijacking();

    DisableDataExecution();
    // ensure that C functions behave consistently under all OS locales
    // (use Win32 functions where localized input or output is desired)
    setlocale(LC_ALL, "C");
    // don't show system-provided dialog boxes when accessing files on drives
    // that are not mounted (e.g. a: drive without floppy or cd rom drive
    // without a cd).
    SetErrorMode(SEM_NOOPENFILEERRORBOX | SEM_FAILCRITICALERRORS);

    srand((unsigned int)time(nullptr));

    if (!gIsAsanBuild) {
        SetupCrashHandler();
    }

    ScopedOle ole;
    InitAllCommonControls();
    ScopedGdiPlus gdiPlus(true);
    mui::Initialize();
    uitask::Initialize();

    // logToFile("C:\\Users\\kjk\\Downloads\\sumlog.txt");

    if (gIsDebugBuild) {
        logToDebugger = true;
    }

    log("Starting SumatraPDF\n");

    // testLogf();

    // LogDpiAwareness();

    // TODO: temporary, to test crash reporting
    // gAddCrashMeMenu = true;

    logf(L"CmdLine: %s\n", GetCommandLineW());
    Flags i;
    ParseCommandLine(GetCommandLineW(), i);

    if (gIsDebugBuild || gIsPreReleaseBuild) {
        if (i.tester) {
            extern int TesterMain(); // in Tester.cpp
            return TesterMain();
        }
        if (i.regress) {
            extern int RegressMain(); // in Regress.cpp
            return RegressMain();
        }
    }

    if (i.ramicro) {
        gIsRaMicroBuild = true;
    }
    if (ExeHasNameOfRaMicro()) {
        gIsRaMicroBuild = true;
    }

    if (i.showHelp && IsInstallerButNotInstalled()) {
        ShowInstallerHelp();
        return 0;
    }

    if (i.justExtractFiles) {
        ExtractInstallerFiles();
        ::ExitProcess(0);
    }

    if ((i.install || i.justExtractFiles) || IsInstallerAndNamedAsSuch()) {
        if (!ExeHasInstallerResources()) {
            ShowNotValidInstallerError();
            return 1;
        }
        retCode = RunInstaller(&i);
        // exit immediately. for some reason exit handlers try to
        // pull in libmupdf.dll which we don't have access to in the installer
        ::ExitProcess(retCode);
    }

    if (i.uninstall) {
        retCode = RunUninstaller(&i);
        ::ExitProcess(retCode);
    }

#if defined(SUPPORTS_AUTO_UPDATE) || defined(DEBUG)
    if (str::StartsWith(cmdLine, "-autoupdate")) {
        bool quit = AutoUpdateMain();
        if (quit)
            return 0;
    }
#endif

    EnsureNotInstaller();

    if (i.testRenderPage) {
        TestRenderPage(i);
        ShutdownCommon();
        return 0;
    }

    if (i.testExtractPage) {
        TestExtractPage(i);
        ShutdownCommon();
        return 0;
    }

    InitializePolicies(i.restrictedUse);
    if (i.appdataDir) {
        SetAppDataPath(i.appdataDir);
    }

    prefs::Load();
    UpdateGlobalPrefs(i);
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
        // TODO(port)
        // fz_redirect_dll_io_to_console();
    }

    if (i.testApp) {
        // in TestApp.cpp
        extern void TestApp(HINSTANCE hInstance);
        TestApp(hInstance);
        return 0;
    }

    if (i.registerAsDefault) {
        AssociateExeWithPdfExtension();
    }

    if (i.pathsToBenchmark.size() > 0) {
        BenchFileOrDir(i.pathsToBenchmark);
        if (i.showConsole)
            system("pause");
    }

    if (i.exitImmediately) {
        goto Exit;
    }

    gCrashOnOpen = i.crashOnOpen;

    GetFixedPageUiColors(gRenderCache.textColor, gRenderCache.backgroundColor);

    gIsStartup = true;
    if (!RegisterWinClass()) {
        goto Exit;
    }

    CrashIf(hInstance != GetModuleHandle(nullptr));
    if (!InstanceInit()) {
        goto Exit;
    }

    if (i.hwndPluginParent) {
        if (!SetupPluginMode(i)) {
            goto Exit;
        }
    }

    if (i.printerName) {
        // note: this prints all PDF files. Another option would be to
        // print only the first one
        for (size_t n = 0; n < i.fileNames.size(); n++) {
            bool ok = PrintFile(i.fileNames.at(n), i.printerName, !i.silent, i.printSettings);
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
    } else if (i.reuseDdeInstance) {
        hPrevWnd = FindWindow(FRAME_CLASS_NAME, nullptr);
    } else if (gGlobalPrefs->reuseInstance || gGlobalPrefs->useTabs) {
        hPrevWnd = FindPrevInstWindow(&hMutex);
    }
    if (hPrevWnd) {
        for (size_t n = 0; n < i.fileNames.size(); n++) {
            OpenUsingDde(hPrevWnd, i.fileNames.at(n), i, 0 == n);
        }
        if (0 == i.fileNames.size()) {
            win::ToForeground(hPrevWnd);
        }
        goto Exit;
    }

    bool restoreSession = false;
    if (gGlobalPrefs->sessionData->size() > 0 && !gPluginURL) {
        restoreSession = gGlobalPrefs->restoreSession;
    }
    if (gGlobalPrefs->reopenOnce->size() > 0 && !gPluginURL) {
        if (gGlobalPrefs->reopenOnce->size() == 1 && str::EqI(gGlobalPrefs->reopenOnce->at(0), L"SessionData")) {
            gGlobalPrefs->reopenOnce->FreeMembers();
            restoreSession = true;
        }
        while (gGlobalPrefs->reopenOnce->size() > 0) {
            i.fileNames.Append(gGlobalPrefs->reopenOnce->Pop());
        }
    }

    bool showStartPage =
        !restoreSession && i.fileNames.size() == 0 && gGlobalPrefs->rememberOpenedFiles && gGlobalPrefs->showStartPage;

    // ShGetFileInfoW triggers ASAN deep in Windows code so probably not my fault
    if (!gIsAsanBuild) {
        if (showStartPage) {
            // make the shell prepare the image list, so that it's ready when the first window's loaded
            SHFILEINFOW sfi = {0};
            UINT flags = SHGFI_SYSICONINDEX | SHGFI_SMALLICON | SHGFI_USEFILEATTRIBUTES;
            SHGetFileInfoW(L".pdf", 0, &sfi, sizeof(sfi), flags);
        }
    }

    WindowInfo* win = nullptr;
    if (restoreSession) {
        for (SessionData* data : *gGlobalPrefs->sessionData) {
            win = CreateAndShowWindowInfo(data);
            for (TabState* state : *data->tabStates) {
                RestoreTabOnStartup(win, state);
            }
            TabsSelect(win, data->tabIndex - 1);
        }
    }
    ResetSessionState(gGlobalPrefs->sessionData);
    // prevent the same session from being restored twice
    if (restoreSession && !(gGlobalPrefs->reuseInstance || gGlobalPrefs->useTabs)) {
        prefs::Save();
    }

    for (const WCHAR* filePath : i.fileNames) {
        if (restoreSession && FindWindowInfoByFile(filePath, false)) {
            continue;
        }
        AutoFree path = strconv::WstrToUtf8(filePath);
        win = LoadOnStartup(filePath, i, !win);
        if (!win) {
            retCode++;
            continue;
        }
        if (i.printDialog) {
            OnMenuPrint(win, i.exitWhenDone);
        }
    }

    int nWithDde = (int)gDdeOpenOnStartup.size();
    if (nWithDde > 0) {
        logf("Loading %d documents queued by dde open\n", nWithDde);
        for (auto&& filePath : gDdeOpenOnStartup) {
            if (restoreSession && FindWindowInfoByFile(filePath, false)) {
                continue;
            }
            AutoFree path = strconv::WstrToUtf8(filePath);
            win = LoadOnStartup(filePath, i, !win);
            if (!win) {
                retCode++;
            }
        }
        gDdeOpenOnStartup.Reset();
    }

    gIsStartup = false;

    if (i.fileNames.size() > 0 && !win) {
        // failed to create any window, even though there
        // were files to load (or show a failure message for)
        goto Exit;
    }
    if (i.printDialog && i.exitWhenDone) {
        goto Exit;
    }

    if (!win) {
        win = CreateAndShowWindowInfo();
        if (!win) {
            goto Exit;
        }
    }

    // Make sure that we're still registered as default,
    // if the user has explicitly told us to be
    if (gGlobalPrefs->associatedExtensions) {
        RegisterForPdfExtentions(win->hwndFrame);
    }

    if (i.stressTestPath) {
        // don't save file history and preference changes
        RestrictPolicies(Perm_SavePreferences);
        RebuildMenuBarForWindow(win);
        StartStressTest(&i, win);
        fastExit = true;
    }

    if (gGlobalPrefs->checkForUpdates) {
        UpdateCheckAsync(win, true);
    }

    // only hide newly missing files when showing the start page on startup
    if (showStartPage && gFileHistory.Get(0)) {
        gFileExistenceChecker = new FileExistenceChecker();
        gFileExistenceChecker->Start();
    }
    // call this once it's clear whether Perm_SavePreferences has been granted
    prefs::RegisterForFileChanges();

    // Change current directory for 2 reasons:
    // * prevent dll hijacking (LoadLibrary first loads from current directory
    //   which could be browser's download directory, which is an easy target
    //   for attackers to put their own fake dlls).
    //   For this to work we also have to /delayload all libraries otherwise
    //   they will be loaded even before WinMain executes.
    // * to not keep a directory opened (and therefore un-deletable) when
    //   launched by double-clicking on a file. In that case the OS sets
    //   current directory to where the file is which means we keep it open
    //   even if the file itself is closed.
    //  c:\windows\system32 is a good directory to use
    ChangeCurrDirToSystem32();

    retCode = RunMessageLoop();
    SafeCloseHandle(&hMutex);
    CleanUpThumbnailCache(gFileHistory);

Exit:
    prefs::UnregisterForFileChanges();

    if (fastExit) {
        // leave all the remaining clean-up to the OS
        // (as recommended for a quick exit)
        ::ExitProcess(retCode);
    }

    while (gWindows.size() > 0) {
        DeleteWindowInfo(gWindows.at(0));
    }

    DeleteCachedCursors();
    DeleteObject(GetDefaultGuiFont());
    DeleteBitmap(gBitmapReloadingCue);

    extern void CleanupDjVuEngine(); // in EngineDjVu.cpp
    CleanupDjVuEngine();

    // wait for FileExistenceChecker to terminate
    // (which should be necessary only very rarely)
    while (gFileExistenceChecker) {
        Sleep(10);
        uitask::DrainQueue();
    }

    mui::Destroy();
    uitask::Destroy();
    trans::Destroy();

    FileWatcherWaitForShutdown();

    SaveCallstackLogs();
    dbghelp::FreeCallstackLogs();

    // must be after uitask::Destroy() because we might have queued prefs::Reload()
    // which crashes if gGlobalPrefs is freed
    gFileHistory.UpdateStatesSource(nullptr);
    prefs::CleanUp();

    FreeAllMenuDrawInfos();

    // it's still possible to crash after this (destructors of static classes,
    // atexit() code etc.) point, but it's very unlikely
    if (!gIsAsanBuild) {
        UninstallCrashHandler();
    }

    delete gLogBuf;
    delete gLogAllocator;

    if (gIsDebugBuild) {
        // output leaks after all destructors of static objects have run
        _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
    }

    return retCode;
}
