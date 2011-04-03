/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef ComicEngine_h
#define ComicEngine_h

#include "BaseUtil.h"
#include "GeomUtil.h"

class WindowInfo;

bool        IsComicBook(const TCHAR *fileName);
WindowInfo *LoadComicBook(const TCHAR *fileName, WindowInfo *win, bool showWin);

// TODO: in sumatrapdf.cpp, find a better place to define
void EnsureWindowVisibility(RectI *rect);

#endif
