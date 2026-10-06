/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// ng: orig's MainWindow.cpp owns the frame HWND and the link handler. Here it
// owns the window's tabs, its document controller callback and the gpui window
// it draws into; the rest of orig's file (LinkHandler, tooltips, UIA, caption)
// arrives with steps 7-9.

#include "gui/GpuiBridge.h"
#include "base/File.h"
#include "base/Launch.h"
#include "base/Win.h"

#include "gui/UIModels.h"

#include "Settings.h"
#include "AppTools.h"
#include "AppSettings.h"
#include "DisplayMode.h"
#include "DocController.h"
#include "EngineBase.h"
#include "base/GuessFileType.h"
#include "EngineAll.h"
#include "ChmModel.h"
#include "MarkdownModel.h"
#include "DisplayModel.h"
#include "RenderCache.h"
#include "TextSelection.h"
#include "CommandAvailability.h"
#include "ExternalViewers.h"
#include "Menu.h"
#include "Notifications.h"
#include "Translations.h"
#include "SumatraPDF.h"
#include "ReadingAutoScroll.h"
#include "ReadAloud.h"
#include "AIChatCommon.h"
#include "AIChatPanel.h"
#include "OverlayScrollbar.h"
#include "MainWindow.h"
#include "WindowTab.h"
#include "gui/AppShell.h"
#include "gui/DocCanvas.h"
#include "SelectionToolbar.h"
#include "Annotation.h"
#include "AnnotEditToolbar.h"
#include "AnnotFilterToolbar.h"
#include "AnnotTextPopup.h"
#include "RefHover.h"
#include "ProgressUpdateUI.h"
#include "TextSearch.h"
#include "SearchAndDDE.h"
#include "StressTesting.h"
#include "FindBar.h"
#include "FindWindow.h"
#include "Toolbar.h"
#include "TableOfContents.h"
#include "gui/Sidebar.h"
#include "gui/TabsUI.h"
#include "gui/TabSwitcher.h"
#include "HomePage.h"
#include "ImageSaveCropResize.h"

#include "SumatraLog.h"

Vec<MainWindow*> gWindows;

// the model calls back into the window: repaint, scrollbars, rendering.
// ng: the render requests go to the shared RenderCache (the render happens on
// a worker thread and comes back through uitask); the scrollbar update fills
// the canvas' CanvasScrollInfo the way orig fills the canvas HWND's SCROLLINFO.
struct ControllerCallbackHandler : DocControllerCallback {
    MainWindow* win{nullptr};

    explicit ControllerCallbackHandler(MainWindow* win) : win(win) {}
    ~ControllerCallbackHandler() override = default;

    void Repaint() override { AppShellInvalidate(win); }
    void PageNoChanged(DocController*, int pageNo) override {
        logf("PageNoChanged: %d\n", pageNo);
        win->currPageNo = pageNo;
        UpdateTocSelection(win, pageNo);
        ShowPageInfoIfWanted(win);
        AppShellInvalidate(win);
    }
    void ZoomChanged(DocController*, float zoomVirtual) override {
        logf("ZoomChanged: %.2f\n", (double)zoomVirtual);
        ShowPageInfoIfWanted(win);
        AppShellInvalidate(win);
    }
    void UpdateScrollbars(DisplayModel* dm, Size canvas) override { CanvasUpdateScrollbars(win, dm, canvas); }
    void RequestRendering(DisplayModel* dm, int pageNo) override {
        if (gRenderCache) {
            gRenderCache->RequestRendering(dm, pageNo);
        }
    }
    void RequestPredictiveRendering(DisplayModel* dm, int originPageNo, const int* pages, int nPages) override {
        if (gRenderCache) {
            gRenderCache->RequestPredictiveRendering(dm, originPageNo, pages, nPages);
        }
    }
    void CleanUp(DisplayModel* dm) override {
        if (gRenderCache) {
            gRenderCache->FreeForDisplayModel(dm);
        }
    }
    void RenderThumbnail(DisplayModel*, Size, const OnBitmapRendered*) override {}
    void GotoLink(IPageDestination* dest) override {
        if (win->linkHandler) {
            win->linkHandler->GotoLink(dest);
        }
    }
    void FocusFrame(bool) override {}
    void SaveDownload(Str, Str) override {}
    void FindResultReceived(int, int, int) override {}
    void FindAllResultReceived(Str) override {}
    void TocChanged(DocController* ctrl) override;
    void PagesRenumbered(DisplayModel* dm) override {
        if (win->AsFixed() == dm) {
            UpdateTocSelection(win, dm->CurrentPageNo());
        }
        AppShellInvalidate(win);
    }
};

// the controller swapped in a new TocTree (a background heading / markdown
// ToC arrived); orig's ControllerCallbackHandler::TocChanged
void ControllerCallbackHandler::TocChanged(DocController* ctrl) {
    WindowTab* tab = win->CurrentTab();
    if (!tab || tab->ctrl != ctrl) {
        if (tab) {
            tab->currToc = nullptr;
        }
        return;
    }
    if (win->tocLoaded) {
        ReloadTocTree(tab);
    } else if (tab->showToc && !win->InPresentation()) {
        // presentation mode has its own sidebar state (tab->showTocPresentation)
        SetSidebarVisibility(win, true, gSettings->showFavorites);
    }
    AppShellInvalidate(win);
}

DocControllerCallback* CreateControllerCallbackHandler(MainWindow* win) {
    return new ControllerCallbackHandler(win);
}

// --- LinkHandler ------------------------------------------------------------

// ng: orig's LinkHandler, from this file. What is not ported: the JavaScript
// menu destination (orig's TrackPopupMenu at the cursor; the port has no popup
// it can open from code and wait on). It is marked below.
struct LinkHandler : ILinkHandler {
    MainWindow* win = nullptr;

    explicit LinkHandler(MainWindow* w) {
        ReportIf(!w);
        win = w;
    }
    ~LinkHandler() override = default;

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
         len(data));
    if (OpenDocumentFromMemory(win, data, fileName)) {
        str::Free(data);
        return;
    }
    TempStr tmpDir = GetTempDirPathTemp();
    TempStr tmpPath = path::JoinTemp(tmpDir, path::GetBaseNameTemp(fileName));
    if (len(tmpDir) == 0 || !file::WriteFile(tmpPath, data)) {
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
    // ng: kindDestinationJsMenu needs a popup menu at the cursor.
    logf("LinkHandler::GotoLink: unhandled kind %s\n", Str(kind));
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
    Location loc = win->ctrl->ResolveDest(dest);
    if (!loc.IsValid()) {
        return;
    }
    int pageNo = win->ctrl->PageNoFromLocation(loc);
    if (!win->ctrl->ValidPageNo(pageNo)) {
        return;
    }
    RectF rect = dest->GetRect();
    float zoom = dest->GetZoom();
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
    ShowLinkDestHighlight(win, pageNo, rect);
}

// Convert file:// / file:/// / file: URIs to a local path (+ optional #fragment).
// Returns false if uri is not a file: scheme.
static bool PathFromFileUriTemp(Str uri, TempStr* pathOut, Str* fragmentOut) {
    Str rest = uri;
    if (!str::TrimPrefixI(rest, StrL("file:"))) {
        return false;
    }
    // file://host/path or file:///path -> drop authority (// or ///)
    if (str::TrimPrefix(rest, StrL("//"))) {
        // empty host: next char is / of absolute path
        if (rest && rest.s[0] == '/') {
            // Windows drive path: /C:/foo -> C:/foo
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
    if (len(uri) == 0) {
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

    // file://... -> open as a local document (or hand it to the shell)
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
void CleanRemoteDestNameInPlace(Str& destName) {
    str::TrimPrefixI(destName, StrL("nameddest="));
}

// for safety, only handle relative paths and only open them in SumatraPDF
// (unless they're of an allowed perceived type)
void LinkHandler::LaunchFile(Str pathOrig, IPageDestination* remoteLink) {
    if (!CanAccessDisk()) {
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
    if (!isAbsPath) {
        auto dir = path::GetDirTemp(win->ctrl->GetFilePath());
        fullPath = path::JoinTemp(dir, path);
        fullPath = path::NormalizeTemp(fullPath);
    }
    path::Type pathType = path::GetType(fullPath);
    if (pathType == path::Type::None) {
        TempStr msg = fmt(Tr("Error loading %s").s, fullPath);
        ShowWarningNotification(win, msg, kNotif5SecsTimeOut);
        return;
    }
    // a directory or a file we can't open is shown in the file manager, never
    // handed to the shell to open (it could be an executable)
    if (pathType == path::Type::Dir || !IsFileSupportedByContent(fullPath)) {
        OpenPathInDefaultFileManager(fullPath);
        return;
    }

    MainWindow* targetWin = LoadDocument(win, fullPath);
    if (!targetWin || !targetWin->IsDocLoaded() || !remoteLink) {
        return;
    }

    Str destName = remoteLink->GetName();
    if (destName) {
        CleanRemoteDestNameInPlace(destName);
        IPageDestination* dest = targetWin->ctrl->GetNamedDest(destName);
        if (dest) {
            ((LinkHandler*)targetWin->linkHandler)->ScrollTo(dest);
        }
    } else {
        ((LinkHandler*)targetWin->linkHandler)->ScrollTo(remoteLink);
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
            // ng: orig also selects the entry in the ToC tree (step 9)
        }
    }
    if (!hasDest && ctrl->HasPageLabels()) {
        int pageNo = ctrl->GetPageByLabel(name);
        if (ctrl->ValidPageNo(pageNo)) {
            ctrl->GoToPage(pageNo, true);
        }
    }
}

MainWindow::MainWindow(gpui::Window* w) {
    gpuiWin = w;
    sidebarBottomContent =
        SidebarContentFromStr(gSettings ? gSettings->sidebarBottomView : Str{}, SidebarContent::Favorites);
    sidebarDx = gSettings ? gSettings->sidebarDx : 0;
    if (sidebarDx < kSidebarMinDx) {
        sidebarDx = kSidebarMinDx;
    }
    linkHandler = new LinkHandler(this);
    cbHandler = CreateControllerCallbackHandler(this);
    tabSelectionHistory = new Vec<WindowTab*>();
}

MainWindow::~MainWindow() {
    isBeingClosed = true;
    FinishStressTest(this);
    // the find / count workers hold this window; join them before it goes away
    AbortFinding(this, true);
    ClearFindMatches(this);
    str::Free(findCountText);
    str::Free(findPageRangeText);
    str::Free(findCountRangeText);
    str::Free(findCountPendingText);
    str::Free(browserFindTerm);
    VecReset(findCountPositions);
    DeleteFindBar(this);
    DeleteFindWindow(this);
    DestroyToolbar(this);
    DeleteSelectionToolbar(this);
    DeleteAnnotEditToolbar(this);
    DeleteAnnotFilterToolbar(this);
    DeleteAnnotationTextPopup(this);
    DeleteAnnotationHoverOverlay(this);
    RefHoverDestroy(refHover);
    refHover = nullptr;
    ReadingAutoScrollDestroy(this);
    ReadAloudPlaybackBarDestroy(this);
    OverlayScrollbarsDelete(this);
    SidebarDelete(this);
    TabsUIDelete(this);
    TabSwitcherDelete(this);
    DocCanvasDelete(this);
    HomePageDelete(this);
    AppShellDeleteWindow(this);
    delete tocFilteredTree;
    tocFilteredTree = nullptr;
    VecReset(tocMatchingItems);
    VecReset(expandedFavorites);
    // the tabs own their controllers; ctrl points into the current one
    ctrl = nullptr;
    currentTabTemp = nullptr;
    for (WindowTab* tab : tabs) {
        delete tab;
    }
    VecReset(tabs);
    delete tabSelectionHistory;
    tabSelectionHistory = nullptr;
    DeleteMenuModel(menu);
    menu = nullptr;
    delete cbHandler;
    cbHandler = nullptr;
    delete linkHandler;
    linkHandler = nullptr;
    str::Free(linkTooltip);
    VecReset(linkFollowTargets);
}

bool MainWindow::HasDocsLoaded() const {
    int nTabs = TabCount();
    if (nTabs == 0) {
        return true;
    }
    for (int i = 0; i < nTabs; i++) {
        auto* tab = GetTab(i);
        if (!tab->IsAboutTab()) {
            return true;
        }
    }
    return false;
}

bool MainWindow::IsCurrentTabAbout() const {
    return nullptr == CurrentTab() || CurrentTab()->IsAboutTab();
}

bool MainWindow::IsDocLoaded() const {
    return ctrl != nullptr;
}

WindowTab* MainWindow::CurrentTab() const {
    return currentTabTemp;
}

int MainWindow::TabCount() const {
    return tabs.len;
}

WindowTab* MainWindow::GetTab(int idx) const {
    if (idx < 0 || idx >= tabs.len) {
        return nullptr;
    }
    return tabs[idx];
}

int MainWindow::GetTabIdx(WindowTab* tab) const {
    for (int i = 0; i < tabs.len; i++) {
        if (tabs[i] == tab) {
            return i;
        }
    }
    return -1;
}

Vec<WindowTab*> MainWindow::Tabs() const {
    Vec<WindowTab*> res;
    for (int i = 0; i < tabs.len; i++) {
        VecAppend(res, tabs[i]);
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

Size MainWindow::GetViewPortSize() const {
    return Size{canvasRc.dx, canvasRc.dy};
}

void MainWindow::RedrawAll(bool) const {
    AppShellInvalidate(const_cast<MainWindow*>(this));
}

void MainWindow::Focus() const {
    AppShellActivateWindow(const_cast<MainWindow*>(this));
}

void MainWindow::MoveDocBy(int dx, int dy) const {
    WindowTab* tab = CurrentTab();
    if (tab) {
        tab->MoveDocBy(dx, dy);
    }
}

void MainWindow::ToggleZoom() const {
    WindowTab* tab = CurrentTab();
    if (tab) {
        tab->ToggleZoom();
    }
}

bool MainWindow::InPresentation() const {
    return presentation != PM_DISABLED;
}

void MainWindow::ChangePresentationMode(PresentationMode mode) {
    presentation = mode;
    if (PM_BLACK_SCREEN == mode || PM_WHITE_SCREEN == mode) {
        DeleteLinkTooltip(this);
    }
    RedrawAll();
}

// ng: orig has these in CommandAvailability.cpp; they are the only part of it
// that needs MainWindow / WindowTab, and keeping them out lets the command
// policy link into the console tools. Bodies are orig's.
static void PopulateTabCloseFlags(AppCommandCtx& ctx) {
    if (!ctx.win) {
        return;
    }
    int nTabs = ctx.win->TabCount();
    ctx.nTabs = nTabs;
    WindowTab* currTab = ctx.tab;
    int tabIdx = ctx.win->GetTabIdx(currTab);
    ctx.canCloseTabsToRight = tabIdx < (nTabs - 1);
    ctx.canCloseTabsToLeft = false;
    int nFirstDocTab = 0;
    for (int i = 0; i < nTabs; i++) {
        WindowTab* t = ctx.win->GetTab(i);
        if (t->IsAboutTab()) {
            nFirstDocTab = 1;
            continue;
        }
        ctx.hasDocTabs = true;
        if (t == currTab) {
            if (i > nFirstDocTab) {
                ctx.canCloseTabsToLeft = true;
            }
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
    ctx.allowToggleMenuBar = true;
    ctx.hasOpenDocuments = HasOpenedDocuments(win);

    if (ctx.tab) {
        ctx.autoScrollOn = ctx.tab->autoScroll.on;
        ctx.readingBarOn = ctx.tab->readingBar.on;
        ctx.isChm = ctx.tab->AsChm() || ctx.tab->AsMarkdown();
        Str currentPath = win->ctrl ? win->ctrl->GetFilePath() : ctx.filePath;
        ctx.isMarkdown = str::EndsWithI(currentPath, StrL(".md")) || str::EndsWithI(currentPath, StrL(".markdown"));
        EngineBase* engine = ctx.tab->GetEngine();
        if (engine && engine->kind == kindEngineComicBooks) {
            ctx.isCbx = true;
        }
        if (engine && engine->IsImageCollection()) {
            ctx.isImageCollection = true;
        }
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
        ctx.hideAnnotations = ctx.tab->hideAnnotations;
        ctx.canContinueReadAloud = CanContinueReadAloud(ctx.tab);
    }
    ctx.ttsAvailable = TtsIsAvailable();
    ctx.isSpeaking = TtsIsSpeaking();
    ctx.clipboardHasImage = ImageEditHasClipboard(win);
    ctx.clipboardReadAsync = OS_WASM;

    ctx.aiChatAvailable = IsAIChatAvailable();
    ctx.aiChatSupported = IsAIChatSupportedForTab(ctx.tab);
    ctx.grokInstalled = IsGrokBuildInstalled();
    ctx.claudeInstalled = IsClaudeCodeInstalled();
    ctx.codexInstalled = IsCodexBuildInstalled();
    ctx.antiGravityInstalled = IsAntiGravityInstalled();

    ctx.hasSelection = ctx.isDocLoaded && ctx.tab && win->showSelection && ctx.tab->selectionOnPage;

    if (ctx.isDocLoaded && win->ctrl) {
        ctx.isSinglePage = IsSingle(win->ctrl->GetDisplayMode());
        ctx.pageCount = win->ctrl->PageCount();
        ctx.hasToc = win->ctrl->HasToc();
    }

    DisplayModel* dm = win->AsFixed();
    if (dm) {
        ctx.isFixedPage = true;
        auto* engine = dm->GetEngine();
        ctx.hasTextSelection = ctx.hasSelection && dm->textSelection->result.len > 0;
        ctx.supportsAnnots = EngineSupportsAnnotations(engine);
        ctx.hasUnsavedAnnotations = EngineHasUnsavedAnnotations(engine);
        ctx.hasRedactMarks = EngineHasRedactMarks(engine);
        ctx.hasUserRedactMarks = EngineHasUserRedactMarks(engine);
        ctx.canUndo = EngineMupdfCanUndo(engine);
        ctx.canRedo = EngineMupdfCanRedo(engine);
        int pageNoUnderCursor = dm->GetPageNoByPoint(cursorPos);
        if (pageNoUnderCursor > 0) {
            ctx.isCursorOnPage = true;
        }
        // orig hit-tests the annotation the context menu was opened on; the
        // canvas keeps the same one in win->annotationUnderCursor
        ctx.annotationUnderCursor = win->annotationUnderCursor;
        if (!ctx.annotationUnderCursor && ctx.tab) {
            ctx.annotationUnderCursor = ctx.tab->selectedAnnotation;
        }
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

BuildMenuCtx* NewBuildMenuCtx(WindowTab* tab, Point pt) {
    auto* ctx = new AppCommandCtx;
    if (tab && tab->win) {
        *ctx = NewAppCommandCtx(tab->win, pt);
    } else if (tab) {
        ctx->tab = tab;
    }
    return ctx;
}

void DeleteBuildMenuCtx(BuildMenuCtx* ctx) {
    delete ctx;
}

bool HasOpenedDocuments(MainWindow* win) {
    for (WindowTab* t : win->Tabs()) {
        if (!t->IsAboutTab()) {
            return true;
        }
    }
    return false;
}

bool IsMainWindowValid(MainWindow* win) {
    return win && VecContains(gWindows, win);
}

// True if `win` still exists and CloseWindow has not started. Use this for
// deferred work (load finish, timers, UI updates) that must not touch a window
// that is tearing down.
bool IsMainWindowValidAndNotClosing(MainWindow* win) {
    return IsMainWindowValid(win) && !win->isBeingClosed;
}

// ng: orig's FindMainWindowByHwnd; the gpui window is what identifies a window
MainWindow* FindMainWindowByGpuiWindow(gpui::Window* gw) {
    if (!gw) {
        return nullptr;
    }
    for (MainWindow* win : gWindows) {
        if (win->gpuiWin == gw) {
            return win;
        }
    }
    return nullptr;
}

// Find MainWindow using WindowTab. Different than WindowTab->win in that
// it validates that WindowTab is still valid
MainWindow* FindMainWindowByTab(WindowTab* tabToFind) {
    if (!tabToFind) {
        return nullptr;
    }
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

WindowTab* FindTabByFilePath(Str path, MainWindow* limitWin) {
    if (len(path) == 0) {
        return nullptr;
    }
    for (MainWindow* win : gWindows) {
        if (limitWin && win != limitWin) {
            continue;
        }
        for (WindowTab* tab : win->Tabs()) {
            if (path::IsSame(tab->filePath, path)) {
                return tab;
            }
        }
    }
    return nullptr;
}
