/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
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

#include "DisplayModel.h"

// Note: adding chm handling to DisplayModel is a hack, because DisplayModel
// doesn't map to chm features well.

// if true, we pre-render the pages right before and after the visible pages
bool gPredictiveRender = true;

ScreenPagePadding gPagePadding = {
    PADDING_PAGE_BORDER_TOP_DEF, PADDING_PAGE_BORDER_LEFT_DEF,
    PADDING_PAGE_BORDER_BOTTOM_DEF, PADDING_PAGE_BORDER_RIGHT_DEF,
    PADDING_BETWEEN_PAGES_X_DEF, PADDING_BETWEEN_PAGES_Y_DEF
}, gPagePaddingPresentation = {
    0, 0,
    0, 0,
    PADDING_BETWEEN_PAGES_X_DEF, PADDING_BETWEEN_PAGES_X_DEF
}, gImagePadding = {
    0, 0,
    0, 0,
    PADDING_BETWEEN_PAGES_X_DEF, PADDING_BETWEEN_PAGES_X_DEF
};

bool displayModeContinuous(DisplayMode displayMode)
{
    return DM_CONTINUOUS == displayMode ||
           DM_CONTINUOUS_FACING == displayMode ||
           DM_CONTINUOUS_BOOK_VIEW == displayMode;
}

bool displayModeSingle(DisplayMode displayMode)
{
    return DM_SINGLE_PAGE == displayMode ||
           DM_CONTINUOUS == displayMode;
}

bool displayModeFacing(DisplayMode displayMode)
{
    return DM_FACING == displayMode ||
           DM_CONTINUOUS_FACING == displayMode;
}

bool displayModeShowCover(DisplayMode displayMode)
{
    return DM_BOOK_VIEW == displayMode ||
           DM_CONTINUOUS_BOOK_VIEW == displayMode;
}

int columnsFromDisplayMode(DisplayMode displayMode)
{
    if (!displayModeSingle(displayMode))
        return 2;
    return 1;
}

int normalizeRotation(int rotation)
{
    assert((rotation % 90) == 0);
    rotation = rotation % 360;
    if (rotation < 0)
        rotation += 360;
    if (rotation < 0 || rotation >= 360 || (rotation % 90) != 0) {
        DBG_OUT("normalizeRotation() invalid rotation: %d\n", rotation);
        return 0;
    }
    return rotation;
}

static bool IsZoomVirtual(float zoomLevel)
{
    if ((ZOOM_FIT_PAGE == zoomLevel) || (ZOOM_FIT_WIDTH == zoomLevel) ||
        (ZOOM_FIT_CONTENT == zoomLevel))
        return true;
    return false;
}

static bool IsValidZoom(float zoomLevel)
{
    if (IsZoomVirtual(zoomLevel))
        return true;
    if ((zoomLevel < ZOOM_MIN) || (zoomLevel > ZOOM_MAX)) {
        DBG_OUT("IsValidZoom() invalid zoom: %.4f\n", zoomLevel);
        return false;
    }
    return true;
}

bool DisplayModel::DisplayStateFromModel(DisplayState *ds)
{
    if (!ds->filePath || !str::Eq(ds->filePath, FileName()))
        str::ReplacePtr(&ds->filePath, FileName());

    ds->displayMode = _presentationMode ? _presDisplayMode : displayMode();
    ds->rotation = _rotation;
    ds->zoomVirtual = _presentationMode ? _presZoomVirtual : _zoomVirtual;

    ScrollState ss = GetScrollState();
    ds->pageNo = ss.page;
    if (_presentationMode)
        ds->scrollPos = PointI();
    else
        ds->scrollPos = PointD(ss.x, ss.y).Convert<int>();

    free(ds->decryptionKey);
    ds->decryptionKey = engine->GetDecryptionKey();

    return true;
}

SizeD DisplayModel::PageSizeAfterRotation(int pageNo, bool fitToContent)
{
    PageInfo *pageInfo = GetPageInfo(pageNo);
    if (fitToContent && pageInfo->contentBox.IsEmpty()) {
        pageInfo->contentBox = engine->PageContentBox(pageNo);
        if (pageInfo->contentBox.IsEmpty())
            return PageSizeAfterRotation(pageNo);
    }

    RectD box = fitToContent ? pageInfo->contentBox : pageInfo->page;
    return engine->Transform(box, pageNo, 1.0, _rotation).Size();
}

/* given 'columns' and an absolute 'pageNo', return the number of the first
   page in a row to which a 'pageNo' belongs e.g. if 'columns' is 2 and we
   have 5 pages in 3 rows (depending on showCover):

   Pages   Result           Pages   Result           Pages   Result (R2L)
   (1,2)   1                  (1)   1                (2,1)   1
   (3,4)   3                (2,3)   2                (4,3)   3
   (5)     5                (4,5)   4                  (5)   5
 */
static int FirstPageInARowNo(int pageNo, int columns, bool showCover)
{
    if (showCover && columns > 1)
        pageNo++;
    int firstPageNo = pageNo - ((pageNo - 1) % columns);
    if (showCover && columns > 1 && firstPageNo > 1)
        firstPageNo--;
    return firstPageNo;
}

static int LastPageInARowNo(int pageNo, int columns, bool showCover, int pageCount)
{
    int lastPageNo = FirstPageInARowNo(pageNo, columns, showCover) + columns - 1;
    if (showCover && pageNo < columns)
        lastPageNo--;
    return min(lastPageNo, pageCount);
}

DisplayModel::DisplayModel(DisplayModelCallback *cb)
{
    _displayMode = DM_AUTOMATIC;
    _presDisplayMode = DM_AUTOMATIC;
    _presZoomVirtual = INVALID_ZOOM;
    _rotation = INVALID_ROTATION;
    _zoomVirtual = INVALID_ZOOM;
    padding = &gPagePadding;
    _presentationMode = false;
    _zoomReal = INVALID_ZOOM;
    dpiFactor = 1.0f;
    _startPage = INVALID_PAGE_NO;
    dmCb = cb;

    engine = NULL;
    engineType = Engine_None;

    textSelection = NULL;
    textSearch = NULL;
    pagesInfo = NULL;

    navHistoryIx = 0;
    navHistoryEnd = 0;

    dontRenderFlag = false;

    displayR2L = false;
}

DisplayModel::~DisplayModel()
{
    dontRenderFlag = true;
    dmCb->CleanUp(this);

    delete textSearch;
    delete textSelection;
    delete engine;
    free(pagesInfo);
}

PageInfo *DisplayModel::GetPageInfo(int pageNo) const
{
    if (!ValidPageNo(pageNo) || AsChmEngine())
        return NULL;
    assert(pagesInfo);
    if (!pagesInfo) return NULL;
    return &(pagesInfo[pageNo-1]);
}

// Call this after Load() but before the first Relayout
void DisplayModel::SetInitialViewSettings(DisplayMode displayMode, int newStartPage, SizeI viewPort, int screenDPI)
{
    totalViewPortSize = viewPort;
    dpiFactor = 1.0f * screenDPI / engine->GetFileDPI();
    if (ValidPageNo(newStartPage))
        _startPage = newStartPage;

    if (AsChmEngine())
        displayMode = DM_SINGLE_PAGE;

    _displayMode = displayMode;
    _presDisplayMode = displayMode;
    PageLayoutType layout = engine->PreferredLayout();
    if (DM_AUTOMATIC == _displayMode) {
        switch (layout & ~Layout_R2L) {
        case Layout_Single: _displayMode = DM_CONTINUOUS; break;
        case Layout_Facing: _displayMode = DM_CONTINUOUS_FACING; break;
        case Layout_Book: _displayMode = DM_CONTINUOUS_BOOK_VIEW; break;
        case Layout_Single | Layout_NonContinuous: _displayMode = DM_SINGLE_PAGE; break;
        case Layout_Facing | Layout_NonContinuous: _displayMode = DM_FACING; break;
        case Layout_Book | Layout_NonContinuous: _displayMode = DM_BOOK_VIEW; break;
        }
    }
    displayR2L = (layout & Layout_R2L) != 0;
    if (!AsChmEngine())
        BuildPagesInfo();
}

// must call SetInitialViewSettings() after Load()
bool DisplayModel::Load(const TCHAR *fileName)
{ 
    assert(fileName);
    engine = EngineManager::CreateEngine(fileName, dmCb, &engineType);
    if (!engine)
        return false;
    assert(engine->PageCount() > 0);
    _startPage = 1;
    if (engine->IsImageCollection())
        padding = &gImagePadding;
    if (AsChmEngine())
        AsChmEngine()->SetNavigationCalback(dmCb);

    textSelection = new TextSelection(engine);
    textSearch = new TextSearch(engine);
    return true;
}

void DisplayModel::BuildPagesInfo()
{
    assert(!pagesInfo);
    int pageCount = PageCount();
    pagesInfo = SAZA(PageInfo, pageCount);

    TCHAR unitSystem[2];
    GetLocaleInfo(LOCALE_USER_DEFAULT, LOCALE_IMEASURE, unitSystem, dimof(unitSystem));
    RectD defaultRect;
    if (unitSystem[0] == '0') // metric A4 size
        defaultRect = RectD(0, 0, 21.0 / 2.54 * engine->GetFileDPI(), 29.7 / 2.54 * engine->GetFileDPI());
    else // imperial letter size
        defaultRect = RectD(0, 0, 8.5 * engine->GetFileDPI(), 11 * engine->GetFileDPI());

    int columns = columnsFromDisplayMode(_displayMode);
    int startPage = _startPage;
    if (displayModeShowCover(_displayMode) && startPage == 1 && columns > 1)
        startPage--;
    for (int pageNo = 1; pageNo <= pageCount; pageNo++) {
        PageInfo *pageInfo = GetPageInfo(pageNo);
        pageInfo->page = engine->PageMediabox(pageNo);
        // layout pages with an empty mediabox as A4 size (resp. letter size)
        if (pageInfo->page.IsEmpty())
            pageInfo->page = defaultRect;
        pageInfo->visibleRatio = 0.0;
        pageInfo->shown = false;
        if (displayModeContinuous(_displayMode))
            pageInfo->shown = true;
        else if (startPage <= pageNo && pageNo < startPage + columns)
            pageInfo->shown = true;
    }
}

// TODO: a better name e.g. ShouldShow() to better distinguish between
// before-layout info and after-layout visibility checks
bool DisplayModel::PageShown(int pageNo)
{
    PageInfo *pageInfo = GetPageInfo(pageNo);
    if (!pageInfo)
        return false;
    return pageInfo->shown;
}

bool DisplayModel::PageVisible(int pageNo)
{
    PageInfo *pageInfo = GetPageInfo(pageNo);
    if (!pageInfo)
        return false;
    return pageInfo->visibleRatio > 0.0;
}

/* Return true if a page is visible or a page in a row below or above is visible */
bool DisplayModel::PageVisibleNearby(int pageNo)
{
    DisplayMode mode = displayMode();
    int columns = columnsFromDisplayMode(mode);

    pageNo = FirstPageInARowNo(pageNo, columns, displayModeShowCover(mode));
    for (int i = pageNo - columns; i < pageNo + 2 * columns; i++) {
        if (ValidPageNo(i) && PageVisible(i))
            return true;
    }
    return false;
}

/* Return true if the first page is fully visible and alone on a line in
   show cover mode (i.e. it's not possible to flip to a previous page) */
bool DisplayModel::FirstBookPageVisible()
{
    if (!displayModeShowCover(displayMode()))
        return false;
    if (CurrentPageNo() != 1)
        return false;
    return true;
}

/* Return true if the last page is fully visible and alone on a line in
   facing or show cover mode (i.e. it's not possible to flip to a next page) */
bool DisplayModel::LastBookPageVisible()
{
    int count = PageCount();
    DisplayMode mode = displayMode();
    if (displayModeSingle(mode))
        return false;
    if (CurrentPageNo() == count)
        return true;
    if (GetPageInfo(count)->visibleRatio < 1.0)
        return false;
    if (FirstPageInARowNo(count, columnsFromDisplayMode(mode),
                          displayModeShowCover(mode)) < count)
        return false;
    return true;
}

/* Given a zoom level that can include a "virtual" zoom levels like ZOOM_FIT_WIDTH,
   ZOOM_FIT_PAGE or ZOOM_FIT_CONTENT, calculate an absolute zoom level */
float DisplayModel::ZoomRealFromVirtualForPage(float zoomVirtual, int pageNo)
{
    if (zoomVirtual != ZOOM_FIT_WIDTH && zoomVirtual != ZOOM_FIT_PAGE && zoomVirtual != ZOOM_FIT_CONTENT)
        return zoomVirtual * 0.01f * dpiFactor;

    SizeD row;
    int columns = columnsFromDisplayMode(displayMode());

    bool fitToContent = (ZOOM_FIT_CONTENT == zoomVirtual);
    if (fitToContent && columns > 1) {
        // Fit the content of all the pages in the same row into the visible area
        // (i.e. don't crop inner margins but just the left-most, right-most, etc.)
        int first = FirstPageInARowNo(pageNo, columns, displayModeShowCover(displayMode()));
        int last = LastPageInARowNo(pageNo, columns, displayModeShowCover(displayMode()), PageCount());
        for (int i = first; i <= last; i++) {
            PageInfo *pageInfo = GetPageInfo(i);
            RectD pageBox = engine->Transform(pageInfo->page, i, 1.0, _rotation);
            row.dx += pageBox.dx;

            if (pageInfo->contentBox.IsEmpty())
                pageInfo->contentBox = engine->PageContentBox(i);
            RectD rotatedContent = engine->Transform(pageInfo->contentBox, i, 1.0, _rotation);
            if (i == first && !rotatedContent.IsEmpty())
                row.dx -= (rotatedContent.x - pageBox.x);
            if (i == last && !rotatedContent.IsEmpty())
                row.dx -= (pageBox.BR().x - rotatedContent.BR().x);

            SizeD pageSize = PageSizeAfterRotation(i, true);
            if (row.dy < pageSize.dy)
                row.dy = pageSize.dy;
        }
    } else {
        row = PageSizeAfterRotation(pageNo, fitToContent);
        row.dx *= columns;
    }

    assert(!RectD(PointD(), row).IsEmpty());
    if (RectD(PointD(), row).IsEmpty())
        return 0;

    int areaForPagesDx = viewPort.dx - padding->left - padding->right - padding->inBetweenX * (columns - 1);
    int areaForPagesDy = viewPort.dy - padding->top - padding->bottom;
    if (areaForPagesDx <= 0 || areaForPagesDy <= 0)
        return 0;

    float zoomX = areaForPagesDx / (float)row.dx;
    float zoomY = areaForPagesDy / (float)row.dy;
    if (zoomX < zoomY || ZOOM_FIT_WIDTH == zoomVirtual)
        return zoomX;
    return zoomY;
}

int DisplayModel::FirstVisiblePageNo() const
{
    if (AsChmEngine())
        return AsChmEngine()->CurrentPageNo();

    assert(pagesInfo);
    if (!pagesInfo) return INVALID_PAGE_NO;

    for (int pageNo = 1; pageNo <= PageCount(); ++pageNo) {
        PageInfo *pageInfo = GetPageInfo(pageNo);
        if (pageInfo->visibleRatio > 0.0)
            return pageNo;
    }

    /* If no pages are visible */
    return INVALID_PAGE_NO;
}

// we consider the most visible page the current one
// (in continuous layout, there's no better criteria)
int DisplayModel::CurrentPageNo() const
{
    if (AsChmEngine())
        return AsChmEngine()->CurrentPageNo();

    if (!displayModeContinuous(displayMode()))
        return _startPage;

    assert(pagesInfo);
    if (!pagesInfo) return INVALID_PAGE_NO;
    // determine the most visible page
    int mostVisiblePage = INVALID_PAGE_NO;
    float ratio = 0;

    for (int pageNo = 1; pageNo <= PageCount(); pageNo++) {
        PageInfo *pageInfo = GetPageInfo(pageNo);
        if (pageInfo->visibleRatio > ratio) {
            mostVisiblePage = pageNo;
            ratio = pageInfo->visibleRatio;
        }
    }

    /* if no page is visible, default to either the first or the last one */
    if (INVALID_PAGE_NO == mostVisiblePage) {
        PageInfo *pageInfo = GetPageInfo(1);
        if (pageInfo && viewPort.y > pageInfo->pos.y + pageInfo->pos.dy)
            mostVisiblePage = PageCount();
        else
            mostVisiblePage = 1;
    }

    return mostVisiblePage;
}

void DisplayModel::SetZoomVirtual(float zoomVirtual)
{
    assert(IsValidZoom(zoomVirtual));
    _zoomVirtual = zoomVirtual;

    if (AsChmEngine()) {
        if (IsZoomVirtual(_zoomVirtual) || !IsValidZoom(_zoomVirtual))
            _zoomVirtual = 100.0f;
        _zoomReal = _zoomVirtual * 0.01f * dpiFactor;
        AsChmEngine()->ZoomTo(_zoomVirtual);
        return;
    }

    if ((ZOOM_FIT_WIDTH == zoomVirtual) || (ZOOM_FIT_PAGE == zoomVirtual)) {
        /* we want the same zoom for all pages, so use the smallest zoom
           across the pages so that the largest page fits. In most documents
           all pages are the same size anyway */
        float minZoom = (float)HUGE_VAL;
        for (int pageNo = 1; pageNo <= PageCount(); pageNo++) {
            if (PageShown(pageNo)) {
                float thisPageZoom = ZoomRealFromVirtualForPage(zoomVirtual, pageNo);
                if (minZoom > thisPageZoom)
                    minZoom = thisPageZoom;
            }
        }
        assert(minZoom != (float)HUGE_VAL);
        this->_zoomReal = minZoom;
    } else if (ZOOM_FIT_CONTENT == zoomVirtual) {
        float newZoom = ZoomRealFromVirtualForPage(zoomVirtual, CurrentPageNo());
        // limit zooming in to 800% on almost empty pages
        if (newZoom > 8.0)
            newZoom = 8.0;
        // don't zoom in by just a few pixels (throwing away a prerendered page)
        if (newZoom < this->_zoomReal || this->_zoomReal / newZoom < 0.95 ||
            this->_zoomReal < ZoomRealFromVirtualForPage(ZOOM_FIT_PAGE, CurrentPageNo()))
            this->_zoomReal = newZoom;
    } else {
        this->_zoomReal = zoomVirtual * 0.01f * dpiFactor;
    }
}

float DisplayModel::ZoomReal(int pageNo)
{
    DisplayMode mode = displayMode();
    if (displayModeContinuous(mode))
        return _zoomReal;
    if (displayModeSingle(mode))
        return ZoomRealFromVirtualForPage(_zoomVirtual, pageNo);
    pageNo = FirstPageInARowNo(pageNo, columnsFromDisplayMode(mode), displayModeShowCover(mode));
    if (pageNo == PageCount() || pageNo == 1 && displayModeShowCover(mode))
        return ZoomRealFromVirtualForPage(_zoomVirtual, pageNo);
    return min(ZoomRealFromVirtualForPage(_zoomVirtual, pageNo), ZoomRealFromVirtualForPage(_zoomVirtual, pageNo + 1));
}

/* Given zoom and rotation, calculate the position of each page on a
   large sheet that is continous view. Needs to be recalculated when:
     * zoom changes
     * rotation changes
     * switching between display modes
     * navigating to another page in non-continuous mode */
void DisplayModel::Relayout(float zoomVirtual, int rotation)
{
    // an optimization: the code below doesn't make sense for chm
    // and causes SetZoomVirtual() to be called 3 times, so skip it
    if (AsChmEngine()) {
        SetZoomVirtual(zoomVirtual);
        return;
    }

    assert(pagesInfo);
    if (!pagesInfo)
        return;

    _rotation = normalizeRotation(rotation);

    bool needHScroll = false;
    bool needVScroll = false;
    viewPort = RectI(viewPort.TL(), totalViewPortSize);

RestartLayout:
    int currPosY = padding->top;
    float currZoomReal = _zoomReal;
    SetZoomVirtual(zoomVirtual);

//    DBG_OUT("DisplayModel::Relayout(), pageCount=%d, zoomReal=%.6f, zoomVirtual=%.2f\n", pageCount, dm->zoomReal, dm->zoomVirtual);

    int newViewPortOffsetX = 0;
    if (0 != currZoomReal && INVALID_ZOOM != currZoomReal)
        newViewPortOffsetX = (int)(viewPort.x * _zoomReal / currZoomReal);
    viewPort.x = newViewPortOffsetX;
    /* calculate the position of each page on the canvas, given current zoom,
       rotation, columns parameters. You can think of it as a simple
       table layout i.e. rows with a fixed number of columns. */
    int columns = columnsFromDisplayMode(displayMode());
    int columnMaxWidth[2] = { 0, 0 };
    int pageInARow = 0;
    int rowMaxPageDy = 0;
    for (int pageNo = 1; pageNo <= PageCount(); ++pageNo) {
        PageInfo *pageInfo = GetPageInfo(pageNo);
        if (!pageInfo->shown) {
            assert(0.0 == pageInfo->visibleRatio);
            continue;
        }
        SizeD pageSize = PageSizeAfterRotation(pageNo);
        RectI pos;
        // don't add the full 0.5 for rounding to account for precision errors
        pos.dx = (int)(pageSize.dx * _zoomReal + 0.499);
        pos.dy = (int)(pageSize.dy * _zoomReal + 0.499);

        if (rowMaxPageDy < pos.dy)
            rowMaxPageDy = pos.dy;
        pos.y = currPosY;

        // restart the layout if we detect we need to show scrollbars
        if (!needVScroll && viewPort.dy < currPosY + rowMaxPageDy) {
            needVScroll = true;
            viewPort.dx -= GetSystemMetrics(SM_CXVSCROLL);
            goto RestartLayout;
        }

        if (displayModeShowCover(displayMode()) && pageNo == 1 && columns - pageInARow > 1)
            pageInARow++;
        if (columnMaxWidth[pageInARow] < pos.dx)
            columnMaxWidth[pageInARow] = pos.dx;

        if (!needHScroll && viewPort.dx < padding->left + columnMaxWidth[0] + (columns == 2 ? padding->inBetweenX + columnMaxWidth[1] : 0) + padding->right) {
            needHScroll = true;
            viewPort.dy -= GetSystemMetrics(SM_CYHSCROLL);
            goto RestartLayout;
        }

        pageInfo->pos = pos;

        pageInARow++;
        assert(pageInARow <= columns);
        if (pageInARow == columns) {
            /* starting next row */
            currPosY += rowMaxPageDy + padding->inBetweenY;
            rowMaxPageDy = 0;
            pageInARow = 0;
        }
/*        DBG_OUT("  page = %3d, (x=%3d, y=%5d, dx=%4d, dy=%4d) orig=(dx=%d,dy=%d)\n",
            pageNo, pageInfo->currPos.x, pageInfo->currPos.y,
                    pageInfo->currPos.dx, pageInfo->currPos.dy,
                    (int)pageSize.dx, (int)pageSize.dy); */
    }

    if (pageInARow != 0) {
        /* this is a partial row */
        currPosY += rowMaxPageDy + padding->inBetweenY;
    }
    // restart the layout if we detect we need to show scrollbars
    // (there are some edge cases we can't catch in the above loop)
    const int canvasDy = currPosY + padding->bottom - padding->inBetweenY;
    if (!needVScroll && canvasDy > viewPort.dy) {
        needVScroll = true;
        viewPort.dx -= GetSystemMetrics(SM_CXVSCROLL);
        goto RestartLayout;
    }

    if (columns == 2 && PageCount() == 1) {
        /* don't center a single page over two columns */
        if (displayModeShowCover(displayMode()))
            columnMaxWidth[0] = columnMaxWidth[1];
        else
            columnMaxWidth[1] = columnMaxWidth[0];
    }

    // restart the layout if we detect we need to show scrollbars
    // (there are some edge cases we can't catch in the above loop)
    int canvasDx = padding->left + columnMaxWidth[0] + (columns == 2 ? padding->inBetweenX + columnMaxWidth[1] : 0) + padding->right;
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

    assert(offX >= 0);
    pageInARow = 0;
    int pageOffX = offX + padding->left;
    for (int pageNo = 1; pageNo <= PageCount(); ++pageNo) {
        PageInfo *pageInfo = GetPageInfo(pageNo);
        if (!pageInfo->shown) {
            assert(0.0 == pageInfo->visibleRatio);
            continue;
        }
        // leave first spot empty in cover page mode
        if (displayModeShowCover(displayMode()) && pageNo == 1)
            pageOffX += columnMaxWidth[pageInARow++] + padding->inBetweenX;
        pageInfo->pos.x = pageOffX + (columnMaxWidth[pageInARow] - pageInfo->pos.dx) / 2;
        // center the cover page over the first two spots in non-continuous mode
        if (displayModeShowCover(displayMode()) && pageNo == 1 && !displayModeContinuous(displayMode()))
            pageInfo->pos.x = offX + padding->left + (columnMaxWidth[0] + padding->inBetweenX + columnMaxWidth[1] - pageInfo->pos.dx) / 2;
        // mirror the page layout when displaying a Right-to-Left document
        if (displayR2L && columns > 1)
            pageInfo->pos.x = canvasDx - pageInfo->pos.x - pageInfo->pos.dx;
        pageOffX += columnMaxWidth[pageInARow++] + padding->inBetweenX;
        assert(pageOffX >= 0 && pageInfo->pos.x >= 0);

        if (pageInARow == columns) {
            pageOffX = offX + padding->left;
            pageInARow = 0;
        }
    }

    /* if after resizing we would have blank space on the right due to x offset
       being too much, make x offset smaller so that there's no blank space */
    if (viewPort.dx - (canvasDx - newViewPortOffsetX) > 0)
        viewPort.x = canvasDx - viewPort.dx;

    /* if a page is smaller than drawing area in y axis, y-center the page */
    if (canvasDy < viewPort.dy) {
        int offY = padding->top + (viewPort.dy - canvasDy) / 2;
        DBG_OUT("  offY = %.2f\n", offY);
        assert(offY >= 0.0);
        for (int pageNo = 1; pageNo <= PageCount(); ++pageNo) {
            PageInfo *pageInfo = GetPageInfo(pageNo);
            if (!pageInfo->shown) {
                assert(0.0 == pageInfo->visibleRatio);
                continue;
            }
            pageInfo->pos.y += offY;
            DBG_OUT("  page = %3d, (x=%3d, y=%5d, dx=%4d, dy=%4d)\n",
                pageNo, pageInfo->pos.x, pageInfo->pos.y,
                        pageInfo->pos.dx, pageInfo->pos.dy);
        }
    }

    canvasSize = SizeI(max(canvasDx, viewPort.dx), max(canvasDy, viewPort.dy));
}

void DisplayModel::ChangeStartPage(int startPage)
{
    assert(ValidPageNo(startPage));
    assert(!displayModeContinuous(displayMode()));

    int columns = columnsFromDisplayMode(displayMode());
    _startPage = startPage;
    if (displayModeShowCover(displayMode()) && startPage == 1 && columns > 1)
        startPage--;
    for (int pageNo = 1; pageNo <= PageCount(); pageNo++) {
        PageInfo *pageInfo = GetPageInfo(pageNo);
        if (displayModeContinuous(displayMode()))
            pageInfo->shown = true;
        else if (pageNo >= startPage && pageNo < startPage + columns) {
            //DBG_OUT("DisplayModel::changeStartPage() set page %d as shown\n", pageNo);
            pageInfo->shown = true;
        } else
            pageInfo->shown = false;
        pageInfo->visibleRatio = 0.0;
    }
    Relayout(_zoomVirtual, _rotation);
}

/* Given positions of each page in a large sheet that is continous view and
   coordinates of a current view into that large sheet, calculate which
   parts of each page is visible on the screen.
   Needs to be recalucated after scrolling the view. */
void DisplayModel::RecalcVisibleParts()
{
    assert(pagesInfo);
    if (!pagesInfo)
        return;

//    DBG_OUT("DisplayModel::RecalcVisibleParts() draw area         (x=%3d,y=%3d,dx=%4d,dy=%4d)\n",
//        viewPort.x, viewPort.y, viewPort.dx, viewPort.dy);
    for (int pageNo = 1; pageNo <= PageCount(); ++pageNo) {
        PageInfo *pageInfo = GetPageInfo(pageNo);
        if (!pageInfo->shown) {
            assert(0.0 == pageInfo->visibleRatio);
            continue;
        }

        RectI pageRect = pageInfo->pos;
        RectI visiblePart = pageRect.Intersect(viewPort);

        pageInfo->visibleRatio = 0.0;
        if (!visiblePart.IsEmpty()) {
            assert(pageRect.dx > 0 && pageRect.dy > 0);
            // calculate with floating point precision to prevent an integer overflow
            pageInfo->visibleRatio = 1.0f * visiblePart.dx * visiblePart.dy / ((float)pageRect.dx * pageRect.dy);
        }
        pageInfo->pageOnScreen = pageRect;
        pageInfo->pageOnScreen.Offset(-viewPort.x, -viewPort.y);
    }
}

int DisplayModel::GetPageNoByPoint(PointI pt) 
{
    // no reasonable answer possible, if zoom hasn't been set yet
    if (_zoomReal <= 0)
        return -1;

    for (int pageNo = 1; pageNo <= PageCount(); ++pageNo) {
        PageInfo *pageInfo = GetPageInfo(pageNo);
        assert(0.0 == pageInfo->visibleRatio || pageInfo->shown);
        if (!pageInfo->shown)
            continue;

        if (pageInfo->pageOnScreen.Inside(pt))
            return pageNo;
    }

    return -1;
}

int DisplayModel::GetPageNextToPoint(PointI pt)
{
    if (_zoomReal <= 0)
        return _startPage;

    double maxDist = -1;
    int closest = _startPage;

    for (int pageNo = 1; pageNo <= PageCount(); ++pageNo) {
        PageInfo *pageInfo = GetPageInfo(pageNo);
        assert(0.0 == pageInfo->visibleRatio || pageInfo->shown);
        if (!pageInfo->shown)
            continue;

        if (pageInfo->pageOnScreen.Inside(pt))
            return pageNo;

        double dist = _hypot(pt.x - pageInfo->pageOnScreen.x - 0.5 * pageInfo->pageOnScreen.dx,
                             pt.y - pageInfo->pageOnScreen.y - 0.5 * pageInfo->pageOnScreen.dy);
        if (maxDist < 0 || dist < maxDist) {
            closest = pageNo;
            maxDist = dist;
        }
    }

    return closest;
}

PointI DisplayModel::CvtToScreen(int pageNo, PointD pt)
{
    PageInfo *pageInfo = GetPageInfo(pageNo);
    assert(pageInfo);
    if (!pageInfo)
        return PointI();

    PointD p = engine->Transform(pt, pageNo, _zoomReal, _rotation);
    p.x += 0.5 + pageInfo->pageOnScreen.x;
    p.y += 0.5 + pageInfo->pageOnScreen.y;

    return p.Convert<int>();
}

RectI DisplayModel::CvtToScreen(int pageNo, RectD r)
{
    PointI TL = CvtToScreen(pageNo, r.TL());
    PointI BR = CvtToScreen(pageNo, r.BR());
    return RectI::FromXY(TL, BR);
}

PointD DisplayModel::CvtFromScreen(PointI pt, int pageNo)
{
    if (!ValidPageNo(pageNo))
        pageNo = GetPageNextToPoint(pt);

    const PageInfo *pageInfo = GetPageInfo(pageNo);
    assert(pageInfo);
    if (!pageInfo)
        return PointD();

    PointD p = PointD(pt.x - 0.5 - pageInfo->pageOnScreen.x,
                      pt.y - 0.5 - pageInfo->pageOnScreen.y);
    return engine->Transform(p, pageNo, _zoomReal, _rotation, true);
}

RectD DisplayModel::CvtFromScreen(RectI r, int pageNo)
{
    PointD TL = CvtFromScreen(r.TL(), pageNo);
    PointD BR = CvtFromScreen(r.BR(), pageNo);
    return RectD::FromXY(TL, BR);
}

/* Given position 'x'/'y' in the draw area, returns a structure describing
   a link or NULL if there is no link at this position. */
PageElement *DisplayModel::GetElementAtPos(PointI pt)
{
    int pageNo = GetPageNoByPoint(pt);
    if (!ValidPageNo(pageNo))
        return NULL;

    PointD pos = CvtFromScreen(pt, pageNo);
    return engine->GetElementAtPos(pageNo, pos);
}

bool DisplayModel::IsOverText(PointI pt)
{
    int pageNo = GetPageNoByPoint(pt);
    if (!ValidPageNo(pageNo))
        return false;

    PointD pos = CvtFromScreen(pt, pageNo);
    return textSelection->IsOverGlyph(pageNo, pos.x, pos.y);
}

void DisplayModel::RenderVisibleParts()
{
    int firstVisible = 0;
    int lastVisible = 0;

    for (int pageNo = 1; pageNo <= PageCount(); ++pageNo) {
        PageInfo *pageInfo = GetPageInfo(pageNo);
        if (pageInfo->visibleRatio > 0.0) {
            assert(pageInfo->shown);
            dmCb->RenderPage(pageNo);
            if (0 == firstVisible)
                firstVisible = pageNo;
            lastVisible = pageNo;
        }
    }

    if (gPredictiveRender) {
        // as a trade-off, we don't prerender two pages each in Facing
        // and Book View modes (else 4 of 8 potential request slots would be taken)
        if (lastVisible < PageCount())
            dmCb->RenderPage(lastVisible + 1);
        if (firstVisible > 1)
            dmCb->RenderPage(firstVisible - 1);
    }
}

void DisplayModel::ChangeViewPortSize(SizeI newViewPortSize)
{
    ScrollState ss;

    bool isDocReady = ValidPageNo(_startPage) && _zoomReal != 0;
    if (isDocReady)
        ss = GetScrollState();

    totalViewPortSize = newViewPortSize;
    Relayout(_zoomVirtual, _rotation);

    if (isDocReady) {
        // when fitting to content, let GoToPage do the necessary scrolling
        if (_zoomVirtual != ZOOM_FIT_CONTENT)
            SetScrollState(ss);
        else
            GoToPage(ss.page, 0);
    } else {
        RecalcVisibleParts();
        RenderVisibleParts();
        dmCb->UpdateScrollbars(canvasSize);
    }
}

RectD DisplayModel::GetContentBox(int pageNo, RenderTarget target)
{
    RectD cbox;
    // we cache the contentBox for the View target
    if (Target_View == target) {
        PageInfo *pageInfo = GetPageInfo(pageNo);
        if (pageInfo->contentBox.IsEmpty())
            pageInfo->contentBox = engine->PageContentBox(pageNo);
        cbox = pageInfo->contentBox;
    }
    else
        cbox = engine->PageContentBox(pageNo, target);

    return engine->Transform(cbox, pageNo, _zoomReal, _rotation);
}

/* get the (screen) coordinates of the point where a page's actual
   content begins (relative to the page's top left corner) */
PointI DisplayModel::GetContentStart(int pageNo)
{
    RectD contentBox = GetContentBox(pageNo);
    if (contentBox.IsEmpty())
        return PointI(0, 0);
    return contentBox.TL().Convert<int>();
}

void DisplayModel::GoToPage(int pageNo, int scrollY, bool addNavPt, int scrollX)
{
    assert(ValidPageNo(pageNo));
    if (!ValidPageNo(pageNo))
        return;

    ChmEngine *chmEngine = AsChmEngine();
    if (chmEngine) {
        chmEngine->DisplayPage(pageNo);
        return;
    }

    if (addNavPt)
        AddNavPoint();

    /* in facing mode only start at odd pages (odd because page
       numbering starts with 1, so odd is really an even page) */
    if (!displayModeSingle(displayMode()))
        pageNo = FirstPageInARowNo(pageNo, columnsFromDisplayMode(displayMode()), displayModeShowCover(displayMode()));

    if (!displayModeContinuous(displayMode())) {
        /* in single page mode going to another page involves recalculating
           the size of canvas */
        ChangeStartPage(pageNo);
    } else if (ZOOM_FIT_CONTENT == _zoomVirtual) {
        // make sure that setZoomVirtual uses the correct page to calculate
        // the zoom level for (visibility will be recalculated below anyway)
        for (int i =  PageCount(); i > 0; i--)
            GetPageInfo(i)->visibleRatio = (i == pageNo ? 1.0f : 0);
        Relayout(_zoomVirtual, _rotation);
    }
    //DBG_OUT("DisplayModel::GoToPage(pageNo=%d, scrollY=%d)\n", pageNo, scrollY);
    PageInfo * pageInfo = GetPageInfo(pageNo);

    // intentionally ignore scrollX and scrollY when fitting to content
    if (ZOOM_FIT_CONTENT == _zoomVirtual) {
        // scroll down to where the actual content starts
        PointI start = GetContentStart(pageNo);
        scrollX = start.x;
        scrollY = start.y;
        if (columnsFromDisplayMode(displayMode()) > 1) {
            int lastPageNo = LastPageInARowNo(pageNo, columnsFromDisplayMode(displayMode()), displayModeShowCover(displayMode()), PageCount());
            PointI second = GetContentStart(lastPageNo);
            scrollY = min(scrollY, second.y);
        }
        viewPort.x = scrollX + pageInfo->pos.x - padding->left;
    }
    else if (-1 != scrollX)
        viewPort.x = scrollX;
    // make sure to not display the blank space beside the first page in cover mode
    else if (-1 == scrollX && 1 == pageNo && displayModeShowCover(displayMode()))
        viewPort.x = pageInfo->pos.x - padding->left;

    /* Hack: if an image is smaller in Y axis than the draw area, then we center
       the image by setting pageInfo->currPos.y in RecalcPagesInfo. So we shouldn't
       scroll (adjust viewPort.y) there because it defeats the purpose.
       TODO: is there a better way of y-centering? */
    viewPort.y = scrollY;
    // Move the next page to the top (unless the remaining pages fit onto a single screen)
    if (displayModeContinuous(displayMode()))
        viewPort.y = pageInfo->pos.y - padding->top + scrollY;

    viewPort.x = limitValue(viewPort.x, 0, canvasSize.dx - viewPort.dx);
    viewPort.y = limitValue(viewPort.y, 0, canvasSize.dy - viewPort.dy);

    RecalcVisibleParts();
    RenderVisibleParts();
    dmCb->UpdateScrollbars(canvasSize);
    dmCb->PageNoChanged(pageNo);
    RepaintDisplay();
}

void DisplayModel::ChangeDisplayMode(DisplayMode displayMode)
{
    if (_displayMode == displayMode)
        return;
    if (AsChmEngine())
        return;

    int currPageNo = CurrentPageNo();
    if (displayModeFacing(displayMode) && displayModeShowCover(_displayMode) && currPageNo < PageCount())
        currPageNo++;
    _displayMode = displayMode;
    if (displayModeContinuous(displayMode)) {
        /* mark all pages as shown but not yet visible. The equivalent code
           for non-continuous mode is in DisplayModel::changeStartPage() called
           from DisplayModel::GoToPage() */
        for (int pageNo = 1; pageNo <= PageCount(); pageNo++) {
            PageInfo *pageInfo = &(pagesInfo[pageNo-1]);
            pageInfo->shown = true;
            pageInfo->visibleRatio = 0.0;
        }
        Relayout(_zoomVirtual, _rotation);
    }
    GoToPage(currPageNo, 0);
}

void DisplayModel::SetPresentationMode(bool enable)
{
    _presentationMode = enable;
    if (enable) {
        _presDisplayMode = _displayMode;
        _presZoomVirtual = _zoomVirtual;
        padding = &gPagePaddingPresentation;
        ChangeDisplayMode(DM_SINGLE_PAGE);
        ZoomTo(ZOOM_FIT_PAGE);
    }
    else {
        padding = &gPagePadding;
        if (engine && engine->IsImageCollection())
            padding = &gImagePadding;
        ChangeDisplayMode(_presDisplayMode);
        if (!IsValidZoom(_presZoomVirtual))
            _presZoomVirtual = _zoomVirtual;
        ZoomTo(_presZoomVirtual);
    }
}

/* In continuous mode just scrolls to the next page. In single page mode
   rebuilds the display model for the next page.
   Returns true if advanced to the next page or false if couldn't advance
   (e.g. because already was at the last page) */
bool DisplayModel::GoToNextPage(int scrollY)
{
    int columns = columnsFromDisplayMode(displayMode());
    int currPageNo = CurrentPageNo();
    // Fully display the current page, if the previous page is still visible
    if (ValidPageNo(currPageNo - columns) && PageVisible(currPageNo - columns) && GetPageInfo(currPageNo)->visibleRatio < 1.0) {
        GoToPage(currPageNo, scrollY);
        return true;
    }
    int firstPageInNewRow = FirstPageInARowNo(currPageNo + columns, columns, displayModeShowCover(displayMode()));
//    DBG_OUT("DisplayModel::goToNextPage(scrollY=%d), currPageNo=%d, firstPageInNewRow=%d\n", scrollY, currPageNo, firstPageInNewRow);
    if (firstPageInNewRow > PageCount()) {
        /* we're on a last row or after it, can't go any further */
        return false;
    }
    GoToPage(firstPageInNewRow, scrollY);
    return true;
}

bool DisplayModel::GoToPrevPage(int scrollY)
{
    if (AsChmEngine()) {
        int pageNo = AsChmEngine()->CurrentPageNo();
        if (pageNo > 1)
            GoToPage(pageNo - 1, 0);
        return pageNo > 1;
    }

    int columns = columnsFromDisplayMode(displayMode());
    int currPageNo = CurrentPageNo();
    DBG_OUT("DisplayModel::goToPrevPage(scrollY=%d), currPageNo=%d\n", scrollY, currPageNo);

    PointI top;
    if ((0 == scrollY || -1 == scrollY) && _zoomVirtual == ZOOM_FIT_CONTENT) {
        currPageNo = FirstVisiblePageNo();
        top = GetContentStart(currPageNo);
    }

    PageInfo * pageInfo = GetPageInfo(currPageNo);
    if (_zoomVirtual == ZOOM_FIT_CONTENT && -pageInfo->pageOnScreen.y <= top.y)
        scrollY = 0; // continue, even though the current page isn't fully visible
    else if (max(-pageInfo->pageOnScreen.y, 0) > scrollY && displayModeContinuous(displayMode())) {
        /* the current page isn't fully visible, so show it first */
        GoToPage(currPageNo, scrollY);
        return true;
    }
    int firstPageInNewRow = FirstPageInARowNo(currPageNo - columns, columns, displayModeShowCover(displayMode()));
    if (firstPageInNewRow < 1) {
        /* we're on a first page, can't go back */
        return false;
    }

    // scroll to the bottom of the page
    if (-1 == scrollY)
        scrollY = GetPageInfo(firstPageInNewRow)->pageOnScreen.dy;

    GoToPage(firstPageInNewRow, scrollY);
    return true;
}

bool DisplayModel::GoToLastPage()
{
    DBG_OUT("DisplayModel::goToLastPage()\n");

    int columns = columnsFromDisplayMode(displayMode());
    int currPageNo = CurrentPageNo();
    int newPageNo = PageCount();
    int firstPageInLastRow = FirstPageInARowNo(newPageNo, columns, displayModeShowCover(displayMode()));

    if (currPageNo == firstPageInLastRow) /* are we on the last page already ? */
        return false;
    GoToPage(firstPageInLastRow, 0, true);
    return true;
}

bool DisplayModel::GoToFirstPage()
{
    DBG_OUT("DisplayModel::goToFirstPage()\n");
    if (AsChmEngine()) {
        GoToPage(1, 0, true);
        return true;
    }

    if (displayModeContinuous(displayMode())) {
        if (0 == viewPort.y) {
            return false;
        }
    } else {
        assert(PageShown(_startPage));
        if (1 == _startPage) {
            /* we're on a first page already */
            return false;
        }
    }
    GoToPage(1, 0, true);
    return true;
}

void DisplayModel::ScrollXTo(int xOff)
{
    DBG_OUT("DisplayModel::scrollXTo(xOff=%d)\n", xOff);

    int currPageNo = CurrentPageNo();
    viewPort.x = xOff;
    RecalcVisibleParts();
    dmCb->UpdateScrollbars(canvasSize);

    if (CurrentPageNo() != currPageNo)
        dmCb->PageNoChanged(CurrentPageNo());
    RepaintDisplay();
}

void DisplayModel::ScrollXBy(int dx)
{
    DBG_OUT("DisplayModel::scrollXBy(dx=%d)\n", dx);

    int newOffX = limitValue(viewPort.x + dx, 0, canvasSize.dx - viewPort.dx);
    if (newOffX != viewPort.x)
        ScrollXTo(newOffX);
}

void DisplayModel::ScrollYTo(int yOff)
{
    DBG_OUT("DisplayModel::scrollYTo(yOff=%d)\n", yOff);

    int currPageNo = CurrentPageNo();
    viewPort.y = yOff;
    RecalcVisibleParts();
    RenderVisibleParts();

    int newPageNo = CurrentPageNo();
    if (newPageNo != currPageNo)
        dmCb->PageNoChanged(newPageNo);
    RepaintDisplay();
}

/* Scroll the doc in y-axis by 'dy'. If 'changePage' is TRUE, automatically
   switch to prev/next page in non-continuous mode if we scroll past the edges
   of current page */
void DisplayModel::ScrollYBy(int dy, bool changePage)
{
    PageInfo *      pageInfo;
    int             currYOff = viewPort.y;
    int             newPageNo;
    int             currPageNo;

    DBG_OUT("DisplayModel::scrollYBy(dy=%d, changePage=%d)\n", dy, (int)changePage);
    assert(0 != dy);
    if (0 == dy) return;

    int newYOff = currYOff;

    if (!displayModeContinuous(displayMode()) && changePage) {
        if ((dy < 0) && (0 == currYOff)) {
            if (_startPage > 1) {
                newPageNo = _startPage-1;
                assert(ValidPageNo(newPageNo));
                pageInfo = GetPageInfo(newPageNo);
                newYOff = pageInfo->pos.dy - viewPort.dy;
                if (newYOff < 0)
                    newYOff = 0; /* TODO: center instead? */
                GoToPrevPage(newYOff);
                return;
            }
        }

        /* see if we have to change page when scrolling forward */
        if ((dy > 0) && (_startPage < PageCount())) {
            if (viewPort.y + viewPort.dy >= canvasSize.dy) {
                GoToNextPage(0);
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
    dmCb->UpdateScrollbars(canvasSize);
    newPageNo = CurrentPageNo();
    if (newPageNo != currPageNo)
        dmCb->PageNoChanged(newPageNo);
    RepaintDisplay();
}

void DisplayModel::ZoomTo(float zoomLevel, PointI *fixPt)
{
    if (!IsValidZoom(zoomLevel))
        return;

    ScrollState ss = GetScrollState();

    int centerPage;
    PointD centerPt;
    if (fixPt) {
        centerPage = GetPageNoByPoint(*fixPt);
        if (ValidPageNo(centerPage))
            centerPt = CvtFromScreen(*fixPt, centerPage);
        else
            fixPt = NULL;
    }

    if (ZOOM_FIT_CONTENT == zoomLevel || ZOOM_FIT_PAGE == zoomLevel) {
        // SetScrollState's first call to GoToPage will already scroll to fit
        ss.x = ss.y = -1;
        fixPt = NULL;
    }

    //DBG_OUT("DisplayModel::zoomTo() zoomLevel=%.6f\n", _zoomLevel);
    Relayout(zoomLevel, _rotation);
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

void DisplayModel::ZoomBy(float zoomFactor, PointI *fixPt)
{
    // zoomTo expects a zoomVirtual, so undo the dpiFactor here
    float newZoom = ZoomAbsolute() * zoomFactor;
    newZoom = limitValue(newZoom, ZOOM_MIN, ZOOM_MAX);
    //DBG_OUT("DisplayModel::zoomBy() zoomReal=%.6f, zoomFactor=%.2f, newZoom=%.2f\n", dm->zoomReal, zoomFactor, newZoom);
    ZoomTo(newZoom, fixPt);
}

void DisplayModel::RotateBy(int newRotation)
{
    newRotation = normalizeRotation(newRotation);
    assert(0 != newRotation);
    if (0 == newRotation)
        return;
    newRotation = normalizeRotation(newRotation + _rotation);

    int currPageNo = CurrentPageNo();
    Relayout(_zoomVirtual, newRotation);
    GoToPage(currPageNo, 0);
}

/* Given <region> (in user coordinates ) on page <pageNo>, copies text in that region
 * into a newly allocated buffer (which the caller needs to free()). */
TCHAR *DisplayModel::GetTextInRegion(int pageNo, RectD region)
{
    RectI *coords;
    TCHAR *pageText = engine->ExtractPageText(pageNo, _T("\r\n"), &coords);
    if (!pageText)
        return NULL;

    RectI regionI = region.Round();
    TCHAR *result = SAZA(TCHAR, str::Len(pageText) + 1), *dest = result;
    for (TCHAR *src = pageText; *src; src++) {
        if (*src != '\r' && *src != '\n') {
            RectI rect = coords[src - pageText];
            RectI isect = regionI.Intersect(rect);
            if (!isect.IsEmpty() && 1.0 * isect.dx * isect.dy / (rect.dx * rect.dy) >= 0.3) {
                *dest++ = *src;
                //DBG_OUT("Char: %c : %d; ushort: %hu\n", (char)c, (int)c, (unsigned short)c);
                //DBG_OUT("Found char: %c : %hu; real %c : %hu\n", c, (unsigned short)(unsigned char) c, ln->text[i].c, ln->text[i].c);
            }
        } else if (dest > result && *(dest - 1) != '\n') {
            *dest++ = *src;
            //DBG_OUT("Char: %c : %d; ushort: %hu\n", (char)buf[p], (int)(unsigned char)buf[p], buf[p]);
        }
    }
    *dest = '\0';

    delete[] coords;
    free(pageText);

    return result;
}

// returns true if it was necessary to scroll the display (horizontally or vertically)
bool DisplayModel::ShowResultRectToScreen(TextSel *res)
{
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

    PageInfo *pageInfo = GetPageInfo(res->pages[0]);
    int sx = 0, sy = 0;

    // vertically, we try to position the search result between 40%
    // (scrolling up) and 60% (scrolling down) of the screen, so that
    // the search direction remains obvious and we still display some
    // context before and after the found text
    if (extremes.y < viewPort.dy * 2 / 5)
        sy = extremes.y - viewPort.dy * 2 / 5;
    else if (extremes.y + extremes.dy > viewPort.dy * 3 / 5)
        sy = min(extremes.y + extremes.dy - viewPort.dy * 3 / 5,
                 extremes.y + extremes.dy / 2 - viewPort.dy * 2 / 5);

    // horizontally, we try to position the search result at the
    // center of the screen, but don't scroll further than page
    // boundaries, so that as much context as possible remains visible
    if (extremes.x < 0)
        sx = max(extremes.x + extremes.dx / 2 - viewPort.dx / 2,
        pageInfo->pageOnScreen.x);
    else if (extremes.x + extremes.dx >= viewPort.dx)
        sx = min(extremes.x + extremes.dx / 2 - viewPort.dx / 2,
                 pageInfo->pageOnScreen.x + pageInfo->pageOnScreen.dx - viewPort.dx);

    if (sx != 0)
        ScrollXBy(sx);
    if (sy != 0)
        ScrollYBy(sy, false);

    return sx != 0 || sy != 0;
}

ScrollState DisplayModel::GetScrollState()
{
    ScrollState state(FirstVisiblePageNo(), -1, -1);
    if (!ValidPageNo(state.page))
        state.page = CurrentPageNo();

    PageInfo *pageInfo = GetPageInfo(state.page);
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

void DisplayModel::SetScrollState(ScrollState state)
{
    // Update the internal metrics first
    GoToPage(state.page, 0);
    // Bail out, if the page wasn't scrolled
    if (state.x < 0 && state.y < 0)
        return;
    // TODO: allow to set scroll state for ChmEngine
    if (AsChmEngine())
        return;

    PointD newPtD(max(state.x, 0), max(state.y, 0));
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

/* Records the current scroll state for later navigating back to. */
void DisplayModel::AddNavPoint(bool keepForward)
{
    ScrollState ss = GetScrollState();

    if (!keepForward && navHistoryIx > 0 && !memcmp(&ss, &navHistory[navHistoryIx - 1], sizeof(ss))) {
        // don't add another point to exact the same position (so overwrite instead of append)
        navHistoryIx--;
    }
    else if (NAV_HISTORY_LEN == navHistoryIx) {
        memmove(navHistory, navHistory + 1, (NAV_HISTORY_LEN - 1) * sizeof(ScrollState));
        navHistoryIx--;
    }
    navHistory[navHistoryIx] = ss;
    navHistoryIx++;
    if (!keepForward || navHistoryIx > navHistoryEnd)
        navHistoryEnd = navHistoryIx;
}

bool DisplayModel::CanNavigate(int dir) const
{
    if (AsChmEngine())
        return AsChmEngine()->CanNavigate(dir);
    return navHistoryIx + dir >= 0 && navHistoryIx + dir < navHistoryEnd && (navHistoryIx != NAV_HISTORY_LEN || navHistoryIx + dir != 0);
}

/* Navigates |dir| steps forward or backwards. */
void DisplayModel::Navigate(int dir)
{
    if (!CanNavigate(dir))
        return;
    if (AsChmEngine())
        return AsChmEngine()->Navigate(dir);
    AddNavPoint(true);
    navHistoryIx += dir - 1; // -1 because adding a nav point increases the index
    if (dir != 0 && navHistory[navHistoryIx].page != 0)
        SetScrollState(navHistory[navHistoryIx]);
}

void DisplayModel::CopyNavHistory(DisplayModel& orig)
{
    navHistoryIx = orig.navHistoryIx;
    navHistoryEnd = orig.navHistoryEnd;
    // copy navigation history for all (still valid) pages over
    for (int i = 0; i < navHistoryEnd; i++) {
        if (ValidPageNo(orig.navHistory[i].page))
            navHistory[i] = orig.navHistory[i];
        else {
            if (i <= navHistoryIx)
                navHistoryIx--;
            navHistoryEnd--;
            i--;
        }
    }
}

DisplayModel *DisplayModel::CreateFromFileName(const TCHAR *fileName, DisplayModelCallback *cb)
{
    DisplayModel *dm = new DisplayModel(cb);
    if (!dm || !dm->Load(fileName)) {
        delete dm;
        return NULL;
    }

//    DBG_OUT("DisplayModel::CreateFromFileName() pageCount = %d, startPage=%d, displayMode=%d\n",
//        dm->PageCount(), startPage, (int)displayMode);
    return dm;
}

ChmEngine *DisplayModel::AsChmEngine() const
{
    if (Engine_Chm != engineType)
        return NULL;

    return static_cast<ChmEngine*>(engine);
}
