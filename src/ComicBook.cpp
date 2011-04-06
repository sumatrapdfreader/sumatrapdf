/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include <windows.h>
#include "ComicBook.h"
#include "SumatraPDF.h"
#include "StrUtil.h"
#include "Vec.h"
#include "WinUtil.h"
#include "FileUtil.h"
#include "WindowInfo.h"
#include "AppPrefs.h"
#include "FileHistory.h"
#include "AppTools.h"
#include "DisplayModel.h"

// mini(un)zip
#include <ioapi.h>
#include <iowin32.h>
#include <unzip.h>

/* Given 'pageInfo', which should contain correct information about
   pageDx, pageDy and rotation, return a page size after applying a global
   rotation */
static void pageSizeAfterRotation(PageInfo& pageInfo, int rotation,
    SizeD *pageSize, bool fitToContent=false)
{
    assert(pageSize);
    if (!pageSize)
        return;

#if 0 // pdf-specific
    if (fitToContent) {
        if (fz_isemptybbox(pageInfo->contentBox))
            return pageSizeAfterRotation(pageInfo, rotation, pageSize);
        pageSize->dx = pageInfo->contentBox.x1 - pageInfo->contentBox.x0;
        pageSize->dy = pageInfo->contentBox.y1 - pageInfo->contentBox.y0;
    } else
#endif
        *pageSize = pageInfo.size;

    rotation += pageInfo.rotation;
    if (rotationFlipped(rotation))
        swap(pageSize->dx, pageSize->dy);
}

/* Given a zoom level that can include a "virtual" zoom levels like ZOOM_FIT_WIDTH,
   ZOOM_FIT_PAGE or ZOOM_FIT_CONTENT, calculate an absolute zoom level */
float Layout::ZoomRealFromVirtualForPage(float zoomVirtual, int pageNo)
{
    if (zoomVirtual != ZOOM_FIT_WIDTH && zoomVirtual != ZOOM_FIT_PAGE && zoomVirtual != ZOOM_FIT_CONTENT)
        return zoomVirtual * 0.01f;

    SizeD row;
    PageInfo pageInfo = GetPageInfo(pageNo);
    int columns = columnsFromDisplayMode(displayMode);

    bool fitToContent = false; // (ZOOM_FIT_CONTENT == zoomVirtual);
    if (fitToContent && columns > 1) {
#if 0 // this is PDF-specific code that would have to be abstracted in some way e.g. via optional proxy class that can return necessary info
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
#endif
    } else {
#if 0 // pdf-specific
        if (fitToContent && fz_isemptybbox(pageInfo->contentBox))
            pageInfo->contentBox = pdfEngine->pageContentBox(pageNo);
#endif
        pageSizeAfterRotation(pageInfo, rotation, &row, fitToContent);
        row.dx *= columns;
    }

    assert(0 != (int)row.dx);
    assert(0 != (int)row.dy);

    int areaForPagesDx = drawAreaSize.dx - padding->pageBorderLeft - padding->pageBorderRight - padding->betweenPagesX * (columns - 1);
    int areaForPagesDy = drawAreaSize.dy - padding->pageBorderTop - padding->pageBorderBottom;
    if (areaForPagesDx <= 0 || areaForPagesDy <= 0)
        return 0;

    float zoomX = areaForPagesDx / (float)row.dx;
    float zoomY = areaForPagesDy / (float)row.dy;
    if (zoomX < zoomY || ZOOM_FIT_WIDTH == zoomVirtual)
        return zoomX;
    return zoomY;
}

// zoom is either in percent or one of the special values
void Layout::SetZoomVirtual(float zoom)
{
    zoomVirtual = zoom;
    if ((ZOOM_FIT_WIDTH == zoom) || (ZOOM_FIT_PAGE == zoom)) {
        /* we want the same zoom for all pages, so use the smallest zoom
           across the pages so that the largest page fits. Usually all
           pages are the same size anyway */
        float minZoom = (float)HUGE_VAL;
        for (int pageNo = 1; pageNo <= PageCount(); pageNo++) {
            if (PageShown(pageNo)) {
                float thisPageZoom = ZoomRealFromVirtualForPage(zoomVirtual, pageNo);
                if (minZoom > thisPageZoom)
                    minZoom = thisPageZoom;
            }
        }
        assert(minZoom != (float)HUGE_VAL);
        zoomReal = minZoom;
    } else if (ZOOM_FIT_CONTENT == zoomVirtual) {
#if 0 // pdf-specific
        float newZoom = ZoomRealFromVirtualForPage(zoomVirtual, currentPageNo());
        // limit zooming in to 800% on almost empty pages
        if (newZoom > 8.0)
            newZoom = 8.0;
        // don't zoom in by just a few pixels (throwing away a prerendered page)
        if (newZoom < zoomReal || zoomReal / newZoom < 0.95 ||
            zoomReal < ZoomRealFromVirtualForPage(ZOOM_FIT_PAGE, currentPageNo()))
            zoomReal = newZoom;
#else
        zoomReal = zoomVirtual * 0.01f;
#endif
    } else
        zoomReal = zoomVirtual * 0.01f;
}

void Layout::DoLayout(float zoomVirtual, int rot)
{
    int         pageNo;
    PageInfo    pageInfo;
    SizeD       pageSize;
    int         totalAreaDx, totalAreaDy;
    int         rowMaxPageDy;
    int         offX;
    int         pageOffX;
    int         pageInARow;
    int         columns;
    int         newAreaOffsetX;
    int         columnOffsets[2];

    // we start layout assuming we can use the whole drawAreaSize
    // when we discover we need to show scrollbars, we restart layout
    // using drawAreaSize - part used by scrollbars
    bool showHorizScrollbar = false;
    bool showVertScrollbar = false;

    // TODO: for now always show scrollbars
    showHorizScrollbar = showVertScrollbar = true;

    normalizeRotation(&rot);
    rotation = rot;
    float currZoomReal = zoomReal;

RestartLayout:
    // substract scrollbars area if needed
    drawAreaSize = totalAreaSize;
    if (showHorizScrollbar)
        drawAreaSize.dy -= horizScrollbarDy;
    if (showVertScrollbar)
        drawAreaSize.dx -= vertScrollbarDx;

    SetZoomVirtual(zoomVirtual);
    int currPosY = padding->pageBorderTop;

    if (0 == currZoomReal || INVALID_ZOOM == currZoomReal)
        newAreaOffsetX = 0;
    else
        newAreaOffsetX = (int)(areaOffset.x * zoomReal / currZoomReal);
    areaOffset.x = newAreaOffsetX;
    /* calculate the position of each page on the canvas, given current zoom,
       rotation, columns parameters. You can think of it as a simple
       table layout i.e. rows with a fixed number of columns. */
    columns = columnsFromDisplayMode(displayMode);

    pageInARow = 0;
    rowMaxPageDy = 0;
    for (pageNo = 1; pageNo <= PageCount(); ++pageNo) {
        PageInfo pageInfo = GetPageInfo(pageNo);
        if (pageInfo.notShown) {
            assert(0.0 == pageInfo.visibleRatio);
            continue;
        }
        pageSizeAfterRotation(pageInfo, rotation, &pageSize);
        pageInfo.currPos.dx = (int)(pageSize.dx * zoomReal + 0.5);
        pageInfo.currPos.dy = (int)(pageSize.dy * zoomReal + 0.5);

        if (rowMaxPageDy < pageInfo.currPos.dy)
            rowMaxPageDy = pageInfo.currPos.dy;
        pageInfo.currPos.y = currPosY;

        if (displayModeShowCover(displayMode) && pageNo == 1 && columns - pageInARow > 1)
            pageInARow++;
        if (columnOffsets[pageInARow] < pageInfo.currPos.dx)
            columnOffsets[pageInARow] = pageInfo.currPos.dx;

        pageInARow++;
        assert(pageInARow <= columns);
        if (pageInARow == columns) {
            /* starting next row */
            currPosY += rowMaxPageDy + padding->betweenPagesY;
            rowMaxPageDy = 0;
            pageInARow = 0;
        }

        // if we're not showing one of the scrollbars but detect that we should
        // because we exceeded available area, note that and restart the layout
        if (!showHorizScrollbar) {
            int maxX = pageInfo.currPos.x + pageInfo.currPos.dx;
            // TODO: take padding into account?
            if (maxX > totalAreaSize.dx) {
                showHorizScrollbar = true;
                goto RestartLayout;
            }
        }

        if (!showVertScrollbar) {
            int maxY = pageInfo.currPos.y + pageInfo.currPos.dy;
            // TODO: take padding into account?
            if (maxY > totalAreaSize.dy) {
                showVertScrollbar = true;
                goto RestartLayout;
            }
        }

/*        DBG_OUT("  page = %3d, (x=%3d, y=%5d, dx=%4d, dy=%4d) orig=(dx=%d,dy=%d)\n",
            pageNo, pageInfo.currPos.x, pageInfo.currPos.y,
                    pageInfo.currPos.dx, pageInfo.currPos.dy,
                    (int)pageSize.dx, (int)pageSize.dy); */
    }

    if (pageInARow != 0)
        /* this is a partial row */
        currPosY += rowMaxPageDy + padding->betweenPagesY;
    if (columns == 2 && PageCount() == 1) {
        /* don't center a single page over two columns */
        if (displayModeShowCover(displayMode))
            columnOffsets[0] = columnOffsets[1];
        else
            columnOffsets[1] = columnOffsets[0];
    }
    totalAreaDx = padding->pageBorderLeft + columnOffsets[0] + (columns == 2 ? padding->betweenPagesX + columnOffsets[1] : 0) + padding->pageBorderRight;

    /* since pages can be smaller than the drawing area, center them in x axis */
    offX = 0;
    if (totalAreaDx < drawAreaSize.dx) {
        areaOffset.x = 0;
        offX = (drawAreaSize.dx - totalAreaDx) / 2;
        totalAreaDx = drawAreaSize.dx;
    }
    assert(offX >= 0);
    pageInARow = 0;
    pageOffX = offX + padding->pageBorderLeft;
    for (pageNo = 1; pageNo <= PageCount(); ++pageNo) {
        pageInfo = GetPageInfo(pageNo);
        if (pageInfo.notShown) {
            assert(0.0 == pageInfo.visibleRatio);
            continue;
        }
        // leave first spot empty in cover page mode
        if (displayModeShowCover(displayMode) && pageNo == 1)
            pageOffX += columnOffsets[pageInARow++] + padding->betweenPagesX;
        pageInfo.currPos.x = pageOffX + (columnOffsets[pageInARow] - pageInfo.currPos.dx) / 2;
        // center the cover page over the first two spots in non-continuous mode
        if (displayModeShowCover(displayMode) && pageNo == 1 && !displayModeContinuous(displayMode))
            pageInfo.currPos.x = offX + padding->pageBorderLeft + (columnOffsets[0] + padding->betweenPagesX + columnOffsets[1] - pageInfo.currPos.dx) / 2;
        // mirror the page layout when displaying a Right-to-Left document
        if (displayR2L && columns > 1)
            pageInfo.currPos.x = totalAreaDx - pageInfo.currPos.x - pageInfo.currPos.dx;
        pageOffX += columnOffsets[pageInARow++] + padding->betweenPagesX;
        assert(pageOffX >= 0 && pageInfo.currPos.x >= 0);

        if (pageInARow == columns) {
            pageOffX = offX + padding->pageBorderLeft;
            pageInARow = 0;
        }
    }

    /* if after resizing we would have blank space on the right due to x offset
       being too much, make x offset smaller so that there's no blank space */
    if (drawAreaSize.dx - (totalAreaDx - newAreaOffsetX) > 0) {
        newAreaOffsetX = totalAreaDx - drawAreaSize.dx;
        areaOffset.x = newAreaOffsetX;
    }

    /* if a page is smaller than drawing area in y axis, y-center the page */
    totalAreaDy = currPosY + padding->pageBorderBottom - padding->betweenPagesY;
    if (totalAreaDy < drawAreaSize.dy) {
        int offY = padding->pageBorderTop + (drawAreaSize.dy - totalAreaDy) / 2;
        DBG_OUT("  offY = %.2f\n", offY);
        assert(offY >= 0.0);
        totalAreaDy = drawAreaSize.dy;
        for (pageNo = 1; pageNo <= PageCount(); ++pageNo) {
            pageInfo = GetPageInfo(pageNo);
            if (pageInfo.notShown) {
                assert(0.0 == pageInfo.visibleRatio);
                continue;
            }
            pageInfo.currPos.y += offY;
            DBG_OUT("  page = %3d, (x=%3d, y=%5d, dx=%4d, dy=%4d) orig=(dx=%d,dy=%d)\n",
                pageNo, pageInfo.currPos.x, pageInfo.currPos.y,
                        pageInfo.currPos.dx, pageInfo.currPos.dy,
                        (int)pageSize.dx, (int)pageSize.dy);
        }
    }

    canvasSize = SizeI(totalAreaDx, totalAreaDy);
}

/* Given positions of each page in a large sheet that is continous view and
   coordinates of a current view into that large sheet, calculate which
   parts of each page is visible on the screen.
   Needs to be recalucated after scrolling the view. */
// TODO: it's convenient for this to be here, but it's not an absolutely
// necessary part of the layout. It could be part of some higher level logic
// that deals with scrollbar state
void Layout::RecalcVisibleParts()
{
    RectI drawAreaRect(areaOffset, drawAreaSize);

//    DBG_OUT("DisplayModel::recalcVisibleParts() draw area         (x=%3d,y=%3d,dx=%4d,dy=%4d)\n",
//        drawAreaRect.x, drawAreaRect.y, drawAreaRect.dx, drawAreaRect.dy);
    for (int pageNo = 1; pageNo <= PageCount(); ++pageNo) {
        PageInfo pageInfo = GetPageInfo(pageNo);
        if (pageInfo.notShown) {
            assert(0.0 == pageInfo.visibleRatio);
            continue;
        }

        RectI pageRect = pageInfo.currPos;
        pageInfo.visibleRatio = 0.0;

        RectI visiblePart = pageRect.Intersect(drawAreaRect);
        if (pageRect.dx > 0 && pageRect.dy > 0 && !visiblePart.IsEmpty()) {
            // calculate with floating point precision to prevent an integer overflow
            pageInfo.visibleRatio = 1.0f * visiblePart.dx * visiblePart.dy / ((float)pageRect.dx * pageRect.dy);

            pageInfo.bitmap = visiblePart;
            pageInfo.bitmap.x -= pageRect.x;
            assert(pageInfo.bitmap.x >= 0);
            pageInfo.bitmap.y -= pageRect.y;
            assert(pageInfo.bitmap.y >= 0);
            pageInfo.screenX = visiblePart.x - areaOffset.x;
            assert(pageInfo.screenX >= 0);
            assert(pageInfo.screenX <= drawAreaSize.dx);
            pageInfo.screenY = visiblePart.y - areaOffset.y;
            assert(pageInfo.screenX >= 0);
            assert(pageInfo.screenY <= drawAreaSize.dy);
/*          DBG_OUT("                                  visible page = %d, (x=%3d,y=%3d,dx=%4d,dy=%4d) at (x=%d,y=%d)\n",
                pageNo, pageInfo.bitmap.x, pageInfo.bitmap.y,
                          pageInfo.bitmap.dx, pageInfo.bitmap.dy,
                          pageInfo.screenX, pageInfo.screenY); */
        }

        pageInfo.pageOnScreen = pageRect;
        pageInfo.pageOnScreen.x = pageInfo.pageOnScreen.x - areaOffset.x;
        pageInfo.pageOnScreen.y = pageInfo.pageOnScreen.y - areaOffset.y;
        assert(max(pageInfo.pageOnScreen.x, 0) == pageInfo.screenX || 0.0 == pageInfo.visibleRatio);
        assert(max(pageInfo.pageOnScreen.y, 0) == pageInfo.screenY || 0.0 == pageInfo.visibleRatio);
    }
}


using namespace Gdiplus;

bool IsComicBook(const TCHAR *fileName)
{
    if (Str::EndsWithI(fileName, _T(".cbz")))
        return true;
#if 0 // not yet
    if (Str::EndsWithI(fileName, _T(".cbr")))
        return true;
#endif
    return false;
}

static bool IsImageFile(char *fileName)
{
    if (Str::EndsWithI(fileName, ".png"))
        return true;
    if (Str::EndsWithI(fileName, ".jpg") || Str::EndsWithI(fileName, ".jpeg"))
        return true;
    return false;
}

// HGLOBAL must be allocated with GlobalAlloc(GMEM_MOVEABLE, ...)
Bitmap *BitmapFromHGlobal(HGLOBAL mem)
{
    IStream *stream = NULL;
    Bitmap *bmp = NULL;

    GlobalLock(mem); // not sure if needed

    if (CreateStreamOnHGlobal(mem, FALSE, &stream) != S_OK)
        goto Exit;

    bmp = Bitmap::FromStream(stream);
    stream->Release();
    if (!bmp)
        goto Exit;

    if (bmp->GetLastStatus() != Ok)
    {
        delete bmp;
        bmp = NULL;
    }
Exit:
    GlobalUnlock(mem);
    return bmp;
}

ComicBookPage *LoadCurrentComicBookPage(unzFile& uf)
{
    char fileName[MAX_PATH];
    unz_file_info64 finfo;
    ComicBookPage *page = NULL;
    HGLOBAL bmpData = NULL;
    int readBytes;

    int err = unzGetCurrentFileInfo64(uf, &finfo, fileName, dimof(fileName), NULL, 0, NULL, 0);
    if (err != UNZ_OK)
        return NULL;

    if (!IsImageFile(fileName))
        return NULL;

    err = unzOpenCurrentFilePassword(uf, NULL);
    if (err != UNZ_OK)
        return NULL;

    unsigned len = (unsigned)finfo.uncompressed_size;
    ZPOS64_T len2 = len;
    if (len2 != finfo.uncompressed_size) // overflow check
        goto Exit;

    bmpData = GlobalAlloc(GMEM_MOVEABLE, len);
    if (!bmpData)
        goto Exit;

    void *buf = GlobalLock(bmpData);
    readBytes = unzReadCurrentFile(uf, buf, len);
    GlobalUnlock(bmpData);

    if ((unsigned)readBytes != len)
        goto Exit;

    // TODO: do I have to keep bmpData locked? Bitmap created from IStream
    // based on HGLOBAL memory data seems to need underlying memory bits
    // for its lifetime (it gets corrupted if I GlobalFree(bmpData)).
    // What happens if this data is moved?
    Bitmap *bmp = BitmapFromHGlobal(bmpData);
    if (!bmp)
        goto Exit;

    page = new ComicBookPage(bmpData, bmp);
    bmpData = NULL;

Exit:
    unzCloseCurrentFile(uf); // ignoring error code
    GlobalFree(bmpData);
    return page;
}

// TODO: similar to WindowInfo_CreateEmpty(), extract common parts
WindowInfo* CreateEmptyComicBookWindow()
{
    RectI windowPos;
    if (gGlobalPrefs.m_windowPos.IsEmpty()) {
        // center the window on the primary monitor
        RECT workArea;
        SystemParametersInfo(SPI_GETWORKAREA, 0, &workArea, 0);
        windowPos.y = workArea.top;
        windowPos.dy = RectDy(&workArea);
        windowPos.dx = (int)min(windowPos.dy * DEF_PAGE_RATIO, RectDx(&workArea));
        windowPos.x = (RectDx(&workArea) - windowPos.dx) / 2;
    }
    else {
        windowPos = gGlobalPrefs.m_windowPos;
        EnsureWindowVisibility(&windowPos);
    }

    HWND hwndFrame = CreateWindow(
            FRAME_CLASS_NAME, SUMATRA_WINDOW_TITLE,
            WS_OVERLAPPEDWINDOW,
            windowPos.x, windowPos.y, windowPos.dx, windowPos.dy,
            NULL, NULL,
            ghinst, NULL);
    if (!hwndFrame)
        return NULL;

    assert(NULL == FindWindowInfoByHwnd(hwndFrame));
    WindowInfo *win = new WindowInfo(hwndFrame);

    HWND hwndCanvas = CreateWindowEx(
            WS_EX_STATICEDGE, 
            CANVAS_CLASS_NAME, NULL,
            WS_CHILD | WS_HSCROLL | WS_VSCROLL,
            0, 0, 0, 0, /* position and size determined in OnSize */
            hwndFrame, NULL,
            ghinst, NULL);
    if (!hwndCanvas)
        return NULL;
    // hide scrollbars to avoid showing/hiding on empty window
    ShowScrollBar(hwndCanvas, SB_BOTH, FALSE);
    win->menu = BuildMenu(win->hwndFrame); // TODO: probably BuildComicBookMenu()

    win->hwndCanvas = hwndCanvas;
    ShowWindow(win->hwndCanvas, SW_SHOW);
    UpdateWindow(win->hwndCanvas);

    win->hwndInfotip = CreateWindowEx(WS_EX_TOPMOST,
        TOOLTIPS_CLASS, NULL, WS_POPUP | TTS_NOPREFIX | TTS_ALWAYSTIP,
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
        win->hwndCanvas, NULL, ghinst, NULL);

    CreateToolbar(win, ghinst); // TODO: a different toolbar for a comic book
    DragAcceptFiles(win->hwndCanvas, TRUE);

    gWindows.Append(win);
    return win;
}

static Vec<ComicBookPage*> *LoadComicBookPages(const TCHAR *filePath)
{
    zlib_filefunc64_def ffunc;
    unzFile uf;
    fill_win32_filefunc64(&ffunc);

    uf = unzOpen2_64(filePath, &ffunc);
    if (!uf)
        goto Exit;

    unz_global_info64 ginfo;
    int err = unzGetGlobalInfo64(uf, &ginfo);
    if (err != UNZ_OK)
        goto Exit;

    // extract all contained files one by one
    Vec<ComicBookPage*> *pages = new Vec<ComicBookPage *>(256);

    unzGoToFirstFile(uf);

    // Note: maybe lazy loading would be beneficial (but at least I would
    // need to parse image headers to extract width/height information)
    for (int n = 0; n < ginfo.number_entry; n++) {
        ComicBookPage *page = LoadCurrentComicBookPage(uf);
        if (page)
            pages->Append(page);
        err = unzGoToNextFile(uf);
        if (err != UNZ_OK)
            break;
    }

Exit:
    if (uf)
        unzClose(uf);
    if (pages && pages->Count() == 0)
    {
        delete pages;
        return NULL;
    }
    return pages;

}

static Vec<PageInfo> *PageInfosFromPages(Vec<ComicBookPage*> *pages)
{
    Vec<PageInfo> *pageInfos = new Vec<PageInfo>(pages->Count());
    PageInfo *currPageInfo = pageInfos->MakeSpaceAt(0, pages->Count());
    for (size_t i=0; i<pages->Count(); i++) {
        ComicBookPage *p = pages->At(i);
        currPageInfo->size = SizeD(p->width, p->height);
        currPageInfo->rotation = 0;
        ++currPageInfo;
    }
    return pageInfos;
}

ComicBookDisplayModel *ComicBookDisplayModelFromFile(WindowInfo *win, const TCHAR *filePath, DisplayMode displayMode)
{    
    Vec<ComicBookPage *> *pages = LoadComicBookPages(filePath);
    if (!pages)
        return NULL;
    Vec<PageInfo> *pageInfos = PageInfosFromPages(pages);
    SizeI totalAreaSize = win->canvasRc.Size();
    Layout *layout = new Layout(totalAreaSize, pageInfos, displayMode);
    return new ComicBookDisplayModel(pages, layout);
}

static bool LoadComicBookIntoWindow(
    const TCHAR *filePath,
    WindowInfo *win,
    const DisplayState *state,
    bool isNewWindow,    // if true then 'win' refers to a newly created window that needs to be resized and placed
    bool showWin,          // window visible or not
    bool placeWindow)      // if true then the Window will be moved/sized according to the 'state' information even if the window was already placed before (isNewWindow=false)
{
    // Never load settings from a preexisting state if the user doesn't wish to
    // (unless we're just refreshing the document, i.e. only if placeWindow == true)
    if (placeWindow && (gGlobalPrefs.m_globalPrefsOnly || state && state->useGlobalValues))
        state = NULL;
    DisplayMode displayMode = gGlobalPrefs.m_defaultDisplayMode;
    size_t startPage = 1;
    ScrollState ss(1, -1, -1);
    bool showAsFullScreen = WIN_STATE_FULLSCREEN == gGlobalPrefs.m_windowState;
    int showType = gGlobalPrefs.m_windowState == WIN_STATE_MAXIMIZED || showAsFullScreen ? SW_MAXIMIZE : SW_NORMAL;

    if (state) {
        startPage = state->pageNo;
        displayMode = state->displayMode;
        showAsFullScreen = WIN_STATE_FULLSCREEN == state->windowState;
        if (state->windowState == WIN_STATE_NORMAL)
            showType = SW_NORMAL;
        else if (state->windowState == WIN_STATE_MAXIMIZED || showAsFullScreen)
            showType = SW_MAXIMIZE;
        else if (state->windowState == WIN_STATE_MINIMIZED)
            showType = SW_MINIMIZE;
    }
    /* TODO: need to get rid of that, but not sure if that won't break something
       i.e. UpdateCanvasSize() caches size of canvas and some code might depend
       on this being a cached value, not the real value at the time of calling */
    win->UpdateCanvasSize();

    free(win->loadedFilePath);

    win->cbdm = ComicBookDisplayModelFromFile(win, filePath, displayMode);
    if (!win->cbdm)
        goto Error;

    win->loadedFilePath = Str::Dup(filePath);

    float zoomVirtual = gGlobalPrefs.m_defaultZoom;
    int rotation = DEFAULT_ROTATION;

    if (state) {
        if (startPage >= 1 && startPage <= (size_t)win->cbdm->PageCount()) {
            ss.page = startPage;
            if (ZOOM_FIT_CONTENT != state->zoomVirtual) {
                ss.x = state->scrollPos.x;
                ss.y = state->scrollPos.y;
            }
            // else relayout scroll to fit the page (again)
        } else if (startPage > (size_t)win->cbdm->PageCount())
            ss.page = win->cbdm->PageCount();
        zoomVirtual = state->zoomVirtual;
        rotation = state->rotation;
    }

    win->cbdm->DoLayout(zoomVirtual, rotation);

    if (!isNewWindow)
        win->RedrawAll();

    int pageCount = win->cbdm->PageCount();
    if (pageCount > 0) {
        UpdateToolbarPageText(win, pageCount);
    }

    const TCHAR *title = Path::GetBaseName(win->loadedFilePath);
    Win::SetText(win->hwndFrame, title);

Error:
    if (isNewWindow || placeWindow && state) {
        assert(win);
        if (isNewWindow && state && !state->windowPos.IsEmpty()) {
            RectI rect = state->windowPos;
            // Make sure it doesn't have a position like outside of the screen etc.
            rect_shift_to_work_area(&rect.ToRECT(), FALSE);
            // This shouldn't happen until !win->IsAboutWindow(), so that we don't
            // accidentally update gGlobalState with this window's dimensions
            MoveWindow(win->hwndFrame, rect.x, rect.y, rect.dx, rect.dy, TRUE);
        }

        if (showWin) {
            ShowWindow(win->hwndFrame, showType);
        }
        UpdateWindow(win->hwndFrame);
    }

//    if (win->ComicBookLoaded())
//        win->dm->setScrollState(&ss);

    UpdateToolbarAndScrollbarsForAllWindows();
    if (!win->ComicBookLoaded()) {
        win->RedrawAll();
        return false;
    }
    // This should only happen after everything else is ready
    if ((isNewWindow || placeWindow) && showWin && showAsFullScreen)
        WindowInfo_EnterFullscreen(win);

//    if (!isNewWindow && win->presentation && win->dm)
//        win->dm->setPresentationMode(true);

    return true;
}

// Load *.cbz / *.cbr file
// TODO: far from being done
WindowInfo *LoadComicBook(const TCHAR *fileName, WindowInfo *win, bool showWin)
{
    ScopedMem<TCHAR> filePath(Path::Normalize(fileName));
    if (!filePath)
        return win;

    bool isNewWindow = false;
    if (!win && 1 == gWindows.Count() && gWindows[0]->IsAboutWindow()) {
        win = gWindows[0];
    } if (!win || !win->IsAboutWindow()) {
        win = CreateEmptyComicBookWindow();
        if (!win)
            return NULL;
        isNewWindow = true;
    }

    DisplayState *ds = gFileHistory.Find(filePath);
    if (ds) {
        AdjustRemovableDriveLetter(filePath);
        CheckPositionAndSize(ds);
    }

    if (!LoadComicBookIntoWindow(filePath, win, ds, isNewWindow, showWin, true)) {
        /* failed to open */
        gFileHistory.MarkFileInexistent(filePath);
        return win;
    }

    if (gGlobalPrefs.m_rememberOpenedFiles)
        gFileHistory.MarkFileLoaded(filePath);

    // Add the file also to Windows' recently used documents (this doesn't
    // happen automatically on drag&drop, reopening from history, etc.)
    SHAddToRecentDocs(SHARD_PATH, filePath);
    return win;
}

void DrawComicBook(WindowInfo *win, HWND hwnd, HDC hdc)
{
    Win::Font::ScopedFont font(hdc, _T("MS Shell Dlg"), 14);
    Win::HdcScopedSelectFont tmp(hdc, font);

    ClientRect r(hwnd); // should get that from hdc ?

    Graphics g(hdc);
    g.SetCompositingQuality(CompositingQualityHighQuality);
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.SetPageUnit(UnitPixel);

    SolidBrush bgBrush(Color(255,242,0));
    Gdiplus::Rect r2(r.y-1, r.x-1, r.dx+1, r.dy+1);
    g.FillRectangle(&bgBrush, r2);

    Vec<ComicBookPage *> *pages = win->cbdm->pages;
    if (0 == pages->Count()) {
        DrawCenteredText(hdc, r, _T("Will show comic books"));
        return;
    }

    ComicBookPage *page = pages->At(0);
    Bitmap *bmp = page->bmp;
    g.DrawImage(bmp, 0, 0, r.dx, r.dy);
}

static void OnPaint(WindowInfo *win)
{
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(win->hwndCanvas, &ps);
    win->ResizeIfNeeded(false);
    DrawComicBook(win, win->hwndCanvas, win->hdcToDraw);
    win->DoubleBuffer_Show(hdc);
    EndPaint(win->hwndCanvas, &ps);
}

LRESULT HandleWindowComicBookMsg(WindowInfo *win, HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam, bool& handled)
{
    assert(win->ComicBookLoaded());
    handled = false;

    switch (message)
    {
        case WM_PAINT:
            OnPaint(win);
            handled = true;
            return 0;
    }

    return 0;
}


