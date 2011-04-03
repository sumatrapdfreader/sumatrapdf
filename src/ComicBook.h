/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef ComicEngine_h
#define ComicEngine_h

#include "BaseUtil.h"
#include "GeomUtil.h"

class ComicBookPage {
public:
    ComicBookPage(Gdiplus::Bitmap *bmp) :
        bmp(bmp),  w(bmp->GetWidth()), h(bmp->GetHeight())
    {
    }

    int                 w, h;
    Gdiplus::Bitmap *   bmp;
};

class WindowInfo;

bool        IsComicBook(const TCHAR *fileName);
WindowInfo *LoadComicBook(const TCHAR *fileName, WindowInfo *win, bool showWin);

// TODO: in sumatrapdf.cpp, find a better place to define
void EnsureWindowVisibility(RectI *rect);

#endif
