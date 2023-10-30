/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
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

#include "SumatraConfig.h"

#include "wingui/UIModels.h"
#include "wingui/Layout.h"
#include "wingui/WinGui.h"

#include "Settings.h"
#include "DisplayMode.h"
#include "DocController.h"
#include "EngineBase.h"
#include "EngineAll.h"
#include "DisplayModel.h"
#include "FileHistory.h"
#include "GlobalPrefs.h"
#include "Accelerators.h"
#include "PdfSync.h"
#include "RenderCache.h"
#include "ProgressUpdateUI.h"
#include "TextSelection.h"
#include "TextSearch.h"
#include "Notifications.h"
#include "SumatraPDF.h"
#include "MainWindow.h"
#include "WindowTab.h"
#include "UpdateCheck.h"
#include "resource.h"
#include "Commands.h"
#include "Flags.h"
#include "Scratch.h"
#include "AppSettings.h"
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
#include "ExternalViewers.h"
#include "AppColors.h"
#include "Theme.h"

#include "utils/Log.h"

// gFileExistenceChecker is initialized at startup and should
// terminate and delete itself asynchronously while the UI is
// being set up
class FileExistenceChecker : public ThreadBase {
    StrVec paths;

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
            char* fp = fs->filePath;
            paths.Append(fp);
        }
    }
    // add missing paths from the list of most frequently opened documents
    Vec<FileState*> frequencyList;
    gFileHistory.GetFrequencyOrder(frequencyList);
    size_t iMax = std::min<size_t>(2 * kFileHistoryMaxFrequent, frequencyList.size());
    for (size_t i = 0; i < iMax; i++) {
        fs = frequencyList.at(i);
        char* fp = fs->filePath;
        paths.AppendIfNotExists(fp);
    }
}

void FileExistenceChecker::HideMissingFiles() {
    for (const char* path : paths) {
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
        const char* path = paths[i];
        if (!path || !path::IsOnFixedDrive(path) || DocumentPathExists(path)) {
            paths.RemoveAt(i--);
        }
    }

    uitask::Post([=] {
        CrashIf(WasCancelRequested());
        HideMissingFiles();
        Terminate();
    });
}

// return false if failed in a way that should abort the app
static NO_INLINE bool MaybeMakePluginWindow(MainWindow* win, HWND hwndParent) {
    if (!hwndParent) {
        return true;
    }
    logfa("MakePluginWindow: win: 0x%p, hwndParent: 0x%x (isWindow: %d), gPluginURL: %s\n", win, hwndParent,
          (int)IsWindow(hwndParent), gPluginURL ? gPluginURL : "<nulL>");
    CrashIf(!gPluginMode);

    if (!IsWindow(hwndParent)) {
        // we validated hwndParent for validity at startup but I'm seeing cases
        // in crash reports were it's not valid here
        // I assume the window went away so we just abort
        return false;
    }

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
    return true;
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

static void SendMyselfDDE(const char* cmdA, HWND targetHwnd) {
    WCHAR* cmd = ToWStrTemp(cmdA);
    if (targetHwnd) {
        // try WM_COPYDATA first, as that allows targetting a specific window
        size_t cbData = (str::Len(cmd) + 1) * sizeof(WCHAR);
        COPYDATASTRUCT cds = {0x44646557 /* DdeW */, (DWORD)cbData, (void*)cmd};
        LRESULT res = SendMessageW(targetHwnd, WM_COPYDATA, 0, (LPARAM)&cds);
        if (res) {
            return;
        }
        // fall-through to DDEExecute if wasn't handled
    }
    DDEExecute(kSumatraDdeServer, kSumatraDdeTopic, cmd);
}

// delegate file opening to a previously running instance by sending a DDE message
static void OpenUsingDDE(HWND targetHwnd, const char* path, Flags& i, bool isFirstWin) {
    char* fullPath = path::NormalizeTemp(path);

    str::Str cmd;
    int newWindow = 0;
    if (i.inNewWindow) {
        // 2 forces opening a new window
        newWindow = 2;
    }
    cmd.AppendFmt("[Open(\"%s\", %d, 1, 0)]", fullPath, newWindow);
    if (i.destName && isFirstWin) {
        cmd.AppendFmt("[GotoNamedDest(\"%s\", \"%s\")]", fullPath, i.destName);
    } else if (i.pageNumber > 0 && isFirstWin) {
        cmd.AppendFmt("[GotoPage(\"%s\", %d)]", fullPath, i.pageNumber);
    }
    if ((i.startView != DisplayMode::Automatic || i.startZoom != kInvalidZoom ||
         i.startScroll.x != -1 && i.startScroll.y != -1) &&
        isFirstWin) {
        const char* viewModeStr = DisplayModeToString(i.startView);
        auto viewMode = ToWStrTemp(viewModeStr);
        cmd.AppendFmt("[SetView(\"%s\", \"%s\", %.2f, %d, %d)]", fullPath, viewMode, i.startZoom, i.startScroll.x,
                      i.startScroll.y);
    }
    if (i.forwardSearchOrigin && i.forwardSearchLine) {
        char* srcPath = path::NormalizeTemp(i.forwardSearchOrigin);
        cmd.AppendFmt("[ForwardSearch(\"%s\", \"%s\", %d, 0, 0, 1)]", fullPath, srcPath, i.forwardSearchLine);
    }
    if (i.search != nullptr) {
        // TODO: quote if i.search has '"' in it
        cmd.AppendFmt("[Search(\"%s\",\"%s\")]", fullPath, i.search);
    }

    if (i.reuseDdeInstance) {
        targetHwnd = nullptr; // force DDEExecute
    }
    SendMyselfDDE(cmd.Get(), targetHwnd);
}

static void FlagsEnterFullscreen(const Flags& flags, MainWindow* win) {
    if (!win) {
        return;
    }
    if (flags.enterPresentation || flags.enterFullScreen) {
        if (flags.enterPresentation && win->isFullScreen || flags.enterFullScreen && win->presentation) {
            ExitFullScreen(win);
        }
        EnterFullScreen(win, flags.enterPresentation);
    }
}

static void MaybeStartSearch(MainWindow* win, const char* searchTerm) {
    if (!win || !searchTerm) {
        return;
    }
    HwndSetText(win->hwndFindEdit, searchTerm);
    bool wasModified = true;
    bool showProgress = true;
    FindTextOnThread(win, TextSearchDirection::Forward, searchTerm, wasModified, showProgress);
}

static MainWindow* LoadOnStartup(const char* filePath, const Flags& flags, bool isFirstWin) {
    LoadArgs args(filePath, nullptr);
    args.showWin = !(flags.printDialog && flags.exitWhenDone) && !gPluginMode;
    MainWindow* win = LoadDocument(&args);
    if (!win) {
        return win;
    }

    if (win->IsDocLoaded() && flags.destName && isFirstWin) {
        char* dest = flags.destName;
        win->linkHandler->GotoNamedDest(dest);
    } else if (win->IsDocLoaded() && flags.pageNumber > 0 && isFirstWin) {
        if (win->ctrl->ValidPageNo(flags.pageNumber)) {
            win->ctrl->GoToPage(flags.pageNumber, false);
        }
    }
    bool ok = MaybeMakePluginWindow(win, flags.hwndPluginParent);
    if (!ok) {
        return nullptr;
    }
    if (!win->IsDocLoaded() || !isFirstWin) {
        return win;
    }
    FlagsEnterFullscreen(flags, win);
    if (flags.startView != DisplayMode::Automatic) {
        SwitchToDisplayMode(win, flags.startView);
    }
    if (flags.startZoom != kInvalidZoom) {
        ZoomToSelection(win, flags.startZoom);
    }
    if ((flags.startScroll.x != -1 || flags.startScroll.y != -1) && win->AsFixed()) {
        DisplayModel* dm = win->AsFixed();
        ScrollState ss = dm->GetScrollState();
        ss.x = flags.startScroll.x;
        ss.y = flags.startScroll.y;
        dm->SetScrollState(ss);
    }
    if (flags.forwardSearchOrigin && flags.forwardSearchLine && win->AsFixed() && win->AsFixed()->pdfSync) {
        int page;
        Vec<Rect> rects;
        char* srcPath = path::NormalizeTemp(flags.forwardSearchOrigin);
        int ret = win->AsFixed()->pdfSync->SourceToDoc(srcPath, flags.forwardSearchLine, 0, &page, rects);
        ShowForwardSearchResult(win, srcPath, flags.forwardSearchLine, 0, ret, page, rects);
    }
    MaybeStartSearch(win, flags.search);
    return win;
}

void SetTabState(WindowTab* tab, TabState* state) {
    if (!tab || !tab->ctrl) {
        return;
    }

    auto win = tab->win;
    DocController* ctrl = tab->ctrl;
    DisplayModel* dm = tab->AsFixed();

    // validate page number from session state
    // TODO: figure out how this happens in the first place i.e.
    // why TabState->pageNo etc. gets saved as 0
    if (state->pageNo < 1) {
        state->pageNo = 1;
        state->scrollPos = {-1, -1};
    } else {
        int nPages = ctrl->PageCount();
        if (state->pageNo > nPages) {
            state->pageNo = nPages;
            state->scrollPos = {-1, -1};
        }
    }

    tab->tocState = *state->tocState;
    SetSidebarVisibility(win, state->showToc, gGlobalPrefs->showFavorites);

    DisplayMode displayMode = DisplayModeFromString(state->displayMode, DisplayMode::Automatic);
    if (displayMode != DisplayMode::Automatic) {
        SwitchToDisplayMode(win, displayMode);
    }

    if (dm) {
        ScrollState scrollState = {state->pageNo, state->scrollPos.x, state->scrollPos.y};
        dm->SetScrollState(scrollState);
    } else {
        ctrl->GoToPage(state->pageNo, true);
    }

    float zoom = ZoomFromString(state->zoom, kInvalidZoom);
    if (zoom != kInvalidZoom) {
        if (dm) {
            dm->Relayout(zoom, state->rotation);
        } else {
            ctrl->SetZoomVirtual(zoom, nullptr);
        }
    }
}

// TODO: when files are lazy loaded, they do not restore TabState. Need to remember
// it in LoadArgs and call SetTabState() if present after loading
static void RestoreTabOnStartup(MainWindow* win, TabState* state, bool lazyLoad = true) {
    logf("RestoreTabOnStartup: state->filePath: '%s'\n", state->filePath);
    LoadArgs args(state->filePath, win);
    args.noSavePrefs = true;
    if (lazyLoad) {
        args.tabState = state;
    }
    args.lazyLoad = lazyLoad;
    if (!LoadDocument(&args)) {
        return;
    }
    WindowTab* tab = win->CurrentTab();
    if (!lazyLoad) {
        SetTabState(tab, state);
    }
}

static bool SetupPluginMode(Flags& i) {
    if (!IsWindow(i.hwndPluginParent) || i.fileNames.size() == 0) {
        return false;
    }

    gPluginURL = i.pluginURL;
    if (!gPluginURL) {
        gPluginURL = i.fileNames[0];
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
        gGlobalPrefs->defaultZoomFloat = kZoomFitWidth;
    }
    // use fixed page UI for all document types (so that the context menu always
    // contains all plugin specific entries and the main window is never closed)
    gGlobalPrefs->chmUI.useFixedPageUI = true;

    // extract some command line arguments from the URL's hash fragment where available
    // see http://www.adobe.com/devnet/acrobat/pdfs/pdf_open_parameters.pdf#nameddest=G4.1501531
    if (i.pluginURL && str::FindChar(i.pluginURL, '#')) {
        TempStr args = str::DupTemp(str::FindChar(i.pluginURL, '#') + 1);
        str::TransCharsInPlace(args, "#", "&");
        StrVec parts;
        Split(parts, args, "&", true);
        for (size_t k = 0; k < parts.size(); k++) {
            char* part = parts.at(k);
            int pageNo;
            if (str::StartsWithI(part, "page=") && str::Parse(part + 4, "=%d%$", &pageNo)) {
                i.pageNumber = pageNo;
            } else if (str::StartsWithI(part, "nameddest=") && part[10]) {
                i.destName = str::Dup(part + 10);
            } else if (!str::FindChar(part, '=') && part[0]) {
                i.destName = str::Dup(part);
            }
        }
    }
    return true;
}

static void SetupCrashHandler() {
    TempStr symDir = AppGenDataFilenameTemp("crashinfo");
    TempStr crashDumpPath = path::JoinTemp(symDir, "sumatrapdfcrash.dmp");
    TempStr crashFilePath = path::JoinTemp(symDir, "sumatrapdfcrash.txt");
    InstallCrashHandler(crashDumpPath, crashFilePath, symDir);
}

static HWND FindPrevInstWindow(HANDLE* hMutex) {
    // create a unique identifier for this executable
    // (allows independent side-by-side installations)
    char* exePath = GetExePathTemp();
    str::ToLowerInPlace(exePath);
    u32 hash = MurmurHash2(exePath, str::Len(exePath));
    TempStr mapId = str::FormatTemp("SumatraPDF-%08x", hash);

    int retriesLeft = 3;
    HANDLE hMap = nullptr;
    HWND hwnd = nullptr;
    DWORD prevProcId = 0;
    DWORD* procId = nullptr;
    bool hasPrevInst;
    DWORD lastErr = 0;
Retry:
    // use a memory mapping containing a process id as mutex
    hMap = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, sizeof(DWORD), ToWStrTemp(mapId));
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

static HACCEL FindAcceleratorsForHwnd(HWND hwnd, HWND* hwndAccel) {
    HACCEL* accTables = GetAcceleratorTables();

    HACCEL accTable = accTables[0];
    HACCEL editAccTable = accTables[1];
    HACCEL treeViewAccTable = accTables[2];
    if (FindPropertyWindowByHwnd(hwnd)) {
        *hwndAccel = hwnd;
        return editAccTable;
    }

    MainWindow* win = FindMainWindowByHwnd(hwnd);
    if (!win) {
        return nullptr;
    }
    if (hwnd == win->hwndFrame || hwnd == win->hwndCanvas) {
        *hwndAccel = win->hwndFrame;
        return accTable;
    }
    WCHAR clsName[256];
    int n = GetClassNameW(hwnd, clsName, dimof(clsName));
    if (n == 0) {
        return nullptr;
    }
    if (str::EqI(clsName, WC_EDITW)) {
        *hwndAccel = win->hwndFrame;
        return editAccTable;
    }

    if (str::EqI(clsName, WC_TREEVIEWW)) {
        *hwndAccel = win->hwndFrame;
        return treeViewAccTable;
    }

    return nullptr;
}

static int RunMessageLoop() {
    MSG msg;
    HACCEL accels;
    HWND hwndDialog;
    HWND hwndAccel;

    while (GetMessage(&msg, nullptr, 0, 0)) {
        if (PreTranslateMessage(msg)) {
            continue;
        }

        // TODO: why mouse events?
        bool doAccels = ((msg.message >= WM_KEYFIRST && msg.message <= WM_KEYLAST) ||
                         (msg.message >= WM_MOUSEFIRST && msg.message <= WM_MOUSELAST));

        if (doAccels) {
            accels = FindAcceleratorsForHwnd(msg.hwnd, &hwndAccel);
            if (accels) {
                auto didTranslate = TranslateAccelerator(hwndAccel, accels, &msg);
                if (didTranslate) {
                    continue;
                }
            }
        }

        hwndDialog = GetCurrentModelessDialog();
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

static void ReplaceColor(char** col, char* maybeColor) {
    ParsedColor c;
    ParseColor(c, maybeColor);
    if (c.parsedOk) {
        TempStr colNewStr = SerializeColorTemp(c.col);
        str::ReplaceWithCopy(col, colNewStr);
    }
}

static void UpdateGlobalPrefs(const Flags& i) {
    if (i.inverseSearchCmdLine) {
        char* cmdLine = str::Dup(i.inverseSearchCmdLine);
        str::ReplacePtr(&gGlobalPrefs->inverseSearchCmdLine, cmdLine);
        gGlobalPrefs->enableTeXEnhancements = true;
    }
    if (i.invertColors) {
        gGlobalPrefs->fixedPageUI.invertColors = true;
    }

    char* arg = nullptr;
    char* param = nullptr;
    for (size_t n = 0; n < i.globalPrefArgs.size(); n++) {
        arg = i.globalPrefArgs.at(n);
        if (str::EqI(arg, "-esc-to-exit")) {
            gGlobalPrefs->escToExit = true;
        } else if (str::EqI(arg, "-bgcolor") || str::EqI(arg, "-bg-color")) {
            // -bgcolor is for backwards compat (was used pre-1.3)
            // -bg-color is for consistency
            param = i.globalPrefArgs.at(++n);
            ReplaceColor(&gGlobalPrefs->mainWindowBackground, param);
        } else if (str::EqI(arg, "-set-color-range")) {
            param = i.globalPrefArgs.at(++n);
            ReplaceColor(&gGlobalPrefs->fixedPageUI.textColor, param);
            param = i.globalPrefArgs.at(++n);
            ReplaceColor(&gGlobalPrefs->fixedPageUI.backgroundColor, param);
        } else if (str::EqI(arg, "-fwdsearch-offset")) {
            param = i.globalPrefArgs.at(++n);
            gGlobalPrefs->forwardSearch.highlightOffset = atoi(param);
            gGlobalPrefs->enableTeXEnhancements = true;
        } else if (str::EqI(arg, "-fwdsearch-width")) {
            param = i.globalPrefArgs.at(++n);
            gGlobalPrefs->forwardSearch.highlightWidth = atoi(param);
            gGlobalPrefs->enableTeXEnhancements = true;
        } else if (str::EqI(arg, "-fwdsearch-color")) {
            param = i.globalPrefArgs.at(++n);
            ReplaceColor(&gGlobalPrefs->forwardSearch.highlightColor, param);
            gGlobalPrefs->enableTeXEnhancements = true;
        } else if (str::EqI(arg, "-fwdsearch-permanent")) {
            param = i.globalPrefArgs.at(++n);
            gGlobalPrefs->forwardSearch.highlightPermanent = atoi(param);
            gGlobalPrefs->enableTeXEnhancements = true;
        } else if (str::EqI(arg, "-manga-mode")) {
            param = i.globalPrefArgs.at(++n);
            gGlobalPrefs->comicBookUI.cbxMangaMode = str::EqI("true", param) || str::Eq("1", param);
        }
    }
}

// we're in installer mode if the name of the executable
// has "install" string in it e.g. SumatraPDF-installer.exe
static bool ExeHasNameOfInstaller() {
    char* exePath = GetExePathTemp();
    const char* exeName = path::GetBaseNameTemp(exePath);
    if (str::FindI(exeName, "uninstall")) {
        return false;
    }
    return str::FindI(exeName, "install");
}

static bool ExeHasInstallerResources() {
    HRSRC resSrc = FindResourceW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(1), RT_RCDATA);
    return resSrc != nullptr;
}

static bool IsInstallerAndNamedAsSuch() {
    if (!ExeHasInstallerResources()) {
        return false;
    }
    return ExeHasNameOfInstaller();
}

static bool IsOurExeInstalled() {
    AutoFreeStr installedDir = GetExistingInstallationDir();
    if (!installedDir.Get()) {
        return false;
    }
    char* exeDir = GetExeDirTemp();
    return str::EqI(installedDir.Get(), exeDir);
}

static bool IsInstallerButNotInstalled() {
    if (!ExeHasInstallerResources()) {
        return false;
    }
    return !IsOurExeInstalled();
}

static void CheckIsStoreBuild() {
    char* exePath = GetExePathTemp();
    const char* exeName = path::GetBaseNameTemp(exePath);
    if (str::FindI(exeName, "store")) {
        gIsStoreBuild = true;
        return;
    }
    char* dir = path::GetDirTemp(exePath);
    char* path = path::JoinTemp(dir, "AppxManifest.xml");
    if (file::Exists(path)) {
        gIsStoreBuild = true;
    }
    return;
}

// TODO: maybe could set font on TDN_CREATED to Consolas, to better show the message
static HRESULT CALLBACK TaskdialogHandleLinkscallback(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam,
                                                      LONG_PTR lpRefData) {
    switch (msg) {
        case TDN_HYPERLINK_CLICKED:
            WCHAR* s = (WCHAR*)lParam;
            LaunchBrowser(ToUtf8Temp(s));
            break;
    }
    return S_OK;
}

// in Installer.cpp
u32 GetLibmupdfDllSize();

// a single exe is both an installer and the app (if libmupdf.dll has been extracted)
// if we don't find libmupdf.dll alongside us, we assume this is installer
// if libmupdf.dll is present but different that ours, it's a damaged installation
static bool ForceRunningAsInstaller() {
    if (!ExeHasInstallerResources()) {
        // this is not a version that needs libmupdf.dll
        return false;
    }

    u32 expectedSize = GetLibmupdfDllSize();
    ReportIf(0 == expectedSize);
    if (0 == expectedSize) {
        // shouldn't happen
        return false;
    }

    TempStr exePath = GetExePathTemp();
    TempStr dir = path::GetDirTemp(exePath);
    TempStr path = path::JoinTemp(dir, "libmupdf.dll");
    auto realSize = file::GetSize(path);
    if (realSize < 0) {
        return true;
    }
    if (realSize == (i64)expectedSize) {
        return false;
    }

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

    auto title = L"SumatraPDF installer";
    TASKDIALOGCONFIG dialogConfig{};

    DWORD flags =
        TDF_ALLOW_DIALOG_CANCELLATION | TDF_SIZE_TO_CONTENT | TDF_ENABLE_HYPERLINKS | TDF_POSITION_RELATIVE_TO_WINDOW;
    if (trans::IsCurrLangRtl()) {
        flags |= TDF_RTL_LAYOUT;
    }
    dialogConfig.cbSize = sizeof(TASKDIALOGCONFIG);
    dialogConfig.pszWindowTitle = title;
    dialogConfig.pszMainInstruction = ToWStrTemp(corruptedInstallation);
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
    TempStr msg = str::ReplaceTemp(kInstallerHelpTmpl, "${appName}", kAppName);

    bool ok = RedirectIOToExistingConsole();
    if (ok) {
        // if we're launched from console, print help to consle window
        printf("%s\n%s\n", msg, "See more at https://www.sumatrapdfreader.org/docs/Installer-cmd-line-arguments");
        return;
    }

    const WCHAR* title = L"SumatraPDF installer usage";
    TASKDIALOGCONFIG dialogConfig{};

    DWORD flags =
        TDF_ALLOW_DIALOG_CANCELLATION | TDF_POSITION_RELATIVE_TO_WINDOW | TDF_SIZE_TO_CONTENT | TDF_ENABLE_HYPERLINKS;
    if (trans::IsCurrLangRtl()) {
        flags |= TDF_RTL_LAYOUT;
    }
    dialogConfig.cbSize = sizeof(TASKDIALOGCONFIG);
    dialogConfig.pszWindowTitle = title;
    dialogConfig.pszMainInstruction = ToWStrTemp(msg);
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

static void ShowNoAdminErrorMessage() {
    TASKDIALOGCONFIG dialogConfig{};
    DWORD flags = TDF_ALLOW_DIALOG_CANCELLATION | TDF_POSITION_RELATIVE_TO_WINDOW | TDF_ENABLE_HYPERLINKS;
    dialogConfig.cbSize = sizeof(TASKDIALOGCONFIG);
    dialogConfig.cxWidth = 340;
    dialogConfig.pszWindowTitle = L"SumatraPDF";
    dialogConfig.pszMainInstruction = L"SumatraPDF is running as admin and cannot open files from a non-admin process";
    ;
    dialogConfig.pszContent =
        LR"(<a href="https://github.com/sumatrapdfreader/sumatrapdf/discussions/2316">Read more about this error</a>)";
    dialogConfig.nDefaultButton = IDOK;
    dialogConfig.dwFlags = flags;
    dialogConfig.pfCallback = TaskdialogHandleLinkscallback;
    dialogConfig.dwCommonButtons = TDCBF_OK_BUTTON;
    dialogConfig.pszMainIcon = TD_INFORMATION_ICON;

    auto hr = TaskDialogIndirect(&dialogConfig, nullptr, nullptr, nullptr);
    CrashIf(hr == E_INVALIDARG);
}

// non-admin process cannot send DDE messages to admin process
// so when that happens we need to alert the user
// TODO: maybe a better fix is to re-launch ourselves as admin?
static bool IsNoAdminToAdmin(HWND hPrevWnd) {
    DWORD otherProcId = 1;
    GetWindowThreadProcessId(hPrevWnd, &otherProcId);
    if (CanTalkToProcess(otherProcId)) {
        return false;
    }
    ShowNoAdminErrorMessage();
    return false;
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

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int) {
    int exitCode = 1; // by default it's error
    int nWithDde = 0;
    MainWindow* win = nullptr;
    bool showStartPage = false;
    bool restoreSession = false;
    HANDLE hMutex = nullptr;
    HWND existingHwnd = nullptr;
    WindowTab* tabToSelect = nullptr;
    const char* logFilePath = nullptr;
    Vec<SessionData*>* sessionData = nullptr;

    CrashIf(hInstance != GetInstance());

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

    if (!IsDebuggerPresent()) {
        // VSCode shows both debugger output and console out which doubles the logging
        // TODO: only if AttachConsole() succeeds?
        gLogToConsole = true;
    }

    Flags flags;
    ParseFlags(GetCommandLineW(), flags);
    gCli = &flags;

    CheckIsStoreBuild();
    bool isInstaller = flags.install || flags.runInstallNow || IsInstallerAndNamedAsSuch();
    bool isUninstaller = flags.uninstall;
    bool noLogHere = isInstaller || isUninstaller;

    if (flags.log && !noLogHere) {
        logFilePath = GetLogFilePath();
        if (logFilePath) {
            StartLogToFile(logFilePath, true);
        }
    }

    {
        char* s = ToUtf8Temp(GetCommandLineW());
        logf("Starting SumatraPDF %s, GetCommandLineW():\n%s\n", UPDATE_CHECK_VERA, s);
    }
#if defined(DEBUG)
    if (gIsDebugBuild || gIsPreReleaseBuild) {
        if (flags.tester) {
            extern int TesterMain(); // in Tester.cpp
            return TesterMain();
        }
        if (flags.regress) {
            extern int RegressMain(); // in Regress.cpp
            return RegressMain();
        }
    }
#endif

    if (flags.showHelp && IsInstallerButNotInstalled()) {
        ShowInstallerHelp();
        HandleRedirectedConsoleOnShutdown();
        return 0;
    }

    if (flags.justExtractFiles) {
        RedirectIOToExistingConsole();
        if (!ExeHasInstallerResources()) {
            log("this is not a SumatraPDF installer, -x option not available\n");
            HandleRedirectedConsoleOnShutdown();
            return 1;
        }
        exitCode = 0;
        if (!ExtractInstallerFiles(gCli->installDir)) {
            log("failed to extract files");
            LogLastError();
            exitCode = 1;
        }
        HandleRedirectedConsoleOnShutdown();
        return exitCode;
    }

    if (isInstaller) {
        if (!ExeHasInstallerResources()) {
            ShowNotValidInstallerError();
            return 1;
        }
        exitCode = RunInstaller();
        // exit immediately. for some reason exit handlers try to
        // pull in libmupdf.dll which we don't have access to in the installer
        ::ExitProcess(exitCode);
    }

    if (isUninstaller) {
        exitCode = RunUninstaller();
        ::ExitProcess(exitCode);
    }

    if (flags.updateSelfTo) {
        RedirectIOToExistingConsole();
        UpdateSelfTo(flags.updateSelfTo);
        if (flags.exitWhenDone) {
            fastExit = !gIsDebugBuild;
            goto Exit;
        }
    }

    if (flags.deleteFile) {
        RedirectIOToExistingConsole();
        // sleeping for a bit to make sure that the program that launched us
        // had time to exit so that we can overwrite it
        if (flags.sleepMs > 0) {
            ::Sleep(flags.sleepMs);
        }
        // TODO: retry if file busy?
        bool ok = file::Delete(flags.deleteFile);
        if (ok) {
            logf("Deleted '%s'\n", flags.deleteFile);
        } else {
            logf("Failed to delete '%s'\n", flags.deleteFile);
        }
        if (flags.exitWhenDone) {
            HandleRedirectedConsoleOnShutdown();
            ::ExitProcess(0);
        }
    }

    if (ForceRunningAsInstaller()) {
        logf("forcing running as an installer\n");
        exitCode = RunInstaller();
        // exit immediately. for some reason exit handlers try to
        // pull in libmupdf.dll which we don't have access to in the installer
        ::ExitProcess(exitCode);
    }

    // do this before running installer etc. so that we have disk / net permissions
    // (default policy is to disallow everything)
    InitializePolicies(flags.restrictedUse);

#if defined(DEBUG)
    if (flags.testRenderPage) {
        TestRenderPage(flags);
        ShutdownCommon();
        return 0;
    }

    if (flags.testExtractPage) {
        TestExtractPage(flags);
        ShutdownCommon();
        return 0;
    }
#endif

    if (flags.appdataDir) {
        SetAppDataPath(flags.appdataDir);
    }

#if defined(DEBUG)
    if (flags.testApp) {
        // in TestApp.cpp
        extern void TestApp(HINSTANCE hInstance);
        TestApp(hInstance);
        return 0;
    }
#endif

    DetectExternalViewers();

    LoadSettings();
    UpdateGlobalPrefs(flags);
    SetCurrentLang(flags.lang ? flags.lang : gGlobalPrefs->uiLanguage);

#if defined(DEBUG)
    void TestBrowser(); // scratch.cpp
    if (flags.testBrowser) {
        TestBrowser();
        return 0;
    }

    void TestUngzip();
    if (false) {
        TestUngzip();
        return 0;
    }
#endif

    if (flags.showConsole) {
        RedirectIOToConsole();
    }

    if (flags.pathsToBenchmark.Size() > 0) {
        BenchFileOrDir(flags.pathsToBenchmark);
    }

    if (flags.exitImmediately) {
        goto Exit;
    }

    gCrashOnOpen = flags.crashOnOpen;

    GetDocumentColors(gRenderCache.textColor, gRenderCache.backgroundColor);
    // logfa("retrieved doc colors in WinMain: 0x%x 0x%x\n", gRenderCache.textColor, gRenderCache.backgroundColor);

    gIsStartup = true;
    if (!RegisterWinClass()) {
        goto Exit;
    }

    CrashIf(hInstance != GetModuleHandle(nullptr));
    if (!InstanceInit()) {
        goto Exit;
    }

    if (flags.hwndPluginParent) {
        // check early to avoid a crash in MakePluginWindow()
        if (!IsWindow(flags.hwndPluginParent)) {
            MessageBoxA(nullptr, "-plugin argument is not a valid window handle (hwnd)", "Error", MB_OK | MB_ICONERROR);
            goto Exit;
        }
    }

    if (flags.hwndPluginParent) {
        if (!SetupPluginMode(flags)) {
            goto Exit;
        }
    }

    {
        // search only applies if there's 1 file
        auto nFiles = flags.fileNames.size();
        if (nFiles != 1) {
            str::FreePtr(&flags.search);
        }
    }
    if (flags.printerName) {
        // note: this prints all PDF files. Another option would be to
        // print only the first one
        for (char* path : flags.fileNames) {
            bool ok = PrintFile(path, flags.printerName, !flags.silent, flags.printSettings);
            if (!ok) {
                exitCode++;
            }
        }
        --exitCode; // was 1 if no print failures, turn 1 into 0
        logf("Finished printing, exitCode: %d\n", exitCode);
        goto Exit;
    }

    if (flags.printDialog || flags.stressTestPath || gPluginMode) {
        // TODO: pass print request through to previous instance?
    } else if (flags.reuseDdeInstance || flags.dde) {
        existingHwnd = FindWindow(FRAME_CLASS_NAME, nullptr);
    } else if (gGlobalPrefs->reuseInstance || gGlobalPrefs->useTabs) {
        existingHwnd = FindPrevInstWindow(&hMutex);
    }

    // call before creating first window and menu. Otherwise menu shortcuts will be missing
    GetAcceleratorTables();

    if (flags.dde) {
        logf("sending flags.dde '%s', hwnd: 0x%p\n", flags.dde, existingHwnd);
        SendMyselfDDE(flags.dde, existingHwnd);
        // TODO: should exit?
    }
    if (existingHwnd) {
        size_t nFiles = flags.fileNames.size();
        // we allow -new-window on its own if no files given
        if (nFiles > 0 && IsNoAdminToAdmin(existingHwnd)) {
            goto Exit;
        }
        for (size_t n = 0; n < nFiles; n++) {
            char* path = flags.fileNames[n];
            bool isFirstWindow = (0 == n);
            OpenUsingDDE(existingHwnd, path, flags, isFirstWindow);
        }
        if (0 == nFiles) {
            // https://github.com/sumatrapdfreader/sumatrapdf/issues/2306
            // if -new-window cmd-line flag given, create a new window
            // even if there are no files to open
            if (flags.inNewWindow) {
                goto ContinueOpenWindow;
            } else {
                // https://github.com/sumatrapdfreader/sumatrapdf/issues/3386
                // e.g. when shift-click in taskbar, open a new window
                SendMyselfDDE("[NewWindow]", existingHwnd);
                goto Exit;
            }
        }
        goto Exit;
    }

ContinueOpenWindow:
    // keep this data alive until the end of program and ensure it's not
    // over-written by re-loading settings file while we're using it
    // and also to keep TabState forever for lazy loading of tabs
    sessionData = gGlobalPrefs->sessionData;
    gGlobalPrefs->sessionData = new Vec<SessionData*>();
    if (sessionData->size() > 0 && !gPluginURL) {
        restoreSession = gGlobalPrefs->restoreSession;
    }

    showStartPage = !restoreSession && flags.fileNames.size() == 0 && gGlobalPrefs->rememberOpenedFiles &&
                    gGlobalPrefs->showStartPage;

    // ShGetFileInfoW triggers ASAN deep in Windows code so probably not my fault
    if (showStartPage) {
        // make the shell prepare the image list, so that it's ready when the first window's loaded
        SHFILEINFOW sfi{};
        uint flg = SHGFI_SYSICONINDEX | SHGFI_SMALLICON | SHGFI_USEFILEATTRIBUTES;
        SHGetFileInfoW(L".pdf", 0, &sfi, sizeof(sfi), flg);
    }

    if (restoreSession) {
        for (SessionData* data : *sessionData) {
            win = CreateAndShowMainWindow(data);
            for (TabState* state : *data->tabStates) {
                if (str::IsEmpty(state->filePath)) {
                    logf("WinMain: skipping RestoreTabOnStartup() because state->filePath is empty\n");
                    continue;
                }
                RestoreTabOnStartup(win, state, gGlobalPrefs->lazyLoading);
            }
            TabsSelect(win, data->tabIndex - 1);
            if (gGlobalPrefs->lazyLoading) {
                ReloadDocument(win, false);
            }
        }
    }

    for (const char* path : flags.fileNames) {
        if (restoreSession) {
            auto tab = FindTabByFile(path);
            if (tab) {
                tabToSelect = tab;
                continue;
            }
        }
        win = LoadOnStartup(path, flags, !win);
        if (!win) {
            exitCode++;
            continue;
        }
        if (flags.printDialog) {
            PrintCurrentFile(win, flags.exitWhenDone);
        }
    }
    if (tabToSelect) {
        SelectTabInWindow(tabToSelect);
        MaybeStartSearch(tabToSelect->win, flags.search);
    }

    nWithDde = (int)gDdeOpenOnStartup.size();
    if (nWithDde > 0) {
        logf("Loading %d documents queued by dde open\n", nWithDde);
        for (char* path : gDdeOpenOnStartup) {
            if (restoreSession && FindMainWindowByFile(path, false)) {
                continue;
            }
            win = LoadOnStartup(path, flags, !win);
            if (!win) {
                exitCode++;
            }
        }
        gDdeOpenOnStartup.Reset();
    }

    gIsStartup = false;

    if (flags.fileNames.size() > 0 && !win) {
        // failed to create any window, even though there
        // were files to load (or show a failure message for)
        goto Exit;
    }
    if (flags.printDialog && flags.exitWhenDone) {
        goto Exit;
    }

    if (!win) {
        win = CreateAndShowMainWindow();
        if (!win) {
            goto Exit;
        }
    }
    if (flags.fileNames.Size() == 0) {
        FlagsEnterFullscreen(flags, win);
    }

    if (flags.stressTestPath) {
        // don't save file history and preference changes
        RestrictPolicies(Perm::SavePreferences);
        RebuildMenuBarForWindow(win);
        StartStressTest(&flags, win);
    }

    // only hide newly missing files when showing the start page on startup
    if (showStartPage && gFileHistory.Get(0)) {
        gFileExistenceChecker = new FileExistenceChecker();
        gFileExistenceChecker->Start();
    }
    // call this once it's clear whether Perm::SavePreferences has been granted
    RegisterSettingsForFileChanges();

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

    exitCode = RunMessageLoop();
    SafeCloseHandle(&hMutex);
    CleanUpThumbnailCache(gFileHistory);

Exit:
    logf("Exiting with exit code: %d\n", exitCode);
    UnregisterSettingsForFileChanges();

    HandleRedirectedConsoleOnShutdown();

    LaunchFileIfExists(logFilePath);
    if (AreDangerousThreadsPending()) {
        fastExit = true;
    }
    if (fastExit) {
        // leave all the remaining clean-up to the OS
        // (as recommended for a quick exit)
        ::ExitProcess(exitCode);
    }
    str::Free(logFilePath);

    if (sessionData) {
        DeleteVecMembers(*sessionData);
        delete sessionData;
    }
    FreeExternalViewers();
    while (gWindows.size() > 0) {
        DeleteMainWindow(gWindows.at(0));
    }

    DeleteCachedCursors();
    DeleteCreatedFonts();
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

    FreeAcceleratorTables();

    FileWatcherWaitForShutdown();

    SaveCallstackLogs();
    dbghelp::FreeCallstackLogs();

    // must be after uitask::Destroy() because we might have queued ReloadSettings()
    // which crashes if gGlobalPrefs is freed
    gFileHistory.UpdateStatesSource(nullptr);
    CleanUpSettings();

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

    return exitCode;
}
