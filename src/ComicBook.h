/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef ComicEngine_h
#define ComicEngine_h

#include "BaseUtil.h"
#include "GeomUtil.h"

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

class WindowInfo;

bool        IsComicBook(const TCHAR *fileName);
WindowInfo *LoadComicBook(const TCHAR *fileName, WindowInfo *win, bool showWin);
LRESULT     HandleWindowComicBookMsg(WindowInfo *win, HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam, bool& handled);

// TODO: in sumatrapdf.cpp, find a better place to define
void EnsureWindowVisibility(RectI *rect);

#endif
