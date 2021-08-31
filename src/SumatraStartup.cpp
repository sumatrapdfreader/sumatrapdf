/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "utils/BaseUtil.h"
#include "utils/ScopedWin.h"
#include "utils/WinDynCalls.h"
#include "utils/CmdLineArgsIter.h"
#include "utils/DbgHelpDyn.h"
#include "utils/Dpi.h"
#include "utils/FileUtil.h"
#include "utils/FileWatcher.h"
#include "utils/HtmlParserLookup.h"
#include "utils/GdiPlusUtil.h"
#include "mui/Mui.h"
#include "utils/SquareTreeParser.h"
#include "utils/ThreadUtil.h"
#include "utils/UITask.h"
#include "utils/WinUtil.h"
#include "utils/Archive.h"
#include "utils/LzmaSimpleArchive.h"

#include "SumatraConfig.h"

#include "wingui/WinGui.h"
#include "wingui/Layout.h"
#include "wingui/Window.h"
//#include "wingui/TooltipCtrl.h"
#include "wingui/SplitterWnd.h"
#include "wingui/LabelWithCloseWnd.h"

#include "Accelerators.h"
#include "wingui/TreeModel.h"
#include "DisplayMode.h"
#include "Controller.h"
#include "EngineBase.h"
#include "EngineAll.h"
#include "SettingsStructs.h"
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
#include "UpdateCheck.h"
#include "resource.h"
#include "Commands.h"
#include "Flags.h"
#include "Scratch.h"
#include "AppPrefs.h"
#include "AppTools.h"
#include "Canvas.h"
#include "Caption.h"
#include "CrashHandler.h"
#include "FileThumbnails.h"
#include "Print.h"
#include "SearchAndDDE.h"
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
#include "ExternalViewers.h"
#include "AppColors.h"

#include "utils/Log.h"

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
    FileState* fs;
    for (size_t i = 0; i < 2 * kFileHistoryMaxRecent && (fs = gFileHistory.Get(i)) != nullptr; i++) {
        if (!fs->isMissing) {
            WCHAR* fp = strconv::Utf8ToWstr(fs->filePath);
            paths.Append(fp);
        }
    }
    // add missing paths from the list of most frequently opened documents
    Vec<FileState*> frequencyList;
    gFileHistory.GetFrequencyOrder(frequencyList);
    size_t iMax = std::min<size_t>(2 * kFileHistoryMaxFrequent, frequencyList.size());
    for (size_t i = 0; i < iMax; i++) {
        fs = frequencyList.at(i);
        WCHAR* fp = strconv::Utf8ToWstr(fs->filePath);
        if (!paths.Contains(fp)) {
            paths.Append(fp);
        } else {
            str::Free(fp);
        }
    }
}

void FileExistenceChecker::HideMissingFiles() {
    for (const WCHAR* path : paths) {
        char* fp = ToUtf8Temp(path);
        gFileHistory.MarkFileInexistent(fp, true);
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
    WCHAR* iconName = MAKEINTRESOURCEW(GetAppIconID());
    FillWndClassEx(wcex, FRAME_CLASS_NAME, WndProcSumatraFrame);
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
    GetFullPathNameW(filePath, dimof(fullpath), fullpath, nullptr);

    str::WStr cmd;
    int newWindow = 0;
    if (i.inNewWindow) {
        // 2 forces opening a new window
        newWindow = 2;
    }
    cmd.AppendFmt(L"[Open(\"%s\", %d, 1, 0)]", fullpath, newWindow);
    if (i.destName && isFirstWin) {
        cmd.AppendFmt(L"[GotoNamedDest(\"%s\", \"%s\")]", fullpath, i.destName);
    } else if (i.pageNumber > 0 && isFirstWin) {
        cmd.AppendFmt(L"[GotoPage(\"%s\", %d)]", fullpath, i.pageNumber);
    }
    if ((i.startView != DisplayMode::Automatic || i.startZoom != INVALID_ZOOM ||
         i.startScroll.x != -1 && i.startScroll.y != -1) &&
        isFirstWin) {
        const char* viewModeStr = DisplayModeToString(i.startView);
        auto viewMode = ToWstrTemp(viewModeStr);
        cmd.AppendFmt(L"[SetView(\"%s\", \"%s\", %.2f, %d, %d)]", fullpath, viewMode.Get(), i.startZoom,
                      i.startScroll.x, i.startScroll.y);
    }
    if (i.forwardSearchOrigin && i.forwardSearchLine) {
        AutoFreeWstr sourcePath(path::Normalize(i.forwardSearchOrigin));
        cmd.AppendFmt(L"[ForwardSearch(\"%s\", \"%s\", %d, 0, 0, 1)]", fullpath, sourcePath.Get(), i.forwardSearchLine);
    }

    if (!i.reuseDdeInstance) {
        // try WM_COPYDATA first, as that allows targetting a specific window
        auto cbData = (cmd.size() + 1) * sizeof(WCHAR);
        COPYDATASTRUCT cds = {0x44646557 /* DdeW */, (DWORD)cbData, cmd.Get()};
        LRESULT res = SendMessageW(targetWnd, WM_COPYDATA, 0, (LPARAM)&cds);
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
    if (i.startView != DisplayMode::Automatic) {
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
        uint page;
        Vec<Rect> rects;
        AutoFreeWstr sourcePath(path::Normalize(i.forwardSearchOrigin));
        int ret = win->AsFixed()->pdfSync->SourceToDoc(sourcePath, i.forwardSearchLine, 0, &page, rects);
        ShowForwardSearchResult(win, sourcePath, i.forwardSearchLine, 0, ret, page, rects);
    }
    return win;
}

static void RestoreTabOnStartup(WindowInfo* win, TabState* state) {
    LoadArgs args(state->filePath, win);
    args.noSavePrefs = true;
    if (!LoadDocument(args)) {
        return;
    }
    TabInfo* tab = win->currentTab;
    if (!tab || !tab->ctrl) {
        return;
    }

    tab->tocState = *state->tocState;
    SetSidebarVisibility(win, state->showToc, gGlobalPrefs->showFavorites);

    DisplayMode displayMode = DisplayModeFromString(state->displayMode, DisplayMode::Automatic);
    if (displayMode != DisplayMode::Automatic) {
        SwitchToDisplayMode(win, displayMode);
    }
    tab->ctrl->GoToPage(state->pageNo, true);

    float zoom = ZoomFromString(state->zoom, INVALID_ZOOM);
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

    CrashIf(i.fileNames.size() != 1);
    while (i.fileNames.size() > 1) {
        free(i.fileNames.Pop());
    }

    // don't save preferences for plugin windows (and don't allow fullscreen mode)
    // TODO: Perm::DiskAccess is required for saving viewed files and printing and
    //       Perm::InternetAccess is required for crash reports
    // (they can still be disabled through sumatrapdfrestrict.ini or -restrict)
    RestrictPolicies(Perm::SavePreferences | Perm::FullscreenAccess);

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
    if (DisplayMode::Automatic == gGlobalPrefs->defaultDisplayModeEnum) {
        // if the user hasn't changed the default display mode,
        // display documents as single page/continuous/fit width
        // (similar to Adobe Reader, Google Chrome and how browsers display HTML)
        gGlobalPrefs->defaultDisplayModeEnum = DisplayMode::Continuous;
        gGlobalPrefs->defaultZoomFloat = ZOOM_FIT_WIDTH;
    }
    // use fixed page UI for all document types (so that the context menu always
    // contains all plugin specific entries and the main window is never closed)
    gGlobalPrefs->chmUI.useFixedPageUI = true;

    // extract some command line arguments from the URL's hash fragment where available
    // see http://www.adobe.com/devnet/acrobat/pdfs/pdf_open_parameters.pdf#nameddest=G4.1501531
    if (i.pluginURL && str::FindChar(i.pluginURL, '#')) {
        AutoFreeWstr args(str::Dup(str::FindChar(i.pluginURL, '#') + 1));
        str::TransCharsInPlace(args, L"#", L"&");
        WStrVec parts;
        parts.Split(args, L"&", true);
        for (size_t k = 0; k < parts.size(); k++) {
            WCHAR* part = parts.at(k);
            int pageNo;
            if (str::StartsWithI(part, L"page=") && str::Parse(part + 4, L"=%d%$", &pageNo)) {
                i.pageNumber = pageNo;
            } else if (str::StartsWithI(part, L"nameddest=") && part[10]) {
                i.destName = str::Dup(part + 10);
            } else if (!str::FindChar(part, '=') && part[0]) {
                i.destName = str::Dup(part);
            }
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
    auto exePath = GetExePathTemp();
    str::ToLowerInPlace(exePath);
    u32 hash = MurmurHash2(exePath, str::Len(exePath) * sizeof(WCHAR));
    AutoFreeWstr mapId = str::Format(L"SumatraPDF-%08x", hash);

    int retriesLeft = 3;
    HANDLE hMap{nullptr};
    HWND hwnd{nullptr};
    DWORD prevProcId{0};
    DWORD* procId{nullptr};
    bool hasPrevInst;
    DWORD lastErr{0};
Retry:
    // use a memory mapping containing a process id as mutex
    hMap = CreateFileMapping(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, sizeof(DWORD), mapId);
    if (!hMap) {
        goto Error;
    }
    lastErr = GetLastError();
    hasPrevInst = (lastErr == ERROR_ALREADY_EXISTS);
    procId = (DWORD*)MapViewOfFile(hMap, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(DWORD));
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
    prevProcId = *procId;
    UnmapViewOfFile(procId);
    CloseHandle(hMap);
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

// Registering happens either through the Installer or the Options dialog;
// here we just make sure that we're still registered
static bool RegisterForPdfExtentions(HWND hwnd) {
    if (IsRunningInPortableMode() || !HasPermission(Perm::RegistryAccess) || gPluginMode) {
        return false;
    }

    if (IsExeAssociatedWithPdfExtension()) {
        return true;
    }

    /* Ask user for permission, unless he previously said he doesn't want to
       see this dialog */
    if (!gGlobalPrefs->associateSilently) {
        INT_PTR result = Dialog_PdfAssociate(hwnd, &gGlobalPrefs->associateSilently);
        str::ReplaceWithCopy(&gGlobalPrefs->associatedExtensions, IDYES == result ? ".pdf" : nullptr);
    }
    // for now, .pdf is the only choice
    if (!str::EqI(gGlobalPrefs->associatedExtensions, ".pdf")) {
        return false;
    }

    AssociateExeWithPdfExtension();
    return true;
}

static int RunMessageLoop() {
    HACCEL accTable = CreateSumatraAcceleratorTable();

    MSG msg{nullptr};

    while (GetMessage(&msg, nullptr, 0, 0)) {
        // dispatch the accelerator to the correct window
        HWND accHwnd = msg.hwnd;
        WindowInfo* win = FindWindowInfoByHwnd(msg.hwnd);
        if (win) {
            accHwnd = win->hwndFrame;
        }
        if (TranslateAccelerator(accHwnd, accTable, &msg)) {
            continue;
        }

        HWND hwndDialog = GetCurrentModelessDialog();
        if (hwndDialog && IsDialogMessage(hwndDialog, &msg)) {
            // DbgLogMsg("dialog: ", msg.hwnd, msg.message, msg.wParam, msg.lParam);
            continue;
        }
        TranslateMessage(&msg);
        DispatchMessage(&msg);
        ResetTempAllocator();
    }

    return (int)msg.wParam;
}

#if defined(DEBUG)
static void ShutdownCommon() {
    mui::Destroy();
    uitask::Destroy();
    UninstallCrashHandler();
    dbghelp::FreeCallstackLogs();
}
#endif

static void ReplaceColor(char** col, WCHAR* maybeColor) {
    ParsedColor c;
    ParseColor(c, ToUtf8Temp(maybeColor).Get());
    if (c.parsedOk) {
        char* colNewStr = SerializeColor(c.col);
        str::ReplacePtr(&gGlobalPrefs->mainWindowBackground, colNewStr);
    }
}

static void UpdateGlobalPrefs(const Flags& i) {
    if (i.inverseSearchCmdLine) {
        char* cmdLine = str::Dup(ToUtf8Temp(i.inverseSearchCmdLine).AsView());
        str::ReplacePtr(&gGlobalPrefs->inverseSearchCmdLine, cmdLine);
        gGlobalPrefs->enableTeXEnhancements = true;
    }
    gGlobalPrefs->fixedPageUI.invertColors = i.invertColors;

    WCHAR* arg{nullptr};
    WCHAR* param{nullptr};
    for (size_t n = 0; n < i.globalPrefArgs.size(); n++) {
        arg = i.globalPrefArgs.at(n);
        if (str::EqI(arg, L"-esc-to-exit")) {
            gGlobalPrefs->escToExit = true;
        } else if (str::EqI(arg, L"-bgcolor") || str::EqI(arg, L"-bg-color")) {
            // -bgcolor is for backwards compat (was used pre-1.3)
            // -bg-color is for consistency
            param = i.globalPrefArgs.at(++n);
            ReplaceColor(&gGlobalPrefs->mainWindowBackground, param);
        } else if (str::EqI(arg, L"-set-color-range")) {
            param = i.globalPrefArgs.at(++n);
            ReplaceColor(&gGlobalPrefs->fixedPageUI.textColor, param);
            param = i.globalPrefArgs.at(++n);
            ReplaceColor(&gGlobalPrefs->fixedPageUI.backgroundColor, param);
        } else if (str::EqI(arg, L"-fwdsearch-offset")) {
            param = i.globalPrefArgs.at(++n);
            gGlobalPrefs->forwardSearch.highlightOffset = _wtoi(param);
            gGlobalPrefs->enableTeXEnhancements = true;
        } else if (str::EqI(arg, L"-fwdsearch-width")) {
            param = i.globalPrefArgs.at(++n);
            gGlobalPrefs->forwardSearch.highlightWidth = _wtoi(param);
            gGlobalPrefs->enableTeXEnhancements = true;
        } else if (str::EqI(arg, L"-fwdsearch-color")) {
            param = i.globalPrefArgs.at(++n);
            ReplaceColor(&gGlobalPrefs->forwardSearch.highlightColor, param);
            gGlobalPrefs->enableTeXEnhancements = true;
        } else if (str::EqI(arg, L"-fwdsearch-permanent")) {
            param = i.globalPrefArgs.at(++n);
            gGlobalPrefs->forwardSearch.highlightPermanent = _wtoi(param);
            gGlobalPrefs->enableTeXEnhancements = true;
        } else if (str::EqI(arg, L"-manga-mode")) {
            param = i.globalPrefArgs.at(++n);
            gGlobalPrefs->comicBookUI.cbxMangaMode = str::EqI(L"true", param) || str::Eq(L"1", param);
        }
    }
}

// we're in installer mode if the name of the executable
// has "install" string in it e.g. SumatraPDF-installer.exe
static bool ExeHasNameOfInstaller() {
    auto exePath = GetExePathTemp();
    const WCHAR* exeName = path::GetBaseNameTemp(exePath);
    if (str::FindI(exeName, L"uninstall")) {
        return false;
    }
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

static bool IsOurExeInstalled() {
    AutoFreeWstr installedDir = GetExistingInstallationDir();
    if (!installedDir.Get()) {
        return false;
    }
    AutoFreeWstr exeDir = GetExeDir();
    return str::EqI(installedDir.Get(), exeDir.Get());
}

static bool IsInstallerButNotInstalled() {
    if (!ExeHasInstallerResources()) {
        return false;
    }
    return !IsOurExeInstalled();
}

// TODO: maybe could set font on TDN_CREATED to Consolas, to better show the message
static HRESULT CALLBACK TaskdialogHandleLinkscallback(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam,
                                                      LONG_PTR lpRefData) {
    switch (msg) {
        case TDN_HYPERLINK_CLICKED:
            WCHAR* s = (WCHAR*)lParam;
            LaunchBrowser(s);
            break;
    }
    return S_OK;
}

// verify that libmupdf.dll matches the .exe
static void VerifyNoLibmupdfMismatch() {
    char* versionCheckFuncName{nullptr};
    FARPROC addr{nullptr};

    if (!ExeHasInstallerResources()) {
        // this is not a version that needs libmupdf.dll
        return;
    }
    if (gIsAsanBuild) {
        return;
    }

    // if we can load libmupdf.dll, then it's fine too. someone extracted libmupdf.dll
    // as well or this could be VS build I'm debugging
    HMODULE h = LoadLibraryA("libmupdf.dll");
    if (!IsValidHandle(h)) {
        goto Error;
    }
    versionCheckFuncName = str::Join("version_check_", CURR_VERSION_MAJOR_STRA);
    // change "3.4" => "3_4"
    str::TransCharsInPlace(versionCheckFuncName, ".", "_");
    addr = GetProcAddress(h, versionCheckFuncName);
    str::Free(versionCheckFuncName);
    if (!addr) {
        goto Error;
    }

    return;
Error:
    constexpr const char* corruptedInstallationConsole = R"(
Looks like corrupted installation of SumatraPDF.

Learn more at https://www.sumatrapdfreader.org/docs/Corrupted-installation
)";
    constexpr const char* corruptedInstallation =
        R"(Looks like corrupted installation of SumatraPDF.
)";
    bool ok = RedirectIOToExistingConsole();
    if (ok) {
        // if we're launched from console, print help to consle window
        printf("%s", corruptedInstallationConsole);
    }

    AutoFreeWstr title = str::Join(GetAppNameTemp(), L" installer");
    TASKDIALOGCONFIG dialogConfig{};

    DWORD flags =
        TDF_ALLOW_DIALOG_CANCELLATION | TDF_SIZE_TO_CONTENT | TDF_ENABLE_HYPERLINKS | TDF_POSITION_RELATIVE_TO_WINDOW;
    if (trans::IsCurrLangRtl()) {
        flags |= TDF_RTL_LAYOUT;
    }
    dialogConfig.cbSize = sizeof(TASKDIALOGCONFIG);
    dialogConfig.pszWindowTitle = title.Get();
    dialogConfig.pszMainInstruction = ToWstrTemp(corruptedInstallation);
    dialogConfig.pszContent =
        LR"(Learn more at <a href="https://www.sumatrapdfreader.org/docs/Corrupted-installation">www.sumatrapdfreader.org/docs/Corrupted-installation</a>.)";
    dialogConfig.nDefaultButton = IDOK;
    dialogConfig.dwFlags = flags;
    dialogConfig.cxWidth = 0;
    dialogConfig.pfCallback = TaskdialogHandleLinkscallback;
    dialogConfig.dwCommonButtons = TDCBF_CLOSE_BUTTON;
    dialogConfig.pszMainIcon = TD_ERROR_ICON;

    auto hr = TaskDialogIndirect(&dialogConfig, nullptr, nullptr, nullptr);
    CrashIf(hr == E_INVALIDARG);
    HandleRedirectedConsoleOnShutdown();
    ::ExitProcess(1);
}

constexpr const char* kInstallerHelpTmpl = R"(${appName} installer options:
[-s] [-d <path>] [-with-filter] [-with-preview] [-x]

-s
    installs ${appName} silently (without user interaction)
-d
    set installation directory
-with-filter
    install search filter
-with-preview
    install shell preview
-x
    extracts the files, doesn't install
-log
    writes installation log to %LOCALAPPDATA%\sumatra-install-log.txt
)";

static void ShowInstallerHelp() {
    // Note: translation services aren't initialized at this point, so English only
    const char* appName = ToUtf8Temp(GetAppNameTemp());
    str::Str msg{kInstallerHelpTmpl};
    str::Replace(msg, "${appName}", appName);

    bool ok = RedirectIOToExistingConsole();
    if (ok) {
        // if we're launched from console, print help to consle window
        printf("%s\n%s\n", msg.Get(), "See more at https://www.sumatrapdfreader.org/docs/Installer-cmd-line-arguments");
        return;
    }

    AutoFreeWstr title = str::Join(GetAppNameTemp(), L" installer usage");
    TASKDIALOGCONFIG dialogConfig{};

    DWORD flags =
        TDF_ALLOW_DIALOG_CANCELLATION | TDF_POSITION_RELATIVE_TO_WINDOW | TDF_SIZE_TO_CONTENT | TDF_ENABLE_HYPERLINKS;
    if (trans::IsCurrLangRtl()) {
        flags |= TDF_RTL_LAYOUT;
    }
    dialogConfig.cbSize = sizeof(TASKDIALOGCONFIG);
    dialogConfig.pszWindowTitle = title.Get();
    dialogConfig.pszMainInstruction = ToWstrTemp(msg.Get());
    dialogConfig.pszContent =
        LR"(<a href="https://www.sumatrapdfreader.org/docs/Installer-cmd-line-arguments">Read more on website</a>)";
    dialogConfig.nDefaultButton = IDOK;
    dialogConfig.dwFlags = flags;
    dialogConfig.pfCallback = TaskdialogHandleLinkscallback;
    dialogConfig.dwCommonButtons = TDCBF_OK_BUTTON;
    dialogConfig.pszMainIcon = TD_INFORMATION_ICON;

    auto hr = TaskDialogIndirect(&dialogConfig, nullptr, nullptr, nullptr);
    CrashIf(hr == E_INVALIDARG);
}

// in Installer.cpp
extern int RunInstaller();

// in Uninstaller.cpp
extern int RunUninstaller();

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
    const char* fileName = path::GetBaseNameTemp(__FILE__);
    WCHAR* gswin32c = L"this is a path";
    WCHAR* tmpFile = L"c:\foo\bar.txt";
    auto gswin = ToUtf8Temp(gswin32c);
    auto tmpFileName = ToUtf8Temp(path::GetBaseNameTemp(tmpFile));
    logf("- %s:%d: using '%s' for creating '%%TEMP%%\\%s'\n", fileName, __LINE__, gswin.Get(), tmpFileName.Get());
}
#endif

// in mupdf_load_system_font.c
extern "C" void destroy_system_font_list();

// in MemLeakDetect.cpp
extern bool MemLeakInit();
extern void DumpMemLeaks();

bool gEnableMemLeak = false;

// some libc functions internally allocate stuff that shows up
// as leaks in MemLeakDetect even though it's probably freed at shutdown
// call this function before MemLeakInit() so that those allocations
// don't show up
static void ForceStartupLeaks() {
    time_t secs{0};
    struct tm tm;
    secs = mktime(&tm);
    gmtime_s(&tm, &secs);
    gmtime(&secs);
    WCHAR* path = GetExePathTemp();
    FILE* fp = _wfopen(path, L"rb");
    if (fp) {
        fclose(fp);
    }
}

int APIENTRY WinMain(HINSTANCE hInstance, __unused HINSTANCE hPrevInstance, __unused LPSTR cmdLine,
                     __unused int nCmdShow) {
    int retCode{1}; // by default it's error
    int nWithDde{0};
    WindowInfo* win{nullptr};
    bool showStartPage{false};
    bool restoreSession{false};
    HANDLE hMutex{nullptr};
    HWND hPrevWnd{nullptr};

    CrashIf(hInstance != GetInstance());

    // TODO: decide if we should enable mem leak detection
#if defined(DEBUG)
    gEnableMemLeak = true;
#endif
    if (IsDebuggerPresent()) {
        gEnableMemLeak = true;
    }
    gEnableMemLeak = false;
    if (gEnableMemLeak) {
        fastExit = false;
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

    ForceStartupLeaks();

    // for testing mem leak detection
    void* maybeLeak{nullptr};
    if (gEnableMemLeak) {
        MemLeakInit();
        maybeLeak = malloc(10);
    }
    // maybeLeak = malloc(10);

    if (!gIsAsanBuild) {
        SetupCrashHandler();
    }

    ScopedOle ole;
    InitAllCommonControls();
    ScopedGdiPlus gdiPlus(true);
    mui::Initialize();
    uitask::Initialize();

    gLogToConsole = true;

    Flags i;
    ParseFlags(GetCommandLineW(), i);
    gCli = &i;

#if defined(DEBUG)
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
#endif

    if (i.showHelp && IsInstallerButNotInstalled()) {
        ShowInstallerHelp();
        HandleRedirectedConsoleOnShutdown();
        return 0;
    }

    if (i.justExtractFiles) {
        RedirectIOToExistingConsole();
        logf("starting ExeHasInstallerResources()\n");
        if (!ExeHasInstallerResources()) {
            log("this is not an installer, -x option not available\n");
            retCode = 1;
        } else {
            if (!ExtractInstallerFiles()) {
                log("failed to extract files");
                LogLastError();
                retCode = 1;
            }
        }
        HandleRedirectedConsoleOnShutdown();
        return retCode;
    }

    if (i.install || IsInstallerAndNamedAsSuch()) {
        if (!ExeHasInstallerResources()) {
            ShowNotValidInstallerError();
            return 1;
        }
        retCode = RunInstaller();
        // exit immediately. for some reason exit handlers try to
        // pull in libmupdf.dll which we don't have access to in the installer
        return retCode;
    }

    if (i.uninstall) {
        retCode = RunUninstaller();
        ::ExitProcess(retCode);
    }

    if (i.updateSelfTo) {
        RedirectIOToExistingConsole();
        UpdateSelfTo(i.updateSelfTo);
        if (i.exitWhenDone) {
            fastExit = !gIsDebugBuild;
            goto Exit;
        }
    }

    if (i.toEpubPath) {
        RedirectIOToExistingConsole();
        auto res = MobiToEpub(i.toEpubPath);
        DeleteVecMembers(res);
        if (i.exitWhenDone) {
            fastExit = !gIsDebugBuild;
            goto Exit;
        }
    }

    if (i.deleteFile) {
        RedirectIOToExistingConsole();
        // sleeping for a bit to make sure that the program that launched us
        // had time to exit so that we can overwrite it
        if (i.sleepMs > 0) {
            ::Sleep(i.sleepMs);
        }
        // TODO: retry if file busy?
        bool ok = file::Delete(i.deleteFile);
        if (ok) {
            logf(L"Deleted '%s'\n", i.deleteFile);
        } else {
            logf(L"Failed to delete '%s'\n", i.deleteFile);
        }
        if (i.exitWhenDone) {
            HandleRedirectedConsoleOnShutdown();
            ::ExitProcess(0);
        }
    }

    log("Starting SumatraPDF\n");

    VerifyNoLibmupdfMismatch();

    // do this before running installer etc. so that we have disk / net permissions
    // (default policy is to disallow everything)
    InitializePolicies(i.restrictedUse);

#if defined(DEBUG)
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
#endif

    if (i.appdataDir) {
        SetAppDataPath(i.appdataDir);
    }

#if defined(DEBUG)
    if (i.testApp) {
        // in TestApp.cpp
        extern void TestApp(HINSTANCE hInstance);
        TestApp(hInstance);
        return 0;
    }
#endif

    DetectExternalViewers();

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

    if (false) {
        // LoadFile();
        LoadRar();
        return 0;
    }

    if (i.showConsole) {
        RedirectIOToConsole();
    }

    if (i.registerAsDefault) {
        AssociateExeWithPdfExtension();
    }

    if (i.pathsToBenchmark.size() > 0) {
        BenchFileOrDir(i.pathsToBenchmark);
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
            if (!ok) {
                retCode++;
            }
        }
        --retCode; // was 1 if no print failures, turn 1 into 0
        goto Exit;
    }

    if (i.printDialog || i.stressTestPath || gPluginMode) {
        // TODO: pass print request through to previous instance?
    } else if (i.reuseDdeInstance) {
        hPrevWnd = FindWindow(FRAME_CLASS_NAME, nullptr);
    } else if (gGlobalPrefs->reuseInstance || gGlobalPrefs->useTabs) {
        hPrevWnd = FindPrevInstWindow(&hMutex);
    }
    if (hPrevWnd) {
        DWORD otherProcId = 1;
        GetWindowThreadProcessId(hPrevWnd, &otherProcId);
        if (!CanTalkToProcess(otherProcId)) {
            // TODO: maybe just launch another instance. The problem with that
            // is that they'll fight for settings file which might cause corruption
            auto msg = "SumatraPDF is running as admin and cannot open files from a non-admin process";
            MessageBoxA(nullptr, msg, "Error", MB_OK | MB_ICONERROR);
            goto Exit;
        }
        size_t nFiles = i.fileNames.size();
        for (size_t n = 0; n < nFiles; n++) {
            OpenUsingDde(hPrevWnd, i.fileNames.at(n), i, 0 == n);
        }
        if (0 == nFiles) {
            win::ToForeground(hPrevWnd);
        }
        goto Exit;
    }

    if (gGlobalPrefs->sessionData->size() > 0 && !gPluginURL) {
        restoreSession = gGlobalPrefs->restoreSession;
    }

    showStartPage =
        !restoreSession && i.fileNames.size() == 0 && gGlobalPrefs->rememberOpenedFiles && gGlobalPrefs->showStartPage;

    // ShGetFileInfoW triggers ASAN deep in Windows code so probably not my fault
    if (showStartPage) {
        // make the shell prepare the image list, so that it's ready when the first window's loaded
        SHFILEINFOW sfi{};
        uint flags = SHGFI_SYSICONINDEX | SHGFI_SMALLICON | SHGFI_USEFILEATTRIBUTES;
        SHGetFileInfoW(L".pdf", 0, &sfi, sizeof(sfi), flags);
    }

    if (restoreSession) {
        for (SessionData* data : *gGlobalPrefs->sessionData) {
            win = CreateAndShowWindowInfo(data);
            for (TabState* state : *data->tabStates) {
                // TODO: if prefs::Save() is called, it deletes gGlobalPrefs->sessionData
                // we're currently iterating (happened e.g. if the file is deleted)
                // the current fix is to not call prefs::Save() below but maybe there's a better way
                // maybe make a copy of TabState so that it isn't invalidated
                // https://github.com/sumatrapdfreader/sumatrapdf/issues/1674
                RestoreTabOnStartup(win, state);
            }
            TabsSelect(win, data->tabIndex - 1);
        }
    }
    ResetSessionState(gGlobalPrefs->sessionData);

    for (const WCHAR* filePath : i.fileNames) {
        if (restoreSession && FindWindowInfoByFile(filePath, false)) {
            continue;
        }
        auto path = ToUtf8Temp(filePath);
        win = LoadOnStartup(filePath, i, !win);
        if (!win) {
            retCode++;
            continue;
        }
        if (i.printDialog) {
            OnMenuPrint(win, i.exitWhenDone);
        }
    }

    nWithDde = (int)gDdeOpenOnStartup.size();
    if (nWithDde > 0) {
        logf("Loading %d documents queued by dde open\n", nWithDde);
        for (auto&& filePath : gDdeOpenOnStartup) {
            if (restoreSession && FindWindowInfoByFile(filePath, false)) {
                continue;
            }
            auto path = ToUtf8Temp(filePath);
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
        RestrictPolicies(Perm::SavePreferences);
        RebuildMenuBarForWindow(win);
        StartStressTest(&i, win);
        fastExit = true;
    }

    // only hide newly missing files when showing the start page on startup
    if (showStartPage && gFileHistory.Get(0)) {
        gFileExistenceChecker = new FileExistenceChecker();
        gFileExistenceChecker->Start();
    }
    // call this once it's clear whether Perm::SavePreferences has been granted
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
    //  \Documents is a good directory to use
    ChangeCurrDirToDocuments();

    CheckForUpdateAsync(win, UpdateCheck::Automatic);

    BringWindowToTop(win->hwndFrame);

    retCode = RunMessageLoop();
    SafeCloseHandle(&hMutex);
    CleanUpThumbnailCache(gFileHistory);

Exit:
    prefs::UnregisterForFileChanges();

    HandleRedirectedConsoleOnShutdown();

    if (fastExit) {
        // leave all the remaining clean-up to the OS
        // (as recommended for a quick exit)
        ::ExitProcess(retCode);
    }

    FreeExternalViewers();
    while (gWindows.size() > 0) {
        DeleteWindowInfo(gWindows.at(0));
    }

    DeleteCachedCursors();
    DeleteObject(GetDefaultGuiFont());
    DeleteBitmap(gBitmapReloadingCue);

    extern void CleanupEngineDjVu(); // in EngineDjVu.cpp
    CleanupEngineDjVu();
    destroy_system_font_list();

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

    ShutdownCleanup();
    EngineEbookCleanup();

    // it's still possible to crash after this (destructors of static classes,
    // atexit() code etc.) point, but it's very unlikely
    if (!gIsAsanBuild) {
        UninstallCrashHandler();
    }

    DestroyLogging();
    DestroyTempAllocator();

    if (gEnableMemLeak) {
        // free(maybeLeak);
        DumpMemLeaks();
    }

#if 0 // no longer seems to be needed in latest vs build, was probably early asan bug
    if (gIsAsanBuild) {
        // TODO: crashes in wild places without this
        // Note: ::ExitProcess(0) also crashes
        ::TerminateProcess(GetCurrentProcess(), 0);
    }
#endif

    return retCode;
}
