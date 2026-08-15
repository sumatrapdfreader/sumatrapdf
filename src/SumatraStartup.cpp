/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include <locale.h>

#include "base/ScopedWin.h"
#include "base/WinDynCalls.h"
#include "base/DbgHelpDyn.h"
#include "base/DirScan.h"
#include "gui/Dpi.h"
#include "base/File.h"
#include "base/FileWatcher.h"
#include "base/GdiPlusUtil.h"
#include "gui/PlatformFont.h"
#include "gui/PlatformText.h"
#include "base/UITask.h"
#include "base/Win.h"
#include "base/LzmaSimpleArchive.h"

#include "SumatraConfig.h"

#include "gui/UIModels.h"
#include "gui/Layout.h"
#include "gui/win/WinGui.h"

#include "Settings.h"
#include "DisplayMode.h"
#include "DocController.h"
#include "EngineBase.h"
#include "base/GuessFileType.h"
#include "EngineAll.h"
#include "DisplayModel.h"
#include "FileHistory.h"
#include "GlobalPrefs.h"
#include "Accelerators.h"
#include "PdfSync.h"
#include "RenderCache.h"
#include "PdfDarkMode.h"
#include "ProgressUpdateUI.h"
#include "TextSelection.h"
#include "TextSearch.h"
#include "SumatraPDF.h"
#include "MainWindow.h"
#include "WindowTab.h"
#include "UpdateCheck.h"
#include "resource.h"
#include "Commands.h"
#include "Flags.h"
#include "ChmDump.h"
#include "ExifDump.h"
#include "AppSettings.h"
#include "Canvas.h"
#include "CrashHandler.h"
#include "HangDetector.h"
#include "Print.h"
#include "SearchAndDDE.h"
#include "SumatraProperties.h"
#include "Tabs.h"
#include "Translations.h"
#include "StressTesting.h"
#include "Version.h"
#include "Tests.h"
#include "AppUnitTests.h"
#include "Menu.h"
#include "AppTools.h"
#include "Installer.h"
#include "RegistryPreview.h"
#include "ExternalViewers.h"
#include "Theme.h"
#include "DarkMode_win.h"
#include "CommandPalette.h"
#include "SelectTextKeyboard.h"
#include "SumatraControl.h"
#include "SumatraLog.h"

// return false if failed in a way that should abort the app
static NO_INLINE bool MaybeMakePluginWindow(MainWindow* win, HWND hwndParent) {
    if (!hwndParent) {
        return true;
    }
    logfa("MakePluginWindow: win: 0x%p, hwndParent: 0x%p (isWindow: %d), gPluginURL: %s\n", win, hwndParent,
          (int)IsWindow(hwndParent), len(gPluginURL) == 0 ? "<nulL>" : gPluginURL);
    ReportIf(!gPluginMode);

    if (!IsWindow(hwndParent)) {
        // we validated hwndParent for validity at startup but I'm seeing cases
        // in crash reports were it's not valid here
        // I assume the window went away so we just abort
        return false;
    }

    auto* hwndFrame = win->hwndFrame;

    // first SetParent as top-level window (may fail but primes the window manager)
    SetParent(hwndFrame, hwndParent);

    // strip styles and set WS_CHILD
    long ws = GetWindowLong(hwndFrame, GWL_STYLE);
    ws &= ~(WS_POPUP | WS_BORDER | WS_CAPTION | WS_THICKFRAME);
    ws |= WS_CHILD;
    SetWindowLong(hwndFrame, GWL_STYLE, ws);

    // second SetParent after WS_CHILD is set
    SetParent(hwndFrame, hwndParent);
    Rect r = HwndClientRect(hwndParent);
    HwndMoveWindow(hwndFrame, &r);
    ShowWindow(hwndFrame, SW_SHOW);
    UpdateWindow(hwndFrame);

    // from here on, we depend on the plugin's host to resize us
    HwndSetFocus(hwndFrame);
    return true;
}

// background brush shared by the frame and canvas window classes; must stay
// valid for as long as windows of those classes exist, deleted at exit
static HBRUSH gWinClassBgBrush = nullptr;

static bool RegisterWinClass() {
    WNDCLASSEX wcex;

    HMODULE h = GetModuleHandleW(nullptr);
    HBRUSH bgBrush = CreateSolidBrush(ThemeMainWindowBackgroundColor());
    gWinClassBgBrush = bgBrush;
    FillWndClassEx(wcex, FRAME_CLASS_NAME, WndProcSumatraFrame);
    // remove CS_HREDRAW | CS_VREDRAW to avoid full invalidation on every resize
    wcex.style = 0;
    wcex.hIcon = LoadIconW(h, MAKEINTRESOURCEW(GetAppIconID()));
    wcex.hbrBackground = bgBrush;
    RegisterClassEx(&wcex);

    FillWndClassEx(wcex, CANVAS_CLASS_NAME, WndProcCanvas);
    // remove CS_HREDRAW | CS_VREDRAW to avoid full invalidation on resize
    wcex.style = CS_DBLCLKS;
    wcex.hbrBackground = bgBrush;
    RegisterClassEx(&wcex);

    return true;
}

static bool InstanceInit() {
    auto* h = GetModuleHandleA(nullptr);
    gCursorDrag = LoadCursor(h, MAKEINTRESOURCE(IDC_CURSORDRAG));
    gBitmapReloadingCue = LoadBitmap(h, MAKEINTRESOURCE(IDB_RELOADING_CUE));
    return true;
}

static void SendMyselfDDE(Str cmdA, HWND targetHwnd) {
    TempWStr cmd = ToWStrTemp(cmdA);
    if (targetHwnd) {
        // try WM_COPYDATA first, as that allows targetting a specific window
        size_t cbData = (len(cmd) + 1) * sizeof(WCHAR);
        COPYDATASTRUCT cds = {kCopyDataDdeW, (DWORD)cbData, (void*)cmd.s};
        LRESULT res = SendMessageW(targetHwnd, WM_COPYDATA, 0, (LPARAM)&cds);
        if (res) {
            return;
        }
        // fall-through to DDEExecute if wasn't handled
    }
    DDEExecute(kSumatraDdeServer, kSumatraDdeTopic, cmd);
}

// Returns true if the only thing the caller wants is to open a file (no
// goto-page, no forward search, no view overrides, etc.). In that case we
// can use the cheaper kCopyDataOpen fast path instead of building a DDE
// grammar string and blocking the caller in SendMessageW while the
// receiver loads the document.
static bool IsSimpleOpenCase(const Flags& i, bool isFirstWin) {
    if (!isFirstWin) {
        return true; // extras only apply to the first window
    }
    if (i.namedDest || i.pageNumber > 0) {
        return false;
    }
    if (i.startView != DisplayMode::Automatic || i.startZoom != kInvalidZoom) {
        return false;
    }
    if (i.startScroll.x != -1 && i.startScroll.y != -1) {
        return false;
    }
    if (i.forwardSearchOrigin && i.forwardSearchLine) {
        return false;
    }
    if (i.search) {
        return false;
    }
    return true;
}

// Send just the path + newWindow flag to an already-running SumatraPDF via
// WM_COPYDATA. Receiver (OnCopyData) handles it asynchronously so this
// SendMessageW returns fast. Returns true if the message was handled.
static bool SendOpenFileToExistingInstance(HWND targetHwnd, Str fullPath, u32 newWindow) {
    size_t pathLen = strlen(fullPath.s);
    size_t cbData = sizeof(SumatraOpenCopyData) + pathLen + 1;
    SumatraOpenCopyData* payload = (SumatraOpenCopyData*)malloc(cbData);
    if (!payload) {
        return false;
    }
    payload->newWindow = newWindow;
    memcpy(payload + 1, fullPath.s, pathLen + 1);
    COPYDATASTRUCT cds = {kCopyDataOpen, (DWORD)cbData, payload};
    LRESULT res = SendMessageW(targetHwnd, WM_COPYDATA, 0, (LPARAM)&cds);
    free(payload);
    return res != 0;
}

static bool SendOpenFilesToExistingInstance(HWND targetHwnd, StrVec& paths, u32 newWindow) {
    size_t cbData = sizeof(SumatraOpenManyCopyData);
    for (Str path : paths) {
        cbData += path.len + 1;
    }
    if (cbData > MAXDWORD) {
        return false;
    }

    u8* payload = (u8*)malloc(cbData);
    if (!payload) {
        return false;
    }
    auto* data = (SumatraOpenManyCopyData*)payload;
    data->newWindow = newWindow;
    data->pathCount = (u32)len(paths);
    u8* dst = payload + sizeof(*data);
    for (Str path : paths) {
        memcpy(dst, path.s, path.len);
        dst += path.len;
        *dst++ = 0;
    }
    COPYDATASTRUCT cds = {kCopyDataOpenMany, (DWORD)cbData, payload};
    LRESULT res = SendMessageW(targetHwnd, WM_COPYDATA, 0, (LPARAM)&cds);
    free(payload);
    return res != 0;
}

// delegate file opening to a previously running instance by sending a DDE message
static void OpenUsingDDE(HWND targetHwnd, Str path, Flags& i, bool isFirstWin) {
    TempStr fullPath = path::NormalizeTemp(path);

    u32 newWindow = 0;
    if (i.inNewWindow) {
        // 2 forces opening a new window
        newWindow = 2;
    }

    // Common case: Explorer double-clicks a file while SumatraPDF is already
    // running (reuseInstance). Use the simpler kCopyDataOpen format; the
    // receiver loads async so Explorer's child SumatraPDF process can exit
    // instantly instead of blocking on the file load.
    if (targetHwnd && !i.reuseDdeInstance && IsSimpleOpenCase(i, isFirstWin)) {
        if (SendOpenFileToExistingInstance(targetHwnd, fullPath, newWindow)) {
            return;
        }
        // fall through to the DDE grammar path if WM_COPYDATA wasn't handled
    }

    str::Builder cmd;
    cmd.Append(fmt("[Open(\"%s\", %d, 1, 0)]", fullPath, newWindow));
    if (i.namedDest && isFirstWin) {
        cmd.Append(fmt("[GotoNamedDest(\"%s\", \"%s\")]", fullPath, i.namedDest));
    } else if (i.pageNumber > 0 && isFirstWin) {
        cmd.Append(fmt("[GotoPage(\"%s\", %d)]", fullPath, i.pageNumber));
    }
    if ((i.startView != DisplayMode::Automatic || i.startZoom != kInvalidZoom ||
         i.startScroll.x != -1 && i.startScroll.y != -1) &&
        isFirstWin) {
        Str viewModeStr = DisplayModeToString(i.startView);
        cmd.Append(fmt("[SetView(\"%s\", \"%s\", %.2f, %d, %d)]", fullPath, viewModeStr, i.startZoom, i.startScroll.x,
                       i.startScroll.y));
    }
    if (i.forwardSearchOrigin && i.forwardSearchLine) {
        TempStr srcPath = path::NormalizeTemp(i.forwardSearchOrigin);
        cmd.Append(fmt("[ForwardSearch(\"%s\", \"%s\", %d, 0, 0, 1)]", fullPath, srcPath, i.forwardSearchLine));
    }
    if (i.search) {
        // TODO: quote if i.search has '"' in it
        cmd.Append(fmt("[Search(\"%s\",\"%s\")]", fullPath, i.search));
    }

    if (i.reuseDdeInstance) {
        targetHwnd = nullptr; // force DDEExecute
    }
    SendMyselfDDE(ToStr(cmd), targetHwnd);
}

static void FlagsEnterFullscreen(const Flags& flags, MainWindow* win) {
    if (!win || !win->IsDocLoaded()) {
        return;
    }
    if (flags.enterPresentation || flags.enterFullScreen) {
        if (flags.enterPresentation && win->isFullScreen || flags.enterFullScreen && win->presentation) {
            ExitFullScreen(win);
        }
        EnterFullScreen(win, flags.enterPresentation);
    }
}

static void MaybeGoTo(MainWindow* win, Str destName, int pageNumber) {
    if (!win->IsDocLoaded()) {
        return;
    }
    if (destName) {
        win->linkHandler->GotoNamedDest(destName);
        return;
    }

    if (pageNumber > 0) {
        if (win->ctrl->ValidPageNo(pageNumber)) {
            win->ctrl->GoToPage(pageNumber, false);
        }
    }
}

static void MaybeStartSearch(MainWindow* win, Str searchTerm) {
    if (!win || !searchTerm) {
        return;
    }
    if (win->findEdit) {
        win->findEdit->SetText(searchTerm);
    }
    bool wasModified = true;
    bool showProgress = true;
    FindTextOnThread(win, TextSearch::Direction::Forward, searchTerm, wasModified, showProgress);
}

static MainWindow* LoadOnStartup(Str filePath, const Flags& flags, bool isFirstWin) {
    LoadArgs args(filePath, nullptr);
    args.showWin = !(flags.printDialog && flags.exitWhenDone) && !gPluginMode;
    // Explorer multi-open with reuseInstance can deliver the same path via both
    // the cmdline and a DDE/COPYDATA open (and password dialogs keep gIsStartup
    // true long enough for both). Prefer the existing tab over a duplicate.
    args.activateExisting = true;
    MainWindow* win = LoadDocument(&args);
    if (!win) {
        return win;
    }

    if (isFirstWin) {
        MaybeGoTo(win, flags.namedDest, flags.pageNumber);
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
        SmartZoom(win, flags.startZoom, nullptr, false);
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
        TempStr srcPath = path::NormalizeTemp(flags.forwardSearchOrigin);
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

    auto* win = tab->win;
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

static void RestoreMissingTabOnStartup(MainWindow* win, TabState* state) {
    logf("RestoreTabOnStartup: file not found '%s', creating placeholder tab\n", state->filePath);
    FileHistoryMarkFileInexistent(state->filePath, true);
    WindowTab* tab = new WindowTab(win);
    tab->SetFilePath(state->filePath);
    tab->tabState = state;
    AddTabToWindow(win, tab);
}

static void RestoreTabOnStartup(MainWindow* win, TabState* state, bool lazyLoad = true) {
    logf("RestoreTabOnStartup: state->filePath: '%s'\n", state->filePath);
    if (!DocumentPathExists(state->filePath)) {
        RestoreMissingTabOnStartup(win, state);
        return;
    }
    LoadArgs args(state->filePath, win);
    args.noSavePrefs = true;
    args.showWin = false;
    args.tabState = state;
    if (!lazyLoad && SettingsUseTabs()) {
        StartLoadDocument(&args);
        return;
    }
    args.lazyLoad = lazyLoad;
    if (!LoadDocument(&args)) {
        RestoreMissingTabOnStartup(win, state);
        return;
    }
    WindowTab* tab = win->CurrentTab();
    if (!lazyLoad) {
        SetTabState(tab, state);
    }
}

static bool SetupPluginMode(Flags& i) {
    if (!IsWindow(i.hwndPluginParent) || len(i.fileNames) == 0) {
        return false;
    }

    gPluginURL = Str(i.pluginURL);
    if (!gPluginURL) {
        gPluginURL = Str(i.fileNames[0]);
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
    int hashIdx = i.pluginURL ? str::IndexOfChar(i.pluginURL, '#') : -1;
    if (hashIdx >= 0) {
        TempStr args = str::DupTemp(i.pluginURL.s + hashIdx + 1);
        str::TransCharsInPlace(args, StrL("#"), StrL("&"));
        StrVec parts;
        Split(&parts, args, "&", true);
        for (int k = 0; k < len(parts); k++) {
            Str part = parts[k];
            int pageNo;
            if (str::StartsWithI(part, StrL("page=")) &&
                !str::IsNull(str::Parse(Str(part.s + 4, part.len - 4), "=%d%$", &pageNo))) {
                i.pageNumber = pageNo;
            } else if (str::StartsWithI(part, StrL("nameddest=")) && part.len > 10) {
                i.namedDest = str::Dup(Str(part.s + 10, part.len - 10));
            } else if (!str::ContainsChar(part, '=') && part) {
                i.namedDest = str::Dup(part);
            }
        }
    }
    return true;
}

// Minimal redeclaration of the shell's IVirtualDesktopManager (Windows 10 1607+),
// to tell whether a window is on the user's current virtual desktop. We use a
// distinct name (and don't include <shobjidl.h>) to avoid clashing with the SDK
// declaration; the vtable layout matches so COM calls dispatch correctly. Lets
// reusing an existing instance avoid yanking focus to another desktop (#5630).
struct ISumatraVirtualDesktopManager : public IUnknown {
    virtual HRESULT STDMETHODCALLTYPE IsWindowOnCurrentVirtualDesktop(HWND topLevelWindow, BOOL* onCurrentDesktop) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetWindowDesktopId(HWND topLevelWindow, GUID* desktopId) = 0;
    virtual HRESULT STDMETHODCALLTYPE MoveWindowToDesktop(HWND topLevelWindow, REFGUID desktopId) = 0;
};

// {AA509086-5CA9-4C25-8F95-589D3C07B48A}
static const GUID kClsidVirtualDesktopManager = {0xAA509086,
                                                 0x5CA9,
                                                 0x4C25,
                                                 {0x8F, 0x95, 0x58, 0x9D, 0x3C, 0x07, 0xB4, 0x8A}};
// {A5CD92FF-29BE-454C-8D04-D42879C3B837}
static const GUID kIidVirtualDesktopManager = {0xA5CD92FF,
                                               0x29BE,
                                               0x454C,
                                               {0x8D, 0x04, 0xD4, 0x28, 0x79, 0xC3, 0xB8, 0x37}};

// returns nullptr on Windows without virtual desktops (e.g. Win7) or on failure.
// COM is already initialized (ScopedOle in WinMain) by the time we call this.
static ISumatraVirtualDesktopManager* CreateVirtualDesktopManager() {
    ISumatraVirtualDesktopManager* mgr = nullptr;
    CoCreateInstance(kClsidVirtualDesktopManager, nullptr, CLSCTX_ALL, kIidVirtualDesktopManager, (void**)&mgr);
    return mgr;
}

// true if hwnd is on the user's current virtual desktop. Defaults to true when
// we can't tell (no manager on Win7, or the query fails), preserving the old
// "reuse the first instance window" behavior.
static bool IsWindowOnCurrentDesktop(ISumatraVirtualDesktopManager* vdm, HWND hwnd) {
    if (!vdm || !hwnd) {
        return true;
    }
    BOOL onCurrent = FALSE;
    HRESULT hr = vdm->IsWindowOnCurrentVirtualDesktop(hwnd, &onCurrent);
    if (FAILED(hr)) {
        return true;
    }
    return onCurrent != FALSE;
}

// Finds a window of a previously running instance to reuse. Prefers a window on
// the current virtual desktop; if the instance only has windows on other
// desktops, returns one of them and sets *openInNewWindow so the caller opens
// the file in a new window (which Windows places on the current desktop),
// instead of switching to another desktop (#5630). Returns nullptr if we can't
// talk to the previous instance (e.g. it runs elevated and we don't), so the
// caller opens files in this process instead of sending DDE messages that the
// elevated process would never receive.
static HWND FindExistingSumatraProcessHwnd(HANDLE* hMutex, bool* openInNewWindow) {
    *openInNewWindow = false;
    // create a unique identifier for this executable and appdata combination
    // (allows independent side-by-side installations)
    TempStr combinedPath = str::JoinTemp(GetSelfExePathTemp(), StrL("|"), GetAppDataDirTemp());
    str::ToLowerInPlace(combinedPath);
    u32 hash = MurmurHash2(combinedPath);
    TempStr mapId = fmt("SumatraPDF-%08x", hash);

    int retriesLeft = 3;
    HANDLE hMap = nullptr;
    HWND hwnd = nullptr;
    DWORD prevProcId = 0;
    DWORD* procId = nullptr;
    bool hasPrevInst;
    DWORD lastErr = 0;
Retry:
    // use a memory mapping containing a process id as mutex
    hMap = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, sizeof(DWORD), CWStrTemp(mapId));
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
    // a non-admin process can't send DDE messages to an admin process, so
    // don't return its window; retrying won't help either
    if (!CanTalkToProcess(prevProcId)) {
        return nullptr;
    }
    {
        // nullptr on Win7 / no virtual desktops -> IsWindowOnCurrentDesktop()
        // returns true for every window, so we reuse the first one as before.
        ISumatraVirtualDesktopManager* vdm = CreateVirtualDesktopManager();
        HWND otherDesktopWnd = nullptr; // a window of the prev instance, on another desktop
        while ((hwnd = FindWindowExW(HWND_DESKTOP, hwnd, FRAME_CLASS_NAME, nullptr)) != nullptr) {
            DWORD wndProcId;
            GetWindowThreadProcessId(hwnd, &wndProcId);
            if (wndProcId != prevProcId) {
                continue;
            }
            if (IsWindowOnCurrentDesktop(vdm, hwnd)) {
                if (vdm) {
                    vdm->Release();
                }
                AllowSetForegroundWindow(prevProcId);
                return hwnd; // reuse the window on the current desktop
            }
            otherDesktopWnd = hwnd;
        }
        if (vdm) {
            vdm->Release();
        }
        if (otherDesktopWnd) {
            // the previous instance has windows, but none on the current virtual
            // desktop. Reuse it but open the file in a new window (it lands on the
            // current desktop) rather than switching desktops (#5630).
            AllowSetForegroundWindow(prevProcId);
            *openInNewWindow = true;
            return otherDesktopWnd;
        }
    }

    // process is alive but its window isn't ready yet (startup race): retry
Error:
    if (--retriesLeft < 0) {
        return nullptr;
    }
    Sleep(100);
    goto Retry;
}

// forwardSysKeys is set when hwnd is a child pane (TOC tree / edit box) whose
// Alt / F10 menu keys should be routed to the frame (see MaybeTranslateAccelerator)
static HACCEL FindAcceleratorsForHwnd(HWND hwnd, HWND* hwndAccel, bool* forwardSysKeys = nullptr) {
    if (forwardSysKeys) {
        *forwardSysKeys = false;
    }
    HACCEL* accTables = GetAcceleratorTables();

    HACCEL accTable = accTables[0];
    HACCEL editAccTable = accTables[1];
    HACCEL treeViewAccTable = accTables[2];
    if (FindPropertyWindowByHwnd(hwnd)) {
        *hwndAccel = hwnd;
        return editAccTable;
    }

    HWND hwndCP = CommandPaletteHwndForAccelerator(hwnd);
    if (hwndCP) {
        *hwndAccel = hwndCP;
        return editAccTable;
    }

    MainWindow* win = FindMainWindowByHwnd(hwnd);
    if (!win) {
        return nullptr;
    }
    // Edit / tree keep a reduced table so letters and arrows type/navigate.
    // Every other child (Virt toolbar, tabs HWND, canvas, WebView host, …)
    // uses the main table — those windows take focus on click, and returning
    // nullptr here is why Ctrl+W stopped closing a tab after the Virt toolbar.
    WCHAR clsName[256];
    int n = GetClassNameW(hwnd, clsName, dimof(clsName));
    *hwndAccel = win->hwndFrame;
    if (n <= 0) {
        return accTable;
    }
    if (wstr::EqI(clsName, WC_EDITW)) {
        if (forwardSysKeys) {
            *forwardSysKeys = true;
        }
        return editAccTable;
    }
    if (wstr::EqI(clsName, WC_TREEVIEWW)) {
        if (forwardSysKeys) {
            *forwardSysKeys = true;
        }
        return treeViewAccTable;
    }
    return accTable;
}

static bool MaybeTranslateAccelerator(MSG& msg) {
    // TODO: why mouse events?
    bool doAccels = ((msg.message >= WM_KEYFIRST && msg.message <= WM_KEYLAST) ||
                     (msg.message >= WM_MOUSEFIRST && msg.message <= WM_MOUSELAST));
    if (!doAccels) return false;

    // Arrows, Home/End and PageUp/PageDown normally accelerate to scroll /
    // go-to-page commands. While the keyboard selection caret is up they move
    // the caret instead, so skip the accelerator and let FrameOnKeydown see the
    // key (issues #4684, #4116).
    if (msg.message == WM_KEYDOWN && !IsAltPressed()) {
        WPARAM key = msg.wParam;
        bool isCaretKey = key == VK_LEFT || key == VK_RIGHT || key == VK_UP || key == VK_DOWN || key == VK_HOME ||
                          key == VK_END || key == VK_PRIOR || key == VK_NEXT;
        if (isCaretKey && SelectTextWithKeyboardActive(FindMainWindowByHwnd(msg.hwnd))) {
            return false;
        }
    }

    // Shift+arrows normally accelerate to scroll. When a PDF text selection is
    // active, skip the accelerator so FrameOnKeydown can extend the selection
    // via TextSelection::ExtendBy (issue #5814).
    if (msg.message == WM_KEYDOWN && IsShiftPressed() && !IsCtrlPressed() && !IsAltPressed()) {
        WPARAM key = msg.wParam;
        if (key == VK_LEFT || key == VK_RIGHT || key == VK_UP || key == VK_DOWN) {
            MainWindow* win = FindMainWindowByHwnd(msg.hwnd);
            if (win && win->AsFixed() && win->AsFixed()->textSelection &&
                win->AsFixed()->textSelection->result.len > 0) {
                return false;
            }
        }
    }

    // On the home page the arrows, Enter and Del drive the keyboard selection of
    // the file list (issue #1136) rather than scrolling / other accelerators.
    // Send them to the canvas, which is where that's handled. The search box is
    // an edit control, so it keeps its own keys.
    if (msg.message == WM_KEYDOWN && !IsCtrlPressed() && !IsShiftPressed() && !IsAltPressed()) {
        WPARAM key = msg.wParam;
        bool isNavKey =
            key == VK_LEFT || key == VK_RIGHT || key == VK_UP || key == VK_DOWN || key == VK_RETURN || key == VK_DELETE;
        if (isNavKey) {
            MainWindow* win = FindMainWindowByHwnd(msg.hwnd);
            // only for the frame / canvas: the search box needs its own arrows
            // for caret movement (it handles Down itself, to enter the list)
            bool isFrameOrCanvas = win && (msg.hwnd == win->hwndFrame || msg.hwnd == win->hwndCanvas);
            if (isFrameOrCanvas && win->IsCurrentTabAbout() && win->hwndCanvas) {
                msg.hwnd = win->hwndCanvas;
                return false;
            }
        }
    }

    HWND hwndAccel;
    bool forwardSysKeys = false;
    HACCEL accels = FindAcceleratorsForHwnd(msg.hwnd, &hwndAccel, &forwardSysKeys);
    if (!accels) return false;
    if (TranslateAcceleratorW(hwndAccel, accels, &msg)) {
        return true;
    }
    // Alt+<mnemonic> / F10 open the menu bar, but those aren't in the accelerator
    // tables; when focus is on a child pane (TOC tree / edit box) the control
    // would swallow them. Retarget the system-key messages to the frame so its
    // menu still responds (issue #4614). Retargeting before TranslateMessage()
    // also makes the generated WM_SYSCHAR go to the frame.
    if (forwardSysKeys && (msg.message == WM_SYSKEYDOWN || msg.message == WM_SYSKEYUP || msg.message == WM_SYSCHAR)) {
        msg.hwnd = hwndAccel;
    }
    return false;
}

// Resets the temp allocator (done once per message-loop iteration) and tracks a
// global high-water mark of its per-iteration allocation count and peak bytes.
// Whenever a new max is reached, logs both so we can size the temp allocator.
static void ResetTempArenaWithLogging() {
    Arena* a = GetTempArena();
    static u64 gMaxAllocs = 0;
    static u64 gPeakBytes = 0;
    u64 nAllocs = a->nAllocsSinceReset;
    u64 peakBytes = a->peakBytesSinceReset;
    bool isNewMax = false;
    if (nAllocs > gMaxAllocs) {
        gMaxAllocs = nAllocs;
        isNewMax = true;
    }
    if (peakBytes > gPeakBytes) {
        gPeakBytes = peakBytes;
        isNewMax = true;
    }
    if (isNewMax) {
        char human[32];
        FormatSizeHumanIntoBuf(gPeakBytes, Str(human, sizeofi(human)));
        logf("temp allocator new max: %s allocations, peak %s bytes (%s)\n",
             str::FormatNumWithThousandSepTemp((i64)gMaxAllocs), str::FormatNumWithThousandSepTemp((i64)gPeakBytes),
             Str(human));
    }
    ResetTempArena();
}

static int RunMessageLoop() {
    MSG msg;

    while (GetMessage(&msg, nullptr, 0, 0)) {
        if (PreTranslateMessage(msg)) {
            continue;
        }

        if (MaybeTranslateAccelerator(msg)) continue;
        HWND hwndDialog = GetCurrentModelessDialog();
        if (hwndDialog && IsDialogMessage(hwndDialog, &msg)) {
            // DbgLogMsg("dialog: ", msg.hwnd, msg.message, msg.wParam, msg.lParam);
            continue;
        }
        TranslateMessage(&msg);
        DispatchMessage(&msg);
        ResetTempArenaWithLogging();
    }

    return (int)msg.wParam;
}

static void FreeLibsumatrapdfDll();

static void ShutdownCommon() {
    PlatformFontDestroy();
    uitask::Destroy();
    FreeLibsumatrapdfDll();
    UninstallCrashHandler();
    dbghelp::FreeCallstackLogs();
}

static void ReplaceColor(ParsedColor& col, Str maybeColor) {
    ParsedColor c;
    ParseColor(c, maybeColor);
    if (c.parsedOk) {
        SetColorText(col, SerializeColorTemp(c.col));
    }
}

static void UpdateGlobalPrefs(const Flags& i) {
    if (!i.windowPos.IsEmpty()) {
        // -window-pos stands in for the remembered position: code that has to
        // know the window's shape before it's on screen reads it from here
        // (EbookLayoutAspectForWindow), and a remembered maximized state would
        // otherwise ignore the rectangle we were given
        gGlobalPrefs->windowPos = i.windowPos;
        gGlobalPrefs->windowState = WIN_STATE_NORMAL;
    }
    if (i.inverseSearchCmdLine) {
        str::ReplaceWithCopy(&gGlobalPrefs->inverseSearchCmdLine, i.inverseSearchCmdLine);
        gGlobalPrefs->enableTeXEnhancements = true;
    }
    if (i.invertColors) {
        SetDocumentColorsFollowTheme(DocumentColorsFollowTheme::Smart);
    }

    Str arg;
    Str param;
    for (int n = 0; n < len(i.globalPrefArgs); n++) {
        arg = i.globalPrefArgs[n];
        if (str::EqI(arg, StrL("-esc-to-exit"))) {
            gGlobalPrefs->escToExit = true;
        } else if (str::EqI(arg, StrL("-bgcolor")) || str::EqI(arg, StrL("-bg-color"))) {
            // -bgcolor is for backwards compat (was used pre-1.3)
            // -bg-color is for consistency
            param = i.globalPrefArgs[++n];
            ReplaceColor(gGlobalPrefs->mainWindowBackground, param);
        } else if (str::EqI(arg, StrL("-set-color-range"))) {
            param = i.globalPrefArgs[++n];
            ReplaceColor(gGlobalPrefs->fixedPageUI.textColor, param);
            param = i.globalPrefArgs[++n];
            ReplaceColor(gGlobalPrefs->fixedPageUI.backgroundColor, param);
        } else if (str::EqI(arg, StrL("-fwdsearch-offset"))) {
            param = i.globalPrefArgs[++n];
            gGlobalPrefs->forwardSearch.highlightOffset = ParseInt(param);
            gGlobalPrefs->enableTeXEnhancements = true;
        } else if (str::EqI(arg, StrL("-fwdsearch-width"))) {
            param = i.globalPrefArgs[++n];
            gGlobalPrefs->forwardSearch.highlightWidth = ParseInt(param);
            gGlobalPrefs->enableTeXEnhancements = true;
        } else if (str::EqI(arg, StrL("-fwdsearch-color"))) {
            param = i.globalPrefArgs[++n];
            ReplaceColor(gGlobalPrefs->forwardSearch.highlightColor, param);
            gGlobalPrefs->enableTeXEnhancements = true;
        } else if (str::EqI(arg, StrL("-fwdsearch-permanent"))) {
            param = i.globalPrefArgs[++n];
            gGlobalPrefs->forwardSearch.highlightPermanent = ParseInt(param);
            gGlobalPrefs->enableTeXEnhancements = true;
        } else if (str::EqI(arg, StrL("-manga-mode"))) {
            param = i.globalPrefArgs[++n];
            gGlobalPrefs->comicBookUI.cbxMangaMode = str::EqI("true", param) || str::Eq("1", param);
        }
    }
}

// we're in installer mode if the name of the executable
// has "install" string in it e.g. SumatraPDF-installer.exe
static bool ExeHasNameOfInstaller() {
    TempStr exePath = GetSelfExePathTemp();
    TempStr exeName = path::GetBaseNameTemp(exePath);
    if (str::ContainsI(exeName, StrL("uninstall"))) {
        return false;
    }
    return str::ContainsI(exeName, StrL("install"));
}

static bool ExeHasNameOfStoreInstaller() {
    TempStr exePath = GetSelfExePathTemp();
    TempStr exeName = path::GetBaseNameTemp(exePath);
    return str::ContainsI(exeName, StrL("install-store"));
}

// Uncompressed size of libsumatrapdf.dll in the IDR_DLL_PAK LzSA resource, or -1 if
// the resource / entry is missing. Cached after the first call (-2 = uncached).
static i64 GetEmbeddedLibsumatrapdfSize() {
    static i64 size = -2;
    if (size != -2) {
        return size;
    }
    size = -1;

    LoadedDataResource res{};
    if (!LockDataResource(IDR_DLL_PAK, &res)) {
        return size;
    }
    lzma::SimpleArchive archive{};
    if (!lzma::ParseSimpleArchive(res.data, res.dataSize, &archive)) {
        return size;
    }
    for (int i = 0; i < archive.filesCount; i++) {
        lzma::FileInfo* fi = &archive.files[i];
        if (str::EqI(fi->name, StrL("libsumatrapdf.dll"))) {
            size = (i64)fi->uncompressedSize;
            break;
        }
    }
    return size;
}

static bool HasEmbeddedLibsumatrapdf() {
    return GetEmbeddedLibsumatrapdfSize() >= 0;
}

static bool IsInstallerAndNamedAsSuch() {
    if (!HasEmbeddedLibsumatrapdf()) {
        return false;
    }
    return ExeHasNameOfInstaller();
}

static bool IsInstallerButNotInstalled() {
    if (!HasEmbeddedLibsumatrapdf()) {
        return false;
    }
    return !IsOurExeInstalled();
}

// we delay load libsumatrapdf.dll but it seems in some cases it fails to load
// as seen in crash reports
// here I'm trying to explicitly LoadLibrary() to hopefully fix that
// if not, at least I can add logging to figure out why it fails
constexpr int kBtnIdLearnMore = 100;
static Str kFailedToLoadURL() {
    return StrL("https://www.sumatrapdfreader.org/docs/Failed-to-load-libmupdf");
}

static HRESULT CALLBACK LoadLibsumatrapdfDialogCallback(HWND /*hwnd*/, UINT msg, WPARAM wParam, LPARAM lParam,
                                                        LONG_PTR /*lpRefData*/) {
    switch (msg) {
        case TDN_HYPERLINK_CLICKED: {
            LaunchBrowser(ToUtf8Temp(WStr((wchar_t*)lParam)));
            break;
        }
        case TDN_BUTTON_CLICKED:
            if ((int)wParam == kBtnIdLearnMore) {
                LaunchBrowser(kFailedToLoadURL());
                return S_FALSE; // don't close the dialog
            }
            break;
    }
    return S_OK;
}

// if true, and libsumatrapdf.dll is not next to the exe, extract it to the build data
// dir and load from there (portable / single-exe). if false, only try next to
// the exe; missing dll is handled by ForceRunningAsInstaller.
static bool gSingleExe = true;

static HMODULE gLibsumatrapdfDll = nullptr;
// Last LoadLibrary(Ex) failure for libsumatrapdf.dll (0 if none / size mismatch skip).
static DWORD gLibsumatrapdfLastLoadError = 0;

static void FreeLibsumatrapdfDll() {
    if (gLibsumatrapdfDll) {
        FreeLibrary(gLibsumatrapdfDll);
        gLibsumatrapdfDll = nullptr;
    }
}

// Errors commonly seen when antivirus is scanning / locking a just-written DLL
// (debug reports e.g. 8bfd12791000001 with err=87 after successful extract).
static bool IsLibsumatrapdfAvRaceError(DWORD err) {
    return err == ERROR_INVALID_PARAMETER     // 87 — sometimes returned by AV hooks
           || err == ERROR_ACCESS_DENIED      // 5
           || err == ERROR_SHARING_VIOLATION; // 32
}

// Windows Application Control (WDAC / AppLocker / Smart App Control) refused
// LoadLibrary — file is on disk but policy blocks execution. e.g. debug report
// 8c09d3b87000001 with err=4551 ("An Application Control policy has blocked this file.").
// Older SDKs / MinGW lack these Smart App Control / System Integrity error codes.
#ifndef ERROR_SYSTEM_INTEGRITY_POLICY_VIOLATION
#define ERROR_SYSTEM_INTEGRITY_POLICY_VIOLATION 4551L
#endif
#ifndef ERROR_SYSTEM_INTEGRITY_REPUTATION_MALICIOUS
#define ERROR_SYSTEM_INTEGRITY_REPUTATION_MALICIOUS 4556L
#endif
#ifndef ERROR_SYSTEM_INTEGRITY_REPUTATION_PUA
#define ERROR_SYSTEM_INTEGRITY_REPUTATION_PUA 4557L
#endif
#ifndef ERROR_SYSTEM_INTEGRITY_REPUTATION_DANGEROUS_EXT
#define ERROR_SYSTEM_INTEGRITY_REPUTATION_DANGEROUS_EXT 4558L
#endif
#ifndef ERROR_SYSTEM_INTEGRITY_REPUTATION_OFFLINE
#define ERROR_SYSTEM_INTEGRITY_REPUTATION_OFFLINE 4559L
#endif
#ifndef ERROR_SYSTEM_INTEGRITY_REPUTATION_UNFRIENDLY_FILE
#define ERROR_SYSTEM_INTEGRITY_REPUTATION_UNFRIENDLY_FILE 4580L
#endif
#ifndef ERROR_SYSTEM_INTEGRITY_REPUTATION_UNATTAINABLE
#define ERROR_SYSTEM_INTEGRITY_REPUTATION_UNATTAINABLE 4581L
#endif
#ifndef ERROR_SYSTEM_INTEGRITY_REPUTATION_EXPLICIT_DENY_FILE
#define ERROR_SYSTEM_INTEGRITY_REPUTATION_EXPLICIT_DENY_FILE 4582L
#endif
#ifndef ERROR_SYSTEM_INTEGRITY_WHQL_NOT_SATISFIED
#define ERROR_SYSTEM_INTEGRITY_WHQL_NOT_SATISFIED 4583L
#endif
static bool IsLibsumatrapdfAppControlError(DWORD err) {
    switch (err) {
        case ERROR_ACCESS_DISABLED_BY_POLICY:                      // 1260 — AppLocker / SRP
        case ERROR_SYSTEM_INTEGRITY_POLICY_VIOLATION:              // 4551
        case ERROR_SYSTEM_INTEGRITY_REPUTATION_MALICIOUS:          // 4556
        case ERROR_SYSTEM_INTEGRITY_REPUTATION_PUA:                // 4557
        case ERROR_SYSTEM_INTEGRITY_REPUTATION_DANGEROUS_EXT:      // 4558
        case ERROR_SYSTEM_INTEGRITY_REPUTATION_OFFLINE:            // 4559
        case ERROR_SYSTEM_INTEGRITY_REPUTATION_UNFRIENDLY_FILE:    // 4580
        case ERROR_SYSTEM_INTEGRITY_REPUTATION_UNATTAINABLE:       // 4581
        case ERROR_SYSTEM_INTEGRITY_REPUTATION_EXPLICIT_DENY_FILE: // 4582
        case ERROR_SYSTEM_INTEGRITY_WHQL_NOT_SATISFIED:            // 4583
            return true;
        default:
            return false;
    }
}

static void LogLibsumatrapdfFileStateAfterLoadFail(Str path, DWORD err) {
    i64 sizeAfter = file::GetSize(path);
    bool existsAfter = file::Exists(path);
    logf("LoadLibsumatrapdfFromFile: after fail path='%s' stillExists=%d size=%lld err=%u\n", path, (int)existsAfter,
         (long long)sizeAfter, err);
    if (existsAfter) {
        DWORD attrs = path::GetCachedAttributes(path);
        logf("LoadLibsumatrapdfFromFile: after fail attrs=0x%x\n", attrs);
    } else {
        // Typical when AV quarantines the DLL right after extract
        logf("LoadLibsumatrapdfFromFile: file missing after LoadLibrary fail (possible AV quarantine)\n");
    }
}

// Load libsumatrapdf.dll from path only if the file size matches expectedSize (the
// embedded/installer copy). Skips mismatched leftover DLLs from other builds.
// useLoadLibraryEx: LoadLibraryExW with LOAD_WITH_ALTERED_SEARCH_PATH (helps
// when AV hooks plain LoadLibrary, and for dependency search next to the DLL).
static bool LoadLibsumatrapdfFromFile(Str path, i64 expectedSize, bool useLoadLibraryEx = false) {
    i64 realSize = file::GetSize(path);
    if (realSize != expectedSize) {
        if (realSize >= 0) {
            logf("LoadLibsumatrapdfFromFile: skip '%s' (size %lld, expected %lld)\n", path, (long long)realSize,
                 (long long)expectedSize);
        }
        return false;
    }
    const WCHAR* wpath = CWStrTemp(path);
    if (useLoadLibraryEx) {
        gLibsumatrapdfDll = LoadLibraryExW(wpath, nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
    } else {
        gLibsumatrapdfDll = LoadLibraryW(wpath);
    }
    if (!gLibsumatrapdfDll) {
        // capture before logf, which can overwrite GetLastError
        DWORD err = GetLastError();
        gLibsumatrapdfLastLoadError = err;
        if (useLoadLibraryEx) {
            logf("LoadLibsumatrapdfFromFile: failed to load '%s' (LoadLibraryExW) err=%u\n", path, err);
        } else {
            logf("LoadLibsumatrapdfFromFile: failed to load '%s' err=%u\n", path, err);
        }
        LogLastError(err);
        // Log whether the file still exists — AV often deletes/quarantines it
        // between a successful write and LoadLibrary (see debug reports).
        LogLibsumatrapdfFileStateAfterLoadFail(path, err);
        SetLastError(err); // so callers' GetLastError() still see LoadLibrary's error
        return false;
    }
    gLibsumatrapdfLastLoadError = 0;
    if (useLoadLibraryEx) {
        logf("LoadLibsumatrapdf: loaded '%s' (LoadLibraryExW)\n", path);
    } else {
        logf("LoadLibsumatrapdf: loaded '%s'\n", path);
    }
    return true;
}

// LoadLibraryW with short retry, optional longer AV-race backoff, then
// LoadLibraryExW. justWritten: we extracted the DLL moments ago (AV most active).
// Missing / wrong-size files fail immediately — no Sleep. Sleeps are only for real
// LoadLibrary failures (AV race). Otherwise portable startup pays ~1s every launch
// when the DLL is not next to the exe (issue #5849).
static bool LoadLibsumatrapdfFromFileRobust(Str path, i64 expectedSize, bool justWritten = false) {
    i64 realSize = file::GetSize(path);
    if (realSize != expectedSize) {
        return false;
    }

    if (LoadLibsumatrapdfFromFile(path, expectedSize)) {
        return true;
    }

    logf("LoadLibsumatrapdfFromFileRobust: retry after 1s '%s'\n", path);
    Sleep(1000);
    if (LoadLibsumatrapdfFromFile(path, expectedSize)) {
        return true;
    }

    // Longer backoff when AV may still be scanning a just-written DLL
    // (errors 5 / 32 / 87 are common in those reports).
    DWORD err = gLibsumatrapdfLastLoadError;
    if (justWritten || IsLibsumatrapdfAvRaceError(err)) {
        static const int kAvBackoffMs[] = {2000, 3000, 5000};
        for (int waitMs : kAvBackoffMs) {
            logf("LoadLibsumatrapdfFromFileRobust: AV-race retry after %dms (err=%u) '%s'\n", waitMs, err, path);
            Sleep(waitMs);
            if (LoadLibsumatrapdfFromFile(path, expectedSize)) {
                return true;
            }
            err = gLibsumatrapdfLastLoadError;
        }
    }

    logf("LoadLibsumatrapdfFromFileRobust: trying LoadLibraryExW '%s'\n", path);
    if (LoadLibsumatrapdfFromFile(path, expectedSize, true)) {
        return true;
    }

    if (justWritten || IsLibsumatrapdfAvRaceError(gLibsumatrapdfLastLoadError)) {
        logf("LoadLibsumatrapdfFromFileRobust: final AV-race LoadLibraryExW after 3s '%s'\n", path);
        Sleep(3000);
        return LoadLibsumatrapdfFromFile(path, expectedSize, true);
    }
    return false;
}

// Extract embedded libsumatrapdf.dll into dir (if size mismatches), then load with
// retry / LoadLibraryExW. Used for single-exe portable layout.
static bool ExtractAndLoadLibsumatrapdfRobust(Str dir, bool extract) {
    if (!dir) {
        return false;
    }
    i64 expectedSize = GetEmbeddedLibsumatrapdfSize();
    if (expectedSize < 0) {
        return false;
    }
    TempStr path = path::JoinTemp(dir, StrL("libsumatrapdf.dll"));
    i64 realSize = file::GetSize(path);
    bool justWritten = false;
    if (realSize != expectedSize) {
        if (realSize >= 0) {
            logf("ExtractAndLoadLibsumatrapdfRobust: overwriting '%s' (size %lld, expected %lld)\n", path,
                 (long long)realSize, (long long)expectedSize);
        }
        if (!extract) {
            // No DLL (or wrong build) here and we must not write — try next candidate.
            return false;
        }
        if (!ExtractLibsumatrapdfToDir(dir)) {
            logf("ExtractAndLoadLibsumatrapdfRobust: ExtractLibsumatrapdfToDir failed for '%s'\n", dir);
            return false;
        }
        justWritten = true;
    }
    return LoadLibsumatrapdfFromFileRobust(path, expectedSize, justWritten);
}

// Log as much as we can about a failed load; ends up in the debug report via gLogBuf.
static void LogLibsumatrapdfLoadFailureDiagnostics(Str selfDir, Str buildDir, i64 expectedSize, DWORD lastErr) {
    TempStr exePath = GetSelfExePathTemp();
    logf("LoadLibsumatrapdf FAILED: lastError=%u (0x%x) gSingleExe=%d expectedSize=%lld\n", lastErr, lastErr,
         (int)gSingleExe, (long long)expectedSize);
    logf("LoadLibsumatrapdf FAILED: exe='%s'\n", exePath);
    logf("LoadLibsumatrapdf FAILED: selfDir='%s'\n", selfDir);
    logf("LoadLibsumatrapdf FAILED: buildDir='%s'\n", buildDir);
    logf("LoadLibsumatrapdf FAILED: gLibsumatrapdfDll=%p\n", (void*)gLibsumatrapdfDll);

    auto logDllCandidate = [](Str dir, Str label) {
        if (!dir) {
            logf("LoadLibsumatrapdf FAILED: %s dir is null\n", label);
            return;
        }
        TempStr path = path::JoinTemp(dir, StrL("libsumatrapdf.dll"));
        i64 size = file::GetSize(path);
        bool exists = file::Exists(path);
        logf("LoadLibsumatrapdf FAILED: %s path='%s' exists=%d size=%lld\n", label, path, (int)exists, (long long)size);
        if (exists) {
            DWORD attrs = path::GetCachedAttributes(path);
            logf("LoadLibsumatrapdf FAILED: %s attrs=0x%x\n", label, attrs);
        }
    };
    logDllCandidate(selfDir, StrL("selfDir"));
    logDllCandidate(buildDir, StrL("buildDir"));

    // FormatMessage for the last LoadLibrary error when available
    if (lastErr != 0) {
        WCHAR* msgBuf = nullptr;
        DWORD n =
            FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                           nullptr, lastErr, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPWSTR)&msgBuf, 0, nullptr);
        if (n > 0 && msgBuf) {
            logf("LoadLibsumatrapdf FAILED: FormatMessage: %s\n", ToUtf8Temp(WStr(msgBuf)));
            LocalFree(msgBuf);
        }
    }
}

static bool LoadLibsumatrapdf(bool showErrorDialog) {
    // Static-linked builds (mingw wine cross-build, MSVC SumatraPDF-static) do
    // not embed/delay-load libsumatrapdf.dll — MuPDF is already in the exe image.
    // HasEmbeddedLibsumatrapdf() is false when IDR_DLL_PAK has no libsumatrapdf.dll entry.
    if (!HasEmbeddedLibsumatrapdf()) {
        logf("LoadLibsumatrapdf: no embedded libsumatrapdf.dll (static build); nothing to load\n");
        return true;
    }

    i64 expectedSize = GetEmbeddedLibsumatrapdfSize();
    Str selfDir = GetSelfExeDirTemp();
    Str buildDir = GetSumatraBuildSpecificDirTemp();
    logf("LoadLibsumatrapdf: start expectedSize=%lld gSingleExe=%d selfDir='%s' buildDir='%s'\n",
         (long long)expectedSize, (int)gSingleExe, selfDir, buildDir);

    // prefer existing libsumatrapdf.dll next to the exe (installer layout)
    if (ExtractAndLoadLibsumatrapdfRobust(selfDir, false)) {
        return true;
    }

    // Portable / single-exe: extract + robust load from build data dir, then
    // from the exe directory (in case AppData is blocked by AV).
    if (gSingleExe) {
        if (ExtractAndLoadLibsumatrapdfRobust(buildDir, true)) {
            return true;
        }
        if (ExtractAndLoadLibsumatrapdfRobust(selfDir, true)) {
            return true;
        }
    }

    // A later attempt can fail while an earlier LoadLibrary still holds a handle.
    if (gLibsumatrapdfDll) {
        logf("LoadLibsumatrapdf: gLibsumatrapdfDll already set after attempts; treating as success\n");
        return true;
    }

    DWORD err = gLibsumatrapdfLastLoadError ? gLibsumatrapdfLastLoadError : GetLastError();
    LogLibsumatrapdfLoadFailureDiagnostics(selfDir, buildDir, expectedSize, err);

    // Upload a debug report (pre-release) without requiring symbols. captureCallstack
    // is false so we skip symbol download; the log (paths/sizes/errors) is the payload.
    _uploadDebugReport(StrL("LoadLibsumatrapdf failed"), FILE_LINE, false, false);

    if (!showErrorDialog) {
        // e.g. -print-to ... -silent invoked by another program:
        // a modal dialog would hang the caller
        return false;
    }

    // Keep this English-only: we may fail before translations / UI language load.
    TempStr msg;
    if (IsLibsumatrapdfAppControlError(err)) {
        msg = fmt(
            R"(SumatraPDF failed to load libsumatrapdf.dll (error code %d).

Windows Application Control blocked the file (WDAC, AppLocker, or Smart App Control). The DLL is present on disk but the OS policy will not allow it to load.

Try:
• Settings → Privacy & security → Windows Security → App & browser control: check Smart App Control and reputation-based protection
• If this is a work or school PC, ask IT to allowlist SumatraPDF.exe and libsumatrapdf.dll
• Unblock the files (right-click → Properties → Unblock) if they show "downloaded from the Internet"
• Move the portable folder out of Downloads and try again, or install from the official website

For more information see <a href="%s">Failed to load libsumatrapdf.dll</a>.)",
            (int)err, kFailedToLoadURL());
    } else {
        msg = fmt(
            R"(SumatraPDF failed to load libsumatrapdf.dll (error code %d).

This is often caused by antivirus software blocking or quarantining libsumatrapdf.dll when SumatraPDF extracts it.

Try:
• Add an exclusion for the SumatraPDF folder and %%LocalAppData%%\SumatraPDF-data
• Temporarily disable real-time antivirus protection, then start SumatraPDF again
• Re-download SumatraPDF from the official website

For more information see <a href="%s">Failed to load libsumatrapdf.dll</a>.)",
            (int)err, kFailedToLoadURL());
    }

    TASKDIALOG_BUTTON buttons[2];
    buttons[0].nButtonID = IDOK;
    buttons[0].pszButtonText = L"Ok";
    buttons[1].nButtonID = kBtnIdLearnMore;
    buttons[1].pszButtonText = L"Learn more";

    TASKDIALOGCONFIG dialogConfig{};
    TASKDIALOG_FLAGS flags = TDF_ALLOW_DIALOG_CANCELLATION | TDF_SIZE_TO_CONTENT | TDF_ENABLE_HYPERLINKS;
    if (trans::IsCurrLangRtl()) {
        flags |= TDF_RTL_LAYOUT;
    }
    dialogConfig.cbSize = sizeof(TASKDIALOGCONFIG);
    dialogConfig.pszWindowTitle = L"SumatraPDF";
    dialogConfig.pszMainInstruction = L"Failed to load libsumatrapdf.dll";
    dialogConfig.pszContent = CWStrTemp(msg);
    dialogConfig.nDefaultButton = IDOK;
    dialogConfig.dwFlags = flags;
    dialogConfig.pfCallback = LoadLibsumatrapdfDialogCallback;
    dialogConfig.pButtons = buttons;
    dialogConfig.cButtons = 2;
    dialogConfig.pszMainIcon = TD_ERROR_ICON;

    TaskDialogIndirect(&dialogConfig, nullptr, nullptr, nullptr);
    return false;
}

// TODO: maybe could set font on TDN_CREATED to Consolas, to better show the message
static HRESULT CALLBACK TaskdialogHandleLinkscallback(HWND /*hwnd*/, UINT msg, WPARAM /*wParam*/, LPARAM lParam,
                                                      LONG_PTR /*lpRefData*/) {
    switch (msg) {
        case TDN_HYPERLINK_CLICKED:
            LaunchBrowser(ToUtf8Temp(WStr((wchar_t*)lParam)));
            break;
    }
    return S_OK;
}

// !gSingleExe (installer build): a single exe is both an installer and the app
// (once libsumatrapdf.dll has been extracted next to it). If we don't find
// libsumatrapdf.dll alongside us, assume this is the installer. If it's present but a
// different size than ours, it's a damaged installation.
static bool ForceRunningAsInstaller() {
    if (!HasEmbeddedLibsumatrapdf()) {
        // this is not a version that needs libsumatrapdf.dll
        return false;
    }

    i64 expectedSize = GetEmbeddedLibsumatrapdfSize();
    ReportIf(expectedSize < 0);
    if (expectedSize < 0) {
        // shouldn't happen
        return false;
    }

    TempStr dir = GetSelfExeDirTemp();
    TempStr path = path::JoinTemp(dir, StrL("libsumatrapdf.dll"));
    auto realSize = file::GetSize(path);
    if (realSize < 0) {
        return true;
    }
    if (realSize == expectedSize) {
        return false;
    }

    Str corruptedInstallationConsole = R"(
Looks like corrupted installation of SumatraPDF.

Learn more at https://www.sumatrapdfreader.org/docs/Corrupted-installation
)";
    Str corruptedInstallation = R"(Looks like corrupted installation of SumatraPDF.
)";
    bool ok = RedirectIOToExistingConsole();
    if (ok) {
        // if we're launched from console, print help to consle window
        printf("%s", corruptedInstallationConsole.s);
    }

    const auto* title = L"SumatraPDF installer";
    TASKDIALOGCONFIG dialogConfig{};

    DWORD flags =
        TDF_ALLOW_DIALOG_CANCELLATION | TDF_SIZE_TO_CONTENT | TDF_ENABLE_HYPERLINKS | TDF_POSITION_RELATIVE_TO_WINDOW;
    if (trans::IsCurrLangRtl()) {
        flags |= TDF_RTL_LAYOUT;
    }
    dialogConfig.cbSize = sizeof(TASKDIALOGCONFIG);
    dialogConfig.pszWindowTitle = title;
    dialogConfig.pszMainInstruction = CWStrTemp(corruptedInstallation);
    dialogConfig.pszContent =
        LR"(Learn more at <a href="https://www.sumatrapdfreader.org/docs/Corrupted-installation">www.sumatrapdfreader.org/docs/Corrupted-installation</a>.)";
    dialogConfig.nDefaultButton = IDOK;
    dialogConfig.dwFlags = (TASKDIALOG_FLAGS)flags;
    dialogConfig.cxWidth = 0;
    dialogConfig.pfCallback = TaskdialogHandleLinkscallback;
    dialogConfig.dwCommonButtons = TDCBF_CLOSE_BUTTON;
    dialogConfig.pszMainIcon = TD_ERROR_ICON;

    TaskDialogIndirect(&dialogConfig, nullptr, nullptr, nullptr);
    HandleRedirectedConsoleOnShutdown();
    ::ExitProcess(1);
}

static Str kInstallerHelpTmpl() {
    return StrL(R"(${appName} installer options:
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
)");
}

static void ShowInstallerHelp() {
    // Note: translation services aren't initialized at this point, so English only
    TempStr msg = str::ReplaceTemp(kInstallerHelpTmpl(), StrL("${appName}"), kAppName);

    bool ok = RedirectIOToExistingConsole();
    if (ok) {
        // if we're launched from console, print help to consle window
        printf("%s\n%s\n", msg.s, "See more at https://www.sumatrapdfreader.org/docs/Installer-cmd-line-arguments");
        return;
    }

    TASKDIALOGCONFIG dialogConfig{};

    DWORD flags =
        TDF_ALLOW_DIALOG_CANCELLATION | TDF_POSITION_RELATIVE_TO_WINDOW | TDF_SIZE_TO_CONTENT | TDF_ENABLE_HYPERLINKS;
    if (trans::IsCurrLangRtl()) {
        flags |= TDF_RTL_LAYOUT;
    }
    dialogConfig.cbSize = sizeof(TASKDIALOGCONFIG);
    dialogConfig.pszWindowTitle = L"SumatraPDF installer usage";
    dialogConfig.pszMainInstruction = CWStrTemp(msg);
    dialogConfig.pszContent =
        LR"(<a href="https://www.sumatrapdfreader.org/docs/Installer-cmd-line-arguments">Read more on website</a>)";
    dialogConfig.nDefaultButton = IDOK;
    dialogConfig.dwFlags = (TASKDIALOG_FLAGS)flags;
    dialogConfig.pfCallback = TaskdialogHandleLinkscallback;
    dialogConfig.dwCommonButtons = TDCBF_OK_BUTTON;
    dialogConfig.pszMainIcon = TD_INFORMATION_ICON;

    TaskDialogIndirect(&dialogConfig, nullptr, nullptr, nullptr);
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
    MsgBox(nullptr, _TRA("Not a valid installer"), _TRA("Error"), MB_OK | MB_ICONERROR);
}

// delete locally cached copies of cbx files that haven't been opened in a
// while. We cache network-drive cbx archives under <data>/cbx-cache/ to
// avoid slow re-reads; they're pure cache so evicting cold entries is
// safe.
static void DeleteStaleCbxCacheFiles() {
    TempStr dataDir = GetSumatraDataDirTemp();
    if (!dataDir) {
        return;
    }
    TempStr cacheDir = path::JoinTemp(dataDir, StrL("cbx-cache"));
    if (path::GetType(cacheDir) != path::Type::Dir) {
        return;
    }

    constexpr i64 kMaxAgeSec = 7LL * 24 * 60 * 60;
    FILETIME nowFt;
    GetSystemTimeAsFileTime(&nowFt);
    ULARGE_INTEGER now;
    now.LowPart = nowFt.dwLowDateTime;
    now.HighPart = nowFt.dwHighDateTime;

    DirIter di{cacheDir};
    di.includeFiles = true;
    di.includeDirs = false;
    for (DirIterEntry* de : di) {
        TempStr ext = path::GetExtTemp(de->name);
        bool isCbx = str::EqI(ext, StrL(".cbx")) || str::EqI(ext, StrL(".cbz")) || str::EqI(ext, StrL(".cbr")) ||
                     str::EqI(ext, StrL(".cb7")) || str::EqI(ext, StrL(".cbt"));
        if (!isCbx) {
            continue;
        }
        FILETIME atime = de->accessTime;
        ULARGE_INTEGER a;
        a.LowPart = atime.dwLowDateTime;
        a.HighPart = atime.dwHighDateTime;
        // FILETIME is 100-ns ticks since 1601; convert delta to seconds.
        i64 ageSec = (i64)((now.QuadPart - a.QuadPart) / 10000000ULL);
        if (ageSec < kMaxAgeSec) {
            continue;
        }
        bool ok = file::Delete(de->filePath);
        logf("DeleteStaleCbxCacheFiles: delete '%s' (age %lld days) -> %d\n", de->filePath,
             (long long)(ageSec / (24LL * 60 * 60)), (int)ok);
    }
}

static i64 FileTimeAgeSec(FILETIME ft, const ULARGE_INTEGER& now) {
    ULARGE_INTEGER t;
    t.LowPart = ft.dwLowDateTime;
    t.HighPart = ft.dwHighDateTime;
    if (t.QuadPart == 0) {
        return -1;
    }
    return (i64)((now.QuadPart - t.QuadPart) / 10000000ULL);
}

static bool IsBuildDirName(Str name) {
    if (name.len != 6) {
        return false;
    }
    for (int i = 0; i < name.len; i++) {
        char c = name.s[i];
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) {
            return false;
        }
    }
    return true;
}

// returns age in seconds of the dir's last activity, or -1 if unknown.
// prefers the directory mtime; falls back to the newest file mtime inside.
static i64 GetDirLastActivityAgeSec(Str dirPath, const ULARGE_INTEGER& now) {
    WIN32_FILE_ATTRIBUTE_DATA fileInfo{};
    WCHAR* dirPathW = CWStrTemp(dirPath);
    if (GetFileAttributesExW(dirPathW, GetFileExInfoStandard, &fileInfo)) {
        i64 age = FileTimeAgeSec(fileInfo.ftLastWriteTime, now);
        if (age >= 0) {
            return age;
        }
    }

    i64 newestAge = -1;
    DirIter di{dirPath};
    di.includeFiles = true;
    di.includeDirs = false;
    di.recurse = true;
    for (DirIterEntry* de : di) {
        i64 age = FileTimeAgeSec(de->modificationTime, now);
        if (age < 0) {
            continue;
        }
        if (newestAge < 0 || age < newestAge) {
            newestAge = age;
        }
    }
    return newestAge;
}

// delete stale build dirs and legacy top-level dirs from previous layouts
// PdfPreview.dll writes one log per preview-host session when logging is enabled
// but, being a restricted shell host, doesn't prune them -- so SumatraPDF.exe
// keeps only the newest `keep` here (the logs live in our per-build data dir).
struct PreviewLogFile {
    Str path;
    FILETIME ft;
};
static int CmpPreviewLogNewestFirst(const PreviewLogFile* a, const PreviewLogFile* b) {
    return -CompareFileTime(&a->ft, &b->ft); // newest (largest time) first
}
static void DeleteOldPdfPreviewLogs(int keep) {
    TempStr dir = GetPdfPreviewLogDirTemp();
    if (!dir || !dir::Exists(dir)) {
        return;
    }
    Vec<PreviewLogFile> files;
    DirIter di{dir};
    di.includeFiles = true;
    di.includeDirs = false;
    for (DirIterEntry* de : di) {
        if (!str::StartsWith(de->name, kPdfPreviewLogPrefix)) {
            continue;
        }
        PreviewLogFile lf{str::Dup(de->filePath), de->modificationTime};
        files.Append(lf);
    }
    int n = len(files);
    if (n > keep) {
        VecSort(files, CmpPreviewLogNewestFirst);
        for (int i = keep; i < n; i++) {
            logf("DeleteOldPdfPreviewLogs: deleting '%s'\n", files[i].path);
            file::Delete(files[i].path);
        }
    }
    for (PreviewLogFile& lf : files) {
        str::Free(lf.path);
    }
}

static void DeleteStaleFilesAsync() {
    DeleteStaleCbxCacheFiles();
    DeleteOldPdfPreviewLogs(32);

    if (!(gIsPreReleaseBuild || gIsDebugBuild)) {
        return;
    }
    TempStr dataDir = GetSumatraDataDirTemp();
    if (!dataDir) {
        return;
    }
    logf("DeleteStaleFilesAsync: dataDir: '%s'\n", dataDir);

    FILETIME nowFt;
    GetSystemTimeAsFileTime(&nowFt);
    ULARGE_INTEGER now;
    now.LowPart = nowFt.dwLowDateTime;
    now.HighPart = nowFt.dwHighDateTime;
    constexpr i64 kMaxAgeSec = 14LL * 24 * 60 * 60;

    DirIter di{dataDir};
    di.includeFiles = false;
    di.includeDirs = true;
    for (DirIterEntry* de : di) {
        Str name = de->name;
        if (str::Eq(name, StrL("cbx-cache"))) {
            continue;
        }

        bool isLegacy = str::StartsWith(name, StrL("manual-")) || str::StartsWith(name, StrL("crashinfo-"));
        bool isBuildDir = IsBuildDirName(name);
        if (!isLegacy && !isBuildDir) {
            logf("DeleteStaleFilesAsync: skipping '%s'\n", name);
            continue;
        }

        if (isBuildDir) {
            i64 ageSec = GetDirLastActivityAgeSec(de->filePath, now);
            if (ageSec < 0) {
                logf("DeleteStaleFilesAsync: skipping '%s', couldn't determine age\n", de->filePath);
                continue;
            }
            if (ageSec < kMaxAgeSec) {
                logf("DeleteStaleFilesAsync: skipping '%s' (age %lld days)\n", de->filePath,
                     (long long)(ageSec / (24LL * 60 * 60)));
                continue;
            }
            logf("DeleteStaleFilesAsync: deleting stale build dir '%s' (age %lld days)\n", de->filePath,
                 (long long)(ageSec / (24LL * 60 * 60)));
        } else {
            logf("DeleteStaleFilesAsync: deleting legacy dir '%s'\n", de->filePath);
        }

        bool ok = dir::RemoveAll(de->filePath);
        logf("DeleteStaleFilesAsync: dir::RemoveAll('%s') returned %d\n", de->filePath, ok);
    }
}

static void LayoutAndFocusOnStartup(MainWindow* win) {
    if (!win || !IsWindow(win->hwndFrame)) {
        return;
    }
    ScheduleUiUpdate(win);
    win->Focus();
}

static int WineDpiFromEnv() {
    static Str scaleVars[] = {StrL("GDK_SCALE"), StrL("QT_SCALE_FACTOR"), StrL("ELM_SCALE")};
    int bestDpi = 0;
    for (Str var : scaleVars) {
        Str val = Str(getenv(var.s));
        if (!val) {
            continue;
        }
        float scale = (float)atof(val.s);
        if (scale < 1.f) {
            continue;
        }
        int dpi = (int)lroundf(96.f * scale);
        bestDpi = std::max(dpi, bestDpi);
    }
    return bestDpi;
}

static void LogWineDpiInfo() {
    if (!IsRunningOnWine()) {
        return;
    }
    HDC screenDC = GetDC(nullptr);
    int screenDpi = screenDC ? GetDeviceCaps(screenDC, LOGPIXELSX) : 0;
    if (screenDC) {
        ReleaseDC(nullptr, screenDC);
    }
    uint monitorDpiX = 0, monitorDpiY = 0;
    HMONITOR mon = MonitorFromWindow(nullptr, MONITOR_DEFAULTTOPRIMARY);
    HRESULT hr = E_FAIL;
    if (mon && DynGetDpiForMonitor) {
        hr = DynGetDpiForMonitor(mon, 0, &monitorDpiX, &monitorDpiY);
    }
    int envDpi = WineDpiFromEnv();
    if (envDpi > 0) {
        DpiSetWineOverride(envDpi);
    }
    logf(
        "WineDpi: screenDpi=%d monitorDpi=(%u,%u) GetDpiForMonitor hr=0x%lx envDpi=%d overrideDpi=%d "
        "SM_CYCAPTION=%d SM_CYFRAME=%d SM_CXPADDEDBORDER=%d screen=(%d,%d)\n",
        screenDpi, monitorDpiX, monitorDpiY, (unsigned long)hr, envDpi, DpiGetForHwnd(HWND_DESKTOP),
        GetSystemMetrics(SM_CYCAPTION), GetSystemMetrics(SM_CYFRAME), GetSystemMetrics(SM_CXPADDEDBORDER),
        GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN));
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
    TempStr fileName = path::GetBaseNameTemp(__FILE__);
    WCHAR* gswin32c = L"this is a path";
    WCHAR* tmpFile = L"c:\foo\bar.txt";
    auto gswin = ToUtf8Temp(gswin32c);
    auto tmpFileName = ToUtf8Temp(path::GetBaseNameTemp(tmpFile));
    logf("- %s:%d: using '%s' for creating '%%TEMP%%\\%s'\n", fileName, __LINE__, gswin.Get(), tmpFileName.Get());
}
#endif

// in mupdf_load_system_font.c
extern "C" void destroy_system_font_list();
extern void DeleteManualBrowserWindow();

extern "C" {
int muconvert_main(int argc, char* argv[]);
int mudraw_main(int argc, char* argv[]);
int mutrace_main(int argc, char* argv[]);
int murun_main(int argc, char* argv[]);

int pdfclean_main(int argc, char* argv[]);
int pdfextract_main(int argc, char* argv[]);
int pdfinfo_main(int argc, char* argv[]);
int pdfposter_main(int argc, char* argv[]);
int pdfshow_main(int argc, char* argv[]);
int pdfpages_main(int argc, char* argv[]);
int pdfcreate_main(int argc, char* argv[]);
int pdfmerge_main(int argc, char* argv[]);
int pdfsign_main(int argc, char* argv[]);
int pdfrecolor_main(int argc, char* argv[]);
int pdftrim_main(int argc, char* argv[]);
int pdfbake_main(int argc, char* argv[]);
int mubar_main(int argc, char* argv[]);
int mugrep_main(int argc, char* argv[]);

int cmapdump_main(int argc, char* argv[]);
int pdfaudit_main(int argc, char* argv[]);

char** fz_argv_from_wargv(int argc, wchar_t** wargv);
void fz_free_argv(int argc, char** argv);
int fz_redirect_io_to_existing_console();
}

// must match premake5.lua
#define FZ_ENABLE_JS 1
#define FZ_ENABLE_PDF 1
#define FZ_ENABLE_BARCODE 0
#define FZ_VERSION "1.27.2"

using MutoolFunc = int (*)(int argc, char* argv[]);

static MutoolFunc toolFuncs[] = {
#if FZ_ENABLE_JS
    murun_main,
#endif
    mudraw_main,   muconvert_main,
#if FZ_ENABLE_PDF
    pdfaudit_main, pdfbake_main,   pdfclean_main,   pdfcreate_main, pdfextract_main, pdfinfo_main, pdfmerge_main,
    pdfpages_main, pdfposter_main, pdfrecolor_main, pdfshow_main,   pdfsign_main,    pdftrim_main,
#endif
    mugrep_main,   mutrace_main,
#if FZ_ENABLE_BARCODE
    mubar_main,
#endif
};

static SeqStrings gToolNames =
#if FZ_ENABLE_JS
    "run\0"
#endif
    "draw\0"
    "convert\0"
#if FZ_ENABLE_PDF
    "audit\0"
    "bake\0"
    "clean\0"
    "create\0"
    "extract\0"
    "info\0"
    "merge\0"
    "pages\0"
    "poster\0"
    "recolor\0"
    "show\0"
    "sign\0"
    "trim\0"
#endif
    "grep\0"
    "trace\0"
#if FZ_ENABLE_BARCODE
    "barcode\0"
#endif
    "\0";

static SeqStrings gToolDescs =
#if FZ_ENABLE_JS
    "run javascript\0"
#endif
    "convert document\0"
    "convert document (with simpler options)\0"
#if FZ_ENABLE_PDF
    "produce usage stats from PDF files\0"
    "bake PDF form into static content\0"
    "rewrite PDF file\0"
    "create PDF document\0"
    "extract font and image resources\0"
    "show information about PDF resources\0"
    "merge pages from multiple PDF sources into a new PDF\0"
    "show information about PDF pages\0"
    "split large PDF page into many tiles\0"
    "change colorspace of PDF document\0"
    "show internal PDF objects\0"
    "manipulate PDF digital signatures\0"
    "trim PDF page contents\0"
#endif
    "search for text\0"
    "trace device calls\0"
#if FZ_ENABLE_BARCODE
    "encode/decode barcodes\0"
#endif
    "\0";

constexpr int kNoMutool = -1234321; // arbitrary negative value that won't collide with any mutool exit code

// Returns the index into gToolNames if the command line invokes one of the
// mupdf-derived command-line tools (e.g. `SumatraPDF.exe info file.pdf`), else
// -1. CommandLineToArgvW always returns the program path as argv[0], so the tool
// name (if any) is argv[1]. This must not depend on how the exe path was spelled
// on the command line (absolute, relative, short, just the name via PATH).
static int ToolIdxFromCmdLine() {
    int argc = 0;
    WCHAR** wargv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!wargv) {
        return -1;
    }
    int idx = -1;
    if (argc >= 2) {
        TempStr toolName = ToUtf8Temp(wargv[1]);
        idx = SeqStrIndexIS(gToolNames, toolName);
    }
    LocalFree((void*)wargv);
    return idx;
}

// true if the command line invokes a command-line tool. Used early to suppress
// console logging so it doesn't contaminate the tool's stdout (issue #5677).
static bool IsRunningTool() {
    if (ToolIdxFromCmdLine() >= 0) {
        return true;
    }

    int argc = 0;
    WCHAR** wargv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!wargv) {
        return false;
    }
    bool isTool = false;
    for (int i = 1; i < argc; i++) {
        if (wstr::EqI(wargv[i], WStrL(L"-dump-chm")) || wstr::EqI(wargv[i], WStrL(L"-dump-exif"))) {
            isTool = true;
            break;
        }
    }
    LocalFree((void*)wargv);
    return isTool;
}

// returns the pointer into the command line just past the first argument
// (our own program path), i.e. the args to forward to the child. Mirrors how
// CommandLineToArgvW parses argv[0]: if quoted, up to the closing quote;
// otherwise up to the first whitespace.
static WStr SkipFirstArg(WStr cmdLine) {
    int s = 0;
    int end = cmdLine.len;
    while (s < end && (cmdLine.s[s] == L' ' || cmdLine.s[s] == L'\t')) {
        s++;
    }
    if (s < end && cmdLine.s[s] == L'"') {
        s++;
        while (s < end && cmdLine.s[s] != L'"') {
            s++;
        }
        if (s < end && cmdLine.s[s] == L'"') {
            s++;
        }
    } else {
        while (s < end && cmdLine.s[s] != L' ' && cmdLine.s[s] != L'\t') {
            s++;
        }
    }
    while (s < end && (cmdLine.s[s] == L' ' || cmdLine.s[s] == L'\t')) {
        s++;
    }
    return WStr(cmdLine.s + s, end - s);
}

// When this is SumatraPDF-dll.exe (carries embedded installer resources) and is
// properly installed - libsumatrapdf.dll and sumatrapdf-tool.exe sit next to it - run
// the requested tool by launching the console sumatrapdf-tool.exe instead of
// running it in-process. This is a GUI (Windows subsystem) exe and interacts
// poorly with cmd.exe / PowerShell; the console exe doesn't. The child inherits
// our standard handles, so console output and redirection (`> out.txt`, `| more`)
// keep working. Returns the child's exit code, or kNoMutool when delegation
// doesn't apply (the caller then runs the tool in-process). The static
// SumatraPDF.exe (no embedded resources) always runs the tool in-process.
static int MaybeDelegateToToolExe() {
    if (!HasEmbeddedLibsumatrapdf()) {
        return kNoMutool;
    }
    TempStr toolExe = GetPathInExeDirTemp("sumatrapdf-tool.exe");
    TempStr libsumatrapdf = GetPathInExeDirTemp("libsumatrapdf.dll");
    if (!file::Exists(toolExe) || !file::Exists(libsumatrapdf)) {
        return kNoMutool;
    }

    // Capture the standard handles cmd.exe set up for us *before* attaching to a
    // console. A stream redirected to a file/pipe (`> out.txt`, `| more`) already
    // has a valid handle here; we must keep it so the child writes there.
    HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    HANDLE hErr = GetStdHandle(STD_ERROR_HANDLE);
    // Attach to the parent (cmd.exe) console so the *non-redirected* streams have
    // somewhere to go; harmless (and a no-op) when there's no parent console.
    AttachConsole(ATTACH_PARENT_PROCESS);
    auto isInvalid = [](HANDLE h) { return h == nullptr || h == INVALID_HANDLE_VALUE; };
    if (isInvalid(hIn)) {
        hIn = GetStdHandle(STD_INPUT_HANDLE);
    }
    if (isInvalid(hOut)) {
        hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    }
    if (isInvalid(hErr)) {
        hErr = GetStdHandle(STD_ERROR_HANDLE);
    }

    WStr rest = SkipFirstArg(WStr(GetCommandLineW()));
    TempStr cmd = fmt("\"%s\" %s", toolExe, ToUtf8Temp(rest));
    WCHAR* cmdW = CWStrTemp(cmd);
    WCHAR* toolExeW = CWStrTemp(toolExe);

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = hIn;
    si.hStdOutput = hOut;
    si.hStdError = hErr;

    PROCESS_INFORMATION pi{};
    BOOL ok = CreateProcessW(toolExeW, cmdW, nullptr, nullptr, TRUE /*inherit handles*/, 0, nullptr, nullptr, &si, &pi);
    if (!ok) {
        logf("MaybeDelegateToToolExe: CreateProcessW failed for '%s'\n", toolExe);
        return kNoMutool; // fall back to running the tool in-process
    }
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD exitCode = 1;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return (int)exitCode;
}

static int MaybeRunMutool() {
    int argc = 0;
    char** argv = nullptr;
    int res = kNoMutool;

    WCHAR** wargv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!wargv) {
        // Very rare – allocation failure or severely malformed command line
        log("CommandLineToArgvW() failed\n");
        LogLastError();
        return kNoMutool;
    }
    WCHAR** wargvOrig = wargv;
    // argv[0] is always the program path; the tool name (if any) is argv[1]
    if (argc < 2) {
        goto Exit;
    }
    argc--;
    wargv++;

    argv = fz_argv_from_wargv(argc, wargv);
    {
        Str toolName = Str(argv[0]);
        int idx = SeqStrIndexIS(gToolNames, toolName);
        if (idx >= 0) {
            // PowerShell pipes a GUI app's stdout through a pipe that the CRT
            // can't write to ("cannot fwrite: Invalid argument"), so the tool's
            // output is silently lost. Detect it and tell the user how to run it
            // properly instead of producing broken/empty output (issue #5677).
            // Emit the message via raw WriteFile (CRT fwrite is what's broken
            // here) to the inherited stderr handle so it survives the bad pipe.
            if (WasLaunchedByPowershellWithPipeRedirect()) {
                static Str msg = R"(SumatraPDF: command-line tools don't work when their output is redirected by
PowerShell (e.g. `SumatraPDF.exe info file.pdf > out.txt` or `... | more`).
PowerShell pipes a GUI app's output through a pipe that drops the data.

Run the command from cmd.exe instead, e.g.:
    cmd /c "SumatraPDF.exe info file.pdf > out.txt"
)";
                HANDLE hErr = GetStdHandle(STD_ERROR_HANDLE);
                if (hErr && hErr != INVALID_HANDLE_VALUE) {
                    DWORD written = 0;
                    WriteFile(hErr, msg.s, (DWORD)len(msg), &written, nullptr);
                }
                res = 1;
                goto Exit;
            }
            // in the installed dll build, hand off to the console
            // sumatrapdf-tool.exe (which interacts properly with the console)
            int delegated = MaybeDelegateToToolExe();
            if (delegated != kNoMutool) {
                res = delegated;
                goto Exit;
            }
            fz_redirect_io_to_existing_console();
            res = toolFuncs[idx](argc, argv);
            goto Exit;
        }
    }

Exit:
    if (wargvOrig) {
        LocalFree((void*)wargvOrig);
    }
    if (argv) fz_free_argv(argc, argv);
    return res;
}

static void LogCommandLine() {
    TempStr s = ToUtf8Temp(GetCommandLineW());
    logf("'%s'\n  ver %s\n", Str(s), StrL(UPDATE_CHECK_VERA));
    LogParentProcessChain();
}

static void InstallSumatraCrashHandler(bool localOnly) {
    if (gIsAsanBuild) {
        return;
    }

    TempStr crashInfoDir = GetCrashInfoDirTemp();
    TempStr crashDumpPath = path::JoinTemp(crashInfoDir, StrL("sumatrapdfcrash.dmp"));
    TempStr crashFilePath = path::JoinTemp(crashInfoDir, StrL("sumatrapdfcrash.txt"));
    TempStr symDir = localOnly ? GetSelfExeDirTemp() : crashInfoDir;
    InstallCrashHandler(crashDumpPath, crashFilePath, symDir, localOnly);
}

int APIENTRY WinMain(_In_ HINSTANCE /*hInstance*/, _In_opt_ HINSTANCE /*hPrevInstance*/, _In_ LPSTR /*lpCmdLine*/,
                     _In_ int /*nCmdShow*/) {
    int exitCode = 1; // by default it's error
    int nWithDde = 0;
    MainWindow* win = nullptr;
    bool showStartPage = false;
    bool restoreSession = false;
    HANDLE hMutex = nullptr;
    HWND existingInstanceHwnd = nullptr;
    HWND existingHwnd = nullptr;
    bool openInNewWindow = false;
    WindowTab* tabToSelect = nullptr;
    Str logFilePath;
    bool logFileBecauseDebug = false;

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

    LogCommandLine();

    if (gIsAsanBuild) {
        TempStr asanOpts = GetEnvVariableTemp("ASAN_OPTIONS");
        logf("ASAN_OPTIONS: '%s'\n", asanOpts ? asanOpts : StrL("<not set>"));
    }

    // Before the store-installer return below: everything past this point may
    // uitask::Post(), and posting before Initialize() neither runs nor frees the
    // task (see the guard in uitask::Post). Only needs a message queue, so it can
    // run this early - ahead of Ole/common controls/GDI+/mui.
    uitask::Initialize();

    Flags flags;
    if (ExeHasNameOfStoreInstaller()) {
        InstallSumatraCrashHandler(false);
        logf("Running store installer\n");
        flags.install = true;
        flags.silent = true;
        flags.storeInstaller = true;
        gCli = &flags;
        int ret = RunInstaller();
        uitask::Destroy();
        return ret;
    }

    ParseFlags(GetPermArena(), GetCommandLineW(), flags, gToolNames);
    gCli = &flags;
    gForTesting = flags.forTesting;
    InstallSumatraCrashHandler(flags.forTesting || flags.controlPipeName);

    ScopedOle ole;
    if (FAILED(ole.hr)) {
        // the UI thread has to be an STA: PrintDlgEx, the shell file dialogs
        // and drag & drop all need one. Without it the Windows 11 unified print
        // dialog hangs inside PrintDlgExW instead of returning
        logf("WinMain: OleInitialize() failed with 0x%08x\n", (uint)ole.hr);
    }
    InitAllCommonControls();
    ScopedGdiPlus gdiPlus(true);

    // when running a command-line tool (e.g. `info file.pdf`), keep logging off
    // the console so it doesn't contaminate the tool's stdout (issue #5677)
    bool isTool = IsRunningTool();
    if (!IsDebuggerPresent() && !isTool) {
        // VSCode shows both debugger output and console out which doubles the logging
        // TODO: only if AttachConsole() succeeds?
        gLogToConsole = true;
    }
    logf("wine: %s\n", Str(IsRunningOnWine() ? "true" : "false"));
    logf("elevated: %d\n", (int)IsProcessRunningElevated());
    LogWineDpiInfo();
    {
        // the cursor is on the monitor the user launched from. GetForegroundWindow
        // / HWND_DESKTOP report the *primary* monitor, which made the first
        // layout 2.5× too big when starting on a 100% screen next to a 250%
        // primary (discussion #4831).
        POINT pt{};
        if (GetCursorPos(&pt)) {
            int dpi = DpiGetForPoint(pt.x, pt.y);
            DpiSet(dpi, dpi);
        } else {
            DpiSetFromHwnd(GetDesktopWindow());
        }
    }

    bool isInstaller = flags.install || flags.runInstallNow || flags.fastInstall || IsInstallerAndNamedAsSuch();
    if (flags.justExtractFiles) {
        isInstaller = false;
    }
    bool isUninstaller = flags.uninstall;
    bool noLogHere = isInstaller || isUninstaller;

    if (gCli->silent) {
        gLogToConsole = false;
    }

    // do this before running installer etc. so that we have disk / net permissions
    // (default policy is to disallow everything)
    InitializePolicies(flags.restrictedUse);

#if defined(DEBUG)
    if (false) {
        Str dir = "C:\\Users\\kjk\\Downloads";
        auto di = DirIter{dir};
        di.recurse = true;
        for (DirIterEntry* d : di) {
            logf("d->filePath: '%s'\n", d->filePath);
        }
    }
    if (false) {
        TempStr exePath = GetSelfExePathTemp();
        RunNonElevated(exePath);
        return 0;
    }
#endif

    // in debug build, default
    if (gIsDebugBuild) {
        if (!flags.logFile) {
            // from the perm arena like all other flag strings (~Flags frees nothing)
            TempStr dir = GetPathInExeDirTemp(StrL("sumlog.txt"));
            flags.logFile = str::Dup(GetPermArena(), dir);
            flags.log = true;
            logFileBecauseDebug = true;
        }
    }
    if (flags.log && !noLogHere) {
        if (flags.logFile) {
            logFilePath = path::NormalizeTemp(flags.logFile);
            dir::CreateForFile(logFilePath);
        } else {
            logFilePath = GetLogFilePathTemp();
        }
        // logFilePath is used after the message loop exits, long after
        // the temp allocator memory it points into has been reused
        logFilePath = str::Dup(logFilePath);
        if (logFilePath) {
            StartLogToFile(logFilePath, true);
            LogCommandLine();
            logf("wine: %s\n", Str(IsRunningOnWine() ? "true" : "false"));
            logf("elevated: %d\n", (int)IsProcessRunningElevated());
        }
        // gRedrawLog = true;
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

    if (flags.updateSelfTo) {
        logf(" flags.updateSelfTo: '%s'\n", flags.updateSelfTo);
        RedirectIOToExistingConsole();
        UpdateSelfTo(flags.updateSelfTo);
        if (flags.exitWhenDone) {
            fastExit = !gIsDebugBuild;
            goto Exit;
        }
    }

    if (flags.upgradeFrom) {
        logf(" flags.upgradeFrom: '%s'\n", flags.upgradeFrom);
        StartInstallerAutoUpgrade(flags.upgradeFrom);
        fastExit = true;
        goto Exit;
    }

    if (flags.deleteFile) {
        logf(" flags.deleteFile: '%s'\n", flags.deleteFile);
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

    if (flags.dumpChm) {
        gLogToConsole = false;
        return DumpChm(flags);
    }

    // must check before isInstaller becase isInstaller can be auto-deduced
    // from -installer.exe pattern in the name, so it would ignore explit -uninstall flag
    if (isUninstaller) {
        exitCode = RunUninstaller();
        ::ExitProcess(exitCode);
    }

    if (isInstaller) {
        if (!HasEmbeddedLibsumatrapdf()) {
            ShowNotValidInstallerError();
            return 1;
        }
        exitCode = RunInstaller();
        // exit immediately. for some reason exit handlers try to
        // pull in libsumatrapdf.dll which we don't have access to in the installer
        ::ExitProcess(exitCode);
    }

    // when not a single self-contained exe, a missing sibling libsumatrapdf.dll means
    // we're really the installer; run it (matches pre-single-exe behavior)
    if (!gSingleExe && ForceRunningAsInstaller() && !flags.dumpExif && !flags.dumpChm && !flags.engineDump &&
        !flags.unitTests) {
        logf("forcing running as an installer\n");
        exitCode = RunInstaller();
        // exit immediately. for some reason exit handlers try to
        // pull in libsumatrapdf.dll which we don't have access to in the installer
        ::ExitProcess(exitCode);
    }

#if defined(DEBUG)
    if (flags.unitTests) {
        CreateSumatraAcceleratorTable();
        exitCode = RunAppUnitTests();
        FreeAcceleratorTables();
        ShutdownCommon();
        return exitCode;
    }
#else
    if (flags.unitTests) {
        fprintf(stderr, "-unit-tests is only available in debug builds\n");
        return 1;
    }
#endif

    // -x extract only needs the embedded LzSA archive in the EXE — not a loaded
    // libsumatrapdf.dll. Do this before LoadLibsumatrapdf so Desktop/sandbox
    // Access Denied or AV blocking LoadLibrary cannot abort a pure extract
    // (see crash report 8c10917a4, WDAG/Sandbox + -x).
    // ParseFlags skips flag parsing when argv[1] is a mutool name (poster, …),
    // so poster’s own -x never sets justExtractFiles; MaybeRunMutool still runs
    // for tools after we load the DLL below.
    if (flags.justExtractFiles) {
        RedirectIOToExistingConsole();
        if (!HasEmbeddedLibsumatrapdf()) {
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

    // load libsumatrapdf.dll eagerly before any code path that might call into it.
    // if we let the delay-load helper do it and it fails, it raises a fatal
    // exception. this must remain after the installer / -x checks above because
    // during installation or extract libsumatrapdf.dll need not be loaded yet
    if (!LoadLibsumatrapdf(!flags.silent)) {
        ::ExitProcess(1);
    }

#if defined(DEBUG)
    if (flags.testExtractPage) {
        TestExtractPage(flags);
        ShutdownCommon();
        return 0;
    }
#endif

    if (flags.engineDump) {
        void EngineDump(const Flags& flags);
        EngineDump(flags);
        return 0;
    }

    if (flags.dumpExif) {
        gLogToConsole = false;
        DumpExif(flags);
        return 0;
    }

    if (flags.appdataDir) {
        SetAppDataDir(flags.appdataDir);
    }

#if defined(DEBUG)
    if (flags.testApp) {
        // in TestApp.cpp
        extern void TestApp();
        TestApp();
        return 0;
    }
    if (flags.testPlugin) {
        // in TestPlugin.cpp
        extern void TestPlugin(WStr cmdLine);
        TestPlugin(WStr(GetCommandLineW()));
        return 0;
    }
    if (flags.testPreview) {
        // in TestPreview.cpp
        extern void TestPreview(WStr cmdLine);
        TestPreview(WStr(GetCommandLineW()));
        return 0;
    }
#endif

    {
        int mutoolRes = MaybeRunMutool();
        if (mutoolRes != kNoMutool) {
            exitCode = mutoolRes; // propagate the tool's exit code to the process
            goto Exit;
        }
    }

    DetectExternalViewers();
    ReRegisterFileAssociations();

    gRenderCache = new RenderCache();

    // TODO: for reasons I don't understand, this must be called before LoadSettings()
    DarkModeInit();

    LoadSettings();
    UpdateGlobalPrefs(flags);
    if (gMyWindowWasEmbedded) {
        str::ReplaceWithCopy(&gGlobalPrefs->scrollbars, "windows");
    }
    SetCurrentLang(flags.lang ? flags.lang : gGlobalPrefs->uiLanguage);
    if (flags.showPrintersDialog) {
        // -console / -silent: list to stdout only, no dialog window (#5810)
        ShowPrintersDialog(flags.silent || flags.showConsole);
        goto Exit;
    }
    FileWatcherInit();

    if (flags.testRenderPage) {
        TestRenderPage(flags);
        ShutdownCommon();
        return 0;
    }

    if (flags.showConsole) {
        RedirectIOToConsole();
    }

    if (len(flags.pathsToBenchmark) > 0) {
        BenchFileOrDir(flags.pathsToBenchmark);
    }

    if (flags.exitImmediately) {
        goto Exit;
    }

    gCrashOnOpen = flags.crashOnOpen;

    gRenderCache->textColor = ThemePageRenderColors(gRenderCache->backgroundColor);
    // logfa("retrieved doc colors in WinMain: 0x%x 0x%x\n", gRenderCache->textColor, gRenderCache->backgroundColor);

    gIsStartup = true;
    if (!RegisterWinClass()) {
        goto Exit;
    }

    if (!InstanceInit()) {
        goto Exit;
    }

    if (flags.hwndPluginParent) {
        // check early to avoid a crash in MakePluginWindow()
        if (!IsWindow(flags.hwndPluginParent)) {
            MsgBox(nullptr, "-plugin argument is not a valid window handle (hwnd)", "Error", MB_OK | MB_ICONERROR);
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
        auto nFiles = len(flags.fileNames);
        if (nFiles != 1) {
            flags.search = {};
        }
    }

    // -print-dialog takes precedence: if the user explicitly asked for the
    // print dialog, show it (handled after the file loads, below) instead of
    // printing silently, even when -print-to/-print-to-default is also given
    // (fixes #3975)
    if (flags.printerName && !flags.printDialog) {
        // note: this prints all PDF files. Another option would be to
        // print only the first one.
        // exit code is 0 on success, otherwise the category of the first
        // failure (see PrintResult), so an automated caller knows why (#3478)
        PrintResult printRes = PrintResult::Ok;
        for (Str path : flags.fileNames) {
            PrintResult r = PrintFile(path, flags.printerName, !flags.silent, flags.printSettings);
            if (r != PrintResult::Ok && printRes == PrintResult::Ok) {
                printRes = r;
            }
        }
        exitCode = (int)printRes;
        logf("Finished printing, exitCode: %d\n", exitCode);
        goto Exit;
    }

    // only call FindExistingSumatraProcessHwnd() once
    existingInstanceHwnd = FindExistingSumatraProcessHwnd(&hMutex, &openInNewWindow);

    if (flags.printDialog || flags.stressTestPath || gPluginMode || gForTesting) {
        // TODO: pass print request through to previous instance?
    } else if (flags.reuseDdeInstance || flags.dde) {
        existingHwnd = FindWindowW(FRAME_CLASS_NAME, nullptr);
    } else if (gGlobalPrefs->reuseInstance) {
        existingHwnd = existingInstanceHwnd;
    }

    if (flags.dde) {
        logf("sending flags.dde '%s', hwnd: 0x%p\n", flags.dde, existingHwnd);
        SendMyselfDDE(flags.dde, existingHwnd);
        goto Exit;
    }

    if (existingHwnd) {
        int nFiles = len(flags.fileNames);
        // reusing an instance whose windows are all on other virtual desktops:
        // open the first file in a new window so it lands on the current desktop
        // (#5630). Subsequent files then open into that (now current) window.
        bool reuseInNewWindow = openInNewWindow && (existingHwnd == existingInstanceHwnd);
        // -new-window with several files: only the first forces a new window;
        // the rest open as tabs in that window (issue #5044). Previously every
        // file got its own window.
        bool userNewWindow = flags.inNewWindow;
        bool canBatchOpen = nFiles > 1 && existingHwnd && !flags.reuseDdeInstance && IsSimpleOpenCase(flags, true);
        if (canBatchOpen) {
            StrVec paths;
            for (Str path : flags.fileNames) {
                paths.Append(path::NormalizeTemp(path));
            }
            u32 newWindow = (reuseInNewWindow || userNewWindow) ? 2 : 0;
            if (SendOpenFilesToExistingInstance(existingHwnd, paths, newWindow)) {
                goto Exit;
            }
        }
        for (int n = 0; n < nFiles; n++) {
            Str path = flags.fileNames[n];
            bool isFirstWindow = (0 == n);
            bool savedInNewWindow = flags.inNewWindow;
            if (reuseInNewWindow && n == 0) {
                flags.inNewWindow = true;
            } else if (userNewWindow) {
                flags.inNewWindow = (n == 0);
            }
            OpenUsingDDE(existingHwnd, path, flags, isFirstWindow);
            flags.inNewWindow = savedInNewWindow;
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
    gInitialSessionData = gGlobalPrefs->sessionData;
    gGlobalPrefs->sessionData = new Vec<SessionData*>();

    restoreSession = SettingsRestoreSession() && (len(*gInitialSessionData) > 0) && !NeedsWindowEmbeddingHacks();
    if (!SettingsUseTabs() && (existingInstanceHwnd != nullptr)) {
        // do not restore a session if tabs are disabled and SumatraPDF is already running
        // TODO: maybe disable restoring if tabs are disabled?
        restoreSession = false;
        logf("not restoring a session because the same exe is already running and tabs are disabled\n");
    }

    showStartPage =
        !restoreSession && len(flags.fileNames) == 0 && SettingsRememberOpenedFiles() && gGlobalPrefs->showStartPage;

    // ShGetFileInfoW triggers ASAN deep in Windows code so probably not my fault
    if (showStartPage) {
        // make the shell prepare the image list, so that it's ready when the first window's loaded
        SHFILEINFOW sfi{};
        uint flg = SHGFI_SYSICONINDEX | SHGFI_SMALLICON | SHGFI_USEFILEATTRIBUTES;
        SHGetFileInfoW(L".pdf", 0, &sfi, sizeof(sfi), flg);
    }

    if (restoreSession) {
        for (SessionData* data : *gInitialSessionData) {
            // create window hidden to avoid flashing the about page
            win = CreateAndShowMainWindow(data, false);
            for (TabState* state : *data->tabStates) {
                if (len(state->filePath) == 0) {
                    logf("WinMain: skipping RestoreTabOnStartup() because state->filePath is empty\n");
                    continue;
                }
                RestoreTabOnStartup(win, state, gGlobalPrefs->lazyLoading);
            }
            // TabIndex is 1-based among document tabs (home tab is not in TabStates).
            // Also accept legacy sessions that stored a UI index including home.
            {
                auto tabs = win->Tabs();
                int nTabs = len(tabs);
                int selectIdx = 0;
                int want = data->tabIndex; // 1-based
                int docOrdinal = 0;
                int firstDocIdx = -1;
                int matchDocIdx = -1;
                for (int i = 0; i < nTabs; i++) {
                    if (tabs[i]->IsAboutTab()) {
                        continue;
                    }
                    if (firstDocIdx < 0) {
                        firstDocIdx = i;
                    }
                    docOrdinal++;
                    if (docOrdinal == want) {
                        matchDocIdx = i;
                    }
                }
                if (matchDocIdx >= 0) {
                    selectIdx = matchDocIdx;
                } else if (want >= 1 && want <= nTabs && !tabs[want - 1]->IsAboutTab()) {
                    // legacy: UI index including home
                    selectIdx = want - 1;
                } else if (firstDocIdx >= 0) {
                    selectIdx = firstDocIdx;
                }
                TabsSelect(win, selectIdx);
            }
            if (gGlobalPrefs->lazyLoading) {
                // trigger loading of the document
                ReloadDocument(win, false);
            }
            ShowMainWindow(win, data->windowState);
            // Docs were loaded while the frame was hidden (normal windowPos size).
            // After show / maximize / fullscreen, force DisplayModel to match the
            // final canvas so scroll isn't stuck on the pre-show viewport
            // (related to #5753 / #5823).
            if (win->IsDocLoaded()) {
                win->canvasRc = {};
                win->UpdateCanvasSize();
            }
        }
    }

    for (Str path : flags.fileNames) {
        if (restoreSession) {
            auto* tab = FindTabByFile(path);
            if (tab) {
                tabToSelect = tab;
                if (flags.forwardSearchOrigin && flags.forwardSearchLine && win->AsFixed() && win->AsFixed()->pdfSync) {
                    int page;
                    Vec<Rect> rects;
                    TempStr srcPath = path::NormalizeTemp(flags.forwardSearchOrigin);
                    int ret = win->AsFixed()->pdfSync->SourceToDoc(srcPath, flags.forwardSearchLine, 0, &page, rects);
                    ShowForwardSearchResult(win, srcPath, flags.forwardSearchLine, 0, ret, page, rects);
                }
                continue;
            }
        }
        // Same path can appear twice on the cmdline (Directory Opus "%1" "%2" …)
        // or already be mid-load from a reuseInstance open that arrived during
        // an earlier password dialog.
        if (IsDocumentOpenOrLoading(path)) {
            if (auto* existing = FindMainWindowByFile(path, true)) {
                win = existing;
                tabToSelect = FindTabByFile(path);
            }
            continue;
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
        MaybeGoTo(win, flags.namedDest, flags.pageNumber);
    }

    nWithDde = len(gDdeOpenOnStartup);
    if (nWithDde > 0) {
        logf("Loading %d documents queued by dde open\n", nWithDde);
        for (Str path : gDdeOpenOnStartup) {
            // Always skip paths already open or mid-load (not only when restoring a
            // session). Multi-select of password PDFs from Explorer commonly queues
            // a DDE open for a file that was also already started (fixes #4576).
            if (IsDocumentOpenOrLoading(path)) {
                logf("LoadOnStartup/dde: skipping already open/loading '%s'\n", path);
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

    if (len(flags.fileNames) > 0 && !win) {
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
    if (len(flags.fileNames) == 0) {
        FlagsEnterFullscreen(flags, win);
    }

    if (flags.stressTestPath) {
        // don't save file history and preference changes
        RestrictPolicies(Perm::SavePreferences);
        RebuildMenuBarForWindow(win);
        StartStressTest(&flags, win);
    }

    // only hide newly missing files when showing the start page on startup
    if (showStartPage && FileHistoryGet(0)) {
        RemoveNonExistentFilesAsync();
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

    StartAsyncUpdateCheck(win, UpdateCheck::Automatic);

    if (IsDebuggerPresent()) {
        // helps when running from 10x under debugger
        // BringWindowToTop(win->hwndFrame);
    }

    {
        auto fn = MkFunc0Void(DeleteStaleFilesAsync);
        RunAsync(fn, "DeleteStaleFilesAsync");
    }

    // needed if RememberOpenedFiles = false
    // https://github.com/sumatrapdfreader/sumatrapdf/issues/5456
    uitask::Post(MkFunc0(LayoutAndFocusOnStartup, win), "LayoutAndFocusOnStartup");

    // home-page bottom bar if we lost default-app status for registered extensions
    if (showStartPage) {
        uitask::Post(MkFunc0(MaybeShowDefaultAppNotification, win), "MaybeShowDefaultAppNotification");
    }

    StartSumatraControl(flags.controlPipeName);

    // on by default in debug builds; release builds can opt in by calling
    // StartUiHangDetector() themselves
    if (gIsDebugBuild) {
        StartUiHangDetector();
    }

    exitCode = RunMessageLoop();
    StopUiHangDetector();
    SafeCloseHandle(&hMutex);
    CleanUpThumbnailCache();

Exit:
    // logf("Exiting with exit code: %d\n", exitCode);
    UnregisterSettingsForFileChanges();

    HandleRedirectedConsoleOnShutdown();
    DeleteManualBrowserWindow();

    LogArenaStats(StrL("temp arena"), GetTempArena());
    LogArenaStats(StrL("perm arena"), gPermArena);

    // don't shell-open the log for -for-testing automation runs: it spawns a
    // stray editor window per run (and, depending on the .txt association,
    // could even launch another non-testing SumatraPDF that saves settings)
    if (!logFileBecauseDebug && !gForTesting) {
        LaunchFileIfExists(logFilePath);
    }
    str::FreePtr(&logFilePath);
    if (AreDangerousThreadsPending()) {
        if (gIsDebugBuild) {
            // in debug builds wait for the threads instead of fast-exiting so
            // that full cleanup runs and leak trackers only report real leaks.
            // Safe: windows are deleted below, so the threads' MainWindow*
            // stay valid while we wait; their queued finish tasks are drained
            // (and orphaned controllers freed) later in uitask::Destroy()
            log("waiting for dangerous threads to finish instead of fast exit\n");
            while (AreDangerousThreadsPending()) {
                ::Sleep(100);
            }
        } else {
            fastExit = true;
        }
    }
    if (fastExit) {
        // leave all the remaining clean-up to the OS
        // (as recommended for a quick exit)
        // note: this intentionally skips freeing engines/windows, so leak
        // trackers will report everything still allocated
        log("fast exit: skipping cleanup, leak reports are expected\n");
        ::ExitProcess(exitCode);
    }

    if (gInitialSessionData) {
        FreeSessionDataVec(gInitialSessionData);
        delete gInitialSessionData;
        gInitialSessionData = nullptr;
    }
    FreeExternalViewers();
    while (len(gWindows) > 0) {
        DeleteMainWindow(gWindows[0]);
    }

    DeleteCachedCursors();
    DeleteLaserPointerCursor();
    DeleteCreatedFonts();
    DeleteBitmap(gBitmapReloadingCue);
    // all frame/canvas windows are destroyed by now
    DeleteBrush(gWinClassBgBrush);

    destroy_system_font_list();

    // TODO: if needed, I could replace it with AtomicBool gFileExistenceInProgress
    // alternatively I can set AtomicBool gAppShutdown and have various threads
    // abort quickly if IsAppShuttingDown()
#if 0
    // wait for FileExistenceChecker to terminate
    // (which should be necessary only very rarely)
    while (gFileExistenceChecker) {
        Sleep(10);
        uitask::DrainQueue();
    }
#endif

    // must run before uitask::Destroy() (these deletes are queued as ui tasks)
    // and before gRenderCache goes away (the waiting threads use it)
    WaitForPendingControllerDeletes();

    PlatformFontDestroy();
    uitask::Destroy();
    trans::Destroy();

    FreeAcceleratorTables();

    FileWatcherWaitForShutdown();
    delete gRenderCache;
    SaveCallstackLogs();
    dbghelp::FreeCallstackLogs();

    // must be after uitask::Destroy() because we might have queued ReloadSettings()
    // which crashes if gGlobalPrefs is freed
    FileHistorySetStates(nullptr);
    CleanUpSettings();

    FreeAllMenuDrawInfos();

    ShutdownCleanup();
    EngineEbookCleanup();
    FreeCustomCommands();
    FreeThemes();

    // it's still possible to crash after this (destructors of static classes,
    // atexit() code etc.) point, but it's very unlikely
    if (!gIsAsanBuild) {
        UninstallCrashHandler();
    }
    DeleteAppTools();
    DestroyLogging();
    DestroyTempArena();
    DestroyPermArena();
    FreeLibsumatrapdfDll();

    return exitCode;
}
