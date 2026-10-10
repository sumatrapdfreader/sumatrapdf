/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include <uiautomationcore.h>
#include <uiautomationcoreapi.h>
#include <mmsystem.h>
#include "base/File.h"
#include "base/Win.h"
#include "gui/Dpi.h"
#include "base/GuessFileType.h"
#include "base/UITask.h"

#include "gui/UIModels.h"
#include "gui/Layout.h"
#include "gui/win/WinGui.h"

#include "gui/PlatformFont.h"
#include "gui/Gfx.h"
#include "gui/VirtCtrl.h"
#include "gui/win/TabsCtrl.h"
#include "gui/win/FrameRateWnd.h"

#include "Settings.h"
#include "DisplayMode.h"
#include "DocController.h"
#include "EngineBase.h"
#include "EngineAll.h"
#include "ChmModel.h"
#include "MarkdownModel.h"
#include "DisplayModel.h"
#include "ProgressUpdateUI.h"
#include "Notifications.h"
#include "ReadAloud.h"
#include "ReadingAutoScroll.h"
#include "TextSelection.h"
#include "Annotation.h"
#include "TextSearch.h"
#include "SumatraPDF.h"
#include "AIChatCommon.h"
#include "AIChatPanel.h"
#include "SelectionToolbar.h"
#include "AnnotEditToolbar.h"
#include "AnnotTextPopup.h"
#include "AnnotFilterToolbar.h"
#include "FindBar.h"
#include "FindWindow.h"
#include "SearchAndDDE.h"
#include "RefHover.h"
#include "WindowTab.h"
#include "PageThumbnails.h"
#include "SidebarPanel.h"
#include "TableOfContents.h"
#include "StressTesting.h"
#include "ExternalViewers.h"
#include "Installer.h"
#include "CommandAvailability.h"
#include "uia/Provider.h"
#include "Theme.h"
#include "Canvas.h"
#include "HomePage.h"
#include "MainWindow.h"
#include "MainWindowCommon.h"

static void SafeDeleteTabsCtrl(TabsCtrl* tabsCtrl) {
    logf("SafeDeleteTabsCtrl: 0x%p\n", tabsCtrl);
    delete tabsCtrl;
}

static void OverlayScrollbarsOnWindowMoved(MainWindow* win) {
    OverlayScrollbarUpdatePos(win->overlayScrollV);
    OverlayScrollbarUpdatePos(win->overlayScrollH);
}

MainWindow::MainWindow(HWND hwnd) {
    hwndFrame = hwnd;
    linkHandler = new LinkHandler(this);
    cbHandler = CreateControllerCallbackHandler(this);
    overlayScrollOnMoved = MkFunc1Void(OverlayScrollbarsOnWindowMoved);
    RegisterOnWindowMoved(&overlayScrollOnMoved);
}

void MainWindow::RegisterOnWindowMoved(Func1List<MainWindow*>* cb) {
    ReportIf(!cb);
    cb->Register(&onWindowMoved);
}

void MainWindow::UnregisterOnWindowMoved(Func1List<MainWindow*>* cb) {
    if (!cb) {
        return;
    }
    cb->Unregister(&onWindowMoved);
}

// fire onWindowMoved: popups placed in screen coords don't follow the frame on their own
void MainWindow::NotifyWindowMoved() {
    if (onWindowMoved) {
        onWindowMoved->CallAll(this);
    }
}

static WORD dotPatternBmp[8] = {0x00aa, 0x0055, 0x00aa, 0x0055, 0x00aa, 0x0055, 0x00aa, 0x0055};

void CreateMovePatternLazy(MainWindow* win) {
    if (win->bmpMovePattern) {
        return;
    }
    win->bmpMovePattern = CreateBitmap(8, 8, 1, 1, dotPatternBmp);
    ReportIf(!win->bmpMovePattern);
    win->brMovePattern = CreatePatternBrush(win->bmpMovePattern);
    ReportIf(!win->brMovePattern);
}

MainWindow::~MainWindow() {
    CancelAnnotationResizeRerender(this);
    KillTimer(hwndCanvas, kSmoothScrollTimerID);
    KillTimer(hwndCanvas, kReadingAutoScrollTimerID);
    if (scrollAnimHiResTimer) {
        timeEndPeriod(1);
        scrollAnimHiResTimer = false;
    }
    scrollAnimActive = false;
    RefHoverDestroy(refHover);
    FinishStressTest(this);

    ReportIf(TabCount() > 0);
    RemoveNotificationsForHwnd(hwndCanvas);
    // ReportIf(ctrl); // TODO: seen in crash report
    ReportIf(linkOnLastButtonDown);
    str::Free(urlOnLastButtonDown);
    str::Free(homeSearchQuery);

    // the panels hand the views' layouts back before those are deleted
    DeleteSidebarPanel(sidebarTop);
    DeleteSidebarPanel(sidebarBottom);
    DeleteSidebarPanel(favoritesTabPanel);
    sidebarTop = sidebarBottom = favoritesTabPanel = nullptr;
    HomePageDestroySearch(this);
    HomePageDestroyChrome(this);

    OverlayScrollbarDestroy(overlayScrollV);
    OverlayScrollbarDestroy(overlayScrollH);

    DeleteObject(brMovePattern);
    DeleteObject(bmpMovePattern);
    DeleteObject(brControlBgColor);

    // Disconnect UIA clients and release our provider. Clients that still hold
    // refs get UIA_E_ELEMENTNOTAVAILABLE after FreeDocument.
    if (uiaProvider) {
        uiaProvider->OnDocumentUnload();
        // Clears UIA's cached link for this hwnd (pairs with WM_GETOBJECT).
        UiaReturnRawElementProvider(hwndCanvas, 0, 0, nullptr);
        // Windows 8+: drop client-side caches (delay-loaded; absent on Win7).
        {
            HMODULE uiaDll = GetModuleHandleW(L"UIAutomationCore.dll");
            if (uiaDll) {
                using PFN = HRESULT(WINAPI*)(IRawElementProviderSimple*);
                auto disconnect = (PFN)GetProcAddress(uiaDll, "UiaDisconnectProvider");
                if (disconnect) {
                    disconnect(uiaProvider);
                }
            }
        }
        uiaProvider->Release();
        uiaProvider = nullptr;
    }

    DeleteFindBar(this);
    DeleteFindWindow(this);

    // stop the find-bar match-count background thread before we're freed
    // (it reads our fields; a pending CountEndTask closes the handle later)
    if (findCountThread) {
        AtomicIntInc(&findCountEpoch);
        WaitForSingleObject(findCountThread, INFINITE);
        findCountThread = nullptr;
    }
    str::FreePtr(&findCountText);
    str::FreePtr(&findPageRangeText);
    str::FreePtr(&findCountRangeText);
    str::FreePtr(&findCountPendingText);
    str::FreePtr(&browserFindTerm);
    ClearFindMatches(this);

    DeleteSelectionToolbar(this);
    DeleteAnnotEditToolbar(this);
    DeleteAnnotFilterToolbar(this);
    DeleteAnnotationHoverOverlay(this);
    DeleteAnnotationTextPopup(this);

    delete linkHandler;
    delete buffer;
    delete tabSelectionHistory;
    ShutdownAIChatForMainWindow(this);
    auto tabs = Tabs();
    DeleteVecMembers(tabs);
    {
        TabsCtrl* tabsCtrlToDelete = tabsCtrl;
        logf("~MainWindow: destroy tabsCtrl: 0x%p, HWND: 0x%p\n", tabsCtrlToDelete, tabsCtrlToDelete->hwnd);
        // Tab close can re-enter comctl32 subclass dispatch while unwinding
        // the current message. Destroy the HWND now, but defer deleting the
        // C++ object until the UI task queue runs after message dispatch.
        tabsCtrlToDelete->Destroy();
        auto fn = MkFunc0(SafeDeleteTabsCtrl, tabsCtrlToDelete);
        uitask::Post(fn, "SafeDeleteTabsCtrl");
        tabsCtrl = nullptr;
    }

    // cbHandler is passed into DocController and must be deleted afterwards
    // (all controllers should have been deleted prior to MainWindow, though)
    delete cbHandler;

    delete frameRateWnd;
    ReadAloudPlaybackBarDestroy(this);
    ReadingAutoScrollDestroy(this);
    UnregisterOnWindowMoved(&overlayScrollOnMoved);
    ReportIf(onWindowMoved);
    delete infotip;
    // the views' layouts own their controls
    delete tocViewLayout;
    delete pageThumbs;
    delete tocFilteredTree;
    if (favTreeView) {
        delete favTreeView->treeModel;
    }
    delete favViewLayout;

    DestroyAIChatPanel(this);

    // owns chrome, the content row, the splitters and the slots
    delete chromeLayout;
    // the splitters tell the root they are going away, so it goes last
    delete frameRoot;
}

void ClearMouseState(MainWindow* win) {
    CancelAnnotationResizeRerender(win);
    HideAnnotationHoverOverlay(win);
    win->dragStartPending = false;
    win->textDragPending = false;
    win->imageDragPending = false;
    win->imageDragElement = nullptr;
    win->imageDragPageNo = -1;
    win->linkOnLastButtonDown = nullptr;
    win->annotationUnderCursor = nullptr;
}

bool MainWindow::IsDocLoaded() const {
    bool isLoaded = (ctrl != nullptr);
    bool isTabLoaded = (CurrentTab() && CurrentTab()->ctrl != nullptr);
    if (isLoaded != isTabLoaded) {
        logf("MainWindow::IsDocLoaded(): isLoaded: %d, isTabLoaded: %d\n", (int)isLoaded, (int)isTabLoaded);
        ReportIf(!gPluginMode);
    }
    return isLoaded;
}

WindowTab* MainWindow::CurrentTab() const {
    WindowTab* curr = currentTabTemp;
    if (curr != nullptr) {
        return curr;
    }
    if (!tabsCtrl) {
        return nullptr;
    }
    int i = tabsCtrl->GetSelected();
    if (i >= 0 && i < tabsCtrl->TabCount()) {
        curr = GetTab(i);
        return curr;
    }
#if 0
    int nTabs = TabCount();
    ReportIf(nTabs > 0);
    if (nTabs > 0) {
        curr = GetTab(0);
        return curr;
    }
#endif
    return nullptr;
}

int MainWindow::TabCount() const {
    return tabsCtrl->TabCount();
}

WindowTab* MainWindow::GetTab(int idx) const {
    WindowTab* tab = GetTabsUserData<WindowTab*>(tabsCtrl, idx);
    return tab;
}

int MainWindow::GetTabIdx(WindowTab* tab) const {
    int nTabs = tabsCtrl->TabCount();
    for (int i = 0; i < nTabs; i++) {
        WindowTab* t = GetTabsUserData<WindowTab*>(tabsCtrl, i);
        if (t == tab) {
            return i;
        }
    }
    return -1;
}

Vec<WindowTab*> MainWindow::Tabs() const {
    Vec<WindowTab*> res;
    if (!tabsCtrl) { // null seen in crash report
        return res;
    }
    int nTabs = tabsCtrl->TabCount();
    for (int i = 0; i < nTabs; i++) {
        WindowTab* tab = GetTabsUserData<WindowTab*>(tabsCtrl, i);
        VecAppend(res, tab);
    }
    return res;
}

// Notify both display model and double-buffer (if they exist)
// about a potential change of available canvas size
void MainWindow::UpdateCanvasSize() {
    if (suppressCanvasSizeUpdate) {
        return;
    }
    Rect rc = HwndClientRect(hwndCanvas);
    if (buffer && canvasRc == rc) {
        return;
    }
    canvasRc = rc;

    // create a new output buffer and notify the model
    // about the change of the canvas size
    delete buffer;
    buffer = new DoubleBuffer(hwndCanvas, canvasRc);

    if (IsDocLoaded()) {
        // the display model needs to know the full size (including scroll bars)
        ctrl->SetViewPortSize(GetViewPortSize());
    }
    if (CurrentTab()) {
        CurrentTab()->canvasRc = canvasRc;
    }

    RelayoutNotifications(hwndCanvas);
    ReadAloudPlaybackBarRelayout(hwndCanvas);
    ReadingAutoScrollRelayout(hwndCanvas);
}

Size MainWindow::GetViewPortSize() const {
    Size size = canvasRc.Size();
    // can be empty transiently during RelayoutFrame / EndDeferWindowPos

    DWORD style = GetWindowLong(hwndCanvas, GWL_STYLE);
    if ((style & WS_VSCROLL)) {
        size.dx += DpiGetSystemMetrics(SM_CXVSCROLL);
    }
    if ((style & WS_HSCROLL)) {
        size.dy += DpiGetSystemMetrics(SM_CYHSCROLL);
    }
    ReportIf((style & (WS_VSCROLL | WS_HSCROLL)) && !AsFixed());
    return size;
}

void MainWindow::RedrawCanvas() const {
    HwndInvalidate(hwndCanvas);
}

static BOOL CALLBACK RedrawHwndCallback(HWND hwnd, LPARAM lp) {
    bool update = (bool)lp;
    HwndInvalidate(hwnd, true);
    if (update) {
        UpdateWindow(hwnd);
    }
    return TRUE;
}

void MainWindow::RedrawAll(bool update) const {
    if (gRedrawLog) {
        logf("redraw: RedrawAll update=%d frame=0x%p\n", (int)update, this->hwndFrame);
    }
    EnumChildWindows(this->hwndFrame, RedrawHwndCallback, (LPARAM)update);
    RedrawHwndCallback(this->hwndFrame, (LPARAM)update);
}

void MainWindow::RedrawAllIncludingNonClient() const {
    if (gRedrawLog) {
        logf("redraw: RedrawAllIncludingNonClient frame=0x%p\n", this->hwndFrame);
    }
    // Full erase of frame + children + non-client so layout transitions (tabs on/off,
    // closing last tab, menu bar) do not leave a ghost of the old toolbar/caption
    // painted on the client area (issue #5750).
    RedrawWindow(this->hwndFrame, nullptr, nullptr,
                 RDW_ERASE | RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_FRAME | RDW_UPDATENOW);
}

void MainWindow::ChangePresentationMode(PresentationMode mode) {
    presentation = mode;
    if (PM_BLACK_SCREEN == mode || PM_WHITE_SCREEN == mode) {
        DeleteToolTip();
    }
    RedrawAll();
}

static HWND FindModalOwnedBy(HWND hwndParent) {
    HWND hwnd = nullptr;
    while (true) {
        hwnd = FindWindowExW(HWND_DESKTOP, hwnd, nullptr, nullptr);
        if (hwnd == nullptr) {
            break;
        }
        bool isDlg = (GetWindowStyle(hwnd) & WS_DLGFRAME) != 0;
        if (!isDlg) {
            continue;
        }
        if (GetWindow(hwnd, GW_OWNER) != hwndParent) {
            continue;
        }
        return hwnd;
    }
    return nullptr;
}

void MainWindow::Focus() const {
    HwndToForeground(hwndFrame);
    // set focus to an owned modal dialog if there is one
    HWND hwnd = FindModalOwnedBy(hwndFrame);
    if (hwnd != nullptr) {
        HwndSetFocus(hwnd);
        return;
    }
    HwndSetFocus(hwndFrame);
}

void MainWindow::MoveDocBy(int dx, int dy) const {
    ReportIf(!CurrentTab());
    CurrentTab()->MoveDocBy(dx, dy);
}

void MainWindow::ShowToolTip(Str text, Rect& rc, bool multiline) const {
    if (len(text) == 0 || IsIconic(hwndFrame)) {
        // Track-mode tips are WS_EX_TOPMOST popups; never show while minimized
        // or they stick on the desktop (often at 0,0) — issue #5928.
        DeleteToolTip();
        return;
    }
    infotip->SetSingle(text, rc, multiline);
}

// Track-mode tip at a fixed screen position (keyboard home-page selection).
// maxRightScreen > 0 clamps the bubble so it does not extend past that x.
void MainWindow::ShowToolTipAt(Str text, const Rect& rc, Point screenPos, bool multiline, int maxRightScreen) const {
    if (len(text) == 0 || IsIconic(hwndFrame)) {
        DeleteToolTip();
        return;
    }
    infotip->SetSingleAt(text, rc, screenPos, multiline, maxRightScreen);
}

void MainWindow::DeleteToolTip() const {
    infotip->Delete();
}

bool MainWindow::CreateUIAProvider() {
    if (uiaProvider) {
        return true;
    }
    uiaProvider = new SumatraUIAutomationProvider(this->hwndCanvas);
    if (!uiaProvider) {
        return false;
    }
    // load data to provider
    if (AsFixed()) {
        uiaProvider->OnDocumentLoad(AsFixed());
    }
    return true;
}

static void LaunchEmbeddedDestination(MainWindow* win, PageDestination* pd) {
    if (pd->embedObjNum <= 0) {
        return;
    }
    EngineBase* engine = win->CurrentTab()->AsFixed()->GetEngine();
    Str data = EngineMupdfLoadAnnotAttachment(engine, pd->embedObjNum);
    if (len(data) == 0) {
        return;
    }
    Str fileName = pd->GetValue();
    logf("GotoLink: opening file attachment annotation '%s', objNum: %d, size: %d\n", fileName, pd->embedObjNum,
         data.len);
    // PDF (and other types we can open): load from memory into a tab
    if (OpenDocumentFromMemory(win, data, fileName)) {
        str::Free(data);
        return;
    }
    TempStr tmpDir = GetTempDirTemp();
    if (len(tmpDir) == 0) {
        str::Free(data);
        return;
    }
    TempStr tmpPath = path::JoinTemp(tmpDir, path::GetBaseNameTemp(fileName));
    if (!file::WriteFile(tmpPath, data)) {
        str::Free(data);
        return;
    }
    // a type the shell isn't allowed to open is shown in the file manager instead
    if (OpenFileExternally(tmpPath)) {
        logf("LaunchEmbeddedDestination: opened '%s'\n", tmpPath);
    } else {
        logf("LaunchEmbeddedDestination: showing '%s' in the file manager\n", tmpPath);
        OpenPathInDefaultFileManager(tmpPath);
    }
    str::Free(data);
}

void LinkHandler::GotoLink(IPageDestination* dest) {
    ReportIf(!win || win->linkHandler != this);
    if (!dest || !win || !win->IsDocLoaded()) {
        return;
    }

    Kind kind = dest->GetKind();

    if (kindDestinationScrollTo == kind) {
        // PDF NewWindow on internal GoTo is not exposed by MuPDF's link URIs.
        // Ctrl+click opens the same document in a new tab/window (see Canvas).
        ScrollTo(dest);
        return;
    }
    if (kindDestinationLaunchURL == kind) {
        auto* d = (PageDestinationURL*)dest;
        LaunchURL(d->url);
        return;
    }
    if (kindDestinationLaunchFile == kind) {
        PageDestinationFile* fileDest = (PageDestinationFile*)dest;
        this->LaunchFile(fileDest->path, dest);
        return;
    }
    if (kindDestinationLaunchEmbedded == kind) {
        LaunchEmbeddedDestination(win, (PageDestination*)dest);
        return;
    }

    if (kindDestinationAttachment == kind) {
        // Not handled here. Must use context menu to trigger launching
        // embedded files
        return;
    }

    if (kindDestinationJsMenu == kind) {
        auto* menuDest = (PageDestinationJsMenu*)dest;
        if (len(menuDest->items) == 0) {
            return;
        }
        HMENU menu = CreatePopupMenu();
        if (!menu) {
            return;
        }
        for (int i = 0; i < len(menuDest->items); i++) {
            Str item = menuDest->items[i];
            if (str::Eq(item, StrL("-"))) {
                AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
                continue;
            }
            AppendMenuW(menu, MF_STRING, (UINT)(i + 1), CWStrTemp(MenuToSafeStringTemp(item)));
        }
        POINT pt{};
        GetCursorPos(&pt);
        int cmd =
            TrackPopupMenu(menu, TPM_RETURNCMD | TPM_NONOTIFY | TPM_LEFTALIGN, pt.x, pt.y, 0, win->hwndFrame, nullptr);
        DestroyMenu(menu);
        if (cmd < 1 || cmd > len(menuDest->items)) {
            return;
        }
        Str chosen = menuDest->items[cmd - 1];
        Str url = chosen;
        int colon = str::IndexOf(chosen, StrL(": "));
        if (colon >= 0) {
            url = Str(chosen.s + colon + 2, chosen.len - colon - 2);
            str::TrimWs(url);
        }
        if (IsExternalUrl(url) || str::StartsWithI(url, StrL("ftp://"))) {
            LaunchURL(url);
        }
        return;
    }

    if (kindDestinationLaunchURL == kind) {
        return;
    }

    logf("LinkHandler::GotoLink: unhandled kind %s\n", Str(kind));
    ReportIf(true);
}

// for safety, only handle relative paths and only open them in SumatraPDF
// (unless they're of an allowed perceived type) and never launch any external
// file in plugin mode (where documents are supposed to be self-contained)
void LinkHandler::LaunchFile(Str pathOrig, IPageDestination* remoteLink) {
    if (gPluginMode || !CanAccessDisk()) {
        return;
    }

    TempStr path = str::ReplaceTemp(pathOrig, StrL("/"), StrL("\\"));
    str::TrimPrefix(path, StrL(".\\"));

    TempStr fullPath = path;
    bool isAbsPath = str::StartsWith(path, StrL("\\"));
    if (len(path) >= 2 && path.s[1] == ':') {
        /* technically c: is not abs, only c:\\ */
        isAbsPath = true;
    }
#if 0
    // we used to not allow absolute links due to security, but if we can open
    // the doc we should assume we can handle it securely
    if (isAbsPath) {
        return;
    }
#endif
    if (!isAbsPath) {
        auto dir = path::GetDirTemp(win->ctrl->GetFilePath());
        fullPath = path::JoinTemp(dir, path);
        fullPath = path::NormalizeTemp(fullPath);
    }
    path::Type pathType = path::GetType(fullPath);
    if (pathType == path::Type::None) {
        auto* win = gWindows[0];
        ShowErrorLoadingNotification(win, fullPath, true);
        return;
    }
    if (pathType == path::Type::Dir) {
        OpenPathInDefaultFileManager(fullPath);
        return;
    }

    bool canWeOpenIt = IsFileSupportedByContent(fullPath);
    if (!canWeOpenIt) {
        OpenPathInDefaultFileManager(fullPath);
        return;
    }

    // Open in a new window when the PDF GoToR NewWindow flag is set (if known)
    // or the user Ctrl+clicks. MuPDF's file: URI conversion does not preserve
    // /NewWindow today; openInNewWindow is for when callers can set it.
    bool wantNewWindow = IsCtrlPressed();
    if (remoteLink && remoteLink->GetKind() == kindDestinationLaunchFile) {
        wantNewWindow = wantNewWindow || ((PageDestinationFile*)remoteLink)->openInNewWindow;
    }

    MainWindow* targetWin = nullptr;
    if (wantNewWindow) {
        targetWin = CreateAndShowMainWindow(nullptr);
        if (!targetWin) {
            return;
        }
        LoadArgs args(fullPath, targetWin);
        args.forceReuse = true;
        args.noPlaceWindow = true;
        targetWin = LoadDocument(&args);
    } else {
        targetWin = FindMainWindowByFile(fullPath, true);
        if (!targetWin) {
            LoadArgs args(fullPath, win);
            targetWin = LoadDocument(&args);
        }
    }
    if (!targetWin) {
        return;
    }

    if (!targetWin->IsDocLoaded()) {
        bool quitIfLast = false;
        CloseCurrentTab(targetWin, quitIfLast);
        // OpenFileExternally rejects files we'd otherwise
        // have to show a notification to be sure (which we
        // consider bad UI and thus simply don't)
        bool ok = OpenFileExternally(fullPath);
        if (!ok) {
            ShowErrorLoadingNotification(targetWin, fullPath, true);
        }
        return;
    }

    targetWin->Focus();
    if (!remoteLink) {
        return;
    }

    Str destName = remoteLink->GetName();
    if (destName) {
        CleanRemoteDestNameInPlace(destName);
        IPageDestination* dest = targetWin->ctrl->GetNamedDest(destName);
        if (dest) {
            targetWin->linkHandler->ScrollTo(dest);
        }
    } else {
        targetWin->linkHandler->ScrollTo(remoteLink);
    }
}

// Select and scroll the ToC tree to tocItem (same idea as GoToTocItem from the palette).
static void SelectTocItemInTree(MainWindow* win, TocItem* tocItem) {
    if (!win || !tocItem || !win->tocLoaded || !win->tocTreeView) {
        return;
    }
    // prevent UpdateTocSelection from undoing the selection when the page changes
    win->tocKeepSelection = true;
    TreeView* treeView = win->tocTreeView;
    HTREEITEM hi = treeView->GetHandleByTreeItem((TreeItem)tocItem);
    if (hi) {
        TreeView_EnsureVisible(treeView->hwnd, hi);
    }
    treeView->SelectItem((TreeItem)tocItem);
    win->tocKeepSelection = false;
}

void LinkHandler::GotoNamedDest(Str name) {
    ReportIf(!win || win->linkHandler != this);
    DocController* ctrl = win->ctrl;
    if (!ctrl) {
        return;
    }

    // Match order:
    // 1. Exact match on internal destination name
    // 2. Fuzzy match on full ToC item title
    // 3. Fuzzy match on a part of a ToC item title
    // 4. Exact match on page label
    IPageDestination* dest = ctrl->GetNamedDest(name);
    bool hasDest = dest != nullptr;
    if (dest) {
        ScrollTo(dest);
    } else if (ctrl->HasToc()) {
        auto* docTree = ctrl->GetToc();
        TocItem* root = docTree->root;
        TempStr fuzName = NormalizeFuzzyTemp(name);
        TocItem* tocItem = FindTocItem(root, fuzName, false);
        if (!tocItem) {
            tocItem = FindTocItem(root, fuzName, true);
        }
        if (tocItem) {
            dest = tocItem->dest;
            if (dest) {
                ScrollTo(dest);
                hasDest = true;
            } else if (tocItem->pageNo > 0) {
                ctrl->GoToPage(tocItem->pageNo, true);
                hasDest = true;
            }
            if (hasDest) {
                SelectTocItemInTree(win, tocItem);
            }
        }
    }
    if (!hasDest && ctrl->HasPageLabels()) {
        int pageNo = ctrl->GetPageByLabel(name);
        if (ctrl->ValidPageNo(pageNo)) {
            ctrl->GoToPage(pageNo, true);
        }
    }
}

static void PopulateTabCloseFlags(AppCommandCtx& ctx) {
    if (!ctx.win) {
        return;
    }

    int nTabs = ctx.win->TabCount();
    ctx.nTabs = nTabs;
    WindowTab* currTab = ctx.tab;
    int tabIdx = ctx.win->GetTabIdx(currTab);
    ctx.canCloseTabsToRight = tabIdx < (nTabs - 1);
    int nFirstDocTab = 0;
    for (int i = 0; i < nTabs; i++) {
        WindowTab* tab = ctx.win->GetTab(i);
        if (tab->IsAboutTab()) {
            nFirstDocTab = 1;
            continue;
        }
        ctx.hasDocTabs = true;
        if (tab == currTab) {
            ctx.canCloseTabsToLeft = i > nFirstDocTab;
            continue;
        }
        ctx.canCloseOtherTabs = true;
    }
}

AppCommandCtx NewAppCommandCtx(MainWindow* win, Point cursorPos) {
    AppCommandCtx ctx;
    ctx.win = win;
    ctx.cursorPos = cursorPos;
    if (!win) {
        return ctx;
    }

    ctx.tab = win->CurrentTab();
    ctx.isDocLoaded = win->IsDocLoaded();
    ctx.filePath = ctx.tab ? ctx.tab->filePath : Str();
    ctx.hasOpenDocuments = HasOpenedDocuments(win);
    ctx.ttsAvailable = true;
    ctx.shellIntegrationInstalled = IsOurExeInstalled();
    ctx.debugDpiOverrideAvailable = true;

    if (ctx.tab) {
        ctx.autoScrollOn = ctx.tab->autoScroll.on;
        ctx.readingBarOn = ctx.tab->readingBar.on;
        ctx.isChm = ctx.tab->AsChm() || ctx.tab->AsMarkdown();
        Str currentPath = win->ctrl ? win->ctrl->GetFilePath() : ctx.filePath;
        ctx.isMarkdown = str::EndsWithI(currentPath, StrL(".md")) || str::EndsWithI(currentPath, StrL(".markdown"));
        EngineBase* engine = ctx.tab->GetEngine();
        ctx.isCbx = engine && engine->kind == kindEngineComicBooks;
        ctx.isImageCollection = engine && engine->isImageCollection;
        ctx.isReflowable = engine && engine->isReflowable;
        ctx.engineKind = ctx.tab->GetEngineType();
        ctx.engineHasErrors = engine && engine->HasErrors();
        ctx.canSendEmail = CanSendAsEmailAttachment(ctx.tab);
#ifndef DISABLE_DOCUMENT_RESTRICTIONS
        ctx.allowsPrinting = !win->AsFixed() || (engine && engine->AllowsPrinting());
#endif
        ctx.isPdf = IsPdfDoc(ctx.tab);
        if (ctx.isPdf && engine) {
            ctx.isPdfEncrypted = EngineMupdfIsEncrypted(engine);
        }
        ctx.canContinueReadAloud = CanContinueReadAloud(ctx.tab);
        ctx.hideAnnotations = ctx.tab->hideAnnotations;
        ctx.selectedAnnotation = ctx.tab->selectedAnnotation;
    }

    ctx.isSpeaking = TtsIsSpeaking();
    ctx.clipboardHasImage = IsClipboardFormatAvailable(CF_BITMAP);
    ctx.aiChatAvailable = IsAIChatAvailable();
    ctx.aiChatSupported = IsAIChatSupportedForTab(ctx.tab);
    ctx.grokInstalled = GetGrokBuildProvider()->IsInstalled();
    ctx.claudeInstalled = GetClaudeCodeProvider()->IsInstalled();
    ctx.codexInstalled = GetCodexBuildProvider()->IsInstalled();
    ctx.antiGravityInstalled = GetAntiGravityProvider()->IsInstalled();
    ctx.hasSelection = ctx.isDocLoaded && ctx.tab && win->showSelection && ctx.tab->selectionOnPage;

    if (ctx.isDocLoaded && win->ctrl) {
        ctx.isSinglePage = IsSingle(win->ctrl->GetDisplayMode());
        ctx.pageCount = win->ctrl->PageCount();
        ctx.hasToc = win->ctrl->HasToc();
    }

    DisplayModel* dm = win->AsFixed();
    if (dm) {
        ctx.isFixedPage = true;
        EngineBase* engine = dm->GetEngine();
        ctx.hasTextSelection = ctx.hasSelection && len(dm->textSelection->result) > 0;
        ctx.supportsAnnots = EngineSupportsAnnotations(engine);
        ctx.hasUnsavedAnnotations = EngineHasUnsavedAnnotations(engine);
        ctx.hasRedactMarks = EngineHasRedactMarks(engine);
        ctx.hasUserRedactMarks = EngineHasUserRedactMarks(engine);
        ctx.canUndo = EngineMupdfCanUndo(engine);
        ctx.canRedo = EngineMupdfCanRedo(engine);
        ctx.isCursorOnPage = dm->GetPageNoByPoint(cursorPos) > 0;
        ctx.annotationUnderCursor = dm->GetAnnotationAtPos(cursorPos, nullptr);
        IPageElement* pageEl = dm->GetElementAtPos(cursorPos, nullptr);
        if (pageEl) {
            Str value = pageEl->GetValue();
            ctx.cursorOnLinkTarget = pageEl->Is(kindPageElementDest) && PageDestHasAddress(pageEl->AsLink());
            ctx.cursorOnComment = value && pageEl->Is(kindPageElementComment);
            ctx.cursorOnImage = pageEl->Is(kindPageElementImage);
        }
        if (ctx.annotationUnderCursor) {
            ctx.cursorOnComment = !str::IsEmptyOrWhiteSpace(Contents(ctx.annotationUnderCursor));
        }
    }

    if (!CanAccessDisk()) {
        ctx.supportsAnnots = false;
        ctx.hasUnsavedAnnotations = false;
    }

    PopulateTabCloseFlags(ctx);
    return ctx;
}

// a debugging aid: flip it (in the source or the debugger) to get a small
// window showing how long painting the canvas takes
bool gShowFrameRate = false;

void MainWindow::ShowFrameRateDur(double durMs) {
    if (!gShowFrameRate) {
        return;
    }
    if (!frameRateWnd) {
        frameRateWnd = new FrameRateWnd();
        frameRateWnd->Create(hwndCanvas);
    }
    frameRateWnd->ShowFrameRateDur(durMs);
}

void UpdateControlsColors(MainWindow* win) {
    Color bgCol = ThemeControlBackgroundColor();
    Color txtCol = ThemeWindowTextColor();

    // logf("retrieved doc colors in tree control: 0x%x 0x%x\n", treeTxtCol, treeBgCol);

    // the panel labels and the splitters are virtual controls: they follow the
    // gui/ color defaults, which SumatraUpdateTheme() already refreshed
    if (win->tocTreeView) {
        win->tocTreeView->SetColors(txtCol, bgCol);

        if (win->tocFilterEdit) {
            win->tocFilterEdit->SetColors(txtCol, bgCol);
        }
        UpdateSidebarColors(win);
    }

    HomePageUpdateSearchColors(win);

    auto* favTreeView = win->favTreeView;
    if (favTreeView) {
        favTreeView->SetColors(txtCol, bgCol);
        if (win->favFilterEdit) {
            win->favFilterEdit->SetColors(txtCol, bgCol);
        }
    }
}

bool IsRightDragging(MainWindow* win) {
    if (win->mouseAction != MouseAction::Dragging) {
        return false;
    }
    return win->dragRightClick;
}

HWND MainWindowHwnd(MainWindow* win) {
    return win ? win->hwndFrame : nullptr;
}

MainWindow* FindMainWindowByHwnd(HWND hwnd) {
    if (!::IsWindow(hwnd)) {
        return nullptr;
    }
    for (MainWindow* win : gWindows) {
        if ((win->hwndFrame == hwnd) || ::IsChild(win->hwndFrame, hwnd)) {
            return win;
        }
    }
    // Owned popups (find bar / find window) and their children are WS_POPUP, not
    // WS_CHILD of the frame, so IsChild misses them. ComboLBox (dropped history)
    // is owned by the combo, so climb owner then parent until we hit a frame.
    HWND cur = hwnd;
    for (int i = 0; i < 16 && cur; i++) {
        HWND owner = GetWindow(cur, GW_OWNER);
        HWND parent = ::GetParent(cur);
        HWND next = owner ? owner : parent;
        if (!next || next == cur) {
            break;
        }
        for (MainWindow* win : gWindows) {
            if (next == win->hwndFrame) {
                return win;
            }
        }
        cur = next;
    }
    return nullptr;
}

// temporarily highlight this tab
void HighlightTab(MainWindow* win, WindowTab* tab) {
    if (!win) {
        return;
    }
    int idx = -1;
    if (tab) {
        idx = win->GetTabIdx(tab);
    }
    win->tabsCtrl->SetHighlighted(idx);
}

HWND GetHwndForNotification() {
    if (len(gWindows) == 0) {
        return nullptr;
    }
    return gWindows[0]->hwndCanvas;
}
