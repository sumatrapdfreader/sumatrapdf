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
#include "DocController.h"
#include "EngineBase.h"
#include "EngineAll.h"
#include "ChmModel.h"
#include "MarkdownModel.h"
#include "DisplayModel.h"
#include "ProgressUpdateUI.h"
#include "Notifications.h"
#include "ReadAloudPlaybackBar.h"
#include "TextSelection.h"
#include "TextSearch.h"
#include "SumatraPDF.h"
#include "AIChatCommon.h"
#include "AIChatPanel.h"
#include "MainWindow.h"
#include "SelectionToolbar.h"
#include "FindBar.h"
#include "FindWindow.h"
#include "SearchAndDDE.h"
#include "RefHover.h"
#include "WindowTab.h"
#include "TableOfContents.h"
#include "StressTesting.h"
#include "uia/Provider.h"

static void SafeDeleteTabsCtrl(TabsCtrl* tabsCtrl) {
    logf("SafeDeleteTabsCtrl: 0x%p\n", tabsCtrl);
    delete tabsCtrl;
}
#include "Theme.h"
#include "Canvas.h"
#include "HomePage.h"

struct LinkHandler : ILinkHandler {
    MainWindow* win = nullptr;

    explicit LinkHandler(MainWindow* w) {
        ReportIf(!w);
        win = w;
    }
    ~LinkHandler() override;

    void GotoLink(IPageDestination* dest) override;
    void GotoNamedDest(Str name) override;
    void GoToPage(int pageNo, bool addNavPoint) override;
    bool GoToNextPage() override;
    bool GoToPrevPage(bool toBottom = false) override;
    void ScrollTo(IPageDestination* dest) override;
    void ScrollTo(int pageNo, RectF rect, float zoom) override;
    void LaunchURL(Str uri) override;
    void LaunchFile(Str path, IPageDestination* remoteLink) override;
    TocItem* FindTocItem(TocItem* item, Str name, bool partially) override;
};

LinkHandler::~LinkHandler() {
    // do nothing
}

Vec<MainWindow*> gWindows;

MainWindow::MainWindow(HWND hwnd) {
    hwndFrame = hwnd;
    linkHandler = new LinkHandler(this);
    cbHandler = CreateControllerCallbackHandler(this);
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
    KillTimer(hwndCanvas, kSmoothScrollTimerID);
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

    UnsubclassToc(this);
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
    delete infotip;
    // tocLayout (VBox) owns the header, tocFilterEdit and tocTreeView; the
    // root only points at the header's virtual controls, so it outlives them
    delete tocLayout;
    delete tocRoot;
    delete tocFilteredTree;
    if (favTreeView) {
        delete favTreeView->treeModel;
    }
    // favLayout (VBox) owns the header, favFilterEdit and favTreeView
    delete favLayout;
    delete favRoot;

    DestroyAIChatPanel(this);

    // owns chrome, the content row, the splitters and the slots
    delete chromeLayout;
    // the splitters tell the root they are going away, so it goes last
    delete frameRoot;
}

void ClearMouseState(MainWindow* win) {
    win->dragStartPending = false;
    win->textDragPending = false;
    win->imageDragPending = false;
    win->imageDragElement = nullptr;
    win->imageDragPageNo = -1;
    win->linkOnLastButtonDown = nullptr;
    win->annotationUnderCursor = nullptr;
}

bool MainWindow::HasDocsLoaded() const {
    int nTabs = TabCount();
    if (nTabs == 0) {
        // logf("HasDocsLoaded: false because nTabs == 0\n");
        return true;
    }
    for (int i = 0; i < nTabs; i++) {
        auto* tab = GetTab(i);
        if (!tab->IsAboutTab()) {
            // logf("HasDocsLoaded: true because GetTab(i) !IsAboutTab()\n");
            return true;
        }
    }
    // logf("HasDocsLoaded: false because all %d tabs are IsAboutTab()\n", nTabs);
    return false;
}

bool MainWindow::IsCurrentTabAbout() const {
    return nullptr == CurrentTab() || CurrentTab()->IsAboutTab();
}

bool MainWindow::IsDocLoaded() const {
    bool isLoaded = (ctrl != nullptr);
    bool isTabLoaded = (CurrentTab() && CurrentTab()->ctrl != nullptr);
    if (isLoaded != isTabLoaded) {
        logfa("MainWindow::IsDocLoaded(): isLoaded: %d, isTabLoaded: %d\n", (int)isLoaded, (int)isTabLoaded);
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
        res.Append(tab);
    }
    return res;
}

DisplayModel* MainWindow::AsFixed() const {
    return ctrl ? ctrl->AsFixed() : nullptr;
}

ChmModel* MainWindow::AsChm() const {
    return ctrl ? ctrl->AsChm() : nullptr;
}

MarkdownModel* MainWindow::AsMarkdown() const {
    return ctrl ? ctrl->AsMarkdown() : nullptr;
}

// Notify both display model and double-buffer (if they exist)
// about a potential change of available canvas size
void MainWindow::UpdateCanvasSize() {
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

bool MainWindow::InPresentation() const {
    return presentation != PM_DISABLED;
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

void MainWindow::ToggleZoom() const {
    if (CurrentTab()) {
        CurrentTab()->ToggleZoom();
    }
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
    Str fileName = pd->GetValue2();
    logf("GotoLink: opening file attachment annotation '%s', objNum: %d, size: %d\n", fileName, pd->embedObjNum,
         (int)data.len);
    TempStr tmpDir = GetTempDirTemp();
    if (!tmpDir) {
        str::Free(data);
        return;
    }
    TempStr tmpPath = path::JoinTemp(tmpDir, path::GetBaseNameTemp(fileName));
    if (!file::WriteFile(tmpPath, data)) {
        str::Free(data);
        return;
    }
    SumatraLaunchBrowser(tmpPath);
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

    if (kindDestinationLaunchURL == kind) {
        return;
    }

    logf("LinkHandler::GotoLink: unhandled kind %s\n", Str(kind));
    ReportIf(true);
}

void LinkHandler::ScrollTo(IPageDestination* dest) {
    ReportIf(!win || !win->ctrl || win->linkHandler != this);
    if (!dest || !win || !win->ctrl || !win->IsDocLoaded()) {
        return;
    }
    // TODO: this seems like a hack, there should be a better way
    // https://github.com/sumatrapdfreader/sumatrapdf/issues/3499
    ChmModel* chm = win->ctrl->AsChm();
    if (chm) {
        chm->HandleLink(dest, nullptr);
        return;
    }
    MarkdownModel* md = win->ctrl->AsMarkdown();
    if (md) {
        md->HandleLink(dest, nullptr);
        return;
    }
    int pageNo = PageDestGetPageNo(dest);
    if (!win->ctrl->ValidPageNo(pageNo)) {
        return;
    }
    RectF rect = PageDestGetRect(dest);
    float zoom = PageDestGetZoom(dest);
    ScrollTo(pageNo, rect, zoom);
}

void LinkHandler::GoToPage(int pageNo, bool addNavPoint) {
    ReportIf(!win || !win->ctrl || win->linkHandler != this);
    if (!win || !win->ctrl || !win->IsDocLoaded()) {
        return;
    }
    win->ctrl->GoToPage(pageNo, addNavPoint);
}

bool LinkHandler::GoToNextPage() {
    ReportIf(!win || !win->ctrl || win->linkHandler != this);
    if (!win || !win->ctrl || !win->IsDocLoaded()) {
        return false;
    }
    return win->ctrl->GoToNextPage();
}

bool LinkHandler::GoToPrevPage(bool toBottom) {
    ReportIf(!win || !win->ctrl || win->linkHandler != this);
    if (!win || !win->ctrl || !win->IsDocLoaded()) {
        return false;
    }
    return win->ctrl->GoToPrevPage(toBottom);
}

void LinkHandler::ScrollTo(int pageNo, RectF rect, float zoom) {
    ReportIf(!win || !win->ctrl || win->linkHandler != this);
    if (!win || !win->ctrl || !win->IsDocLoaded()) {
        return;
    }
    win->ctrl->ScrollTo(pageNo, rect, zoom);
}

// Convert file:// / file:/// / file: URIs to a local path (+ optional #fragment).
// Returns false if uri is not a file: scheme.
static bool PathFromFileUriTemp(Str uri, TempStr* pathOut, Str* fragmentOut) {
    if (!str::StartsWithI(uri, StrL("file:"))) {
        return false;
    }
    // Skip "file:" case-insensitively (str::TrimPrefix is case-sensitive).
    Str rest = Str(uri.s + 5, uri.len - 5);
    // file://host/path or file:///path → drop authority (// or ///)
    if (str::TrimPrefix(rest, StrL("//"))) {
        // empty host: next char is / of absolute path
        if (rest && rest.s[0] == '/') {
            // Windows drive path: /C:/foo → C:/foo
            if (rest.len >= 3 && rest.s[1] && rest.s[2] == ':') {
                rest = Str(rest.s + 1, rest.len - 1);
            }
        }
    }
    TempStr path = str::DupTemp(rest);
    Str pathStr = path;
    Str frag = str::SliceFromChar(pathStr, '#');
    if (frag) {
        pathStr = Str(pathStr.s, (int)(frag.s - pathStr.s));
        frag = Str(frag.s + 1, frag.len - 1);
    }
    path = url::DecodeTemp(pathStr);
    str::TransCharsInPlace(path, StrL("/"), StrL("\\"));
    *pathOut = path;
    if (fragmentOut) {
        *fragmentOut = frag ? str::DupTemp(frag) : Str{};
    }
    return true;
}

void LinkHandler::LaunchURL(Str uri) {
    if (!uri) {
        /* ignore missing URLs */;
        return;
    }

    TempStr path = str::DupTemp(uri);
    int colon = str::IndexOfChar(path, ':');
    int hash = str::IndexOfChar(path, '#');
    if (colon < 0 || (hash >= 0 && colon > hash)) {
        // treat relative URIs as file paths (without fragment identifier)
        if (hash >= 0) {
            path.len = hash;
        }
        str::TransCharsInPlace(path, StrL("/"), StrL("\\"));
        path = url::DecodeTemp(path);
        // LaunchFile will reject unsupported file types
        this->LaunchFile(path, nullptr);
        return;
    }

    // file://... → open as a local document (or explorer if unsupported)
    TempStr filePath;
    Str fragment;
    if (PathFromFileUriTemp(uri, &filePath, &fragment)) {
        if (len(fragment) > 0) {
            // Carry destination name for LaunchFile scroll-to (named dest / page)
            PageDestinationFile dest(filePath, fragment);
            this->LaunchFile(filePath, &dest);
        } else {
            this->LaunchFile(filePath, nullptr);
        }
        return;
    }

    // LaunchBrowser will reject unsupported URI schemes
    SumatraLaunchBrowser(path);
}

// return true if we can load the file based on sniffing file type from content
static bool IsFileSupportedByContent(Str filePath) {
    FileType kindSniffed = GuessFileType(filePath, true);
    return IsSupportedFileType(kindSniffed, true);
}

// MuPDF encodes GoToR named destinations as "nameddest=<name>" in the link URI
// fragment, but EngineBase::GetNamedDest prepends "#nameddest=" itself -- so the
// prefix must be stripped or the lookup becomes "#nameddest=nameddest=<name>"
// and fails, leaving the remote PDF on page 1 (issue #5642).
// strips mupdf's "nameddest=" prefix from a remote link's destination name
// so it can be passed to GetNamedDest (issue #5642)
Str CleanRemoteDestName(Str destName) {
    if (destName && str::StartsWithI(destName, StrL("nameddest="))) {
        return Str(destName.s + 10, destName.len - 10);
    }
    return destName;
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
        SumatraOpenPathInDefaultFileManager(fullPath);
        return;
    }

    bool canWeOpenIt = IsFileSupportedByContent(fullPath);
    if (!canWeOpenIt) {
        SumatraOpenPathInDefaultFileManager(fullPath);
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

    Str destName = PageDestGetName(remoteLink);
    if (destName) {
        IPageDestination* dest = targetWin->ctrl->GetNamedDest(CleanRemoteDestName(destName));
        if (dest) {
            targetWin->linkHandler->ScrollTo(dest);
            delete dest;
        }
    } else {
        targetWin->linkHandler->ScrollTo(remoteLink);
    }
}

// normalizes case and whitespace in the string
static TempStr NormalizeFuzzyTemp(Str str) {
    TempStr dup = str::DupTemp(str);
    str::ToLowerInPlace(dup);
    str::NormalizeWSInPlace(dup);
    // cf. AddTocItemToView
    return dup;
}

static bool MatchFuzzy(Str s1, Str s2, bool partially) {
    if (!partially) {
        return str::Eq(s1, s2);
    }

    // only match at the start of a word (at the beginning and after a space)
    Str rest = s1;
    while (len(rest) > 0) {
        int idx = str::IndexOf(rest, s2);
        if (idx < 0) {
            break;
        }
        const char* found = rest.s + idx;
        if (found == s1.s || *(found - 1) == ' ') {
            return true;
        }
        int off = idx + 1;
        rest.s += off;
        rest.len -= off;
    }
    return false;
}

// finds the first ToC entry that (partially) matches a given normalized name
// (ignoring case and whitespace differences)
TocItem* LinkHandler::FindTocItem(TocItem* item, Str name, bool partially) {
    for (; item; item = item->next) {
        if (item->title) {
            TempStr fuzTitle = NormalizeFuzzyTemp(item->title);
            if (MatchFuzzy(fuzTitle, name, partially)) {
                return item;
            }
        }
        TocItem* found = FindTocItem(item->child, name, partially);
        if (found) {
            return found;
        }
    }
    return nullptr;
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
        delete dest;
    } else if (ctrl->HasToc()) {
        auto* docTree = ctrl->GetToc();
        TocItem* root = docTree->root;
        TempStr fuzName = NormalizeFuzzyTemp(name);
        TocItem* tocItem = FindTocItem(root, fuzName, false);
        if (!tocItem) {
            tocItem = FindTocItem(root, fuzName, true);
        }
        if (tocItem) {
            dest = tocItem->GetPageDestination();
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

bool HasOpenedDocuments(MainWindow* win) {
    for (WindowTab* t : win->Tabs()) {
        if (!t->IsAboutTab()) {
            return true;
        }
    }
    return false;
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

    // logfa("retrieved doc colors in tree control: 0x%x 0x%x\n", treeTxtCol, treeBgCol);

    // the panel labels and the splitters are virtual controls: they follow the
    // gui/ color defaults, which SumatraUpdateTheme() already refreshed
    {
        auto* tocTreeView = win->tocTreeView;
        tocTreeView->SetColors(txtCol, bgCol);

        if (win->tocFilterEdit) {
            win->tocFilterEdit->SetColors(txtCol, bgCol);
        }
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

// sometimes we stash MainWindow pointer, do something on a thread and
// then go back on main thread to finish things. At that point MainWindow
// could have been destroyed so we need to check if it's still valid
bool IsMainWindowValid(MainWindow* win) {
    return win && gWindows.Contains(win);
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
    return nullptr;
}

// Find MainWindow using WindowTab. Diffrent than WindowTab->win in that
// it validates that WindowTab is still valid
MainWindow* FindMainWindowByTab(WindowTab* tabToFind) {
    if (!tabToFind) return nullptr;
    for (MainWindow* win : gWindows) {
        for (WindowTab* tab : win->Tabs()) {
            if (tab == tabToFind) {
                return win;
            }
        }
    }
    return nullptr;
}

bool IsWindowTabValid(WindowTab* tab) {
    return FindMainWindowByTab(tab) != nullptr;
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
