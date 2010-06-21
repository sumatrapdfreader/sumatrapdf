/* Copyright Krzysztof Kowalczyk 2006-2009
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

#include "SumatraPDF.h"
#include "DisplayModel.h"
#include "tstr_util.h"
#include "strlist_util.h"

#include <assert.h>
#include <stdlib.h>

#ifdef _WIN32
#ifndef DEBUG
#define PREDICTIVE_RENDER 1
#endif
#endif

#define MAX_BITMAPS_CACHED 256

static CRITICAL_SECTION     cacheMutex;
static int cacheMutexInitialized = 0;
static BitmapCacheEntry *   gBitmapCache[MAX_BITMAPS_CACHED] = {0};
static int                  gBitmapCacheCount = 0;

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
    ds->filePath = tstr_dup(fileName());
    if (!ds->filePath)
        return false;

    ds->displayMode = presMode ? _presDisplayMode : displayMode();
    ds->rotation = rotation();
    ds->zoomVirtual = presMode ? _presZoomVirtual : zoomVirtual();
    ds->showToc = _showToc;

    ScrollState ss;
    getScrollState(&ss);
    ds->pageNo = ss.page;
    ds->scrollX = presMode ? 0 : floor(ss.x + 0.5);
    ds->scrollY = presMode ? 0 : floor(ss.y + 0.5);

    return true;
}

/* Given 'pageInfo', which should contain correct information about
   pageDx, pageDy and rotation, return a page size after applying a global
   rotation */
void pageSizeAfterRotation(PdfPageInfo *pageInfo, int rotation,
    double *pageDxOut, double *pageDyOut)
{
    assert(pageInfo && pageDxOut && pageDyOut);
    if (!pageInfo || !pageDxOut || !pageDyOut)
        return;

    *pageDxOut = pageInfo->pageDx;
    *pageDyOut = pageInfo->pageDy;

    rotation += pageInfo->rotation;
    if (rotationFlipped(rotation))
        swap_double(pageDxOut, pageDyOut);
}

double limitValue(double val, double min, double max)
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

DisplayModel::DisplayModel(DisplayMode displayMode, int dpi)
{
    _displayMode = displayMode;
    _rotation = INVALID_ROTATION;
    _zoomVirtual = INVALID_ZOOM;
    _padding = &gDisplaySettings;
    _presentationMode = false;
    // 1 PDF user space unit equals 1/72 inch
    _dpiFactor = dpi * 1.0 / 72.0;
    _showToc = TRUE;
    _startPage = INVALID_PAGE_NO;
    _appData = NULL;
    pdfEngine = NULL;
    _pdfSearch = NULL;
    _pagesInfo = NULL;

    _links = NULL;
    _linksCount = 0;

    _navHistory = (ScrollState *)malloc(NAV_HISTORY_LEN * sizeof(ScrollState));
    _navHistoryIx = 0;
    _navHistoryEnd = 0;
    
    searchHitPageNo = INVALID_PAGE_NO;
    searchState.searchState = eSsNone;
}

DisplayModel::~DisplayModel()
{
    RenderQueue_RemoveForDisplayModel(this);
    BitmapCache_FreeForDisplayModel(this);
    cancelRenderingForDisplayModel(this);

    if (_pagesInfo)
        free(_pagesInfo);
    if (_links)
        free(_links);
    free(_navHistory);
    if (_pdfSearch)
        delete _pdfSearch;
    if (pdfEngine)
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

bool DisplayModel::load(const TCHAR *fileName, int startPage, WindowInfo *win, bool tryrepair)
{ 
    assert(fileName);
    pdfEngine = new PdfEngine();
    if (!pdfEngine->load(fileName, win, tryrepair)) {
        delete pdfEngine;
        return false;
    }

    if (validPageNo(startPage))
        _startPage = startPage;
    else
        _startPage = 1;

    const char *pageLayoutName = pdfEngine->getPageLayoutName();
    if (DM_AUTOMATIC == _displayMode) {
        if (str_endswith(pageLayoutName, "Right"))
            _displayMode = DM_CONTINUOUS_BOOK_VIEW;
        else if (str_startswith(pageLayoutName, "Two"))
            _displayMode = DM_CONTINUOUS_FACING;
        else
            _displayMode = DM_CONTINUOUS;
    }

    if (!buildPagesInfo())
        return false;

    _pdfSearch = new PdfSearch(pdfEngine);
    _pdfSearch->tracker = (PdfSearchTracker *)win;
    return true;
}

bool DisplayModel::buildPagesInfo(void)
{
    assert(!_pagesInfo);
    int _pageCount = pageCount();

    _pagesInfo = (PdfPageInfo*)calloc(1, _pageCount * sizeof(PdfPageInfo));
    if (!_pagesInfo)
        return false;

    int columns = columnsFromDisplayMode(_displayMode);
    int startPage = _startPage;
    if (displayModeShowCover(_displayMode) && startPage == 1 && columns > 1)
        startPage--;
    for (int pageNo = 1; pageNo <= _pageCount; pageNo++) {
        PdfPageInfo *pageInfo = getPageInfo(pageNo);
        SizeD pageSize = pdfEngine->pageSize(pageNo);
        pageInfo->pageDx = pageSize.dx();
        pageInfo->pageDy = pageSize.dy();
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

/* Return true if a page is visible or a page below or above is visible */
bool DisplayModel::pageVisibleNearby(int pageNo)
{
    /* TODO: should it check 2 pages above and below in facing mode? */
    if (pageVisible(pageNo))
        return true;
    if (validPageNo(pageNo-1) && pageVisible(pageNo-1))
        return true;
    if (validPageNo(pageNo+1) && pageVisible(pageNo+1))
        return true;
    return false;
}

/* Given a zoom level that can include a "virtual" zoom levels like ZOOM_FIT_WIDTH
   and ZOOM_FIT_PAGE, calculate an absolute zoom level */
double DisplayModel::zoomRealFromVirtualForPage(double zoomVirtual, int pageNo)
{
    double          zoomX, zoomY, pageDx, pageDy;
    double          areaForPageDx, areaForPageDy;
    int             areaForPageDxInt;
    int             columns;

    if (zoomVirtual != ZOOM_FIT_WIDTH && zoomVirtual != ZOOM_FIT_PAGE)
        return zoomVirtual * this->_dpiFactor;

    pageSizeAfterRotation(getPageInfo(pageNo), rotation(), &pageDx, &pageDy);

    assert(0 != (int)pageDx);
    assert(0 != (int)pageDy);

    columns = columnsFromDisplayMode(displayMode());
    areaForPageDx = (drawAreaSize.dx() - _padding->pageBorderLeft - _padding->pageBorderRight);
    areaForPageDx -= _padding->betweenPagesX * (columns - 1);
    areaForPageDxInt = (int)(areaForPageDx / columns);
    areaForPageDx = (double)areaForPageDxInt;
    areaForPageDy = drawAreaSize.dy() - _padding->pageBorderTop - _padding->pageBorderBottom;

    /* TODO: should use gWinDx if we don't show scrollbarY */
    if (areaForPageDx <= 0 || areaForPageDy <= 0)
        return 0;

    zoomX = (areaForPageDx * 100.0) / (double)pageDx;
    zoomY = (areaForPageDy * 100.0) / (double)pageDy;

    if (ZOOM_FIT_WIDTH == zoomVirtual)
        return zoomX;

    assert(ZOOM_FIT_PAGE == zoomVirtual);
    if (zoomX < zoomY)
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
    double ratio = 0;

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
        if (this->areaOffset.y > pageInfo->currPosY + pageInfo->currDy)
            mostVisiblePage = pageCount();
        else
            mostVisiblePage = 1;
    }

    return mostVisiblePage;
}

void DisplayModel::setZoomVirtual(double zoomVirtual)
{
    int     pageNo;
    double  minZoom = INVALID_BIG_ZOOM;
    double  thisPageZoom;

    assert(ValidZoomVirtual(zoomVirtual));
    _zoomVirtual = zoomVirtual;

    if ((ZOOM_FIT_WIDTH == zoomVirtual) || (ZOOM_FIT_PAGE == zoomVirtual)) {
        /* we want the same zoom for all pages, so use the smallest zoom
           across the pages so that the largest page fits. In most PDFs all
           pages are the same size anyway */
        for (pageNo = 1; pageNo <= pageCount(); pageNo++) {
            if (pageShown(pageNo)) {
                thisPageZoom = zoomRealFromVirtualForPage(this->zoomVirtual(), pageNo);
                if (minZoom > thisPageZoom)
                    minZoom = thisPageZoom;
            }
        }
        assert(minZoom != INVALID_BIG_ZOOM);
        this->_zoomReal = minZoom;
    } else
        this->_zoomReal = zoomVirtual * this->_dpiFactor;
}

double DisplayModel::zoomReal(int pageNo)
{
    DisplayMode mode = displayMode();
    if (displayModeContinuous(mode))
        return _zoomReal;
    if (!displayModeFacing(mode))
        return zoomRealFromVirtualForPage(_zoomVirtual, pageNo);
    pageNo = FirstPageInARowNo(pageNo, columnsFromDisplayMode(mode), displayModeShowCover(mode));
    if (pageNo == pageCount())
        return zoomRealFromVirtualForPage(_zoomVirtual, pageNo);
    return min(zoomRealFromVirtualForPage(_zoomVirtual, pageNo), zoomRealFromVirtualForPage(_zoomVirtual, pageNo + 1));
}

/* Given pdf info and zoom/rotation, calculate the position of each page on a
   large sheet that is continous view. Needs to be recalculated when:
     * zoom changes
     * rotation changes
     * switching between display modes
     * navigating to another page in non-continuous mode */
void DisplayModel::relayout(double zoomVirtual, int rotation)
{
    int         pageNo;
    PdfPageInfo*pageInfo = NULL;
    double      currPosX;
    double      pageDx=0, pageDy=0;
    int         currDxInt, currDyInt;
    double      totalAreaDx, totalAreaDy;
    double      rowMaxPageDy;
    double      offX, offY;
    double      pageOffX;
    int         pageInARow;
    int         columns;
    double      newAreaOffsetX;
    int       * columnOffsets;

    assert(_pagesInfo);
    if (!_pagesInfo)
        return;

    normalizeRotation(&rotation);
    assert(validRotation(rotation));

    _rotation = rotation;

    double currPosY = _padding->pageBorderTop;
    double currZoomReal = _zoomReal;
    setZoomVirtual(zoomVirtual);

//    DBG_OUT("DisplayModel::relayout(), pageCount=%d, zoomReal=%.6f, zoomVirtual=%.2f\n", pageCount, dm->zoomReal, dm->zoomVirtual);

    if (0 == currZoomReal)
        newAreaOffsetX = 0.0;
    else
        newAreaOffsetX = areaOffset.x * _zoomReal / currZoomReal;
    areaOffset.x = newAreaOffsetX;
    /* calculate the position of each page on the canvas, given current zoom,
       rotation, columns parameters. You can think of it as a simple
       table layout i.e. rows with a fixed number of columns. */
    columns = columnsFromDisplayMode(displayMode());
    columnOffsets = (int *)calloc(columns, sizeof(int));
    pageInARow = 0;
    currPosX = _padding->pageBorderLeft;
    rowMaxPageDy = 0;
    for (pageNo = 1; pageNo <= pageCount(); ++pageNo) {
        pageInfo = getPageInfo(pageNo);
        if (!pageInfo->shown) {
            assert(!pageInfo->visible);
            continue;
        }
        pageSizeAfterRotation(pageInfo, rotation, &pageDx, &pageDy);
        currDxInt = (int)(pageDx * _zoomReal * 0.01 + 0.5);
        currDyInt = (int)(pageDy * _zoomReal * 0.01 + 0.5);
        pageInfo->currDx = (double)currDxInt;
        pageInfo->currDy = (double)currDyInt;

        if (rowMaxPageDy < pageInfo->currDy)
            rowMaxPageDy = pageInfo->currDy;
        pageInfo->currPosY = currPosY;

        if (displayModeShowCover(displayMode()) && pageNo == 1 && columns - pageInARow > 1)
            pageInARow++;
        if (columnOffsets[pageInARow] < pageInfo->currDx)
            columnOffsets[pageInARow] = pageInfo->currDx;

        pageInARow++;
        assert(pageInARow <= columns);
        if (pageInARow == columns) {
            /* starting next row */
            currPosY += rowMaxPageDy + _padding->betweenPagesY;
            rowMaxPageDy = 0;
            pageInARow = 0;
        }
/*        DBG_OUT("  page = %3d, (x=%3d, y=%5d, dx=%4d, dy=%4d) orig=(dx=%d,dy=%d)\n",
            pageNo, (int)pageInfo->currPosX, (int)pageInfo->currPosY,
                    (int)pageInfo->currDx, (int)pageInfo->currDy,
                    (int)pageDx, (int)pageDy); */
    }

    if (pageInARow != 0)
        /* this is a partial row */
        currPosY += rowMaxPageDy + _padding->betweenPagesY;
    totalAreaDx = _padding->pageBorderLeft + columnOffsets[0] + (columns == 2 ? _padding->betweenPagesX + columnOffsets[1] : 0) + _padding->pageBorderRight;

    /* since pages can be smaller than the drawing area, center them in x axis */
    offX = 0;
    if (totalAreaDx < drawAreaSize.dx()) {
        areaOffset.x = 0.0;
        offX = (drawAreaSize.dx() - totalAreaDx) / 2.0;
        totalAreaDx = drawAreaSize.dx();
    }
    assert(offX >= 0.0);
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
        pageInfo->currPosX = pageOffX + (columnOffsets[pageInARow] - pageInfo->currDx) / 2;
        // center the cover page over the first two spots in non-continuous mode
        if (displayModeShowCover(displayMode()) && pageNo == 1 && !displayModeContinuous(displayMode()))
            pageInfo->currPosX = offX + _padding->pageBorderLeft + (columnOffsets[0] + _padding->betweenPagesX + columnOffsets[1] - pageInfo->currDx) / 2;
        pageOffX += columnOffsets[pageInARow++] + _padding->betweenPagesX;
        assert(pageOffX >= 0 && pageInfo->currPosX >= 0);

        if (pageInARow == columns) {
            pageOffX = offX + _padding->pageBorderLeft;
            pageInARow = 0;
        }
    }
    free(columnOffsets);

    /* if after resizing we would have blank space on the right due to x offset
       being too much, make x offset smaller so that there's no blank space */
    if (drawAreaSize.dx() - (totalAreaDx - newAreaOffsetX) > 0) {
        newAreaOffsetX = totalAreaDx - drawAreaSize.dx();
        areaOffset.x = newAreaOffsetX;
    }

    /* if a page is smaller than drawing area in y axis, y-center the page */
    totalAreaDy = currPosY + _padding->pageBorderBottom - _padding->betweenPagesY;
    if (totalAreaDy < drawAreaSize.dy()) {
        offY = _padding->pageBorderTop + (drawAreaSize.dy() - totalAreaDy) / 2;
        DBG_OUT("  offY = %.2f\n", offY);
        assert(offY >= 0.0);
        totalAreaDy = drawAreaSize.dy();
        for (pageNo = 1; pageNo <= pageCount(); ++pageNo) {
            pageInfo = getPageInfo(pageNo);
            if (!pageInfo->shown) {
                assert(!pageInfo->visible);
                continue;
            }
            pageInfo->currPosY += offY;
            DBG_OUT("  page = %3d, (x=%3d, y=%5d, dx=%4d, dy=%4d) orig=(dx=%d,dy=%d)\n",
                pageNo, (int)pageInfo->currPosX, (int)pageInfo->currPosY,
                        (int)pageInfo->currDx, (int)pageInfo->currDy,
                        (int)pageDx, (int)pageDy);
        }
    }

    _canvasSize = SizeD(totalAreaDx, totalAreaDy);
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
    relayout(zoomVirtual(), rotation());
}

/* Given positions of each page in a large sheet that is continous view and
   coordinates of a current view into that large sheet, calculate which
   parts of each page is visible on the screen.
   Needs to be recalucated after scrolling the view. */
void DisplayModel::recalcVisibleParts(void)
{
    int             pageNo;
    RectI           drawAreaRect;
    RectI           pageRect;
    RectI           intersect;
    PdfPageInfo*    pageInfo;

    assert(_pagesInfo);
    if (!_pagesInfo)
        return;

    drawAreaRect.x = (int)areaOffset.x;
    drawAreaRect.y = (int)areaOffset.y;
    drawAreaRect.dx = drawAreaSize.dxI();
    drawAreaRect.dy = drawAreaSize.dyI();

//    DBG_OUT("DisplayModel::recalcVisibleParts() draw area         (x=%3d,y=%3d,dx=%4d,dy=%4d)\n",
//        drawAreaRect.x, drawAreaRect.y, drawAreaRect.dx, drawAreaRect.dy);
    for (pageNo = 1; pageNo <= pageCount(); ++pageNo) {
        pageInfo = getPageInfo(pageNo);
        if (!pageInfo->shown) {
            assert(!pageInfo->visible);
            continue;
        }
        pageRect.x = (int)pageInfo->currPosX;
        pageRect.y = (int)pageInfo->currPosY;
        pageRect.dx = (int)pageInfo->currDx;
        pageRect.dy = (int)pageInfo->currDy;
        pageInfo->visible = 0;
        if (pageRect.dx * pageRect.dy > 0 && RectI_Intersect(&pageRect, &drawAreaRect, &intersect)) {
            pageInfo->visible = 1.0 * intersect.dx * intersect.dy / (pageRect.dx * pageRect.dy);
            
            pageInfo->bitmapX = (int) ((double)intersect.x - pageInfo->currPosX);
            assert(pageInfo->bitmapX >= 0);
            pageInfo->bitmapY = (int) ((double)intersect.y - pageInfo->currPosY);
            assert(pageInfo->bitmapY >= 0);
            pageInfo->bitmapDx = intersect.dx;
            pageInfo->bitmapDy = intersect.dy;
            pageInfo->screenX = (int) ((double)intersect.x - areaOffset.x);
            assert(pageInfo->screenX >= 0);
            assert(pageInfo->screenX <= drawAreaSize.dx());
            pageInfo->screenY = (int) ((double)intersect.y - areaOffset.y);
            assert(pageInfo->screenX >= 0);
            assert(pageInfo->screenY <= drawAreaSize.dy());
/*            DBG_OUT("                                  visible page = %d, (x=%3d,y=%3d,dx=%4d,dy=%4d) at (x=%d,y=%d)\n",
                pageNo, pageInfo->bitmapX, pageInfo->bitmapY,
                          pageInfo->bitmapDx, pageInfo->bitmapDy,
                          pageInfo->screenX, pageInfo->screenY); */
        }
    }
}


/* Map rectangle <r> on the page <pageNo> to point on the screen. */
bool DisplayModel::rectCvtUserToScreen(int pageNo, RectD *r)
{
    double          sx, sy, ex, ey;

    sx = r->x;
    sy = r->y;
    ex = r->x + r->dx;
    ey = r->y + r->dy;

    bool ok = cvtUserToScreen(pageNo, &sx, &sy);
    ok = ok && cvtUserToScreen(pageNo, &ex, &ey);
    ok = ok && RectD_FromXY(r, sx, ex, sy, ey);
    return ok;
}

/* Map rectangle <r> on the page <pageNo> to point on the screen. */
bool DisplayModel::rectCvtScreenToUser(int *pageNo, RectD *r)
{
    double          sx, sy, ex, ey;

    sx = r->x;
    sy = r->y;
    ex = r->x + r->dx;
    ey = r->y + r->dy;

    return cvtScreenToUser(pageNo, &sx, &sy)
        && cvtScreenToUser(pageNo, &ex, &ey)
        && RectD_FromXY(r, sx, ex, sy, ey);
}

int DisplayModel::getPageNoByPoint (double x, double y) 
{
    for (int pageNo = 1; pageNo <= pageCount(); ++pageNo) {
        PdfPageInfo *pageInfo = getPageInfo(pageNo);
        if (!pageInfo->visible)
            continue;
        assert(pageInfo->shown);
        if (!pageInfo->shown)
            continue;

        RectI pageOnScreen;
        pageOnScreen.x = pageInfo->screenX;
        pageOnScreen.y = pageInfo->screenY;
        pageOnScreen.dx = pageInfo->bitmapDx;
        pageOnScreen.dy = pageInfo->bitmapDy;

        if (RectI_Inside (&pageOnScreen, (int)x, (int)y))
            return pageNo;
    }
    return POINT_OUT_OF_PAGE;
}

void DisplayModel::recalcSearchHitCanvasPos(void)
{
    int             pageNo;
    RectD           rect;

    pageNo = searchHitPageNo;
    if (INVALID_PAGE_NO == pageNo) return;
    rect = searchHitRectPage;
    rectCvtUserToScreen(pageNo, &rect);
    searchHitRectCanvas.x = (int)rect.x;
    searchHitRectCanvas.y = (int)rect.y;
    searchHitRectCanvas.dx = (int)rect.dx;
    searchHitRectCanvas.dy = (int)rect.dy;
}

/* Recalculates the position of each link on the canvas i.e. applies current
   rotation and zoom level and offsets it by the offset of each page in
   the canvas.
   TODO: applying rotation and zoom level could be split into a separate
         function for speedup, since it only has to change after rotation/zoomLevel
         changes while this function has to be called after each scrolling.
         But I'm not sure if this would be a significant speedup */
void DisplayModel::recalcLinksCanvasPos(void)
{
    PdfLink *       pdfLink;
    PdfPageInfo *   pageInfo;
    int             linkNo;
    RectD           rect;
    int             linkCount = getLinkCount();
    // TODO: calling it here is a bit of a hack
    recalcSearchHitCanvasPos();

    DBG_OUT("DisplayModel::recalcLinksCanvasPos() linkCount=%d\n", linkCount);

    if (0 == linkCount)
        return;

    for (linkNo = 0; linkNo < linkCount; linkNo++) {
        pdfLink = &_links[linkNo];
        pageInfo = getPageInfo(pdfLink->pageNo);
        if (!pageInfo->visible) {
            /* hack: make the links on pages that are not shown invisible by
                     moving it off canvas. A better solution would probably be
                     not adding those links in the first place */
            pdfLink->rectCanvas.x = -100;
            pdfLink->rectCanvas.y = -100;
            pdfLink->rectCanvas.dx = 0;
            pdfLink->rectCanvas.dy = 0;
            continue;
        }

        rect = pdfLink->rectPage;
        rectCvtUserToScreen(pdfLink->pageNo, &rect);

#if 0 // this version is correct but needs to be made generic, not specific to poppler
        /* hack: in PDFs that have a crop-box (like treo700psprint_UG.pdf)
           we need to shift links by the offset of crop-box. Since we do it
           after conversion involving ctm, we need to apply current zoom and
           rotation. This is probably not the best place to be doing this
           but it's the only one we managed to make work */
        double offX = dm->pdfDoc->getCatalog()->getPage(pdfLink->pageNo)->getCropBox()->x1;
        double offY = dm->pdfDoc->getCatalog()->getPage(pdfLink->pageNo)->getCropBox()->y1;
        if (flippedRotation(dm->rotation)) {
            double tmp = offX;
            offX = offY;
            offY = tmp;
        }
        offX = offX * dm->zoomReal * 0.01;
        offY = offY * dm->zoomReal * 0.01;
#else
        pdfLink->rectCanvas.x = (int)rect.x;
        pdfLink->rectCanvas.y = (int)rect.y;
        pdfLink->rectCanvas.dx = (int)rect.dx;
        assert(pdfLink->rectCanvas.dx >= 0);
        pdfLink->rectCanvas.dy = (int)rect.dy;
        assert(pdfLink->rectCanvas.dy >= 0);
#endif
        DBG_OUT("  link on page (x=%d, y=%d, dx=%d, dy=%d),\n",
            (int)pdfLink->rectPage.x, (int)pdfLink->rectPage.y,
            (int)pdfLink->rectPage.dx, (int)pdfLink->rectPage.dy);
        DBG_OUT("        screen (x=%d, y=%d, dx=%d, dy=%d)\n",
                (int)rect.x, (int)rect.y,
                (int)rect.dx, (int)rect.dy);
    }
}

void DisplayModel::clearSearchHit(void)
{
    DBG_OUT("DisplayModel::clearSearchHit()\n");
    searchHitPageNo = INVALID_PAGE_NO;
}

void DisplayModel::setSearchHit(int pageNo, RectD *hitRect)
{
    //DBG_OUT("DisplayModel::setSearchHit() page=%d at pos (%.2f, %.2f)-(%.2f,%.2f)\n", pageNo, xs, ys, xe, ye);
    searchHitPageNo = pageNo;
    searchHitRectPage = *hitRect;
    recalcSearchHitCanvasPos();
}

/* Given position 'x'/'y' in the draw area, returns a structure describing
   a link or NULL if there is no link at this position.
   Note: DisplayModelSplash owns this memory so it should not be changed by the
   caller and caller should not reference it after it has changed (i.e. process
   it immediately since it will become invalid after each _relayout()).
   TODO: this function is called frequently from UI code so make sure that
         it's fast enough for a decent number of link.
         Possible speed improvement: remember which links are visible after
         scrolling and skip the _Inside test for those invisible.
         Another way: build another list with only those visible, so we don't
         even have to traverse those that are invisible.
   */
PdfLink *DisplayModel::linkAtPosition(int x, int y)
{
    int linkCount = getLinkCount();
    if (0 == linkCount) return NULL;

    int canvasPosX = x;
    int canvasPosY = y;
    for (int i = 0; i < linkCount; i++) {
        PdfLink *currLink = &_links[i];

        if (RectI_Inside(&(currLink->rectCanvas), canvasPosX, canvasPosY))
            return currLink;
    }
    return NULL;
}

/* Send the request to render a given page to a rendering thread */
void DisplayModel::startRenderingPage(int pageNo)
{
    RenderQueue_Add(this, pageNo);
}

void DisplayModel::renderVisibleParts(void)
{
    int             pageNo;
    PdfPageInfo*    pageInfo;
    int             lastVisible = 0;

//    DBG_OUT("DisplayModel::renderVisibleParts()\n");
    for (pageNo = 1; pageNo <= pageCount(); ++pageNo) {
        pageInfo = getPageInfo(pageNo);
        if (pageInfo->visible) {
            assert(pageInfo->shown);
            startRenderingPage(pageNo);
            lastVisible = pageNo;
        }
    }
    
#ifdef PREDICTIVE_RENDER
    if (0 != lastVisible && lastVisible != pageCount())
        startRenderingPage(lastVisible+1);
#endif
}

void DisplayModel::changeTotalDrawAreaSize(SizeD totalDrawAreaSize)
{
    ScrollState ss;

    bool isDocLoaded = getScrollState(&ss);
    setTotalDrawAreaSize(totalDrawAreaSize);
    relayout(zoomVirtual(), rotation());
    if (isDocLoaded) {
        setScrollState(&ss);
    } else {
        recalcVisibleParts();
        renderVisibleParts();
        setScrollbarsState();
    }
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
    }
    //DBG_OUT("DisplayModel::goToPage(pageNo=%d, scrollY=%d)\n", pageNo, scrollY);
    PdfPageInfo * pageInfo = getPageInfo(pageNo);
    if (-1 != scrollX)
        areaOffset.x = (double)scrollX;
    // make sure to not display the blank space beside the first page in cover mode
    else if (-1 == scrollX && 1 == pageNo && displayModeShowCover(displayMode()))
        areaOffset.x = pageInfo->currPosX - _padding->pageBorderLeft;

    /* Hack: if an image is smaller in Y axis than the draw area, then we center
       the image by setting pageInfo->currPosY in RecalcPagesInfo. So we shouldn't
       scroll (adjust areaOffset.y) there because it defeats the purpose.
       TODO: is there a better way of y-centering? */
    areaOffset.y = (double)scrollY;
    // Move the next page to the top (unless the remaining pages fit onto a single screen)
    if (displayModeContinuous(displayMode()))
        areaOffset.y = pageInfo->currPosY - _padding->pageBorderTop + (double)scrollY;

    areaOffset.x = limitValue(areaOffset.x, 0, _canvasSize.dx() - drawAreaSize.dx());
    areaOffset.y = limitValue(areaOffset.y, 0, _canvasSize.dy() - drawAreaSize.dy());

    recalcVisibleParts();
    recalcLinksCanvasPos();
    renderVisibleParts();
    setScrollbarsState();
    pageChanged();
    repaintDisplay(true);
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
        relayout(zoomVirtual(), rotation());
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

    PdfPageInfo * pageInfo = getPageInfo(currPageNo);
    if (pageInfo->bitmapY > scrollY && displayModeContinuous(displayMode())) {
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
        scrollY = getPageInfo(firstPageInNewRow)->bitmapDy;

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
    areaOffset.x = (double)xOff;
    recalcVisibleParts();
    recalcLinksCanvasPos();
    setScrollbarsState();
    
    if (currentPageNo() != currPageNo)
        pageChanged();
    repaintDisplay(false);
}

void DisplayModel::scrollXBy(int dx)
{
    DBG_OUT("DisplayModel::scrollXBy(dx=%d)\n", dx);

    double newOffX = limitValue(areaOffset.x + dx, 0, _canvasSize.dx() - drawAreaSize.dx());
    if (newOffX != areaOffset.x)
        scrollXTo((int)newOffX);
}

void DisplayModel::scrollYTo(int yOff)
{
    DBG_OUT("DisplayModel::scrollYTo(yOff=%d)\n", yOff);

    int currPageNo = currentPageNo();
    areaOffset.y = (double)yOff;
    recalcVisibleParts();
    recalcLinksCanvasPos();
    renderVisibleParts();

    int newPageNo = currentPageNo();
    if (newPageNo != currPageNo)
        pageChanged();
    repaintDisplay(false);
}

/* Scroll the doc in y-axis by 'dy'. If 'changePage' is TRUE, automatically
   switch to prev/next page in non-continuous mode if we scroll past the edges
   of current page */
void DisplayModel::scrollYBy(int dy, bool changePage)
{
    PdfPageInfo *   pageInfo;
    int             currYOff = (int)areaOffset.y;
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
                newYOff = (int)pageInfo->currDy - drawAreaSize.dyI();
                if (newYOff < 0)
                    newYOff = 0; /* TODO: center instead? */
                goToPrevPage(newYOff);
                return;
            }
        }

        /* see if we have to change page when scrolling forward */
        if ((dy > 0) && (_startPage < pageCount())) {
            if ((int)areaOffset.y + drawAreaSize.dyI() >= _canvasSize.dyI()) {
                goToNextPage(0);
                return;
            }
        }
    }

    newYOff += dy;
    newYOff = limitValue(newYOff, 0, _canvasSize.dy() - drawAreaSize.dy());
    if (newYOff == currYOff)
        return;

    currPageNo = currentPageNo();
    areaOffset.y = (double)newYOff;
    recalcVisibleParts();
    recalcLinksCanvasPos();
    renderVisibleParts();
    setScrollbarsState();
    newPageNo = currentPageNo();
    if (newPageNo != currPageNo)
        pageChanged();
    repaintDisplay(false);
}

void DisplayModel::zoomTo(double zoomVirtual)
{
    ScrollState ss;
    if (getScrollState(&ss)) {
        //DBG_OUT("DisplayModel::zoomTo() zoomVirtual=%.6f\n", _zoomVirtual);
        relayout(zoomVirtual, rotation());
        setScrollState(&ss);
    }
}

void DisplayModel::zoomBy(double zoomFactor)
{
    // zoomTo expects a zoomVirtual, so undo the _dpiFactor here
    double newZoom = _zoomReal / _dpiFactor * zoomFactor;
    //DBG_OUT("DisplayModel::zoomBy() zoomReal=%.6f, zoomFactor=%.2f, newZoom=%.2f\n", dm->zoomReal, zoomFactor, newZoom);
    if (newZoom > ZOOM_MAX)
        return;
    if (newZoom < ZOOM_MIN)
        return;
    zoomTo(newZoom);
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

    newRotation += rotation();
    normalizeRotation(&newRotation);
    assert(validRotation(newRotation));
    if (!validRotation(newRotation))
        return;

    int currPageNo = currentPageNo();
    relayout(zoomVirtual(), newRotation);
    goToPage(currPageNo, 0);
}

static inline void InitCacheMutext() {
    if (!cacheMutexInitialized) {
        InitializeCriticalSection(&cacheMutex);
        cacheMutexInitialized = 1;
    }
}

void LockCache(void) {
    InitCacheMutext();
    EnterCriticalSection(&cacheMutex);
}

void UnlockCache(void) {
    LeaveCriticalSection(&cacheMutex);
}

static void BitmapCacheEntry_Free(BitmapCacheEntry *entry) {
    assert(entry);
    if (!entry) return;
    delete entry->bitmap;
    free((void*)entry);
}

void BitmapCache_FreeAll(void) {
    LockCache();
    for (int i=0; i < gBitmapCacheCount; i++) {
        BitmapCacheEntry_Free(gBitmapCache[i]);
        gBitmapCache[i] = NULL;
    }
    gBitmapCacheCount = 0;
    UnlockCache();
}

/* Free all bitmaps in the cache that are not visible. Returns true if freed
   at least one item. */
bool BitmapCache_FreeNotVisible(void) {
    LockCache();
    bool freedSomething = false;
    int cacheCount = gBitmapCacheCount;
    int curPos = 0;
    for (int i = 0; i < cacheCount; i++) {
        BitmapCacheEntry* entry = gBitmapCache[i];
        bool shouldFree = !entry->dm->pageVisibleNearby(entry->pageNo);
         if (shouldFree) {
            if (!freedSomething)
                DBG_OUT("BitmapCache_FreeNotVisible() ");
            DBG_OUT("freed %d ", entry->pageNo);
            freedSomething = true;
            BitmapCacheEntry_Free(gBitmapCache[i]);
            gBitmapCache[i] = NULL;
            --gBitmapCacheCount;
        }

        if (curPos != i)
            gBitmapCache[curPos] = gBitmapCache[i];

        if (!shouldFree)
             ++curPos;
    }
    UnlockCache();
    if (freedSomething)
        DBG_OUT("\n");
    return freedSomething;
}

bool BitmapCache_FreePage(DisplayModel *dm, int pageNo) {
    LockCache();
    int cacheCount = gBitmapCacheCount;
    bool freedSomething = false;
    int curPos = 0;
    for (int i = 0; i < cacheCount; i++) {
        bool shouldFree = (gBitmapCache[i]->dm == dm) && (gBitmapCache[i]->pageNo == pageNo);
        if (shouldFree) {
            if (!freedSomething)
                DBG_OUT("BitmapCache_FreePage() ");
            DBG_OUT("freed %d ", gBitmapCache[i]->pageNo);
            freedSomething = true;
            BitmapCacheEntry_Free(gBitmapCache[i]);
            gBitmapCache[i] = NULL;
            --gBitmapCacheCount;
        }

        if (curPos != i)
            gBitmapCache[curPos] = gBitmapCache[i];

        if (!shouldFree)
            ++curPos;
    }
    UnlockCache();
    if (freedSomething)
        DBG_OUT("\n");
    return freedSomething;
}

/* Free all bitmaps cached for a given <dm>. Returns TRUE if freed
   at least one item. */
bool BitmapCache_FreeForDisplayModel(DisplayModel *dm) {
    LockCache();
    int cacheCount = gBitmapCacheCount;
    bool freedSomething = false;
    int curPos = 0;
    for (int i = 0; i < cacheCount; i++) {
        bool shouldFree = (gBitmapCache[i]->dm == dm);
        if (shouldFree) {
            if (!freedSomething)
                DBG_OUT("BitmapCache_FreeForDisplayModel() ");
            DBG_OUT("freed %d ", gBitmapCache[i]->pageNo);
            freedSomething = true;
            BitmapCacheEntry_Free(gBitmapCache[i]);
            gBitmapCache[i] = NULL;
            --gBitmapCacheCount;
        }

        if (curPos != i)
            gBitmapCache[curPos] = gBitmapCache[i];

        if (!shouldFree)
            ++curPos;
    }
    UnlockCache();
    if (freedSomething)
        DBG_OUT("\n");
    return freedSomething;
}

void BitmapCache_Add(DisplayModel *dm, int pageNo, double zoomLevel, int rotation, 
    RenderedBitmap *bitmap, double renderTime) {
    assert(gBitmapCacheCount <= MAX_BITMAPS_CACHED);
    assert(dm);
    assert(validRotation(rotation));

    normalizeRotation(&rotation);
    DBG_OUT("BitmapCache_Add(pageNo=%d, zoomLevel=%.2f%%, rotation=%d)\n", pageNo, zoomLevel, rotation);
    LockCache();

    /* It's possible there still is a cached bitmap with different zoomLevel/rotation */
    BitmapCache_FreePage(dm, pageNo);

    if (gBitmapCacheCount >= MAX_BITMAPS_CACHED - 1) {
        /* TODO: find entry that is not visible and remove it from cache to
           make room for new entry */
        delete bitmap;
        goto UnlockAndExit;
    }
    BitmapCacheEntry* entry = (BitmapCacheEntry*)malloc(sizeof(BitmapCacheEntry));
    if (!entry) {
        delete bitmap;
        goto UnlockAndExit;
    }
    entry->dm = dm;
    entry->pageNo = pageNo;
    entry->zoomLevel = zoomLevel;
    entry->rotation = rotation;
    entry->bitmap = bitmap;
    entry->renderTime = renderTime;
    gBitmapCache[gBitmapCacheCount++] = entry;
UnlockAndExit:
    UnlockCache();
}

BitmapCacheEntry *BitmapCache_Find(DisplayModel *dm, int pageNo) {
    BitmapCacheEntry* entry;
    LockCache();
    for (int i = 0; i < gBitmapCacheCount; i++) {
        entry = gBitmapCache[i];
        if ( (dm == entry->dm) && (pageNo == entry->pageNo) ) {
             goto Exit;
        }
    }
    entry = NULL;
Exit:
    UnlockCache();
    return entry;
}

BitmapCacheEntry *BitmapCache_Find(DisplayModel *dm, int pageNo, double zoomLevel, int rotation) {
    BitmapCacheEntry *entry;
    normalizeRotation(&rotation);
    LockCache();
    for (int i = 0; i < gBitmapCacheCount; i++) {
        entry = gBitmapCache[i];
        if ( (dm == entry->dm) && (pageNo == entry->pageNo) && 
             (zoomLevel == entry->zoomLevel) && (rotation == entry->rotation)) {
             goto Exit;
        }
    }
    entry = NULL;
Exit:
    UnlockCache();
    return entry;
}

/* Return true if a bitmap for a page defined by <dm>, <pageNo>, <zoomLevel>
   and <rotation> exists in the cache */
bool BitmapCache_Exists(DisplayModel *dm, int pageNo, double zoomLevel, int rotation) {
    if (BitmapCache_Find(dm, pageNo, zoomLevel, rotation))
        return true;
    return false;
}

PdfSearchResult *DisplayModel::Find(PdfSearchDirection direction, TCHAR *text, UINT fromPage)
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

DisplayModel *DisplayModel_CreateFromFileName(
  const TCHAR *fileName,
  SizeD totalDrawAreaSize,
  int scrollbarXDy, int scrollbarYDx,
  DisplayMode displayMode, int startPage,
  WindowInfo *win, bool tryrepair)
{
    DisplayModel *    dm = NULL;

    dm = new DisplayModel(displayMode, win->dpi);
    if (!dm)
        goto Error;

    if (!dm->load(fileName, startPage, win, tryrepair))
        goto Error;

    dm->setScrollbarsSize(scrollbarXDy, scrollbarYDx);
    dm->setTotalDrawAreaSize(totalDrawAreaSize);

//    DBG_OUT("DisplayModel_CreateFromPageTree() pageCount = %d, startPage=%d, displayMode=%d\n",
//        dm->pageCount(), (int)dm->startPage, (int)displayMode);
    return dm;
Error:
    delete dm;
    return NULL;
}

bool DisplayModel::cvtUserToScreen(int pageNo, double *x, double *y)
{
    pdf_page *page = pdfEngine->getPdfPage(pageNo);
    double zoom = zoomReal();
    int rot = rotation();
    fz_point p;
    // assert(page);
    if(!page)
        return false;

    normalizeRotation(&rot);
    zoom *= 0.01;

    p.x = *x;
    p.y = *y;

    fz_matrix ctm = pdfEngine->viewctm(page, zoom, rot);

    fz_point tp = fz_transformpoint(ctm, p);

    PdfPageInfo *pageInfo = getPageInfo(pageNo);

    rot += pageInfo->rotation;
    normalizeRotation(&rot);
    double vx = 0, vy = 0;
    if (rot == 90 || rot == 180)
        vx += pageInfo->currDx;
    if (rot == 180 || rot == 270)
        vy += pageInfo->currDy;

    *x = tp.x + 0.5 + pageInfo->screenX - pageInfo->bitmapX + vx;
    *y = tp.y + 0.5 + pageInfo->screenY - pageInfo->bitmapY + vy;
    return true;
}

bool DisplayModel::cvtScreenToUser(int *pageNo, double *x, double *y)
{
    double zoom = zoomReal();
    int rot = rotation();
    fz_point p;

    normalizeRotation(&rot);
    zoom *= 0.01;

    *pageNo = getPageNoByPoint(*x, *y);
    if (*pageNo == POINT_OUT_OF_PAGE) 
        return false;

    pdf_page *page = pdfEngine->getPdfPage(*pageNo);
    if (!page)
        return false;

    const PdfPageInfo *pageInfo = getPageInfo(*pageNo);

    p.x = *x - 0.5 - pageInfo->screenX + pageInfo->bitmapX;
    p.y = *y - 0.5 - pageInfo->screenY + pageInfo->bitmapY;

    fz_matrix ctm = pdfEngine->viewctm(page, zoom, rot);
    fz_matrix invCtm = fz_invertmatrix(ctm);

    fz_point tp = fz_transformpoint(invCtm, p);

    rot += pageInfo->rotation;
    normalizeRotation(&rot);
    double vx = 0, vy = 0;
    if (rot == 90 || rot == 180)
        vy -= pageInfo->pageDy;
    if (rot == 180 || rot == 270)
        vx += pageInfo->pageDx;

    *x = tp.x + vx;
    *y = tp.y + vy;
    return true;
}

TCHAR *DisplayModel::getLinkPath(pdf_link *link)
{
    fz_obj *obj;
    switch (link ? link->kind : -1) {
        case PDF_LURI:
            return utf8_to_tstr(fz_tostrbuf(link->dest));
        case PDF_LLAUNCH:
            obj = fz_dictgets(link->dest, "Type");
            if (fz_isname(obj) && !strcmp(fz_toname(obj), "FileSpec"))
                return utf8_to_tstr(fz_tostrbuf(fz_dictgets(link->dest, "F")));
            // fall through
        default:
            return NULL;
    }
}

void DisplayModel::goToTocLink(pdf_link* link)
{
    TCHAR *path = NULL;
    if (!link)
        return;
    if (PDF_LURI == link->kind && (path = getLinkPath(link))) {
        if (tstr_startswithi(path, _T("http:")) || tstr_startswithi(path, _T("https:")))
            launch_url(path);
        /* else: unsupported uri type */
        free(path);
    } else if (PDF_LGOTO == link->kind) {
        goToPdfDest(link->dest);
    } else if (PDF_LLAUNCH == link->kind && (path = getLinkPath(link))) {
        tstr_trans_chars(path, _T("/"), _T("\\"));
        /* for safety, only handle relative PDF paths and only open them in SumatraPDF */
        if (!tstr_startswith(path, _T("\\")) && tstr_endswithi(path, _T(".pdf"))) {
            TCHAR *basePath = FilePath_GetDir(fileName());
            TCHAR *combinedPath = tstr_cat3(basePath, _T(DIR_SEP_STR), path);
            /* TODO: The new window gets pushed to the background */
            LoadPdf(combinedPath);
            free(combinedPath);
            free(basePath);
        }
        free(path);
    }
}

void DisplayModel::handleLink(PdfLink *link)
{
    goToTocLink(link->link);
}

void DisplayModel::goToPdfDest(fz_obj *dest)
{
    int pageNo = pdfEngine->findPageNo(dest);
    if (pageNo > 0) {
        double scrollY = 0, scrollX = -1;
        fz_obj *obj = fz_arrayget(dest, 1);
        if (fz_isname(obj) && !strcmp(fz_toname(obj), "XYZ")) {
            scrollX = fz_toint(fz_arrayget(dest, 2));
            scrollY = fz_toint(fz_arrayget(dest, 3));
            cvtUserToScreen(pageNo, &scrollX, &scrollY);

            // goToPage needs scrolling info relative to the page's top border
            // and the page line's left margin
            PdfPageInfo * pageInfo = getPageInfo(pageNo);
            // TODO: These values are not up-to-date, if the page has not been shown yet
            scrollX += pageInfo->bitmapX + pageInfo->currPosX;
            scrollY += pageInfo->bitmapY;
        }
        goToPage(pageNo, scrollY, true, scrollX);
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

    RectI regionI;
    RectI_FromRectD(&regionI, region);
    TCHAR *result = (TCHAR *)malloc((lstrlen(pageText) + 1) * sizeof(TCHAR)), *dest = result;
    for (TCHAR *src = pageText; *src; src++) {
        if (*src != '\n') {
            fz_bbox *bbox = &coords[src - pageText];
            RectI rect = { bbox->x0, bbox->y0, bbox->x1 - bbox->x0, bbox->y1 - bbox->y0 };
            if (RectI_Intersect(&regionI, &rect, NULL)) {
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
TCHAR *DisplayModel::extractAllText(void)
{
    TStrList *pages = NULL;

    for (int pageNo = 1; pageNo <= pageCount(); pageNo++)
        TStrList_InsertAndOwn(&pages, pdfEngine->ExtractPageText(pageNo));

    TStrList_Reverse(&pages);
    TCHAR *content = TStrList_Join(pages, NULL);
    TStrList_Destroy(&pages);

    return content;
}

void DisplayModel::MapResultRectToScreen(PdfSearchResult *res)
{
    PdfPageInfo *pageInfo = getPageInfo(res->page);
    pdf_page *page = pdfEngine->getPdfPage(res->page);
    if(!page)
        return;
    int rot = rotation();
    normalizeRotation (&rot);

    int dispRot = rot + pageInfo->rotation;
    normalizeRotation(&dispRot);
    double vx = pageInfo->screenX - pageInfo->bitmapX,
           vy = pageInfo->screenY - pageInfo->bitmapY;
    if (dispRot == 90 || dispRot == 180)
        vx += pageInfo->currDx;
    if (dispRot == 180 || dispRot == 270)
        vy += pageInfo->currDy;

    RECT extremes = { 0 };
    for (int i = 0; i < res->len; i++) {
        RECT *rect = &res->rects[i];
        double left = rect->left, top = rect->top, right = rect->right, bottom = rect->bottom;
        fz_matrix ctm = pdfEngine->viewctm(page, zoomReal() * 0.01, rot);
        fz_point tp, p1 = {left, top}, p2 = {right, bottom};

        tp = fz_transformpoint(ctm, p1);
        left = tp.x - 0.5 + vx;
        top = tp.y - 0.5 + vy;

        tp = fz_transformpoint(ctm, p2);
        right = tp.x + 0.5 + vx;
        bottom = tp.y + 0.5 + vy;

        if (left > right)
            swap_double(&left, &right);
        if (top > bottom)
            swap_double(&top, &bottom);

        rect->left = (int)floor(left);
        rect->top = (int)floor(top);
        rect->right = (int)ceil(right) + 5;
        rect->bottom = (int)ceil(bottom) + 5;

        if (0 == i) {
            memcpy(&extremes, rect, sizeof(RECT));
        } else {
            extremes.left = min(rect->left, extremes.left);
            extremes.right = max(rect->right, extremes.right);
            extremes.top = min(rect->top, extremes.top);
            extremes.bottom = max(rect->bottom, extremes.bottom);
        }
    }

    int sx = 0, sy = 0;
    if (extremes.bottom > pageInfo->screenY + pageInfo->bitmapY + pageInfo->bitmapDy)
        sy = extremes.bottom - pageInfo->screenY - pageInfo->bitmapY - pageInfo->bitmapDy;

    if (extremes.left < pageInfo->screenX + pageInfo->bitmapX) {
        sx = extremes.left - pageInfo->screenX - pageInfo->bitmapX - 5;
        if (sx + pageInfo->screenX + pageInfo->bitmapX < 0)
            sx = -(pageInfo->screenX + pageInfo->bitmapX);
    } else if (extremes.right > pageInfo->screenX + pageInfo->bitmapDx) {
        sx = extremes.right - (pageInfo->screenX + pageInfo->bitmapDx) + 5;
    }

    if (sy < 0)
        sy = 0;
    if (sx != 0)
        scrollXBy(sx);
    if (sy != 0)
        scrollYBy(sy, false);

    for (int i = 0; i < res->len; i++) {
        res->rects[i].left -= sx;
        res->rects[i].right -= sx;
        res->rects[i].top -= sy;
        res->rects[i].bottom -= sy;
    }
}

void DisplayModel::rebuildLinks()
{
    int count = pdfEngine->linkCount();
    assert(count > _linksCount);
    if (_links)
        free(_links);
    _links = (PdfLink*)malloc(count * sizeof(PdfLink));
    _linksCount = count;
    pdfEngine->fillPdfLinks(_links, _linksCount);
    recalcLinksCanvasPos();
}

int DisplayModel::getLinkCount()
{
    /* TODO: let's hope it's not too expensive. An alternative would be
       to update link count only when it could have changed i.e. after
       loading a page */
    int count = pdfEngine->linkCount();
    if (count != _linksCount)
    {
        assert(count > _linksCount);
        rebuildLinks();
    }
    return count;
}

bool DisplayModel::getScrollState(ScrollState *state)
{
    state->page = currentPageNo();
    if (state->page <= 0)
        return false;

    PdfPageInfo *pageInfo = getPageInfo(state->page);
    state->x = max(pageInfo->screenX - pageInfo->bitmapX, 0);
    state->y = max(pageInfo->screenY - pageInfo->bitmapY, 0);
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
    goToPage(state->page, newY, false, newX);
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
    memcpy(&_navHistory[_navHistoryIx], &ss, sizeof(ss));
    _navHistoryIx++;
    if (!keepForward || _navHistoryIx > _navHistoryEnd)
        _navHistoryEnd = _navHistoryIx;

    return ss.page != 0;
}

bool DisplayModel::canNavigate(int dir)
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
