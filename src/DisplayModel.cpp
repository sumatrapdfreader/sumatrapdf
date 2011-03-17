/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

/* How to think of display logic: physical screen of size
   drawAreaSize is a window into (possibly much larger)
   total area (canvas) of size canvasSize.

   In DM_SINGLE_PAGE mode total area is the size of currently displayed page
   given current zoomLovel and rotation.
   In DM_CONTINUOUS mode total area consist of all pages rendered sequentially
   with a given zoomLevel and rotation. totalAreaDy is the sum of heights
   of all pages plus spaces between them and totalAreaDx is the size of
   the widest page.

   A possible configuration could look like this:

 -----------------------------------
 |                                 |
 |          -------------          |
 |          | screen    |          |
 |          | i.e.      |          |
 |          | drawArea  |          |
 |          -------------          |
 |                                 |
 |                                 |
 |    canvas                       |
 |                                 |
 |                                 |
 |                                 |
 |                                 |
 -----------------------------------

  We calculate the total area size and position of each page we display on the
  canvas. Canvas has to be >= draw area.

  Changing zoomLevel or rotation requires recalculation of total area and
  position of pdf pages in it.

  We keep the offset of draw area relative to total area. The offset changes
  due to scrolling (with keys or using scrollbars).

  To draw we calculate which part of each page overlaps draw area, we render
  those pages to a bitmap and display those bitmaps.
*/

#include "BaseUtil.h"
#include "WindowInfo.h"
#include "DisplayModel.h"
#include "Resource.h"
#include "WinUtil.h"
#include "FileUtil.h"

#ifndef DEBUG
#define PREDICTIVE_RENDER 1
#endif

DisplaySettings gDisplaySettings = {
  PADDING_PAGE_BORDER_TOP_DEF,
  PADDING_PAGE_BORDER_BOTTOM_DEF,
  PADDING_PAGE_BORDER_LEFT_DEF,
  PADDING_PAGE_BORDER_RIGHT_DEF,
  PADDING_BETWEEN_PAGES_X_DEF,
  PADDING_BETWEEN_PAGES_Y_DEF
}, gDisplaySettingsPresentation = {
  0, 0, 0, 0,
  PADDING_BETWEEN_PAGES_X_DEF, PADDING_BETWEEN_PAGES_X_DEF
};

bool displayModeFacing(DisplayMode displayMode)
{
    return DM_FACING == displayMode || DM_CONTINUOUS_FACING == displayMode || displayModeShowCover(displayMode);
}

bool displayModeContinuous(DisplayMode displayMode)
{
    return DM_CONTINUOUS == displayMode || DM_CONTINUOUS_FACING == displayMode || DM_CONTINUOUS_BOOK_VIEW == displayMode;
}

bool displayModeShowCover(DisplayMode displayMode)
{
    return DM_BOOK_VIEW == displayMode || DM_CONTINUOUS_BOOK_VIEW == displayMode;
}

int columnsFromDisplayMode(DisplayMode displayMode)
{
    if (displayModeFacing(displayMode))
        return 2;
    return 1;
}

bool rotationFlipped(int rotation)
{
    normalizeRotation(&rotation);
    assert(validRotation(rotation));
    if (90 == rotation || 270 == rotation)
        return true;
    return false;
}

bool DisplayModel::displayStateFromModel(DisplayState *ds)
{
    bool presMode = getPresentationMode();

    if (!ds->filePath || !Str::Eq(ds->filePath, fileName())) {
        TCHAR *filePath = Str::Dup(fileName());
        if (!filePath)
            return false;

        free(ds->filePath);
        ds->filePath = filePath;
    }

    ds->displayMode = presMode ? _presDisplayMode : displayMode();
    ds->rotation = _rotation;
    ds->zoomVirtual = presMode ? _presZoomVirtual : _zoomVirtual;

    ScrollState ss;
    getScrollState(&ss);
    ds->pageNo = ss.page;
    if (presMode)
        ds->scrollPos = PointI();
    else
        ds->scrollPos = PointD(ss.x, ss.y).Convert<int>();

    free(ds->decryptionKey);
    ds->decryptionKey = pdfEngine->getDecryptionKey();

    return true;
}

/* Given 'pageInfo', which should contain correct information about
   pageDx, pageDy and rotation, return a page size after applying a global
   rotation */
static void pageSizeAfterRotation(PdfPageInfo *pageInfo, int rotation,
    SizeD *pageSize, bool fitToContent=false)
{
    assert(pageInfo && pageSize);
    if (!pageInfo || !pageSize)
        return;

    if (fitToContent) {
        if (fz_isemptybbox(pageInfo->contentBox))
            return pageSizeAfterRotation(pageInfo, rotation, pageSize);
        pageSize->dx = pageInfo->contentBox.x1 - pageInfo->contentBox.x0;
        pageSize->dy = pageInfo->contentBox.y1 - pageInfo->contentBox.y0;
    } else
        *pageSize = pageInfo->page;

    rotation += pageInfo->rotation;
    if (rotationFlipped(rotation))
        swap(pageSize->dx, pageSize->dy);
}

int limitValue(int val, int min, int max)
{
    assert(max >= min);
    if (val < min)
        return min;
    if (val > max)
        return max;
    return val;
}

/* given 'columns' and an absolute 'pageNo', return the number of the first
   page in a row to which a 'pageNo' belongs e.g. if 'columns' is 2 and we
   have 5 pages in 3 rows (depending on showCover):

   Pages   Result           Pages   Result
   (1,2)   1                  (1)   1
   (3,4)   3                (2,3)   2
   (5)     5                (4,5)   4
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

DisplayModel::DisplayModel(DisplayMode displayMode, int dpi)
{
    _displayMode = displayMode;
    _presDisplayMode = displayMode;
    _presZoomVirtual = INVALID_ZOOM;
    _rotation = INVALID_ROTATION;
    _zoomVirtual = INVALID_ZOOM;
    _padding = &gDisplaySettings;
    _presentationMode = false;
    _zoomReal = INVALID_ZOOM;
    _dpiFactor = dpi / PDF_FILE_DPI;
    _startPage = INVALID_PAGE_NO;
    _appData = NULL;
    pdfEngine = NULL;
    textSelection = NULL;
    _pdfSearch = NULL;
    _pagesInfo = NULL;

    _navHistory = SAZA(ScrollState, NAV_HISTORY_LEN);
    _navHistoryIx = 0;
    _navHistoryEnd = 0;
    
    _dontRenderFlag = false;
}

DisplayModel::~DisplayModel()
{
    _dontRenderFlag = true;
    clearAllRenderings();

    free(_pagesInfo);
    free(_navHistory);
    delete _pdfSearch;
    delete textSelection;
    delete pdfEngine;
}

PdfPageInfo *DisplayModel::getPageInfo(int pageNo) const
{
    if (!validPageNo(pageNo))
        return NULL;
    assert(validPageNo(pageNo));
    assert(_pagesInfo);
    if (!_pagesInfo) return NULL;
    return &(_pagesInfo[pageNo-1]);
}

bool DisplayModel::load(const TCHAR *fileName, int startPage, WindowInfo *win)
{ 
    assert(fileName);
    pdfEngine = PdfEngine::CreateFromFileName(fileName, win);
    if (!pdfEngine)
        return false;

    _appData = win;
    setTotalDrawAreaSize(win->winSize());

    if (validPageNo(startPage))
        _startPage = startPage;
    else
        _startPage = 1;

    const char *pageLayoutName = pdfEngine->getPageLayoutName();
    if (DM_AUTOMATIC == _displayMode) {
        if (Str::EndsWith(pageLayoutName, "Right"))
            _displayMode = DM_CONTINUOUS_BOOK_VIEW;
        else if (Str::StartsWith(pageLayoutName, "Two"))
            _displayMode = DM_CONTINUOUS_FACING;
        else
            _displayMode = DM_CONTINUOUS;
    }
    _displayR2L = pdfEngine->isDocumentDirectionR2L();

    if (!buildPagesInfo())
        return false;

    textSelection = new PdfSelection(pdfEngine);
    _pdfSearch = new PdfSearch(pdfEngine, win);
    return true;
}

bool DisplayModel::buildPagesInfo(void)
{
    assert(!_pagesInfo);
    int _pageCount = pageCount();

    _pagesInfo = SAZA(PdfPageInfo, _pageCount);
    if (!_pagesInfo)
        return false;

    int columns = columnsFromDisplayMode(_displayMode);
    int startPage = _startPage;
    if (displayModeShowCover(_displayMode) && startPage == 1 && columns > 1)
        startPage--;
    for (int pageNo = 1; pageNo <= _pageCount; pageNo++) {
        PdfPageInfo *pageInfo = getPageInfo(pageNo);
        pageInfo->page = pdfEngine->pageSize(pageNo);
        pageInfo->rotation = pdfEngine->pageRotation(pageNo);
        pageInfo->visible = 0;
        pageInfo->shown = false;
        if (displayModeContinuous(_displayMode)) {
            pageInfo->shown = true;
        } else if (pageNo >= startPage && pageNo < startPage + columns) {
            DBG_OUT("DisplayModelSplash::CreateFromPdfDoc() set page %d as shown\n", pageNo);
            pageInfo->shown = true;
        }
    }
    return true;
}

bool DisplayModel::pageShown(int pageNo)
{
    PdfPageInfo *pageInfo = getPageInfo(pageNo);
    if (!pageInfo)
        return false;
    return pageInfo->shown;
}

bool DisplayModel::pageVisible(int pageNo)
{
    PdfPageInfo *pageInfo = getPageInfo(pageNo);
    if (!pageInfo)
        return false;
    return pageInfo->visible > 0;
}

/* Return true if a page is visible or a page in a row below or above is visible */
bool DisplayModel::pageVisibleNearby(int pageNo)
{
    DisplayMode mode = displayMode();
    int columns = columnsFromDisplayMode(mode);

    pageNo = FirstPageInARowNo(pageNo, columns, displayModeShowCover(mode));
    for (int i = pageNo - columns; i < pageNo + 2 * columns - 1; i++)
        if (validPageNo(i) && pageVisible(i))
            return true;

    return false;
}

/* Return true if the first page is fully visible and alone on a line in
   show cover mode (i.e. it's not possible to flip to a previous page) */
bool DisplayModel::firstBookPageVisible()
{
    if (!displayModeShowCover(displayMode()))
        return false;
    if (currentPageNo() != 1)
        return false;
    return true;
}

/* Return true if the last page is fully visible and alone on a line in
   facing or show cover mode (i.e. it's not possible to flip to a next page) */
bool DisplayModel::lastBookPageVisible()
{
    int count = pageCount();
    DisplayMode mode = displayMode();
    if (!displayModeFacing(mode))
        return false;
    if (currentPageNo() == count)
        return true;
    if (getPageInfo(count)->visible < 1.0)
        return false;
    if (FirstPageInARowNo(count, columnsFromDisplayMode(mode),
                          displayModeShowCover(mode)) < count)
        return false;
    return true;
}

/* Given a zoom level that can include a "virtual" zoom levels like ZOOM_FIT_WIDTH,
   ZOOM_FIT_PAGE or ZOOM_FIT_CONTENT, calculate an absolute zoom level */
float DisplayModel::zoomRealFromVirtualForPage(float zoomVirtual, int pageNo)
{
    if (zoomVirtual != ZOOM_FIT_WIDTH && zoomVirtual != ZOOM_FIT_PAGE && zoomVirtual != ZOOM_FIT_CONTENT)
        return zoomVirtual * 0.01f * this->_dpiFactor;

    SizeD row;
    PdfPageInfo *pageInfo = getPageInfo(pageNo);
    int columns = columnsFromDisplayMode(displayMode());

    bool fitToContent = (ZOOM_FIT_CONTENT == zoomVirtual);
    if (fitToContent && columns > 1) {
        // Fit the content of all the pages in the same row into the visible area
        // (i.e. don't crop inner margins but just the left-most, right-most, etc.)
        int first = FirstPageInARowNo(pageNo, columns, displayModeShowCover(displayMode()));
        int last = LastPageInARowNo(pageNo, columns, displayModeShowCover(displayMode()), pageCount());
        for (int i = first; i <= last; i++) {
            SizeD pageSize;
            pageInfo = getPageInfo(i);
            if (fz_isemptybbox(pageInfo->contentBox))
                pageInfo->contentBox = pdfEngine->pageContentBox(i);
            pageSizeAfterRotation(pageInfo, _rotation, &pageSize);
            row.dx += pageSize.dx;
            if (i == first && !fz_isemptybbox(pageInfo->contentBox)) {
                if (rotationFlipped(pageInfo->rotation + _rotation))
                    row.dx -= pageInfo->contentBox.y0 - pdfEngine->pageMediabox(first).y0;
                else
                    row.dx -= pageInfo->contentBox.x0 - pdfEngine->pageMediabox(first).x0;
            }
            if (i == last && !fz_isemptybbox(pageInfo->contentBox)) {
                if (rotationFlipped(pageInfo->rotation + _rotation))
                    row.dx -= pdfEngine->pageMediabox(last).y1 - pageInfo->contentBox.y1;
                else
                    row.dx -= pdfEngine->pageMediabox(last).x1 - pageInfo->contentBox.x1;
            }
            pageSizeAfterRotation(pageInfo, _rotation, &pageSize, true);
            if (row.dy < pageSize.dy)
                row.dy = pageSize.dy;
        }
    }
    else {
        if (fitToContent && fz_isemptybbox(pageInfo->contentBox))
            pageInfo->contentBox = pdfEngine->pageContentBox(pageNo);
        pageSizeAfterRotation(pageInfo, _rotation, &row, fitToContent);
        row.dx *= columns;
    }

    assert(0 != (int)row.dx);
    assert(0 != (int)row.dy);

    int areaForPagesDx = drawAreaSize.dx - _padding->pageBorderLeft - _padding->pageBorderRight - _padding->betweenPagesX * (columns - 1);
    int areaForPagesDy = drawAreaSize.dy - _padding->pageBorderTop - _padding->pageBorderBottom;
    if (areaForPagesDx <= 0 || areaForPagesDy <= 0)
        return 0;

    float zoomX = areaForPagesDx / (float)row.dx;
    float zoomY = areaForPagesDy / (float)row.dy;
    if (zoomX < zoomY || ZOOM_FIT_WIDTH == zoomVirtual)
        return zoomX;
    return zoomY;
}

int DisplayModel::firstVisiblePageNo(void) const
{
    assert(_pagesInfo);
    if (!_pagesInfo) return INVALID_PAGE_NO;

    for (int pageNo = 1; pageNo <= pageCount(); ++pageNo) {
        PdfPageInfo *pageInfo = getPageInfo(pageNo);
        if (pageInfo->visible)
            return pageNo;
    }
    
    /* If no pages are visible */
    return INVALID_PAGE_NO;
}

int DisplayModel::currentPageNo(void) const
{
    if (!displayModeContinuous(displayMode()))
        return _startPage;

    assert(_pagesInfo);
    if (!_pagesInfo) return INVALID_PAGE_NO;
    // determine the most visible page
    int mostVisiblePage = INVALID_PAGE_NO;
    float ratio = 0;

    for (int pageNo = 1; pageNo <= pageCount(); pageNo++) {
        PdfPageInfo *pageInfo = getPageInfo(pageNo);
        if (pageInfo->visible > ratio) {
            mostVisiblePage = pageNo;
            ratio = pageInfo->visible;
        }
    }

    /* if no page is visible, default to either the first or the last one */
    if (INVALID_PAGE_NO == mostVisiblePage) {
        PdfPageInfo *pageInfo = getPageInfo(1);
        if (this->areaOffset.y > pageInfo->currPos.y + pageInfo->currPos.dy)
            mostVisiblePage = pageCount();
        else
            mostVisiblePage = 1;
    }

    return mostVisiblePage;
}

void DisplayModel::setZoomVirtual(float zoomVirtual)
{
    assert(ValidZoomVirtual(zoomVirtual));
    _zoomVirtual = zoomVirtual;

    if ((ZOOM_FIT_WIDTH == zoomVirtual) || (ZOOM_FIT_PAGE == zoomVirtual)) {
        /* we want the same zoom for all pages, so use the smallest zoom
           across the pages so that the largest page fits. In most PDFs all
           pages are the same size anyway */
        float minZoom = (float)HUGE_VAL;
        for (int pageNo = 1; pageNo <= pageCount(); pageNo++) {
            if (pageShown(pageNo)) {
                float thisPageZoom = zoomRealFromVirtualForPage(zoomVirtual, pageNo);
                if (minZoom > thisPageZoom)
                    minZoom = thisPageZoom;
            }
        }
        assert(minZoom != (float)HUGE_VAL);
        this->_zoomReal = minZoom;
    } else if (ZOOM_FIT_CONTENT == zoomVirtual) {
        float newZoom = zoomRealFromVirtualForPage(zoomVirtual, currentPageNo());
        // limit zooming in to 800% on almost empty pages
        if (newZoom > 8.0)
            newZoom = 8.0;
        // don't zoom in by just a few pixels (throwing away a prerendered page)
        if (newZoom < this->_zoomReal || this->_zoomReal / newZoom < 0.95 ||
            this->_zoomReal < zoomRealFromVirtualForPage(ZOOM_FIT_PAGE, currentPageNo()))
            this->_zoomReal = newZoom;
    } else
        this->_zoomReal = zoomVirtual * 0.01f * this->_dpiFactor;
}

float DisplayModel::zoomReal(int pageNo)
{
    DisplayMode mode = displayMode();
    if (displayModeContinuous(mode))
        return _zoomReal;
    if (!displayModeFacing(mode))
        return zoomRealFromVirtualForPage(_zoomVirtual, pageNo);
    pageNo = FirstPageInARowNo(pageNo, columnsFromDisplayMode(mode), displayModeShowCover(mode));
    if (pageNo == pageCount() || pageNo == 1 && displayModeShowCover(mode))
        return zoomRealFromVirtualForPage(_zoomVirtual, pageNo);
    return min(zoomRealFromVirtualForPage(_zoomVirtual, pageNo), zoomRealFromVirtualForPage(_zoomVirtual, pageNo + 1));
}

/* Given pdf info and zoom/rotation, calculate the position of each page on a
   large sheet that is continous view. Needs to be recalculated when:
     * zoom changes
     * rotation changes
     * switching between display modes
     * navigating to another page in non-continuous mode */
void DisplayModel::relayout(float zoomVirtual, int rotation)
{
    int         pageNo;
    PdfPageInfo*pageInfo = NULL;
    SizeD       pageSize;
    int         totalAreaDx, totalAreaDy;
    int         rowMaxPageDy;
    int         offX;
    int         pageOffX;
    int         pageInARow;
    int         columns;
    int         newAreaOffsetX;
    int       * columnOffsets;

    assert(_pagesInfo);
    if (!_pagesInfo)
        return;

    normalizeRotation(&rotation);
    assert(validRotation(rotation));

    _rotation = rotation;

    int currPosY = _padding->pageBorderTop;
    float currZoomReal = _zoomReal;
    setZoomVirtual(zoomVirtual);

//    DBG_OUT("DisplayModel::relayout(), pageCount=%d, zoomReal=%.6f, zoomVirtual=%.2f\n", pageCount, dm->zoomReal, dm->zoomVirtual);

    if (0 == currZoomReal || INVALID_ZOOM == currZoomReal)
        newAreaOffsetX = 0;
    else
        newAreaOffsetX = (int)(areaOffset.x * _zoomReal / currZoomReal);
    areaOffset.x = newAreaOffsetX;
    /* calculate the position of each page on the canvas, given current zoom,
       rotation, columns parameters. You can think of it as a simple
       table layout i.e. rows with a fixed number of columns. */
    columns = columnsFromDisplayMode(displayMode());
    columnOffsets = SAZA(int, columns);
    pageInARow = 0;
    rowMaxPageDy = 0;
    for (pageNo = 1; pageNo <= pageCount(); ++pageNo) {
        pageInfo = getPageInfo(pageNo);
        if (!pageInfo->shown) {
            assert(!pageInfo->visible);
            continue;
        }
        pageSizeAfterRotation(pageInfo, rotation, &pageSize);
        pageInfo->currPos.dx = (int)(pageSize.dx * _zoomReal + 0.5);
        pageInfo->currPos.dy = (int)(pageSize.dy * _zoomReal + 0.5);

        if (rowMaxPageDy < pageInfo->currPos.dy)
            rowMaxPageDy = pageInfo->currPos.dy;
        pageInfo->currPos.y = currPosY;

        if (displayModeShowCover(displayMode()) && pageNo == 1 && columns - pageInARow > 1)
            pageInARow++;
        if (columnOffsets[pageInARow] < pageInfo->currPos.dx)
            columnOffsets[pageInARow] = pageInfo->currPos.dx;

        pageInARow++;
        assert(pageInARow <= columns);
        if (pageInARow == columns) {
            /* starting next row */
            currPosY += rowMaxPageDy + _padding->betweenPagesY;
            rowMaxPageDy = 0;
            pageInARow = 0;
        }
/*        DBG_OUT("  page = %3d, (x=%3d, y=%5d, dx=%4d, dy=%4d) orig=(dx=%d,dy=%d)\n",
            pageNo, pageInfo->currPos.x, pageInfo->currPos.y,
                    pageInfo->currPos.dx, pageInfo->currPos.dy,
                    (int)pageSize.dx, (int)pageSize.dy); */
    }

    if (pageInARow != 0)
        /* this is a partial row */
        currPosY += rowMaxPageDy + _padding->betweenPagesY;
    if (columns == 2 && pageCount() == 1) {
        /* don't center a single page over two columns */
        if (displayModeShowCover(displayMode()))
            columnOffsets[0] = columnOffsets[1];
        else
            columnOffsets[1] = columnOffsets[0];
    }
    totalAreaDx = _padding->pageBorderLeft + columnOffsets[0] + (columns == 2 ? _padding->betweenPagesX + columnOffsets[1] : 0) + _padding->pageBorderRight;

    /* since pages can be smaller than the drawing area, center them in x axis */
    offX = 0;
    if (totalAreaDx < drawAreaSize.dx) {
        areaOffset.x = 0;
        offX = (drawAreaSize.dx - totalAreaDx) / 2;
        totalAreaDx = drawAreaSize.dx;
    }
    assert(offX >= 0);
    pageInARow = 0;
    pageOffX = offX + _padding->pageBorderLeft;
    for (pageNo = 1; pageNo <= pageCount(); ++pageNo) {
        pageInfo = getPageInfo(pageNo);
        if (!pageInfo->shown) {
            assert(!pageInfo->visible);
            continue;
        }
        // leave first spot empty in cover page mode
        if (displayModeShowCover(displayMode()) && pageNo == 1)
            pageOffX += columnOffsets[pageInARow++] + _padding->betweenPagesX;
        pageInfo->currPos.x = pageOffX + (columnOffsets[pageInARow] - pageInfo->currPos.dx) / 2;
        // center the cover page over the first two spots in non-continuous mode
        if (displayModeShowCover(displayMode()) && pageNo == 1 && !displayModeContinuous(displayMode()))
            pageInfo->currPos.x = offX + _padding->pageBorderLeft + (columnOffsets[0] + _padding->betweenPagesX + columnOffsets[1] - pageInfo->currPos.dx) / 2;
        // mirror the page layout when displaying a Right-to-Left document
        if (_displayR2L && columns > 1)
            pageInfo->currPos.x = totalAreaDx - pageInfo->currPos.x - pageInfo->currPos.dx;
        pageOffX += columnOffsets[pageInARow++] + _padding->betweenPagesX;
        assert(pageOffX >= 0 && pageInfo->currPos.x >= 0);

        if (pageInARow == columns) {
            pageOffX = offX + _padding->pageBorderLeft;
            pageInARow = 0;
        }
    }
    free(columnOffsets);

    /* if after resizing we would have blank space on the right due to x offset
       being too much, make x offset smaller so that there's no blank space */
    if (drawAreaSize.dx - (totalAreaDx - newAreaOffsetX) > 0) {
        newAreaOffsetX = totalAreaDx - drawAreaSize.dx;
        areaOffset.x = newAreaOffsetX;
    }

    /* if a page is smaller than drawing area in y axis, y-center the page */
    totalAreaDy = currPosY + _padding->pageBorderBottom - _padding->betweenPagesY;
    if (totalAreaDy < drawAreaSize.dy) {
        int offY = _padding->pageBorderTop + (drawAreaSize.dy - totalAreaDy) / 2;
        DBG_OUT("  offY = %.2f\n", offY);
        assert(offY >= 0.0);
        totalAreaDy = drawAreaSize.dy;
        for (pageNo = 1; pageNo <= pageCount(); ++pageNo) {
            pageInfo = getPageInfo(pageNo);
            if (!pageInfo->shown) {
                assert(!pageInfo->visible);
                continue;
            }
            pageInfo->currPos.y += offY;
            DBG_OUT("  page = %3d, (x=%3d, y=%5d, dx=%4d, dy=%4d) orig=(dx=%d,dy=%d)\n",
                pageNo, pageInfo->currPos.x, pageInfo->currPos.y,
                        pageInfo->currPos.dx, pageInfo->currPos.dy,
                        (int)pageSize.dx, (int)pageSize.dy);
        }
    }

    _canvasSize = SizeI(totalAreaDx, totalAreaDy);
}

void DisplayModel::changeStartPage(int startPage)
{
    assert(validPageNo(startPage));
    assert(!displayModeContinuous(displayMode()));

    int columns = columnsFromDisplayMode(displayMode());
    _startPage = startPage;
    if (displayModeShowCover(displayMode()) && startPage == 1 && columns > 1)
        startPage--;
    for (int pageNo = 1; pageNo <= pageCount(); pageNo++) {
        PdfPageInfo *pageInfo = getPageInfo(pageNo);
        if (displayModeContinuous(displayMode()))
            pageInfo->shown = true;
        else if (pageNo >= startPage && pageNo < startPage + columns) {
            //DBG_OUT("DisplayModel::changeStartPage() set page %d as shown\n", pageNo);
            pageInfo->shown = true;
        } else
            pageInfo->shown = false;
        pageInfo->visible = 0;
    }
    relayout(_zoomVirtual, _rotation);
}

/* Given positions of each page in a large sheet that is continous view and
   coordinates of a current view into that large sheet, calculate which
   parts of each page is visible on the screen.
   Needs to be recalucated after scrolling the view. */
void DisplayModel::recalcVisibleParts(void)
{
    assert(_pagesInfo);
    if (!_pagesInfo)
        return;

    RectI drawAreaRect(areaOffset, drawAreaSize);

//    DBG_OUT("DisplayModel::recalcVisibleParts() draw area         (x=%3d,y=%3d,dx=%4d,dy=%4d)\n",
//        drawAreaRect.x, drawAreaRect.y, drawAreaRect.dx, drawAreaRect.dy);
    for (int pageNo = 1; pageNo <= pageCount(); ++pageNo) {
        PdfPageInfo *pageInfo = getPageInfo(pageNo);
        if (!pageInfo->shown) {
            assert(!pageInfo->visible);
            continue;
        }

        RectI pageRect = pageInfo->currPos;
        pageInfo->visible = 0;

        RectI intersect = pageRect.Intersect(drawAreaRect);
        if (pageRect.dx > 0 && pageRect.dy > 0 && !intersect.IsEmpty()) {
            // calculate with floating point precision to prevent an integer overflow
            pageInfo->visible = 1.0f * intersect.dx * intersect.dy / ((float)pageRect.dx * pageRect.dy);

            pageInfo->bitmap = intersect;
            pageInfo->bitmap.x -= pageRect.x;
            assert(pageInfo->bitmap.x >= 0);
            pageInfo->bitmap.y -= pageRect.y;
            assert(pageInfo->bitmap.y >= 0);
            pageInfo->screenX = intersect.x - areaOffset.x;
            assert(pageInfo->screenX >= 0);
            assert(pageInfo->screenX <= drawAreaSize.dx);
            pageInfo->screenY = intersect.y - areaOffset.y;
            assert(pageInfo->screenX >= 0);
            assert(pageInfo->screenY <= drawAreaSize.dy);
/*          DBG_OUT("                                  visible page = %d, (x=%3d,y=%3d,dx=%4d,dy=%4d) at (x=%d,y=%d)\n",
                pageNo, pageInfo->bitmap.x, pageInfo->bitmap.y,
                          pageInfo->bitmap.dx, pageInfo->bitmap.dy,
                          pageInfo->screenX, pageInfo->screenY); */
        }

        pageInfo->pageOnScreen = pageRect;
        pageInfo->pageOnScreen.x = pageInfo->pageOnScreen.x - areaOffset.x;
        pageInfo->pageOnScreen.y = pageInfo->pageOnScreen.y - areaOffset.y;
        assert(max(pageInfo->pageOnScreen.x, 0) == pageInfo->screenX || !pageInfo->visible);
        assert(max(pageInfo->pageOnScreen.y, 0) == pageInfo->screenY || !pageInfo->visible);
    }
}

/* Map rectangle <r> on the page <pageNo> to point on the screen. */
bool DisplayModel::rectCvtUserToScreen(int pageNo, RectD *r)
{
    double sx = r->x, ex = r->x + r->dx;
    double sy = r->y, ey = r->y + r->dy;

    if (!cvtUserToScreen(pageNo, &sx, &sy) || !cvtUserToScreen(pageNo, &ex, &ey))
        return false;

    *r = RectD::FromXY(sx, sy, ex, ey);
    return true;
}

fz_rect DisplayModel::rectCvtUserToScreen(int pageNo, fz_rect rect)
{
    double x0 = rect.x0, x1 = rect.x1;
    double y0 = rect.y0, y1 = rect.y1;

    if (!cvtUserToScreen(pageNo, &x0, &y0) || !cvtUserToScreen(pageNo, &x1, &y1))
        return fz_emptyrect;
    
    rect.x0 = (float)MIN(x0, x1); rect.x1 = (float)MAX(x0, x1);
    rect.y0 = (float)MIN(y0, y1); rect.y1 = (float)MAX(y0, y1);
    
    return rect;
}

/* Map rectangle <r> on the page <pageNo> to point on the screen. */
bool DisplayModel::rectCvtScreenToUser(int *pageNo, RectD *r)
{
    double sx = r->x, ex = r->x + r->dx;
    double sy = r->y, ey = r->y + r->dy;

    if (!cvtScreenToUser(pageNo, &sx, &sy) || !cvtScreenToUser(pageNo, &ex, &ey))
        return false;

    *r = RectD::FromXY(sx, sy, ex, ey);
    return true;
}

int DisplayModel::getPageNoByPoint(int x, int y) 
{
    // no reasonable answer possible, if zoom hasn't been set yet
    if (!_zoomReal)
        return POINT_OUT_OF_PAGE;

    for (int pageNo = 1; pageNo <= pageCount(); ++pageNo) {
        PdfPageInfo *pageInfo = getPageInfo(pageNo);
        assert(!pageInfo->visible || pageInfo->shown);
        if (!pageInfo->shown)
            continue;

        if (pageInfo->pageOnScreen.Inside(PointI(x, y)))
            return pageNo;
    }
    return POINT_OUT_OF_PAGE;
}

/* Given position 'x'/'y' in the draw area, returns a structure describing
   a link or NULL if there is no link at this position.
   Note: PdfEngine owns this memory so it should not be changed by the
   caller and caller should not reference it after it has changed.
   TODO: this function is called frequently from UI code so make sure that
         it's fast enough for a decent number of link.
         Possible speed improvement: remember which links are visible after
         scrolling and skip the _Inside test for those invisible.
         Another way: build a list with only those visible, so we don't
         even have to traverse those that are invisible.
   */
pdf_link *DisplayModel::getLinkAtPosition(int x, int y)
{
    int pageNo = INVALID_PAGE_NO;
    double posX = x, posY = y;
    if (!cvtScreenToUser(&pageNo, &posX, &posY))
        return NULL;

    return pdfEngine->getLinkAtPosition(pageNo, (float)posX, (float)posY);
}

int DisplayModel::getPdfLinks(int pageNo, pdf_link **links)
{
    int count = pdfEngine->getPdfLinks(pageNo, links);
    for (int i = 0; i < count; i++)
        (*links)[i].rect = rectCvtUserToScreen(pageNo, (*links)[i].rect);
    return count;
}

bool DisplayModel::isOverText(int x, int y)
{
    int pageNo = INVALID_PAGE_NO;
    double posX = x, posY = y;
    if (!cvtScreenToUser(&pageNo, &posX, &posY))
        return false;

    return textSelection->IsOverGlyph(pageNo, posX, posY);
}

void DisplayModel::renderVisibleParts(void)
{
    int lastVisible = 0;

//    DBG_OUT("DisplayModel::renderVisibleParts()\n");
    for (int pageNo = 1; pageNo <= pageCount(); ++pageNo) {
        PdfPageInfo *pageInfo = getPageInfo(pageNo);
        if (pageInfo->visible) {
            assert(pageInfo->shown);
            StartRenderingPage(pageNo);
            lastVisible = pageNo;
        }
    }

#ifdef PREDICTIVE_RENDER
    // TODO: prerender two pages in Facing and Book View modes?
    if (0 < lastVisible && lastVisible < pageCount())
        StartRenderingPage(lastVisible+1);
    if (lastVisible > 1)
        StartRenderingPage(lastVisible-1);
#endif
}

void DisplayModel::changeTotalDrawAreaSize(SizeI totalDrawAreaSize)
{
    ScrollState ss;

    bool isDocLoaded = getScrollState(&ss);
    setTotalDrawAreaSize(totalDrawAreaSize);
    relayout(_zoomVirtual, _rotation);
    if (isDocLoaded) {
        // when fitting to content, let goToPage do the necessary scrolling
        if (_zoomVirtual != ZOOM_FIT_CONTENT)
            setScrollState(&ss);
        else
            goToPage(ss.page, 0);
    } else {
        recalcVisibleParts();
        renderVisibleParts();
        setScrollbarsState();
    }
}

fz_rect DisplayModel::getContentBox(int pageNo, fz_matrix ctm, RenderTarget target)
{
    fz_bbox cbox;
    // we cache the contentBox for the View target
    if (Target_View == target) {
        PdfPageInfo *pageInfo = getPageInfo(pageNo);
        if (fz_isemptybbox(pageInfo->contentBox))
            pageInfo->contentBox = pdfEngine->pageContentBox(pageNo);
        cbox = pageInfo->contentBox;
    }
    else
        cbox = pdfEngine->pageContentBox(pageNo, target);

    fz_rect rect = fz_bboxtorect(cbox);
    return fz_transformrect(ctm, rect);
}

/* get the (screen) coordinates of the point where a page's actual
   content begins (relative to the page's top left corner) */
void DisplayModel::getContentStart(int pageNo, int *x, int *y)
{
    fz_matrix ctm = pdfEngine->viewctm(pageNo, _zoomReal, _rotation);
    fz_rect contentBox = getContentBox(pageNo, ctm);
    if (fz_isemptyrect(contentBox)) {
        *x = *y = 0;
        return;
    }

    *x = (int)min(contentBox.x0, contentBox.x1);
    *y = (int)min(contentBox.y0, contentBox.y1);
}

void DisplayModel::goToPage(int pageNo, int scrollY, bool addNavPt, int scrollX)
{
    assert(validPageNo(pageNo));
    if (!validPageNo(pageNo))
        return;
    if (addNavPt)
        addNavPoint();

    /* in facing mode only start at odd pages (odd because page
       numbering starts with 1, so odd is really an even page) */
    if (displayModeFacing(displayMode()))
        pageNo = FirstPageInARowNo(pageNo, columnsFromDisplayMode(displayMode()), displayModeShowCover(displayMode()));

    if (!displayModeContinuous(displayMode())) {
        /* in single page mode going to another page involves recalculating
           the size of canvas */
        changeStartPage(pageNo);
    } else if (ZOOM_FIT_CONTENT == _zoomVirtual) {
        // make sure that setZoomVirtual uses the correct page to calculate
        // the zoom level for (visibility will be recalculated below anyway)
        for (int i =  pageCount(); i > 0; i--)
            getPageInfo(i)->visible = (i == pageNo ? 1.0f : 0);
        relayout(_zoomVirtual, _rotation);
    }
    //DBG_OUT("DisplayModel::goToPage(pageNo=%d, scrollY=%d)\n", pageNo, scrollY);
    PdfPageInfo * pageInfo = getPageInfo(pageNo);

    if (-1 == scrollX && 0 == scrollY && ZOOM_FIT_CONTENT == _zoomVirtual) {
        // scroll down to where the actual content starts
        getContentStart(pageNo, &scrollX, &scrollY);
        if (columnsFromDisplayMode(displayMode()) > 1) {
            int lastPageNo = LastPageInARowNo(pageNo, columnsFromDisplayMode(displayMode()), displayModeShowCover(displayMode()), pageCount()), secondX, secondY;
            getContentStart(lastPageNo, &secondX, &secondY);
            scrollY = min(scrollY, secondY);
        }
        areaOffset.x = scrollX + pageInfo->currPos.x - _padding->pageBorderLeft;
    }
    else if (-1 != scrollX)
        areaOffset.x = scrollX;
    // make sure to not display the blank space beside the first page in cover mode
    else if (-1 == scrollX && 1 == pageNo && displayModeShowCover(displayMode()))
        areaOffset.x = pageInfo->currPos.x - _padding->pageBorderLeft;

    /* Hack: if an image is smaller in Y axis than the draw area, then we center
       the image by setting pageInfo->currPos.y in RecalcPagesInfo. So we shouldn't
       scroll (adjust areaOffset.y) there because it defeats the purpose.
       TODO: is there a better way of y-centering? */
    areaOffset.y = scrollY;
    // Move the next page to the top (unless the remaining pages fit onto a single screen)
    if (displayModeContinuous(displayMode()))
        areaOffset.y = pageInfo->currPos.y - _padding->pageBorderTop + scrollY;

    areaOffset.x = limitValue(areaOffset.x, 0, _canvasSize.dx - drawAreaSize.dx);
    areaOffset.y = limitValue(areaOffset.y, 0, _canvasSize.dy - drawAreaSize.dy);

    recalcVisibleParts();
    renderVisibleParts();
    setScrollbarsState();
    pageChanged();
    RepaintDisplay();
}

void DisplayModel::changeDisplayMode(DisplayMode displayMode)
{
    if (_displayMode == displayMode)
        return;

    int currPageNo = currentPageNo();
    _displayMode = displayMode;
    if (displayModeContinuous(displayMode)) {
        /* mark all pages as shown but not yet visible. The equivalent code
           for non-continuous mode is in DisplayModel::changeStartPage() called
           from DisplayModel::goToPage() */
        for (int pageNo = 1; pageNo <= pageCount(); pageNo++) {
            PdfPageInfo *pageInfo = &(_pagesInfo[pageNo-1]);
            pageInfo->shown = true;
            pageInfo->visible = 0;
        }
        relayout(_zoomVirtual, _rotation);
    }
    goToPage(currPageNo, 0);
}

void DisplayModel::setPresentationMode(bool enable)
{
    _presentationMode = enable;
    if (enable) {
        _presDisplayMode = _displayMode;
        _presZoomVirtual = _zoomVirtual;
        _padding = &gDisplaySettingsPresentation;
        changeDisplayMode(DM_SINGLE_PAGE);
        zoomTo(ZOOM_FIT_PAGE);
    }
    else {
        _padding = &gDisplaySettings;
        changeDisplayMode(_presDisplayMode);
        if (!ValidZoomVirtual(_presZoomVirtual))
            _presZoomVirtual = _zoomVirtual;
        zoomTo(_presZoomVirtual);
    }
}

/* In continuous mode just scrolls to the next page. In single page mode
   rebuilds the display model for the next page.
   Returns true if advanced to the next page or false if couldn't advance
   (e.g. because already was at the last page) */
bool DisplayModel::goToNextPage(int scrollY)
{
    int columns = columnsFromDisplayMode(displayMode());
    int currPageNo = currentPageNo();
    // Fully display the current page, if the previous page is still visible
    if (validPageNo(currPageNo - columns) && pageVisible(currPageNo - columns) && getPageInfo(currPageNo)->visible < 1) {
        goToPage(currPageNo, scrollY);
        return true;
    }
    int firstPageInNewRow = FirstPageInARowNo(currPageNo + columns, columns, displayModeShowCover(displayMode()));
//    DBG_OUT("DisplayModel::goToNextPage(scrollY=%d), currPageNo=%d, firstPageInNewRow=%d\n", scrollY, currPageNo, firstPageInNewRow);
    if (firstPageInNewRow > pageCount()) {
        /* we're on a last row or after it, can't go any further */
        return FALSE;
    }
    goToPage(firstPageInNewRow, scrollY);
    return TRUE;
}

bool DisplayModel::goToPrevPage(int scrollY)
{
    int columns = columnsFromDisplayMode(displayMode());
    int currPageNo = currentPageNo();
    DBG_OUT("DisplayModel::goToPrevPage(scrollY=%d), currPageNo=%d\n", scrollY, currPageNo);

    int topX, topY;
    if ((0 == scrollY || -1 == scrollY) && _zoomVirtual == ZOOM_FIT_CONTENT)
        getContentStart(currPageNo, &topX, &topY);

    PdfPageInfo * pageInfo = getPageInfo(currPageNo);
    if (_zoomVirtual == ZOOM_FIT_CONTENT && pageInfo->bitmap.y <= topY)
        scrollY = 0; // continue, even though the current page isn't fully visible
    else if (pageInfo->bitmap.y > scrollY && displayModeContinuous(displayMode())) {
        /* the current page isn't fully visible, so show it first */
        goToPage(currPageNo, scrollY);
        return true;
    }
    int firstPageInNewRow = FirstPageInARowNo(currPageNo - columns, columns, displayModeShowCover(displayMode()));
    if (firstPageInNewRow < 1) {
        /* we're on a first page, can't go back */
        return FALSE;
    }

    // scroll to the bottom of the page
    if (-1 == scrollY)
        scrollY = getPageInfo(firstPageInNewRow)->bitmap.dy;

    goToPage(firstPageInNewRow, scrollY);
    return TRUE;
}

bool DisplayModel::goToLastPage(void)
{
    DBG_OUT("DisplayModel::goToLastPage()\n");

    int columns = columnsFromDisplayMode(displayMode());
    int currPageNo = currentPageNo();
    int newPageNo = pageCount();
    int firstPageInLastRow = FirstPageInARowNo(newPageNo, columns, displayModeShowCover(displayMode()));

    if (currPageNo == firstPageInLastRow) /* are we on the last page already ? */
        return FALSE;
    goToPage(firstPageInLastRow, 0, true);
    return TRUE;
}

bool DisplayModel::goToFirstPage(void)
{
    DBG_OUT("DisplayModel::goToFirstPage()\n");

    if (displayModeContinuous(displayMode())) {
        if (0 == areaOffset.y) {
            return FALSE;
        }
    } else {
        assert(pageShown(_startPage));
        if (1 == _startPage) {
            /* we're on a first page already */
            return FALSE;
        }
    }
    goToPage(1, 0, true);
    return TRUE;
}

void DisplayModel::scrollXTo(int xOff)
{
    DBG_OUT("DisplayModel::scrollXTo(xOff=%d)\n", xOff);
    
    int currPageNo = currentPageNo();
    areaOffset.x = xOff;
    recalcVisibleParts();
    setScrollbarsState();
    
    if (currentPageNo() != currPageNo)
        pageChanged();
    RepaintDisplay();
}

void DisplayModel::scrollXBy(int dx)
{
    DBG_OUT("DisplayModel::scrollXBy(dx=%d)\n", dx);

    int newOffX = limitValue(areaOffset.x + dx, 0, _canvasSize.dx - drawAreaSize.dx);
    if (newOffX != areaOffset.x)
        scrollXTo(newOffX);
}

void DisplayModel::scrollYTo(int yOff)
{
    DBG_OUT("DisplayModel::scrollYTo(yOff=%d)\n", yOff);

    int currPageNo = currentPageNo();
    areaOffset.y = yOff;
    recalcVisibleParts();
    renderVisibleParts();

    int newPageNo = currentPageNo();
    if (newPageNo != currPageNo)
        pageChanged();
    RepaintDisplay();
}

/* Scroll the doc in y-axis by 'dy'. If 'changePage' is TRUE, automatically
   switch to prev/next page in non-continuous mode if we scroll past the edges
   of current page */
void DisplayModel::scrollYBy(int dy, bool changePage)
{
    PdfPageInfo *   pageInfo;
    int             currYOff = areaOffset.y;
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
                assert(validPageNo(newPageNo));
                pageInfo = getPageInfo(newPageNo);
                newYOff = pageInfo->currPos.dy - drawAreaSize.dy;
                if (newYOff < 0)
                    newYOff = 0; /* TODO: center instead? */
                goToPrevPage(newYOff);
                return;
            }
        }

        /* see if we have to change page when scrolling forward */
        if ((dy > 0) && (_startPage < pageCount())) {
            if (areaOffset.y + drawAreaSize.dy >= _canvasSize.dy) {
                goToNextPage(0);
                return;
            }
        }
    }

    newYOff += dy;
    newYOff = limitValue(newYOff, 0, _canvasSize.dy - drawAreaSize.dy);
    if (newYOff == currYOff)
        return;

    currPageNo = currentPageNo();
    areaOffset.y = newYOff;
    recalcVisibleParts();
    renderVisibleParts();
    setScrollbarsState();
    newPageNo = currentPageNo();
    if (newPageNo != currPageNo)
        pageChanged();
    RepaintDisplay();
}

void DisplayModel::zoomTo(float zoomVirtual, POINT *fixPt)
{
    ScrollState ss;
    if (getScrollState(&ss)) {
        ScrollState center;
        if (fixPt) {
            center.x = fixPt->x;
            center.y = fixPt->y;
            if (!cvtScreenToUser(&center.page, &center.x, &center.y))
                fixPt = NULL;
        }

        if (ZOOM_FIT_CONTENT == zoomVirtual)
            // setScrollState's first call to goToPage will already scroll to fit
            ss.x = ss.y = -1;

        //DBG_OUT("DisplayModel::zoomTo() zoomVirtual=%.6f\n", _zoomVirtual);
        relayout(zoomVirtual, _rotation);
        setScrollState(&ss);

        if (fixPt) {
            // scroll so that the fix point remains in the same screen location after zooming
            cvtUserToScreen(center.page, &center.x, &center.y);
            if ((int)(center.x - fixPt->x) != 0)
                scrollXBy((int)(center.x - fixPt->x));
            if ((int)(center.y - fixPt->y) != 0)
                scrollYBy((int)(center.y - fixPt->y), false);
        }
    }
}

void DisplayModel::zoomBy(float zoomFactor, POINT *fixPt)
{
    // zoomTo expects a zoomVirtual, so undo the _dpiFactor here
    float newZoom = 100.0f * _zoomReal / _dpiFactor * zoomFactor;
    //DBG_OUT("DisplayModel::zoomBy() zoomReal=%.6f, zoomFactor=%.2f, newZoom=%.2f\n", dm->zoomReal, zoomFactor, newZoom);
    if (newZoom > ZOOM_MAX)
        newZoom = ZOOM_MAX;
    if (newZoom < ZOOM_MIN)
        newZoom = ZOOM_MIN;
    zoomTo(newZoom, fixPt);
}

void DisplayModel::rotateBy(int newRotation)
{
    normalizeRotation(&newRotation);
    assert(0 != newRotation);
    if (0 == newRotation)
        return;
    assert(validRotation(newRotation));
    if (!validRotation(newRotation))
        return;

    newRotation += _rotation;
    normalizeRotation(&newRotation);
    assert(validRotation(newRotation));
    if (!validRotation(newRotation))
        return;

    int currPageNo = currentPageNo();
    relayout(_zoomVirtual, newRotation);
    goToPage(currPageNo, 0);
}

void DisplayModel::RepaintDisplay()
{
    _appData->RepaintAsync();
}

PdfSel *DisplayModel::Find(PdfSearchDirection direction, TCHAR *text, UINT fromPage)
{
    bool forward = (direction == FIND_FORWARD);
    _pdfSearch->SetDirection(forward);
    if (text != NULL)
        bFoundText = _pdfSearch->FindFirst(fromPage ? fromPage : currentPageNo(), text);
    else
        bFoundText = _pdfSearch->FindNext();

    if (bFoundText)
        return &_pdfSearch->result;
    return NULL;
}

bool DisplayModel::cvtUserToScreen(int pageNo, double *x, double *y)
{
    PdfPageInfo *pageInfo = getPageInfo(pageNo);
    if (!pageInfo)
        return false;

    fz_point p = { (float)*x, (float)*y };
    fz_matrix ctm = pdfEngine->viewctm(pageNo, _zoomReal, _rotation);
    fz_point tp = fz_transformpoint(ctm, p);

    *x = tp.x + 0.5 + pageInfo->currPos.x - areaOffset.x;
    *y = tp.y + 0.5 + pageInfo->currPos.y - areaOffset.y;
    return true;
}

bool DisplayModel::cvtScreenToUser(int *pageNo, double *x, double *y)
{
    *pageNo = getPageNoByPoint((int)*x, (int)*y);
    if (*pageNo == POINT_OUT_OF_PAGE) 
        return false;
    const PdfPageInfo *pageInfo = getPageInfo(*pageNo);

    fz_point p;
    p.x = (float)(*x - 0.5 - pageInfo->currPos.x + areaOffset.x);
    p.y = (float)(*y - 0.5 - pageInfo->currPos.y + areaOffset.y);

    fz_matrix ctm = pdfEngine->viewctm(*pageNo, _zoomReal, _rotation);
    fz_point tp = fz_transformpoint(fz_invertmatrix(ctm), p);

    *x = tp.x;
    *y = tp.y;
    return true;
}

TCHAR *DisplayModel::getLinkPath(pdf_link *link)
{
    TCHAR *path = NULL;
    fz_obj *obj;

    switch (link ? link->kind : -1) {
        case PDF_LURI:
            path = pdf_to_tstr(link->dest);
            break;
        case PDF_LLAUNCH:
            obj = fz_dictgets(link->dest, "Type");
            if (!fz_isname(obj) || !Str::Eq(fz_toname(obj), "Filespec"))
                break;
            obj = fz_dictgets(link->dest, "UF"); 
            if (!fz_isstring(obj))
                obj = fz_dictgets(link->dest, "F"); 

            if (fz_isstring(obj)) {
                path = pdf_to_tstr(obj);
                tstr_trans_chars(path, _T("/"), _T("\\"));
            }
            break;
        case PDF_LACTION:
            obj = fz_dictgets(link->dest, "S");
            if (!fz_isname(obj))
                break;
            if (Str::Eq(fz_toname(obj), "GoToR")) {
                obj = fz_dictgets(link->dest, "F");
                if (fz_isstring(obj)) {
                    path = pdf_to_tstr(obj);
                    tstr_trans_chars(path, _T("/"), _T("\\"));
                }
            }
            break;
    }

    return path;
}

void DisplayModel::goToTocLink(pdf_link* link)
{
    TCHAR *path = NULL;
    if (!link)
        return;
    if (PDF_LURI == link->kind && (path = getLinkPath(link))) {
        if (Str::StartsWithI(path, _T("http:")) || Str::StartsWithI(path, _T("https:")))
            launch_url(path);
        /* else: unsupported uri type */
        free(path);
    }
    else if (PDF_LGOTO == link->kind) {
        goToPdfDest(link->dest);
    }
    else if (PDF_LLAUNCH == link->kind && fz_dictgets(link->dest, "EF")) {
        fz_obj *embeddedList = fz_dictgets(link->dest, "EF");
        fz_obj *embedded = fz_dictgets(embeddedList, "UF");
        if (!embedded)
            embedded = fz_dictgets(embeddedList, "F");
        path = getLinkPath(link);
        if (path && Str::EndsWithI(path, _T(".pdf"))) {
            // open embedded PDF documents in a new window
            ScopedMem<TCHAR> combinedPath(tstr_printf(_T("%s:%d:%d"), fileName(), fz_tonum(embedded), fz_togen(embedded)));
            LoadDocument(combinedPath);
        } else {
            // offer to save other attachments to a file
            fz_buffer *data = pdfEngine->getStreamData(fz_tonum(embedded), fz_togen(embedded));
            if (data) {
                saveStreamAs(data->data, data->len, path);
                fz_dropbuffer(data);
            }
        }
        free(path);
    }
    else if (PDF_LLAUNCH == link->kind && (path = getLinkPath(link))) {
        /* for safety, only handle relative PDF paths and only open them in SumatraPDF */
        if (!Str::StartsWith(path, _T("\\")) && Str::EndsWithI(path, _T(".pdf"))) {
            ScopedMem<TCHAR> basePath(Path::GetDir(fileName()));
            ScopedMem<TCHAR> combinedPath(Path::Join(basePath, path));
            LoadDocument(combinedPath);
        }
        free(path);
    }
    else if (PDF_LNAMED == link->kind) {
        char *name = fz_toname(link->dest);
        if (Str::Eq(name, "NextPage"))
            goToNextPage(0);
        else if (Str::Eq(name, "PrevPage"))
            goToPrevPage(0);
        else if (Str::Eq(name, "FirstPage"))
            goToFirstPage();
        else if (Str::Eq(name, "LastPage"))
            goToLastPage();
        // Adobe Reader extensions to the spec, cf. http://www.tug.org/applications/hyperref/manual.html
        else if (Str::Eq(name, "FullScreen"))
            PostMessage(_appData->hwndFrame, WM_COMMAND, IDM_VIEW_PRESENTATION_MODE, 0);
        else if (Str::Eq(name, "GoBack"))
            navigate(-1);
        else if (Str::Eq(name, "GoForward"))
            navigate(1);
        else if (Str::Eq(name, "Print"))
            PostMessage(_appData->hwndFrame, WM_COMMAND, IDM_PRINT, 0);
    }
    else if (PDF_LACTION == link->kind) {
        char *type = fz_toname(fz_dictgets(link->dest, "S"));
        if (type && Str::Eq(type, "GoToR") && fz_dictgets(link->dest, "D") && (path = getLinkPath(link))) {
            /* for safety, only handle relative PDF paths and only open them in SumatraPDF */
            if (!Str::StartsWith(path, _T("\\")) && Str::EndsWithI(path, _T(".pdf"))) {
                ScopedMem<TCHAR> basePath(Path::GetDir(fileName()));
                ScopedMem<TCHAR> combinedPath(Path::Join(basePath, path));
                // TODO: respect fz_tobool(fz_dictgets(link->dest, "NewWindow"))
                WindowInfo *newWin = LoadDocument(combinedPath);
                if (newWin && newWin->dm)
                    newWin->dm->goToPdfDest(fz_dictgets(link->dest, "D"));
            }
            free(path);
        }
        /* else unsupported action */
    }
}

void DisplayModel::goToPdfDest(fz_obj *dest)
{
    int pageNo = pdfEngine->findPageNo(dest);
    if (pageNo > 0) {
        double scrollY = 0, scrollX = -1;
        fz_obj *obj = fz_arrayget(dest, 1);
        if (Str::Eq(fz_toname(obj), "XYZ")) {
            scrollX = fz_toreal(fz_arrayget(dest, 2));
            scrollY = fz_toreal(fz_arrayget(dest, 3));
            cvtUserToScreen(pageNo, &scrollX, &scrollY);

            // goToPage needs scrolling info relative to the page's top border
            // and the page line's left margin
            PdfPageInfo * pageInfo = getPageInfo(pageNo);
            // TODO: These values are not up-to-date, if the page has not been shown yet
            if (pageInfo->shown) {
                scrollX -= pageInfo->pageOnScreen.x;
                scrollY -= pageInfo->pageOnScreen.y;
            }

            // NULL values for the coordinates mean: keep the current position
            if (fz_isnull(fz_arrayget(dest, 2)))
                scrollX = -1;
            if (fz_isnull(fz_arrayget(dest, 3))) {
                pageInfo = getPageInfo(currentPageNo());
                scrollY = -(pageInfo->pageOnScreen.y - _padding->pageBorderTop);
                scrollY = MAX(scrollY, 0); // Adobe Reader never shows the previous page
            }
        }
        else if (Str::Eq(fz_toname(obj), "FitR")) {
            scrollX = fz_toreal(fz_arrayget(dest, 2)); // left
            scrollY = fz_toreal(fz_arrayget(dest, 5)); // top
            cvtUserToScreen(pageNo, &scrollX, &scrollY);
            // TODO: adjust zoom so that the bottom right corner is also visible?

            // goToPage needs scrolling info relative to the page's top border
            // and the page line's left margin
            PdfPageInfo * pageInfo = getPageInfo(pageNo);
            // TODO: These values are not up-to-date, if the page has not been shown yet
            if (pageInfo->shown) {
                scrollX -= pageInfo->pageOnScreen.x;
                scrollY -= pageInfo->pageOnScreen.y;
            }
        }
        /* // ignore author-set zoom settings (at least as long as there's no way to overrule them)
        else if (Str::Eq(fz_toname(obj), "Fit")) {
            zoomTo(ZOOM_FIT_PAGE);
            _appData->UpdateToolbarState();
        }
        // */
        goToPage(pageNo, (int)scrollY, true, (int)scrollX);
    }
}

void DisplayModel::goToNamedDest(const char *name)
{
    goToPdfDest(pdfEngine->getNamedDest(name));
}

/* Given <region> (in user coordinates ) on page <pageNo>, copies text in that region
 * into a newly allocated buffer (which the caller needs to free()). */
TCHAR *DisplayModel::getTextInRegion(int pageNo, RectD *region)
{
    fz_bbox *coords;
    TCHAR *pageText = pdfEngine->ExtractPageText(pageNo, _T("\n\n"), &coords);
    if (!pageText)
        return NULL;

    RectI regionI = region->Convert<int>();
    TCHAR *result = SAZA(TCHAR, Str::Len(pageText) + 1), *dest = result;
    for (TCHAR *src = pageText; *src; src++) {
        if (*src != '\n') {
            fz_bbox *bbox = &coords[src - pageText];
            RectI rect(bbox->x0, bbox->y0, bbox->x1 - bbox->x0, bbox->y1 - bbox->y0);
            RectI isect = regionI.Intersect(rect);
            if (!isect.IsEmpty() && 1.0 * isect.dx * isect.dy / (rect.dx * rect.dy) >= 0.3) {
                *dest++ = *src;
                //DBG_OUT("Char: %c : %d; ushort: %hu\n", (char)c, (int)c, (unsigned short)c);
                //DBG_OUT("Found char: %c : %hu; real %c : %hu\n", c, (unsigned short)(unsigned char) c, ln->text[i].c, ln->text[i].c);
            }
        } else if (dest > result && *(dest - 1) != '\n') {
            *dest++ = DOS_NEWLINE[0];
            *dest++ = DOS_NEWLINE[1];
            //DBG_OUT("Char: %c : %d; ushort: %hu\n", (char)buf[p], (int)(unsigned char)buf[p], buf[p]);
        }
    }
    *dest = 0;

    free(coords);
    free(pageText);

    return result;
}

/* extract all text from the document (caller needs to free() the result) */
TCHAR *DisplayModel::extractAllText(RenderTarget target)
{
    Str::Str<TCHAR> txt(1024);

    for (int pageNo = 1; pageNo <= pageCount(); pageNo++)
    {
        txt.AppendAndFree(pdfEngine->ExtractPageText(pageNo, DOS_NEWLINE, NULL, target));
    }

    return txt.StealData();
}

// returns true if it was necessary to scroll the display (horizontally or vertically)
bool DisplayModel::ShowResultRectToScreen(PdfSel *res)
{
    if (!res->len)
        return false;

    RectI extremes;
    for (int i = 0; i < res->len; i++) {
        RectD rectD = res->rects[i].Convert<double>();
        rectCvtUserToScreen(res->pages[i], &rectD);
        RectI rect = RectI::FromXY((int)floor(rectD.x), (int)floor(rectD.y), 
                                   (int)ceil(rectD.x + rectD.dx), (int)ceil(rectD.y + rectD.dy));
        extremes = extremes.Union(rect);
    }

    PdfPageInfo *pageInfo = getPageInfo(res->pages[0]);
    int sx = 0, sy = 0;
    //
    // Vertical scroll
    //

    // scroll up to make top side of selection visible
    if (extremes.y < 0) {
        sy = extremes.y - 5;
        if (sy + pageInfo->screenY + pageInfo->bitmap.y < 0)
            sy = -(pageInfo->screenY + pageInfo->bitmap.y);
    }
    // scroll down to make top side of selection visible
    else if (extremes.y >= drawAreaSize.dy) {
        sy = (int)(extremes.y - drawAreaSize.dy + 5);
    }

    // scroll up to make bottom side of selection visible
    // (if selection height fits in visible area)
    if (extremes.y + extremes.dy > drawAreaSize.dy
        && extremes.dy <= drawAreaSize.dy + 5) {
        sy = (int)(extremes.y + extremes.dy - drawAreaSize.dy + 5);
    }

    //
    // Horizontal scroll
    //

    // scroll left to make left side of selection visible
    if (extremes.x < 0) {
        sx = extremes.x - 5;
        if (sx + pageInfo->screenX + pageInfo->bitmap.x < 0)
            sx = -(pageInfo->screenX + pageInfo->bitmap.x);
    }
    // scroll right to make left side of selection visible
    else if (extremes.x >= drawAreaSize.dx) {
        sx = (int)(extremes.x - drawAreaSize.dx + 5);
    }
    // scroll left to make right side of selection visible
    // (if selection width fits in visible area)
    if (extremes.x + extremes.dx > drawAreaSize.dx
        && extremes.dx <= drawAreaSize.dx - 5) {
        sx = (int)(extremes.x + extremes.dx - drawAreaSize.dx + 5);
    }

    if (sx != 0)
        scrollXBy(sx);
    if (sy != 0)
        scrollYBy(sy, false);

    return sx != 0 || sy != 0;
}

bool DisplayModel::getScrollState(ScrollState *state)
{
    state->page = firstVisiblePageNo();
    if (state->page <= 0)
        return false;

    PdfPageInfo *pageInfo = getPageInfo(state->page);
    state->x = max(pageInfo->screenX - pageInfo->bitmap.x, 0);
    state->y = max(pageInfo->screenY - pageInfo->bitmap.y, 0);
    // Shortcut: don't calculate precise positions, if the
    // page wasn't scrolled right/down at all
    if (pageInfo->screenX > 0 && pageInfo->screenY > 0) {
        state->x = state->y = -1;
        return true;
    }
    
    bool result = cvtScreenToUser(&state->page, &state->x, &state->y);
    // Remember to show the margin, if it's currently visible
    if (pageInfo->screenX > 0)
        state->x = -1;
    if (pageInfo->screenY > 0)
        state->y = -1;
    return result;
}

void DisplayModel::setScrollState(ScrollState *state)
{
    // Update the internal metrics first
    goToPage(state->page, 0);
    // Bail out, if the page wasn't scrolled
    if (state->x < 0 && state->y < 0)
        return;

    double newX = max(state->x, 0);
    double newY = max(state->y, 0);
    cvtUserToScreen(state->page, &newX, &newY);

    // Also show the margins, if this has been requested
    if (state->x < 0)
        newX = -1;
    else
        newX += areaOffset.x;
    if (state->y < 0)
        newY = 0;
    goToPage(state->page, (int)newY, false, (int)newX);
}

/* Records the current scroll state for later navigating back to. */
bool DisplayModel::addNavPoint(bool keepForward)
{
    ScrollState ss;
    if (!getScrollState(&ss))
        ss.page = 0; // invalid nav point

    if (!keepForward && _navHistoryIx > 0 && !memcmp(&ss, &_navHistory[_navHistoryIx - 1], sizeof(ss))) {
        // don't add another point to exact the same position (so overwrite instead of append)
        _navHistoryIx--;
    }
    else if (NAV_HISTORY_LEN == _navHistoryIx) {
        memmove(_navHistory, _navHistory + 1, (NAV_HISTORY_LEN - 1) * sizeof(ScrollState));
        _navHistoryIx--;
    }
    _navHistory[_navHistoryIx] = ss;
    _navHistoryIx++;
    if (!keepForward || _navHistoryIx > _navHistoryEnd)
        _navHistoryEnd = _navHistoryIx;

    return ss.page != 0;
}

bool DisplayModel::canNavigate(int dir) const
{
    return _navHistoryIx + dir >= 0 && _navHistoryIx + dir < _navHistoryEnd && (_navHistoryIx != NAV_HISTORY_LEN || _navHistoryIx + dir != 0);
}

/* Navigates |dir| steps forward or backwards. */
void DisplayModel::navigate(int dir)
{
    if (!canNavigate(dir))
        return;
    addNavPoint(true);
    _navHistoryIx += dir - 1; // -1 because adding a nav point increases the index
    if (dir != 0 && _navHistory[_navHistoryIx].page != 0)
        setScrollState(&_navHistory[_navHistoryIx]);
}

DisplayModel *DisplayModel::CreateFromFileName(WindowInfo *win,
    const TCHAR *fileName, DisplayMode displayMode, int startPage)
{
    DisplayModel *dm = new DisplayModel(displayMode, win->dpi);
    if (!dm || !dm->load(fileName, startPage, win)) {
        delete dm;
        return NULL;
    }

//    DBG_OUT("DisplayModel_CreateFromPageTree() pageCount = %d, startPage=%d, displayMode=%d\n",
//        dm->pageCount(), dm->startPage, (int)displayMode);
    return dm;
}
