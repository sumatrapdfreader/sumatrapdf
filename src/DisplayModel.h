/* Copyright Krzysztof Kowalczyk 2006-2009
   License: GPLv3 */
#ifndef _DISPLAY_MODEL_H_
#define _DISPLAY_MODEL_H_

#include "base_util.h"
#include "geom_util.h"
#include "DisplayState.h"
#include "PdfEngine.h"
#include "PdfSearch.h"

#define INVALID_ZOOM        -99
#define INVALID_BIG_ZOOM    999999.0   /* arbitrary but big */

typedef struct DisplaySettings {
    int     paddingPageBorderTop;
    int     paddingPageBorderBottom;
    int     paddingPageBorderLeft;
    int     paddingPageBorderRight;
    int     paddingBetweenPagesX;
    int     paddingBetweenPagesY;
} DisplaySettings;

/* the default distance between a page and window border edges, in pixels */
#define PADDING_PAGE_BORDER_TOP_DEF      5
#define PADDING_PAGE_BORDER_BOTTOM_DEF   7
#define PADDING_PAGE_BORDER_LEFT_DEF     5
#define PADDING_PAGE_BORDER_RIGHT_DEF    7
/* the distance between pages in x axis, in pixels. Only applicable if
   columns > 1 */
#define PADDING_BETWEEN_PAGES_X_DEF      8
/* the distance between pages in y axis, in pixels. Only applicable if
   more than one page in y axis (continuous mode) */
#define PADDING_BETWEEN_PAGES_Y_DEF      PADDING_PAGE_BORDER_TOP_DEF + PADDING_PAGE_BORDER_BOTTOM_DEF

#define PADDING_PAGE_BORDER_TOP      gDisplaySettings.paddingPageBorderTop
#define PADDING_PAGE_BORDER_BOTTOM   gDisplaySettings.paddingPageBorderBottom
#define PADDING_PAGE_BORDER_LEFT     gDisplaySettings.paddingPageBorderLeft
#define PADDING_PAGE_BORDER_RIGHT    gDisplaySettings.paddingPageBorderRight
#define PADDING_BETWEEN_PAGES_X      gDisplaySettings.paddingBetweenPagesX
#define PADDING_BETWEEN_PAGES_Y      gDisplaySettings.paddingBetweenPagesY

#define POINT_OUT_OF_PAGE           0

#define NAV_HISTORY_LEN             50

/* Describes many attributes of one page in one, convenient place */
typedef struct PdfPageInfo {
    /* data that is constant for a given page. page size and rotation
       recorded in PDF file */
    double          pageDx; // TODO: consider SizeD instead of pageDx/pageDy
    double          pageDy;
    int             rotation;

    /* data that needs to be set before DisplayModel_relayout().
       Determines whether a given page should be shown on the screen. */
    bool            shown;

    /* data that changes when zoom and rotation changes */
    /* position and size within total area after applying zoom and rotation.
       Represents display rectangle for a given page.
       Calculated in DisplayModel_relayout() */

    /* TODO: change it to RectD ?*/
    double          currDx;
    double          currDy;
    double          currPosX;
    double          currPosY;

    /* data that changes due to scrolling. Calculated in DisplayModel_RecalcVisibleParts() */
    double          visible; /* visible ratio of the page (0 = invisible, 1 = fully visible) */
    /* part of the image that should be shown */
    int             bitmapX, bitmapY, bitmapDx, bitmapDy;
    /* where it should be blitted on the screen */
    int             screenX, screenY;
} PdfPageInfo;

/* When searching, we can be in one of those states. The state determines what
   will happen after searching for next or previous term.
   */
enum SearchState { 
    /* Search hasn't started yet. 'Next' will start searching from the top
       of current page, searching forward. 'Previous' will start searching from
       the top of current page, searching backward. */
    eSsNone,
    /* We searched the whole document and didn't find the search term at all. 'Next'
       and 'prev' do nothing. */
    eSsNotFound,
    /* Previous 'next' search found the term, without wrapping. 'Next' will
       continue searching forward from the current position. 'Previous' will
       search backward from the current position.*/
    eSsFoundNext, 
    /* Like eSsFoundNext but we wrapped past last page. In that case we show
       a message about being wrapped and continuing from top. */
    eSsFoundNextWrapped,
    /* Previous 'prev' search found the term, without wrapping. 'Next' will
       search forward from the current position. 'Prev' will continue searching
       backward */
    eSsFoundPrev,
    /* Like eSsFoundPrev, but wrapped around first page. */
    eSsFoundPrevWrapped,
/*   TODO: add eSsFoundOnlyOne as optimization i.e. if the hit is the same
   as previous hit, 'next' and 'previous' will do nothing, to avoid (possibly
   long) no-op search.
    eSsFoundOnlyOne */
};

/* The current state of searching */
typedef struct SearchStateData {
    /* The page on which we started the search */
    int             startPage;
    /* did we wrap (crossed last page when searching forward or first page
       when searching backwards) */
    BOOL            wrapped;
    SearchState     searchState;
    BOOL            caseSensitive;
    int             currPage; /* page for the last hit */
} SearchStateData;

/* The current scroll state (needed for saving/restoring the scroll position) */
typedef struct ScrollState {
    int page;
    double x; /* in user space units (per page) */
    double y; /* in user space units (per page) */
} ScrollState;

/* Information needed to drive the display of a given PDF document on a screen.
   You can think of it as a model in the MVC pardigm.
   All the display changes should be done through changing this model via
   API and re-displaying things based on new display information */
class DisplayModel
{
public:
    DisplayModel(DisplayMode displayMode, int dpi=96);
    ~DisplayModel();

    RenderedBitmap *renderBitmap(int pageNo, double zoomReal, int rotation,
                         fz_rect *pageRect, /* if NULL: defaults to the page's mediabox */
                         BOOL (*abortCheckCbkA)(void *data),
                         void *abortCheckCbkDataA) {
        if (!pdfEngine) return NULL;
        return pdfEngine->renderBitmap(pageNo, zoomReal, rotation, pageRect, abortCheckCbkA, abortCheckCbkDataA);
    }

    /* number of pages in PDF document */
    int  pageCount() const { return pdfEngine->pageCount(); }
    bool load(const TCHAR *fileName, int startPage, WindowInfo *win, bool tryrepair);
    bool validPageNo(int pageNo) const { return pdfEngine->validPageNo(pageNo); }
    bool hasTocTree() { return pdfEngine->hasTocTree(); }
    PdfTocItem *getTocTree() { return pdfEngine->getTocTree(); }

    /* current rotation selected by user */
    int rotation(void) const { return _rotation; }
    void setRotation(int rotation) { _rotation = rotation; }
    DisplayMode displayMode() const { return _displayMode; }

    void changeDisplayMode(DisplayMode displayMode);
    const TCHAR *fileName(void) const { return pdfEngine->fileName(); }

    /* a "virtual" zoom level. Can be either a real zoom level in percent
       (i.e. 100.0 is original size) or one of virtual values ZOOM_FIT_PAGE
       or ZOOM_FIT_WIDTH, whose real value depends on draw area size */
    double zoomVirtual(void) const { return _zoomVirtual; }
    void setZoomVirtual(double zoomVirtual);

    double zoomReal(void) const { return _zoomReal; }

    int startPage(void) const { return _startPage; }

    int currentPageNo(void) const;

    PdfEngine *     pdfEngine;

    /* an arbitrary pointer that can be used by an app e.g. a multi-window GUI
       could link this to a data describing window displaying  this document */
    void * appData() const { return _appData; }

    void setAppData(void *appData) { _appData = appData; }

    /* TODO: rename to pageInfo() */
    PdfPageInfo * getPageInfo(int pageNo) const;

    /* an array of PdfPageInfo, len of array is pageCount */
    PdfPageInfo *   _pagesInfo;

    /* areaOffset is "polymorphic". If drawAreaSize.dx > totalAreSize.dx then
       areaOffset.x is offset of total area rect inside draw area, otherwise
       an offset of draw area inside total area.
       The same for areaOff.y, except it's for dy */
    PointD          areaOffset;

    /* size of draw area i.e. _totalDrawAreaSize minus scrollbarsSize (if
       they're shown) */
    SizeD           drawAreaSize;

    SearchStateData searchState;

    int             searchHitPageNo;
    RectD           searchHitRectPage;
    RectI           searchHitRectCanvas;

    void            setScrollbarsSize(int scrollbarXDy, int scrollbarYDx) {
        _scrollbarXDy = scrollbarXDy;
        _scrollbarYDx = scrollbarYDx;
    }

    void            setTotalDrawAreaSize(SizeD size) {
        _totalDrawAreaSize = size;
        drawAreaSize = SizeD(size.dx() - _scrollbarYDx, size.dy() - _scrollbarXDy);
    }
    
    bool            needHScroll() { return drawAreaSize.dxI() < _canvasSize.dxI(); }
    bool            needVScroll() { return drawAreaSize.dyI() < _canvasSize.dyI(); }

    void            changeTotalDrawAreaSize(SizeD totalDrawAreaSize);

    bool            pageShown(int pageNo);
    bool            pageVisible(int pageNo);
    bool            pageVisibleNearby(int pageNo);
    void            relayout(double zoomVirtual, int rotation);

    void            goToPage(int pageNo, int scrollY, int scrollX=-1);
    bool            goToPrevPage(int scrollY);
    bool            goToNextPage(int scrollY);
    bool            goToFirstPage(void);
    bool            goToLastPage(void);
    void            goToTocLink(void *link);

    void            scrollXTo(int xOff);
    void            scrollXBy(int dx);

    void            scrollYTo(int yOff);
    void            scrollYBy(int dy, bool changePage);
    void            scrollYByAreaDy(bool forward, bool changePage);

    void            zoomTo(double zoomVirtual);
    void            zoomBy(double zoomFactor);
    void            rotateBy(int rotation);

    int             getTextInRegion(int pageNo, RectD *region, WCHAR *buf, int buflen);
    int             extractAllText(TCHAR *buffer, int buf_len=0);

    void            clearSearchHit(void);
    void            setSearchHit(int pageNo, RectD *hitRect);
    void            recalcLinksCanvasPos(void);

    int             getLinkCount(void);
    PdfLink *       linkAtPosition(int x, int y);

    void            handleLink(PdfLink *pdfLink);
    void            goToNamedDest(const char *name);

    bool            cvtUserToScreen(int pageNo, double *x, double *y);
    bool            cvtScreenToUser(int *pageNo, double *x, double *y);
    bool            rectCvtUserToScreen(int pageNo, RectD *r);
    bool            rectCvtScreenToUser(int *pageNo, RectD *r);

    void            SetFindMatchCase(bool match) { _pdfSearch->SetSensitive(match); }
    PdfSearchResult *Find(PdfSearchDirection direction = FIND_FORWARD, TCHAR *text = NULL, UINT fromPage = 0);
    BOOL            bFoundText;

    BOOL            _showToc;
    BOOL            _tocBeforeFullScreen;

    int             getPageNoByPoint (double x, double y);

    void            MapResultRectToScreen(PdfSearchResult *rect);

    void            rebuildLinks();
    void            handleLink2(pdf_link* link);

    bool            getScrollState(ScrollState *state);
    void            setScrollState(ScrollState *state);

    bool            addNavPoint(bool keepForward=false);
    void            navigate(int dir);

protected:

    void            startRenderingPage(int pageNo);

    bool            buildPagesInfo(void);
    double          zoomRealFromVirtualForPage(double zoomVirtual, int pageNo);
    int             firstVisiblePageNo(void) const;
    void            changeStartPage(int startPage);
    void            recalcVisibleParts(void);
    void            recalcSearchHitCanvasPos(void);
    void            renderVisibleParts(void);
    /* Those need to be implemented somewhere else by the GUI */
    void            setScrollbarsState(void);
    /* called when a page number changes */
    void            pageChanged(void);
    /* called when we decide that the display needs to be redrawn */
    void            repaintDisplay(bool delayed);

    PdfSearch *     _pdfSearch;
    DisplayMode     _displayMode; /* TODO: not used yet */
    /* In non-continuous mode is the first page from a PDF file that we're
       displaying.
       No meaning in continous mode. */
    int             _startPage;
    void *          _appData;

    /* size of scrollbars */
    int             _scrollbarXDy;
    int             _scrollbarYDx;

    /* size of total draw area (i.e. window size) */
    SizeD           _totalDrawAreaSize;

    /* size of virtual canvas containing all rendered pages.
       TODO: re-consider, 32 signed number should be large enough for everything. */
    SizeD           _canvasSize;

    /* real zoom value calculated from zoomVirtual. Same as zoomVirtual except
       for ZOOM_FIT_PAGE and ZOOM_FIT_WIDTH */
    double          _zoomReal;
    double          _zoomVirtual;
    int             _rotation;
    /* dpi correction factor by which _zoomVirtual has to be multiplied in
       order to get _zoomReal */
    double          _dpiFactor;
    /* display odd pages on the right? */
    bool            _showCover;

    ScrollState   * _navHistory;
    int             _navHistoryIx;
    int             _navHistoryEnd;

public:
    /* an array of 'totalLinksCount' size, each entry describing a link */
    PdfLink *       _links;
    int             _linksCount;
};

bool                displayModeContinuous(DisplayMode displayMode);
bool                displayModeFacing(DisplayMode displayMode);
bool                displayModeShowCover(DisplayMode displayMode);
DisplaySettings *   globalDisplaySettings(void);
int                 columnsFromDisplayMode(DisplayMode displayMode);
void                pageSizeAfterRotation(PdfPageInfo *pageInfo, int rotation, double *pageDxOut, double *pageDyOut);
bool                displayStateFromDisplayModel(DisplayState *ds, DisplayModel *dm);

DisplayModel *DisplayModel_CreateFromFileName(
  const TCHAR *fileName,
  SizeD totalDrawAreaSize,
  int scrollbarXDy, int scrollbarYDx,
  DisplayMode displayMode, int startPage,
  WindowInfo *win, bool tryrepair);

extern DisplaySettings gDisplaySettings;

/* We keep a cache of rendered bitmaps. BitmapCacheEntry keeps data
   that uniquely identifies rendered page (dm, pageNo, rotation, zoomReal)
   and corresponding rendered bitmap.
*/
typedef struct {
  DisplayModel * dm;
  int            pageNo;
  int            rotation;
  double         zoomLevel;
  RenderedBitmap *bitmap;
  double         renderTime;
} BitmapCacheEntry;

typedef struct {
    DisplayModel *  dm;
    int             pageNo;
    double          zoomLevel;
    int             rotation;
    int             abort;
} PageRenderRequest;

/* Lock protecting both bitmap cache and page render queue */
void              LockCache();
void              UnlockCache();

void              RenderQueue_Add(DisplayModel *dm, int pageNo);
extern void       RenderQueue_RemoveForDisplayModel(DisplayModel *dm);
extern void       cancelRenderingForDisplayModel(DisplayModel *dm);

BitmapCacheEntry *BitmapCache_Find(DisplayModel *dm, int pageNo, double zoomLevel, int rotation);
BitmapCacheEntry *BitmapCache_Find(DisplayModel *dm, int pageNo);
bool              BitmapCache_Exists(DisplayModel *dm, int pageNo, double zoomLevel, int rotation);
void              BitmapCache_Add(DisplayModel *dm, int pageNo, double zoomLevel, int rotation, 
                                  RenderedBitmap *bitmap, double renderTime);
void              BitmapCache_FreeAll(void);
bool              BitmapCache_FreeForDisplayModel(DisplayModel *dm);
bool              BitmapCache_FreeNotVisible(void);

#endif
