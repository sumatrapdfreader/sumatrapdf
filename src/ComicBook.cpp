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

// TODO: use gDisplaySettings
DisplaySettings gDisplaySettings2 = {
  PADDING_PAGE_BORDER_TOP_DEF,
  PADDING_PAGE_BORDER_BOTTOM_DEF,
  PADDING_PAGE_BORDER_LEFT_DEF,
  PADDING_PAGE_BORDER_RIGHT_DEF,
  PADDING_BETWEEN_PAGES_X_DEF,
  PADDING_BETWEEN_PAGES_Y_DEF
}, gDisplaySettingsPresentation2 = {
  0, 0, 0, 0,
  PADDING_BETWEEN_PAGES_X_DEF, PADDING_BETWEEN_PAGES_X_DEF
};

// Information about a page for the purpose of the layout.
// Note: not happy about the name. It represents a rectangle on the screen.
// It's a page in the pdf but an image in cb* file. BoxInfo? Box?
struct PageInfo {

    // constant info that is set only once
    SizeD   size;
    // native rotation. Only valid for PDF, always 0 for others
    int     rotation;

    // data that needs to set before calling Layout()
    // notShown is used to support non-continuous mode with the same layout
    // code. In non-continous mode only one (in single page mode) or
    // two (in facing or book view mode) pages are considered visible by
    // layout and all others are marked as notShown
    bool    notShown;

    // data that changes as a result of layout process

};

// Note: this layout implementation handles scrollbars
// Note: for compat with DisplayModel, page numbers are 1-based
// but eventually should be 0-based
// Note: it doesn't do dpi scaling (_dpiFactor), it seems like wrong
// place to do it
class Layout {
public:
    Vec<PageInfo> *pages;

    DisplaySettings * padding;

    DisplayMode       displayMode;

    /* size of draw area, including part that might be taken by scrollbars */
    SizeI           drawAreaSize; // TODO: a better name e.g. totalBoxSize
    int             vertScrollbarDx, horizScrollbarDy;

    // drawAreaSize
    /* areaOffset is "polymorphic". If drawAreaSize.dx > totalAreSize.dx then
       areaOffset.x is offset of total area rect inside draw area, otherwise
       an offset of draw area inside total area.
       The same for areaOff.y, except it's for dy */
    PointI          areaOffset;

    float zoomVirtual, zoomReal;
    int rotation;

    Layout(SizeI drawAreaSize, Vec<PageInfo> *pages, DisplayMode dm) : 
        drawAreaSize(drawAreaSize), pages(pages), displayMode(dm)
    {
        padding = &gDisplaySettings2;
        vertScrollbarDx = GetSystemMetrics(SM_CXVSCROLL);
        horizScrollbarDy = GetSystemMetrics(SM_CYHSCROLL);
    }

    ~Layout() {
        delete pages;
    }

    int PageCount() const { return pages->Count(); }
    PageInfo& GetPageInfo(int pageNo) const { return pages->At(pageNo - 1); }
    bool PageShown(int pageNo) {
        PageInfo pi = GetPageInfo(pageNo);
        return !pi.notShown;
    }

    float ZoomRealFromVirtualForPage(float zoomVirtual, int pageNo);
    void DoLayout(float zoomVirtual, int rot);

    void SetZoomVirtual(float zoom);
};

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
    // we start layout assuming we can use the whole drawAreaSize
    // when we discover we need to show scrollbars, we restart layout
    // using drawAreaSize - part used by scrollbars
    bool showHorizScrollbar = false;
    bool showVertScrollbar = false;

    normalizeRotation(&rot);
    rotation = rot;
    float currZoomReal = zoomReal;
    SetZoomVirtual(zoomVirtual);

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

static bool LoadComicBookIntoWindow(
    const TCHAR *filePath,
    WindowInfo *win,
    const DisplayState *state,
    bool isNewWindow,    // if true then 'win' refers to a newly created window that needs to be resized and placed
    bool showWin,          // window visible or not
    bool placeWindow)      // if true then the Window will be moved/sized according to the 'state' information even if the window was already placed before (isNewWindow=false)
{
    Vec<ComicBookPage *> *pages = NULL;

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
    win->loadedFilePath = Str::Dup(filePath);

    pages = LoadComicBookPages(filePath);
    if (!pages)
        goto Error;

    win->comicPages = pages;
    float zoomVirtual = gGlobalPrefs.m_defaultZoom;
    int rotation = DEFAULT_ROTATION;

    if (state) {
        if (startPage >= 1 && startPage <= pages->Count()) {
            ss.page = startPage;
            if (ZOOM_FIT_CONTENT != state->zoomVirtual) {
                ss.x = state->scrollPos.x;
                ss.y = state->scrollPos.y;
            }
            // else relayout scroll to fit the page (again)
        } else if (startPage > pages->Count())
            ss.page = pages->Count();
        zoomVirtual = state->zoomVirtual;
        rotation = state->rotation;
    }
    // TODO: layout

    if (!isNewWindow)
        win->RedrawAll();

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

    Vec<ComicBookPage *> *pages = win->comicPages;
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


