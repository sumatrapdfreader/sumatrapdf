/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

/* How to think of display logic: physical screen of size
   viewPort.Size() is a window into (possibly much larger)
   total area (canvas) of size canvasSize.

   In DM_SINGLE_PAGE mode total area is the size of currently displayed page
   given current zoom level and rotation.
   In DM_CONTINUOUS mode canvas area consist of all pages rendered sequentially
   with a given zoom level and rotation. canvasSize.dy is the sum of heights
   of all pages plus spaces between them and canvasSize.dx is the size of
   the widest page.

   A possible configuration could look like this:

 -----------------------------------
 |                                 |
 |          -------------          |
 |          | window    |          |
 |          | i.e.      |          |
 |          | view port |          |
 |          -------------          |
 |                                 |
 |                                 |
 |    canvas                       |
 |                                 |
 |                                 |
 |                                 |
 |                                 |
 -----------------------------------

  We calculate the canvas size and position of each page we display on the
  canvas.

  Changing zoom level or rotation requires recalculation of canvas size and
  position of pages in it.

  We keep the offset of view port relative to canvas. The offset changes
  due to scrolling (with keys or using scrollbars).

  To draw we calculate which part of each page overlaps draw area, we render
  those pages to a bitmap and display those bitmaps.
*/

#include "base/Base.h"
#include "base/Win.h"
#include "gui/Dpi.h"
#include "base/Timer.h"

#include "gui/UIModels.h"

#include "Settings.h"
#include "DisplayMode.h"
#include "DocumentLayout.h"
#include "DocController.h"
#include "EngineBase.h"
#include "base/GuessFileType.h"
#include "EngineAll.h"
#include "DisplayModel.h"
#include "GlobalPrefs.h"
#include "SumatraPDF.h"
#include "PdfSync.h"
#include "ProgressUpdateUI.h"
#include "TextSelection.h"
#include "TextSearch.h"
#include "RenderCache.h"
#include "base/UITask.h"
#include "WindowTab.h"
#include "MainWindow.h"
#include "Notifications.h"
#include "SumatraConfig.h"

// if true, we pre-render the pages right before and after the visible pages
bool gPredictiveRender = true;

static int ColumnsFromDisplayMode(DisplayMode displayMode) {
    if (!IsSingle(displayMode)) {
        return 2;
    }
    return 1;
}

ScrollState::ScrollState(int page, double x, double y) {
    this->page = page;
    this->x = x;
    this->y = y;
}

bool ScrollState::operator==(const ScrollState& other) const {
    return page == other.page && x == other.x && y == other.y;
}

Str DisplayModel::GetFilePath() const {
    return engine->FilePath();
}

Str DisplayModel::GetDefaultFileExt() const {
    return engine->defaultExt;
}

int DisplayModel::PageCount() const {
    if (!engine) {
        return 0;
    }
    return engine->PageCount();
}

TempStr DisplayModel::GetPropertyTemp(DocProp prop) {
    return engine->GetPropertyTemp(prop);
}

void DisplayModel::GoToPage(int pageNo, bool addNavPoint) {
    GoToPage(pageNo, 0, addNavPoint);
}

DisplayMode DisplayModel::GetDisplayMode() const {
    return displayMode;
}

TocTree* DisplayModel::GetToc() {
    if (!engine) {
        return nullptr;
    }
    return engine->GetToc();
}

IPageDestination* DisplayModel::GetNamedDest(Str name) {
    return engine->GetNamedDest(name);
}

void DisplayModel::CreateThumbnail(Size size, const OnBitmapRendered* saveThumbnail) {
    cb->RenderThumbnail(this, size, saveThumbnail);
}

// page labels (optional)
bool DisplayModel::HasPageLabels() const {
    return engine->HasPageLabels();
}

TempStr DisplayModel::GetPageLabeTemp(int pageNo) const {
    return engine->GetPageLabeTemp(pageNo);
}

int DisplayModel::GetPageByLabel(Str label) const {
    return engine->GetPageByLabel(label);
}

// common shortcuts
bool DisplayModel::ValidPageNo(int pageNo) const {
    if (!engine) {
        return false;
    }
    return 1 <= pageNo && pageNo <= engine->PageCount();
}

bool DisplayModel::GoToPrevPage(bool toBottom) {
    return GoToPrevPage(toBottom ? -1 : 0);
}

// for quick type determination and type-safe casting
DisplayModel* DisplayModel::AsFixed() {
    return this;
}

// the following is specific to DisplayModel
EngineBase* DisplayModel::GetEngine() const {
    return engine;
}

Kind DisplayModel::GetEngineType() const {
    if (!engine) {
        return nullptr;
    }
    return engine->kind;
}

/* current rotation selected by user */
int DisplayModel::GetRotation() const {
    return rotation;
}

Rect DisplayModel::GetViewPort() const {
    return viewPort;
}

bool DisplayModel::IsHScrollbarVisible() const {
    return viewPort.dx < totalViewPortSize.dx;
}

bool DisplayModel::IsVScrollbarVisible() const {
    return viewPort.dy < totalViewPortSize.dy;
}

bool DisplayModel::NeedHScroll() const {
    return viewPort.dx < canvasSize.dx;
}

bool DisplayModel::NeedVScroll() const {
    return viewPort.dy < canvasSize.dy;
}

bool DisplayModel::CanScrollRight() const {
    return viewPort.x + viewPort.dx < canvasSize.dx;
}

bool DisplayModel::CanScrollLeft() const {
    return viewPort.x > 0;
}

bool DisplayModel::CanScrollDown() const {
    if (viewPort.dy >= canvasSize.dy) {
        return false;
    }
    return viewPort.y + viewPort.dy < canvasSize.dy;
}

bool DisplayModel::CanScrollUp() const {
    return viewPort.y > 0;
}

Size DisplayModel::GetCanvasSize() const {
    return canvasSize;
}

void DisplayModel::SetDisplayR2L(bool r2l) {
    displayR2L = r2l;
}

bool DisplayModel::GetDisplayR2L() const {
    return displayR2L;
}

// toRight: user moved/keyed toward the right (VK_RIGHT, swipe right).
// LTR: right = next page; manga R2L: left = next page (issue #3964).
bool DisplayModel::GoToPageHorizontal(bool toRight) {
    bool goNext = toRight != displayR2L;
    if (goNext) {
        return GoToNextPage();
    }
    return GoToPrevPage();
}

// called when we decide that the display needs to be redrawn
void DisplayModel::RepaintDisplay() {
    cb->Repaint();
}

static bool IsDisplayModelValid(DisplayModel* dm) {
    for (MainWindow* win : gWindows) {
        for (WindowTab* tab : win->Tabs()) {
            if (tab->AsFixed() == dm) {
                return true;
            }
        }
    }
    return false;
}

static void RenderFinishedOnUIThread(PageRenderRequest* req) {
    if (!IsDisplayModelValid(req->dm)) {
        delete req;
        return;
    }
    req->dm->RenderFinished(req);
    delete req;
}

void DisplayModel::RenderFinishedAsync(PageRenderRequest* req) {
    if (req->abort) {
        return;
    }
    auto* copy = new PageRenderRequest(*req);
    auto fn = MkFunc0(RenderFinishedOnUIThread, copy);
    uitask::Post(fn, "RenderFinished");
}

void DisplayModel::RenderFinished(PageRenderRequest* req) {
    if (req->errorCode != 0) {
        PageInfo* pageInfo = GetPageInfo(req->pageNo);
        if (pageInfo) {
            pageInfo->failedToRender = true;
        }
        RepaintDisplay();
    } else if (PageVisibleNearby(req->pageNo)) {
        RepaintDisplay();
    }
    // continue chained predictive rendering: render the next predicted page
    // (RequestPredictiveRendering stops the chain if the origin page is no
    // longer visible). A failed render still continues the chain so one bad
    // page doesn't stop predicting the rest.
    if (req->nPredictiveRequests > 0) {
        cb->RequestPredictiveRendering(this, req->predictiveOriginPageNo, req->predictiveRequests,
                                       req->nPredictiveRequests);
    }
}

bool DisplayModel::InPresentation() const {
    return inPresentation;
}

void DisplayModel::GetDisplayState(FileState* fs) {
    Str fileNameA = engine->FilePath();
    SetFileStatePath(fs, fileNameA);

    fs->useDefaultState = !gGlobalPrefs->rememberStatePerDocument;

    DisplayMode savedMode = GetDisplayMode();
    float savedZoom = zoomVirtual;
    if (inPresentation) {
        savedMode = presDisplayMode;
        savedZoom = presZoomVirtual;
    } else if (fsDisplayModeSaved) {
        savedMode = fsSavedDisplayMode;
    }
    str::ReplaceWithCopy(&fs->displayMode, DisplayModeToString(savedMode));
    ZoomToString(&fs->zoom, savedZoom, fs);

    ScrollState ss = GetScrollState();
    fs->pageNo = ss.page;
    fs->scrollPos = PointF();
    if (!inPresentation) {
        fs->scrollPos = PointF((float)ss.x, (float)ss.y);
    }
    fs->rotation = rotation;
    fs->displayR2L = displayR2L;

    str::Free(fs->decryptionKey);
    fs->decryptionKey = engine->decryptionKey.s ? str::Dup(engine->decryptionKey.s) : nullptr;
}

// Display rotation of an already-known rectangle. Engine::Transform also
// applies the engine's page transform and may load the page; use this when
// we only have an estimate and must not poke the engine.
static SizeF SizeAfterDisplayRotation(SizeF size, int rotation) {
    rotation = NormalizeRotation(rotation);
    if (rotation == 90 || rotation == 270) {
        std::swap(size.dx, size.dy);
    }
    return size;
}

SizeF DisplayModel::PageSizeAfterRotation(int pageNo, bool fitToContent) const {
    PageInfo* pageInfo = GetPageInfo(pageNo);
    ReportIf(!pageInfo);

    if (fitToContent && pageInfo->contentBox.IsEmpty()) {
        pageInfo->contentBox = engine->PageContentBox(pageNo);
        if (pageInfo->contentBox.IsEmpty()) {
            return PageSizeAfterRotation(pageNo);
        }
    }

    RectF pageBox = PageMediaBoxForLayout(pageNo);
    RectF box = fitToContent ? pageInfo->contentBox : pageBox;
    // EngineImages::Transform calls PageMediabox, which extracts (and may
    // decode) that page. Continuous fit-width walks every page here; for
    // un-measured comic/image pages we only have an estimate and must not
    // load them. Visible pages are measured later in
    // EnsureMediaBoxesForVisiblePages().
    if (useLazyMediaBoxes && !IsMediaBoxKnown(pageInfo->mediaBox)) {
        return SizeAfterDisplayRotation(box.Size(), rotation);
    }
    return engine->Transform(box, pageNo, 1.0, rotation).Size();
}

/* given 'columns' and an absolute 'pageNo', return the number of the first
   page in a row to which a 'pageNo' belongs e.g. if 'columns' is 2 and we
   have 5 pages in 3 rows (depending on showCover):

   Pages   Result           Pages   Result           Pages   Result (R2L)
   (1,2)   1                  (1)   1                (2,1)   1
   (3,4)   3                (2,3)   2                (4,3)   3
   (5)     5                (4,5)   4                  (5)   5
 */
static int FirstPageInARowNo(int pageNo, int columns, bool showCover) {
    if (showCover && columns > 1) {
        pageNo++;
    }
    int firstPageNo = pageNo - ((pageNo - 1) % columns);
    if (showCover && columns > 1 && firstPageNo > 1) {
        firstPageNo--;
    }
    return firstPageNo;
}

static int LastPageInARowNo(int pageNo, int columns, bool showCover, int pageCount) {
    int lastPageNo = FirstPageInARowNo(pageNo, columns, showCover) + columns - 1;
    if (showCover && pageNo < columns) {
        lastPageNo--;
    }
    return std::min(lastPageNo, pageCount);
}

// Stable-view nav point tracking: remember views the user dwelled on so
// that Back / Forward have history entries for plain scrolling and page
// turns, without turning every intermediate scroll position into a fake
// history entry. A view becomes a candidate once it is reached; it is
// committed into navHistory by the next view-changing operation, but only
// if the user stayed on it for long enough.
static constexpr DWORD64 kStableNavPointDelayMs = 1500;

static bool IsValidNavScrollState(const ScrollState& ss) {
    return ss.page > 0;
}

// two views count as "the same" when they show the same page within about
// half a viewport of each other, so small scroll adjustments don't create
// distinct history entries
static bool IsMeaningfullyDifferentNavScrollState(DisplayModel* dm, const ScrollState& a, const ScrollState& b) {
    if (!IsValidNavScrollState(a) || !IsValidNavScrollState(b)) {
        return true;
    }
    if (a.page != b.page) {
        return true;
    }
    Rect viewPort = dm->GetViewPort();
    double minDx = std::max(32.0, viewPort.dx * 0.50);
    double minDy = std::max(32.0, viewPort.dy * 0.50);
    return fabs(a.x - b.x) > minDx || fabs(a.y - b.y) > minDy;
}

// called right before an explicit view change with the current (pre-change)
// scroll state. Returns true if that view should be committed to navHistory
// (i.e. the caller should AddNavPoint()).
static bool ShouldCommitStableNavPointBeforeViewChange(DisplayModel* dm, const ScrollState& curr) {
    auto& nav = dm->stableNavPoint;
    if (nav.suppress || !IsValidNavScrollState(curr)) {
        return false;
    }

    DWORD64 now = GetTickCount64();

    // first explicit user movement: remember the initial view so that Back
    // can return to it. AddNavPoint() de-duplicates exact repeats.
    if (!nav.hasPending) {
        nav.pending = curr;
        nav.pendingTick = now;
        nav.hasPending = true;
        nav.lastCommitted = curr;
        nav.hasLastCommitted = true;
        return true;
    }

    // the visible view changed since the last candidate was recorded; start
    // the stability window again instead of committing an intermediate view
    if (IsMeaningfullyDifferentNavScrollState(dm, curr, nav.pending)) {
        nav.pending = curr;
        nav.pendingTick = now;
        return false;
    }

    if (now - nav.pendingTick < kStableNavPointDelayMs) {
        return false;
    }

    if (nav.hasLastCommitted && !IsMeaningfullyDifferentNavScrollState(dm, curr, nav.lastCommitted)) {
        return false;
    }

    nav.lastCommitted = curr;
    nav.hasLastCommitted = true;
    return true;
}

// called after a view change completed; the new view becomes the candidate
// for the next stable nav point
static void RememberStableNavPointCandidateAfterViewChange(DisplayModel* dm, const ScrollState& curr) {
    auto& nav = dm->stableNavPoint;
    if (nav.suppress || !IsValidNavScrollState(curr)) {
        return;
    }
    nav.pending = curr;
    nav.pendingTick = GetTickCount64();
    nav.hasPending = true;
}

// must call SetInitialViewSettings() after creation
DisplayModel::DisplayModel(EngineBase* engine, DocControllerCallback* cb) : DocController(cb) {
    this->engine = engine;
    ReportIf(!engine || engine->PageCount() <= 0);
    engineType = engine->kind;

    SetUiDpi(96);

    textSelection = new TextSelection(engine);
    textSearch = new TextSearch(engine);
}

// WindowMargin and PageSpacing are screen-space sizes written by the user at
// 100% scaling, so they have to be scaled to the dpi of the window showing the
// document. Called again from the WM_DPICHANGED path so moving the window to a
// monitor with different scaling re-scales them (the caller relayouts).
void DisplayModel::SetUiDpi(int dpi) {
    if (dpi <= 0) {
        dpi = 96;
    }
    uiDpi = dpi;
    WindowMargin m;
    Size sp;
    if (!engine->IsImageCollection()) {
        m = gGlobalPrefs->fixedPageUI.windowMargin;
        sp = gGlobalPrefs->fixedPageUI.pageSpacing;
    } else {
        m = gGlobalPrefs->comicBookUI.windowMargin;
        sp = gGlobalPrefs->comicBookUI.pageSpacing;
    }
    auto scale = [dpi](int n) -> int { return MulDiv(n, dpi, 96); };
    windowMargin = {scale(m.top), scale(m.right), scale(m.bottom), scale(m.left)};
    pageSpacing = {scale(sp.dx), scale(sp.dy)};
#ifdef DRAW_PAGE_SHADOWS
    windowMargin.top += scale(3);
    windowMargin.bottom += scale(5);
    windowMargin.right += scale(3);
    windowMargin.left += scale(1);
    pageSpacing.dx += scale(4);
    pageSpacing.dy += scale(4);
#endif
}

DisplayModel::~DisplayModel() {
    logf("~DisplayModel: 0x%p\n", this);
    pauseRendering = true;
    if (cb) {
        cb->CleanUp(this);
    }

    delete pdfSync;
    delete textSearch;
    delete textSelection;
    SafeEngineRelease(&engine);
    free(pagesInfo);
}

// the page size we assume when we don't know the real one: A4 (Letter in
// countries using the imperial system), in the document's own units
static RectF DefaultMediaBox(EngineBase* engine) {
    float fileDPI = engine->GetFileDPI();
    if (0 == GetMeasurementSystem()) {
        return {0, 0, (float)(21.0 / 2.54 * fileDPI), (float)(29.7 / 2.54 * fileDPI)};
    }
    return {0, 0, (float)(8.5 * fileDPI), 11 * fileDPI};
}

RectF DisplayModel::PageMediaBox(int pageNo) const {
    PageInfo* pi = GetPageInfo(pageNo);
    if (!pi) {
        return {};
    }
    if (pi->state == PageInfoState::Known) {
        return pi->mediaBox;
    }
    if (pi->state == PageInfoState::Error) {
        return {};
    }
    pi->mediaBox = engine->PageMediabox(pageNo);
    if (pi->mediaBox.IsEmpty()) {
        pi->mediaBox = DefaultMediaBox(engine);
        pi->state = PageInfoState::Error;
    } else {
        pi->state = PageInfoState::Known;
    }
    return pi->mediaBox;
}

// The media box to lay a page out with. For comic books and image directories
// measuring a page means reading the image off disk (slow on network drives),
// and continuous mode needs the size of *every* page. So we only measure the
// pages the user can actually see and lay the rest out with an estimate; they
// get measured (and the layout redone) once they scroll into view.
RectF DisplayModel::PageMediaBoxForLayout(int pageNo) const {
    PageInfo* pi = GetPageInfo(pageNo);
    if (!pi) {
        return {};
    }
    if (IsMediaBoxKnown(pi->mediaBox)) {
        // includes PageInfoState::Error, where mediaBox is the default box
        return pi->mediaBox;
    }
    if (!useLazyMediaBoxes) {
        return PageMediaBox(pageNo);
    }
    return estimatedMediaBox;
}

// Pick the media box to lay out not-yet-measured pages with: the most common
// size among the visible pages, since comic book pages are usually all the same
// size. Falls back to the pages measured so far (right after switching to
// continuous mode nothing is visible yet) and finally to A4.
void DisplayModel::UpdateEstimatedMediaBox() {
    if (!useLazyMediaBoxes) {
        return;
    }
    // a box this small is a thumbnail or a spacer image, not representative
    constexpr float kMinEstimateSize = 100;
    // distinct page sizes we tally; more than a handful means the pages have no
    // common size worth finding
    constexpr int kMaxSizes = 16;

    struct SizeCount {
        SizeF size;
        int nVisible;
        int nTotal;
    };
    SizeCount sizes[kMaxSizes];
    int nSizes = 0;

    for (int pageNo = 1; pageNo <= PageCount(); pageNo++) {
        PageInfo* pi = GetPageInfo(pageNo);
        if (!IsMediaBoxKnown(pi->mediaBox)) {
            continue;
        }
        SizeF size = pi->mediaBox.Size();
        int idx = -1;
        for (int i = 0; i < nSizes; i++) {
            if (sizes[i].size == size) {
                idx = i;
                break;
            }
        }
        if (idx < 0) {
            if (nSizes == kMaxSizes) {
                continue;
            }
            idx = nSizes++;
            sizes[idx] = {size, 0, 0};
        }
        sizes[idx].nTotal++;
        if (pi->visibleRatio > 0) {
            sizes[idx].nVisible++;
        }
    }

    SizeF best;
    int bestVisible = 0;
    int bestTotal = 0;
    for (int i = 0; i < nSizes; i++) {
        const SizeCount& sc = sizes[i];
        // most common among the visible pages; only if none of them is measured
        // does the count over all measured pages decide
        bool better = (sc.nVisible > bestVisible) || (bestVisible == 0 && sc.nTotal > bestTotal);
        if (better) {
            best = sc.size;
            bestVisible = sc.nVisible;
            bestTotal = sc.nTotal;
        }
    }

    if (bestTotal == 0 || best.dx < kMinEstimateSize || best.dy < kMinEstimateSize) {
        estimatedMediaBox = DefaultMediaBox(engine);
        return;
    }
    estimatedMediaBox = RectF(0, 0, best.dx, best.dy);
}

PageInfo* DisplayModel::GetPageInfo(int pageNo) const {
    if (!ValidPageNo(pageNo)) {
        return nullptr;
    }
    ReportIf(!pagesInfo);
    PageInfo* pi = &(pagesInfo[pageNo - 1]);
    return pi;
}

static DocumentLayoutMargin ToDocumentLayoutMargin(WindowMargin margin) {
    return {margin.top, margin.right, margin.bottom, margin.left};
}

static void CopyDocumentLayoutToPageInfo(const DisplayModel* dm, const DocumentLayout& layout) {
    for (int pageNo = 1; pageNo <= dm->PageCount(); pageNo++) {
        PageInfo* pageInfo = dm->GetPageInfo(pageNo);
        const DocumentLayoutPage* page = layout.GetPage(pageNo);
        if (!pageInfo || !page) {
            continue;
        }
        pageInfo->pos = page->pos;
        pageInfo->visibleRatio = page->visibleRatio;
        pageInfo->pageOnScreen = page->pageOnScreen;
        pageInfo->zoomReal = page->zoomReal;
        pageInfo->isShown = page->isShown;
    }
}

// Call this before the first Relayout
void DisplayModel::SetInitialViewSettings(DisplayMode newDisplayMode, int newStartPage, Size viewPort, int screenDPI) {
    totalViewPortSize = viewPort;
    if (screenDPI <= 0) {
        // a non-positive DPI (e.g. from bogus CustomScreenDPI in settings)
        // would make dpiFactor and therefore zoomReal negative
        screenDPI = 96;
    }
    dpiFactor = 1.0f * (float)screenDPI / engine->GetFileDPI();
    if (ValidPageNo(newStartPage)) {
        startPage = newStartPage;
    }

    displayMode = newDisplayMode;
    presDisplayMode = newDisplayMode;
    PageLayout layout = engine->preferredLayout;
    if (DisplayMode::Automatic == displayMode) {
        if (layout.type == PageLayout::Type::Single) {
            displayMode = DisplayMode::Continuous;
            if (layout.nonContinuous) {
                displayMode = DisplayMode::SinglePage;
            }
        } else if (layout.type == PageLayout::Type::Facing) {
            displayMode = DisplayMode::ContinuousFacing;
            if (layout.nonContinuous) {
                displayMode = DisplayMode::Facing;
            }
        } else if (layout.type == PageLayout::Type::Book) {
            displayMode = DisplayMode::ContinuousBookView;
            if (layout.nonContinuous) {
                displayMode = DisplayMode::BookView;
            }
        }
    }
    displayR2L = layout.r2l;
    BuildPagesInfo();
}

void DisplayModel::BuildPagesInfo() {
    ReportIf(pagesInfo);
    int pageCount = PageCount();
    pagesInfo = AllocArray<PageInfo>(pageCount);
    // +1 so we can index by pageNo (1-based)
    auto timeStart = TimeGet();
    defer {
        auto dur = TimeSinceInMs(timeStart);
        logf("DisplayModel::BuildPagesInfo took %.2f ms\n", dur);
    };

    int columns = ColumnsFromDisplayMode(displayMode);
    int newStartPage = startPage;
    if (IsBookView(displayMode) && newStartPage == 1 && columns > 1) {
        newStartPage--;
    }

    // measuring a page of a comic book / image directory reads the image off
    // disk, so measure them lazily (only what's visible) instead of measuring
    // every page for a continuous layout, which is very slow on network drives
    useLazyMediaBoxes = engine->IsImageCollection();
    estimatedMediaBox = DefaultMediaBox(engine);

    bool isCont = IsContinuous(displayMode);
    for (int pageNo = 1; pageNo <= pageCount; pageNo++) {
        PageInfo* pageInfo = &pagesInfo[pageNo - 1];
        pageInfo->visibleRatio = 0.0;
        // AllocArray() zeroes the memory, so the "not measured yet" size in the
        // PageInfo declaration doesn't apply - set it here
        pageInfo->mediaBox = RectF(0, 0, -1, -1);
        bool isShown = isCont || (newStartPage <= pageNo && pageNo < newStartPage + columns);
        pagesInfo[pageNo - 1].isShown = isShown;
    }

    if (isCont && useLazyMediaBoxes) {
        // no layout yet, so we don't know what will be visible: measure a few
        // pages from where we start so the estimate for the rest is a real page
        // size rather than A4
        constexpr int kSeedPages = 4;
        for (int pageNo = startPage; pageNo < startPage + kSeedPages && pageNo <= pageCount; pageNo++) {
            PageMediaBox(pageNo);
        }
    }
    // otherwise Relayout() measures the shown pages, on demand in non-continuous mode
}

// TODO: a better name e.g. ShouldShow() to better distinguish between
// before-layout info and after-layout visibility checks
bool DisplayModel::PageShown(int pageNo) const {
    if (!ValidPageNo(pageNo) || !pagesInfo) {
        return false;
    }
    return pagesInfo[pageNo - 1].isShown;
}

bool DisplayModel::PageVisible(int pageNo) const {
    PageInfo* pageInfo = GetPageInfo(pageNo);
    if (!pageInfo) {
        return false;
    }
    return pageInfo->visibleRatio > 0.0;
}

/* Return true if a page is visible or a page in a row below or above is visible */
bool DisplayModel::PageVisibleNearby(int pageNo) const {
    DisplayMode mode = GetDisplayMode();
    int columns = ColumnsFromDisplayMode(mode);

    pageNo = FirstPageInARowNo(pageNo, columns, IsBookView(mode));
    for (int i = pageNo - columns; i < pageNo + (2 * columns); i++) {
        if (ValidPageNo(i) && PageVisible(i)) {
            return true;
        }
    }
    return false;
}

/* Return true if the first page is fully visible and alone on a line in
   show cover mode (i.e. it's not possible to flip to a previous page) */
bool DisplayModel::FirstBookPageVisible() const {
    if (!IsBookView(GetDisplayMode())) {
        return false;
    }
    if (CurrentPageNo() != 1) {
        return false;
    }
    return true;
}

/* Return true if the last page is fully visible and alone on a line in
   facing or show cover mode (i.e. it's not possible to flip to a next page) */
bool DisplayModel::LastBookPageVisible() const {
    int count = PageCount();
    DisplayMode mode = GetDisplayMode();
    if (IsSingle(mode)) {
        return false;
    }
    if (CurrentPageNo() == count) {
        return true;
    }
    if (GetPageInfo(count)->visibleRatio < 1.0) {
        return false;
    }
    if (FirstPageInARowNo(count, ColumnsFromDisplayMode(mode), IsBookView(mode)) < count) {
        return false;
    }
    return true;
}

// ComicBookUI / ImageUI LimitToWindowWidth / LimitToWindowHeight (issue #2197).
static void GetImageLimitToWindowFlags(EngineBase* engine, bool& limitWidth, bool& limitHeight) {
    limitWidth = false;
    limitHeight = false;
    if (!engine || !engine->IsImageCollection()) {
        return;
    }
    if (engine->kind == kindEngineComicBooks) {
        limitWidth = gGlobalPrefs->comicBookUI.limitToWindowWidth;
        limitHeight = gGlobalPrefs->comicBookUI.limitToWindowHeight;
    } else {
        limitWidth = gGlobalPrefs->imageUI.limitToWindowWidth;
        limitHeight = gGlobalPrefs->imageUI.limitToWindowHeight;
    }
}

static bool IsVirtualFitZoom(float zoomVirtual) {
    return zoomVirtual == kZoomFitWidth || zoomVirtual == kZoomFitHeight || zoomVirtual == kZoomFitPage ||
           zoomVirtual == kZoomFitContent || zoomVirtual == kZoomShrinkToFit || zoomVirtual == kZoomFitByOrientation;
}

// Comics / image collections often have pages of different pixel sizes. In facing
// view a single shared zoom leaves a gap under the shorter page. Match display
// heights across the row instead (issue #5921), like dedicated comic readers.
// Returns <= 0 if not applicable / empty page; otherwise per-page zoom for pageNo.
static float ZoomRealMatchFacingHeights(const DisplayModel* dm, float zoomVirtual, int pageNo) {
    if (!dm || !dm->GetEngine() || !dm->GetEngine()->IsImageCollection()) {
        return 0;
    }
    DisplayMode mode = dm->GetDisplayMode();
    if (IsSingle(mode)) {
        return 0;
    }

    bool isShrinkToFit = (kZoomShrinkToFit == zoomVirtual);
    if (kZoomFitByOrientation == zoomVirtual) {
        Rect vp = dm->GetViewPort();
        zoomVirtual = (vp.dx > vp.dy) ? kZoomFitWidth : kZoomFitPage;
    }
    if (isShrinkToFit) {
        zoomVirtual = kZoomFitPage;
    }
    if (zoomVirtual != kZoomFitWidth && zoomVirtual != kZoomFitHeight && zoomVirtual != kZoomFitPage &&
        zoomVirtual != kZoomFitContent) {
        return 0;
    }

    bool fitToContent = (kZoomFitContent == zoomVirtual);
    int columns = ColumnsFromDisplayMode(mode);
    bool book = IsBookView(mode);
    int first = FirstPageInARowNo(pageNo, columns, book);
    int last = LastPageInARowNo(pageNo, columns, book, dm->PageCount());

    Rect viewPort = dm->GetViewPort();
    int areaDx = viewPort.dx - dm->windowMargin.left - dm->windowMargin.right;
    int areaDy = viewPort.dy - dm->windowMargin.top - dm->windowMargin.bottom;
    if (areaDx <= 0 || areaDy <= 0) {
        return 0;
    }

    // Equal display heights: targetH * (w1/h1 + w2/h2) + spacing = row width.
    float aspectSum = 0;
    int nInRow = 0;
    for (int i = first; i <= last; i++) {
        SizeF sz = dm->PageSizeAfterRotation(i, fitToContent);
        if (sz.dx <= 0 || sz.dy <= 0) {
            continue;
        }
        aspectSum += sz.dx / sz.dy;
        nInRow++;
    }
    if (aspectSum <= 0 || nInRow == 0) {
        return 0;
    }

    float spacing = (float)dm->pageSpacing.dx * (float)(nInRow - 1);
    float usableDx = (float)areaDx - spacing;
    if (usableDx <= 0) {
        return 0;
    }
    float targetHFromWidth = usableDx / aspectSum;
    float targetHFromHeight = (float)areaDy;
    float targetH;
    if (kZoomFitWidth == zoomVirtual) {
        targetH = targetHFromWidth;
    } else if (kZoomFitHeight == zoomVirtual) {
        targetH = targetHFromHeight;
    } else {
        // fit page / fit content
        targetH = std::min(targetHFromWidth, targetHFromHeight);
    }
    if (targetH <= 0) {
        return 0;
    }

    SizeF mySz = dm->PageSizeAfterRotation(pageNo, fitToContent);
    if (mySz.dy <= 0) {
        return 0;
    }
    float zoom = targetH / mySz.dy;
    if (isShrinkToFit) {
        zoom = std::min(zoom, 1.0f * dm->dpiFactor);
    }
    return zoom;
}

/* Given a zoom level that can include a "virtual" zoom levels like kZoomFitWidth,
   kZoomFitPage or kZoomFitContent, calculate an absolute zoom level */
float DisplayModel::ZoomRealFromVirtualForPage(float zoomVirtual, int pageNo) const {
    if (kZoomFitByOrientation == zoomVirtual) {
        // pick fit width for a landscape viewport, fit page for portrait, so the
        // most readable zoom is used as the window/screen is rotated (issue #702).
        // Recomputed on every relayout, so it tracks resizes automatically.
        zoomVirtual = (viewPort.dx > viewPort.dy) ? kZoomFitWidth : kZoomFitPage;
    }
    bool isShrinkToFit = (kZoomShrinkToFit == zoomVirtual);
    if (isShrinkToFit) {
        zoomVirtual = kZoomFitPage;
    }
    if (zoomVirtual != kZoomFitWidth && zoomVirtual != kZoomFitHeight && zoomVirtual != kZoomFitPage &&
        zoomVirtual != kZoomFitContent) {
        // Absolute zoom (e.g. 150%). Optionally cap each image/comic page so it
        // never exceeds the window width and/or height — lets single pages stay
        // large while double-page spreads shrink to fit (issue #2197).
        float zoom = zoomVirtual * 0.01f * dpiFactor;
        bool limitWidth = false;
        bool limitHeight = false;
        GetImageLimitToWindowFlags(engine, limitWidth, limitHeight);
        if (limitWidth || limitHeight) {
            if (limitWidth && limitHeight) {
                float fit = ZoomRealFromVirtualForPage(kZoomFitPage, pageNo);
                if (fit > 0) {
                    zoom = std::min(zoom, fit);
                }
            } else if (limitWidth) {
                float fit = ZoomRealFromVirtualForPage(kZoomFitWidth, pageNo);
                if (fit > 0) {
                    zoom = std::min(zoom, fit);
                }
            } else {
                float fit = ZoomRealFromVirtualForPage(kZoomFitHeight, pageNo);
                if (fit > 0) {
                    zoom = std::min(zoom, fit);
                }
            }
        }
        return zoom;
    }

    SizeF row;
    int columns = ColumnsFromDisplayMode(GetDisplayMode());

    bool fitToContent = (kZoomFitContent == zoomVirtual);
    if (fitToContent && columns > 1) {
        // Fit the content of all the pages in the same row into the visible area
        // (i.e. don't crop inner margins but just the left-most, right-most, etc.)
        int first = FirstPageInARowNo(pageNo, columns, IsBookView(GetDisplayMode()));
        int last = LastPageInARowNo(pageNo, columns, IsBookView(GetDisplayMode()), PageCount());
        RectF box;
        for (int i = first; i <= last; i++) {
            PageInfo* pageInfo = GetPageInfo(i);
            if (pageInfo->contentBox.IsEmpty()) {
                pageInfo->contentBox = engine->PageContentBox(i);
            }

            RectF mbox = PageMediaBoxForLayout(i);
            RectF pageBox = engine->Transform(mbox, i, 1.0, rotation);
            RectF contentBox = engine->Transform(pageInfo->contentBox, i, 1.0, rotation);
            if (contentBox.IsEmpty()) {
                contentBox = pageBox;
            }

            contentBox.x += row.dx;
            box = box.Union(contentBox);
            row.dx += pageBox.dx + (float)pageSpacing.dx;
        }
        row = box.Size();
    } else {
        row = PageSizeAfterRotation(pageNo, fitToContent);
        row.dx *= (float)columns;
        row.dx += (float)((double)pageSpacing.dx * (double)(columns - 1));
    }

    if (RectF(PointF(), row).IsEmpty()) {
        return 0;
    }

    int areaForPagesDx = viewPort.dx - windowMargin.left - windowMargin.right;
    int areaForPagesDy = viewPort.dy - windowMargin.top - windowMargin.bottom;
    if (areaForPagesDx <= 0 || areaForPagesDy <= 0) {
        return 0;
    }

    float zoomX = (float)areaForPagesDx / row.dx;
    float zoomY = (float)areaForPagesDy / row.dy;
    float zoom;
    // NOLINTNEXTLINE(bugprone-branch-clone): distinct fit modes that happen to pick the same axis
    if (kZoomFitWidth == zoomVirtual) {
        zoom = zoomX;
    } else if (kZoomFitHeight == zoomVirtual) { // NOLINT(bugprone-branch-clone)
        zoom = zoomY;                           // issue #1714
    } else if (zoomX < zoomY) {
        zoom = zoomX; // Fit Page / Fit Content: fit both axes
    } else {
        zoom = zoomY;
    }
    if (isShrinkToFit) {
        float maxZoom = 1.0f * dpiFactor;
        zoom = std::min(zoom, maxZoom);
    }
    return zoom;
}

int DisplayModel::FirstVisiblePageNo() const {
    ReportIf(!pagesInfo);
    if (!pagesInfo) {
        return kInvalidPageNo;
    }

    for (int pageNo = 1; pageNo <= PageCount(); ++pageNo) {
        PageInfo* pageInfo = GetPageInfo(pageNo);
        if (pageInfo->visibleRatio > 0.0) {
            return pageNo;
        }
    }

    /* If no pages are visible */
    return kInvalidPageNo;
}

// we consider the most visible page the current one
// (in continuous layout, there's no better criteria)
int DisplayModel::CurrentPageNo() const {
    if (!IsContinuous(GetDisplayMode())) {
        return startPage;
    }

    ReportIf(!pagesInfo);
    if (!pagesInfo) {
        return kInvalidPageNo;
    }
    // determine the most visible page
    int mostVisiblePage = kInvalidPageNo;
    float ratio = 0;

    for (int pageNo = 1; pageNo <= PageCount(); pageNo++) {
        PageInfo* pageInfo = GetPageInfo(pageNo);
        if (pageInfo->visibleRatio > ratio) {
            mostVisiblePage = pageNo;
            ratio = pageInfo->visibleRatio;
        }
    }

    /* No page overlaps the viewport at all. That is not only "above the first
       page / below the last one": when one page is much wider than the others
       the canvas is as wide as that page and the narrow ones sit centered in
       it, so scrolled fully left the viewport misses every page horizontally.
       Answering "the last page" there sent a restored view to the end of the
       document (issue #1438), so go by the vertical band the viewport is in,
       which is what "current page" means in continuous mode. */
    if (kInvalidPageNo == mostVisiblePage) {
        mostVisiblePage = PageCount();
        for (int pageNo = 1; pageNo <= PageCount(); pageNo++) {
            PageInfo* pageInfo = GetPageInfo(pageNo);
            if (pageInfo && viewPort.y < pageInfo->pos.y + pageInfo->pos.dy) {
                mostVisiblePage = pageNo;
                break;
            }
        }
    }

    return mostVisiblePage;
}

void DisplayModel::CalcZoomReal(float newZoomVirtual) {
    ReportIf(!IsValidZoom(newZoomVirtual));
    zoomVirtual = newZoomVirtual;
    int nPages = PageCount();
    bool matchFacingHeights =
        engine && engine->IsImageCollection() && !IsSingle(GetDisplayMode()) && IsVirtualFitZoom(newZoomVirtual);

    if (matchFacingHeights) {
        // per-page zoom so facing comic pages share the same display height
        float minZoom = (float)HUGE_VAL;
        for (int pageNo = 1; pageNo <= nPages; pageNo++) {
            PageInfo* pi = GetPageInfo(pageNo);
            if (!pi->isShown) {
                continue;
            }
            float zoom = ZoomRealMatchFacingHeights(this, newZoomVirtual, pageNo);
            if (zoom <= 0) {
                zoom = ZoomRealFromVirtualForPage(newZoomVirtual, pageNo);
            }
            ReportDebugIf(zoom < 0.01f);
            pi->zoomReal = zoom;
            if (zoom > 0) {
                minZoom = std::min(minZoom, zoom);
            }
        }
        ReportIf(minZoom == (float)HUGE_VAL);
        zoomReal = minZoom;
    } else if ((kZoomFitWidth == newZoomVirtual) || (kZoomFitHeight == newZoomVirtual) ||
               (kZoomFitPage == newZoomVirtual) || (kZoomShrinkToFit == newZoomVirtual) ||
               (kZoomFitByOrientation == newZoomVirtual)) {
        /* we want the same zoom for all pages, so use the smallest zoom
           across the pages so that the largest page fits. In most documents
           all pages are the same size anyway */
        float minZoom = (float)HUGE_VAL;
        for (int pageNo = 1; pageNo <= nPages; pageNo++) {
            PageInfo* pi = GetPageInfo(pageNo);
            if (pi->isShown) {
                float zoom = ZoomRealFromVirtualForPage(newZoomVirtual, pageNo);
                ReportDebugIf(zoom < 0.01f);
                pi->zoomReal = zoom;
                minZoom = std::min(minZoom, zoom);
            }
        }
        ReportIf(minZoom == (float)HUGE_VAL);
        zoomReal = minZoom;
    } else if (kZoomFitContent == newZoomVirtual) {
        float newZoom = ZoomRealFromVirtualForPage(newZoomVirtual, CurrentPageNo());
        // limit zooming in to 800% on almost empty pages. zoomReal is a percentage
        // premultiplied by dpiFactor (see the absolute zoom below), so the cap has
        // to be too. A bare 8.0f was never 800%: dpiFactor is screenDPI/fileDPI, so
        // for a PDF (72 dpi) it capped at 600% on a normal 96 dpi screen and 400%
        // at 150% scaling
        newZoom = std::min(newZoom, 8.0f * dpiFactor);
        // Zooming in by just a few pixels isn't worth invalidating the render
        // cache (including the prerendered neighbour pages), so incidental
        // relayouts - paging through a document whose content boxes differ
        // slightly - keep the current zoom. An explicit Fit Content request has
        // to be exact, though: the damping is sticky (the next request compares
        // against the zoom it just kept), so without exactFitContent the fit was
        // unreachable - Ctrl+3 left the content a few % short of the window and
        // pressing it again changed nothing. Only visible in the continuous
        // modes: GetZoomReal() recomputes the exact per-page zoom for the others,
        // and Relayout() writes that back over whatever was decided here.
        if (exactFitContent || newZoom < zoomReal || zoomReal / newZoom < 0.95 ||
            zoomReal < ZoomRealFromVirtualForPage(kZoomFitPage, CurrentPageNo())) {
            zoomReal = newZoom;
        }
        ReportIf(zoomReal < 0.01f);
        for (int pageNo = 1; pageNo <= nPages; pageNo++) {
            PageInfo* pageInfo = GetPageInfo(pageNo);
            pageInfo->zoomReal = zoomReal;
        }
    } else {
        // Absolute zoom. ZoomRealFromVirtualForPage may cap per page when
        // ComicBookUI/ImageUI LimitToWindowWidth/Height is set (issue #2197).
        zoomReal = zoomVirtual * 0.01f * dpiFactor;
        ReportIf(zoomReal < 0.01f);
        float minZoom = zoomReal;
        for (int pageNo = 1; pageNo <= nPages; pageNo++) {
            PageInfo* pageInfo = GetPageInfo(pageNo);
            float z = ZoomRealFromVirtualForPage(zoomVirtual, pageNo);
            pageInfo->zoomReal = z;
            if (z > 0 && z < minZoom) {
                minZoom = z;
            }
        }
        zoomReal = minZoom;
    }
}

float DisplayModel::GetZoomReal(int pageNo) const {
    DisplayMode mode = GetDisplayMode();
    if (IsContinuous(mode)) {
        PageInfo* pageInfo = GetPageInfo(pageNo);
        return pageInfo->zoomReal;
    }
    if (IsSingle(mode)) {
        return ZoomRealFromVirtualForPage(zoomVirtual, pageNo);
    }
    // facing / book: comics match heights per page (issue #5921); PDFs keep a
    // uniform zoom so both pages in the row share the same scale
    if (engine && engine->IsImageCollection() && IsVirtualFitZoom(zoomVirtual)) {
        float zoom = ZoomRealMatchFacingHeights(this, zoomVirtual, pageNo);
        if (zoom > 0) {
            return zoom;
        }
    }
    pageNo = FirstPageInARowNo(pageNo, ColumnsFromDisplayMode(mode), IsBookView(mode));
    if (pageNo == PageCount() || pageNo == 1 && IsBookView(mode)) {
        return ZoomRealFromVirtualForPage(zoomVirtual, pageNo);
    }
    float zoomCurr = ZoomRealFromVirtualForPage(zoomVirtual, pageNo);
    float zoomNext = ZoomRealFromVirtualForPage(zoomVirtual, pageNo + 1);
    return std::min(zoomCurr, zoomNext);
}

/* Given zoom and rotation, calculate the position of each page on a
   large sheet that is continuous view. Needs to be recalculated when:
     * zoom changes
     * rotation changes
     * switching between display modes
     * navigating to another page in non-continuous mode */
void DisplayModel::Relayout(float newZoomVirtual, int newRotation) {
    ReportIf(!pagesInfo);
    if (!pagesInfo) {
        return;
    }

    rotation = NormalizeRotation(newRotation);

    bool needHScroll = false;
    bool needVScroll = false;
    viewPort = Rect(viewPort.TL(), totalViewPortSize);
    bool hideScrollbars = ScrollbarsAreHidden();
    bool useOverlayScrollbar = ScrollbarsUseOverlay();

    // the size to lay out pages we haven't measured yet with; must be picked
    // before CalcZoomReal(), which already asks for page sizes
    UpdateEstimatedMediaBox();

    DocumentLayout layout;
    for (;;) {
        float currZoomReal = zoomReal;
        CalcZoomReal(newZoomVirtual);

        int newViewPortOffsetX = 0;
        if (0 != currZoomReal && kInvalidZoom != currZoomReal) {
            newViewPortOffsetX = (int)((float)viewPort.x * zoomReal / currZoomReal);
        }
        viewPort.x = newViewPortOffsetX;

        layout.Reset(PageCount());
        for (int pageNo = 1; pageNo <= PageCount(); pageNo++) {
            PageInfo* pi = GetPageInfo(pageNo);
            pi->usedEstimatedMediaBox = false;
            DocumentLayoutPage* layoutPage = layout.GetPage(pageNo);
            if (!layoutPage || !PageShown(pageNo)) {
                continue;
            }
            layoutPage->mediaBox = PageMediaBoxForLayout(pageNo);
            // remember that this page's position is only a guess, so that
            // EnsureMediaBoxesForVisiblePages() fixes it up once it's on screen
            pi->usedEstimatedMediaBox = !IsMediaBoxKnown(pi->mediaBox);
            layoutPage->zoomReal = GetZoomReal(pageNo);
        }

        DocumentLayoutParams params;
        params.displayMode = displayMode;
        params.startPage = startPage;
        params.viewPortSize = viewPort.Size();
        params.viewPortOffset = viewPort.TL();
        params.zoomVirtual = zoomVirtual;
        params.dpiFactor = dpiFactor;
        params.rotation = rotation;
        params.displayR2L = displayR2L;
        params.usePageZooms = true;
        params.windowMargin = ToDocumentLayoutMargin(windowMargin);
        params.pageSpacing = pageSpacing;
        params.paddingAfterLastPage = gGlobalPrefs->paddingAfterLastPage;
        layout.Relayout(params);

        if (!hideScrollbars && !useOverlayScrollbar && !needVScroll && layout.canvasSize.dy > layout.viewPort.dy) {
            needVScroll = true;
            viewPort = layout.viewPort;
            viewPort.dx -= DpiGetSystemMetrics(SM_CXVSCROLL, uiDpi);
            continue;
        }
        if (!hideScrollbars && !useOverlayScrollbar && !needHScroll && layout.canvasSize.dx > layout.viewPort.dx) {
            needHScroll = true;
            viewPort = layout.viewPort;
            viewPort.dy -= DpiGetSystemMetrics(SM_CYHSCROLL, uiDpi);
            continue;
        }
        break;
    }

    viewPort = layout.viewPort;
    canvasSize = layout.canvasSize;
    zoomReal = layout.zoomReal;
    CopyDocumentLayoutToPageInfo(this, layout);
}

// Re-do the layout after page sizes changed, keeping the user looking at the
// same place: pages before the first visible one may have grown or shrunk, so
// the scroll position has to move with it.
void DisplayModel::RelayoutKeepingView() {
    int anchorPageNo = FirstVisiblePageNo();
    if (!ValidPageNo(anchorPageNo)) {
        anchorPageNo = CurrentPageNo();
    }
    int dyInPage = 0;
    if (ValidPageNo(anchorPageNo)) {
        dyInPage = viewPort.y - GetPageInfo(anchorPageNo)->pos.y;
    }

    Relayout(zoomVirtual, rotation);

    if (ValidPageNo(anchorPageNo)) {
        int newY = GetPageInfo(anchorPageNo)->pos.y + dyInPage;
        viewPort.y = limitValue(newY, 0, std::max(0, canvasSize.dy - viewPort.dy));
    }
    RecalcVisibleParts();
    RenderVisibleParts();
    cb->UpdateScrollbars(canvasSize);
    RepaintDisplay();
}

static void NotifyMediaBoxRelayout(DisplayModel* dm, Str msg) {
    if (!gIsDebugBuild) {
        return;
    }
    for (MainWindow* win : gWindows) {
        if (win->AsFixed() != dm) {
            continue;
        }
        NotificationCreateArgs args;
        args.hwndParent = win->hwndCanvas;
        args.groupId = kNotifLazyLayout;
        args.timeoutMs = kNotif5SecsTimeOut;
        args.corner = NotifCorner::BottomLeft;
        args.msg = msg;
        ShowNotification(args);
        return;
    }
}

// Pages laid out with the estimated media box (see PageMediaBoxForLayout) are
// only a guess. Once such a page becomes visible we need its real size, so
// measure the newly visible ones and lay out again with the truth.
// Keyed off usedEstimatedMediaBox rather than "is it measured", because a page
// can get measured by something else (e.g. rendering it) without the layout,
// which is what's stale here, having been redone.
// Returns true if we re-laid out.
bool DisplayModel::EnsureMediaBoxesForVisiblePages() {
    if (!useLazyMediaBoxes || !pagesInfo || inMediaBoxUpdate) {
        return false;
    }
    inMediaBoxUpdate = true;
    defer {
        inMediaBoxUpdate = false;
    };

    int nMeasured = 0;
    // what we measured, as "12 (1200x1800), 13 (1600x1000)". Only so many fit
    // in a notification, the rest are counted
    constexpr int kMaxListed = 4;
    TempStr measured = StrL("");
    int nListed = 0;

    // a relayout can scroll further un-measured pages into view; bounded so
    // that a document of very small pages can't spin here
    constexpr int kMaxRelayouts = 4;
    for (int i = 0; i < kMaxRelayouts; i++) {
        int nInPass = 0;
        for (int pageNo = 1; pageNo <= PageCount(); pageNo++) {
            PageInfo* pi = GetPageInfo(pageNo);
            if (pi->visibleRatio <= 0 || !pi->usedEstimatedMediaBox) {
                continue;
            }
            PageMediaBox(pageNo);
            nInPass++;
            if (nListed < kMaxListed) {
                // pi->mediaBox, not PageMediaBox()'s result: for a page we
                // failed to measure that returns nothing, while mediaBox holds
                // the default box we laid it out with
                SizeF size = pi->mediaBox.Size();
                Str sep = nListed > 0 ? StrL(", ") : StrL("");
                measured = fmt("%s%s%d (%dx%d)", measured, sep, pageNo, (int)size.dx, (int)size.dy);
                nListed++;
            }
        }
        if (nInPass == 0) {
            break;
        }
        nMeasured += nInPass;
        RelayoutKeepingView();
    }
    if (nMeasured == 0) {
        return false;
    }

    Str pageWord = nMeasured == 1 ? StrL("page") : StrL("pages");
    TempStr msg = fmt("re-layout: measured %s %s", pageWord, measured);
    if (nMeasured > nListed) {
        msg = fmt("%s and %d more", msg, nMeasured - nListed);
    }
    logf("EnsureMediaBoxesForVisiblePages: %s\n", msg);
    NotifyMediaBoxRelayout(this, msg);
    return true;
}

void DisplayModel::ChangeStartPage(int newStartPage) {
    ReportIf(!ValidPageNo(newStartPage));
    ReportIf(IsContinuous(GetDisplayMode()));

    int columns = ColumnsFromDisplayMode(GetDisplayMode());
    startPage = newStartPage;
    if (IsBookView(GetDisplayMode()) && newStartPage == 1 && columns > 1) {
        newStartPage--;
    }
    for (int pageNo = 1; pageNo <= PageCount(); pageNo++) {
        bool isShown = IsContinuous(GetDisplayMode()) || (pageNo >= newStartPage && pageNo < newStartPage + columns);
        pagesInfo[pageNo - 1].isShown = isShown;
        PageInfo* pageInfo = GetPageInfo(pageNo);
        pageInfo->visibleRatio = 0.0;
    }
    Relayout(zoomVirtual, rotation);
}

/* Given positions of each page in a large sheet that is continuous view and
   coordinates of a current view into that large sheet, calculate which
   parts of each page is visible on the screen.
   Needs to be recalucated after scrolling the view. */
void DisplayModel::RecalcVisibleParts() const {
    ReportIf(!pagesInfo);
    if (!pagesInfo) {
        return;
    }

    DocumentLayout layout;
    layout.Reset(PageCount());
    layout.viewPort = viewPort;
    for (int pageNo = 1; pageNo <= PageCount(); ++pageNo) {
        DocumentLayoutPage* layoutPage = layout.GetPage(pageNo);
        PageInfo* pageInfo = GetPageInfo(pageNo);
        if (!layoutPage || !pageInfo) {
            continue;
        }
        layoutPage->pos = pageInfo->pos;
        layoutPage->isShown = pageInfo->isShown;
        layoutPage->zoomReal = pageInfo->zoomReal;
    }
    layout.RecalcVisibleParts();
    CopyDocumentLayoutToPageInfo(this, layout);
}

int DisplayModel::GetPageNoByPoint(Point pt) const {
    // no reasonable answer possible, if zoom hasn't been set yet
    if (zoomReal <= 0) {
        return -1;
    }

    for (int pageNo = 1; pageNo <= PageCount(); ++pageNo) {
        if (!pagesInfo[pageNo - 1].isShown) {
            continue;
        }
        PageInfo* pageInfo = GetPageInfo(pageNo);

        if (pageInfo->pageOnScreen.Contains(pt)) {
            return pageNo;
        }
    }

    return -1;
}

int DisplayModel::GetPageNextToPoint(Point pt) const {
    if (zoomReal <= 0) {
        return startPage;
    }

    unsigned int maxDist = UINT_MAX;
    int closest = startPage;

    for (int pageNo = 1; pageNo <= PageCount(); ++pageNo) {
        if (!pagesInfo[pageNo - 1].isShown) {
            continue;
        }
        PageInfo* pageInfo = GetPageInfo(pageNo);

        if (pageInfo->pageOnScreen.Contains(pt)) {
            return pageNo;
        }

        Rect r = pageInfo->pageOnScreen;
        unsigned int dist = distSq(pt.x - r.x - (r.dx / 2), pt.y - r.y - (r.dy / 2));
        if (dist < maxDist) {
            closest = pageNo;
            maxDist = dist;
        }
    }

    return closest;
}

// TODO: try to track down why sometimes zoom on a page is 0
// like https://github.com/sumatrapdfreader/sumatrapdf/issues/2014
static float getZoomSafe(DisplayModel* dm, int pageNo, const PageInfo* pageInfo) {
    float zoom = pageInfo->zoomReal;
    if (zoom > 0) {
        return zoom;
    }
    Str name = dm->GetFilePath();
    logf(
        "getZoomSafe: invalid zoom in doc: %s\npageNo: %d\npageInfo->zoomReal\n%.2f\ndm->zoomReal: %.2f\n"
        "dm->zoomVirtual: %.2f\n",
        name, pageNo, zoom, pageInfo->zoomReal, dm->zoomReal, dm->zoomVirtual);
    ReportDebugIf(true);

    if (dm->zoomReal > 0) {
        return dm->zoomReal;
    }

    if (dm->zoomVirtual > 0) {
        return dm->zoomVirtual;
    }
    // hail mary, return 100%
    return 1.f;
}

Point DisplayModel::CvtToScreen(int pageNo, PointF pt) {
    PageInfo* pageInfo = GetPageInfo(pageNo);
    if (!pageInfo) {
        Str isValid = ValidPageNo(pageNo) ? "yes" : "no";
        logf("DisplayModel::CvtToScreen: GetPageInfo(%d) failed, is valid page: %s\n", pageNo, isValid);
        ReportIf(!pageInfo);
        return {};
    }

    float zoom = getZoomSafe(this, pageNo, pageInfo);

    PointF p = engine->Transform(pt, pageNo, zoom, rotation);
    // don't add the full 0.5 for rounding to account for precision errors
    Rect r = pageInfo->pageOnScreen;
    p.x += 0.499f + (float)r.x;
    p.y += 0.499f + (float)r.y;

    return ToPoint(p);
}

Rect DisplayModel::CvtToScreen(int pageNo, RectF r) {
    Point TL = CvtToScreen(pageNo, r.TL());
    Point BR = CvtToScreen(pageNo, r.BR());
    return Rect::FromXY(TL, BR);
}

PointF DisplayModel::CvtFromScreen(Point pt, int pageNo) {
    if (!ValidPageNo(pageNo)) {
        pageNo = GetPageNextToPoint(pt);
    }

    const PageInfo* pageInfo = GetPageInfo(pageNo);
    ReportIf(!pageInfo);
    if (!pageInfo) {
        return {};
    }

    // don't add the full 0.5 for rounding to account for precision errors
    Rect r = pageInfo->pageOnScreen;
    PointF p = PointF((float)pt.x - 0.499f - (float)r.x, (float)pt.y - 0.499f - (float)r.y);

    float zoom = getZoomSafe(this, pageNo, pageInfo);
    return engine->Transform(p, pageNo, zoom, rotation, true);
}

RectF DisplayModel::CvtFromScreen(Rect r, int pageNo) {
    if (!ValidPageNo(pageNo)) {
        pageNo = GetPageNextToPoint(r.TL());
    }

    PointF TL = CvtFromScreen(r.TL(), pageNo);
    PointF BR = CvtFromScreen(r.BR(), pageNo);
    return RectF::FromXY(TL, BR);
}

// Given position 'x'/'y' in the draw area, returns a structure describing
// a link or nullptr if there is no link at this position.
// don't delete the result
IPageElement* DisplayModel::GetElementAtPos(Point pt, int* pageNoOut) {
    int pageNo = GetPageNoByPoint(pt);
    if (!ValidPageNo(pageNo)) {
        return nullptr;
    }
    // only return visible elements (for cursor interaction)
    if (!Rect(Point(), viewPort.Size()).Contains(pt)) {
        return nullptr;
    }
    if (pageNoOut) {
        *pageNoOut = pageNo;
    }
    PointF pos = CvtFromScreen(pt, pageNo);
    return engine->GetElementAtPos(pageNo, pos);
}

Annotation* DisplayModel::GetAnnotationAtPos(Point pt, Annotation* annot) {
    if (AnnotationsAreDisabled()) {
        return nullptr;
    }
    int pageNo = GetPageNoByPoint(pt);
    if (!ValidPageNo(pageNo)) {
        return nullptr;
    }
    // only return visible elements (for cursor interaction)
    if (!Rect(Point(), viewPort.Size()).Contains(pt)) {
        return nullptr;
    }

    PointF pos = CvtFromScreen(pt, pageNo);
    return EngineGetAnnotationAtPos(engine, pageNo, pos, annot);
}

// form fields (widgets) are hit-tested separately from annotations
Annotation* DisplayModel::GetWidgetAtPos(Point pt) {
    if (AnnotationsAreDisabled()) {
        return nullptr;
    }
    int pageNo = GetPageNoByPoint(pt);
    if (!ValidPageNo(pageNo)) {
        return nullptr;
    }
    if (!Rect(Point(), viewPort.Size()).Contains(pt)) {
        return nullptr;
    }
    PointF pos = CvtFromScreen(pt, pageNo);
    return EngineGetWidgetAtPos(engine, pageNo, pos);
}

// note: returns false for pages that haven't been rendered yet
bool DisplayModel::IsOverText(Point pt) {
    int pageNo = GetPageNoByPoint(pt);
    if (!ValidPageNo(pageNo)) {
        return false;
    }
    // only return visible elements (for cursor interaction)
    if (!Rect(Point(), viewPort.Size()).Contains(pt)) {
        return false;
    }
    if (!engine->HasTextForPage(pageNo)) {
        return false;
    }

    PointF pos = CvtFromScreen(pt, pageNo);
    return textSelection->IsOverGlyph(pageNo, pos.x, pos.y);
}

void DisplayModel::RenderVisibleParts() {
    int firstVisiblePage = 0;
    int lastVisiblePage = 0;

    for (int pageNo = 1; pageNo <= PageCount(); ++pageNo) {
        PageInfo* pageInfo = GetPageInfo(pageNo);
        if (pageInfo->visibleRatio > 0.0) {
            ReportIf(!pagesInfo[pageNo - 1].isShown);
            if (0 == firstVisiblePage) {
                firstVisiblePage = pageNo;
            }
            lastVisiblePage = pageNo;
        }
    }
    // no page is visible if e.g. the window is resized
    // vertically until only the title bar remains visible
    if (0 == firstVisiblePage) {
        return;
    }

    // rendering happens LIFO except if the queue is currently
    // empty, so request the visible pages first and last to
    // make sure they're rendered before the predicted pages
    for (int pageNo = firstVisiblePage; pageNo <= lastVisiblePage; pageNo++) {
        cb->RequestRendering(this, pageNo);
    }

    if (gPredictiveRender) {
        // build a chain of pages to prerender (most likely next page first),
        // then render them one at a time so they don't flood the queue.
        // The chain is anchored to lastVisiblePage and stops once it scrolls
        // out of view (see RenderCache::RequestPredictiveRendering).
        int pred[kMaxPredictiveRequests];
        int nPred = 0;
        if (lastVisiblePage < PageCount()) {
            pred[nPred++] = lastVisiblePage + 1;
        }
        if (firstVisiblePage > 1) {
            pred[nPred++] = firstVisiblePage - 1;
        }
        // prerender two more pages in facing and book view modes
        if (!IsSingle(GetDisplayMode())) {
            if (lastVisiblePage + 1 < PageCount()) {
                pred[nPred++] = lastVisiblePage + 2;
            }
            if (firstVisiblePage > 2) {
                pred[nPred++] = firstVisiblePage - 2;
            }
        }
        if (nPred > 0) {
            cb->RequestPredictiveRendering(this, lastVisiblePage, pred, nPred);
        }
    }

    // re-request the visible pages again so:
    // * they get picked first by rendering thread
    // * if queue fills up, the invisible pages from predictive rendering
    //   wont be rendered
    for (int pageNo = lastVisiblePage; pageNo >= firstVisiblePage; pageNo--) {
        cb->RequestRendering(this, pageNo);
    }
}

void DisplayModel::SetViewPortSize(Size newViewPortSize) {
    ScrollState ss;

    bool isDocReady = ValidPageNo(startPage) && zoomReal != 0;
    if (isDocReady) {
        ss = GetScrollState();
    }

    totalViewPortSize = newViewPortSize;
    // during document swap in ReplaceDocumentInCurrentTab a WM_PAINT can
    // arrive before a valid zoom is set; relayout would corrupt the state
    if (!IsValidZoom(zoomVirtual)) {
        cb->UpdateScrollbars(canvasSize);
        return;
    }
    Relayout(zoomVirtual, rotation);

    if (isDocReady) {
        // when fitting to content, let GoToPage do the necessary scrolling
        if (zoomVirtual != kZoomFitContent) {
            SetScrollState(ss);
        } else {
            GoToPage(ss.page, 0);
        }
    } else {
        RecalcVisibleParts();
        EnsureMediaBoxesForVisiblePages();
        RenderVisibleParts();
        cb->UpdateScrollbars(canvasSize);
    }
}

RectF DisplayModel::GetContentBox(int pageNo) const {
    RectF cbox{};
    // we cache the contentBox
    PageInfo* pageInfo = GetPageInfo(pageNo);
    if (!pageInfo) {
        return cbox;
    }
    if (pageInfo->contentBox.IsEmpty()) {
        pageInfo->contentBox = engine->PageContentBox(pageNo);
    }
    cbox = pageInfo->contentBox;
    float zoom = pageInfo->zoomReal;
    // TODO: must be a better way
    if (zoom == 0) {
        zoom = zoomReal;
    }
    return engine->Transform(cbox, pageNo, zoom, rotation);
}

/* get the (screen) coordinates of the point where a page's actual
   content begins (relative to the page's top left corner) */
Point DisplayModel::GetContentStart(int pageNo) const {
    RectF contentBox = GetContentBox(pageNo);
    if (contentBox.IsEmpty()) {
        return {0, 0};
    }
    return ToPoint(contentBox.TL());
}

// TODO: what's GoToPage supposed to do for Facing at 400% zoom?
void DisplayModel::GoToPage(int pageNo, int scrollY, bool addNavPt, int scrollX) {
    if (!ValidPageNo(pageNo)) {
        logf("DisplayModel::GoToPage: invalid pageNo: %d, nPages: %d\n", pageNo, engine->PageCount());
        ReportIf(true);
        return;
    }

    if (addNavPt || ShouldCommitStableNavPointBeforeViewChange(this, GetScrollState())) {
        AddNavPoint();
    }

    /* in facing mode only start at odd pages (odd because page
       numbering starts with 1, so odd is really an even page) */
    if (!IsSingle(GetDisplayMode())) {
        pageNo = FirstPageInARowNo(pageNo, ColumnsFromDisplayMode(GetDisplayMode()), IsBookView(GetDisplayMode()));
    }

    if (!IsContinuous(GetDisplayMode())) {
        /* in single page mode going to another page involves recalculating
           the size of canvas */
        ChangeStartPage(pageNo);
    } else if (kZoomFitContent == zoomVirtual) {
        // make sure that CalcZoomReal uses the correct page to calculate
        // the zoom level for (visibility will be recalculated below anyway)
        for (int i = PageCount(); i > 0; i--) {
            GetPageInfo(i)->visibleRatio = (i == pageNo ? 1.0f : 0);
        }
        Relayout(zoomVirtual, rotation);
    }
    // lf("DisplayModel::GoToPage(pageNo=%d, scrollY=%d)", pageNo, scrollY);
    PageInfo* pageInfo = GetPageInfo(pageNo);

    // intentionally ignore scrollX and scrollY when fitting to content
    if (kZoomFitContent == zoomVirtual) {
        // scroll down to where the actual content starts
        Point start = GetContentStart(pageNo);
        scrollX = start.x;
        scrollY = start.y;
        if (ColumnsFromDisplayMode(GetDisplayMode()) > 1) {
            int nColumns = ColumnsFromDisplayMode(GetDisplayMode());
            bool isBook = IsBookView(GetDisplayMode());
            int nPages = PageCount();
            int lastPageNo = LastPageInARowNo(pageNo, nColumns, isBook, nPages);
            Point second = GetContentStart(lastPageNo);
            scrollY = std::min(scrollY, second.y);
        }
        viewPort.x = scrollX + pageInfo->pos.x - windowMargin.left;
    } else if (-1 != scrollX) {
        viewPort.x = scrollX;
    } else if (1 == pageNo && IsBookView(GetDisplayMode())) {
        // make sure to not display the blank space beside the first page in cover mode
        viewPort.x = pageInfo->pos.x - windowMargin.left;
    } else if (viewPort.x >= pageInfo->pos.x + pageInfo->pos.dx) {
        // make sure that at least part of the page is visible
        viewPort.x = pageInfo->pos.x;
    }
    // NOTE: a caller-supplied scrollX is an absolute canvas position that
    // already accounts for which column of the row the page is in, so it must
    // be used as-is. We used to add the previous page's width when pageNo was
    // the second page of a facing/book-view row, from a time when scrollX was
    // page-relative; with SetScrollState() as the only caller passing one,
    // that scrolled a whole page too far right when restoring a view of such a
    // page (tab switch, window resize, session restore) (fixes #3591).

    /* Hack: if an image is smaller in Y axis than the draw area, then we center
       the image by setting pageInfo->currPos.y in RecalcPagesInfo. So we shouldn't
       scroll (adjust viewPort.y) there because it defeats the purpose.
       TODO: is there a better way of y-centering? */
    viewPort.y = scrollY;
    // Move the next page to the top (unless the remaining pages fit onto a single screen)
    if (IsContinuous(GetDisplayMode())) {
        viewPort.y = pageInfo->pos.y - windowMargin.top + scrollY;
    }

    viewPort.x = limitValue(viewPort.x, 0, canvasSize.dx - viewPort.dx);
    viewPort.y = limitValue(viewPort.y, 0, canvasSize.dy - viewPort.dy);

    RecalcVisibleParts();
    EnsureMediaBoxesForVisiblePages();
    RenderVisibleParts();
    cb->UpdateScrollbars(canvasSize);
    cb->PageNoChanged(this, pageNo);
    RepaintDisplay();
    RememberStableNavPointCandidateAfterViewChange(this, GetScrollState());
}

void DisplayModel::SetDisplayMode(DisplayMode newDisplayMode, bool keepContinuous) {
    if (keepContinuous && IsContinuous(displayMode)) {
        if (newDisplayMode == DisplayMode::SinglePage) {
            newDisplayMode = DisplayMode::Continuous;
        } else if (newDisplayMode == DisplayMode::Facing) {
            newDisplayMode = DisplayMode::ContinuousFacing;
        } else if (newDisplayMode == DisplayMode::BookView) {
            newDisplayMode = DisplayMode::ContinuousBookView;
        }
    }
    if (displayMode == newDisplayMode) {
        return;
    }

    int currPageNo = CurrentPageNo();
    if (IsFacing(newDisplayMode) && IsBookView(displayMode) && currPageNo < PageCount()) {
        currPageNo++;
    }
    displayMode = newDisplayMode;
    if (IsContinuous(newDisplayMode) && useLazyMediaBoxes) {
        // only the pages the user is looking at get measured on the ui thread;
        // the rest are laid out with an estimate derived from them and measured
        // as they scroll into view (see EnsureMediaBoxesForVisiblePages)
        for (int pageNo = 1; pageNo <= PageCount(); pageNo++) {
            if (pagesInfo[pageNo - 1].visibleRatio > 0) {
                PageMediaBox(pageNo);
            }
        }
    }
    if (IsContinuous(newDisplayMode)) {
        /* mark all pages as shown but not yet visible. The equivalent code
           for non-continuous mode is in DisplayModel::changeStartPage() called
           from DisplayModel::GoToPage() */
        for (int pageNo = 1; pageNo <= PageCount(); pageNo++) {
            pagesInfo[pageNo - 1].isShown = true;
            PageInfo* pageInfo = &(pagesInfo[pageNo - 1]);
            pageInfo->visibleRatio = 0.0;
        }
        Relayout(zoomVirtual, rotation);
    }
    GoToPage(currPageNo, 0);
}

void DisplayModel::SetInPresentation(bool enable) {
    inPresentation = enable;
    if (inPresentation) {
        presDisplayMode = displayMode;
        presZoomVirtual = zoomVirtual;
        // disable the window margin during presentations
        windowMargin.top = windowMargin.right = windowMargin.bottom = windowMargin.left = 0;
        // Fullscreen.DisplayMode overrides the built-in single-page layout
        DisplayMode mode = DisplayMode::SinglePage;
        TryParseDisplayMode(gGlobalPrefs->fullscreen.displayMode, &mode);
        SetDisplayMode(mode);
        SetZoomVirtual(kZoomFitPage, nullptr);
        return;
    }
    if (engine && engine->IsImageCollection()) {
        windowMargin = gGlobalPrefs->comicBookUI.windowMargin;
    } else {
        windowMargin = gGlobalPrefs->fixedPageUI.windowMargin;
    }
#ifdef DRAW_PAGE_SHADOWS
    windowMargin.top += 3;
    windowMargin.bottom += 5;
    windowMargin.right += 3;
    windowMargin.left += 1;
#endif
    SetDisplayMode(presDisplayMode);
    if (!IsValidZoom(presZoomVirtual)) {
        presZoomVirtual = zoomVirtual;
    }
    SetZoomVirtual(presZoomVirtual, nullptr);
}

// Windowed fullscreen (F11 / Shift+Ctrl+L): switch to Fullscreen.DisplayMode
// when that setting is set, and restore the previous layout on exit.
// Presentation uses SetInPresentation instead.
void DisplayModel::ApplyFullscreenDisplayMode(bool enable) {
    DisplayMode wanted{};
    bool havePref = TryParseDisplayMode(gGlobalPrefs->fullscreen.displayMode, &wanted);
    if (enable) {
        if (!havePref) {
            return;
        }
        if (!fsDisplayModeSaved) {
            fsSavedDisplayMode = displayMode;
            fsDisplayModeSaved = true;
        }
        SetDisplayMode(wanted);
        return;
    }
    if (fsDisplayModeSaved) {
        SetDisplayMode(fsSavedDisplayMode);
        fsDisplayModeSaved = false;
    }
}

// Page-relative view offset so next/prev page can open at the same place
// on the page (RememberViewOffsetOnPageTurn). scrollX is an absolute
// canvas x (what GoToPage expects); scrollY is page-relative.
static void GetRememberedViewOffset(DisplayModel* dm, int pageNo, int* scrollXOut, int* scrollYOut) {
    PageInfo* pi = dm->GetPageInfo(pageNo);
    if (!pi) {
        *scrollXOut = -1;
        *scrollYOut = 0;
        return;
    }
    *scrollXOut = dm->viewPort.x;
    if (IsContinuous(dm->GetDisplayMode())) {
        *scrollYOut = dm->viewPort.y - pi->pos.y + dm->windowMargin.top;
    } else {
        *scrollYOut = dm->viewPort.y;
    }
}

/* In continuous mode just scrolls to the next page. In single page mode
   rebuilds the display model for the next page.
   Returns true if advanced to the next page or false if couldn't advance
   (e.g. because already was at the last page) */
bool DisplayModel::GoToNextPage() {
    int columns = ColumnsFromDisplayMode(GetDisplayMode());
    int currPageNo = CurrentPageNo();
    // Fully display the current page, if the previous page is still visible
    if (ValidPageNo(currPageNo - columns) && PageVisible(currPageNo - columns) &&
        GetPageInfo(currPageNo)->visibleRatio < 1.0) {
        GoToPage(currPageNo, false);
        return true;
    }
    int firstPageInNewRow = FirstPageInARowNo(currPageNo + columns, columns, IsBookView(GetDisplayMode()));
    if (firstPageInNewRow > PageCount()) {
        /* we're on a last row or after it, can't go any further */
        return false;
    }
    int scrollY = 0;
    int scrollX = -1;
    if (gGlobalPrefs->rememberViewOffsetOnPageTurn) {
        GetRememberedViewOffset(this, currPageNo, &scrollX, &scrollY);
    }
    GoToPage(firstPageInNewRow, scrollY, false, scrollX);
    return true;
}

// true when the view is at the bottom and GoToNextPage would fail
bool DisplayModel::IsAtDocumentEnd() const {
    if (CanScrollDown()) {
        return false;
    }
    // continuous: one canvas for the whole document
    if (IsContinuous(GetDisplayMode())) {
        return true;
    }
    // non-continuous: also on the last page row (GoToNextPage would fail)
    int columns = ColumnsFromDisplayMode(GetDisplayMode());
    int currPageNo = CurrentPageNo();
    int firstPageInNewRow = FirstPageInARowNo(currPageNo + columns, columns, IsBookView(GetDisplayMode()));
    return firstPageInNewRow > PageCount();
}

bool DisplayModel::GoToPrevPage(int scrollY) {
    int columns = ColumnsFromDisplayMode(GetDisplayMode());
    int currPageNo = CurrentPageNo();

    Point top;
    if ((0 == scrollY || -1 == scrollY) && zoomVirtual == kZoomFitContent) {
        currPageNo = FirstVisiblePageNo();
        top = GetContentStart(currPageNo);
    }

    PageInfo* pageInfo = GetPageInfo(currPageNo);
    if (!pageInfo) {
        return false;
    }
    if (zoomVirtual == kZoomFitContent && -pageInfo->pageOnScreen.y <= top.y) {
        scrollY = 0; // continue, even though the current page isn't fully visible
    } else if (std::max(-pageInfo->pageOnScreen.y, 0) > scrollY && IsContinuous(GetDisplayMode())) {
        /* the current page isn't fully visible, so show it first */
        GoToPage(currPageNo, scrollY);
        return true;
    }
    int firstPageInNewRow = FirstPageInARowNo(currPageNo - columns, columns, IsBookView(GetDisplayMode()));
    if (firstPageInNewRow < 1 || 1 == currPageNo) {
        /* we're on a first page, can't go back */
        return false;
    }

    // scroll to the bottom of the page
    if (-1 == scrollY) {
        scrollY = GetPageInfo(firstPageInNewRow)->pageOnScreen.dy;
    } else if (gGlobalPrefs->rememberViewOffsetOnPageTurn && scrollY == 0) {
        int scrollX = -1;
        GetRememberedViewOffset(this, currPageNo, &scrollX, &scrollY);
        GoToPage(firstPageInNewRow, scrollY, false, scrollX);
        return true;
    }

    GoToPage(firstPageInNewRow, scrollY);
    return true;
}

bool DisplayModel::GoToLastPage() {
    int columns = ColumnsFromDisplayMode(GetDisplayMode());
    int currPageNo = CurrentPageNo();
    int newPageNo = PageCount();
    int firstPageInLastRow = FirstPageInARowNo(newPageNo, columns, IsBookView(GetDisplayMode()));

    if (currPageNo == firstPageInLastRow) { /* are we on the last page already ? */
        return false;
    }
    GoToPage(firstPageInLastRow, 0, true);
    return true;
}

bool DisplayModel::GoToFirstPage() {
    if (IsContinuous(GetDisplayMode())) {
        if (0 == viewPort.y) {
            return false;
        }
    } else {
        ReportIf(!PageShown(startPage));
        if (1 == startPage) {
            /* we're on a first page already */
            return false;
        }
    }
    GoToPage(1, 0, true);
    return true;
}

bool DisplayModel::HandleLink(IPageDestination* dest, ILinkHandler* lh) {
    return engine->HandleLink(dest, lh);
}

void DisplayModel::ScrollXTo(int xOff) {
    if (ShouldCommitStableNavPointBeforeViewChange(this, GetScrollState())) {
        AddNavPoint();
    }

    int currPageNo = CurrentPageNo();
    viewPort.x = xOff;
    RecalcVisibleParts();
    EnsureMediaBoxesForVisiblePages();
    cb->UpdateScrollbars(canvasSize);

    if (CurrentPageNo() != currPageNo) {
        cb->PageNoChanged(this, CurrentPageNo());
    }
    RepaintDisplay();
    RememberStableNavPointCandidateAfterViewChange(this, GetScrollState());
}

void DisplayModel::ScrollXBy(int dx) {
    int newOffX = limitValue(viewPort.x + dx, 0, canvasSize.dx - viewPort.dx);
    if (newOffX != viewPort.x) {
        ScrollXTo(newOffX);
    }
}

void DisplayModel::ScrollYTo(int yOff) {
    if (ShouldCommitStableNavPointBeforeViewChange(this, GetScrollState())) {
        AddNavPoint();
    }

    int currPageNo = CurrentPageNo();
    viewPort.y = yOff;
    RecalcVisibleParts();
    EnsureMediaBoxesForVisiblePages();
    RenderVisibleParts();
    // Match ScrollXTo: keep scrollbar thumb (and smart overlay reveal) in sync.
    cb->UpdateScrollbars(canvasSize);

    int newPageNo = CurrentPageNo();
    if (newPageNo != currPageNo) {
        cb->PageNoChanged(this, newPageNo);
    }
    RepaintDisplay();
    RememberStableNavPointCandidateAfterViewChange(this, GetScrollState());
}

/* Scroll the doc in y-axis by 'dy'. If 'changePage' is TRUE, automatically
   switch to prev/next page in non-continuous mode if we scroll past the edges
   of current page */
void DisplayModel::ScrollYBy(int dy, bool changePage) {
    PageInfo* pageInfo;
    int currYOff = viewPort.y;
    int newPageNo;
    int currPageNo;

    ReportIf(0 == dy);
    if (0 == dy) {
        return;
    }

    if (ShouldCommitStableNavPointBeforeViewChange(this, GetScrollState())) {
        AddNavPoint();
    }

    int newYOff = currYOff;

    if (!IsContinuous(GetDisplayMode()) && changePage) {
        if ((dy < 0) && (0 == currYOff)) {
            if (startPage > 1) {
                newPageNo = startPage - 1;
                ReportIf(!ValidPageNo(newPageNo));
                pageInfo = GetPageInfo(newPageNo);
                newYOff = pageInfo->pos.dy - viewPort.dy;
                newYOff = std::max(newYOff, 0); /* TODO: center instead? */
                GoToPrevPage(newYOff);
                return;
            }
        }

        /* see if we have to change page when scrolling forward */
        if ((dy > 0) && (startPage < PageCount())) {
            if (viewPort.y + viewPort.dy >= canvasSize.dy) {
                GoToNextPage();
                return;
            }
        }
    }

    newYOff += dy;
    newYOff = limitValue(newYOff, 0, canvasSize.dy - viewPort.dy);
    if (newYOff == currYOff) {
        return;
    }

    currPageNo = CurrentPageNo();
    viewPort.y = newYOff;
    RecalcVisibleParts();
    EnsureMediaBoxesForVisiblePages();
    RenderVisibleParts();
    cb->UpdateScrollbars(canvasSize);
    newPageNo = CurrentPageNo();
    if (newPageNo != currPageNo) {
        cb->PageNoChanged(this, newPageNo);
    }
    RepaintDisplay();
    RememberStableNavPointCandidateAfterViewChange(this, GetScrollState());
}

int DisplayModel::yOffset() {
    return viewPort.y;
}

// Pages are laid out in pixels on a canvas whose size is an int, as are the
// page positions and the scroll offsets computed from it, so there is a zoom
// above which a given document no longer fits. It's far beyond kZoomMaxDefault
// for anything of a normal length, which is why this only matters once
// ZoomLevels raises the limit (issue #1195): rather than letting the canvas
// overflow, we stop zooming in at the largest zoom it can be laid out at.
float DisplayModel::MaxZoomForDocument() const {
    // room to spare for margins, page spacing and the arithmetic done on positions
    constexpr float kMaxCanvasSize = (float)(1 << 29);

    float totalDy = 0;
    float maxDx = 0;
    int nPages = PageCount();
    for (int pageNo = 1; pageNo <= nPages; pageNo++) {
        RectF box = PageMediaBoxForLayout(pageNo);
        float dx = box.dx;
        float dy = box.dy;
        if (rotation == 90 || rotation == 270) {
            std::swap(dx, dy);
        }
        totalDy += dy;
        maxDx = std::max(maxDx, dx);
    }
    // facing modes put two pages side by side
    maxDx *= 2;
    float largest = std::max(totalDy, maxDx);
    if (largest <= 0) {
        return kZoomMax;
    }
    float maxZoomReal = kMaxCanvasSize / largest;
    return maxZoomReal * 100 / dpiFactor;
}

void DisplayModel::SetZoomVirtual(float zoomLevel, Point* fixPt) {
    if (zoomLevel > 0) {
        zoomLevel = limitValue(zoomLevel, kZoomMin, kZoomMax);
        if (zoomLevel > kZoomMaxDefault) {
            zoomLevel = std::max(kZoomMin, std::min(zoomLevel, MaxZoomForDocument()));
        }
    }
    if (!IsValidZoom(zoomLevel)) {
        return;
    }

    bool scrollToFitPage = kZoomFitPage == zoomLevel || kZoomFitHeight == zoomLevel || kZoomFitContent == zoomLevel ||
                           kZoomShrinkToFit == zoomLevel;
    if (zoomVirtual == zoomLevel && (fixPt || !scrollToFitPage)) {
        return;
    }

    ScrollState ss = GetScrollState();

    int centerPage = -1;
    PointF centerPt;
    if (fixPt) {
        centerPage = GetPageNoByPoint(*fixPt);
        if (ValidPageNo(centerPage)) {
            centerPt = CvtFromScreen(*fixPt, centerPage);
        } else {
            fixPt = nullptr;
        }
    }

    if (scrollToFitPage) {
        ss.page = CurrentPageNo();
        // SetScrollState's first call to GoToPage will already scroll to fit
        ss.x = ss.y = -1;
    }

    // lf("DisplayModel::SetZoomVirtual() zoomLevel=%.6f", _zoomLevel);
    // the user asked for this zoom, so make Fit Content land exactly on the
    // content. Held across SetScrollState() too: in continuous mode GoToPage()
    // relayouts again for the page it scrolls to, and that is the page whose
    // content the zoom must fit
    exactFitContent = (kZoomFitContent == zoomLevel);
    Relayout(zoomLevel, rotation);
    SetScrollState(ss);
    exactFitContent = false;

    if (fixPt) {
        // scroll so that the fix point remains in the same screen location after zooming
        Point centerI = CvtToScreen(centerPage, centerPt);
        if (centerI.x - fixPt->x != 0) {
            ScrollXBy(centerI.x - fixPt->x);
        }
        if (centerI.y - fixPt->y != 0) {
            ScrollYBy(centerI.y - fixPt->y, false);
        }
    }

    cb->ZoomChanged(this, zoomLevel);
}

float DisplayModel::GetZoomVirtual(bool absolute) const {
    if (absolute) {
        // revert the dpiFactor premultiplication for converting zoomReal back to zoomVirtual
        return zoomReal * 100 / dpiFactor;
    }
    return zoomVirtual;
}

bool MaybeGetNextZoomByIncrement(float* currZoomInOut, float towardsLevel) {
    auto zoomIncrPerc = gGlobalPrefs->zoomIncrement;
    if (zoomIncrPerc <= 0) {
        return false;
    }
    float factor = (zoomIncrPerc / 100) + 1;
    float currZoom = *currZoomInOut;
    float newZoom = currZoom;
    if (currZoom < towardsLevel) {
        newZoom = std::min(currZoom * factor, towardsLevel);
    } else if (currZoom > towardsLevel) {
        newZoom = std::max(currZoom / factor, towardsLevel);
    }
    *currZoomInOut = newZoom;
    return true;
}

// differences to Adobe Reader: starts at 8.33 (instead of 1 and 6.25)
// and has four additional intermediary zoom levels ("added")
// clang-format off
static float defaultZoomLevels[] = {
    8.33f, 12.5f, 18 /* added */, 25, 33.33f, 50, 66.67f, 75,
    100, 125, 150, 200, 300, 400, 600, 800, 1000 /* added */,
    1200, 1600, 2000 /* added */, 2400, 3200, 4800 /* added */, 6400
};
// clang-format on

float* GetDefaultZoomLevels(int* nZoomLevelsOut) {
    float* zoomLevels = defaultZoomLevels;
    int nZoomLevels = dimofi(defaultZoomLevels);

    int nCustomZooms = len(*gGlobalPrefs->zoomLevels);
    if (nCustomZooms > 0) {
        // ReportIf(((*defaultZooms)[0] < kZoomMin || defaultZooms->Last() > kZoomMax));
        // ReportIf((*defaultZooms)[0] > defaultZooms->Last());
        zoomLevels = gGlobalPrefs->zoomLevels->LendData();
        nZoomLevels = nCustomZooms;
    }
    *nZoomLevelsOut = nZoomLevels;
    return zoomLevels;
}

float DisplayModel::GetNextZoomStep(float towardsLevel) const {
    float currZoom = GetZoomVirtual(true);
    if (currZoom == towardsLevel) {
        return towardsLevel;
    }

    if (MaybeGetNextZoomByIncrement(&currZoom, towardsLevel)) {
        return currZoom;
    }

    // ReportIf(defaultZooms[0] != kZoomMin || defaultZooms[dimof(defaultZooms)-1] != kZoomMax);

    int nZoomLevels;
    float* zoomLevels = GetDefaultZoomLevels(&nZoomLevels);

    float pageZoom = (float)HUGE_VAL, widthZoom = (float)HUGE_VAL;
    int nPages = PageCount();
    for (int pageNo = 1; pageNo <= nPages; pageNo++) {
        if (PageShown(pageNo)) {
            float pagePageZoom = ZoomRealFromVirtualForPage(kZoomFitPage, pageNo);
            pageZoom = std::min(pageZoom, pagePageZoom);
            float pageWidthZoom = ZoomRealFromVirtualForPage(kZoomFitWidth, pageNo);
            widthZoom = std::min(widthZoom, pageWidthZoom);
        }
    }
    ReportIf(pageZoom == (float)HUGE_VAL || widthZoom == (float)HUGE_VAL);
    ReportIf(pageZoom > widthZoom);
    pageZoom *= 100 / dpiFactor;
    widthZoom *= 100 / dpiFactor;

    const float FUZZ = 0.01f;
    float newZoom = towardsLevel;
    if (currZoom + FUZZ < towardsLevel) {
        for (int i = 0; i < nZoomLevels; i++) {
            float zoom = zoomLevels[i];
            if (zoom - FUZZ > currZoom) {
                newZoom = zoom;
                break;
            }
        }
        if (currZoom + FUZZ < pageZoom && pageZoom < newZoom - FUZZ) {
            newZoom = kZoomFitPage;
        } else if (currZoom + FUZZ < widthZoom && widthZoom < newZoom - FUZZ) {
            newZoom = kZoomFitWidth;
        }
    } else if (currZoom - FUZZ > towardsLevel) {
        for (int i = nZoomLevels - 1; i >= 0; i--) {
            float zoom = zoomLevels[i];
            if (zoom + FUZZ < currZoom) {
                newZoom = zoom;
                break;
            }
        }
        // skip Fit Width if it results in the same value as Fit Page (same as when zooming in)
        if (newZoom + FUZZ < widthZoom && widthZoom < currZoom - FUZZ && widthZoom != pageZoom) {
            newZoom = kZoomFitWidth;
        } else if (newZoom + FUZZ < pageZoom && pageZoom < currZoom - FUZZ) {
            newZoom = kZoomFitPage;
        }
    }

    // logf("currZoom: %.2f, towardsLevel: %.2f, newZoom: %.2f\n", currZoom, towardsLevel, newZoom);
    return newZoom;
}

/* a "virtual" zoom level. Can be either a real zoom level in percent
       (i.e. 100.0 is original size) or one of virtual values kZoomFitPage,
kZoomFitWidth or kZoomFitContent, whose real value depends on draw area size */
void DisplayModel::RotateBy(int newRotation) {
    newRotation = NormalizeRotation(newRotation);
    ReportIf(0 == newRotation);
    if (0 == newRotation) {
        return;
    }
    newRotation = NormalizeRotation(newRotation + rotation);

    int currPageNo = CurrentPageNo();
    Relayout(zoomVirtual, newRotation);
    GoToPage(currPageNo, 0);
}

// True when glyph i's bbox intersects region enough to count as selected.
static bool GlyphInRegion(const Rect* coords, int textLen, int i, Rect regionI) {
    if (i < 0 || i >= textLen || !coords) {
        return false;
    }
    Rect rect = coords[i];
    if (!rect.dx && !rect.dy) {
        return false; // empty box (newline / soft-join space): not a hit by itself
    }
    Rect isect = regionI.Intersect(rect);
    if (isect.IsEmpty()) {
        return false;
    }
    return 1.0 * isect.dx * isect.dy / (rect.dx * rect.dy) >= 0.3;
}

/* Given <region> (in user coordinates) on page <pageNo>, returns text in that region. */
Str DisplayModel::GetTextInRegion(int pageNo, RectF region) const {
    Rect* coords;
    int textLen = 0;
    Str pageText = engine->GetTextForPage(pageNo, &textLen, &coords);
    if (!pageText) {
        return {};
    }

    str::Builder result;
    Rect regionI = region.Round();
    int byteIdx = 0;
    for (int i = 0; i < textLen; i++) {
        int charStart = byteIdx;
        int c = Utf8CodepointNext(pageText, byteIdx);
        if (c == '\n') {
            if (result.LastChar() != '\n') {
                result.Append(StrL("\r\n"));
            }
            continue;
        }
        Rect rect = coords[i];
        // Soft-join spaces from FzTextPageToUtf8 (#5793) use empty rects (like hard
        // newlines). Include them when both neighboring content glyphs are selected
        // so Select-All / rectangular copy keeps spaces at wrap points.
        if (!rect.dx && !rect.dy) {
            if (c != ' ' && c != '\t') {
                continue;
            }
            int prev = i - 1;
            while (prev >= 0 && !coords[prev].dx && !coords[prev].dy) {
                prev--;
            }
            int next = i + 1;
            while (next < textLen && !coords[next].dx && !coords[next].dy) {
                next++;
            }
            if (GlyphInRegion(coords, textLen, prev, regionI) && GlyphInRegion(coords, textLen, next, regionI)) {
                result.Append(Str(pageText.s + charStart, byteIdx - charStart));
            }
            continue;
        }
        if (GlyphInRegion(coords, textLen, i, regionI)) {
            result.Append(Str(pageText.s + charStart, byteIdx - charStart));
        }
    }

    return result.TakeStr();
}

// returns true if it was necessary to scroll the display (horizontally or vertically)
bool DisplayModel::ShowResultRectToScreen(TextSel* res) {
    if (!res->len) {
        return false;
    }

    Rect extremes;
    for (int i = 0; i < res->len; i++) {
        Rect rc = CvtToScreen(res->pages[i], ToRectF(res->rects[i]));
        extremes = extremes.Union(rc);
    }
    return ScrollScreenToRect(res->pages[0], extremes);
}

bool DisplayModel::ScrollScreenToRect(int pageNo, Rect rec) {
    // don't scroll if the whole result is already visible
    if (Rect(Point(), viewPort.Size()).Intersect(rec) == rec) {
        return false;
    }

    PageInfo* pageInfo = GetPageInfo(pageNo);
    int sx = 0, sy = 0;

    // vertically, we try to position the search result between 40%
    // (scrolling up) and 60% (scrolling down) of the screen, so that
    // the search direction remains obvious and we still display some
    // context before and after the found text
    if (rec.y < viewPort.dy * 2 / 5) {
        sy = rec.y - (viewPort.dy * 2 / 5);
    } else if (rec.y + rec.dy > viewPort.dy * 3 / 5) {
        sy = std::min(rec.y + rec.dy - (viewPort.dy * 3 / 5), rec.y + (rec.dy / 2) - (viewPort.dy * 2 / 5));
    }

    // horizontally, we try to position the search result at the
    // center of the screen, but don't scroll further than page
    // boundaries, so that as much context as possible remains visible
    if (rec.x < 0) {
        sx = std::max(rec.x + (rec.dx / 2) - (viewPort.dx / 2), pageInfo->pageOnScreen.x);
    } else if (rec.x + rec.dx >= viewPort.dx) {
        sx = std::min(rec.x + (rec.dx / 2) - (viewPort.dx / 2),
                      pageInfo->pageOnScreen.x + pageInfo->pageOnScreen.dx - viewPort.dx);
    }

    if (sx != 0) {
        ScrollXBy(sx);
    }
    if (sy != 0) {
        ScrollYBy(sy, false);
    }

    return sx != 0 || sy != 0;
}

static bool gLogScrollState = false;

ScrollState DisplayModel::GetScrollState() {
    ScrollState state(FirstVisiblePageNo(), -1, -1);
    if (!ValidPageNo(state.page)) {
        state.page = CurrentPageNo();
        ReportIf(!ValidPageNo(state.page));
    }

    PageInfo* pageInfo = GetPageInfo(state.page);
    // Shortcut: don't calculate precise positions, if the
    // page wasn't scrolled right/down at all
    if (!pageInfo || pageInfo->pageOnScreen.x > 0 && pageInfo->pageOnScreen.y > 0) {
        ReportIf(!ValidPageNo(state.page));
        if (gLogScrollState) {
            logf("GetScrollState: page: %d, pos: %d,%d\n", state.page, (int)state.x, (int)state.y);
        }
        return state;
    }
    if (gLogScrollState) {
        logf("GetScrollState: page: %d, pageOnScreen: %d,%d\n", state.page, pageInfo->pageOnScreen.x,
             pageInfo->pageOnScreen.y);
    }

    Rect screen(Point(), viewPort.Size());
    Rect pageVis = pageInfo->pageOnScreen.Intersect(screen);
    state.page = GetPageNextToPoint(pageVis.TL());
    ReportIf(!ValidPageNo(state.page));
    PointF ptD = CvtFromScreen(pageVis.TL(), state.page);
    if (gLogScrollState) {
        logf("  page: %d, pageVis: %d,%d, ptD: %d,%d\n", state.page, pageVis.x, pageVis.y, (int)ptD.x, (int)ptD.y);
    }
    // Remember to show the margin, if it's currently visible
    if (pageInfo->pageOnScreen.x <= 0) {
        state.x = ptD.x;
    }
    if (pageInfo->pageOnScreen.y <= 0) {
        state.y = ptD.y;
    }
    if (gLogScrollState) {
        logf("  page: %d, state: %d,%d\n", state.page, (int)state.x, (int)state.y);
    }
    return state;
}

void DisplayModel::SetScrollState(const ScrollState& state) {
    if (gLogScrollState) {
        logf("SetScrollState: page: %d, pos: %d,%d\n", state.page, (int)state.x, (int)state.y);
    }
    // this restores a view (session restore, Back / Forward themselves):
    // don't let stable nav point tracking record it as user navigation
    stableNavPoint.suppress = true;
    // must have both GoToPage() calls
    GoToPage(state.page, false);
    // Bail out, if the page wasn't scrolled
    if (state.x < 0 && state.y < 0) {
        if (gLogScrollState) {
            logf("  exit because not scrolled\n");
        }
        stableNavPoint.suppress = false;
        return;
    }

    PointF newPtD((float)std::max(state.x, (double)0), (float)std::max(state.y, (double)0));
    Point newPt = CvtToScreen(state.page, newPtD);
    if (gLogScrollState) {
        logf("  newPtD: %d,%d\n", (int)newPtD.x, (int)newPtD.y);
        logf("  newPt:  %d,%d\n", newPt.x, newPt.y);
    }

    // Also show the margins, if this has been requested
    if (state.x < 0) {
        newPt.x = -1;
    } else {
        if (gLogScrollState) {
            logf("  x += viewPort.x (%d), state.x: %d\n", viewPort.x, (int)state.x);
        }
        newPt.x += viewPort.x;
    }
    if (state.y < 0) {
        newPt.y = 0;
    }
    if (gLogScrollState) {
        logf("  newPt:  %d,%d\n", newPt.x, newPt.y);
    }
    GoToPage(state.page, newPt.y, false, newPt.x);
    stableNavPoint.suppress = false;
}

// don't remember more than "enough" history entries (same number as Firefox uses)
#define MAX_NAV_HISTORY_LEN 50

/* Records the current scroll state for later navigating back to.
   With rememberZoom the entry also carries the current zoom, so navigating back
   to it undoes a zoom change as well as the scrolling (Zoom To Selection). */
void DisplayModel::AddNavPoint(bool rememberZoom) {
    ScrollState ss = GetScrollState();
    if (rememberZoom) {
        ss.zoom = zoomVirtual;
    }
    // remove the current and all Forward history entries
    if (navHistoryIdx < len(navHistory)) {
        navHistory.RemoveAt(navHistoryIdx, len(navHistory) - navHistoryIdx);
    }
    // don't add another entry for the exact same position
    if (navHistoryIdx > 0 && ss == navHistory[navHistoryIdx - 1]) {
        return;
    }
    // make sure that the history doesn't grow overly large
    if (navHistoryIdx >= MAX_NAV_HISTORY_LEN) {
        ReportIf(navHistoryIdx > MAX_NAV_HISTORY_LEN);
        navHistory.RemoveAt(0, navHistoryIdx - MAX_NAV_HISTORY_LEN + 1);
        navHistoryIdx = MAX_NAV_HISTORY_LEN - 1;
    }
    // add a new Back history entry
    navHistory.Append(ss);
    navHistoryIdx++;
}

bool DisplayModel::CanNavigate(int dir) const {
    ReportIf(navHistoryIdx > len(navHistory));
    if (dir < 0) {
        return navHistoryIdx >= -dir;
    }
    return navHistoryIdx + dir < len(navHistory);
}

/* Navigates |dir| steps forward or backwards. */
void DisplayModel::Navigate(int dir) {
    if (!CanNavigate(dir)) {
        return;
    }
    // update the current history entry, keeping the zoom it was recorded with
    ScrollState ss = GetScrollState();
    if (navHistoryIdx < len(navHistory)) {
        ss.zoom = navHistory[navHistoryIdx].zoom;
        navHistory[navHistoryIdx] = ss;
    } else {
        navHistory.Append(ss);
    }
    navHistoryIdx += dir;
    ScrollState target = navHistory[navHistoryIdx];
    // zoom first: the position in the entry is relative to a laid out document
    if (target.zoom != 0 && target.zoom != zoomVirtual && IsValidZoom(target.zoom)) {
        SetZoomVirtual(target.zoom, nullptr);
    }
    SetScrollState(target);
}

void DisplayModel::CopyNavHistory(DisplayModel& orig) {
    navHistory = orig.navHistory;
    navHistoryIdx = orig.navHistoryIdx;
    // remove navigation history entries for all no longer valid pages
    for (int i = len(navHistory); i > 0; i--) {
        if (!ValidPageNo(navHistory[i - 1].page)) {
            navHistory.RemoveAt(i - 1);
            if (i - 1 < navHistoryIdx) {
                navHistoryIdx--;
            }
        }
    }
}

bool DisplayModel::ShouldCacheRendering(int /*pageNo*/) const {
    // recommend caching for all documents
    return true;
}

#if 0
void DisplayModel::ScrollToLink(IPageDestination* dest) {
    ReportIf(!dest || dest->GetPageNo() <= 0);
    if (!dest) {
        return;
    }
    int pageNo = dest->GetPageNo();
    RectF rect = dest->GetRect();
    float zoom = dest->GetZoom();
    ScrollTo(pageNo, rect, zoom);
}
#endif

void DisplayModel::ScrollTo(int pageNo, RectF rect, float zoom) {
    Point scroll(-1, 0);

    // zoom: absolute fraction (1.0 = 100%) for /XYZ; virtual Fit* modes
    // (kZoomFitPage / FitWidth / FitContent); 0 = leave zoom (issue #5828).
    // FitContent uses CurrentPageNo() for the content box, so switch page first
    // for virtual modes, then apply zoom, then fine-tune scroll.
    if (gGlobalPrefs->ignoreDestinationZoom) {
        // the reader stays at the zoom the user picked; the destination still
        // decides the page and the position on it (discussion #5938)
        zoom = 0;
    }
    bool isVirtualZoom = zoom == kZoomFitPage || zoom == kZoomFitWidth || zoom == kZoomFitHeight ||
                         zoom == kZoomFitContent || zoom == kZoomShrinkToFit || zoom == kZoomFitByOrientation;
    bool isAbsZoom = zoom > 0;

    if (isVirtualZoom) {
        GoToPage(pageNo, 0, true, -1);
        SetZoomVirtual(zoom, nullptr);
    } else if (isAbsZoom) {
        SetZoomVirtual(100 * zoom, nullptr);
        CalcZoomReal(zoomVirtual);
    }

    // use per-page zoom which may differ from global zoomReal
    // when pages have varying sizes in fit-width/fit-page mode
    float pageZoom = GetZoomReal(pageNo);

    if (rect.IsEmpty() || (rect.dx == kDestUseDefault && rect.dy == kDestUseDefault)) {
        // PDF: /XYZ, /Fit, /FitB — scroll to rect.TL() (defaults = page top)
        PointF scrollD = engine->Transform(rect.TL(), pageNo, pageZoom, rotation);
        scroll = ToPoint(scrollD);

        // Unspecified X: keep horizontal scroll.
        if (kDestUseDefault == rect.x) {
            scroll.x = -1;
        }
        // Unspecified Y (page-level /Fit, /XYZ with null top): land at the top
        // of the *target* page. Reusing the current page's on-screen offset
        // often scrolls continuous view so the next page is most visible
        // ("bookmark lands one page ahead", #2799 / #3310).
        if (kDestUseDefault == rect.y) {
            if (pageNo == CurrentPageNo()) {
                PageInfo* pageInfo = GetPageInfo(pageNo);
                scroll.y = -(pageInfo->pageOnScreen.y - windowMargin.top);
            } else {
                scroll.y = 0;
            }
        }
    } else if (rect.dx != kDestUseDefault && rect.dy != kDestUseDefault) {
        // PDF: /FitR left bottom right top
        RectF rectD = engine->Transform(rect, pageNo, pageZoom, rotation);
        scroll = ToPoint(rectD.TL());
    } else if (rect.y != kDestUseDefault) {
        // PDF: /FitH top  or  /FitBH top
        PointF scrollD = engine->Transform(rect.TL(), pageNo, pageZoom, rotation);
        scroll.y = (int)scrollD.y;
    }
    // TODO: prevent scroll.y from getting too large?
    scroll.y = std::max(scroll.y, 0); // Adobe Reader never shows the previous page
    if (isVirtualZoom) {
        // already on pageNo; only adjust scroll after fit zoom
        if (scroll.y > 0) {
            ScrollYTo(scroll.y);
        }
    } else {
        GoToPage(pageNo, scroll.y, true, -1);
    }
    if (rect.x != kDestUseDefault) {
        float docY = (rect.y != kDestUseDefault) ? rect.y : 0.f;
        Rect destScreen = CvtToScreen(pageNo, RectF(rect.x, docY, 1, 1));
        ScrollScreenToRect(pageNo, destScreen);
    } else if (rect.dx != kDestUseDefault && rect.dy != kDestUseDefault) {
        Rect destScreen = CvtToScreen(pageNo, rect);
        ScrollScreenToRect(pageNo, destScreen);
    }
}
