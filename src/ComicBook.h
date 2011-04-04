/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef ComicEngine_h
#define ComicEngine_h

#include "BaseUtil.h"
#include "GeomUtil.h"
#include "Vec.h"
#include "DisplayModel.h"

class ComicBookPage {
public:
    HGLOBAL             bmpData;
    Gdiplus::Bitmap *   bmp;
    int                 w, h;

    ComicBookPage(HGLOBAL bmpData, Gdiplus::Bitmap *bmp) :
        bmpData(bmpData), bmp(bmp),  w(bmp->GetWidth()), h(bmp->GetHeight())
    {
    }

    ~ComicBookPage() {
        delete bmp;
        GlobalFree(bmpData);
    }
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
    /* position and size within total area after applying zoom and rotation.
       Represents display rectangle for a given page.
       Calculated in DisplayModel::relayout() */
    RectI           currPos;

    // data that changes due to scrolling. Calculated in DisplayModel::recalcVisibleParts() */
    float           visibleRatio; /* (0.0 = invisible, 1.0 = fully visible) */
    /* part of the image that should be shown */
    RectI           bitmap;
    /* where it should be blitted on the screen */
    int             screenX, screenY;
    /* position of page relative to visible draw area */
    RectI           pageOnScreen;
};

// Terminology:
// viewPort - what DisplayMode calls drawArea i.e. the visible 

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
    bool              displayR2L;

    // width/height of vertical/horizontal scrollbar
    int             vertScrollbarDx, horizScrollbarDy;

    /* size of draw area, including part that might be taken by scrollbars. Updated by
       clients when window size changes. */
    SizeI           totalAreaSize; // TODO: better name, e.g. totalViewPortSize

    /* drawAreaSize = totalAreaSize - area taken by visible scrollbars */
    SizeI           drawAreaSize; // TODO: a better name e.g. viewPortSize

    /* areaOffset is "polymorphic". If drawAreaSize.dx > canvasSize.dx then
       areaOffset.x is offset of canvas rect inside draw area, otherwise
       an offset of draw area inside canvas.
       The same for areaOff.y, except it's for dy */
    PointI          areaOffset;

    SizeI           canvasSize; // TODO: better name e.g. areaSize

    float zoomVirtual, zoomReal;
    int rotation;

    Layout(SizeI totalAreaSize, Vec<PageInfo> *pages, DisplayMode dm) : 
        totalAreaSize(totalAreaSize), pages(pages), displayMode(dm)
    {
        padding = &gDisplaySettings;
        vertScrollbarDx = GetSystemMetrics(SM_CXVSCROLL);
        horizScrollbarDy = GetSystemMetrics(SM_CYHSCROLL);
        displayR2L = false;
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

class ComicBookDisplayModel {
public:
    ComicBookDisplayModel() {};
    ~ComicBookDisplayModel() {}
};

class WindowInfo;

bool        IsComicBook(const TCHAR *fileName);
WindowInfo *LoadComicBook(const TCHAR *fileName, WindowInfo *win, bool showWin);
LRESULT     HandleWindowComicBookMsg(WindowInfo *win, HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam, bool& handled);

// TODO: in sumatrapdf.cpp, find a better place to define
void EnsureWindowVisibility(RectI *rect);

#endif
