/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
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

#include "utils/BaseUtil.h"
#include "utils/WinUtil.h"
#include "utils/ScopedWin.h"
#include "utils/Log.h"

#include "wingui/TreeModel.h"
#include "EngineBase.h"
#include "EngineManager.h"

#include "SettingsStructs.h"
#include "Controller.h"
#include "DisplayModel.h"
#include "GlobalPrefs.h"
#include "PdfSync.h"
#include "ProgressUpdateUI.h"
#include "TextSelection.h"
#include "TextSearch.h"

// if true, we pre-render the pages right before and after the visible pages
static bool gPredictiveRender = true;

static int ColumnsFromDisplayMode(DisplayMode displayMode) {
    if (!IsSingle(displayMode))
        return 2;
    return 1;
}

int NormalizeRotation(int rotation) {
    CrashIf((rotation % 90) != 0);
    rotation = rotation % 360;
    if (rotation < 0)
        rotation += 360;
    if (rotation < 0 || rotation >= 360 || (rotation % 90) != 0) {
        CrashIf(true);
        return 0;
    }
    return rotation;
}

void DisplayModel::GetDisplayState(DisplayState* ds) {
    if (!ds->filePath || !str::EqI(ds->filePath, engine->FileName()))
        str::ReplacePtr(&ds->filePath, engine->FileName());

    ds->useDefaultState = !gGlobalPrefs->rememberStatePerDocument;

    str::ReplacePtr(&ds->displayMode,
                    prefs::conv::FromDisplayMode(presentationMode ? presDisplayMode : GetDisplayMode()));
    prefs::conv::FromZoom(&ds->zoom, presentationMode ? presZoomVirtual : zoomVirtual, ds);

    ScrollState ss = GetScrollState();
    ds->pageNo = ss.page;
    ds->scrollPos = PointI();
    if (!presentationMode) {
        ds->scrollPos = PointD(ss.x, ss.y).ToInt();
    }
    ds->rotation = rotation;
    ds->displayR2L = displayR2L;

    free(ds->decryptionKey);
    ds->decryptionKey = engine->GetDecryptionKey();
}

SizeD DisplayModel::PageSizeAfterRotation(int pageNo, bool fitToContent) const {
    PageInfo* pageInfo = GetPageInfo(pageNo);
    if (fitToContent && pageInfo->contentBox.IsEmpty()) {
        pageInfo->contentBox = engine->PageContentBox(pageNo);
        if (pageInfo->contentBox.IsEmpty())
            return PageSizeAfterRotation(pageNo);
    }

    RectD box = fitToContent ? pageInfo->contentBox : pageInfo->page;
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

// must call SetInitialViewSettings() after creation
DisplayModel::DisplayModel(EngineBase* eng, ControllerCallback* cb) : Controller(cb), engine(engine) {
    engine = eng;
    CrashIf(!engine || engine->PageCount() <= 0);
    engineType = engine->kind;

    if (!engine->IsImageCollection()) {
        windowMargin = gGlobalPrefs->fixedPageUI.windowMargin;
        pageSpacing = gGlobalPrefs->fixedPageUI.pageSpacing;
    } else {
        windowMargin = gGlobalPrefs->comicBookUI.windowMargin;
        pageSpacing = gGlobalPrefs->comicBookUI.pageSpacing;
    }
#ifdef DRAW_PAGE_SHADOWS
    windowMargin.top += 3;
    windowMargin.bottom += 5;
    windowMargin.right += 3;
    windowMargin.left += 1;
    pageSpacing.dx += 4;
    pageSpacing.dy += 4;
#endif

    textCache = new PageTextCache(engine);
    textSelection = new TextSelection(engine, textCache);
    textSearch = new TextSearch(engine, textCache);
}

DisplayModel::~DisplayModel() {
    dontRenderFlag = true;
    cb->CleanUp(this);

    delete pdfSync;
    delete userAnnots;
    delete textSearch;
    delete textSelection;
    delete textCache;
    delete engine;
    free(pagesInfo);
}

PageInfo* DisplayModel::GetPageInfo(int pageNo) const {
    if (!ValidPageNo(pageNo)) {
        return nullptr;
    }
    CrashIf(!pagesInfo);
    if (!pagesInfo) {
        return nullptr;
    }
    return &(pagesInfo[pageNo - 1]);
}

// Call this before the first Relayout
void DisplayModel::SetInitialViewSettings(DisplayMode newDisplayMode, int newStartPage, SizeI viewPort, int screenDPI) {
    totalViewPortSize = viewPort;
    dpiFactor = 1.0f * screenDPI / engine->GetFileDPI();
    if (ValidPageNo(newStartPage)) {
        startPage = newStartPage;
    }

    displayMode = newDisplayMode;
    presDisplayMode = newDisplayMode;
    PageLayoutType layout = engine->preferredLayout;
    if (DM_AUTOMATIC == displayMode) {
        switch (layout & ~Layout_R2L) {
            case Layout_Single:
                displayMode = DM_CONTINUOUS;
                break;
            case Layout_Facing:
                displayMode = DM_CONTINUOUS_FACING;
                break;
            case Layout_Book:
                displayMode = DM_CONTINUOUS_BOOK_VIEW;
                break;
            case Layout_Single | Layout_NonContinuous:
                displayMode = DM_SINGLE_PAGE;
                break;
            case Layout_Facing | Layout_NonContinuous:
                displayMode = DM_FACING;
                break;
            case Layout_Book | Layout_NonContinuous:
                displayMode = DM_BOOK_VIEW;
                break;
        }
    }
    displayR2L = (layout & Layout_R2L) != 0;
    BuildPagesInfo();
}

void DisplayModel::BuildPagesInfo() {
    CrashIf(pagesInfo);
    int pageCount = PageCount();
    pagesInfo = AllocArray<PageInfo>(pageCount);

    RectD defaultRect;
    float fileDPI = engine->GetFileDPI();
    if (0 == GetMeasurementSystem()) {
        defaultRect = RectD(0, 0, 21.0 / 2.54 * fileDPI, 29.7 / 2.54 * fileDPI);
    } else {
        defaultRect = RectD(0, 0, 8.5 * fileDPI, 11 * fileDPI);
    }

    int columns = ColumnsFromDisplayMode(displayMode);
    int newStartPage = startPage;
    if (IsBookView(displayMode) && newStartPage == 1 && columns > 1) {
        newStartPage--;
    }

    for (int pageNo = 1; pageNo <= pageCount; pageNo++) {
        PageInfo* pageInfo = GetPageInfo(pageNo);
        pageInfo->page = engine->PageMediabox(pageNo);
        // layout pages with an empty mediabox as A4 size (resp. letter size)
        if (pageInfo->page.IsEmpty()) {
            pageInfo->page = defaultRect;
        }
        pageInfo->visibleRatio = 0.0;
        pageInfo->shown = false;
        if (IsContinuous(displayMode)) {
            pageInfo->shown = true;
        } else if (newStartPage <= pageNo && pageNo < newStartPage + columns) {
            pageInfo->shown = true;
        }
    }
}

// TODO: a better name e.g. ShouldShow() to better distinguish between
// before-layout info and after-layout visibility checks
bool DisplayModel::PageShown(int pageNo) const {
    PageInfo* pageInfo = GetPageInfo(pageNo);
    if (!pageInfo) {
        return false;
    }
    return pageInfo->shown;
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
    for (int i = pageNo - columns; i < pageNo + 2 * columns; i++) {
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
    if (IsSingle(mode))
        return false;
    if (CurrentPageNo() == count)
        return true;
    if (GetPageInfo(count)->visibleRatio < 1.0)
        return false;
    if (FirstPageInARowNo(count, ColumnsFromDisplayMode(mode), IsBookView(mode)) < count)
        return false;
    return true;
}

/* Given a zoom level that can include a "virtual" zoom levels like ZOOM_FIT_WIDTH,
   ZOOM_FIT_PAGE or ZOOM_FIT_CONTENT, calculate an absolute zoom level */
float DisplayModel::ZoomRealFromVirtualForPage(float zoomVirtual, int pageNo) const {
    if (zoomVirtual != ZOOM_FIT_WIDTH && zoomVirtual != ZOOM_FIT_PAGE && zoomVirtual != ZOOM_FIT_CONTENT)
        return zoomVirtual * 0.01f * dpiFactor;

    SizeD row;
    int columns = ColumnsFromDisplayMode(GetDisplayMode());

    bool fitToContent = (ZOOM_FIT_CONTENT == zoomVirtual);
    if (fitToContent && columns > 1) {
        // Fit the content of all the pages in the same row into the visible area
        // (i.e. don't crop inner margins but just the left-most, right-most, etc.)
        int first = FirstPageInARowNo(pageNo, columns, IsBookView(GetDisplayMode()));
        int last = LastPageInARowNo(pageNo, columns, IsBookView(GetDisplayMode()), PageCount());
        RectD box;
        for (int i = first; i <= last; i++) {
            PageInfo* pageInfo = GetPageInfo(i);
            if (pageInfo->contentBox.IsEmpty())
                pageInfo->contentBox = engine->PageContentBox(i);

            RectD pageBox = engine->Transform(pageInfo->page, i, 1.0, rotation);
            RectD contentBox = engine->Transform(pageInfo->contentBox, i, 1.0, rotation);
            if (contentBox.IsEmpty())
                contentBox = pageBox;

            contentBox.x += row.dx;
            box = box.Union(contentBox);
            row.dx += pageBox.dx + pageSpacing.dx;
        }
        row = box.Size();
    } else {
        row = PageSizeAfterRotation(pageNo, fitToContent);
        row.dx *= columns;
        row.dx += pageSpacing.dx * (columns - 1);
    }

    AssertCrash(!RectD(PointD(), row).IsEmpty());
    if (RectD(PointD(), row).IsEmpty())
        return 0;

    int areaForPagesDx = viewPort.dx - windowMargin.left - windowMargin.right;
    int areaForPagesDy = viewPort.dy - windowMargin.top - windowMargin.bottom;
    if (areaForPagesDx <= 0 || areaForPagesDy <= 0)
        return 0;

    float zoomX = areaForPagesDx / (float)row.dx;
    float zoomY = areaForPagesDy / (float)row.dy;
    if (zoomX < zoomY || ZOOM_FIT_WIDTH == zoomVirtual)
        return zoomX;
    return zoomY;
}

int DisplayModel::FirstVisiblePageNo() const {
    AssertCrash(pagesInfo);
    if (!pagesInfo)
        return INVALID_PAGE_NO;

    for (int pageNo = 1; pageNo <= PageCount(); ++pageNo) {
        PageInfo* pageInfo = GetPageInfo(pageNo);
        if (pageInfo->visibleRatio > 0.0)
            return pageNo;
    }

    /* If no pages are visible */
    return INVALID_PAGE_NO;
}

// we consider the most visible page the current one
// (in continuous layout, there's no better criteria)
int DisplayModel::CurrentPageNo() const {
    if (!IsContinuous(GetDisplayMode()))
        return startPage;

    AssertCrash(pagesInfo);
    if (!pagesInfo) {
        return INVALID_PAGE_NO;
    }
    // determine the most visible page
    int mostVisiblePage = INVALID_PAGE_NO;
    float ratio = 0;

    for (int pageNo = 1; pageNo <= PageCount(); pageNo++) {
        PageInfo* pageInfo = GetPageInfo(pageNo);
        if (pageInfo->visibleRatio > ratio) {
            mostVisiblePage = pageNo;
            ratio = pageInfo->visibleRatio;
        }
    }

    /* if no page is visible, default to either the first or the last one */
    if (INVALID_PAGE_NO == mostVisiblePage) {
        PageInfo* pageInfo = GetPageInfo(1);
        if (pageInfo && viewPort.y > pageInfo->pos.y + pageInfo->pos.dy) {
            mostVisiblePage = PageCount();
        } else {
            mostVisiblePage = 1;
        }
    }

    return mostVisiblePage;
}

void DisplayModel::CalcZoomReal(float newZoomVirtual) {
    CrashIf(!IsValidZoom(newZoomVirtual));
    zoomVirtual = newZoomVirtual;
    if ((ZOOM_FIT_WIDTH == newZoomVirtual) || (ZOOM_FIT_PAGE == newZoomVirtual)) {
        /* we want the same zoom for all pages, so use the smallest zoom
           across the pages so that the largest page fits. In most documents
           all pages are the same size anyway */
        float minZoom = (float)HUGE_VAL;
        for (int pageNo = 1; pageNo <= PageCount(); pageNo++) {
            if (PageShown(pageNo)) {
                float zoom = ZoomRealFromVirtualForPage(newZoomVirtual, pageNo);
                PageInfo* pageInfo = GetPageInfo(pageNo);
                pageInfo->zoomReal = zoom;
                if (minZoom > zoom) {
                    minZoom = zoom;
                }
            }
        }
        CrashIf(minZoom == (float)HUGE_VAL);
        zoomReal = minZoom;
    } else if (ZOOM_FIT_CONTENT == newZoomVirtual) {
        float newZoom = ZoomRealFromVirtualForPage(newZoomVirtual, CurrentPageNo());
        // limit zooming in to 800% on almost empty pages
        if (newZoom > 8.0) {
            newZoom = 8.0;
        }
        // don't zoom in by just a few pixels (throwing away a prerendered page)
        if (newZoom < zoomReal || zoomReal / newZoom < 0.95 ||
            zoomReal < ZoomRealFromVirtualForPage(ZOOM_FIT_PAGE, CurrentPageNo())) {
            zoomReal = newZoom;
        }
        for (int pageNo = 1; pageNo <= PageCount(); pageNo++) {
            PageInfo* pageInfo = GetPageInfo(pageNo);
            pageInfo->zoomReal = zoomReal;
        }
    } else {
        zoomReal = zoomVirtual * 0.01f * dpiFactor;
        for (int pageNo = 1; pageNo <= PageCount(); pageNo++) {
            PageInfo* pageInfo = GetPageInfo(pageNo);
            pageInfo->zoomReal = zoomReal;
        }
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
    pageNo = FirstPageInARowNo(pageNo, ColumnsFromDisplayMode(mode), IsBookView(mode));
    if (pageNo == PageCount() || pageNo == 1 && IsBookView(mode)) {
        return ZoomRealFromVirtualForPage(zoomVirtual, pageNo);
    }
    float zoomCurr = ZoomRealFromVirtualForPage(zoomVirtual, pageNo);
    float zoomNext = ZoomRealFromVirtualForPage(zoomVirtual, pageNo + 1);
    return std::min(zoomCurr, zoomNext);
}

/* Given zoom and rotation, calculate the position of each page on a
   large sheet that is continous view. Needs to be recalculated when:
     * zoom changes
     * rotation changes
     * switching between display modes
     * navigating to another page in non-continuous mode */
void DisplayModel::Relayout(float newZoomVirtual, int newRotation) {
    CrashIf(!pagesInfo);
    if (!pagesInfo) {
        return;
    }

    rotation = NormalizeRotation(newRotation);

    bool needHScroll = false;
    bool needVScroll = false;
    viewPort = RectI(viewPort.TL(), totalViewPortSize);

RestartLayout:
    int currPosY = windowMargin.top;
    float currZoomReal = zoomReal;
    CalcZoomReal(newZoomVirtual);

    int newViewPortOffsetX = 0;
    if (0 != currZoomReal && INVALID_ZOOM != currZoomReal) {
        newViewPortOffsetX = (int)(viewPort.x * zoomReal / currZoomReal);
    }
    viewPort.x = newViewPortOffsetX;
    /* calculate the position of each page on the canvas, given current zoom,
       rotation, columns parameters. You can think of it as a simple
       table layout i.e. rows with a fixed number of columns. */
    int columns = ColumnsFromDisplayMode(GetDisplayMode());
    int columnMaxWidth[2] = {0, 0};
    int pageInARow = 0;
    int rowMaxPageDy = 0;
    for (int pageNo = 1; pageNo <= PageCount(); ++pageNo) {
        PageInfo* pageInfo = GetPageInfo(pageNo);
        if (!pageInfo->shown) {
            CrashIf(0.0 != pageInfo->visibleRatio);
            continue;
        }
        SizeD pageSize = PageSizeAfterRotation(pageNo);
        RectI pos;
        // don't add the full 0.5 for rounding to account for precision errors
        float zoom = GetZoomReal(pageNo);
        pos.dx = (int)(pageSize.dx * zoom + 0.499);
        pos.dy = (int)(pageSize.dy * zoom + 0.499);

        if (rowMaxPageDy < pos.dy) {
            rowMaxPageDy = pos.dy;
        }
        pos.y = currPosY;

        // restart the layout if we detect we need to show scrollbars
        if (!needVScroll && viewPort.dy < currPosY + rowMaxPageDy) {
            needVScroll = true;
            viewPort.dx -= GetSystemMetrics(SM_CXVSCROLL);
            goto RestartLayout;
        }

        if (IsBookView(GetDisplayMode()) && pageNo == 1 && columns - pageInARow > 1) {
            pageInARow++;
        }
        CrashIf(pageInARow >= dimof(columnMaxWidth));
        if (columnMaxWidth[pageInARow] < pos.dx) {
            columnMaxWidth[pageInARow] = pos.dx;
        }

        if (!needHScroll && viewPort.dx < windowMargin.left + columnMaxWidth[0] +
                                              (columns == 2 ? pageSpacing.dx + columnMaxWidth[1] : 0) +
                                              windowMargin.right) {
            needHScroll = true;
            viewPort.dy -= GetSystemMetrics(SM_CYHSCROLL);
            goto RestartLayout;
        }

        pageInfo->pos = pos;

        pageInARow++;
        AssertCrash(pageInARow <= columns);
        if (pageInARow == columns) {
            /* starting next row */
            currPosY += rowMaxPageDy + pageSpacing.dy;
            rowMaxPageDy = 0;
            pageInARow = 0;
        }
    }

    if (pageInARow != 0) {
        /* this is a partial row */
        currPosY += rowMaxPageDy + pageSpacing.dy;
    }
    // restart the layout if we detect we need to show scrollbars
    // (there are some edge cases we can't catch in the above loop)
    const int canvasDy = currPosY + windowMargin.bottom - pageSpacing.dy;
    if (!needVScroll && canvasDy > viewPort.dy) {
        needVScroll = true;
        viewPort.dx -= GetSystemMetrics(SM_CXVSCROLL);
        goto RestartLayout;
    }

    if (columns == 2 && PageCount() == 1) {
        /* don't center a single page over two columns */
        if (IsBookView(GetDisplayMode())) {
            columnMaxWidth[0] = columnMaxWidth[1];
        } else {
            columnMaxWidth[1] = columnMaxWidth[0];
        }
    }

    // restart the layout if we detect we need to show scrollbars
    // (there are some edge cases we can't catch in the above loop)
    int canvasDx = windowMargin.left + columnMaxWidth[0] + (columns == 2 ? pageSpacing.dx + columnMaxWidth[1] : 0) +
                   windowMargin.right;
    if (!needHScroll && canvasDx > viewPort.dx) {
        needHScroll = true;
        viewPort.dy -= GetSystemMetrics(SM_CYHSCROLL);
        goto RestartLayout;
    }

    /* since pages can be smaller than the drawing area, center them in x axis */
    int offX = 0;
    if (canvasDx < viewPort.dx) {
        viewPort.x = 0;
        offX = (viewPort.dx - canvasDx) / 2;
        canvasDx = viewPort.dx;
    }

    CrashIf(offX < 0);
    pageInARow = 0;
    int pageOffX = offX + windowMargin.left;
    for (int pageNo = 1; pageNo <= PageCount(); ++pageNo) {
        PageInfo* pageInfo = GetPageInfo(pageNo);
        if (!pageInfo->shown) {
            CrashIf(0.0 != pageInfo->visibleRatio);
            continue;
        }
        // leave first spot empty in cover page mode
        if (IsBookView(GetDisplayMode()) && pageNo == 1) {
            CrashIf(pageInARow >= dimof(columnMaxWidth));
            pageOffX += columnMaxWidth[pageInARow] + pageSpacing.dx;
            ++pageInARow;
        }
        CrashIf(pageInARow >= dimof(columnMaxWidth));
        // center pages in a single column but right/left align them when using two columns
        if (1 == columns)
            pageInfo->pos.x = pageOffX + (columnMaxWidth[0] - pageInfo->pos.dx) / 2;
        else if (0 == pageInARow)
            pageInfo->pos.x = pageOffX + columnMaxWidth[0] - pageInfo->pos.dx;
        else
            pageInfo->pos.x = pageOffX;
        // center the cover page over the first two spots in non-continuous mode
        if (IsBookView(GetDisplayMode()) && pageNo == 1 && !IsContinuous(GetDisplayMode())) {
            pageInfo->pos.x = offX + windowMargin.left +
                              (columnMaxWidth[0] + pageSpacing.dx + columnMaxWidth[1] - pageInfo->pos.dx) / 2;
        }
        // mirror the page layout when displaying a Right-to-Left document
        if (displayR2L && columns > 1) {
            pageInfo->pos.x = canvasDx - pageInfo->pos.x - pageInfo->pos.dx;
        }

        CrashIf(pageInARow >= dimof(columnMaxWidth));
        pageOffX += columnMaxWidth[pageInARow] + pageSpacing.dx;
        ++pageInARow;
        AssertCrash(pageOffX >= 0 && pageInfo->pos.x >= 0);

        if (pageInARow == columns) {
            pageOffX = offX + windowMargin.left;
            pageInARow = 0;
        }
    }

    /* if after resizing we would have blank space on the right due to x offset
       being too much, make x offset smaller so that there's no blank space */
    if (viewPort.dx - (canvasDx - newViewPortOffsetX) > 0) {
        viewPort.x = canvasDx - viewPort.dx;
    }

    /* if a page is smaller than drawing area in y axis, y-center the page */
    if (canvasDy < viewPort.dy) {
        int offY = windowMargin.top + (viewPort.dy - canvasDy) / 2;
        CrashIf(offY < 0.0);
        for (int pageNo = 1; pageNo <= PageCount(); ++pageNo) {
            PageInfo* pageInfo = GetPageInfo(pageNo);
            if (!pageInfo->shown) {
                CrashIf(0.0 != pageInfo->visibleRatio);
                continue;
            }
            pageInfo->pos.y += offY;
        }
    }

    canvasSize = SizeI(std::max(canvasDx, viewPort.dx), std::max(canvasDy, viewPort.dy));
}

void DisplayModel::ChangeStartPage(int newStartPage) {
    AssertCrash(ValidPageNo(newStartPage));
    AssertCrash(!IsContinuous(GetDisplayMode()));

    int columns = ColumnsFromDisplayMode(GetDisplayMode());
    startPage = newStartPage;
    if (IsBookView(GetDisplayMode()) && newStartPage == 1 && columns > 1) {
        newStartPage--;
    }
    for (int pageNo = 1; pageNo <= PageCount(); pageNo++) {
        PageInfo* pageInfo = GetPageInfo(pageNo);
        if (IsContinuous(GetDisplayMode())) {
            pageInfo->shown = true;
        } else if (pageNo >= newStartPage && pageNo < newStartPage + columns) {
            // lf("DisplayModel::changeStartPage() set page %d as shown", pageNo);
            pageInfo->shown = true;
        } else {
            pageInfo->shown = false;
        }
        pageInfo->visibleRatio = 0.0;
    }
    Relayout(zoomVirtual, rotation);
}

/* Given positions of each page in a large sheet that is continous view and
   coordinates of a current view into that large sheet, calculate which
   parts of each page is visible on the screen.
   Needs to be recalucated after scrolling the view. */
void DisplayModel::RecalcVisibleParts() {
    CrashIf(!pagesInfo);
    if (!pagesInfo) {
        return;
    }

    for (int pageNo = 1; pageNo <= PageCount(); ++pageNo) {
        PageInfo* pageInfo = GetPageInfo(pageNo);
        if (!pageInfo->shown) {
            CrashIf(0.0 != pageInfo->visibleRatio);
            continue;
        }

        RectI pageRect = pageInfo->pos;
        RectI visiblePart = pageRect.Intersect(viewPort);

        pageInfo->visibleRatio = 0.0;
        if (!visiblePart.IsEmpty()) {
            CrashIf(pageRect.dx <= 0 || pageRect.dy <= 0);
            // calculate with floating point precision to prevent an integer overflow
            pageInfo->visibleRatio = 1.0f * visiblePart.dx * visiblePart.dy / ((float)pageRect.dx * pageRect.dy);
        }
        pageInfo->pageOnScreen = pageRect;
        pageInfo->pageOnScreen.Offset(-viewPort.x, -viewPort.y);
    }
}

int DisplayModel::GetPageNoByPoint(PointI pt) {
    // no reasonable answer possible, if zoom hasn't been set yet
    if (zoomReal <= 0) {
        return -1;
    }

    for (int pageNo = 1; pageNo <= PageCount(); ++pageNo) {
        PageInfo* pageInfo = GetPageInfo(pageNo);
        AssertCrash(0.0 == pageInfo->visibleRatio || pageInfo->shown);
        if (!pageInfo->shown) {
            continue;
        }

        if (pageInfo->pageOnScreen.Contains(pt)) {
            return pageNo;
        }
    }

    return -1;
}

int DisplayModel::GetPageNextToPoint(PointI pt) {
    if (zoomReal <= 0) {
        return startPage;
    }

    unsigned int maxDist = UINT_MAX;
    int closest = startPage;

    for (int pageNo = 1; pageNo <= PageCount(); ++pageNo) {
        PageInfo* pageInfo = GetPageInfo(pageNo);
        CrashIf(0.0 != pageInfo->visibleRatio && !pageInfo->shown);
        if (!pageInfo->shown) {
            continue;
        }

        if (pageInfo->pageOnScreen.Contains(pt)) {
            return pageNo;
        }

        RectI r = pageInfo->pageOnScreen;
        unsigned int dist = distSq(pt.x - r.x - r.dx / 2, pt.y - r.y - r.dy / 2);
        if (dist < maxDist) {
            closest = pageNo;
            maxDist = dist;
        }
    }

    return closest;
}

PointI DisplayModel::CvtToScreen(int pageNo, PointD pt) {
    PageInfo* pageInfo = GetPageInfo(pageNo);
    SubmitCrashIf(!pageInfo);
    if (!pageInfo) {
        return PointI();
    }

    float zoom = pageInfo->zoomReal;
    // TODO: must be a better way
    if (zoom == 0) {
        zoom = zoomReal;
    }
    PointD p = engine->Transform(pt, pageNo, zoom, rotation);
    // don't add the full 0.5 for rounding to account for precision errors
    RectI r = pageInfo->pageOnScreen;
    p.x += 0.499 + r.x;
    p.y += 0.499 + r.y;

    return p.ToInt();
}

RectI DisplayModel::CvtToScreen(int pageNo, RectD r) {
    PointI TL = CvtToScreen(pageNo, r.TL());
    PointI BR = CvtToScreen(pageNo, r.BR());
    return RectI::FromXY(TL, BR);
}

PointD DisplayModel::CvtFromScreen(PointI pt, int pageNo) {
    if (!ValidPageNo(pageNo)) {
        pageNo = GetPageNextToPoint(pt);
    }

    const PageInfo* pageInfo = GetPageInfo(pageNo);
    CrashIf(!pageInfo);
    if (!pageInfo) {
        return PointD();
    }

    // don't add the full 0.5 for rounding to account for precision errors
    RectI r = pageInfo->pageOnScreen;
    PointD p = PointD(pt.x - 0.499 - r.x, pt.y - 0.499 - r.y);
    float zoom = pageInfo->zoomReal;
    // TODO: must be a better way
    if (zoom == 0) {
        zoom = zoomReal;
    }
    return engine->Transform(p, pageNo, zoom, rotation, true);
}

RectD DisplayModel::CvtFromScreen(RectI r, int pageNo) {
    if (!ValidPageNo(pageNo)) {
        pageNo = GetPageNextToPoint(r.TL());
    }

    PointD TL = CvtFromScreen(r.TL(), pageNo);
    PointD BR = CvtFromScreen(r.BR(), pageNo);
    return RectD::FromXY(TL, BR);
}

/* Given position 'x'/'y' in the draw area, returns a structure describing
   a link or nullptr if there is no link at this position. */
PageElement* DisplayModel::GetElementAtPos(PointI pt) {
    int pageNo = GetPageNoByPoint(pt);
    if (!ValidPageNo(pageNo)) {
        return nullptr;
    }
    // only return visible elements (for cursor interaction)
    if (!RectI(PointI(), viewPort.Size()).Contains(pt)) {
        return nullptr;
    }

    PointD pos = CvtFromScreen(pt, pageNo);
    return engine->GetElementAtPos(pageNo, pos);
}

// note: returns false for pages that haven't been rendered yet
bool DisplayModel::IsOverText(PointI pt) {
    int pageNo = GetPageNoByPoint(pt);
    if (!ValidPageNo(pageNo)) {
        return false;
    }
    // only return visible elements (for cursor interaction)
    if (!RectI(PointI(), viewPort.Size()).Contains(pt)) {
        return false;
    }
    if (!textCache->HasData(pageNo)) {
        return false;
    }

    PointD pos = CvtFromScreen(pt, pageNo);
    return textSelection->IsOverGlyph(pageNo, pos.x, pos.y);
}

void DisplayModel::RenderVisibleParts() {
    int firstVisiblePage = 0;
    int lastVisiblePage = 0;

    for (int pageNo = 1; pageNo <= PageCount(); ++pageNo) {
        PageInfo* pageInfo = GetPageInfo(pageNo);
        if (pageInfo->visibleRatio > 0.0) {
            AssertCrash(pageInfo->shown);
            if (0 == firstVisiblePage)
                firstVisiblePage = pageNo;
            lastVisiblePage = pageNo;
        }
    }
    // no page is visible if e.g. the window is resized
    // vertically until only the title bar remains visible
    if (0 == firstVisiblePage)
        return;

    // rendering happens LIFO except if the queue is currently
    // empty, so request the visible pages first and last to
    // make sure they're rendered before the predicted pages
    for (int pageNo = firstVisiblePage; pageNo <= lastVisiblePage; pageNo++) {
        cb->RequestRendering(pageNo);
    }

    if (gPredictiveRender) {
        // prerender two more pages in facing and book view modes
        // if the rendering queue still has place for them
        if (!IsSingle(GetDisplayMode())) {
            if (firstVisiblePage > 2) {
                cb->RequestRendering(firstVisiblePage - 2);
            }
            if (lastVisiblePage + 1 < PageCount()) {
                cb->RequestRendering(lastVisiblePage + 2);
            }
        }
        if (firstVisiblePage > 1) {
            cb->RequestRendering(firstVisiblePage - 1);
        }
        if (lastVisiblePage < PageCount()) {
            cb->RequestRendering(lastVisiblePage + 1);
        }
    }

    // request the visible pages last so that the above requested
    // invisible pages are not rendered if the queue fills up
    for (int pageNo = lastVisiblePage; pageNo >= firstVisiblePage; pageNo--) {
        cb->RequestRendering(pageNo);
    }
}

void DisplayModel::SetViewPortSize(SizeI newViewPortSize) {
    ScrollState ss;

    bool isDocReady = ValidPageNo(startPage) && zoomReal != 0;
    if (isDocReady) {
        ss = GetScrollState();
    }

    totalViewPortSize = newViewPortSize;
    Relayout(zoomVirtual, rotation);

    if (isDocReady) {
        // when fitting to content, let GoToPage do the necessary scrolling
        if (zoomVirtual != ZOOM_FIT_CONTENT) {
            SetScrollState(ss);
        } else {
            GoToPage(ss.page, 0);
        }
    } else {
        RecalcVisibleParts();
        RenderVisibleParts();
        cb->UpdateScrollbars(canvasSize);
    }
}

RectD DisplayModel::GetContentBox(int pageNo) {
    RectD cbox{};
    // we cache the contentBox
    PageInfo* pageInfo = GetPageInfo(pageNo);
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
PointI DisplayModel::GetContentStart(int pageNo) {
    RectD contentBox = GetContentBox(pageNo);
    if (contentBox.IsEmpty()) {
        return PointI(0, 0);
    }
    return contentBox.TL().ToInt();
}

// TODO: what's GoToPage supposed to do for Facing at 400% zoom?
void DisplayModel::GoToPage(int pageNo, int scrollY, bool addNavPt, int scrollX) {
    if (!ValidPageNo(pageNo)) {
        logf("DisplayModel::GoToPage: invalid pageNo: %d, nPages: %d\n", pageNo, engine->PageCount());
        SubmitCrashIf(ValidPageNo(pageNo));
        return;
    }

    if (addNavPt) {
        AddNavPoint();
    }

    /* in facing mode only start at odd pages (odd because page
       numbering starts with 1, so odd is really an even page) */
    bool scrollToNextPage = false;
    if (!IsSingle(GetDisplayMode())) {
        int actualPageNo = pageNo;
        pageNo = FirstPageInARowNo(pageNo, ColumnsFromDisplayMode(GetDisplayMode()), IsBookView(GetDisplayMode()));
        scrollToNextPage = pageNo == actualPageNo - 1;
    }

    if (!IsContinuous(GetDisplayMode())) {
        /* in single page mode going to another page involves recalculating
           the size of canvas */
        ChangeStartPage(pageNo);
    } else if (ZOOM_FIT_CONTENT == zoomVirtual) {
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
    if (ZOOM_FIT_CONTENT == zoomVirtual) {
        // scroll down to where the actual content starts
        PointI start = GetContentStart(pageNo);
        scrollX = start.x;
        scrollY = start.y;
        if (ColumnsFromDisplayMode(GetDisplayMode()) > 1) {
            int nColumns = ColumnsFromDisplayMode(GetDisplayMode());
            bool isBook = IsBookView(GetDisplayMode());
            int nPages = PageCount();
            int lastPageNo = LastPageInARowNo(pageNo, nColumns, isBook, nPages);
            PointI second = GetContentStart(lastPageNo);
            scrollY = std::min(scrollY, second.y);
        }
        viewPort.x = scrollX + pageInfo->pos.x - windowMargin.left;
    } else if (-1 != scrollX) {
        viewPort.x = scrollX;
    } else if (-1 == scrollX && 1 == pageNo && IsBookView(GetDisplayMode())) {
        // make sure to not display the blank space beside the first page in cover mode
        viewPort.x = pageInfo->pos.x - windowMargin.left;
    } else if (viewPort.x >= pageInfo->pos.x + pageInfo->pos.dx) {
        // make sure that at least part of the page is visible
        viewPort.x = pageInfo->pos.x;
    }
    // make sure to scroll to the correct page
    if (-1 != scrollX && scrollToNextPage) {
        viewPort.x += pageInfo->pos.dx;
    }

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
    RenderVisibleParts();
    cb->UpdateScrollbars(canvasSize);
    cb->PageNoChanged(this, pageNo);
    RepaintDisplay();
}

void DisplayModel::SetDisplayMode(DisplayMode newDisplayMode, bool keepContinuous) {
    if (keepContinuous && IsContinuous(displayMode)) {
        switch (newDisplayMode) {
            case DM_SINGLE_PAGE:
                newDisplayMode = DM_CONTINUOUS;
                break;
            case DM_FACING:
                newDisplayMode = DM_CONTINUOUS_FACING;
                break;
            case DM_BOOK_VIEW:
                newDisplayMode = DM_CONTINUOUS_BOOK_VIEW;
                break;
        }
    }
    if (displayMode == newDisplayMode)
        return;

    int currPageNo = CurrentPageNo();
    if (IsFacing(newDisplayMode) && IsBookView(displayMode) && currPageNo < PageCount())
        currPageNo++;
    displayMode = newDisplayMode;
    if (IsContinuous(newDisplayMode)) {
        /* mark all pages as shown but not yet visible. The equivalent code
           for non-continuous mode is in DisplayModel::changeStartPage() called
           from DisplayModel::GoToPage() */
        for (int pageNo = 1; pageNo <= PageCount(); pageNo++) {
            PageInfo* pageInfo = &(pagesInfo[pageNo - 1]);
            pageInfo->shown = true;
            pageInfo->visibleRatio = 0.0;
        }
        Relayout(zoomVirtual, rotation);
    }
    GoToPage(currPageNo, 0);
}

void DisplayModel::SetPresentationMode(bool enable) {
    presentationMode = enable;
    if (enable) {
        presDisplayMode = displayMode;
        presZoomVirtual = zoomVirtual;
        // disable the window margin during presentations
        windowMargin.top = windowMargin.right = windowMargin.bottom = windowMargin.left = 0;
        SetDisplayMode(DM_SINGLE_PAGE);
        SetZoomVirtual(ZOOM_FIT_PAGE, nullptr);
    } else {
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
    GoToPage(firstPageInNewRow, false);
    return true;
}

bool DisplayModel::GoToPrevPage(int scrollY) {
    int columns = ColumnsFromDisplayMode(GetDisplayMode());
    int currPageNo = CurrentPageNo();

    PointI top;
    if ((0 == scrollY || -1 == scrollY) && zoomVirtual == ZOOM_FIT_CONTENT) {
        currPageNo = FirstVisiblePageNo();
        top = GetContentStart(currPageNo);
    }

    PageInfo* pageInfo = GetPageInfo(currPageNo);
    if (zoomVirtual == ZOOM_FIT_CONTENT && -pageInfo->pageOnScreen.y <= top.y)
        scrollY = 0; // continue, even though the current page isn't fully visible
    else if (std::max(-pageInfo->pageOnScreen.y, 0) > scrollY && IsContinuous(GetDisplayMode())) {
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
    if (-1 == scrollY)
        scrollY = GetPageInfo(firstPageInNewRow)->pageOnScreen.dy;

    GoToPage(firstPageInNewRow, scrollY);
    return true;
}

bool DisplayModel::GoToLastPage() {
    int columns = ColumnsFromDisplayMode(GetDisplayMode());
    int currPageNo = CurrentPageNo();
    int newPageNo = PageCount();
    int firstPageInLastRow = FirstPageInARowNo(newPageNo, columns, IsBookView(GetDisplayMode()));

    if (currPageNo == firstPageInLastRow) /* are we on the last page already ? */
        return false;
    GoToPage(firstPageInLastRow, 0, true);
    return true;
}

bool DisplayModel::GoToFirstPage() {
    if (IsContinuous(GetDisplayMode())) {
        if (0 == viewPort.y) {
            return false;
        }
    } else {
        AssertCrash(PageShown(startPage));
        if (1 == startPage) {
            /* we're on a first page already */
            return false;
        }
    }
    GoToPage(1, 0, true);
    return true;
}

void DisplayModel::ScrollXTo(int xOff) {
    int currPageNo = CurrentPageNo();
    viewPort.x = xOff;
    RecalcVisibleParts();
    cb->UpdateScrollbars(canvasSize);

    if (CurrentPageNo() != currPageNo)
        cb->PageNoChanged(this, CurrentPageNo());
    RepaintDisplay();
}

void DisplayModel::ScrollXBy(int dx) {
    int newOffX = limitValue(viewPort.x + dx, 0, canvasSize.dx - viewPort.dx);
    if (newOffX != viewPort.x)
        ScrollXTo(newOffX);
}

void DisplayModel::ScrollYTo(int yOff) {
    int currPageNo = CurrentPageNo();
    viewPort.y = yOff;
    RecalcVisibleParts();
    RenderVisibleParts();

    int newPageNo = CurrentPageNo();
    if (newPageNo != currPageNo)
        cb->PageNoChanged(this, newPageNo);
    RepaintDisplay();
}

/* Scroll the doc in y-axis by 'dy'. If 'changePage' is TRUE, automatically
   switch to prev/next page in non-continuous mode if we scroll past the edges
   of current page */
void DisplayModel::ScrollYBy(int dy, bool changePage) {
    PageInfo* pageInfo;
    int currYOff = viewPort.y;
    int newPageNo;
    int currPageNo;

    AssertCrash(0 != dy);
    if (0 == dy)
        return;

    int newYOff = currYOff;

    if (!IsContinuous(GetDisplayMode()) && changePage) {
        if ((dy < 0) && (0 == currYOff)) {
            if (startPage > 1) {
                newPageNo = startPage - 1;
                AssertCrash(ValidPageNo(newPageNo));
                pageInfo = GetPageInfo(newPageNo);
                newYOff = pageInfo->pos.dy - viewPort.dy;
                if (newYOff < 0)
                    newYOff = 0; /* TODO: center instead? */
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
    if (newYOff == currYOff)
        return;

    currPageNo = CurrentPageNo();
    viewPort.y = newYOff;
    RecalcVisibleParts();
    RenderVisibleParts();
    cb->UpdateScrollbars(canvasSize);
    newPageNo = CurrentPageNo();
    if (newPageNo != currPageNo)
        cb->PageNoChanged(this, newPageNo);
    RepaintDisplay();
}

void DisplayModel::SetZoomVirtual(float zoomLevel, PointI* fixPt) {
    if (zoomLevel > 0)
        zoomLevel = limitValue(zoomLevel, ZOOM_MIN, ZOOM_MAX);
    if (!IsValidZoom(zoomLevel))
        return;

    bool scrollToFitPage = ZOOM_FIT_PAGE == zoomLevel || ZOOM_FIT_CONTENT == zoomLevel;
    if (zoomVirtual == zoomLevel && (fixPt || !scrollToFitPage))
        return;

    ScrollState ss = GetScrollState();

    int centerPage = -1;
    PointD centerPt;
    if (fixPt) {
        centerPage = GetPageNoByPoint(*fixPt);
        if (ValidPageNo(centerPage))
            centerPt = CvtFromScreen(*fixPt, centerPage);
        else
            fixPt = nullptr;
    }

    if (scrollToFitPage) {
        ss.page = CurrentPageNo();
        // SetScrollState's first call to GoToPage will already scroll to fit
        ss.x = ss.y = -1;
    }

    // lf("DisplayModel::SetZoomVirtual() zoomLevel=%.6f", _zoomLevel);
    Relayout(zoomLevel, rotation);
    SetScrollState(ss);

    if (fixPt) {
        // scroll so that the fix point remains in the same screen location after zooming
        PointI centerI = CvtToScreen(centerPage, centerPt);
        if (centerI.x - fixPt->x != 0)
            ScrollXBy(centerI.x - fixPt->x);
        if (centerI.y - fixPt->y != 0)
            ScrollYBy(centerI.y - fixPt->y, false);
    }
}

float DisplayModel::GetZoomVirtual(bool absolute) const {
    if (absolute) {
        // revert the dpiFactor premultiplication for converting zoomReal back to zoomVirtual
        return zoomReal * 100 / dpiFactor;
    }
    return zoomVirtual;
}

float DisplayModel::GetNextZoomStep(float towardsLevel) const {
    if (gGlobalPrefs->zoomIncrement > 0) {
        float zoom = GetZoomVirtual(true);
        if (zoom < towardsLevel)
            return std::min(zoom * (gGlobalPrefs->zoomIncrement / 100 + 1), towardsLevel);
        if (zoom > towardsLevel)
            return std::max(zoom / (gGlobalPrefs->zoomIncrement / 100 + 1), towardsLevel);
        return zoom;
    }

#if 0
    // differences to Adobe Reader: starts at 8.33 (instead of 1 and 6.25)
    // and has four additional intermediary zoom levels ("added")
    static float zoomLevels[] = {
        8.33f, 12.5f, 18 /* added */, 25, 33.33f, 50, 66.67f, 75,
        100, 125, 150, 200, 300, 400, 600, 800, 1000 /* added */,
        1200, 1600, 2000 /* added */, 2400, 3200, 4800 /* added */, 6400
    };
    CrashIf(zoomLevels[0] != ZOOM_MIN || zoomLevels[dimof(zoomLevels)-1] != ZOOM_MAX);
#endif
    Vec<float>* zoomLevels = gGlobalPrefs->zoomLevels;
    CrashIf(zoomLevels->size() != 0 && (zoomLevels->at(0) < ZOOM_MIN || zoomLevels->Last() > ZOOM_MAX));
    CrashIf(zoomLevels->size() != 0 && zoomLevels->at(0) > zoomLevels->Last());

    float currZoom = GetZoomVirtual(true);
    float pageZoom = (float)HUGE_VAL, widthZoom = (float)HUGE_VAL;
    for (int pageNo = 1; pageNo <= PageCount(); pageNo++) {
        if (PageShown(pageNo)) {
            float pagePageZoom = ZoomRealFromVirtualForPage(ZOOM_FIT_PAGE, pageNo);
            pageZoom = std::min(pageZoom, pagePageZoom);
            float pageWidthZoom = ZoomRealFromVirtualForPage(ZOOM_FIT_WIDTH, pageNo);
            widthZoom = std::min(widthZoom, pageWidthZoom);
        }
    }
    CrashIf(pageZoom == (float)HUGE_VAL || widthZoom == (float)HUGE_VAL);
    CrashIf(pageZoom > widthZoom);
    pageZoom *= 100 / dpiFactor;
    widthZoom *= 100 / dpiFactor;

    const float FUZZ = 0.01f;
    float newZoom = towardsLevel;
    if (currZoom < towardsLevel) {
        for (size_t i = 0; i < zoomLevels->size(); i++) {
            if (zoomLevels->at(i) - FUZZ > currZoom) {
                newZoom = zoomLevels->at(i);
                break;
            }
        }
        if (currZoom + FUZZ < pageZoom && pageZoom < newZoom - FUZZ)
            newZoom = ZOOM_FIT_PAGE;
        else if (currZoom + FUZZ < widthZoom && widthZoom < newZoom - FUZZ)
            newZoom = ZOOM_FIT_WIDTH;
    } else if (currZoom > towardsLevel) {
        for (size_t i = zoomLevels->size(); i > 0; i--) {
            if (zoomLevels->at(i - 1) + FUZZ < currZoom) {
                newZoom = zoomLevels->at(i - 1);
                break;
            }
        }
        // skip Fit Width if it results in the same value as Fit Page (same as when zooming in)
        if (newZoom + FUZZ < widthZoom && widthZoom < currZoom - FUZZ && widthZoom != pageZoom)
            newZoom = ZOOM_FIT_WIDTH;
        else if (newZoom + FUZZ < pageZoom && pageZoom < currZoom - FUZZ)
            newZoom = ZOOM_FIT_PAGE;
    }

    return newZoom;
}

void DisplayModel::RotateBy(int newRotation) {
    newRotation = NormalizeRotation(newRotation);
    AssertCrash(0 != newRotation);
    if (0 == newRotation)
        return;
    newRotation = NormalizeRotation(newRotation + rotation);

    int currPageNo = CurrentPageNo();
    Relayout(zoomVirtual, newRotation);
    GoToPage(currPageNo, 0);
}

/* Given <region> (in user coordinates ) on page <pageNo>, copies text in that region
 * into a newly allocated buffer (which the caller needs to free()). */
WCHAR* DisplayModel::GetTextInRegion(int pageNo, RectD region) {
    RectI* coords;
    const WCHAR* pageText = textCache->GetData(pageNo, nullptr, &coords);
    if (str::IsEmpty(pageText))
        return nullptr;

    str::WStr result;
    RectI regionI = region.Round();
    for (const WCHAR* src = pageText; *src; src++) {
        if (*src != '\n') {
            RectI rect = coords[src - pageText];
            RectI isect = regionI.Intersect(rect);
            if (!isect.IsEmpty() && 1.0 * isect.dx * isect.dy / (rect.dx * rect.dy) >= 0.3)
                result.Append(*src);
        } else if (result.size() > 0 && result.Last() != '\n')
            result.Append(L"\r\n", 2);
    }

    return result.StealData();
}

// returns true if it was necessary to scroll the display (horizontally or vertically)
bool DisplayModel::ShowResultRectToScreen(TextSel* res) {
    if (!res->len)
        return false;

    RectI extremes;
    for (int i = 0; i < res->len; i++) {
        RectI rc = CvtToScreen(res->pages[i], res->rects[i].Convert<double>());
        extremes = extremes.Union(rc);
    }

    // don't scroll if the whole result is already visible
    if (RectI(PointI(), viewPort.Size()).Intersect(extremes) == extremes)
        return false;

    PageInfo* pageInfo = GetPageInfo(res->pages[0]);
    int sx = 0, sy = 0;

    // vertically, we try to position the search result between 40%
    // (scrolling up) and 60% (scrolling down) of the screen, so that
    // the search direction remains obvious and we still display some
    // context before and after the found text
    if (extremes.y < viewPort.dy * 2 / 5)
        sy = extremes.y - viewPort.dy * 2 / 5;
    else if (extremes.y + extremes.dy > viewPort.dy * 3 / 5)
        sy = std::min(extremes.y + extremes.dy - viewPort.dy * 3 / 5,
                      extremes.y + extremes.dy / 2 - viewPort.dy * 2 / 5);

    // horizontally, we try to position the search result at the
    // center of the screen, but don't scroll further than page
    // boundaries, so that as much context as possible remains visible
    if (extremes.x < 0)
        sx = std::max(extremes.x + extremes.dx / 2 - viewPort.dx / 2, pageInfo->pageOnScreen.x);
    else if (extremes.x + extremes.dx >= viewPort.dx)
        sx = std::min(extremes.x + extremes.dx / 2 - viewPort.dx / 2,
                      pageInfo->pageOnScreen.x + pageInfo->pageOnScreen.dx - viewPort.dx);

    if (sx != 0)
        ScrollXBy(sx);
    if (sy != 0)
        ScrollYBy(sy, false);

    return sx != 0 || sy != 0;
}

ScrollState DisplayModel::GetScrollState() {
    ScrollState state(FirstVisiblePageNo(), -1, -1);
    if (!ValidPageNo(state.page))
        state.page = CurrentPageNo();

    PageInfo* pageInfo = GetPageInfo(state.page);
    // Shortcut: don't calculate precise positions, if the
    // page wasn't scrolled right/down at all
    if (!pageInfo || pageInfo->pageOnScreen.x > 0 && pageInfo->pageOnScreen.y > 0)
        return state;

    RectI screen(PointI(), viewPort.Size());
    RectI pageVis = pageInfo->pageOnScreen.Intersect(screen);
    state.page = GetPageNextToPoint(pageVis.TL());
    PointD ptD = CvtFromScreen(pageVis.TL(), state.page);

    // Remember to show the margin, if it's currently visible
    if (pageInfo->pageOnScreen.x <= 0)
        state.x = ptD.x;
    if (pageInfo->pageOnScreen.y <= 0)
        state.y = ptD.y;

    return state;
}

void DisplayModel::SetScrollState(ScrollState state) {
    // Update the internal metrics first
    GoToPage(state.page, 0);
    // Bail out, if the page wasn't scrolled
    if (state.x < 0 && state.y < 0)
        return;

    PointD newPtD(std::max(state.x, (double)0), std::max(state.y, (double)0));
    PointI newPt = CvtToScreen(state.page, newPtD);

    // Also show the margins, if this has been requested
    if (state.x < 0)
        newPt.x = -1;
    else
        newPt.x += viewPort.x;
    if (state.y < 0)
        newPt.y = 0;
    GoToPage(state.page, newPt.y, false, newPt.x);
}

// don't remember more than "enough" history entries (same number as Firefox uses)
#define MAX_NAV_HISTORY_LEN 50

/* Records the current scroll state for later navigating back to. */
void DisplayModel::AddNavPoint() {
    ScrollState ss = GetScrollState();
    // remove the current and all Forward history entries
    if (navHistoryIx < navHistory.size())
        navHistory.RemoveAt(navHistoryIx, navHistory.size() - navHistoryIx);
    // don't add another entry for the exact same position
    if (navHistoryIx > 0 && ss == navHistory.at(navHistoryIx - 1))
        return;
    // make sure that the history doesn't grow overly large
    if (navHistoryIx >= MAX_NAV_HISTORY_LEN) {
        CrashIf(navHistoryIx > MAX_NAV_HISTORY_LEN);
        navHistory.RemoveAt(0, navHistoryIx - MAX_NAV_HISTORY_LEN + 1);
        navHistoryIx = MAX_NAV_HISTORY_LEN - 1;
    }
    // add a new Back history entry
    navHistory.Append(ss);
    navHistoryIx++;
}

bool DisplayModel::CanNavigate(int dir) const {
    CrashIf(navHistoryIx > navHistory.size());
    if (dir < 0)
        return navHistoryIx >= (size_t)-dir;
    return navHistoryIx + dir < navHistory.size();
}

/* Navigates |dir| steps forward or backwards. */
void DisplayModel::Navigate(int dir) {
    if (!CanNavigate(dir))
        return;
    // update the current history entry
    ScrollState ss = GetScrollState();
    if (navHistoryIx < navHistory.size())
        navHistory.at(navHistoryIx) = ss;
    else
        navHistory.Append(ss);
    navHistoryIx += dir;
    SetScrollState(navHistory.at(navHistoryIx));
}

void DisplayModel::CopyNavHistory(DisplayModel& orig) {
    navHistory = orig.navHistory;
    navHistoryIx = orig.navHistoryIx;
    // remove navigation history entries for all no longer valid pages
    for (size_t i = navHistory.size(); i > 0; i--) {
        if (!ValidPageNo(navHistory.at(i - 1).page)) {
            navHistory.RemoveAt(i - 1);
            if (i - 1 < navHistoryIx)
                navHistoryIx--;
        }
    }
}

bool DisplayModel::ShouldCacheRendering(int pageNo) {
    // recommend caching for all documents which are non-trivial to render
    if (!engine->IsImageCollection()) {
        return true;
    }

    // recommend caching large images (mainly photos) as well, as shrinking
    // them for every UI update (WM_PAINT) can cause notable lags, and also
    // for smaller images which are scaled up
    PageInfo* info = GetPageInfo(pageNo);
    return info->page.dx * info->page.dy > 1024 * 1024 || info->pageOnScreen.dx * info->pageOnScreen.dy > 1024 * 1024;
}

void DisplayModel::ScrollToLink(PageDestination* dest) {
    CrashIf(!dest || dest->GetPageNo() <= 0);

    PointI scroll(-1, 0);
    RectD rect = dest->GetRect();
    int pageNo = dest->GetPageNo();

    if (rect.IsEmpty()) {
        // PDF: /XYZ top left
        // scroll to rect.TL()
        PointD scrollD = engine->Transform(rect.TL(), pageNo, zoomReal, rotation);
        scroll = scrollD.ToInt();

        // default values for the coordinates mean: keep the current position
        if (DEST_USE_DEFAULT == rect.x)
            scroll.x = -1;
        if (DEST_USE_DEFAULT == rect.y) {
            PageInfo* pageInfo = GetPageInfo(CurrentPageNo());
            scroll.y = -(pageInfo->pageOnScreen.y - windowMargin.top);
        }
    } else if (rect.dx != DEST_USE_DEFAULT && rect.dy != DEST_USE_DEFAULT) {
        // PDF: /FitR left bottom right top
        RectD rectD = engine->Transform(rect, pageNo, zoomReal, rotation);
        scroll = rectD.TL().ToInt();

        // Rect<float> rectF = _engine->Transform(rect, pageNo, 1.0, rotation).Convert<float>();
        // zoom = 100.0f * std::min(viewPort.dx / rectF.dx, viewPort.dy / rectF.dy);
    } else if (rect.y != DEST_USE_DEFAULT) {
        // PDF: /FitH top  or  /FitBH top
        PointD scrollD = engine->Transform(rect.TL(), pageNo, zoomReal, rotation);
        scroll.y = scrollD.ToInt().y;
        // zoom = FitBH ? ZOOM_FIT_CONTENT : ZOOM_FIT_WIDTH
    }
    // else if (Fit || FitV) zoom = ZOOM_FIT_PAGE
    // else if (FitB || FitBV) zoom = ZOOM_FIT_CONTENT
    /* // ignore author-set zoom settings (at least as long as there's no way to overrule them)
    if (zoom != INVALID_ZOOM) {
        // TODO: adjust the zoom level before calculating the scrolling coordinates
        SetZoomVirtual(zoom);
        UpdateToolbarState(owner);
    }
    // */
    // TODO: prevent scroll.y from getting too large?
    if (scroll.y < 0) {
        scroll.y = 0; // Adobe Reader never shows the previous page
    }
    GoToPage(pageNo, scroll.y, true, scroll.x);
}
